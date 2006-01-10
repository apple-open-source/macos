/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

/*
 * NIS.c
 *
 * NIS lookup agent for lookupd
 * Written by Marc Majka
 */

#include <NetInfo/system_log.h>
#include <NetInfo/syslock.h>
#ifdef RPC_SUCCESS
#undef RPC_SUCCESS
#endif
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <stdlib.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/DynaAPI.h>
#include <NetInfo/ffparser.h>
#include <NetInfo/config.h>

extern int _yplib_timeout;

extern int close(int);
extern char *nettoa(unsigned long);

static syslock *rpcLock = NULL;

#define BUFSIZE 8192

#define DefaultTimeToLive 300
#define SERVER_CHECK_LATENCY 60

typedef struct
{
	char *nis_domain_name;
	int map_order_number[20];
	int map_order_time[20];
	unsigned int timeToLive;
	dynainfo *dyna;
} agent_private;


static char *categoryMap[] =
{
	"passwd.byname",
	"group.byname",
	"hosts.byname",
	"networks.byname",
	"services.byname",
	"protocols.byname",
	"rpc.byname",
	"mounts.byname",
	"printcap.byname",
	"bootparams.byname",
	"bootptab.byaddr",
	"mail.aliases",
	NULL,
	NULL,
	"netgroup",
	NULL,
	NULL
};

static int categoryMapIndex[] =
{
	0,	/* passwd.byname */
	2,	/* group.byname */
	4,	/* hosts.byname */
	6,	/* networks.byname */
	8,	/* services.byname */
	9,	/* protocols.byname */
	11,	/* rpc.byname */
	13,	/* mounts.byname */
	14,	/* printcap.byname */
	15,	/* bootparams.byname */
	16,	/* bootptab.byaddr */
	18,	/* mail.aliases */
	-1,
	-1,
	19,	/* netgroup */
	-1,
	-1
};

static char *singleMap[] =
{
	"passwd.byname",		/*  0 */
	"passwd.byuid",			/*  1 */
	"group.byname",			/*  2 */
	"group.bygid",			/*  3 */
	"hosts.byname",			/*  4 */
	"hosts.byaddr",			/*  5 */
	"networks.byname",		/*  6 */
	"networks.byaddr",		/*  7 */
	"services.byname",		/*  8 */
	"protocols.byname",		/*  9 */
	"protocols.bynumber",	/* 10 */
	"rpc.byname",			/* 11 */
	"rpc.bynumber",			/* 12 */
	"mounts.byname",		/* 13 */
	"printcap.byname",		/* 14 */
	"bootparams.byname",	/* 15 */
	"bootptab.byaddr",		/* 16 */
	"bootptab.byether",		/* 17 */
	"mail.aliases",			/* 18 */
	"netgroup"				/* 19 */
};

	
u_int32_t
NIS_new(void **c, char *args, dynainfo *d)
{
	agent_private *ap;
	dsrecord *r;
	dsattribute *a;
	dsdata *x;
	int i, status;
	char *dn;

	if (c == NULL) return 1;

	rpcLock = syslock_get(RPCLockName);

	syslock_lock(rpcLock);
	if (args == NULL) yp_get_default_domain(&dn);	
	else dn = args;
	syslock_unlock(rpcLock);

	if (dn == NULL) return 1;

	syslock_lock(rpcLock);
	status = yp_bind(dn);
	syslock_unlock(rpcLock);

	if (status != 0) return 1;

	ap = (agent_private *)malloc(sizeof(agent_private));
	*c = ap;

	ap->nis_domain_name = copyString(dn);
	for (i = 0; i < 20; i++)
	{
		ap->map_order_number[i] = 0;
		ap->map_order_time[i] = 0;
	}
	ap->dyna = d;

	system_log(LOG_DEBUG, "Allocated NIS 0x%08x\n", (int)ap);

	ap->timeToLive = DefaultTimeToLive;
	r = NULL;

	if (ap->dyna != NULL)
	{
		if (ap->dyna->dyna_config_agent != NULL)
		{
			status = (ap->dyna->dyna_config_agent)(ap->dyna, -1, &r);
			if (status == 0)
			{
				x = cstring_to_dsdata("TimeToLive");
				a = dsrecord_attribute(r, x, SELECT_ATTRIBUTE);
				dsdata_release(x);
				if (a != NULL)
				{
					x = dsattribute_value(a, 0);
					if (x != NULL)
					{
						ap->timeToLive = atoi(dsdata_to_cstring(x));
						dsdata_release(x);
					}
					dsattribute_release(a);
				}

				dsrecord_release(r);
			}
		}		
	}

	return 0;
}

u_int32_t
NIS_free(void *c)
{
	agent_private *ap;

	if (c == NULL) return 0;

	ap = (agent_private *)c;

	if (ap->nis_domain_name != NULL) free(ap->nis_domain_name);
	ap->nis_domain_name = NULL;

	system_log(LOG_DEBUG, "Deallocated NIS 0x%08x\n", (int)ap);

	free(ap);
	c = NULL;

	return 0;
}

static int
get_order_number(agent_private *ap, int map_index)
{
	time_t now;
	int order, status;

	if (map_index < 0) return 0;
	if (map_index >= 20) return 0;

	now = time(0);
	if (now <= ap->map_order_time[map_index]) return ap->map_order_number[map_index];

	order = 0;
	
	syslock_lock(rpcLock);
	status = yp_order(ap->nis_domain_name, singleMap[map_index], &order);
	syslock_unlock(rpcLock);
	
	if (status != 0) order = 0;
	ap->map_order_number[map_index] = order;
	ap->map_order_time[map_index] = now + SERVER_CHECK_LATENCY;

	return order;
}

static void
add_validation(dsrecord *r, int map_index, int order)
{
	dsdata *d;
	dsattribute *a;
	char str[32];

	if (r == NULL) return;

	d = cstring_to_dsdata("lookup_validation");
	dsrecord_remove_key(r, d, SELECT_META_ATTRIBUTE);

	a = dsattribute_new(d);
	dsrecord_append_attribute(r, a, SELECT_META_ATTRIBUTE);

	dsdata_release(d);

	sprintf(str, "%d %d", map_index, order);

	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);

	dsattribute_release(a);
}

u_int32_t
NIS_validate(void *c, char *v)
{
	agent_private *ap;
	int n, vmap, vorder, order;

	if (c == NULL) return 0;
	if (v == NULL) return 0;

	vorder = 0;
	vmap = -1;

	n = sscanf(v, "%d %d", &vmap, &vorder);
	if (n != 2) return 0;

	ap = (agent_private *)c;

	order = get_order_number(ap, vmap);
	if (order != vorder) return 0;

	return 1;
}

static dsrecord *
parse(char *data, int cat)
{
	if (data == NULL) return NULL;
	if (data[0] == '#') return NULL;

	switch (cat)
	{
		case LUCategoryUser: return ff_parse_user(data);
		case LUCategoryGroup: return ff_parse_group(data);
		case LUCategoryHost: return ff_parse_host(data);
		case LUCategoryNetwork: return ff_parse_network(data);
		case LUCategoryService: return ff_parse_service(data);
		case LUCategoryProtocol: return ff_parse_protocol(data);
		case LUCategoryRpc: return ff_parse_rpc(data);
		case LUCategoryMount: return ff_parse_mount(data);
		case LUCategoryPrinter: return ff_parse_printer(data);
		case LUCategoryBootparam: return ff_parse_bootparam(data);
		case LUCategoryBootp: return ff_parse_bootp(data);
		case LUCategoryAlias: return ff_parse_alias(data);
		case LUCategoryNetDomain: return ff_parse_netgroup(data);
		case LUCategoryEthernet: return ff_parse_ethernet(data);
		case LUCategoryNetgroup: return ff_parse_netgroup(data);
		default: return NULL;
	}

	return NULL;
}

int
map_index_for_key(char *key, LUCategory cat)
{
	switch (cat)
	{
		case LUCategoryUser:
		{
			if (streq(key, "name")) return 0;
			if (streq(key, "uid")) return 1;
			return -1;
		}
			
		case LUCategoryGroup:
		{
			if (streq(key, "name")) return 2;
			if (streq(key, "gid")) return 3;
			return -1;
		}
			
		case LUCategoryHost:
		{
			if (streq(key, "name")) return 4;
			if (streq(key, "ip_address")) return 5;
			return -1;
		}
			
		case LUCategoryNetwork:
		{
			if (streq(key, "name")) return 6;
			if (streq(key, "address")) return 7;
			return -1;
		}
			
		case LUCategoryService:
		{
			if (streq(key, "name")) return 8;
			return -1;
		}
			
		case LUCategoryProtocol:
		{
			if (streq(key, "name")) return 9;
			if (streq(key, "number")) return 10;
			return -1;
		}
			
		case LUCategoryRpc:
		{
			if (streq(key, "name")) return 11;
			if (streq(key, "number")) return 12;
			return -1;
		}
			
		case LUCategoryMount:
		{
			if (streq(key, "name")) return 13;
			return -1;
		}
			
		case LUCategoryPrinter:
		{
			if (streq(key, "name")) return 14;
			return -1;
		}
			
		case LUCategoryBootparam:
		{
			if (streq(key, "name")) return 15;
			return -1;
		}
			
		case LUCategoryBootp:
		{
			if (streq(key, "ip_address")) return 16;
			if (streq(key, "en_address")) return 17;
			return -1;
		}
			
		case LUCategoryAlias:
		{
			if (streq(key, "name")) return 18;
			return -1;
		}
			
		case LUCategoryNetgroup:
		{
			return 19;
		}
			
		default:
		{
			return -1;
		}
	}
	
	return -1;
}

static int
_NIS_match(agent_private *ap, LUCategory cat, dsdata *k, dsdata *v, dsrecord **list)
{
	char *key, *val, *out;
	const char *map;
	int x, vallen, outlen, order, status;
	dsrecord *item = NULL;
	time_t now;

	key = dsdata_to_cstring(k);
	val = dsdata_to_cstring(v);

	x = map_index_for_key(key, cat);
	if (x == -1) return 1;

	map = singleMap[x];

	vallen = strlen(val);
	out = NULL;
	outlen = 0;

	now = time(0);

	syslock_lock(rpcLock);

	status = yp_match(ap->nis_domain_name, map, val, vallen, &out, &outlen);

	order = 0;
	status = yp_order(ap->nis_domain_name, map, &order);
	if (status != 0) order = 0;
	
	ap->map_order_number[x] = order;
	ap->map_order_time[x] = now + SERVER_CHECK_LATENCY;

	syslock_unlock(rpcLock);

	if (status != 0) return 1;
	if (out == NULL) return 1;
	if (outlen == 0) return 1;
	
	item = parse(out, cat);
	free(out);

	if (item == NULL) return 1;

	add_validation(item, x, order);
	*list = item;

	return 0;
}

u_int32_t
NIS_query(void *c, dsrecord *pattern, dsrecord **list)
{
	agent_private *ap;
	u_int32_t cat;
	dsattribute *a;
	dsdata *k;
	dsrecord *lastrec;
	char *map;
	dsrecord *item = NULL;
	int single_item, stamp;
	int x, order, status;
	char *key, *val, *lastkey, scratch[4096];
	int match, keylen, vallen, lastlen;
	
	if (c == NULL) return 1;
	if (pattern == NULL) return 1;
	if (list == NULL) return 1;

	*list = NULL;
	lastrec = NULL;
	single_item = 0;
	stamp = 0;

	ap = (agent_private *)c;

	k = cstring_to_dsdata(CATEGORY_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);

	if (a == NULL) return 1;
	if (a->count == 0) return 1;

	cat = atoi(dsdata_to_cstring(a->value[0]));
	dsattribute_release(a);

	x = categoryMapIndex[cat];
	map = categoryMap[cat];
	if (map == NULL) return 1;

	k = cstring_to_dsdata(STAMP_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);
	if (a != NULL)
	{
		dsrecord_remove_attribute(pattern, a, SELECT_META_ATTRIBUTE);
		stamp = 1;
	}
	dsattribute_release(a);

	if (stamp == 1)
	{
		order = get_order_number(ap, x);		
		item = dsrecord_new();
		add_validation(item, x, order);
		*list = item;
		return 0;
	}

	a = NULL;
	if (cat != LUCategoryService)
	{
		k = cstring_to_dsdata(SINGLE_KEY);
		a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
		dsdata_release(k);
	}

	if (a != NULL)
	{
		dsattribute_release(a);
		single_item = 1;

		if ((pattern->count == 1) && (pattern->attribute[0]->count == 1))
		{
			status = _NIS_match(ap, cat, pattern->attribute[0]->key, pattern->attribute[0]->value[0], list);
			return status;
		}
	}

	key = NULL;
	val = NULL;
	vallen = 0;
	lastkey = NULL;

	syslock_lock(rpcLock);

	order = 0;
	status = yp_order(ap->nis_domain_name, map, &order);
	if (status != 0) order = 0;

	ap->map_order_number[x] = order;
	ap->map_order_time[x] = time(0) + SERVER_CHECK_LATENCY;
	
	status = yp_first(ap->nis_domain_name, map, &key, &keylen, &val, &vallen);
	if (status != 0)
	{
		syslock_unlock(rpcLock);
		return 1;
	}

	while (status == 0)
	{
		switch (cat)
		{
			case LUCategoryNetgroup:
				bcopy(key, scratch, keylen);
				scratch[keylen] = ' ';
				bcopy(val, scratch+keylen+1, vallen);
				scratch[keylen + vallen + 1] = '\0';
				break;
			case LUCategoryAlias:
				bcopy(key, scratch, keylen);
				scratch[keylen] = ':';
				scratch[keylen + 1] = ' ';
				bcopy(val, scratch+keylen+2, vallen);
				scratch[keylen + vallen + 2] = '\0';
				break;
			default:
				bcopy(val, scratch, vallen);
				scratch[vallen] = '\0';
		}

		freeString(val);
		val = NULL;
		vallen = 0;

		item = parse(scratch, cat);

		freeString(lastkey);
		lastkey = NULL;

		if (item != NULL) 
		{
			match = dsrecord_match_select(item, pattern, SELECT_ATTRIBUTE);
			if (match == 1)
			{
				add_validation(item, x, order);

				if (*list == NULL) *list = dsrecord_retain(item);
				else lastrec->next = dsrecord_retain(item);

				lastrec = item;

				if (single_item == 1)
				{
					dsrecord_release(item);
					break;
				}
			}
			dsrecord_release(item);
		}

		lastkey = key;
		lastlen = keylen;

		status = yp_next(ap->nis_domain_name, map, lastkey, lastlen, &key, &keylen, &val, &vallen);
	}

	syslock_unlock(rpcLock);

	freeString(lastkey);

	return 0;
}
