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
#include <netinet/in.h>
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
#include "daemon.h"

#define forever for(;;)

#define MY_ID "udp_in"
#define MAXLINE 4096

#define MAXSOCK 16
static int nsock = 0;
static int ufd[MAXSOCK];

static char uline[MAXLINE + 1];

#define FMT_LEGACY 0
#define FMT_ASL 1

asl_msg_t *
udp_convert(int fmt, char *s, int len, char *from)
{
	char *out;
	asl_msg_t *m;

	out = NULL;
	m = NULL;

	if (fmt == FMT_ASL)
	{
		m = asl_msg_from_string(s);
		if (from != NULL) asl_set(m, ASL_KEY_HOST, from);
		return m;
	}

	return asl_syslog_input_convert(uline, len, from, 0);
}

asl_msg_t *
udp_in_acceptmsg(int fd)
{
	int format, status, x, fromlen;
	size_t off;
	ssize_t len;
	struct sockaddr_storage from;
	char fromstr[64], *r, *p;
	struct sockaddr_in *s4;
	struct sockaddr_in6 *s6;

	fromlen = sizeof(struct sockaddr_storage);
	memset(&from, 0, fromlen);

	len = recvfrom(fd, uline, MAXLINE, 0, (struct sockaddr *)&from, &fromlen);
	if (len <= 0) return NULL;

	fromstr[0] = '\0';
	r = NULL;

	if (from.ss_family == AF_INET)
	{
		s4 = (struct sockaddr_in *)&from;
		inet_ntop(from.ss_family, &(s4->sin_addr), fromstr, 64);
		r = fromstr;
		asldebug("%s: recvfrom %s len %d\n", MY_ID, fromstr, len);
	}
	else if (from.ss_family == AF_INET6)
	{
		s6 = (struct sockaddr_in6 *)&from;
		inet_ntop(from.ss_family, &(s6->sin6_addr), fromstr, 64);
		r = fromstr;
		asldebug("%s: recvfrom %s len %d\n", MY_ID, fromstr, len);
	}

	uline[len] = '\0';

	p = strrchr(uline, '\n');
	if (p != NULL) *p = '\0';


	/*
	 * Determine if the input is "old" syslog format or new ASL format.
	 * Old format lines should start with "<", but they can just be
	 * straight text.  ASL input starts with a length (10 bytes)
	 * followed by a space and a '['.
	 */
	format = FMT_LEGACY;
	off = 0;

	if ((uline[0] != '<') && (len > 11))
	{
		status = sscanf(uline, "%d ", &x);
		if (status == 1) 
		{
			if ((uline[10] == ' ') && (uline[11] == '['))
			{
				format = FMT_ASL;
				off = 11;
			}
		}
	}

	return udp_convert(format, uline+off, len-off, r);
}

int
udp_in_init(void)
{
	struct addrinfo hints, *gai, *ai;
	int status, i;

	asldebug("%s: init\n", MY_ID);
	if (nsock > 0) return 0;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	status = getaddrinfo(NULL, "syslog", &hints, &gai);
	if (status != 0) return -1;

	for (ai = gai; (ai != NULL) && (nsock < MAXSOCK); ai = ai->ai_next)
	{
		ufd[nsock] = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (ufd[nsock] < 0)
		{
			asldebug("%s: socket: %s\n", MY_ID, strerror(errno));
			continue;
		}

		if (bind(ufd[nsock], ai->ai_addr, ai->ai_addrlen) < 0)
		{
			asldebug("%s: bind: %s\n", MY_ID, strerror(errno));
			close(ufd[nsock]);
			continue;
		}

		nsock++;
	}

	freeaddrinfo(gai);

	if (nsock == 0)
	{
		asldebug("%s: no input sockets\n", MY_ID);
		return -1;
	}

	for (i = 0; i < nsock; i++) aslevent_addfd(ufd[i], udp_in_acceptmsg, NULL, NULL);
	return 0;
}

int
udp_in_reset(void)
{
	return 0;
}

int
udp_in_close(void)
{
	int i;

	if (nsock == 0) return 1;

	for (i = 0; i < nsock; i++)
	{
		close(ufd[i]);
		ufd[i] = -1;
	}

	nsock = 0;

	return 0;
}
