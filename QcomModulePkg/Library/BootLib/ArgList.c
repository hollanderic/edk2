
#if 0
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define CHAR8 uint8_t
#define EFI_STATUS uint32_t
#define UINT32 uint32_t
#define AllocatePool malloc
#endif
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Protocol/Print2.h>


typedef struct arg_list arg_list_t;

struct arg_list {
    CHAR8* arg;
    struct arg_list* next;
};


static void arg_copy(CHAR8* dst, CHAR8* src) {
    while (*src) {
        *dst = *src;
        src++;
        dst++;
    }
    *dst = 0;
}

void ArgListDump(arg_list_t* list) {


    while (list) {
        //printf("arg: %zd  %s\n",strlen(list->arg),list->arg);
        list = list->next;
    }
}


void ArgListFree(arg_list_t** list) {
    arg_list_t* temp;
    while (*list) {
        temp = *list;
        *list = temp->next;
        FreePool(temp->arg);
        FreePool(temp);
    }
}

UINT32 ArgListCat(arg_list_t* list, CHAR8** outstr){
    UINT32 charcount = 0;
    arg_list_t* temp = list;
    while(temp) {
        charcount += (AsciiStrLen(temp->arg) + 1);
        temp = temp->next;
    }
    //printf("total of %d characters in list\n",charcount);
    temp =  list;
    *outstr = AllocatePool(charcount);
    CHAR8* local = *outstr;
    while(temp) {
        arg_copy(local,temp->arg);
        local+=AsciiStrLen(temp->arg);
        *local=' ';
        local++;
        temp=temp->next;
    }
    return 0;
}

arg_list_t* ArgListNewNode(arg_list_t** list, CHAR8* str, UINT32 len) {

    arg_list_t* new;
    new = AllocatePool(sizeof(arg_list_t));
    new->next=NULL;
    new->arg = AllocatePool(len);
    arg_copy(new->arg,str);

    if (*list == NULL) {
        *list = new;
    } else {
        arg_list_t* temp = *list;
        while(temp->next)
            temp=temp->next;
        temp->next = new;
    }
    return NULL;
}
#if 0
int main(int argc, char** argv) {
    uint8_t str1[] = "Eric";
    uint8_t str2[] = "Holland";

    uint8_t* finalstring = NULL;

    arg_list_t* mylist=NULL;
    ArgListNewNode(&mylist,str1,strlen(str1));
    printf("list is now: %p\n",mylist);
    ArgListNewNode(&mylist,str2,strlen(str2));
    ArgListDump(mylist);

    printf("list is now: %p\n",mylist);
    ArgListCat(mylist,&finalstring);
    printf("Final string : %s\n",finalstring);


    ArgListFree(&mylist);





}
#endif