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
 * Glues the library routines to the stub routines
 * Copyright (C) 1989 by NeXT, Inc.
 */
#include <libc.h>
#include <syslog.h>
#include <netinfo/ni.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>
#include <rpc/xdr.h>
#include <net/if.h>
#include <ctype.h>
#include "clib.h"
#include "sys_interfaces.h"

#define LOCAL_PORT 1033

#define NI_TIMEOUT_SHORT 5	/* 5 second timeout for transactions */
#define NI_TIMEOUT_LONG 60 	/* 60 second timeout for writes */
#define NI_TRIES   5		/* number of retries per timeout (udp only) */
#define NI_SLEEPTIME 4	 	/* 4 second sleeptime, in case of errors */
#define NI_MAXSLEEPTIME 64 	/* 64 second max sleep time */
#define NI_MAXCONNTRIES 2 	/* Try to form a connection twice before sleeping */

/* Hack for determining if an IP address is a broadcast address. -GRS */
/* Note that addr is network byte order (big endian) - BKM */
 
#define IS_BROADCASTADDR(addr)	(((unsigned char *) &addr)[0] == 0xFF)

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK		(u_long)0x7f000001
#endif
#define debug(msg) syslog(LOG_ERR, msg)

#define clnt_debug(ni, msg)  /* do nothing */

typedef struct ni_private {
	int naddrs;		/* number of addresses */
	struct in_addr *addrs;	/* addresses of servers - network byte order */
	int whichwrite;	/* which one of the above is the master */
	ni_name *tags;		/* tags of servers */
	int pid;		/* pid, to detect forks */
	int tsock;		/* tcp socket */
	int tport;		/* tcp local port name - host byte order */
	CLIENT *tc;		/* tcp client */
	long tv_sec;		/* timeout for this call */
	long rtv_sec;		/* read timeout - 0 if default */
	long wtv_sec;		/* write timeout - 0 if default */
	int abort;		/* abort on timeout? */
	int needwrite;		/* need to lock writes? */
	int uid;		/* user id */
	ni_name passwd;		/* password */
} ni_private;

#define NIP(ni) ((ni_private *)(ni))

static const ni_name NAME_NAME = "name";
static const ni_name NAME_SERVES = "serves";
static const ni_name NAME_MACHINES = "machines";
static const ni_name NAME_IP_ADDRESS = "ip_address";
static const ni_name NAME_MASTER = "master";
static const ni_name NAME_USERS = "users";
static const ni_name NAME_UID = "uid";

typedef struct getreg_stuff {
	nibind_getregister_res res;
	ni_private *ni;
} getreg_stuff;
	

static int socket_open(struct sockaddr_in *raddr, int, int, int, int, int);


/*
 * Keep track of our port, in case somebody closes our socket
 * on us.
 */
static int
getmyport(
	  int sock
	  )
{
	struct sockaddr_in sin;
	int sinlen;

	sinlen = sizeof(sin);
	if (getsockname(sock, (struct sockaddr *)&sin, &sinlen) == 0) {
		if (sin.sin_port == 0) {
			(void)bind(sock, (struct sockaddr *)&sin, sizeof(sin));
			sinlen = sizeof(sin);
			(void)getsockname(sock, (struct sockaddr *)&sin,
					  &sinlen);
		}
		return (ntohs(sin.sin_port));
	}
	return (-1);
}


static void
createauth(
	   ni_private *ni
	   )
{
	if (ni->passwd != NULL && ni->tc != NULL) {
		auth_destroy(ni->tc->cl_auth);
		ni->tc->cl_auth = authunix_create(ni->passwd, ni->uid, 0, 0, 
						  NULL);
	}
}


static void
fixtimeout(
	   struct timeval *tv,
	   long sec,
	   int tries
	   )
{
	tv->tv_sec = sec / tries;
	tv->tv_usec = ((sec % tries) * 1000000) / tries;
}


static void
ni_settimeout(
	      ni_private *ni,
	      int timeout
	      )
{
	struct timeval tv;

	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	ni->tv_sec = timeout;
	if (ni->tc != NULL) {
		clnt_control(ni->tc, CLSET_TIMEOUT, &tv);
	}
}


/*
 * Connect to a given address/tag
 */
static int
connectit(ni_private *ni)
{
	struct sockaddr_in sin;
	int sock;
	CLIENT *cl;
	struct timeval tv;
	enum clnt_stat stat;
	nibind_getregister_res res;

	sock = -1;
	bzero(&sin, sizeof(sin));
	sin.sin_port = 0;
	sin.sin_family = AF_INET;
	
	tv.tv_sec = ni->rtv_sec == 0 ? NI_TIMEOUT_SHORT : ni->rtv_sec;
	tv.tv_usec = 0;
	
	ni_settimeout(ni, tv.tv_sec);
	fixtimeout(&tv, ni->tv_sec, NI_TRIES);

	/*
	 * If connecting to local domain, try using the "well-known" port first.
	 */
	if (!strcmp(ni->tags[0], "local"))
	{
		interface_list_t *ilist;

		ilist = sys_interfaces();
		if (sys_is_my_address(ilist, &ni->addrs[0]))
		{
			sin.sin_port = htons(LOCAL_PORT);
			sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			sock = socket_open(&sin, NI_PROG, NI_VERS, ni->tv_sec, NI_TRIES, IPPROTO_TCP);
		}
		sys_interfaces_release(ilist);
	}

	/*
	 * If connecting to a domain other than the local domain,
	 * or if connection to local didn't work with local's well-known port,
	 * then go through portmap & nibindd to find the port and connect.
	 */
	if (sock < 0)
	{
		sin.sin_port = 0;
		sin.sin_addr = ni->addrs[0];

		sock = socket_open(&sin, NIBIND_PROG, NIBIND_VERS, ni->tv_sec, NI_TRIES, IPPROTO_UDP);
		if (sock < 0) return (0);

		cl = clntudp_create(&sin, NIBIND_PROG, NIBIND_VERS, tv, &sock);
		if (cl == NULL)
		{
			close(sock);
			return (0);
		}

		tv.tv_sec = ni->rtv_sec == 0 ? NI_TIMEOUT_SHORT : ni->rtv_sec;
		tv.tv_usec = 0;

		stat = clnt_call(cl, NIBIND_GETREGISTER, xdr_ni_name, &ni->tags[0], xdr_nibind_getregister_res, &res, tv);
		clnt_destroy(cl);
		close(sock);
		if (stat != RPC_SUCCESS || res.status != NI_OK) return (0);
	
		sin.sin_port = htons(res.nibind_getregister_res_u.addrs.tcp_port);
		sock = socket_open(&sin, NI_PROG, NI_VERS, ni->tv_sec, NI_TRIES, IPPROTO_TCP);
	}

	if (sock < 0) return (0);

	cl = clnttcp_create(&sin, NI_PROG, NI_VERS, &sock, 0, 0);
	if (cl == NULL)
	{
		close(sock);
		return (0);
	}

	clnt_control(cl, CLSET_TIMEOUT, &tv);
	ni->tc = cl;
	ni->tsock = sock;
	ni->tport = getmyport(sock);
	createauth(ni);
	fcntl(ni->tsock, F_SETFD, 1);
	return (1);
}


void
ni_setabort(
	    void *ni,
	    int abort
	    )
{
	((ni_private *)ni)->abort = abort;
}


void
ni_setwritetimeout(
		   void *ni,
		   int timeout
		   )
{
	((ni_private *)ni)->wtv_sec = timeout;
}


void
ni_setreadtimeout(
		  void *ni,
		  int timeout
		  )
{
	((ni_private *)ni)->rtv_sec = timeout;
}


void
ni_needwrite(
	     void *ni,
	     int needwrite
	     )
{
	((ni_private *)ni)->needwrite = needwrite;
}


/*
 * Returns a client handle to the NetInfo server, if it's running
 */
static int
connectlocal(ni_private *ni)
{
	int printed = 0;

	ni->naddrs = 1;
	ni->addrs = (struct in_addr *)malloc(sizeof(struct in_addr));
	ni->addrs[0].s_addr = htonl(INADDR_LOOPBACK);
	ni->tags = (ni_name *)malloc(sizeof(ni_name));
	ni->tags[0] = ni_name_dup("local");
	ni->whichwrite = 0;

	while (!connectit(ni))
	{
		if (!printed)
		{
			syslog(LOG_ERR, "NetInfo timeout connecting to local domain, sleeping");
			printed++;
		}

		sleep(NI_SLEEPTIME);
		/* wait forever */
	}

	if (printed)
	{
		syslog(LOG_ERR, "NetInfo connection to local domain waking");
	}

	return (1);
}


/*
 * Destroy the client handle
 */
static void
clnt_kill(
	  CLIENT *cl,
	  int sock,
	  int port
	  )
{
	int save = 0;

	if (sock >= 0 && getmyport(sock) != port) {
		/*
		 * Somebody else has the descriptor open. Do not close it,
		 * it's not ours.
		 */
		save++;
	}
	if (cl != NULL) {
		auth_destroy(cl->cl_auth);
		clnt_destroy(cl);
	}
	if (!save) {
		/*
		 * It's ours and we can close it
		 */
		(void)close(sock);
	}
}


/*
 * Reinitialize everything
 */
static void
reinit(
       ni_private *ni
       )
{
	if (ni->tc != NULL) {
		clnt_kill(ni->tc, ni->tsock, ni->tport);
		ni->tc = NULL;
	}
	ni->tsock = -1;
	ni->tport = -1;
	ni->pid = getpid();
}


/*
 * Switch to a new server
 */
static void
ni_switch(
	  ni_private *ni,
	  ni_index which
	  )
{
	struct in_addr tmp_addr;
	ni_name tmp_tag;

	if (which == 0) {
		return;
	}
	reinit(ni);
	tmp_addr = ni->addrs[0];
	tmp_tag = ni->tags[0];

	ni->addrs[0] = ni->addrs[which];
	ni->tags[0] = ni->tags[which];
	
	ni->addrs[which] = tmp_addr;
	ni->tags[which] = tmp_tag;

	if (ni->whichwrite == 0) {
		ni->whichwrite = which;
	}
	else if (ni->whichwrite == which) {
		ni->whichwrite = 0;
	}
}


/*
 * Swap two servers' positions
 */
static void
ni_swap(
	  ni_private *ni,
	  ni_index a,
	  ni_index b
	  )
{
	struct in_addr tmp_addr;
	ni_name tmp_tag;

	if (a == b) return;

	tmp_addr = ni->addrs[a];
	tmp_tag = ni->tags[a];

	ni->addrs[a] = ni->addrs[b];
	ni->tags[a] = ni->tags[b];
	
	ni->addrs[b] = tmp_addr;
	ni->tags[b] = tmp_tag;

	if (ni->whichwrite == a) {
		ni->whichwrite = b;
	}
	else if (ni->whichwrite == b) {
		ni->whichwrite = a;
	}
}


/*
 * Callback routine for multi_call
 * XXX: should save returned port numbers
 */
static bool_t
eachresult(
	   void *vstuff,
	   struct sockaddr_in *sin,
	   int which
	   )
{
	getreg_stuff *stuff = (getreg_stuff *)vstuff;
	
	if (stuff->res.status != NI_OK) {
		return (FALSE);
	}
	ni_switch(stuff->ni, which);
	return (TRUE);
}


/*
 * shuffle addresses
 */
static void
shuffle(ni_private *ni)
{
	int *shuffle;
	int i, j;
	int rfd;

	if (ni->naddrs <= 1) return;

	rfd = open("/dev/random", O_RDONLY, 0);
	shuffle = (int *)malloc(ni->naddrs * sizeof(int));
	for (i = 0; i < ni->naddrs; i++) shuffle[i] = i;
	for (i = 0, j = ni->naddrs; j > 0; i++, j--) {
		unsigned int rEnt;
		long rVal;
		int tEnt;

		/* get a random number */
		if ((rfd < 0) ||
		    (read(rfd, &rVal, sizeof(rVal)) != sizeof(rVal))) {
			/* if we could not read from /dev/random */
			static int initialized = 0;
			if (!initialized)
			{
				srandom(gethostid() ^ time(NULL));
				initialized++;
			}
			rVal = random();
		}

		rEnt = (unsigned int)rVal % j;	/* pick one of the remaining entries */
		tEnt = shuffle[rEnt];		/* grab the random entry */
		shuffle[rEnt] = shuffle[j-1];	/* the last entry moves to the random slot */ 
		shuffle[j-1]  = tEnt;		/* the last slot gets the random entry */
		ni_swap(ni, rEnt, j-1);		/* and swap the actual NI addresses */
	}
	free(shuffle);
	if (rfd > 0) (void)close(rfd);
	return;
}


static int
rebind(
       ni_private *ni
       )
{
	enum clnt_stat stat;
	getreg_stuff stuff;
	int sleeptime = NI_SLEEPTIME;
	int printed = 0;
	int nlocal;
	int nnetwork;
	interface_list_t *ilist;
	int i;

	if (ni->naddrs == 1) {
		ni->whichwrite = 0;
		return (1);
	}

	/*
	 * Majka - 1994.04.27
	 * re-order the servers so that:
	 * servers on the local host are at the start of the list, then
	 * servers on the local network are next, then
	 * all other servers are next
	 */

	ilist = sys_interfaces();

	/*
	 * shuffle addresses
	 */
	shuffle(ni);

	/*
	 * move local servers to the head of the list
	 */
	nlocal = 0;
	for (i = nlocal; i < ni->naddrs; i++) {
		if (sys_is_my_address(ilist, &ni->addrs[i]))
		{
			ni_swap(ni, nlocal, i);
			nlocal++;
		}
	}

	/*
	 * move servers on this network to follow local servers
	 */
	nnetwork = nlocal;
 	for (i = nnetwork; i < ni->naddrs; i++) {
		if (sys_is_my_network(ilist, &ni->addrs[i]) ||
			IS_BROADCASTADDR(ni->addrs[i].s_addr))
		{
			ni_swap(ni, nnetwork, i);
			nnetwork++;
		}
	}

	sys_interfaces_release(ilist);

	stuff.ni = ni;
	for (;;) {
		/*
		 * call local servers first
		 */
		if (nlocal > 0) {
			for (i = 0; i < nlocal; i++) {
				syslog(LOG_DEBUG, "NetInfo connect call to: %s/%s (local %d)",
					inet_ntoa(ni->addrs[i]), ni->tags[i], i);
			}
			stat = multi_call(nlocal, ni->addrs,
				  NIBIND_PROG, NIBIND_VERS, NIBIND_GETREGISTER,
				  xdr_ni_name, ni->tags,
				  sizeof(ni_name),
				  xdr_nibind_getregister_res,
				  &stuff, eachresult, 
				  NI_TIMEOUT_SHORT);
			if (stat == RPC_SUCCESS) {
				break;
			}
		}

		/*
		 * call local servers and this network's servers
		 */
		if (nnetwork > nlocal) {
			for (i = 0; i < nnetwork; i++) {
				syslog(LOG_DEBUG, "NetInfo connect call to: %s/%s (network %d)",
					inet_ntoa(ni->addrs[i]), ni->tags[i], i);
			}
			stat = multi_call(nnetwork, ni->addrs,
				  NIBIND_PROG, NIBIND_VERS, NIBIND_GETREGISTER,
				  xdr_ni_name, ni->tags,
				  sizeof(ni_name),
				  xdr_nibind_getregister_res,
				  &stuff, eachresult, 
				  NI_TIMEOUT_SHORT);
			if (stat == RPC_SUCCESS) {
				break;
			}
		}

		/*
		 * call all servers
		 */
		for (i = 0; i < ni->naddrs; i++) {
			syslog(LOG_DEBUG, "NetInfo connect call to: %s/%s (world %d)",
			inet_ntoa(ni->addrs[i]), ni->tags[i], i);
		}
		stat = multi_call(ni->naddrs,
				  ni->addrs, NIBIND_PROG, NIBIND_VERS, 
				  NIBIND_GETREGISTER,
				  xdr_ni_name, ni->tags,
				  sizeof(ni_name),
				  xdr_nibind_getregister_res,
				  &stuff, eachresult, 
				  ni->rtv_sec == 0 ? NI_TIMEOUT_SHORT : ni->rtv_sec);
		if (stat == RPC_SUCCESS) {
			break;
		}

		if (ni->abort) {
			return (0);
		}
		if (!printed) {
			if (ni->whichwrite >= 0) {
				syslog(LOG_ERR, 
					"NetInfo connect timeout (domain with master %s/%s), sleeping",
					inet_ntoa(ni->addrs[ni->whichwrite]), ni->tags[ni->whichwrite]);
			}
			else {
				syslog(LOG_ERR, "NetInfo connect timeout (domain with server %s/%s), sleeping",
					inet_ntoa(ni->addrs[0]), ni->tags[0]);
			}
			printed++;
		}
		sleep(sleeptime);
		if (sleeptime < NI_MAXSLEEPTIME) {
			sleeptime *= 2; /* backoff */
		}
	}

	syslog(LOG_INFO, "NetInfo connected to %s/%s", inet_ntoa(ni->addrs[0]), ni->tags[0]);

	if (printed) {
		syslog(LOG_ERR, "NetInfo connected to %s/%s", inet_ntoa(ni->addrs[0]), ni->tags[0]);
	}
	return (1);
}


/*
 * Confirm that our tcp socket is still valid
 */
static int
confirm_tcp(
	    ni_private *ni,
	    int needwrite
	    )
{
	if (ni->tsock != -1) {
		if (getmyport(ni->tsock) == ni->tport) {
			return (1);
		}
		/*
		 * Somebody closed our socket. Do not close it, it could
		 * be owned by somebody else now.
		 */
		auth_destroy(ni->tc->cl_auth);
		clnt_destroy(ni->tc);
		ni->tc = NULL;
	}
	if (!needwrite && !rebind(ni) && ni->abort) {
		return (0);
	}
	return (connectit(ni));
}


static int
setmaster(
	  ni_private *ni
	  )
{
	ni_id root;
	ni_namelist nl;
	ni_name sep;
	ni_idlist idl;
	ni_name master;
	ni_index i;
	ni_index j;
	ni_id id;
	struct in_addr addr;
	int needwrite;

	if (ni->naddrs == 1) {
		/*
		 * One server - must be the master
		 */
		ni->whichwrite = 0;
		return (1);
	}
	needwrite = ni->needwrite;
	ni->needwrite = 0;
	if (ni_root(ni, &root) != NI_OK) {
		ni->needwrite = needwrite;
		return (0);
	}
	NI_INIT(&nl);
	if (ni_lookupprop(ni, &root, NAME_MASTER, &nl) != NI_OK) {
		ni->needwrite = needwrite;
		return (0);
	}
	if (nl.ninl_len == 0) {
		ni->needwrite = needwrite;
		return (0);
	}
	sep = index(nl.ninl_val[0], '/');
	if (sep == NULL) {
		ni->needwrite = needwrite;
		return (0);
	}
	*sep++ = 0;
	master = nl.ninl_val[0];
	NI_INIT(&idl);
	if (ni_lookup(ni, &root, NAME_NAME, NAME_MACHINES, &idl) != NI_OK) {
		ni->needwrite = needwrite;
		ni_namelist_free(&nl);
		return (0);
	}
	if (idl.niil_len < 1) {
		ni->needwrite = needwrite;
		return (0);
	}
	id.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);
	NI_INIT(&idl);
	if (ni_lookup(ni, &id, NAME_NAME, master, &idl) != NI_OK) {
		ni_namelist_free(&nl);
		ni->needwrite = needwrite;
		return (0);
	}
	ni_namelist_free(&nl);
	if (idl.niil_len < 1) {
		ni->needwrite = needwrite;
		return (0);
	}
	id.nii_object = idl.niil_val[0];
	ni_idlist_free(&idl);
	NI_INIT(&nl);
	if (ni_lookupprop(ni, &id, NAME_IP_ADDRESS, &nl) != NI_OK) {
		return (0);
	}
	for (i = 0; i < nl.ninl_len; i++) {
		addr.s_addr = inet_addr(nl.ninl_val[i]);
		for (j = 0; j < ni->naddrs; j++) {
			if (addr.s_addr == ni->addrs[j].s_addr) {
				ni->whichwrite = j;
				ni_namelist_free(&nl);
				ni->needwrite = needwrite;
				return (1);
			}
		}
	}
	ni->needwrite = needwrite;
	ni_namelist_free(&nl);
	return (0);
}


static void *
callit(
       ni_private *ni,
       void *(*stub)(),
       void *args,
       int needwrite
       )
{
	void *resp;
	struct rpc_err err;
	int i;
	int sleeptime = 0;
	int printed = 0;

	if (getpid() != ni->pid) {
		reinit(ni);
	}
	if (needwrite || ni->needwrite) {
		if (ni->whichwrite >= 0) {
			ni_switch(ni, ni->whichwrite);
		} else {
			if (!setmaster(ni)) {
				return (NULL);
			}
			ni_switch(ni, ni->whichwrite);
		}
		if (!needwrite) {
			ni_settimeout(ni, (ni->rtv_sec == 0 ?
					   NI_TIMEOUT_SHORT : ni->rtv_sec));
			needwrite = 1;
		} else {
			ni_settimeout(ni, (ni->wtv_sec == 0 ?
					   NI_TIMEOUT_LONG : ni->wtv_sec));
		}
	} else {
		ni_settimeout(ni, (ni->rtv_sec == 0 ?
				   NI_TIMEOUT_SHORT : ni->rtv_sec));
	}
	for (;;) {
		/*
		 * Try more than once, in case server closed connection.
		 */
		for (i = 0; i < NI_MAXCONNTRIES; i++) {
			if (!confirm_tcp(ni, needwrite)) {
				break;
			}
			if ((resp = (*stub)(args, ni->tc)) != NULL) {
				if (printed) {
					syslog(LOG_ERR, "NetInfo connected to %s/%s",
						inet_ntoa(ni->addrs[0]), ni->tags[0]);
				}
				return (resp);
			}
			clnt_geterr(ni->tc, &err);
			if (err.re_status != RPC_CANTRECV) {
				break;
			}
			if (i + 1 < NI_MAXCONNTRIES) {
				/*
				 * Server closed connection. Reinit and try
				 * again.
				 */
				reinit(ni);
			}
		}
		if (err.re_status == RPC_PROCUNAVAIL) {
			return (NULL);
		}
		if (needwrite || ni->abort) {
			/*
			 * We time out for writes or if it is explicitly
			 * requested.
			 */
			if (ni->abort) {
				reinit(ni);
			}
			syslog(LOG_ERR,
				"NetInfo connection failed for server %s/%s",
				inet_ntoa(ni->addrs[0]), ni->tags[0]);
			return (NULL);
		}
		if (!printed) {
			if (ni->tc != NULL) {
				if (!(sleeptime == 0 &&
				      err.re_status == RPC_TIMEDOUT)) {
					/*
					 * Do not print message on
					 * first timeout. It is likely
					 * we will find another server soon.
					 * Let's not needlessly alarm the
					 * poor user!
					 */
					syslog(LOG_ERR, "%s on connection to %s/%s",
						clnt_sperror(ni->tc,"NetInfo connection timeout"),
						inet_ntoa(ni->addrs[0]), ni->tags[0]);
					printed++;
				}
				else {
					/* first attempt failed */
					syslog(LOG_ERR, "%s on initial connection to %s/%s",
						clnt_sperror(ni->tc,"NetInfo connection timeout"),
						inet_ntoa(ni->addrs[0]), ni->tags[0]);
				}
			} else {
				syslog(LOG_ERR,
					"NetInfo connection failed for server %s/%s",
					inet_ntoa(ni->addrs[0]), ni->tags[0]);
				printed++;
			}
		}
		if (sleeptime > 0) {
			sleep(sleeptime);
			if (sleeptime < NI_MAXSLEEPTIME) {
				sleeptime *= 2; /* backoff */
			}
		} else {
			/*
			 * Do not sleep on the first timeout.
			 * It is likely we will find another server quickly.
			 */
			sleeptime = NI_SLEEPTIME;
		}
		reinit(ni);
		(void)rebind(ni);
	}
}


#define RCALLIT(a, b, c) callit((ni_private *)(a), (void *(*)())(b), \
				  (void *)c, 0)


#define WCALLIT(a, b, c) callit((ni_private *)(a), (void *(*)())(b), \
				(void *)c, 1)


static void
ni_clear(
	 ni_private *ni
	 )
{
     ni->needwrite = 0;
     ni->naddrs = 0;
     ni->addrs = NULL;
     ni->tags = NULL;
     ni->tc = NULL;
     ni->tsock = -1;
     ni->tport = -1;
     ni->whichwrite = -1;
     ni->passwd = NULL;
}


static void *
ni_alloc(
	 void
	 )
{
	ni_private *ni;

	ni = (ni_private *)malloc(sizeof(*ni));
	ni->naddrs = 0;
	ni->whichwrite = -1;
	ni->pid = getpid();
	ni->tsock = -1;
	ni->tport = -1;
	ni->tc = NULL;
	ni->tv_sec = NI_TIMEOUT_SHORT;
	ni->rtv_sec = 0;
	ni->wtv_sec = 0;
	ni->abort = 0;
	ni->passwd = NULL;
	ni->uid = getuid();
	ni->needwrite = 0;
	return ((void *)ni);
}


void *
_ni_dup(
	void *ni
       )
{
	ni_private *dupni;
	ni_index i;

	dupni = (ni_private *)ni_alloc();
	*dupni = *NIP(ni);
	ni_clear(dupni);
	dupni->naddrs = NIP(ni)->naddrs;
	dupni->whichwrite = NIP(ni)->whichwrite;
	if (dupni->naddrs > 0) {
		dupni->addrs = ((struct in_addr *)
				malloc(NIP(ni)->naddrs * sizeof(struct in_addr)));
		bcopy(NIP(ni)->addrs, dupni->addrs,
		      NIP(ni)->naddrs * sizeof(struct in_addr));
		dupni->tags = ((ni_name *)
			       malloc(NIP(ni)->naddrs * sizeof(ni_name)));
		for (i = 0; i < NIP(ni)->naddrs; i++) {
			dupni->tags[i] = ni_name_dup(NIP(ni)->tags[i]);
		}
	}
	if (NIP(ni)->passwd != NULL) {
		dupni->passwd = ni_name_dup(NIP(ni)->passwd);
	}
	return ((void *)dupni);
}


static int
match(
      ni_name domain,
      ni_name domtag,
      ni_name *tag
      )
{
	int len = strlen(domain);
	ni_name sep;
	
	sep = index(domtag, '/');
	if (sep == NULL) {
		return (0);
	}
	if (strncmp(domain, domtag, len) == 0 &&
	    domtag[len] == '/') {
		*tag = ni_name_dup(sep + 1);
		return (1);
	}
	return (0);
}


static int
addaddr(
	void *ni,
	ni_index ido,
	ni_name tag,
	ni_private *target_ni
	)
{
	ni_id id;
	ni_namelist nl;
	struct in_addr addr;
	int i;

	id.nii_object = ido;
	NI_INIT(&nl);
	if (ni_lookupprop(ni, &id, NAME_IP_ADDRESS, &nl) != NI_OK) {
		return (0);
	}
	if (nl.ninl_len == 0) {
		return(0);
	}

	if (target_ni->naddrs == 0) {
		target_ni->addrs =
			(struct in_addr *)malloc(nl.ninl_len * sizeof(struct in_addr));
		target_ni->tags =
			(ni_name *)malloc(nl.ninl_len * sizeof(ni_name));
	} else {
		target_ni->addrs =
			(struct in_addr *)realloc(target_ni->addrs,
						((target_ni->naddrs + nl.ninl_len) * sizeof(struct in_addr)));
		target_ni->tags =
			(ni_name *)realloc(target_ni->tags,
						((target_ni->naddrs + nl.ninl_len) * sizeof(ni_name)));
	}

	for (i=0; i<nl.ninl_len; i++) {
		addr.s_addr = inet_addr(nl.ninl_val[i]);
		target_ni->addrs[target_ni->naddrs] = addr;
		target_ni->tags[target_ni->naddrs] = ni_name_dup(tag);
		target_ni->naddrs++;
	}

	ni_namelist_free(&nl);
	return (1);
}


static int
get_daddr(
	  ni_private *ni,
	  ni_name dom,
	  ni_private *target_ni
	)
{
	ni_id id;
	ni_idlist ids;
	ni_namelist nl;
	ni_entrylist entries;
	ni_index i;
	ni_index j;
	ni_name tag;

	if (ni_root(ni, &id) != NI_OK) {
		return(0);
	}

	NI_INIT(&ids);
	if (ni_lookup(ni, &id, NAME_NAME, NAME_MACHINES, &ids) != NI_OK) {
		return (0);
	}

	id.nii_object = ids.niil_val[0];
	ni_idlist_free(&ids);

	NI_INIT(&entries);
	if (ni_list(ni, &id, NAME_SERVES, &entries) != NI_OK) {
		return (0);
	}

	for (i = 0; i < entries.niel_len; i++) {
		if (entries.niel_val[i].names != NULL) {
			nl = *entries.niel_val[i].names;
			for (j = 0; j < nl.ninl_len; j++) {
				if (match(dom, nl.ninl_val[j], &tag)) {
					if (addaddr(ni,
						    entries.niel_val[i].id,
						    tag,
						    target_ni)) {
						ni_name_free(&tag);
						break;
					}
					ni_name_free(&tag);
				}
			}
		}

	}
	ni_entrylist_free(&entries);
	return (target_ni->naddrs > 0);
}


#ifdef notdef
static int
get_haddr(
	  ni_private *ni,
	  ni_name hname,
	  ni_name tag,
	  ni_private *target_ni
	)
{
	ni_id id;
	ni_idlist ids;

	if (ni_root(ni, &id) != NI_OK) {
		return(0);
	}
	NI_INIT(&ids);
	if (ni_lookup(ni, &id, NAME_NAME, NAME_MACHINES, &ids) != NI_OK) {
		return (0);
	}
	id.nii_object = ids.niil_val[0];
	ni_idlist_free(&ids);

	NI_INIT(&ids);
	if (ni_lookup(ni, &id, NAME_NAME, hname, &ids) != NI_OK) {
		return (0);
	}
	id.nii_object = ids.niil_val[0];
	ni_idlist_free(&ids);
	if (!addaddr(ni, id.nii_object, tag, target_ni)) {
		return (0);
	}
	return (1);
}
#endif


static ni_status
getparent(ni_private *oldni, ni_private **newni)
{
	ni_rparent_res *resp;
	ni_private *ni;
	ni_private *dupni;
	int found;
	ni_index i;
	struct in_addr raddr;
	int printed = 0;
	int inlist = 0;

	found = 0;
	while (!found) {
		/*
		 * First, find our parent, any parent
		 */
		for (;;) {
			resp = RCALLIT(oldni, _ni_rparent_2, NULL);
			if (resp == NULL) {
				return (NI_FAILED);
			}
			if (resp->status != NI_NORESPONSE) {
				break;
			}
			if (!printed) {
				syslog(LOG_ERR, "NetInfo timeout finding server for parent of %s/%s, sleeping",
					inet_ntoa(oldni->addrs[0]), oldni->tags[0]);
				printed++;
			}
			sleep(NI_SLEEPTIME);
		}
		if (printed) {
			raddr.s_addr = htonl(resp->ni_rparent_res_u.binding.addr);
			
			syslog(LOG_ERR, "NetInfo %s/%s found parent %s/%s",
					inet_ntoa(oldni->addrs[0]), oldni->tags[0],
					inet_ntoa(raddr), resp->ni_rparent_res_u.binding.tag);
		}
		if (resp->status != NI_OK) {
			return (resp->status);
		}
		ni = ni_alloc();
		*ni = *oldni;
		ni_clear(ni);
		ni->naddrs = 1;
		ni->addrs = (struct in_addr *)malloc(sizeof(struct in_addr));
		ni->addrs[0].s_addr=htonl(resp->ni_rparent_res_u.binding.addr);
		ni->tags = (ni_name *)malloc(sizeof(ni_name));
		ni->tags[0] = ni_name_dup(resp->ni_rparent_res_u.binding.tag);
		
		xdr_free(xdr_ni_rparent_res, resp);
		
		dupni = ni;
		ni = ni_alloc();
		*ni = *dupni;
		ni_clear(ni);
		if (get_daddr(dupni, ".", ni)) {

			/*
			 * Now make sure returned parent is head of
			 * list
			 */
			for (i = 0; i < ni->naddrs; i++) {
				if (ni->addrs[i].s_addr ==
				    dupni->addrs[0].s_addr) {
					ni_switch(ni, i);
					inlist++;
					break;
				}
			}

			/*
			 * Reuse dupni client info
			 */
			ni->tsock = dupni->tsock;
			ni->tport = dupni->tport;
			ni->tc = dupni->tc;
			dupni->tsock = -1;
			dupni->tport = -1;
			dupni->tc = NULL;
			found++;

			/*
			 * If returned parent wasn't in list, it's a rogue.
			 * Log an error and drop the connection.
			 */
			if (inlist == 0) {
				syslog(LOG_ERR, "Rogue NetInfo server detected: %s/%s",
					inet_ntoa(dupni->addrs[0]), dupni->tags[0]);
				reinit(ni);
			}

		}
		ni_free(dupni);
	}
	if (found) {
		*newni = ni;
		return (NI_OK);
	} else {
		ni_free(ni);
		return (NI_FAILED);
	}
}


void *
ni_connect(
	   struct sockaddr_in *sin,
	   const char *tag
	   )
{
	void *ni;

	ni = ni_alloc();
	NIP(ni)->naddrs = 1;
	NIP(ni)->addrs = (struct in_addr *
		     )malloc(sizeof(struct in_addr));
	NIP(ni)->addrs[0] = sin->sin_addr;
	NIP(ni)->tags = (ni_name *)malloc(sizeof(ni_name));
	NIP(ni)->tags[0] = ni_name_dup(tag);
	return (ni);
}


ni_status
ni_addrtag(
	   void *ni,
	   struct sockaddr_in *addr,
	   ni_name *tag
	   )
{

	if (!confirm_tcp(ni, 0)) {
		return (NI_FAILED);
	}
	*tag = ni_name_dup(NIP(ni)->tags[0]);
	addr->sin_addr = NIP(ni)->addrs[0];
	addr->sin_port = htons(NIP(ni)->tport);
	addr->sin_family = AF_INET;
	bzero(addr->sin_zero, sizeof(addr->sin_zero));
	return (NI_OK);
}


void *
ni_new(
       void *oldni,
       const char *domain
       )
{
	ni_private *ni;
	ni_status status;
	ni_name sep, addr, tag;
	struct sockaddr_in sin;
	struct hostent *he;

	if (oldni == NULL) {
		ni = ni_alloc();
		if (!connectlocal(ni)) {
			free(ni);
			return (NULL);
		}
		if (strcmp(domain, "..") == 0) {
			oldni = ni;
			status = getparent((ni_private *)oldni, &ni);
			ni_free(oldni);
			if (status != NI_OK) {
				return (NULL);
			}
		} else if ((sep = index(domain, '@')) != NULL) {
			free(ni);
			tag  = strncpy((char *)malloc(sep-domain+1), domain, sep-domain);
			tag[sep-domain] = '\0';
			addr = strcpy ((char *)malloc(strlen(sep+1)), sep+1);
			sin.sin_addr.s_addr = inet_addr(addr);
			if (sin.sin_addr.s_addr == INADDR_NONE) {
				he = gethostbyname(addr);
				if (he == NULL) {
					free(addr);
					free(tag);
					return (NULL);
				}
				bcopy(he->h_addr_list[0], &sin.sin_addr.s_addr, he->h_length);
			}
			ni = ni_connect(&sin, tag);
			free(addr);
			free(tag);
		} else if (strcmp(domain, ".") != 0) {
			/*
			 * nothing else makes sense
			 */
			free(ni);
			return (NULL);
		}
	} else {
		if (strcmp(domain, "..") == 0) {
			status = getparent((ni_private *)oldni, &ni);
			if (status != NI_OK) {
				return (NULL);
			}
		} else if ((sep = index(domain, '@')) != NULL) {
			tag  = strncpy((char *)malloc(sep-domain+1), domain, sep-domain);
			tag[sep-domain] = '\0';
			addr = strcpy ((char *)malloc(strlen(sep+1)), sep+1);
			sin.sin_addr.s_addr = inet_addr(addr);
			if (sin.sin_addr.s_addr == INADDR_NONE) {
				he = gethostbyname(addr);
				if (he == NULL) {
					free(addr);
					free(tag);
					return (NULL);
				}
				bcopy(he->h_addr_list[0], &sin.sin_addr.s_addr, he->h_length);
			}
			ni = ni_connect(&sin, tag);
			free(addr);
			free(tag);
		} else {
			ni = ni_alloc();
			*ni = *NIP(oldni);
			ni_clear(ni);
			if (!get_daddr(oldni, (ni_name)domain, ni))  {
				ni_free(ni);
				ni = NULL;
			}
		}
	}
	return ((void *)ni);
}


void
ni_free(
	void *ni
	)
{
	ni_index i;

	if (NIP(ni)->tc != NULL) {
		clnt_kill(NIP(ni)->tc, NIP(ni)->tsock, NIP(ni)->tport);
	}
	if (NIP(ni)->naddrs > 0) {
		free(NIP(ni)->addrs);
		for (i = 0; i < NIP(ni)->naddrs; i++) {
			ni_name_free(&NIP(ni)->tags[i]);
		}
		free(NIP(ni)->tags);
	}
	if (NIP(ni)->passwd != NULL) {
		ni_name_free(&NIP(ni)->passwd);
	}
	free(ni);
}


/*
 * The rest of these are just wrappers that end up doing
 * RPC calls to the local NetInfo server.
 */
ni_status
ni_statistics(
	      void *ni,
	      ni_proplist *pl
	      )
{
	ni_proplist *resp;

	if ((resp = (ni_proplist *)RCALLIT(ni, _ni_statistics_2, NULL)) 
	    == NULL) {
		return (NI_FAILED);
	}
	*pl = *resp;
	return (NI_OK);
}


ni_status
ni_root(
	void *ni,
	ni_id *id
	)
{
	ni_id_res *resp;

	if ((resp = RCALLIT(ni, _ni_root_2, id)) == NULL) {
		clnt_debug(ni, "_ni_root");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_self(
	void *ni,
	ni_id *id
	)
{
	ni_id_res *resp;

	if ((resp = RCALLIT(ni, _ni_self_2, id)) == NULL) {
		clnt_debug(ni, "_ni_self");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_parent(
	  void *ni,
	  ni_id *id,
	  ni_index *parent_id_p
	  )
{
	ni_parent_res *resp;

	if ((resp = RCALLIT(ni, _ni_parent_2, id)) == NULL) {
		clnt_debug(ni, "_ni_parent");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*parent_id_p = resp->ni_parent_res_u.stuff.object_id;
		*id = resp->ni_parent_res_u.stuff.self_id;
	}
	return (resp->status);
}


ni_status
ni_children(
	    void *ni,
	    ni_id *id,
	    ni_idlist *children
	    )
{
	ni_children_res *resp;

	if ((resp = RCALLIT(ni, _ni_children_2, id)) == NULL) {
		clnt_debug(ni, "_ni_children");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*children = resp->ni_children_res_u.stuff.children;
		*id = resp->ni_children_res_u.stuff.self_id;
	}
	return (resp->status);
}


ni_status
ni_create(
	  void *ni,
	  ni_id *parent_id,
	  ni_proplist pl,
	  ni_id *child_id_p,
	  ni_index where
	  )
{
	ni_create_args args;
	ni_create_res *resp;

	args.id = *parent_id;
	args.props = pl;
	args.where = where;
	args.target_id = NULL;
	if ((resp = WCALLIT(ni, _ni_create_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_create");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*child_id_p = resp->ni_create_res_u.stuff.id;
		*parent_id = resp->ni_create_res_u.stuff.self_id;
	}
	return (resp->status);
}


ni_status
ni_destroy(
	   void *ni,
	   ni_id *parent_id,
	   ni_id self_id
	   )
{
	ni_id_res *resp;
	ni_destroy_args args;

	args.parent_id = *parent_id;
	args.self_id = self_id;
	if ((resp = WCALLIT(ni, _ni_destroy_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_destroy");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*parent_id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_write(
	 void *ni,
	 ni_id *self_id,
	 ni_proplist pl
	  )
{
	ni_proplist_stuff args;
	ni_id_res *resp;

	args.id = *self_id;
	args.props = pl;
	if ((resp = WCALLIT(ni, _ni_write_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_write");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*self_id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_read(
	void *ni,
	ni_id *self_id,
	ni_proplist *pl
	)
{
	ni_proplist_res *resp;

	if ((resp = RCALLIT(ni, _ni_read_2, self_id)) == NULL) {
		clnt_debug(ni, "_ni_read");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*self_id = resp->ni_proplist_res_u.stuff.id;
		*pl = resp->ni_proplist_res_u.stuff.props;
	}
	return (resp->status);
}


ni_status
ni_lookup(
	  void *ni,
	  ni_id *id,
	  ni_name_const pname,
	  ni_name_const pval,
	  ni_idlist *hits
	  )
{
	ni_lookup_res *resp;
	ni_lookup_args args;

	args.id = *id;
	args.key = (ni_name)pname;
	args.value = (ni_name)pval;
	if ((resp = RCALLIT(ni, _ni_lookup_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_lookup");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*hits = resp->ni_lookup_res_u.stuff.idlist;
		*id = resp->ni_lookup_res_u.stuff.self_id;
	}
	return (resp->status);
}


ni_status
ni_lookupread(
	      void *ni,
	      ni_id *id,
	      ni_name_const pname,
	      ni_name_const pval,
	      ni_proplist *props
	  )
{
	ni_proplist_res *resp;
	ni_lookup_args args;

	args.id = *id;
	args.key = (ni_name)pname;
	args.value = (ni_name)pval;
	if ((resp = RCALLIT(ni, _ni_lookupread_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_lookupread");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*props = resp->ni_proplist_res_u.stuff.props;
		*id = resp->ni_proplist_res_u.stuff.id;
	}
	return (resp->status);
}


ni_status
ni_list(
	void *ni,
	ni_id *id,
	ni_name_const pname,
	ni_entrylist *entries
	)
{
	ni_list_res *resp;
	ni_name_args args;

	args.id = *id;
	args.name = (ni_name)pname;
	if ((resp = RCALLIT(ni, _ni_list_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_list");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*entries = resp->ni_list_res_u.stuff.entries;
		*id = resp->ni_list_res_u.stuff.self_id;
	}
	return (resp->status);
}


ni_status
ni_listall(
	   void *ni,
	   ni_id *id,
	   ni_proplist_list *entries
	   )
{
	ni_listall_res *resp;

	if ((resp = RCALLIT(ni, _ni_listall_2, id)) == NULL) {
		clnt_debug(ni, "_ni_listall");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*entries = resp->ni_listall_res_u.stuff.entries;
		*id = resp->ni_listall_res_u.stuff.self_id;
	}
	return (resp->status);
}


ni_status
ni_readprop(
	    void *ni,
	    ni_id *id,
	    ni_index which,
	    ni_namelist *propval_p
	   )
{
	ni_namelist_res *resp;
	ni_prop_args args;

	args.id = *id;
	args.prop_index = which;
	if ((resp = RCALLIT(ni, _ni_readprop_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_readprop");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*propval_p = resp->ni_namelist_res_u.stuff.values;
		*id = resp->ni_namelist_res_u.stuff.self_id;
	}
	return (resp->status);
}


ni_status
ni_writeprop(
	     void *ni,
	     ni_id *id,
	     ni_index which,
	     ni_namelist propval
	     )
{
	ni_id_res *resp;
	ni_writeprop_args args;

	args.id = *id;
	args.prop_index = which;
	args.values = propval;
	if ((resp = WCALLIT(ni, _ni_writeprop_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_writeprop");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_listprops(
	     void *ni,
	     ni_id *id,
	     ni_namelist *propnames
	     )
{
	ni_namelist_res *resp;

	if ((resp = RCALLIT(ni, _ni_listprops_2, id)) == NULL) {
		clnt_debug(ni, "_ni_listprops");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*propnames = resp->ni_namelist_res_u.stuff.values;
		*id = resp->ni_namelist_res_u.stuff.self_id;
	}
	return (resp->status);
}

	
ni_status
ni_createprop(
	      void *ni,
	      ni_id *id,
	      ni_property prop,
	      ni_index where
	      )
{
	ni_id_res *resp;
	ni_createprop_args args;

	args.id = *id;
	args.prop = prop;
	args.where = where;
	if ((resp = WCALLIT(ni, _ni_createprop_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_createprop");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_destroyprop(
	       void *ni,
	       ni_id *id,
	       ni_index which
	       )
{
	ni_id_res *resp;
	ni_prop_args args;

	args.id = *id;
	args.prop_index = which;
	if ((resp = WCALLIT(ni, _ni_destroyprop_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_destroyprop");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_renameprop(
	      void *ni,
	      ni_id *id,
	      ni_index prop_index,
	      ni_name_const name
	      )
{
	ni_id_res *resp;
	ni_propname_args args;

	args.id = *id;
	args.prop_index = prop_index;
	args.name = (ni_name)name;
	if ((resp = WCALLIT(ni, _ni_renameprop_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_renameprop");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_createname(
	      void *ni,
	      ni_id *id,
	      ni_index prop_index,
	      ni_name_const name,
	      ni_index where
	      )
{
	ni_id_res *resp;
	ni_createname_args args;

	args.id = *id;
	args.prop_index = prop_index;
	args.name = (ni_name)name;
	args.where = where;
	if ((resp = WCALLIT(ni, _ni_createname_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_createname");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_destroyname(
	       void *ni,
	       ni_id *id,
	       ni_index prop_index,
	       ni_index name_index
	       )
{
	ni_id_res *resp;
	ni_nameindex_args args;

	args.id = *id;
	args.prop_index = prop_index;
	args.name_index = name_index;
	if ((resp = WCALLIT(ni, _ni_destroyname_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_destroyname");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_writename(
	      void *ni,
	      ni_id *id,
	      ni_index prop_index,
	      ni_index name_index,
	      ni_name_const name
	     )
{
	ni_id_res *resp;
	ni_writename_args args;

	args.id = *id;
	args.prop_index = prop_index;
	args.name_index = name_index;
	args.name = (ni_name)name;
	if ((resp = WCALLIT(ni, _ni_writename_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_writename");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_id_res_u.id;
	}
	return (resp->status);
}


ni_status
ni_readname(
	      void *ni,
	      ni_id *id,
	      ni_index prop_index,
	      ni_index name_index,
	      ni_name *name
	      )
{
	ni_readname_res *resp;
	ni_nameindex_args args;

	args.id = *id;
	args.prop_index = prop_index;
	args.name_index = name_index;
	if ((resp = RCALLIT(ni, _ni_readname_2, &args)) == NULL) {
		clnt_debug(ni, "_ni_readname");
		return (NI_FAILED);
	}
	if (resp->status == NI_OK) {
		*id = resp->ni_readname_res_u.stuff.id;
		*name = resp->ni_readname_res_u.stuff.name;
	}
	return (resp->status);
}


ni_status
ni_resync(
	  void *ni
	  )
{
	ni_status *resp;

	if ((resp = (ni_status *)RCALLIT(ni, _ni_resync_2, NULL)) == NULL) {
		return (NI_FAILED);
	}
	return (*resp);
}


ni_status
ni_setuser(
	   void *ni,
	   ni_name_const user
	   )
{
	ni_id id;
	ni_idlist ids;
	ni_namelist nl;
	char *p;

	if (user == NULL) {
		NIP(ni)->uid = getuid();
		return (ni_setpassword(ni, NULL));
	}

	if (ni_root(ni, &id) != NI_OK) {
		return(NI_NOUSER);
	}
	NI_INIT(&ids);
	if (ni_lookup(ni, &id, NAME_NAME, NAME_USERS, &ids) != NI_OK) {
		return (NI_NOUSER);
	}
	id.nii_object = ids.niil_val[0];
	ni_idlist_free(&ids);

	NI_INIT(&ids);
	if (ni_lookup(ni, &id, NAME_NAME, user, &ids) != NI_OK) {
		return (NI_NOUSER);
	}
	id.nii_object = ids.niil_val[0];
	ni_idlist_free(&ids);
	NI_INIT(&nl);
	if (ni_lookupprop(ni, &id, NAME_UID, &nl) != NI_OK) {
		return (NI_NOUSER);
	}
	if (nl.ninl_len == 0) {
		return (NI_NOUSER);
	}
	for (p = nl.ninl_val[0]; *p; p++) {
		if (!isdigit(*p)) {
			ni_namelist_free(&nl);
			return (NI_NOUSER);
		}
	}
	NIP(ni)->uid = atoi(nl.ninl_val[0]);
	if (NIP(ni)->passwd == NULL) {
		NIP(ni)->passwd = ni_name_dup("");
	}
	createauth(NIP(ni));
	return (NI_OK);
}


ni_status
ni_setpassword(
	       void *ni,
	       ni_name_const passwd
	       )
{
	char *p;

	if (NIP(ni)->passwd != NULL) {
		ni_name_free(&NIP(ni)->passwd);
	}
	if (passwd == NULL) {
		NIP(ni)->passwd = NULL;
		if (NIP(ni)->tc != NULL) {
			auth_destroy(NIP(ni)->tc->cl_auth);
			NIP(ni)->tc->cl_auth = authnone_create();
		}
		return (NI_OK);
	}
	NIP(ni)->passwd = ni_name_dup(passwd);
	/*
	 * Our trivial encryption scheme
	 */
	for (p = NIP(ni)->passwd; *p; p++) {
		*p = ~(*p);
	}
	createauth(NIP(ni));
	return (NI_OK);
}


extern int bindresvport(int, struct sockaddr_in *);


/*
 *	NeXT note:
 * 	The procedure pmap_getport_to below is derived
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
/*
 * Client interface to pmap rpc service.
 *
 * Find the mapped port for program,version.
 * Calls the pmap service remotely to do the lookup.
 * Returns 0 if no map exists. 
 */ 
static u_short
pmap_getport_to(address, program, version, protocol, timeout_secs, ntries)
	struct sockaddr_in *address;
	u_long program;
	u_long version;
	u_int protocol;
	int timeout_secs;
	int ntries;
{
	u_short port = 0;
	int sock = -1;
	register CLIENT *client;
	struct pmap parms;
	struct timeval timeout;
	
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		return (0);
	}
	address->sin_port = htons(PMAPPORT);
	timeout.tv_usec = ((timeout_secs % ntries) * 1000000) / ntries;
	timeout.tv_sec = (timeout_secs / ntries);
	client = clntudp_bufcreate(address, PMAPPROG,
	    PMAPVERS, timeout, &sock, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client != (CLIENT *)NULL) {
		parms.pm_prog = program;
		parms.pm_vers = version;
		parms.pm_prot = protocol;
		parms.pm_port = 0;  /* not needed or used */
		timeout.tv_usec = 0;
		timeout.tv_sec = timeout_secs;
		if (CLNT_CALL(client, PMAPPROC_GETPORT, xdr_pmap, &parms,
			      xdr_u_short, &port, timeout) != RPC_SUCCESS){
			rpc_createerr.cf_stat = RPC_PMAPFAILURE;
			clnt_geterr(client, &rpc_createerr.cf_error);
			port = 0;
		} else if (port == 0) {
			rpc_createerr.cf_stat = RPC_PROGNOTREGISTERED;
		}
	}
	if (client != NULL) {
		clnt_destroy(client);
	}
	(void)close(sock);
	address->sin_port = 0;
	return (port);
}


/*
 * Open a socket, but do not use the default portmap timeout
 */
static int
socket_open(
	    struct sockaddr_in *raddr,
	    int prog, 
	    int vers,
	    int timeout,
	    int ntries,
	    int proto
	    )
{
	int sock;
	int reuse = 1;

	/*
	 * If no port number given ask the pmap for one
	 */
	if (raddr->sin_port == 0) {
		u_short port;
		if ((port = pmap_getport_to(raddr, prog, vers, 
					    IPPROTO_UDP, timeout,
					    ntries)) == 0) {
			return (-1);
		}
		raddr->sin_port = htons(port);
	}

	sock = socket(AF_INET, proto == IPPROTO_UDP ? SOCK_DGRAM : SOCK_STREAM,
		      proto);
	if (sock < 0) {
		return (-1);
	}
	(void)bindresvport(sock, (struct sockaddr_in *)0);
	setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(int));
	if (proto == IPPROTO_TCP) {
		if (connect(sock, (struct sockaddr *)raddr,
			    sizeof(*raddr)) < 0) {
			(void)close(sock);
			return (-1);
		}
	}
	return (sock);
}
