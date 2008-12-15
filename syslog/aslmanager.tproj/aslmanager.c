/*
 * Copyright (c) 2007-2008 Apple Inc. All rights reserved.
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

#define SECONDS_PER_DAY 86400
#define DEFAULT_MAX_SIZE 51200000
#define DEFAULT_TTL 2

typedef struct name_list_s
{
	char *name;
	size_t size;
	struct name_list_s *next;
} name_list_t;

void
mgr_exit(const char *store, int status)
{
	char *s;

	if (store == NULL) exit(status);

	s = NULL;
	asprintf(&s, "%s/%s", store, FILE_ASL_STORE_SWEEP_SEMAPHORE);
	if (s != NULL)
	{
		unlink(s);
		free(s);
	}
	else exit(1);

	exit(status);
}

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
do_match(const char *infile, const char *outfile, int do_ttl, time_t expire_time)
{
	asl_search_result_t q, *query, *res;
	asl_msg_t *m, *qm[1];
	asl_file_t *in, *out;
	uint32_t status, i;
	char str[64];
	uint64_t mid;

	if (infile == NULL) return ASL_STATUS_INVALID_ARG;
	if (outfile == NULL) return ASL_STATUS_INVALID_ARG;

	in = NULL;
	status = asl_file_open_read(infile, &in);
	if (status != ASL_STATUS_OK) return status;

	query = NULL;
	q.count = 1;
	q.curr = 0;
	q.msg = qm;
	qm[0] = NULL;
	m = NULL;

	if (do_ttl == 1)
	{
		query = &q;
		m = asl_new(ASL_TYPE_QUERY);
		if (m == NULL)
		{
			asl_file_close(in);
			return ASL_STATUS_NO_MEMORY;
		}

		qm[0] = m;

		if (expire_time != 0)
		{
			snprintf(str, sizeof(str), "%llu", (long long unsigned int)expire_time);
			if (asl_set_query(m, ASL_KEY_EXPIRE_TIME, str, ASL_QUERY_OP_NUMERIC | ASL_QUERY_OP_GREATER_EQUAL) != 0)
			{
				asl_file_close(in);
				asl_free(m);
				return ASL_STATUS_NO_MEMORY;
			}
		}
		else
		{
			if (asl_set_query(m, ASL_KEY_EXPIRE_TIME, NULL, ASL_QUERY_OP_TRUE) != 0)
			{
				asl_file_close(in);
				asl_free(m);
				return ASL_STATUS_NO_MEMORY;
			}
		}
	}

	res = NULL;
	mid = 0;
	status = asl_file_match(in, query, &res, &mid, 0, 0, 1);
	if (m != NULL) asl_free(m);
	asl_file_close(in);

	if (status != ASL_STATUS_OK) return status;

	/*
	 * N.B. "ASL_STATUS_NOT_FOUND" is never returned by asl_file_match.
	 * We use it here to signal the caller that no records were found by the match.
	 */
	if (res == NULL) return ASL_STATUS_NOT_FOUND;
	if (res->count == 0)
	{
		aslresponse_free(res);
		return ASL_STATUS_NOT_FOUND;
	}

	out = NULL;
	status = asl_file_open_write(outfile, 0644, -1, -1, &out);
	if (status != ASL_STATUS_OK) return status;

	out->flags = ASL_FILE_FLAG_UNLIMITED_CACHE | ASL_FILE_FLAG_PRESERVE_MSG_ID;

	for (i = 0; i < res->count; i++)
	{
		mid = 0;
		status = asl_file_save(out, res->msg[i], &mid);
		if (status != ASL_STATUS_OK) break;
	}

	asl_file_close(out);
	return status;
}

int
main(int argc, const char *argv[])
{
	int i, bbstrlen, debug;
	const char *archive, *store_dir;
	time_t now, best_before, ttl;
	struct tm ctm;
	char bbstr[32], *str, *p;
	DIR *dp;
	struct dirent *dent;
	name_list_t *list, *e;
	uint32_t status;
	size_t file_size, store_size, max_size;
	struct stat sb;

	list = NULL;

	archive = NULL;
	store_dir = PATH_ASL_STORE;
	ttl = DEFAULT_TTL * SECONDS_PER_DAY;
	max_size = DEFAULT_MAX_SIZE;
	store_size = 0;
	debug = 0;

	for (i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i], "-a"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) archive = argv[++i];
			else archive = PATH_ASL_ARCHIVE;
		}
		else if (!strcmp(argv[i], "-s"))
		{
			if (((i + 1) < argc) && (argv[i + 1][0] != '-')) store_dir = argv[++i];
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

	/* check archive */
	if (archive != NULL)
	{
		memset(&sb, 0, sizeof(struct stat));
		if (stat(archive, &sb) == 0)
		{
			/* must be a directory */
			if ((sb.st_mode & S_IFDIR) == 0)
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

	/* determine current time and time TTL ago */
	now = time(NULL);
	best_before = 0;
	if (ttl > 0) best_before = now - ttl;

	/* construct best before date as YYYY.MM.DD */
	memset(&ctm, 0, sizeof(struct tm));
	if (localtime_r((const time_t *)&best_before, &ctm) == NULL) mgr_exit(store_dir, 1);

	snprintf(bbstr, sizeof(bbstr), "%d.%02d.%02d.", ctm.tm_year + 1900, ctm.tm_mon + 1, ctm.tm_mday);
	bbstrlen = strlen(bbstr);

	if (debug == 1) printf("Best Before Date %s\n", bbstr);

	dp = opendir(store_dir);
	if (dp == NULL) mgr_exit(store_dir, 1);

	/* gather a list of files for dates before the best before date */

	while ((dent = readdir(dp)) != NULL)
	{
		if ((dent->d_name[0] < '0') || (dent->d_name[0] > '9')) continue;

		memset(&sb, 0, sizeof(struct stat));
		file_size = 0;
		if (stat(dent->d_name, &sb) == 0) file_size = sb.st_size;
		store_size += file_size;

		list = add_to_list(list, dent->d_name, file_size);
	}

	closedir(dp);

	if (debug == 1)
	{
		printf("\nData Store Size = %lu\n", store_size);
		printf("\nData Store Files\n");
		for (e = list; e != NULL; e = e->next) printf("	%s   %lu\n", e->name, e->size);
	}

	/* copy messages in each expired file with ASLExpireTime values to LongTTL files */
	if (debug == 1) printf("\nStart Scan\n");

	e = list;
	while (e != NULL)
	{
		if ((store_size <= max_size) && (strncmp(e->name, bbstr, bbstrlen) >= 0)) break;

		/* find '.' after year */
		p = strchr(e->name, '.');
		if (p == NULL) continue;

		/* find '.' after month */
		p++;
		p = strchr(p, '.');
		if (p == NULL) continue;

		/* find '.' after day */
		p++;
		p = strchr(p, '.');
		if (p == NULL) continue;

		str = NULL;
		asprintf(&str, "LongTTL%s", p);
		if (str == NULL) mgr_exit(store_dir, 1);

		/* syslog -x [str] -db [e->name] -k ASLExpireTime */
		if (debug == 1) printf("	scan %s ---> %s\n", e->name, str);
		else status = do_match(e->name, str, 1, 0);

		free(str);
		str = NULL;

		if (archive != NULL)
		{
			str = NULL;
			asprintf(&str, "%s/%s", archive, e->name);
			if (str == NULL) mgr_exit(store_dir, 1);

			/* syslog -x [str] -db [e->name] */
			if (debug == 1) printf("	copy %s ---> %s\n", e->name, str);
			else status = do_match(e->name, str, 0, 0);
			free(str);
		}

		if (debug == 1) printf("	unlink %s\n", e->name);
		else unlink(e->name);

		store_size -= e->size;
		e->size = 0;

		e = e->next;
	}

	if (debug == 1)
	{
		printf("Finished Scan\n");
		printf("\nData Store Size = %lu\n", store_size);
	}

	free_list(list);
	list = NULL;

	dp = opendir(PATH_ASL_STORE);
	if (dp == NULL) mgr_exit(store_dir, 1);

	/* gather a list of LongTTL files */

	while ((dent = readdir(dp)) != NULL)
	{
		if (!strncmp(dent->d_name, "LongTTL.", 8)) list = add_to_list(list, dent->d_name, 0);
	}

	closedir(dp);

	if (debug == 1)
	{
		printf("\nData Store LongTTL Files\n");
		for (e = list; e != NULL; e = e->next) printf("	%s\n", e->name);
	}

	if (debug == 1) printf("\nScan for expired messages\n");

	e = list;
	while (e != NULL)
	{
		/* syslog -x LongTTL.new -db [e->name] -k ASLExpireTime Nge [now] */
		if (debug == 1)
		{
			printf("	%s\n", e->name);
		}
		else
		{
			status = do_match(e->name, "LongTTL.new", 1, now);
			unlink(e->name);
			if (status == ASL_STATUS_OK) rename("LongTTL.new", e->name);
		}

		e = e->next;
	}

	if (debug == 1) printf("Finished scan for expired messages\n");

	free_list(list);
	list = NULL;

	mgr_exit(store_dir, 0);
	/* UNREACHED */
	return 0;
}

