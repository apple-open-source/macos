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

/*
 *	Copyright (c) 1988, 1989 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */


/* @(#)at_open.c: , 1.2; 9/10/92; Copyright 1988-89, Apple Computer, Inc. */
/*
  8-14-95   tan  performance improvement
*/

#include <stdlib.h>
#include <unistd.h>

#include <netat/appletalk.h>
#include <netat/atp.h>

#include "at_proto.h"

extern int ATsocket(int protocol);
	/* Used to create an old-style (pre-BSD) AppleTalk socket */

atp_open(socket)
	at_socket	*socket;
{
	int 		fd;
	int 		len;
	at_socket	newsock;

	newsock = socket ? *socket : 0;

	fd = ATsocket(ATPROTO_ATP);
	if (fd < 0)
		return(-1);

	len = sizeof(newsock);
	if (at_send_to_dev(fd, AT_ATP_BIND_REQ, &newsock, &len) == -1) {
		close(fd);
		return(-1);
	}
	if (socket)
		*socket = newsock;

	return(fd);
}

atp_close(fd)
	int	fd;
{
	return(close(fd));
}
