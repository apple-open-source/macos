/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * ipcfg.h
 * - structure to hold the IP interface configuration
 */

/*
 * Modification History
 * 5/28/99	Dieter Siegmund (dieter@apple.com)
 * - initial version
 */

#import <sys/socket.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/if_ether.h>
#import <mach/boolean.h>

#import "ipconfig_types.h"
#import "dynarray.h"

typedef dynarray_t hostconfig_t;

void			hostconfig_free(hostconfig_t * * h);
hostconfig_t *		hostconfig_read(char * msg);
char *			hostconfig_lookup(hostconfig_t * h, char * prop);

typedef struct ipcfg {
    char		ifname[32];
    ipconfig_method_t	method;
    struct in_addr	addr;
    struct in_addr	mask;
    struct in_addr	broadcast;
} ipcfg_t;

typedef dynarray_t ipcfg_table_t;

void		ipcfg_free(ipcfg_table_t * * t);
ipcfg_table_t *	ipcfg_from_file(char * msg);
ipcfg_t *	ipcfg_lookup(ipcfg_table_t * table, char * ifname, int *where);
void		ipcfg_entry_print(ipcfg_t * ipcfg);
void		ipcfg_print(ipcfg_table_t * t);
ipcfg_t *	ipcfg_element(ipcfg_table_t * t, int i);
int		ipcfg_count(ipcfg_table_t * t);
