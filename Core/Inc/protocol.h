#ifndef PROTOCOL_H
#define PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PROTOCOL_SOF1 (0xAAU)
#define PROTOCOL_SOF2 (0x55U)
#define PROTOCOL_VERSION (0x01U)

#define PROTOCOL_HEADER_SIZE (6U) /* version, msg_type, flags, seq, len_l, len_h */
#define PROTOCOL_CRC_SIZE (2U)
#define PROTOCOL_MAX_PAYLOAD (64U)
#define PROTOCOL_MAX_FRAME_SIZE (2U + PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD + PROTOCOL_CRC_SIZE)

typedef enum
{
  PROTOCOL_MSG_TELEMETRY = 0x01U,
  PROTOCOL_MSG_CMD_SET_SPEED = 0x10U,
  PROTOCOL_MSG_CMD_STOP = 0x11U,
  PROTOCOL_MSG_CMD_PING = 0x12U,
  PROTOCOL_MSG_PONG = 0x13U
} protocol_msg_type_t;

typedef struct
{
  uint8_t version;
  uint8_t msg_type;
  uint8_t flags;
  uint8_t seq;
  uint16_t payload_len;
  uint8_t payload[PROTOCOL_MAX_PAYLOAD];
} protocol_frame_t;

typedef struct
{
  uint8_t state;
  uint8_t header[PROTOCOL_HEADER_SIZE];
  uint16_t header_pos;
  uint8_t payload[PROTOCOL_MAX_PAYLOAD];
  uint16_t payload_pos;
  uint16_t expected_payload_len;
  uint8_t crc_lsb;
  uint8_t crc_buf[PROTOCOL_HEADER_SIZE + PROTOCOL_MAX_PAYLOAD];
} protocol_parser_t;

uint16_t protocol_crc16_ccitt(const uint8_t *data, uint16_t len);

size_t protocol_encode_frame(const protocol_frame_t *frame, uint8_t *out, size_t out_size);

void protocol_parser_init(protocol_parser_t *parser);

bool protocol_parser_process_byte(protocol_parser_t *parser, uint8_t byte, protocol_frame_t *out_frame);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
