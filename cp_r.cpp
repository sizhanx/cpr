#include "buf_alloc.hpp"
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <liburing.h>
#include <unordered_map>
#include <unordered_set>
#include <deque>

constexpr size_t HALF_GIG = 1 << 29;

struct file_info{
  off_t file_sz = 0;
  void* buf = nullptr;
  size_t nbytes = 0;
  int write_fd = -1;
  bool is_write = false;
};


struct stat *st;
struct io_uring ring;
std::unordered_map<int, std::unordered_set<int>> src_fd_to_buf_idx;
std::deque<file_info*> free_file_infos;
std::deque<void*> free_buffer_infos;
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

void do_copy(const std::string &src_path,
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
      do_copy(new_src_path, new_dest_path);
    }
  } else if (S_ISREG(st->st_mode)) {
    int dest_file_fd = creat(new_dest_path.c_str(), FILE_MODE);
    fallocate(dest_file_fd, 0, 0, st->st_size);
    close(dest_file_fd);
  }
}

int main(int argc, char *argv[]) {
  std::string test = "test";
  printf("%s\n", test.c_str());
  if (argc != 3) {
    puts("cpr: invalid argument number");
    puts("Usage: cpr src_folder dest_folder");
    return EXIT_FAILURE;
  }

  st = new struct stat;

  std::string src_abs_path = std::string(argv[1]);
  std::string dest_abs_path = std::string(argv[2]);
  file_info* file_infos = new file_info[10];
  buff_alloc data_buff(HALF_GIG);

  if (stat(dest_abs_path.c_str(), st) != 0) {
    if (mkdir(dest_abs_path.c_str(), FILE_MODE) != 0) {
      perror("Failed to make top-level dest directory!");
      goto error;
    }
  }

  // init liburing stuff......
  io_uring_queue_init(QUEUE_DEPTH, &ring, 0);
  for (int i = 0;i < 10;i++) {
    free_file_infos.push_back(file_infos + i);
  }

  // allocate the big buffer for copying data, and all those info into a free list

  do_copy(src_abs_path, dest_abs_path);

  delete st;
  io_uring_queue_exit(&ring);
  delete[] file_infos;

  return EXIT_SUCCESS;

error:
  delete st;
  io_uring_queue_exit(&ring);
  delete[] file_infos;

  return EXIT_FAILURE;
}