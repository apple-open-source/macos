/*
 * Copyright (c) 2006,2011,2014 Apple Inc. All Rights Reserved.
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
#ifndef _SEC_XDR_H
#define _SEC_XDR_H

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <stdint.h>
 
__BEGIN_DECLS

extern bool_t sec_xdr_bytes(XDR *, uint8_t **, unsigned int *, unsigned int);
extern bool_t sec_xdr_array(XDR *, uint8_t **, unsigned int *, unsigned int, unsigned int, xdrproc_t);
extern bool_t sec_xdr_charp(XDR *, char **, u_int);
extern bool_t sec_xdr_reference(XDR *xdrs, uint8_t **pp, u_int size, xdrproc_t proc);
extern bool_t sec_xdr_pointer(XDR *xdrs, uint8_t **objpp, u_int obj_size, xdrproc_t xdr_obj);

bool_t sec_mem_alloc(XDR *xdr, u_int bsize, uint8_t **data);
void sec_mem_free(XDR *xdr, void *ptr, u_int bsize);

void sec_xdrmem_create(XDR *xdrs, char *addr, u_int size, enum xdr_op op);

typedef struct sec_xdr_arena_allocator {
    uint32_t magic;
    uint8_t *offset;
    uint8_t *data;
    uint8_t *end;
} sec_xdr_arena_allocator_t;

#define xdr_arena_magic 0xAEA1
#define xdr_size_magic 0xDEAD

void sec_xdr_arena_init_size_alloc(sec_xdr_arena_allocator_t *arena, XDR *xdr);
bool_t sec_xdr_arena_init(sec_xdr_arena_allocator_t *arena, XDR *xdr,
                size_t in_length, uint8_t *in_data);
void sec_xdr_arena_free(sec_xdr_arena_allocator_t *alloc, void *ptr, size_t bsize);
void *sec_xdr_arena_data(sec_xdr_arena_allocator_t *alloc);
sec_xdr_arena_allocator_t *sec_xdr_arena_allocator(XDR *xdr);
bool_t sec_xdr_arena_size_allocator(XDR *xdr);

bool_t copyin(void * data, xdrproc_t proc, void ** copy, u_int * size);
bool_t copyout(const void * copy, u_int size, xdrproc_t proc, void ** data, u_int *length);
bool_t copyout_chunked(const void * copy, u_int size, xdrproc_t proc, void ** data);

u_int sec_xdr_sizeof_in(xdrproc_t func, void * data);
u_int sec_xdr_sizeof_out(const void * copy, u_int size, xdrproc_t func, void ** data);

__END_DECLS

#endif /* !_SEC_XDR_H */
