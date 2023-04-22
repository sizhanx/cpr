#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <dirent.h>
#include <fcntl.h>
#include <liburing.h>
#include <memory>
#include <shared_mutex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "buff_alloc.hpp"
#include "common.hpp"

const int FILE_MODE = S_IRWXU;
const unsigned IO_URING_QUEUE_DEPTH = 100;
const unsigned IO_URING_FLAGS = 0;
const unsigned IO_URING_CQE_PEEK_COUNT = 100;
constexpr size_t TWO_GB = GB << 1;
constexpr size_t MB_256 = GB >> 2;


struct cp_task {
  void *buff_addr;
  const off_t file_off;
  const size_t op_len;
  const int src_fd;
  const int dest_fd;
  bool read_done;
  cp_task() : file_off(0), op_len(0), src_fd(-1), dest_fd(-1) {}
  cp_task(int src_fd, int dest_fd, off_t file_off, size_t op_len)
      : buff_addr(nullptr), file_off(file_off), op_len(op_len), src_fd(src_fd),
        dest_fd(dest_fd), read_done(false) {}
  void set_read_done(bool val) { this->read_done = val; }
};

std::shared_timed_mutex mtx;

bool is_special_path(const std::string &path) {
  if (path == "." || path == "..")
    return true;
  return false;
}

std::string get_last_dir(const std::string path) {
  auto last_slash_pos = path.rfind('/');
  return path.substr(last_slash_pos + 1, path.size() - last_slash_pos - 1);
}

void init_folder_files(const std::string &src_path,
                       const std::string &dest_path,
                       std::vector<cp_task *> &cp_tasks,
                       std::deque<cp_task *> &read_queue,
                       std::unordered_map<int, size_t> &check_done) {
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
      init_folder_files(new_src_path, new_dest_path, cp_tasks, read_queue,
                        check_done);
    }
    closedir(src_dir);
  } else if (S_ISREG(st.st_mode)) {
    size_t remaining_file_sz = st.st_size;
    int dest_file_fd = creat(new_dest_path.c_str(), FILE_MODE);
    fallocate(dest_file_fd, 0, 0, st.st_size);
    int src_file_fd = open(src_path.c_str(), O_RDONLY);
    while (remaining_file_sz > 0) {
      struct cp_task *task =
          new struct cp_task(src_file_fd, dest_file_fd,
                             st.st_size - remaining_file_sz, remaining_file_sz);
      cp_tasks.push_back(task);
      read_queue.push_back(task);
      check_done[dest_file_fd]++;
      if (remaining_file_sz < PAGE_SIZE) {
        remaining_file_sz = 0;
      } else {
        remaining_file_sz -= PAGE_SIZE;
      }
    }
  }
}

void sqe_submitter_worker(buff_alloc &buff_allocator, struct io_uring *ring,
                          std::deque<struct cp_task *> &read_queue,
                          std::deque<struct cp_task *> &write_queue,
                          std::unordered_map<int, size_t> &check_done) {
  // return;
  // sizhan::write_lock l(mtx);
  while (!check_done.empty()) {
    if (!write_queue.empty()) {
      struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
      if (sqe == nullptr) {
        io_uring_submit(ring);
        continue;
      }
      struct cp_task *task_ptr = nullptr;
      {
        sizhan::write_lock l(mtx);
        task_ptr = write_queue.front();
        write_queue.pop_front();
      }

      io_uring_prep_write(sqe, task_ptr->dest_fd, task_ptr->buff_addr,
                          task_ptr->op_len, task_ptr->file_off);
      io_uring_sqe_set_data(sqe, task_ptr);

      if (write_queue.empty())
        io_uring_submit(ring);
    }
    if (!read_queue.empty()) {
      void *page = buff_allocator.alloc_buff_page();
      if (page == nullptr) {
        io_uring_submit(ring);
        continue;
      }
      struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
      if (sqe == nullptr) {
        buff_allocator.relese_buff_page(page);
        io_uring_submit(ring);
        continue;
      }
      auto task_ptr = read_queue.front();
      task_ptr->buff_addr = page;
      assert(!task_ptr->read_done);
      read_queue.pop_front();

      io_uring_prep_read(sqe, task_ptr->src_fd, page, task_ptr->op_len,
                         task_ptr->file_off);
      io_uring_sqe_set_data(sqe, task_ptr);

      if (read_queue.empty())
        io_uring_submit(ring);
    }
  }
}

void cqe_handler_worker(struct io_uring_cqe **cqe_buff, struct io_uring *ring,
                        std::unordered_map<int, size_t> &check_done,
                        std::deque<struct cp_task *> &write_queue, buff_alloc& buff_allocator) {
  // int* b = (int*) 0x99999;
  // int a = *((int*) b - 0x99999);

  while (!check_done.empty()) {
    int num_cqes =
        io_uring_peek_batch_cqe(ring, cqe_buff, IO_URING_CQE_PEEK_COUNT);
    for (int i = 0; i < num_cqes; i++) {
      struct io_uring_cqe *current_cqe = cqe_buff[i];
      struct cp_task *task_ptr =
          (struct cp_task *)io_uring_cqe_get_data(current_cqe);
      // break;
      if (task_ptr->read_done) {
        int dest_fd = task_ptr->dest_fd;
        check_done[dest_fd]--;
        buff_allocator.relese_buff_page(task_ptr->buff_addr);
        if (check_done[dest_fd] == 0) {
          check_done.erase(dest_fd);
          close(dest_fd);
          close(task_ptr->src_fd);
        }
      } else {
        sizhan::write_lock l(mtx);
        task_ptr->set_read_done(true);
        write_queue.push_front(task_ptr);
      }
      io_uring_cqe_seen(ring, current_cqe);
    }
  }
  // printf("worker function is actually done\n");
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    puts("cpr: invalid argument number");
    puts("Usage: cpr src_folder dest_folder");
    return EXIT_FAILURE;
  }

  std::string src_path = std::string(argv[1]);
  std::string dest_path = std::string(argv[2]);

  struct stat
      st; // this stat struct is only used for checking top_level dest folder

  std::vector<struct cp_task *> cp_tasks;
  std::deque<struct cp_task *> read_queue;
  std::deque<struct cp_task *> write_queue;
  std::unordered_map<int, size_t> check_done;
  struct io_uring *ring;

  ring = new struct io_uring;

  if (stat(dest_path.c_str(), &st) != 0) {
    if (mkdir(dest_path.c_str(), FILE_MODE) != 0) {
      perror("Failed to make top-level dest directory!");
      // goto error;
      exit(-1);
    }
  }

  init_folder_files(src_path, dest_path, cp_tasks, read_queue, check_done);

  io_uring_queue_init(IO_URING_QUEUE_DEPTH, ring, 0);

  buff_alloc buff_allocator(MB_256);

  struct io_uring_cqe **cqe_buff = (struct io_uring_cqe **)malloc(
      sizeof(struct io_uring_cqe *) * IO_URING_CQE_PEEK_COUNT);
  // new struct io_uring_cqe*[IO_URING_CQE_PEEK_COUNT];

  // struct io_uring_cqe **double_cqe_buff = &cqe_buff;

  // auto t = std::thread(sqe_submitter_worker, std::ref(buff_allocator), ring,
  // std::ref(read_queue), std::ref(write_queue),
  //                      std::ref(check_done));

  auto t = std::thread(cqe_handler_worker, cqe_buff, ring, std::ref(check_done),
                       std::ref(write_queue), std::ref(buff_allocator));

  sqe_submitter_worker(buff_allocator, ring, read_queue, write_queue,
                       check_done);

  t.join();

  for (struct cp_task *t : cp_tasks) {
    delete t;
  }

  delete ring;
  free(cqe_buff);
  io_uring_queue_exit(ring);

  return EXIT_SUCCESS;
}