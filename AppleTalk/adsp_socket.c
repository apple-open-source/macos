/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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
/*
 *	Copyright (c) 1994, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 *
 *  Change Log:
 *      Created November 22, 1994 by Tuyen Nguyen
 *
 */

#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>


#include "at_proto.h"


#define	SET_ERRNO(e) errno = e


/*
 * Name: ADSPaccept
 */
int
ADSPaccept(int fd, void *name, int *namelen)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPbind
 */
int
ADSPbind(int fd, void *name, int namelen)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPclose
 */
int
ADSPclose(int fd)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPconnect
 */
int
ADSPconnect(int fd, void *name, int namelen)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPdisconnect
 */
int
ADSPdisconnect(int fd, int abort)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPfwdreset
 */
int
ADSPfwdreset(int fd)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPgetpeername
 */
int
ADSPgetpeername(int fd, void *name, int *namelen)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPgetsockname
 */
int
ADSPgetsockname(int fd, void *name, int *namelen)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPgetsockopt
 */
int
ADSPgetsockopt(int fd, int level, int optname, char *optval, int *optlen)
{
	SET_ERRNO(EOPNOTSUPP);
	return -1;
}

/*
 * Name: ADSPlisten
 */
int
ADSPlisten(int fd, int backlog)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPrecv
 */
int
ADSPrecv(int fd, char *buf, int len, int flags)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPrecvfrom
 */
int
ADSPrecvfrom(int fd, char *buf, int len, int flags, void *from, int fromlen)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPsendto
 */
int
ADSPsendto(int fd, char *buf, int len, int flags, void *to, int tolen)
{
	SET_ERRNO(ENXIO);
	return -1;
} /* ADSPsendto */

/*
 * Name: ADSPsend
 */
int
ADSPsend(int fd, char *buf, int len, int flags)
{
	return ADSPsendto(fd, buf, len, flags, NULL, 0);
}

/*
 * Name: ADSPsetsockopt
 */
int
ADSPsetsockopt(int fd, int level, int optname, char *optval, int optlen)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ADSPsocket
 */
int
ADSPsocket(int domain, int type, int protocol)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ASYNCread
 */
int
ASYNCread(int fd, char *buf, int len)
{
	SET_ERRNO(ENXIO);
	return -1;
}

/*
 * Name: ASYNCread_complete
 */
int
ASYNCread_complete(int fd, char *buf, int len)
{
	SET_ERRNO(ENXIO);
	return -1;
}
