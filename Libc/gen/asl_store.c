/*
 * Copyright (c) 2007-2008 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2007 Apple Inc.  All Rights
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <asl.h>
#include <asl_private.h>
#include <asl_core.h>
#include <asl_store.h>
#include <notify.h>

extern time_t asl_parse_time(const char *str);
extern uint64_t asl_file_cursor(asl_file_t *s);
extern uint32_t asl_file_match_start(asl_file_t *s, uint64_t start_id, int32_t direction);
extern uint32_t asl_file_match_next(asl_file_t *s, aslresponse query, asl_msg_t **msg, uint64_t *last_id, int32_t direction, int32_t ruid, int32_t rgid);

#define SECONDS_PER_DAY 86400

/* 
 * The ASL Store is organized as a set of files in a common directory.
 * Files are prefixed by the date (YYYY.MM.DD) of their contents.
 * There are also files for long-TTL (> 1 day) messages.
 *
 * Messages with no access controls are saved in YYYY.MM.DD.asl
 * Messages with access limited to UID U are saved in YYYY.MM.DD.uU.asl
 * Messages with access limited to GID G are saved in YYYY.MM.DD.gG.asl
 * Messages with access limited to UID U and GID G are saved in YYYY.MM.DD.uU.gG.asl
 *
 * An external tool runs daily and deletes "old" files.
 */

/*
 * The base directory contains a data file which stores
 * the last record ID.
 *
 * | MAX_ID (uint64_t) |
 *
 */
uint32_t
asl_store_open_write(const char *basedir, asl_store_t **s)
{
	asl_store_t *out;
	asl_file_t *db;
	struct stat sb;
	uint32_t i, status;
	char *path, *subpath;
	time_t now;
	struct tm ctm;
	FILE *sd;
	uint64_t last_id;

	if (s == NULL) return ASL_STATUS_INVALID_ARG;

	if (basedir == NULL) basedir = PATH_ASL_STORE;

	memset(&sb, 0, sizeof(struct stat));
	if (stat(basedir, &sb) != 0) return ASL_STATUS_INVALID_STORE;
	if ((sb.st_mode & S_IFDIR) == 0) return ASL_STATUS_INVALID_STORE;

	path = NULL;
	asprintf(&path, "%s/%s", basedir, FILE_ASL_STORE_DATA);
	if (path == NULL) return ASL_STATUS_NO_MEMORY;

	sd = NULL;

	memset(&sb, 0, sizeof(struct stat));
	if (stat(path, &sb) != 0)
	{
		if (errno != ENOENT)
		{
			free(path);
			return ASL_STATUS_FAILED;
		}

		sd = fopen(path, "w+");
		free(path);

		if (sd == NULL) return ASL_STATUS_FAILED;

		last_id = 0;

		if (fwrite(&last_id, sizeof(uint64_t), 1, sd) != 1)
		{
			fclose(sd);
			return ASL_STATUS_WRITE_FAILED;
		}
	}
	else
	{
		sd = fopen(path, "r+");
		free(path);

		if (sd == NULL) return ASL_STATUS_FAILED;
		if (fread(&last_id, sizeof(uint64_t), 1, sd) != 1)
		{
			fclose(sd);
			return ASL_STATUS_READ_FAILED;
		}

		last_id = asl_core_ntohq(last_id);
	}

	memset(&ctm, 0, sizeof(struct tm));
	now = time(NULL);

	if (localtime_r((const time_t *)&now, &ctm) == NULL)
	{
		fclose(sd);
		return ASL_STATUS_FAILED;
	}

	subpath = NULL;
	asprintf(&subpath, "%s/%d.%02d.%02d", basedir, ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
	if (subpath == NULL)
	{
		fclose(sd);
		return ASL_STATUS_NO_MEMORY;
	}

	path = NULL;
	asprintf(&path, "%s.asl", subpath);
	free(subpath);
	if (path == NULL)
	{
		fclose(sd);
		return ASL_STATUS_NO_MEMORY;
	}

	db = NULL;
	status = asl_file_open_write(path, 0644, 0, 0, &db);
	free(path);
	if ((status != ASL_STATUS_OK) || (db == NULL))
	{
		fclose(sd);
		return ASL_STATUS_FAILED;
	}

	out = (asl_store_t *)calloc(1, sizeof(asl_store_t));
	if (out == NULL)
	{
		fclose(sd);
		asl_file_close(db);
		return ASL_STATUS_NO_MEMORY;
	}

	if (basedir == NULL) out->base_dir = strdup(PATH_ASL_STORE);
	else out->base_dir = strdup(basedir);

	if (out->base_dir == NULL)
	{
		fclose(sd);
		asl_file_close(db);
		free(out);
		return ASL_STATUS_NO_MEMORY;
	}

	ctm.tm_sec = 0;
	ctm.tm_min = 0;
	ctm.tm_hour = 0;

	out->start_today = mktime(&ctm);
	out->start_tomorrow = out->start_today + SECONDS_PER_DAY;
	out->db = db;
	out->storedata = sd;
	out->next_id = last_id + 1;

	for (i = 0; i < FILE_CACHE_SIZE; i++)
	{
		memset(&out->file_cache[i], 0, sizeof(asl_cached_file_t));
		out->file_cache[i].u = -1;
		out->file_cache[i].g = -1;
	}

	*s = out;
	return ASL_STATUS_OK;
}

uint32_t
asl_store_statistics(asl_store_t *s, aslmsg *msg)
{
	aslmsg out;
	char str[256];

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msg == NULL) return ASL_STATUS_INVALID_ARG;

	out = (aslmsg)calloc(1, sizeof(asl_msg_t));
	if (out == NULL) return ASL_STATUS_NO_MEMORY;

	snprintf(str, sizeof(str), "%u", s->db->string_count);
	asl_set(out, "StringCount", str);

	*msg = out;
	return ASL_STATUS_OK;
}

uint32_t
asl_store_open_read(const char *basedir, asl_store_t **s)
{
	asl_store_t *out;
	struct stat sb;

	if (s == NULL) return ASL_STATUS_INVALID_ARG;

	if (basedir == NULL) basedir = PATH_ASL_STORE;

	memset(&sb, 0, sizeof(struct stat));
	if (stat(basedir, &sb) != 0) return ASL_STATUS_INVALID_STORE;
	if ((sb.st_mode & S_IFDIR) == 0) return ASL_STATUS_INVALID_STORE;

	out = (asl_store_t *)calloc(1, sizeof(asl_store_t));
	if (out == NULL) return ASL_STATUS_NO_MEMORY;

	if (basedir == NULL) out->base_dir = strdup(PATH_ASL_STORE);
	else out->base_dir = strdup(basedir);

	if (out->base_dir == NULL)
	{
		free(out);
		return ASL_STATUS_NO_MEMORY;
	}

	*s = out;
	return ASL_STATUS_OK;
}

uint32_t
asl_store_max_file_size(asl_store_t *s, size_t max)
{
	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	s->max_file_size = max;
	return ASL_STATUS_OK;
}

void
asl_store_file_closeall(asl_store_t *s)
{
	uint32_t i;

	if (s == NULL) return;

	for (i = 0; i < FILE_CACHE_SIZE; i++)
	{
		if (s->file_cache[i].f != NULL) asl_file_close(s->file_cache[i].f);
		s->file_cache[i].f = NULL;
		if (s->file_cache[i].path != NULL) free(s->file_cache[i].path);
		s->file_cache[i].path = NULL;
		s->file_cache[i].u = -1;
		s->file_cache[i].g = -1;
		s->file_cache[i].ts = 0;
	}
}

uint32_t
asl_store_close(asl_store_t *s)
{
	if (s == NULL) return ASL_STATUS_OK;

	if (s->base_dir != NULL) free(s->base_dir);
	s->base_dir = NULL;
	asl_file_close(s->db);
	asl_store_file_closeall(s);
	if (s->storedata != NULL) fclose(s->storedata);

	free(s);

	return ASL_STATUS_OK;
}

uint32_t
asl_store_signal_sweep(asl_store_t *s)
{
	char *str;
	int semfd;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	asprintf(&str, "%s/%s", s->base_dir, FILE_ASL_STORE_SWEEP_SEMAPHORE);
	if (str == NULL) return ASL_STATUS_NO_MEMORY;

	semfd = open(str, O_WRONLY | O_CREAT | O_NONBLOCK, 0644);
	free(str);

	if (semfd <  0) return ASL_STATUS_WRITE_FAILED;

	close(semfd);
	return ASL_STATUS_OK;
}

/*
 * Sweep the file cache.
 * Close any files that have not been used in the last FILE_CACHE_TTL seconds.
 * Returns least recently used or unused cache slot.
 */
static uint32_t
asl_store_file_cache_lru(asl_store_t *s, time_t now)
{
	time_t min;
	uint32_t i, x;

	if (s == NULL) return 0;

	x = 0;
	min = now - FILE_CACHE_TTL;
	
	for (i = 0; i < FILE_CACHE_SIZE; i++)
	{
		if (s->file_cache[i].ts < min)
		{
			asl_file_close(s->file_cache[i].f);
			s->file_cache[i].f = NULL;
			if (s->file_cache[i].path != NULL) free(s->file_cache[i].path);
			s->file_cache[i].path = NULL;
			s->file_cache[i].u = -1;
			s->file_cache[i].g = -1;
			s->file_cache[i].ts = 0;			
		}
		
		if (s->file_cache[i].ts < s->file_cache[x].ts) x = i;
	}

	return x;
}

uint32_t
asl_store_sweep_file_cache(asl_store_t *s)
{
	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	asl_store_file_cache_lru(s, time(NULL));
	return ASL_STATUS_OK;
}

static uint32_t
asl_store_file_open_write(asl_store_t *s, char *subpath, int32_t ruid, int32_t rgid, asl_file_t **f, time_t now, uint32_t check_cache)
{
	char *path;
	mode_t m;
	int32_t i, x, u, g;
	uint32_t status;
	asl_file_t *out;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	/* see if the file is already open and in the cache */
	for (i = 0; i < FILE_CACHE_SIZE; i++)
	{
		if ((s->file_cache[i].u == ruid) && (s->file_cache[i].g == rgid) && (s->file_cache[i].f != NULL))
		{
			s->file_cache[i].ts = now;
			*f = s->file_cache[i].f;
			if (check_cache == 1) asl_store_file_cache_lru(s, now);
			return ASL_STATUS_OK;
		}
	}

	path = NULL;
	u = 0;
	g = 0;
	m = 0644;

	if (ruid == -1)
	{
		if (rgid == -1)
		{
			asprintf(&path, "%s.asl", subpath);
		}
		else
		{
			g = rgid;
			m = 0640;
			asprintf(&path, "%s.G%d.asl", subpath, g);
		}
	}
	else
	{
		u = ruid;
		if (rgid == -1)
		{
			m = 0600;
			asprintf(&path, "%s.U%d.asl", subpath, u);
		}
		else
		{
			g = rgid;
			m = 0640;
			asprintf(&path, "%s.U%d.G%u.asl", subpath, u, g);
		}
	}

	if (path == NULL) return ASL_STATUS_NO_MEMORY;

	out = NULL;
	status = asl_file_open_write(path, m, u, g, &out);
	if (status != ASL_STATUS_OK)
	{
		free(path);
		return status;
	}

	x = asl_store_file_cache_lru(s, now);
	if (s->file_cache[x].f != NULL) asl_file_close(s->file_cache[x].f);
	if (s->file_cache[x].path != NULL) free(s->file_cache[x].path);

	s->file_cache[x].f = out;
	s->file_cache[x].path = path;
	s->file_cache[x].u = ruid;
	s->file_cache[x].g = rgid;
	s->file_cache[x].ts = time(NULL);

	*f = out;

	return ASL_STATUS_OK;
}

char *
asl_store_file_path(asl_store_t *s, asl_file_t *f)
{
	uint32_t i;

	if (s == NULL) return NULL;

	for (i = 0; i < FILE_CACHE_SIZE; i++)
	{
		if (s->file_cache[i].f == f)
		{
			if (s->file_cache[i].path == NULL) return NULL;
			return strdup(s->file_cache[i].path);
		}
	}

	return NULL;
}

void
asl_store_file_close(asl_store_t *s, asl_file_t *f)
{
	uint32_t i;

	if (s == NULL) return;

	for (i = 0; i < FILE_CACHE_SIZE; i++)
	{
		if (s->file_cache[i].f == f)
		{
			asl_file_close(s->file_cache[i].f);
			s->file_cache[i].f = NULL;
			if (s->file_cache[i].path != NULL) free(s->file_cache[i].path);
			s->file_cache[i].path = NULL;
			s->file_cache[i].u = -1;
			s->file_cache[i].g = -1;
			s->file_cache[i].ts = 0;
			return;
		}
	}
}

uint32_t
asl_store_save(asl_store_t *s, aslmsg msg)
{
	struct tm ctm;
	time_t t, now;
	char *path, *subpath;
	const char *val;
	uid_t ruid;
	gid_t rgid;
	asl_file_t *f;
	uint32_t status, check_cache;
	asl_store_t *tmp;
	uint64_t xid, ftime;
	size_t fsize;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (msg == NULL) return ASL_STATUS_INVALID_ARG;

	now = time(NULL);

	val = asl_get(msg, ASL_KEY_TIME);
	t = 0;
	if (val == NULL) t = now;
	else t = asl_parse_time(val);

	if (t >= s->start_tomorrow)
	{
		if (now >= s->start_tomorrow)
		{
			/* new day begins */
			tmp = NULL;
			status = asl_store_open_write(s->base_dir, &tmp);
			asl_file_close(s->db);
			s->db = NULL;
			if (status != ASL_STATUS_OK)
			{
				fclose(s->storedata);
				free(s->base_dir);
				free(s);
				return status;
			}

			s->db = tmp->db;
			s->start_today = tmp->start_today;
			s->start_tomorrow = tmp->start_tomorrow;
			free(tmp->base_dir);
			fclose(tmp->storedata);
			free(tmp);

			status = asl_store_signal_sweep(s);
			/* allow this to fail quietly */
		}
	}

	val = asl_get(msg, ASL_KEY_READ_UID);
	ruid = -1;
	if (val != NULL) ruid = atoi(val);

	val = asl_get(msg, ASL_KEY_READ_GID);
	rgid = -1;
	if (val != NULL) rgid = atoi(val);

	if (fseeko(s->storedata, 0, SEEK_SET) != 0) return ASL_STATUS_WRITE_FAILED;

	xid = asl_core_htonq(s->next_id);
	if (fwrite(&xid, sizeof(uint64_t), 1, s->storedata) != 1) return ASL_STATUS_WRITE_FAILED;

	xid = s->next_id;
	s->next_id++;

	check_cache = 0;
	if ((s->last_write + FILE_CACHE_TTL) <= now) check_cache = 1;

	s->last_write = now;
	
	if ((t >= s->start_today) && (t < s->start_tomorrow) && (ruid == -1) && (rgid == -1))
	{
		status = asl_file_save(s->db, msg, &xid);
		if (check_cache == 1) asl_store_file_cache_lru(s, now);
		return status;
	}

	if (localtime_r((const time_t *)&t, &ctm) == NULL) return ASL_STATUS_FAILED;

	asprintf(&subpath, "%s/%d.%02d.%02d", s->base_dir, ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
	if (subpath == NULL) return ASL_STATUS_NO_MEMORY;

	f = NULL;
	status = asl_store_file_open_write(s, subpath, ruid, rgid, &f, now, check_cache);
	free(subpath);
	subpath = NULL;

	if (status != ASL_STATUS_OK) return status;

	status = asl_file_save(f, msg, &xid);
	if (status != ASL_STATUS_OK) return status;

	fsize = asl_file_size(f);
	ftime = asl_file_ctime(f);

	/* if file is larger than max_file_size, rename it and create semaphore file in the store */
	if ((s->max_file_size != 0) && (fsize > s->max_file_size))
	{
		status = ASL_STATUS_OK;

		path = asl_store_file_path(s, f);
		subpath = NULL;

		asl_store_file_close(s, f);

		if (path != NULL)
		{
			asprintf(&subpath, "%s.%llu", path, ftime);
			if (subpath == NULL)
			{
				status = ASL_STATUS_NO_MEMORY;
			}
			else
			{
				if (rename(path, subpath) != 0) status = ASL_STATUS_FAILED;
				free(subpath);
			}

			free(path);
		}

		if (status == ASL_STATUS_OK) status = asl_store_signal_sweep(s);
	}

	return status;
}

uint32_t
asl_store_match_timeout(asl_store_t *s, aslresponse query, aslresponse *res, uint64_t *last_id, uint64_t start_id, uint32_t count, int32_t direction, uint32_t usec)
{
	DIR *dp;
	struct dirent *dent;
	uint32_t status;
	asl_file_t *f;
	char *path;
	asl_file_list_t *files;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (res == NULL) return ASL_STATUS_INVALID_ARG;

	files = NULL;

	/*
	 * Open all readable files
	 */
	dp = opendir(s->base_dir);
	if (dp == NULL) return ASL_STATUS_READ_FAILED;

	while ((dent = readdir(dp)) != NULL)
	{
		if (dent->d_name[0] == '.') continue;

		path = NULL;
		asprintf(&path, "%s/%s", s->base_dir, dent->d_name);

		/* NB asl_file_open_read will fail if path is NULL, if the file is not an ASL store file, or if it isn't readable */
		status = asl_file_open_read(path, &f);
		if (path != NULL) free(path);
		if ((status != ASL_STATUS_OK) || (f == NULL)) continue;

		files = asl_file_list_add(files, f);
	}

	closedir(dp);

	status = asl_file_list_match_timeout(files, query, res, last_id, start_id, count, direction, usec);
	asl_file_list_close(files);
	return status;
}

uint32_t
asl_store_match(asl_store_t *s, aslresponse query, aslresponse *res, uint64_t *last_id, uint64_t start_id, uint32_t count, int32_t direction)
{
	return asl_store_match_timeout(s, query, res, last_id, start_id, count, direction, 0);
}

uint32_t
asl_store_match_start(asl_store_t *s, uint64_t start_id, int32_t direction)
{
	DIR *dp;
	struct dirent *dent;
	uint32_t status;
	asl_file_t *f;
	char *path;
	asl_file_list_t *files;

	if (s == NULL) return ASL_STATUS_INVALID_STORE;

	if (s->work != NULL) asl_file_list_match_end(s->work);
	s->work = NULL;

	files = NULL;

	/*
	 * Open all readable files
	 */
	dp = opendir(s->base_dir);
	if (dp == NULL) return ASL_STATUS_READ_FAILED;

	while ((dent = readdir(dp)) != NULL)
	{
		if (dent->d_name[0] == '.') continue;

		path = NULL;
		asprintf(&path, "%s/%s", s->base_dir, dent->d_name);

		/* NB asl_file_open_read will fail if path is NULL, if the file is not an ASL store file, or if it isn't readable */
		status = asl_file_open_read(path, &f);
		if (path != NULL) free(path);
		if ((status != ASL_STATUS_OK) || (f == NULL)) continue;

		files = asl_file_list_add(files, f);
	}

	closedir(dp);

	s->work = asl_file_list_match_start(files, start_id, direction);
	if (s->work == NULL) return ASL_STATUS_FAILED;

	return ASL_STATUS_OK;
}

uint32_t
asl_store_match_next(asl_store_t *s, aslresponse query, aslresponse *res, uint32_t count)
{
	if (s == NULL) return ASL_STATUS_INVALID_STORE;
	if (s->work == NULL) return ASL_STATUS_OK;

	return asl_file_list_match_next(s->work, query, res, count);
}
