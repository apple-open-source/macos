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
 * Flat File agent
 * Written by Marc Majka
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <notify.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <NetInfo/config.h>
#include <NetInfo/ffparser.h>
#include <NetInfo/dsutil.h>
#include <NetInfo/dsengine.h>
#include <NetInfo/syslock.h>
#include <NetInfo/system_log.h>
#include <NetInfo/DynaAPI.h>

#define DEFAULT_FF_DIR "/etc"
#define NOTIFY_PREFIX "com.apple.system.lookupd.FF"
#define BUFSIZE 8192

extern uint32_t notify_monitor_file(int token, const char *name, int flags);

typedef struct ff_cache_s
{
	int notify_token;
	pthread_mutex_t lock;
	long modtime;
	dsrecord *crecord;
} ff_cache_t;

static ff_cache_t host_cache = {-1, PTHREAD_MUTEX_INITIALIZER, 0, NULL};

typedef struct
{
	char *dir;
	int flags;
	dynainfo *dyna;
} agent_private;

char *categoryFilename[] =
{
#ifdef _UNIX_BSD_43_
	"passwd",
#else
	"master.passwd",
#endif
	"group",
	"hosts",
	"networks",
	"services",
	"protocols",
	"rpc",
	"fstab",
	"printcap",
	"bootparams",
	"bootptab",
	"aliases",
	NULL,
	"ethers",
	"netgroup",
	NULL,
	NULL
};

static void
add_validation(dsrecord *r, char *fname, long ts)
{
	char *str;
	dsdata *d;
	dsattribute *a;

	if (r == NULL) return;

	d = cstring_to_dsdata("lookup_validation");
	dsrecord_remove_key(r, d, SELECT_META_ATTRIBUTE);

	a = dsattribute_new(d);
	dsrecord_append_attribute(r, a, SELECT_META_ATTRIBUTE);

	dsdata_release(d);

	asprintf(&str, "%s %lu", fname, ts);

	d = cstring_to_dsdata(str);
	dsattribute_append(a, d);
	dsdata_release(d);
	free(str);

	dsattribute_release(a);
}

char *
getLineFromFile(FILE *fp)
{
	char s[BUFSIZE];
	char *out;
	int len;

	s[0] = '\0';

	fgets(s, BUFSIZE, fp);
	if (s == NULL || s[0] == '\0') return NULL;

	if (s[0] == '#')
	{
		out = copyString("#");
		return out;
	}

	len = strlen(s);
	if (s[len] == '\n') len--;
	s[len] = '\0';

	out = copyString(s);
	return out;
}

static dsrecord *
parse(char *data, int cat)
{
	if (data == NULL) return NULL;
	if (data[0] == '#') return NULL;

	switch (cat)
	{
		case LUCategoryUser: return ff_parse_user_A(data);
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

static ff_cache_t *
load_cache(int cat, ff_cache_t *cache)
{
	dsrecord *lastrec;
	char *fname;
	FILE *fp;
	char *line;
	dsrecord *item = NULL;
	char *fpath;
	int status;
	struct stat sb;

	if (cache == NULL) return NULL;

	dsrecord_release(cache->crecord);
	cache->crecord = NULL;

	fname = categoryFilename[cat];
	if (fname == NULL) return NULL;

	asprintf(&fpath, "%s/%s", DEFAULT_FF_DIR, fname);

	memset(&sb, 0, sizeof(struct stat));
	status = stat(fpath, &sb);
	if (status < 0)
	{
		free(fpath);
		return NULL;
	}

	cache->modtime = sb.st_mtime;

	fp = fopen(fpath, "r");
	if (fp == NULL)
	{
		free(fpath);
		return NULL;
	}

	/* bootptab entries start after a "%%" line */
	if (cat == LUCategoryBootp)
	{
		while (NULL != (line = getLineFromFile(fp)))
		{
			if (!strncmp(line, "%%", 2)) break;
			freeString(line);
			line = NULL;
		}

		if (line == NULL)
		{
			fclose(fp);
			free(fpath);
			return 0;
		}

		freeString(line);
		line = NULL;
	}

	lastrec = NULL;

	while (NULL != (line = getLineFromFile(fp)))	
	{
		if (line[0] == '#')
		{
			freeString(line);
			line = NULL;
			continue;
		}

		item = parse(line, cat);

		freeString(line);
		line = NULL;

		if (item == NULL) continue;

		add_validation(item, fpath, cache->modtime);

		if (cache->crecord == NULL) cache->crecord = item;
		if (lastrec != NULL) lastrec->next = item;
		lastrec = item;
	}

	free(fpath);
	fclose(fp);

	return cache;
}

static ff_cache_t *
prep_cache(int cat, ff_cache_t *cache)
{
	u_int32_t status, check;
	char *s;
	ff_cache_t *c;

	if (cache == NULL) return NULL;

	if (cache->notify_token == -1)
	{
		s = NULL;
		asprintf(&s, "%s.%s", NOTIFY_PREFIX, categoryFilename[cat]);
		status = notify_register_check(s, &(cache->notify_token));
		free(s);
		if (status != NOTIFY_STATUS_OK) return NULL;

		s = NULL;
		asprintf(&s, "%s/%s", DEFAULT_FF_DIR, categoryFilename[cat]);
		status = notify_monitor_file(cache->notify_token, s, 0);
		free(s);
		if (status != NOTIFY_STATUS_OK) return NULL;
	}

	pthread_mutex_lock(&(cache->lock));
	check = 1;
	if (cache->modtime != 0) 
	{
		status = notify_check(cache->notify_token, &check);
		if ((status == NOTIFY_STATUS_OK) && (check == 0))
		{
			pthread_mutex_unlock(&(cache->lock));
			return cache;
		}
	}

	c = load_cache(cat, cache);
	pthread_mutex_unlock(&(cache->lock));
	return c;
}

static ff_cache_t *
cache_for_category(int cat)
{
	if (cat == LUCategoryHost) return prep_cache(LUCategoryHost, &host_cache);
	return NULL;
}

u_int32_t
FF_new(void **c, char *args, dynainfo *d)
{
	agent_private *ap;

	if (c == NULL) return 1;
	ap = (agent_private *)calloc(1, sizeof(agent_private));
	*c = ap;

	if (args == NULL)
	{
		ap->dir = copyString(DEFAULT_FF_DIR);
		ap->flags = 0;
	}
	else
	{
		ap->dir = copyString(args);
		ap->flags = 1;
	}

	ap->dyna = d;

	system_log(LOG_DEBUG, "Allocated FF 0x%08x\n", (int)ap);

	return 0;
}

u_int32_t
FF_free(void *c)
{
	agent_private *ap;

	if (c == NULL) return 0;

	ap = (agent_private *)c;

	if (ap->dir != NULL) free(ap->dir);
	ap->dir = NULL;

	system_log(LOG_DEBUG, "Deallocated FF 0x%08x\n", (int)ap);

	free(ap);
	c = NULL;

	return 0;
}

u_int32_t
cache_query(ff_cache_t *cache, u_int32_t cat, int single_item, dsrecord *pattern, dsrecord **list)
{
	dsrecord *item, *host, *lastrec;
	int match;
	dsdata *k, *k4, *k6;
	dsattribute *a;

	lastrec = NULL;

	for (item = cache->crecord; item != NULL; item = item->next)
	{
		match = dsrecord_match_select(item, pattern, SELECT_ATTRIBUTE);
		if (match == 1)
		{
			if (*list == NULL) 
			{
				*list = dsrecord_copy(item);
				lastrec = *list;
			}
			else
			{
				lastrec->next = dsrecord_copy(item);
				lastrec = lastrec->next;
			}

			lastrec->next = NULL;
		
			if (cat == LUCategoryHost)
			{
			}
			else if (single_item == 1)
			{
				break;
			}
		}
	}

	if ((cat == LUCategoryHost) && (single_item == 1))
	{
		if ((*list) == NULL) return 0;
		if ((*list)->next == NULL) return 0;

		k = cstring_to_dsdata("name");
		k4 = cstring_to_dsdata("ip_address");
		k6 = cstring_to_dsdata("ipv6_address");
		host = *list;

		for (item = host->next; item != NULL; item = item->next)
		{
			a = dsrecord_attribute(item, k, SELECT_ATTRIBUTE);
			dsrecord_merge_attribute(host, a, SELECT_ATTRIBUTE);
			dsattribute_release(a);

			a = dsrecord_attribute(item, k4, SELECT_ATTRIBUTE);
			dsrecord_merge_attribute(host, a, SELECT_ATTRIBUTE);
			dsattribute_release(a);

			a = dsrecord_attribute(item, k6, SELECT_ATTRIBUTE);
			dsrecord_merge_attribute(host, a, SELECT_ATTRIBUTE);
			dsattribute_release(a);
		}

		dsdata_release(k);
		dsdata_release(k4);
		dsdata_release(k6);

		dsrecord_release(host->next);
		host->next = NULL;
	}

	return 0;
}

u_int32_t
FF_query(void *c, dsrecord *pattern, dsrecord **list)
{
	agent_private *ap;
	u_int32_t cat;
	dsattribute *a;
	dsdata *k, *k4, *k6;
	dsrecord *lastrec;
	char *fname;
	FILE *fp;
	char *line;
	dsrecord *item = NULL;
	dsrecord *host = NULL;
	char *fpath;
	long ts;
	int match, single_item, stamp;
	ff_cache_t *cache;
	struct stat sb;

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

	fname = categoryFilename[cat];
	if (fname == NULL) return 1;

	k = cstring_to_dsdata(STAMP_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);
	if (a != NULL)
	{
		dsrecord_remove_attribute(pattern, a, SELECT_META_ATTRIBUTE);
		stamp = 1;
	}
	dsattribute_release(a);

	k = cstring_to_dsdata(SINGLE_KEY);
	a = dsrecord_attribute(pattern, k, SELECT_META_ATTRIBUTE);
	dsdata_release(k);
	if (a != NULL)
	{
		dsattribute_release(a);
		single_item = 1;
	}

	asprintf(&fpath, "%s/%s", ap->dir, fname);

	ts = 0;
	cache = NULL;
	if (ap->flags == 0) cache = cache_for_category(cat);
	if (cache != NULL)
	{
		ts = cache->modtime;
	}
	else
	{
		memset(&sb, 0, sizeof(struct stat));
		if (stat(fpath, &sb) < 0) ts = 0;
		else ts = sb.st_mtime;
	}

	if (stamp == 1)
	{
		item = dsrecord_new();
		add_validation(item, fpath, ts);
		*list = item;
		free(fpath);
		return 0;
	}

	if (cache != NULL)
	{
		free(fpath);
		return cache_query(cache, cat, single_item, pattern, list);
	}

	fp = fopen(fpath, "r");
	if (fp == NULL)
	{
		free(fpath);
		return 1;
	}

	/* bootptab entries start after a "%%" line */
	if (cat == LUCategoryBootp)
	{
		while (NULL != (line = getLineFromFile(fp)))
		{
			if (!strncmp(line, "%%", 2)) break;
			freeString(line);
			line = NULL;
		}
		if (line == NULL)
		{
			fclose(fp);
			free(fpath);
			return 0;
		}

		freeString(line);
		line = NULL;
	}
		
	while (NULL != (line = getLineFromFile(fp)))
	{
		if (line[0] == '#') 
		{
			freeString(line);
			line = NULL;
			continue;
		}

		item = parse(line, cat);

		freeString(line);
		line = NULL;

		if (item == NULL) continue;

		match = dsrecord_match_select(item, pattern, SELECT_ATTRIBUTE);
		if (match == 1)
		{
			add_validation(item, fpath, ts);

			if (*list == NULL) *list = dsrecord_retain(item);
			else lastrec->next = dsrecord_retain(item);

			lastrec = item;

			if (cat == LUCategoryHost)
			{
			}
			else if (single_item == 1)
			{
				dsrecord_release(item);
				break;
			}
		}

		dsrecord_release(item);
	}

	free(fpath);
	fclose(fp);

	if ((cat == LUCategoryHost) && (single_item == 1))
	{
		if ((*list) == NULL) return 0;
		if ((*list)->next == NULL) return 0;

		k = cstring_to_dsdata("name");
		k4 = cstring_to_dsdata("ip_address");
		k6 = cstring_to_dsdata("ipv6_address");
		host = *list;

		for (item = host->next; item != NULL; item = item->next)
		{
			a = dsrecord_attribute(item, k, SELECT_ATTRIBUTE);
			dsrecord_merge_attribute(host, a, SELECT_ATTRIBUTE);
			dsattribute_release(a);

			a = dsrecord_attribute(item, k4, SELECT_ATTRIBUTE);
			dsrecord_merge_attribute(host, a, SELECT_ATTRIBUTE);
			dsattribute_release(a);

			a = dsrecord_attribute(item, k6, SELECT_ATTRIBUTE);
			dsrecord_merge_attribute(host, a, SELECT_ATTRIBUTE);
			dsattribute_release(a);
		}

		dsdata_release(k);
		dsdata_release(k4);
		dsdata_release(k6);

		dsrecord_release(host->next);
		host->next = NULL;
	}

	return 0;
}

u_int32_t
FF_validate(void *c, char *v)
{
	agent_private *ap;
	int n;
	u_int32_t ts, status, check;
	struct stat st;
	char fpath[MAXPATHLEN + 1];

	if (c == NULL) return 0;
	if (v == NULL) return 0;

	ap = (agent_private *)c;

	n = sscanf(v, "%s %u", fpath, &ts);
	if (n != 2) return 0;

	if ((!strcmp(fpath, "/etc/hosts")) && (host_cache.notify_token != -1))
	{
		check = 1;
		status = notify_check(host_cache.notify_token, &check);
		if ((status == NOTIFY_STATUS_OK) && (check == 0)) return 1;
		host_cache.modtime = 0;
	}

	if (stat(fpath, &st) < 0) return 0;
	if (ts == st.st_mtime) return 1;

	return 0;
}
