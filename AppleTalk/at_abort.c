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
 *	Copyright (c) 1988 - 1994 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 */

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/atp.h>

#include "at_proto.h"

int
atp_abort (fd,dest,tid)
	int		fd;
	at_inet_t 	*dest;
	u_short		tid;
{
	int len;
	unsigned int tr_tid;
	at_ddp_t *ddp;
	at_atp_t  *atp;
	char buff[TOTAL_ATP_HDR_SIZE];

  if (dest != (at_inet_t *)0) {
	/*
	 *	abort an ATP reponse transaction
	 */
	ddp = (at_ddp_t *)buff;
	*(u_short *)&ddp->src_net = *(u_short *)&dest->net;
	ddp->src_node = dest->node;
	ddp->src_socket = dest->socket;
	atp = ATP_ATP_HDR(ddp);
	*(u_short *)&atp->tid = tid;
	len = sizeof(buff);
	return at_send_to_dev(fd, AT_ATP_RELEASE_RESPONSE, buff, &len);

  } else {
	/*
	 *	abort an ATP request transaction
	 */
	tr_tid = (unsigned int)tid;
	len = sizeof(tr_tid);
	return at_send_to_dev(fd, AT_ATP_CANCEL_REQUEST, &tr_tid, &len);
  }
}
