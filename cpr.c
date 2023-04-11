#define _GNU_SOURCE           
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/io_uring.h>
#include <dirent.h> 

#include "io_uring.h"

const int FILE_MODE = S_IRWXU;

char *strcpy_m(const char *src) {
  char *temp = malloc(strlen(src));
  if (temp == NULL) {
    perror("failed to allocate memory to copy string");
    return temp;
  }
  strcpy(temp, src);
  return temp;
}

int main(int argc, char* argv[]) {
  // printf("this is a test\n");
  // app_setup_uring(NULL);
  if (argc != 3) {
    puts("cpr: invalid argument number");
    puts("Usage: cpr src_folder dest_folder");
  }

  char *src_path = strcpy_m(argv[1]);
  char *dest_path = strcpy_m(argv[2]);

  if (src_path == NULL || dest_path == NULL) {
    goto fail;
  }

  //open destination directory
  int dest_dir_fd = open(dest_path, O_DIRECTORY);
  if (dest_dir_fd == -1) {
    perror("cannot open dest_dir");
    goto fail_with_path;
  }
  //use mkdirat
  mkdirat(dest_dir_fd, src_path, FILE_MODE);


  free(src_path);
  free(dest_path);

  return EXIT_SUCCESS;

fail_with_path:

  free(src_path);
  free(dest_path);

fail:

  return EXIT_FAILURE;
}