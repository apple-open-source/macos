/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 2004 Apple Computer, Inc.  All Rights
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

#define MY_ID "bsd_in"
#define MAXLINE 1024

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
	return asl_syslog_input_convert(line, n, NULL, 0);
}

int
bsd_in_init(void)
{
	struct sockaddr_un sun;
	int rbufsize;
	int len;

	asldebug("%s: init\n", MY_ID);
	if (sock >= 0) return sock;

	unlink(_PATH_SYSLOG_IN);
	sock = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0)
	{
		asldebug("%s: couldn't create socket for %s: %s\n", MY_ID, _PATH_SYSLOG_IN, strerror(errno));
		return -1;
	}

	asldebug("%s: creating %s for fd %d\n", MY_ID, _PATH_SYSLOG_IN, sock);

	memset(&sun, 0, sizeof(sun));
	sun.sun_family = AF_UNIX;
	strcpy(sun.sun_path, _PATH_SYSLOG_IN);

	len = sizeof(struct sockaddr_un);
	if (bind(sock, (struct sockaddr *)&sun, len) < 0)
	{
		asldebug("%s: couldn't bind socket %d for %s: %s\n", MY_ID, sock, _PATH_SYSLOG_IN, strerror(errno));
		close(sock);
		sock = -1;
		return -1;
	}

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

	chmod(_PATH_SYSLOG_IN, 0666);

	return aslevent_addfd(sock, bsd_in_acceptmsg, NULL, NULL);
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
