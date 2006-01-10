/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "daemon.h"

#define forever for(;;)

#define MY_ID "asl"

static int sock = -1;
static FILE *aslfile = NULL;
static asl_msg_t *query = NULL;

extern int asl_log_filter;

#define MATCH_EOF -1
#define MATCH_NULL 0
#define MATCH_TRUE 1
#define MATCH_FALSE 2

extern int prune;

static int filter_token = -1;

struct prune_query_entry
{
	asl_msg_t *query;
	TAILQ_ENTRY(prune_query_entry) entries;
};

static TAILQ_HEAD(pql, prune_query_entry) pquery;

static int
_search_next(FILE *log, char **outstr)
{
	char *str;
	aslmsg m;
	int match, i;
	struct prune_query_entry *p;

	*outstr = NULL;

	if (log == NULL) return MATCH_EOF;

	str = get_line_from_file(log);
	if (str == NULL) return MATCH_EOF;

	m = asl_msg_from_string(str);
	if (m == NULL)
	{
		free(str);
		return MATCH_NULL;
	}

	*outstr = str;

	for (i = 0, p = pquery.tqh_first; p != NULL; p = p->entries.tqe_next, i++)
	{
		match = asl_msg_cmp(p->query, m);
		if (match == 1)
		{
			asl_free(m);
			return MATCH_TRUE;
		}
	}

	asl_free(m);
	return MATCH_FALSE;
}

/*
 * Pruning the main output file (asl.log)
 *
 * The prune file (_PATH_ASL_PRUNE) is set up by the syslog command-line utiliy.
 * It contains a set of queries.  The main output file is read, and each
 * message is matched against these queries.  If any query matches, we save
 * that message.  Anything that doesn't match is discarded.
 *
 */
int
asl_prune(asl_msg_t *inq)
{
	char *pname, *str;
	FILE *pfile, *qfile;
	struct prune_query_entry *p, *n;
	asl_msg_t *q;
	int status, incount, outcount;

	asldebug("syslogd: pruning %s\n", _PATH_ASL_OUT);

	if (inq != NULL)
	{
		TAILQ_INIT(&pquery);
		p = (struct prune_query_entry *)calloc(1, sizeof(struct prune_query_entry));
		if (p == NULL) return -1;

		p->query = inq;
		TAILQ_INSERT_TAIL(&pquery, p, entries);
	}
	else
	{
		qfile = fopen(_PATH_ASL_PRUNE, "r");
		if (qfile == NULL)
		{
			asldebug("syslogd: can't read %s: %s\n", _PATH_ASL_PRUNE, strerror(errno));
			return 0;
		}

		TAILQ_INIT(&pquery);

		forever
		{
			str = get_line_from_file(qfile);
			if (str == NULL) break;

			q = asl_msg_from_string(str);
			asldebug("syslogd: prune line %s\n", str);

			free(str);
			if (q == NULL) continue;

			if (q->type != ASL_TYPE_QUERY)
			{
				asl_free(q);
				continue;
			}

			p = (struct prune_query_entry *)calloc(1, sizeof(struct prune_query_entry));
			if (p == NULL) return -1;

			p->query = q;
			TAILQ_INSERT_TAIL(&pquery, p, entries);
		}
	}

	pname = NULL;
	asprintf(&pname, "%s.%d", _PATH_ASL_OUT, getpid());
	if (pname == NULL) return -1;

	pfile = fopen(pname, "w");
	if (pfile == NULL)
	{
		asldebug("syslogd: can't write %s: %s\n", pname, strerror(errno));
		free(pname);
		return -1;
	}

	fclose(aslfile);
	aslfile = fopen(_PATH_ASL_OUT, "r");
	if (aslfile == NULL)
	{
		asldebug("syslogd: can't read %s: %s\n", _PATH_ASL_OUT, strerror(errno));
		free(pname);
		aslfile = fopen(_PATH_ASL_OUT, "a");
		return -1;
	}

	incount = 0;
	outcount = 0;

	do
	{
		str = NULL;
		incount++;
		status = _search_next(aslfile, &str);

		/*
		 * Pruning deletes records that match the search.
		 * If the match fails, we keep the record.
		 */
		if (status == MATCH_FALSE)
		{
			outcount++;
			fprintf(pfile, "%s\n", str);
		}

		if (str != NULL) free(str);
	}
	while (status != MATCH_EOF);

	fclose(pfile);
	fclose(aslfile);

	unlink(_PATH_ASL_OUT);
	rename(pname, _PATH_ASL_OUT);
	free(pname);
	unlink(_PATH_ASL_PRUNE);
	aslfile = fopen(_PATH_ASL_OUT, "a");

	n = NULL;
	for (p = pquery.tqh_first; p != NULL; p = n)
	{
		n = p->entries.tqe_next;

		if (p->query != NULL) asl_free(p->query);

		TAILQ_REMOVE(&pquery, p, entries);
		free(p);
	}

	asldebug("syslogd: prune %d records in, %d records out\n", incount, outcount);

	return 0;
}

asl_msg_t *
asl_in_getmsg(int fd)
{
	char *out;
	asl_msg_t *m;
	uint32_t len, n;
	char ls[16];

	n = read(fd, ls, 11);
	if (n < 11)
	{
		if (n <= 0)
		{
			asldebug("%s: read error (len): %s\n", MY_ID, strerror(errno));
			if (errno != EINTR)
			{
				close(fd);
				aslevent_removefd(fd);
				return NULL;
			}
		}

		return NULL;
	}

	len = atoi(ls);
	asldebug("%s: expecting message length %d bytes\n", MY_ID, len);
	out = malloc(len);
	if (out == NULL) return NULL;

	n = read(fd, out, len);
	if (n < len)
	{
		if (n <= 0)
		{
			asldebug("%s: read error (body): %s\n", MY_ID, strerror(errno));
			if (errno != EINTR)
			{
				close(fd);
				aslevent_removefd(fd);
				free(out);
				return NULL;
			}
		}
	}

	m = asl_msg_from_string(out);
	free(out);
	return m;
}

asl_msg_t *
asl_in_acceptmsg(int fd)
{
	int clientfd;

	asldebug("%s: accepting message\n", MY_ID);
	clientfd = accept(fd, NULL, 0);
	if (clientfd < 0)
	{
		asldebug("%s: error accepting socket fd %d: %s\n", MY_ID, fd, strerror(errno));
		return NULL;
	}

	if (fcntl(clientfd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(clientfd);
		clientfd = -1;
		asldebug("%s: couldn't set O_NONBLOCK for fd %d: %s\n", MY_ID, clientfd, strerror(errno));
		return NULL;
	}

	aslevent_addfd(clientfd, asl_in_getmsg, NULL, NULL);
	return NULL;
}

int
aslmod_sendmsg(asl_msg_t *msg, const char *outid)
{
	const char *vlevel;
	char *mstr;
	uint32_t n, lmask;
	int status, x, level;

	if (aslfile == NULL) return -1;

	/* set up com.apple.syslog.asl_filter */
	if (filter_token == -1)
	{
		status = notify_register_check(NOTIFY_SYSTEM_ASL_FILTER, &filter_token);
		if (status != NOTIFY_STATUS_OK)
		{
			filter_token = -1;
		}
		else
		{
			status = notify_check(filter_token, &x);
			if (status == NOTIFY_STATUS_OK) status = notify_set_state(filter_token, asl_log_filter);
			if (status != NOTIFY_STATUS_OK)
			{
				notify_cancel(filter_token);
				filter_token = -1;
			}
		}
	}

	if (filter_token >= 0)
	{
		x = 1;
		status = notify_check(filter_token, &x);
		if ((status == NOTIFY_STATUS_OK) && (x == 1))
		{
			x = asl_log_filter;
			status = notify_get_state(filter_token, &x);
			if ((status == NOTIFY_STATUS_OK) && (x != 0)) asl_log_filter = x;
		}
	}

	vlevel = asl_get(msg, ASL_KEY_LEVEL);
	level = 7;
	if (vlevel != NULL) level = atoi(vlevel);
	lmask = ASL_FILTER_MASK(level);
	if ((lmask & asl_log_filter) == 0) return 0;

	mstr = asl_msg_to_string(msg, &n);
	if (mstr != NULL)
	{
		fprintf(aslfile, "%s\n", mstr);
		fflush(aslfile);
		free(mstr);
	}

	return 0;
}

int
asl_in_init(void)
{
	struct sockaddr_un sun;
	int rbufsize;
	int len;

	asldebug("%s: init\n", MY_ID);
	if (sock >= 0) return sock;

	unlink(_PATH_ASL_IN);
	sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
	{
		asldebug("%s: couldn't create socket for %s: %s\n", MY_ID, _PATH_ASL_IN, strerror(errno));
		return -1;
	}

	asldebug("%s: creating %s for fd %d\n", MY_ID, _PATH_ASL_IN, sock);

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, _PATH_ASL_IN);

	len = sizeof(struct sockaddr_un);
	if (bind(sock, (struct sockaddr *)&sun, len) < 0)
	{
		asldebug("%s: couldn't bind socket %d for %s: %s\n", MY_ID, sock, _PATH_ASL_IN, strerror(errno));
		close(sock);
		sock = -1;
		return -1;
	}

	rbufsize = 128 * 1024;
	len = sizeof(rbufsize);

	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rbufsize, len) < 0)
	{
		asldebug("%s: couldn't set receive buffer size for %s: %s\n", MY_ID, sock, _PATH_ASL_IN, strerror(errno));
		close(sock);
		sock = -1;
		return -1;
	}
	
	if (listen(sock, SOMAXCONN) < 0)
	{
		asldebug("%s: couldn't listen on socket %d for %s: %s\n", MY_ID, sock, _PATH_ASL_IN, strerror(errno));
		close(sock);
		sock = -1;
		unlink(_PATH_ASL_IN);
		return -1;
	}

	if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
	{
		asldebug("%s: couldn't set O_NONBLOCK for socket %d (%s): %s\n", MY_ID, sock, _PATH_ASL_IN, strerror(errno));
		close(sock);
		sock = -1;
		return -1;
	}

	chmod(_PATH_ASL_IN, 0666);

	/* Add logger routine for main output file */
	aslfile = fopen(_PATH_ASL_OUT, "a");
	if (aslfile != NULL)
	{
		query = asl_new(ASL_TYPE_QUERY);
		aslevent_addmatch(query, MY_ID);
		aslevent_addoutput(aslmod_sendmsg, MY_ID);
	}

	return aslevent_addfd(sock, asl_in_acceptmsg, NULL, NULL);
}

int
asl_in_reset(void)
{
	return 0;
}

int
asl_in_close(void)
{
	if (sock < 0) return 1;

	if (filter_token >= 0) notify_cancel(filter_token);
	filter_token = -1;
	asl_log_filter = 0;

	asl_free(query);
	close(sock);
	if (aslfile != NULL) fclose(aslfile);
	unlink(_PATH_ASL_IN);

	return 0;
}
