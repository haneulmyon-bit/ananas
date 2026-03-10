#ifndef TELEMETRY_UART_H
#define TELEMETRY_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "motor_control.h"
#include "protocol.h"
#include "ringbuf.h"

#include <stdint.h>

typedef struct
{
  uint16_t emg;
  uint16_t fsr2;
  uint16_t fsr1;
  uint8_t hall1;
  uint8_t hall2;
  uint8_t hall3;
} telemetry_inputs_t;

typedef struct
{
  UART_HandleTypeDef *huart;
  const motor_control_iface_t *motor_iface;
  telemetry_inputs_t inputs;
  protocol_parser_t parser;
  ringbuf_t tx_ring;
  uint8_t tx_storage[512];
  volatile uint8_t tx_busy;
  uint8_t tx_byte;
  uint8_t rx_byte;
  uint8_t tx_seq;
  uint32_t dropped_frames;
} telemetry_uart_t;

HAL_StatusTypeDef telemetry_uart_init(telemetry_uart_t *self, UART_HandleTypeDef *huart,
                                      const motor_control_iface_t *motor_iface);

HAL_StatusTypeDef telemetry_uart_start(telemetry_uart_t *self);

void telemetry_uart_set_inputs(telemetry_uart_t *self, const telemetry_inputs_t *inputs);

void telemetry_uart_send_telemetry(telemetry_uart_t *self, uint32_t timestamp_ms);

void telemetry_uart_handle_rx_irq(telemetry_uart_t *self, UART_HandleTypeDef *huart);

void telemetry_uart_handle_tx_irq(telemetry_uart_t *self, UART_HandleTypeDef *huart);

void telemetry_uart_handle_error_irq(telemetry_uart_t *self, UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_UART_H */
