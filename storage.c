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
	int iNode_num;
} file_entry;

typedef struct directory {
	int num_entries;
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
get_inode(int index)
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
is_inode_dir(iNode* node)
{
	return (node->mode & S_IFMT) == S_IFDIR;
}

directory*
get_dir(iNode* node)
{
	assert(is_inode_dir(node));
	return (directory*) get_data_block(node->data_block_ids[0]);
}

void
root_init()
{
	iNode* root = get_inode(0);
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
	dot.iNode_num = 0;
	file_entry dotdot;
	dotdot.name = "..";
	dotdot.iNode_num = 0;

	directory* dir = get_dir(root);
	dir->num_entries = 2;
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

//	todo: remove
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
	iNode* inode = get_inode(inode_index);
	if(!is_inode_dir(inode)) {
		return -1;
	}

	directory* dir = get_dir(inode);
	for (int ii = 0; ii < dir->num_entries; ++ii) {
		file_entry* entry = (&dir->entries + ii);
		if (strcmp(entry->name, inode_name) == 0) {
			return entry->iNode_num;
		}
	}
	return -1;
}

int
inode_index_from_path(const char* path)
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


//todo: remove
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
	int inode_index = inode_index_from_path(path);
	
	if (inode_index < 0) {
		return -1;
	}

	iNode* inode = get_inode(inode_index);

	memset(st, 0, sizeof(struct stat));
	st->st_dev = makedev(0, 0);
	st->st_ino = inode_index;
	st->st_mode = inode->mode;
	st->st_nlink = inode->num_hard_links;
	st->st_uid = inode->user_id;
	st->st_gid = inode->group_id;
	st->st_rdev = makedev(0, 0);
	st->st_size = inode->size;
	st->st_blksize = 4096;
	st->st_blocks = (int) ceil(inode->size / 512.0);
	st->st_atime = inode->last_time_accessed;
	st->st_mtime = inode->last_time_modified;
	st->st_ctime = inode->last_time_status_change;
	
	return 0;
}

slist*
get_filenames_from_dir(const char* path)
{
	int inode_index = inode_index_from_path(path);
	if(inode_index < 0) {
		return (slist*) -ENOENT;
	}

	iNode* inode = get_inode(inode_index);
	if(!is_inode_dir(inode)) {
		return (slist*) -ENOTDIR;
	}

	slist* entry_list = NULL;
	directory* dir = get_dir(inode);
	for(int ii = 0; ii < dir->num_entries; ++ii) {
		file_entry* entry = (&dir->entries + ii);
		entry_list = s_cons(entry->name, entry_list);
	}
	
	return entry_list;
}

//todo: remove
const char*
get_data(const char* path)
{
    file_data* dat = get_file_data(path);
    if (!dat) {
        return 0;
    }

    return dat->data;
}

int
get_next_available_data_block()
{
	//todo: implement this
	//return -1 if there arent any available blocks left
}

int
create_dir(const char* path)
{
	slist* tokens = s_split(path, '/');
	if(tokens == NULL) {
		// find error code
		return -1;
	}

	int parent_index = 0;
	slist* curr = tokens;
	while(curr->next != NULL) {
		parent_index = inode_child(parent_index, curr->data);
		if(parent_index == -1) {
			// find error code
			return -1;
		}

		curr = curr->next;
	}

	char* new_dir_name = curr->data;

	// make inode, fill out information
	int new_inode_index = inode_index_from_path(path);
	iNode* new_inode = get_inode(new_inode_index);
	new_inode->mode = S_IFDIR | S_IRWXU;
	new_inode->num_hard_links = 1;
	new_inode->user_id = getuid();
	new_inode->group_id = getgid();
	new_inode->size = sizeof(directory);

	time_t current_time = time(NULL);
	new_inode->last_time_accessed = current_time;
	new_inode->last_time_modified = current_time;
	new_inode->last_time_status_change = current_time;

	int data_block_index = get_next_available_data_block();
	if(data_block_index < 0) {
		// find error code
		return -1;
	}
	new_inode->data_block_ids[0] = data_block_index;
	for(int ii = 1; ii < 10; ii++) {
		new_inode->data_block_ids[ii] = -1;
	}
	
	directory* dir = get_dir(new_inode);
	dir->num_entries = 2;
	//todo: add . and .. directories

	iNode* parent_inode = get_inode(parent_index);
	if(!is_inode_dir(parent_inode)) {
		// find error code
		return -1;
	}

	directory* parent_dir = get_dir(parent_inode);
	//todo: make sure this doesn't overflow the data block
	file_entry* new_entry = (&parent_dir->entries + parent_dir->num_entries);
	new_entry->name = new_dir_name;
	new_entry->iNode_num = new_inode_index;

	parent_dir->num_entries = parent_dir->num_entries + 1;

	//confirm we have enough data blocks to add this stuff
	//make inode, fill out information
	//get data block for it
	//add . and .. directories
	//set up directory structure
	//add reference to new directory in parent, increment number of entries
}



