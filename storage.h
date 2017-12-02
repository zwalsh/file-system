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

#endif
