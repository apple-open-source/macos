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
 * NIAgent.m
 * Written by Marc Majka
 */

#import "NIAgent.h"
#import "Config.h"
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import <sys/types.h>
#import <sys/socket.h>
#import <netinet/in.h>
#import <arpa/inet.h>
#import <NetInfo/syslock.h>
#import <NetInfo/system_log.h>
#import <NetInfo/dsutil.h>
#import <NetInfo/nilib2.h>
#import "LUGlobal.h"

#define forever for(;;)

#define DEFAULT_TIMEOUT 30
#define DEFAULT_CONNECT_TIMEOUT 300
#define DEFAULT_LATENCY 30

static unsigned long _shared_handle_count_ = 0;
static ni_shared_handle_t **_shared_handle_ = NULL;
static ni_shared_handle_t *_shared_local_ = NULL;

static char *pathForCategory[] =
{
	"/users",
	"/groups",
	"/machines",
	"/networks",
	"/services",
	"/protocols",
	"/rpcs",
	"/mounts",
	"/printers",
	"/machines",
	"/machines",
	"/aliases",
	"/netdomains",
	NULL,
	NULL,
	NULL,
	NULL
};

typedef struct ni_private
{
        int naddrs;
        struct in_addr *addrs;
        int whichwrite;
        ni_name *tags;
        int pid;
        int tsock;
        int tport;
        CLIENT *tc;
        long tv_sec;
        long rtv_sec;
        long wtv_sec;
        int abort;
        int needwrite;
        int uid;
        ni_name passwd;
} ni_private;

static ni_shared_handle_t *
ni_shared_handle(struct in_addr *addr, char *tag)
{
	struct sockaddr_in sa;
	void *domain, *d2;
	ni_status status;
	ni_id root;
	ni_shared_handle_t *h;

	if (addr == NULL) return NULL;
	if (tag == NULL) return NULL;

	memset(&sa, 0, sizeof(struct in_addr));
	sa.sin_family = AF_INET;
	sa.sin_addr = *addr;

	domain = ni_connect(&sa, tag);

	if (domain == NULL) return NULL;

	if (strcmp(tag, "local") != 0)
	{
		d2 = ni_new(domain, ".");
		ni_free(domain);
		domain = d2;
		if (domain == NULL) return NULL;
	}

	root.nii_object = 0;
	root.nii_instance = 0;

	status = ni_self(domain, &root);

	if (status != NI_OK) 
	{
		ni_free(domain);
		return NULL;
	}

	h = (ni_shared_handle_t *)malloc(sizeof(ni_shared_handle_t));
	memset(h, 0, sizeof(ni_shared_handle_t));
	h->refcount = 1;
	h->ni = domain;

	return h;
}
	
static ni_shared_handle_t *
ni_shared_local(void)
{
	struct in_addr loop;

	if (_shared_local_ != NULL)
	{
		_shared_local_->refcount++;
		return _shared_local_;
	}

	memset(&loop, 0, sizeof(struct in_addr));
	loop.s_addr = htonl(INADDR_LOOPBACK);

	_shared_local_ = ni_shared_handle(&loop, "local");

	return _shared_local_;
}

static int
ni_shared_match(ni_shared_handle_t *h, struct in_addr *a, char *t)
{
	ni_private *ni;
	unsigned long i;

	if (h == NULL) return 0;
	if (h->ni == NULL) return 0;
	if (a == NULL) return 0;
	if (t == NULL) return 0;

	ni = (ni_private *)h->ni;
	if (ni == NULL) return 0;

	for (i = 0; i < ni->naddrs; i++)
	{
		if ((ni->addrs[i].s_addr == a->s_addr) && (strcmp(ni->tags[i], t) ==  0)) return 1;
	}

	return 0;
}

static ni_shared_handle_t *
ni_shared_connection(struct in_addr *addr, char *tag)
{
	unsigned long i;
	ni_shared_handle_t *h;

	if (addr == NULL) return NULL;
	if (tag == NULL) return NULL;

	if (!strcmp(tag, "local") && (addr->s_addr == htonl(INADDR_LOOPBACK)))
	{
		return ni_shared_local();
	}

	for (i = 0; i < _shared_handle_count_; i++)
	{
		if (ni_shared_match(_shared_handle_[i], addr, tag))
		{
			_shared_handle_[i]->refcount++;
			return _shared_handle_[i];
		}
	}
	
	h = ni_shared_handle(addr, tag);
	if (h == NULL) return NULL;

	if (_shared_handle_count_ == 0)
	{
		_shared_handle_ = (ni_shared_handle_t **)malloc(sizeof(ni_shared_handle_t *));
	}
	else
	{
		_shared_handle_ = (ni_shared_handle_t **)realloc(_shared_handle_, (_shared_handle_count_ + 1) * sizeof(ni_shared_handle_t *));
	}

	_shared_handle_[_shared_handle_count_] = h;
	_shared_handle_count_++;

	return h;
}

static void
ni_shared_release(ni_shared_handle_t *h)
{
	unsigned long i, j;

	if (h == NULL) return;

	if (h->refcount > 0) h->refcount--;
	if (h->refcount > 0) return;
	
	ni_free(h->ni);

	if (h == _shared_local_)
	{
		free(_shared_local_);
		_shared_local_ = NULL;
		return;
	}

	for (i = 0; i < _shared_handle_count_; i++)
	{
		if (_shared_handle_[i] == h)
		{
			free(_shared_handle_[i]);
			for (j = i + 1; j < _shared_handle_count_; j++, i++)
			{
				_shared_handle_[i] = _shared_handle_[j];
			}
			_shared_handle_count_--;
			if (_shared_handle_count_ == 0)
			{
				free(_shared_handle_);
				_shared_handle_ = NULL;
				return;
			}
			_shared_handle_ = (ni_shared_handle_t **)realloc(_shared_handle_, _shared_handle_count_ * sizeof(ni_shared_handle_t *));
			return;
		}
	}
}

static ni_shared_handle_t *
ni_shared_parent(ni_shared_handle_t *h)
{
	ni_rparent_res *rpres;
	ni_private *ni;
	struct in_addr addr;
	ni_shared_handle_t *p;

	if (h == NULL) return NULL;
	if (h->ni == NULL) return NULL;

	ni = (ni_private *)h->ni;

	rpres = _ni_rparent_2(NULL, ni->tc);

	if (rpres == NULL) return NULL;
	if (rpres->status != NI_OK) return NULL;

	addr.s_addr = htonl(rpres->ni_rparent_res_u.binding.addr);
	p = ni_shared_connection(&addr, rpres->ni_rparent_res_u.binding.tag);
	free(rpres->ni_rparent_res_u.binding.tag);

	return p;
}

static ni_shared_handle_t *
ni_shared_open(void *x, char *rel)
{
	void *d;
	ni_private *ni;
	ni_status status;
	ni_shared_handle_t *h;

	if (rel == NULL) return NULL;

	status = ni_open(x, rel, &d);
	if (status != NI_OK) return NULL;

	ni = (ni_private *)d;
	h = ni_shared_connection(&(ni->addrs[0]), ni->tags[0]);
	ni_free(d);
	return h;
}

static ni_status
_ni_find(ni_data *domain, unsigned long domain_count, char *str, unsigned long *where, ni_id *dir)
{
	ni_status status;
	unsigned long i;

	if (str == NULL) return NI_NODIR;
	if (where == NULL) return NI_NODIR;
	if (dir == NULL) return NI_NODIR;

	for (i = 0; i < domain_count; i++)
	{
		status = ni_pathsearch(domain[i].handle->ni, dir, str);
		if (status == NI_OK)
		{
			*where = i;
			return status;
		}
	}
	return NI_NODIR;
}

static ni_status
_ni_find_any(ni_data *domain, unsigned long domain_count, char **str, unsigned long *where, ni_id *dir)
{
	ni_status status;
	unsigned long i, j;

	if (str == NULL) return NI_NODIR;
	if (where == NULL) return NI_NODIR;
	if (dir == NULL) return NI_NODIR;

	for (i = 0; i < domain_count; i++)
	{
		for (j = 0; str[j] != NULL; j++)
		{
			status = ni_pathsearch(domain[i].handle->ni, dir, str[j]);
			if (status == NI_OK)
			{
				*where = i;
				return status;
			}
		}
	}
	return NI_NODIR;
}

@implementation NIAgent

- (NIAgent *)init
{
	return (NIAgent *)[self initWithArg:NULL];
}

- (LUAgent *)initWithArg:(char *)arg
{
	LUDictionary *config;
	LUDictionary *global;
	char *p, **domain_order;
	int i, len;

	[super initWithArg:arg];

	latency = DEFAULT_LATENCY;
	timeout = DEFAULT_TIMEOUT;
	connect_timeout = DEFAULT_CONNECT_TIMEOUT;

	config = [configManager configForAgent:"NIAgent" fromConfig:configurationArray];
	global = [configManager configGlobal:configurationArray];
	if (global != nil)
	{
		if ([global valueForKey:"Timeout"] != NULL)
			timeout = [global unsignedLongForKey:"Timeout"];
		if ([global valueForKey:"ConnectTimeout"] != NULL)
			connect_timeout = [global unsignedLongForKey:"ConnectTimeout"];
		if ([global valueForKey:"ValidationLatency"] != NULL)
			latency = [global unsignedLongForKey:"ValidationLatency"];
	}
	if (config != nil)
	{
		if ([config valueForKey:"Timeout"] != NULL)
			timeout = [config unsignedLongForKey:"Timeout"];
		if ([config valueForKey:"ConnectTimeout"] != NULL)
			connect_timeout = [config unsignedLongForKey:"ConnectTimeout"];
		if ([config valueForKey:"ValidationLatency"] != NULL)
			latency = [config unsignedLongForKey:"ValidationLatency"];
	}

	p = NULL;

	if ((arg == NULL) && (config != nil))
	{
		domain_order = [config valuesForKey:"DomainOrder"];
		if (domain_order != NULL)
		{
			len = 0;
			for (i = 0; domain_order[i] != NULL; i++) len += (strlen(domain_order[i]) + 1);
			if (len > 0)
			{
				p = malloc(len + 1);
				memset(p, 0, len + 1);
				for (i = 0; domain_order[i] != NULL; i++)
				{
					strcat(p, domain_order[i]);
					strcat(p, ",");
				}
				p[len - 1] = '\0';
			}
		}
	}

	domain = NULL;
	domain_count = 0;

	if (p == NULL)
	{
		[self setSource:arg];
	}
	else
	{
		[self setSource:p];
		free(p);
	}

	return self;
}

- (void)dealloc
{
	unsigned long i;

	syslock_lock(rpcLock);
	for (i = 0; i < domain_count; i++)
	{
		ni_shared_release(domain[i].handle);
	}
	syslock_unlock(rpcLock);

	free(domain);
	domain = NULL;
	domain_count = 0;

	system_log(LOG_DEBUG, "Deallocated NIAgent 0x%08x\n", (int)self);

	[super dealloc];
}

- (void)climbToRoot
{
	ni_shared_handle_t *h0, *h1;

	if (domain_count == 0) return;

	h0 = domain[domain_count - 1].handle;

	ni_setabort(h0->ni, 1);
	ni_setreadtimeout(h0->ni, connect_timeout);

	forever
	{
		syslock_lock(rpcLock);
		h1 = ni_shared_parent(h0);
		syslock_unlock(rpcLock);

		if (h1 == NULL) return;
	
		h0 = h1;

		ni_setreadtimeout(h0->ni, timeout);
		ni_setpassword(h0->ni, "checksum");
		domain = (ni_data *)realloc(domain, (domain_count + 1) * sizeof(ni_data));
		domain[domain_count].handle = h0;
		domain[domain_count].checksum = 0;
		domain[domain_count].checksum_time = 0;
		domain_count++;
	}
}

- (void)setSource:(char *)arg
{
	ni_shared_handle_t *h;
	char **list, *str;
	unsigned long i, len;

	if (arg == NULL)
	{
		syslock_lock(rpcLock);
		h = ni_shared_local();
		syslock_unlock(rpcLock);

		if (h == NULL) return;

		domain = (ni_data *)malloc(sizeof(ni_data));
		ni_setreadtimeout(h->ni, timeout);
		ni_setpassword(h->ni, "checksum");
		domain[0].handle = h;
		domain[0].checksum = 0;
		domain[0].checksum_time = 0;
		domain_count = 1;

		[self climbToRoot];
		return;
	}
	
	str = malloc(strlen(arg) + 16);
	sprintf(str, "NIAgent (%s)", arg);
	[self setBanner:str];
	free(str);

	list = explode(arg, ",");
	len = listLength(list);
	for (i = 0; i < len; i++)
	{
		if (streq(list[i], "...")) [self climbToRoot];
		else
		{
			syslock_lock(rpcLock);
			h = ni_shared_open(NULL, list[i]);
			syslock_unlock(rpcLock);

			if (h == NULL) continue;

			ni_setreadtimeout(h->ni, timeout);
			ni_setpassword(h->ni, "checksum");
			domain = (ni_data *)realloc(domain, (domain_count + 1) * sizeof(ni_data));
			domain[domain_count].handle = h;
			domain[domain_count].checksum = 0;
			domain[domain_count].checksum_time = 0;
			domain_count++;
		}
	}
	freeList(list);
}

- (BOOL)findDirectory:(char *)path domain:(void **)d nidir:(ni_id *)nid
{
	ni_status status;
	unsigned long where;

	where = IndexNull;
	status = _ni_find(domain, domain_count, path, &where, nid);
	if (status == NI_OK) *d = domain[where].handle->ni;
	return (status == NI_OK);
}

- (LUDictionary *)dictionaryForNIProplist:(ni_proplist *)p
{
	LUDictionary *item;
	int i, j, len;
	unsigned int where;

	if (p == NULL) return nil;

	item = [[LUDictionary alloc] init];

	for (i = 0; i < p->ni_proplist_len; i++)
	{
		if (p->ni_proplist_val[i].nip_name == NULL) continue;

		where = [item addKey:p->ni_proplist_val[i].nip_name];
		if (where == IndexNull) continue;

		len = p->ni_proplist_val[i].nip_val.ni_namelist_len;
		for (j = 0; j < len; j++)
		{
			[item addValue:p->ni_proplist_val[i].nip_val.ni_namelist_val[j] atIndex:where];
		}
	}

	return item;
}

- (const char *)shortName
{
	return "NI";
}

- (unsigned long)checksumForIndex:(unsigned long)i
{
	struct timeval now;
	unsigned long age;
	ni_status status;
	ni_proplist pl;
	unsigned long sum;
	ni_index where;

	gettimeofday(&now, (struct timezone *)NULL);
	age = now.tv_sec - domain[i].checksum_time;
	if (age <= latency) return domain[i].checksum;

	NI_INIT(&pl);

	syslock_lock(rpcLock);
	status = ni_statistics(domain[i].handle->ni, &pl);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		system_log(LOG_ERR, "ni_statistics: %s", ni_error(status));
		ni_proplist_free(&pl);
		return 0;
	}

	/* checksum should be first (and only!) property */
	where = NI_INDEX_NULL;
	if (pl.ni_proplist_len > 0)
	{
		if (strcmp(pl.ni_proplist_val[0].nip_name, "checksum"))
			where = 0;
		else
			where = ni_proplist_match(pl, "checksum", NULL);
	}

	if (where == NI_INDEX_NULL)
	{
		system_log(LOG_ERR, "Domain %lu: can't get checksum", i);
		ni_proplist_free(&pl);
		return 0;
	}

	sscanf(pl.ni_proplist_val[where].nip_val.ni_namelist_val[0], "%lu", &sum);
	ni_proplist_free(&pl);

	domain[i].checksum_time = now.tv_sec;
	domain[i].checksum = sum;

	return sum;
}

- (LUDictionary *)stamp:(LUDictionary *)item index:(unsigned long)i
{
	char str[64];

	if (item == nil) return nil;


	[item setAgent:self];
	[item setValue:"NI" forKey:"_lookup_info_system"];

	sprintf(str, "%lu", i);
	[item setValue:str forKey:"_lookup_NI_index"];

	sprintf(str, "%lu", [self checksumForIndex:i]);
	[item setValue:str forKey:"_lookup_NI_checksum"];

	return item;
}

- (BOOL)isValid:(LUDictionary *)item
{
	unsigned long oldsum, newsum, i;
	char *c;

	if (item == nil) return NO;
	if ([self isStale]) return NO;

	c = [item valueForKey:"_lookup_NI_checksum"];
	if (c == NULL) return NO;
	sscanf(c, "%lu", &oldsum);

	c = [item valueForKey:"_lookup_NI_index"];
	if (c == NULL) return NO;
	sscanf(c, "%lu", &i);

	newsum = [self checksumForIndex:i];

	if (oldsum != newsum) return NO;
	return YES;
}

- (LUDictionary *)itemWithKey:(char *)key
	value:(char *)val
	category:(LUCategory)cat
{
	char *path, *str, **names;
	LUDictionary *item;
	ni_status status;
	ni_proplist pl;
	ni_id dir;
	unsigned long where;
	BOOL tryRealName;

	if (key == NULL) return nil;
	if (val == NULL) return nil;
	
	path = pathForCategory[cat];
	if (path == NULL) return nil;

	str = malloc(strlen(path) + strlen(key) + strlen(val) + 3);
	sprintf(str, "%s/%s=%s", path, key, val);

	/*
	 * Special case for user by name queries:
	 * we check for both "name" and "realname" with the given value.
	 */
	tryRealName = NO;
	if ((cat == LUCategoryUser) && (streq(key, "name")))
	{
		tryRealName = YES;
		names = appendString(str, NULL);
		free(str);
		str = malloc(strlen(path) + strlen(val) + 11);
		sprintf(str, "%s/realname=%s", path, val);
		names = appendString(str, names);
	}

	where = IndexNull;
	status = NI_NODIR;

	syslock_lock(rpcLock);
	if (tryRealName)
	{
		status = _ni_find_any(domain, domain_count, names, &where, &dir);
		freeList(names);
	}
	else
	{
		status = _ni_find(domain, domain_count, str, &where, &dir);
	}
	syslock_unlock(rpcLock);

	free(str);
	if (status != NI_OK) return nil;

	NI_INIT(&pl);
	syslock_lock(rpcLock);
	status = ni_read(domain[where].handle->ni, &dir, &pl);
	syslock_unlock(rpcLock);

	if (status != NI_OK)
	{
		system_log(LOG_ERR, "ni_read: %s", ni_error(status));
		ni_proplist_free(&pl);
		return nil;
	}

	item = [self dictionaryForNIProplist:&pl];
	ni_proplist_free(&pl);

	return [self stamp:item index:where];
}

- (void)allChildren:(char *)path index:(unsigned long)where append:(LUArray *)all
{
	ni_proplist_list l;
	ni_id dir;
	int i;
	ni_status status;
	LUDictionary *item;

	syslock_lock(rpcLock);
	status = ni_pathsearch(domain[where].handle->ni, &dir, path);
	syslock_unlock(rpcLock);

	if (status != NI_OK) return;

	NI_INIT(&l);
	syslock_lock(rpcLock);
	status = ni_listall(domain[where].handle->ni, &dir, &l);
	syslock_unlock(rpcLock);

	if (status == NI_NODIR) return;
	if (status != NI_OK)
	{
		system_log(LOG_ERR, "ni_listall: %s", ni_error(status));
		return;
	}

	if (l.ni_proplist_list_len == 0) return;
	
	for (i = 0; i < l.ni_proplist_list_len; i++)
	{
		item = [self dictionaryForNIProplist:&(l.ni_proplist_list_val[i])];
		if (item == nil) continue;

		[all addObject:item];
		[item release];
	}

	ni_proplist_list_free(&l);
}

- (void)lookupChildren:(char *)path key:(char *)key value:(char *)val index:(unsigned long)where append:(LUArray *)all
{
	ni_proplist l;
	ni_idlist idl;
	ni_id dir;
	int i;
	ni_status status;
	LUDictionary *item;

	syslock_lock(rpcLock);
	status = ni_pathsearch(domain[where].handle->ni, &dir, path);
	syslock_unlock(rpcLock);

	if (status != NI_OK) return;

	NI_INIT(&idl);
	syslock_lock(rpcLock);
	status = ni_lookup(domain[where].handle->ni, &dir, key, val, &idl);
	syslock_unlock(rpcLock);

	if (status == NI_NODIR) return;
	if (status != NI_OK)
	{
		system_log(LOG_ERR, "ni_lookup: %s", ni_error(status));
		ni_idlist_free(&idl);
		return;
	}

	if (idl.ni_idlist_len == 0) return;
	
	for (i = 0; i < idl.ni_idlist_len; i++)
	{
		dir.nii_object = idl.ni_idlist_val[i];

		NI_INIT(&l);
		syslock_lock(rpcLock);
		status = ni_read(domain[where].handle->ni, &dir, &l);
		syslock_unlock(rpcLock);
		
		item = [self dictionaryForNIProplist:&l];
		ni_proplist_free(&l);
		if (item == nil) continue;

		[all addObject:item];
		[item release];
	}

	ni_idlist_free(&idl);
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat key:(char *)key value:(char *)val
{
	char *path;
	LUArray *all;
	LUDictionary *vstamp;
	unsigned long i;
	char str[128];

	path = pathForCategory[cat];
	if (path == NULL) return nil;

	all = [[LUArray alloc] init];

	for (i = 0; i < domain_count; i++)
	{
		if (key == NULL) [self allChildren:path index:i append:all];
		else [self lookupChildren:path key:key value:val index:i append:all];

		vstamp = [[LUDictionary alloc] init];
		sprintf(str, "V-0x%08x", (int)vstamp);
		[vstamp setBanner:str];

		[self stamp:vstamp index:i];
		[all addValidationStamp:vstamp];
		[vstamp release];
	}

	return all;
}

- (LUArray *)allItemsWithCategory:(LUCategory)cat
{
	return [self allItemsWithCategory:cat key:NULL value:NULL];
}

/*
 * Faster query for NetInfo searches.
 */
- (LUArray *)query:(LUDictionary *)pattern category:(LUCategory)cat
{
	LUArray *all, *list;
	int i, len;
	char *p, *k, *v;

	if (pattern == nil) return [self allItemsWithCategory:cat key:NULL value:NULL];

	len = [pattern count];
	if (len == 0) return [self allItemsWithCategory:cat key:NULL value:NULL];

	k = NULL;
	v = NULL;

	/* Use "name" key if given (it has a good chance of matching fewer records) */
	i = [pattern indexForKey:"name"];
	if (i != IndexNull)
	{
		k = [pattern keyAtIndex:i];
		v = [pattern valueAtIndex:i];
	}

	/* If there was no "name" key, use first key without "_lookup_" prefix */
	for (i = 0; (i < len) && (k == NULL); i++)
	{
		p = [pattern keyAtIndex:i];
		if (!strncmp(p, "_lookup_", 8)) continue;

		k = p;
		v = [pattern valueAtIndex:i];
	}
	
	all = [self allItemsWithCategory:cat key:k value:v];

	list = nil;
	if (all != nil)
	{
		list = [all filter:pattern];
		if (list != nil)
		{
			len = [all validationStampCount];
			for (i = 0; i < len; i++)
			{
				[list addValidationStamp:[all validationStampAtIndex:i]];
			}
		}
		[all release];
	}

	return list;
}

/*
 * Custom lookup for security options
 *
 * Special case: "all" enables all security options
 */
- (BOOL)isSecurityEnabledForOption:(char *)option
{
	ni_id dir;
	ni_status status;
	unsigned long i, j;
	ni_namelist nl;

	if (option == NULL) return NO;

	dir.nii_object = 0;

	for (i = 0; i < domain_count; i++)
	{
		NI_INIT(&nl);
		syslock_lock(rpcLock);
		status = ni_lookupprop(domain[i].handle->ni, &dir, "security_options", &nl);
		syslock_unlock(rpcLock);

		if (status != NI_OK)
		{
			ni_namelist_free(&nl);
			continue;
		}

		for (j = 0; j < nl.ni_namelist_len; j++)
		{
			if (streq(nl.ni_namelist_val[j], option) || streq(nl.ni_namelist_val[j], "all"))
			{
				ni_namelist_free(&nl);
				return YES;
			}
		}

		ni_namelist_free(&nl);
	}

	return NO;
}

@end
