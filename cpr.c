#include <fcntl.h>
#include <linux/aio_abi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "cpr.h"

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