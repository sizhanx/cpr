#ifndef _USER_DATA_HPP_
#define _USER_DATA_HPP_

#include <cstdint>
#include <cstdlib>

class user_data {
private:
  uint64_t data;

public:
  explicit user_data(uint64_t packed);
  explicit user_data(int src_fd, int dest_fd, size_t buff_idx,
                        size_t file_off_idx, bool read_done, bool write_done);
  int src_fd();
  int dest_fd();
  size_t buff_idx();
  size_t file_off_idx();
  bool read_done();
  void read_done(bool val);
  bool write_done();
  void write_done(bool val);

  uint64_t get_data();
};

#endif