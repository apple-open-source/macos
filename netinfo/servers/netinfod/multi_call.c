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
 * multi_call: send out multiple call messages, wait for first reply
 * Copyright (C) 1991 by NeXT, Inc.
 */
#include <NetInfo/config.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinfo/ni.h>
#include <NetInfo/socket_lock.h>
#include "multi_call.h"
#include "ni_globals.h"
#include "getstuff.h"
#include <NetInfo/system.h>
#include <NetInfo/system_log.h>

#define NRETRIES 3
#define USECS_PER_SEC 1000000
#define MAX_RETRY_TIMEOUT 2

extern int alert_aborted(void);
extern void xdr_free();

/*
 * Encode a call message
 */
static int
encodemsg(char *buf, int buflen, struct rpc_msg *call, unsigned prognum, unsigned versnum, unsigned procnum, xdrproc_t xdr_args, void *arg)
{
	XDR xdr;
	unsigned size;
	unsigned pos;

	xdrmem_create(&xdr, buf, buflen, XDR_ENCODE);

	if (xdr_callmsg(&xdr, call) == 0) return 0;
	if (xdr_u_int(&xdr, &prognum) == 0) return 0;
	if (xdr_u_int(&xdr, &versnum) == 0) return 0;
	if (xdr_u_int(&xdr, &procnum) == 0) return 0;

	pos = xdr_getpos(&xdr);
	xdr_setpos(&xdr, pos + BYTES_PER_XDR_UNIT);
	if ((*xdr_args)(&xdr, arg) == 0) return 0;

	size = xdr_getpos(&xdr) - pos;
	xdr_setpos(&xdr, pos);

	if (xdr_u_int(&xdr, &size) == 0) return 0;

	return (pos + BYTES_PER_XDR_UNIT + size);
}

/*
 * Decode a reply message
 */
static int
decodemsg(XDR *xdr, xdrproc_t xdr_res, void *res)
{
	unsigned port;
	unsigned len;
	long *buf;
	XDR bufxdr;

	if (xdr_u_int(xdr, &port) == 0) return 0;
	if (xdr_u_int(xdr, &len) == 0) return 0;

	buf = xdr_inline(xdr, len);
	if (buf == NULL) return 0;
	
	xdrmem_create(&bufxdr, (char *)buf, len * BYTES_PER_XDR_UNIT, XDR_DECODE);
	if ((*xdr_res)(&bufxdr, res) == 0) return 0;

	return 1;
}

/*
 * Do the real work
 *
 * GRS 2/16/92 - I've added a preferred_provider argument to this routine
 * to help encourage machines to bind to themselves when they serve their
 * own .. domains.  The preferred provider is the index into addrs which
 * holds the goodies for the local host (or -1 if no preferred provider).
 * If there is a preferred provider, the whole probe sequence is performed
 * with it as the only recipient.  If, after the normal number of retries, 
 * no connection is established, the whole enchilada is repeated again, this
 * time with all entries (including the preferred provider) included.
 */
 
enum clnt_stat 
ni_multi_call(unsigned naddrs, struct in_addr *addrs, unsigned prognum, unsigned versnum, unsigned procnum, xdrproc_t xdr_args, void *argsvec, unsigned argsize, xdrproc_t xdr_res, void *res, int (*eachresult)(void *, struct sockaddr_in *, int), int preferred_provider)
{
	struct authunix_parms aup;
	char credbuf[MAX_AUTH_BYTES];
	struct opaque_auth cred;
	struct opaque_auth verf;
	int gids[NGROUPS];
	int s, i;
	struct timeval tv;
	int callno;
	int serverno;
	struct rpc_msg call;
	struct rpc_msg reply;
	struct sockaddr_in sin;
	struct sockaddr_in from;
	int fromsize;
	char buf[UDPMSGSIZE];
	int buflen;
	unsigned trans_id;
	int dtablesize = getdtablesize();
	XDR xdr;
	int sendlen;
	fd_set fds;

	/*
	 * Fill in Unix auth stuff
	 */
	aup.aup_time = time(0);
	aup.aup_machname = sys_hostname();
	aup.aup_uid = getuid();
	aup.aup_gid = getgid();
	aup.aup_gids = gids;
	aup.aup_len = getgroups(NGROUPS, aup.aup_gids);

	/*
	 * Encode unix auth
	 */
	xdrmem_create(&xdr, credbuf, sizeof(credbuf), XDR_ENCODE);
	if (xdr_authunix_parms(&xdr, &aup) == 0) return RPC_CANTENCODEARGS;

	cred.oa_flavor = AUTH_UNIX;
	cred.oa_base = credbuf;
	cred.oa_length = xdr_getpos(&xdr);

	verf.oa_flavor = AUTH_NULL;
	verf.oa_length = 0;

	/*
	 * Set up call header information
	 */
	trans_id = time(0) ^ getpid();
	call.rm_xid = trans_id;
	call.rm_direction = CALL;
	call.rm_call.cb_rpcvers = 2;
	call.rm_call.cb_prog = PMAPPROG;
	call.rm_call.cb_vers = PMAPVERS;
	call.rm_call.cb_proc = PMAPPROC_CALLIT;
	call.rm_call.cb_cred = cred;
	call.rm_call.cb_verf = verf;

	/*
	 * Open socket
	 */
	socket_lock();
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	socket_unlock();
	if (s < 0)
	{
		system_log(LOG_ERR, "multi_call: socket: %m");
		return RPC_FAILED;
	}

	i = 1;
	if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &i, sizeof(int)) < 0)
	{
		system_log(LOG_ERR, "multi_call can't broadcast: %m");
	}

	/*
	 * 1/2 second initial timeout
	 */
	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	/*
	 * Init address info
	 */
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PMAPPORT);
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	
	do_probes:
	
	for (callno = 0; callno <= NRETRIES; callno++)
	{
		/*
		 * Send a call message to each host with the appropriate args
		 */
	  	for (serverno = 0; serverno < naddrs; serverno++)
		{
			if ((preferred_provider >= 0) &&  (serverno != preferred_provider)) continue;

			call.rm_xid = trans_id + serverno;
			buflen = encodemsg(buf, sizeof(buf), &call, prognum, versnum, procnum, xdr_args, (argsvec + (serverno * argsize)));
			if (buflen == 0)
			{
				/* Encode failed */
				continue;
			}

			sin.sin_addr = addrs[serverno];
			sendlen = sendto(s, buf, buflen, 0, (struct sockaddr *)&sin, sizeof(sin));
			if (sendlen != buflen)
			{
				system_log(LOG_ERR,  "Cannot send multicall packet to %s: %m", inet_ntoa(addrs[serverno]));
			}
		}

		/*
		 * Double the timeout after each call
		 */
		if (callno > 1)
		{
			tv.tv_sec *= 2;
			tv.tv_usec *= 2;

			if (tv.tv_usec >= USECS_PER_SEC)
			{
				tv.tv_usec -= USECS_PER_SEC;
				tv.tv_sec++;
			}

			if (tv.tv_sec >= MAX_RETRY_TIMEOUT)
			{
				tv.tv_sec = MAX_RETRY_TIMEOUT;
				tv.tv_usec = 0;
			}
		}

		/*
		 * Check for cancel by user
		 */
		if ((alert_aborted() != 0) || (get_binding_status() == NI_FAILED))
		{
			system_log(LOG_DEBUG, "multi_call aborted");
			socket_lock();
			close(s);
			socket_unlock();
			return RPC_FAILED;
		}

		/*
		 * Wait for reply
		 */
		FD_ZERO(&fds);
		FD_SET(s, &fds);

		switch (select(dtablesize, &fds, NULL, NULL, &tv))
		{
			case -1:
				system_log(LOG_ERR, "select failure: %m");
				continue;
			case 0:
				system_log(LOG_DEBUG, "multicall timeout: %u+%u", tv.tv_sec, tv.tv_usec);
				continue;
			default:
				break;
		}

		/*
		 * Receive packet
		 */
		fromsize = sizeof(from);
		buflen = recvfrom(s, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromsize);
		if (buflen < 0) continue;

		/*
		 * Decode packet and if no errors, call eachresult
		 */
		xdrmem_create(&xdr, buf, buflen, XDR_DECODE);
		reply.rm_reply.rp_acpt.ar_results.proc = xdr_void;
		reply.rm_reply.rp_acpt.ar_results.where = NULL;
		if (xdr_replymsg(&xdr, &reply) &&
			(reply.rm_xid >= trans_id) &&
			(reply.rm_xid < trans_id + naddrs) &&
			(reply.rm_reply.rp_stat == MSG_ACCEPTED) &&
			(reply.acpted_rply.ar_stat == SUCCESS) &&
			decodemsg(&xdr, xdr_res, res))
		{
			if ((*eachresult)(res, &from, reply.rm_xid - trans_id))
			{
				xdr_free(xdr_res, res);
				socket_lock();
				close(s);
				socket_unlock();
				return RPC_SUCCESS;
			}
		}

		xdr_free(xdr_res, res);
	}
	
	/* 
	 * If we were trying to favor a particular server, repeat the whole
	 * procedure with favoritism turned off.
	 */
	
	if (preferred_provider >= 0)
	{
		preferred_provider = -1;
		goto do_probes;
	}
	
	socket_lock();
	close(s);
	socket_unlock();
	return RPC_TIMEDOUT;
}


