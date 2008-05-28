/*
 * Copyright (c) 1999-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Unix group lookup
 * Copyright (C) 1989 by NeXT, Inc.
 */

#include <stdlib.h>
#include <mach/mach.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pwd.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <servers/bootstrap.h>
#include <sys/syscall.h>
#include "lu_utils.h"
#include "lu_overrides.h"

#define ENTRY_SIZE sizeof(struct group)
#define ENTRY_KEY _li_data_key_group
#define GROUP_CACHE_SIZE 10

static pthread_mutex_t _group_cache_lock = PTHREAD_MUTEX_INITIALIZER;
static void *_group_cache[GROUP_CACHE_SIZE] = { NULL };
static unsigned int _group_cache_index = 0;
static unsigned int _group_cache_init = 0;

static pthread_mutex_t _group_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * Note that we don't include grp.h and define struct group privately in here
 * to avoid needing to produce a variant version of setgrent, which changed
 * for UXIX03 conformance.
 */
struct group
{
	char *gr_name;
	char *gr_passwd;
	gid_t gr_gid;
	char **gr_mem;
};

/* forward */
int setgrent(void);
struct group *getgrent(void);
void endgrent(void);

/*
 * Support for memberd calls
 */
#define MEMBERD_NAME "com.apple.memberd"
typedef uint32_t GIDArray[16];
extern kern_return_t memberdDSmig_GetGroups(mach_port_t server, uint32_t uid, uint32_t *numGroups, GIDArray gids, audit_token_t *token);
extern kern_return_t memberdDSmig_GetAllGroups(mach_port_t server, uint32_t uid, uint32_t *numGroups, gid_t **gids, uint32_t *gidsCnt, audit_token_t *token);
__private_extern__ uid_t audit_token_uid(audit_token_t a);

#define GR_GET_NAME 1
#define GR_GET_GID 2
#define GR_GET_ENT 3

static struct group *
copy_group(struct group *in)
{
	if (in == NULL) return NULL;

	return (struct group *)LI_ils_create("ss4*", in->gr_name, in->gr_passwd, in->gr_gid, in->gr_mem);
}

/*
 * Extract the next group entry from a kvarray.
 */
static void *
extract_group(kvarray_t *in)
{
	struct group tmp;
	uint32_t d, k, kcount;
	char *empty[1];

	if (in == NULL) return NULL;

	d = in->curr;
	in->curr++;

	if (d >= in->count) return NULL;

	empty[0] = NULL;
	memset(&tmp, 0, ENTRY_SIZE);

	tmp.gr_gid = -2;

	kcount = in->dict[d].kcount;

	for (k = 0; k < kcount; k++)
	{
		if (!strcmp(in->dict[d].key[k], "gr_name"))
		{
			if (tmp.gr_name != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.gr_name = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "gr_passwd"))
		{
			if (tmp.gr_passwd != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.gr_passwd = (char *)in->dict[d].val[k][0];
		}
		else if (!strcmp(in->dict[d].key[k], "gr_gid"))
		{
			if (in->dict[d].vcount[k] == 0) continue;
			tmp.gr_gid = atoi(in->dict[d].val[k][0]);
		}
		else if (!strcmp(in->dict[d].key[k], "gr_mem"))
		{
			if (tmp.gr_mem != NULL) continue;
			if (in->dict[d].vcount[k] == 0) continue;

			tmp.gr_mem = (char **)in->dict[d].val[k];
		}
	}

	if (tmp.gr_name == NULL) tmp.gr_name = "";
	if (tmp.gr_passwd == NULL) tmp.gr_passwd = "";
	if (tmp.gr_mem == NULL) tmp.gr_mem = empty;

	return copy_group(&tmp);
}

static int
copy_group_r(struct group *in, struct group *out, char *buffer, int buflen)
{
	int i, len, hsize;
	unsigned long addr;
	char *bp, *ap;

	if (in == NULL) return -1;
	if (out == NULL) return -1;

	if (buffer == NULL) buflen = 0;

	/* Calculate size of input */
	hsize = 0;
	if (in->gr_name != NULL) hsize += (strlen(in->gr_name) + 1);
	if (in->gr_passwd != NULL) hsize += (strlen(in->gr_passwd) + 1);

	/* NULL pointer at end of list */
	hsize += sizeof(char *);

	len = 0;
	if (in->gr_mem != NULL)
	{
		for (len = 0; in->gr_mem[len] != NULL; len++)
		{
			hsize += sizeof(char *);
			hsize += (strlen(in->gr_mem[len]) + 1);
		}
	}

	/* Check buffer space */
	if (hsize > buflen) return -1;

	/* Copy result into caller's struct group, using buffer for memory */
	bp = buffer;

	out->gr_name = NULL;
	if (in->gr_name != NULL)
	{
		out->gr_name = bp;
		hsize = strlen(in->gr_name) + 1;
		memmove(bp, in->gr_name, hsize);
		bp += hsize;
	}

	out->gr_passwd = NULL;
	if (in->gr_passwd != NULL)
	{
		out->gr_passwd = bp;
		hsize = strlen(in->gr_passwd) + 1;
		memmove(bp, in->gr_passwd, hsize);
		bp += hsize;
	}

	out->gr_gid = in->gr_gid;

	out->gr_mem = NULL;
	ap = bp + ((len + 1) * sizeof(char *));

	if (in->gr_mem != NULL)
	{
		out->gr_mem = (char **)bp;
		for (i = 0; i < len; i++)
		{
			addr = (unsigned long)ap;
			memmove(bp, &addr, sizeof(unsigned long));
			bp += sizeof(unsigned long);

			hsize = strlen(in->gr_mem[i]) + 1;
			memmove(ap, in->gr_mem[i], hsize);
			ap += hsize;
		}
	}

	memset(bp, 0, sizeof(unsigned long));
	bp = ap;

	return 0;
}

static void
cache_group(struct group *gr)
{
	struct group *grcache;

	if (gr == NULL) return;

	pthread_mutex_lock(&_group_cache_lock);

	grcache = copy_group(gr);

	if (_group_cache[_group_cache_index] != NULL) LI_ils_free(_group_cache[_group_cache_index], ENTRY_SIZE);

	_group_cache[_group_cache_index] = grcache;
	_group_cache_index = (_group_cache_index + 1) % GROUP_CACHE_SIZE;

	_group_cache_init = 1;

	pthread_mutex_unlock(&_group_cache_lock);
}

static int
group_cache_check()
{
	uint32_t i, status;

	/* don't consult cache if it has not been initialized */
	if (_group_cache_init == 0) return 1;

	status = LI_L1_cache_check(ENTRY_KEY);

	/* don't consult cache if it is disabled or if we can't validate */
	if ((status == LI_L1_CACHE_DISABLED) || (status == LI_L1_CACHE_FAILED)) return 1;

	/* return 0 if cache is OK */
	if (status == LI_L1_CACHE_OK) return 0;

	/* flush cache */
	pthread_mutex_lock(&_group_cache_lock);

	for (i = 0; i < GROUP_CACHE_SIZE; i++)
	{
		LI_ils_free(_group_cache[i], ENTRY_SIZE);
		_group_cache[i] = NULL;
	}

	_group_cache_index = 0;

	pthread_mutex_unlock(&_group_cache_lock);

	/* don't consult cache - it's now empty */
	return 1;
}


static struct group *
cache_getgrnam(const char *name)
{
	int i;
	struct group *gr, *res;

	if (name == NULL) return NULL;
	if (group_cache_check() != 0) return NULL;

	pthread_mutex_lock(&_group_cache_lock);

	for (i = 0; i < GROUP_CACHE_SIZE; i++)
	{
		gr = (struct group *)_group_cache[i];
		if (gr == NULL) continue;

		if (gr->gr_name == NULL) continue;

		if (!strcmp(name, gr->gr_name))
		{
			res = copy_group(gr);
			pthread_mutex_unlock(&_group_cache_lock);
			return res;
		}
	}

	pthread_mutex_unlock(&_group_cache_lock);
	return NULL;
}

static struct group *
cache_getgrgid(int gid)
{
	int i;
	struct group *gr, *res;

	if (group_cache_check() != 0) return NULL;

	pthread_mutex_lock(&_group_cache_lock);

	for (i = 0; i < GROUP_CACHE_SIZE; i++)
	{
		gr = (struct group *)_group_cache[i];
		if (gr == NULL) continue;

		if ((gid_t)gid == gr->gr_gid)
		{
			res = copy_group(gr);
			pthread_mutex_unlock(&_group_cache_lock);
			return res;
		}
	}

	pthread_mutex_unlock(&_group_cache_lock);
	return NULL;
}

static struct group *
ds_getgrgid(int gid)
{
	static int proc = -1;
	char val[16];

	snprintf(val, sizeof(val), "%d", gid);
	return (struct group *)LI_getone("getgrgid", &proc, extract_group, "gid", val);
}

static struct group *
ds_getgrnam(const char *name)
{
	static int proc = -1;

	return (struct group *)LI_getone("getgrnam", &proc, extract_group, "name", name);
}

/*
 * add a group to a list
 *
 * if dupok is non-zero, it's OK to add a duplicate entry
 * if dupok is zero, we only add the gid if it is new
 * (*listcount) is incremented if the gid was added
 * returns -1 if adding the gid would overflow the list
 *
 */
static void
_add_group(gid_t g, gid_t **list, uint32_t *count, int dupok)
{
	uint32_t i, n, addit;

	addit = 1;

	if (list == NULL) return;
	if (*list == NULL) *count = 0;

	n = *count;

	if (dupok == 0) 
	{
		for (i = 0; (i < n) && (addit == 1); i++)
		{
			if ((*list)[i] == g) addit = 0;
		}
	}

	if (addit == 0) return;

	if (*list == NULL) *list = (gid_t *)calloc(1, sizeof(gid_t));
	else *list = (gid_t *)realloc(*list, (n + 1) * sizeof(gid_t));

	if (*list == NULL)
	{
		*count = 0;
		return;
	}

	(*list)[n] = g;
	*count = n + 1;
}

int
_old_getgrouplist(const char *uname, int basegid, int *groups, int *grpcnt)
{
	struct group *grp;
	int i, status;
	uint32_t maxgroups, gg_count;
	gid_t *gg_list;

	status = 0;
	maxgroups = (uint32_t)*grpcnt;
	*grpcnt = 0;

	gg_list = NULL;
	gg_count = 0;

	/*
	 * When installing primary group, duplicate it;
	 * the first element of groups is the effective gid
	 * and will be overwritten when a setgid file is executed.
	 */
	_add_group(basegid, &gg_list, &gg_count, 0);
	_add_group(basegid, &gg_list, &gg_count, 1);

	if (gg_list == NULL)
	{
		errno = ENOMEM;
		return 0;
	}

	/*
	 * Scan the group file to find additional groups.
	 */
	setgrent();

	while ((grp = getgrent()))
	{
		if (grp->gr_gid == (gid_t)basegid) continue;
		for (i = 0; grp->gr_mem[i]; i++)
		{
			if (!strcmp(grp->gr_mem[i], uname))
			{
				_add_group(grp->gr_gid, &gg_list, &gg_count, 0);
				break;
			}
		}
	}

	endgrent();

	if (gg_list == NULL)
	{
		errno = ENOMEM;
		return 0;
	}

	/* return -1 if the user-supplied list is too short */
	status = 0;
	if (gg_count > maxgroups) status = -1;

	/* copy at most maxgroups gids from gg_list to groups */
	for (i = 0; (i < maxgroups) && (i < gg_count); i++) groups[i] = gg_list[i];

	*grpcnt = gg_count;
	free(gg_list);

	return status;
}

/*
 * Guess at the size of a password buffer for getpwnam_r
 * pw_name can be MAXLOGNAME + 1         256 - sys/param.h
 * pw_passwd can be _PASSWORD_LEN + 1    129 - pwd.h
 * pw_dir can be MAXPATHLEN + 1         1025 - sys/syslimits.h
 * pw_shell can be MAXPATHLEN +         1025 - sys/syslimits.h
 * We allow pw_class and pw_gecos to take a maximum of 4098 bytes (there's no limit on these).
 * This adds to 6533 bytes (until one of the constants changes)
 */
#define MAXPWBUF (MAXLOGNAME + 1 + _PASSWORD_LEN + 1 + MAXPATHLEN + 1 + MAXPATHLEN + 1 + 4098)

/*
 * This is the "old" client side routine from memberd.
 * It now talks to DirectoryService, but it retains the old style where
 * the caller provides an array for the output gids.  It fetches the 
 * user's gids from DS, then copies as many as possible into the
 * caller-supplied array.
 */
static int
mbr_getgrouplist(const char *name, int basegid, int *groups, int *grpcnt, int dupbase)
{
	struct passwd p, *res;
	char buf[MAXPWBUF];
	kern_return_t kstatus;
	uint32_t i, maxgroups, count, gg_count;
	int pwstatus;
	GIDArray gids;
	gid_t *gidptr, *gg_list;
	size_t gidptrsz;
	int status, do_dealloc;
	audit_token_t token;

	if (_ds_port == MACH_PORT_NULL) return 0;
	if (name == NULL) return 0;
	if (groups == NULL) return 0;
	if (grpcnt == NULL) return 0;

	maxgroups = (uint32_t)(*grpcnt);
	do_dealloc = 0;
	*grpcnt = 0;
	gidptr = NULL;
	gidptrsz = 0;
	gg_list = NULL;
	gg_count = 0;
	
	_add_group(basegid, &gg_list, &gg_count, 0);
	if (dupbase != 0) _add_group(basegid, &gg_list, &gg_count, 1);

	if (gg_list == NULL)
	{
		errno = ENOMEM;
		return 0;
	}

	memset(&p, 0, sizeof(struct passwd));
	memset(buf, 0, sizeof(buf));
	res = NULL;

	pwstatus = getpwnam_r(name, &p, buf, MAXPWBUF, &res);
	if (pwstatus != 0) return 0;
	if (res == NULL) return 0;

	count = 0;
	memset(&token, 0, sizeof(audit_token_t));

	kstatus = 0;
	if (maxgroups > 16)
	{
		uint32_t gidptrCnt = 0;
		kstatus = memberdDSmig_GetAllGroups(_ds_port, p.pw_uid, &count, &gidptr, &gidptrCnt, &token);
		gidptrsz = gidptrCnt * sizeof(gid_t);
		do_dealloc = 1;
	}
	else
	{
		kstatus = memberdDSmig_GetGroups(_ds_port, p.pw_uid, &count, gids, &token);
		gidptr = (gid_t *)gids;
	}

	if (kstatus != KERN_SUCCESS) return 0;
	if (audit_token_uid(token) != 0)
	{
		if (gg_list != NULL) free(gg_list);
		return 0;
	}

	for (i = 0; i < count; i++) _add_group(gidptr[i], &gg_list, &gg_count, 0);

	if ((do_dealloc == 1) && (gidptr != NULL)) vm_deallocate(mach_task_self(), (vm_address_t)gidptr, gidptrsz);

	if (gg_list == NULL)
	{
		errno = ENOMEM;
		return 0;
	}

	/* return -1 if the user-supplied list is too short */
	status = 0;
	if (gg_count > maxgroups) status = -1;

	/* copy at most maxgroups gids from gg_list to groups */
	for (i = 0; (i < maxgroups) && (i < gg_count); i++) groups[i] = gg_list[i];

	*grpcnt = gg_count;
	free(gg_list);

	return status;
}

/*
 * This is the "modern" routine for fetching the group list for a user.
 * The grplist output parameter is allocated and filled with the gids
 * of the specified user's groups.  Returns the number of gids in the
 * list or -1 on failure.  Caller must free() the returns grplist.
 */
static int32_t
ds_getgrouplist(const char *name, gid_t basegid, gid_t **grplist, int dupbase)
{
	struct passwd p, *res;
	char buf[MAXPWBUF];
	kern_return_t kstatus;
	uint32_t i, count, gidptrCnt, out_count;
	int pwstatus;
	gid_t *gidptr, *out_list;
	size_t gidptrsz;
	audit_token_t token;

	if (_ds_port == MACH_PORT_NULL) return -1;
	if (name == NULL) return -1;
	if (grplist == NULL) return -1;

	gidptr = NULL;
	gidptrCnt = 0;
	gidptrsz = 0;
	out_list = NULL;
	out_count = 0;

	_add_group(basegid, &out_list, &out_count, 0);
	if (dupbase != 0) _add_group(basegid, &out_list, &out_count, 1);

	if (out_list == NULL) return -1;

	memset(&p, 0, sizeof(struct passwd));
	memset(buf, 0, sizeof(buf));
	res = NULL;
	
	pwstatus = getpwnam_r(name, &p, buf, MAXPWBUF, &res);
	if (pwstatus != 0) return -1;
	if (res == NULL) return -1;
	
	count = 0;
	memset(&token, 0, sizeof(audit_token_t));
	
	kstatus = memberdDSmig_GetAllGroups(_ds_port, p.pw_uid, &count, &gidptr, &gidptrCnt, &token);
	if (kstatus != KERN_SUCCESS)
	{
		if (out_list != NULL) free(out_list);
		return -1;
	}
	gidptrsz = gidptrCnt * sizeof(gid_t);

	if (audit_token_uid(token) != 0)
	{
		if (out_list != NULL) free(out_list);
		if (gidptr != NULL) vm_deallocate(mach_task_self(), (vm_address_t)gidptr, gidptrsz);
		return -1;
	}

	for (i = 0; i < count; i++) _add_group(gidptr[i], &out_list, &out_count, 0);

	if (gidptr != NULL) vm_deallocate(mach_task_self(), (vm_address_t)gidptr, gidptrsz);

	*grplist = out_list;
	return out_count;
}

static int
getgrouplist_internal(const char *name, int basegid, int *groups, int *grpcnt, int dupbase)
{
	int status, in_grpcnt;

	/*
	 * The man page says that the grpcnt parameter will be set to the actual number
	 * of groups that were found.  Unfortunately, older impementations of this API
	 * have always set grpcnt to the number of groups that are being returned.
	 * To prevent regressions in callers of this API, we respect the old and 
	 * incorrect implementation.
	 */

	in_grpcnt = *grpcnt;
	status = 0;

	if (_ds_running()) status = mbr_getgrouplist(name, basegid, groups, grpcnt, dupbase);
	else status = _old_getgrouplist(name, basegid, groups, grpcnt);

	if ((status < 0) && (*grpcnt > in_grpcnt)) *grpcnt = in_grpcnt;
	return status;
}

static int32_t
getgrouplist_internal_2(const char *name, gid_t basegid, gid_t **gid_list, int dupbase)
{
	int status;
	uint32_t gid_count;

	if (name == NULL) return -1;
	if (gid_list == NULL) return -1;

	*gid_list = NULL;

	if (_ds_running()) return ds_getgrouplist(name, basegid, gid_list, dupbase);

	gid_count = NGROUPS + 1;
	*gid_list = (gid_t *)calloc(gid_count, sizeof(gid_t));
	if (*gid_list == NULL) return -1;

	status = _old_getgrouplist(name, basegid, (int *)gid_list, (int *)&gid_count);
	if (status < 0) return -1;
	return gid_count;
}

int
getgrouplist(const char *uname, int agroup, int *groups, int *grpcnt)
{
	return getgrouplist_internal(uname, agroup, groups, grpcnt, 0);
}

int32_t
getgrouplist_2(const char *uname, gid_t agroup, gid_t **groups)
{
	return getgrouplist_internal_2(uname, agroup, groups, 0);
}

static void
ds_endgrent(void)
{
	LI_data_free_kvarray(LI_data_find_key(ENTRY_KEY));
}

static void
ds_setgrent(void)
{
	ds_endgrent();
}

static struct group *
ds_getgrent()
{
	static int proc = -1;

	return (struct group *)LI_getent("getgrent", &proc, extract_group, ENTRY_KEY, ENTRY_SIZE);
}

static struct group *
getgr_internal(const char *name, gid_t gid, int source)
{
	struct group *res = NULL;
	int add_to_cache;

	add_to_cache = 0;
	res = NULL;

	switch (source)
	{
		case GR_GET_NAME:
			res = cache_getgrnam(name);
			break;
		case GR_GET_GID:
			res = cache_getgrgid(gid);
			break;
		default: res = NULL;
	}

	if (res != NULL)
	{
	}
	else if (_ds_running())
	{
		switch (source)
		{
			case GR_GET_NAME:
				res = ds_getgrnam(name);
				break;
			case GR_GET_GID:
				res = ds_getgrgid(gid);
				break;
			case GR_GET_ENT:
				res = ds_getgrent();
				break;
			default: res = NULL;
		}

		if (res != NULL) add_to_cache = 1;
	}
	else
	{
		pthread_mutex_lock(&_group_lock);

		switch (source)
		{
			case GR_GET_NAME:
				res = copy_group(_old_getgrnam(name));
				break;
			case GR_GET_GID:
				res = copy_group(_old_getgrgid(gid));
				break;
			case GR_GET_ENT:
				res = copy_group(_old_getgrent());
				break;
			default: res = NULL;
		}

		pthread_mutex_unlock(&_group_lock);
	}

	if (add_to_cache == 1) cache_group(res);

	return res;
}

static struct group *
getgr(const char *name, gid_t gid, int source)
{
	struct group *res = NULL;
	struct li_thread_info *tdata;

	tdata = LI_data_create_key(ENTRY_KEY, ENTRY_SIZE);
	if (tdata == NULL) return NULL;

	res = getgr_internal(name, gid, source);

	LI_data_recycle(tdata, res, ENTRY_SIZE);
	return (struct group *)tdata->li_entry;
}

static int
getgr_r(const char *name, gid_t gid, int source, struct group *grp, char *buffer, size_t bufsize, struct group **result)
{
	struct group *res = NULL;
	int status;

	*result = NULL;

	res = getgr_internal(name, gid, source);
	if (res == NULL) return 0;

	status = copy_group_r(res, grp, buffer, bufsize);

	LI_ils_free(res, ENTRY_SIZE);

	if (status != 0) return ERANGE;

	*result = grp;
	return 0;
}

int
initgroups(const char *name, int basegid)
{
	int status, pwstatus, ngroups, groups[NGROUPS];
	struct passwd p, *res;
	char buf[MAXPWBUF];

	/* get the UID for this user */
	memset(&p, 0, sizeof(struct passwd));
	memset(buf, 0, sizeof(buf));
	res = NULL;

	pwstatus = getpwnam_r(name, &p, buf, MAXPWBUF, &res);
	if (pwstatus != 0) return -1;
	if (res == NULL) return -1;

	ngroups = NGROUPS;

	status = getgrouplist_internal(name, basegid, groups, &ngroups, 0);
	if (status < 0) return status;

	status = syscall(SYS_initgroups, ngroups, groups, p.pw_uid);
	if (status < 0) return -1;

	return 0;
}

struct group *
getgrnam(const char *name)
{
	return getgr(name, -2, GR_GET_NAME);
}

struct group *
getgrgid(gid_t gid)
{
	return getgr(NULL, gid, GR_GET_GID);
}

struct group *
getgrent(void)
{
	return getgr(NULL, -2, GR_GET_ENT);
}

int
setgrent(void)
{
	if (_ds_running()) ds_setgrent();
	else _old_setgrent();
	return 1;
}

void
endgrent(void)
{
	if (_ds_running()) ds_endgrent();
	else _old_endgrent();
}

int
getgrnam_r(const char *name, struct group *grp, char *buffer, size_t bufsize, struct group **result)
{
	return getgr_r(name, -2, GR_GET_NAME, grp, buffer, bufsize, result);
}

int
getgrgid_r(gid_t gid, struct group *grp, char *buffer, size_t bufsize, struct group **result)
{
	return getgr_r(NULL, gid, GR_GET_GID, grp, buffer, bufsize, result);
}
