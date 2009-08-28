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
 */

/* "@(#)nbp_reg.c: 2.0, 1.10; 9/27/89; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	nbp_reg.c
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
#include <stdio.h>
#include <unistd.h>
#include <mach/boolean.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>

#include <ifaddrs.h>


#include "at_proto.h"


#define	SET_ERRNO(e) errno = e

#define IFR_NEXT(ifr)   \
  ((struct ifreq *) ((char *) (ifr) + sizeof(*(ifr)) + \
      MAX(0, (int) (ifr)->ifr_addr.sa_len - (int) sizeof((ifr)->ifr_addr))))
		
/*
  nbp_reg_lookup()

  Used to make sure an NBP entity does not exist before it is registered.

  Returns 1 	if the entity already exists, 
  	  0 	if the entry does not exist
	 -1	for an error; e.g.no local zones exist

  Does the right thing in multihoming mode, namely if the zone is
  "*" (the default), it does the lookup in the default zone for
  each interface.

*/

int nbp_reg_lookup(entity, retry)
     at_entity_t     *entity;
     at_retry_t      *retry;
{
	SET_ERRNO(ENXIO);
	return (-1);
} /* nbp_reg_lookup */

int nbp_register(entity, fd, retry)
	at_entity_t	*entity;
	int		fd;
	at_retry_t	*retry;
{
	SET_ERRNO(ENXIO);
	return (-1);
} /* nbp_register */	
