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
 * pmap_getport.c
 * Client interface to pmap rpc service.
 */

/*
 *	NeXT note:
 * 	The procedure pmap_getport below is derived
 *	from Sun Microsystems RPC source code.  As such the following
 *	statement applies to it.:
 *	
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#include <NetInfo/config.h>
#include <NetInfo/syslock.h>
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>
#include <sys/time.h>
#include <NetInfo/socket_lock.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>

#ifdef NeXT
#include <libc.h>
#endif

static syslock *socket_syslock = NULL;

extern int bindresvport(int, struct sockaddr_in *);

static struct timeval timeout = { 4, 0 };
static struct timeval tottimeout = { 20, 0 };
static struct timeval bind_retry = {0, 250 * 1000};	/* 1/4 second */

/*
 * Find the mapped port for program,version.
 * Calls the pmap service remotely to do the lookup.
 * Returns 0 if no map exists.
 */
static u_short
sl_pmap_getport(address, program, version, protocol)
	struct sockaddr_in *address;
	u_long program;
	u_long version;
	u_int protocol;
{
	u_short port = 0;
	int sock = -1;
	register CLIENT *client;
	struct pmap parms;

	socket_lock();
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	socket_unlock();
	if (sock < 0) return (0);

	address->sin_port = htons(PMAPPORT);
	client = clntudp_bufcreate(address, PMAPPROG,
	    PMAPVERS, timeout, &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client != (CLIENT *)NULL)
	{
		parms.pm_prog = program;
		parms.pm_vers = version;
		parms.pm_prot = protocol;
		parms.pm_port = 0;  /* not needed or used */
		if (CLNT_CALL(client, PMAPPROC_GETPORT, xdr_pmap, &parms,
		    xdr_u_short, &port, tottimeout) != RPC_SUCCESS)
		{
			rpc_createerr.cf_stat = RPC_PMAPFAILURE;
			clnt_geterr(client, &rpc_createerr.cf_error);
			port = 0;
		}
		else if (port == 0)
		{
			rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		}
	}

	socket_lock();
	if (client != NULL) clnt_destroy(client);

	close(sock);
	socket_unlock();
	address->sin_port = 0;
	return (port);
}

int
socket_close(int sock)
{
	int ret;

	socket_lock();
	ret = close(sock);
	socket_unlock();
	return (ret);
}

int
socket_connect(struct sockaddr_in *raddr, int prog, int vers)
{
	int sock;
	int errno_saved;
	bool_t got_port;
	int i;
#define MAX_RESV_RETRY 40
	
	/*
	 * If no port number given ask the pmap for one
	 */
	if (raddr->sin_port == 0) {
		u_short port;
		if ((port = sl_pmap_getport(raddr, prog, vers, 
					 IPPROTO_TCP)) == 0) {
			syslog(LOG_DEBUG,
			       "socket_open: %s",
			       clnt_spcreateerror("pmap_getport failed"));
			return (-1);
		}
		raddr->sin_port = htons(port);
	}

	socket_lock();
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	errno_saved = errno;
	socket_unlock();
	if (sock < 0) {
		syslog(LOG_WARNING,
		       "socket_connect: couldn't get socket - %s",
		       strerror(errno_saved));
		return (-1);
	}
	if (0 != bindresvport(sock, (struct sockaddr_in *)0)) {
	    if (EADDRINUSE != errno) {
		socket_close(sock);
		return(-1);	/* Just be done with the silly thing */
	    }
	    syslog(LOG_WARNING,
		   "socket_connect: retrying reserved port acquisition - %m");
	    for (i = 0, got_port = FALSE; i < MAX_RESV_RETRY; i++) {
		select(0, NULL, NULL, NULL, &bind_retry);	/* Sleep */
		if (0 == bindresvport(sock, (struct sockaddr_in *)0)) {
		    syslog(LOG_INFO,
			   "socket_connect: got reserved port after %d tr%s",
			   i + 1, i != 0 ? "ies" : "y");
		    got_port = TRUE;
		    break;
		}
	    }
	    if (!got_port) {
		syslog(LOG_ERR,
		       "socket_connect: couldn't get reserved port after "
		       "%d tries; last error was %m", i);
		(void)socket_close(sock);	/* Avoid leaking fds */
		return(-1);
	    }
	}
	if (connect(sock, (struct sockaddr *)raddr,
		    sizeof(*raddr)) < 0) {
		syslog(LOG_WARNING, "socket_connect: couldn't connect() - %m");
		socket_close(sock);
		return (-1);
	}
	return (sock);
}

int
socket_open(
	    struct sockaddr_in *raddr,
	    int prog, 
	    int vers
	    )
{
	int sock;
	int errno_saved;

	/*
	 * If no port number given ask the pmap for one
	 */
	if (raddr->sin_port == 0) {
		u_short port;
		if ((port = sl_pmap_getport(raddr, prog, vers, 
					 IPPROTO_UDP)) == 0) {
			syslog(LOG_DEBUG,
			       "socket_open: %s",
			       clnt_spcreateerror("pmap_getport failed"));
			return (-1);
		}
		raddr->sin_port = htons(port);
	}

	socket_lock();
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	errno_saved = errno;
	socket_unlock();
	if (sock < 0) {
		syslog(LOG_WARNING, "socket_open: couldn't get socket - %s",
		    strerror(errno_saved));
		return (-1);
	}

	if (0 != bindresvport(sock, (struct sockaddr_in *)0)) {
	    socket_unlock();
	    syslog(LOG_WARNING,
		   "socket_open: retrying reserved port acquisition - %m");
	    select(0, NULL, NULL, NULL, &bind_retry);	/* Sleep a bit */
	    if (0 != bindresvport(sock, (struct sockaddr_in *)0)) {
		syslog(LOG_ERR,
		       "socket_open: couldn't get reserved port - %m");
	    }
	    (void)socket_close(sock);	/* Avoid leaking fds */
	    return(-1);
	}
	return (sock);
}

void
socket_lock(void)
{
	if (socket_syslock == NULL) socket_syslock = syslock_new(FALSE);
	syslock_lock(socket_syslock);
}

void
socket_unlock(void)
{
	syslock_unlock(socket_syslock);
}
