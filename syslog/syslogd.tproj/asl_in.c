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

#define ASL_SOCKET_NAME "AppleSystemLogger"
#define MY_ID "asl"

static int sock = -1;
static asl_msg_t *query = NULL;

#define MATCH_EOF -1
#define MATCH_NULL 0
#define MATCH_TRUE 1
#define MATCH_FALSE 2

extern void db_enqueue(asl_msg_t *m);

static int filter_token = -1;

asl_msg_t *
asl_in_getmsg(int fd)
{
	char *out;
	asl_msg_t *m;
	uint32_t len, n;
	char tmp[16];
	int status;
	uid_t uid;
	gid_t gid;

	n = read(fd, tmp, 11);
	if (n < 11)
	{
		if (n == 0)
		{
			close(fd);
			aslevent_removefd(fd);
			return NULL;
		}

		if (n < 0)
		{
			asldebug("%s: read error (len=%d): %s\n", MY_ID, n, strerror(errno));
			if (errno != EINTR)
			{
				close(fd);
				aslevent_removefd(fd);
				return NULL;
			}
		}

		return NULL;
	}

	len = atoi(tmp);
	if (len == 0) return NULL;

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

	asldebug("asl_in_getmsg: %s\n", (out == NULL) ? "NULL" : out);

	uid = -2;
	gid = -2;

	status = getpeereid(fd, &uid, &gid);
	m = asl_msg_from_string(out);
	if (m == NULL)
	{
		free(out);
		return NULL;
	}

	snprintf(tmp, sizeof(tmp), "%d", uid);
	asl_set(m, ASL_KEY_UID, tmp);

	snprintf(tmp, sizeof(tmp), "%d", gid);
	asl_set(m, ASL_KEY_GID, tmp);

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

	aslevent_addfd(clientfd, ADDFD_FLAGS_LOCAL, asl_in_getmsg, NULL, NULL);
	return NULL;
}

int
aslmod_sendmsg(asl_msg_t *msg, const char *outid)
{
	const char *vlevel, *facility, *sender, *ignore;
	uint32_t lmask;
	uint64_t v64;
	int status, x, level, log_me;

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
			if (status == NOTIFY_STATUS_OK)
			{
				v64 = global.asl_log_filter;
				status = notify_set_state(filter_token, v64);
			}
			if (status != NOTIFY_STATUS_OK)
			{
				notify_cancel(filter_token);
				filter_token = -1;
			}
		}
	}

	if (filter_token >= 0)
	{
		x = 0;
		status = notify_check(filter_token, &x);
		if ((status == NOTIFY_STATUS_OK) && (x == 1))
		{
			v64 = 0;
			status = notify_get_state(filter_token, &v64);
			if ((status == NOTIFY_STATUS_OK) && (v64 != 0)) global.asl_log_filter = v64;
		}
	}

	facility = asl_get(msg, ASL_KEY_FACILITY);
	sender = asl_get(msg, ASL_KEY_SENDER);

	log_me = 0;
	if ((facility != NULL) && (!strcmp(facility, "kern"))) log_me = 1;
	else if ((sender != NULL) && (!strcmp(sender, "launchd"))) log_me = 1;
	else
	{
		vlevel = asl_get(msg, ASL_KEY_LEVEL);
		level = 7;
		if (vlevel != NULL) level = atoi(vlevel);
		lmask = ASL_FILTER_MASK(level);
		if ((lmask & global.asl_log_filter) != 0) log_me = 1;
	}

	if (log_me == 1)
	{
		ignore = asl_get(msg, ASL_KEY_IGNORE);
		if ((ignore != NULL) && (!strcasecmp(ignore, "yes"))) log_me = 0;
	}

	if (log_me == 1) db_enqueue(msg);

	return 0;
}

int
asl_in_init(void)
{
	int rbufsize;
	int len;
	launch_data_t sockets_dict, fd_array, fd_dict;

	asldebug("%s: init\n", MY_ID);
	if (sock >= 0) return sock;
	if (global.launch_dict == NULL)
	{
		asldebug("%s: laucnchd dict is NULL\n", MY_ID);
		return -1;
	}

	sockets_dict = launch_data_dict_lookup(global.launch_dict, LAUNCH_JOBKEY_SOCKETS);
	if (sockets_dict == NULL)
	{
		asldebug("%s: laucnchd lookup of LAUNCH_JOBKEY_SOCKETS failed\n", MY_ID);
		return -1;
	}

	fd_array = launch_data_dict_lookup(sockets_dict, ASL_SOCKET_NAME);
	if (fd_array == NULL)
	{
		asldebug("%s: laucnchd lookup of ASL_SOCKET_NAME failed\n", MY_ID);
		return -1;
	}

	len = launch_data_array_get_count(fd_array);
	if (len <= 0)
	{
		asldebug("%s: laucnchd fd array is empty\n", MY_ID);
		return -1;
	}

	if (len > 1)
	{
		asldebug("%s: warning! laucnchd fd array has %d sockets\n", MY_ID, len);
	}

	fd_dict = launch_data_array_get_index(fd_array, 0);
	if (fd_dict == NULL)
	{
		asldebug("%s: laucnchd file discriptor array element 0 is NULL\n", MY_ID);
		return -1;
	}

	sock = launch_data_get_fd(fd_dict);

	rbufsize = 128 * 1024;
	len = sizeof(rbufsize);

	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rbufsize, len) < 0)
	{
		asldebug("%s: couldn't set receive buffer size for %d (%s): %s\n", MY_ID, sock, _PATH_ASL_IN, strerror(errno));
		close(sock);
		sock = -1;
		return -1;
	}

	if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
	{
		asldebug("%s: couldn't set O_NONBLOCK for socket %d (%s): %s\n", MY_ID, sock, _PATH_ASL_IN, strerror(errno));
		close(sock);
		sock = -1;
		return -1;
	}

	query = asl_new(ASL_TYPE_QUERY);
	aslevent_addmatch(query, MY_ID);
	aslevent_addoutput(aslmod_sendmsg, MY_ID);

	return aslevent_addfd(sock, ADDFD_FLAGS_LOCAL, asl_in_acceptmsg, NULL, NULL);
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
	global.asl_log_filter = 0;

	asl_free(query);
	close(sock);
	unlink(_PATH_ASL_IN);

	return 0;
}
