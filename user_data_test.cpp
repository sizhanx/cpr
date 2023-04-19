#include "user_data.hpp"

#include <cassert>

int main() {
  user_data ud(1, 2, 3, 4, true, true);
  assert(ud.src_fd() == 1);
  assert(ud.dest_fd() == 2);
  assert(ud.buff_idx() == 3);
  assert(ud.file_off_idx() == 4);
  assert(ud.read_done());
  assert(ud.write_done());
  ud.read_done(false);
  assert(!ud.read_done());
}