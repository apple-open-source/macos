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
 * multi_call: send out multiple call messages, wait for first reply
 * Copyright (C) 1991 by NeXT, Inc.
 */
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <sys/socket.h>
#include <syslog.h>
#include <sys/param.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#ifdef NETINFOD
#	include "socket_lock.h"
#	include "clib.h"
#	define multi_call _multi_call
#else
#	define socket_lock()
#	define socket_unlock()
#	include "clib.h"
#endif


#define NRETRIES 5
#define USECS_PER_SEC 1000000


/*
 * Wrapper for gethostname() syscall
 */
static char *
get_hostname(void)
{
	int len;
	static char hostname[MAXHOSTNAMELEN + 1];

	len = gethostname(hostname, sizeof(hostname));
	if (len < 0) {
		hostname[0] = 0;
	} else {
		hostname[len] = 0;
	}
	return (hostname);
}


/*
 * Encode a call message
 */
static int
encodemsg(
	  char *buf,
	  int buflen,
	  struct rpc_msg *call,
	  unsigned prognum,
	  unsigned versnum,
	  unsigned procnum,
	  xdrproc_t xdr_args,
	  void *arg
	  )
{
	XDR xdr;
	unsigned size;
	unsigned pos;

	xdrmem_create(&xdr, buf, buflen, XDR_ENCODE);
	if (!xdr_callmsg(&xdr, call) ||
	    !xdr_u_int(&xdr, &prognum) ||
	    !xdr_u_int(&xdr, &versnum) ||
	    !xdr_u_int(&xdr, &procnum)) {
		return (0);
	}
	pos = xdr_getpos(&xdr);
	xdr_setpos(&xdr, pos + BYTES_PER_XDR_UNIT);
	if (!(*xdr_args)(&xdr, arg)) {
		return (0);
	}
	size = xdr_getpos(&xdr) - pos;
	xdr_setpos(&xdr, pos);
	if (!xdr_u_int(&xdr, &size)) {
		return (0);
	}
	return (pos + BYTES_PER_XDR_UNIT + size);
}

/*
 * Decode a reply message
 */
static int
decodemsg(
	  XDR *xdr,
	  xdrproc_t xdr_res,
	  void *res
	  )
{
	unsigned port;
	unsigned len;
	long *buf;
	XDR bufxdr;

	if (!xdr_u_int(xdr, &port) ||
	    !xdr_u_int(xdr, &len) ||
	    !(buf = (long *)xdr_inline(xdr, len))) {
		return (0);
	}
	xdrmem_create(&bufxdr, (char *)buf, len * BYTES_PER_XDR_UNIT, 
		      XDR_DECODE);
	if (!(*xdr_res)(&bufxdr, res)) {
		return (0);
	}
	return (1);
	
}

/*
 * Do the real work
 */
enum clnt_stat 
multi_call(
	   unsigned naddrs,		
	   struct in_addr *addrs, 
	   u_long prognum,
	   u_long versnum,
	   u_long procnum,
	   xdrproc_t xdr_args,
	   void *argsvec,
	   unsigned argsize,
	   xdrproc_t xdr_res,
	   void *res,
	   int (*eachresult)(void *, struct sockaddr_in *, int),
	   int timeout
	   )
{
	struct authunix_parms aup;
	char credbuf[MAX_AUTH_BYTES];
	struct opaque_auth cred;
	struct opaque_auth verf;
	int gids[NGROUPS];
	int s;
	struct timeval tv;
	struct timeval subtimeout;
	unsigned long long utimeout;
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
	aup.aup_machname = get_hostname();
	aup.aup_uid = getuid();
	aup.aup_gid = getgid();
	aup.aup_gids = gids;
	aup.aup_len = getgroups(NGROUPS, aup.aup_gids);

	/*
	 * Encode unix auth
	 */
	xdrmem_create(&xdr, credbuf, sizeof(credbuf), XDR_ENCODE);
	if (!xdr_authunix_parms(&xdr, &aup)) {
		return (RPC_CANTENCODEARGS);
	}
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
	if (s < 0) {
		syslog(LOG_ERR, "multi_call: socket: %m");
		return (RPC_FAILED);
	}

	/*
	 * Init timeouts
	 */
	utimeout = ((unsigned long long) timeout) * USECS_PER_SEC;
	subtimeout.tv_sec = (utimeout >> NRETRIES) / USECS_PER_SEC;
	subtimeout.tv_usec = (utimeout >> NRETRIES) % USECS_PER_SEC;
	tv = subtimeout;

	/*
	 * Init address info
	 */
	sin.sin_family = AF_INET;
	sin.sin_port = htons(PMAPPORT);
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	
	for (callno = 0; callno <= NRETRIES; callno++) {
		/*
		 * Send a call message to each host with the appropriate args
		 */
	  	for (serverno = 0; serverno < naddrs; serverno++) {
			call.rm_xid = trans_id + serverno;
			buflen = encodemsg(buf, sizeof(buf), &call, 
					   prognum, versnum, procnum, 
					   xdr_args, (argsvec + 
						      (serverno * argsize)));
			if (buflen == 0) {
				/*
				 * Encode failed
				 */
				continue;
			}
			sin.sin_addr = addrs[serverno];
			sendlen = sendto(s, buf, buflen, 0, 
					 (struct sockaddr *)&sin, sizeof(sin));
			if (sendlen != buflen) {
				syslog(LOG_ERR, 
				       "Cannot send multicall packet to %s: %m",
				       inet_ntoa(addrs[serverno]));
			}
		}

		/*
		 * Double the timeout from previous timeout, if necessary
		 */
		if (callno > 1) {
			tv.tv_sec *= 2;
			tv.tv_usec *= 2;
			if (tv.tv_usec >= USECS_PER_SEC) {
				tv.tv_usec -= USECS_PER_SEC;
				tv.tv_sec++;
			}
		}


#ifdef NETINFOD
		/*
		 * Check for cancel by user
		 */
		if (alert_aborted()) {
			socket_lock();
			close(s);
			socket_unlock();
			return (RPC_FAILED);
		}
#endif 
		/*
		 * Wait for reply
		 */
		FD_ZERO(&fds);
		FD_SET(s, &fds);
		switch (select(dtablesize, &fds, NULL, NULL, &tv)) {
		case -1:
			syslog(LOG_ERR, "select failure: %m");
			continue;
		case 0:
			continue;
		default:
			break;
		}

		/*
		 * Receive packet
		 */
		fromsize = sizeof(from);
		buflen = recvfrom(s, buf, sizeof(buf), 0,
				  (struct sockaddr *)&from, &fromsize);
		if (buflen < 0) {
			continue;
		}

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
		    decodemsg(&xdr, xdr_res, res)) {
			if ((*eachresult)(res, &from, 
					  reply.rm_xid - trans_id)) {
				xdr_free(xdr_res, res);
				socket_lock();
				close(s);
				socket_unlock();
				return (RPC_SUCCESS);
			}
		}
		xdr_free(xdr_res, res);
	}
	socket_lock();
	close(s);
	socket_unlock();
	return (RPC_TIMEDOUT);
}




