#include "buff_alloc.hpp"

#include <cassert>
#include <cstdint>

const uint64_t ONE = 1;
const uint64_t GB = ONE << 30;

int main() {
  buff_alloc buff(GB);
  void* a = buff.alloc_buff_page();
  void* b = buff.alloc_buff_page();
  void* c = buff.alloc_buff_page();
}