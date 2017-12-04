#ifndef SLIST_H
#define SLIST_H

typedef struct slist {
    char* data;
    int   refs;
    struct slist* next;
} slist;

typedef struct ilist {
	int data;
	int refs;
	struct ilist* next;
} ilist;

ilist* i_cons(int i, ilist* rest);
void   i_free(ilist* list);

slist* s_cons(const char* text, slist* rest);
void   s_free(slist* xs);
slist* s_split(const char* text, char delim);
slist* s_drop_last(slist* list);
char*  s_get_last(slist* list);

#endif

