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

/* "@(#)nbp_conf.c: 2.0, 1.6; 9/28/89; Copyright 1988-89, Apple Computer, Inc." */

/*
 * Title:	nbp_confirm.c
 *
 * Facility:	AppleTalk Zone Information Protocol Library Interface
 *
 * Author:	Kumar Vora, Creation Date: Mar-22-1989
 *
 * History:
 * X01-001	Kumar Vora	22-Mar-1989
 *	 	Initial Creation.
 *
 */

#include <string.h>
#include <sys/errno.h>
#include <netat/appletalk.h>
#include <netat/nbp.h>
#include <netat/zip.h>

int nbp_confirm (entity, dest, retry)
     at_entity_t	*entity;
     at_inet_t	*dest;
     at_retry_t	*retry;
{
	int		got,
			fd;
	at_nbptuple_t	buf;

	if (!valid_at_addr(dest)) {
		errno = EINVAL;
		return(-1);
	}

	got = _nbp_send_(NBP_CONFIRM, dest, entity, &buf, 1, retry);
	switch(got) {
	case 0:
	case 1:
#ifdef NOT_YET
		if (got)
			dest->socket = nbpIn->tuple[0].enu_addr.socket;
#endif
		return(got);
	default:
		return(-1);
	}
}	
