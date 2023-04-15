#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct stat *st;
const int FILE_MODE = S_IRWXU;

bool is_special_path(const std::string &path) {
  if (path == "." || path == "..")
    return true;
  return false;
}

std::string get_last_dir(const std::string path) {
  auto last_slash_pos = path.rfind('/');
  return path.substr(last_slash_pos + 1, path.size() - last_slash_pos - 1);
}

void init_folders_files(const std::string &src_path,
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
    struct dirent *src_dir_entry = NULL;
    close(dest_dir_fd);

    while ((src_dir_entry = readdir(src_dir)) != NULL) {
      std::string new_src_path = std::string(src_path);
      new_src_path += '/';
      new_src_path += src_dir_entry->d_name;
      init_folders_files(new_src_path, new_dest_path);
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
  }

  st = new struct stat;

  std::string src_abs_path = std::string(argv[1]);
  std::string dest_abs_path = std::string(argv[2]);

  if (stat(dest_abs_path.c_str(), st) != 0) {
    if (mkdir(dest_abs_path.c_str(), FILE_MODE) != 0) {
      perror("Failed to make top-level dest directory!");
    }
  }

  init_folders_files(src_abs_path, dest_abs_path);

  delete st;
}