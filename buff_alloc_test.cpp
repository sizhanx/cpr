#include "buff_alloc.hpp"

#include <cassert>
#include <cstdint>


int main() {
  buff_alloc buff(GB);
  void* a = buff.alloc_buff_page();
  void* b = buff.alloc_buff_page();
  void* c = buff.alloc_buff_page();
}