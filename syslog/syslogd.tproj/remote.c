/*
 * Copyright (c) 2004-2008 Apple Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#include <notify.h>
#include <asl_core.h>
#include <asl_memory.h>
#include "daemon.h"

#define forever for(;;)

#define MY_ID "remote"
#define MAXLINE 4096
#define LOCKDOWN_PATH "/var/run/lockdown"
#define SYSLOG_SOCK_PATH "/var/run/lockdown/syslog.sock"
#define ASL_REMOTE_PORT 203

#define PRINT_STD 0
#define PRINT_RAW 1

#define MAXSOCK 1

static int rfd4 = -1;
static int rfd6 = -1;
static int rfdl = -1;

#ifdef NSS32 
typedef uint32_t notify_state_t;
extern int notify_set_state(int, notify_state_t);
#else
typedef uint64_t notify_state_t;
#endif

extern char *asl_list_to_string(asl_search_result_t *list, uint32_t *outlen);
extern size_t asl_memory_size(asl_memory_t *s);
extern uint32_t db_query(aslresponse query, aslresponse *res, uint64_t startid, int count, int flags, uint64_t *lastid, int32_t ruid, int32_t rgid);

#define SESSION_WRITE(f,x) if (write(f, x, strlen(x)) < 0) goto exit_session

uint32_t
remote_db_size(uint32_t sel)
{
	if (sel == DB_TYPE_FILE) return global.db_file_max;
	if (sel == DB_TYPE_MEMORY) return global.db_memory_max;
	if (sel == DB_TYPE_MINI) return global.db_mini_max;
	return 0;
}

uint32_t
remote_db_set_size(uint32_t sel, uint32_t size)
{
	if (sel == DB_TYPE_FILE) global.db_file_max = size;
	if (sel == DB_TYPE_MEMORY) global.db_memory_max = size;
	if (sel == DB_TYPE_MINI) global.db_mini_max = size;
	return 0;
}

asl_msg_t *
remote_db_stats(uint32_t sel)
{
	asl_msg_t *m;
	m = NULL;

	if (sel == DB_TYPE_FILE) asl_store_statistics(global.file_db, &m);
	if (sel == DB_TYPE_MEMORY) asl_memory_statistics(global.memory_db, &m);
	if (sel == DB_TYPE_MINI) asl_mini_memory_statistics(global.mini_db, &m);
	return m;
}

void
session(void *x)
{
	int i, *sp, s, wfd, status, pfmt, watch, wtoken, nfd, do_prompt, filter;
	aslresponse res;
	asl_search_result_t ql;
	uint32_t outlen;
	asl_msg_t *stats;
	asl_msg_t *query;
	asl_msg_t *qlq[1];
	char str[1024], *p, *qs, *out;
	ssize_t len;
	fd_set readfds;
	uint64_t low_id, high_id;
	notify_state_t nstate;
	uint32_t dbselect;

	if (x == NULL) pthread_exit(NULL);

	sp = (int *)x;
	s = *sp;
	free(x);

	watch = 0;
	wfd = -1;
	wtoken = -1;

	dbselect = 0;
	if (global.dbtype & DB_TYPE_MEMORY) dbselect = DB_TYPE_MEMORY;
	else if (global.dbtype & DB_TYPE_MINI) dbselect = DB_TYPE_MINI;
	else if (global.dbtype & DB_TYPE_FILE) dbselect = DB_TYPE_FILE;

	low_id = 0;
	high_id = 0;

	pfmt = PRINT_STD;
	query = NULL;
	memset(&ql, 0, sizeof(asl_search_result_t));

	snprintf(str, sizeof(str) - 1, "\n========================\nASL is here to serve you\n");
	if (write(s, str, strlen(str)) < 0)
	{
		close(s);
		pthread_exit(NULL);
		return;
	}

	do_prompt = 1;

	forever
	{
		if (do_prompt > 0)
		{
			snprintf(str, sizeof(str) - 1, "> ");
			SESSION_WRITE(s, str);
		}

		do_prompt = 1;
		memset(str, 0, sizeof(str));

		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		nfd = s;

		if (wfd != -1)
		{
			FD_SET(wfd, &readfds);
			if (wfd > nfd) nfd = wfd;
		}

		status = select(nfd + 1, &readfds, NULL, NULL, NULL);

		if ((wfd != -1) && (FD_ISSET(wfd, &readfds)))
		{
			len = read(wfd, &i, sizeof(int));
		}

		if (FD_ISSET(s, &readfds))
		{
			len = read(s, str, sizeof(str) - 1);
			if (len <= 0) goto exit_session;

			while ((len > 1) && ((str[len - 1] == '\n') || (str[len - 1] == '\r')))
			{
				str[len - 1] = '\0';
				len--;
			}

			if ((!strcmp(str, "q")) || (!strcmp(str, "quit")) || (!strcmp(str, "exit")))
			{
				snprintf(str, sizeof(str) - 1, "Goodbye\n");
				write(s, str, strlen(str));
				close(s);
				s = -1;
				break;
			}

			if ((!strcmp(str, "?")) || (!strcmp(str, "help")))
			{
				snprintf(str, sizeof(str) - 1, "Commands\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    quit                 exit session\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    select [val]         get [set] current database\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "                         val must be \"file\", \"mem\", or \"mini\"\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    file [on/off]        enable / disable file store\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    memory [on/off]      enable / disable memory store\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    mini [on/off]        enable / disable mini memory store\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    stats                database statistics\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    flush                flush database\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    dbsize [val]         get [set] database size (# of records)\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    filter [val]         get [set] current database filter\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "                         [p]anic (emergency)  [a]lert  [c]ritical  [e]rror\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "                         [w]arning  [n]otice  [i]nfo  [d]ebug\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    watch                print new messages as they arrive\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    stop                 stop watching for new messages\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    raw                  use raw format for printing messages\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    std                  use standard format for printing messages\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    *                    show all log messages\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    * key val            equality search for messages (single key/value pair)\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    * op key val         search for matching messages (single key/value pair)\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "    * [op key val] ...   search for matching messages (multiple key/value pairs)\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "                         operators:  =  <  >  ! (not equal)  T (key exists)  R (regex)\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "                         modifiers (must follow operator):\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "                                 C=casefold  N=numeric  S=substring  A=prefix  Z=suffix\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "\n");
				SESSION_WRITE(s, str);
				continue;
			}
			else if (!strcmp(str, "stats"))
			{
				stats = remote_db_stats(dbselect);
				out = asl_format_message(stats, ASL_MSG_FMT_RAW, ASL_TIME_FMT_SEC, ASL_ENCODE_NONE, &outlen);
				write(s, out, outlen);
				free(out);
				asl_free(stats);
				continue;
			}
			else if (!strcmp(str, "flush"))
			{}
			else if (!strncmp(str, "select", 6))
			{
				p = str + 6;
				while ((*p == ' ') || (*p == '\t')) p++;
				if (*p == '\0')
				{
					if (dbselect == 0) snprintf(str, sizeof(str) - 1, "no store\n");
					else if (dbselect == DB_TYPE_FILE) snprintf(str, sizeof(str) - 1, "file store\n");
					else if (dbselect == DB_TYPE_MEMORY) snprintf(str, sizeof(str) - 1, "memory store\n");
					else if (dbselect == DB_TYPE_MINI) snprintf(str, sizeof(str) - 1, "mini memory store\n");
					SESSION_WRITE(s, str);
					continue;
				}

				if (!strncmp(p, "file", 4))
				{
					if ((global.dbtype & DB_TYPE_FILE) == 0)
					{
						snprintf(str, sizeof(str) - 1, "file database is not enabled\n");
						SESSION_WRITE(s, str);
						continue;
					}

					dbselect = DB_TYPE_FILE;
				}
				else if (!strncmp(p, "mem", 3))
				{
					if ((global.dbtype & DB_TYPE_MEMORY) == 0)
					{
						snprintf(str, sizeof(str) - 1, "memory database is not enabled\n");
						SESSION_WRITE(s, str);
						continue;
					}

					dbselect = DB_TYPE_MEMORY;
				}
				else if (!strncmp(p, "mini", 4))
				{
					if ((global.dbtype & DB_TYPE_MINI) == 0)
					{
						if (global.mini_db != NULL)
						{
							snprintf(str, sizeof(str) - 1, "mini memory database is enabled for disaster messages\n");
							SESSION_WRITE(s, str);
						}
						else
						{
							snprintf(str, sizeof(str) - 1, "mini memory database is not enabled\n");
							SESSION_WRITE(s, str);
							continue;
						}
					}

					dbselect = DB_TYPE_MINI;
				}
				else
				{
					snprintf(str, sizeof(str) - 1, "unknown database type\n");
					SESSION_WRITE(s, str);
					continue;
				}

				snprintf(str, sizeof(str) - 1, "OK\n");
				SESSION_WRITE(s, str);
				continue;
			}
			else if (!strncmp(str, "file", 4))
			{
				p = str + 4;
				while ((*p == ' ') || (*p == '\t')) p++;
				if (*p == '\0')
				{
					snprintf(str, sizeof(str) - 1, "file database is %senabled\n", (global.dbtype & DB_TYPE_FILE) ? "" : "not ");
					SESSION_WRITE(s, str);
					if ((global.dbtype & DB_TYPE_FILE) != 0) dbselect = DB_TYPE_FILE;
					continue;
				}

				if (!strcmp(p, "on")) global.dbtype |= DB_TYPE_FILE;
				else if (!strcmp(p, "off")) global.dbtype &= ~ DB_TYPE_FILE;

				snprintf(str, sizeof(str) - 1, "OK\n");
				SESSION_WRITE(s, str);
				continue;
			}
			else if (!strncmp(str, "memory", 6))
			{
				p = str + 6;
				while ((*p == ' ') || (*p == '\t')) p++;
				if (*p == '\0')
				{
					snprintf(str, sizeof(str) - 1, "memory database is %senabled\n", (global.dbtype & DB_TYPE_MEMORY) ? "" : "not ");
					SESSION_WRITE(s, str);
					if ((global.dbtype & DB_TYPE_MEMORY) != 0) dbselect = DB_TYPE_MEMORY;
					continue;
				}

				if (!strcmp(p, "on")) global.dbtype |= DB_TYPE_MEMORY;
				else if (!strcmp(p, "off")) global.dbtype &= ~ DB_TYPE_MEMORY;

				snprintf(str, sizeof(str) - 1, "OK\n");
				SESSION_WRITE(s, str);
				continue;
			}
			else if (!strncmp(str, "mini", 4))
			{
				p = str + 4;
				while ((*p == ' ') || (*p == '\t')) p++;
				if (*p == '\0')
				{
					snprintf(str, sizeof(str) - 1, "mini database is %senabled\n", (global.dbtype & DB_TYPE_MINI) ? "" : "not ");
					SESSION_WRITE(s, str);
					if ((global.dbtype & DB_TYPE_MINI) != 0) dbselect = DB_TYPE_MINI;
					continue;
				}

				if (!strcmp(p, "on")) global.dbtype |= DB_TYPE_MINI;
				else if (!strcmp(p, "off")) global.dbtype &= ~ DB_TYPE_MINI;

				snprintf(str, sizeof(str) - 1, "OK\n");
				SESSION_WRITE(s, str);
				continue;
			}
			else if (!strncmp(str, "dbsize", 6))
			{
				if (dbselect == 0)
				{
					snprintf(str, sizeof(str) - 1, "no store\n");
					SESSION_WRITE(s, str);
					continue;
				}

				p = str + 6;
				while ((*p == ' ') || (*p == '\t')) p++;
				if (*p == '\0')
				{
					snprintf(str, sizeof(str) - 1, "DB size %u\n", remote_db_size(dbselect));
					SESSION_WRITE(s, str);
					continue;
				}

				i = atoi(p);
				remote_db_set_size(dbselect, i);

				snprintf(str, sizeof(str) - 1, "OK\n");
				SESSION_WRITE(s, str);
				continue;
			}
			else if (!strncmp(str, "filter", 6))
			{
				p = str + 6;
				while ((*p == ' ') || (*p == '\t')) p++;
				if (*p == '\0')
				{
					snprintf(str, sizeof(str) - 1, "%s%s%s%s%s%s%s%s\n", 
							 (global.asl_log_filter & ASL_FILTER_MASK_EMERG) ? "p" : "", 
							 (global.asl_log_filter & ASL_FILTER_MASK_ALERT) ? "a" : "", 
							 (global.asl_log_filter & ASL_FILTER_MASK_CRIT) ? "c" : "", 
							 (global.asl_log_filter & ASL_FILTER_MASK_ERR) ? "e" : "", 
							 (global.asl_log_filter & ASL_FILTER_MASK_WARNING) ? "w" : "", 
							 (global.asl_log_filter & ASL_FILTER_MASK_NOTICE) ? "n" : "", 
							 (global.asl_log_filter & ASL_FILTER_MASK_INFO) ? "i" : "", 
							 (global.asl_log_filter & ASL_FILTER_MASK_DEBUG) ? "d" : "");
					SESSION_WRITE(s, str);
					continue;
				}

				filter = 0;
				if ((*p >= '0') && (*p <= '7'))
				{
					i = atoi(p);
					filter = ASL_FILTER_MASK_UPTO(i);
				}
				else
				{
					while (*p != '\0')
					{
						if ((*p == 'p') || (*p == 'P')) filter |= ASL_FILTER_MASK_EMERG;
						else if ((*p == 'a') || (*p == 'A')) filter |= ASL_FILTER_MASK_ALERT;
						else if ((*p == 'c') || (*p == 'C')) filter |= ASL_FILTER_MASK_CRIT;
						else if ((*p == 'e') || (*p == 'E')) filter |= ASL_FILTER_MASK_ERR;
						else if ((*p == 'w') || (*p == 'W')) filter |= ASL_FILTER_MASK_WARNING;
						else if ((*p == 'n') || (*p == 'N')) filter |= ASL_FILTER_MASK_NOTICE;
						else if ((*p == 'i') || (*p == 'I')) filter |= ASL_FILTER_MASK_INFO;
						else if ((*p == 'd') || (*p == 'D')) filter |= ASL_FILTER_MASK_DEBUG;
						p++;
					}
				}

				status = notify_register_check(NOTIFY_SYSTEM_ASL_FILTER, &i);
				if (status != NOTIFY_STATUS_OK)
				{
					snprintf(str, sizeof(str) - 1, "FAILED %d\n", status);
					SESSION_WRITE(s, str);
				}
				else
				{
					nstate = filter;
					status = notify_set_state(i, nstate);
					if (status != NOTIFY_STATUS_OK)
					{
						snprintf(str, sizeof(str) - 1, "FAILED %d\n", status);
						SESSION_WRITE(s, str);
						continue;
					}

					status = notify_post(NOTIFY_SYSTEM_ASL_FILTER);
					notify_cancel(i);

					global.asl_log_filter = filter;

					snprintf(str, sizeof(str) - 1, "OK\n");
					SESSION_WRITE(s, str);
				}

				continue;
			}
			else if (!strcmp(str, "stop"))
			{
				if (watch == 1)
				{
					watch = 0;
					notify_cancel(wtoken);
					wfd = -1;
					wtoken = -1;

					low_id = 0;
					high_id = 0;

					if (query != NULL) free(query);
					query = NULL;

					snprintf(str, sizeof(str) - 1, "OK\n");
					SESSION_WRITE(s, str);
					continue;
				}

				snprintf(str, sizeof(str) - 1, "not watching!\n");
				SESSION_WRITE(s, str);
				continue;
			}
			else if (!strcmp(str, "raw"))
			{
				pfmt = PRINT_RAW;
				continue;
			}
			else if (!strcmp(str, "std"))
			{
				pfmt = PRINT_STD;
				continue;
			}
			else if (!strcmp(str, "watch"))
			{
				if (watch == 1)
				{
					snprintf(str, sizeof(str) - 1, "already watching!\n");
					SESSION_WRITE(s, str);
					continue;
				}

				status = notify_register_file_descriptor(SELF_DB_NOTIFICATION, &wfd, 0, &wtoken);
				if (status != 0)
				{
					snprintf(str, sizeof(str) - 1, "notify_register_file_descriptor failed: %d\n", status);
					SESSION_WRITE(s, str);
					continue;
				}

				watch = 1;

				snprintf(str, sizeof(str) - 1, "OK\n");
				SESSION_WRITE(s, str);
				do_prompt = 2;
			}
			else if ((str[0] == '*') || (str[0] == 'T') || (str[0] == '=') || (str[0] == '!') || (str[0] == '<') || (str[0] == '>'))
			{
				memset(&ql, 0, sizeof(asl_search_result_t));
				if (query != NULL) free(query);
				query = NULL;

				p = str;
				if (*p == '*') p++;
				while ((*p == ' ') || (*p == '\t')) p++;

				if (*p == '\0')
				{
					/* NULL query */
				}
				else if (*p == '[')
				{
					qs = NULL;
					asprintf(&qs, "Q %s", p);
					query = asl_msg_from_string(qs);
					free(qs);
				}
				else if ((*p == 'T') || (*p == '=') || (*p == '!') || (*p == '<') || (*p == '>') || (*p == 'R'))
				{
					qs = NULL;
					asprintf(&qs, "Q [%s]", p);
					query = asl_msg_from_string(qs);
					free(qs);
				}
				else
				{
					qs = NULL;
					asprintf(&qs, "Q [= %s]", p);
					query = asl_msg_from_string(qs);
					free(qs);
				}
			}
			else
			{
				snprintf(str, sizeof(str) - 1, "unrecognized command\n");
				SESSION_WRITE(s, str);
				snprintf(str, sizeof(str) - 1, "enter \"help\" for help\n");
				SESSION_WRITE(s, str);
				continue;
			}
		}

		if (query != NULL)
		{
			ql.count = 1;
			qlq[0] = query;
			ql.msg = qlq;
		}

		if (watch == 0) low_id = 0;

		memset(&res, 0, sizeof(aslresponse));
		high_id = 0;
		status = db_query(&ql, (aslresponse *)&res, low_id, 0, 0, &high_id, 0, 0);

		if ((watch == 1) && (high_id >= low_id)) low_id = high_id + 1;

		if (res == NULL)
		{
			if (watch == 0)
			{
				snprintf(str, sizeof(str) - 1, "-nil-\n");
				SESSION_WRITE(s, str);
			}
			else
			{
				if (do_prompt != 2) do_prompt = 0;
			}
		}
		else if (pfmt == PRINT_RAW)
		{
			if (watch == 1)
			{
				snprintf(str, sizeof(str) - 1, "\n");
				SESSION_WRITE(s, str);
			}

			outlen = 0;
			out = asl_list_to_string((asl_search_result_t *)res, &outlen);
			write(s, out, outlen);
			free(out);

			snprintf(str, sizeof(str) - 1, "\n");
			SESSION_WRITE(s, str);
		}
		else
		{
			if (watch == 1)
			{
				snprintf(str, sizeof(str) - 1, "\n");
				SESSION_WRITE(s, str);
			}

			for (i = 0; i < res->count; i++)
			{
				out = asl_format_message(res->msg[i], ASL_MSG_FMT_STD, ASL_TIME_FMT_LCL, ASL_ENCODE_SAFE, &outlen);
				write(s, out, outlen);
				free(out);
			}
		}

		aslresponse_free(res);
	}

exit_session:

	if (s >= 0)
	{
		close(s);
		s = -1;
	}

	if (watch == 1) notify_cancel(wtoken);
	if (query != NULL) asl_free(query);
	pthread_exit(NULL);
}

asl_msg_t *
remote_acceptmsg(int fd, int tcp)
{
	socklen_t fromlen;
	int s, status, flags, *sp;
	pthread_attr_t attr;
	pthread_t t;
	struct sockaddr_storage from;

	fromlen = sizeof(struct sockaddr_un);
	if (tcp == 1) fromlen = sizeof(struct sockaddr_storage);

	memset(&from, 0, sizeof(from));

	s = accept(fd, (struct sockaddr *)&from, &fromlen);
	if (s == -1)
	{
		asldebug("%s: accept: %s\n", MY_ID, strerror(errno));
		return NULL;
	}

	flags = fcntl(s, F_GETFL, 0);
	flags &= ~ O_NONBLOCK;
	status = fcntl(s, F_SETFL, flags);
	if (status < 0)
	{
		asldebug("%s: fcntl: %s\n", MY_ID, strerror(errno));
		close(s);
		return NULL;
	}

	if (tcp == 1)
	{
		flags = 1;
		setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(int));
	}

	sp = malloc(sizeof(int));
	if (sp == NULL)
	{
		asldebug("%s: malloc: %s\n", MY_ID, strerror(errno));
		close(s);
		return NULL;
	}

	*sp = s;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&t, &attr, (void *(*)(void *))session, (void *)sp);
	pthread_attr_destroy(&attr);

	return NULL;
}

asl_msg_t *
remote_acceptmsg_local(int fd)
{
	return remote_acceptmsg(fd, 0);
}

asl_msg_t *
remote_acceptmsg_tcp(int fd)
{
	return remote_acceptmsg(fd, 1);
}

int
remote_init_lockdown(void)
{
	int status, reuse, fd;
	struct sockaddr_un local;

	fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0)
	{
		asldebug("%s: socket: %s\n", MY_ID, strerror(errno));
		return -1;
	}

	reuse = 1;
	status = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int));
	if (status < 0)
	{
		asldebug("%s: setsockopt: %s\n", MY_ID, strerror(errno));
		close(fd);
		return -1;
	}

	/* make sure the lockdown directory exists */
	mkdir(LOCKDOWN_PATH, 0777);

	memset(&local, 0, sizeof(local));
	local.sun_family = AF_UNIX;
	strlcpy(local.sun_path, SYSLOG_SOCK_PATH, sizeof(local.sun_path));
	unlink(local.sun_path);

	status = bind(fd, (struct sockaddr *)&local, sizeof(local.sun_family) + sizeof(local.sun_path));

	if (status < 0)
	{
		asldebug("%s: bind: %s\n", MY_ID, strerror(errno));
		close(fd);
		return -1;
	}

	status = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (status < 0)
	{
		asldebug("%s: fcntl: %s\n", MY_ID, strerror(errno));
		close(fd);
		return -1;
	}

	status = listen(fd, 5);
	if (status < 0)
	{
		asldebug("%s: listen: %s\n", MY_ID, strerror(errno));
		close(fd);
		return -1;
	}

	chmod(SYSLOG_SOCK_PATH, 0666);

	aslevent_addfd(fd, 0, remote_acceptmsg_local, NULL, NULL);
	return fd;
}

int
remote_init_tcp(int family)
{
	int status, reuse, fd;
	struct sockaddr_in a4;
	struct sockaddr_in6 a6;
	struct sockaddr *s;
	socklen_t len;

	fd = socket(family, SOCK_STREAM, 0);
	if (fd < 0)
	{
		asldebug("%s: socket: %s\n", MY_ID, strerror(errno));
		return -1;
	}

	reuse = 1;
	status = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int));
	if (status < 0)
	{
		asldebug("%s: setsockopt: %s\n", MY_ID, strerror(errno));
		close(fd);
		return -1;
	}

	memset(&(a4.sin_addr), 0, sizeof(struct in_addr));
	a4.sin_family = AF_INET;
	a4.sin_port = htons(ASL_REMOTE_PORT);

	memset(&(a6.sin6_addr), 0, sizeof(struct in6_addr));
	a6.sin6_family = AF_INET6;
	a6.sin6_port = htons(ASL_REMOTE_PORT);

	s = (struct sockaddr *)&a4;
	len = sizeof(struct sockaddr_in);

	if (family == AF_INET6)
	{
		s = (struct sockaddr *)&a6;
		len = sizeof(struct sockaddr_in6);
	}

	status = bind(fd, s, len);
	if (status < 0)
	{
		asldebug("%s: bind: %s\n", MY_ID, strerror(errno));
		close(fd);
		return -1;
	}

	status = fcntl(fd, F_SETFL, O_NONBLOCK);
	if (status < 0)
	{
		asldebug("%s: fcntl: %s\n", MY_ID, strerror(errno));
		close(fd);
		return -1;
	}

	status = listen(fd, 5);
	if (status < 0)
	{
		asldebug("%s: listen: %s\n", MY_ID, strerror(errno));
		close(fd);
		return -1;
	}

	aslevent_addfd(fd, 0, remote_acceptmsg_tcp, NULL, NULL);
	return fd;
}

int
remote_init(void)
{
	asldebug("%s: init\n", MY_ID);

#ifdef LOCKDOWN
	rfdl = remote_init_lockdown();
#endif

#ifdef REMOTE_IPV4
	rfd4 = remote_init_tcp(AF_INET);
#endif

#ifdef REMOTE_IPV6
	rfd6 = remote_init_tcp(AF_INET6);
#endif

	return 0;
}

int
remote_close(void)
{
	if (rfdl >= 0) close(rfdl);
	rfdl = -1;

	if (rfd4 >= 0) close(rfd4);
	rfd4 = -1;

	if (rfd6 >= 0) close(rfd6);
	rfd6 = -1;

	return 0;
}

int
remote_reset(void)
{
	remote_close();
	return remote_init();
}
