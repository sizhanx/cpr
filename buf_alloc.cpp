#include "buf_alloc.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>

buff_alloc::buff_alloc(size_t sz)
    : total_size(sz),
      total_pages((sz % PAGE_SIZE == 0 ? 0 : 1) + (sz / PAGE_SIZE)) {
  // buff_alloc::buff = malloc(sz);
  posix_memalign(&(buff_alloc::buff), PAGE_SIZE, sz);
  if (buff == nullptr) {
    perror("Allocating buffer for buff_alloc obj failed");
  }
  avail_pages = total_pages;
  void *curr_buff_addr = buff_alloc::buff;
  for (int i = 0; i < total_pages; i++) {
    buff_alloc::free_page_list.push_back(
        (void *)((size_t)curr_buff_addr + PAGE_SIZE));
  }
}

buff_alloc::~buff_alloc() { free(buff_alloc::buff); }

void *buff_alloc::alloc_buf_page() {
  if (buff_alloc::free_page_list.empty())
    return nullptr;
  void *free_page = buff_alloc::free_page_list.front();
  buff_alloc::free_page_list.pop_front();
  return free_page;
}

void buff_alloc::relese_buf_page(void *ptr) {
  assert(ptr >= buff_alloc::buff);
  assert(ptr < (void *)((size_t)buff_alloc::buff + total_size));
  size_t diff = ((size_t)ptr) % PAGE_SIZE;
  void *rounded_ptr = (void *)((size_t)ptr - diff);
  buff_alloc::free_page_list.push_back(rounded_ptr);
}
