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
 *      Copyright (c) 1998 Apple Computer, Inc. 
 *
 *      The information contained herein is subject to change without
 *      notice and  should not be  construed as a commitment by Apple
 *      Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *      for any errors that may appear.
 *
 *      Confidential and Proprietary to Apple Computer, Inc.
 *
 */

#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/select.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/pap.h>

#include "at_proto.h"

#define	SET_ERRNO(e) errno = e

#define		PAP_CONNID		0
#define		PAP_TYPE		1
#define		PAP_EOF			2

#define		REQ_COMPLETED(p)	(*((char *) p))
#define		REQ_ARRIVED(p)		(!*((char *) p))

#define		CONNID_OF(p)		(((u_char *)&p)[0])
#define		TYPE_OF(p)			(((u_char *)&p)[1])

extern int papm[NOFILE];
extern struct pap_state *paps[];

int pap_write(fd, data, len, eof, flush)
int  fd;
char *data;
register int len;
int  eof, flush;
{
	register struct pap_state *papp;
	at_resp_t resp, pap_ignore_resp;
	at_inet_t src, dest;
	int rc, userdata, xo, reqlen;
	u_short tid;
	u_char bitmap, *puserdata;
	fd_set selmask;
	char tmpbuf[AT_PAP_DATA_SIZE];

				/* check for invalid paramaters! */
	if (fd < 0) {
		SET_ERRNO(EINVAL);
		return -1;
	} else if ((papp = paps[papm[fd]]) == NULL) {
		SET_ERRNO(EINVAL);
		return -1;
	}

	if (papp->pap_eof_sent)
		return 0;
	papp->pap_eof_sent = eof;

	do {
		register unsigned int i;

		/*
		 * Break up the outgoing data into a set of
		 * response packets to reply to an incoming
		 * PAP 'SENDDATA' request
		 */

		for (i = 0; i < papp->pap_flow; i++) {
			puserdata = (u_char *)&resp.userdata[i];
			puserdata[PAP_CONNID] = papp->pap_connID;
			puserdata[PAP_TYPE] = AT_PAP_TYPE_DATA;
			puserdata[PAP_EOF] = eof ? 1 : 0;
			resp.resp[i].iov_base = (caddr_t)data;
			if (data)
			    data += AT_PAP_DATA_SIZE;
			if (len <= AT_PAP_DATA_SIZE) {
				resp.resp[i].iov_len = (int)len;
				len = 0;
				if (eof)
					papp->pap_closing = 1;
				break;
			}
			else {
				resp.resp[i].iov_len = (int)AT_PAP_DATA_SIZE;
				len -= AT_PAP_DATA_SIZE;
			}
		}
		resp.bitmap = (1 << (i + 1)) - 1;
		dest.net = papp->pap_to.net;
		dest.node = papp->pap_to.node;

		/*
		 * Wait for the 'SENDDATA' request to arrive
		 */
	for (;;) {
		FD_ZERO(&selmask);
		FD_SET(fd, &selmask);
		if (select(fd+1, &selmask, 0, 0, 0) <= 0)
			return -1;
		if ((rc = atp_look(fd)) < 0)
			return -1;
		if (rc) {
			pap_ignore_resp.resp[0].iov_base = tmpbuf;
			pap_ignore_resp.resp[0].iov_len = sizeof(tmpbuf);
			pap_ignore_resp.bitmap = 0x01;
			atp_getresp(fd, &tid, &pap_ignore_resp);
			if (tid == papp->pap_ignore_id) {
				papp->pap_ignore_id = 0;
				papp->pap_read_ignore = 0;
				puserdata = (u_char *)&pap_ignore_resp.userdata[0];
				if (puserdata[PAP_EOF])
					papp->pap_eof = 1;
				else
					pap_read_ignore(fd);
			}
			continue;
		}
		reqlen = sizeof(tmpbuf);
		if (atp_getreq(fd, &src, tmpbuf,
				&reqlen, &userdata, &xo, &tid, &bitmap, 0) < 0)
			return -1;
		if (CONNID_OF(userdata) == papp->pap_connID) {
			/*
			 * Reset timer
			 */
			alarm(0);
			alarm(PAP_TIMEOUT);
			dest.socket = src.socket;
            if (TYPE_OF(userdata) == AT_PAP_TYPE_SEND_DATA)
				break;
		}
	}

		/*
		 * Write out the data as a PAP 'DATA' response
		 */
		if (atp_sendrsp(fd, &dest, xo, tid, &resp) < 0)
			return -1;
	} while (len > 0);

	return(0);
}
