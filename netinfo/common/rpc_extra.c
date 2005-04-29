/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
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

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)svc_udp.c 1.24 87/08/11 Copyr 1984 Sun Micro";*/
/*static char *sccsid = "from: @(#)svc_udp.c	2.2 88/07/29 4.0 RPCSRC";*/
static char *rcsid = "$Id: rpc_extra.c,v 1.4 2004/10/06 17:09:08 majka Exp $";
#endif

/*
 * svc_udp.c,
 * Server side for UDP/IP based RPC.  (Does some caching in the hopes of
 * achieving execute-at-most-once semantics.)
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <NetInfo/config.h>
#include <stdio.h>
#ifdef RPC_SUCCESS
#undef RPC_SUCCESS
#endif
#include <rpc/rpc.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <NetInfo/system_log.h>
#ifdef _OS_NEXT_
#include <libc.h>
#endif

#define rpc_buffer(xprt) ((xprt)->xp_p1)

static bool_t		svcudp_recv();
static bool_t		svcudp_reply();
static enum xprt_stat	svcudp_stat();
static bool_t		svcudp_getargs();
static bool_t		svcudp_freeargs();
static void		svcudp_destroy();

static struct xp_ops svcudp_op = {
	svcudp_recv,
	svcudp_stat,
	svcudp_getargs,
	svcudp_reply,
	svcudp_freeargs,
	svcudp_destroy
};

extern int errno;

/*
 * kept in xprt->xp_p2
 */
struct svcudp_data {
	u_int   su_iosz;	/* byte size of send.recv buffer */
	u_long	su_xid;		/* transaction id */
	XDR	su_xdrs;	/* XDR handle */
	char	su_verfbody[MAX_AUTH_BYTES];	/* verifier body */
	char * 	su_cache;	/* cached data, NULL if no cache */
};
#define	su_data(xprt)	((struct svcudp_data *)(xprt->xp_p2))


/*
 * Ops vector for TCP/IP based rpc service handle
 */
static bool_t		svctcp_recv();
static enum xprt_stat	svctcp_stat();
static bool_t		svctcp_getargs();
static bool_t		svctcp_reply();
static bool_t		svctcp_freeargs();
static void		svctcp_destroy();

static struct xp_ops svctcp_op = {
	svctcp_recv,
	svctcp_stat,
	svctcp_getargs,
	svctcp_reply,
	svctcp_freeargs,
	svctcp_destroy
};

/*
 * Ops vector for TCP/IP rendezvous handler
 */
static bool_t		rendezvous_request();
static enum xprt_stat	rendezvous_stat();

static struct xp_ops svctcp_rendezvous_op = {
	rendezvous_request,
	rendezvous_stat,
	(bool_t (*)())abort,
	(bool_t (*)())abort,
	(bool_t (*)())abort,
	svctcp_destroy
};

static int readtcp(), writetcp();
static SVCXPRT *makefd_xprt();

extern int bindresvport(int, struct sockaddr_in *);
extern int _rpc_dtablesize();

struct tcp_rendezvous { /* kept in xprt->xp_p1 */
	u_int sendsize;
	u_int recvsize;
};

struct tcp_conn {  /* kept in xprt->xp_p1 */
	enum xprt_stat strm_stat;
	u_long x_id;
	XDR xdrs;
	char verf_body[MAX_AUTH_BYTES];
};

SVCXPRT *
svcudp_bufbind(int sock, struct sockaddr_in addr, u_int sendsz, u_int recvsz)
{
	bool_t madesock = FALSE;
	SVCXPRT *xprt;
	struct svcudp_data *su;
	int len, reuse, status;

	len = sizeof(struct sockaddr_in);
	reuse = 1;

	if (sock == RPC_ANYSOCK)
	{
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock < 0)
		{
			system_log(LOG_ERR, "svcudp_bufbind - udp socket: %m");
			return NULL;
		}
		madesock = TRUE;
	}

	if (addr.sin_port != 0)
	{
		status = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int));
		if (status < 0)
		{
			system_log(LOG_ERR, "svcudp_bufbind - setsockopt: %m");
			if (madesock) close(sock);
			return NULL;
		}
		status = bind(sock, (struct sockaddr *)&addr, len);
		if (status < 0)
		{
			system_log(LOG_ERR, "svcudp_bufbind - bind: %m");
			if (madesock) close(sock);
			return NULL;
		}
	}
	else if (bindresvport(sock, &addr))
	{
		addr.sin_port = 0;
		status = bind(sock, (struct sockaddr *)&addr, len);
		if (status < 0)
		{
			system_log(LOG_ERR, "svcudp_bufbind - bind: %m");
			if (madesock) close(sock);
			return NULL;
		}
	}

	if (getsockname(sock, (struct sockaddr *)&addr, &len) != 0)
	{
		system_log(LOG_ERR, "svcudp_bufbind - getsockname %m");
		if (madesock) close(sock);
		return NULL;
	}

	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL)
	{
		system_log(LOG_ERR, "svcudp_bufbind: out of memory");
		return NULL;
	}

	su = (struct svcudp_data *)mem_alloc(sizeof(*su));
	if (su == NULL)
	{
		system_log(LOG_ERR, "svcudp_bufbind: out of memory");
		return NULL;
	}

	su->su_iosz = ((MAX(sendsz, recvsz) + 3) / 4) * 4;
	if ((rpc_buffer(xprt) = mem_alloc(su->su_iosz)) == NULL)
	{
		system_log(LOG_ERR, "svcudp_bufbind: out of memory");
		return NULL;
	}

	xdrmem_create(&(su->su_xdrs), rpc_buffer(xprt), su->su_iosz, XDR_DECODE);
	su->su_cache = NULL;
	xprt->xp_p2 = (caddr_t)su;
	xprt->xp_verf.oa_base = su->su_verfbody;
	xprt->xp_ops = &svcudp_op;
	xprt->xp_port = ntohs(addr.sin_port);
	xprt->xp_sock = sock;
	xprt_register(xprt);

	return xprt;
}

SVCXPRT *
svcudp_bind(int sock, struct sockaddr_in addr)
{
	return(svcudp_bufbind(sock, addr, UDPMSGSIZE, UDPMSGSIZE));
}

static enum xprt_stat
svcudp_stat(xprt)
	SVCXPRT *xprt;
{

	return (XPRT_IDLE); 
}

static bool_t
svcudp_recv(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct svcudp_data *su = su_data(xprt);
	XDR *xdrs = &(su->su_xdrs);
	int rlen;
	char *reply;
	u_long replylen;
	static int cache_get();

    again:
	xprt->xp_addrlen = sizeof(struct sockaddr_in);
	rlen = recvfrom(xprt->xp_sock, rpc_buffer(xprt), (int) su->su_iosz,
	    0, (struct sockaddr *)&(xprt->xp_raddr), &(xprt->xp_addrlen));
	if (rlen == -1 && errno == EINTR)
		goto again;
	if (rlen < 4*sizeof(u_long))
		return (FALSE);
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	if (! xdr_callmsg(xdrs, msg))
		return (FALSE);
	su->su_xid = msg->rm_xid;
	if (su->su_cache != NULL) {
		if (cache_get(xprt, msg, &reply, &replylen)) {
			(void) sendto(xprt->xp_sock, reply, (int) replylen, 0,
			  (struct sockaddr *) &xprt->xp_raddr, xprt->xp_addrlen);
			return (TRUE);
		}
	}
	return (TRUE);
}

static bool_t
svcudp_reply(xprt, msg)
	SVCXPRT *xprt; 
	struct rpc_msg *msg; 
{
	struct svcudp_data *su = su_data(xprt);
	XDR *xdrs = &(su->su_xdrs);
	int slen;
	bool_t stat = FALSE;
	static void cache_set();

	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	msg->rm_xid = su->su_xid;
	if (xdr_replymsg(xdrs, msg)) {
		slen = (int)XDR_GETPOS(xdrs);
		if (sendto(xprt->xp_sock, rpc_buffer(xprt), slen, 0,
		    (struct sockaddr *)&(xprt->xp_raddr), xprt->xp_addrlen)
		    == slen) {
			stat = TRUE;
			if (su->su_cache && slen >= 0) {
				cache_set(xprt, (u_long) slen);
			}
		}
	}
	return (stat);
}

static bool_t
svcudp_getargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{

	return ((*xdr_args)(&(su_data(xprt)->su_xdrs), args_ptr));
}

static bool_t
svcudp_freeargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	XDR *xdrs = &(su_data(xprt)->su_xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
}

static void
svcudp_destroy(xprt)
	SVCXPRT *xprt;
{
	struct svcudp_data *su = su_data(xprt);

	xprt_unregister(xprt);
	(void)close(xprt->xp_sock);
	XDR_DESTROY(&(su->su_xdrs));
	mem_free(rpc_buffer(xprt), su->su_iosz);
	mem_free((caddr_t)su, sizeof(struct svcudp_data));
	mem_free((caddr_t)xprt, sizeof(SVCXPRT));
}


/***********this could be a separate file*********************/

/*
 * Fifo cache for udp server
 * Copies pointers to reply buffers into fifo cache
 * Buffers are sent again if retransmissions are detected.
 */

#define SPARSENESS 4	/* 75% sparse */

#define CACHE_PERROR(msg)	\
	system_log(LOG_ERR, "%s", msg)

#define ALLOC(type, size)	\
	(type *) mem_alloc((unsigned) (sizeof(type) * (size)))

#define BZERO(addr, type, size)	 \
	bzero((char *) addr, sizeof(type) * (int) (size)) 

/*
 * An entry in the cache
 */
typedef struct cache_node *cache_ptr;
struct cache_node {
	/*
	 * Index into cache is xid, proc, vers, prog and address
	 */
	u_long cache_xid;
	u_long cache_proc;
	u_long cache_vers;
	u_long cache_prog;
	struct sockaddr_in cache_addr;
	/*
	 * The cached reply and length
	 */
	char * cache_reply;
	u_long cache_replylen;
	/*
 	 * Next node on the list, if there is a collision
	 */
	cache_ptr cache_next;	
};



/*
 * The entire cache
 */
struct udp_cache {
	u_long uc_size;		/* size of cache */
	cache_ptr *uc_entries;	/* hash table of entries in cache */
	cache_ptr *uc_fifo;	/* fifo list of entries in cache */
	u_long uc_nextvictim;	/* points to next victim in fifo list */
	u_long uc_prog;		/* saved program number */
	u_long uc_vers;		/* saved version number */
	u_long uc_proc;		/* saved procedure number */
	struct sockaddr_in uc_addr; /* saved caller's address */
};


/*
 * the hashing function
 */
#define CACHE_LOC(transp, xid)	\
 (xid % (SPARSENESS*((struct udp_cache *) su_data(transp)->su_cache)->uc_size))	


#ifdef NOTDEF
/*
 * Enable use of the cache. 
 * Note: there is no disable.
 */
int
svcudp_enablecache(transp, size)
	SVCXPRT *transp;
	u_long size;
{
	struct svcudp_data *su = su_data(transp);
	struct udp_cache *uc;

	if (su->su_cache != NULL) {
		CACHE_PERROR("enablecache: cache already enabled");
		return(0);	
	}
	uc = ALLOC(struct udp_cache, 1);
	if (uc == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache");
		return(0);
	}
	uc->uc_size = size;
	uc->uc_nextvictim = 0;
	uc->uc_entries = ALLOC(cache_ptr, size * SPARSENESS);
	if (uc->uc_entries == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache data");
		return(0);
	}
	BZERO(uc->uc_entries, cache_ptr, size * SPARSENESS);
	uc->uc_fifo = ALLOC(cache_ptr, size);
	if (uc->uc_fifo == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache fifo");
		return(0);
	}
	BZERO(uc->uc_fifo, cache_ptr, size);
	su->su_cache = (char *) uc;
	return(1);
}
#endif


/*
 * Set an entry in the cache
 */
static void
cache_set(xprt, replylen)
	SVCXPRT *xprt;
	u_long replylen;	
{
	cache_ptr victim;	
	cache_ptr *vicp;
	struct svcudp_data *su = su_data(xprt);
	struct udp_cache *uc = (struct udp_cache *) su->su_cache;
	u_int loc;
	char *newbuf;

	/*
 	 * Find space for the new entry, either by
	 * reusing an old entry, or by mallocing a new one
	 */
	victim = uc->uc_fifo[uc->uc_nextvictim];
	if (victim != NULL) {
		loc = CACHE_LOC(xprt, victim->cache_xid);
		for (vicp = &uc->uc_entries[loc]; 
		  *vicp != NULL && *vicp != victim; 
		  vicp = &(*vicp)->cache_next) 
				;
		if (*vicp == NULL) {
			CACHE_PERROR("cache_set: victim not found");
			return;
		}
		*vicp = victim->cache_next;	/* remote from cache */
		newbuf = victim->cache_reply;
	} else {
		victim = ALLOC(struct cache_node, 1);
		if (victim == NULL) {
			CACHE_PERROR("cache_set: victim alloc failed");
			return;
		}
		newbuf = mem_alloc(su->su_iosz);
		if (newbuf == NULL) {
			CACHE_PERROR("cache_set: could not allocate new rpc_buffer");
			return;
		}
	}

	/*
	 * Store it away
	 */
	victim->cache_replylen = replylen;
	victim->cache_reply = rpc_buffer(xprt);
	rpc_buffer(xprt) = newbuf;
	xdrmem_create(&(su->su_xdrs), rpc_buffer(xprt), su->su_iosz, XDR_ENCODE);
	victim->cache_xid = su->su_xid;
	victim->cache_proc = uc->uc_proc;
	victim->cache_vers = uc->uc_vers;
	victim->cache_prog = uc->uc_prog;
	victim->cache_addr = uc->uc_addr;
	loc = CACHE_LOC(xprt, victim->cache_xid);
	victim->cache_next = uc->uc_entries[loc];	
	uc->uc_entries[loc] = victim;
	uc->uc_fifo[uc->uc_nextvictim++] = victim;
	uc->uc_nextvictim %= uc->uc_size;
}

/*
 * Try to get an entry from the cache
 * return 1 if found, 0 if not found
 */
static int
cache_get(xprt, msg, replyp, replylenp)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
	char **replyp;
	u_long *replylenp;
{
	u_int loc;
	cache_ptr ent;
	struct svcudp_data *su = su_data(xprt);
	struct udp_cache *uc = (struct udp_cache *) su->su_cache;

#	define EQADDR(a1, a2)	(bcmp((char*)&a1, (char*)&a2, sizeof(a1)) == 0)

	loc = CACHE_LOC(xprt, su->su_xid);
	for (ent = uc->uc_entries[loc]; ent != NULL; ent = ent->cache_next) {
		if (ent->cache_xid == su->su_xid &&
		  ent->cache_proc == uc->uc_proc &&
		  ent->cache_vers == uc->uc_vers &&
		  ent->cache_prog == uc->uc_prog &&
		  EQADDR(ent->cache_addr, uc->uc_addr)) {
			*replyp = ent->cache_reply;
			*replylenp = ent->cache_replylen;
			return(1);
		}
	}
	/*
	 * Failed to find entry
	 * Remember a few things so we can do a set later
	 */
	uc->uc_proc = msg->rm_call.cb_proc;
	uc->uc_vers = msg->rm_call.cb_vers;
	uc->uc_prog = msg->rm_call.cb_prog;
	uc->uc_addr = xprt->xp_raddr;
	return(0);
}

/*
 * Usage:
 *	xprt = svctcp_create(sock, send_buf_size, recv_buf_size);
 *
 * Creates, registers, and returns a (rpc) tcp based transporter.
 * Once *xprt is initialized, it is registered as a transporter
 * see (svc.h, xprt_register).  This routine returns
 * a NULL if a problem occurred.
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svctcp_create
 * binds it to an arbitrary port.  The routine then starts a tcp
 * listener on the socket's associated port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 *
 * Since tcp streams do buffered io similar to stdio, the caller can specify
 * how big the send and receive buffers are via the second and third parms;
 * 0 => use the system default.
 */
SVCXPRT *
svctcp_bind(int sock, struct sockaddr_in s, u_int sendsize, u_int recvsize)
{
	struct sockaddr_in name;
	bool_t madesock = FALSE;
	SVCXPRT *xprt;
	struct tcp_rendezvous *r;
	int len, reuse, status;
	
	reuse = 1;

	if (sock == RPC_ANYSOCK)
	{
		sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (sock < 0)
		{
			system_log(LOG_ERR, "svctcp_bind - tcp socket: %m");
			return NULL;
		}
		madesock = TRUE;
	}

	if (s.sin_port != 0)
	{
		status = setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int));
		if (status < 0)
		{
			system_log(LOG_ERR, "svctcp_bind - setsockopt: %m");
			if (madesock) close(sock);
			return NULL;
		}
		status = bind(sock, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
		if (status < 0)
		{
			system_log(LOG_ERR, "svctcp_bind - bind: %m");
			if (madesock) close(sock);
			return NULL;
		}
	}
	else if (bindresvport(sock, &s))
	{
		s.sin_port = 0;
		status = bind(sock, (struct sockaddr *)&s, sizeof(struct sockaddr_in));
		if (status < 0)
		{
			system_log(LOG_ERR, "svctcp_bind - bind: %m");
			if (madesock) close(sock);
			return NULL;
		}
	}

	len = sizeof(struct sockaddr_in);
	if ((getsockname(sock, (struct sockaddr *)&name, &len) != 0) || (listen(sock, 2) != 0))
	{
		system_log(LOG_ERR, "svctcp_bind - getsockname / listen: %m");
		if (madesock) close(sock);
		return NULL;
	}

	r = (struct tcp_rendezvous *)mem_alloc(sizeof(*r));
	if (r == NULL)
	{
		system_log(LOG_ERR, "svctcp_bind: out of memory");
		if (madesock) close(sock);
		return NULL;
	}

	r->sendsize = sendsize;
	r->recvsize = recvsize;

	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL)
	{
		system_log(LOG_ERR, "svctcp_bind: out of memory");
		mem_free(r, sizeof(struct tcp_rendezvous));
		if (madesock) close(sock);
		return NULL;
	}

	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)r;
	xprt->xp_verf = _null_auth;
	xprt->xp_ops = &svctcp_rendezvous_op;
	xprt->xp_port = ntohs(name.sin_port);
	xprt->xp_sock = sock;
	xprt_register(xprt);

	return xprt;
}

#ifdef NOTDEF
/*
 * Like svtcp_create(), except the routine takes any *open* UNIX file
 * descriptor as its first input.
 */
SVCXPRT *
svcfd_create(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{

	return (makefd_xprt(fd, sendsize, recvsize));
}
#endif

static SVCXPRT *
makefd_xprt(fd, sendsize, recvsize)
	int fd;
	u_int sendsize;
	u_int recvsize;
{
	SVCXPRT *xprt;
	struct tcp_conn *cd;
 
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == (SVCXPRT *)NULL) {
		system_log(LOG_ERR, "ni_svc_tcp: makefd_xprt: out of memory");
		goto done;
	}
	cd = (struct tcp_conn *)mem_alloc(sizeof(struct tcp_conn));
	if (cd == (struct tcp_conn *)NULL) {
		system_log(LOG_ERR, "ni_svc_tcp: makefd_xprt: out of memory");
		mem_free((char *) xprt, sizeof(SVCXPRT));
		xprt = (SVCXPRT *)NULL;
		goto done;
	}
	cd->strm_stat = XPRT_IDLE;
	xdrrec_create(&(cd->xdrs), sendsize, recvsize,
	    (caddr_t)xprt, readtcp, writetcp);
	xprt->xp_p2 = NULL;
	xprt->xp_p1 = (caddr_t)cd;
	xprt->xp_verf.oa_base = cd->verf_body;
	xprt->xp_addrlen = 0;
	xprt->xp_ops = &svctcp_op;  /* truely deals with calls */
	xprt->xp_port = 0;  /* this is a connection, not a rendezvouser */
	xprt->xp_sock = fd;
	xprt_register(xprt);
    done:
	return (xprt);
}

static bool_t
rendezvous_request(xprt)
	SVCXPRT *xprt;
{
	int sock;
	struct tcp_rendezvous *r;
	struct sockaddr_in addr;
	int len;
	int dontblock;

	r = (struct tcp_rendezvous *)xprt->xp_p1;
    again:
	len = sizeof(struct sockaddr_in);
	if ((sock = accept(xprt->xp_sock, (struct sockaddr *)&addr,
	    &len)) < 0) {
		if (errno == EINTR)
			goto again;
	       return (FALSE);
	}

	dontblock = 1;
	(void)ioctl(sock, FIONBIO, &dontblock);

	/*
	 * make a new transporter (re-uses xprt)
	 */
	xprt = makefd_xprt(sock, r->sendsize, r->recvsize);
	xprt->xp_raddr = addr;
	xprt->xp_addrlen = len;
	return (FALSE); /* there is never an rpc msg to be processed */
}

static enum xprt_stat
rendezvous_stat()
{

	return (XPRT_IDLE);
}

static void
svctcp_destroy(SVCXPRT *xprt)
{
	struct tcp_conn *cd = (struct tcp_conn *)xprt->xp_p1;

	xprt_unregister(xprt);
	(void)close(xprt->xp_sock);
	if (xprt->xp_port != 0) {
		/* a rendezvouser socket */
		xprt->xp_port = 0;
	} else {
		/* an actual connection socket */
		XDR_DESTROY(&(cd->xdrs));
	}
	mem_free((caddr_t)cd, sizeof(struct tcp_conn));
	mem_free((caddr_t)xprt, sizeof(SVCXPRT));
}

/*
 * All read operations timeout after 35 seconds.
 * A timeout is fatal for the connection.
 */
static struct timeval wait_per_try = { 35, 0 };

/*
 * reads data from the tcp conection.
 * any error is fatal and the connection is closed.
 * (And a read of zero bytes is a half closed stream => error.)
 */
static int
readtcp(xprt, buf, len)
	SVCXPRT *xprt;
	caddr_t buf;
	int len;
{
	int sock = xprt->xp_sock;
#ifdef FD_SETSIZE
	fd_set mask;
	fd_set readfds;

	if (((struct tcp_conn *)(xprt->xp_p1))->strm_stat == XPRT_DIED) {
		return (-1);
	}
	FD_ZERO(&mask);
	FD_SET(sock, &mask);
#else
	int mask = 1 << sock;
	int readfds;
#endif /* def FD_SETSIZE */
	do {
		readfds = mask;
		if (select(_rpc_dtablesize(), &readfds, (fd_set*)NULL, (fd_set*)NULL, 
			   &wait_per_try) <= 0) {
			if (errno == EINTR) {
				continue;
			}
			goto fatal_err;
		}
#ifdef FD_SETSIZE
	} while (!FD_ISSET(sock, &readfds));
#else
	} while (readfds != mask);
#endif /* def FD_SETSIZE */
	if ((len = read(sock, buf, len)) > 0) {
		return (len);
	}
fatal_err:
	((struct tcp_conn *)(xprt->xp_p1))->strm_stat = XPRT_DIED;
	return (-1);
}

/*
 * writes data to the tcp connection.
 * Any error is fatal and the connection is closed.
 */
static int
writetcp(xprt, buf, len)
	SVCXPRT *xprt;
	caddr_t buf;
	int len;
{
	int sock = xprt->xp_sock;
	int i, cnt;
	fd_set mask;
	fd_set writefds;

	if (((struct tcp_conn *)(xprt->xp_p1))->strm_stat == XPRT_DIED) {
		return (-1);
	}
	for (cnt = len; cnt > 0; cnt -= i, buf += i) {
		FD_ZERO(&mask);
		FD_SET(sock, &mask);
		do {
			writefds = mask;
			if (select(_rpc_dtablesize(), (fd_set *)NULL, &writefds, 
				   (fd_set*)NULL, &wait_per_try) <= 0) {
				if (errno == EINTR) {
					continue;
				}
				goto fatal_err;
			}
		} while (!FD_ISSET(sock, &writefds));
		if ((i = write(xprt->xp_sock, buf, cnt)) < 0) {
			goto fatal_err;
		}
	}
	return (len);
fatal_err:
	((struct tcp_conn *)(xprt->xp_p1))->strm_stat = XPRT_DIED;
	return (-1);
}

static enum xprt_stat
svctcp_stat(xprt)
	SVCXPRT *xprt;
{
	struct tcp_conn *cd = (struct tcp_conn *)(xprt->xp_p1);

	if (cd->strm_stat == XPRT_DIED)
		return (XPRT_DIED);
	if (! xdrrec_eof(&(cd->xdrs)))
		return (XPRT_MOREREQS);
	return (XPRT_IDLE);
}

static bool_t
svctcp_recv(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct tcp_conn *cd = (struct tcp_conn *)(xprt->xp_p1);
	XDR *xdrs = &(cd->xdrs);

	xdrs->x_op = XDR_DECODE;
	(void)xdrrec_skiprecord(xdrs);
	if (xdr_callmsg(xdrs, msg)) {
		cd->x_id = msg->rm_xid;
		return (TRUE);
	}
	return (FALSE);
}

static bool_t
svctcp_getargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{

	return ((*xdr_args)(&(((struct tcp_conn *)(xprt->xp_p1))->xdrs), args_ptr));
}

static bool_t
svctcp_freeargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	XDR *xdrs = &(((struct tcp_conn *)(xprt->xp_p1))->xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
}

static bool_t
svctcp_reply(xprt, msg)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	struct tcp_conn *cd = (struct tcp_conn *)(xprt->xp_p1);
	XDR *xdrs = &(cd->xdrs);
	bool_t stat;

	xdrs->x_op = XDR_ENCODE;
	msg->rm_xid = cd->x_id;
	stat = xdr_replymsg(xdrs, msg);
	(void)xdrrec_endofrecord(xdrs, TRUE);
	return (stat);
}
