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

#endif
