#ifndef _S_NICACHE_PRIVATE_H
#define _S_NICACHE_PRIVATE_H
/*
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
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
 * NICachePrivate.h
 */
PLCacheEntry_t *PLCacheEntry_create(ni_id dir, ni_proplist pl);
void		PLCacheEntry_free(PLCacheEntry_t * ent);

void		PLCache_init(PLCache_t * cache);
void		PLCache_free(PLCache_t * cache);
int		PLCache_count(PLCache_t * c);
boolean_t	PLCache_read(PLCache_t * cache, u_char * filename);
boolean_t	PLCache_write(PLCache_t * cache, u_char * filename);
void		PLCache_add(PLCache_t * cache, PLCacheEntry_t * entry);
void		PLCache_append(PLCache_t * cache, PLCacheEntry_t * entry);
void		PLCache_remove(PLCache_t * cache, PLCacheEntry_t * entry);
void		PLCache_set_max(PLCache_t * c, int m);
PLCacheEntry_t *PLCache_lookup_prop(PLCache_t * PLCache, 
				    char * prop, char * value, boolean_t make_head);
PLCacheEntry_t *PLCache_lookup_hw(PLCache_t * PLCache, 
				  u_char hwtype, void * hwaddr, int hwlen,
				  NICacheFunc_t * func, void * arg,
				  struct in_addr * client_ip,
				  boolean_t * has_binding);
PLCacheEntry_t *PLCache_lookup_identifier(PLCache_t * PLCache, 
					  char * idstr, 
					  NICacheFunc_t * func, void * arg,
					  struct in_addr * client_ip,
					  boolean_t * has_binding);
PLCacheEntry_t *PLCache_lookup_ip(PLCache_t * PLCache, struct in_addr iaddr);
void		PLCache_make_head(PLCache_t * cache, PLCacheEntry_t * entry);
void		PLCache_print(PLCache_t * cache);

#endif _S_NICACHE_PRIVATE_H
