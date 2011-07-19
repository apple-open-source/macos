/*
 * Copyright (c) 1999-2010 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/socketvar.h>

#include <oncrpc/rpc.h>
#include <oncrpc/pmap_clnt.h>
#include <oncrpc/pmap_prot.h>

#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>

#include "common.h"

int nfstcpsock, nfstcp6sock;
int nfsudpsock, nfsudp6sock;

/*
 * The incredibly complex nfsd thread function
 */
static void *
nfsd_server_thread(__unused void *arg)
{
	set_thread_sigmask();
	if (nfssvc(NFSSVC_NFSD, NULL) < 0)
		log(LOG_ERR, "nfssvc: %s (%d)", strerror(errno), errno);
	DEBUG(1, "nfsd thread exiting.");
	return (NULL);
}

void *
nfsd_accept_thread(__unused void *arg)
{
	struct nfsd_args nfsdargs;
	fd_set ready, sockbits;
	struct sockaddr_storage peer;
	struct sockaddr *sa = (struct sockaddr *)&peer;
	socklen_t len;
	int maxlistensock = -1;
	int newsock, on = 1;
	struct timeval tv;
	char hostbuf[NI_MAXHOST];

	set_thread_sigmask();

	FD_ZERO(&sockbits);
	if (config.tcp) {
		if (nfstcpsock != -1)
			FD_SET(nfstcpsock, &sockbits);
		if (nfstcp6sock != -1)
			FD_SET(nfstcp6sock, &sockbits);
		maxlistensock = MAX(nfstcpsock, nfstcp6sock);
	}

	/*
	 * Loop until terminated.
	 * If accepting connections, pass new sockets into the kernel.
	 * Otherwise, just sleep waiting for a signal.
	 */
	while (!gotterm) {
		tv.tv_sec = 3600;
		tv.tv_usec = 0;
		ready = sockbits;
		if (select(maxlistensock+1, ((maxlistensock < 0) ? NULL : &ready), NULL, NULL, &tv) < 0) {
			if (errno == EINTR)
				continue;
			log(LOG_ERR, "select failed: %s (%d)", strerror(errno), errno);
			break;
		}
		if (!config.tcp || (maxlistensock < 0))
			continue;
		if ((nfstcpsock >= 0) && FD_ISSET(nfstcpsock, &ready)) {
			len = sizeof(peer);
			if ((newsock = accept(nfstcpsock, (struct sockaddr *)&peer, &len)) < 0) {
				log(LOG_WARNING, "accept failed: %s (%d)", strerror(errno), errno);
				continue;
			}
			if (config.verbose >= 3) {
				hostbuf[0] = '\0';
				getnameinfo(sa, sa->sa_len, hostbuf, sizeof(hostbuf), NULL, 0, 0);
				DEBUG(1, "NFS IPv4 socket accepted from %s", hostbuf);
			}
			memset(&((struct sockaddr_in*)&peer)->sin_zero[0], 0, sizeof(&((struct sockaddr_in*)&peer)->sin_zero[0]));
			if (setsockopt(newsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				log(LOG_NOTICE, "setsockopt SO_KEEPALIVE: %s (%d)", strerror(errno), errno);
			nfsdargs.sock = newsock;
			nfsdargs.name = (caddr_t)&peer;
			nfsdargs.namelen = len;
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			close(newsock);
		}
		if ((nfstcp6sock >= 0) && FD_ISSET(nfstcp6sock, &ready)) {
			len = sizeof(peer);
			if ((newsock = accept(nfstcp6sock, (struct sockaddr *)&peer, &len)) < 0) {
				log(LOG_WARNING, "accept failed: %s (%d)", strerror(errno), errno);
				continue;
			}
			if (config.verbose >= 3) {
				hostbuf[0] = '\0';
				getnameinfo(sa, sa->sa_len, hostbuf, sizeof(hostbuf), NULL, 0, 0);
				DEBUG(1, "NFS IPv6 socket accepted from %s", hostbuf);
			}
			if (setsockopt(newsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				log(LOG_NOTICE, "setsockopt SO_KEEPALIVE: %s (%d)", strerror(errno), errno);
			nfsdargs.sock = newsock;
			nfsdargs.name = (caddr_t)&peer;
			nfsdargs.namelen = len;
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			close(newsock);
		}
	}

	return (NULL);
}

/*
 * start a given number of NFS server threads
 */
void
nfsd_start_server_threads(int count)
{
	int threadcnt, i, rv;
	pthread_t thd;

	/* set up the server threads */
	threadcnt = 0;
	for (i=0; i < count; i++) {
		rv = pthread_create(&thd, &pattr, nfsd_server_thread, NULL);
		if (rv) {
			log(LOG_ERR, "pthread_create: %s (%d)", strerror(rv), rv);
			continue;
		}
		threadcnt++;
	}
	DEBUG(1, "Started %d of %d new nfsd threads", threadcnt, count);
	/* if no threads started exit */
	if (!threadcnt)
		log(LOG_ERR, "unable to start any nfsd threads");
	if (threadcnt != count)
		log(LOG_WARNING, "only able to create %d of %d nfsd threads", threadcnt, count);
}

/*
 * NFS server daemon mostly just a user context for nfssvc()
 *
 * 1 - set up server socket(s)
 * 2 - create the nfsd server threads
 * 3 - create the nfsd accept thread
 *
 * For connectionless protocols, just pass the socket into the kernel via.  nfssvc().
 * For connection based sockets, the accept thread loops doing the accepts.
 * When we get a new socket from accept, pass it into the kernel via. nfssvc().
 */
void
nfsd(void)
{
	struct nfsd_args nfsdargs;
	struct sockaddr_storage saddr;
	struct sockaddr_in *sin = (struct sockaddr_in*)&saddr;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&saddr;
	int rv, on = 1;
	pthread_t thd;

	nfsudpsock = nfsudp6sock = -1;
	nfstcpsock = nfstcp6sock = -1;

	/* If we are serving UDP, set up the NFS/UDP sockets. */
	if (config.udp) {

		/* IPv4 */
		if ((nfsudpsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			log(LOG_WARNING, "can't create NFS/UDP IPv4 socket");
		if (nfsudpsock >= 0) {
			nfsudpport = config.port;
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = INADDR_ANY;
			sin->sin_port = htons(config.port);
			sin->sin_len = sizeof(*sin);
			if (bind(nfsudpsock, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
				/* socket may still be lingering from previous incarnation */
				/* wait a few seconds and try again */
				sleep(6);
				if (bind(nfsudpsock, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
					log(LOG_WARNING, "can't bind NFS/UDP IPv4 addr");
					close(nfsudpsock);
					nfsudpsock = -1;
					nfsudpport = 0;
				}
			}
		}
		if (nfsudpsock >= 0) {
			nfsdargs.sock = nfsudpsock;
			nfsdargs.name = NULL;
			nfsdargs.namelen = 0;
			if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
				log(LOG_WARNING, "can't add NFS/UDP IPv4 socket");
				close(nfsudpsock);
				nfsudpsock = -1;
				nfsudpport = 0;
			} else {
				close(nfsudpsock);
			}
		}

		/* IPv6 */
		if ((nfsudp6sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			log(LOG_WARNING, "can't create NFS/UDP IPv6 socket");
		if (nfsudp6sock >= 0) {
			nfsudp6port = config.port;
			if (setsockopt(nfsudp6sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof(on)) < 0)
				log(LOG_WARNING, "setsockopt NFS/UDP IPV6_V6ONLY: %s (%d)", strerror(errno), errno);
			sin6->sin6_family = AF_INET6;
			sin6->sin6_addr = in6addr_any;
			sin6->sin6_port = htons(config.port);
			sin6->sin6_len = sizeof(*sin6);
			if (bind(nfsudp6sock, (struct sockaddr *)sin6, sizeof(*sin6)) < 0) {
				/* socket may still be lingering from previous incarnation */
				/* wait a few seconds and try again */
				sleep(6);
				if (bind(nfsudp6sock, (struct sockaddr *)sin6, sizeof(*sin6)) < 0) {
					log(LOG_WARNING, "can't bind NFS/UDP IPv6 addr");
					close(nfsudp6sock);
					nfsudp6sock = -1;
					nfsudp6port = 0;
				}
			}
		}
		if (nfsudp6sock >= 0) {
			nfsdargs.sock = nfsudp6sock;
			nfsdargs.name = NULL;
			nfsdargs.namelen = 0;
			if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
				log(LOG_WARNING, "can't add NFS/UDP IPv6 socket");
				close(nfsudp6sock);
				nfsudp6sock = -1;
				nfsudp6port = 0;
			} else {
				close(nfsudp6sock);
			}
		}

	}

	/* If we are serving TCP, set up the NFS/TCP socket. */
	if (config.tcp) {

		/* IPv4 */
		if ((nfstcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			log(LOG_WARNING, "can't create NFS/TCP IPv4 socket");
		if (nfstcpsock >= 0) {
			nfstcpport = config.port;
			if (setsockopt(nfstcpsock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
				log(LOG_WARNING, "setsockopt NFS/TCP IPv4 SO_REUSEADDR: %s (%d)", strerror(errno), errno);
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = INADDR_ANY;
			sin->sin_port = htons(config.port);
			sin->sin_len = sizeof(*sin);
			if (bind(nfstcpsock, (struct sockaddr *)sin, sizeof(*sin)) < 0) {
				log(LOG_WARNING, "can't bind NFS/TCP IPv4 addr");
				close(nfstcpsock);
				nfstcpsock = -1;
				nfstcpport = 0;
			}
		}
		if ((nfstcpsock >= 0) && (listen(nfstcpsock, 128) < 0)) {
			log(LOG_WARNING, "NFS IPv4 listen failed");
			close(nfstcpsock);
			nfstcpsock = -1;
			nfstcpport = 0;
		}

		/* IPv6 */
		if ((nfstcp6sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
			log(LOG_WARNING, "can't create NFS/TCP IPv6 socket");
		if (nfstcp6sock >= 0) {
			nfstcp6port = config.port;
			if (setsockopt(nfstcp6sock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
				log(LOG_WARNING, "setsockopt NFS/TCP IPv6 SO_REUSEADDR: %s (%d)", strerror(errno), errno);
			if (setsockopt(nfstcp6sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&on, sizeof(on)) < 0)
				log(LOG_WARNING, "setsockopt NFS/TCP IPV6_V6ONLY: %s (%d)", strerror(errno), errno);
			sin6->sin6_family = AF_INET6;
			sin6->sin6_addr = in6addr_any;
			sin6->sin6_port = htons(config.port);
			sin6->sin6_len = sizeof(*sin6);
			if (bind(nfstcp6sock, (struct sockaddr *)sin6, sizeof(*sin6)) < 0) {
				log(LOG_WARNING, "can't bind NFS/TCP IPv6 addr");
				close(nfstcp6sock);
				nfstcp6sock = -1;
				nfstcp6port = 0;
			}
		}
		if ((nfstcp6sock >= 0) && (listen(nfstcp6sock, 128) < 0)) {
			log(LOG_WARNING, "NFS IPv6 listen failed");
			close(nfstcp6sock);
			nfstcp6sock = -1;
			nfstcp6port = 0;
		}

	}

	if ((nfsudp6sock < 0) && (nfstcp6sock < 0))
		log(LOG_WARNING, "Can't create NFS IPv6 sockets");
	if ((nfsudpsock < 0) && (nfstcpsock < 0))
		log(LOG_WARNING, "Can't create NFS IPv4 sockets");
	if ((nfsudp6sock < 0) && (nfstcp6sock < 0) &&
	    (nfsudpsock < 0) && (nfstcpsock < 0)) {
		log(LOG_ERR, "Can't create any NFS sockets!");
		exit(1);
	}

	/* start up all the server threads */
	sysctl_set("vfs.generic.nfs.server.nfsd_thread_max", config.nfsd_threads);
	nfsd_start_server_threads(config.nfsd_threads);

	/* set up the accept thread */
	rv = pthread_create(&thd, &pattr, nfsd_accept_thread, NULL);
	if (rv) {
		log(LOG_ERR, "pthread_create: %s (%d)", strerror(rv), rv);
		exit(1);
	}
}

