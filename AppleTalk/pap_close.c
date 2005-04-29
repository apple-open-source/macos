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

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/errno.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/pap.h>
#include <netat/atp.h>

#define	SET_ERRNO(e) errno = e

extern struct pap_state *paps[];
extern int papm[];
extern int papfd;

int	pap_close(fd)
int	fd;
{
	register struct pap_state *papp;
	unsigned char rdata[ATP_DATA_SIZE];
	int userdata;
	u_char	*puserdata = (u_char *)&userdata;
	at_resp_t resp;
	at_retry_t retry;
	at_inet_t dest;

	papp = paps[papm[fd]];
	if (papp == NULL) {
		SET_ERRNO(EINVAL);
		return -1;
	}
	alarm(0);
	papp->pap_timer = 0;

	/*
	 * Cancel the ignore read request to the LaserWriter as required.
	 */
	if (papp->pap_read_ignore)
	    pap_can_request(fd, papp->pap_ignore_id);

	/*
	 * Cancel the tickles
	 */
	pap_can_request(fd, papp->pap_tickle_id);
	
	/*
	 * Close the connection
	 */
	dest = papp->pap_to;
	puserdata[0] = papp->pap_connID;
	puserdata[1] = AT_PAP_TYPE_CLOSE_CONN;
	puserdata[2] = 0;
	puserdata[3] = 0;
	retry.interval = 2;
	retry.retries = 5;
	resp.bitmap = 0x01;
	resp.resp[0].iov_base = rdata;
	resp.resp[0].iov_len = sizeof(rdata);
	atp_sendreq(fd, &dest, 0, 0, userdata, 1, 0, 0, &resp, &retry, 0);

	free(papp);
	paps[papm[fd]] = (struct pap_state *)0;
	papm[fd] = 0;
	papfd = 0;
	return(close(fd));
}

int
pap_can_request(fd, id)
	int fd;
	int id;
{
	int len;
	sigset_t sv, osv;
	
	len = sizeof(int);
	sigemptyset(&sv);
	sigaddset(&sv, SIGIO);
	(void) sigprocmask(SIG_SETMASK, &sv, &osv);
	if (at_send_to_dev(fd, AT_ATP_CANCEL_REQUEST, &id, &len) < 0) {
		(void) sigprocmask(SIG_SETMASK, &osv, NULL);
		return -1;
	}
	(void) sigprocmask(SIG_SETMASK, &osv, NULL);
	return 0;
}

