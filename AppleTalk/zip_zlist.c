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
 *	Copyright (c) 1988, 1989, 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

/* "@(#)zip_zlist.c: 2.0, 1.12; 2/10/93; Copyright 1988-89, Apple Computer, Inc." */

#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <mach/boolean.h>

#include <sys/errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/sockio.h>
#include <net/if.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/zip.h>
#include <netat/atp.h>
#include <netat/at_var.h>

#include "at_proto.h"

#define	SET_ERRNO(e) errno = e

static at_inet_t	abridge = { 0, 0, 0 };

int zip_getzonesfrombridge(
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
		   ZIP_GETLOCALZONES / ZIP_GETZONELIST request sent over ATP 
		*/
	int size,
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
	int local
		/* TRUE for local zones, FALSE for all zones */
)
{
	int start = *context;
	int fd;
	int userdata;
	at_if_cfg_t cfg;
	u_char *puserdata = (u_char *)&userdata;
	at_inet_t dest;
	at_retry_t retry;
	at_resp_t resp;

	if (start < ZIP_FIRST_ZONE) {
		return(-1);
	}

	if (start == 1) {
		/* This is the first call, get the bridge node id */
		if (ifName)
			strncpy(cfg.ifr_name, ifName, sizeof(cfg.ifr_name));
		else
			cfg.ifr_name[0] = '\0'; /* use the default interface */
		if ((fd = socket(AF_APPLETALK, SOCK_RAW, 0)) < 0) {
			SET_ERRNO(EINVAL);
			return(-1);
		}
		if (ioctl(fd, AIOCGETIFCFG, (caddr_t)&cfg) < 0) {
			SET_ERRNO(ENETUNREACH);
			return(-1);
		}
		close(fd);
		abridge.node = cfg.router.s_node;
		abridge.socket = 0;
		abridge.net = cfg.router.s_net;
		if (abridge.node == 0) { /* no router */
			at_nvestr_t *zone;
			zone = (at_nvestr_t *)zones;
			zone->len = 1;
			zone->str[0] = '*';
			zone->str[1] = '\0';
			return(-1);
		}
	} else {
		/* This isn't the 1st call, make sure we use the same ABridge */
		if (abridge.node == 0) {
			SET_ERRNO(EINVAL); /* Never started with start == 1 */
			return(-1);
		}
	}

	fd = atp_open(NULL);
	if (fd < 0)
		return(-1);

	dest.net = abridge.net;
	dest.node = abridge.node;
	dest.socket = ZIP_SOCKET;
	puserdata[0] = (local)? ZIP_GETLOCALZONES: ZIP_GETZONELIST;
	puserdata[1] = 0;
	*(short *)(&puserdata[2]) = start;
	resp.bitmap = 0x01;
	resp.resp[0].iov_base = zones;
	resp.resp[0].iov_len = ATP_DATA_SIZE;
	retry.interval = 2;
	retry.retries = 5;

/*
	printf("%s sent; start = %d\n", 
		       (local)? "ZIP_GETLOCALZONES": "ZIP_GETZONELIST", 
		       *(short *)(&puserdata[2]));
*/
	if (atp_sendreq(fd, &dest, 0, 0, userdata, 0, 0, 0,
			&resp, &retry, 0) >= 0) {
		/* Connection established okay, just for the sake of our
		* sanity, check the other fields in the packet
		*/
		puserdata = (u_char *)&resp.userdata[0];
		if (puserdata[0] != 0) {
			*context = ZIP_NO_MORE_ZONES;
			abridge.node = 0;
			abridge.net = 0;		
		} else {
			*context = (start + *(short *)(&puserdata[2]));
		}
/*
		printf("%s returned %d entries\n", 
		       (local)? "ZIP_GETLOCALZONES": "ZIP_GETZONELIST", 
		       *(short *)(&puserdata[2]));
*/
		atp_close(fd);
		return(*(short *)(&puserdata[2]));
	} 
	atp_close(fd);
	return(-1);
}

/* zip_getzonelist() will return the zone count on success, 
   and -1 on failure. */

int zip_getzonelist(
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
		   ZIP_GETZONELIST request sent over ATP 
		*/
	int size
		/* Length of the "zones" buffer; must be at least 
		   (ATP_DATA_SIZE+1) bytes in length.
		*/
)
{
	return(zip_getzonesfrombridge(ifName, context, zones, size, FALSE));
}
