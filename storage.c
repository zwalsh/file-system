#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "storage.h"
#include "pages.h"
#include "slist.h"
#include "util.h"

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
	file_entry entries;
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

bool
is_iNode_directory(iNode* node)
{
	return (node->mode & S_IFMT) == S_IFDIR;
}

directory*
get_directory_struct(iNode* node)
{
	assert(is_iNode_directory(node));
	return (directory*) get_data_block(node->data_block_ids[0]);
}

void
root_init()
{
	iNode* root = get_iNode(0);
	if(root->mode != 0) {
		return;
	}

	root->mode = S_IFDIR | S_IRWXU;
	root->num_hard_links = 1;
	root->user_id = getuid();
	root->group_id = getgid();
	//could also be page size
	root->size = sizeof(directory);

	time_t current_time = time(NULL);
	root->last_time_accessed = current_time;
	root->last_time_modified = current_time;
	root->last_time_status_change = current_time;

	void* data_bitmap = pages_get_page(DATA_BITMAP_PAGE);
	void* inode_bitmap = pages_get_page(INODE_BITMAP_PAGE);

	memset(data_bitmap, 0, 4096);
	memset(inode_bitmap, 0, 4096);

	root->data_block_ids[0] = 0;
	for(int ii = 1; ii < 10; ii++) {
		root->data_block_ids[ii] = -1;
	}

	file_entry dot;
	dot.name = ".";
	dot.iNode_number = 0;
	file_entry dotdot;
	dotdot.name = "..";
	dotdot.iNode_number = 0;

	directory* dir = get_directory_struct(root);
	dir->number_of_entries = 2;
	dir->entries = dot;
	*(&dir->entries + 1) = dotdot;

	bitmap_set((char*) data_bitmap, 0, true);
	bitmap_set((char*) inode_bitmap, 0, true);
}

void
storage_init(const char* path)
{
	pages_init(path);
	root_init();
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

int
inode_child(int inode_index, char* inode_name)
{
	iNode* inode = get_iNode(inode_index);
	int data_block = inode->data_block_ids[0];
	directory* dir = (directory*) get_data_block(data_block);
	for (int ii = 0; ii < dir->number_of_entries; ++ii) {
		file_entry* entry = (&dir->entries + ii);
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
	while (current != NULL && strcmp(current->data, "") != 0) {
		inode_index = inode_child(inode_index, current->data);
		if (inode_index == -1) {
			return -1;
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
		return -1;
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

slist*
get_filenames_from_dir(const char* path)
{
	int node_index = get_inode_from_path(path);
	if(node_index < 0) {
		//think on this
		return (slist*) -ENOENT;
	}

	iNode* node = get_iNode(node_index);
	if(!is_iNode_directory(node)) {
		return (slist*) -ENOTDIR;
	}

	slist* entry_list = NULL;
	directory* dir = get_directory_struct(node);
	for(int ii = 0; ii < dir->number_of_entries; ++ii) {
		file_entry* entry = (&dir->entries + ii);
		entry_list = s_cons(entry->name, entry_list);
	}
	
	return entry_list;
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

