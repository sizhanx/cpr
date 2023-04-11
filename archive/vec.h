#ifndef _VEC_H_
#define _VEC_H_

#include <linux/aio_abi.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <assert.h>

typedef struct {
  int count;
  int capacity;
  struct iocb** arr;
} iocb_vec;

iocb_vec* vec_init() {
  iocb_vec* v = (iocb_vec*) calloc(1, sizeof(iocb_vec));
  if (v == NULL) {
    return NULL;
  }
  v->capacity = 8;
  v->arr = (struct iocb**) calloc(8, sizeof(struct iocb**));
  if (v->arr == NULL) {
    free(v);
    return NULL;
  }
  return v;
}

void vec_add(iocb_vec* vec, struct iocb* item) {
  assert(vec != NULL);
  assert(item != NULL);
  while (vec->count >= vec->capacity) {
    size_t curr_arr_size = sizeof(vec->arr) * vec->capacity;
    vec->arr = (struct iocb**) realloc((void*) (vec->arr), 2 * curr_arr_size);
    memset(vec->arr + vec->capacity, 0, curr_arr_size);
    vec->capacity *= 2;
  }
  vec->arr[vec->count] = (struct iocb*) malloc(sizeof(struct iocb));
  memcpy(vec->arr[vec->count], item, sizeof(struct iocb)); 
  vec->count++;
}

void vec_destroy(iocb_vec* vec) {
  assert(vec != NULL);
  for (int i = 0;i < vec->count;i++) {
    free(vec->arr[i]);
  }
  free(vec->arr);
  free(vec);
}


#endif