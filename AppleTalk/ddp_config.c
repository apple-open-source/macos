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
 *	Copyright (c) 1988, 1989 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

/* "@(#)ddp_config.c: 2.0, 1.5; 7/28/89; Copyright 1988-89, Apple Computer, Inc." */

/* Title:	ddp_cfg.c
 *
 * Facility:	AppleTalk DDP Library Interface
 *
 * Author:	Kumar Vora
 *
 */

#include <unistd.h>

#include <sys/param.h>
#include <sys/socket.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>

#include "at_proto.h"

/* *** this will be replaced by a getsockopt() *** */
int ddp_config(fd, node)
     int fd;
     ddp_addr_t	*node;
{
	int dyn_fd = -1;
	int status = 0;
	int size = sizeof(ddp_addr_t);

	if (fd == -1)
	        if ((dyn_fd = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0) 
			return(-1);
    	status = at_send_to_dev((fd == -1 ? dyn_fd : fd), DDP_IOC_GET_CFG, 
				node, &size);
	if (dyn_fd != -1)
		(void)close(dyn_fd);
	return((status)? -1: 0);
}
