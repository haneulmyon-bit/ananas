#include "ringbuf.h"

#include <stddef.h>

void ringbuf_init(ringbuf_t *rb, uint8_t *storage, uint16_t capacity)
{
  if ((rb == NULL) || (storage == NULL) || (capacity < 2U))
  {
    return;
  }

  rb->buffer = storage;
  rb->capacity = capacity;
  rb->head = 0U;
  rb->tail = 0U;
}

uint16_t ringbuf_count(const ringbuf_t *rb)
{
  if ((rb == NULL) || (rb->capacity == 0U))
  {
    return 0U;
  }

  if (rb->head >= rb->tail)
  {
    return (uint16_t)(rb->head - rb->tail);
  }

  return (uint16_t)(rb->capacity - (rb->tail - rb->head));
}

uint16_t ringbuf_free(const ringbuf_t *rb)
{
  if (rb == NULL)
  {
    return 0U;
  }

  return (uint16_t)((rb->capacity - 1U) - ringbuf_count(rb));
}

bool ringbuf_push(ringbuf_t *rb, uint8_t value)
{
  uint16_t next_head;

  if ((rb == NULL) || (rb->capacity == 0U))
  {
    return false;
  }

  next_head = (uint16_t)((rb->head + 1U) % rb->capacity);
  if (next_head == rb->tail)
  {
    return false;
  }

  rb->buffer[rb->head] = value;
  rb->head = next_head;
  return true;
}

bool ringbuf_pop(ringbuf_t *rb, uint8_t *value)
{
  if ((rb == NULL) || (value == NULL) || (rb->capacity == 0U))
  {
    return false;
  }

  if (rb->head == rb->tail)
  {
    return false;
  }

  *value = rb->buffer[rb->tail];
  rb->tail = (uint16_t)((rb->tail + 1U) % rb->capacity);
  return true;
}

uint16_t ringbuf_write(ringbuf_t *rb, const uint8_t *data, uint16_t len)
{
  uint16_t written = 0U;

  if ((rb == NULL) || (data == NULL))
  {
    return 0U;
  }

  while (written < len)
  {
    if (!ringbuf_push(rb, data[written]))
    {
      break;
    }
    written++;
  }

  return written;
}
