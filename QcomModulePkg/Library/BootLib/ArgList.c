#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/ArgList.h>
#include <Protocol/Print2.h>


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
    (*outstr)[charcount-1]='\0';
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
