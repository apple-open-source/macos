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

static unsigned long timeToLive;

typedef struct
{
	char *nis_domain_name;
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

u_int32_t
NIS_new(void **c, char *args, dynainfo *d)
{
	agent_private *ap;
	dsrecord *r;
	dsattribute *a;
	dsdata *x;
	int status, didSetTTL;
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
	ap->dyna = d;

	system_log(LOG_DEBUG, "Allocated NIS 0x%08x\n", (int)ap);

	timeToLive = DefaultTimeToLive;

	r = NULL;
	didSetTTL = 0;

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
						timeToLive = atoi(dsdata_to_cstring(x));
						dsdata_release(x);
						didSetTTL = 1;
					}
					dsattribute_release(a);
				}
				dsrecord_release(r);
			}
		}

		if ((didSetTTL == 0) && (ap->dyna->dyna_config_global != NULL))
		{
			status = (ap->dyna->dyna_config_global)(ap->dyna, -1, &r);
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
						timeToLive = atoi(dsdata_to_cstring(x));
						dsdata_release(x);
						didSetTTL = 1;
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

static void
add_validation(dsrecord *r)
{
	dsdata *d;
	dsattribute *a;
	time_t best_before;
	char str[32];

	if (r == NULL) return;

	d = cstring_to_dsdata("lookup_validation");
	dsrecord_remove_key(r, d, SELECT_META_ATTRIBUTE);

	a = dsattribute_new(d);
	dsrecord_append_attribute(r, a, SELECT_META_ATTRIBUTE);

	dsdata_release(d);

	best_before = time(0) + timeToLive;
	sprintf(str, "%lu", best_before);

	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);

	dsattribute_release(a);
}

u_int32_t
NIS_validate(void *c, char *v)
{
	agent_private *ap;
	u_int32_t t;

	if (c == NULL) return 0;
	if (v == NULL) return 0;

	ap = (agent_private *)c;

	t = atoi(v);

	if (time(0) > t) return 0;

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
	int match;
	char *key, *val, *lastkey;
	int status, keylen, vallen, lastlen;
	char scratch[4096];
	int single_item, stamp;

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
		item = dsrecord_new();
		add_validation(item);
		*list = item;
		return 0;
	}

	k = cstring_to_dsdata(SINGLE_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);
	if (a != NULL)
	{
		dsattribute_release(a);
		single_item = 1;
	}

	key = NULL;
	val = NULL;
	vallen = 0;
	lastkey = NULL;

	syslock_lock(rpcLock);
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
				add_validation(item);

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
