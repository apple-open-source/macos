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
#include <sys/fcntl.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "daemon.h"

#define BSD_SOCKET_NAME "BSDSystemLogger"
#define MY_ID "bsd_in"
#define MAXLINE 4096

static int sock = -1;

asl_msg_t *
bsd_in_acceptmsg(int fd)
{
	uint32_t len;
	int n;
	char line[MAXLINE + 1];
	struct sockaddr_un sun;

	len = sizeof(struct sockaddr_un);
	n = recvfrom(fd, line, MAXLINE, 0, (struct sockaddr *)&sun, &len);

	if (n <= 0) return NULL;

	line[n] = '\0';

	return asl_input_parse(line, n, NULL, 0);
}

int
bsd_in_init(void)
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

	fd_array = launch_data_dict_lookup(sockets_dict, BSD_SOCKET_NAME);
	if (fd_array == NULL)
	{
		asldebug("%s: laucnchd lookup of BSD_SOCKET_NAME failed\n", MY_ID);
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
		asldebug("%s: couldn't set receive buffer size for socket %d (%s): %s\n", MY_ID, sock, _PATH_SYSLOG_IN, strerror(errno));
		close(sock);
		sock = -1;
		return -1;
	}

	if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0)
	{
		asldebug("%s: couldn't set O_NONBLOCK for socket %d (%s): %s\n", MY_ID, sock, _PATH_SYSLOG_IN, strerror(errno));
		close(sock);
		sock = -1;
		return -1;
	}

	return aslevent_addfd(sock, ADDFD_FLAGS_LOCAL, bsd_in_acceptmsg, NULL, NULL);
}

int
bsd_in_reset(void)
{
	return 0;
}

int
bsd_in_close(void)
{
	if (sock < 0) return 1;

	close(sock);
	unlink(_PATH_SYSLOG_IN);
	return 0;
}
