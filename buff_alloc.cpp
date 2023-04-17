#include "buff_alloc.hpp"
#include <cassert>
#include <cstdio>
#include <cstdlib>

buff_alloc::buff_alloc(size_t sz)
    : total_size(sz),
      total_pages((sz % PAGE_SIZE == 0 ? 0 : 1) + (sz / PAGE_SIZE)) {
  // this->buff = malloc(sz);
  posix_memalign(&(this->buff), PAGE_SIZE, sz);
  if (buff == nullptr) {
    perror("Allocating buffer for buff_alloc obj failed");
  }
  avail_pages = total_pages;
  void *curr_buff_addr = this->buff;
  for (size_t i = 0; i < total_pages; i++) {
    this->free_page_list.push_back(
        (void *)((size_t)curr_buff_addr + PAGE_SIZE));
  }
}

buff_alloc::~buff_alloc() { free(this->buff); }

void *buff_alloc::alloc_buf_page() {
  if (this->free_page_list.empty())
    return nullptr;
  void *free_page = this->free_page_list.front();
  this->free_page_list.pop_front();
  return free_page;
}

int buff_alloc::get_buf_page_idx(void* ptr) {
  assert(ptr >= this->buff);
  assert(ptr < (void *)((size_t)this->buff + total_size));
  return (int) ((size_t) ((size_t) ptr - (size_t) this->buff ) / PAGE_SIZE);
}

void buff_alloc::relese_buf_page(void *ptr) {
  assert(ptr >= this->buff);
  assert(ptr < (void *)((size_t)this->buff + total_size));
  size_t diff = ((size_t)ptr) % PAGE_SIZE;
  void *rounded_ptr = (void *)((size_t)ptr - diff);
  this->free_page_list.push_back(rounded_ptr);
}

bool buff_alloc::empty() {
  return this->free_page_list.empty();
}