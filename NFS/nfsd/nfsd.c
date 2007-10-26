/*
 * Copyright (c) 1999-2007 Apple Inc.  All rights reserved.
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

#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <rpc/pmap_prot.h>

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

int nfstcpsock;

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
	struct sockaddr_in inetpeer;
	socklen_t len;
	int msgsock, on = 1;
	struct timeval tv;

	set_thread_sigmask();

	FD_ZERO(&sockbits);
	if (config.tcp)
		FD_SET(nfstcpsock, &sockbits);

	/*
	 * Loop until terminated.
	 * If accepting connections, pass new sockets into the kernel.
	 * Otherwise, just sleep waiting for a signal.
	 */
	while (!gotterm) {
		tv.tv_sec = 3600;
		tv.tv_usec = 0;
		ready = sockbits;
		if (select(nfstcpsock+1, ((nfstcpsock < 0) ? NULL : &ready), NULL, NULL, &tv) < 0) {
			if (errno == EINTR)
				continue;
			log(LOG_ERR, "select failed: %s (%d)", strerror(errno), errno);
			break;
		}
		if (config.tcp && FD_ISSET(nfstcpsock, &ready)) {
			len = sizeof(inetpeer);
			if ((msgsock = accept(nfstcpsock, (struct sockaddr *)&inetpeer, &len)) < 0) {
				log(LOG_WARNING, "accept failed: %s (%d)", strerror(errno), errno);
				continue;
			}
			DEBUG(1, "NFS socket accepted");
			memset(inetpeer.sin_zero, 0, sizeof(inetpeer.sin_zero));
			if (setsockopt(msgsock, SOL_SOCKET,
			    SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0)
				log(LOG_NOTICE, "setsockopt SO_KEEPALIVE: %s (%d)", strerror(errno), errno);
			nfsdargs.sock = msgsock;
			nfsdargs.name = (caddr_t)&inetpeer;
			nfsdargs.namelen = sizeof(inetpeer);
			nfssvc(NFSSVC_ADDSOCK, &nfsdargs);
			close(msgsock);
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
	struct sockaddr_in inetaddr;
	int rv;
	int on, sock;
	pthread_t thd;

	nfstcpsock = -1;

	/* If we are serving UDP, set up the NFS/UDP socket. */
	if (config.udp) {
		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			log(LOG_ERR, "can't create NFS/UDP socket");
			exit(1);
		}
		inetaddr.sin_family = AF_INET;
		inetaddr.sin_addr.s_addr = INADDR_ANY;
		inetaddr.sin_port = htons(config.port);
		inetaddr.sin_len = sizeof(inetaddr);
		if (bind(sock, (struct sockaddr *)&inetaddr, sizeof(inetaddr)) < 0) {
			/* socket may still be lingering from previous incarnation */
			/* wait a few seconds and try again */
			sleep(6);
			if (bind(sock, (struct sockaddr *)&inetaddr, sizeof(inetaddr)) < 0) {
				log(LOG_ERR, "can't bind NFS/UDP addr");
				exit(1);
			}
		}
		nfsdargs.sock = sock;
		nfsdargs.name = NULL;
		nfsdargs.namelen = 0;
		if (nfssvc(NFSSVC_ADDSOCK, &nfsdargs) < 0) {
			log(LOG_ERR, "can't add NFS/UDP socket");
			exit(1);
		}
		close(sock);
	}

	/* If we are serving TCP, set up the NFS/TCP socket. */
	on = 1;
	if (config.tcp) {
		if ((nfstcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			log(LOG_ERR, "can't create NFS/TCP socket");
			exit(1);
		}
		if (setsockopt(nfstcpsock, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on)) < 0)
			log(LOG_WARNING, "setsockopt SO_REUSEADDR: %s (%d)", strerror(errno), errno);
		inetaddr.sin_family = AF_INET;
		inetaddr.sin_addr.s_addr = INADDR_ANY;
		inetaddr.sin_port = htons(config.port);
		inetaddr.sin_len = sizeof(inetaddr);
		if (bind(nfstcpsock, (struct sockaddr *)&inetaddr, sizeof (inetaddr)) < 0) {
			log(LOG_ERR, "can't bind NFS/TCP addr");
			exit(1);
		}
		if (listen(nfstcpsock, 128) < 0) {
			log(LOG_ERR, "NFS listen failed");
			exit(1);
		}
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

