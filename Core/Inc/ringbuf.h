#ifndef RINGBUF_H
#define RINGBUF_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint8_t *buffer;
  uint16_t capacity;
  volatile uint16_t head;
  volatile uint16_t tail;
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb, uint8_t *storage, uint16_t capacity);

uint16_t ringbuf_count(const ringbuf_t *rb);

uint16_t ringbuf_free(const ringbuf_t *rb);

bool ringbuf_push(ringbuf_t *rb, uint8_t value);

bool ringbuf_pop(ringbuf_t *rb, uint8_t *value);

uint16_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* RINGBUF_H */
