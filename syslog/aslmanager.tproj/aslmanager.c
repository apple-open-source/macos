/*
 * Copyright (c) 2007-2009 Apple Inc. All rights reserved.
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
#include <sys/param.h>
#include <servers/bootstrap.h>
#include <bootstrap_priv.h>
#include <mach/mach.h>
#include <copyfile.h>
#include <fcntl.h>
#include <zlib.h>
#include <xpc/xpc.h>
#include <xpc/private.h>
#include <os/assumes.h>
#include <vproc_priv.h>
#include <asl.h>
#include <asl_private.h>
#include <asl_core.h>
#include <asl_file.h>
#include <asl_store.h>
#include "asl_common.h"

#define DEFAULT_MAX_SIZE 150000000
#define IOBUFSIZE 4096

#define DO_ASLDB	0x00000001
#define DO_MODULE	0x00000002
#define DO_CHECKPT	0x00000004

#define DEBUG_FLAG_MASK  0xfffffff0
#define DEBUG_LEVEL_MASK 0x0000000f
#define DEBUG_STDERR     0x00000010
#define DEBUG_ASL        0x00000020

#define AUX_URL_MINE "file:///var/log/asl/"
#define AUX_URL_MINE_LEN 20

/* length of "file://" */
#define AUX_URL_PATH_OFFSET 7

extern kern_return_t _asl_server_query
(
 mach_port_t server,
 caddr_t request,
 mach_msg_type_number_t requestCnt,
 uint64_t startid,
 int count,
 int flags,
 caddr_t *reply,
 mach_msg_type_number_t *replyCnt,
 uint64_t *lastid,
 int *status,
 security_token_t *token
);

/* global */
static time_t module_ttl;
static uint32_t debug;
static int dryrun;
static int asl_aux_fd = -1;
static aslclient aslc;
static mach_port_t asl_server_port;
static xpc_connection_t listener;
static dispatch_queue_t serverq;

typedef struct name_list_s
{
	char *name;
	size_t size;
	struct name_list_s *next;
} name_list_t;

static const char *
keep_str(uint8_t mask)
{
	static char str[9];
	uint32_t x = 0;

	memset(str, 0, sizeof(str));
	if (mask & 0x01) str[x++] = '0';
	if (mask & 0x02) str[x++] = '1';
	if (mask & 0x04) str[x++] = '2';
	if (mask & 0x08) str[x++] = '3';
	if (mask & 0x10) str[x++] = '4';
	if (mask & 0x20) str[x++] = '5';
	if (mask & 0x40) str[x++] = '6';
	if (mask & 0x80) str[x++] = '7';
	if (x == 0) str[x++] = '-';
	return str;
}

void
set_debug(int flag, const char *str)
{
	int level, x;

	if (str == NULL) x = ASL_LEVEL_ERR;
	else if (((str[0] == 'L') || (str[0] == 'l')) && ((str[1] >= '0') && (str[1] <= '7')) && (str[2] == '\0')) x = atoi(str+1);
	else if ((str[0] >= '0') && (str[0] <= '7') && (str[1] == '\0')) x = ASL_LEVEL_CRIT + atoi(str);
	else x = ASL_LEVEL_ERR;

	if (x <= 0) x = 0;
	else if (x > 7) x = 7;

	level = debug & DEBUG_LEVEL_MASK;
	if (x > level) level = x;

	debug = debug & DEBUG_FLAG_MASK;
	debug |= flag;
	debug |= level;
}

void
debug_log(int level, const char *str, ...)
{
	va_list v;

	if ((debug & DEBUG_STDERR) && (level <= (debug & DEBUG_LEVEL_MASK)))
	{
		va_start(v, str);
		vfprintf(stderr, str, v);
		va_end(v);
	}

	if (debug & DEBUG_ASL)
	{
		char *line = NULL;

		if (aslc == NULL)
		{
			aslc = asl_open("aslmanager", "syslog", 0);
			asl_msg_t *msg = asl_msg_new(ASL_TYPE_MSG);

			asl_msg_set_key_val(msg, ASL_KEY_MSG, "Status Report");
			asl_msg_set_key_val(msg, ASL_KEY_LEVEL, ASL_STRING_NOTICE);
			asl_create_auxiliary_file((asl_object_t)msg, "Status Report", "public.text", &asl_aux_fd);
			asl_msg_release(msg);
		}

		va_start(v, str);
		vasprintf(&line, str, v);
		va_end(v);

		if (line != NULL) write(asl_aux_fd, line, strlen(line));
		free(line);
	}
}

__attribute__((noreturn)) static void
xpc_server_exit(int status)
{
	xpc_connection_cancel(listener);
	xpc_release(listener);
	dispatch_release(serverq);
	exit(status);
}

name_list_t *
add_to_name_list(name_list_t *l, const char *name, size_t size)
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

	e->size = size;

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

void
free_name_list(name_list_t *l)
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
/*
 * Copy ASL files by reading and writing each record.
 */
uint32_t
copy_asl_file(const char *src, const char *dst, mode_t mode)
{
	asl_msg_list_t *res;
	asl_file_t *f;
	uint32_t status, i;
	uint64_t mid;
	size_t rcount;

	if (src == NULL) return ASL_STATUS_INVALID_ARG;
	if (dst == NULL) return ASL_STATUS_INVALID_ARG;

	f = NULL;
	status = asl_file_open_read(src, &f);
	if (status != ASL_STATUS_OK) return status;

	res = NULL;
	mid = 0;

	res = asl_file_match(f, NULL, &mid, 0, 0, 0, 1);
	asl_file_close(f);

	if (res == NULL) return ASL_STATUS_OK;
	rcount = asl_msg_list_count(res);
	if (rcount == 0)
	{
		asl_msg_list_release(res);
		return ASL_STATUS_OK;
	}

	f = NULL;
	status = asl_file_open_write(dst, mode, -1, -1, &f);
	if (status != ASL_STATUS_OK) return status;
	if (f == ASL_STATUS_OK) return ASL_STATUS_FAILED;

	f->flags = ASL_FILE_FLAG_PRESERVE_MSG_ID;

	for (i = 0; i < rcount; i++)
	{
		mid = 0;
		status = asl_file_save(f, asl_msg_list_get_index(res, i), &mid);
		if (status != ASL_STATUS_OK) break;
	}

	asl_file_close(f);
	return status;
}

int
copy_compress_file(asl_out_dst_data_t *asldst, const char *src, const char *dst)
{
	int in, out;
	size_t n;
	gzFile gz;
	char buf[IOBUFSIZE];

	in = open(src, O_RDONLY, 0);
	if (in < 0) return -1;

	out = open(dst, O_WRONLY | O_CREAT, asldst->mode);
	if (out >= 0) out = asl_out_dst_set_access(out, asldst);
	if (out < 0)
	{
		close(in);
		return -1;
	}

	gz = gzdopen(out, "w");
	if (gz == NULL)
	{
		close(in);
		close(out);
		return -1;
	}

	do {
		n = read(in, buf, sizeof(buf));
		if (n > 0) gzwrite(gz, buf, n);
	} while (n == IOBUFSIZE);

	gzclose(gz);
	close(in);
	close(out);

	return 0;
}

void
filesystem_rename(const char *src, const char *dst)
{
	int status = 0;

	debug_log(ASL_LEVEL_NOTICE, "  rename %s ---> %s\n", src, dst);
	if (dryrun == 1) return;

	status = rename(src, dst);
	if (status != 0) debug_log(ASL_LEVEL_ERR, "  FAILED status %d errno %d [%s] rename %s ---> %s\n", status, errno, strerror(errno), src, dst);
}

void
filesystem_unlink(const char *path)
{
	int status = 0;
	
	debug_log(ASL_LEVEL_NOTICE, "  remove %s\n", path);
	if (dryrun == 1) return;
	
	status = unlink(path);
	if (status != 0) debug_log(ASL_LEVEL_ERR, "  FAILED status %d errno %d [%s] unlink %s\n", status, errno, strerror(errno), path);
}

void
filesystem_truncate(const char *path)
{
	int status = 0;
	
	debug_log(ASL_LEVEL_NOTICE, "  truncate %s\n", path);
	if (dryrun == 1) return;
	
	status = truncate(path, 0);
	if (status != 0) debug_log(ASL_LEVEL_ERR, "  FAILED status %d errno %d [%s] unlink %s\n", status, errno, strerror(errno), path);
}

void
filesystem_rmdir(const char *path)
{
	int status = 0;

	debug_log(ASL_LEVEL_NOTICE, "  remove directory %s\n", path);
	if (dryrun == 1) return;

	status = rmdir(path);
	if (status != 0) debug_log(ASL_LEVEL_ERR, "  FAILED status %d errno %d [%s] rmdir %s\n", status, errno, strerror(errno), path);
}

int32_t
filesystem_copy(asl_out_dst_data_t *asldst, const char *src, const char *dst, uint32_t flags)
{
	char *dot;

	if ((src == NULL) || (dst == NULL)) return 0;

	dot = strrchr(src, '.');
	if ((dot != NULL) && (!strcmp(dot, ".gz"))) flags &= ~MODULE_FLAG_COMPRESS;

	if (((flags & MODULE_FLAG_COMPRESS) == 0) && (!strcmp(src, dst))) return 0;

	if (flags & MODULE_FLAG_TYPE_ASL) debug_log(ASL_LEVEL_NOTICE, "  copy asl %s ---> %s\n", src, dst);
	else if (flags & MODULE_FLAG_COMPRESS) debug_log(ASL_LEVEL_NOTICE, "  copy compress %s ---> %s.gz\n", src, dst);
	else debug_log(ASL_LEVEL_NOTICE, "  copy %s ---> %s\n", src, dst);

	if (dryrun == 1) return 0;

	if (flags & MODULE_FLAG_TYPE_ASL)
	{
		uint32_t status = copy_asl_file(src, dst, asldst->mode);
		if (status != 0)
		{
			debug_log(ASL_LEVEL_ERR, "  FAILED status %u [%s] asl copy %s ---> %s\n", status, asl_core_error(status), src, dst);
			return 0;
		}
	}
	else if (flags & MODULE_FLAG_COMPRESS)
	{
		char gzdst[MAXPATHLEN];

		snprintf(gzdst, sizeof(gzdst), "%s.gz", dst);

		int status = copy_compress_file(asldst, src, gzdst);
		if (status != 0)
		{
			debug_log(ASL_LEVEL_ERR, "  FAILED status %d errno %d [%s] copy & compress %s ---> %s\n", status, errno, strerror(errno), src, dst);
			return 0;
		}
	}
	else
	{
		int status = copyfile(src, dst, NULL, COPYFILE_ALL | COPYFILE_RECURSIVE);
		if (status != 0)
		{
			debug_log(ASL_LEVEL_ERR, "  FAILED status %d errno %d [%s] copy %s ---> %s\n", status, errno, strerror(errno), src, dst);
			return 0;
		}
	}

	return 1;
}

int
remove_directory(const char *path)
{
	DIR *dp;
	struct dirent *dent;
	char *str;

	dp = opendir(path);
	if (dp == NULL) return 0;

	while ((dent = readdir(dp)) != NULL)
	{
		if ((!strcmp(dent->d_name, ".")) || (!strcmp(dent->d_name, ".."))) continue;
		asprintf(&str, "%s/%s", path, dent->d_name);
		if (str != NULL)
		{
			filesystem_unlink(str);
			free(str);
			str = NULL;
		}
	}

	closedir(dp);
	filesystem_rmdir(path);

	return 0;
}

/*
 * Determine the age (in whole days) of a YMD file from its name.
 * Also determines UID and GID from ".Unnn.Gnnn" part of file name.
 */
uint32_t
ymd_file_age(const char *name, time_t now, uid_t *u, gid_t *g)
{
	struct tm ftime;
	time_t created;
	uint32_t days;
	const char *p;

	if (name == NULL) return 0;

	if (now == 0) now = time(NULL);

	memset(&ftime, 0, sizeof(struct tm));
	ftime.tm_hour = 24;

	/* name is YYYY.MM.DD.<...> */

	if ((name[0] < '0') || (name[0] > '9')) return 0;
	ftime.tm_year = 1000 * (name[0] - '0');

	if ((name[1] < '0') || (name[1] > '9')) return 0;
	ftime.tm_year += 100 * (name[1] - '0');

	if ((name[2] < '0') || (name[2] > '9')) return 0;
	ftime.tm_year += 10 * (name[2] - '0');

	if ((name[3] < '0') || (name[3] > '9')) return 0;
	ftime.tm_year += name[3] - '0';
	ftime.tm_year -= 1900;

	if (name[4] != '.') return 0;

	if ((name[5] < '0') || (name[5] > '9')) return 0;
	ftime.tm_mon = 10 * (name[5] - '0');

	if ((name[6] < '0') || (name[6] > '9')) return 0;
	ftime.tm_mon += name[6] - '0';
	ftime.tm_mon -= 1;

	if (name[7] != '.') return 0;

	if ((name[8] < '0') || (name[8] > '9')) return 0;
	ftime.tm_mday = 10 * (name[8] - '0');

	if ((name[9] < '0') || (name[9] > '9')) return 0;
	ftime.tm_mday += name[9] - '0';

	if (name[10] != '.') return 0;

	created = mktime(&ftime);
	if (created > now) return 0;

	days = (now - created) / 86400;

	if (u != NULL)
	{
		*u = -1;
		p = strchr(name+10, 'U');
		if (p != NULL) *u = atoi(p+1);
	}

	if (g != NULL)
	{
		*g = -1;
		p = strchr(name+10, 'G');
		if (p != NULL) *g = atoi(p+1);
	}

	return days;
}

void
aux_url_callback(const char *url)
{
	if (url == NULL) return;
	if (!strncmp(url, AUX_URL_MINE, AUX_URL_MINE_LEN)) filesystem_unlink(url + AUX_URL_PATH_OFFSET);
}

uint32_t
ymd_file_filter(const char *name, const char *path, uint32_t keep_mask, mode_t ymd_mode, uid_t ymd_uid, gid_t ymd_gid)
{
	asl_file_t *f = NULL;
	uint8_t km = keep_mask;
	uint32_t status, len, dstcount = 0;
	char src[MAXPATHLEN];
	char dst[MAXPATHLEN];

	if (snprintf(src, MAXPATHLEN, "%s/%s", path, name) >= MAXPATHLEN) return ASL_STATUS_FAILED;
	if (snprintf(dst, MAXPATHLEN, "%s/%s", path, name) >= MAXPATHLEN) return ASL_STATUS_FAILED;
	len = strlen(src) - 3;
	snprintf(dst + len, 4, "tmp");

	//TODO: check if src file is already filtered
	debug_log(ASL_LEVEL_NOTICE, "  filter %s %s ---> %s\n", src, keep_str(km), dst);

	status = ASL_STATUS_OK;

	if (dryrun == 0)
	{
		status = asl_file_open_read(name, &f);
		if (status != ASL_STATUS_OK) return status;

		status = asl_file_filter_level(f, dst, keep_mask, ymd_mode, ymd_uid, ymd_gid, &dstcount, aux_url_callback);
		asl_file_close(f);
	}

	filesystem_unlink(src);
	if ((status != ASL_STATUS_OK) || (dstcount == 0)) filesystem_unlink(dst);
	else filesystem_rename(dst, src);

	return status;
}

/*
 * Used to set config parameters.
 * Line format "= name value"
 */
static void
_aslmanager_set_param(asl_out_dst_data_t *dst, char *s)
{
	char **l;
	uint32_t count;

	if (s == NULL) return;
	if (s[0] == '\0') return;

	/* skip '=' and whitespace */
	if (*s == '=') s++;
	while ((*s == ' ') || (*s == '\t')) s++;

	l = explode(s, " \t");
	if (l == NULL) return;

	for (count = 0; l[count] != NULL; count++);

	/* name is required */
	if (count == 0)
	{
		free_string_list(l);
		return;
	}

	/* value is required */
	if (count == 1)
	{
		free_string_list(l);
		return;
	}

	if (!strcasecmp(l[0], "aslmanager_debug"))
	{
		/* = debug level */
		set_debug(DEBUG_ASL, l[1]);
	}
	else if (!strcasecmp(l[0], "store_ttl"))
	{
		/* = store_ttl days */
		dst->ttl[LEVEL_ALL] = (time_t)atoll(l[1]);
	}
	else if (!strcasecmp(l[0], "module_ttl"))
	{
		/* = module_ttl days */
		module_ttl = (time_t)atoll(l[1]);
	}
	else if (!strcasecmp(l[0], "max_store_size"))
	{
		/* = max_file_size bytes */
		dst->all_max = atoi(l[1]);
	}
	else if (!strcasecmp(l[0], "archive"))
	{
		free(dst->rotate_dir);
		dst->rotate_dir = NULL;

		/* = archive {0|1} path */
		if (!strcmp(l[1], "1"))
		{
			if (l[2] == NULL) dst->rotate_dir = strdup(PATH_ASL_ARCHIVE);
			else dst->rotate_dir = strdup(l[2]);
		}
	}
	else if (!strcasecmp(l[0], "store_path"))
	{
		/* = archive path */
		free(dst->path);
		dst->path = strdup(l[1]);
	}
	else if (!strcasecmp(l[0], "archive_mode"))
	{
		dst->mode = strtol(l[1], NULL, 0);
		if ((dst->mode == 0) && (errno == EINVAL)) dst->mode = 0400;
	}

	free_string_list(l);
}

size_t
directory_size(const char *path)
{
	DIR *dp;
	struct dirent *dent;
	struct stat sb;
	size_t size;
	char *str;

	dp = opendir(path);
	if (dp == NULL) return 0;

	size = 0;
	while ((dent = readdir(dp)) != NULL)
	{
		if ((!strcmp(dent->d_name, ".")) || (!strcmp(dent->d_name, ".."))) continue;

		memset(&sb, 0, sizeof(struct stat));
		str = NULL;
		asprintf(&str, "%s/%s", path, dent->d_name);

		if ((str != NULL) && (stat(str, &sb) == 0) && S_ISREG(sb.st_mode))
		{
			size += sb.st_size;
			free(str);
		}
	}

	closedir(dp);
	return size;
}

static int
process_asl_data_store(asl_out_dst_data_t *dst)
{
	int32_t today_ymd_stringlen, expire_ymd_stringlen;
	time_t now, ttl, ymd_expire;
	struct tm ctm;
	char today_ymd_string[32], expire_ymd_string[32], *str;
	DIR *dp;
	struct dirent *dent;
	name_list_t *ymd_list, *bb_list, *aux_list, *bb_aux_list, *e;
	size_t file_size, store_size;
	struct stat sb;

	ymd_list = NULL;
	bb_list = NULL;
	aux_list = NULL;
	bb_aux_list = NULL;
	store_size = 0;

	if (dst == NULL) return 0;
	if (dst->path == NULL) return 0;

	debug_log(ASL_LEVEL_NOTICE, "----------------------------------------\n");
	debug_log(ASL_LEVEL_NOTICE, "Processing data store %s\n", dst->path);

	if (dst->rotate_dir != NULL)
	{
		/* check archive */
		memset(&sb, 0, sizeof(struct stat));
		if (stat(dst->rotate_dir, &sb) == 0)
		{
			/* must be a directory */
			if (!S_ISDIR(sb.st_mode))
			{
				debug_log(ASL_LEVEL_ERR, "aslmanager error: archive %s is not a directory", dst->rotate_dir);
				return -1;
			}
		}
		else
		{
			if (errno == ENOENT)
			{
				/* archive doesn't exist - create it */
				if (mkdir(dst->rotate_dir, 0755) != 0)
				{
					debug_log(ASL_LEVEL_ERR, "aslmanager error: can't create archive %s: %s\n", dst->rotate_dir, strerror(errno));
					return -1;
				}
			}
			else
			{
				/* stat failed for some other reason */
				debug_log(ASL_LEVEL_ERR, "aslmanager error: can't stat archive %s: %s\n", dst->rotate_dir, strerror(errno));
				return -1;
			}
		}
	}

	chdir(dst->path);

	/* determine current time */
	now = time(NULL);

	/* ttl 0 means files never expire */
	ymd_expire = 0;
	ttl = dst->ttl[LEVEL_ALL] * SECONDS_PER_DAY;

	if ((ttl > 0) && (ttl <= now)) ymd_expire = now - ttl;

	/* construct today's date as YYYY.MM.DD */
	memset(&ctm, 0, sizeof(struct tm));
	if (localtime_r((const time_t *)&now, &ctm) == NULL) return -1;

	snprintf(today_ymd_string, sizeof(today_ymd_string), "%d.%02d.%02d.", ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
	today_ymd_stringlen = strlen(today_ymd_string);

	/* construct regular file expiry date as YYYY.MM.DD */
	memset(&ctm, 0, sizeof(struct tm));
	if (localtime_r((const time_t *)&ymd_expire, &ctm) == NULL) return -1;

	snprintf(expire_ymd_string, sizeof(expire_ymd_string), "%d.%02d.%02d.", ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
	expire_ymd_stringlen = strlen(expire_ymd_string);

	debug_log(ASL_LEVEL_NOTICE, "Expiry Date %s\n", expire_ymd_string);

	dp = opendir(dst->path);
	if (dp == NULL) return -1;

	/* gather a list of YMD files, AUX dirs, BB.AUX dirs, and BB files */
	while ((dent = readdir(dp)) != NULL)
	{
		memset(&sb, 0, sizeof(struct stat));
		file_size = 0;
		if (stat(dent->d_name, &sb) == 0) file_size = sb.st_size;

		if ((dent->d_name[0] >= '0') && (dent->d_name[0] <= '9'))
		{
			ymd_list = add_to_name_list(ymd_list, dent->d_name, file_size);
			store_size += file_size;
		}
		else if (!strncmp(dent->d_name, "AUX.", 4) && (dent->d_name[4] >= '0') && (dent->d_name[4] <= '9') && S_ISDIR(sb.st_mode))
		{
			file_size = directory_size(dent->d_name);
			aux_list = add_to_name_list(aux_list, dent->d_name, file_size);
			store_size += file_size;
		}
		else if (!strncmp(dent->d_name, "BB.AUX.", 7) && (dent->d_name[7] >= '0') && (dent->d_name[7] <= '9') && S_ISDIR(sb.st_mode))
		{
			file_size = directory_size(dent->d_name);
			bb_aux_list = add_to_name_list(bb_aux_list, dent->d_name, file_size);
			store_size += file_size;
		}
		else if (!strncmp(dent->d_name, "BB.", 3) && (dent->d_name[3] >= '0') && (dent->d_name[3] <= '9'))
		{
			bb_list = add_to_name_list(bb_list, dent->d_name, file_size);
			store_size += file_size;
		}
		else if ((!strcmp(dent->d_name, ".")) || (!strcmp(dent->d_name, "..")))
		{}
		else if ((!strcmp(dent->d_name, "StoreData")) || (!strcmp(dent->d_name, "SweepStore")))
		{}
		else
		{
			debug_log(ASL_LEVEL_ERR, "aslmanager: unexpected file %s in ASL data store\n", dent->d_name);
		}
	}

	closedir(dp);

	debug_log(ASL_LEVEL_NOTICE, "Data Store Size = %lu\n", store_size);
	debug_log(ASL_LEVEL_NOTICE, "Data Store YMD Files\n");
	for (e = ymd_list; e != NULL; e = e->next) debug_log(ASL_LEVEL_NOTICE, "	%s   %lu\n", e->name, e->size);
	debug_log(ASL_LEVEL_NOTICE, "Data Store AUX Directories\n");
	for (e = aux_list; e != NULL; e = e->next) debug_log(ASL_LEVEL_NOTICE, "	%s   %lu\n", e->name, e->size);
	debug_log(ASL_LEVEL_NOTICE, "Data Store BB.AUX Directories\n");
	for (e = bb_aux_list; e != NULL; e = e->next) debug_log(ASL_LEVEL_NOTICE, "	%s   %lu\n", e->name, e->size);
	debug_log(ASL_LEVEL_NOTICE, "Data Store BB Files\n");
	for (e = bb_list; e != NULL; e = e->next) debug_log(ASL_LEVEL_NOTICE, "	%s   %lu\n", e->name, e->size);

	/* Delete/achive expired YMD files */
	debug_log(ASL_LEVEL_NOTICE, "Start YMD File Scan\n");

	e = ymd_list;
	while (e != NULL)
	{
		if (strncmp(e->name, expire_ymd_string, expire_ymd_stringlen) <= 0)
		{
			/* file has expired, archive it if required, then unlink it */
			if (dst->rotate_dir != NULL)
			{
				str = NULL;
				asprintf(&str, "%s/%s", dst->rotate_dir, e->name);
				if (str == NULL) return -1;

				filesystem_copy(dst, e->name, str, 0);
				free(str);
			}

			filesystem_unlink(e->name);
			store_size -= e->size;
			e->size = 0;
		}
		else
		{
			/* check if there are any per-level TTLs and filter the file if required */
			uint32_t i, bit, keep_mask;
			uid_t ymd_uid = -1;
			gid_t ymd_gid = -1;
			mode_t ymd_mode = 0600;
			uint32_t age = ymd_file_age(e->name, now, &ymd_uid, &ymd_gid);

			if (age > 0)
			{
				keep_mask = 0x000000ff;
				bit = 1;
				for (i = 0; i <= 7; i++)
				{
					if ((dst->ttl[i] > 0) && (age >= dst->ttl[i])) keep_mask &= ~bit;
					bit *= 2;
				}

				memset(&sb, 0, sizeof(struct stat));
				if (stat(e->name, &sb) == 0) ymd_mode = sb.st_mode & 0777;

				if (keep_mask != 0x000000ff) ymd_file_filter(e->name, dst->path, keep_mask, ymd_mode, ymd_uid, ymd_gid);
			}
		}

		e = e->next;
	}

	debug_log(ASL_LEVEL_NOTICE, "Finished YMD File Scan\n");

	/* Delete/achive expired YMD AUX directories */
	debug_log(ASL_LEVEL_NOTICE, "Start AUX Directory Scan\n");

	e = aux_list;
	while (e != NULL)
	{
		/* stop when a file name/date is after the expire date */
		if (strncmp(e->name + 4, expire_ymd_string, expire_ymd_stringlen) > 0) break;

		if (dst->rotate_dir != NULL)
		{
			str = NULL;
			asprintf(&str, "%s/%s", dst->rotate_dir, e->name);
			if (str == NULL) return -1;

			filesystem_copy(dst, e->name, str, 0);
			free(str);
		}

		remove_directory(e->name);
		store_size -= e->size;
		e->size = 0;

		e = e->next;
	}

	debug_log(ASL_LEVEL_NOTICE, "Finished AUX Directory Scan\n");

	/* Delete/achive expired BB.AUX directories */
	debug_log(ASL_LEVEL_NOTICE, "Start BB.AUX Directory Scan\n");

	e = bb_aux_list;
	while (e != NULL)
	{
		/* stop when a file name/date is after the expire date */
		if (strncmp(e->name + 7, today_ymd_string, today_ymd_stringlen) > 0) break;

		if (dst->rotate_dir != NULL)
		{
			str = NULL;
			asprintf(&str, "%s/%s", dst->rotate_dir, e->name);
			if (str == NULL) return -1;

			filesystem_copy(dst, e->name, str, 0);
			free(str);
		}

		remove_directory(e->name);
		store_size -= e->size;
		e->size = 0;

		e = e->next;
	}

	debug_log(ASL_LEVEL_NOTICE, "Finished BB.AUX Directory Scan\n");

	/* Delete/achive expired BB files */
	debug_log(ASL_LEVEL_NOTICE, "Start BB Scan\n");

	e = bb_list;
	while (e != NULL)
	{
		/* stop when a file name/date is after the expire date */
		if (strncmp(e->name + 3, today_ymd_string, today_ymd_stringlen) > 0) break;

		if (dst->rotate_dir != NULL)
		{
			str = NULL;
			asprintf(&str, "%s/%s", dst->rotate_dir, e->name);
			if (str == NULL) return -1;

			/* syslog -x [str] -f [e->name] */
			filesystem_copy(dst, e->name, str, 0);
			free(str);
		}

		filesystem_unlink(e->name);
		store_size -= e->size;
		e->size = 0;

		e = e->next;
	}

	debug_log(ASL_LEVEL_NOTICE, "Finished BB Scan\n");

	if (dst->all_max > 0)
	{
		/* if data store is over max_size, delete/archive more YMD files */
		if (store_size > dst->all_max) debug_log(ASL_LEVEL_NOTICE, "Additional YMD Scan\n");

		e = ymd_list;
		while ((e != NULL) && (store_size > dst->all_max))
		{
			if (e->size != 0)
			{
				if (strncmp(e->name, today_ymd_string, today_ymd_stringlen) == 0)
				{
					/* do not touch active file YYYY.MM.DD.asl */
					if (strcmp(e->name + today_ymd_stringlen, "asl") == 0)
					{
						e = e->next;
						continue;
					}
				}

				if (dst->rotate_dir != NULL)
				{
					str = NULL;
					asprintf(&str, "%s/%s", dst->rotate_dir, e->name);
					if (str == NULL) return -1;

					/* syslog -x [str] -f [e->name] */
					filesystem_copy(dst, e->name, str, 0);
					free(str);
				}

				filesystem_unlink(e->name);
				store_size -= e->size;
				e->size = 0;
			}

			e = e->next;
		}

		/* if data store is over dst->all_max, delete/archive more BB files */
		if (store_size > dst->all_max) debug_log(ASL_LEVEL_NOTICE, "Additional BB Scan\n");

		e = bb_list;
		while ((e != NULL) && (store_size > dst->all_max))
		{
			if (e->size != 0)
			{
				if (dst->rotate_dir != NULL)
				{
					str = NULL;
					asprintf(&str, "%s/%s", dst->rotate_dir, e->name);
					if (str == NULL) return -1;

					/* syslog -x [str] -f [e->name] */
					filesystem_copy(dst, e->name, str, 0);
					free(str);
				}

				filesystem_unlink(e->name);
				store_size -= e->size;
				e->size = 0;
			}

			e = e->next;
		}
	}

	free_name_list(ymd_list);	 
	free_name_list(bb_list);
	free_name_list(aux_list);
	free_name_list(bb_aux_list);

	debug_log(ASL_LEVEL_NOTICE, "Data Store Size = %lu\n", store_size);

	return 0;
}

/* move sequenced source files to dst dir, renaming as we go */
static int
module_copy_rename(asl_out_dst_data_t *dst)
{
	asl_out_file_list_t *src_list, *dst_list, *f, *dst_last;
	char *base, *dst_dir;
	char fpathsrc[MAXPATHLEN], fpathdst[MAXPATHLEN];
	uint32_t src_count, dst_count;
	int32_t x, moved;

	if (dst == NULL) return -1;
	if (dst->path == NULL) return -1;

	base = strrchr(dst->path, '/');
	if (base == NULL) return -1;

	src_list = asl_list_src_files(dst);
	if (src_list == 0)
	{
		debug_log(ASL_LEVEL_INFO, "    no src files\n");
		return 0;
	}

	debug_log(ASL_LEVEL_INFO, "    src files\n");

	src_count = 0;
	for (f = src_list; f != NULL; f = f->next)
	{
		debug_log(ASL_LEVEL_INFO, "      %s\n", f->name);
		src_count++;
	}

	dst_list = asl_list_dst_files(dst);

	*base = '\0';
	base++;

	dst_dir = dst->rotate_dir;
	if (dst_dir == NULL) dst_dir = dst->path;

	dst_count = 0;
	dst_last = dst_list;

	if (dst_list == NULL) debug_log(ASL_LEVEL_INFO, "    no dst files\n");
	else debug_log(ASL_LEVEL_INFO, "    dst files\n");

	for (f = dst_list; f != NULL; f = f->next)
	{
		debug_log(ASL_LEVEL_INFO, "      %s\n", f->name);
		dst_last = f;
		dst_count++;
	}

	if (dst->flags & MODULE_FLAG_STYLE_SEQ)
	{
		for (f = dst_last; f != NULL; f = f->prev)
		{
			int is_gz = 0;
			char *dot = strrchr(f->name, '.');
			if ((dot != NULL) && (!strcmp(dot, ".gz"))) is_gz = 1;

			snprintf(fpathsrc, sizeof(fpathsrc), "%s/%s", dst_dir, f->name);
			snprintf(fpathdst, sizeof(fpathdst), "%s/%s.%d%s", dst_dir, base, f->seq+src_count, (is_gz == 1) ? ".gz" : "");
			filesystem_rename(fpathsrc, fpathdst);
		}

		for (f = src_list, x = 0; f != NULL; f = f->next, x++)
		{
			snprintf(fpathsrc, sizeof(fpathsrc), "%s/%s", dst->path, f->name);
			snprintf(fpathdst, sizeof(fpathdst), "%s/%s.%d", dst_dir, base, x);
			moved = filesystem_copy(dst, fpathsrc, fpathdst, dst->flags);
			if (moved != 0)
			{
				if (dst->flags & MODULE_FLAG_TRUNCATE) filesystem_truncate(fpathsrc);
				else filesystem_unlink(fpathsrc);
			}
		}
	}
	else
	{
		for (f = src_list; f != NULL; f = f->next)
		{
			/* final / active base stamped file looks like a checkpointed file - ignore it */
			if ((dst->flags & MODULE_FLAG_BASESTAMP) && (f->next == NULL)) break;

			snprintf(fpathsrc, sizeof(fpathsrc), "%s/%s", dst->path, f->name);

			/* MODULE_FLAG_EXTERNAL files are not decorated with a timestamp */
			if (dst->flags & MODULE_FLAG_EXTERNAL)
			{
				char tstamp[32];

				asl_make_timestamp(f->ftime, dst->flags, tstamp, sizeof(tstamp));
				snprintf(fpathdst, sizeof(fpathdst), "%s/%s.%s", dst_dir, base, tstamp);
			}
			else
			{
				snprintf(fpathdst, sizeof(fpathdst), "%s/%s", dst_dir, f->name);
			}

			moved = filesystem_copy(dst, fpathsrc, fpathdst, dst->flags);
			if (moved != 0)
			{
				if (dst->flags & MODULE_FLAG_TRUNCATE) filesystem_truncate(fpathsrc);
				else filesystem_unlink(fpathsrc);
			}
		}
	}

	asl_out_file_list_free(src_list);
	asl_out_file_list_free(dst_list);

	if (base != NULL) *--base = '/';

	return 0;
}

/* delete expired files */
static int
module_expire(asl_out_dst_data_t *dst)
{
	asl_out_file_list_t *dst_list, *f;
	char *base, *dst_dir, fpath[MAXPATHLEN];
	time_t now, ttl, cutoff;

	if (dst == NULL) return -1;
	if (dst->path == NULL) return -1;
	if (dst->ttl[LEVEL_ALL] == 0) return 0;

	ttl = 0;
	if (module_ttl > 0) ttl = module_ttl;
	else ttl = dst->ttl[LEVEL_ALL];

	ttl *= SECONDS_PER_DAY;

	now = time(NULL);
	if (ttl > now) return 0;

	cutoff = now - ttl;

	base = strrchr(dst->path, '/');
	if (base == NULL) return -1;

	dst_list = asl_list_dst_files(dst);

	*base = '\0';

	dst_dir = dst->rotate_dir;
	if (dst_dir == NULL) dst_dir = dst->path;

	if (dst_list == NULL)
	{
		debug_log(ASL_LEVEL_INFO, "    no dst files\n");
	}
	else
	{
		debug_log(ASL_LEVEL_INFO, "    dst files\n");
		for (f = dst_list; f != NULL; f = f->next) debug_log(ASL_LEVEL_INFO, "      %s\n", f->name);
	}

	for (f = dst_list; f != NULL; f = f->next)
	{
		if (f->ftime <= cutoff)
		{
			snprintf(fpath, sizeof(fpath), "%s/%s", dst_dir, f->name);
			filesystem_unlink(fpath);
		}
	}

	asl_out_file_list_free(dst_list);

	if (base != NULL) *base = '/';

	return 0;
}

/* check all_max size and delete files (oldest first) to stay within size limit */
static int
module_check_size(asl_out_dst_data_t *dst)
{
	asl_out_file_list_t *dst_list, *f, *dst_end;
	char *base, *dst_dir, fpath[MAXPATHLEN];
	size_t total;

	if (dst == NULL) return -1;
	if (dst->path == NULL) return -1;

	if (dst->all_max == 0) return 0;

	dst_list = asl_list_dst_files(dst);
	if (dst_list == NULL)
	{
		debug_log(ASL_LEVEL_INFO, "    no dst files\n");
		return 0;
	}

	base = NULL;
	dst_dir = dst->rotate_dir;
	if (dst_dir == NULL)
	{
		dst_dir = dst->path;
		base = strrchr(dst->path, '/');
		if (base == NULL)
		{
			asl_out_file_list_free(dst_list);
			return -1;
		}

		*base = '\0';
	}

	debug_log(ASL_LEVEL_INFO, "    dst files\n");
	dst_end = dst_list;
	for (f = dst_list; f != NULL; f = f->next)
	{
		dst_end = f;
		debug_log(ASL_LEVEL_INFO, "      %s size %lu\n", f->name, f->size);
	}

	total = 0;
	for (f = dst_list; f != NULL; f = f->next) total += f->size;
	
	for (f = dst_end; (total > dst->all_max) && (f != NULL); f = f->prev)
	{
		snprintf(fpath, sizeof(fpath), "%s/%s", dst_dir, f->name);
		filesystem_unlink(fpath);
		total -= f->size;
	}

	asl_out_file_list_free(dst_list);

	if (base != NULL) *base = '/';

	return 0;
}


static int
process_module(asl_out_module_t *mod)
{
	asl_out_rule_t *r;

	if (mod == NULL) return -1;

	debug_log(ASL_LEVEL_NOTICE, "----------------------------------------\n");
	debug_log(ASL_LEVEL_NOTICE, "Processing module %s\n", (mod->name == NULL) ? "asl.conf" : mod->name);

	for (r = mod->ruleset; r != NULL; r = r->next)
	{
		if (r->action == ACTION_OUT_DEST)
		{
			if (r->dst == NULL)
			{
				debug_log(ASL_LEVEL_NOTICE, "NULL dst data for output rule - skipped\n");
			}
			else if (r->dst->flags & MODULE_FLAG_ROTATE)
			{
				debug_log(ASL_LEVEL_NOTICE, "Checking file %s\n", r->dst->path);
				debug_log(ASL_LEVEL_NOTICE, "- Rename, move to destination directory, and compress as required\n");

				module_copy_rename(r->dst);

				if (r->dst->ttl[LEVEL_ALL] > 0)
				{
					debug_log(ASL_LEVEL_NOTICE, "- Check for expired files - TTL = %d days\n", r->dst->ttl[LEVEL_ALL]);
					module_expire(r->dst);
				}

				if (r->dst->all_max > 0)
				{
					debug_log(ASL_LEVEL_NOTICE, "- Check total storage used - MAX = %lu\n", r->dst->all_max);
					module_check_size(r->dst);
				}
			}
			else if ((r->dst->flags & MODULE_FLAG_TYPE_ASL_DIR) && (r->dst->ttl[LEVEL_ALL] > 0))
			{
				process_asl_data_store(r->dst);
			}
		}
	}

	debug_log(ASL_LEVEL_NOTICE, "Finished processing module %s\n", (mod->name == NULL) ? "asl.conf" : mod->name);
	return 0;
}

asl_msg_list_t *
control_query(asl_msg_t *a)
{
	asl_msg_list_t *out;
	char *qstr, *str, *res;
	uint32_t len, reslen, status;
	uint64_t cmax, qmin;
	kern_return_t kstatus;
	caddr_t vmstr;
	security_token_t sec;

	if (asl_server_port == MACH_PORT_NULL)
	{
		bootstrap_look_up2(bootstrap_port, ASL_SERVICE_NAME, &asl_server_port, 0, BOOTSTRAP_PRIVILEGED_SERVER);
		if (asl_server_port == MACH_PORT_NULL) return NULL;
	}

	qstr = asl_msg_to_string((asl_msg_t *)a, &len);

	str = NULL;
	if (qstr == NULL)
	{
		asprintf(&str, "1\nQ [= ASLOption control]\n");
	}
	else
	{
		asprintf(&str, "1\n%s [= ASLOption control]\n", qstr);
		free(qstr);
	}

	if (str == NULL) return NULL;

	/* length includes trailing nul */
	len = strlen(str) + 1;
	out = NULL;
	qmin = 0;
	cmax = 0;
	sec.val[0] = -1;
	sec.val[1] = -1;

	res = NULL;
	reslen = 0;
	status = ASL_STATUS_OK;

	kstatus = vm_allocate(mach_task_self(), (vm_address_t *)&vmstr, len, TRUE);
	if (kstatus != KERN_SUCCESS) return NULL;

	memmove(vmstr, str, len);
	free(str);

	status = 0;
	kstatus = _asl_server_query(asl_server_port, vmstr, len, qmin, 1, 0, (caddr_t *)&res, &reslen, &cmax, (int *)&status, &sec);
	if (kstatus != KERN_SUCCESS) return NULL;

	if (res == NULL) return NULL;

	out = asl_msg_list_from_string(res);
	vm_deallocate(mach_task_self(), (vm_address_t)res, reslen);

	return out;
}

int
checkpoint(const char *name)
{
	/* send checkpoint message to syslogd */
	debug_log(ASL_LEVEL_NOTICE, "Checkpoint module %s\n", (name == NULL) ? "*" : name);
	if (dryrun != 0) return 0;

	asl_msg_t *qmsg = asl_msg_new(ASL_TYPE_QUERY);
	char *tmp = NULL;
	asl_msg_list_t *res;

	asprintf(&tmp, "%s checkpoint", (name == NULL) ? "*" : name);
	asl_msg_set_key_val_op(qmsg, "action", tmp, ASL_QUERY_OP_EQUAL);
	free(tmp);

	res = control_query(qmsg);

	asl_msg_list_release(res);
	return 0;
}

int
cli_main(int argc, char *argv[])
{
	int i, work;
	asl_out_module_t *mod, *m;
	asl_out_rule_t *r;
	asl_out_dst_data_t store, *asl_store_dst = NULL;
	const char *mname = NULL;

	if (geteuid() != 0)
	{
		if (argc == 0) debug = DEBUG_ASL;
		else debug = DEBUG_STDERR;

		debug_log(ASL_LEVEL_ERR, "aslmanager must be run by root\n");
		exit(1);
	}

	module_ttl = DEFAULT_TTL;

	/* cobble up a dst_data with defaults and parameter settings */
	memset(&store, 0, sizeof(store));
	store.ttl[LEVEL_ALL] = DEFAULT_TTL;
	store.all_max = DEFAULT_MAX_SIZE;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-s"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-'))
			{
				store.path = strdup(argv[++i]);
				asl_store_dst = &store;
			}
		}
	}

	/* get parameters from asl.conf */
	mod = asl_out_module_init();

	if (mod != NULL)
	{
		for (r = mod->ruleset; r != NULL; r = r->next)
		{
			if ((asl_store_dst == NULL) && (r->action == ACTION_OUT_DEST) && (!strcmp(r->dst->path, PATH_ASL_STORE)))
				asl_store_dst = r->dst;
		}

		for (r = mod->ruleset; r != NULL; r = r->next)
		{
			if (r->action == ACTION_SET_PARAM)
			{
				if (r->query == NULL) _aslmanager_set_param(asl_store_dst, r->options);
			}
		}
	}

	work = DO_ASLDB | DO_MODULE;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-a"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) asl_store_dst->rotate_dir = strdup(argv[++i]);
			else asl_store_dst->rotate_dir = strdup(PATH_ASL_ARCHIVE);
			asl_store_dst->mode = 0400;
		}
		else if (!strcmp(argv[i], "-store_ttl"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) asl_store_dst->ttl[LEVEL_ALL] = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-module_ttl"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) module_ttl = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-ttl"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) module_ttl = asl_store_dst->ttl[LEVEL_ALL] = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-size"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) asl_store_dst->all_max = asl_str_to_size(argv[++i]);
		}
		else if (!strcmp(argv[i], "-checkpoint"))
		{
			work |= DO_CHECKPT;
		}
		else if (!strcmp(argv[i], "-module"))
		{
			work &= ~DO_ASLDB;

			/* optional name follows -module */
			if ((i +1) < argc)
			{
				if (argv[i + 1][0] != '-') mname = argv[++i];
			}
		}
		else if (!strcmp(argv[i], "-asldb"))
		{
			work = DO_ASLDB;
		}
		else if (!strcmp(argv[i], "-d"))
		{
			if (((i + i) < argc) && (argv[i+1][0] != '-')) set_debug(DEBUG_STDERR, argv[++i]);
			else set_debug(DEBUG_STDERR, NULL);
		}
		else if (!strcmp(argv[i], "-dd"))
		{
			dryrun = 1;

			if (((i + i) < argc) && (argv[i+1][0] != '-')) set_debug(DEBUG_STDERR, argv[++i]);
			else set_debug(DEBUG_STDERR, NULL);
		}
	}

	if (asl_store_dst->path == NULL) asl_store_dst->path = strdup(PATH_ASL_STORE);

	debug_log(ASL_LEVEL_ERR, "aslmanager starting%s\n", (dryrun == 1) ? " dryrun" : "");

	if (work & DO_ASLDB) process_asl_data_store(asl_store_dst);

	if (work & DO_MODULE)
	{
		if (work & DO_CHECKPT) checkpoint(mname);

		if (mod != NULL)
		{
			for (m = mod; m != NULL; m = m->next)
			{
				if ((mname == NULL) || ((m->name != NULL) && (!strcmp(m->name, mname))))
				{
					process_module(m);
				}
			}
		}
	}

	asl_out_module_free(mod);

	debug_log(ASL_LEVEL_NOTICE, "----------------------------------------\n");
	debug_log(ASL_LEVEL_ERR, "aslmanager finished%s\n", (dryrun == 1) ? " dryrun" : "");
	if (asl_aux_fd >= 0) asl_close_auxiliary_file(asl_aux_fd);

	return 0;
}

static void
accept_connection(xpc_connection_t peer)
{
	xpc_connection_set_event_handler(peer, ^(xpc_object_t request) {
		if (xpc_get_type(request) == XPC_TYPE_DICTIONARY)
		{
			uid_t uid = xpc_connection_get_euid(peer);

			/* send a reply immediately */
			xpc_object_t reply = xpc_dictionary_create_reply(request);
			xpc_connection_send_message(peer, reply);
			xpc_release(reply);

			/*
			 * Some day, we may use the dictionary to pass parameters
			 * to aslmanager, but for now, we ignore the input.
			 */
			if (uid == 0) cli_main(0, NULL);
		}
		else if (xpc_get_type(request) == XPC_TYPE_ERROR)
		{
			/* disconnect */
		}

		dispatch_async(serverq, ^__attribute__((noreturn)) { xpc_server_exit(0); });
	});

	xpc_connection_resume(peer);
}

int
main(int argc, char *argv[])
{
	int64_t is_managed = 0;

	vproc_swap_integer(NULL, VPROC_GSK_IS_MANAGED, NULL, &is_managed);

	if (is_managed == 0) return cli_main(argc, argv);

	/* XPC server */
	serverq = dispatch_queue_create("aslmanager", NULL);
	xpc_track_activity();

	/* Handle incoming messages. */
	listener = xpc_connection_create_mach_service("com.apple.aslmanager", serverq, XPC_CONNECTION_MACH_SERVICE_LISTENER);
	xpc_connection_set_event_handler(listener, ^(xpc_object_t peer) {
		if (xpc_get_type(peer) == XPC_TYPE_CONNECTION) accept_connection(peer);
	});
	xpc_connection_resume(listener);

	dispatch_main();
}
