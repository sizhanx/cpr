#include "user_data.hpp"

const uint64_t BITMASK11 = 0x7FF;
const uint64_t BITMASK20 = 0xFFFFF;
const uint64_t BITMASK1 = 0x1;


user_data::user_data(uint64_t packed) : data(packed) {}

user_data::user_data(int src_fd, int dest_fd, size_t buff_idx,
                     size_t file_off_idx, bool read_done, bool write_done) {
  this->data = 0;
  this->data |= (((int64_t)src_fd << 53) & 0xFFE0000000000000);
  this->data |= (((int64_t)dest_fd << 42) & 0x1FFC0000000000);
  this->data |= (((uint64_t)buff_idx << 22) & 0x3FFFFC00000);
  this->data |= (((uint64_t)file_off_idx << 2) & 0x3FFFFC);
  this->data |= (((uint64_t)read_done << 1) & 0x2);
  this->data |= (write_done & 0x1);
}

int user_data::src_fd() {
  return (int) ((this->data >> 53) & BITMASK11);
}
int user_data::dest_fd() {
  return (int) ((this->data >> 42) & BITMASK11); 
}
size_t user_data::buff_idx() {
  return (size_t) ((this->data >> 22) & BITMASK20); 
}
size_t user_data::file_off_idx() {
  return (size_t) ((this->data >> 2) & BITMASK20);  
}
bool user_data::read_done() {
  return (bool) ((this->data >> 1) & BITMASK1);  
}

void user_data::read_done(bool val) {
  if (val) {
    this->data |= 0x2;
  } else {
    this->data &= 0xFFFFFFFFFFFFFFFD;
  }
}

bool user_data::write_done() {
  return (bool) (this->data & BITMASK1);  
}

void user_data::write_done(bool val) {
  if (val) {
    this->data |= 0x1;
  } else {
    this->data &= 0xFFFFFFFFFFFFFFFE;
  } 
}

uint64_t user_data::get_data() {
  return this->data;
}