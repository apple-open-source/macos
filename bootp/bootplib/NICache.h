#ifndef _S_NICACHE_H
#define _S_NICACHE_H
/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 * NICache.h
 * - netinfo cache routines
 */

#import "netinfo.h"
#import "dynarray.h"
#import "NIDomain.h"

struct PLCacheEntry;
typedef struct PLCacheEntry PLCacheEntry_t;

struct PLCacheEntry {
    ni_proplist		pl;
    ni_id		dir;
    void *		value1;
    void *		value2;
    PLCacheEntry_t *	next;
    PLCacheEntry_t *	prev;
};

struct IDCacheEntry;
typedef struct IDCacheEntry IDCacheEntry_t;

struct IDCacheEntry {
    u_char		hwtype;		/* hardware type */
    int			hwlen;		/* hardware length */
    union {				/* hardware address */
      struct ether_addr	en;		/* if htype == 1 */
      void *		other;		/* if htype != 1 */
    } hwaddr;
    IDCacheEntry_t *	next;
    IDCacheEntry_t *	prev;
};
struct IDCache {
    IDCacheEntry_t *	head;
    IDCacheEntry_t *	tail;
    int			max_entries;
    int			count;
};
typedef struct IDCache IDCache_t;

struct PLCache {
    PLCacheEntry_t *	head;
    PLCacheEntry_t *	tail;
    int			max_entries;
    int			count;
};
typedef struct PLCache PLCache_t;

typedef struct {
    u_long		dir_id;
    struct ether_addr	en_addr;
    struct in_addr	ip_addr;
} en_binding_t;

typedef struct {
    en_binding_t *	list;
    int			count;
} en_binding_list_t;

typedef struct {
    u_long		dir_id;
    struct in_addr	ip_addr;
} ip_binding_t;

typedef struct {
    ip_binding_t *	list;
    int			count;
} ip_binding_list_t;

typedef struct {
    NIDomain_t *	domain;		/* domain */
    ni_id		dir;		/* /machines dir id */
    unsigned long	checksum;	/* domain database checksum */
    struct timeval	last_checked;	/* time we last checked */

    unsigned long	check_interval;	/* how often to check domain for changes*/
    en_binding_list_t	en_bindings;
    ip_binding_list_t 	ip_bindings;

    PLCache_t		pos; 		/* positive cache */

    IDCache_t		neg;		/* negative cache */
} NIHostCache_t;

typedef struct {
    dynarray_t		list;		/* list of NIHostCache_t's */
    unsigned long	check_interval;	/* how often to check domains for changes*/
} NICache_t;

typedef boolean_t NICacheFunc_t(void * arg, struct in_addr iaddr);


#define CACHE_MIN			10
#define CACHE_MAX			256

boolean_t	NICache_init(NICache_t * cache, unsigned long check_interval);
void		NICache_free(NICache_t * cache);
boolean_t	NICache_add_domain(NICache_t * cache, NIDomain_t * domain);
void		NICache_refresh(NICache_t * cache, 
				struct timeval * tv_p);

PLCacheEntry_t *
		NICache_lookup_hw(NICache_t * cache, 
				  struct timeval * tv_p, 
				  u_char hwtype, void * hwaddr, int hwlen,
				  NICacheFunc_t * func, void * arg,
				  NIDomain_t * * domain_p,
				  struct in_addr * client_ip);

PLCacheEntry_t *	
		NICache_lookup_ip(NICache_t * cache, 
				  struct timeval * tv_p, 
				  struct in_addr iaddr,
				  NIDomain_t * * domain_p);

boolean_t	NICache_ip_in_use(NICache_t * cache, 
				  struct in_addr iaddr,
				  NIDomain_t * * domain_p);

NIHostCache_t *	NICache_host_cache(NICache_t * cache, NIDomain_t * domain);


PLCacheEntry_t *
		NIHostCache_lookup_hw(NIHostCache_t * cache, 
				      struct timeval * tv_p, 
				      u_char hwtype, void * hwaddr, int hwlen,
				      NICacheFunc_t * func, void * arg,
				      struct in_addr * client_ip);

PLCacheEntry_t *
		NIHostCache_lookup_ip(NIHostCache_t * cache, 
				      struct timeval * tv_p, 
				      struct in_addr iaddr);

#endif _S_NICACHE_H
