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

/*
8-17-93 jjs restored lib interface removed in A/UX 3.0
*/


/* @(#)at_snd_rsp.c: 1.0, 1.5; 9/28/93; Copyright 1988-89, Apple Computer, Inc. */

#include <sys/errno.h>

#include <netat/appletalk.h>
#include <netat/atp.h>
#include <netat/ddp.h>

int
atp_sendrsp (fd, dest, xo, tid, resp)
	int		fd;
	at_inet_t	*dest;
	int		xo;
	u_short		tid;
	at_resp_t	*resp;
{
	char respbuff[TOTAL_ATP_HDR_SIZE + sizeof(struct atpBDS)*ATP_TRESP_MAX];
	int		nbds;
	at_ddp_t	*ddphdr   = (at_ddp_t *) respbuff;
	at_atp_t	*atphdr   = (at_atp_t *) ddphdr->data;
	struct atpBDS	*bds = (struct atpBDS *) &respbuff[TOTAL_ATP_HDR_SIZE];
	int		i;
	struct iovec	iov[ATP_TRESP_MAX];
	int		resplen, datalen = 0;

	if (!valid_at_addr(dest)) /* validate dest addr */
		return (-1);

	/* Set up the ATP response data */
	for (i = nbds = 0; i < ATP_TRESP_MAX; i++) {
		if (!(resp->bitmap & (1 << i)))
			continue;
		if (resp->resp[i].iov_len > ATP_DATA_SIZE) {
			errno = EINVAL;
			return (-1);
		}

		/* copy userdata and response data */

		*(u_long*)bds->bdsUserData = resp->userdata[i];
		UAL_ASSIGN(bds->bdsBuffAddr, (resp->resp[i].iov_base)); 
		UAS_ASSIGN(bds->bdsBuffSz, (short)resp->resp[i].iov_len);
		iov[nbds].iov_base = (caddr_t)resp->resp[i].iov_base;
		iov[nbds].iov_len  = (int)resp->resp[i].iov_len;
		bds++;
		nbds++;
		datalen += (int)resp->resp[i].iov_len;
	}

	/* fill in DDP & ATP headers */

	UAS_ASSIGN((bds-nbds)->bdsDataSz, nbds);	
	ddphdr->type       	= DDP_ATP;
	UAS_ASSIGN(ddphdr->checksum, 0);
	NET_ASSIGN(ddphdr->dst_net, dest->net);
	ddphdr->dst_node   	= dest->node;
	ddphdr->dst_socket 	= dest->socket;
	UAS_ASSIGN(atphdr->tid, tid);
	atphdr->xo	 	= xo;

	/* We're ready to send the response buffers,
	 * send the resp data
	 */
	resplen = TOTAL_ATP_HDR_SIZE+(sizeof(struct atpBDS)*nbds);
	return ATPsndrsp(fd, respbuff, resplen, datalen);

	return (0);
}	
