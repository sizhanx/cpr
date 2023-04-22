#ifndef _COMMON_CPP_
#define _COMMON_CPP_

#include <cstdlib>
#include <shared_mutex>

const size_t ONE = 1;
constexpr size_t PAGE_SIZE = ONE << 15; // 32KB
constexpr size_t GB = ONE << 30;

namespace sizhan {
typedef std::shared_lock<std::shared_timed_mutex> read_lock;
typedef std::lock_guard<std::shared_timed_mutex> write_lock;
} // namespace sizhan

#endif