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

/* "@(#)zip_myzone.c: 2.0, 1.11; 11/2/92; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	zip_myzone.c
 *
 * Facility:	AppleTalk Zone Information Protocol Library Interface
 *
 * Author:	Gregory Burns, Creation Date: Jun-24-1988
 *
 * History:
 * X01-001	Gregory Burns	24-Jun-1988
 *	 	Initial Creation.
 *
 */

#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <sys/select.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/at_var.h>

/* zip_getmyzone() will return 0 on success, and -1 on failure. */

int zip_getmyzone(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	at_nvestr_t *zone
)
{
	at_if_cfg_t cfg;
	int fd;

        if ((fd = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0) 
		return(-1);

	if (!ifName) 
		cfg.ifr_name[0] = '\0'; /* use the default interface */
	else
		strncpy(cfg.ifr_name, ifName, sizeof(cfg.ifr_name));

	if (ioctl(fd, AIOCGETIFCFG, (caddr_t)&cfg) < 0) {
		close(fd);
		return(-1);
	}

	zone->len = cfg.zonename.len;
	strncpy(zone->str, cfg.zonename.str, zone->len);

	close(fd);
	return (0);
}
