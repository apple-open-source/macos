/*
 * Copyright (c) 2006,2011-2014 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "sec_xdr.h"

#define ALIGNMENT sizeof(void*)
#define ALIGNUP(LEN) (((LEN - 1) & ~(ALIGNMENT - 1)) + ALIGNMENT)

/*
 * XDR counted bytes
 * *cpp is a pointer to the bytes, *sizep is the count.
 * If *cpp is NULL maxsize bytes are allocated
 */
bool_t sec_xdr_bytes(XDR *xdrs, uint8_t **cpp, u_int *sizep, u_int maxsize)
{
    uint8_t *sp = cpp ? *cpp : NULL;  /* sp is the actual string pointer */
    u_int nodesize = sizep ? *sizep : 0;

    /*
     * first deal with the length since xdr bytes are counted
     */
    if (! xdr_u_int(xdrs, &nodesize))
        return (FALSE);

    if ((nodesize > maxsize) && (xdrs->x_op != XDR_FREE))
        return (FALSE);

    if (sizep && (xdrs->x_op == XDR_DECODE))
        *sizep = nodesize;

    bool_t sizeof_alloc = sec_xdr_arena_size_allocator(xdrs);

    /*
     * now deal with the actual bytes
     */
    switch (xdrs->x_op) {
    case XDR_DECODE:
        if (nodesize == 0)
            return (TRUE);
        if (!sp) {
            if (!sec_mem_alloc(xdrs, nodesize, &sp))
                return (FALSE);
            if (!sizeof_alloc && cpp != NULL)
                *cpp = sp; /* sp can be NULL when counting required space */
        }
        /* FALLTHROUGH */
    case XDR_ENCODE:
        return (xdr_opaque(xdrs, (char *)sp, nodesize));

    case XDR_FREE:
        if (sp != NULL) {
            sec_mem_free(xdrs, sp, nodesize);
            *cpp = NULL;
        }
        return (TRUE);
    }
    /* NOTREACHED */
    return (FALSE);
}

bool_t sec_xdr_charp(XDR *xdrs, char **cpp, u_int maxsize)
{
    char *sp = cpp ? *cpp : NULL;  /* sp is the actual string pointer */
    u_int size = 0;

    switch (xdrs->x_op) {
    case XDR_FREE:
        if (sp == NULL) return(TRUE);   /* already free */
        sec_mem_free(xdrs, sp, size);
        *cpp = NULL;
        return (TRUE);
    case XDR_ENCODE:
        if (sp) size = (u_int)(strlen(sp) + 1);
        /* FALLTHROUGH */
    case XDR_DECODE:
        return sec_xdr_bytes(xdrs, (uint8_t**)cpp, &size, maxsize);
    }
    /* NOTREACHED */
    return (FALSE);
}

bool_t sec_mem_alloc(XDR *xdr, u_int bsize, uint8_t **data)
{
    if (!xdr || !data)
        return (FALSE);

    assert(xdr->x_op == XDR_DECODE);

    sec_xdr_arena_allocator_t *allocator = sec_xdr_arena_allocator(xdr);
    if (allocator) {
        if (*data != NULL)
            return (TRUE); // no allocation needed
        size_t bytes_left;
        switch(allocator->magic) {
        case xdr_arena_magic: 
            bytes_left = allocator->end - allocator->offset;
            if (bsize > bytes_left)
                return (FALSE);
            else {
                uint8_t *temp = allocator->offset;
                allocator->offset += bsize;
                *data = temp;
                return (TRUE);
            }
        case xdr_size_magic:
            allocator->offset = (uint8_t*)((size_t)allocator->offset + bsize);
            return (TRUE);
        }
    }

    void *alloc = calloc(1, bsize);
    if (!alloc)
        return (FALSE);

    *data = alloc;
    return (TRUE);
}

void sec_mem_free(XDR *xdr, void *ptr, u_int bsize)
{
    if (sec_xdr_arena_allocator(xdr))
        return;

    return free(ptr);
}

static const sec_xdr_arena_allocator_t size_alloc = { xdr_size_magic, 0, 0, 0 };
void sec_xdr_arena_init_size_alloc(sec_xdr_arena_allocator_t *arena, XDR *xdr)
{
    memcpy(arena, &size_alloc, sizeof(size_alloc));
    xdr->x_public = (char *)arena;
}

bool_t sec_xdr_arena_init(sec_xdr_arena_allocator_t *arena, XDR *xdr, 
        size_t in_length, uint8_t *in_data)
{
    if (!xdr)
        return FALSE;
    uint8_t *data = in_data ? in_data : calloc(1, ALIGNUP(in_length));
    if (!data)
        return FALSE;
    arena->magic = xdr_arena_magic;
    arena->offset = data;
    arena->data = data;
    arena->end = data + in_length;
    xdr->x_public = (void*)arena;
    return TRUE;
}

void sec_xdr_arena_free(sec_xdr_arena_allocator_t *arena, 
        void *ptr, size_t bsize)
{
    assert(arena->magic == xdr_arena_magic);
    free(arena->data);
}

void *sec_xdr_arena_data(sec_xdr_arena_allocator_t *arena)
{
    if (arena)
        return arena->data;

    return NULL;
}

sec_xdr_arena_allocator_t *sec_xdr_arena_allocator(XDR *xdr)
{
    sec_xdr_arena_allocator_t *allocator = xdr ? (sec_xdr_arena_allocator_t *)xdr->x_public : NULL;

    if (allocator && 
            (allocator->magic == xdr_arena_magic ||
             allocator->magic == xdr_size_magic))
        return allocator;

    return NULL;
}

bool_t sec_xdr_arena_size_allocator(XDR *xdr)
{
    sec_xdr_arena_allocator_t *allocator = xdr ? (sec_xdr_arena_allocator_t *)xdr->x_public : NULL;
    if (allocator && (allocator->magic == xdr_size_magic))
        return TRUE;

    return FALSE;
}


bool_t copyin(void *data, xdrproc_t proc, void** copy, u_int *size)
{
    if (!copy)
        return (FALSE);

    // xdr_sizeof is illbehaved
    u_int length = sec_xdr_sizeof_in(proc, data);
    uint8_t *xdr_data = malloc(length);
    if (!xdr_data)
        return (FALSE);

    XDR xdr;
    sec_xdrmem_create(&xdr, (char *)xdr_data, length, XDR_ENCODE);

    // cast to void* - function can go both ways (xdr->x_op) 
    if (proc(&xdr, data, 0)) {
        *copy = xdr_data;
        if (size) *size = length;
        return (TRUE);
    }

    free(xdr_data);
    return (FALSE);
}

// Unmarshall xdr data and return pointer to single allocation containing data
// Generally use *_PTR for xdrproc_t taking an objp that matches data's type
// ie. xdr_CSSM_POSSIBLY_KEY_IN_DATA_PTR(XDR *xdrs, CSSM_DATA_PTR *objp)
// with data matching objp 
// If you pass in length pointing to a non-zero value, data will be assumed
// to have pre-allocated space for use by copyout in that amount.  
// If *data is not NULL it will be assumed to be allocated already.  
bool_t copyout(const void *copy, u_int size, xdrproc_t proc, void **data, u_int *length)
{

    if (!data || (size > ~(u_int)0))
        return (FALSE);

    XDR xdr;
    sec_xdrmem_create(&xdr, (void *)copy, size, XDR_DECODE);

    u_int length_required = sec_xdr_sizeof_out(copy, size, proc, data);
    u_int length_out = length ? *length : 0;

    if (length_out && (length_required > length_out))
        return (FALSE);

    bool_t passed_in_data = (*data && length_out);
    sec_xdr_arena_allocator_t arena;
    // set up arena with memory passed in (length_out > 0) or ask to allocate
    if (!sec_xdr_arena_init(&arena, &xdr, length_out ? length_out : length_required, length_out ? *data : NULL))
        return (FALSE);

    if (proc(&xdr, data, 0))
    {
        if(length) {
            *length = length_required;
        }
        return (TRUE);
    }

    if (!passed_in_data)
        sec_xdr_arena_free(sec_xdr_arena_allocator(&xdr), NULL, 0);
    return (FALSE);
}

// unmarshall xdr data and return pointer to individual allocations containing data
// only use *_PTR for xdrproc_ts and pointers for data
bool_t copyout_chunked(const void *copy, u_int size, xdrproc_t proc, void **data)
{
    if (!data || (size > ~(u_int)0))
        return (FALSE);

    XDR xdr;
    sec_xdrmem_create(&xdr, (void *)copy, size, XDR_DECODE);

    void *data_out = NULL;

    if (proc(&xdr, &data_out, 0))
    {
        *data = data_out;
        return (TRUE);
    }
    return (FALSE);
}
