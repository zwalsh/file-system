#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <bsd/string.h>
#include <dirent.h>
#include <assert.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "storage.h"
#include "slist.h"

const static int MAX_FILENAME = 256;

// implementation for: man 2 access
// Checks if a file exists.
int
nufs_access(const char *path, int mask)
{
    printf("\n\naccess(%s, %04o)\n", path, mask);
    return 0;
}

// implementation for: man 2 stat
// gets an object's attributes (type, permissions, size, etc)
int
nufs_getattr(const char *path, struct stat *st)
{
    printf("\n\ngetattr(%s)= ", path);
    int rv = get_stat(path, st);
    printf("%li bytes.\n", st->st_size);
    
    if (rv == -1) {
        return -ENOENT;
    }
    else {
        return 0;
    }
}

// implementation for: man 2 readdir
// lists the contents of a directory
int
nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
             off_t offset, struct fuse_file_info *fi)
{
    struct stat st;

    printf("\n\nreaddir(%s)\n", path);
	slist* filenames = get_filenames_from_dir(path);
	slist* curr_filename = filenames;
	while(curr_filename != NULL) {
		char filepath[MAX_FILENAME];
		strcpy(filepath, path);
		strcat(filepath, curr_filename->data);

		get_stat(filepath, &st);
		//should curr_filename->data be null terminated?
		// wtf do we pass in as an offset
		int rv = filler(buf, curr_filename->data, &st, 0);
		if(rv != 0) {
			break;
		}
		
		curr_filename = curr_filename->next;
	}

	s_free(filenames);
    return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
int
nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    printf("\n\nmknod(%s, %04o)\n", path, mode);
    return create_inode_at_path(path, mode);
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int
nufs_mkdir(const char *path, mode_t mode)
{
	printf("\n\nmkdir(%s, %i)\n", path, mode);
	return create_dir(path);
}

int
nufs_link(const char *path_old, const char *path_new)
{
	printf("\n\nlink(%s, %s)\n", path_old, path_new);
	return link_file(path_old, path_new);
}

int
nufs_unlink(const char *path)
{
    printf("\n\nunlink(%s)\n", path);
    return unlink_file(path);
}

// must be empty to succeed
int
nufs_rmdir(const char *path)
{
    printf("\n\nrmdir(%s)\n", path);
    return -1;
}

// implements: man 2 rename
// called to move a file within the same filesystem
int
nufs_rename(const char *from, const char *to)
{
    printf("\n\nrename(%s => %s)\n", from, to);
    return rename_file(from, to);
}

int
nufs_chmod(const char *path, mode_t mode)
{
    printf("\n\nchmod(%s, %04o)\n", path, mode);
    return -1;
}

int
nufs_truncate(const char *path, off_t size)
{
    printf("\n\ntruncate(%s, %ld bytes)\n", path, size);
    return truncate(path, size);
}

// this is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
int
nufs_open(const char *path, struct fuse_file_info *fi)
{
    printf("\n\nopen(%s)\n", path);
    return 0;
}

// Actually read data
int
nufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("\n\nread(%s, %ld bytes, @%ld)\n", path, size, offset);
    return read_file(path, buf, size, offset);
}

// Actually write data
int
nufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    printf("\n\nwrite(%s, %ld bytes, @%ld)\n", path, size, offset);
    return write_file(path, buf, size, offset);
}

// Update the timestamps on a file or directory.
int
nufs_utimens(const char* path, const struct timespec ts[2])
{
    //int rv = storage_set_time(path, ts);
    int rv = 1;
    printf("\n\nutimens(%s, [%ld, %ld; %ld %ld]) -> %d\n",
           path, ts[0].tv_sec, ts[0].tv_nsec, ts[1].tv_sec, ts[1].tv_nsec, rv);
	return rv;
}

void
nufs_init_ops(struct fuse_operations* ops)
{
    memset(ops, 0, sizeof(struct fuse_operations));
    ops->access   = nufs_access;
    ops->getattr  = nufs_getattr;
    ops->readdir  = nufs_readdir;
    ops->mknod    = nufs_mknod;
    ops->mkdir    = nufs_mkdir;
	ops->link	  = nufs_link;
    ops->unlink   = nufs_unlink;
    ops->rmdir    = nufs_rmdir;
    ops->rename   = nufs_rename;
    ops->chmod    = nufs_chmod;
    ops->truncate = nufs_truncate;
    ops->open	  = nufs_open;
    ops->read     = nufs_read;
    ops->write    = nufs_write;
    ops->utimens  = nufs_utimens;
};

struct fuse_operations nufs_ops;

int
main(int argc, char *argv[])
{
    assert(argc > 2 && argc < 6);
    storage_init(argv[--argc]);
    nufs_init_ops(&nufs_ops);
    return fuse_main(argc, argv, &nufs_ops, NULL);
}

