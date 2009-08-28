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
#define MY_ID "asl_in"

static int sock = -1;

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
			asl_client_count_decrement();

			close(fd);
			aslevent_removefd(fd);
			return NULL;
		}

		if (n < 0)
		{
			asldebug("%s: read error (len=%d): %s\n", MY_ID, n, strerror(errno));
			if (errno != EINTR)
			{
				asl_client_count_decrement();

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
				asl_client_count_decrement();

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
asl_in_new_connection(int fd)
{
	int clientfd;

	asldebug("%s: accepting connection\n", MY_ID);
	clientfd = accept(fd, NULL, 0);
	if (clientfd < 0)
	{
		asldebug("%s: error connecting socket fd %d: %s\n", MY_ID, fd, strerror(errno));
		return NULL;
	}

	if (fcntl(clientfd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(clientfd);
		clientfd = -1;
		asldebug("%s: couldn't set O_NONBLOCK for fd %d: %s\n", MY_ID, clientfd, strerror(errno));
		return NULL;
	}

	asl_client_count_increment();

	aslevent_addfd(SOURCE_ASL_SOCKET, clientfd, ADDFD_FLAGS_LOCAL, asl_in_getmsg, NULL, NULL);
	return NULL;
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
		asldebug("%s: launchd dict is NULL\n", MY_ID);
		return -1;
	}

	sockets_dict = launch_data_dict_lookup(global.launch_dict, LAUNCH_JOBKEY_SOCKETS);
	if (sockets_dict == NULL)
	{
		asldebug("%s: launchd lookup of LAUNCH_JOBKEY_SOCKETS failed\n", MY_ID);
		return -1;
	}

	fd_array = launch_data_dict_lookup(sockets_dict, ASL_SOCKET_NAME);
	if (fd_array == NULL)
	{
		asldebug("%s: launchd lookup of ASL_SOCKET_NAME failed\n", MY_ID);
		return -1;
	}

	len = launch_data_array_get_count(fd_array);
	if (len <= 0)
	{
		asldebug("%s: launchd fd array is empty\n", MY_ID);
		return -1;
	}

	if (len > 1)
	{
		asldebug("%s: warning! launchd fd array has %d sockets\n", MY_ID, len);
	}

	fd_dict = launch_data_array_get_index(fd_array, 0);
	if (fd_dict == NULL)
	{
		asldebug("%s: launchd file discriptor array element 0 is NULL\n", MY_ID);
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

	return aslevent_addfd(SOURCE_ASL_SOCKET, sock, ADDFD_FLAGS_LOCAL, asl_in_new_connection, NULL, NULL);
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

	close(sock);
	unlink(_PATH_ASL_IN);

	return 0;
}
