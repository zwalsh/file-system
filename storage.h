#ifndef NUFS_STORAGE_H
#define NUFS_STORAGE_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "slist.h"

void storage_init(const char* path);
int         get_stat(const char* path, struct stat* st);
const char* get_data(const char* path);
slist* get_filenames_from_dir(const char* path);
int create_dir(const char* path);
// should this include rdev from mknod??
int create_inode_at_path(const char* path, mode_t mode);
int truncate(const char* path, off_t size);
int read_file(const char* path, char* buf, size_t size, off_t offset_in_file);
int write_file(const char* path, const char* buf, size_t size, off_t offset_in_file);
int link_file(const char* path_old, const char* path_new);
int unlink_file(const char* path);
int rename_file(const char* from, const char* to);
int remove_dir(const char* path);
int set_time(const char* path, const struct timespec ts[2]);
int set_mode(const char* path, mode_t mode);

#endif
