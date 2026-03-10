/*
 * Optional host-side C stub for protocol encode smoke check.
 * Build example (arm-none-eabi-gcc or native gcc with include paths):
 *   gcc -I../../Core/Inc ../../Core/Src/protocol.c protocol_encode_stub.c -o proto_stub
 */

#include <stdio.h>

#include "protocol.h"

int main(void)
{
  protocol_frame_t frame = {0};
  uint8_t out[PROTOCOL_MAX_FRAME_SIZE];
  size_t n;

  frame.version = PROTOCOL_VERSION;
  frame.msg_type = PROTOCOL_MSG_CMD_SET_SPEED;
  frame.seq = 1U;
  frame.payload_len = 1U;
  frame.payload[0] = 30U;

  n = protocol_encode_frame(&frame, out, sizeof(out));
  printf("encoded_len=%u\n", (unsigned)n);
  return 0;
}
