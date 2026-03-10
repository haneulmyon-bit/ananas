#include "protocol.h"

#include <string.h>

enum
{
  PROTOCOL_PARSER_WAIT_SOF1 = 0,
  PROTOCOL_PARSER_WAIT_SOF2,
  PROTOCOL_PARSER_READ_HEADER,
  PROTOCOL_PARSER_READ_PAYLOAD,
  PROTOCOL_PARSER_READ_CRC_L,
  PROTOCOL_PARSER_READ_CRC_H
};

uint16_t protocol_crc16_ccitt(const uint8_t *data, uint16_t len)
{
  uint16_t crc = 0xFFFFU;
  uint16_t i;

  if (data == NULL)
  {
    return 0U;
  }

  for (i = 0U; i < len; ++i)
  {
    crc ^= (uint16_t)data[i] << 8;
    for (uint8_t bit = 0U; bit < 8U; ++bit)
    {
      if ((crc & 0x8000U) != 0U)
      {
        crc = (uint16_t)((crc << 1) ^ 0x1021U);
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}

size_t protocol_encode_frame(const protocol_frame_t *frame, uint8_t *out, size_t out_size)
{
  size_t total_size;
  uint16_t crc;
  size_t payload_offset;

  if ((frame == NULL) || (out == NULL))
  {
    return 0U;
  }

  if (frame->payload_len > PROTOCOL_MAX_PAYLOAD)
  {
    return 0U;
  }

  total_size = 2U + PROTOCOL_HEADER_SIZE + (size_t)frame->payload_len + PROTOCOL_CRC_SIZE;
  if (out_size < total_size)
  {
    return 0U;
  }

  out[0] = PROTOCOL_SOF1;
  out[1] = PROTOCOL_SOF2;
  out[2] = frame->version;
  out[3] = frame->msg_type;
  out[4] = frame->flags;
  out[5] = frame->seq;
  out[6] = (uint8_t)(frame->payload_len & 0xFFU);
  out[7] = (uint8_t)((frame->payload_len >> 8) & 0xFFU);

  payload_offset = 8U;
  if (frame->payload_len > 0U)
  {
    (void)memcpy(&out[payload_offset], frame->payload, frame->payload_len);
  }

  crc = protocol_crc16_ccitt(&out[2], (uint16_t)(PROTOCOL_HEADER_SIZE + frame->payload_len));
  out[payload_offset + frame->payload_len] = (uint8_t)(crc & 0xFFU);
  out[payload_offset + frame->payload_len + 1U] = (uint8_t)((crc >> 8) & 0xFFU);

  return total_size;
}

void protocol_parser_init(protocol_parser_t *parser)
{
  if (parser == NULL)
  {
    return;
  }

  (void)memset(parser, 0, sizeof(*parser));
  parser->state = PROTOCOL_PARSER_WAIT_SOF1;
}

static void protocol_parser_reset(protocol_parser_t *parser)
{
  parser->state = PROTOCOL_PARSER_WAIT_SOF1;
  parser->header_pos = 0U;
  parser->payload_pos = 0U;
  parser->expected_payload_len = 0U;
  parser->crc_lsb = 0U;
}

bool protocol_parser_process_byte(protocol_parser_t *parser, uint8_t byte, protocol_frame_t *out_frame)
{
  uint16_t rx_crc;
  uint16_t calc_crc;

  if ((parser == NULL) || (out_frame == NULL))
  {
    return false;
  }

  switch (parser->state)
  {
    case PROTOCOL_PARSER_WAIT_SOF1:
      if (byte == PROTOCOL_SOF1)
      {
        parser->state = PROTOCOL_PARSER_WAIT_SOF2;
      }
      break;

    case PROTOCOL_PARSER_WAIT_SOF2:
      if (byte == PROTOCOL_SOF2)
      {
        parser->state = PROTOCOL_PARSER_READ_HEADER;
        parser->header_pos = 0U;
      }
      else if (byte != PROTOCOL_SOF1)
      {
        parser->state = PROTOCOL_PARSER_WAIT_SOF1;
      }
      break;

    case PROTOCOL_PARSER_READ_HEADER:
      parser->header[parser->header_pos] = byte;
      parser->crc_buf[parser->header_pos] = byte;
      parser->header_pos++;
      if (parser->header_pos >= PROTOCOL_HEADER_SIZE)
      {
        parser->expected_payload_len = (uint16_t)parser->header[4] | ((uint16_t)parser->header[5] << 8);
        if (parser->expected_payload_len > PROTOCOL_MAX_PAYLOAD)
        {
          protocol_parser_reset(parser);
        }
        else if (parser->expected_payload_len == 0U)
        {
          parser->state = PROTOCOL_PARSER_READ_CRC_L;
        }
        else
        {
          parser->payload_pos = 0U;
          parser->state = PROTOCOL_PARSER_READ_PAYLOAD;
        }
      }
      break;

    case PROTOCOL_PARSER_READ_PAYLOAD:
      parser->payload[parser->payload_pos] = byte;
      parser->crc_buf[PROTOCOL_HEADER_SIZE + parser->payload_pos] = byte;
      parser->payload_pos++;
      if (parser->payload_pos >= parser->expected_payload_len)
      {
        parser->state = PROTOCOL_PARSER_READ_CRC_L;
      }
      break;

    case PROTOCOL_PARSER_READ_CRC_L:
      parser->crc_lsb = byte;
      parser->state = PROTOCOL_PARSER_READ_CRC_H;
      break;

    case PROTOCOL_PARSER_READ_CRC_H:
      rx_crc = (uint16_t)parser->crc_lsb | ((uint16_t)byte << 8);
      calc_crc = protocol_crc16_ccitt(parser->crc_buf,
                                      (uint16_t)(PROTOCOL_HEADER_SIZE + parser->expected_payload_len));
      if (rx_crc == calc_crc)
      {
        out_frame->version = parser->header[0];
        out_frame->msg_type = parser->header[1];
        out_frame->flags = parser->header[2];
        out_frame->seq = parser->header[3];
        out_frame->payload_len = parser->expected_payload_len;
        if (parser->expected_payload_len > 0U)
        {
          (void)memcpy(out_frame->payload, parser->payload, parser->expected_payload_len);
        }
        protocol_parser_reset(parser);
        return true;
      }
      protocol_parser_reset(parser);
      break;

    default:
      protocol_parser_reset(parser);
      break;
  }

  return false;
}
