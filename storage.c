
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "storage.h"
#include "pages.h"

const int DATA_BITMAP_PAGE = 0;
const int INODE_BITMAP_PAGE = 1;
const int INODE_PAGE = 2;
const int DATA_BLOCK_PAGE = 20;

typedef struct file_entry {
	char* name;
	int iNode_number;
} file_entry;

typedef struct directory {
	int number_of_entries;
	file_entry* entries;
} directory;

// contains metadata for each file or directory
typedef struct iNode {
	// indicates object type (e.g. dir, file) and permissions
	int mode;
	int num_hard_links;
	int user_id;
	int group_id;
	// size of file this iNode represents in bytes
	int size;
	time_t last_time_accessed;
	time_t last_time_modified;
	time_t last_time_status_change;
	// array of blocks that store this thing
	int data_block_ids[10];
	int indirect_data_block_id;
} iNode;

void* 
get_iNode(int index)
{
	iNode* inode_start = pages_get_page(INODE_PAGE);
	return inode_start + index;
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

