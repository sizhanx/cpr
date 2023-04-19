#ifndef _BUF_ALLOC_HPP_
#define _BUF_ALLOC_HPP_

#include <cstdint>
#include <cstdlib>
#include <deque>

constexpr size_t PAGE_SIZE = ((uint64_t)1) << 12;

class buff_alloc {
private:
  void *buff = nullptr;
  const size_t total_size;
  const size_t total_pages;
  size_t avail_pages;
  std::deque<void *> free_page_list;

public:
  explicit buff_alloc(size_t sz);
  ~buff_alloc();
  void *alloc_buff_page();
  void relese_buff_page(void *ptr);
  int get_buff_page_idx(void *addr);
  void release_buff_page_by_idx(int idx);
  bool empty();
};

#endif