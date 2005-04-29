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
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "daemon.h"

#define forever for(;;)

#define MY_ID "klog_in"
#define MAXLINE 4096

static int kfd = -1;

static int kx = 0;
static char kline[MAXLINE + 1];

asl_msg_t *
klog_in_acceptmsg(int fd)
{
	int n;
	char c;

	n = read(fd, &c, 1);

	while ((n == 1) && (c != '\n'))
	{
		if (kx < MAXLINE) kline[kx++] = c;
		n = read(fd, &c, 1);
	}

	if (kx == 0) return NULL;

	n = kx - 1;
	kline[kx] = '\0';
	kx = 0;

	return asl_syslog_input_convert(kline, n, NULL, 1);
}

int
klog_in_init(void)
{
	asldebug("%s: init\n", MY_ID);
	if (kfd >= 0) return kfd;

	kfd = open(_PATH_KLOG, O_RDONLY, 0);
	if (kfd < 0)
	{
		asldebug("%s: couldn't open %s: %s\n", MY_ID, _PATH_KLOG, strerror(errno));
		return -1;
	}

	if (fcntl(kfd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(kfd);
		kfd = -1;
		asldebug("%s: couldn't set O_NONBLOCK for fd %d (%s): %s\n", MY_ID, kfd, _PATH_KLOG, strerror(errno));
		return -1;
	}

	return aslevent_addfd(kfd, klog_in_acceptmsg, NULL, NULL);
}

int
klog_in_reset(void)
{
	return 0;
}

int
klog_in_close(void)
{
	if (kfd < 0) return 1;

	close(kfd);
	kfd = -1;

	return 0;
}
