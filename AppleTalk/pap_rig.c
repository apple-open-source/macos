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
#include <fcntl.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/pap.h>

extern struct pap_state *paps[];
extern int papm[];

int	pap_read_ignore(fd)
int fd;
{
	register struct pap_state *papp;

				/* check for invalid paramaters! */
	if (fd < 0)
	    return -1;

	papp = paps[papm[fd]];
	if (papp == 0)
	    return -1;
	if ((papp->pap_ignore_id = 
	     pap_send_request(fd, papp, AT_PAP_TYPE_SEND_DATA, 1, 1)) < 0)
	    return -1;
	papp->pap_read_ignore = 1;
	return(0);
}

