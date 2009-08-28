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
/* Title:	lap_init.c
 *
 * Facility:	Generic AppleTalk Link Access Protocol Interface
 *		(ALAP, ELAP, etc...)
 *
 * Author:	Gregory Burns, Creation Date: April-1988
 *
 ******************************************************************************
 *                                                                            *
 *        Copyright (c) 1988, 1998 Apple Computer, Inc.                       *
 *                                                                            *
 *        The information contained herein is subject to change without       *
 *        notice and  should not be  construed as a commitment by Apple       *
 *        Computer, Inc. Apple Computer, Inc. assumes no responsibility       *
 *        for any errors that may appear.                                     *
 *                                                                            *
 *        Confidential and Proprietary to Apple Computer, Inc.                *
 *                                                                            *
 ******************************************************************************
 */

/* "@(#)lap_init.c: 2.0, 1.19; 2/26/93; Copyright 1988-92, Apple Computer, Inc." */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <mach/boolean.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>

#include "at_proto.h"
#include "at_paths.h"

#define	SET_ERRNO(e) errno = e



/* save the current configuration in pram */
int at_savecfgdefaults(fd, ifName)
     int fd;
     char *ifName;
{
	SET_ERRNO(ENXIO);
	return (-1);
}

int at_getdefaultaddr(ifName, init_address)
     char *ifName;
     struct at_addr *init_address;
{
	SET_ERRNO(ENXIO);
	return (-1);
}

int at_setdefaultaddr(ifName, init_address)
     char *ifName;
     struct at_addr *init_address;
{
	SET_ERRNO(ENXIO);
	return (-1);
}

int at_getdefaultzone(ifName, zone_name)
     char *ifName;
     at_nvestr_t *zone_name;
{
	SET_ERRNO(ENXIO);
	return (-1);
}

int at_setdefaultzone(ifName, zone_name)
     char *ifName;
     at_nvestr_t *zone_name;
{
	SET_ERRNO(ENXIO);
	return (-1);
}




