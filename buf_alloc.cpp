#include <cstdlib>
#include <cstdio>
#include "buf_alloc.hpp"

buff_alloc::buff_alloc(size_t sz) {
  buff_alloc::buff = malloc(sz);
  if (buff == nullptr) {
    perror("Allocating buffer for buff_alloc obj failed");
  }
}

buff_alloc::~buff_alloc() {
  free(buff_alloc::buff);
}

