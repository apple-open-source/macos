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

#include <netat/appletalk.h>
#include <netat/atp.h>
#include <netat/ddp.h>

#include "at_proto.h"

#define	SET_ERRNO(e) errno = e

#define	atpBDSsize (sizeof(struct atpBDS)*ATP_TRESP_MAX)

int	at_send_nowait_request(fd,treq_buff)
	int        	fd;
	at_atpreq	*treq_buff;
{
 	int        	status;
	char	   	buff[640+atpBDSsize];
	int 		i, len;
	struct atp_set_default *pp;
	struct atpBDS *atpBDS;
	at_atp_t	*atp;
	at_ddp_t 	*ddp;
 
	len = treq_buff->at_atpreq_treq_length +
	  sizeof(struct atp_set_default) + TOTAL_ATP_HDR_SIZE + atpBDSsize;
 	(void) memset (buff, 0, len);

	atpBDS = (struct atpBDS *)buff;
	for (i = 0; i < ATP_TRESP_MAX; i++) {
		UAL_ASSIGN(atpBDS[i].bdsBuffAddr, 1);
		UAS_ASSIGN(atpBDS[i].bdsBuffSz, ATP_DATA_SIZE);
	}

	if ((*treq_buff).at_atpreq_type != ATP_CMD_TREQ) {
	    SET_ERRNO(EINVAL);
	    return(-1);
	}

	if (!(1 < (*treq_buff).at_atpreq_to.socket && 
	        (*treq_buff).at_atpreq_to.socket <= 254)) {
	    SET_ERRNO(EINVAL);
	    return(-1);
	}

	if (!((*treq_buff).at_atpreq_treq_length <= ATP_DATA_SIZE)) {
	    SET_ERRNO(EMSGSIZE);
	    return(-1);
	}

	pp = (struct atp_set_default *)&buff[atpBDSsize];
	pp->def_retries = (int)treq_buff->at_atpreq_maximum_retries;
	pp->def_rate = (int)treq_buff->at_atpreq_retry_timeout*100;
	pp->def_bdsp = (struct atpBDS *)1;
	pp->def_BDSlen = sizeof (struct atpBDS) * ATP_TRESP_MAX;

	ddp = (at_ddp_t *)(&buff[sizeof(struct atp_set_default)+atpBDSsize]);
	NET_ASSIGN(ddp->dst_net, treq_buff->at_atpreq_to.net);
	ddp->dst_node = treq_buff->at_atpreq_to.node;
	ddp->dst_socket = treq_buff->at_atpreq_to.socket;
	atp = ATP_ATP_HDR(&buff[sizeof(struct atp_set_default)+atpBDSsize]);
	atp->xo = treq_buff->at_atpreq_xo;
	atp->xo_relt = treq_buff->at_atpreq_xo_relt;
	atp->bitmap = treq_buff->at_atpreq_treq_bitmap;
	UAL_UAL(atp->user_bytes, treq_buff->at_atpreq_treq_user_bytes);

	(void) memcpy((char *)atp->data,
	       	      (char *)treq_buff->at_atpreq_treq_data,
	       	      treq_buff->at_atpreq_treq_length);

	while ((status = ATPsndreq(fd, buff, len, 1)) < 0) {
		if (errno == EINTR || errno == EAGAIN)
			continue;
		perror("ATPsndreq");
		return -1;
	}

	treq_buff->at_atpreq_tid = status;
	return 0;
}

int	at_send_request(fd,treq_buff)
	int        	fd;
	at_atpreq	*treq_buff;
{
 	int        	status;
	char	   	buff[640+atpBDSsize];
	int 		i, len;
	struct atp_set_default *pp;
	struct atpBDS *atpBDS;
	register struct atpBDS *newbdsp;
	register int newcount;
	at_atp_t	*atp;
	at_ddp_t 	*ddp;
 
     len = treq_buff->at_atpreq_treq_length +
         sizeof(struct atp_set_default) + TOTAL_ATP_HDR_SIZE + atpBDSsize;
 	(void) memset (buff, 0, len);

	atpBDS = (struct atpBDS *)buff;
	for (i = 0; i < ATP_TRESP_MAX; i++) {
		UAL_ASSIGN(atpBDS[i].bdsBuffAddr,
		    (long) treq_buff->at_atpreq_tresp_data[i]);
		UAS_ASSIGN(atpBDS[i].bdsBuffSz, treq_buff->at_atpreq_tresp_lengths[i]);
	}
        if ((*treq_buff).at_atpreq_type != ATP_CMD_TREQ) {
		SET_ERRNO(EINVAL);
            	return(-1);
        }

        if (!(1 < (*treq_buff).at_atpreq_to.socket && 
              (*treq_buff).at_atpreq_to.socket <= 254)) {
		SET_ERRNO(EINVAL);
            	return(-1);
        }

        if (!((*treq_buff).at_atpreq_treq_length <= ATP_DATA_SIZE)) {
		SET_ERRNO(EMSGSIZE);
            	return(-1);
        }

	pp = (struct atp_set_default *)&buff[atpBDSsize];
	pp->def_retries = (int)treq_buff->at_atpreq_maximum_retries;
	pp->def_rate = (int)treq_buff->at_atpreq_retry_timeout*100;
	pp->def_bdsp = (struct atpBDS *)1;
	pp->def_BDSlen = sizeof (struct atpBDS) * ATP_TRESP_MAX;

	ddp = (at_ddp_t *)(&buff[sizeof(struct atp_set_default)+atpBDSsize]);
	NET_ASSIGN(ddp->dst_net, treq_buff->at_atpreq_to.net);
	ddp->dst_node = treq_buff->at_atpreq_to.node;
	ddp->dst_socket = treq_buff->at_atpreq_to.socket;
	atp = ATP_ATP_HDR(&buff[sizeof(struct atp_set_default)+atpBDSsize]);
	atp->xo = treq_buff->at_atpreq_xo;
	atp->xo_relt = treq_buff->at_atpreq_xo_relt;
	atp->bitmap = treq_buff->at_atpreq_treq_bitmap;
	UAL_UAL(atp->user_bytes, treq_buff->at_atpreq_treq_user_bytes);

	(void) memcpy((char *)atp->data,
	       	      (char *)treq_buff->at_atpreq_treq_data,
	       	      treq_buff->at_atpreq_treq_length);

	while ((status = ATPsndreq(fd, buff, len, 0)) < 0) {
		if (errno == EINTR || errno == EAGAIN)
			continue;
		perror("ATPsndreq");
		return -1;
	}

	treq_buff->at_atpreq_tid = status;

	newbdsp = (struct atpBDS *)buff;
	newcount = UAS_VALUE(newbdsp->bdsBuffSz);

	status = 0;
	if (newcount > 0) {
		treq_buff->at_atpreq_tresp_bitmap  = (1 << newcount) - 1;
		treq_buff->at_atpreq_tresp_eom_seqno = newcount-1;

		for (i = 0; i < newcount; i++) {
			UAL_UAL(treq_buff->at_atpreq_tresp_user_bytes[i], newbdsp->bdsUserData);
			treq_buff->at_atpreq_tresp_lengths[i] = 
			    UAS_VALUE(newbdsp->bdsDataSz);
			status += UAS_VALUE(newbdsp->bdsDataSz);
			newbdsp++;
		}
		return(status);
	} else
	    return -1;
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
	int		i;
	u_char		bitmap;
	at_atpreq	treq;

		/* validate the value of xo release timer */
	if (xo && (xo_relt&0x07 > ATP_XO_8MIN)) {
		SET_ERRNO(EINVAL);
		return (-1);
	}

	if (retry && (retry->interval <= 0)) {
		SET_ERRNO(EINVAL);
		return (-1);
	}
       
	if (!valid_at_addr(dest)) 	/* validate dest addr */
		return (-1);

	treq.at_atpreq_type			= ATP_CMD_TREQ;
        UAL_ASSIGN(treq.at_atpreq_treq_user_bytes, userdata);
        treq.at_atpreq_to 			= *dest;
        treq.at_atpreq_treq_data		= reqbuf;
        treq.at_atpreq_treq_length		= reqlen;
        treq.at_atpreq_treq_bitmap		= resp ? resp->bitmap : 0xff;
        treq.at_atpreq_xo			= xo;
        treq.at_atpreq_xo_relt			= xo ? xo_relt : 0;

		/* Set up the ATP retry and retry interval parameters.  If retry is NULL,
	 	 * set interval to 0 set defaults instead 
	 	 */
        treq.at_atpreq_retry_timeout	= retry ? retry->interval : ATP_DEF_INTERVAL;
        treq.at_atpreq_maximum_retries	= retry ? retry->retries  : ATP_DEF_RETRIES;


		/* validate response buffers */
	if (resp && !nowait) {
		bitmap = resp->bitmap;
		for (i = 0; i < ATP_TRESP_MAX; i++) {
			if (!(bitmap & (1 << i)))
				continue;
			if (resp->resp[i].iov_len < 0 || resp->resp[i].iov_len > ATP_DATA_SIZE) {
				SET_ERRNO(EINVAL);
				return (-1);
			}
			/* set buffer pointers and lengths */ 
       			treq.at_atpreq_tresp_data[i]  = (u_char *)resp->resp[i].iov_base;
			treq.at_atpreq_tresp_lengths[i] = resp->resp[i].iov_len;
		}
	} else
		bitmap = 0;

  if (nowait) {
	if (at_send_nowait_request(fd, &treq) == -1)
		return(-1);
	}
  else {
	if (at_send_request(fd, &treq) == -1)
		return(-1);
	}

		/* Request succeeded, get TID and the response data */
	if (tid) 
        	*tid = treq.at_atpreq_tid;

  if (nowait)
	return (0);

		/* If we want response data, fill in the response structure */
	if (resp) {
		resp->bitmap = treq.at_atpreq_tresp_bitmap;
		for (i = 0; i < ATP_TRESP_MAX; i++) {
			if (!(bitmap & (1 << i)))  	/* if we didn't ask for it, skip it */
				continue;
			resp->userdata[i] = *(long*)treq.at_atpreq_tresp_user_bytes[i];
			resp->resp[i].iov_len = treq.at_atpreq_tresp_lengths[i];
		}
	}
	return (0);
}	
