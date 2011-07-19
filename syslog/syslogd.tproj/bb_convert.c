/*
 * Copyright (c) 2009-2010 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <asl.h>
#include <asl_private.h>
#include <asl_core.h>
#include <asl_file.h>
#include <asl_store.h>

extern time_t asl_parse_time(const char *);

#define TEMP_NAME "_TMP_.asl"
#define STORE_DATA_FLAGS 0x00000000

static const char *store_path = PATH_ASL_STORE;

/*
 * Cache the output file for BB writes.
 * we write messages in the order in which they were generated,
 * so we are almost guaranteed to use the cache in most cases.
 */
static asl_file_t *cache_file = NULL;
static uid_t cache_uid = -1;
static uid_t cache_gid = -1;
static time_t cache_bb = 0;

typedef struct name_list_s
{
	char *name;
	struct name_list_s *next;
} name_list_t;

static name_list_t *
add_to_list(name_list_t *l, const char *name)
{
	name_list_t *e, *x;

	if (name == NULL) return l;

	e = (name_list_t *)calloc(1, sizeof(name_list_t));
	if (e == NULL) return NULL;

	e->name = strdup(name);
	if (e->name == NULL)
	{
		free(e);
		return NULL;
	}

	/* list is sorted by name (i.e. primarily by timestamp) */
	if (l == NULL) return e;

	if (strcmp(e->name, l->name) <= 0)
	{
		e->next = l;
		return e;
	}

	for (x = l; (x->next != NULL) && (strcmp(e->name, x->next->name) > 0) ; x = x->next);

	e->next = x->next;
	x->next = e;
	return l;
}

static void
free_list(name_list_t *l)
{
	name_list_t *e;

	while (l != NULL)
	{
		e = l;
		l = l->next;
		free(e->name);
		free(e);
	}

	free(l);
}

/* find all messages that have an ASLExpireTime key */
static uint32_t
do_ASLExpireTime_search(asl_store_t *s, asl_search_result_t **out)
{
	asl_search_result_t q, *query, *res;
	asl_msg_t *qm[1];
	uint32_t status;
	uint64_t mid;

	qm[0] = asl_msg_new(ASL_TYPE_QUERY);
	if (qm[0] == NULL) return ASL_STATUS_NO_MEMORY;

	q.count = 1;
	q.curr = 0;
	q.msg = qm;
	query = &q;

	if (asl_msg_set_key_val_op(qm[0], ASL_KEY_EXPIRE_TIME, NULL, ASL_QUERY_OP_TRUE) != 0)
	{
		asl_msg_release(qm[0]);
		return ASL_STATUS_NO_MEMORY;
	}

	res = NULL;
	mid = 0;
	status = asl_store_match(s, query, out, &mid, 0, 0, 1);

	asl_msg_release(qm[0]);
	return status;
}

/* remove all messages that have an ASLExpireTime key */
static uint32_t
do_ASLExpireTime_filter(const char *name)
{
	aslmsg msg;
	asl_file_t *in, *out;
	uint32_t status;
	uint64_t mid;
	char *inpath, *outpath;
	struct stat sb;

	if (name == NULL) return ASL_STATUS_INVALID_ARG;

	in = NULL;
	inpath = NULL;
	asprintf(&inpath, "%s/%s", store_path, name);
	if (inpath == NULL) return ASL_STATUS_NO_MEMORY;

	memset(&sb, 0, sizeof(struct stat));
	if (stat(inpath, &sb) < 0)
	{
		free(inpath);
		return ASL_STATUS_INVALID_STORE;
	}

	status = asl_file_open_read(inpath, &in);
	if (status != ASL_STATUS_OK) 
	{
		free(inpath);
		return ASL_STATUS_OK;
	}

	out = NULL;
	outpath = NULL;
	asprintf(&outpath, "%s/%s", store_path, TEMP_NAME);
	if (outpath == NULL)
	{
		asl_file_close(in);
		free(inpath);
		return ASL_STATUS_NO_MEMORY;
	}

	status = asl_file_open_write(outpath, sb.st_mode, sb.st_uid, sb.st_gid, &out);
	if (status != ASL_STATUS_OK)
	{
		asl_file_close(in);
		free(inpath);
		free(outpath);
		return status;
	}

	out->flags = ASL_FILE_FLAG_PRESERVE_MSG_ID;

	msg = NULL;
	while (asl_file_fetch_next(in, &msg) == ASL_STATUS_OK)
	{
		if (msg == NULL) break;

		mid = 0;

		if (asl_get(msg, ASL_KEY_EXPIRE_TIME) == NULL) status = asl_file_save(out, msg, &mid);

		asl_free(msg);
		msg = NULL;

		if (status != ASL_STATUS_OK) break;
	}

	asl_file_close(in);
	asl_file_close(out);

	unlink(inpath);
	rename(outpath, inpath);

	free(inpath);
	free(outpath);

	return status;
}

/* qsort compare function for sorting by message ID */
static int
sort_compare(const void *a, const void *b)
{
	const char *va, *vb;
	uint64_t na, nb;

	va = asl_get(*(aslmsg *)a, ASL_KEY_MSG_ID);
	vb = asl_get(*(aslmsg *)b, ASL_KEY_MSG_ID);

	if (va == NULL) return -1;
	if (vb == NULL) return 1;

	na = atoll(va);
	nb = atoll(vb);

	if (na < nb) return -1;
	if (na > nb) return 1;
	return 0;
}

/* save a message to an appropriately named BB file */
static uint32_t 
save_bb_msg(aslmsg msg)
{
	const char *val;
	uid_t u, ruid;
	gid_t g, rgid;
	struct tm ctm;
	time_t msg_time, bb;
	char *path, *tstring;
	asl_file_t *out;
	uint64_t mid;
	mode_t m;
	uint32_t status;

	if (msg == NULL) return ASL_STATUS_OK;

	val = asl_get(msg, ASL_KEY_EXPIRE_TIME);
	if (val == NULL)  return ASL_STATUS_INVALID_ARG;
	msg_time = asl_parse_time(val);

	val = asl_get(msg, ASL_KEY_READ_UID);
	ruid = -1;
	if (val != NULL) ruid = atoi(val);

	val = asl_get(msg, ASL_KEY_READ_GID);
	rgid = -1;
	if (val != NULL) rgid = atoi(val);

	if (localtime_r((const time_t *)&msg_time, &ctm) == NULL) return ASL_STATUS_FAILED;

	/*
	 * This supports 12 monthy "Best Before" buckets.
	 * We advance the actual expiry time to day zero of the following month.
	 * mktime() is clever enough to know that you actually mean the last day
	 * of the previous month.  What we get back from localtime is the last
	 * day of the month in which the message expires, which we use in the name.
	 */
	ctm.tm_sec = 0;
	ctm.tm_min = 0;
	ctm.tm_hour = 0;
	ctm.tm_mday = 0;
	ctm.tm_mon += 1;

	bb = mktime(&ctm);

	u = 0;
	g = 0;
	if (ruid != -1) u = ruid;
	if (rgid != -1) g = rgid;

	out = NULL;

	if (cache_file != NULL)
	{
		if ((cache_uid == u) && (cache_gid == g) && (cache_bb == bb))
		{
			out = cache_file;
		}
		else
		{
			asl_file_close(cache_file);
			cache_file = NULL;
			cache_uid = -1;
			cache_gid = -1;
			cache_bb = 0;
		}
	}

	if (out == NULL)
	{
		if (localtime_r((const time_t *)&bb, &ctm) == NULL) return ASL_STATUS_FAILED;

		tstring = NULL;
		asprintf(&tstring, "%s/BB.%d.%02d.%02d", store_path, ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
		if (tstring == NULL) return ASL_STATUS_NO_MEMORY;

		path = NULL;
		m = 0644;

		if (ruid == -1)
		{
			if (rgid == -1)
			{
				asprintf(&path, "%s.asl", tstring);
			}
			else
			{
				m = 0640;
				asprintf(&path, "%s.G%d.asl", tstring, g);
			}
		}
		else
		{
			if (rgid == -1)
			{
				m = 0600;
				asprintf(&path, "%s.U%d.asl", tstring, u);
			}
			else
			{
				m = 0640;
				asprintf(&path, "%s.U%d.G%u.asl", tstring, u, g);
			}
		}

		if (path == NULL) return ASL_STATUS_NO_MEMORY;

		status = asl_file_open_write(path, m, u, g, &out);
		free(path);
		if (status != ASL_STATUS_OK) return status;
		if (out == NULL) return ASL_STATUS_FAILED;

		out->flags = ASL_FILE_FLAG_PRESERVE_MSG_ID;

		cache_file = out;
		cache_uid = u;
		cache_gid = g;
		cache_bb = bb;
	}

	status = asl_file_save(out, msg, &mid);

	return status;
}

static uint32_t
finish_conversion()
{
	FILE *sd;
	uint32_t store_flags;
	int status;
	char *path;

	path = NULL;
	asprintf(&path, "%s/%s", store_path, FILE_ASL_STORE_DATA);

	sd = fopen(path, "a");
	free(path);
	if (sd == NULL) return ASL_STATUS_WRITE_FAILED;

	store_flags = STORE_DATA_FLAGS;
	status = fwrite(&store_flags, sizeof(uint32_t), 1, sd);
	fclose(sd);

	if (status != 1) return ASL_STATUS_WRITE_FAILED;

	return ASL_STATUS_OK;
}

/*
 * Utility to convert a data store with LongTTL files into
 * a store with Best Before files.
 *
 * Returns quickly if the data store has already been converted.
 *
 * Older versions of the data store included messages with non-standard time-to-live
 * records in the daily data files (yyyy.mm.dd.asl).  When the files expired, aslmanager
 * first copied messages with ASLExpireTime keys to a LongTTL file, then deleted the
 * original data file.
 *
 * We now write ASLExpireTime messages to a Best Before file (BB.yyyy.mm.dd.asl)
 * and aslmanager just deletes these files after the Best Before date has passed.
 *
 * If StoreData is bigger than 8 bytes, the store has been converted.  Do nothing.
 *
 * Convert the store:
 *    Search the store for messages that have an ASLExpireTime.
 *    Sort by ASLMessageID
 *    Remove all BB.* files and all LongTTL.* files
 *    Write the ASLExpireTime messages into a new set of BB files
 *    Re-write each YMD file without messages that have an ASLExpireTime
 *    Add a new 4-byte flags field to StoreData
 */

uint32_t
bb_convert(const char *name)
{
	struct stat sb;
	asl_store_t *store;
	uint32_t status;
	asl_search_result_t *expire_time_records;
	DIR *dp;
	struct dirent *dent;
	int i;
	name_list_t *list, *e;
	char *path;

	if (name != NULL) store_path = name;

	/* StoreData must exist */
	path = NULL;
	asprintf(&path, "%s/%s", store_path, FILE_ASL_STORE_DATA);
	if (path == NULL) return ASL_STATUS_NO_MEMORY;

	memset(&sb, 0, sizeof(struct stat));
	i = stat(path, &sb);
	free(path);
	if (i != 0) return ASL_STATUS_INVALID_STORE;

	/* must be a regular file */
	if (!S_ISREG(sb.st_mode)) return ASL_STATUS_INVALID_STORE;

	/* check is the store has already been converted */
	if (sb.st_size > sizeof(uint64_t)) return ASL_STATUS_OK;

	/* find ASLExpireTime messages */
	status = asl_store_open_read(store_path, &store);
	if (status != ASL_STATUS_OK) return status;

	expire_time_records = NULL;
	status = do_ASLExpireTime_search(store, &expire_time_records);

	asl_store_close(store);
	if (status != ASL_STATUS_OK) return status;

	/* unlink BB.* and LongTTL.* */
	dp = opendir(store_path);
	if (dp == NULL) return ASL_STATUS_READ_FAILED;

	while ((dent = readdir(dp)) != NULL)
	{
		if ((!strncmp(dent->d_name, "BB.", 3)) || (!strncmp(dent->d_name, "LongTTL.", 8)))
		{
			path = NULL;
			asprintf(&path, "%s/%s", store_path, dent->d_name);
			if (path == NULL)
			{
				closedir(dp);
				return ASL_STATUS_NO_MEMORY;
			}

			unlink(path);
			free(path);
		}
	}

	closedir(dp);

	if ((expire_time_records == NULL) || (expire_time_records->count == 0)) return finish_conversion();

	/* sort by ASLMessageID */
	qsort(expire_time_records->msg, expire_time_records->count, sizeof(aslmsg), sort_compare);

	/* save the ASLExpireTime messages into a new set of BB files */
	for (i = 0; i < expire_time_records->count; i++)
	{
		status = save_bb_msg((aslmsg)expire_time_records->msg[i]);
		if (status != ASL_STATUS_OK)
		{
			if (cache_file != NULL) asl_file_close(cache_file);
			return status;
		}
	}

	if (cache_file != NULL) asl_file_close(cache_file);

	aslresponse_free(expire_time_records);

	/* Re-write each YMD file without messages that have an ASLExpireTime */
	dp = opendir(store_path);
	if (dp == NULL) return ASL_STATUS_READ_FAILED;

	list = NULL;

	while ((dent = readdir(dp)) != NULL)
	{
		if ((dent->d_name[0] < '0') || (dent->d_name[0] > '9')) continue;
		list = add_to_list(list, dent->d_name);
	}

	closedir(dp);

	e = list;
	for (e = list; e != NULL; e = e->next)
	{
		status = do_ASLExpireTime_filter(e->name);
		if (status != ASL_STATUS_OK)
		{
			free_list(list);
			return status;
		}
	}

	free_list(list);

	return finish_conversion();
}
