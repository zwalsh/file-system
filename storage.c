
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "storage.h"
#include "pages.h"

const int DATA_BITMAP_PAGE = 0;
const int INODE_BITMAP_PAGE = 1;
const int INODE_PAGE = 2;
const int DATA_BLOCK_PAGE = 20;

typedef struct file_entry {
	// file name
	char* name;
	// iNode number
	int iNode_number;
	// 0 if file isn't a dir
	int number_of_entries;
	// null if file isn't a dir
	struct file_entry* entries;
} file_entry;

// contains metadata for each file or directory
typedef struct iNode {
	// indicates object type (e.g. dir, file) and permissions
	int mode;
	// carries additional metadata 
	// if file is a directory, will store number of entries and
	file_entry* dir;
	// size of file this iNode represents
	int size;
	// user that created the file
	// not sure if we need this? It's in my notes but seems mostly useless
	int user;
	// time this file was created
	int time_created;
	// array of blocks that store this thing
	int data_block_ids[10];
} iNode;

void* 
get_iNode(int index)
{
	return pages_get_page(INODE_PAGE + index);
}

void*
get_data_block(int index)
{
	return pages_get_page(DATA_BLOCK_PAGE + index);
}

void
storage_init(const char* path)
{
	pages_init(path);
}

typedef struct file_data {
    const char* path;
    int         mode;
    const char* data;
} file_data;

static file_data file_table[] = {
    {"/", 040755, 0},
    {"/hello.txt", 0100644, "hello\n"},
    {0, 0, 0},
};

static int
streq(const char* aa, const char* bb)
{
    return strcmp(aa, bb) == 0;
}

static file_data*
get_file_data(const char* path) {
    for (int ii = 0; 1; ++ii) {
        file_data row = file_table[ii];

        if (file_table[ii].path == 0) {
            break;
        }

        if (streq(path, file_table[ii].path)) {
            return &(file_table[ii]);
        }
    }

    return 0;
}

int
get_stat(const char* path, struct stat* st)
{
    file_data* dat = get_file_data(path);
    if (!dat) {
        return -1;
    }

    memset(st, 0, sizeof(struct stat));
    st->st_uid  = getuid();
    st->st_mode = dat->mode;
    if (dat->data) {
        st->st_size = strlen(dat->data);
    }
    else {
        st->st_size = 0;
    }
    return 0;
}

const char*
get_data(const char* path)
{
    file_data* dat = get_file_data(path);
    if (!dat) {
        return 0;
    }

    return dat->data;
}

