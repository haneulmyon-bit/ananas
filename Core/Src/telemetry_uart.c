#include "telemetry_uart.h"

#include "protocol.h"
#include "ringbuf.h"

#include <string.h>

static void telemetry_uart_kick_tx(telemetry_uart_t *self);
static void telemetry_uart_dispatch_frame(telemetry_uart_t *self, const protocol_frame_t *frame);
static HAL_StatusTypeDef telemetry_uart_queue_frame(telemetry_uart_t *self, const protocol_frame_t *frame);

HAL_StatusTypeDef telemetry_uart_init(telemetry_uart_t *self, UART_HandleTypeDef *huart,
                                      const motor_control_iface_t *motor_iface)
{
  if ((self == NULL) || (huart == NULL) || (motor_iface == NULL))
  {
    return HAL_ERROR;
  }

  (void)memset(self, 0, sizeof(*self));
  self->huart = huart;
  self->motor_iface = motor_iface;

  protocol_parser_init(&self->parser);
  ringbuf_init(&self->tx_ring, self->tx_storage, (uint16_t)sizeof(self->tx_storage));
  return HAL_OK;
}

HAL_StatusTypeDef telemetry_uart_start(telemetry_uart_t *self)
{
  if ((self == NULL) || (self->huart == NULL))
  {
    return HAL_ERROR;
  }

  return HAL_UART_Receive_IT(self->huart, &self->rx_byte, 1U);
}

void telemetry_uart_set_inputs(telemetry_uart_t *self, const telemetry_inputs_t *inputs)
{
  if ((self == NULL) || (inputs == NULL))
  {
    return;
  }

  self->inputs = *inputs;
}

void telemetry_uart_send_telemetry(telemetry_uart_t *self, uint32_t timestamp_ms)
{
  protocol_frame_t frame = {0};
  motor_status_t motor_status = {0};
  uint8_t payload[15];
  uint8_t hall_bits = 0U;

  if ((self == NULL) || (self->motor_iface == NULL) || (self->motor_iface->get_status == NULL))
  {
    return;
  }

  self->motor_iface->get_status(self->motor_iface->ctx, &motor_status);

  hall_bits |= (self->inputs.hall1 != 0U) ? (1U << 0) : 0U;
  hall_bits |= (self->inputs.hall2 != 0U) ? (1U << 1) : 0U;
  hall_bits |= (self->inputs.hall3 != 0U) ? (1U << 2) : 0U;

  payload[0] = (uint8_t)(timestamp_ms & 0xFFU);
  payload[1] = (uint8_t)((timestamp_ms >> 8) & 0xFFU);
  payload[2] = (uint8_t)((timestamp_ms >> 16) & 0xFFU);
  payload[3] = (uint8_t)((timestamp_ms >> 24) & 0xFFU);
  payload[4] = (uint8_t)(self->inputs.emg & 0xFFU);
  payload[5] = (uint8_t)((self->inputs.emg >> 8) & 0xFFU);
  payload[6] = (uint8_t)(self->inputs.fsr2 & 0xFFU);
  payload[7] = (uint8_t)((self->inputs.fsr2 >> 8) & 0xFFU);
  payload[8] = (uint8_t)(self->inputs.fsr1 & 0xFFU);
  payload[9] = (uint8_t)((self->inputs.fsr1 >> 8) & 0xFFU);
  payload[10] = hall_bits;
  payload[11] = (uint8_t)motor_status.signed_speed_pct;
  payload[12] = motor_status.duty_pct;
  payload[13] = (uint8_t)motor_status.direction;
  payload[14] = (uint8_t)(motor_status.fault_flags & 0xFFU);

  frame.version = PROTOCOL_VERSION;
  frame.msg_type = PROTOCOL_MSG_TELEMETRY;
  frame.flags = 0U;
  frame.seq = self->tx_seq++;
  frame.payload_len = (uint16_t)sizeof(payload);
  (void)memcpy(frame.payload, payload, sizeof(payload));

  (void)telemetry_uart_queue_frame(self, &frame);
}

void telemetry_uart_handle_rx_irq(telemetry_uart_t *self, UART_HandleTypeDef *huart)
{
  protocol_frame_t frame = {0};

  if ((self == NULL) || (huart != self->huart))
  {
    return;
  }

  if (protocol_parser_process_byte(&self->parser, self->rx_byte, &frame))
  {
    telemetry_uart_dispatch_frame(self, &frame);
  }

  (void)HAL_UART_Receive_IT(self->huart, &self->rx_byte, 1U);
}

void telemetry_uart_handle_tx_irq(telemetry_uart_t *self, UART_HandleTypeDef *huart)
{
  if ((self == NULL) || (huart != self->huart))
  {
    return;
  }

  __disable_irq();
  self->tx_busy = 0U;
  __enable_irq();
  telemetry_uart_kick_tx(self);
}

void telemetry_uart_handle_error_irq(telemetry_uart_t *self, UART_HandleTypeDef *huart)
{
  if ((self == NULL) || (huart != self->huart))
  {
    return;
  }

  __disable_irq();
  self->tx_busy = 0U;
  __enable_irq();
  (void)HAL_UART_Receive_IT(self->huart, &self->rx_byte, 1U);
  telemetry_uart_kick_tx(self);
}

static HAL_StatusTypeDef telemetry_uart_queue_frame(telemetry_uart_t *self, const protocol_frame_t *frame)
{
  uint8_t encoded[PROTOCOL_MAX_FRAME_SIZE];
  size_t encoded_len;
  uint16_t written;
  uint16_t free_space;

  if ((self == NULL) || (frame == NULL))
  {
    return HAL_ERROR;
  }

  encoded_len = protocol_encode_frame(frame, encoded, sizeof(encoded));
  if (encoded_len == 0U)
  {
    return HAL_ERROR;
  }

  __disable_irq();
  free_space = ringbuf_free(&self->tx_ring);
  if (free_space < (uint16_t)encoded_len)
  {
    __enable_irq();
    self->dropped_frames++;
    return HAL_BUSY;
  }

  written = ringbuf_write(&self->tx_ring, encoded, (uint16_t)encoded_len);
  __enable_irq();
  if (written != (uint16_t)encoded_len)
  {
    self->dropped_frames++;
    return HAL_BUSY;
  }

  telemetry_uart_kick_tx(self);
  return HAL_OK;
}

static void telemetry_uart_send_pong(telemetry_uart_t *self, const protocol_frame_t *ping)
{
  protocol_frame_t response = {0};
  uint16_t payload_len = ping->payload_len;

  if (payload_len > PROTOCOL_MAX_PAYLOAD)
  {
    payload_len = PROTOCOL_MAX_PAYLOAD;
  }

  response.version = PROTOCOL_VERSION;
  response.msg_type = PROTOCOL_MSG_PONG;
  response.flags = 0U;
  response.seq = self->tx_seq++;
  response.payload_len = payload_len;
  if (payload_len > 0U)
  {
    (void)memcpy(response.payload, ping->payload, payload_len);
  }

  (void)telemetry_uart_queue_frame(self, &response);
}

static void telemetry_uart_dispatch_frame(telemetry_uart_t *self, const protocol_frame_t *frame)
{
  if ((self == NULL) || (frame == NULL) || (frame->version != PROTOCOL_VERSION) ||
      (self->motor_iface == NULL))
  {
    return;
  }

  switch ((protocol_msg_type_t)frame->msg_type)
  {
    case PROTOCOL_MSG_CMD_SET_SPEED:
      if ((frame->payload_len >= 1U) && (self->motor_iface->set_speed_pct != NULL))
      {
        self->motor_iface->set_speed_pct(self->motor_iface->ctx, (int8_t)frame->payload[0]);
      }
      break;

    case PROTOCOL_MSG_CMD_STOP:
      if (self->motor_iface->stop != NULL)
      {
        self->motor_iface->stop(self->motor_iface->ctx);
      }
      break;

    case PROTOCOL_MSG_CMD_PING:
      telemetry_uart_send_pong(self, frame);
      break;

    case PROTOCOL_MSG_TELEMETRY:
    case PROTOCOL_MSG_PONG:
    default:
      break;
  }
}

static void telemetry_uart_kick_tx(telemetry_uart_t *self)
{
  uint8_t have_byte = 0U;

  if ((self == NULL) || (self->huart == NULL))
  {
    return;
  }

  __disable_irq();
  if ((self->tx_busy == 0U) && ringbuf_pop(&self->tx_ring, &self->tx_byte))
  {
    self->tx_busy = 1U;
    have_byte = 1U;
  }
  __enable_irq();

  if (have_byte != 0U)
  {
    if (HAL_UART_Transmit_IT(self->huart, &self->tx_byte, 1U) != HAL_OK)
    {
      __disable_irq();
      self->tx_busy = 0U;
      __enable_irq();
    }
  }
}
