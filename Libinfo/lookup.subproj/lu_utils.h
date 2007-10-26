/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Useful macros and other stuff for generic lookups
 * Copyright (C) 1989 by NeXT, Inc.
 */

#ifndef _LU_UTILS_H_
#define _LU_UTILS_H_

#include "DSlibinfoMIG_types.h"
#include "kvbuf.h"
#include <stdarg.h>

#define _li_data_key_alias       10010
#define _li_data_key_bootp       10020
#define _li_data_key_bootparams  10030
#define _li_data_key_fstab       10040
#define _li_data_key_group       10050
#define _li_data_key_host        10060
#define _li_data_key_netgroup    10070
#define _li_data_key_network     10080
#define _li_data_key_printer     10090
#define _li_data_key_protocol    10100
#define _li_data_key_rpc         10110
#define _li_data_key_service     10120
#define _li_data_key_user        10130

/*
 * Return values for LI_L1_cache_check.
 */
#define LI_L1_CACHE_OK 0
#define LI_L1_CACHE_STALE 1
#define LI_L1_CACHE_DISABLED 2
#define LI_L1_CACHE_FAILED 3

struct li_thread_info
{
	void *li_entry;
	size_t li_entry_size;
	char *li_vm;
	uint32_t li_vm_length;
	uint32_t li_vm_cursor;
	uint32_t li_flags;
};

extern mach_port_t _ds_port;
extern int _ds_running(void);

/*
 * Thread-local data management
 */
__private_extern__ void *LI_data_find_key(uint32_t key);
__private_extern__ void *LI_data_create_key(uint32_t key, size_t esize);
__private_extern__ void LI_data_set_key(uint32_t key, void *data);
__private_extern__ void *LI_data_get_key(uint32_t key);
__private_extern__ void LI_data_free_kvarray(struct li_thread_info *tdata);
__private_extern__ void LI_data_recycle(struct li_thread_info *tdata, void *entry, size_t entrysize);
__private_extern__ void *LI_ils_create(char *fmt, ...);
__private_extern__ int LI_ils_free(void *ils, size_t len);

kern_return_t _lookup_link(mach_port_t server, char *name, int *procno);
kern_return_t _lookup_one(mach_port_t server, int proc, char *indata, mach_msg_type_number_t indataCnt, char *outdata, mach_msg_type_number_t *outdataCnt);
kern_return_t _lookup_all(mach_port_t server, int proc, char *indata, mach_msg_type_number_t indataCnt, char **outdata, mach_msg_type_number_t *outdataCnt);
kern_return_t _lookup_ooall(mach_port_t server, int proc, char *indata, mach_msg_type_number_t indataCnt,  char **outdata, mach_msg_type_number_t *outdataCnt);

/*
 * Directory Service queries
 */
kern_return_t LI_DSLookupGetProcedureNumber(const char *name, int *procno);

__private_extern__ kern_return_t LI_DSLookupQuery(int32_t proc, kvbuf_t *request, kvarray_t **reply);
__private_extern__ void *LI_getent(const char *procname, int *procnum, void *(*extract)(kvarray_t *), int tkey, size_t esize);
__private_extern__ void *LI_getone(const char *procname, int *procnum, void *(*extract)(kvarray_t *), const char *key, const char *val);

/*
 * L1 cache
 * Takes _li_data_key_xxx as an argument.
 * Returns 0 is the cache is valid, non-zero if it is invalid.
 */
__private_extern__ int LI_L1_cache_check(int tkey);

/*
 * Async support
 */
void LI_async_call_cancel(mach_port_t p, void **context);
kern_return_t LI_async_handle_reply(mach_msg_header_t *msg, kvarray_t **reply, void **callback, void **context);
kern_return_t LI_async_receive(mach_port_t p, kvarray_t **reply);
kern_return_t LI_async_send(mach_port_t *p, uint32_t proc, kvbuf_t *query);
kern_return_t LI_async_start(mach_port_t *p, uint32_t proc, kvbuf_t *query, void *callback, void *context);

/*
 * kvbuf query support
 */
__private_extern__ kvbuf_t *kvbuf_query(char *fmt, ...);
__private_extern__ kvbuf_t *kvbuf_query_key_int(const char *key, int32_t i);
__private_extern__ kvbuf_t *kvbuf_query_key_uint(const char *key, uint32_t u);
__private_extern__ kvbuf_t *kvbuf_query_key_val(const char *key, const char *val);

#endif /* ! _LU_UTILS_H_ */
