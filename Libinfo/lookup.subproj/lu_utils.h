/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Useful macros and other stuff for generic lookups
 * Copyright (C) 1989 by NeXT, Inc.
 */

#ifndef _LU_UTILS_H_
#define _LU_UTILS_H_

#import <netinfo/lookup_types.h>
#include <netinfo/ni.h>
#include <stdarg.h>

#define LU_COPY_STRING(x) strdup(((x) == NULL) ? "" : x)

#define LU_LONG_STRING_LENGTH 8192

#define _lu_data_key_alias       10010
#define _lu_data_key_bootp       10020
#define _lu_data_key_bootparams  10030
#define _lu_data_key_fstab       10040
#define _lu_data_key_group       10050
#define _lu_data_key_host        10060
#define _lu_data_key_netgroup    10070
#define _lu_data_key_network     10080
#define _lu_data_key_printer     10090
#define _lu_data_key_protocol    10100
#define _lu_data_key_rpc         10110
#define _lu_data_key_service     10120
#define _lu_data_key_user        10130

struct lu_thread_info
{
	void *lu_entry;
	XDR *lu_xdr;
	char *lu_vm;
	unsigned int lu_vm_length;
	unsigned int lu_vm_cursor;
};

extern mach_port_t _lu_port;
extern unit *_lookup_buf;
extern int _lu_running(void);

void *_lu_data_create_key(unsigned int key, void (*destructor)(void *));
void _lu_data_set_key(unsigned int key, void *data);
void *_lu_data_get_key(unsigned int key);
void _lu_data_free_vm_xdr(struct lu_thread_info *tdata);

int _lu_xdr_attribute(XDR *xdr, char **key, char ***val, unsigned int *count);

ni_proplist *_lookupd_xdr_dictionary(XDR *inxdr);
int lookupd_query(ni_proplist *l, ni_proplist ***out);
ni_proplist *lookupd_make_query(char *cat, char *fmt, ...);
void ni_property_merge(ni_property *a, ni_property *b);
void ni_proplist_merge(ni_proplist *a, ni_proplist *b);

kern_return_t _lookup_link(mach_port_t server, lookup_name name, int *procno);
kern_return_t _lookup_one(mach_port_t server, int proc, inline_data indata, mach_msg_type_number_t indataCnt, inline_data outdata, mach_msg_type_number_t *outdataCnt);
kern_return_t _lookup_all(mach_port_t server, int proc, inline_data indata, mach_msg_type_number_t indataCnt, ooline_data *outdata, mach_msg_type_number_t *outdataCnt);
kern_return_t _lookup_ooall(mach_port_t server, int proc, ooline_data indata, mach_msg_type_number_t indataCnt, ooline_data *outdata, mach_msg_type_number_t *outdataCnt);

#endif /* ! _LU_UTILS_H_ */
