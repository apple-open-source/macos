/*
 * Copyright (c) 2008-2009 Apple Inc.  All rights reserved.
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

#include <stdlib.h>
#include <paths.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "si_module.h"

#define _PATH_SI_CONF "/etc/sysinfo.conf"

#define SEARCH_FLAG_MAC				0x00000001
#define SEARCH_FLAG_APPLETV			0x00000002
#define SEARCH_FLAG_IPHONE			0x00000004
#define SEARCH_FLAG_CACHE_ENABLED	0x00010000

typedef struct
{
	si_mod_t **module;
	uint32_t count;
	uint32_t flags;
} search_list_t;

typedef struct
{
	uint32_t flags;
	search_list_t search_list[CATEGORY_COUNT];
	si_mod_t *cache;
	si_mod_t *dns;
	si_mod_t *mdns;
	si_mod_t *file;
	si_mod_t *ds;
} search_si_private_t;

__private_extern__ void si_cache_add_item(si_mod_t *si, si_mod_t *src, si_item_t *item);
__private_extern__ void si_cache_add_list(si_mod_t *si, si_mod_t *src, si_list_t *list);

__private_extern__ char **_fsi_tokenize(char *data, const char *sep, int trailing_empty, int *ntokens);
__private_extern__ char *_fsi_get_line(FILE *fp);

#ifdef DS_AVAILABLE
extern int _ds_running();
#else
static inline int _ds_running(void) { return 0; }
#endif

static __attribute__((noinline)) si_mod_t *
search_get_module(search_si_private_t *pp, int cat, int *n)
{
	int x;

	if ((pp == NULL) || (n == NULL)) return NULL;

	x = *n;
	*n = x + 1;

	/* Use custom search list if available */
	if (x < pp->search_list[cat].count)
	{
		return pp->search_list[cat].module[x];
	}
	
	/* 
	 * Search order:
	 * 1) cache
	 * 2) DS if available, otherwise flat files
	 * 3) mdns (for host lookups only)
	 */
	switch (x)
	{
		case 0: return pp->cache;
		case 1: if (_ds_running()) return pp->ds;
				else return pp->file;
		case 2: return pp->mdns;
		default: return NULL;
	}
}

static si_mod_t *
search_cat_cache(search_si_private_t *pp, int cat)
{
	if (pp == NULL) return NULL;
	if ((cat < 0) || (cat > CATEGORY_COUNT)) return NULL;

	if (pp->search_list[cat].count > 0)
	{
		if (pp->search_list[cat].flags & SEARCH_FLAG_CACHE_ENABLED) return pp->cache;
		return NULL;
	}

	if ((pp->flags & SEARCH_FLAG_MAC) || (pp->flags & SEARCH_FLAG_APPLETV) || (pp->flags & SEARCH_FLAG_IPHONE)) return pp->cache;
	return NULL;
}

static void
search_close(si_mod_t *si)
{
	int i;
	search_si_private_t *pp;

	if (si == NULL) return;
	if (si->private == NULL) return;

	pp = (search_si_private_t *)si->private;

	si_module_release(pp->cache);
	si_module_release(pp->file);
	si_module_release(pp->dns);
	si_module_release(pp->mdns);
	si_module_release(pp->ds);

	for (i = 0; i < CATEGORY_COUNT; i++)
	{
		if (pp->search_list[i].module != NULL)
		{
			free(pp->search_list[i].module);
			pp->search_list[i].module = NULL;
			pp->search_list[i].count = 0;
			pp->search_list[i].flags = 0;
		}
	}

	free(pp);
}

static si_item_t *
search_item_byname(si_mod_t *si, const char *name, int cat, si_item_t *(*call)(si_mod_t *, const char *))
{
	int i;
	search_si_private_t *pp;
	si_item_t *item;
	si_mod_t *src;

	if (si == NULL) return NULL;
	if (call == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		item = call(src, name);
		if (item != NULL)
		{
			si_cache_add_item(search_cat_cache(pp, cat), src, item);
			return item;
		}
	}

	return NULL;
}

static si_item_t *
search_item_bynumber(si_mod_t *si, uint32_t number, int cat, si_item_t *(*call)(si_mod_t *, uint32_t))
{
	int i;
	search_si_private_t *pp;
	si_item_t *item;
	si_mod_t *src;

	if (si == NULL) return NULL;
	if (call == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		item = call(src, number);
		if (item != NULL)
		{
			si_cache_add_item(search_cat_cache(pp, cat), src, item);
			return item;
		}
	}

	return NULL;
}

static si_list_t *
search_list(si_mod_t *si, int cat, si_list_t *(*call)(si_mod_t *))
{
	int i;
	search_si_private_t *pp;
	si_list_t *list, *all;
	si_mod_t *cache, *src;

	if (si == NULL) return NULL;
	if (call == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	cache = search_cat_cache(pp, cat);
	if (cache != NULL)
	{
		list = call(cache);
		if (list != NULL) return list;
	}

	i = 0;

	all = NULL;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		if (src == pp->cache) continue;

		list = call(src);
		if (list == NULL) continue;

		all = si_list_concat(all, list);
		si_list_release(list);
	}

	si_cache_add_list(cache, si, all);
	return all;
}

__private_extern__ si_item_t *
search_user_byname(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_USER, si_user_byname);
}

__private_extern__ si_item_t *
search_user_byuid(si_mod_t *si, uid_t uid)
{
	return search_item_bynumber(si, (uint32_t)uid, CATEGORY_USER, si_user_byuid);
}

__private_extern__ si_list_t *
search_user_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_USER, si_user_all);
}

__private_extern__ si_item_t *
search_group_byname(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_GROUP, si_group_byname);
}

__private_extern__ si_item_t *
search_group_bygid(si_mod_t *si, gid_t gid)
{
	return search_item_bynumber(si, (uint32_t)gid, CATEGORY_USER, si_group_bygid);
}

__private_extern__ si_list_t *
search_group_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_GROUP, si_group_all);
}

__private_extern__ si_item_t *
search_groupist(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_GROUPLIST, si_grouplist);
}

__private_extern__ si_list_t *
search_netgroup_byname(si_mod_t *si, const char *name)
{
	int i, cat;
	search_si_private_t *pp;
	si_list_t *list, *all;
	si_mod_t *cache, *src;

	if (si == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	cat = CATEGORY_NETGROUP;

	cache = search_cat_cache(pp, cat);
	if (cache != NULL)
	{
		list = si_netgroup_byname(cache, name);
		if (list != NULL) return list;
	}

	i = 0;

	all = NULL;
	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		if (src == pp->cache) continue;

		list = si_netgroup_byname(src, name);
		if (list == NULL) continue;

		all = si_list_concat(all, list);
		si_list_release(list);
	}

	si_cache_add_list(cache, si, all);
	return all;
}

__private_extern__ int
search_in_netgroup(si_mod_t *si, const char *group, const char *host, const char *user, const char *domain)
{
	int i, cat, innetgr;
	search_si_private_t *pp;
	si_mod_t *src;

	if (si == NULL) return 0;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return 0;

	cat = CATEGORY_NETGROUP;
	i = 0;
	innetgr = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		innetgr = si_in_netgroup(src, group, host, user, domain);
		if (innetgr != 0) return 1;
	}

	return 0;
}

__private_extern__ si_item_t *
search_alias_byname(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_ALIAS, si_alias_byname);
}

__private_extern__ si_list_t *
search_alias_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_ALIAS, si_alias_all);
}

__private_extern__ si_item_t *
search_host_byname(si_mod_t *si, const char *name, int af, const char *interface, uint32_t *err)
{
	int i, cat;
	search_si_private_t *pp;
	si_item_t *item;
	si_mod_t *src;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	if ((si == NULL) || (name == NULL))
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	pp = (search_si_private_t *)si->private;
	if (pp == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	cat = CATEGORY_HOST_IPV4;
	if (af == AF_INET6) cat = CATEGORY_HOST_IPV6;

	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		item = si_host_byname(src, name, af, interface, err);
		if (item != NULL)
		{
			si_cache_add_item(search_cat_cache(pp, cat), src, item);
			return item;
		}
	}

	if (err != NULL) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;
	return NULL;
}

__private_extern__ si_item_t *
search_host_byaddr(si_mod_t *si, const void *addr, int af, const char *interface, uint32_t *err)
{
	int i, cat;
	search_si_private_t *pp;
	si_item_t *item;
	si_mod_t *src;

	if (err != NULL) *err = SI_STATUS_NO_ERROR;

	if ((si == NULL) || (addr == NULL))
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	pp = (search_si_private_t *)si->private;
	if (pp == NULL)
	{
		if (err != NULL) *err = SI_STATUS_H_ERRNO_NO_RECOVERY;
		return NULL;
	}

	cat = CATEGORY_HOST_IPV4;
	if (af == AF_INET6) cat = CATEGORY_HOST_IPV6;
	
	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		item = si_host_byaddr(src, addr, af, interface, err);
		if (item != NULL)
		{
			si_cache_add_item(search_cat_cache(pp, cat), src, item);
			return item;
		}
	}

	if (err != NULL) *err = SI_STATUS_H_ERRNO_HOST_NOT_FOUND;
	return NULL;
}

__private_extern__ si_list_t *
search_host_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_HOST, si_host_all);
}

__private_extern__ si_item_t *
search_network_byname(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_NETWORK, si_network_byname);
}

__private_extern__ si_item_t *
search_network_byaddr(si_mod_t *si, uint32_t addr)
{
	return search_item_bynumber(si, addr, CATEGORY_NETWORK, si_network_byaddr);
}

__private_extern__ si_list_t *
search_network_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_NETWORK, si_network_all);
}

__private_extern__ si_item_t *
search_service_byname(si_mod_t *si, const char *name, const char *proto)
{
	int i, cat;
	si_item_t *item;
	search_si_private_t *pp;
	si_mod_t *src;

	if (si == NULL) return NULL;
	if (name == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	cat = CATEGORY_SERVICE;
	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		item = si_service_byname(src, name, proto);
		if (item != NULL)
		{
			si_cache_add_item(search_cat_cache(pp, cat), src, item);
			return item;
		}
	}

	return NULL;
}

__private_extern__ si_item_t *
search_service_byport(si_mod_t *si, int port, const char *proto)
{
	int i, cat;
	search_si_private_t *pp;
	si_item_t *item;
	si_mod_t *src;

	if (si == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	cat = CATEGORY_SERVICE;
	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		item = si_service_byport(src, port, proto);
		if (item != NULL)
		{
			si_cache_add_item(search_cat_cache(pp, cat), src, item);
			return item;
		}
	}

	return NULL;
}

__private_extern__ si_list_t *
search_service_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_SERVICE, si_service_all);
}

__private_extern__ si_item_t *
search_protocol_byname(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_PROTOCOL, si_protocol_byname);
}

__private_extern__ si_item_t *
search_protocol_bynumber(si_mod_t *si, int number)
{
	return search_item_bynumber(si, (uint32_t)number, CATEGORY_PROTOCOL, si_protocol_bynumber);
}

__private_extern__ si_list_t *
search_protocol_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_PROTOCOL, si_protocol_all);
}

__private_extern__ si_item_t *
search_rpc_byname(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_RPC, si_rpc_byname);
}

__private_extern__ si_item_t *
search_rpc_bynumber(si_mod_t *si, int number)
{
	int i, cat;
	search_si_private_t *pp;
	si_item_t *item;
	si_mod_t *src;

	if (si == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	cat = CATEGORY_RPC;
	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		item = si_rpc_bynumber(src, number);
		if (item != NULL)
		{
			si_cache_add_item(search_cat_cache(pp, cat), src, item);
			return item;
		}
	}

	return NULL;
}

__private_extern__ si_list_t *
search_rpc_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_RPC, si_rpc_all);
}

__private_extern__ si_item_t *
search_fs_byspec(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_FS, si_fs_byspec);
}

__private_extern__ si_item_t *
search_fs_byfile(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_FS, si_fs_byfile);
}

__private_extern__ si_list_t *
search_fs_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_FS, si_fs_all);
}

__private_extern__ si_item_t *
search_mac_byname(si_mod_t *si, const char *name)
{
	return search_item_byname(si, name, CATEGORY_MAC, si_mac_byname);
}

__private_extern__ si_item_t *
search_mac_bymac(si_mod_t *si, const char *mac)
{
	return search_item_byname(si, mac, CATEGORY_MAC, si_mac_bymac);
}

__private_extern__ si_list_t *
search_mac_all(si_mod_t *si)
{
	return search_list(si, CATEGORY_MAC, si_mac_all);
}

__private_extern__ si_list_t *
search_srv_byname(si_mod_t *si, const char* qname, const char *interface, uint32_t *err)
{
	int i, cat;
	si_list_t *list = NULL;
	si_mod_t *src;
	search_si_private_t *pp;

	if (si == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	cat = CATEGORY_SRV;
	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		if (src == pp->cache) continue;

		if (src->sim_srv_byname != NULL)
		{
			list = src->sim_srv_byname(src, qname, interface, err);
			if (list != NULL) return list;
		}
	}

	if ((i > 0) && (err != NULL)) *err = SI_STATUS_EAI_NONAME;
	return NULL;
}

__private_extern__ int
search_wants_addrinfo(si_mod_t *si)
{
	int i, cat;
	si_mod_t *src;
	search_si_private_t *pp;

	if (si == NULL) return 0;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return 0;

	cat = CATEGORY_ADDRINFO;
	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		if (src == pp->cache) continue;
		if (src->sim_addrinfo != NULL) return 1;
	}

	return 0;
}

__private_extern__ si_list_t *
search_addrinfo(si_mod_t *si, const void *node, const void *serv, uint32_t family, uint32_t socktype, uint32_t protocol, uint32_t flags, const char *interface, uint32_t *err)
{
	int i, cat;
	search_si_private_t *pp;
	si_list_t *list = NULL;
	si_mod_t *src;

	if (err != NULL) *err = SI_STATUS_EAI_FAIL;

	if (si == NULL) return NULL;

	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	cat = CATEGORY_ADDRINFO;
	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		if (src == pp->cache) continue;

		if (src->sim_addrinfo != NULL)
		{
			list = src->sim_addrinfo(src, node, serv, family, socktype, protocol, flags, interface, err);
			if (list != NULL) return list;
		}
	}

	if ((i > 0) && (err != NULL)) *err = SI_STATUS_EAI_NONAME;
	return NULL;
}

__private_extern__ si_item_t *
search_nameinfo(si_mod_t *si, const struct sockaddr *sa, int flags, const char *interface, uint32_t *err)
{
	int i, cat;
	search_si_private_t *pp;
	si_item_t *item;
	si_mod_t *src;

	if (err != NULL) *err = SI_STATUS_EAI_FAIL;

	if (si == NULL) return NULL;
	
	pp = (search_si_private_t *)si->private;
	if (pp == NULL) return NULL;

	cat = CATEGORY_NAMEINFO;
	i = 0;

	while (NULL != (src = search_get_module(pp, cat, &i)))
	{
		item = si_nameinfo(src, sa, flags, interface, err);
		if (item != NULL)
		{
			si_cache_add_item(search_cat_cache(pp, cat), src, item);
			if (err != NULL) *err = SI_STATUS_NO_ERROR;
			return item;
		}
	}

	if ((i > 0) && (err != NULL)) *err = SI_STATUS_EAI_NONAME;
	return NULL;
}

__private_extern__ int
search_is_valid(si_mod_t *si, si_item_t *item)
{
	si_mod_t *src;

	if (si == NULL) return 0;
	if (item == NULL) return 0;
	if (si->name == NULL) return 0;
	if (item->src == NULL) return 0;

	src = (si_mod_t *)item->src;

	if (src->name == NULL) return 0;
	if (string_not_equal(si->name, src->name)) return 0;
	return 0;
}

static si_mod_t *
search_alloc()
{
	si_mod_t *out;
	char *outname;
	search_si_private_t *pp;

	out = (si_mod_t *)calloc(1, sizeof(si_mod_t));
	outname = strdup("search");
	pp = (search_si_private_t *)calloc(1, sizeof(search_si_private_t));

	if ((out == NULL) || (outname == NULL) || (pp == NULL))
	{
		if (out != NULL) free(out);
		if (outname != NULL) free(outname);
		if (pp != NULL) free(pp);

		errno = ENOMEM;
		return NULL;
	}

	out->name = outname;
	out->vers = 1;
	out->refcount = 1;
	out->private = pp;

	out->sim_close = search_close;

	out->sim_is_valid = search_is_valid;

	out->sim_user_byname = search_user_byname;
	out->sim_user_byuid = search_user_byuid;
	out->sim_user_all = search_user_all;

	out->sim_group_byname = search_group_byname;
	out->sim_group_bygid = search_group_bygid;
	out->sim_group_all = search_group_all;

	out->sim_grouplist = search_groupist;

	out->sim_netgroup_byname = search_netgroup_byname;
	out->sim_in_netgroup = search_in_netgroup;

	out->sim_alias_byname = search_alias_byname;
	out->sim_alias_all = search_alias_all;

	out->sim_host_byname = search_host_byname;
	out->sim_host_byaddr = search_host_byaddr;
	out->sim_host_all = search_host_all;

	out->sim_network_byname = search_network_byname;
	out->sim_network_byaddr = search_network_byaddr;
	out->sim_network_all = search_network_all;

	out->sim_service_byname = search_service_byname;
	out->sim_service_byport = search_service_byport;
	out->sim_service_all = search_service_all;

	out->sim_protocol_byname = search_protocol_byname;
	out->sim_protocol_bynumber = search_protocol_bynumber;
	out->sim_protocol_all = search_protocol_all;

	out->sim_rpc_byname = search_rpc_byname;
	out->sim_rpc_bynumber = search_rpc_bynumber;
	out->sim_rpc_all = search_rpc_all;

	out->sim_fs_byspec = search_fs_byspec;
	out->sim_fs_byfile = search_fs_byfile;
	out->sim_fs_all = search_fs_all;

	out->sim_mac_byname = search_mac_byname;
	out->sim_mac_bymac = search_mac_bymac;
	out->sim_mac_all = search_mac_all;

	out->sim_addrinfo = search_addrinfo;
	out->sim_wants_addrinfo = search_wants_addrinfo;
	out->sim_nameinfo = search_nameinfo;

	out->sim_srv_byname = search_srv_byname;
	
	return out;
}

static void
init_optional_modules(search_si_private_t *pp)
{
	if (pp->mdns == NULL)
	{
		pp->mdns = si_module_with_name("mdns");
		/* allow this to fail */
	}

#ifdef DS_AVAILABLE
	if (pp->flags & SEARCH_FLAG_MAC)
	{
		if (pp->ds == NULL)
		{
			pp->ds = si_module_with_name("ds");
			/* allow this to fail */
		}
	}
#endif

	if (pp->flags & (SEARCH_FLAG_APPLETV | SEARCH_FLAG_IPHONE))
	{
		if (pp->dns == NULL)
		{
			pp->dns = si_module_with_name("dns");
			/* allow this to fail */
		}
	}
}

__private_extern__ si_mod_t *
si_module_static_search()
{
	si_mod_t *out;
	search_si_private_t *pp;
	FILE *conf;
	char *line, **tokens;
	int cat, i, j, ntokens;

	out = search_alloc();
	if (out == NULL) return NULL;

	pp = (search_si_private_t *)out->private;
	if (pp == NULL)
	{
		free(out);
		return NULL;
	}

#ifdef CONFIG_MAC
	pp->flags = SEARCH_FLAG_CACHE_ENABLED | SEARCH_FLAG_MAC;
#endif
#ifdef CONFIG_APPLETV
	pp->flags = SEARCH_FLAG_CACHE_ENABLED | SEARCH_FLAG_APPLETV;
#endif
	
#ifdef CONFIG_IPHONE
	pp->flags = SEARCH_FLAG_CACHE_ENABLED | SEARCH_FLAG_IPHONE;
#endif

	pp->cache = si_module_with_name("cache");
	if (pp->cache == NULL) 
	{
		search_close(out);
		return NULL;
	}

	pp->file = si_module_with_name("file");
	if (pp->file == NULL)
	{
		search_close(out);
		return NULL;
	}

	init_optional_modules(pp);

	conf = fopen(_PATH_SI_CONF, "r");
	if (conf == NULL) return out;

	forever
	{
		line = _fsi_get_line(conf);
		if (line == NULL) break;

		if (line[0] == '#') 
		{
			free(line);
			line = NULL;
			continue;
		}

		ntokens = 0;
		tokens = _fsi_tokenize(line, "	: ", 0, &ntokens);

		if (ntokens < 2)
		{
			free(tokens);
			tokens = NULL;
			free(line);
			line = NULL;
			continue;
		}

		if (string_equal(tokens[0], "config"))
		{
			if (string_equal(tokens[1], "mac")) pp->flags = SEARCH_FLAG_CACHE_ENABLED | SEARCH_FLAG_MAC;
			else if (string_equal(tokens[1], "appletv")) pp->flags = SEARCH_FLAG_CACHE_ENABLED | SEARCH_FLAG_APPLETV;
			else if (string_equal(tokens[1], "iphone")) pp->flags = SEARCH_FLAG_CACHE_ENABLED | SEARCH_FLAG_IPHONE;

			init_optional_modules(pp);

			free(tokens);
			tokens = NULL;
			free(line);
			line = NULL;
			continue;
		}

		if (string_equal(tokens[0], "user")) cat = CATEGORY_USER;
		else if (string_equal(tokens[0], "group")) cat = CATEGORY_GROUP;
		else if (string_equal(tokens[0], "grouplist")) cat = CATEGORY_GROUPLIST;
		else if (string_equal(tokens[0], "netgroup")) cat = CATEGORY_NETGROUP;
		else if (string_equal(tokens[0], "alias")) cat = CATEGORY_ALIAS;
		else if (string_equal(tokens[0], "host")) cat = CATEGORY_HOST_IPV4;
		else if (string_equal(tokens[0], "network")) cat = CATEGORY_NETWORK;
		else if (string_equal(tokens[0], "service")) cat = CATEGORY_SERVICE;
		else if (string_equal(tokens[0], "protocol")) cat = CATEGORY_PROTOCOL;
		else if (string_equal(tokens[0], "rpc")) cat = CATEGORY_RPC;
		else if (string_equal(tokens[0], "fs")) cat = CATEGORY_FS;
		else if (string_equal(tokens[0], "mac")) cat = CATEGORY_MAC;
		else if (string_equal(tokens[0], "addrinfo")) cat = CATEGORY_ADDRINFO;
		else if (string_equal(tokens[0], "nameinfo")) cat = CATEGORY_NAMEINFO;
		else
		{
			free(tokens);
			tokens = NULL;
			free(line);
			line = NULL;
			continue;
		}

	do_ipv6:

		if (pp->search_list[cat].module != NULL)
		{
			free(tokens);
			tokens = NULL;
			free(line);
			line = NULL;
			continue;
		}

		pp->search_list[cat].count = ntokens - 1;
		pp->search_list[cat].module = (si_mod_t **)calloc(pp->search_list[cat].count, sizeof(si_mod_t *));
		if (pp->search_list[cat].module == NULL)
		{
			search_close(out);
			free(tokens);
			tokens = NULL;
			free(line);
			line = NULL;
			return NULL;
		}

		for (i = 1, j = 0; i < ntokens; i++, j++)
		{
			if (string_equal(tokens[i], "cache"))
			{
				pp->search_list[cat].module[j] = pp->cache;
				pp->search_list[cat].flags = SEARCH_FLAG_CACHE_ENABLED;
			}
			else if (string_equal(tokens[i], "file"))
			{
				if (pp->file == NULL) pp->file = si_module_with_name("file");				
				pp->search_list[cat].module[j] = pp->file;
			}
			else if (string_equal(tokens[i], "dns"))
			{
				if (pp->dns == NULL) pp->dns = si_module_with_name("dns");				
				pp->search_list[cat].module[j] = pp->dns;
			}
			else if (string_equal(tokens[i], "mdns"))
			{
				if (pp->mdns == NULL) pp->mdns = si_module_with_name("mdns");				
				pp->search_list[cat].module[j] = pp->mdns;
			}
			else if (string_equal(tokens[i], "ds"))
			{
				if (pp->ds == NULL) pp->ds = si_module_with_name("ds");				
				pp->search_list[cat].module[j] = pp->ds;
			}
		}

		if (cat == CATEGORY_HOST_IPV4)
		{
			cat = CATEGORY_HOST_IPV6;
			goto do_ipv6;
		}

		free(tokens);
		tokens = NULL;
		free(line);
		line = NULL;
	}

	return out;
}

__private_extern__ si_mod_t *
search_custom(int n, ...)
{
	va_list ap;
	si_mod_t *out, *m;
	search_si_private_t *pp;
	int cat, i;
	char *name;

	if (n == 0) return si_module_static_search();

	out = search_alloc();
	if (out == NULL) return NULL;

	pp = (search_si_private_t *)out->private;
	if (pp == NULL)
	{
		free(out);
		return NULL;
	}

	for (cat = 0; cat < CATEGORY_COUNT; cat++)
	{
		pp->search_list[cat].count = n;
		pp->search_list[cat].module = (si_mod_t **)calloc(pp->search_list[cat].count, sizeof(si_mod_t *));
		if (pp->search_list[cat].module == NULL)
		{
			search_close(out);
			return NULL;
		}

	}

	va_start(ap, n);

	for (i = 0; i < n; i++)
	{
		name = va_arg(ap, char *);
		if (name == NULL) break;

		m = NULL;
		if (string_equal(name, "cache"))
		{
			if (pp->cache == NULL)
			{
				pp->cache = si_module_with_name("cache");
				m = pp->cache;
				if (pp->cache == NULL)
				{
					search_close(out);
					return NULL;
				}
			}
		}
		else if (string_equal(name, "file"))
		{
			if (pp->file == NULL)
			{
				pp->file = si_module_with_name("file");
				m = pp->file;
				if (pp->file == NULL)
				{
					search_close(out);
					return NULL;
				}
			}
		}
		else if (string_equal(name, "dns"))
		{
			if (pp->dns == NULL)
			{
				pp->dns = si_module_with_name("dns");
				m = pp->dns;
				if (pp->dns == NULL)
				{
					search_close(out);
					return NULL;
				}
			}
		}
		else if (string_equal(name, "mdns"))
		{
			if (pp->mdns == NULL)
			{
				pp->mdns = si_module_with_name("mdns");
				m = pp->mdns;
				if (pp->mdns == NULL)
				{
					search_close(out);
					return NULL;
				}
			}
		}
		else if (string_equal(name, "ds"))
		{
			if (pp->ds == NULL)
			{
				pp->ds = si_module_with_name("ds");
				m = pp->ds;
				if (pp->ds == NULL)
				{
					search_close(out);
					return NULL;
				}
			}
		}

		for (cat = 0; cat < CATEGORY_COUNT; cat++)
		{
			if (string_equal(name, "cache")) pp->search_list[cat].flags = SEARCH_FLAG_CACHE_ENABLED;
			pp->search_list[cat].module[i] = m;
		}
	}

	va_end(ap);

	return out;
}
