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

#include <unistd.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/atp.h>

#include "at_proto.h"

#define RSP_ARRIVED(p) (*((char *)p))

int
atp_getresp (fd,tid,resp)
	int		fd;
	u_short		*tid;
	at_resp_t 	*resp;
{
	int		status, i;
	u_char		bitmap;
	struct atpBDS *newbdsp;
	int newcount;
	char	   	buff[640];

	/*
	 * get and check the event byte for response indicator
	 */
	while (1) {
		if ((i=read(fd, buff, 1)) == -1)
			return -1;
		if (i == 1) {
			if (RSP_ARRIVED(buff))
				break;
		}
	}

	newbdsp = (struct atpBDS *)buff;
	for (i = 0; i < ATP_TRESP_MAX; i++) {
		UAL_ASSIGN(newbdsp[i].bdsBuffAddr, resp->resp[i].iov_base);
		UAS_ASSIGN(newbdsp[i].bdsBuffSz, resp->resp[i].iov_len);
	}
	if ((status = ATPgetrsp(fd, newbdsp)) == -1)
		return -1;

	/*
	 * return pertinent info to the caller
	 */
	*tid = (u_short)status;
	bitmap = resp->bitmap;
	newbdsp = (struct atpBDS *)buff;
	newcount = UAS_VALUE(newbdsp->bdsBuffSz);

	if (newcount > 0) {
		resp->bitmap = (1 << newcount) - 1;

		for (i = 0; i < newcount; i++) {
			if ((bitmap & (1 << i))) {
				UAL_UAL(((long *)&resp->userdata[i]), newbdsp->bdsUserData);
				resp->resp[i].iov_len = UAS_VALUE(newbdsp->bdsDataSz);
			}
			newbdsp++;
		}
	}

	return (0);
}
