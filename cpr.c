#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <linux/io_uring.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "io_uring.h"

const int FILE_MODE = S_IRWXU;
struct stat sb;

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

/**
 * @brief
 *
 * @param src_path the path of the folder that we want to replicate
 * @param dest_path the path that we should replicate the folder to
 */
void create_folders(char *src_path, char *dest_path) {
  if (stat(src_path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    int dest_dir_fd = open(dest_path, O_DIRECTORY);
    if (dest_dir_fd < 0) {
      perror("opening dest dir failed");
      return;
    }
    char *src_last_dir = get_last_dir(src_path);
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
    goto fail;
  }

  // char *ret = get_last_dir(src_abs_path);
  // printf("%s\n", ret);

  // free(ret);
  create_folders(src_abs_path, dest_abs_path);

  // //open destination directory
  // int dest_dir_fd = open(dest_path, O_DIRECTORY);
  // if (dest_dir_fd == -1) {
  //   perror("cannot open dest_dir");
  //   goto fail_with_path;
  // }
  // //use mkdirat
  // mkdirat(dest_dir_fd, src_path, FILE_MODE);

  free(src_abs_path);
  free(dest_abs_path);

  return EXIT_SUCCESS;

fail_with_path:

  free(src_abs_path);
  free(dest_abs_path);

fail:

  return EXIT_FAILURE;
}