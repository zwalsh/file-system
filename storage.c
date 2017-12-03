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

const int PAGE_SIZE = 4096;
const int DATA_BITMAP_PAGE = 0;
const int INODE_BITMAP_PAGE = 1;
const int INODE_PAGE = 2;
const int DATA_BLOCK_PAGE = 20;
const int NUM_DATA_BLOCKS = 236;
const int NUM_ENTRIES_IN_DIR = 15;
//use this later
const int NUM_DATA_BLOCK_IDS = 10;

typedef struct file_entry {
	char name[256];
	int iNode_num;
} file_entry;

typedef struct directory {
	//bitmap is size 15
	char* file_entry_bitmap;
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

int
get_num_inodes()
{
	return (DATA_BLOCK_PAGE - INODE_PAGE) * PAGE_SIZE / sizeof(iNode);
}

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
is_inode_file(iNode* node)
{
	return (node->mode & S_IFMT) == S_IFREG;
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

iNode*
configure_inode(int inode_id, int mode, int size, int* data_block_ids, 
	int indirect_data_block_id)
{
	iNode* inode = get_inode(inode_id);
	inode->mode = mode;
	inode->num_hard_links = 1;
	inode->user_id = getuid();
	inode->group_id = getgid();
	//could also be page size
	inode->size = size;

	time_t current_time = time(NULL);
	inode->last_time_accessed = current_time;
	inode->last_time_modified = current_time;
	inode->last_time_status_change = current_time;

	memcpy(inode->data_block_ids, data_block_ids, 10 * sizeof(int));
	inode->indirect_data_block_id = indirect_data_block_id;
}

char*
get_inode_bitmap()
{
	return (char*) pages_get_page(INODE_BITMAP_PAGE);
}

char*
get_data_bitmap()
{
	return (char*) pages_get_page(DATA_BITMAP_PAGE);
}

int
reserve_inode()
{
	char* inode_bitmap = get_inode_bitmap();
	int new_inode_index = bitmap_first_free(inode_bitmap, get_num_inodes());
	if(new_inode_index < 0) {
		return -ENOMEM;
	}
	bitmap_set(inode_bitmap, new_inode_index, true);
	printf("reserved inode %i \n", new_inode_index);
	return new_inode_index;
}

int
reserve_data_block()
{
	char* data_bitmap = get_data_bitmap();
	int new_block_index = bitmap_first_free(data_bitmap, NUM_DATA_BLOCKS);
	if(new_block_index < 0) {
		return -ENOMEM;
	}
	bitmap_set(data_bitmap, new_block_index, true);
	return new_block_index;
}

int
add_entry_to_inode(iNode* inode, const char* entry_name, int inode_num)
{
	directory* working_dir;
	int file_entry_index = -1;

	for(int ii = 0; ii < 10; ii++) {
		int curr_data_block = inode->data_block_ids[ii];
		if(curr_data_block == -1) {
			int new_block_id = reserve_data_block();
			inode->data_block_ids[ii] = new_block_id;
			working_dir = (directory*) get_data_block(new_block_id);
			file_entry_index = 0;
			break;
		} else {
			working_dir = (directory*) get_data_block(curr_data_block);
			char* file_entry_bitmap = (char*) &working_dir->file_entry_bitmap;
			file_entry_index = bitmap_first_free(file_entry_bitmap, 15);
			if(file_entry_index != -1) {
				break;
			}
		}
	}

	printf("placing: %s ", entry_name);
	printf("ref to: %i", inode_num);
	printf("file entry index: %i\n", file_entry_index);

	if(file_entry_index == -1) {
		//use indirect block
	}

	file_entry entry;
	strcpy(entry.name, entry_name);
	entry.iNode_num = inode_num;

	*(&working_dir->entries + file_entry_index) = entry;
	char* file_entry_bitmap = (char*) &working_dir->file_entry_bitmap;
	bitmap_set(file_entry_bitmap, file_entry_index, true);

	return 0;
}

void
root_init()
{
	iNode* root = get_inode(0);
	// check for a root node that already exists
	if (root->mode != 0) {
		return;
	}
	void* data_bitmap = pages_get_page(DATA_BITMAP_PAGE);
	void* inode_bitmap = pages_get_page(INODE_BITMAP_PAGE);
	memset(data_bitmap, 0, 4096);
	memset(inode_bitmap, 0, 4096);

	int root_index = reserve_inode();
	root = get_inode(root_index);

	int root_mode = S_IFDIR | S_IRWXU;
	int* data_block_ids = malloc(10 * sizeof(int));
	int data_block_index = reserve_data_block();
	data_block_ids[0] = data_block_index;
	for(int ii = 1; ii < 10; ii++) {
		data_block_ids[ii] = -1;
	}

	root = configure_inode(root_index, root_mode, sizeof(directory), data_block_ids, -1);
	free(data_block_ids);

	add_entry_to_inode(root, ".", data_block_index);
	add_entry_to_inode(root, "..", data_block_index);
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
		return -ENOTDIR;
	}

	for(int ii = 0; ii < 10; ii++) {
		int data_block_id = inode->data_block_ids[ii];
		directory* curr_dir = (directory*) get_data_block(data_block_id);
		for(int jj = 0; jj < NUM_ENTRIES_IN_DIR; jj++) {
			char* file_entry_bitmap = (char*) &curr_dir->file_entry_bitmap;
			if(bitmap_read(file_entry_bitmap, jj)) {
				file_entry* entry = (&curr_dir->entries + jj);
				if(strcmp(entry->name, inode_name) == 0) {
					return entry->iNode_num;
				}
			}
		}
	}
	
	return -ENOENT;
}

slist*
get_path_components(const char* path)
{
	slist* path_components = s_split(path, '/');
	if(path_components == NULL) {
		return (slist*) -ENOENT;
	}

	if(strcmp(path_components->data, "") == 0) {
		path_components = path_components->next;
	} else {
		return (slist*) -ENOENT;
	}

	return path_components;
}

int
inode_index_from_path_components(slist* path_components)
{
	if(path_components < 0) {
		return -ENOENT;
	}
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

int
inode_index_from_path(const char* path)
{
	slist* path_components = get_path_components(path);
	return inode_index_from_path_components(path_components);
}

int
parent_inode_index_from_path(const char* path)
{
	slist* path_components = get_path_components(path);
	slist* parent_path = s_drop_last(path_components);
	return inode_index_from_path_components(parent_path);
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
	printf("getting filenames: %s", path);
	int inode_index = inode_index_from_path(path);
	printf(" inode index: %i\n", inode_index);
	if(inode_index < 0) {
		return (slist*) -ENOENT;
	}

	iNode* inode = get_inode(inode_index);
	if(!is_inode_dir(inode)) {
		return (slist*) -ENOTDIR;
	}

	slist* entry_list = NULL;
	for(int ii = 0; ii < 10; ii++) {
		int data_block_id = inode->data_block_ids[ii];
		directory* curr_dir = (directory*) get_data_block(data_block_id);
		for(int jj = 0; jj < 15; jj++) {
			char* file_entry_bitmap = (char*) &curr_dir->file_entry_bitmap;
			if(bitmap_read(file_entry_bitmap, jj)) {
				file_entry* entry = (&curr_dir->entries + jj);
				entry_list = s_cons(entry->name, entry_list);
			}
		}
	}

	//check indirect blocks;

	return entry_list;
}

void
free_data_block(int index)
{
	if (index < 0) {
		return;
	}
	void* block = get_data_block(index);
	memset(block, 0, PAGE_SIZE);
	
	bitmap_set(get_data_bitmap(), index, false);
}

void
free_all_blocks(iNode* node)
{
	for (int ii = 0; ii < 10; ii++) {
		free_data_block(node->data_block_ids[ii]);
		node->data_block_ids[ii] = -1;
	}
	
	// handle indirect block ;P
}

int
num_blocks_used(iNode* node)
{
	for (int ii = 0; ii < 10; ii++) {
		if (node->data_block_ids[ii] == -1) {
			return ii;
		}
	}
	return 10;
}

int 
reserve_blocks_for_node(iNode* node, int blocks_needed) 
{
	// try to find a contiguous range of blocks
	int start_of_range = bitmap_find_range(
	get_data_bitmap(), blocks_needed, NUM_DATA_BLOCKS);
	// if we can't find a continuous range, reserve one-by-one
	if (start_of_range < 0) {
		int blocks_reserved = 0; 
		while (blocks_reserved < blocks_needed) {
			int block_id = reserve_data_block();
			if (block_id < 0) {
				free_all_blocks(node);
				return -ENOSPC;
			}
			node->data_block_ids[blocks_reserved] = block_id;
			blocks_reserved++;
			printf("reserve_blocks_for_node(): reserved block: %i.\n", block_id);
		}
		return 0;
	}

	// if we find a contiguous range, set them all
	for (int ii = 0; ii < blocks_needed; ii++) {
		node->data_block_ids[ii] = start_of_range + ii;
		bitmap_set(get_data_bitmap(), start_of_range + ii, true);
		printf("reserve_blocks_for_node(): reserved block: %i.\n", start_of_range + ii);
		// handle indirect block (fuck)
	}
	return 0;
}

int
remove_blocks_from_node(iNode* node, int num_to_remove)
{
	int curr_num_blocks = num_blocks_used(node);
	int index_to_remove = curr_num_blocks - 1;
	int num_removed = 0;
	while (num_removed < num_to_remove) {
		free_data_block(node->data_block_ids[index_to_remove]);
		node->data_block_ids[index_to_remove] = -1;
		index_to_remove--;
		if (index_to_remove < 0) {
			return -1;
		}
		num_removed++;
	}
	return 0;
}

int
set_file_to_size(const char* path, off_t size)
{
	printf("setting: %s, to: %li.\n", path, size);
	int inode_index = inode_index_from_path(path);
	if (inode_index < 0) {
		return -ENOENT;
	}
	
	// clear out the file
	iNode* node = get_inode(inode_index);
	
	if (!is_inode_file(node)) {
		return -EISDIR;
	}
	
	int curr_num_blocks = num_blocks_used(node);
	int total_blocks = (int) ceil(size / (PAGE_SIZE * 1.0));
	int blocks_to_add = total_blocks - curr_num_blocks;
	
	node->size = size;
	if (blocks_to_add == 0) {
		return 0;
	} else if (blocks_to_add < 0) {
		// remove some blocks
		return remove_blocks_from_node(node, -blocks_to_add);
	} else {
		// reserve some blocks
		return reserve_blocks_for_node(node, blocks_to_add);
	}
}

int
next_read_size(iNode* node, int offset_in_file, int size)
{
	size = min(node->size, size);
	int curr_block_index = offset_in_file / PAGE_SIZE;
	int offset_in_block = offset_in_file % PAGE_SIZE;
	
	int read_size = 0;
	
	while (read_size < size && curr_block_index < 10) {
		read_size += PAGE_SIZE - offset_in_block;
		offset_in_block = 0;
		
		if (curr_block_index == 9) {
			break;
		}
		
		int curr_block = node->data_block_ids[curr_block_index];
		int next_block = node->data_block_ids[curr_block_index + 1];
		
		if (curr_block + 1 != next_block) {
			break;
		}
		curr_block_index++;		
	}
	
	// handle indirect block
	
	return read_size;	
}

int
read_file(const char* path, char* buf, size_t size, off_t offset_in_file)
{
	int inode_index = inode_index_from_path(path);
	if (inode_index < 0) {
		return -ENOENT;
	}
	iNode* node = get_inode(inode_index);
	if (!is_inode_file(node)) {
		return -EISDIR;
	}
	size = min(size, node->size);
	
	int offset_in_buf = 0;
	while (offset_in_buf < size) {
		int read_size = next_read_size(node, offset_in_file, size);
		
		int curr_block = offset_in_file / PAGE_SIZE;
		int offset_in_block = offset_in_file % PAGE_SIZE;
		void* block = get_data_block(node->data_block_ids[curr_block]);
	
		memcpy(buf + offset_in_buf, block + offset_in_block, read_size);
		offset_in_buf += read_size;
		offset_in_file += read_size;
	}
	
	return size;
}

int
write_file(const char* path, const char* buf, size_t size, off_t offset_in_file)
{
	int inode_index = inode_index_from_path(path);
	if (inode_index < 0) {
		return -ENOENT;
	}
	iNode* node = get_inode(inode_index);
	if (!is_inode_file(node)) {
		return -EISDIR;
	}
	if (node->size < size + offset_in_file) {
		set_file_to_size(path, size + offset_in_file);
	}
	
	int offset_in_buf = 0;
	while (offset_in_buf < size) {
		int read_size = next_read_size(node, offset_in_file, size);
		
		int curr_block = offset_in_file / PAGE_SIZE;
		int offset_in_block = offset_in_file % PAGE_SIZE;
		void* block = get_data_block(node->data_block_ids[curr_block]);
	
		memcpy(block + offset_in_block, buf + offset_in_buf, read_size);
		offset_in_buf += read_size;
		offset_in_file += read_size;
	}
	
	return size;
}


int
create_dir(const char* path)
{

	slist* tokens = get_path_components(path);
	if(tokens < 0) {
		return -ENOENT;
	}
	printf("token: %s\n", tokens->data);
	if(tokens == NULL) {
		return -ENOENT;
	}

	int parent_index = 0;
	slist* curr = tokens;
	while(curr->next != NULL) {
		parent_index = inode_child(parent_index, curr->data);
		if(parent_index == -1) {
			return -ENOENT;
		}

		curr = curr->next;
	}

	iNode* parent_inode = get_inode(parent_index);
	if(!is_inode_dir(parent_inode)) {
		return -ENOTDIR;
	}

	char* new_dir_name = curr->data;

	int new_inode_index = reserve_inode();
	int new_data_block_index = reserve_data_block();
	if(new_inode_index < 0 || new_data_block_index < 0) {
		return -ENOMEM;
	}

	int mode = S_IFDIR | S_IRWXU;
	int* data_block_ids = malloc(10 * sizeof(int));
	data_block_ids[0] = new_data_block_index;
	for(int ii = 1; ii < 10; ii++) {
		data_block_ids[ii] = -1;
	}
	
	iNode* new_inode = configure_inode(new_inode_index, mode, sizeof(directory), data_block_ids, -1);
	
	add_entry_to_inode(new_inode, ".", new_inode_index);
	add_entry_to_inode(new_inode, "..", parent_index);
	add_entry_to_inode(parent_inode, new_dir_name, new_inode_index);

	return 0;
}

int
create_inode_at_path(const char* path, mode_t mode)
{
	int inode_index = reserve_inode();
	if (inode_index < 0) {
		return -ENOENT;
	}
	int* data_block_ids = malloc(10 * sizeof(int));
	for(int ii = 0; ii < 10; ii++) {
		data_block_ids[ii] = -1;
	}
	configure_inode(inode_index, mode, 0, data_block_ids, -1);
	
	int parent_inode_index = parent_inode_index_from_path(path);
	iNode* parent = get_inode(parent_inode_index);
	
	const char* file_name = s_get_last(get_path_components(path));
	add_entry_to_inode(parent, file_name, inode_index);
	return 0;
}

int
truncate(const char* path, off_t size)
{
	printf("truncating: %s, to: %li.\n", path, size);
	int inode_index = inode_index_from_path(path);
	if (inode_index < 0) {
		return -ENOENT;
	}
	
	// clear out the file
	iNode* node = get_inode(inode_index);
	free_all_blocks(node);
	set_file_to_size(path, size);
}

int
remove_entry_from_dir(directory* dir, const char* entry_name)
{
	char* file_entry_bitmap = (char*) &dir->file_entry_bitmap;
	for(int ii = 0; ii < NUM_ENTRIES_IN_DIR; ii++) {
		int entry_in_use = bitmap_read(file_entry_bitmap, ii);
		if(entry_in_use) {
			file_entry entry = *(&dir->entries + ii);
			if(strcmp(entry.name, entry_name) == 0) {
				bitmap_set(file_entry_bitmap, ii, false);
				memset(&dir->entries + ii, 0, sizeof(file_entry));
				return 0;
			}
		}
	}

	return -1;
}

int
remove_entry_from_inode(iNode* inode, const char* entry_name)
{
	for(int ii = 0; ii < 10; ii++) {
		int curr_data_block = inode->data_block_ids[ii];
		if(curr_data_block < 0) {
			continue;
		}

		directory* working_dir = (directory*) get_data_block(curr_data_block);
		int rv = remove_entry_from_dir(working_dir, entry_name);
		if(rv == 0) {
			return 0;
		}
	}

	//handle indirect block :0)

	return -ENOENT;
}

void
free_inode(int inode_index)
{
	iNode* inode = get_inode(inode_index);
	free_all_blocks(inode);
	memset(inode, 0, sizeof(iNode));

	char* inode_bitmap = get_inode_bitmap();
	bitmap_set(inode_bitmap, inode_index, false);
}

int
unlink_file(const char* path)
{
	int inode_index = inode_index_from_path(path);
	if (inode_index < 0) {
		return -ENOENT;
	}

	int parent_inode_index = parent_inode_index_from_path(path);
	if(parent_inode_index < 0) {
		return -ENOENT;
	}

	iNode* parent_inode = get_inode(parent_inode_index);
	slist* path_components = get_path_components(path);
	const char* entry_name = s_get_last(path_components);

	int rv = remove_entry_from_inode(parent_inode, entry_name);
	s_free(path_components);
	if(rv != 0) {
		return -ENOENT;
	}
	
	iNode* inode = get_inode(inode_index);
	inode->num_hard_links--;
	if(inode->num_hard_links > 0) {
		return 0;
	}

	free_inode(inode_index);
	return 0;
}

int
link_file(const char* path_old, const char* path_new)
{
	int inode_index = inode_index_from_path(path_old);
	if (inode_index < 0) {
		return -ENOENT;
	}

	iNode* inode = get_inode(inode_index);
	
	int parent_inode_index = parent_inode_index_from_path(path_new);
	if(parent_inode_index < 0) {
		return -ENOENT;
	}

	iNode* parent_inode = get_inode(parent_inode_index);
	slist* path_components = get_path_components(path_new);
	const char* entry_name = s_get_last(path_components);

	int rv = add_entry_to_inode(parent_inode, entry_name, inode_index);
	s_free(path_components);
	if(rv != 0) {
		return -ENOTDIR;
	}

	inode->num_hard_links++;
	return 0;
}

int
rename_file(const char* from, const char* to)
{
	int rv = link_file(from, to);
	if(rv != 0) {
		return rv;
	}

	return unlink_file(from); 
}

int
remove_dir(const char* path)
{
	int inode_index = inode_index_from_path(path);
	if (inode_index < 0) {
		return -ENOENT;
	}

	iNode* inode = get_inode(inode_index);

	if(!is_inode_dir(inode)) {
		return -ENOTDIR;
	}

	for(int ii = 0; ii < NUM_DATA_BLOCK_IDS; ii++) {
		if(inode->data_block_ids[ii] < 0) {
			continue;
		}

		directory* curr_dir = (directory*) get_data_block(inode->data_block_ids[ii]);
		for(int ii = 0; ii < NUM_ENTRIES_IN_DIR; ii++) {
			char* dir_bitmap = (char*) &curr_dir->file_entry_bitmap;
			if(bitmap_read(dir_bitmap, ii)) {
				file_entry entry = *(&curr_dir->entries + ii);
				if(!streq(entry.name, ".") && !streq(entry.name, "..")) {
					return -ENOTEMPTY;
				}
			}
		}
	}
	
	//check if indirect is empty :)
	
	int rv = unlink_file(path);
	return rv;
}


















