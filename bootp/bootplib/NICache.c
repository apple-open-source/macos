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
 * NICache.c
 * - netinfo host cache routines
 */

/*
 * Ethernet address <-> IP address binding in netinfo host entry
 *
 * The en_address and ip_address properties are treated as parallel arrays. 
 * If there are fewer ip_address properties than en_address properties, then 
 * there's a choice involved.  We calculate the ip_address index modulo 
 * n_elements(ip_address) to ensure that we don't walk off the end of its array.
 * This is best explained by some examples.
 *
 * Example 1 (two network interfaces, same IP address):
 * en[] = {0:1:2:3:4:1, 0:1:2:3:4:2}
 * ip[] = {17.202.42.112}
 * 
 * implies
 * en[0] <-> ip[0] 
 * en[1] <-> ip[0]
 * 
 * Example 2 (three network interfaces, two addresses specified):
 * en[] = {0:1:2:3:4:1, 0:1:2:3:4:2, 0:1:2:3:4:3}
 * ip[] = {17.202.42.112, 17.202.42.110}
 * 
 * implies
 * en[0] <-> ip[0]
 * en[1] <-> ip[1]
 * en[2] <-> ip[0]
 *
 * Example 3 (one network interface, two addresses specified):
 * en[] = {0:1:2:3:4:1}
 * ip[] = {17.202.42.112, 17.202.42.110}
 * 
 * implies:
 * en[0] <-> ip[0]
 */

#import <unistd.h>
#import <stdlib.h>
#import <stdio.h>
#import <sys/stat.h>
#import <sys/socket.h>
#import <sys/ioctl.h>
#import <sys/file.h>
#import <sys/time.h>
#import <sys/types.h>
#import <net/if.h>
#import <netinet/in.h>
#import <netinet/in_systm.h>
#import <netinet/ip.h>
#import <netinet/udp.h>
#import <netinet/bootp.h>
#import <net/ethernet.h>
#import <netinet/if_ether.h>
#import <net/if_arp.h>
#import <mach/boolean.h>
#import <errno.h>
#import <ctype.h>
#import <arpa/inet.h>
#import <netinfo/ni.h>
#import <string.h>
#import <syslog.h>
#import "dprintf.h"
#import "NICache.h"
#import "NICachePrivate.h"
#import "util.h"

extern struct ether_addr *	ether_aton(char *);

#ifdef NICACHE_TEST
#define TIMESTAMPS
#endif NICACHE_TEST
#ifdef READ_TEST
#define TIMESTAMPS
#endif READ_TEST

#ifdef TIMESTAMPS
static __inline__ void
S_timestamp(char * msg)
{
    timestamp_printf(msg);
}
#else NICACHE_TEST
static __inline__ void
S_timestamp(char * msg)
{
}
#endif TIMESTAMPS

/**
 ** Module: PLCacheEntry
 **/

PLCacheEntry_t *
PLCacheEntry_create(ni_id dir, ni_proplist pl)
{
    PLCacheEntry_t * entry = malloc(sizeof(*entry));

    if (entry == NULL)
	return (NULL);
    entry->pl = ni_proplist_dup(pl);
    entry->dir = dir;
    entry->next = entry->prev = NULL;
    return (entry);
}

void
PLCacheEntry_free(PLCacheEntry_t * ent)
{
    ni_proplist_free(&ent->pl);
    bzero(ent, sizeof(*ent));
    free(ent);
    return;
}

/**
 ** Module: PLCache
 **/

void
PLCache_print(PLCache_t * cache)
{
    int			i;
    PLCacheEntry_t *	scan;

    printf("PLCache contains %d elements\n", cache->count);
    for (i = 0, scan = cache->head; scan; scan = scan->next, i++) {
	printf("\nEntry %d\n", i);
	ni_proplist_dump(&scan->pl);
    }
}

void
PLCache_init(PLCache_t * cache)
{
    bzero(cache, sizeof(*cache));
    cache->max_entries = CACHE_MAX;
    return;
}

int
PLCache_count(PLCache_t * c)
{
    return (c->count);
}

void
PLCache_set_max(PLCache_t * c, int m)
{
    if (m < CACHE_MIN)
	m = CACHE_MIN;
    c->max_entries = m;
    if (c->count > c->max_entries) {
	int 			i;
	int 			num = c->count - c->max_entries;
	PLCacheEntry_t * 	prev;
	PLCacheEntry_t * 	scan;

	dprintf(("Count %d max %d, removing %d\n", c->count, c->max_entries,
		 num));

	/* drop num items from the cache */
	prev = NULL;
	scan = c->tail;
	for (i = 0; i < num; i++) {
	    dprintf(("Deleting %d\n", i));
	    prev = scan->prev;
	    PLCacheEntry_free(scan);
	    scan = prev;
	}
	c->tail = prev;
	if (c->tail) {
	    c->tail->next = NULL;
	}
	else {
	    c->head = NULL;
	}
	c->count = c->max_entries;
    }
    return;
}

void
PLCache_free(PLCache_t * cache)
{
    PLCacheEntry_t * scan = cache->head;

    for (scan = cache->head; scan; ) {
	PLCacheEntry_t * next;

	next = scan->next;
	PLCacheEntry_free(scan);
	scan = next;
	dprintf(("deleting %d\n", ++i));
    }
    bzero(cache, sizeof(*cache));
    return;
}

void
PLCache_add(PLCache_t * cache, PLCacheEntry_t * entry)
{
    if (entry == NULL)
	return;

    entry->next = cache->head;
    entry->prev = NULL;
    if (cache->head == NULL) {
	cache->head = cache->tail = entry;
    }
    else {
	cache->head->prev = entry;
	cache->head = entry;
    }
    cache->count++;
    return;
}

void
PLCache_append(PLCache_t * cache, PLCacheEntry_t * entry)
{
    if (entry == NULL)
	return;
    
    entry->next = NULL;
    entry->prev = cache->tail;
    if (cache->head == NULL) {
	cache->head = cache->tail = entry;
    }
    else {
	cache->tail->next = entry;
	cache->tail = entry;
    }
    cache->count++;
    return;
}

boolean_t
PLCache_read(PLCache_t * cache, u_char * filename)
{
    FILE *	file = NULL;
    int		line_number = 0;
    char	line[1024];
    ni_proplist	pl;
    enum { 
	nowhere_e,
	start_e, 
	body_e, 
	end_e 
    }		where = nowhere_e;

    NI_INIT(&pl);
    file = fopen(filename, "r");
    if (file == NULL) {
	perror(filename);
	goto failed;
    }

    while (1) {
	if (fgets(line, sizeof(line), file) != line) {
	    if (where == start_e || where == body_e) {
		fprintf(stderr, "file ends prematurely\n");
	    }
	    break;
	}
	line_number++;
	if (strcmp(line, "{\n") == 0) {
	    if (where != end_e && where != nowhere_e) {
		fprintf(stderr, "unexpected '{' at line %d\n", 
			line_number);
		goto failed;
	    }
	    where = start_e;
	}
	else if (strcmp(line, "}\n") == 0) {
	    if (where != start_e && where != body_e) {
		fprintf(stderr, "unexpected '}' at line %d\n", 
			line_number);
		goto failed;
	    }
	    if (pl.nipl_len > 0) {
		ni_id dir = {0,0};

		PLCache_append(cache, PLCacheEntry_create(dir, pl));
		ni_proplist_free(&pl);
	    }
	    where = end_e;
	}
	else {
	    char	propname[128];
	    char	propval[768] = "";
	    int 	len = strlen(line);
	    char *	sep = strchr(line, '=');
	    int 	whitespace_len = strspn(line, " \t\n");

	    if (whitespace_len == len) {
		continue;
	    }
	    if (sep) {
		int nlen = (sep - line) - whitespace_len;
		int vlen = len - whitespace_len - nlen - 2;

		strncpy(propname, line + whitespace_len, nlen);
		propname[nlen] = '\0';
		strncpy(propval, sep + 1, vlen);
		propval[vlen] = '\0';
		ni_proplist_insertprop(&pl, propname, propval, NI_INDEX_NULL);
	    }
	    else {
		int nlen = len - whitespace_len - 1;

		strncpy(propname, line + whitespace_len, nlen);
		propname[nlen] = '\0';
		ni_proplist_insertprop(&pl, propname, NULL, NI_INDEX_NULL);
	    }
	    where = body_e;
	}
    }

 failed:
    if (file)
	fclose(file);
    ni_proplist_free(&pl);
    return (TRUE);
}

boolean_t
PLCache_write(PLCache_t * cache, u_char * filename)
{
    FILE *		file = NULL;
    PLCacheEntry_t *	scan;
    char		tmp_filename[256];

    sprintf(tmp_filename, "%s-", filename);
    file = fopen(tmp_filename, "w");
    if (file == NULL) {
	perror(tmp_filename);
	return (FALSE);
    }

    for (scan = cache->head; scan; scan = scan->next) {
	int i;

	fprintf(file, "{\n");
	for (i = 0; i < scan->pl.nipl_len; i++) {
	    ni_property * prop = &(scan->pl.nipl_val[i]);
	    ni_namelist * nl_p = &prop->nip_val;
	    if (nl_p->ninl_len == 0) {
		fprintf(file, "\t%s\n", prop->nip_name);
	    }
	    else {
		fprintf(file, "\t%s=%s\n", prop->nip_name,
			nl_p->ninl_val[0]);
	    }
	}
	fprintf(file, "}\n");
    }
    fclose(file);
    rename(tmp_filename, filename);
    return (TRUE);
}

void
PLCache_remove(PLCache_t * cache, PLCacheEntry_t * entry)
{
    if (entry == NULL)
	return;

    if (entry->prev)
	entry->prev->next = entry->next;
    if (entry->next)
	entry->next->prev = entry->prev;
    if (entry == cache->head) {
	cache->head = cache->head->next;
    }
    if (entry == cache->tail) {
	cache->tail = cache->tail->prev;
    }
    entry->next = entry->prev = NULL;
    cache->count--;
    return;
}

void
PLCache_make_head(PLCache_t * cache, PLCacheEntry_t * entry)
{
    if (entry == cache->head)
	return; /* already the head */

    PLCache_remove(cache, entry);
    PLCache_add(cache, entry);
}

PLCacheEntry_t *
PLCache_lookup_prop(PLCache_t * PLCache, char * prop, char * value, boolean_t make_head)
{
    PLCacheEntry_t * scan;

    for (scan = PLCache->head; scan; scan = scan->next) {
	int		name_index;

	name_index = ni_proplist_match(scan->pl, prop, value);
	if (name_index != NI_INDEX_NULL) {
	    if (make_head) {
		PLCache_make_head(PLCache, scan);
	    }
	    return (scan);
	}
    }
    return (NULL);
}

PLCacheEntry_t *
PLCache_lookup_hw(PLCache_t * PLCache, 
		  u_char hwtype, void * hwaddr, int hwlen,
		  NICacheFunc_t * func, void * arg,
		  struct in_addr * client_ip,
		  boolean_t * has_binding)
{
    struct ether_addr *	en_search = (struct ether_addr *)hwaddr;
    PLCacheEntry_t *	scan;

    if (has_binding)
	*has_binding = FALSE;
    for (scan = PLCache->head; scan; scan = scan->next) {
	ni_namelist *	en_nl_p;
	int 		n;
	int		en_index;
	int		ip_index;
	ni_namelist *	ip_nl_p;

	en_index = ni_proplist_match(scan->pl, NIPROP_ENADDR, NULL);
	if (en_index == NI_INDEX_NULL)
	    continue;
	ip_index = ni_proplist_match(scan->pl, NIPROP_IPADDR, NULL);
	if (ip_index == NI_INDEX_NULL)
	    continue;

	en_nl_p = &scan->pl.nipl_val[en_index].nip_val;
	ip_nl_p = &scan->pl.nipl_val[ip_index].nip_val;
	if (en_nl_p->ninl_len == 0 || ip_nl_p->ninl_len == 0)
	    continue;
	for (n = 0; n < en_nl_p->ninl_len; n++) {
	    struct ether_addr * en_p = ether_aton(en_nl_p->ninl_val[n]);
	    if (en_p == NULL)
		continue;
	    if (bcmp(en_p, en_search, sizeof(*en_search)) == 0) {
		int		which_ip; 

		which_ip = n % ip_nl_p->ninl_len;
		if (inet_aton(ip_nl_p->ninl_val[which_ip], client_ip) == 0)
		    continue;
		if (has_binding)
		    *has_binding = TRUE;
		if (func == NULL || (*func)(arg, *client_ip)) {
		    PLCache_make_head(PLCache, scan);
		    return (scan);
		}
	    }
	}
    }
    return (NULL);
}


PLCacheEntry_t *
PLCache_lookup_identifier(PLCache_t * PLCache, 
			 char * idstr, NICacheFunc_t * func, void * arg,
			 struct in_addr * client_ip,
			 boolean_t * has_binding)
{
    PLCacheEntry_t *	scan;

    if (has_binding)
	*has_binding = FALSE;
    for (scan = PLCache->head; scan; scan = scan->next) {
	ni_namelist *	ident_nl_p;
	int 		n;
	int		ident_index;
	int		ip_index;
	ni_namelist *	ip_nl_p;

	ident_index = ni_proplist_match(scan->pl, NIPROP_IDENTIFIER, 
					NULL);
	if (ident_index == NI_INDEX_NULL)
	    continue;
	ident_nl_p = &scan->pl.nipl_val[ident_index].nip_val;
	if (ident_nl_p->ninl_len == 0)
	    continue;

	if (client_ip == NULL) { /* don't care about IP binding */
	    if (strcmp(ident_nl_p->ninl_val[0], idstr) == 0) {
		if (has_binding)
		    *has_binding = TRUE;
		PLCache_make_head(PLCache, scan);
		return (scan);
	    }
	}
	    
	ip_index = ni_proplist_match(scan->pl, NIPROP_IPADDR, NULL);
	if (ip_index == NI_INDEX_NULL)
	    continue;
	ip_nl_p = &scan->pl.nipl_val[ip_index].nip_val;
	if (ip_nl_p->ninl_len == 0)
	    continue;

	for (n = 0; n < ident_nl_p->ninl_len; n++) {
	    if (strcmp(ident_nl_p->ninl_val[n], idstr) == 0) {
		int		which_ip; 
		
		which_ip = n % ip_nl_p->ninl_len;
		if (inet_aton(ip_nl_p->ninl_val[which_ip], client_ip) == 0)
		    continue;
		if (has_binding)
		    *has_binding = TRUE;
		if (func == NULL || (*func)(arg, *client_ip)) {
		    PLCache_make_head(PLCache, scan);
		    return (scan);
		}
	    }
	}
    }
    return (NULL);
}


PLCacheEntry_t *
PLCache_lookup_ip(PLCache_t * PLCache, struct in_addr iaddr)
{
    PLCacheEntry_t *	scan;

    for (scan = PLCache->head; scan; scan = scan->next) {
	int 		n;
	int		ip_index;
	ni_namelist *	ip_nl_p;

	ip_index = ni_proplist_match(scan->pl, NIPROP_IPADDR, NULL);
	if (ip_index == NI_INDEX_NULL)
	    continue;
	ip_nl_p = &scan->pl.nipl_val[ip_index].nip_val;
	for (n = 0; n < ip_nl_p->ninl_len; n++) {
	    struct in_addr entry_ip;
	    if (inet_aton(ip_nl_p->ninl_val[n], &entry_ip) == 0)
		continue;
	    if (iaddr.s_addr == entry_ip.s_addr) {
		PLCache_make_head(PLCache, scan);
		return (scan);
	    }
	}
    }
    return (NULL);
}

/**
 ** Module: IDCacheEntry
 **/

IDCacheEntry_t *
IDCacheEntry_create(u_char hwtype, void * hwaddr, int hwlen)
{
    IDCacheEntry_t * entry = malloc(sizeof(*entry));

    if (entry == NULL)
	return (NULL);

    bzero(entry, sizeof(*entry));
    if (hwtype == ARPHRD_ETHER)
	bcopy(hwaddr, &entry->hwaddr.en, sizeof(entry->hwaddr.en));
    else if (hwlen > 0) {
	entry->hwaddr.other = malloc(hwlen);
	if (entry->hwaddr.other)
	    bcopy(hwaddr, entry->hwaddr.other, hwlen);
    }
    entry->hwtype = hwtype;
    entry->hwlen = hwlen;
    entry->next = entry->prev = NULL;
    return (entry);
}

void
IDCacheEntry_free(IDCacheEntry_t * entry)
{
    if (entry->hwtype != ARPHRD_ETHER && entry->hwaddr.other) {
	free(entry->hwaddr.other);
    }
    bzero(entry, sizeof(*entry));
    free(entry);
    return;
}

/**
 ** Module: IDCache
 **/

void
IDCache_print(IDCache_t * cache)
{
    int			i;
    IDCacheEntry_t *	scan;

    printf("The negative cache contains %d elements\n", cache->count);
    for (i = 0, scan = cache->head; scan; scan = scan->next, i++) {
	if (scan->hwtype == ARPHRD_ETHER) {
	    printf("%3d. ethernet %s\n", i + 1, ether_ntoa(&scan->hwaddr.en));
	}
	else {
	    printf("%3d. type %d addr %s len %d\n", i + 1, scan->hwtype,
		   ether_ntoa((struct ether_addr *)scan->hwaddr.other),
		   scan->hwlen);
	}
    }
}

void
IDCache_init(IDCache_t * cache)
{
    bzero(cache, sizeof(*cache));
    cache->max_entries = CACHE_MAX;
    return;
}

int
IDCache_count(IDCache_t * c)
{
    return (c->count);
}

void
IDCache_set_max(IDCache_t * c, int m)
{
    if (m < CACHE_MIN)
	m = CACHE_MIN;
    c->max_entries = m;
    if (c->count > c->max_entries) {
	int 			i;
	int 			num = c->count - c->max_entries;
	IDCacheEntry_t * 	prev;
	IDCacheEntry_t * 	scan;

	dprintf(("Count %d max %d, removing %d\n", c->count, c->max_entries,
		 num));

	/* drop num items from the cache */
	prev = NULL;
	scan = c->tail;
	for (i = 0; i < num; i++) {
	    dprintf(("Deleting %d\n", i));
	    prev = scan->prev;
	    IDCacheEntry_free(scan);
	    scan = prev;
	}
	c->tail = prev;
	if (c->tail) {
	    c->tail->next = NULL;
	}
	else {
	    c->head = NULL;
	}
	c->count = c->max_entries;
    }
    return;
}

void
IDCache_free(IDCache_t * cache)
{
    IDCacheEntry_t * scan = cache->head;

    for (scan = cache->head; scan; ) {
	IDCacheEntry_t * next;

	next = scan->next;
	IDCacheEntry_free(scan);
	scan = next;
	dprintf(("deleting %d\n", ++i));
    }
    bzero(cache, sizeof(*cache));
    return;
}

void
IDCache_add(IDCache_t * cache, IDCacheEntry_t * entry)
{
    if (entry == NULL)
	return;

    entry->next = cache->head;
    entry->prev = NULL;
    if (cache->head == NULL) {
	cache->head = cache->tail = entry;
    }
    else {
	cache->head->prev = entry;
	cache->head = entry;
    }
    cache->count++;
    return;
}
void
IDCache_remove(IDCache_t * cache, IDCacheEntry_t * entry)
{
    if (entry == NULL)
	return;

    if (entry->prev)
	entry->prev->next = entry->next;
    if (entry->next)
	entry->next->prev = entry->prev;
    if (entry == cache->head) {
	cache->head = cache->head->next;
    }
    if (entry == cache->tail) {
	cache->tail = cache->tail->prev;
    }
    entry->next = entry->prev = NULL;
    cache->count--;
    return;
}

void
IDCache_make_head(IDCache_t * cache, IDCacheEntry_t * entry)
{
    if (entry == cache->head)
	return; /* already the head */

    IDCache_remove(cache, entry);
    IDCache_add(cache, entry);
}

IDCacheEntry_t *
IDCache_lookup_hw(IDCache_t * IDCache, 
		 u_char hwtype, void * hwaddr, int hwlen)

{
    IDCacheEntry_t *	scan;

    for (scan = IDCache->head; scan; scan = scan->next) {
	if (scan->hwtype == hwtype
	    && scan->hwlen == hwlen) {
	    if (hwtype == ARPHRD_ETHER) {
		if (bcmp(hwaddr, &scan->hwaddr.en, 
			 sizeof(scan->hwaddr.en)) == 0) {
		    IDCache_make_head(IDCache, scan);
		    return (scan);
		}
	    }
	    else if (hwlen && bcmp(hwaddr, scan->hwaddr.other, hwlen) == 0) {
		IDCache_make_head(IDCache, scan);
		return (scan);
	    }
	}     
    }
    return (NULL);
}

/**
 ** Module: NIHostCache
 **/
void
NIHostCache_print(NIHostCache_t * cache)
{
    IDCache_print(&cache->neg);
    PLCache_print(&cache->pos);
}

int
compare_ni_entry(const void * val1, const void * val2)
{
    ni_entry * entry1 = (ni_entry *)val1;
    ni_entry * entry2 = (ni_entry *)val2;

    if (entry1->id < entry2->id)
	return (-1);
    if (entry1->id > entry2->id)
	return (1);
    return (0);
}

int
compare_en_binding(const void * val1, const void * val2)
{
    en_binding_t *	entry1 = (en_binding_t *)val1;
    en_binding_t *	entry2 = (en_binding_t *)val2;
    return (ether_cmp(&entry1->en_addr, &entry2->en_addr));
}

int
compare_ip_binding(const void * val1, const void * val2)
{
    ip_binding_t *	entry1 = (ip_binding_t *)val1;
    ip_binding_t *	entry2 = (ip_binding_t *)val2;
    return (entry1->ip_addr.s_addr - entry2->ip_addr.s_addr);
}

static ni_entry *
entrylist_find_id(ni_entrylist * list, u_long dir_id)
{
    ni_entry	entry;

    entry.id = dir_id;
    return(bsearch((const void *)&entry,
		   list->ni_entrylist_val, list->ni_entrylist_len, 
		   sizeof(list->ni_entrylist_val[0]),
		   compare_ni_entry));
}

#if 0
static void
en_binding_list_print(en_binding_list_t * list)
{
    int i;

    for (i = 0; i < list->count; i++) {
	printf("%03d. %s %s %ld\n", i,
	       ether_ntoa(&list->list[i].en_addr), 
	       inet_ntoa(list->list[i].ip_addr),
	       list->list[i].dir_id);
    }
}

static void
ip_binding_list_print(ip_binding_list_t * list)
{
    int i;

    for (i = 0; i < list->count; i++) {
	printf("%03d. %s %ld\n", i,
	       inet_ntoa(list->list[i].ip_addr),
	       list->list[i].dir_id);
    }
}
#endif 0

static boolean_t
build_id_cache(NIHostCache_t * cache, ni_entrylist * en_list_p,
	       ni_entrylist * ip_list_p)
{
    ni_entry *		en_entry;
    int 		i;
    ni_entry *		ip_entry;
    en_binding_t *	new_en_list = NULL;
    int			new_en_list_count = 0;
    int			new_en_list_size = en_list_p->niel_len;
    ip_binding_t *	new_ip_list = NULL;
    int			new_ip_list_count = 0;
    int			new_ip_list_size = ip_list_p->niel_len;

    qsort(ip_list_p->niel_val, ip_list_p->niel_len, 
	  sizeof(ip_list_p->niel_val[0]), compare_ni_entry);

    new_en_list = malloc(new_en_list_size * sizeof(*new_en_list));
    if (new_en_list == NULL) {
	goto failed;
    }
    for (en_entry = en_list_p->niel_val, i = 0; 
	 i < en_list_p->niel_len; 
	 i++, en_entry++) {
	int 		n;
	ni_namelist * 	en_nl_p;
	ni_namelist * 	ip_nl_p;
	
	ip_entry = entrylist_find_id(ip_list_p, en_entry->id);
	if (ip_entry == NULL) {
	    continue;
	}
	en_nl_p = en_entry->names;
	ip_nl_p = ip_entry->names;
	if (en_nl_p == NULL || ip_nl_p == NULL
	    || en_nl_p->ninl_len == 0 || ip_nl_p->ninl_len == 0)
	    continue;
	for (n = 0; n < en_nl_p->ninl_len; n++) {
	    struct ether_addr * en_p;
	    struct in_addr	ip;
	    int			which_ip; 

	    if (en_nl_p->ninl_val[n] == NULL)
		continue;
	    en_p = ether_aton(en_nl_p->ninl_val[n]);
	    if (en_p == NULL)
		continue;
	    which_ip = n % ip_nl_p->ninl_len;
	    if (inet_aton(ip_nl_p->ninl_val[which_ip], &ip) == 0)
		continue;
	    if (new_en_list_count == new_en_list_size) {
		new_en_list_size += 100;
		new_en_list = realloc(new_en_list,
				      new_en_list_size * sizeof(*new_en_list));
	    }
	    new_en_list[new_en_list_count].dir_id = en_entry->id;
	    new_en_list[new_en_list_count].en_addr = *en_p;
	    new_en_list[new_en_list_count].ip_addr = ip;
	    new_en_list_count++;
	}
    }

    /* build the IP address list */
    new_ip_list = malloc(new_ip_list_size * sizeof(*new_ip_list));
    if (new_ip_list == NULL) {
	goto failed;
    }
    for (ip_entry = ip_list_p->niel_val, i = 0; 
	 i < ip_list_p->niel_len; 
	 i++, ip_entry++) {
	int 		n;
	ni_namelist * 	ip_nl_p;
	
	ip_nl_p = ip_entry->names;
	if (ip_nl_p == NULL || ip_nl_p->ninl_len == 0) {
	    continue;
	}
	for (n = 0; n < ip_nl_p->ninl_len; n++) {
	    struct in_addr	ip;

	    if (ip_nl_p->ninl_val[n] == NULL)
		continue;
	    if (inet_aton(ip_nl_p->ninl_val[n], &ip) == 0)
		continue;
	    if (new_ip_list_count == new_ip_list_size) {
		new_ip_list_size += 100;
		new_ip_list = realloc(new_ip_list,
				      new_ip_list_size * sizeof(*new_ip_list));
	    }
	    new_ip_list[new_ip_list_count].dir_id = ip_entry->id;
	    new_ip_list[new_ip_list_count].ip_addr = ip;
	    new_ip_list_count++;
	}
    }

    if (cache->en_bindings.list != NULL) {
	free(cache->en_bindings.list);
	cache->en_bindings.list = NULL;
    }
    cache->en_bindings.count = new_en_list_count;
    if (new_en_list_count > 0) {
	new_en_list = realloc(new_en_list,
			   new_en_list_count * sizeof(*new_en_list));
	cache->en_bindings.list = new_en_list;
	qsort(new_en_list, new_en_list_count, sizeof(*new_en_list),
	      compare_en_binding);
    }
    else if (new_en_list != NULL) {
	free(new_en_list);
    }
    if (cache->ip_bindings.list != NULL) {
	free(cache->ip_bindings.list);
	cache->ip_bindings.list = NULL;
    }
    cache->ip_bindings.count = new_ip_list_count;
    if (new_ip_list_count > 0) {
	new_ip_list = realloc(new_ip_list,
			   new_ip_list_count * sizeof(*new_ip_list));
	cache->ip_bindings.list = new_ip_list;
	qsort(new_ip_list, new_ip_list_count, sizeof(*new_ip_list),
	      compare_ip_binding);
    }
    else if (new_ip_list != NULL) {
	free(new_ip_list);
    }
    ni_entrylist_free(en_list_p);
    ni_entrylist_free(ip_list_p);
    return (TRUE);

 failed:
    ni_entrylist_free(en_list_p);
    ni_entrylist_free(ip_list_p);
    return (FALSE);
}

void
NIHostCache_refresh(NIHostCache_t * cache, struct timeval * tv_p)
{
    unsigned long 	checksum_before = 0;
    unsigned long 	checksum_after = 0;
    ni_entrylist	en_list;
    int 		i = 0;
    ni_entrylist	ip_list;
    ni_status 		status;
    boolean_t		updated = FALSE;

    if (tv_p && ((tv_p->tv_sec - cache->last_checked.tv_sec) 
		 < cache->check_interval)) {
	/* not time to check yet */
	return;
    }
    
    ni_get_checksum(NIDomain_handle(cache->domain), &checksum_after);
    if (checksum_after == cache->checksum)
	goto done;

    dprintf(("Refilling cache\n"));

    S_timestamp("before pathsearch");
    status = ni_pathsearch(NIDomain_handle(cache->domain), &cache->dir, 
			   NIDIR_MACHINES);
    S_timestamp("after pathsearch");
    if (status != NI_OK) {
	syslog(LOG_INFO, "NIHostCache_refresh: ni_pathsearch %s failed, %s\n",
	       NIDIR_MACHINES, ni_error(status));
	goto done;
    }

    NI_INIT(&en_list);
    NI_INIT(&ip_list);

    while (1) {
#define MAX_LIST_TRY		3

	if (++i > MAX_LIST_TRY)
	    goto done;

	checksum_before = checksum_after;
	dprintf(("Before: checksum = %ld\n", checksum_before));
	S_timestamp("before list - en");
	status = ni_list(NIDomain_handle(cache->domain), 
			 &cache->dir, NIPROP_ENADDR, &en_list);
	S_timestamp("after list - en");
	if (status != NI_OK) {
	    dprintf(("ni_list failed, %s\n", ni_error(status)));
	    continue;
	}
	S_timestamp("before list - ip");
	status = ni_list(NIDomain_handle(cache->domain), 
			 &cache->dir, NIPROP_IPADDR,
			 &ip_list);
	S_timestamp("after list - ip");
	if (status != NI_OK) {
	    ni_entrylist_free(&en_list);
	    dprintf(("ni_list failed, %s\n", ni_error(status)));
	    continue;
	}
	ni_get_checksum(NIDomain_handle(cache->domain), &checksum_after);
	dprintf(("After: checksum = %ld\n", checksum_after));
	if (checksum_before == checksum_after) {
	    break; /* success */
	}
	ni_entrylist_free(&en_list);
	ni_entrylist_free(&ip_list);
    }
    S_timestamp("before build");
    updated = build_id_cache(cache, &en_list, &ip_list);
    S_timestamp("after build");

    /* Update the cache */
    if (updated) {
	PLCache_free(&cache->pos);
	PLCache_init(&cache->pos);

	IDCache_free(&cache->neg);
	IDCache_init(&cache->neg);
	cache->checksum = checksum_after;
    }

 done:
    gettimeofday(&cache->last_checked, 0);
    return;
}

boolean_t
NIHostCache_init(NIHostCache_t * cache, NIDomain_t * domain,
		 unsigned long check_interval)
{
    bzero(cache, sizeof(*cache));
    PLCache_init(&cache->pos);
    IDCache_init(&cache->neg);
    cache->domain = domain;
    ni_get_checksum(NIDomain_handle(domain), &cache->checksum);
    cache->checksum = ~cache->checksum;
    cache->check_interval = check_interval;
    NIHostCache_refresh(cache, NULL);
    return (TRUE);
}

static en_binding_t *
en_binding_lookup(en_binding_list_t * list,
		  struct ether_addr * en_search,
		  NICacheFunc_t * func, void * arg,
		  boolean_t * has_binding)
{
    en_binding_t	en_search_entry;
    en_binding_t *	entry;
    int			i;
    int 		offset;
    en_binding_t *	scan;

    if (has_binding) {
	*has_binding = FALSE;
    }
    en_search_entry.en_addr = *en_search;
    entry = bsearch((const void *)&en_search_entry,
		    list->list, list->count,
		    sizeof(list->list[0]),
		    compare_en_binding);
    if (entry == NULL) {
	return (NULL);
    }
    if (has_binding)
	*has_binding = TRUE;
    if (func == NULL || (*func)(arg, entry->ip_addr)) {
	return (entry);
    }
    /* check if host has multiple entries with same en_address */
    offset = entry - list->list;
    for (scan = entry + 1, i = offset + 1; i < list->count; i++, scan++) {
	if (bcmp(&scan->en_addr, &entry->en_addr, sizeof(entry->en_addr))) {
	    break;
	}
	if (func == NULL || (*func)(arg, scan->ip_addr)) {
	    return (scan);
	}
    }
    for (scan = entry - 1, i = offset - 1; i >= 0; i--, scan--) {
	if (bcmp(&scan->en_addr, &entry->en_addr, sizeof(entry->en_addr))) {
	    break;
	}
	if (func == NULL || (*func)(arg, scan->ip_addr)) {
	    return (scan);
	}
    }
    return (NULL);
}

static ip_binding_t *
ip_binding_lookup(ip_binding_list_t * list,
		  struct in_addr ip)
{
    ip_binding_t	ip_search_entry;

    ip_search_entry.ip_addr = ip;
    return (bsearch((const void *)&ip_search_entry,
		    list->list, list->count,
		    sizeof(list->list[0]),
		    compare_ip_binding));
}

PLCacheEntry_t *
NIHostCache_lookup_hw(NIHostCache_t * cache, struct timeval * tv_p, 
		      u_char hwtype, void * hwaddr, int hwlen,
		      NICacheFunc_t * func, void * arg,
		      struct in_addr * client_ip)
{
    struct ether_addr *	en_search = (struct ether_addr *)hwaddr;
    en_binding_t *	en_entry = NULL;
    boolean_t		has_binding = FALSE;
    PLCacheEntry_t *	scan;
    boolean_t		some_binding = FALSE;

    if (hwtype != ARPHRD_ETHER || hwlen != ETHER_ADDR_LEN)
	return (NULL);

    /* refresh the cache if necessary */
    NIHostCache_refresh(cache, tv_p);

    { /* check negative cache */
	IDCacheEntry_t * entry;
	entry = IDCache_lookup_hw(&cache->neg, hwtype, hwaddr, hwlen);
	if (entry) {
	    IDCache_make_head(&cache->neg, entry);
	    return (NULL);
	}
    }

    /* check the positive cache */
    scan = PLCache_lookup_hw(&cache->pos, hwtype, hwaddr, hwlen, 
			    func, arg, client_ip, &some_binding);
    if (scan)
	return (scan);

    if (some_binding == TRUE)
	has_binding = TRUE;

    /* check the whole list of addresses */
    en_entry = en_binding_lookup(&cache->en_bindings, en_search, 
				 func, arg, &some_binding);
    if (en_entry) {
	ni_proplist	pl;
	ni_id		dir;
	ni_status	status;

	NI_INIT(&pl);
	dir.nii_object = en_entry->dir_id;
	status = ni_read(NIDomain_handle(cache->domain), &dir, &pl);
	if (status == NI_OK) {
	    PLCache_t *		PLCache = &cache->pos;
	    PLCacheEntry_t *	entry = PLCacheEntry_create(dir, pl);

	    PLCache_add(PLCache, entry);
	    PLCache_set_max(PLCache, PLCache->max_entries);
	    ni_proplist_free(&pl);
	    *client_ip = en_entry->ip_addr;
	    return (entry);
	}
    }

    if (some_binding == TRUE) {
	has_binding = TRUE;
    }

    if (has_binding == FALSE) {
	/* create a negative cache entry */
	IDCache_add(&cache->neg, IDCacheEntry_create(hwtype, hwaddr, hwlen));
    }
    return (NULL);
}

PLCacheEntry_t *
NIHostCache_lookup_ip(NIHostCache_t * cache, struct timeval * tv_p, 
		      struct in_addr iaddr)
{
    ip_binding_t *	ip_entry;
    PLCacheEntry_t *	scan;

    /* refresh the cache if necessary */
    NIHostCache_refresh(cache, tv_p);

    /* check the positive cache */
    scan = PLCache_lookup_ip(&cache->pos, iaddr);
    if (scan) {
	return (scan);
    }

    /* check the whole list of ip addresses */
    ip_entry = ip_binding_lookup(&cache->ip_bindings, iaddr);
    if (ip_entry != NULL) {
	ni_proplist	pl;
	ni_id		dir;
	ni_status	status;

	NI_INIT(&pl);
	dir.nii_object = ip_entry->dir_id;
	status = ni_read(NIDomain_handle(cache->domain), &dir, &pl);
	if (status == NI_OK) {
	    PLCache_t *		PLCache = &cache->pos;
	    PLCacheEntry_t *	entry = PLCacheEntry_create(dir, pl);

	    PLCache_add(PLCache, entry);
	    PLCache_set_max(PLCache, PLCache->max_entries);
	    ni_proplist_free(&pl);
	    return (entry);
	}
    }

    return (NULL);
}

boolean_t
NIHostCache_ip_in_use(NIHostCache_t * cache, struct in_addr iaddr)
{
    ip_binding_t *	ip_entry;

    /* check the whole list of ip addresses */
    ip_entry = ip_binding_lookup(&cache->ip_bindings, iaddr);
    if (ip_entry != NULL) {
	return (TRUE);
    }
    return (FALSE);
}

void
NIHostCache_free(void * c)
{
    NIHostCache_t * cache = (NIHostCache_t *)c;

    PLCache_free(&cache->pos);
    IDCache_free(&cache->neg);
    if (cache->en_bindings.list != NULL)
	free(cache->en_bindings.list);
    if (cache->ip_bindings.list != NULL)
	free(cache->ip_bindings.list);
    bzero(cache, sizeof(*cache));
    free(cache);
    return;
}

boolean_t
NICache_init(NICache_t * cache, unsigned long check_interval)
{
    bzero(cache, sizeof(*cache));
    dynarray_init(&cache->list, NIHostCache_free, NULL);
    cache->check_interval = check_interval;
    return (TRUE);
}

void
NICache_free(NICache_t * cache)
{
    dynarray_free(&cache->list);
    return;
}

boolean_t
NICache_add_domain(NICache_t * cache, NIDomain_t * domain)
{
    NIHostCache_t *	element;

    element = malloc(sizeof(*element));
    if (element == NULL)
	goto error;

    if (NIHostCache_init(element, domain, cache->check_interval) == FALSE)
	goto error;

    if (dynarray_add(&cache->list, element) == FALSE)
	goto error;

    return (TRUE);
 error:
    if (cache)
	free(cache);
    return (FALSE);
}

void
NICache_refresh(NICache_t * cache, struct timeval * tv_p)
{
    int i;

    for (i = 0; i < dynarray_count(&cache->list); i++) {
	NIHostCache_t *	element = dynarray_element(&cache->list, i);
	
	NIHostCache_refresh(element, tv_p);
    }
    return;
}

NIHostCache_t *
NICache_host_cache(NICache_t * cache, NIDomain_t * domain)
{
    int i;

    for (i = 0; i < dynarray_count(&cache->list); i++) {
	NIHostCache_t *	element = dynarray_element(&cache->list, i);
	if (element->domain == domain)
	    return (element);
    }
    return (NULL);
    
}

PLCacheEntry_t *	
NICache_lookup_hw(NICache_t * cache, struct timeval * tv_p, 
		  u_char hwtype, void * hwaddr, int hwlen,
		  NICacheFunc_t * func, void * arg,
		  NIDomain_t * * domain_p,
		  struct in_addr * client_ip)
{
    int i;

    for (i = 0; i < dynarray_count(&cache->list); i++) {
	NIHostCache_t *	element = dynarray_element(&cache->list, i);
	PLCacheEntry_t * entry;
	entry = NIHostCache_lookup_hw(element, tv_p, hwtype, hwaddr, hwlen, 
				      func, arg, client_ip);
	if (entry) {
	    if (domain_p)
		*domain_p = element->domain;
	    return (entry);
	}
    }
    return (NULL);
}

PLCacheEntry_t *	
NICache_lookup_ip(NICache_t * cache, struct timeval * tv_p, 
		  struct in_addr iaddr,
		  NIDomain_t * * domain_p)
{
    int i;

    for (i = 0; i < dynarray_count(&cache->list); i++) {
	NIHostCache_t *	element = dynarray_element(&cache->list, i);
	PLCacheEntry_t * entry;
	entry = NIHostCache_lookup_ip(element, tv_p, iaddr);
	if (entry) {
	    if (domain_p)
		*domain_p = element->domain;
	    return (entry);
	}
    }
    return (NULL);
}

boolean_t
NICache_ip_in_use(NICache_t * cache, struct in_addr iaddr,
		  NIDomain_t * * domain_p)
{
    int i;

    for (i = 0; i < dynarray_count(&cache->list); i++) {
	NIHostCache_t *	element = dynarray_element(&cache->list, i);
	if (NIHostCache_ip_in_use(element, iaddr)) {
	    if (domain_p)
		*domain_p = element->domain;
	    return (TRUE);
	}
    }
    return (FALSE);
}


#ifdef READ_TEST
int
main(int argc, char * argv[])
{
    PLCache_t 	PLCache;

    if (argc < 2)
	exit(1);

    PLCache_init(&PLCache);
    PLCache_set_max(&PLCache, 100 * 1024 * 1024);
    S_timestamp("before read");
    if (PLCache_read(&PLCache, argv[1]) == TRUE) {
	S_timestamp("after read");
	PLCache_print(&PLCache);

	printf("writing /tmp/readtest.out\n");
	PLCache_write(&PLCache, "/tmp/readtest.out");
    }
    else {
	S_timestamp("after read");
	printf("PLCache_read failed\n");
    }
    exit(0);
}

#endif READ_TEST

#ifdef NICACHE_TEST
#define NET_INDEX	0
#define MASK_INDEX	1

boolean_t
same_subnet(void * arg, struct in_addr iaddr)
{
    struct in_addr * net_and_mask = (struct in_addr *)arg;

    return (in_subnet(net_and_mask[NET_INDEX], net_and_mask[MASK_INDEX],iaddr));
}

int
main(int argc, char * argv[])
{
    void *		arg = NULL;
    NIDomain_t *	domain;
    NICache_t		cache;
    NICacheFunc_t *	func = NULL;
    NIHostCache_t *	hcache;
    ni_status		status;
    struct in_addr	net_and_mask[2];

    if (argc < 2)
	exit(1);

    if (argc == 4) {
	if (inet_aton(argv[2], &net_and_mask[NET_INDEX]) == 0) {
	    fprintf(stderr, "%s is not a valid IP\n", argv[2]);
	    exit(1);
	}
	if (inet_aton(argv[3], &net_and_mask[MASK_INDEX]) == 0) {
	    fprintf(stderr, "%s is not a valid IP\n", argv[3]);
	    exit(1);
	}
	func = same_subnet;
	arg = net_and_mask;
    }
    S_timestamp("before open");
    domain = NIDomain_init(argv[1]);
    S_timestamp("after open");
    if (domain == NULL) {
	fprintf(stderr, "open %s failed\n", argv[1]);
	exit(1);
    }

    NICache_init(&cache, 60);
    NICache_add_domain(&cache, domain);
    hcache = (NIHostCache_t *)dynarray_element(&cache.list, 0);
    while (1) {
	char	query[128];
	ni_namelist * nl_p;
	struct timeval	tv;

	gettimeofday(&tv, 0);
	
	printf("Enter an address, p=print, q=quit\n");
	if (fgets(query, sizeof(query), stdin) != query)
	    break;
	if (strchr(query, '.')) {
	    PLCacheEntry_t *	entry;
	    struct in_addr 	iaddr;

	    if (inet_aton(query, &iaddr) == 1) {
		S_timestamp("before ip in use");
		if (NICache_ip_in_use(&cache, iaddr, NULL)) {
		    printf("ip address %s is in use\n", 
			   inet_ntoa(iaddr));
		}
		S_timestamp("after ip in use");
		S_timestamp("before ip lookup");
		entry = NICache_lookup_ip(&cache, &tv, iaddr, NULL);
		S_timestamp("after ip lookup");
		if (entry == NULL) {
		    printf("no entry for %s\n", inet_ntoa(iaddr));
		}
		else {
		    printf("entry found for %s\n", inet_ntoa(iaddr));
		    ni_proplist_dump(&entry->pl);
		}
		continue;
	    }
	}
	{
	    PLCacheEntry_t *	entry;
	    struct ether_addr * en_p;
	    struct ether_addr 	en_address;
	    struct in_addr	ip;
	    
	    if (*query == 'k') {
		unsigned long size;

		size = strtoul(query + 1, 0, 0);

		printf("Setting cache size of %ld\n", size);
		PLCache_set_max(&hcache->pos, size);
		continue;
	    }
	    if (*query == '\n')
		continue;
	    if (*query == 'q' || *query == 'Q')
		break;
	    if (*query == 'p' || *query == 'P') {
		NIHostCache_print(hcache);
		continue;
	    }
	    en_p = ether_aton(query);
	    if (en_p == NULL) {
		printf("%.*s is not an en_address\n",
		       strlen(query) - 1, query);
		continue;
	    }
	    en_address = *en_p;
	    S_timestamp("before lookup");
	    entry = NICache_lookup_hw(&cache, &tv, ARPHRD_ETHER, 
				      &en_address, 6,
				      func, arg, NULL, &ip);
	    S_timestamp("after lookup");
	    if (entry == NULL) {
		printf("%.*s not found\n", strlen(query) - 1, query);
	    }
	    else {
		printf("%.*s has IP %s\n", strlen(query) - 1, query,
		       inet_ntoa(ip));
		//printf("Here's the entry:\n");
		//ni_proplist_dump(&entry->pl);
	    }
	}
    }
    NICache_free(&cache);
    exit(0);
}
#endif NICACHE_TEST
