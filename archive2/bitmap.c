#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>

#include "bitmap.h"

bitmap_ptr bitmap_init(size_t size_in_bytes, bool init_value) {
  void* data = NULL;
  data = malloc(size_in_bytes);
  if (data == NULL) {
    perror("Failed to create data field for bitmap");
  }
  memset(data, init_value == true ? BYTEONE : BYTEZERO, size_in_bytes);
  // return ret;
  bitmap_ptr bm = (bitmap_ptr) malloc(sizeof(bitmap_t));
  if (bm == NULL) {
    perror("Failed to create bitmap");
  } 
  bm->data = (uint8_t*) data;
  bm->size = size_in_bytes;
  return bm;
}

void bitmap_destroy(bitmap_ptr ptr) {
  free(ptr->data);
  free(ptr);
}

void bitmap_set(bitmap_ptr ptr, uint64_t bit_idx, bool value) {
  assert(bit_idx >= 0 && bit_idx < (ptr->size << 3));
  uint64_t byte_idx = bit_idx >> 3;
  uint8_t bit_offset = bit_idx % 8;
  uint8_t mask = 0x1 << bit_offset;

  if (value) {
    ptr->data[byte_idx] |= mask; 
  } else {
    ptr->data[byte_idx] &= ~mask;
  }
}

bool bitmap_get(bitmap_ptr ptr, uint64_t bit_idx) {
  assert(bit_idx >= 0 && bit_idx < ptr->size * 8);
  uint64_t byte_idx = bit_idx >> 3;
  uint8_t bit_offset = bit_idx % 8;
  uint8_t mask = 0x1 << bit_offset;

  return ((bool) (ptr->data[byte_idx] & mask));
}