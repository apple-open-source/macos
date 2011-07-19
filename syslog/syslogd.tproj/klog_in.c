/*
 * Copyright (c) 2004-2010 Apple Inc. All rights reserved.
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

static int kx = 0;
static char kline[MAXLINE + 1];

aslmsg
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

	return asl_input_parse(kline, n, NULL, SOURCE_KERN);
}

int
klog_in_init(void)
{
	asldebug("%s: init\n", MY_ID);
	if (global.kfd >= 0) return global.kfd;

	global.kfd = open(_PATH_KLOG, O_RDONLY, 0);
	if (global.kfd < 0)
	{
		asldebug("%s: couldn't open %s: %s\n", MY_ID, _PATH_KLOG, strerror(errno));
		return -1;
	}

	if (fcntl(global.kfd, F_SETFL, O_NONBLOCK) < 0)
	{
		close(global.kfd);
		global.kfd = -1;
		asldebug("%s: couldn't set O_NONBLOCK for fd %d (%s): %s\n", MY_ID, global.kfd, _PATH_KLOG, strerror(errno));
		return -1;
	}

	return aslevent_addfd(SOURCE_KERN, global.kfd, ADDFD_FLAGS_LOCAL, klog_in_acceptmsg, NULL, NULL);
}

int
klog_in_close(void)
{
	if (global.kfd < 0) return 1;

	aslevent_removefd(global.kfd);
	close(global.kfd);
	global.kfd = -1;

	return 0;
}

int
klog_in_reset(void)
{
	return 0;
}
