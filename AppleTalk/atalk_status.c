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
 *	Copyright (c) 1997, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 *
 */


/* @(#)atalk_status.c:  Copyright 1997, Apple Computer, Inc. */
/*
  11-21-97 Vida Amani new
*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/at_var.h>

#include <AppleTalk/at_proto.h>

/* if OTHERERROR is returned then errno may be checked for the reason */
int
checkATStack()
{
	int		s, rc=0;
	at_state_t	global_state;

	if ((s = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(NOTLOADED);
	rc = ioctl(s, AIOCGETSTATE, (caddr_t)&global_state);
	(void)close(s);

	if (rc == 0)
		if (global_state.flags & AT_ST_STARTED)
			return(RUNNING);
		else
			return(LOADED);
	else
		return(OTHERERROR);
}

