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

/* "@(#)zip_lolist.c: 2.0, 1.12; 2/10/93; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	zip_locallist.c
 *
 * Facility:	AppleTalk Zone Information Protocol Library Interface
 *
 * History:
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <mach/boolean.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/atp.h>
#include <netat/zip.h>
#include <netat/at_var.h>

#include "at_proto.h"

#define	SET_ERRNO(e) errno = e

#define TOTAL_ALLOWED ATP_DATA_SIZE-sizeof(at_nvestr_t)

extern int zip_getzonesfrombridge(char *ifName, int *context, 
				  u_char *zones, int size, int local);


/* in_list() returns 1 if ifName is on interface list nameList,
   and 0, if it's not.  in_list() returns 0 if IfName is NULL or empty.
*/

int in_list(ifName, nameList)
     char *ifName;
     at_ifnames_t *nameList;
{
	int ifno;

	if (!ifName || !ifName[0])
	    return(0);

	/* find matching ifno */
	for (ifno=0; ifno < IF_TOTAL_MAX; ifno++) {
	     if (!strcmp(ifName, (char *)nameList->at_if[ifno])) {
		return(1);
	     }
	}
	return(0);
}

/* zip_getlocalzones() will return the zone count on success, 
   and -1 on failure. */

int zip_getlocalzones(
	char *ifName,
		/* If ifName is a null pointer (ZIP_DEF_INTERFACE) the default
		   interface will be used.
		*/
	int *context,
		/* *context should be set to ZIP_FIRST_ZONE for the first call.
		   The returned value may be used in the next call, unless it
		   is equal to ZIP_NO_MORE_ZONES.
		*/
	u_char *zones,
		/* Pointer to the beginning of the "zones" buffer.
		   Zone data returned will be a sequence of at_nvestr_t
		   Pascal-style strings, as it comes back from the 
		   ZIP_GETLOCALZONES request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
)
{
	int start = *context;
	int if_id;
	at_state_t      global_state;
	at_nvestr_t	*pz;
	int total=0, i, skipped = 0;
	zone_usage_t	ifz;

	if (start < ZIP_FIRST_ZONE) {
		return(-1);
	}
	if (0 > (if_id = socket(AF_APPLETALK, SOCK_RAW, 0)))
		return(-1);

	if (ioctl(if_id, AIOCGETSTATE, (caddr_t)&global_state)) {
		fprintf(stderr,"error getting Appletalk mode\n");
		(void)close (if_id);
		return(-1);
	}
	if (!(global_state.flags & AT_ST_ROUTER) &&
	    !(global_state.flags & AT_ST_MULTIHOME)) {
		(void)close (if_id);
		return(zip_getzonesfrombridge(ifName, context, zones, 
					      size, TRUE));
	} else { /* multihome or router mode */

		pz = (at_nvestr_t *)zones;
		ifz.zone_index = i = start-1;
		total=0;
		for ( ; total < TOTAL_ALLOWED; ifz.zone_index = ++i) {
			if (ioctl(if_id, AIOCGETZNUSAGE, (caddr_t)&ifz))
			    if (errno != ENOENT) {
				fprintf(stderr,"error getting Appletalk local zones\n");
				(void)close (if_id);
				return(-1);
			    } else {
				*context = ZIP_NO_MORE_ZONES;
				(void)close (if_id);
				return(i-start+1-skipped);
			    }
			/* if we're looking for a certain interface */
			if (ifName && strlen(ifName)) {
				/* if it's not for this interface, get the 
				   next local zone */
				if (!in_list(ifName, &ifz.zone_iflist)) {
					skipped++;
				  	continue;
				}
			}
			*pz = ifz.zone_name; 
			total += (pz->len + 1);
			pz = (at_nvestr_t*)((caddr_t)pz + pz->len +1);
		}
		*context = i+2;
		(void)close (if_id);
		return(i-start+2-skipped);
	}
	(void)close (if_id);
	return(-1);
}
