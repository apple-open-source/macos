/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/ioccom.h>
#include <netat/sysglue.h> /* needed for ioccmd_t */

extern int ATsocket(int protocol);
	/* Used to create an old-style (pre-BSD) AppleTalk socket */

int at_send_to_dev(fd, cmd, dp, length)
int	fd;
int	cmd;
char	*dp;
int	*length;
{
	int rval;
	ioccmd_t ioc;

	ioc.ic_cmd = cmd;
	ioc.ic_timout = -1;
	ioc.ic_len = length ? *length : 0;
	ioc.ic_dp = dp;

	if ((rval = ioctl(fd, (IOC_VOID | 0xff99), &ioc)) == -1) 
	    return -1;

	if (length)
	    *length = ioc.ic_len;

	return rval;
}

int at_open_dev(proto)
	int proto;
{
	return ATsocket(proto);
}
