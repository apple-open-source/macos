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

/*
  11-4-93   jjs  added back standard A/UX atp_sendreq() call
  10-6-94   tan  added nowait option
  8-14-95   tan  performance improvement
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>


#include "at_proto.h"

#define	SET_ERRNO(e) errno = e


int	at_send_nowait_request(fd,treq_buff)
	int        	fd;
	at_atpreq	*treq_buff;
{
	SET_ERRNO(ENXIO);
	return (-1);
}

int	at_send_request(fd,treq_buff)
	int        	fd;
	at_atpreq	*treq_buff;
{
	SET_ERRNO(ENXIO);
	return (-1);
}




int
atp_sendreq (fd,dest,reqbuf,reqlen,userdata,xo,xo_relt,tid,resp,retry,nowait)
	int		fd;
	at_inet_t	*dest;
	char		*reqbuf;
	int		reqlen, userdata, xo, xo_relt;
	u_short		*tid;
	at_resp_t 	*resp;
	at_retry_t	*retry;
	int		nowait;
{
	SET_ERRNO(ENXIO);
	return (-1);
}	
