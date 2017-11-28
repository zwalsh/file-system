#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#include "storage.h"
#include "pages.h"
#include "slist.h"

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

iNode* 
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

int
inode_child(int inode_index, char* inode_name)
{
	iNode* inode = get_iNode(inode_index);
	int data_block = inode->data_block_ids[0];
	directory* dir = (directory*) get_data_block(data_block);
	for (int ii = 0; ii < dir->number_of_entries; ++ii) {
		file_entry* entry = (dir->entries + ii);
		if (strcmp(entry->name, inode_name) == 0) {
			return entry->iNode_number;
		}
	}
	return -1;
}

int
get_inode_from_path(const char* path)
{
	slist* path_components = s_split(path, '/');
	int inode_index = 0;
	slist* current = path_components;
	while (current != NULL) {
		inode_index = inode_child(inode_index, current->data);
		if (inode_index == -1) {
			return -ENOENT;
		}
		current = current->next;
	}
	return inode_index;
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
	int inode_index = get_inode_from_path(path);
	
	if (inode_index < 0) {
		return inode_index;
	}

	iNode* node = get_iNode(inode_index);

	memset(st, 0, sizeof(struct stat));
	st->st_dev = makedev(0, 0);
	st->st_ino = inode_index;
	st->st_mode = node->mode;
	st->st_nlink = node->num_hard_links;
	st->st_uid = node->user_id;
	st->st_gid = node->group_id;
	st->st_rdev = makedev(0, 0);
	st->st_size = node->size;
	st->st_blksize = 4096;
	st->st_blocks = (int) ceil(node->size / 512.0);
	st->st_atime = node->last_time_accessed;
	st->st_mtime = node->last_time_modified;
	st->st_ctime = node->last_time_status_change;
	
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

