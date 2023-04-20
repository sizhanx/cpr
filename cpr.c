#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <liburing.h>

#define QUEUE_DEPTH   8
#define BLOCK_SZ      131072
#define MAX_NUM_FILES 65536

const int FILE_MODE = S_IRWXU;
struct stat sb;

struct io_uring ring;

uint32_t num_submitted = 0;
uint32_t num_completed = 0;

typedef struct file_info file_info;

struct file_info {
    off_t offset;
    file_info* next;
    uint32_t is_write;
    uint32_t read_fd_index;
    uint32_t write_fd_index;
    uint32_t num_blocks;
    struct iovec iovecs[];      /* Referred by readv/writev */
};

void *main_buffer;
uint32_t num_buffers_allocated = 0;

struct linked_list {
  struct file_info *head;
  struct file_info *tail;
};

struct linked_list read_list;
int *fd_arr;
uint32_t next_index = 0;

void add_to_list(struct file_info *item) {
  if (read_list.head == NULL) {
    read_list.head = item;
    read_list.tail = item;
  } else {
    struct file_info *second_to_last = read_list.tail;
    second_to_last->next = item;
    read_list.tail = item;
  }
}

struct file_info *remove_from_list() {
  if (read_list.head == NULL) {
    return NULL;
  } else if (read_list.head == read_list.tail) {
    struct file_info *item = read_list.head;
    read_list.head = NULL;
    read_list.tail = NULL;
    return item;
  } else {
    struct file_info *item = read_list.head;
    read_list.head = item->next;
    item->next = NULL;
    return item;
  }
}

void *get_buffer() {
  if (num_buffers_allocated < QUEUE_DEPTH) {
    void *buf = main_buffer + (BLOCK_SZ * num_buffers_allocated);
    num_buffers_allocated++;
    return buf;
  }
  return NULL;
}

char *strcpy_m(const char *src) {
  char *temp = malloc(strlen(src));
  if (temp == NULL) {
    perror("failed to allocate memory to copy string");
    return temp;
  }
  strcpy(temp, src);
  return temp;
}


char *get_last_dir(char *path) {
  size_t len = strlen(path);
  int slash_pos = len - 1;
  while (path[slash_pos] != '/') {
    slash_pos--;
  }
  size_t last_dir_size = len - slash_pos;
  char *last_dir = malloc(last_dir_size);
  if (last_dir == NULL) {
    perror("Fail to allcoate memory for last_dir");
    return NULL;
  }
  last_dir[last_dir_size - 1] = 0;
  strcpy(last_dir, path + slash_pos + 1);
  return last_dir;
}

void print_sq_poll_kernel_thread_status() {
    if (system("ps --ppid 2 | grep io_uring-sq" ) == 0)
        printf("Kernel thread io_uring-sq found running...\n");
    else
        printf("Kernel thread io_uring-sq is not running.\n");
}

int submit_read_request(char *file_path, size_t size, int dest_fd_index) {
  int file_fd = open(file_path, O_RDONLY);
  if (file_fd < 0) {
      perror("open");
      return 1;
  }
  uint32_t file_fd_index = next_index;
  fd_arr[file_fd_index] = file_fd;
  next_index++;
  off_t file_sz = size;
  off_t bytes_remaining = file_sz;
  off_t offset = 0;
  int current_block = 0;
  int blocks = (int) file_sz / BLOCK_SZ;
  if (file_sz % BLOCK_SZ) blocks++;
  // struct file_info *fi = malloc(sizeof(*fi) +
  //                                       (sizeof(struct iovec) * blocks));
  // char *buff = malloc(file_sz);
  // if (!buff) {
  //     fprintf(stderr, "Unable to allocate memory.\n");
  //     return 1;
  // }

  while (bytes_remaining) {
      off_t bytes_to_read = bytes_remaining;
      if (bytes_to_read > BLOCK_SZ)
          bytes_to_read = BLOCK_SZ;


      struct file_info *fi = (malloc(sizeof(*fi) + sizeof(struct iovec)));

      fi->offset = offset;
      fi->is_write = 0;
      fi->read_fd_index = file_fd_index;
      fi->write_fd_index = dest_fd_index;
      fi->num_blocks = blocks;
      fi->next = NULL;
      fi->iovecs[0].iov_len = bytes_to_read;

      void *buf = get_buffer();
      if (buf == NULL) {
        add_to_list(fi);
      } else {
        fi->iovecs[0].iov_base = buf;

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (sqe == NULL) {
          perror("could not allocate sqe");
        }
        io_uring_prep_readv(sqe, fi->read_fd_index, fi->iovecs, 1, fi->offset);
        io_uring_sqe_set_flags(sqe, IOSQE_FIXED_FILE);
        io_uring_sqe_set_data(sqe, fi);
        // io_uring_submit(&ring);
      }
      num_submitted++;
      // fi->iovecs[current_block].iov_len = bytes_to_read;
      // void *buf;
      // if( posix_memalign(&buf, BLOCK_SZ, BLOCK_SZ)) {
      //     perror("posix_memalign");
      //     return 1;
      // }
      // fi->iovecs[current_block].iov_base = buf;
      offset += bytes_to_read;
      current_block++;
      bytes_remaining -= bytes_to_read;
  }

  return 0;
}

int submit_write_request(int dest_fd_index, struct file_info *data) {
  struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);

  data->is_write = 1;

  io_uring_prep_writev(sqe, dest_fd_index, data->iovecs, 1, data->offset);
  io_uring_sqe_set_flags(sqe, IOSQE_FIXED_FILE);
  io_uring_sqe_set_data(sqe, data);
  io_uring_submit(&ring);

  return 0;
}

/**
 * @brief
 *
 * @param src_path the path of the folder that we want to replicate
 * @param dest_path the path that we should replicate the folder to
 */
void create_folders(char *src_path, char *dest_path) {
  if (stat(src_path, &sb) != 0 ) {
    return;
  }
  char *src_last_dir = get_last_dir(src_path);
  if (S_ISDIR(sb.st_mode)) {
    int dest_dir_fd = open(dest_path, O_DIRECTORY);
    if (dest_dir_fd < 0) {
      perror("opening dest dir failed");
      return;
    }
    if (strcmp(src_last_dir, ".") == 0 || strcmp(src_last_dir, "..") == 0) return;
    if (mkdirat(dest_dir_fd, src_last_dir, FILE_MODE) < 0) {
      perror("making dir at dest_path failed");
    }
    DIR *src_dir = opendir(src_path);
    struct dirent *src_dir_entry = NULL;
    close(dest_dir_fd);
    char *new_dest_path = malloc(strlen(src_last_dir) + strlen(dest_path) + 2);
    strcpy(new_dest_path, dest_path);
    strcpy(new_dest_path + strlen(dest_path), "/");
    strcpy(new_dest_path  + strlen(dest_path) + 1, src_last_dir);
    while ((src_dir_entry = readdir(src_dir)) != NULL) {
      
      char *new_src_path =
          malloc(strlen(src_path) + strlen(src_dir_entry->d_name) + 2);
      strcpy(new_src_path, src_path);
      strcpy(new_src_path + strlen(src_path), "/");
      strcpy(new_src_path  + strlen(src_path) + 1, src_dir_entry->d_name);
      create_folders(new_src_path, new_dest_path);
      free(new_src_path);
    }
    free(new_dest_path);
    free(src_last_dir);
  } else if (S_ISREG(sb.st_mode)) {
    char* dest_file_path = malloc(strlen(dest_path) + strlen(src_last_dir) + 2);
    strcpy(dest_file_path, dest_path);
    strcpy(dest_file_path + strlen(dest_path), "/");
    strcpy(dest_file_path + strlen(dest_path) + 1, src_last_dir);
    // int creat_file_fd = creat(dest_file_path, FILE_MODE);
    int creat_file_fd = open(dest_file_path, O_WRONLY | O_CREAT | O_TRUNC, sb.st_mode);
    fallocate(creat_file_fd, 0, 0, sb.st_size);

    uint32_t dest_fd_index = next_index;
    fd_arr[dest_fd_index] = creat_file_fd;
    next_index++;
    submit_read_request(src_path, sb.st_size, dest_fd_index);

    free(dest_file_path);
    // close(creat_file_fd);
  }
}

int main(int argc, char *argv[]) {
  // printf("this is a test\n");
  // app_setup_uring(NULL);
  if (argc != 3) {
    puts("cpr: invalid argument number");
    puts("Usage: cpr src_folder dest_folder");
  }
  char *src_abs_path = realpath(argv[1], NULL);
  char *dest_abs_path = realpath(argv[2], NULL);
  if (src_abs_path == NULL || dest_abs_path == NULL) {
    fprintf(stderr, "invalid path\n");
    return EXIT_FAILURE;
  }

  main_buffer = malloc(QUEUE_DEPTH * BLOCK_SZ);
  if (main_buffer == NULL) {
    fprintf(stderr, "malloc failed to allocate main buffer\n");
    return EXIT_FAILURE;
  }
  memset(main_buffer, 0, QUEUE_DEPTH * BLOCK_SZ);

  read_list.head = NULL;
  read_list.tail = NULL;

  fd_arr = (int *) mmap(NULL, sizeof(int) * MAX_NUM_FILES, 
                        PROT_READ | PROT_WRITE, 
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (fd_arr == NULL) {
    perror("mmap failed");
    return 1;
  }

  struct io_uring_params params;
  memset(&params, 0, sizeof(params));
  params.flags |= IORING_SETUP_SQPOLL;
  params.sq_thread_idle = 2000;
  io_uring_queue_init_params(QUEUE_DEPTH * 2, &ring, &params);

  create_folders(src_abs_path, dest_abs_path);

  int ret = io_uring_register_files(&ring, fd_arr, MAX_NUM_FILES);
  if(ret) {
      fprintf(stderr, "Error registering buffers: %s", strerror(-ret));
      return 1;
  }
  io_uring_submit(&ring);


  while (num_submitted > num_completed) {
    struct io_uring_cqe *cqe;
    int ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
      perror("io_uring_wait_cqe");
      return 1;
    }
    if (cqe->res < 0) {
      fprintf(stderr, "Async operation failed.\n");
      return 1;
    }
    struct file_info *fi = io_uring_cqe_get_data(cqe);
    io_uring_cqe_seen(&ring, cqe);

    if (fi->is_write) {
      // printf("write completed\n\n");
      fflush(stdout);
      num_completed++;
      // close(fi->write_fd);
      struct file_info* next_read = remove_from_list();
      if (next_read != NULL) {
        next_read->iovecs[0].iov_base = fi->iovecs[0].iov_base;
        memset(next_read->iovecs[0].iov_base, 0, BLOCK_SZ);

        struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
        if (sqe == NULL) {
          fprintf(stderr, "failed to get sqe\n");
          return 1;
        }
        io_uring_prep_readv(sqe, next_read->read_fd_index, next_read->iovecs, 1, next_read->offset);
        io_uring_sqe_set_flags(sqe, IOSQE_FIXED_FILE);
        io_uring_sqe_set_data(sqe, next_read);

        io_uring_submit(&ring);
      }
      free(fi);
    } else {
      // printf("%.100s\n\n", (char *) (fi->iovecs[0].iov_base));
      fflush(stdout);
      submit_write_request(fi->write_fd_index, fi);
    }

  }

  io_uring_queue_exit(&ring);

  free(src_abs_path);
  free(dest_abs_path);
  free(main_buffer);
  return EXIT_SUCCESS;
}
