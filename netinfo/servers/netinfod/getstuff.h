/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Lookup various things definitions
 * Copyright (C) 1989 by NeXT, Inc.
 */
unsigned long getaddress(void *, ni_name);
unsigned long getmasteraddr(void *, ni_name *);
int getmaster(void *, ni_name *, ni_name *);
int is_trusted_network(void *, struct sockaddr_in *);
bool_t is_desktop(void *ni);
void get_readall_info(void *, int *, bool_t *);
int get_max_subthreads(void *ni);
int get_update_latency(void *ni);
int get_cleanupwait(void *ni);
int get_localbindattempts(void *ni);
bool_t get_forced_root(void *ni);
bool_t get_clone_readall(void *ni);
void get_logging_info(void *ni, int *facility, int *level);
bool_t get_sanitycheck(void *ni);
unsigned short get_port(void *ni, char *proto);
ni_status get_binding_status(void);
void set_binding_status(ni_status stat);
bool_t get_promote_admins(void *ni);
