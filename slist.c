#include <string.h>
#include <stdlib.h>
#include <alloca.h>

#include "slist.h"

ilist*
i_cons(int i, ilist* rest)
{
	ilist* xs = malloc(sizeof(ilist));
	xs->data = i;
	xs->refs = 1;
	xs->next = rest;
	return xs;
}

void
i_free(ilist* xs)
{
	if(xs == 0) {
		return;
	}
	xs->refs -= 1;
	
	if(xs->refs == 0) {
		i_free(xs->next);
		free(xs);
	}
}

slist*
s_cons(const char* text, slist* rest)
{
    slist* xs = malloc(sizeof(slist));
    xs->data = strdup(text);
    xs->refs = 1;
    xs->next = rest;
    return xs;
}

void
s_free(slist* xs)
{
    if (xs == 0) {
        return;
    }

    xs->refs -= 1;

    if (xs->refs == 0) {
        s_free(xs->next);
        free(xs->data);
        free(xs);
    }
}

slist*
s_split(const char* text, char delim)
{
    if (*text == 0) {
        return 0;
    }

    int plen = 0;
    while (text[plen] != 0 && text[plen] != delim) {
        plen += 1;
    }

    int skip = 0;
    if (text[plen] == delim) {
        skip = 1;
    }

    slist* rest = s_split(text + plen + skip, delim);
    char*  part = alloca(plen + 2);
    memcpy(part, text, plen);
    part[plen] = 0;

    return s_cons(part, rest);
}

slist*
s_drop_last(slist* list)
{
	if (list == NULL || list->next == NULL) {
		s_free(list);
		return NULL;
	}
	slist* curr = list;
	while (curr->next != NULL) {
		curr = curr->next;
	}
	s_free(curr);
	return list;
}

char*
s_get_last(slist* list)
{
	if (list == NULL) {
		return NULL;
	}
	while (list->next != NULL) {
		list = list->next;
	}
	return list->data;
}









