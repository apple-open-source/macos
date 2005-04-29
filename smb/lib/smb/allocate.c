/*
 * 
 * (c) Copyright 1992 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1992 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1992 DIGITAL EQUIPMENT CORPORATION
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
/*
**
**
**  NAME:
**
**      allocate.c
**
**  FACILITY:
**
**      IDL Stub Support Routines
**
**  ABSTRACT:
**
**  Stub memory allocation and free routines to keep track of all allocated
**  memory so that it can readily be freed
**
**  VERSION: DCE 1.0
**
*/

#include <dce/rpc.h>
#include <dce/stubbase.h>
#include <lsysdep.h>

#ifdef DEBUG_VERBOSE
#   include <stdio.h>
#endif

#ifdef PERFMON
#include <dce/idl_log.h>
#endif


/*
 * The following is the header information at the beginning of all
 * memory returned
 */
typedef struct header
{
    rpc_ss_mem_handle *head;    /* only valid for the first entry in the list */
    struct header *next;
    struct header *prev;
} header;

/*
 * Leave room in the buffer allocated before the data address for a pointer
 * to the buffer address (for freeing) and a node number (for the convenience
 * of server stubs)
 */
typedef struct
{
    header **buffer_address;
    ndr_ulong_int node_number;
} ptr_and_long;

/*
 * If the data address is 8-byte aligned and the gap is a multiple of 8-bytes
 * in size then the gap is maximally aligned, too
 */
#define GAP ((sizeof(ptr_and_long) + 7) & ~7)

byte_p_t rpc_ss_mem_alloc
(
    rpc_ss_mem_handle *handle,
    unsigned bytes
)
{
    /*
     * In addition to the memory requested by the user, allocate space for
     * the header, gap, and 8-byte alignment
     */
    byte_p_t data_addr;
    header *new;

#ifdef PERFMON
    RPC_SS_MEM_ALLOC_N;
#endif

    new = (header *)malloc(sizeof(header) + GAP + 7 + bytes);

#ifdef DEBUG_VERBOSE
    printf("Allocated %d bytes at %lx\n", sizeof(header) + GAP + 7 + bytes,
           new);
#endif

    if (new == NULL) RAISE( rpc_x_no_memory );

    if (handle->memory) ((header *)(handle->memory))->prev = new;

    new->head = handle;
    new->prev = NULL;
    new->next = (header *)handle->memory;
    handle->memory = (rpc_void_p_t)new;

    /*
     * Skip the header, and gap and align to an 8-byte boundary
     */
    data_addr = (byte_p_t)
        (((char *)new - (char *)0 + sizeof(header) + GAP + 7) & ~7);

    /*
     * Stash away the buffer address for _release and _dealloc
     */
    *((header **)((char *)data_addr - GAP)) = new;

#ifdef DEBUG_VERBOSE
    printf("Returning %lx\n", data_addr);
#endif

#ifdef PERFMON
    RPC_SS_MEM_ALLOC_X;
#endif

    return data_addr;
}

/*
 * Any changes to rpc_sm_mem_alloc should be parralleled in rpc_ss_mem_alloc
 *    Changed to avoid leaving mutex locked in rpc_ss_allocate
 *    6-15-94 Simcoe
 */

byte_p_t rpc_sm_mem_alloc
(
    rpc_ss_mem_handle *handle,
    unsigned bytes,
    error_status_t *st
)
{
    /*
     * In addition to the memory requested by the user, allocate space for
     * the header, gap, and 8-byte alignment
     */
    byte_p_t data_addr;
    header *new;

#ifdef PERFMON
    RPC_SM_MEM_ALLOC_N;
#endif

    new = (header *)malloc(sizeof(header) + GAP + 7 + bytes);

#ifdef DEBUG_VERBOSE
    printf("Allocated %d bytes at %lx\n", sizeof(header) + GAP + 7 + bytes,
           new);
#endif

    if (new == NULL)
    {
      *st = rpc_s_no_memory;
      return(NULL);
    }
    else *st = error_status_ok;

    if (handle->memory) ((header *)(handle->memory))->prev = new;

    new->head = handle;
    new->prev = NULL;
    new->next = (header *)handle->memory;
    handle->memory = (rpc_void_p_t)new;

    /*
     * Skip the header, and gap and align to an 8-byte boundary
     */
    data_addr = (byte_p_t)
        (((char *)new - (char *)0 + sizeof(header) + GAP + 7) & ~7);

    /*
     * Stash away the buffer address for _release and _dealloc
     */
    *((header **)((char *)data_addr - GAP)) = new;

#ifdef DEBUG_VERBOSE
    printf("Returning %lx\n", data_addr);
#endif

#ifdef PERFMON
    RPC_SM_MEM_ALLOC_X;
#endif

    return data_addr;
}

void rpc_ss_mem_free
(
    rpc_ss_mem_handle *handle
)
{
    header *tmp;

#ifdef PERFMON
    RPC_SS_MEM_FREE_N;
#endif

    while (handle->memory)
    {
        tmp = (header *)handle->memory;
        handle->memory = (rpc_void_p_t)((header *)handle->memory)->next;

#ifdef DEBUG_VERBOSE
        printf("Freeing %lx\n", tmp);
#endif

        free((byte_p_t)tmp);
    }

#ifdef PERFMON
    RPC_SS_MEM_FREE_X;
#endif

}

void rpc_ss_mem_release
(
    rpc_ss_mem_handle *handle,
    byte_p_t data_addr,
    int freeit
)
{
    header *this = *(header **)((char *)data_addr - GAP);

#ifdef PERFMON
    RPC_SS_MEM_RELEASE_N;
#endif

#ifdef DEBUG_VERBOSE
    printf("Releasing %lx\n", this);
#endif

    if (this->next) this->next->prev = this->prev;
    if (this->prev) this->prev->next = this->next;
    else
    {
        /*
         * We've deleted the first element of the list
         */
        handle->memory = (rpc_void_p_t)this->next;
        if (this->next) this->next->head = handle;
    }
    if (freeit) free((byte_p_t)this);

#ifdef PERFMON
    RPC_SS_MEM_RELEASE_X;
#endif

}

#ifdef MIA
void rpc_ss_mem_item_free
(
    rpc_ss_mem_handle *handle,
    byte_p_t data_addr
)
{
    header *this = *(header **)((char *)data_addr - GAP);

#ifdef PERFMON
    RPC_SS_MEM_ITEM_FREE_N;
#endif

#ifdef DEBUG_VERBOSE
    printf("Releasing %lx\n", this);
#endif

    if (this->next) this->next->prev = this->prev;
    if (this->prev) this->prev->next = this->next;
    else
    {
        /*
         * We've deleted the first element of the list
         */
        handle->memory = (rpc_void_p_t)this->next;
        if (this->next) this->next->head = handle;
    }
    free((byte_p_t)this);

#ifdef PERFMON
    RPC_SS_MEM_ITEM_FREE_X;
#endif

}
#endif

void rpc_ss_mem_dealloc
(
    byte_p_t data_addr
)
{
    header *tmp = *((header **)((char *)data_addr - GAP));

#ifdef PERFMON
    RPC_SS_MEM_DEALLOC_N;
#endif

    /*
     * find first element of list to get to handle
     */
    while (tmp->prev) tmp = tmp->prev;

    rpc_ss_mem_release(tmp->head, data_addr, 1);

#ifdef PERFMON
    RPC_SS_MEM_DEALLOC_X;
#endif

}

#if 0
void traverse_list(rpc_ss_mem_handle handle)
{
    printf("List contains:");
    while (handle)
    {
        printf(" %d", handle);
        handle = ((header *)handle)->next;
    }
    printf(" (done)\n");
}

void main()
{
    char buf[100];
    byte_p_t tmp, *buff_addr;
    rpc_ss_mem_handle handle = NULL;

    do
    {
        printf("q/a bytes/f addr/d addr:");
        gets(buf);
        if (*buf == 'q')
        {
            rpc_ss_mem_free(&handle);
            exit();
        }
        if (*buf == 'a')
            if ((tmp = rpc_ss_mem_alloc(&handle, atoi(buf+2))) == NULL)
                printf("\tCouldn't get memory\n");
                else printf("\tGot %d\n", tmp);
        if (*buf == 'f')
            rpc_ss_mem_release(&handle, (byte_p_t)atoi(buf+2), 1);
        if (*buf == 'd')
            rpc_ss_mem_dealloc((byte_p_t)atoi(buf+2));
        traverse_list(handle);
    } while (*buf != 'q');
}
#endif
