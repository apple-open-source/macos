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
#include <fcntl.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/errno.h>

#include <netat/appletalk.h>
#include <netat/ddp.h>
#include <netat/pap.h>
#include <netat/atp.h>

#define	SET_ERRNO(e) errno = e

/*#define MSGSTR(num,str)		catgets(catd, MS_PAP, num,str)*/
#define MSGSTR(num,str)            str

extern	long time();
struct pap_state *paps[NPAPSESSIONS];
int papm[NOFILE];
static int pap_active = 0;
int papfd = 0;

int pap_open(tuple)
at_nbptuple_t	*tuple;
{
	int fd, k;
	long tm;
	int reqid;
	unsigned char data[10], rdata[ATP_DATA_SIZE];
	at_socket socket = 0;
	struct pap_state *papp;
	int	i;
	int	status;
	int userdata;
	u_char	*puserdata = (u_char *)&userdata;
	at_resp_t resp;
	at_retry_t retry;
	int error;

	SET_ERRNO(0);
	
	if (tuple == NULL) {
		SET_ERRNO(EINVAL);
		return (-1);
	}
	if (_nbp_validate_entity_(&tuple->enu_entity,0,1)==0){/*nometa,zone ok*/
		SET_ERRNO(EINVAL);
		return (-1);
	}

	if (!valid_at_addr(&tuple->enu_addr))
		return(-1);

	pap_status_update("=", PAP_SOCKERR, strlen(PAP_SOCKERR));

	for (i = 0; i < pap_active; i++)
	    if (paps[i] == (struct pap_state *)0)
		break;
	if (i == pap_active) 
	    if (pap_active < NPAPSESSIONS) {
		    pap_active++;
	    } else
		return -1;

	fd = atp_open(&socket);
	if (fd < 0)
		return(-1);
	tm = time(NULL);
	srand(tm);
	if ((paps[i] = 
	     (struct pap_state *)malloc(sizeof(struct pap_state))) == 0) {
		atp_close(fd);
		return -1;
	}
	papm[fd] = i;
	papp = paps[i];
	
	papp->pap_req_socket = socket;
	papp->pap_inuse = 1;
	papp->pap_tickle = 0;
	papp->pap_send_count = 0;
	papp->pap_rcv_count = 0;
	papp->pap_request = 0;
	papp->pap_error = 0;
	papp->pap_eof = 0;
	papp->pap_eof_sent = 0;
	papp->pap_read_ignore = 0;
	papp->pap_closing = 0;
	papp->pap_connID = 0xffff;
	papp->pap_timer = 0;
	papp->pap_ending = 0;
	papp->pap_request_count = 0;
	papp->pap_req_timer = 0;
	
	/*
	 * Open connection to the requested printer.
	 */
	strcpy(&rdata[5], MSGSTR(M_NO_PRINTERS, "No Printers"));
	rdata[4] = strlen(&rdata[5]);
	
	reqid = (rand()&0xff) | 0x01;
	puserdata[0] = reqid;
	puserdata[1] = AT_PAP_TYPE_OPEN_CONN;
	puserdata[2] = 0;
	puserdata[3] = 0;
	retry.interval = 2;
	retry.retries = 5;
	resp.bitmap = 0x01;
	resp.resp[0].iov_base = rdata;
	resp.resp[0].iov_len = sizeof(rdata);
	i = time(NULL) - tm;
	data[0] = socket;
	data[1] = 8;
	data[2] = i>>8;
	data[3] = i & 0xff;

  for (k=0; k < 10; k++) {
	status = atp_sendreq(fd, &tuple->enu_addr, data, 4, userdata, 1, 0, 0,
		&resp, &retry, 0);
	if (status >= 0) {
		error = 0;
		SET_ERRNO(0);
		if (*(short *)&rdata[2] != 0) {
			pap_status_update(tuple->enu_entity.type.str, 
					  &rdata[5], rdata[4]&0xff);
			error = ECONNREFUSED;
			SET_ERRNO(ECONNREFUSED);
			if (rdata[1] == 8) {
				sleep(1);
				continue;
			}
			goto bad;
		}
		/* Connection established okay, just for the sake of our
		 * sanity, check the other fields in the packet
		 */
		puserdata = (u_char *)&resp.userdata[0];
		if ((puserdata[0]&0xff) != reqid){
			error = EINVAL;
			SET_ERRNO(EINVAL);
			goto bad;
		}
		if (puserdata[1] !=
			AT_PAP_TYPE_OPEN_CONN_REPLY) {
			error = EINVAL;
			SET_ERRNO(EINVAL);
			goto bad;
		}
		break;
	} else {
		strcpy(&rdata[5], MSGSTR(M_UNREACH,"Destination unreachable"));
		rdata[4] = strlen(&rdata[5]);
		error = ECONNREFUSED;
		SET_ERRNO(ECONNREFUSED);
	}
  }
	pap_status_update(tuple->enu_entity.type.str, &rdata[5], 
			  rdata[4]&0xff);

	if (error)
	    goto bad;

	/*
	 * Update the default request buffer (allocated above)
	 */
	papp->pap_to.net = tuple->enu_addr.net;
	papp->pap_to.node = tuple->enu_addr.node;
	papp->pap_to.socket = rdata[0];
	papp->pap_connID = reqid;
	papp->pap_flow = rdata[1];

	/* 
	 * start the tickle
	 */
	if (pap_start_tickle(fd, papp) < 0)
	    goto bad;
	return(fd);

bad:
	free(papp);
	paps[papm[fd]] = (struct pap_state *)0;
	atp_close(fd);
	return(-1);
}

/* ARGSUSED */
void
pap_timeout(n) {
	struct pap_state *papp = paps[papm[papfd]];
	
	if (papp == NULL)
	    return;		/* stream is already closed */
	/*
	 *	If we are closing let it complete
	 */

	if (papp->pap_closing)
		papp->pap_inuse = 0; 

	papp->pap_closing = 1;

	/*
	 * Do what is needed here...
	 */
	pap_close(papfd);
}

int
pap_start_tickle(fd, papp) 
	int fd;
	struct pap_state *papp;
{
	int err;

	if ((err = pap_send_request(fd, papp, AT_PAP_TYPE_TICKLE, 0, 0)) < 0) {
		papp->pap_tickle = 0;
		return -1;
	} else {
		papp->pap_tickle_id = err;
		papp->pap_tickle = 1;
	}
	/*
	 *	start the connection timeout
	 */
	papfd = fd;
	if (!papp->pap_timer) {	/* ????? */
		signal(SIGALRM, pap_timeout);
		alarm(PAP_TIMEOUT);
		papp->pap_timer = 1;
	}
	return 0;
}

int
pap_send_request(fd, papp, function, xo, seqno)
	int fd;
	struct pap_state *papp;
	int function, xo, seqno;
{
	u_short tid;
	int err;
	sigset_t sv, osv;
	int userdata;
	u_char *puserdata = (u_char *)&userdata;
	at_inet_t dest;
	at_retry_t retry;
	at_resp_t resp;

	dest = papp->pap_to;
	puserdata[0] = papp->pap_connID;
	puserdata[1] = function;
	resp.bitmap = seqno;
	retry.interval = 10;
	retry.retries = -1; /* was ATP_INFINITE_RETRIES */
	if (seqno) {
		papp->pap_send_count++;
		if (papp->pap_send_count == 0)
		    papp->pap_send_count = 1;
		*(u_short *)&puserdata[2] = papp->pap_send_count;
	} else
		*(u_short *)&puserdata[2] = 0;

	sigemptyset(&sv);
	sigaddset(&sv, SIGIO);
	(void) sigprocmask(SIG_SETMASK, &sv, &osv);
	err = atp_sendreq(fd, &dest, 0, 0, userdata, xo, 0, &tid, &resp, &retry, 1);
	(void) sigprocmask(SIG_SETMASK, &osv, NULL);
	return (int)tid;
}
