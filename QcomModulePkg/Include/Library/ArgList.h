#ifndef __ARGLIST_H__
#define __ARGLIST_H__

typedef struct arg_list arg_list_t;

struct arg_list {
    CHAR8* arg;
    struct arg_list* next;
};

void ArgListFree(arg_list_t** list);
arg_list_t* ArgListNewNode(arg_list_t** list, CHAR8* str, UINT32 len);
UINT32 ArgListCat(arg_list_t* list, CHAR8** outstr);
#endif