#ifndef _BUF_ALLOC_HPP_
#define _BUF_ALLOC_HPP_

#include <cstdlib>

class buff_alloc {
private:
  void* buff = nullptr;
  


public:
  explicit buff_alloc(size_t sz);
  ~buff_alloc();
  void* get_page_addr(int idx);
  int alloc_buf_page();
  void relese_buf_page(int idx);
};

#endif