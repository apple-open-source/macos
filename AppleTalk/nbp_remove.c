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
 *	Copyright (c) 1988, 1989, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 *
 */

/* "@(#)nbp_remove.c: 2.0, 1.5; 9/8/89; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	nbp_remove.c
 *
 * Facility:	AppleTalk Name Binding Protocol Library Interface
 *
 * Author:	Gregory Burns, Creation Date: Jul-14-1988
 *
 * History:
 * X01-001	Gregory Burns	14-Jul-1988
 *	 	Initial Creation.
 *
 */

#include <unistd.h>
#include <stdio.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/at_var.h>
#include <netat/nbp.h>

#define	SET_ERRNO(e) errno = e

int nbp_remove (entity, fd)
	at_entity_t	*entity;
	int		fd;	/* not currently used */
{
	int		if_id;
	at_nbp_reg_t    reg;

	if (!entity) {
		SET_ERRNO(EINVAL);
		return(-1);
	}

	bzero((caddr_t)&reg, sizeof(reg));
	reg.name = *entity;

	if ((if_id = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0)
		return(-1);

	if ((ioctl(if_id, AIOCNBPREMOVE, (caddr_t)&reg)) < 0) {
		SET_ERRNO(EADDRNOTAVAIL);
		(void)close(if_id);
		return(-1);
	}

	(void)close(if_id);
	return(0);
}	
