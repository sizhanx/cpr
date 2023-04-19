#include "buff_alloc.hpp"
#include "user_data.hpp"
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <dirent.h>
#include <fcntl.h>
#include <liburing.h>
#include <shared_mutex>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <cassert>

namespace sizhan {
typedef std::shared_lock<std::shared_timed_mutex> read_lock;
typedef std::lock_guard<std::shared_timed_mutex> write_lock;
} // namespace sizhan

constexpr size_t HALF_GIG = 1 << 29;
constexpr size_t NUM_BUFF_PAGES = 10;
constexpr size_t NUM_TOTAL_FILE_INFO =
    (NUM_BUFF_PAGES * 2) - (NUM_BUFF_PAGES / 2);
constexpr size_t NUM_BATCH_RECV = 8;

struct pair_hash {
  template <class T1, class T2>
  std::size_t operator()(const std::pair<T1, T2> &p) const {
    auto h1 = std::hash<T1>{}(p.first);
    auto h2 = std::hash<T2>{}(p.second);
    return h1 ^ h2;
  }
};

struct stat *st;
struct io_uring ring;
std::unordered_map<std::pair<int, int>,
                   std::pair<std::unordered_set<int>, size_t>, pair_hash>
    fd_to_file_idx;
std::unordered_map<int, std::unordered_set<int>> check_file_done;
struct io_uring_cqe *cqe_batch;
std::shared_timed_mutex m;
const int FILE_MODE = S_IRWXU;
const size_t QUEUE_DEPTH = 10;

bool is_special_path(const std::string &path) {
  if (path == "." || path == "..")
    return true;
  return false;
}

std::string get_last_dir(const std::string path) {
  auto last_slash_pos = path.rfind('/');
  return path.substr(last_slash_pos + 1, path.size() - last_slash_pos - 1);
}

/**
 * @brief Walk the \p src_path to create all the directories and `fallocate` all
 * the files in \p dest_path. Store the file descriptors of src file and dest
 * file in a `std::pair`. The pair is used as a key in a `unordered_map` to an
 * `unordered_set` that contains outstanding file block to be read from src file
 * to corresposing dest file
 *
 * @param src_path the root of the directory tree to be copied
 * @param dest_path the directory that the src folder is intended to be copuie
 * to
 */
void init_folder_files(const std::string &src_path,
                       const std::string &dest_path) {
  if (stat(src_path.c_str(), st) != 0) {
    return;
  }
  std::string src_last_dir = get_last_dir(src_path);
  if (is_special_path(src_last_dir))
    return;
  std::string new_dest_path = std::string(dest_path);
  new_dest_path += "/";
  new_dest_path += src_last_dir;
  if (S_ISDIR(st->st_mode)) {
    int dest_dir_fd = open(dest_path.c_str(), O_DIRECTORY);
    if (dest_dir_fd < 0) {
      perror("opening dest dir failed");
      return;
    }
    if (mkdirat(dest_dir_fd, src_last_dir.c_str(), FILE_MODE) < 0) {
      perror("making dir at dest_path failed");
    }
    DIR *src_dir = opendir(src_path.c_str());
    struct dirent *src_dir_entry = nullptr;
    close(dest_dir_fd);

    while ((src_dir_entry = readdir(src_dir)) != nullptr) {
      std::string new_src_path = std::string(src_path);
      new_src_path += '/';
      new_src_path += src_dir_entry->d_name;
      init_folder_files(new_src_path, new_dest_path);
    }
  } else if (S_ISREG(st->st_mode)) {
    size_t remaining_file_sz = st->st_size;
    int chunk_idx = 0;
    int dest_file_fd = creat(new_dest_path.c_str(), FILE_MODE);
    fallocate(dest_file_fd, 0, 0, st->st_size);
    int src_file_fd = open(src_path.c_str(), O_RDONLY);
    while (remaining_file_sz > 0) {
      fd_to_file_idx[{src_file_fd, dest_file_fd}].first.insert(chunk_idx);
      check_file_done[dest_file_fd].insert(chunk_idx);
      chunk_idx++;
      if (remaining_file_sz < PAGE_SIZE) {
        fd_to_file_idx[{src_file_fd, dest_file_fd}].second = remaining_file_sz;
        remaining_file_sz = 0;
      } else {
        remaining_file_sz -= PAGE_SIZE;
        if (remaining_file_sz == 0) {
          fd_to_file_idx[{src_file_fd, dest_file_fd}].second = 0;
        }
      }
    }

  }
}

/**
 * @brief This function is resposible for scheduling all the reads
 *
 */
void submit_read(buff_alloc &data_buff) {
  for (auto fd_it : fd_to_file_idx) {

    auto fd_pair = fd_it.first;
    auto fd_val = fd_it.second;
    auto chunk_set = fd_val.first;
    auto remainder_size = fd_val.second;
    size_t chunk_set_size = chunk_set.size();

    for (int chunk_idx : chunk_set) {
      void *page = nullptr;
    retry_submit_read: // scope for RAII lock

      page = data_buff.alloc_buff_page();
      if (page == nullptr) {
        io_uring_submit(&ring);
        goto retry_submit_read;
      }

      size_t read_len = chunk_idx == chunk_set_size - 1
                            ? remainder_size == 0 ? PAGE_SIZE : remainder_size
                            : PAGE_SIZE;
      off_t read_off = chunk_idx * PAGE_SIZE;
      struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
      io_uring_prep_read(sqe, fd_pair.first, page, read_len, read_off);
      user_data ud(fd_pair.first, fd_pair.second,
                   data_buff.get_buff_page_idx(page), chunk_idx, false, false);
      io_uring_sqe_set_data(sqe, (void *)ud.get_data());
    }
  }
}

/**
 * @brief this function is resposible popping finished reads & writes and
 * redirect reads to writes
 *
 */
void handle_write(buff_alloc *data_buff) {
  while (true /*shoud be some condition, but true for now*/) {
    int num_recvd_cqe =
        io_uring_peek_batch_cqe(&ring, &cqe_batch, NUM_BATCH_RECV);
    for (int i = 0; i < num_recvd_cqe; i++) {
      struct io_uring_cqe *current_cqe = cqe_batch + i;
      user_data ud((uint64_t)io_uring_cqe_get_data(current_cqe));
      if (!ud.read_done()) {
        /**
         * Check the return value
         *
         */
        //  std::shared_lock<std::shared_timed_mutex> read_lock(m);
        
        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        auto info = fd_to_file_idx[{ud.src_fd(), ud.dest_fd()}];
        size_t chunk_set_size = info.first.size();
        size_t remainder_size = info.second;
        int chunk_idx = ud.file_off_idx();
        size_t write_len =
            chunk_idx == chunk_set_size - 1
                ? remainder_size == 0 ? PAGE_SIZE : remainder_size
                : PAGE_SIZE;
        io_uring_prep_write(sqe, ud.dest_fd(),
                            data_buff->get_buff_page_addr(ud.buff_idx()),
                            write_len, ud.file_off_idx() * PAGE_SIZE);
        ud.read_done(true);
        io_uring_sqe_set_data(sqe, (void *)ud.get_data());
        io_uring_submit(&ring);
        io_uring_cqe_seen(&ring, current_cqe);
      } else if (!ud.write_done()) {
        /**
         *
         *
         */
        //  std::shared_lock<std::shared_timed_mutex> read_lock(m)
        // struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

        // } else {
        /**
         * closing file decriptors, return the data buffers. Remove block num
         * from correspoding fd pairs if the fd map is empty, great success not
         * implementing retry for mow
         */
        assert(current_cqe->res >=0);
        io_uring_cqe_seen(&ring, current_cqe);
        // {
          // sizhan::write_lock lock(m);  
        data_buff->release_buff_page_by_idx(ud.buff_idx());
        // }
        if (check_file_done.find(ud.dest_fd()) != check_file_done.end()) {
          std::unordered_set<int> file_offset_set =
              check_file_done[ud.dest_fd()];
          file_offset_set.erase(ud.file_off_idx());
          if (file_offset_set.empty()) {
            close(ud.src_fd());
            close(ud.dest_fd());
          }
          check_file_done.erase(ud.dest_fd());
          if (check_file_done.empty())
            return;
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  // std::string test = "test";
  // printf("%s\n", test.c_str());
  if (argc != 3) {
    puts("cpr: invalid argument number");
    puts("Usage: cpr src_folder dest_folder");
    return EXIT_FAILURE;
  }

  st = new struct stat;

  std::string src_abs_path = std::string(argv[1]);
  std::string dest_abs_path = std::string(argv[2]);
  // file_info *file_infos = new file_info[10];
  buff_alloc data_buff(HALF_GIG);

  if (stat(dest_abs_path.c_str(), st) != 0) {
    if (mkdir(dest_abs_path.c_str(), FILE_MODE) != 0) {
      perror("Failed to make top-level dest directory!");
      // goto error;
      exit(-1);
    }
  }

  // init liburing stuff......
  io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
  // for (int i = 0; i < 10; i++) {
  //   free_file_infos.push_back(file_infos + i);
  // }

  // allocate the big buffer for copying data, and all those info into a free
  // list

  init_folder_files(src_abs_path, dest_abs_path);
  cqe_batch = new struct io_uring_cqe[NUM_BUFF_PAGES];

  submit_read(data_buff); 
  io_uring_submit(&ring);

  std::thread handle_write_thread(handle_write, &data_buff);
  handle_write_thread.join();

  delete st;
  io_uring_queue_exit(&ring);
  // delete[] cqe_batch;

  return EXIT_SUCCESS;

  // error:
  //   delete st;
  //   io_uring_queue_exit(&ring);
  //   delete[] file_infos;

  //   return EXIT_FAILURE;
}