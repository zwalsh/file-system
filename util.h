#ifndef UTIL_H
#define UTIL_H

#include <string.h>
#include <stdbool.h>

static int
streq(const char* aa, const char* bb)
{
    return strcmp(aa, bb) == 0;
}

static int
min(int x, int y)
{
    return (x < y) ? x : y;
}

static int
max(int x, int y)
{
    return (x > y) ? x : y;
}

static int
clamp(int x, int v0, int v1)
{
    return max(v0, min(x, v1));
}

static void
bitmap_set(char* bitmap, int index, bool on)
{
	char* byte = bitmap + (index / 8);
	int pos = index % 8;
	if (on) {
		*byte = *byte | (char)(1 << pos);
	} else {
		*byte = *byte & (char)~(1 << pos);
	}
}

static int
bitmap_read(char* bitmap, int index)
{
	char* byte = bitmap + (index / 8);
	int pos = index % 8;
	return (int) ((*byte & (char)(1 << pos)) >> pos);
}

static int
bitmap_next_free(char* bitmap, int from, int size)
{
	for (int ii = from; ii < size; ++ii) {
		if (bitmap_read(bitmap, ii) == 0) {
			return ii;
		}
	}
	return -1;
}

static int
bitmap_first_free(char* bitmap, int size)
{
	return bitmap_next_free(bitmap, 0, size);
}

static int
free_range_size(char* bitmap, int start, int range, int size)
{
	if (start + range >= size) {
		return -1;
	}
	
	for (int ii = start; ii < start + range; ii++) {
		if (bitmap_read(bitmap, ii)) {
			return ii - start;
		}
	}
	return range;
}

static int
bitmap_find_range(char* bitmap, int range, int size)
{
	int curr_range_start = 0;
	int curr_range_size = 0;
	while (curr_range_size < range) {
		curr_range_start = bitmap_next_free(bitmap, 
						    curr_range_start + curr_range_size,
						    size);
		if (curr_range_start < 0) {
			return -1;
		}
		curr_range_size = free_range_size(bitmap, curr_range_start, range, size);
		if (curr_range_size < 0) {
			return -1;
		}
	}
	return curr_range_start;
}

static bool
bitmap_all_free(char* bitmap, int size)
{
	for (int ii = 0; ii < size; ii++) {
		if (bitmap_read(bitmap, ii)) {
			return false;
		}
	}
	return true;
}




#endif
