#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/aio_abi.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

// #include "cpr.h"
#include <libaio.h>
#include "vec.h"

const size_t MB = 1 << 20;
const size_t MB32 = MB << 5;

char *strcpy_m(const char *src) {
  char *temp = malloc(strlen(src));
  if (temp == NULL) {
    perror("failed to allocate memory to copy string");
  }
  strcpy(temp, src);
  return temp;
}

/**
 * @brief This is the main fucntion, which should parse the commandline, and
 * call the routine
 *
 * @param argc 3 and errors otherwise
 * @param argv should be program name, dir to be copied, and dest dir
 * @return int
 */
int main(int argc, char *argv[]) {

  size_t nevents = 1;
  struct io_event events[nevents];

  if (argc != 3) {
    fprintf(stderr, "Incorrect arg number!\n"
                    "Usage: cpr src_path dest_path\n");
    goto fail;
  }

  char *src_path = strcpy_m(argv[1]);
  char *dest_path = strcpy_m(argv[2]);

  aio_context_t ioctx = 0;
  unsigned maxevents = 128;

  if (io_setup(maxevents, &ioctx) < 0) {
    perror("io_setup");
    goto fail_with_path;
  }

  char buff[512];

  int fd = open("cpr.c", O_RDONLY );

  struct iocb iocb1 = {0};
  iocb1.aio_data = 0xbeef;
  iocb1.aio_buf = (__u64)buff;
  iocb1.aio_fildes = fd;
  iocb1.aio_lio_opcode = IOCB_CMD_PREAD;
  iocb1.aio_nbytes = sizeof(buff);
  iocb1.aio_offset = 0;
  iocb1.aio_reqprio = 0;

  iocb_vec *v = vec_init();

  vec_add(v, &iocb1);

  io_submit(ioctx, 1, v->arr);

  struct timespec t = {
    .tv_sec = 0,
    .tv_nsec = 200000000,
  };


  while (true) {
    int ret = io_getevents(ioctx, 1 /* min */, nevents, events, &t);
    if (ret < 0) {
      perror("io_getevents");
      exit(1);
    }

    printf("the ret is %d\n", ret);

    struct io_event *ev = &events[0];
    assert(ev->data == 0xbeef || ev->data == 0xbaba);
    if (ev->res >= 0) {
      buff[511] = 0;
      printf("%s", buff);
      break;
    } else {
      printf("error code is: %d\n", ev->res);
    }
  }

  if (io_destroy(ioctx) != 0) {
    perror("io_destroy");
    goto fail_with_path;
  }

  free(src_path);
  free(dest_path);

  return EXIT_SUCCESS;

fail_with_path:

  free(src_path);
  free(dest_path);

fail:

  return EXIT_FAILURE;
}