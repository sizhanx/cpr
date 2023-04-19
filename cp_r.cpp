// #define _GNU_SOURCE
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <liburing.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <cassert>

#include "buff_alloc.hpp"
#include "user_data.hpp"

const uint64_t ONE = 1;
const int FILE_MODE = S_IRWXU;

constexpr size_t DATA_BUFF_SIZE = ONE << 29;

const unsigned IO_URING_QUEUE_DEPTH = 100;
const unsigned IO_URING_FLAGS = 0;
const unsigned IO_URING_CQE_PEEK_COUNT = 100;

struct pair_hash {
  template <class T1, class T2>
  std::size_t operator()(const std::pair<T1, T2> &p) const {
    auto h1 = std::hash<T1>{}(p.first);
    auto h2 = std::hash<T2>{}(p.second);
    return h1 ^ h2;
  }
};

std::unordered_map<std::pair<int, int>,
                     std::pair<std::unordered_set<int>, size_t>, pair_hash>
      fd_to_file_idx;
  std::unordered_map<int, std::unordered_set<int>> check_file_done;

struct io_uring_cqe** cqe_buff = nullptr;

bool is_special_path(const std::string &path) {
  if (path == "." || path == "..")
    return true;
  return false;
}

std::string get_last_dir(const std::string path) {
  auto last_slash_pos = path.rfind('/');
  return path.substr(last_slash_pos + 1, path.size() - last_slash_pos - 1);
}

void init_folder_files(
    const std::string &src_path, const std::string &dest_path) {
  struct stat st;
  if (stat(src_path.c_str(), &st) != 0) {
    return;
  }
  std::string src_last_dir = get_last_dir(src_path);
  if (is_special_path(src_last_dir))
    return;
  std::string new_dest_path = std::string(dest_path);
  new_dest_path += "/";
  new_dest_path += src_last_dir;
  if (S_ISDIR(st.st_mode)) {
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
    closedir(src_dir);
  } else if (S_ISREG(st.st_mode)) {
    size_t remaining_file_sz = st.st_size;
    int chunk_idx = 0;
    int dest_file_fd = creat(new_dest_path.c_str(), FILE_MODE);
    fallocate(dest_file_fd, 0, 0, st.st_size);
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

void submit_read(buff_alloc &data_buff, struct io_uring *ring,
                 size_t& read_submitted) {

  for (auto fd_it : fd_to_file_idx) {

    auto fd_pair = fd_it.first;
    auto fd_val = fd_it.second;
    auto chunk_set = fd_val.first;
    auto remainder_size = fd_val.second;
    size_t chunk_set_size = chunk_set.size();

    for (int chunk_idx : chunk_set) {
      void *page = nullptr;
      struct io_uring_sqe *sqe = nullptr;
    retry_submit_read:

      page = data_buff.alloc_buff_page();
      if (page == nullptr) {
        goto retry_submit_read;
      }
      sqe = io_uring_get_sqe(ring);
      if (sqe == nullptr) {
        goto retry_submit_read;
      }

      size_t read_len = chunk_idx == chunk_set_size - 1
                            ? remainder_size == 0 ? PAGE_SIZE : remainder_size
                            : PAGE_SIZE;
      off_t read_off = chunk_idx * PAGE_SIZE;
      io_uring_prep_read(sqe, fd_pair.first, page, read_len, read_off);
      // printf("fd pair in submit read: {%d, %d}\n", fd_pair.first,
      //        fd_pair.second);
      user_data ud(fd_pair.first, fd_pair.second,
                   data_buff.get_buff_page_idx(page), chunk_idx, false, false);
      io_uring_sqe_set_data(sqe, (void *)ud.get_data());
      read_submitted++;
    }
  }
}

void handle_write(
    buff_alloc *data_buff, struct io_uring *ring,  size_t total_reads) {
  size_t num_writes_issued;
  struct io_uring_sqe *sqe = nullptr;
  while (true /*shoud be some condition, but true for now*/) {
    
    int num_recvd_cqe =
        io_uring_peek_batch_cqe(ring, cqe_buff, IO_URING_CQE_PEEK_COUNT);
    for (int i = 0; i < num_recvd_cqe; i++) {
      struct io_uring_cqe *current_cqe = cqe_buff[i];
      user_data ud((uint64_t)io_uring_cqe_get_data(current_cqe));
      if (!ud.read_done()) {
        // printf("receiving read completion: %d\n", ud.dest_fd());
        sqe = io_uring_get_sqe(ring);
        if (sqe == nullptr) {
          perror("stuck in getting sqe for writes");
          io_uring_submit(ring);
          continue;
        }
        auto info = fd_to_file_idx[{ud.src_fd(), ud.dest_fd()}];
        size_t chunk_set_size = info.first.size();
        size_t remainder_size = info.second;
        int chunk_idx = ud.file_off_idx();
        size_t write_len =
            chunk_idx == chunk_set_size - 1
                ? remainder_size == 0 ? PAGE_SIZE : remainder_size
                : PAGE_SIZE;
        // printf("prepping write for %d\n", ud.dest_fd());
        io_uring_prep_write(sqe, ud.dest_fd(),
                            data_buff->get_buff_page_addr(ud.buff_idx()),
                            write_len, ud.file_off_idx() * PAGE_SIZE);
        ud.read_done(true);
        io_uring_sqe_set_data(sqe, (void *)ud.get_data());
        io_uring_cqe_seen(ring, current_cqe);
        num_writes_issued++;
        // printf("total reads: %lu, num_writes_issued: %lu\n",total_reads, num_writes_issued);
        if (total_reads == num_writes_issued)
          io_uring_submit(ring);
      } else if (!ud.write_done()) {

        assert(current_cqe->res >= 0);
        io_uring_cqe_seen(ring, current_cqe);
        // {
        // sizhan::write_lock lock(m);
        data_buff->release_buff_page_by_idx(ud.buff_idx());
        // }
        if (check_file_done.find(ud.dest_fd()) != check_file_done.end()) {
          std::unordered_set<int> file_offset_set =
              check_file_done[ud.dest_fd()];
          // printf("completing %lu for file %d\n", ud.file_off_idx(),
          //        ud.dest_fd());
          file_offset_set.erase(ud.file_off_idx());
          if (file_offset_set.empty()) {
            close(ud.src_fd());
            close(ud.dest_fd());
          }
          // printf("closing dest file: %d\n", ud.dest_fd());
          check_file_done.erase(ud.dest_fd());
          if (check_file_done.empty())
            return;
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    puts("cpr: invalid argument number");
    puts("Usage: cpr src_folder dest_folder");
    return EXIT_FAILURE;
  }

  std::string src_abs_path = std::string(argv[1]);
  std::string dest_abs_path = std::string(argv[2]);

  struct stat
      st; // this stat struct is only used for checking top_level dest folder
  struct io_uring* ring;
  size_t read_submitted = 0;

  if (stat(dest_abs_path.c_str(), &st) != 0) {
    if (mkdir(dest_abs_path.c_str(), FILE_MODE) != 0) {
      perror("Failed to make top-level dest directory!");
      // goto error;
      exit(-1);
    }
  }

  init_folder_files(src_abs_path, dest_abs_path);

  ring = new struct io_uring;

  io_uring_queue_init(IO_URING_QUEUE_DEPTH, ring, IO_URING_FLAGS);

  // cqe_buff = new struct io_uring_cqe[IO_URING_CQE_PEEK_COUNT];
  cqe_buff = (struct io_uring_cqe**) malloc(sizeof(struct io_uring_cqe*) * IO_URING_CQE_PEEK_COUNT);

  buff_alloc data_buff(DATA_BUFF_SIZE);

  submit_read(data_buff, ring, read_submitted);
  io_uring_submit(ring);

  // struct io_uring_sqe* s = nullptr;
  // while ((s = io_uring_get_sqe(ring)) != nullptr) {
  //   printf("getting sqe!\n");
  // }
  // io_uring_get_sqe(ring);

  std::thread t(handle_write, &data_buff, ring, read_submitted);

  t.join();

  // std::thread te(test, &data_buff, ring, cqe_buff, &fd_to_file_idx, &check_file_done,read_submitted);
  // te.join();

  io_uring_queue_exit(ring);

  delete ring;
  // delete[] cqe_buff;
  free(cqe_buff);
  return EXIT_SUCCESS;
}