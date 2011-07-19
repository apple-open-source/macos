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
#include <copyfile.h>
#include <asl.h>
#include <asl_private.h>
#include <asl_core.h>
#include <asl_file.h>
#include <asl_store.h>

#define SECONDS_PER_DAY 86400
#define DEFAULT_MAX_SIZE 150000000
#define DEFAULT_TTL 7

#define _PATH_ASL_CONF "/etc/asl.conf"

/* global */
static char *archive = NULL;
static char *store_dir = PATH_ASL_STORE;
static time_t ttl;
static size_t max_size;
static mode_t archive_mode = 0400;
static int debug;

typedef struct name_list_s
{
	char *name;
	size_t size;
	struct name_list_s *next;
} name_list_t;

name_list_t *
add_to_list(name_list_t *l, const char *name, size_t size)
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

uint32_t
do_copy(const char *infile, const char *outfile, mode_t mode)
{
	asl_search_result_t *res;
	asl_file_t *f;
	uint32_t status, i;
	uint64_t mid;

	if (infile == NULL) return ASL_STATUS_INVALID_ARG;
	if (outfile == NULL) return ASL_STATUS_INVALID_ARG;

	f = NULL;
	status = asl_file_open_read(infile, &f);
	if (status != ASL_STATUS_OK) return status;

	res = NULL;
	mid = 0;

	status = asl_file_match(f, NULL, &res, &mid, 0, 0, 1);
	asl_file_close(f);

	if (status != ASL_STATUS_OK) return status;
	if (res->count == 0)
	{
		aslresponse_free(res);
		return ASL_STATUS_OK;
	}

	f = NULL;
	status = asl_file_open_write(outfile, mode, -1, -1, &f);
	if (status != ASL_STATUS_OK) return status;
	if (f == ASL_STATUS_OK) return ASL_STATUS_FAILED;

	f->flags = ASL_FILE_FLAG_UNLIMITED_CACHE | ASL_FILE_FLAG_PRESERVE_MSG_ID;

	for (i = 0; i < res->count; i++)
	{
		mid = 0;
		status = asl_file_save(f, (aslmsg)(res->msg[i]), &mid);
		if (status != ASL_STATUS_OK) break;
	}

	asl_file_close(f);
	return status;
}

int
do_dir_archive(const char *indir, const char *outdir)
{
	return copyfile(indir, outdir, NULL, COPYFILE_ALL | COPYFILE_RECURSIVE);
}

int
remove_directory(const char *path)
{
	DIR *dp;
	struct dirent *dent;
	char *str;
	int status;

	dp = opendir(path);
	if (dp == NULL) return 0;

	while ((dent = readdir(dp)) != NULL)
	{
		if ((!strcmp(dent->d_name, ".")) || (!strcmp(dent->d_name, ".."))) continue;
		asprintf(&str, "%s/%s", path, dent->d_name);
		if (str != NULL)
		{
			status = unlink(str);
			free(str);
			str = NULL;
		}
	}

	closedir(dp);
	status = rmdir(path);

	return status;
}


static char **
_insertString(char *s, char **l, uint32_t x)
{
	int i, len;

	if (s == NULL) return l;
	if (l == NULL) 
	{
		l = (char **)malloc(2 * sizeof(char *));
		if (l == NULL) return NULL;

		l[0] = strdup(s);
		if (l[0] == NULL)
		{
			free(l);
			return NULL;
		}

		l[1] = NULL;
		return l;
	}

	for (i = 0; l[i] != NULL; i++);
	len = i + 1; /* count the NULL on the end of the list too! */

	l = (char **)reallocf(l, (len + 1) * sizeof(char *));
	if (l == NULL) return NULL;

	if ((x >= (len - 1)) || (x == IndexNull))
	{
		l[len - 1] = strdup(s);
		if (l[len - 1] == NULL)
		{
			free(l);
			return NULL;
		}

		l[len] = NULL;
		return l;
	}

	for (i = len; i > x; i--) l[i] = l[i - 1];
	l[x] = strdup(s);
	if (l[x] == NULL) return NULL;

	return l;
}

char **
explode(const char *s, const char *delim)
{
	char **l = NULL;
	const char *p;
	char *t, quote;
	int i, n;

	if (s == NULL) return NULL;

	quote = '\0';

	p = s;
	while (p[0] != '\0')
	{
		/* scan forward */
		for (i = 0; p[i] != '\0'; i++)
		{
			if (quote == '\0')
			{
				/* not inside a quoted string: check for delimiters and quotes */
				if (strchr(delim, p[i]) != NULL) break;
				else if (p[i] == '\'') quote = p[i];
				else if (p[i] == '"') quote = p[i];
			}
			else
			{
				/* inside a quoted string - look for matching quote */
				if (p[i] == quote) quote = '\0';
			}
		}

		n = i;
		t = malloc(n + 1);
		if (t == NULL) return NULL;

		for (i = 0; i < n; i++) t[i] = p[i];
		t[n] = '\0';
		l = _insertString(t, l, IndexNull);
		free(t);
		t = NULL;
		if (p[i] == '\0') return l;
		if (p[i + 1] == '\0') l = _insertString("", l, IndexNull);
		p = p + i + 1;
	}

	return l;
}

void
freeList(char **l)
{
	int i;

	if (l == NULL) return;
	for (i = 0; l[i] != NULL; i++) free(l[i]);
	free(l);
}

/*
 * Used to sed config parameters.
 * Line format "= name value"
 */
static void
_parse_set_param(char *s)
{
	char **l;
	uint32_t count;

	if (s == NULL) return;
	if (s[0] == '\0') return;

	/* skip '=' and whitespace */
	s++;
	while ((*s == ' ') || (*s == '\t')) s++;

	l = explode(s, " \t");
	if (l == NULL) return;

	for (count = 0; l[count] != NULL; count++);

	/* name is required */
	if (count == 0)
	{
		freeList(l);
		return;
	}

	/* value is required */
	if (count == 1)
	{
		freeList(l);
		return;
	}

	if (!strcasecmp(l[0], "aslmanager_debug"))
	{
		/* = debug {0|1} */
		debug = atoi(l[1]);
	}
	else if (!strcasecmp(l[0], "store_ttl"))
	{
		/* = store_ttl days */
		ttl = SECONDS_PER_DAY * (time_t)atoll(l[1]);
	}
	else if (!strcasecmp(l[0], "max_store_size"))
	{
		/* = max_file_size bytes */
		max_size = atoi(l[1]);
	}
	else if (!strcasecmp(l[0], "archive"))
	{
		/* = archive {0|1} path */
		if (!strcmp(l[1], "1"))
		{
			if (l[2] == NULL) archive = PATH_ASL_ARCHIVE;
			else archive = strdup(l[2]); /* never freed */
		}
		else archive = NULL;
	}
	else if (!strcasecmp(l[0], "store_path"))
	{
		/* = archive path */
		store_dir = strdup(l[1]); /* never freed */
	}
	else if (!strcasecmp(l[0], "archive_mode"))
	{
		archive_mode = strtol(l[1], NULL, 0);
		if ((archive_mode == 0) && (errno == EINVAL)) archive_mode = 0400;
	}

	freeList(l);
}

static void
_parse_line(char *s)
{
	if (s == NULL) return;
	while ((*s == ' ') || (*s == '\t')) s++;

	/*
	 * First non-whitespace char is the rule type.
	 * aslmanager only checks "=" (set parameter) rules.
	 */
	if (*s == '=') _parse_set_param(s);
}

char *
get_line_from_file(FILE *f)
{
	char *s, *out;
	size_t len;

	out = fgetln(f, &len);
	if (out == NULL) return NULL;
	if (len == 0) return NULL;

	s = malloc(len + 1);
	if (s == NULL) return NULL;

	memcpy(s, out, len);

	s[len - 1] = '\0';
	return s;
}

static int
_parse_config_file(const char *name)
{
	FILE *cf;
	char *line;

	cf = fopen(name, "r");
	if (cf == NULL) return 1;

	while (NULL != (line = get_line_from_file(cf)))
	{
		_parse_line(line);
		free(line);
	}

	fclose(cf);

	return 0;
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

int
main(int argc, const char *argv[])
{
	int i, today_ymd_stringlen, expire_ymd_stringlen;
	time_t now, ymd_expire;
	struct tm ctm;
	char today_ymd_string[32], expire_ymd_string[32], *str;
	DIR *dp;
	struct dirent *dent;
	name_list_t *ymd_list, *bb_list, *aux_list, *bb_aux_list, *e;
	uint32_t status;
	size_t file_size, store_size;
	struct stat sb;

	ymd_list = NULL;
	bb_list = NULL;
	aux_list = NULL;
	bb_aux_list = NULL;

	ttl = DEFAULT_TTL * SECONDS_PER_DAY;
	max_size = DEFAULT_MAX_SIZE;
	store_size = 0;
	debug = 0;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-a"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) archive = (char *)argv[++i];
			else archive = PATH_ASL_ARCHIVE;
		}
		else if (!strcmp(argv[i], "-s"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) store_dir = (char *)argv[++i];
		}
		else if (!strcmp(argv[i], "-ttl"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) ttl = atoi(argv[++i]) * SECONDS_PER_DAY;
		}
		else if (!strcmp(argv[i], "-size"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) max_size = atoi(argv[++i]);
		}
		else if (!strcmp(argv[i], "-d"))
		{
			debug = 1;
		}
	}

	_parse_config_file(_PATH_ASL_CONF);

	if (debug == 1) printf("aslmanager starting\n");

	/* check archive */
	if (archive != NULL)
	{
		memset(&sb, 0, sizeof(struct stat));
		if (stat(archive, &sb) == 0)
		{
			/* must be a directory */
			if (!S_ISDIR(sb.st_mode))
			{
				fprintf(stderr, "aslmanager error: archive %s is not a directory", archive);
				return -1;
			}
		}
		else
		{
			if (errno == ENOENT)
			{
				/* archive doesn't exist - create it */
				if (mkdir(archive, 0755) != 0)
				{
					fprintf(stderr, "aslmanager error: can't create archive %s: %s\n", archive, strerror(errno));
					return -1;
				}
			}
			else
			{
				/* stat failed for some other reason */
				fprintf(stderr, "aslmanager error: can't stat archive %s: %s\n", archive, strerror(errno));
				return -1;
			}
		}
	}

	chdir(store_dir);

	/* determine current time */
	now = time(NULL);

	/* ttl 0 means files never expire */
	ymd_expire = 0;
	if (ttl > 0) ymd_expire = now - ttl;

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

	if (debug == 1) printf("Expiry Date %s\n", expire_ymd_string);

	dp = opendir(store_dir);
	if (dp == NULL) return -1;

	/* gather a list of YMD files, AUX dirs, BB.AUX dirs, and BB files */
	while ((dent = readdir(dp)) != NULL)
	{
		memset(&sb, 0, sizeof(struct stat));
		file_size = 0;
		if (stat(dent->d_name, &sb) == 0) file_size = sb.st_size;

		if ((dent->d_name[0] >= '0') && (dent->d_name[0] <= '9'))
		{
			ymd_list = add_to_list(ymd_list, dent->d_name, file_size);
			store_size += file_size;
		}
		else if (!strncmp(dent->d_name, "AUX.", 4) && (dent->d_name[4] >= '0') && (dent->d_name[4] <= '9') && S_ISDIR(sb.st_mode))
		{
			file_size = directory_size(dent->d_name);
			aux_list = add_to_list(aux_list, dent->d_name, file_size);
			store_size += file_size;
		}
		else if (!strncmp(dent->d_name, "BB.AUX.", 7) && (dent->d_name[7] >= '0') && (dent->d_name[7] <= '9') && S_ISDIR(sb.st_mode))
		{
			file_size = directory_size(dent->d_name);
			bb_aux_list = add_to_list(bb_aux_list, dent->d_name, file_size);
			store_size += file_size;
		}
		else if (!strncmp(dent->d_name, "BB.", 3) && (dent->d_name[3] >= '0') && (dent->d_name[3] <= '9'))
		{
			bb_list = add_to_list(bb_list, dent->d_name, file_size);
			store_size += file_size;
		}
		else if ((!strcmp(dent->d_name, ".")) || (!strcmp(dent->d_name, "..")))
		{}
		else if ((!strcmp(dent->d_name, "StoreData")) || (!strcmp(dent->d_name, "SweepStore")))
		{}
		else
		{
			fprintf(stderr, "aslmanager: unexpected file %s in ASL data store\n", dent->d_name);
		}
	}

	closedir(dp);

	if (debug == 1)
	{
		printf("Data Store Size = %lu\n", store_size);
		printf("Data Store YMD Files\n");
		for (e = ymd_list; e != NULL; e = e->next) printf("	%s   %lu\n", e->name, e->size);
		printf("Data Store AUX Directories\n");
		for (e = aux_list; e != NULL; e = e->next) printf("	%s   %lu\n", e->name, e->size);
		printf("Data Store BB.AUX Directories\n");
		for (e = bb_aux_list; e != NULL; e = e->next) printf("	%s   %lu\n", e->name, e->size);
		printf("Data Store BB Files\n");
		for (e = bb_list; e != NULL; e = e->next) printf("	%s   %lu\n", e->name, e->size);
	}

	/* Delete/achive expired YMD files */
	if (debug == 1) printf("Start YMD File Scan\n");

	e = ymd_list;
	while (e != NULL)
	{
		/* stop when a file name/date is after the expire date */
		if (strncmp(e->name, expire_ymd_string, expire_ymd_stringlen) > 0) break;

		if (archive != NULL)
		{
			str = NULL;
			asprintf(&str, "%s/%s", archive, e->name);
			if (str == NULL) return -1;

			if (debug == 1) printf("  copy %s ---> %s\n", e->name, str);
			status = do_copy(e->name, str, archive_mode);
			free(str);
		}

		if (debug == 1) printf("  unlink %s\n", e->name);
		unlink(e->name);

		store_size -= e->size;
		e->size = 0;

		e = e->next;
	}

	if (debug == 1) printf("Finished YMD FILE Scan\n");

	/* Delete/achive expired YMD AUX directories */
	if (debug == 1) printf("Start AUX Directory Scan\n");

	e = aux_list;
	while (e != NULL)
	{
		/* stop when a file name/date is after the expire date */
		if (strncmp(e->name + 4, expire_ymd_string, expire_ymd_stringlen) > 0) break;

		if (archive != NULL)
		{
			str = NULL;
			asprintf(&str, "%s/%s", archive, e->name);
			if (str == NULL) return -1;

			if (debug == 1) printf("  copy %s ---> %s\n", e->name, str);
			do_dir_archive(e->name, str);
			free(str);
		}

		if (debug == 1) printf("    Remove %s\n", e->name);
		remove_directory(e->name);

		store_size -= e->size;
		e->size = 0;

		e = e->next;
	}

	if (debug == 1) printf("Finished AUX Directory Scan\n");

	/* Delete/achive expired BB.AUX directories */
	if (debug == 1) printf("Start BB.AUX Directory Scan\n");

	e = bb_aux_list;
	while (e != NULL)
	{
		/* stop when a file name/date is after the expire date */
		if (strncmp(e->name + 7, today_ymd_string, today_ymd_stringlen) > 0) break;

		if (archive != NULL)
		{
			str = NULL;
			asprintf(&str, "%s/%s", archive, e->name);
			if (str == NULL) return -1;

			if (debug == 1) printf("  copy %s ---> %s\n", e->name, str);
			do_dir_archive(e->name, str);
			free(str);
		}

		if (debug == 1) printf("  remove %s\n", e->name);
		remove_directory(e->name);

		store_size -= e->size;
		e->size = 0;

		e = e->next;
	}

	if (debug == 1) printf("Finished BB.AUX Directory Scan\n");

	/* Delete/achive expired BB files */
	if (debug == 1) printf("Start BB Scan\n");

	e = bb_list;
	while (e != NULL)
	{
		/* stop when a file name/date is after the expire date */
		if (strncmp(e->name + 3, today_ymd_string, today_ymd_stringlen) > 0) break;

		if (archive != NULL)
		{
			str = NULL;
			asprintf(&str, "%s/%s", archive, e->name);
			if (str == NULL) return -1;

			/* syslog -x [str] -f [e->name] */
			if (debug == 1) printf("  copy %s ---> %s\n", e->name, str);
			status = do_copy(e->name, str, archive_mode);
			free(str);
		}

		if (debug == 1) printf("  unlink %s\n", e->name);
		unlink(e->name);

		store_size -= e->size;
		e->size = 0;

		e = e->next;
	}

	if (debug == 1) printf("Finished BB Scan\n");

	/* if data store is over max_size, delete/archive more YMD files */
	if ((debug == 1) && (store_size > max_size)) printf("Additional YMD Scan\n");

	e = ymd_list;
	while ((e != NULL) && (store_size > max_size))
	{
		if (e->size != 0)
		{
			/* stop when we get to today's files */
			if (strncmp(e->name, today_ymd_string, today_ymd_stringlen) == 0) break;

			if (archive != NULL)
			{
				str = NULL;
				asprintf(&str, "%s/%s", archive, e->name);
				if (str == NULL) return -1;

				/* syslog -x [str] -f [e->name] */
				if (debug == 1) printf("  copy %s ---> %s\n", e->name, str);
				status = do_copy(e->name, str, archive_mode);
				free(str);
			}

			if (debug == 1) printf("  unlink %s\n", e->name);
			unlink(e->name);

			store_size -= e->size;
			e->size = 0;
		}

		e = e->next;
	}

	/* if data store is over max_size, delete/archive more BB files */
	if ((debug == 1) && (store_size > max_size)) printf("Additional BB Scan\n");

	e = bb_list;
	while ((e != NULL) && (store_size > max_size))
	{
		if (e->size != 0)
		{
			if (archive != NULL)
			{
				str = NULL;
				asprintf(&str, "%s/%s", archive, e->name);
				if (str == NULL) return -1;

				/* syslog -x [str] -f [e->name] */
				if (debug == 1) printf("  copy %s ---> %s\n", e->name, str);
				status = do_copy(e->name, str, archive_mode);
				free(str);
			}

			if (debug == 1) printf("  unlink %s\n", e->name);
			unlink(e->name);

			store_size -= e->size;
			e->size = 0;
		}

		e = e->next;
	}

	free_list(ymd_list);	 
	free_list(bb_list);

	if (debug == 1)
	{
		printf("Data Store Size = %lu\n", store_size);
		printf("aslmanager finished\n");
	}

	return 0;
}

