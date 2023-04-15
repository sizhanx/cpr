#ifndef _BITMAP_H_
#define _BITMAP_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

const uint8_t BYTEONE = 0xFF;
const uint8_t BYTEZERO = 0x0;

typedef struct {
  uint8_t* data;
  size_t size;
} bitmap_t,*bitmap_ptr;

bitmap_ptr bitmap_init(size_t size_in_bytes, bool init_value);

void bitmap_destroy(bitmap_ptr ptr);

void bitmap_set(bitmap_ptr ptr, uint64_t bit_idx, bool value);

bool bitmap_get(bitmap_ptr ptr, uint64_t bit_idx);

#endif