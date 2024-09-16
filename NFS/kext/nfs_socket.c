/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1989, 1991, 1993, 1995
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
 *
 *	@(#)nfs_socket.c	8.5 (Berkeley) 3/30/95
 * FreeBSD-Id: nfs_socket.c,v 1.30 1997/10/28 15:59:07 bde Exp $
 */

#include "nfs_client.h"
#include "nfs_kdebug.h"

/*
 * Socket operations for use by nfs
 */

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kpi_mbuf.h>
#include <sys/un.h>
#include <sys/syslog.h>
#include <sys/reboot.h>
#include <sys/tprintf.h>

#include <libkern/OSAtomic.h>
#include <IOKit/IOPlatformExpert.h>
#include <kern/kalloc.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <nfs/krpc.h>
#include <nfs/xdr_subs.h>
#include <nfs/nfsm_subs.h>
#include <nfs/nfs_gss.h>
#include <nfs/nfsmount.h>
#include <nfs/nfsnode.h>

#define NFS_SOCK_DBG(...) NFSCLNT_DBG(NFSCLNT_FAC_SOCK, 7, ## __VA_ARGS__)
#define NFS_SOCK_DUMP_MBUF(msg, mb) if (NFSCLNT_IS_DBG(NFSCLNT_FAC_SOCK, 15)) nfs_dump_mbuf(__func__, __LINE__, (msg), (mb))
#define SP(SP, FMT, ...) NFS_SOCK_DBG(FMT " [sock 0x%lx]\n", ##__VA_ARGS__, nfs_kernel_hideaddr(SP))

#ifndef SUN_LEN
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strnlen((su)->sun_path, sizeof((su)->sun_path)))
#endif /* SUN_LEN */

const nfserr_info_t nfserrs_common[NFSERR_INFO_COMMON_SIZE] = {
	NFSERR_INFO_COMMON
};

const nfserr_info_t nfserrs_v4[NFSERR_INFO_V4_SIZE] = {
	NFSERR_INFO_V4
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#endif

/* XXX */
kern_return_t   thread_terminate(thread_t);

static int
nfs_sendmbuf(__unused struct sockaddr *saddr, socket_t sock, const struct msghdr *msg, mbuf_t data,
    int flags, size_t *sentlen)
{
	int error;
	int wait = saddr && (saddr->sa_family == AF_LOCAL);

	NFS_KDBG_ENTRY(NFSDBG_OP_SEND_SENDMBUF, saddr, sock, flags, wait);

#ifdef HAS_SOCK_SENDMBUF_CAN_WAIT
	if (wait) {
		error = sock_sendmbuf_can_wait(sock, msg, data, flags, sentlen);
	} else
#endif /* HAS_SOCK_SENDMBUF_CAN_WAIT */
	{
		error = sock_sendmbuf(sock, msg, data, flags, sentlen);
	}

	NFS_KDBG_EXIT(NFSDBG_OP_SEND_SENDMBUF, saddr, sock, sentlen, error);
	return error;
}

static int
is_error_in_range(const nfserr_info_t *arr, int arr_size, int error)
{
	if (arr_size == 0) {
		return 0;
	}
	return error >= arr[0].nei_error && error <= arr[arr_size - 1].nei_error;
}

void
nfsstat_update_nfserror(int error, int vers)
{
	if (is_error_in_range(nfserrs_common, ARRAY_SIZE(nfserrs_common), error)) {
		for (uint32_t i = 0; i < ARRAY_SIZE(nfserrs_common); i++) {
			if (error == nfserrs_common[i].nei_error) {
				nfsclntstats.nfs_errs.errs_common[nfserrs_common[i].nei_index]++;
				return;
			}
		}
	}

	if (vers >= NFS_VER4 && is_error_in_range(nfserrs_v4, ARRAY_SIZE(nfserrs_v4), error)) {
		for (uint32_t i = 0; i < ARRAY_SIZE(nfserrs_v4); i++) {
			if (error == nfserrs_v4[i].nei_error) {
				nfsclntstats.nfs_errs.errs_v4[nfserrs_v4[i].nei_index]++;
				return;
			}
		}
	}

	/* Unknown error */
	nfsclntstats.nfs_errs.errs_unknown++;
}

/*
 * compare two sockaddr structures
 */
int
nfs_sockaddr_cmp(struct sockaddr *sa1, struct sockaddr *sa2)
{
	if (!sa1) {
		return -1;
	}
	if (!sa2) {
		return 1;
	}
	if (sa1->sa_family != sa2->sa_family) {
		return (sa1->sa_family < sa2->sa_family) ? -1 : 1;
	}
	if (sa1->sa_len != sa2->sa_len) {
		return (sa1->sa_len < sa2->sa_len) ? -1 : 1;
	}
	if (sa1->sa_family == AF_INET) {
		return bcmp(&((struct sockaddr_in*)sa1)->sin_addr,
		           &((struct sockaddr_in*)sa2)->sin_addr, sizeof(((struct sockaddr_in*)sa1)->sin_addr));
	}
	if (sa1->sa_family == AF_INET6) {
		return bcmp(&((struct sockaddr_in6*)sa1)->sin6_addr,
		           &((struct sockaddr_in6*)sa2)->sin6_addr, sizeof(((struct sockaddr_in6*)sa1)->sin6_addr));
	}
	return -1;
}

int     nfs_connect_search_new_socket(struct nfsmount *, struct nfs_socket_search *, struct timeval *);
int     nfs_connect_search_socket_connect(struct nfsmount *, struct nfs_socket *, int);
int     nfs_connect_search_ping(struct nfsmount *, struct nfs_socket *, struct timeval *);
void    nfs_connect_search_socket_found(struct nfsmount *, struct nfs_socket_search *, struct nfs_socket *);
void    nfs_connect_search_socket_reap(struct nfsmount *, struct nfs_socket_search *, struct timeval *);
int     nfs_connect_search_check(struct nfsmount *, struct nfs_socket_search *, struct timeval *);
int     nfs_reconnect(struct nfsmount *);
int     nfs_connect_setup(struct nfsmount *);
void    nfs_mount_sock_thread(void *, wait_result_t);
void    nfs_udp_rcv(socket_t, void*, int);
void    nfs_tcp_rcv(socket_t, void*, int);
void    nfs_sock_poke(struct nfsmount *);
void    nfs_request_match_reply(struct nfsmount *, mbuf_t);
void    nfs_reqdequeue(struct nfsreq *);
void    nfs_reqbusy(struct nfsreq *);
struct nfsreq *nfs_reqnext(struct nfsreq *);
int     nfs_wait_reply(struct nfsreq *);
void    nfs_softterm(struct nfsreq *);
int     nfs_can_squish(struct nfsmount *);
int     nfs_is_squishy(struct nfsmount *);
int     nfs_is_dead(int, struct nfsmount *);

/*
 * Estimate rto for an nfs rpc sent via. an unreliable datagram.
 * Use the mean and mean deviation of rtt for the appropriate type of rpc
 * for the frequent rpcs and a default for the others.
 * The justification for doing "other" this way is that these rpcs
 * happen so infrequently that timer est. would probably be stale.
 * Also, since many of these rpcs are
 * non-idempotent, a conservative timeout is desired.
 * getattr, lookup - A+2D
 * read, write     - A+4D
 * other	   - nm_timeo
 */
#define NFS_RTO(n, t) \
	((t) == 0 ? (n)->nm_timeo : \
	 ((t) < 3 ? \
	  (((((n)->nm_srtt[t-1] + 3) >> 2) + (n)->nm_sdrtt[t-1] + 1) >> 1) : \
	  ((((n)->nm_srtt[t-1] + 7) >> 3) + (n)->nm_sdrtt[t-1] + 1)))
#define NFS_SRTT(r)     (r)->r_nmp->nm_srtt[proct[(r)->r_procnum] - 1]
#define NFS_SDRTT(r)    (r)->r_nmp->nm_sdrtt[proct[(r)->r_procnum] - 1]

/*
 * Defines which timer to use for the procnum.
 * 0 - default
 * 1 - getattr
 * 2 - lookup
 * 3 - read
 * 4 - write
 */
static const int proct[] = {
	[NFSPROC_NULL]                  =       0,
	[NFSPROC_GETATTR]               =       1,
	[NFSPROC_SETATTR]               =       0,
	[NFSPROC_LOOKUP]                =       2,
	[NFSPROC_ACCESS]                =       1,
	[NFSPROC_READLINK]              =       3,
	[NFSPROC_READ]                  =       3,
	[NFSPROC_WRITE]                 =       4,
	[NFSPROC_CREATE]                =       0,
	[NFSPROC_MKDIR]                 =       0,
	[NFSPROC_SYMLINK]               =       0,
	[NFSPROC_MKNOD]                 =       0,
	[NFSPROC_REMOVE]                =       0,
	[NFSPROC_RMDIR]                 =       0,
	[NFSPROC_RENAME]                =       0,
	[NFSPROC_LINK]                  =       0,
	[NFSPROC_READDIR]               =       3,
	[NFSPROC_READDIRPLUS]           =       3,
	[NFSPROC_FSSTAT]                =       0,
	[NFSPROC_FSINFO]                =       0,
	[NFSPROC_PATHCONF]              =       0,
	[NFSPROC_COMMIT]                =       0,
	[NFSPROC_NOOP]                  =       0,
};

/*
 * There is a congestion window for outstanding rpcs maintained per mount
 * point. The cwnd size is adjusted in roughly the way that:
 * Van Jacobson, Congestion avoidance and Control, In "Proceedings of
 * SIGCOMM '88". ACM, August 1988.
 * describes for TCP. The cwnd size is chopped in half on a retransmit timeout
 * and incremented by 1/cwnd when each rpc reply is received and a full cwnd
 * of rpcs is in progress.
 * (The sent count and cwnd are scaled for integer arith.)
 * Variants of "slow start" were tried and were found to be too much of a
 * performance hit (ave. rtt 3 times larger),
 * I suspect due to the large rtt that nfs rpcs have.
 */
#define NFS_CWNDSCALE   256
#define NFS_MAXCWND     (NFS_CWNDSCALE * 32)
static int nfs_backoff[8] = { 2, 4, 8, 16, 32, 64, 128, 256, };

/*
 * Increment location index to next address/server/location.
 */
void
nfs_location_next(struct nfs_fs_locations *nlp, struct nfs_location_index *nlip)
{
	uint8_t loc = nlip->nli_loc;
	uint8_t serv = nlip->nli_serv;
	uint8_t addr = nlip->nli_addr;

	/* move to next address */
	addr++;
	if (addr >= nlp->nl_locations[loc]->nl_servers[serv]->ns_addrcount) {
		/* no more addresses on current server, go to first address of next server */
next_server:
		addr = 0;
		serv++;
		if (serv >= nlp->nl_locations[loc]->nl_servcount) {
			/* no more servers on current location, go to first server of next location */
			serv = 0;
			loc++;
			if (loc >= nlp->nl_numlocs) {
				loc = 0; /* after last location, wrap back around to first location */
			}
		}
	}
	/*
	 * It's possible for this next server to not have any addresses.
	 * Check for that here and go to the next server.
	 * But bail out if we've managed to come back around to the original
	 * location that was passed in. (That would mean no servers had any
	 * addresses.  And we don't want to spin here forever.)
	 */
	if ((loc == nlip->nli_loc) && (serv == nlip->nli_serv) && (addr == nlip->nli_addr)) {
		return;
	}
	if (addr >= nlp->nl_locations[loc]->nl_servers[serv]->ns_addrcount) {
		goto next_server;
	}

	nlip->nli_loc = loc;
	nlip->nli_serv = serv;
	nlip->nli_addr = addr;
}

/*
 * Compare two location indices.
 */
int
nfs_location_index_cmp(struct nfs_location_index *nlip1, struct nfs_location_index *nlip2)
{
	if (nlip1->nli_loc != nlip2->nli_loc) {
		return nlip1->nli_loc - nlip2->nli_loc;
	}
	if (nlip1->nli_serv != nlip2->nli_serv) {
		return nlip1->nli_serv - nlip2->nli_serv;
	}
	return nlip1->nli_addr - nlip2->nli_addr;
}

/*
 * Get the mntfromname (or path portion only) for a given location.
 */
void
nfs_location_mntfromname(struct nfs_fs_locations *locs, struct nfs_location_index idx, char *s, size_t size, int pathonly)
{
	struct nfs_fs_location *fsl = locs->nl_locations[idx.nli_loc];
	char *p;
	int cnt, i;

	p = s;
	if (!pathonly) {
		char *name = fsl->nl_servers[idx.nli_serv]->ns_name;
		if (name == NULL) {
			name = "";
		}
		if (*name == '\0') {
			if (*fsl->nl_servers[idx.nli_serv]->ns_addresses[idx.nli_addr]) {
				name = fsl->nl_servers[idx.nli_serv]->ns_addresses[idx.nli_addr];
			}
			cnt = scnprintf(p, size, "<%s>:", name);
		} else {
			cnt = scnprintf(p, size, "%s:", name);
		}
		p += cnt;
		size -= cnt;
	}
	if (fsl->nl_path.np_compcount == 0) {
		/* mounting root export on server */
		if (size > 0) {
			*p++ = '/';
			*p++ = '\0';
		}
		return;
	}
	/* append each server path component */
	for (i = 0; (size > 0) && (i < (int)fsl->nl_path.np_compcount); i++) {
		cnt = scnprintf(p, size, "/%s", fsl->nl_path.np_components[i]);
		p += cnt;
		size -= cnt;
	}
}

/*
 * NFS client connect socket upcall.
 * (Used only during socket connect/search.)
 */
void
nfs_connect_upcall(socket_t so, void *arg, __unused int waitflag)
{
	struct nfs_socket *nso = arg;
	size_t rcvlen;
	mbuf_t m;
	int error = 0, recv = 1;

	lck_mtx_lock(&nso->nso_lock);
	if (nso->nso_flags & NSO_CONNECTING) {
		SP(nso, "nfs connect - socket upcall - connecting flags = %8.8x", nso->nso_flags);
		if (nso->nso_wake) {
			wakeup(nso->nso_wake);
		}
		lck_mtx_unlock(&nso->nso_lock);
		return;
	}

	if ((nso->nso_flags & (NSO_UPCALL | NSO_DISCONNECTING | NSO_DEAD)) || !(nso->nso_flags & NSO_PINGING)) {
		SP(nso, "nfs connect - socket upcall - nevermind");
		lck_mtx_unlock(&nso->nso_lock);
		return;
	}
	SP(nso, "nfs connect - socket upcall %8.8x", nso->nso_flags);
	nso->nso_flags |= NSO_UPCALL;

	/* loop while we make error-free progress */
	while (!error && recv) {
		/* make sure we're still interested in this socket */
		if (nso->nso_flags & (NSO_DISCONNECTING | NSO_DEAD)) {
			break;
		}
		lck_mtx_unlock(&nso->nso_lock);
		m = NULL;
		if (nso->nso_sotype == SOCK_STREAM) {
			error = nfs_rpc_record_read(so, &nso->nso_rrs, MSG_DONTWAIT, &recv, &m);
			NFS_SOCK_DBG("nfs_rpc_record_read returned %d recv = %d\n", error, recv);
		} else {
			rcvlen = 1000000;
			error = sock_receivembuf(so, NULL, &m, MSG_DONTWAIT, &rcvlen);
			recv = m ? 1 : 0;
		}
		lck_mtx_lock(&nso->nso_lock);
		if (m) {
			/* match response with request */
			struct nfsm_chain nmrep;
			uint32_t reply = 0, rxid = 0, verf_type, verf_len;
			uint32_t reply_status, rejected_status, accepted_status;

			NFS_SOCK_DUMP_MBUF("Got mbuf from ping", m);
			nfsm_chain_dissect_init(error, &nmrep, m);
			nfsm_chain_get_32(error, &nmrep, rxid);
			nfsm_chain_get_32(error, &nmrep, reply);
			if (!error && ((reply != RPC_REPLY) || (rxid != nso->nso_pingxid))) {
				error = EBADRPC;
			}
			nfsm_chain_get_32(error, &nmrep, reply_status);
			if (!error && (reply_status == RPC_MSGDENIED)) {
				nfsm_chain_get_32(error, &nmrep, rejected_status);
				if (!error) {
					error = (rejected_status == RPC_MISMATCH) ? ERPCMISMATCH : EACCES;
				}
			}
			nfsm_chain_get_32(error, &nmrep, verf_type); /* verifier flavor */
			nfsm_chain_get_32(error, &nmrep, verf_len); /* verifier length */
			nfsmout_if(error);
			if (verf_len) {
				nfsm_chain_adv(error, &nmrep, nfsm_rndup(verf_len));
			}
			nfsm_chain_get_32(error, &nmrep, accepted_status);
			nfsmout_if(error);
			NFS_SOCK_DBG("Recevied accepted_status of %d  nso_version = %d\n", accepted_status, nso->nso_version);
			if ((accepted_status == RPC_PROGMISMATCH) && !nso->nso_version) {
				uint32_t minvers, maxvers;
				nfsm_chain_get_32(error, &nmrep, minvers);
				nfsm_chain_get_32(error, &nmrep, maxvers);
				nfsmout_if(error);
				if (nso->nso_protocol == PMAPPROG) {
					if ((minvers > RPCBVERS4) || (maxvers < PMAPVERS)) {
						error = EPROGMISMATCH;
					} else if ((nso->nso_saddr->sa_family == AF_INET) &&
					    (PMAPVERS >= minvers) && (PMAPVERS <= maxvers)) {
						nso->nso_version = PMAPVERS;
					} else if (nso->nso_saddr->sa_family == AF_INET6) {
						if ((RPCBVERS4 >= minvers) && (RPCBVERS4 <= maxvers)) {
							nso->nso_version = RPCBVERS4;
						} else if ((RPCBVERS3 >= minvers) && (RPCBVERS3 <= maxvers)) {
							nso->nso_version = RPCBVERS3;
						}
					}
				} else if (nso->nso_protocol == NFS_PROG) {
					int vers;

					/*
					 * N.B. Both portmapper and rpcbind V3 are happy to return
					 * addresses for other versions than the one you ask (getport or
					 * getaddr) and thus we may have fallen to this code path. So if
					 * we get a version that we support, use highest supported
					 * version.  This assumes that the server supports all versions
					 * between minvers and maxvers.  Note for IPv6 we will try and
					 * use rpcbind V4 which has getversaddr and we should not get
					 * here if that was successful.
					 */
					for (vers = nso->nso_nfs_max_vers; vers >= (int)nso->nso_nfs_min_vers; vers--) {
						if (vers >= (int)minvers && vers <= (int)maxvers) {
							break;
						}
					}
					nso->nso_version = (vers < (int)nso->nso_nfs_min_vers) ? 0 : vers;
				}
				if (!error && nso->nso_version) {
					accepted_status = RPC_SUCCESS;
				}
			}
			if (!error) {
				switch (accepted_status) {
				case RPC_SUCCESS:
					error = 0;
					break;
				case RPC_PROGUNAVAIL:
					error = EPROGUNAVAIL;
					break;
				case RPC_PROGMISMATCH:
					error = EPROGMISMATCH;
					break;
				case RPC_PROCUNAVAIL:
					error = EPROCUNAVAIL;
					break;
				case RPC_GARBAGE:
					error = EBADRPC;
					break;
				case RPC_SYSTEM_ERR:
				default:
					error = EIO;
					break;
				}
			}
nfsmout:
			nso->nso_flags &= ~NSO_PINGING;
			if (error) {
				NFS_SOCK_DBG("nfs upcalled failed for %d program %d vers error = %d\n",
				    nso->nso_protocol, nso->nso_version, error);
				nso->nso_error = error;
				nso->nso_flags |= NSO_DEAD;
			} else {
				nso->nso_flags |= NSO_VERIFIED;
			}
			mbuf_freem(m);
			/* wake up search thread */
			wakeup(nso->nso_wake);
			break;
		}
	}

	nso->nso_flags &= ~NSO_UPCALL;
	if ((error != EWOULDBLOCK) && (error || !recv)) {
		/* problems with the socket... */
		NFS_SOCK_DBG("connect upcall failed %d\n", error);
		nso->nso_error = error ? error : EPIPE;
		nso->nso_flags |= NSO_DEAD;
		wakeup(nso->nso_wake);
	}
	if (nso->nso_flags & NSO_DISCONNECTING) {
		wakeup(&nso->nso_flags);
	}
	lck_mtx_unlock(&nso->nso_lock);
}

/*
 * Create/initialize an nfs_socket structure.
 */
int
nfs_socket_create(
	struct nfsmount *nmp,
	struct sockaddr *sa,
	uint8_t sotype,
	in_port_t port,
	uint32_t protocol,
	uint32_t vers,
	int resvport,
	struct nfs_socket **nsop)
{
	struct nfs_socket *nso;
	struct timeval now;
	int error;
#define NFS_SOCKET_DEBUGGING
#ifdef NFS_SOCKET_DEBUGGING
	char naddr[sizeof((struct sockaddr_un *)0)->sun_path];
	void *sinaddr;

	switch (sa->sa_family) {
	case AF_INET:
		if (sa->sa_len != sizeof(struct sockaddr_in)) {
			return EINVAL;
		}
		sinaddr = &((struct sockaddr_in*)sa)->sin_addr;
		if (inet_ntop(sa->sa_family, sinaddr, naddr, sizeof(naddr)) != naddr) {
			strlcpy(naddr, "<unknown>", sizeof(naddr));
		}
		break;
	case AF_INET6:
		if (sa->sa_len != sizeof(struct sockaddr_in6)) {
			return EINVAL;
		}
		sinaddr = &((struct sockaddr_in6*)sa)->sin6_addr;
		if (inet_ntop(sa->sa_family, sinaddr, naddr, sizeof(naddr)) != naddr) {
			strlcpy(naddr, "<unknown>", sizeof(naddr));
		}
		break;
	case AF_LOCAL:
		if (sa->sa_len != sizeof(struct sockaddr_un) && sa->sa_len != SUN_LEN((struct sockaddr_un *)sa)) {
			return EINVAL;
		}
		strlcpy(naddr, ((struct sockaddr_un *)sa)->sun_path, sizeof(naddr));
		break;
	default:
		strlcpy(naddr, "<unsupported address family>", sizeof(naddr));
		break;
	}
#else
	char naddr[1] =  { 0 };
#endif

	*nsop = NULL;

	/* Create the socket. */
	nso = kalloc_type(struct nfs_socket, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	nso->nso_saddr = kalloc_data(sa->sa_len, Z_WAITOK | Z_ZERO | Z_NOFAIL);

	lck_mtx_init(&nso->nso_lock, get_lck_group(NLG_REQUEST), LCK_ATTR_NULL);
	nso->nso_sotype = sotype;
	if (nso->nso_sotype == SOCK_STREAM) {
		nfs_rpc_record_state_init(&nso->nso_rrs);
	}
	microuptime(&now);
	nso->nso_timestamp = now.tv_sec;
	bcopy(sa, nso->nso_saddr, sa->sa_len);
	switch (sa->sa_family) {
	case AF_INET:
	case AF_INET6:
		if (sa->sa_family == AF_INET) {
			((struct sockaddr_in*)nso->nso_saddr)->sin_port = htons(port);
		} else if (sa->sa_family == AF_INET6) {
			((struct sockaddr_in6*)nso->nso_saddr)->sin6_port = htons(port);
		}
		break;
	case AF_LOCAL:
		break;
	}
	nso->nso_protocol = protocol;
	nso->nso_version = vers;
	nso->nso_nfs_min_vers = PVER2MAJOR(nmp->nm_min_vers);
	nso->nso_nfs_max_vers = PVER2MAJOR(nmp->nm_max_vers);

	error = sock_socket(sa->sa_family, nso->nso_sotype, 0, NULL, NULL, &nso->nso_so);

	/* Some servers require that the client port be a reserved port number. */
	if (!error && resvport && ((sa->sa_family == AF_INET) || (sa->sa_family == AF_INET6))) {
		struct sockaddr_storage ss;
		int level = (sa->sa_family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6;
		int optname = (sa->sa_family == AF_INET) ? IP_PORTRANGE : IPV6_PORTRANGE;
		int portrange = IP_PORTRANGE_LOW;

		error = sock_setsockopt(nso->nso_so, level, optname, &portrange, sizeof(portrange));
		if (!error) {   /* bind now to check for failure */
			ss.ss_len = sa->sa_len;
			ss.ss_family = sa->sa_family;
			if (ss.ss_family == AF_INET) {
				((struct sockaddr_in*)&ss)->sin_addr.s_addr = INADDR_ANY;
				((struct sockaddr_in*)&ss)->sin_port = htons(0);
			} else if (ss.ss_family == AF_INET6) {
				((struct sockaddr_in6*)&ss)->sin6_addr = in6addr_any;
				((struct sockaddr_in6*)&ss)->sin6_port = htons(0);
			} else {
				error = EINVAL;
			}
			if (!error) {
				error = sock_bind(nso->nso_so, (struct sockaddr*)&ss);
			}
		}
	}

	if (error) {
		SP(nso, "nfs connect %s error %d creating socket %s type %d%s port %d prot %d %d",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error, naddr, sotype,
		    resvport ? "r" : "", port, protocol, vers);
		nfs_socket_destroy(nso);
	} else {
		SP(nso, "nfs connect %s created socket <%s> type %d%s port %d prot %d %d",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, naddr,
		    sotype, resvport ? "r" : "", port, protocol, vers);
		*nsop = nso;
	}
	return error;
}

/*
 * Destroy an nfs_socket structure.
 */
void
nfs_socket_destroy(struct nfs_socket *nso)
{
	struct timespec ts = { .tv_sec = 4, .tv_nsec = 0 };

	SP(nso, "Destoring socket flags = %8.8x error = %d", nso->nso_flags, nso->nso_error);
	lck_mtx_lock(&nso->nso_lock);
	nso->nso_flags |= NSO_DISCONNECTING;
	if (nso->nso_flags & NSO_UPCALL) { /* give upcall a chance to complete */
		msleep(&nso->nso_flags, &nso->nso_lock, PZERO - 1, "nfswaitupcall", &ts);
	}
	lck_mtx_unlock(&nso->nso_lock);
	sock_setupcall(nso->nso_so, NULL, NULL);
	sock_shutdown(nso->nso_so, SHUT_RDWR);
	sock_close(nso->nso_so);
	if (nso->nso_sotype == SOCK_STREAM) {
		nfs_rpc_record_state_cleanup(&nso->nso_rrs);
	}
	lck_mtx_destroy(&nso->nso_lock, get_lck_group(NLG_REQUEST));

	if (nso->nso_saddr) {
		kfree_data(nso->nso_saddr, nso->nso_saddr->sa_len);
	}
	if (nso->nso_saddr2) {
		kfree_data(nso->nso_saddr2, nso->nso_saddr2->sa_len);
	}

	SP(nso, "nfs connect - socket destroyed");
	kfree_type(struct nfs_socket, nso);
}

/*
 * Set common socket options on an nfs_socket.
 */
void
nfs_socket_options(struct nfsmount *nmp, struct nfs_socket *nso)
{
	/*
	 * Set socket send/receive timeouts
	 * - Receive timeout shouldn't matter because most receives are performed
	 *   in the socket upcall non-blocking.
	 * - Send timeout should allow us to react to a blocked socket.
	 *   Soft mounts will want to abort sooner.
	 */
	struct timeval timeo;
	int on = 1, proto, reserve, error;

	timeo.tv_usec = 0;
	timeo.tv_sec = (NMFLAG(nmp, SOFT) || nfs_can_squish(nmp)) ? 5 : 60;
	sock_setsockopt(nso->nso_so, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
	sock_setsockopt(nso->nso_so, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
	if (nso->nso_sotype == SOCK_STREAM) {
		/* Assume that SOCK_STREAM always requires a connection */
		sock_setsockopt(nso->nso_so, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on));
		/* set nodelay for TCP */
		sock_gettype(nso->nso_so, NULL, NULL, &proto);
		if (proto == IPPROTO_TCP) {
			sock_setsockopt(nso->nso_so, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
		}
	}

	/* set socket buffer sizes for UDP/TCP */
	reserve = (nso->nso_sotype == SOCK_DGRAM) ? NFS_UDPSOCKBUF : MAX(nfs_tcp_sockbuf, nmp->nm_wsize * 2);
	error = sock_setsockopt(nso->nso_so, SOL_SOCKET, SO_SNDBUF, &reserve, sizeof(reserve));
	if (error) {
		log(LOG_INFO, "nfs_socket_options: error %d setting SO_SNDBUF to %u\n", error, reserve);
	}

	reserve = (nso->nso_sotype == SOCK_DGRAM) ? NFS_UDPSOCKBUF : MAX(nfs_tcp_sockbuf, nmp->nm_rsize * 2);
	error = sock_setsockopt(nso->nso_so, SOL_SOCKET, SO_RCVBUF, &reserve, sizeof(reserve));
	if (error) {
		log(LOG_INFO, "nfs_socket_options: error %d setting SO_RCVBUF to %u\n", error, reserve);
	}

	/* set SO_NOADDRERR to detect network changes ASAP */
	sock_setsockopt(nso->nso_so, SOL_SOCKET, SO_NOADDRERR, &on, sizeof(on));
	/* just playin' it safe with upcalls */
	sock_setsockopt(nso->nso_so, SOL_SOCKET, SO_UPCALLCLOSEWAIT, &on, sizeof(on));
	/* socket should be interruptible if the mount is */
	if (!NMFLAG(nmp, INTR)) {
		sock_nointerrupt(nso->nso_so, 1);
	}
}

/*
 * Release resources held in an nfs_socket_search.
 */
void
nfs_socket_search_cleanup(struct nfs_socket_search *nss)
{
	struct nfs_socket *nso, *nsonext;

	TAILQ_FOREACH_SAFE(nso, &nss->nss_socklist, nso_link, nsonext) {
		TAILQ_REMOVE(&nss->nss_socklist, nso, nso_link);
		nss->nss_sockcnt--;
		nfs_socket_destroy(nso);
	}
	if (nss->nss_sock) {
		nfs_socket_destroy(nss->nss_sock);
		nss->nss_sock = NULL;
	}
}

/*
 * Prefer returning certain errors over others.
 * This function returns a ranking of the given error.
 */
int
nfs_connect_error_class(int error)
{
	switch (error) {
	case 0:
		return 0;
	case ETIMEDOUT:
	case EAGAIN:
		return 1;
	case EPIPE:
	case EADDRNOTAVAIL:
	case ENETDOWN:
	case ENETUNREACH:
	case ENETRESET:
	case ECONNABORTED:
	case ECONNRESET:
	case EISCONN:
	case ENOTCONN:
	case ESHUTDOWN:
	case ECONNREFUSED:
	case EHOSTDOWN:
	case EHOSTUNREACH:
		return 2;
	case ERPCMISMATCH:
	case EPROCUNAVAIL:
	case EPROGMISMATCH:
	case EPROGUNAVAIL:
		return 3;
	case EBADRPC:
		return 4;
	default:
		return 5;
	}
}

/*
 * Make sure a socket search returns the best error.
 */
void
nfs_socket_search_update_error(struct nfs_socket_search *nss, int error)
{
	if (nfs_connect_error_class(error) >= nfs_connect_error_class(nss->nss_error)) {
		nss->nss_error = error;
	}
}

/* nfs_connect_search_new_socket:
 *      Given a socket search structure for an nfs mount try to find a new socket from the set of addresses specified
 *	by nss.
 *
 *	nss_last is set to -1 at initialization to indicate the first time. Its set to -2 if address was found but
 *	could not be used or if a socket timed out.
 */
int
nfs_connect_search_new_socket(struct nfsmount *nmp, struct nfs_socket_search *nss, struct timeval *now)
{
	struct nfs_fs_location *fsl;
	struct nfs_fs_server *fss;
	struct sockaddr_storage ss;
	struct nfs_socket *nso;
	char *addrstr;
	int error = 0;


	NFS_SOCK_DBG("nfs connect %s nss_addrcnt = %d\n",
	    vfs_statfs(nmp->nm_mountp)->f_mntfromname, nss->nss_addrcnt);

	/*
	 * while there are addresses and:
	 *	we have no sockets or
	 *	the last address failed and did not produce a socket (nss_last < 0) or
	 *	Its been a while (2 seconds) and we have less than the max number of concurrent sockets to search (4)
	 *	then attempt to create a socket with the current address.
	 */
	while (nss->nss_addrcnt > 0 && ((nss->nss_last < 0) || (nss->nss_sockcnt == 0) ||
	    ((nss->nss_sockcnt < 4) && (now->tv_sec >= (nss->nss_last + 2))))) {
		if (nmp->nm_sockflags & NMSOCK_UNMOUNT) {
			return EINTR;
		}
		/* Can we convert the address to a sockaddr? */
		fsl = nmp->nm_locations.nl_locations[nss->nss_nextloc.nli_loc];
		fss = fsl->nl_servers[nss->nss_nextloc.nli_serv];
		addrstr = fss->ns_addresses[nss->nss_nextloc.nli_addr];
		NFS_SOCK_DBG("Trying address %s for program %d on port %d\n", addrstr, nss->nss_protocol, nss->nss_port);
		if (*addrstr == '\0') {
			/*
			 * We have an unspecified local domain address. We use the program to translate to
			 * a well known local transport address. We only support PMAPROG and NFS for this.
			 */
			if (nss->nss_protocol == PMAPPROG) {
				addrstr = (nss->nss_sotype == SOCK_DGRAM) ? RPCB_TICLTS_PATH : RPCB_TICOTSORD_PATH;
			} else if (nss->nss_protocol == NFS_PROG) {
				addrstr = nmp->nm_nfs_localport;
				if (!addrstr || *addrstr == '\0') {
					addrstr = (nss->nss_sotype == SOCK_DGRAM) ? NFS_TICLTS_PATH : NFS_TICOTSORD_PATH;
				}
			}
			NFS_SOCK_DBG("Calling  prog %d with <%s>\n", nss->nss_protocol, addrstr);
		}
		if (!nfs_uaddr2sockaddr(addrstr, (struct sockaddr*)&ss)) {
			NFS_SOCK_DBG("Could not convert address %s to socket\n", addrstr);
			nfs_location_next(&nmp->nm_locations, &nss->nss_nextloc);
			nss->nss_addrcnt -= 1;
			nss->nss_last = -2;
			continue;
		}
		/* Check that socket family is acceptable. */
		if (nmp->nm_sofamily && (ss.ss_family != nmp->nm_sofamily)) {
			NFS_SOCK_DBG("Skipping socket family %d, want mount family %d\n", ss.ss_family, nmp->nm_sofamily);
			nfs_location_next(&nmp->nm_locations, &nss->nss_nextloc);
			nss->nss_addrcnt -= 1;
			nss->nss_last = -2;
			continue;
		}

		/* Create the socket. */
		error = nfs_socket_create(nmp, (struct sockaddr*)&ss, nss->nss_sotype,
		    nss->nss_port, nss->nss_protocol, nss->nss_version,
		    ((nss->nss_protocol == NFS_PROG) && NMFLAG(nmp, RESVPORT)), &nso);
		if (error) {
			return error;
		}

		nso->nso_location = nss->nss_nextloc;
		nso->nso_wake = nss;

		TAILQ_INSERT_TAIL(&nss->nss_socklist, nso, nso_link);
		nss->nss_sockcnt++;
		nfs_location_next(&nmp->nm_locations, &nss->nss_nextloc);
		nss->nss_addrcnt -= 1;

		nss->nss_last = now->tv_sec;
	}

	if (nss->nss_addrcnt == 0 && nss->nss_last < 0) {
		nss->nss_last = now->tv_sec;
	}

	return error;
}

/*
 * nfs_connect_search_socket_connect:	Connect an nfs socket nso for nfsmount nmp.
 *					If successful set the socket options for the socket as require from the mount.
 *
 * Assumes:				nso->nso_lock is held on entry and return.
 */
int
nfs_connect_search_socket_connect(struct nfsmount *nmp, struct nfs_socket *nso, int verbose)
{
	int error;

	if ((nso->nso_sotype != SOCK_STREAM) && NMFLAG(nmp, NOCONNECT)) {
		/* no connection needed, just say it's already connected */
		SP(nso, "nfs connect %s UDP socket noconnect",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname);
		nso->nso_flags |= NSO_CONNECTED;
		nfs_socket_options(nmp, nso);
		return 1;   /* Socket is connected and setup */
	} else if (!(nso->nso_flags & NSO_CONNECTING)) {
		/* initiate the connection */
		nso->nso_flags |= NSO_CONNECTING;
		lck_mtx_unlock(&nso->nso_lock);
		SP(nso, "nfs connect %s connecting socket %s",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname,
		    nso->nso_saddr->sa_family == AF_LOCAL ? ((struct sockaddr_un*)nso->nso_saddr)->sun_path : "");
		error = sock_connect(nso->nso_so, nso->nso_saddr, MSG_DONTWAIT);
		if (error) {
			SP(nso, "nfs connect %s connecting socket returned %d",
			    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error);
		}
		lck_mtx_lock(&nso->nso_lock);
		if (error && (error != EINPROGRESS)) {
			nso->nso_error = error;
			nso->nso_flags |= NSO_DEAD;
			return 0;
		}
	}
	if (nso->nso_flags & NSO_CONNECTING) {
		/* check the connection */
		if (sock_isconnected(nso->nso_so)) {
			SP(nso, "nfs connect %s socket is connected",
			    vfs_statfs(nmp->nm_mountp)->f_mntfromname);

			/* set connect upcall once socket is connected */
			lck_mtx_unlock(&nso->nso_lock);
			error = sock_setupcall(nso->nso_so, nfs_connect_upcall, nso);
			lck_mtx_lock(&nso->nso_lock);
			if (error) {
				SP(nso, "sock_setupcall failed for socket setting nfs_connect_upcall error = %d", error);
				nso->nso_error = error;
				nso->nso_flags |= NSO_DEAD;
				return 0;
			}

			nso->nso_flags &= ~NSO_CONNECTING;
			nso->nso_flags |= NSO_CONNECTED;
			nfs_socket_options(nmp, nso);
			return 1;   /* Socket is connected and setup */
		} else {
			int optlen = sizeof(error);
			error = 0;
			sock_getsockopt(nso->nso_so, SOL_SOCKET, SO_ERROR, &error, &optlen);
			if (error) { /* we got an error on the socket */
				SP(nso, "nfs connect %s socket connection error %d",
				    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error);
				if (verbose) {
					printf("nfs connect socket error %d for %s\n",
					    error, vfs_statfs(nmp->nm_mountp)->f_mntfromname);
				}
				nso->nso_error = error;
				nso->nso_flags |= NSO_DEAD;
				return 0;
			}
		}
	}

	return 0;  /* Waiting to be connected */
}

/*
 * nfs_connect_search_ping:	Send a null proc on the nso socket.
 */
int
nfs_connect_search_ping(struct nfsmount *nmp, struct nfs_socket *nso, struct timeval *now)
{
	/* initiate a NULL RPC request */
	uint64_t xid = nso->nso_pingxid;
	mbuf_t m, mreq = NULL;
	struct msghdr msg;
	size_t reqlen, sentlen;
	uint32_t vers = nso->nso_version;
	int error;

	if (!vers) {
		if (nso->nso_protocol == PMAPPROG) {
			vers = (nso->nso_saddr->sa_family == AF_INET) ? PMAPVERS : RPCBVERS4;
		} else if (nso->nso_protocol == NFS_PROG) {
			vers = PVER2MAJOR(nmp->nm_max_vers);
		}
	}
	lck_mtx_unlock(&nso->nso_lock);
	SP(nso, "Pinging  socket %d %d %d", nso->nso_sotype, nso->nso_protocol, vers);
	error = nfsm_rpchead2(nmp, nso->nso_sotype, nso->nso_protocol, vers, 0, RPCAUTH_SYS,
	    vfs_context_ucred(vfs_context_kernel()), NULL, NULL, &xid, &mreq);
	lck_mtx_lock(&nso->nso_lock);
	if (!error) {
		nso->nso_flags |= NSO_PINGING;
		nso->nso_pingxid = R_XID32(xid);
		nso->nso_reqtimestamp = now->tv_sec;
		bzero(&msg, sizeof(msg));
		if ((nso->nso_sotype != SOCK_STREAM) && !sock_isconnected(nso->nso_so)) {
			msg.msg_name = nso->nso_saddr;
			msg.msg_namelen = nso->nso_saddr->sa_len;
		}
		for (reqlen = 0, m = mreq; m; m = mbuf_next(m)) {
			reqlen += mbuf_len(m);
		}
		lck_mtx_unlock(&nso->nso_lock);
		NFS_SOCK_DUMP_MBUF("Sending ping packet", mreq);
		error = nfs_sendmbuf(nso->nso_saddr, nso->nso_so, &msg, mreq, 0, &sentlen);
		SP(nso, "nfs connect %s verifying socket send rv %d",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error);
		lck_mtx_lock(&nso->nso_lock);
		if (!error && (sentlen != reqlen)) {
			error = ETIMEDOUT;
		}
	}
	if (error) {
		nso->nso_error = error;
		nso->nso_flags |= NSO_DEAD;
		return 0;
	}

	return 1;
}

/*
 * nfs_connect_search_socket_found:	Take the found socket of the socket search list and assign it to the searched socket.
 *					Set the nfs socket protocol and version if needed.
 */
void
nfs_connect_search_socket_found(struct nfsmount *nmp, struct nfs_socket_search *nss, struct nfs_socket *nso)
{
	SP(nso, "nfs connect %s socket verified",
	    vfs_statfs(nmp->nm_mountp)->f_mntfromname);
	if (!nso->nso_version) {
		/* If the version isn't set, the default must have worked. */
		if (nso->nso_protocol == PMAPPROG) {
			nso->nso_version = (nso->nso_saddr->sa_family == AF_INET) ? PMAPVERS : RPCBVERS4;
		}
		if (nso->nso_protocol == NFS_PROG) {
			nso->nso_version = PVER2MAJOR(nmp->nm_max_vers);
		}
	}
	TAILQ_REMOVE(&nss->nss_socklist, nso, nso_link);
	nss->nss_sockcnt--;
	nss->nss_sock = nso;
}

/*
 * nfs_connect_search_socket_reap:	For each socket in the search list mark any timed out socket as dead and remove from
 *					the list. Dead socket are then destroyed.
 */
void
nfs_connect_search_socket_reap(struct nfsmount *nmp __unused, struct nfs_socket_search *nss, struct timeval *now)
{
	struct nfs_socket *nso, *nsonext;

	TAILQ_FOREACH_SAFE(nso, &nss->nss_socklist, nso_link, nsonext) {
		lck_mtx_lock(&nso->nso_lock);
		if (now->tv_sec >= (nso->nso_timestamp + nss->nss_timeo)) {
			/* took too long */
			SP(nso, "nfs connect %s socket timed out",
			    vfs_statfs(nmp->nm_mountp)->f_mntfromname);
			nso->nso_error = ETIMEDOUT;
			nso->nso_flags |= NSO_DEAD;
		}
		if (!(nso->nso_flags & NSO_DEAD)) {
			lck_mtx_unlock(&nso->nso_lock);
			continue;
		}
		lck_mtx_unlock(&nso->nso_lock);
		SP(nso, "nfs connect %s reaping socket error = %d flags = %8.8x",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, nso->nso_error, nso->nso_flags);
		nfs_socket_search_update_error(nss, nso->nso_error);
		TAILQ_REMOVE(&nss->nss_socklist, nso, nso_link);
		nss->nss_sockcnt--;
		nfs_socket_destroy(nso);
		/* If there are more sockets to try, force the starting of another socket */
		if (nss->nss_addrcnt > 0) {
			nss->nss_last = -2;
		}
	}
}

/*
 * nfs_connect_search_check:	Check on the status of search and wait for replies if needed.
 */
int
nfs_connect_search_check(struct nfsmount *nmp, struct nfs_socket_search *nss, struct timeval *now)
{
	int error, timeo;

	/* log a warning if connect is taking a while */
	if (((now->tv_sec - nss->nss_timestamp) >= 8) && ((nss->nss_flags & (NSS_VERBOSE | NSS_WARNED)) == NSS_VERBOSE)) {
		printf("nfs_connect: socket connect taking a while for %s\n", vfs_statfs(nmp->nm_mountp)->f_mntfromname);
		nss->nss_flags |= NSS_WARNED;
	}
	if (nmp->nm_sockflags & NMSOCK_UNMOUNT) {
		return EINTR;
	}
	if ((error = nfs_sigintr(nmp, NULL, current_thread(), 0))) {
		return error;
	}

	/* If we were succesfull at sending a ping, wait up to a second for a reply  */
	if (nss->nss_last >= 0) {
		timeo = (nmp->nm_sockflags & NMSOCK_HASCONNECTED) ? hz : hz / 100;
		tsleep(nss, PSOCK, "nfs_connect_search_wait", timeo);
	}

	return 0;
}


/*
 * Continue the socket search until we have something to report.
 */
int
nfs_connect_search_loop(struct nfsmount *nmp, struct nfs_socket_search *nss)
{
	struct nfs_socket *nso;
	struct timeval now;
	int error;
	int verbose = (nss->nss_flags & NSS_VERBOSE);

loop:
	microuptime(&now);
	NFS_SOCK_DBG("nfs connect %s search %ld\n", vfs_statfs(nmp->nm_mountp)->f_mntfromname, now.tv_sec);

	/* add a new socket to the socket list if needed and available */
	error = nfs_connect_search_new_socket(nmp, nss, &now);
	if (error) {
		NFS_SOCK_DBG("nfs connect returned %d\n", error);
		return error;
	}

	/* check each active socket on the list and try to push it along */
	TAILQ_FOREACH(nso, &nss->nss_socklist, nso_link) {
		lck_mtx_lock(&nso->nso_lock);

		/* If not connected connect it */
		if (!(nso->nso_flags & NSO_CONNECTED)) {
			if (!nfs_connect_search_socket_connect(nmp, nso, verbose)) {
				lck_mtx_unlock(&nso->nso_lock);
				continue;
			}
		}

		/* If the socket hasn't been verified or in a ping, ping it. We also handle UDP retransmits */
		if (!(nso->nso_flags & (NSO_PINGING | NSO_VERIFIED)) ||
		    ((nso->nso_sotype == SOCK_DGRAM) && (now.tv_sec >= nso->nso_reqtimestamp + 2))) {
			if (!nfs_connect_search_ping(nmp, nso, &now)) {
				lck_mtx_unlock(&nso->nso_lock);
				continue;
			}
		}

		/* Has the socket been verified by the up call routine? */
		if (nso->nso_flags & NSO_VERIFIED) {
			/* WOOHOO!! This socket looks good! */
			nfs_connect_search_socket_found(nmp, nss, nso);
			lck_mtx_unlock(&nso->nso_lock);
			break;
		}
		lck_mtx_unlock(&nso->nso_lock);
	}

	/* Check for timed out sockets and mark as dead and then remove all dead sockets. */
	nfs_connect_search_socket_reap(nmp, nss, &now);

	/*
	 * Keep looping if we haven't found a socket yet and we have more
	 * sockets to (continue to) try.
	 */
	error = 0;
	if (!nss->nss_sock && (!TAILQ_EMPTY(&nss->nss_socklist) || nss->nss_addrcnt)) {
		error = nfs_connect_search_check(nmp, nss, &now);
		if (!error) {
			goto loop;
		}
	}

	NFS_SOCK_DBG("nfs connect %s returning %d\n", vfs_statfs(nmp->nm_mountp)->f_mntfromname, error);
	return error;
}

/*
 * Initialize a new NFS connection.
 *
 * Search for a location to connect a socket to and initialize the connection.
 *
 * An NFS mount may have multiple locations/servers/addresses available.
 * We attempt to connect to each one asynchronously and will start
 * several sockets in parallel if other locations are slow to answer.
 * We'll use the first NFS socket we can successfully set up.
 *
 * The search may involve contacting the portmapper service first.
 *
 * A mount's initial connection may require negotiating some parameters such
 * as socket type and NFS version.
 */

int
nfs_connect(struct nfsmount *nmp, int verbose, int timeo)
{
	struct nfs_socket_search nss;
	struct nfs_socket *nso, *nsonfs;
	struct sockaddr_storage ss;
	struct sockaddr *saddr, *oldsaddr;
	sock_upcall upcall;
#if CONFIG_NFS4
	struct timeval now;
#endif
	struct timeval start;
	int error, savederror, nfsvers;
	int tryv4 = 1;
	uint8_t sotype = nmp->nm_sotype ? nmp->nm_sotype : SOCK_STREAM;
	fhandle_t *fh = NULL;
	char *path = NULL;
	in_port_t port = 0;
	int addrtotal = 0;

	/* paranoia... check that we have at least one address in the locations */
	uint32_t loc, serv;
	for (loc = 0; loc < nmp->nm_locations.nl_numlocs; loc++) {
		for (serv = 0; serv < nmp->nm_locations.nl_locations[loc]->nl_servcount; serv++) {
			addrtotal += nmp->nm_locations.nl_locations[loc]->nl_servers[serv]->ns_addrcount;
			if (nmp->nm_locations.nl_locations[loc]->nl_servers[serv]->ns_addrcount == 0) {
				NFS_SOCK_DBG("nfs connect %s search, server %s has no addresses\n",
				    vfs_statfs(nmp->nm_mountp)->f_mntfromname,
				    nmp->nm_locations.nl_locations[loc]->nl_servers[serv]->ns_name);
			}
		}
	}

	if (addrtotal == 0) {
		NFS_SOCK_DBG("nfs connect %s search failed, no addresses\n",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname);
		return EINVAL;
	} else {
		NFS_SOCK_DBG("nfs connect %s has %d addresses\n",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, addrtotal);
	}

	lck_mtx_lock(&nmp->nm_lock);
	nmp->nm_sockflags |= NMSOCK_CONNECTING;
	nmp->nm_nss = &nss;
	lck_mtx_unlock(&nmp->nm_lock);
	microuptime(&start);
	savederror = error = 0;

tryagain:
	/* initialize socket search state */
	bzero(&nss, sizeof(nss));
	nss.nss_addrcnt = addrtotal;
	nss.nss_error = savederror;
	TAILQ_INIT(&nss.nss_socklist);
	nss.nss_sotype = sotype;
	nss.nss_startloc = nmp->nm_locations.nl_current;
	nss.nss_timestamp = start.tv_sec;
	nss.nss_timeo = timeo;
	if (verbose) {
		nss.nss_flags |= NSS_VERBOSE;
	}

	/* First time connecting, we may need to negotiate some things */
	if (!(nmp->nm_sockflags & NMSOCK_HASCONNECTED)) {
		NFS_SOCK_DBG("so_family = %d\n", nmp->nm_sofamily);
		NFS_SOCK_DBG("nfs port = %d local: <%s>\n", nmp->nm_nfsport, nmp->nm_nfs_localport ? nmp->nm_nfs_localport : "");
		NFS_SOCK_DBG("mount port = %d local: <%s>\n", nmp->nm_mountport, nmp->nm_mount_localport ? nmp->nm_mount_localport : "");
		if (!nmp->nm_vers) {
			/* No NFS version specified... */
			if (!nmp->nm_nfsport || (!NM_OMATTR_GIVEN(nmp, FH) && !nmp->nm_mountport)) {
#if CONFIG_NFS4
				if (PVER2MAJOR(nmp->nm_max_vers) >= NFS_VER4 && tryv4) {
					nss.nss_port = NFS_PORT;
					nss.nss_protocol = NFS_PROG;
					nss.nss_version = 4;
					nss.nss_flags |= NSS_FALLBACK2PMAP;
				} else {
#endif
				/* ...connect to portmapper first if we (may) need any ports. */
				nss.nss_port = PMAPPORT;
				nss.nss_protocol = PMAPPROG;
				nss.nss_version = 0;
#if CONFIG_NFS4
			}
#endif
			} else {
				/* ...connect to NFS port first. */
				nss.nss_port = nmp->nm_nfsport;
				nss.nss_protocol = NFS_PROG;
				nss.nss_version = 0;
			}
#if CONFIG_NFS4
		} else if (nmp->nm_vers >= NFS_VER4) {
			if (tryv4) {
				/* For NFSv4, we use the given (or default) port. */
				nss.nss_port = nmp->nm_nfsport ? nmp->nm_nfsport : NFS_PORT;
				nss.nss_protocol = NFS_PROG;
				nss.nss_version = 4;
				/*
				 * set NSS_FALLBACK2PMAP here to pick up any non standard port
				 * if no port is specified on the mount;
				 * Note nm_vers is set so we will only try NFS_VER4.
				 */
				if (!nmp->nm_nfsport) {
					nss.nss_flags |= NSS_FALLBACK2PMAP;
				}
			} else {
				nss.nss_port = PMAPPORT;
				nss.nss_protocol = PMAPPROG;
				nss.nss_version = 0;
			}
#endif
		} else {
			/* For NFSv3/v2... */
			if (!nmp->nm_nfsport || (!NM_OMATTR_GIVEN(nmp, FH) && !nmp->nm_mountport)) {
				/* ...connect to portmapper first if we need any ports. */
				nss.nss_port = PMAPPORT;
				nss.nss_protocol = PMAPPROG;
				nss.nss_version = 0;
			} else {
				/* ...connect to NFS port first. */
				nss.nss_port = nmp->nm_nfsport;
				nss.nss_protocol = NFS_PROG;
				nss.nss_version = nmp->nm_vers;
			}
		}
		NFS_SOCK_DBG("nfs connect first %s, so type %d port %d prot %d %d\n",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, nss.nss_sotype, nss.nss_port,
		    nss.nss_protocol, nss.nss_version);
	} else {
		/* we've connected before, just connect to NFS port */
		if (!nmp->nm_nfsport) {
			/* need to ask portmapper which port that would be */
			nss.nss_port = PMAPPORT;
			nss.nss_protocol = PMAPPROG;
			nss.nss_version = 0;
		} else {
			nss.nss_port = nmp->nm_nfsport;
			nss.nss_protocol = NFS_PROG;
			nss.nss_version = nmp->nm_vers;
		}
		NFS_SOCK_DBG("nfs connect %s, so type %d port %d prot %d %d\n",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, nss.nss_sotype, nss.nss_port,
		    nss.nss_protocol, nss.nss_version);
	}

	/* Set next location to first valid location. */
	/* If start location is invalid, find next location. */
	nss.nss_nextloc = nss.nss_startloc;
	if ((nss.nss_nextloc.nli_serv >= nmp->nm_locations.nl_locations[nss.nss_nextloc.nli_loc]->nl_servcount) ||
	    (nss.nss_nextloc.nli_addr >= nmp->nm_locations.nl_locations[nss.nss_nextloc.nli_loc]->nl_servers[nss.nss_nextloc.nli_serv]->ns_addrcount)) {
		nfs_location_next(&nmp->nm_locations, &nss.nss_nextloc);
		if (!nfs_location_index_cmp(&nss.nss_nextloc, &nss.nss_startloc)) {
			NFS_SOCK_DBG("nfs connect %s search failed, couldn't find a valid location index\n",
			    vfs_statfs(nmp->nm_mountp)->f_mntfromname);
			return ENOENT;
		}
	}
	nss.nss_last = -1;

keepsearching:

	error = nfs_connect_search_loop(nmp, &nss);
	if (error || !nss.nss_sock) {
		/* search failed */
		nfs_socket_search_cleanup(&nss);
		if (nss.nss_flags & NSS_FALLBACK2PMAP) {
			tryv4 = 0;
			NFS_SOCK_DBG("nfs connect %s TCP failed for V4 %d %d, trying PORTMAP\n",
			    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error, nss.nss_error);
			goto tryagain;
		}

		if (!error && (nss.nss_sotype == SOCK_STREAM) && !nmp->nm_sotype && (nmp->nm_vers < NFS_VER4)) {
			if (nss.nss_error == EPROGUNAVAIL) {
				/* Try using UDP only when TCP is not supported */
				sotype = SOCK_DGRAM;
				savederror = nss.nss_error;
				NFS_SOCK_DBG("nfs connect %s TCP failed %d %d, trying UDP\n",
				    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error, nss.nss_error);
				goto tryagain;
			} else {
				NFS_SOCK_DBG("nfs connect %s TCP failed %d %d, aborting\n",
				    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error, nss.nss_error);
			}
		}
		if (!error) {
			error = nss.nss_error ? nss.nss_error : ETIMEDOUT;
		}
		lck_mtx_lock(&nmp->nm_lock);
		nmp->nm_sockflags &= ~NMSOCK_CONNECTING;
		nmp->nm_nss = NULL;
		lck_mtx_unlock(&nmp->nm_lock);
		if (nss.nss_flags & NSS_WARNED) {
			log(LOG_INFO, "nfs_connect: socket connect aborted for %s\n",
			    vfs_statfs(nmp->nm_mountp)->f_mntfromname);
		}
		if (fh) {
			NFS_ZFREE(get_zone(NFS_FILE_HANDLE_ZONE), fh);
		}
		if (path) {
			NFS_ZFREE(get_zone(NFS_NAMEI), path);
		}
		NFS_SOCK_DBG("nfs connect %s search failed, returning %d\n",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error);
		return error;
	}

	/* try to use nss_sock */
	nso = nss.nss_sock;
	nss.nss_sock = NULL;

	/* We may be speaking to portmap first... to determine port(s). */
	if (nso->nso_saddr->sa_family == AF_INET) {
		port = ntohs(((struct sockaddr_in*)nso->nso_saddr)->sin_port);
	} else if (nso->nso_saddr->sa_family == AF_INET6) {
		port = ntohs(((struct sockaddr_in6*)nso->nso_saddr)->sin6_port);
	} else if (nso->nso_saddr->sa_family == AF_LOCAL) {
		if (nso->nso_protocol == PMAPPROG) {
			port = PMAPPORT;
		}
	}

	if (port == PMAPPORT) {
		/* Use this portmapper port to get the port #s we need. */
		SP(nso, "nfs connect %s got portmapper socket",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname);

		/* remove the connect upcall so nfs_portmap_lookup() can use this socket */
		sock_setupcall(nso->nso_so, NULL, NULL);

		/* Set up socket address and port for NFS socket. */
		bcopy(nso->nso_saddr, &ss, nso->nso_saddr->sa_len);

		/* If NFS version not set, try nm_max_vers down to nm_min_vers */
		nfsvers = nmp->nm_vers ? nmp->nm_vers : PVER2MAJOR(nmp->nm_max_vers);
		if (!(port = nmp->nm_nfsport)) {
			if (ss.ss_family == AF_INET) {
				((struct sockaddr_in*)&ss)->sin_port = htons(0);
			} else if (ss.ss_family == AF_INET6) {
				((struct sockaddr_in6*)&ss)->sin6_port = htons(0);
			} else if (ss.ss_family == AF_LOCAL) {
				if (((struct sockaddr_un*)&ss)->sun_path[0] == '/') {
					NFS_SOCK_DBG("Looking up  NFS socket over %s\n", ((struct sockaddr_un*)&ss)->sun_path);
				}
			}
			for (; nfsvers >= (int)PVER2MAJOR(nmp->nm_min_vers); nfsvers--) {
				if (nmp->nm_vers && nmp->nm_vers != nfsvers) {
					continue; /* Wrong version */
				}
#if CONFIG_NFS4
				if (nfsvers == NFS_VER4 && nso->nso_sotype == SOCK_DGRAM) {
					continue; /* NFSv4 does not do UDP */
				}
#endif
				if (ss.ss_family == AF_LOCAL && nmp->nm_nfs_localport) {
					struct sockaddr_un *sun = (struct sockaddr_un *)&ss;
					NFS_SOCK_DBG("Using supplied local address %s for NFS_PROG\n", nmp->nm_nfs_localport);
					strlcpy(sun->sun_path, nmp->nm_nfs_localport, sizeof(sun->sun_path));
					error = 0;
				} else {
					NFS_SOCK_DBG("Calling Portmap/Rpcbind for NFS_PROG");
					error = nfs_portmap_lookup(nmp, vfs_context_current(), (struct sockaddr*)&ss,
					    nso->nso_so, NFS_PROG, nfsvers, nso->nso_sotype, timeo);
				}
				if (!error) {
					if (ss.ss_family == AF_INET) {
						port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
					} else if (ss.ss_family == AF_INET6) {
						port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
					} else if (ss.ss_family == AF_LOCAL) {
						port = ((struct sockaddr_un *)&ss)->sun_path[0] ? NFS_PORT : 0;
					}
					if (!port) {
						error = EPROGUNAVAIL;
					}
#if CONFIG_NFS4
					if (port == NFS_PORT && nfsvers == NFS_VER4 && tryv4 == 0) {
						continue; /* We already tried this */
					}
#endif
				}
				if (!error) {
					break;
				}
			}
			if (nfsvers < (int)PVER2MAJOR(nmp->nm_min_vers) && error == 0) {
				error = EPROGUNAVAIL;
			}
			if (error) {
				nfs_socket_search_update_error(&nss, error);
				nfs_socket_destroy(nso);
				NFS_SOCK_DBG("Could not lookup NFS socket address for version %d error = %d\n", nfsvers, error);
				goto keepsearching;
			}
		} else if (nmp->nm_nfs_localport) {
			strlcpy(((struct sockaddr_un*)&ss)->sun_path, nmp->nm_nfs_localport, sizeof(((struct sockaddr_un*)&ss)->sun_path));
			NFS_SOCK_DBG("Using supplied nfs_local_port %s for NFS_PROG\n", nmp->nm_nfs_localport);
		}

		/* Create NFS protocol socket and add it to the list of sockets. */
		/* N.B. If nfsvers is NFS_VER4 at this point then we're on a non standard port */
		if (ss.ss_family == AF_LOCAL) {
			NFS_SOCK_DBG("Creating NFS socket for %s port = %d\n", ((struct sockaddr_un*)&ss)->sun_path, port);
		}
		error = nfs_socket_create(nmp, (struct sockaddr*)&ss, nso->nso_sotype, port,
		    NFS_PROG, nfsvers, NMFLAG(nmp, RESVPORT), &nsonfs);
		if (error) {
			nfs_socket_search_update_error(&nss, error);
			nfs_socket_destroy(nso);
			NFS_SOCK_DBG("Could not create NFS socket: %d\n", error);
			goto keepsearching;
		}
		nsonfs->nso_location = nso->nso_location;
		nsonfs->nso_wake = &nss;
		TAILQ_INSERT_TAIL(&nss.nss_socklist, nsonfs, nso_link);
		nss.nss_sockcnt++;
		if ((nfsvers < NFS_VER4) && !(nmp->nm_sockflags & NMSOCK_HASCONNECTED) && !NM_OMATTR_GIVEN(nmp, FH)) {
			/* Set up socket address and port for MOUNT socket. */
			error = 0;
			bcopy(nso->nso_saddr, &ss, nso->nso_saddr->sa_len);
			port = nmp->nm_mountport;
			NFS_SOCK_DBG("mount port = %d\n", port);
			if (ss.ss_family == AF_INET) {
				((struct sockaddr_in*)&ss)->sin_port = htons(port);
			} else if (ss.ss_family == AF_INET6) {
				((struct sockaddr_in6*)&ss)->sin6_port = htons(port);
			} else if (ss.ss_family == AF_LOCAL && nmp->nm_mount_localport) {
				NFS_SOCK_DBG("Setting mount address to %s port = %d\n", nmp->nm_mount_localport, nmp->nm_mountport);
				strlcpy(((struct sockaddr_un*)&ss)->sun_path, nmp->nm_mount_localport, sizeof(((struct sockaddr_un*)&ss)->sun_path));
			}
			if (!port) {
				/* Get port/sockaddr for MOUNT version corresponding to NFS version. */
				/* If NFS version is unknown, optimistically choose for NFSv3. */
				int mntvers = (nfsvers == NFS_VER2) ? RPCMNT_VER1 : RPCMNT_VER3;
				int mntproto = (NM_OMFLAG(nmp, MNTUDP) || (nso->nso_sotype == SOCK_DGRAM)) ? IPPROTO_UDP : IPPROTO_TCP;
				SP(nso, "Looking up mount port with so 0x%lx", nfs_kernel_hideaddr(nso->nso_so));
				error = nfs_portmap_lookup(nmp, vfs_context_current(), (struct sockaddr*)&ss,
				    nso->nso_so, RPCPROG_MNT, mntvers, mntproto == IPPROTO_UDP ? SOCK_DGRAM : SOCK_STREAM, timeo);
			}
			if (!error) {
				if (ss.ss_family == AF_INET) {
					port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
				} else if (ss.ss_family == AF_INET6) {
					port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
				} else if (ss.ss_family == AF_LOCAL) {
					port = (((struct sockaddr_un*)&ss)->sun_path[0] != '\0');
				}
				if (!port) {
					error = EPROGUNAVAIL;
				}
			}
			/* create sockaddr for MOUNT */
			if (!error) {
				nsonfs->nso_saddr2 = kalloc_data(ss.ss_len, Z_WAITOK | Z_ZERO | Z_NOFAIL);
			}
			if (!error) {
				bcopy(&ss, nsonfs->nso_saddr2, ss.ss_len);
			}
			if (error) {
				NFS_SOCK_DBG("Could not create mount socket address %d", error);
				lck_mtx_lock(&nsonfs->nso_lock);
				nsonfs->nso_error = error;
				nsonfs->nso_flags |= NSO_DEAD;
				lck_mtx_unlock(&nsonfs->nso_lock);
			}
		}
		SP(nso, "Destroying socket so 0x%lx", nfs_kernel_hideaddr(nso->nso_so));
		nfs_socket_destroy(nso);
		goto keepsearching;
	}

	/* nso is an NFS socket */
	SP(nso, "nfs connect %s got NFS socket", vfs_statfs(nmp->nm_mountp)->f_mntfromname);

	/* If NFS version wasn't specified, it was determined during the connect. */
	nfsvers = nmp->nm_vers ? nmp->nm_vers : (int)nso->nso_version;

	/* Perform MOUNT call for initial NFSv2/v3 connection/mount. */
	if ((nfsvers < NFS_VER4) && !(nmp->nm_sockflags & NMSOCK_HASCONNECTED) && !NM_OMATTR_GIVEN(nmp, FH)) {
		error = 0;
		saddr = nso->nso_saddr2;
		if (!saddr) {
			/* Need sockaddr for MOUNT port */
			NFS_SOCK_DBG("Getting mount address mountport = %d, mount_localport = %s\n", nmp->nm_mountport, nmp->nm_mount_localport);
			bcopy(nso->nso_saddr, &ss, nso->nso_saddr->sa_len);
			port = nmp->nm_mountport;
			if (ss.ss_family == AF_INET) {
				((struct sockaddr_in*)&ss)->sin_port = htons(port);
			} else if (ss.ss_family == AF_INET6) {
				((struct sockaddr_in6*)&ss)->sin6_port = htons(port);
			} else if (ss.ss_family == AF_LOCAL && nmp->nm_mount_localport) {
				NFS_SOCK_DBG("Setting mount address to %s port = %d\n", nmp->nm_mount_localport, nmp->nm_mountport);
				strlcpy(((struct sockaddr_un*)&ss)->sun_path, nmp->nm_mount_localport, sizeof(((struct sockaddr_un*)&ss)->sun_path));
			}
			if (!port) {
				/* Get port/sockaddr for MOUNT version corresponding to NFS version. */
				int mntvers = (nfsvers == NFS_VER2) ? RPCMNT_VER1 : RPCMNT_VER3;
				int so_type = NM_OMFLAG(nmp, MNTUDP) ? SOCK_DGRAM : nso->nso_sotype;
				error = nfs_portmap_lookup(nmp, vfs_context_current(), (struct sockaddr*)&ss,
				    NULL, RPCPROG_MNT, mntvers, so_type, timeo);
				if (ss.ss_family == AF_INET) {
					port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
				} else if (ss.ss_family == AF_INET6) {
					port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
				}
			}
			if (!error) {
				if (port) {
					saddr = (struct sockaddr*)&ss;
				} else {
					error = EPROGUNAVAIL;
				}
			}
		}
		if (!error) {
			error = nfs3_check_lockmode(nmp, saddr, nso->nso_sotype, timeo);
			if (error) {
				nfs_socket_search_update_error(&nss, error);
				nfs_socket_destroy(nso);
				return error;
			}
		}
		if (saddr) {
			fh = zalloc(get_zone(NFS_FILE_HANDLE_ZONE));
		}
		if (saddr && fh) {
			path = zalloc(get_zone(NFS_NAMEI));
		}
		if (!saddr || !fh || !path) {
			if (!error) {
				error = ENOMEM;
			}
			if (fh) {
				NFS_ZFREE(get_zone(NFS_FILE_HANDLE_ZONE), fh);
			}
			if (path) {
				NFS_ZFREE(get_zone(NFS_NAMEI), path);
			}
			nfs_socket_search_update_error(&nss, error);
			nfs_socket_destroy(nso);
			goto keepsearching;
		}
		nfs_location_mntfromname(&nmp->nm_locations, nso->nso_location, path, MAXPATHLEN, 1);
		error = nfs3_mount_rpc(nmp, saddr, nso->nso_sotype, nfsvers,
		    path, vfs_context_current(), timeo, fh, &nmp->nm_servsec);
		SP(nso, "nfs connect %s socket mount %d",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error);
		if (!error) {
			/* Make sure we can agree on a security flavor. */
			int o, s;  /* indices into mount option and server security flavor lists */
			int found = 0;

			if ((nfsvers == NFS_VER3) && !nmp->nm_servsec.count) {
				/* Some servers return an empty list to indicate RPCAUTH_SYS? */
				nmp->nm_servsec.count = 1;
				nmp->nm_servsec.flavors[0] = RPCAUTH_SYS;
			}
			if (nmp->nm_sec.count) {
				/* Choose the first flavor in our list that the server supports. */
				if (!nmp->nm_servsec.count) {
					/* we don't know what the server supports, just use our first choice */
					nmp->nm_auth = nmp->nm_sec.flavors[0];
					found = 1;
				}
				for (o = 0; !found && (o < nmp->nm_sec.count); o++) {
					for (s = 0; !found && (s < nmp->nm_servsec.count); s++) {
						if (nmp->nm_sec.flavors[o] == nmp->nm_servsec.flavors[s]) {
							nmp->nm_auth = nmp->nm_sec.flavors[o];
							found = 1;
						}
					}
				}
			} else {
				/* Choose the first one we support from the server's list. */
				if (!nmp->nm_servsec.count) {
					nmp->nm_auth = RPCAUTH_SYS;
					found = 1;
				}
				for (s = 0; s < nmp->nm_servsec.count; s++) {
					switch (nmp->nm_servsec.flavors[s]) {
					case RPCAUTH_SYS:
						/* prefer RPCAUTH_SYS to RPCAUTH_NONE */
						if (found && (nmp->nm_auth == RPCAUTH_NONE)) {
							found = 0;
						}
						OS_FALLTHROUGH;
					case RPCAUTH_NONE:
					case RPCAUTH_KRB5:
					case RPCAUTH_KRB5I:
					case RPCAUTH_KRB5P:
						if (!found) {
							nmp->nm_auth = nmp->nm_servsec.flavors[s];
							found = 1;
						}
						break;
					}
				}
			}
			error = !found ? EAUTH : 0;
		}
		NFS_ZFREE(get_zone(NFS_NAMEI), path);
		if (error) {
			nfs_socket_search_update_error(&nss, error);
			NFS_ZFREE(get_zone(NFS_FILE_HANDLE_ZONE), fh);
			nfs_socket_destroy(nso);
			goto keepsearching;
		}
		if (nmp->nm_fh) {
			NFS_ZFREE(get_zone(NFS_FILE_HANDLE_ZONE), nmp->nm_fh);
		}
		nmp->nm_fh = fh;
		fh = NULL;
		NFS_BITMAP_SET(nmp->nm_flags, NFS_MFLAG_CALLUMNT);
	}

	/* put the real upcall in place */
	upcall = (nso->nso_sotype == SOCK_STREAM) ? nfs_tcp_rcv : nfs_udp_rcv;
	error = sock_setupcall(nso->nso_so, upcall, nmp);
	if (error) {
		nfs_socket_search_update_error(&nss, error);
		nfs_socket_destroy(nso);
		goto keepsearching;
	}

	if (!(nmp->nm_sockflags & NMSOCK_HASCONNECTED)) {
		/* set mntfromname to this location */
		if (!NM_OMATTR_GIVEN(nmp, MNTFROM)) {
			nfs_location_mntfromname(&nmp->nm_locations, nso->nso_location,
			    vfs_statfs(nmp->nm_mountp)->f_mntfromname,
			    sizeof(vfs_statfs(nmp->nm_mountp)->f_mntfromname), 0);
		}
		/* some negotiated values need to remain unchanged for the life of the mount */
		if (!nmp->nm_sotype) {
			nmp->nm_sotype = nso->nso_sotype;
		}
		if (!nmp->nm_vers) {
			nmp->nm_vers = nfsvers;
#if CONFIG_NFS4
			/* If we negotiated NFSv4, set nm_nfsport if we ended up on the standard NFS port */
			if ((nfsvers >= NFS_VER4) && !NFS_BITMAP_ISSET(nmp->nm_mattrs, NFS_MATTR_NFS_PORT)) {
				if (nso->nso_saddr->sa_family == AF_INET) {
					port = ((struct sockaddr_in*)nso->nso_saddr)->sin_port = htons(port);
				} else if (nso->nso_saddr->sa_family == AF_INET6) {
					port = ((struct sockaddr_in6*)nso->nso_saddr)->sin6_port = htons(port);
				} else {
					port = 0;
				}
				if (port == NFS_PORT) {
					nmp->nm_nfsport = NFS_PORT;
				}
			}
#endif
		}
#if CONFIG_NFS4
		/* do some version-specific pre-mount set up */
		if (nmp->nm_vers >= NFS_VER4) {
			microtime(&now);
			nmp->nm_mounttime = ((uint64_t)now.tv_sec << 32) | now.tv_usec;
			if (!NMFLAG(nmp, NOCALLBACK)) {
				nfs4_mount_callback_setup(nmp);
			}
		}
#endif
	}

	/* Initialize NFS socket state variables */
	lck_mtx_lock(&nmp->nm_lock);
	nmp->nm_srtt[0] = nmp->nm_srtt[1] = nmp->nm_srtt[2] =
	    nmp->nm_srtt[3] = (NFS_TIMEO << 3);
	nmp->nm_sdrtt[0] = nmp->nm_sdrtt[1] = nmp->nm_sdrtt[2] =
	    nmp->nm_sdrtt[3] = 0;
	if (nso->nso_sotype == SOCK_DGRAM) {
		nmp->nm_cwnd = NFS_MAXCWND / 2;     /* Initial send window */
		nmp->nm_sent = 0;
	} else if (nso->nso_sotype == SOCK_STREAM) {
		nmp->nm_timeouts = 0;
	}
	nmp->nm_sockflags &= ~NMSOCK_CONNECTING;
	nmp->nm_sockflags |= NMSOCK_SETUP;
	/* move the socket to the mount structure */
	nmp->nm_nso = nso;
	oldsaddr = nmp->nm_saddr;
	nmp->nm_saddr = nso->nso_saddr;
	lck_mtx_unlock(&nmp->nm_lock);
	error = nfs_connect_setup(nmp);
	lck_mtx_lock(&nmp->nm_lock);
	nmp->nm_sockflags &= ~NMSOCK_SETUP;
	if (!error) {
		nmp->nm_sockflags |= NMSOCK_READY;
		wakeup(&nmp->nm_sockflags);
	}
	if (error) {
		SP(nso, "nfs connect %s socket setup failed %d",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error);
		nfs_socket_search_update_error(&nss, error);
		nmp->nm_saddr = oldsaddr;
		if (!(nmp->nm_sockflags & NMSOCK_HASCONNECTED)) {
			/* undo settings made prior to setup */
			if (!NFS_BITMAP_ISSET(nmp->nm_mattrs, NFS_MATTR_SOCKET_TYPE)) {
				nmp->nm_sotype = 0;
			}
#if CONFIG_NFS4
			if (nmp->nm_vers >= NFS_VER4) {
				if (!NFS_BITMAP_ISSET(nmp->nm_mattrs, NFS_MATTR_NFS_PORT)) {
					nmp->nm_nfsport = 0;
				}
				if (nmp->nm_cbid) {
					nfs4_mount_callback_shutdown(nmp);
				}
				if (IS_VALID_CRED(nmp->nm_mcred)) {
					kauth_cred_unref(&nmp->nm_mcred);
				}
				bzero(&nmp->nm_un, sizeof(nmp->nm_un));
			}
#endif
			nmp->nm_vers = 0;
		}
		lck_mtx_unlock(&nmp->nm_lock);
		nmp->nm_nso = NULL;
		nfs_socket_destroy(nso);
		goto keepsearching;
	}

	/* update current location */
	if ((nmp->nm_locations.nl_current.nli_flags & NLI_VALID) &&
	    (nmp->nm_locations.nl_current.nli_serv != nso->nso_location.nli_serv)) {
		/* server has changed, we should initiate failover/recovery */
		// XXX
	}
	nmp->nm_locations.nl_current = nso->nso_location;
	nmp->nm_locations.nl_current.nli_flags |= NLI_VALID;

	if (!(nmp->nm_sockflags & NMSOCK_HASCONNECTED)) {
		/* We have now successfully connected... make a note of it. */
		nmp->nm_sockflags |= NMSOCK_HASCONNECTED;
	}

	lck_mtx_unlock(&nmp->nm_lock);

	if (oldsaddr) {
		kfree_data(oldsaddr, oldsaddr->sa_len);
	}

	if (nss.nss_flags & NSS_WARNED) {
		log(LOG_INFO, "nfs_connect: socket connect completed for %s\n",
		    vfs_statfs(nmp->nm_mountp)->f_mntfromname);
	}

	nmp->nm_nss = NULL;
	nfs_socket_search_cleanup(&nss);
	if (fh) {
		NFS_ZFREE(get_zone(NFS_FILE_HANDLE_ZONE), fh);
	}
	if (path) {
		NFS_ZFREE(get_zone(NFS_NAMEI), path);
	}
	NFS_SOCK_DBG("nfs connect %s success\n", vfs_statfs(nmp->nm_mountp)->f_mntfromname);
	return 0;
}


/* setup & confirm socket connection is functional */
int
nfs_connect_setup(
#if !CONFIG_NFS4
	__unused
#endif
	struct nfsmount *nmp)
{
	int error = 0;
#if CONFIG_NFS4
	if (nmp->nm_vers >= NFS_VER4) {
		if (nmp->nm_state & NFSSTA_CLIENTID) {
			/* first, try to renew our current state */
			error = nfs4_renew(nmp, R_SETUP);
			if ((error == NFSERR_ADMIN_REVOKED) ||
			    (error == NFSERR_CB_PATH_DOWN) ||
			    (error == NFSERR_EXPIRED) ||
			    (error == NFSERR_LEASE_MOVED) ||
			    (error == NFSERR_STALE_CLIENTID)) {
				lck_mtx_lock(&nmp->nm_lock);
				nfs_need_recover(nmp, error);
				lck_mtx_unlock(&nmp->nm_lock);
			}
		}
		error = nfs4_setclientid(nmp, 0);
	}
#endif
	return error;
}

/*
 * NFS socket reconnect routine:
 * Called when a connection is broken.
 * - disconnect the old socket
 * - nfs_connect() again
 * - set R_MUSTRESEND for all outstanding requests on mount point
 * If this fails the mount point is DEAD!
 */
int
nfs_reconnect(struct nfsmount *nmp)
{
	struct nfsreq *rq;
	struct timeval now;
	thread_t thd = current_thread();
	int error, wentdown = 0, verbose = 1;
	time_t lastmsg;
	int timeo;

	microuptime(&now);
	lastmsg = now.tv_sec - (nmp->nm_tprintf_delay - nmp->nm_tprintf_initial_delay);

	nfs_disconnect(nmp);


	lck_mtx_lock(&nmp->nm_lock);
	timeo = nfs_is_squishy(nmp) ? 8 : 30;
	lck_mtx_unlock(&nmp->nm_lock);

	while ((error = nfs_connect(nmp, verbose, timeo))) {
		verbose = 0;
		nfs_disconnect(nmp);
		if ((error == EINTR) || (error == ERESTART)) {
			return EINTR;
		}
		if (error == EIO) {
			return EIO;
		}
		microuptime(&now);
		if ((lastmsg + nmp->nm_tprintf_delay) < now.tv_sec) {
			lastmsg = now.tv_sec;
			nfs_down(nmp, thd, error, NFSSTA_TIMEO, "can not connect", 0);
			wentdown = 1;
		}
		lck_mtx_lock(&nmp->nm_lock);
		if (!(nmp->nm_state & NFSSTA_MOUNTED)) {
			/* we're not yet completely mounted and */
			/* we can't reconnect, so we fail */
			lck_mtx_unlock(&nmp->nm_lock);
			NFS_SOCK_DBG("Not mounted returning %d\n", error);
			return error;
		}

		if (nfs_mount_check_dead_timeout(nmp)) {
			nfs_mount_make_zombie(nmp);
			lck_mtx_unlock(&nmp->nm_lock);
			return ENXIO;
		}

		if ((error = nfs_sigintr(nmp, NULL, thd, 1))) {
			lck_mtx_unlock(&nmp->nm_lock);
			return error;
		}
		lck_mtx_unlock(&nmp->nm_lock);
		tsleep(nfs_reconnect, PSOCK, "nfs_reconnect_delay", 2 * hz);
		if ((error = nfs_sigintr(nmp, NULL, thd, 0))) {
			return error;
		}
	}

	if (wentdown) {
		nfs_up(nmp, thd, NFSSTA_TIMEO, "connected");
	}

	/*
	 * Loop through outstanding request list and mark all requests
	 * as needing a resend.  (Though nfs_need_reconnect() probably
	 * marked them all already.)
	 */
	lck_mtx_lock(get_lck_mtx(NLM_REQUEST));
	TAILQ_FOREACH(rq, &nfs_reqq, r_chain) {
		if (rq->r_nmp == nmp) {
			lck_mtx_lock(&rq->r_mtx);
			if (!rq->r_error && !rq->r_nmrep.nmc_mhead && !(rq->r_flags & R_MUSTRESEND)) {
				rq->r_flags |= R_MUSTRESEND;
				rq->r_rtt = -1;
				wakeup(rq);
				if ((rq->r_flags & (R_IOD | R_ASYNC | R_ASYNCWAIT | R_SENDING)) == R_ASYNC) {
					nfs_asyncio_resend(rq);
				}
			}
			lck_mtx_unlock(&rq->r_mtx);
		}
	}
	lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));
	return 0;
}

/*
 * NFS disconnect. Clean up and unlink.
 */
void
nfs_disconnect(struct nfsmount *nmp)
{
	struct nfs_socket *nso;

	lck_mtx_lock(&nmp->nm_lock);
tryagain:
	if (nmp->nm_nso) {
		struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
		lck_mtx_unlock(&nmp->nm_lock);
		lck_mtx_lock(&nmp->nm_sndstate_lock);
		if (nmp->nm_sndstate & NFSSENDSTA_SENDING) { /* wait for sending to complete */
			nmp->nm_sndstate |= NFSSENDSTA_WANTSND;
			msleep(&nmp->nm_sndstate, &nmp->nm_sndstate_lock, PDROP | (PZERO - 1), "nfswaitsending", &ts);;
			lck_mtx_lock(&nmp->nm_lock);
			goto tryagain;
		}
		lck_mtx_unlock(&nmp->nm_sndstate_lock);
		lck_mtx_lock(&nmp->nm_lock);
		if (nmp->nm_sockflags & NMSOCK_POKE) { /* wait for poking to complete */
			msleep(&nmp->nm_sockflags, &nmp->nm_lock, PZERO - 1, "nfswaitpoke", &ts);
			goto tryagain;
		}
		nmp->nm_sockflags |= NMSOCK_DISCONNECTING;
		nmp->nm_sockflags &= ~NMSOCK_READY;
		nso = nmp->nm_nso;
		nmp->nm_nso = NULL;
		if (nso->nso_saddr == nmp->nm_saddr) {
			nso->nso_saddr = NULL;
		}
		lck_mtx_unlock(&nmp->nm_lock);
		nfs_socket_destroy(nso);
		lck_mtx_lock(&nmp->nm_lock);
		nmp->nm_sockflags &= ~NMSOCK_DISCONNECTING;
		lck_mtx_unlock(&nmp->nm_lock);
	} else {
		lck_mtx_unlock(&nmp->nm_lock);
	}
}

/*
 * mark an NFS mount as needing a reconnect/resends.
 */
void
nfs_need_reconnect(struct nfsmount *nmp)
{
	struct nfsreq *rq;

	lck_mtx_lock(&nmp->nm_lock);
	nmp->nm_sockflags &= ~(NMSOCK_READY | NMSOCK_SETUP);
	lck_mtx_unlock(&nmp->nm_lock);

	/*
	 * Loop through outstanding request list and
	 * mark all requests as needing a resend.
	 */
	lck_mtx_lock(get_lck_mtx(NLM_REQUEST));
	TAILQ_FOREACH(rq, &nfs_reqq, r_chain) {
		if (rq->r_nmp == nmp) {
			lck_mtx_lock(&rq->r_mtx);
			if (!rq->r_error && !rq->r_nmrep.nmc_mhead && !(rq->r_flags & R_MUSTRESEND)) {
				rq->r_flags |= R_MUSTRESEND;
				rq->r_rtt = -1;
				wakeup(rq);
				if ((rq->r_flags & (R_IOD | R_ASYNC | R_ASYNCWAIT | R_SENDING)) == R_ASYNC) {
					nfs_asyncio_resend(rq);
				}
			}
			lck_mtx_unlock(&rq->r_mtx);
		}
	}
	lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));
}


/*
 * thread to handle miscellaneous async NFS socket work (reconnects/resends)
 */
void
nfs_mount_sock_thread(void *arg, __unused wait_result_t wr)
{
	struct nfsmount *nmp = arg;
	struct timespec ts = { .tv_sec = 30, .tv_nsec = 0 };
	thread_t thd = current_thread();
	struct nfsreq *req;
	struct timeval now;
	int error, dofinish;
	nfsnode_t np;
	int do_reconnect_sleep = 0;

	lck_mtx_lock(&nmp->nm_lock);
	while (!(nmp->nm_sockflags & NMSOCK_READY) ||
	    !TAILQ_EMPTY(&nmp->nm_resendq) ||
	    !LIST_EMPTY(&nmp->nm_monlist) ||
	    nmp->nm_deadto_start ||
	    (nmp->nm_state & NFSSTA_RECOVER) ||
	    ((nmp->nm_vers >= NFS_VER4) && !TAILQ_EMPTY(&nmp->nm_dreturnq))) {
		if (nmp->nm_sockflags & NMSOCK_UNMOUNT) {
			break;
		}
		/* do reconnect, if necessary */
		if (!(nmp->nm_sockflags & NMSOCK_READY) && !(nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD))) {
			if (nmp->nm_reconnect_start <= 0) {
				microuptime(&now);
				nmp->nm_reconnect_start = now.tv_sec;
			}
			lck_mtx_unlock(&nmp->nm_lock);
			NFS_SOCK_DBG("nfs reconnect %s\n", vfs_statfs(nmp->nm_mountp)->f_mntfromname);
			/*
			 * XXX We don't want to call reconnect again right away if returned errors
			 * before that may not have blocked. This has caused spamming null procs
			 * from machines in the pass.
			 */
			if (do_reconnect_sleep) {
				tsleep(nfs_mount_sock_thread, PSOCK, "nfs_reconnect_sock_thread_delay", hz);
			}
			error = nfs_reconnect(nmp);
			if (error) {
				int lvl = 7;
				if (error == EIO || error == EINTR) {
					lvl = (do_reconnect_sleep++ % 600) ? 7 : 0;
				}
				NFSCLNT_DBG(NFSCLNT_FAC_SOCK, lvl, "nfs reconnect %s: returned %d\n",
				    vfs_statfs(nmp->nm_mountp)->f_mntfromname, error);
			} else {
				nmp->nm_reconnect_start = 0;
				do_reconnect_sleep = 0;
			}
			lck_mtx_lock(&nmp->nm_lock);
		}
		if ((nmp->nm_sockflags & NMSOCK_READY) &&
		    (nmp->nm_state & NFSSTA_RECOVER) &&
		    !(nmp->nm_sockflags & NMSOCK_UNMOUNT) &&
		    !(nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD))) {
			/* perform state recovery */
			lck_mtx_unlock(&nmp->nm_lock);
			nfs_recover(nmp);
			lck_mtx_lock(&nmp->nm_lock);
		}
#if CONFIG_NFS4
		/* handle NFSv4 delegation returns */
		while ((nmp->nm_vers >= NFS_VER4) && !(nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD)) &&
		    (nmp->nm_sockflags & NMSOCK_READY) && !(nmp->nm_state & NFSSTA_RECOVER) &&
		    ((np = TAILQ_FIRST(&nmp->nm_dreturnq)))) {
			lck_mtx_unlock(&nmp->nm_lock);
			nfs4_delegation_return(np, R_RECOVER, thd, nmp->nm_mcred);
			lck_mtx_lock(&nmp->nm_lock);
		}
#endif
		/* do resends, if necessary/possible */
		while ((((nmp->nm_sockflags & NMSOCK_READY) && !(nmp->nm_state & NFSSTA_RECOVER)) ||
		    (nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD))) &&
		    ((req = TAILQ_FIRST(&nmp->nm_resendq)))) {
			if (req->r_resendtime) {
				microuptime(&now);
			}
			while (req && !(nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD)) && req->r_resendtime && (now.tv_sec < req->r_resendtime)) {
				req = TAILQ_NEXT(req, r_rchain);
			}
			if (!req) {
				break;
			}
			/* acquire both locks in the right order: first req->r_mtx and then nmp->nm_lock */
			lck_mtx_unlock(&nmp->nm_lock);
			lck_mtx_lock(&req->r_mtx);
			lck_mtx_lock(&nmp->nm_lock);
			if ((req->r_flags & R_RESENDQ) == 0 || (req->r_rchain.tqe_next == NFSREQNOLIST)) {
				lck_mtx_unlock(&req->r_mtx);
				continue;
			}
			TAILQ_REMOVE(&nmp->nm_resendq, req, r_rchain);
			req->r_flags &= ~R_RESENDQ;
			req->r_rchain.tqe_next = NFSREQNOLIST;
			lck_mtx_unlock(&nmp->nm_lock);
			/* Note that we have a reference on the request that was taken nfs_asyncio_resend */
			if (req->r_error || req->r_nmrep.nmc_mhead) {
				dofinish = req->r_callback.rcb_func && !(req->r_flags & R_WAITSENT);
				wakeup(req);
				lck_mtx_unlock(&req->r_mtx);
				if (dofinish) {
					nfs_asyncio_finish(req);
				}
				nfs_request_rele(req);
				lck_mtx_lock(&nmp->nm_lock);
				continue;
			}
			if ((req->r_flags & R_RESTART) || nfs_request_using_gss(req)) {
				req->r_flags &= ~R_RESTART;
				req->r_resendtime = 0;
				lck_mtx_unlock(&req->r_mtx);
				/* async RPCs on GSS mounts need to be rebuilt and resent. */
				nfs_reqdequeue(req);
#if CONFIG_NFS_GSS
				if (nfs_request_using_gss(req)) {
					nfs_gss_clnt_rpcdone(req);
					error = nfs_gss_clnt_args_restore(req);
					if (error == ENEEDAUTH) {
						req->r_xid = 0;
					}
				}
#endif /* CONFIG_NFS_GSS */
				NFS_SOCK_DBG("nfs async%s restart: p %d x 0x%llx f 0x%x rtt %d\n",
				    nfs_request_using_gss(req) ? " gss" : "", req->r_procnum, req->r_xid,
				    req->r_flags, req->r_rtt);
				error = nfs_sigintr(nmp, req, req->r_thread, 0);
				if (!error) {
					error = nfs_request_add_header(req);
				}
				if (!error) {
					error = nfs_request_send(req, 0);
				}
				lck_mtx_lock(&req->r_mtx);
				if (error) {
					req->r_error = error;
				}
				wakeup(req);
				dofinish = error && req->r_callback.rcb_func && !(req->r_flags & R_WAITSENT);
				lck_mtx_unlock(&req->r_mtx);
				if (dofinish) {
					nfs_asyncio_finish(req);
				}
				nfs_request_rele(req);
				lck_mtx_lock(&nmp->nm_lock);
				error = 0;
				continue;
			}
			NFS_SOCK_DBG("nfs async resend: p %d x 0x%llx f 0x%x rtt %d\n",
			    req->r_procnum, req->r_xid, req->r_flags, req->r_rtt);
			error = nfs_sigintr(nmp, req, req->r_thread, 0);
			if (!error) {
				req->r_flags |= R_SENDING;
				lck_mtx_unlock(&req->r_mtx);
				error = nfs_send(req, 0);
				lck_mtx_lock(&req->r_mtx);
				if (!error) {
					wakeup(req);
					lck_mtx_unlock(&req->r_mtx);
					nfs_request_rele(req);
					lck_mtx_lock(&nmp->nm_lock);
					continue;
				}
			}
			req->r_error = error;
			wakeup(req);
			dofinish = req->r_callback.rcb_func && !(req->r_flags & R_WAITSENT);
			lck_mtx_unlock(&req->r_mtx);
			if (dofinish) {
				nfs_asyncio_finish(req);
			}
			nfs_request_rele(req);
			lck_mtx_lock(&nmp->nm_lock);
		}
		if (nfs_mount_check_dead_timeout(nmp)) {
			nfs_mount_make_zombie(nmp);
			break;
		}

		if (nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD)) {
			break;
		}
		/* check monitored nodes, if necessary/possible */
		if (!LIST_EMPTY(&nmp->nm_monlist)) {
			nmp->nm_state |= NFSSTA_MONITOR_SCAN;
			LIST_FOREACH(np, &nmp->nm_monlist, n_monlink) {
				if (!(nmp->nm_sockflags & NMSOCK_READY) ||
				    (nmp->nm_state & (NFSSTA_RECOVER | NFSSTA_UNMOUNTING | NFSSTA_FORCE | NFSSTA_DEAD))) {
					break;
				}
				np->n_mflag |= NMMONSCANINPROG;
				lck_mtx_unlock(&nmp->nm_lock);
				error = nfs_getattr(np, NULL, vfs_context_kernel(), (NGA_UNCACHED | NGA_MONITOR));
				if (!error && ISSET(np->n_flag, NUPDATESIZE)) { /* update quickly to avoid multiple events */
					nfs_data_update_size(np, 0);
				}
				lck_mtx_lock(&nmp->nm_lock);
				np->n_mflag &= ~NMMONSCANINPROG;
				if (np->n_mflag & NMMONSCANWANT) {
					np->n_mflag &= ~NMMONSCANWANT;
					wakeup(&np->n_mflag);
				}
				if (error || !(nmp->nm_sockflags & NMSOCK_READY) ||
				    (nmp->nm_state & (NFSSTA_RECOVER | NFSSTA_UNMOUNTING | NFSSTA_FORCE | NFSSTA_DEAD))) {
					break;
				}
			}
			nmp->nm_state &= ~NFSSTA_MONITOR_SCAN;
			if (nmp->nm_state & NFSSTA_UNMOUNTING) {
				wakeup(&nmp->nm_state); /* let unmounting thread know scan is done */
			}
		}
		if ((nmp->nm_sockflags & NMSOCK_READY) || (nmp->nm_state & (NFSSTA_RECOVER | NFSSTA_UNMOUNTING))) {
			if (nmp->nm_deadto_start || !TAILQ_EMPTY(&nmp->nm_resendq) ||
			    (nmp->nm_state & NFSSTA_RECOVER)) {
				ts.tv_sec = 1;
			} else {
				ts.tv_sec = 5;
			}
			msleep(&nmp->nm_sockthd, &nmp->nm_lock, PSOCK, "nfssockthread", &ts);
		}
	}

	/* If we're unmounting, send the unmount RPC, if requested/appropriate. */
	if ((nmp->nm_sockflags & NMSOCK_UNMOUNT) &&
	    (nmp->nm_state & NFSSTA_MOUNTED) && NMFLAG(nmp, CALLUMNT) &&
	    (nmp->nm_vers < NFS_VER4) && !(nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD))) {
		lck_mtx_unlock(&nmp->nm_lock);
		nfs3_umount_rpc(nmp, vfs_context_kernel(),
		    (nmp->nm_sockflags & NMSOCK_READY) ? 6 : 2);
		lck_mtx_lock(&nmp->nm_lock);
	}

	if (nmp->nm_sockthd == thd) {
		nmp->nm_sockthd = NULL;
	}
	lck_mtx_unlock(&nmp->nm_lock);
	wakeup(&nmp->nm_sockthd);
	thread_terminate(thd);
}

/* start or wake a mount's socket thread */
void
nfs_mount_sock_thread_wake(struct nfsmount *nmp)
{
	if (nmp->nm_sockthd) {
		wakeup(&nmp->nm_sockthd);
	} else if (kernel_thread_start(nfs_mount_sock_thread, nmp, &nmp->nm_sockthd) == KERN_SUCCESS) {
		thread_deallocate(nmp->nm_sockthd);
	}
}

/*
 * Check if we should mark the mount dead because the
 * unresponsive mount has reached the dead timeout.
 * (must be called with nmp locked)
 */
int
nfs_mount_check_dead_timeout(struct nfsmount *nmp)
{
	struct timeval now;

	if (nmp->nm_state & NFSSTA_DEAD) {
		return 1;
	}
	if (nmp->nm_deadto_start == 0) {
		return 0;
	}
	nfs_is_squishy(nmp);
	if (nmp->nm_curdeadtimeout <= 0) {
		return 0;
	}
	microuptime(&now);
	if ((now.tv_sec - nmp->nm_deadto_start) < nmp->nm_curdeadtimeout) {
		return 0;
	}
	return 1;
}

/*
 * Call nfs_mount_zombie to remove most of the
 * nfs state for the mount, and then ask to be forcibly unmounted.
 *
 * Assumes the nfs mount structure lock nm_lock is held.
 */

void
nfs_mount_make_zombie(struct nfsmount *nmp)
{
	fsid_t fsid;

	if (!nmp) {
		return;
	}

	if (nmp->nm_state & NFSSTA_DEAD) {
		return;
	}

	printf("nfs server %s: %sdead\n", vfs_statfs(nmp->nm_mountp)->f_mntfromname,
	    (nmp->nm_curdeadtimeout != nmp->nm_deadtimeout) ? "squished " : "");
	fsid = vfs_statfs(nmp->nm_mountp)->f_fsid;
	lck_mtx_unlock(&nmp->nm_lock);
	nfs_mount_zombie(nmp, NFSSTA_DEAD);
	vfs_event_signal(&fsid, VQ_DEAD, 0);
	lck_mtx_lock(&nmp->nm_lock);
}


/*
 * NFS callback channel socket state
 */
struct nfs_callback_socket {
	TAILQ_ENTRY(nfs_callback_socket) ncbs_link;
	socket_t                        ncbs_so;        /* the socket */
	struct sockaddr_storage         ncbs_saddr;     /* socket address */
	struct nfs_rpc_record_state     ncbs_rrs;       /* RPC record parsing state */
	time_t                          ncbs_stamp;     /* last accessed at */
	uint32_t                        ncbs_flags;     /* see below */
};
#define NCBSOCK_UPCALL          0x0001
#define NCBSOCK_UPCALLWANT      0x0002
#define NCBSOCK_DEAD            0x0004

#if CONFIG_NFS4
/*
 * NFS callback channel state
 *
 * One listening socket for accepting socket connections from servers and
 * a list of connected sockets to handle callback requests on.
 * Mounts registered with the callback channel are assigned IDs and
 * put on a list so that the callback request handling code can match
 * the requests up with mounts.
 */
socket_t nfs4_cb_so = NULL;
socket_t nfs4_cb_so6 = NULL;
in_port_t nfs4_cb_port = 0;
in_port_t nfs4_cb_port6 = 0;
uint32_t nfs4_cb_id = 0;
uint32_t nfs4_cb_so_usecount = 0;
TAILQ_HEAD(nfs4_cb_sock_list, nfs_callback_socket) nfs4_cb_socks;
TAILQ_HEAD(nfs4_cb_mount_list, nfsmount) nfs4_cb_mounts;

int nfs4_cb_handler(struct nfs_callback_socket *, mbuf_t);

/*
 * Set up the callback channel for the NFS mount.
 *
 * Initializes the callback channel socket state and
 * assigns a callback ID to the mount.
 */
void
nfs4_mount_callback_setup(struct nfsmount *nmp)
{
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	socket_t so = NULL;
	socket_t so6 = NULL;
	struct timeval timeo;
	int error, on = 1;
	in_port_t port;

	lck_mtx_lock(get_lck_mtx(NLM_GLOBAL));
	if (nfs4_cb_id == 0) {
		TAILQ_INIT(&nfs4_cb_mounts);
		TAILQ_INIT(&nfs4_cb_socks);
		nfs4_cb_id++;
	}
	nmp->nm_cbid = nfs4_cb_id++;
	if (nmp->nm_cbid == 0) {
		nmp->nm_cbid = nfs4_cb_id++;
	}
	nfs4_cb_so_usecount++;
	TAILQ_INSERT_HEAD(&nfs4_cb_mounts, nmp, nm_cblink);

	if (nfs4_cb_so) {
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
		return;
	}

	/* IPv4 */
	error = sock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nfs4_cb_accept, NULL, &nfs4_cb_so);
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d creating listening IPv4 socket\n", error);
		goto fail;
	}
	so = nfs4_cb_so;

	if (NFS_PORT_INVALID(nfs_callback_port)) {
		error = EINVAL;
		log(LOG_INFO, "nfs callback setup: error %d nfs_callback_port %d is not valid\n", error, nfs_callback_port);
		goto fail;
	}

	sock_setsockopt(so, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	sin.sin_len = sizeof(struct sockaddr_in);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = htons((in_port_t)nfs_callback_port); /* try to use specified port */
	error = sock_bind(so, (struct sockaddr *)&sin);
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d binding listening IPv4 socket\n", error);
		goto fail;
	}
	error = sock_getsockname(so, (struct sockaddr *)&sin, sin.sin_len);
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d getting listening IPv4 socket port\n", error);
		goto fail;
	}
	nfs4_cb_port = ntohs(sin.sin_port);

	error = sock_listen(so, 32);
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d on IPv4 listen\n", error);
		goto fail;
	}

	/* receive timeout shouldn't matter.  If timeout on send, we'll want to drop the socket */
	timeo.tv_usec = 0;
	timeo.tv_sec = 60;
	error = sock_setsockopt(so, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d setting IPv4 socket rx timeout\n", error);
	}
	error = sock_setsockopt(so, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d setting IPv4 socket tx timeout\n", error);
	}
	sock_setsockopt(so, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
	sock_setsockopt(so, SOL_SOCKET, SO_NOADDRERR, &on, sizeof(on));
	sock_setsockopt(so, SOL_SOCKET, SO_UPCALLCLOSEWAIT, &on, sizeof(on));
	error = 0;

	/* IPv6 */
	error = sock_socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP, nfs4_cb_accept, NULL, &nfs4_cb_so6);
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d creating listening IPv6 socket\n", error);
		goto fail;
	}
	so6 = nfs4_cb_so6;

	sock_setsockopt(so6, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	sock_setsockopt(so6, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
	/* try to use specified port or same port as IPv4 */
	port = nfs_callback_port ? (in_port_t)nfs_callback_port : nfs4_cb_port;
ipv6_bind_again:
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_family = AF_INET6;
	sin6.sin6_addr = in6addr_any;
	sin6.sin6_port = htons(port);
	error = sock_bind(so6, (struct sockaddr *)&sin6);
	if (error) {
		if (port != nfs_callback_port) {
			/* if we simply tried to match the IPv4 port, then try any port */
			port = 0;
			goto ipv6_bind_again;
		}
		log(LOG_INFO, "nfs callback setup: error %d binding listening IPv6 socket\n", error);
		goto fail;
	}
	error = sock_getsockname(so6, (struct sockaddr *)&sin6, sin6.sin6_len);
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d getting listening IPv6 socket port\n", error);
		goto fail;
	}
	nfs4_cb_port6 = ntohs(sin6.sin6_port);

	error = sock_listen(so6, 32);
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d on IPv6 listen\n", error);
		goto fail;
	}

	/* receive timeout shouldn't matter.  If timeout on send, we'll want to drop the socket */
	timeo.tv_usec = 0;
	timeo.tv_sec = 60;
	error = sock_setsockopt(so6, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d setting IPv6 socket rx timeout\n", error);
	}
	error = sock_setsockopt(so6, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
	if (error) {
		log(LOG_INFO, "nfs callback setup: error %d setting IPv6 socket tx timeout\n", error);
	}
	sock_setsockopt(so6, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
	sock_setsockopt(so6, SOL_SOCKET, SO_NOADDRERR, &on, sizeof(on));
	sock_setsockopt(so6, SOL_SOCKET, SO_UPCALLCLOSEWAIT, &on, sizeof(on));
	error = 0;

fail:
	if (error) {
		nfs4_cb_so = nfs4_cb_so6 = NULL;
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
		if (so) {
			sock_shutdown(so, SHUT_RDWR);
			sock_close(so);
		}
		if (so6) {
			sock_shutdown(so6, SHUT_RDWR);
			sock_close(so6);
		}
	} else {
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
	}
}

/*
 * Shut down the callback channel for the NFS mount.
 *
 * Clears the mount's callback ID and releases the mounts
 * reference on the callback socket.  Last reference dropped
 * will also shut down the callback socket(s).
 */
void
nfs4_mount_callback_shutdown(struct nfsmount *nmp)
{
	struct nfs_callback_socket *ncbsp;
	socket_t so, so6;
	struct nfs4_cb_sock_list cb_socks;
	struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };

	lck_mtx_lock(get_lck_mtx(NLM_GLOBAL));
	if (nmp->nm_cbid == 0) {
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
		return;
	}
	TAILQ_REMOVE(&nfs4_cb_mounts, nmp, nm_cblink);
	/* wait for any callbacks in progress to complete */
	while (nmp->nm_cbrefs) {
		msleep(&nmp->nm_cbrefs, get_lck_mtx(NLM_GLOBAL), PSOCK, "cbshutwait", &ts);
	}
	nmp->nm_cbid = 0;
	if (--nfs4_cb_so_usecount) {
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
		return;
	}
	so = nfs4_cb_so;
	so6 = nfs4_cb_so6;
	nfs4_cb_so = nfs4_cb_so6 = NULL;
	TAILQ_INIT(&cb_socks);
	TAILQ_CONCAT(&cb_socks, &nfs4_cb_socks, ncbs_link);
	lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
	if (so) {
		sock_shutdown(so, SHUT_RDWR);
		sock_close(so);
	}
	if (so6) {
		sock_shutdown(so6, SHUT_RDWR);
		sock_close(so6);
	}
	while ((ncbsp = TAILQ_FIRST(&cb_socks))) {
		TAILQ_REMOVE(&cb_socks, ncbsp, ncbs_link);
		sock_shutdown(ncbsp->ncbs_so, SHUT_RDWR);
		sock_close(ncbsp->ncbs_so);
		nfs_rpc_record_state_cleanup(&ncbsp->ncbs_rrs);
		kfree_type(struct nfs_callback_socket, ncbsp);
	}
}

/*
 * Check periodically for stale/unused nfs callback sockets
 */
#define NFS4_CB_TIMER_PERIOD    30
#define NFS4_CB_IDLE_MAX        300
void
nfs4_callback_timer(__unused void *param0, __unused void *param1)
{
	struct nfs_callback_socket *ncbsp, *nextncbsp;
	struct timeval now;

loop:
	lck_mtx_lock(get_lck_mtx(NLM_GLOBAL));
	if (TAILQ_EMPTY(&nfs4_cb_socks)) {
		nfs4_callback_timer_on = 0;
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
		return;
	}
	microuptime(&now);
	TAILQ_FOREACH_SAFE(ncbsp, &nfs4_cb_socks, ncbs_link, nextncbsp) {
		if (!(ncbsp->ncbs_flags & NCBSOCK_DEAD) &&
		    (now.tv_sec < (ncbsp->ncbs_stamp + NFS4_CB_IDLE_MAX))) {
			continue;
		}
		TAILQ_REMOVE(&nfs4_cb_socks, ncbsp, ncbs_link);
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
		sock_shutdown(ncbsp->ncbs_so, SHUT_RDWR);
		sock_close(ncbsp->ncbs_so);
		nfs_rpc_record_state_cleanup(&ncbsp->ncbs_rrs);
		kfree_type(struct nfs_callback_socket, ncbsp);
		goto loop;
	}
	nfs4_callback_timer_on = 1;
	nfs_interval_timer_start(nfs4_callback_timer_call,
	    NFS4_CB_TIMER_PERIOD * 1000);
	lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
}

/*
 * Accept a new callback socket.
 */
void
nfs4_cb_accept(socket_t so, __unused void *arg, __unused int waitflag)
{
	socket_t newso = NULL;
	struct nfs_callback_socket *ncbsp;
	struct nfsmount *nmp;
	struct timeval timeo, now;
	int error, on = 1, ip;

	if (so == nfs4_cb_so) {
		ip = 4;
	} else if (so == nfs4_cb_so6) {
		ip = 6;
	} else {
		return;
	}

	/* allocate/initialize a new nfs_callback_socket */
	ncbsp = kalloc_type(struct nfs_callback_socket,
	    Z_WAITOK | Z_ZERO | Z_NOFAIL);
	ncbsp->ncbs_saddr.ss_len = (ip == 4) ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
	nfs_rpc_record_state_init(&ncbsp->ncbs_rrs);

	/* accept a new socket */
	error = sock_accept(so, (struct sockaddr*)&ncbsp->ncbs_saddr,
	    ncbsp->ncbs_saddr.ss_len, MSG_DONTWAIT,
	    nfs4_cb_rcv, ncbsp, &newso);
	if (error) {
		log(LOG_INFO, "nfs callback accept: error %d accepting IPv%d socket\n", error, ip);
		kfree_type(struct nfs_callback_socket, ncbsp);
		return;
	}

	/* set up the new socket */
	/* receive timeout shouldn't matter.  If timeout on send, we'll want to drop the socket */
	timeo.tv_usec = 0;
	timeo.tv_sec = 60;
	error = sock_setsockopt(newso, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
	if (error) {
		log(LOG_INFO, "nfs callback socket: error %d setting IPv%d socket rx timeout\n", error, ip);
	}
	error = sock_setsockopt(newso, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
	if (error) {
		log(LOG_INFO, "nfs callback socket: error %d setting IPv%d socket tx timeout\n", error, ip);
	}
	sock_setsockopt(newso, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
	sock_setsockopt(newso, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	sock_setsockopt(newso, SOL_SOCKET, SO_NOADDRERR, &on, sizeof(on));
	sock_setsockopt(newso, SOL_SOCKET, SO_UPCALLCLOSEWAIT, &on, sizeof(on));

	ncbsp->ncbs_so = newso;
	microuptime(&now);
	ncbsp->ncbs_stamp = now.tv_sec;

	lck_mtx_lock(get_lck_mtx(NLM_GLOBAL));

	/* add it to the list */
	TAILQ_INSERT_HEAD(&nfs4_cb_socks, ncbsp, ncbs_link);

	/* verify it's from a host we have mounted */
	TAILQ_FOREACH(nmp, &nfs4_cb_mounts, nm_cblink) {
		/* check if socket's source address matches this mount's server address */
		if (!nmp->nm_saddr) {
			continue;
		}
		if (nfs_sockaddr_cmp((struct sockaddr*)&ncbsp->ncbs_saddr, nmp->nm_saddr) == 0) {
			break;
		}
	}
	if (!nmp) { /* we don't want this socket, mark it dead */
		ncbsp->ncbs_flags |= NCBSOCK_DEAD;
	}

	/* make sure the callback socket cleanup timer is running */
	/* (shorten the timer if we've got a socket we don't want) */
	if (!nfs4_callback_timer_on) {
		nfs4_callback_timer_on = 1;
		nfs_interval_timer_start(nfs4_callback_timer_call,
		    !nmp ? 500 : (NFS4_CB_TIMER_PERIOD * 1000));
	} else if (!nmp && (nfs4_callback_timer_on < 2)) {
		nfs4_callback_timer_on = 2;
		thread_call_cancel(nfs4_callback_timer_call);
		nfs_interval_timer_start(nfs4_callback_timer_call, 500);
	}

	lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
}

/*
 * Receive mbufs from callback sockets into RPC records and process each record.
 * Detect connection has been closed and shut down.
 */
void
nfs4_cb_rcv(socket_t so, void *arg, __unused int waitflag)
{
	struct nfs_callback_socket *ncbsp = arg;
	struct timespec ts = { .tv_sec = 1, .tv_nsec = 0 };
	struct timeval now;
	mbuf_t m;
	int error = 0, recv = 1;

	lck_mtx_lock(get_lck_mtx(NLM_GLOBAL));
	while (ncbsp->ncbs_flags & NCBSOCK_UPCALL) {
		/* wait if upcall is already in progress */
		ncbsp->ncbs_flags |= NCBSOCK_UPCALLWANT;
		msleep(ncbsp, get_lck_mtx(NLM_GLOBAL), PSOCK, "cbupcall", &ts);
	}
	ncbsp->ncbs_flags |= NCBSOCK_UPCALL;
	lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));

	/* loop while we make error-free progress */
	while (!error && recv) {
		error = nfs_rpc_record_read(so, &ncbsp->ncbs_rrs, MSG_DONTWAIT, &recv, &m);
		if (m) { /* handle the request */
			error = nfs4_cb_handler(ncbsp, m);
		}
	}

	/* note: no error and no data indicates server closed its end */
	if ((error != EWOULDBLOCK) && (error || !recv)) {
		/*
		 * Socket is either being closed or should be.
		 * We can't close the socket in the context of the upcall.
		 * So we mark it as dead and leave it for the cleanup timer to reap.
		 */
		ncbsp->ncbs_stamp = 0;
		ncbsp->ncbs_flags |= NCBSOCK_DEAD;
	} else {
		microuptime(&now);
		ncbsp->ncbs_stamp = now.tv_sec;
	}

	lck_mtx_lock(get_lck_mtx(NLM_GLOBAL));
	ncbsp->ncbs_flags &= ~NCBSOCK_UPCALL;
	lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
	wakeup(ncbsp);
}

/*
 * Handle an NFS callback channel request.
 */
int
nfs4_cb_handler(struct nfs_callback_socket *ncbsp, mbuf_t mreq)
{
	socket_t so = ncbsp->ncbs_so;
	struct nfsm_chain nmreq, nmrep;
	mbuf_t mhead = NULL, mrest = NULL, m;
	struct msghdr msg;
	struct nfsmount *nmp;
	fhandle_t *fh;
	nfsnode_t np;
	nfs_stateid stateid;
	uint32_t bitmap[NFS_ATTR_BITMAP_LEN], rbitmap[NFS_ATTR_BITMAP_LEN], bmlen, truncate, attrbytes;
	uint32_t val, xid, procnum, taglen, cbid, numops, op, status;
	uint32_t auth_type, auth_len;
	uint32_t numres, *pnumres;
	int error = 0, replen, len;
	size_t sentlen = 0;

	xid = numops = op = status = procnum = taglen = cbid = 0;
	fh = zalloc(get_zone(NFS_FILE_HANDLE_ZONE));

	nfsm_chain_dissect_init(error, &nmreq, mreq);
	nfsm_chain_get_32(error, &nmreq, xid);          // RPC XID
	nfsm_chain_get_32(error, &nmreq, val);          // RPC Call
	nfsm_assert(error, (val == RPC_CALL), EBADRPC);
	nfsm_chain_get_32(error, &nmreq, val);          // RPC Version
	nfsm_assert(error, (val == RPC_VER2), ERPCMISMATCH);
	nfsm_chain_get_32(error, &nmreq, val);          // RPC Program Number
	nfsm_assert(error, (val == NFS4_CALLBACK_PROG), EPROGUNAVAIL);
	nfsm_chain_get_32(error, &nmreq, val);          // NFS Callback Program Version Number
	nfsm_assert(error, (val == NFS4_CALLBACK_PROG_VERSION), EPROGMISMATCH);
	nfsm_chain_get_32(error, &nmreq, procnum);      // NFS Callback Procedure Number
	nfsm_assert(error, (procnum <= NFSPROC4_CB_COMPOUND), EPROCUNAVAIL);

	/* Handle authentication */
	/* XXX just ignore auth for now - handling kerberos may be tricky */
	nfsm_chain_get_32(error, &nmreq, auth_type);    // RPC Auth Flavor
	nfsm_chain_get_32(error, &nmreq, auth_len);     // RPC Auth Length
	nfsm_assert(error, (auth_len <= RPCAUTH_MAXSIZ), EBADRPC);
	if (!error && (auth_len > 0)) {
		nfsm_chain_adv(error, &nmreq, nfsm_rndup(auth_len));
	}
	nfsm_chain_adv(error, &nmreq, NFSX_UNSIGNED);   // verifier flavor (should be AUTH_NONE)
	nfsm_chain_get_32(error, &nmreq, auth_len);     // verifier length
	nfsm_assert(error, (auth_len <= RPCAUTH_MAXSIZ), EBADRPC);
	if (!error && (auth_len > 0)) {
		nfsm_chain_adv(error, &nmreq, nfsm_rndup(auth_len));
	}
	if (error) {
		status = error;
		error = 0;
		goto nfsmout;
	}

	switch (procnum) {
	case NFSPROC4_CB_NULL:
		status = NFSERR_RETVOID;
		break;
	case NFSPROC4_CB_COMPOUND:
		/* tag, minorversion, cb ident, numops, op array */
		nfsm_chain_get_32(error, &nmreq, taglen);       /* tag length */
		nfsm_assert(error, (val <= NFS4_OPAQUE_LIMIT), EBADRPC);

		/* start building the body of the response */
		nfsm_mbuf_get(error, &mrest, nfsm_rndup(taglen) + 5 * NFSX_UNSIGNED);
		nfsm_chain_init(&nmrep, mrest);

		/* copy tag from request to response */
		nfsm_chain_add_32(error, &nmrep, taglen);       /* tag length */
		for (len = (int)taglen; !error && (len > 0); len -= NFSX_UNSIGNED) {
			nfsm_chain_get_32(error, &nmreq, val);
			nfsm_chain_add_32(error, &nmrep, val);
		}

		/* insert number of results placeholder */
		numres = 0;
		nfsm_chain_add_32(error, &nmrep, numres);
		pnumres = (uint32_t*)(nmrep.nmc_ptr - NFSX_UNSIGNED);

		nfsm_chain_get_32(error, &nmreq, val);          /* minorversion */
		nfsm_assert(error, (val == 0), NFSERR_MINOR_VERS_MISMATCH);
		nfsm_chain_get_32(error, &nmreq, cbid);         /* callback ID */
		nfsm_chain_get_32(error, &nmreq, numops);       /* number of operations */
		if (error) {
			if ((error == EBADRPC) || (error == NFSERR_MINOR_VERS_MISMATCH)) {
				status = error;
			} else if ((error == ENOBUFS) || (error == ENOMEM)) {
				status = NFSERR_RESOURCE;
			} else {
				status = NFSERR_SERVERFAULT;
			}
			error = 0;
			nfsm_chain_null(&nmrep);
			goto nfsmout;
		}
		/* match the callback ID to a registered mount */
		lck_mtx_lock(get_lck_mtx(NLM_GLOBAL));
		TAILQ_FOREACH(nmp, &nfs4_cb_mounts, nm_cblink) {
			if (nmp->nm_cbid != cbid) {
				continue;
			}
			/* verify socket's source address matches this mount's server address */
			if (!nmp->nm_saddr) {
				continue;
			}
			if (nfs_sockaddr_cmp((struct sockaddr*)&ncbsp->ncbs_saddr, nmp->nm_saddr) == 0) {
				break;
			}
		}
		/* mark the NFS mount as busy */
		if (nmp) {
			nmp->nm_cbrefs++;
		}
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
		if (!nmp) {
			/* if no mount match, just drop socket. */
			error = EPERM;
			nfsm_chain_null(&nmrep);
			goto out;
		}

		/* process ops, adding results to mrest */
		while (numops > 0) {
			numops--;
			nfsm_chain_get_32(error, &nmreq, op);
			if (error) {
				break;
			}
			switch (op) {
			case NFS_OP_CB_GETATTR:
				// (FH, BITMAP) -> (STATUS, BITMAP, ATTRS)
				np = NULL;
				nfsm_chain_get_fh(error, &nmreq, NFS_VER4, fh);
				bmlen = NFS_ATTR_BITMAP_LEN;
				nfsm_chain_get_bitmap(error, &nmreq, bitmap, bmlen);
				if (error) {
					status = error;
					error = 0;
					numops = 0; /* don't process any more ops */
				} else {
					/* find the node for the file handle */
					error = nfs_nget(nmp->nm_mountp, NULL, NULL, fh->fh_data, fh->fh_len, NULL, NULL, RPCAUTH_UNKNOWN, NG_NOCREATE, &np);
					if (error || !np) {
						status = NFSERR_BADHANDLE;
						error = 0;
						np = NULL;
						numops = 0; /* don't process any more ops */
					}
				}
				nfsm_chain_add_32(error, &nmrep, op);
				nfsm_chain_add_32(error, &nmrep, status);
				if (!error && (status == EBADRPC)) {
					error = status;
				}
				if (np) {
					/* only allow returning size, change, and mtime attrs */
					NFS_CLEAR_ATTRIBUTES(&rbitmap);
					attrbytes = 0;
					if (NFS_BITMAP_ISSET(&bitmap, NFS_FATTR_CHANGE)) {
						NFS_BITMAP_SET(&rbitmap, NFS_FATTR_CHANGE);
						attrbytes += 2 * NFSX_UNSIGNED;
					}
					if (NFS_BITMAP_ISSET(&bitmap, NFS_FATTR_SIZE)) {
						NFS_BITMAP_SET(&rbitmap, NFS_FATTR_SIZE);
						attrbytes += 2 * NFSX_UNSIGNED;
					}
					if (NFS_BITMAP_ISSET(&bitmap, NFS_FATTR_TIME_MODIFY)) {
						NFS_BITMAP_SET(&rbitmap, NFS_FATTR_TIME_MODIFY);
						attrbytes += 3 * NFSX_UNSIGNED;
					}
					nfsm_chain_add_bitmap(error, &nmrep, rbitmap, NFS_ATTR_BITMAP_LEN);
					nfsm_chain_add_32(error, &nmrep, attrbytes);
					if (NFS_BITMAP_ISSET(&bitmap, NFS_FATTR_CHANGE)) {
						nfsm_chain_add_64(error, &nmrep,
						    np->n_vattr.nva_change + ((np->n_flag & NMODIFIED) ? 1 : 0));
					}
					if (NFS_BITMAP_ISSET(&bitmap, NFS_FATTR_SIZE)) {
						nfsm_chain_add_64(error, &nmrep, np->n_size);
					}
					if (NFS_BITMAP_ISSET(&bitmap, NFS_FATTR_TIME_MODIFY)) {
						nfsm_chain_add_64(error, &nmrep, np->n_vattr.nva_timesec[NFSTIME_MODIFY]);
						nfsm_chain_add_32(error, &nmrep, np->n_vattr.nva_timensec[NFSTIME_MODIFY]);
					}
					nfs_node_unlock(np);
					vnode_put(NFSTOV(np));
					np = NULL;
				}
				/*
				 * If we hit an error building the reply, we can't easily back up.
				 * So we'll just update the status and hope the server ignores the
				 * extra garbage.
				 */
				break;
			case NFS_OP_CB_RECALL:
				// (STATEID, TRUNCATE, FH) -> (STATUS)
				np = NULL;
				nfsm_chain_get_stateid(error, &nmreq, &stateid);
				nfsm_chain_get_32(error, &nmreq, truncate);
				nfsm_chain_get_fh(error, &nmreq, NFS_VER4, fh);
				if (error) {
					status = error;
					error = 0;
					numops = 0; /* don't process any more ops */
				} else {
					/* find the node for the file handle */
					error = nfs_nget(nmp->nm_mountp, NULL, NULL, fh->fh_data, fh->fh_len, NULL, NULL, RPCAUTH_UNKNOWN, NG_NOCREATE, &np);
					if (error || !np) {
						status = NFSERR_BADHANDLE;
						error = 0;
						np = NULL;
						numops = 0; /* don't process any more ops */
					} else if (!(np->n_openflags & N_DELEG_MASK) ||
					    bcmp(&np->n_dstateid, &stateid, sizeof(stateid))) {
						/* delegation stateid state doesn't match */
						status = NFSERR_BAD_STATEID;
						numops = 0; /* don't process any more ops */
					}
					if (!status) { /* add node to recall queue, and wake socket thread */
						nfs4_delegation_return_enqueue(np);
					}
					if (np) {
						nfs_node_unlock(np);
						vnode_put(NFSTOV(np));
					}
				}
				nfsm_chain_add_32(error, &nmrep, op);
				nfsm_chain_add_32(error, &nmrep, status);
				if (!error && (status == EBADRPC)) {
					error = status;
				}
				break;
			case NFS_OP_CB_ILLEGAL:
			default:
				nfsm_chain_add_32(error, &nmrep, NFS_OP_CB_ILLEGAL);
				status = NFSERR_OP_ILLEGAL;
				nfsm_chain_add_32(error, &nmrep, status);
				numops = 0; /* don't process any more ops */
				break;
			}
			numres++;
		}

		if (!status && error) {
			if (error == EBADRPC) {
				status = error;
			} else if ((error == ENOBUFS) || (error == ENOMEM)) {
				status = NFSERR_RESOURCE;
			} else {
				status = NFSERR_SERVERFAULT;
			}
			error = 0;
		}

		/* Now, set the numres field */
		*pnumres = txdr_unsigned(numres);
		nfsm_chain_build_done(error, &nmrep);
		nfsm_chain_null(&nmrep);

		/* drop the callback reference on the mount */
		lck_mtx_lock(get_lck_mtx(NLM_GLOBAL));
		nmp->nm_cbrefs--;
		if (!nmp->nm_cbid) {
			wakeup(&nmp->nm_cbrefs);
		}
		lck_mtx_unlock(get_lck_mtx(NLM_GLOBAL));
		break;
	}

nfsmout:
	if (status == EBADRPC) {
		OSAddAtomic64(1, &nfsclntstats.rpcinvalid);
	}

	/* build reply header */
	error = mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &mhead);
	nfsm_chain_init(&nmrep, mhead);
	nfsm_chain_add_32(error, &nmrep, 0); /* insert space for an RPC record mark */
	nfsm_chain_add_32(error, &nmrep, xid);
	nfsm_chain_add_32(error, &nmrep, RPC_REPLY);
	if ((status == ERPCMISMATCH) || (status & NFSERR_AUTHERR)) {
		nfsm_chain_add_32(error, &nmrep, RPC_MSGDENIED);
		if (status & NFSERR_AUTHERR) {
			nfsm_chain_add_32(error, &nmrep, RPC_AUTHERR);
			nfsm_chain_add_32(error, &nmrep, (status & ~NFSERR_AUTHERR));
		} else {
			nfsm_chain_add_32(error, &nmrep, RPC_MISMATCH);
			nfsm_chain_add_32(error, &nmrep, RPC_VER2);
			nfsm_chain_add_32(error, &nmrep, RPC_VER2);
		}
	} else {
		/* reply status */
		nfsm_chain_add_32(error, &nmrep, RPC_MSGACCEPTED);
		/* XXX RPCAUTH_NULL verifier */
		nfsm_chain_add_32(error, &nmrep, RPCAUTH_NULL);
		nfsm_chain_add_32(error, &nmrep, 0);
		/* accepted status */
		switch (status) {
		case EPROGUNAVAIL:
			nfsm_chain_add_32(error, &nmrep, RPC_PROGUNAVAIL);
			break;
		case EPROGMISMATCH:
			nfsm_chain_add_32(error, &nmrep, RPC_PROGMISMATCH);
			nfsm_chain_add_32(error, &nmrep, NFS4_CALLBACK_PROG_VERSION);
			nfsm_chain_add_32(error, &nmrep, NFS4_CALLBACK_PROG_VERSION);
			break;
		case EPROCUNAVAIL:
			nfsm_chain_add_32(error, &nmrep, RPC_PROCUNAVAIL);
			break;
		case EBADRPC:
			nfsm_chain_add_32(error, &nmrep, RPC_GARBAGE);
			break;
		default:
			nfsm_chain_add_32(error, &nmrep, RPC_SUCCESS);
			if (status != NFSERR_RETVOID) {
				nfsm_chain_add_32(error, &nmrep, status);
			}
			break;
		}
	}
	nfsm_chain_build_done(error, &nmrep);
	if (error) {
		nfsm_chain_null(&nmrep);
		goto out;
	}
	error = mbuf_setnext(nmrep.nmc_mcur, mrest);
	if (error) {
		printf("nfs cb: mbuf_setnext failed %d\n", error);
		goto out;
	}
	mrest = NULL;
	/* Calculate the size of the reply */
	replen = 0;
	for (m = nmrep.nmc_mhead; m; m = mbuf_next(m)) {
		replen += mbuf_len(m);
	}
	mbuf_pkthdr_setlen(mhead, replen);
	error = mbuf_pkthdr_setrcvif(mhead, NULL);
	nfsm_chain_set_recmark(error, &nmrep, (replen - NFSX_UNSIGNED) | 0x80000000);
	nfsm_chain_null(&nmrep);

	/* send the reply */
	bzero(&msg, sizeof(msg));
	error = nfs_sendmbuf((struct sockaddr *)&ncbsp->ncbs_saddr, so, &msg, mhead, 0, &sentlen);
	mhead = NULL;
	if (!error && ((int)sentlen != replen)) {
		error = EWOULDBLOCK;
	}
	if (error == EWOULDBLOCK) { /* inability to send response is considered fatal */
		error = ETIMEDOUT;
	}
out:
	if (error) {
		nfsm_chain_cleanup(&nmrep);
	}
	if (mhead) {
		mbuf_freem(mhead);
	}
	if (mrest) {
		mbuf_freem(mrest);
	}
	if (mreq) {
		mbuf_freem(mreq);
	}
	NFS_ZFREE(get_zone(NFS_FILE_HANDLE_ZONE), fh);
	return error;
}
#endif /* CONFIG_NFS4 */

/*
 * Initialize an nfs_rpc_record_state structure.
 */
void
nfs_rpc_record_state_init(struct nfs_rpc_record_state *nrrsp)
{
	bzero(nrrsp, sizeof(*nrrsp));
	nrrsp->nrrs_markerleft = sizeof(nrrsp->nrrs_fragleft);
}

/*
 * Clean up an nfs_rpc_record_state structure.
 */
void
nfs_rpc_record_state_cleanup(struct nfs_rpc_record_state *nrrsp)
{
	if (nrrsp->nrrs_m) {
		mbuf_freem(nrrsp->nrrs_m);
		nrrsp->nrrs_m = nrrsp->nrrs_mlast = NULL;
	}
}

/*
 * Read the next (marked) RPC record from the socket.
 *
 * *recvp returns if any data was received.
 * *mp returns the next complete RPC record
 */
int
nfs_rpc_record_read(socket_t so, struct nfs_rpc_record_state *nrrsp, int flags, int *recvp, mbuf_t *mp)
{
	struct iovec aio;
	struct msghdr msg;
	size_t rcvlen;
	int error = 0;
	mbuf_t m;

	*recvp = 0;
	*mp = NULL;

	/* read the TCP RPC record marker */
	while (!error && nrrsp->nrrs_markerleft) {
		aio.iov_base = ((char*)&nrrsp->nrrs_fragleft +
		    sizeof(nrrsp->nrrs_fragleft) - nrrsp->nrrs_markerleft);
		aio.iov_len = nrrsp->nrrs_markerleft;
		bzero(&msg, sizeof(msg));
		msg.msg_iov = &aio;
		msg.msg_iovlen = 1;
		error = sock_receive(so, &msg, flags, &rcvlen);
		if (error || !rcvlen) {
			break;
		}
		*recvp = 1;
		nrrsp->nrrs_markerleft -= rcvlen;
		if (nrrsp->nrrs_markerleft) {
			continue;
		}
		/* record marker complete */
		nrrsp->nrrs_fragleft = ntohl(nrrsp->nrrs_fragleft);
		if (nrrsp->nrrs_fragleft & 0x80000000) {
			nrrsp->nrrs_lastfrag = 1;
			nrrsp->nrrs_fragleft &= ~0x80000000;
		}
		nrrsp->nrrs_reclen += nrrsp->nrrs_fragleft;
		if (nrrsp->nrrs_reclen > NFS_MAXPACKET) {
			/* This is SERIOUS! We are out of sync with the sender. */
			log(LOG_ERR, "impossible RPC record length (%d) on callback", nrrsp->nrrs_reclen);
			error = EFBIG;
		}
	}

	/* read the TCP RPC record fragment */
	while (!error && !nrrsp->nrrs_markerleft && nrrsp->nrrs_fragleft) {
		m = NULL;
		rcvlen = nrrsp->nrrs_fragleft;
		error = sock_receivembuf(so, NULL, &m, flags, &rcvlen);
		if (error || !rcvlen || !m) {
			break;
		}
		*recvp = 1;
		/* append mbufs to list */
		nrrsp->nrrs_fragleft -= rcvlen;
		if (!nrrsp->nrrs_m) {
			nrrsp->nrrs_m = m;
		} else {
			error = mbuf_setnext(nrrsp->nrrs_mlast, m);
			if (error) {
				printf("nfs tcp rcv: mbuf_setnext failed %d\n", error);
				mbuf_freem(m);
				break;
			}
		}
		while (mbuf_next(m)) {
			m = mbuf_next(m);
		}
		nrrsp->nrrs_mlast = m;
	}

	/* done reading fragment? */
	if (!error && !nrrsp->nrrs_markerleft && !nrrsp->nrrs_fragleft) {
		/* reset socket fragment parsing state */
		nrrsp->nrrs_markerleft = sizeof(nrrsp->nrrs_fragleft);
		if (nrrsp->nrrs_lastfrag) {
			/* RPC record complete */
			*mp = nrrsp->nrrs_m;
			/* reset socket record parsing state */
			nrrsp->nrrs_reclen = 0;
			nrrsp->nrrs_m = nrrsp->nrrs_mlast = NULL;
			nrrsp->nrrs_lastfrag = 0;
		}
	}

	return error;
}



/*
 * The NFS client send routine.
 *
 * Send the given NFS request out the mount's socket.
 * Holds nfs_sndlock() for the duration of this call.
 *
 * - check for request termination (sigintr)
 * - wait for reconnect, if necessary
 * - UDP: check the congestion window
 * - make a copy of the request to send
 * - UDP: update the congestion window
 * - send the request
 *
 * If sent successfully, R_MUSTRESEND and R_RESENDERR are cleared.
 * rexmit count is also updated if this isn't the first send.
 *
 * If the send is not successful, make sure R_MUSTRESEND is set.
 * If this wasn't the first transmit, set R_RESENDERR.
 * Also, undo any UDP congestion window changes made.
 *
 * If the error appears to indicate that the socket should
 * be reconnected, mark the socket for reconnection.
 *
 * Only return errors when the request should be aborted.
 */
int
nfs_send(struct nfsreq *req, int wait)
{
	struct nfsmount *nmp;
	struct nfs_socket *nso;
	int error, error2, sotype, rexmit, slpflag = 0, needrecon;
	struct msghdr msg;
	struct sockaddr *sendnam;
	mbuf_t mreqcopy;
	size_t sentlen = 0;
	struct timespec ts = { .tv_sec = 2, .tv_nsec = 0 };

again:
	error = mbuf_copym(req->r_mhead, 0, MBUF_COPYALL,
	    wait ? MBUF_WAITOK : MBUF_DONTWAIT, &mreqcopy);

	if (error) {
		if (wait) {
			log(LOG_INFO, "nfs_send: mbuf copy failed %d\n", error);
		}
		lck_mtx_lock(&req->r_mtx);
		req->r_flags &= ~R_SENDING;
		req->r_flags |= R_MUSTRESEND;
		req->r_rtt = 0;
		lck_mtx_unlock(&req->r_mtx);
		return 0;
	}

	error = nfs_sndlock(req);
	if (error) {
		lck_mtx_lock(&req->r_mtx);
		req->r_error = error;
		req->r_flags &= ~R_SENDING;
		lck_mtx_unlock(&req->r_mtx);
		return error;
	}

	error = nfs_sigintr(req->r_nmp, req, NULL, 0);
	if (error) {
		nfs_sndunlock(req);
		lck_mtx_lock(&req->r_mtx);
		req->r_error = error;
		req->r_flags &= ~R_SENDING;
		lck_mtx_unlock(&req->r_mtx);
		return error;
	}
	nmp = req->r_nmp;
	sotype = nmp->nm_sotype;

	/*
	 * If it's a setup RPC but we're not in SETUP... must need reconnect.
	 * If it's a recovery RPC but the socket's not ready... must need reconnect.
	 */
	if (((req->r_flags & R_SETUP) && !(nmp->nm_sockflags & NMSOCK_SETUP)) ||
	    ((req->r_flags & R_RECOVER) && !(nmp->nm_sockflags & NMSOCK_READY))) {
		error = ETIMEDOUT;
		nfs_sndunlock(req);
		lck_mtx_lock(&req->r_mtx);
		req->r_error = error;
		req->r_flags &= ~R_SENDING;
		lck_mtx_unlock(&req->r_mtx);
		return error;
	}

	/* If the socket needs reconnection, do that now. */
	/* wait until socket is ready - unless this request is part of setup */
	lck_mtx_lock(&nmp->nm_lock);
	if (!(nmp->nm_sockflags & NMSOCK_READY) &&
	    !((nmp->nm_sockflags & NMSOCK_SETUP) && (req->r_flags & R_SETUP))) {
		if (NMFLAG(nmp, INTR) && !(req->r_flags & R_NOINTR)) {
			slpflag |= PCATCH;
		}
		lck_mtx_unlock(&nmp->nm_lock);
		nfs_sndunlock(req);
		if (!wait) {
			lck_mtx_lock(&req->r_mtx);
			req->r_flags &= ~R_SENDING;
			req->r_flags |= R_MUSTRESEND;
			req->r_rtt = 0;
			lck_mtx_unlock(&req->r_mtx);
			return 0;
		}
		NFS_SOCK_DBG("nfs_send: 0x%llx wait reconnect\n", req->r_xid);
		lck_mtx_lock(&req->r_mtx);
		req->r_flags &= ~R_MUSTRESEND;
		req->r_rtt = 0;
		lck_mtx_unlock(&req->r_mtx);
		lck_mtx_lock(&nmp->nm_lock);
		while (!(nmp->nm_sockflags & NMSOCK_READY)) {
			/* don't bother waiting if the socket thread won't be reconnecting it */
			if (nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD)) {
				error = EIO;
				break;
			}
			if ((NMFLAG(nmp, SOFT) || (req->r_flags & R_SOFT)) && (nmp->nm_reconnect_start > 0)) {
				struct timeval now;
				microuptime(&now);
				if ((now.tv_sec - nmp->nm_reconnect_start) >= 8) {
					/* soft mount in reconnect for a while... terminate ASAP */
					OSAddAtomic64(1, &nfsclntstats.rpctimeouts);
					req->r_flags |= R_SOFTTERM;
					req->r_error = error = ETIMEDOUT;
					break;
				}
			}
			/* make sure socket thread is running, then wait */
			nfs_mount_sock_thread_wake(nmp);
			if ((error = nfs_sigintr(req->r_nmp, req, req->r_thread, 1))) {
				break;
			}
			msleep(req, &nmp->nm_lock, slpflag | PSOCK, "nfsconnectwait", &ts);
			slpflag = 0;
		}
		lck_mtx_unlock(&nmp->nm_lock);
		if (error) {
			lck_mtx_lock(&req->r_mtx);
			req->r_error = error;
			req->r_flags &= ~R_SENDING;
			lck_mtx_unlock(&req->r_mtx);
			return error;
		}
		goto again;
	}
	nso = nmp->nm_nso;
	lck_mtx_unlock(&nmp->nm_lock);
	/* note that we're using the mount's socket to do the send */
	lck_mtx_lock(&nmp->nm_sndstate_lock);
	nmp->nm_sndstate |= NFSSENDSTA_SENDING; /* will be cleared by nfs_sndunlock() */
	lck_mtx_unlock(&nmp->nm_sndstate_lock);

	if (!nso) {
		nfs_sndunlock(req);
		lck_mtx_lock(&req->r_mtx);
		req->r_flags &= ~R_SENDING;
		req->r_flags |= R_MUSTRESEND;
		req->r_rtt = 0;
		lck_mtx_unlock(&req->r_mtx);
		return 0;
	}

	lck_mtx_lock(&req->r_mtx);
	rexmit = (req->r_flags & R_SENT);

	if (sotype == SOCK_DGRAM) {
		lck_mtx_lock(&nmp->nm_lock);
		if (!(req->r_flags & R_CWND) && (nmp->nm_sent >= nmp->nm_cwnd)) {
			/* if we can't send this out yet, wait on the cwnd queue */
			slpflag = (NMFLAG(nmp, INTR) && req->r_thread) ? PCATCH : 0;
			lck_mtx_unlock(&nmp->nm_lock);
			nfs_sndunlock(req);
			req->r_flags &= ~R_SENDING;
			req->r_flags |= R_MUSTRESEND;
			lck_mtx_unlock(&req->r_mtx);
			if (!wait) {
				req->r_rtt = 0;
				return 0;
			}
			lck_mtx_lock(&nmp->nm_lock);
			while (nmp->nm_sent >= nmp->nm_cwnd) {
				if ((error = nfs_sigintr(req->r_nmp, req, req->r_thread, 1))) {
					break;
				}
				TAILQ_INSERT_TAIL(&nmp->nm_cwndq, req, r_cchain);
				msleep(req, &nmp->nm_lock, slpflag | (PZERO - 1), "nfswaitcwnd", &ts);
				slpflag = 0;
				if ((req->r_cchain.tqe_next != NFSREQNOLIST)) {
					TAILQ_REMOVE(&nmp->nm_cwndq, req, r_cchain);
					req->r_cchain.tqe_next = NFSREQNOLIST;
				}
			}
			lck_mtx_unlock(&nmp->nm_lock);
			goto again;
		}
		/*
		 * We update these *before* the send to avoid racing
		 * against others who may be looking to send requests.
		 */
		if (!rexmit) {
			/* first transmit */
			req->r_flags |= R_CWND;
			nmp->nm_sent += NFS_CWNDSCALE;
		} else {
			/*
			 * When retransmitting, turn timing off
			 * and divide congestion window by 2.
			 */
			req->r_flags &= ~R_TIMING;
			nmp->nm_cwnd >>= 1;
			if (nmp->nm_cwnd < NFS_CWNDSCALE) {
				nmp->nm_cwnd = NFS_CWNDSCALE;
			}
		}
		lck_mtx_unlock(&nmp->nm_lock);
	}

	req->r_flags &= ~R_MUSTRESEND;
	lck_mtx_unlock(&req->r_mtx);

	bzero(&msg, sizeof(msg));
	if ((sotype != SOCK_STREAM) && !sock_isconnected(nso->nso_so) && ((sendnam = nmp->nm_saddr))) {
		msg.msg_name = (caddr_t)sendnam;
		msg.msg_namelen = sendnam->sa_len;
	}
	NFS_SOCK_DUMP_MBUF("Sending mbuf\n", mreqcopy);
	error = nfs_sendmbuf(nso->nso_saddr, nso->nso_so, &msg, mreqcopy, 0, &sentlen);
	if (error || (sentlen != req->r_mreqlen)) {
		NFS_SOCK_DBG("nfs_send: 0x%llx sent %d/%d error %d\n",
		    req->r_xid, (int)sentlen, (int)req->r_mreqlen, error);
	}

	if (!error && (sentlen != req->r_mreqlen)) {
		error = EWOULDBLOCK;
	}
	needrecon = ((sotype == SOCK_STREAM) && sentlen && (sentlen != req->r_mreqlen));

	lck_mtx_lock(&req->r_mtx);
	req->r_flags &= ~R_SENDING;
	req->r_rtt = 0;
	if (rexmit && (++req->r_rexmit > NFS_MAXREXMIT)) {
		req->r_rexmit = NFS_MAXREXMIT;
	}

	if (!error) {
		/* SUCCESS */
		req->r_flags &= ~R_RESENDERR;
		if (rexmit) {
			OSAddAtomic64(1, &nfsclntstats.rpcretries);
		}
		req->r_flags |= R_SENT;
		if (req->r_flags & R_WAITSENT) {
			req->r_flags &= ~R_WAITSENT;
			wakeup(req);
		}
		nfs_sndunlock(req);
		lck_mtx_unlock(&req->r_mtx);
		return 0;
	}

	/* send failed */
	req->r_flags |= R_MUSTRESEND;
	if (rexmit) {
		req->r_flags |= R_RESENDERR;
	}
	if ((error == EINTR) || (error == ERESTART)) {
		req->r_error = error;
	}
	lck_mtx_unlock(&req->r_mtx);

	if (sotype == SOCK_DGRAM) {
		/*
		 * Note: even though a first send may fail, we consider
		 * the request sent for congestion window purposes.
		 * So we don't need to undo any of the changes made above.
		 */
		/*
		 * Socket errors ignored for connectionless sockets??
		 * For now, ignore them all
		 */
		if ((error != EINTR) && (error != ERESTART) &&
		    (error != EWOULDBLOCK) && (error != EIO) && (nso == nmp->nm_nso)) {
			int clearerror = 0, optlen = sizeof(clearerror);
			sock_getsockopt(nso->nso_so, SOL_SOCKET, SO_ERROR, &clearerror, &optlen);
#ifdef NFS_SOCKET_DEBUGGING
			if (clearerror) {
				NFS_SOCK_DBG("nfs_send: ignoring UDP socket error %d so %d\n",
				    error, clearerror);
			}
#endif
		}
	}

	/* check if it appears we should reconnect the socket */
	switch (error) {
	case EWOULDBLOCK:
		/* if send timed out, reconnect if on TCP */
		if (sotype != SOCK_STREAM) {
			break;
		}
		OS_FALLTHROUGH;
	case EPIPE:
	case EADDRNOTAVAIL:
	case ENETDOWN:
	case ENETUNREACH:
	case ENETRESET:
	case ECONNABORTED:
	case ECONNRESET:
	case ENOTCONN:
	case ESHUTDOWN:
	case ECONNREFUSED:
	case EHOSTDOWN:
	case EHOSTUNREACH:
		/* case ECANCELED??? */
		needrecon = 1;
		break;
	}
	if (needrecon && (nso == nmp->nm_nso)) { /* mark socket as needing reconnect */
		NFS_SOCK_DBG("nfs_send: 0x%llx need reconnect %d\n", req->r_xid, error);
		nfs_need_reconnect(nmp);
	}

	nfs_sndunlock(req);

	if (nfs_is_dead(error, nmp)) {
		error = EIO;
	}

	/*
	 * Don't log some errors:
	 * EPIPE errors may be common with servers that drop idle connections.
	 * EADDRNOTAVAIL may occur on network transitions.
	 * ENOTCONN may occur under some network conditions.
	 */
	if ((error == EPIPE) || (error == EADDRNOTAVAIL) || (error == ENOTCONN)) {
		error = 0;
	}
	if (error && (error != EINTR) && (error != ERESTART)) {
		log(LOG_INFO, "nfs send error %d for server %s\n", error,
		    !req->r_nmp ? "<unmounted>" :
		    vfs_statfs(req->r_nmp->nm_mountp)->f_mntfromname);
	}

	/* prefer request termination error over other errors */
	error2 = nfs_sigintr(req->r_nmp, req, req->r_thread, 0);
	if (error2) {
		error = error2;
	}

	/* only allow the following errors to be returned */
	if ((error != EINTR) && (error != ERESTART) && (error != EIO) &&
	    (error != ENXIO) && (error != ETIMEDOUT)) {
		/*
		 * We got some error we don't know what do do with,
		 * i.e., we're not reconnecting, we map it to
		 * EIO. Presumably our send failed and we better tell
		 * the caller so they don't wait for a reply that is
		 * never going to come.  If we are reconnecting we
		 * return 0 and the request will be resent.
		 */
		error = needrecon ? 0 : EIO;
	}
	return error;
}

/*
 * NFS client socket upcalls
 *
 * Pull RPC replies out of an NFS mount's socket and match them
 * up with the pending request.
 *
 * The datagram code is simple because we always get whole
 * messages out of the socket.
 *
 * The stream code is more involved because we have to parse
 * the RPC records out of the stream.
 */

/* NFS client UDP socket upcall */
void
nfs_udp_rcv(socket_t so, void *arg, __unused int waitflag)
{
	struct nfsmount *nmp = arg;
	struct nfs_socket *nso = nmp->nm_nso;
	size_t rcvlen;
	mbuf_t m;
	int error = 0;

	if (nmp->nm_sockflags & NMSOCK_CONNECTING) {
		return;
	}

	do {
		/* make sure we're on the current socket */
		if (!nso || (nso->nso_so != so)) {
			return;
		}

		m = NULL;
		rcvlen = 1000000;
		error = sock_receivembuf(so, NULL, &m, MSG_DONTWAIT, &rcvlen);
		if (m) {
			nfs_request_match_reply(nmp, m);
		}
	} while (m && !error);

	if (error && (error != EWOULDBLOCK)) {
		/* problems with the socket... mark for reconnection */
		NFS_SOCK_DBG("nfs_udp_rcv: need reconnect %d\n", error);
		nfs_need_reconnect(nmp);
	}
}

/* NFS client TCP socket upcall */
void
nfs_tcp_rcv(socket_t so, void *arg, __unused int waitflag)
{
	struct nfsmount *nmp = arg;
	struct nfs_socket *nso = nmp->nm_nso;
	struct nfs_rpc_record_state nrrs;
	mbuf_t m;
	int error = 0;
	int recv = 1;
	int wup = 0;

	if (nmp->nm_sockflags & NMSOCK_CONNECTING) {
		return;
	}

	/* make sure we're on the current socket */
	lck_mtx_lock(&nmp->nm_lock);
	nso = nmp->nm_nso;
	if (!nso || (nso->nso_so != so) || (nmp->nm_sockflags & (NMSOCK_DISCONNECTING))) {
		lck_mtx_unlock(&nmp->nm_lock);
		return;
	}
	lck_mtx_unlock(&nmp->nm_lock);

	/* make sure this upcall should be trying to do work */
	lck_mtx_lock(&nso->nso_lock);
	if (nso->nso_flags & (NSO_UPCALL | NSO_DISCONNECTING | NSO_DEAD)) {
		lck_mtx_unlock(&nso->nso_lock);
		return;
	}
	nso->nso_flags |= NSO_UPCALL;
	nrrs = nso->nso_rrs;
	lck_mtx_unlock(&nso->nso_lock);

	/* loop while we make error-free progress */
	while (!error && recv) {
		error = nfs_rpc_record_read(so, &nrrs, MSG_DONTWAIT, &recv, &m);
		if (m) { /* match completed response with request */
			nfs_request_match_reply(nmp, m);
		}
	}

	/* Update the sockets's rpc parsing state */
	lck_mtx_lock(&nso->nso_lock);
	nso->nso_rrs = nrrs;
	if (nso->nso_flags & NSO_DISCONNECTING) {
		wup = 1;
	}
	nso->nso_flags &= ~NSO_UPCALL;
	lck_mtx_unlock(&nso->nso_lock);
	if (wup) {
		wakeup(&nso->nso_flags);
	}

#ifdef NFS_SOCKET_DEBUGGING
	if (!recv && (error != EWOULDBLOCK)) {
		NFS_SOCK_DBG("nfs_tcp_rcv: got nothing, error %d, got FIN?\n", error);
	}
#endif
	/* note: no error and no data indicates server closed its end */
	if ((error != EWOULDBLOCK) && (error || !recv)) {
		/* problems with the socket... mark for reconnection */
		NFS_SOCK_DBG("nfs_tcp_rcv: need reconnect %d\n", error);
		nfs_need_reconnect(nmp);
	}
}

/*
 * "poke" a socket to try to provoke any pending errors
 */
void
nfs_sock_poke(struct nfsmount *nmp)
{
	struct iovec aio;
	struct msghdr msg;
	size_t len;
	int error = 0;
	int dummy;

	lck_mtx_lock(&nmp->nm_lock);
	if ((nmp->nm_sockflags & NMSOCK_UNMOUNT) ||
	    !(nmp->nm_sockflags & NMSOCK_READY) || !nmp->nm_nso || !nmp->nm_nso->nso_so) {
		/* Nothing to poke */
		nmp->nm_sockflags &= ~NMSOCK_POKE;
		wakeup(&nmp->nm_sockflags);
		lck_mtx_unlock(&nmp->nm_lock);
		return;
	}
	lck_mtx_unlock(&nmp->nm_lock);
	aio.iov_base = &dummy;
	aio.iov_len = 0;
	len = 0;
	bzero(&msg, sizeof(msg));
	msg.msg_iov = &aio;
	msg.msg_iovlen = 1;
	error = sock_send(nmp->nm_nso->nso_so, &msg, MSG_DONTWAIT, &len);
	NFS_SOCK_DBG("nfs_sock_poke: error %d\n", error);
	lck_mtx_lock(&nmp->nm_lock);
	nmp->nm_sockflags &= ~NMSOCK_POKE;
	wakeup(&nmp->nm_sockflags);
	lck_mtx_unlock(&nmp->nm_lock);
	nfs_is_dead(error, nmp);
}

/*
 * Match an RPC reply with the corresponding request
 */
void
nfs_request_match_reply(struct nfsmount *nmp, mbuf_t mrep)
{
	struct nfsreq *req = NULL;
	struct nfsm_chain nmrep;
	vnode_t vp = NULLVP;
	u_int32_t reply = 0, rxid = 0;
	int error = 0, asyncioq, t1;

	bzero(&nmrep, sizeof(nmrep));
	/* Get the xid and check that it is an rpc reply */
	nfsm_chain_dissect_init(error, &nmrep, mrep);
	nfsm_chain_get_32(error, &nmrep, rxid);
	nfsm_chain_get_32(error, &nmrep, reply);

	NFS_KDBG_ENTRY(NFSDBG_OP_REQ_MATCH_REPLY, nmp, nmp ? nmp->nm_sotype : 0, rxid, reply);

	if (error || (reply != RPC_REPLY)) {
		OSAddAtomic64(1, &nfsclntstats.rpcinvalid);
		mbuf_freem(mrep);
		goto out_return;
	}

	/*
	 * Loop through the request list to match up the reply
	 * Iff no match, just drop it.
	 */
	lck_mtx_lock(get_lck_mtx(NLM_REQUEST));
	TAILQ_FOREACH(req, &nfs_reqq, r_chain) {
		if (req->r_nmrep.nmc_mhead || (rxid != R_XID32(req->r_xid))) {
			continue;
		}
		/* looks like we have it, grab lock and double check */
		lck_mtx_lock(&req->r_mtx);
		if (req->r_nmrep.nmc_mhead || (rxid != R_XID32(req->r_xid))) {
			lck_mtx_unlock(&req->r_mtx);
			continue;
		}
		/* Found it.. */
		req->r_nmrep = nmrep;
		vp = NFSTOV(req->r_np);
		lck_mtx_lock(&nmp->nm_lock);
		if (nmp->nm_sotype == SOCK_DGRAM) {
			/*
			 * Update congestion window.
			 * Do the additive increase of one rpc/rtt.
			 */
			NFS_KDBG_INFO(NFSDBG_OP_REQ_MATCH_REPLY, 0xabc001, R_XID32(req->r_xid), req, nmp->nm_sent);
			if (nmp->nm_cwnd <= nmp->nm_sent) {
				nmp->nm_cwnd +=
				    ((NFS_CWNDSCALE * NFS_CWNDSCALE) +
				    (nmp->nm_cwnd >> 1)) / nmp->nm_cwnd;
				if (nmp->nm_cwnd > NFS_MAXCWND) {
					nmp->nm_cwnd = NFS_MAXCWND;
				}
			}
			if (req->r_flags & R_CWND) {
				nmp->nm_sent -= NFS_CWNDSCALE;
				req->r_flags &= ~R_CWND;
			}
			if ((nmp->nm_sent < nmp->nm_cwnd) && !TAILQ_EMPTY(&nmp->nm_cwndq)) {
				/* congestion window is open, poke the cwnd queue */
				struct nfsreq *req2 = TAILQ_FIRST(&nmp->nm_cwndq);
				TAILQ_REMOVE(&nmp->nm_cwndq, req2, r_cchain);
				req2->r_cchain.tqe_next = NFSREQNOLIST;
				wakeup(req2);
			}
		}
		/*
		 * Update rtt using a gain of 0.125 on the mean
		 * and a gain of 0.25 on the deviation.
		 */
		if (req->r_flags & R_TIMING) {
			/*
			 * Since the timer resolution of
			 * NFS_HZ is so course, it can often
			 * result in r_rtt == 0. Since
			 * r_rtt == N means that the actual
			 * rtt is between N+dt and N+2-dt ticks,
			 * add 1.
			 */
			if (proct[req->r_procnum] == 0) {
				panic("nfs_request_match_reply: proct[%d] is zero", req->r_procnum);
			}
			t1 = req->r_rtt + 1;
			t1 -= (NFS_SRTT(req) >> 3);
			NFS_SRTT(req) += t1;
			if (t1 < 0) {
				t1 = -t1;
			}
			t1 -= (NFS_SDRTT(req) >> 2);
			NFS_SDRTT(req) += t1;
		}
		nmp->nm_timeouts = 0;
		lck_mtx_unlock(&nmp->nm_lock);
		/* signal anyone waiting on this request */
		wakeup(req);
		asyncioq = (req->r_callback.rcb_func != NULL);
#if CONFIG_NFS_GSS
		if (nfs_request_using_gss(req)) {
			nfs_gss_clnt_rpcdone(req);
		}
#endif /* CONFIG_NFS_GSS */
		lck_mtx_unlock(&req->r_mtx);
		lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));
		/* if it's an async RPC with a callback, queue it up */
		if (asyncioq) {
			nfs_asyncio_finish(req);
		}
		break;
	}

	if (!req) {
		/* not matched to a request, so drop it. */
		lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));
		OSAddAtomic64(1, &nfsclntstats.rpcunexpected);
		mbuf_freem(mrep);
	}

out_return:
	NFS_KDBG_EXIT(NFSDBG_OP_REQ_MATCH_REPLY, vp, req, rxid, error);
}

/*
 * Wait for the reply for a given request...
 * ...potentially resending the request if necessary.
 */
int
nfs_wait_reply(struct nfsreq *req)
{
	struct timespec ts = { .tv_sec = 2, .tv_nsec = 0 };
	int error = 0, slpflag, first = 1;

	if (req->r_nmp && NMFLAG(req->r_nmp, INTR) && req->r_thread && !(req->r_flags & R_NOINTR)) {
		slpflag = PCATCH;
	} else {
		slpflag = 0;
	}

	lck_mtx_lock(&req->r_mtx);
	while (!req->r_nmrep.nmc_mhead) {
		if ((error = nfs_sigintr(req->r_nmp, req, first ? NULL : req->r_thread, 0))) {
			break;
		}
		if (((error = req->r_error)) || req->r_nmrep.nmc_mhead) {
			break;
		}
		/* check if we need to resend */
		if (req->r_flags & R_MUSTRESEND) {
			NFS_SOCK_DBG("nfs wait resend: p %d x 0x%llx f 0x%x rtt %d\n",
			    req->r_procnum, req->r_xid, req->r_flags, req->r_rtt);
			req->r_flags |= R_SENDING;
			lck_mtx_unlock(&req->r_mtx);
			if (nfs_request_using_gss(req)) {
				/*
				 * It's an RPCSEC_GSS request.
				 * Can't just resend the original request
				 * without bumping the cred sequence number.
				 * Go back and re-build the request.
				 */
				lck_mtx_lock(&req->r_mtx);
				req->r_flags &= ~R_SENDING;
				lck_mtx_unlock(&req->r_mtx);
				return EAGAIN;
			}
			error = nfs_send(req, 1);
			lck_mtx_lock(&req->r_mtx);
			NFS_SOCK_DBG("nfs wait resend: p %d x 0x%llx f 0x%x rtt %d err %d\n",
			    req->r_procnum, req->r_xid, req->r_flags, req->r_rtt, error);
			if (error) {
				break;
			}
			if (((error = req->r_error)) || req->r_nmrep.nmc_mhead) {
				break;
			}
		}
		/* need to poll if we're P_NOREMOTEHANG */
		if (nfs_noremotehang(req->r_thread)) {
			ts.tv_sec = 1;
		}
		msleep(req, &req->r_mtx, slpflag | (PZERO - 1), "nfswaitreply", &ts);
		first = slpflag = 0;
	}
	lck_mtx_unlock(&req->r_mtx);

	return error;
}

/*
 * An NFS request goes something like this:
 * (nb: always frees up mreq mbuf list)
 * nfs_request_create()
 *	- allocates a request struct if one is not provided
 *	- initial fill-in of the request struct
 * nfs_request_add_header()
 *	- add the RPC header
 * nfs_request_send()
 *	- link it into list
 *	- call nfs_send() for first transmit
 * nfs_request_wait()
 *	- call nfs_wait_reply() to wait for the reply
 * nfs_request_finish()
 *	- break down rpc header and return with error or nfs reply
 *	  pointed to by nmrep.
 * nfs_request_rele()
 * nfs_request_destroy()
 *      - clean up the request struct
 *      - free the request struct if it was allocated by nfs_request_create()
 */

/*
 * Set up an NFS request struct (allocating if no request passed in).
 */
int
nfs_request_create(
	nfsnode_t np,
	mount_t mp,     /* used only if !np */
	struct nfsm_chain *nmrest,
	int procnum,
	thread_t thd,
	kauth_cred_t cred,
	struct nfsreq **reqp)
{
	struct nfsreq *req, *newreq = NULL;
	struct nfsmount *nmp;

	req = *reqp;
	if (!req) {
		/* allocate a new NFS request structure */
		req = newreq = zalloc_flags(get_zone(NFS_REQUEST_ZONE), Z_WAITOK | Z_ZERO);
	} else {
		/*
		 * Make sure the request is not in the queue.
		 */
		if (req->r_lflags & RL_QUEUED) {
	#if DEVELOPMENT
			panic("nfs_request_send: req 0x%lx is in global requests queue", nfs_kernel_hideaddr(req));
	#else
			TAILQ_REMOVE(&nfs_reqq, req, r_chain);
			req->r_lflags &= ~RL_QUEUED;
	#endif /* DEVELOPMENT */
		}

		bzero(req, sizeof(*req));
	}
	if (req == newreq) {
		req->r_flags = R_ALLOCATED;
	}

	nmp = VFSTONFS(np ? NFSTOMP(np) : mp);
	if (nfs_mount_gone(nmp)) {
		if (newreq) {
			NFS_ZFREE(get_zone(NFS_REQUEST_ZONE), newreq);
		}
		return ENXIO;
	}
	lck_mtx_lock(&nmp->nm_lock);
	if ((nmp->nm_state & (NFSSTA_FORCE | NFSSTA_DEAD)) &&
	    (nmp->nm_state & NFSSTA_TIMEO)) {
		lck_mtx_unlock(&nmp->nm_lock);
		mbuf_freem(nmrest->nmc_mhead);
		nmrest->nmc_mhead = NULL;
		if (newreq) {
			NFS_ZFREE(get_zone(NFS_REQUEST_ZONE), newreq);
		}
		return ENXIO;
	}

	if ((nmp->nm_vers != NFS_VER4) && (procnum >= 0) && (procnum < NFS_NPROCS)) {
		OSAddAtomic64(1, &nfsclntstats.rpccntv3[procnum]);
	}
	if (nmp->nm_vers == NFS_VER4) {
		if (procnum == NFSPROC4_COMPOUND || procnum == NFSPROC4_NULL) {
			OSAddAtomic64(1, &nfsclntstats.opcntv4[procnum]);
		} else {
			panic("nfs_request: invalid NFSv4 RPC request %d", procnum);
		}
	}

	lck_mtx_init(&req->r_mtx, get_lck_group(NLG_REQUEST), LCK_ATTR_NULL);
	req->r_nmp = nmp;
	nmp->nm_ref++;
	req->r_np = np;
	req->r_thread = thd;
	if (!thd) {
		req->r_flags |= R_NOINTR;
	}
	if (IS_VALID_CRED(cred)) {
		kauth_cred_ref(cred);
		req->r_cred = cred;
	}
	req->r_procnum = procnum;
	if (proct[procnum] > 0) {
		req->r_flags |= R_TIMING;
	}
	req->r_nmrep.nmc_mhead = NULL;
	SLIST_INIT(&req->r_gss_seqlist);
	req->r_achain.tqe_next = NFSREQNOLIST;
	req->r_rchain.tqe_next = NFSREQNOLIST;
	req->r_cchain.tqe_next = NFSREQNOLIST;

	/* set auth flavor to use for request */
	if (!req->r_cred) {
		req->r_auth = RPCAUTH_NONE;
	} else if (req->r_np && (req->r_np->n_auth != RPCAUTH_INVALID)) {
		req->r_auth = req->r_np->n_auth;
	} else {
		req->r_auth = nmp->nm_auth;
	}

	lck_mtx_unlock(&nmp->nm_lock);

	/* move the request mbuf chain to the nfsreq */
	req->r_mrest = nmrest->nmc_mhead;
	nmrest->nmc_mhead = NULL;

	req->r_flags |= R_INITTED;
	req->r_refs = 1;
	if (newreq) {
		*reqp = req;
	}
	return 0;
}

/*
 * Clean up and free an NFS request structure.
 */
void
nfs_request_destroy(struct nfsreq *req)
{
	struct nfsmount *nmp;
	int clearjbtimeo = 0;

#if CONFIG_NFS_GSS
	struct gss_seq *gsp, *ngsp;
#endif

	if (!req || !(req->r_flags & R_INITTED)) {
		return;
	}
	nmp  = req->r_nmp;
	req->r_flags &= ~R_INITTED;
	if (req->r_lflags & RL_QUEUED) {
		nfs_reqdequeue(req);
	}

	if (req->r_achain.tqe_next != NFSREQNOLIST) {
		/*
		 * Still on an async I/O queue?
		 * %%% But which one, we may be on a local iod.
		 */
		lck_mtx_lock(get_lck_mtx(NLM_NFSIOD));
		if (nmp && req->r_achain.tqe_next != NFSREQNOLIST) {
			TAILQ_REMOVE(&nmp->nm_iodq, req, r_achain);
			req->r_achain.tqe_next = NFSREQNOLIST;
		}
		lck_mtx_unlock(get_lck_mtx(NLM_NFSIOD));
	}

	lck_mtx_lock(&req->r_mtx);
	if (nmp) {
		lck_mtx_lock(&nmp->nm_lock);
		if (req->r_flags & R_CWND) {
			/* Decrement the outstanding request count.  */
			req->r_flags &= ~R_CWND;
			nmp->nm_sent -= NFS_CWNDSCALE;
			if ((nmp->nm_sent < nmp->nm_cwnd) && !TAILQ_EMPTY(&nmp->nm_cwndq)) {
				/* congestion window is open, poke the cwnd queue */
				struct nfsreq *req2 = TAILQ_FIRST(&nmp->nm_cwndq);
				TAILQ_REMOVE(&nmp->nm_cwndq, req2, r_cchain);
				req2->r_cchain.tqe_next = NFSREQNOLIST;
				wakeup(req2);
			}
		}
		/* XXX should we just remove this conditional, we should have a reference if we're resending */
		if ((req->r_flags & R_RESENDQ) && req->r_rchain.tqe_next != NFSREQNOLIST) {
			TAILQ_REMOVE(&nmp->nm_resendq, req, r_rchain);
			req->r_flags &= ~R_RESENDQ;
			req->r_rchain.tqe_next = NFSREQNOLIST;
		}
		if (req->r_cchain.tqe_next != NFSREQNOLIST) {
			TAILQ_REMOVE(&nmp->nm_cwndq, req, r_cchain);
			req->r_cchain.tqe_next = NFSREQNOLIST;
		}
		if (req->r_flags & R_JBTPRINTFMSG) {
			req->r_flags &= ~R_JBTPRINTFMSG;
			nmp->nm_jbreqs--;
			clearjbtimeo = (nmp->nm_jbreqs == 0) ? NFSSTA_JUKEBOXTIMEO : 0;
		}
		lck_mtx_unlock(&nmp->nm_lock);
	}
	lck_mtx_unlock(&req->r_mtx);

	if (clearjbtimeo) {
		nfs_up(nmp, req->r_thread, clearjbtimeo, NULL);
	}
	if (req->r_mhead) {
		mbuf_freem(req->r_mhead);
	} else if (req->r_mrest) {
		mbuf_freem(req->r_mrest);
	}
	if (req->r_nmrep.nmc_mhead) {
		mbuf_freem(req->r_nmrep.nmc_mhead);
	}
	if (IS_VALID_CRED(req->r_cred)) {
		kauth_cred_unref(&req->r_cred);
	}
#if CONFIG_NFS_GSS
	if (nfs_request_using_gss(req)) {
		nfs_gss_clnt_rpcdone(req);
	}
	SLIST_FOREACH_SAFE(gsp, &req->r_gss_seqlist, gss_seqnext, ngsp)
	kfree_type(struct gss_seq, gsp);
	if (req->r_gss_ctx) {
		nfs_gss_clnt_ctx_unref(req);
	}
#endif /* CONFIG_NFS_GSS */
	if (req->r_wrongsec) {
		kfree_data(req->r_wrongsec, NX_MAX_SEC_FLAVORS * sizeof(uint32_t));
	}
	if (nmp) {
		nfs_mount_rele(nmp);
	}
	lck_mtx_destroy(&req->r_mtx, get_lck_group(NLG_REQUEST));
	if (req->r_flags & R_ALLOCATED) {
		NFS_ZFREE(get_zone(NFS_REQUEST_ZONE), req);
	}
}

void
nfs_request_ref(struct nfsreq *req, int locked)
{
	if (!locked) {
		lck_mtx_lock(&req->r_mtx);
	}
	if (req->r_refs <= 0) {
		panic("nfsreq reference error");
	}
	req->r_refs++;
	if (!locked) {
		lck_mtx_unlock(&req->r_mtx);
	}
}

void
nfs_request_rele(struct nfsreq *req)
{
	int destroy;

	lck_mtx_lock(&req->r_mtx);
	if (req->r_refs <= 0) {
		panic("nfsreq reference underflow");
	}
	req->r_refs--;
	destroy = (req->r_refs == 0);
	lck_mtx_unlock(&req->r_mtx);
	if (destroy) {
		nfs_request_destroy(req);
	}
}


/*
 * Add an (updated) RPC header with authorization to an NFS request.
 */
int
nfs_request_add_header(struct nfsreq *req)
{
	struct nfsmount *nmp;
	int error = 0;
	mbuf_t m;

	/* free up any previous header */
	if ((m = req->r_mhead)) {
		while (m && (m != req->r_mrest)) {
			m = mbuf_free(m);
		}
		req->r_mhead = NULL;
	}

	nmp = req->r_nmp;
	if (nfs_mount_gone(nmp)) {
		return ENXIO;
	}

	error = nfsm_rpchead(req, req->r_mrest, &req->r_xid, &req->r_mhead);
	if (error) {
		return error;
	}

	req->r_mreqlen = mbuf_pkthdr_len(req->r_mhead);
	nmp = req->r_nmp;
	if (nfs_mount_gone(nmp)) {
		return ENXIO;
	}
	lck_mtx_lock(&nmp->nm_lock);
	if (NMFLAG(nmp, SOFT) || (req->r_flags & R_SOFT)) {
		req->r_retry = nmp->nm_retry;
	} else {
		req->r_retry = NFS_MAXREXMIT + 1;       /* past clip limit */
	}
	lck_mtx_unlock(&nmp->nm_lock);

	return error;
}


/*
 * Queue an NFS request up and send it out.
 */
int
nfs_request_send(struct nfsreq *req, int wait)
{
	struct nfsmount *nmp;
	struct timeval now;

	lck_mtx_lock(&req->r_mtx);
	req->r_flags |= R_SENDING;
	lck_mtx_unlock(&req->r_mtx);

	lck_mtx_lock(get_lck_mtx(NLM_REQUEST));

	nmp = req->r_nmp;
	if (nfs_mount_gone(nmp)) {
		lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));
		return ENXIO;
	}

	microuptime(&now);
	if (!req->r_start) {
		req->r_start = now.tv_sec;
		req->r_lastmsg = now.tv_sec - (nmp->nm_tprintf_delay - nmp->nm_tprintf_initial_delay);
	}

	OSAddAtomic64(1, &nfsclntstats.rpcrequests);

	/*
	 * Chain request into list of outstanding requests. Be sure
	 * to put it LAST so timer finds oldest requests first.
	 * Make sure that the request queue timer is running
	 * to check for possible request timeout.
	 */
	TAILQ_INSERT_TAIL(&nfs_reqq, req, r_chain);
	req->r_lflags |= RL_QUEUED;
	if (!nfs_request_timer_on) {
		nfs_request_timer_on = 1;
		nfs_interval_timer_start(nfs_request_timer_call,
		    NFS_REQUESTDELAY);
	}
	lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));

	/* Send the request... */
	return nfs_send(req, wait);
}

/*
 * Call nfs_wait_reply() to wait for the reply.
 */
void
nfs_request_wait(struct nfsreq *req)
{
	req->r_error = nfs_wait_reply(req);
}

/*
 * Finish up an NFS request by dequeueing it and
 * doing the initial NFS request reply processing.
 */
int
nfs_request_finish(
	struct nfsreq *req,
	struct nfsm_chain *nmrepp,
	int *status)
{
	struct nfsmount *nmp;
	mbuf_t mrep;
	int verf_type = 0;
	struct timeval now;
	uint32_t verf_len = 0;
	uint32_t reply_status = 0;
	uint32_t rejected_status = 0;
	uint32_t auth_status = 0;
	uint32_t accepted_status = 0;
	struct nfsm_chain nmrep;
	int error, clearjbtimeo;

	error = req->r_error;

	if (nmrepp) {
		nmrepp->nmc_mhead = NULL;
	}

	/* RPC done, unlink the request. */
	nfs_reqdequeue(req);

	mrep = req->r_nmrep.nmc_mhead;

	nmp = req->r_nmp;

	if ((req->r_flags & R_CWND) && nmp) {
		/*
		 * Decrement the outstanding request count.
		 */
		req->r_flags &= ~R_CWND;
		lck_mtx_lock(&nmp->nm_lock);
		NFS_KDBG_INFO(NFSDBG_OP_REQ_FINISH, 0xabc001, R_XID32(req->r_xid), req, nmp->nm_sent);
		nmp->nm_sent -= NFS_CWNDSCALE;
		if ((nmp->nm_sent < nmp->nm_cwnd) && !TAILQ_EMPTY(&nmp->nm_cwndq)) {
			/* congestion window is open, poke the cwnd queue */
			struct nfsreq *req2 = TAILQ_FIRST(&nmp->nm_cwndq);
			TAILQ_REMOVE(&nmp->nm_cwndq, req2, r_cchain);
			req2->r_cchain.tqe_next = NFSREQNOLIST;
			wakeup(req2);
		}
		lck_mtx_unlock(&nmp->nm_lock);
	}

#if CONFIG_NFS_GSS
	if (nfs_request_using_gss(req)) {
		/*
		 * If the request used an RPCSEC_GSS credential
		 * then reset its sequence number bit in the
		 * request window.
		 */
		nfs_gss_clnt_rpcdone(req);

		/*
		 * If we need to re-send, go back and re-build the
		 * request based on a new sequence number.
		 * Note that we're using the original XID.
		 */
		if (error == EAGAIN) {
			req->r_error = 0;
			if (mrep) {
				mbuf_freem(mrep);
			}
			error = nfs_gss_clnt_args_restore(req); // remove any trailer mbufs
			req->r_nmrep.nmc_mhead = NULL;
			req->r_flags |= R_RESTART;
			if (error == ENEEDAUTH) {
				req->r_xid = 0;         // get a new XID
				error = 0;
			}
			goto nfsmout;
		}
	}
#endif /* CONFIG_NFS_GSS */

	/*
	 * If there was a successful reply, make sure to mark the mount as up.
	 * If a tprintf message was given (or if this is a timed-out soft mount)
	 * then post a tprintf message indicating the server is alive again.
	 */
	if (!error) {
		if ((req->r_flags & R_TPRINTFMSG) ||
		    (nmp && (NMFLAG(nmp, SOFT) || (req->r_flags & R_SOFT)) &&
		    ((nmp->nm_state & (NFSSTA_TIMEO | NFSSTA_FORCE | NFSSTA_DEAD)) == NFSSTA_TIMEO))) {
			nfs_up(nmp, req->r_thread, NFSSTA_TIMEO, "is alive again");
		} else {
			nfs_up(nmp, req->r_thread, NFSSTA_TIMEO, NULL);
		}
	}
	if (!error && !nmp) {
		error = ENXIO;
	}
	nfsmout_if(error);

	/*
	 * break down the RPC header and check if ok
	 */
	nmrep = req->r_nmrep;
	nfsm_chain_get_32(error, &nmrep, reply_status);
	nfsmout_if(error);
	if (reply_status == RPC_MSGDENIED) {
		nfsm_chain_get_32(error, &nmrep, rejected_status);
		nfsmout_if(error);
		if (rejected_status == RPC_MISMATCH) {
			error = ENOTSUP;
			goto nfsmout;
		}
		nfsm_chain_get_32(error, &nmrep, auth_status);
		nfsmout_if(error);
		switch (auth_status) {
#if CONFIG_NFS_GSS
		case RPCSEC_GSS_CREDPROBLEM:
		case RPCSEC_GSS_CTXPROBLEM:
			/*
			 * An RPCSEC_GSS cred or context problem.
			 * We can't use it anymore.
			 * Restore the args, renew the context
			 * and set up for a resend.
			 */
			error = nfs_gss_clnt_args_restore(req);
			if (error && error != ENEEDAUTH) {
				break;
			}

			if (!error) {
				error = nfs_gss_clnt_ctx_renew(req);
				if (error) {
					break;
				}
			}
			mbuf_freem(mrep);
			req->r_nmrep.nmc_mhead = NULL;
			req->r_xid = 0;         // get a new XID
			req->r_flags |= R_RESTART;
			goto nfsmout;
#endif /* CONFIG_NFS_GSS */
		default:
			error = EACCES;
			break;
		}
		goto nfsmout;
	}

	/* Now check the verifier */
	nfsm_chain_get_32(error, &nmrep, verf_type); // verifier flavor
	nfsm_chain_get_32(error, &nmrep, verf_len);  // verifier length
	nfsmout_if(error);

	switch (req->r_auth) {
	case RPCAUTH_NONE:
	case RPCAUTH_SYS:
		/* Any AUTH_SYS verifier is ignored */
		if (verf_len > 0) {
			nfsm_chain_adv(error, &nmrep, nfsm_rndup(verf_len));
		}
		nfsm_chain_get_32(error, &nmrep, accepted_status);
		break;
#if CONFIG_NFS_GSS
	case RPCAUTH_KRB5:
	case RPCAUTH_KRB5I:
	case RPCAUTH_KRB5P:
		error = nfs_gss_clnt_verf_get(req, &nmrep,
		    verf_type, verf_len, &accepted_status);
		break;
#endif /* CONFIG_NFS_GSS */
	}
	nfsmout_if(error);

	switch (accepted_status) {
	case RPC_SUCCESS:
		if (req->r_procnum == NFSPROC_NULL) {
			/*
			 * The NFS null procedure is unique,
			 * in not returning an NFS status.
			 */
			*status = NFS_OK;
		} else {
			nfsm_chain_get_32(error, &nmrep, *status);
			nfsmout_if(error);
		}
		nfsstat_update_nfserror(*status, nmp->nm_vers);

		if ((nmp->nm_vers != NFS_VER2) && (*status == NFSERR_TRYLATER)) {
			/*
			 * It's a JUKEBOX error - delay and try again
			 */
			int delay, slpflag = (NMFLAG(nmp, INTR) && !(req->r_flags & R_NOINTR)) ? PCATCH : 0;

			mbuf_freem(mrep);
			req->r_nmrep.nmc_mhead = NULL;
			if ((req->r_delay >= 30) && !(nmp->nm_state & NFSSTA_MOUNTED)) {
				/* we're not yet completely mounted and */
				/* we can't complete an RPC, so we fail */
				OSAddAtomic64(1, &nfsclntstats.rpctimeouts);
				nfs_softterm(req);
				error = req->r_error;
				goto nfsmout;
			}
			req->r_delay = !req->r_delay ? NFS_TRYLATERDEL : (req->r_delay * 2);
			if (req->r_delay > 30) {
				req->r_delay = 30;
			}
			if (nmp->nm_tprintf_initial_delay && (req->r_delay >= nmp->nm_tprintf_initial_delay)) {
				if (!(req->r_flags & R_JBTPRINTFMSG)) {
					req->r_flags |= R_JBTPRINTFMSG;
					lck_mtx_lock(&nmp->nm_lock);
					nmp->nm_jbreqs++;
					lck_mtx_unlock(&nmp->nm_lock);
				}
				nfs_down(req->r_nmp, req->r_thread, 0, NFSSTA_JUKEBOXTIMEO,
				    "resource temporarily unavailable (jukebox)", 0);
			}
			if ((NMFLAG(nmp, SOFT) || (req->r_flags & R_SOFT)) && (req->r_delay == 30) &&
			    !(req->r_flags & R_NOINTR)) {
				/* for soft mounts, just give up after a short while */
				OSAddAtomic64(1, &nfsclntstats.rpctimeouts);
				nfs_softterm(req);
				error = req->r_error;
				goto nfsmout;
			}
			delay = req->r_delay;
			if (req->r_callback.rcb_func) {
				microuptime(&now);
				req->r_resendtime = now.tv_sec + delay;
			} else {
				do {
					if ((error = nfs_sigintr(req->r_nmp, req, req->r_thread, 0))) {
						goto nfsmout;
					}
					tsleep(nfs_request_finish, PSOCK | slpflag, "nfs_jukebox_trylater", hz);
					slpflag = 0;
				} while (--delay > 0);
			}
			req->r_xid = 0;                 // get a new XID
			req->r_flags |= R_RESTART;
			req->r_start = 0;
			NFS_KDBG_INFO(NFSDBG_OP_REQ_FINISH, 0xabc002, R_XID32(req->r_xid), nmp, req);
			return 0;
		}

		if (req->r_flags & R_JBTPRINTFMSG) {
			req->r_flags &= ~R_JBTPRINTFMSG;
			lck_mtx_lock(&nmp->nm_lock);
			nmp->nm_jbreqs--;
			clearjbtimeo = (nmp->nm_jbreqs == 0) ? NFSSTA_JUKEBOXTIMEO : 0;
			lck_mtx_unlock(&nmp->nm_lock);
			nfs_up(nmp, req->r_thread, clearjbtimeo, "resource available again");
		}

#if CONFIG_NFS4
		if ((nmp->nm_vers >= NFS_VER4) && (*status == NFSERR_WRONGSEC)) {
			/*
			 * Hmmm... we need to try a different security flavor.
			 * The first time a request hits this, we will allocate an array
			 * to track flavors to try.  We fill the array with the mount's
			 * preferred flavors or the server's preferred flavors or just the
			 * flavors we support.
			 */
			uint32_t srvflavors[NX_MAX_SEC_FLAVORS];
			int srvcount, i, j;

			/* Call SECINFO to try to get list of flavors from server. */
			srvcount = NX_MAX_SEC_FLAVORS;
			nfs4_secinfo_rpc(nmp, &req->r_secinfo, req->r_cred, srvflavors, &srvcount);

			if (!req->r_wrongsec) {
				/* first time... set up flavor array */
				req->r_wrongsec = kalloc_data(NX_MAX_SEC_FLAVORS * sizeof(uint32_t), Z_WAITOK);
				if (!req->r_wrongsec) {
					error = EACCES;
					goto nfsmout;
				}
				i = 0;
				if (nmp->nm_sec.count) { /* use the mount's preferred list of flavors */
					for (; i < nmp->nm_sec.count; i++) {
						req->r_wrongsec[i] = nmp->nm_sec.flavors[i];
					}
				} else if (srvcount) { /* otherwise use the server's list of flavors */
					for (; i < srvcount; i++) {
						req->r_wrongsec[i] = srvflavors[i];
					}
				} else { /* otherwise, just try the flavors we support. */
					req->r_wrongsec[i++] = RPCAUTH_KRB5P;
					req->r_wrongsec[i++] = RPCAUTH_KRB5I;
					req->r_wrongsec[i++] = RPCAUTH_KRB5;
					req->r_wrongsec[i++] = RPCAUTH_SYS;
					req->r_wrongsec[i++] = RPCAUTH_NONE;
				}
				for (; i < NX_MAX_SEC_FLAVORS; i++) { /* invalidate any remaining slots */
					req->r_wrongsec[i] = RPCAUTH_INVALID;
				}
			}

			/* clear the current flavor from the list */
			for (i = 0; i < NX_MAX_SEC_FLAVORS; i++) {
				if (req->r_wrongsec[i] == req->r_auth) {
					req->r_wrongsec[i] = RPCAUTH_INVALID;
				}
			}

			/* find the next flavor to try */
			for (i = 0; i < NX_MAX_SEC_FLAVORS; i++) {
				if (req->r_wrongsec[i] != RPCAUTH_INVALID) {
					if (!srvcount) { /* no server list, just try it */
						break;
					}
					/* check that it's in the server's list */
					for (j = 0; j < srvcount; j++) {
						if (req->r_wrongsec[i] == srvflavors[j]) {
							break;
						}
					}
					if (j < srvcount) { /* found */
						break;
					}
					/* not found in server list */
					req->r_wrongsec[i] = RPCAUTH_INVALID;
				}
			}
			if (i == NX_MAX_SEC_FLAVORS) {
				/* nothing left to try! */
				error = EACCES;
				goto nfsmout;
			}

			/* retry with the next auth flavor */
			req->r_auth = req->r_wrongsec[i];
			req->r_xid = 0;                 // get a new XID
			req->r_flags |= R_RESTART;
			req->r_start = 0;
			NFS_KDBG_INFO(NFSDBG_OP_REQ_FINISH, 0xabc003, R_XID32(req->r_xid), nmp, req);
			return 0;
		}
		if ((nmp->nm_vers >= NFS_VER4) && req->r_wrongsec) {
			/*
			 * We renegotiated security for this request; so update the
			 * default security flavor for the associated node.
			 */
			if (req->r_np) {
				req->r_np->n_auth = req->r_auth;
			}
		}
#endif /* CONFIG_NFS4 */
		if (*status == NFS_OK) {
			/*
			 * Successful NFS request
			 */
			if (nmrepp == NULL) {
				error = EINVAL;
				goto nfsmout;
			}
			*nmrepp = nmrep;
			req->r_nmrep.nmc_mhead = NULL;
			microuptime(&now);
			nmp->nm_lastrcv = now.tv_sec - (nmp->nm_tprintf_delay - nmp->nm_tprintf_initial_delay);
			break;
		}
		/* Got an NFS error of some kind */

		/*
		 * If the File Handle was stale, invalidate the
		 * lookup cache, just in case.
		 */
		if ((*status == ESTALE) && req->r_np) {
			cache_purge(NFSTOV(req->r_np));
			/* if monitored, also send delete event */
			if (vnode_ismonitored(NFSTOV(req->r_np))) {
				nfs_vnode_notify(req->r_np, (VNODE_EVENT_ATTRIB | VNODE_EVENT_DELETE));
			}
		}
		if (nmp->nm_vers == NFS_VER2) {
			mbuf_freem(mrep);
		} else {
			if (nmrepp == NULL) {
				error = EINVAL;
				goto nfsmout;
			}
			*nmrepp = nmrep;
		}
		req->r_nmrep.nmc_mhead = NULL;
		error = 0;
		break;
	case RPC_PROGUNAVAIL:
		error = EPROGUNAVAIL;
		break;
	case RPC_PROGMISMATCH:
		error = ERPCMISMATCH;
		break;
	case RPC_PROCUNAVAIL:
		error = EPROCUNAVAIL;
		break;
	case RPC_GARBAGE:
		error = EBADRPC;
		break;
	case RPC_SYSTEM_ERR:
	default:
		error = EIO;
		break;
	}
nfsmout:
	if (req->r_flags & R_JBTPRINTFMSG) {
		req->r_flags &= ~R_JBTPRINTFMSG;
		if (nmp) {
			lck_mtx_lock(&nmp->nm_lock);
			nmp->nm_jbreqs--;
			clearjbtimeo = (nmp->nm_jbreqs == 0) ? NFSSTA_JUKEBOXTIMEO : 0;
			lck_mtx_unlock(&nmp->nm_lock);
			if (clearjbtimeo) {
				nfs_up(nmp, req->r_thread, clearjbtimeo, NULL);
			}
		}
	}
	NFS_KDBG_INFO(NFSDBG_OP_REQ_FINISH, 0xabc004, R_XID32(req->r_xid), req, error);
	return error;
}

/*
 * NFS request using a GSS/Kerberos security flavor?
 */
int
nfs_request_using_gss(struct nfsreq *req)
{
	if (!req->r_gss_ctx) {
		return 0;
	}
	switch (req->r_auth) {
	case RPCAUTH_KRB5:
	case RPCAUTH_KRB5I:
	case RPCAUTH_KRB5P:
		return 1;
	}
	return 0;
}

/*
 * Perform an NFS request synchronously.
 */

int
nfs_request(
	nfsnode_t np,
	mount_t mp,     /* used only if !np */
	struct nfsm_chain *nmrest,
	int procnum,
	vfs_context_t ctx,
	struct nfsreq_secinfo_args *si,
	struct nfsm_chain *nmrepp,
	u_int64_t *xidp,
	int *status)
{
	return nfs_request2(np, mp, nmrest, procnum,
	           vfs_context_thread(ctx), vfs_context_ucred(ctx),
	           si, 0, nmrepp, xidp, status);
}

int
nfs_request2(
	nfsnode_t np,
	mount_t mp,     /* used only if !np */
	struct nfsm_chain *nmrest,
	int procnum,
	thread_t thd,
	kauth_cred_t cred,
	struct nfsreq_secinfo_args *si,
	int flags,
	struct nfsm_chain *nmrepp,
	u_int64_t *xidp,
	int *status)
{
	struct nfsreq *req;
	int error;

	req = zalloc_flags(get_zone(NFS_REQUEST_ZONE), Z_WAITOK | Z_ZERO);
	if ((error = nfs_request_create(np, mp, nmrest, procnum, thd, cred, &req))) {
		goto out_free;
	}
	req->r_flags |= (flags & (R_OPTMASK | R_SOFT));
	if (si) {
		req->r_secinfo = *si;
	}

	NFS_KDBG_ENTRY(NFSDBG_OP_REQ_2, NFSTOV(np), req, R_XID32(req->r_xid), procnum);

	do {
		req->r_error = 0;
		req->r_flags &= ~R_RESTART;
		if ((error = nfs_request_add_header(req))) {
			break;
		}
		if (xidp) {
			*xidp = req->r_xid;
		}
		if ((error = nfs_request_send(req, 1))) {
			break;
		}
		nfs_request_wait(req);
		if ((error = nfs_request_finish(req, nmrepp, status))) {
			break;
		}
	} while (req->r_flags & R_RESTART);

	NFS_KDBG_EXIT(NFSDBG_OP_REQ_2, NFSTOV(np), R_XID32(req->r_xid), procnum, error);

	nfs_request_rele(req);
out_free:
	NFS_ZFREE(get_zone(NFS_REQUEST_ZONE), req);
	return error;
}


#if CONFIG_NFS_GSS
/*
 * Set up a new null proc request to exchange GSS context tokens with the
 * server. Associate the context that we are setting up with the request that we
 * are sending.
 */

int
nfs_request_gss(
	mount_t mp,
	struct nfsm_chain *nmrest,
	thread_t thd,
	kauth_cred_t cred,
	int flags,
	struct nfs_gss_clnt_ctx *cp,           /* Set to gss context to renew or setup */
	struct nfsm_chain *nmrepp,
	int *status)
{
	struct nfsreq *req;
	int error, wait = 1;

	req = zalloc_flags(get_zone(NFS_REQUEST_ZONE), Z_WAITOK | Z_ZERO);
	if ((error = nfs_request_create(NULL, mp, nmrest, NFSPROC_NULL, thd, cred, &req))) {
		goto out_free;
	}
	req->r_flags |= (flags & R_OPTMASK);

	if (cp == NULL) {
		printf("nfs_request_gss request has no context\n");
		nfs_request_rele(req);
		error = NFSERR_EAUTH;
		goto out_free;
	}
	nfs_gss_clnt_ctx_ref(req, cp);

	/*
	 * Don't wait for a reply to a context destroy advisory
	 * to avoid hanging on a dead server.
	 */
	if (cp->gss_clnt_proc == RPCSEC_GSS_DESTROY) {
		wait = 0;
	}

	NFS_KDBG_ENTRY(NFSDBG_OP_REQ_GSS, NFSTOV(req->r_np), req, R_XID32(req->r_xid), flags);

	do {
		req->r_error = 0;
		req->r_flags &= ~R_RESTART;
		if ((error = nfs_request_add_header(req))) {
			break;
		}

		if ((error = nfs_request_send(req, wait))) {
			break;
		}
		if (!wait) {
			break;
		}

		nfs_request_wait(req);
		if ((error = nfs_request_finish(req, nmrepp, status))) {
			break;
		}
	} while (req->r_flags & R_RESTART);

	NFS_KDBG_EXIT(NFSDBG_OP_REQ_GSS, NFSTOV(req->r_np), req, R_XID32(req->r_xid), error);

	nfs_gss_clnt_ctx_unref(req);
	nfs_request_rele(req);
out_free:
	NFS_ZFREE(get_zone(NFS_REQUEST_ZONE), req);
	return error;
}
#endif /* CONFIG_NFS_GSS */

/*
 * Create and start an asynchronous NFS request.
 */
int
nfs_request_async(
	nfsnode_t np,
	mount_t mp,     /* used only if !np */
	struct nfsm_chain *nmrest,
	int procnum,
	thread_t thd,
	kauth_cred_t cred,
	struct nfsreq_secinfo_args *si,
	int flags,
	struct nfsreq_cbinfo *cb,
	struct nfsreq **reqp)
{
	struct nfsreq *req;
	struct nfsmount *nmp;
	int error, sent, release = 0;

	error = nfs_request_create(np, mp, nmrest, procnum, thd, cred, reqp);
	req = *reqp;
	NFS_KDBG_ENTRY(NFSDBG_OP_REQ_ASYNC, NFSTOV(np), (req ? R_XID32(req->r_xid) : 0), procnum, error);
	if (error) {
		goto out_return;
	}
	req->r_flags |= (flags & R_OPTMASK);
	req->r_flags |= R_ASYNC;
	if (si) {
		req->r_secinfo = *si;
	}
	if (cb) {
		req->r_callback = *cb;
	}
	error = nfs_request_add_header(req);
	if (!error) {
		req->r_flags |= R_WAITSENT;
		if (req->r_callback.rcb_func) {
			nfs_request_ref(req, 0);
		}
		error = nfs_request_send(req, 1);
		lck_mtx_lock(&req->r_mtx);
		if (!error && !(req->r_flags & R_SENT) && req->r_callback.rcb_func) {
			/* make sure to wait until this async I/O request gets sent */
			int slpflag = (req->r_nmp && NMFLAG(req->r_nmp, INTR) && req->r_thread && !(req->r_flags & R_NOINTR)) ? PCATCH : 0;
			struct timespec ts = { .tv_sec = 2, .tv_nsec = 0 };
			while (!(req->r_flags & R_SENT)) {
				nmp = req->r_nmp;
				if ((req->r_flags & R_RESENDQ) && nmp && !nfs_mount_gone(nmp)) {
					lck_mtx_lock(&nmp->nm_lock);
					if ((req->r_flags & R_RESENDQ) && (nmp->nm_state & NFSSTA_RECOVER) && (req->r_rchain.tqe_next != NFSREQNOLIST)) {
						/*
						 * It's not going to get off the resend queue if we're in recovery.
						 * So, just take it off ourselves.  We could be holding mount state
						 * busy and thus holding up the start of recovery.
						 */
						TAILQ_REMOVE(&nmp->nm_resendq, req, r_rchain);
						req->r_flags &= ~R_RESENDQ;
						req->r_rchain.tqe_next = NFSREQNOLIST;
						lck_mtx_unlock(&nmp->nm_lock);
						req->r_flags |= R_SENDING;
						lck_mtx_unlock(&req->r_mtx);
						error = nfs_send(req, 1);
						/* Remove the R_RESENDQ reference */
						nfs_request_rele(req);
						lck_mtx_lock(&req->r_mtx);
						if (error) {
							break;
						}
						continue;
					}
					lck_mtx_unlock(&nmp->nm_lock);
				}
				if ((error = nfs_sigintr(req->r_nmp, req, req->r_thread, 0))) {
					break;
				}
				msleep(req, &req->r_mtx, slpflag | (PZERO - 1), "nfswaitsent", &ts);
				slpflag = 0;
			}
		}
		sent = req->r_flags & R_SENT;
		lck_mtx_unlock(&req->r_mtx);
		if (error && req->r_callback.rcb_func && !sent) {
			nfs_request_rele(req);
		}
	}
	if (error || req->r_callback.rcb_func) {
		release = 1;
	}

out_return:
	NFS_KDBG_EXIT(NFSDBG_OP_REQ_ASYNC, NFSTOV(np), (req ? R_XID32(req->r_xid) : 0), procnum, error);
	if (release) {
		nfs_request_rele(req);
	}
	return error;
}

/*
 * Wait for and finish an asynchronous NFS request.
 */
int
nfs_request_async_finish(
	struct nfsreq *req,
	struct nfsm_chain *nmrepp,
	u_int64_t *xidp,
	int *status)
{
	int error = 0, release = 0, asyncio = req->r_callback.rcb_func ? 1 : 0;
	struct nfsmount *nmp;

	NFS_KDBG_ENTRY(NFSDBG_OP_REQ_ASYNC_FINISH, NFSTOV(req->r_np), req, R_XID32(req->r_xid), req->r_procnum);

	lck_mtx_lock(&req->r_mtx);
	if (!asyncio) {
		req->r_flags |= R_ASYNCWAIT;
	}
	while (req->r_flags & R_RESENDQ) {  /* wait until the request is off the resend queue */
		struct timespec ts = { .tv_sec = 2, .tv_nsec = 0 };

		if ((nmp = req->r_nmp)) {
			lck_mtx_lock(&nmp->nm_lock);
			if ((req->r_flags & R_RESENDQ) && (nmp->nm_state & NFSSTA_RECOVER) && (req->r_rchain.tqe_next != NFSREQNOLIST)) {
				/*
				 * It's not going to get off the resend queue if we're in recovery.
				 * So, just take it off ourselves.  We could be holding mount state
				 * busy and thus holding up the start of recovery.
				 */
				TAILQ_REMOVE(&nmp->nm_resendq, req, r_rchain);
				req->r_flags &= ~R_RESENDQ;
				req->r_rchain.tqe_next = NFSREQNOLIST;
				/* Remove the R_RESENDQ reference */
				assert(req->r_refs > 0);
				req->r_refs--;
				lck_mtx_unlock(&nmp->nm_lock);
				break;
			}
			lck_mtx_unlock(&nmp->nm_lock);
		}
		if ((error = nfs_sigintr(req->r_nmp, req, req->r_thread, 0))) {
			break;
		}
		msleep(req, &req->r_mtx, PZERO - 1, "nfsresendqwait", &ts);
	}
	lck_mtx_unlock(&req->r_mtx);

	if (!error) {
		nfs_request_wait(req);
		error = nfs_request_finish(req, nmrepp, status);
	}

	while (!error && (req->r_flags & R_RESTART)) {
		if (asyncio) {
			assert(req->r_achain.tqe_next == NFSREQNOLIST);
			lck_mtx_lock(&req->r_mtx);
			req->r_flags &= ~R_IOD;
			if (req->r_resendtime) {  /* send later */
				nfs_asyncio_resend(req);
				lck_mtx_unlock(&req->r_mtx);
				error = EINPROGRESS;
				goto out_return;
			}
			lck_mtx_unlock(&req->r_mtx);
		}
		req->r_error = 0;
		req->r_flags &= ~R_RESTART;
		if ((error = nfs_request_add_header(req))) {
			break;
		}
		if ((error = nfs_request_send(req, !asyncio))) {
			break;
		}
		if (asyncio) {
			return EINPROGRESS;
		}
		nfs_request_wait(req);
		if ((error = nfs_request_finish(req, nmrepp, status))) {
			break;
		}
	}
	if (xidp) {
		*xidp = req->r_xid;
	}

	release = 1;

out_return:
	NFS_KDBG_EXIT(NFSDBG_OP_REQ_ASYNC_FINISH, NFSTOV(req->r_np), R_XID32(req->r_xid), req->r_procnum, error);
	if (release) {
		nfs_request_rele(req);
	}
	return error;
}

/*
 * Cancel a pending asynchronous NFS request.
 */
void
nfs_request_async_cancel(struct nfsreq *req)
{
	NFS_KDBG_INFO(NFSDBG_OP_REQ_ASYNC_CANCEL, 0xabc001, NFSTOV(req->r_np), R_XID32(req->r_xid), req->r_procnum);
	nfs_request_rele(req);
}

/*
 * Flag a request as being terminated.
 */
void
nfs_softterm(struct nfsreq *req)
{
	struct nfsmount *nmp = req->r_nmp;
	req->r_flags |= R_SOFTTERM;
	req->r_error = ETIMEDOUT;
	if (!(req->r_flags & R_CWND) || nfs_mount_gone(nmp)) {
		return;
	}
	/* update congestion window */
	req->r_flags &= ~R_CWND;
	lck_mtx_lock(&nmp->nm_lock);
	NFS_KDBG_INFO(NFSDBG_OP_SOFT_TERM, 0xabc001, R_XID32(req->r_xid), req, nmp->nm_sent);

	nmp->nm_sent -= NFS_CWNDSCALE;
	if ((nmp->nm_sent < nmp->nm_cwnd) && !TAILQ_EMPTY(&nmp->nm_cwndq)) {
		/* congestion window is open, poke the cwnd queue */
		struct nfsreq *req2 = TAILQ_FIRST(&nmp->nm_cwndq);
		TAILQ_REMOVE(&nmp->nm_cwndq, req2, r_cchain);
		req2->r_cchain.tqe_next = NFSREQNOLIST;
		wakeup(req2);
	}
	lck_mtx_unlock(&nmp->nm_lock);
}

/*
 * Ensure req isn't in use by the timer, then dequeue it.
 */
void
nfs_reqdequeue(struct nfsreq *req)
{
	lck_mtx_lock(get_lck_mtx(NLM_REQUEST));
	while (req->r_lflags & RL_BUSY) {
		req->r_lflags |= RL_WAITING;
		msleep(&req->r_lflags, get_lck_mtx(NLM_REQUEST), PSOCK, "reqdeq", NULL);
	}
	if (req->r_lflags & RL_QUEUED) {
		TAILQ_REMOVE(&nfs_reqq, req, r_chain);
		req->r_lflags &= ~RL_QUEUED;
	}
	lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));
}

/*
 * Busy (lock) a nfsreq, used by the nfs timer to make sure it's not
 * free()'d out from under it.
 */
void
nfs_reqbusy(struct nfsreq *req)
{
	if (req->r_lflags & RL_BUSY) {
		panic("req locked");
	}
	req->r_lflags |= RL_BUSY;
}

/*
 * Unbusy the nfsreq passed in, return the next nfsreq in the chain busied.
 */
struct nfsreq *
nfs_reqnext(struct nfsreq *req)
{
	struct nfsreq * nextreq;

	if (req == NULL) {
		return NULL;
	}
	/*
	 * We need to get and busy the next req before signalling the
	 * current one, otherwise wakeup() may block us and we'll race to
	 * grab the next req.
	 */
	nextreq = TAILQ_NEXT(req, r_chain);
	if (nextreq != NULL) {
		nfs_reqbusy(nextreq);
	}
	/* unbusy and signal. */
	req->r_lflags &= ~RL_BUSY;
	if (req->r_lflags & RL_WAITING) {
		req->r_lflags &= ~RL_WAITING;
		wakeup(&req->r_lflags);
	}
	return nextreq;
}

/*
 * NFS request queue timer routine
 *
 * Scan the NFS request queue for any requests that have timed out.
 *
 * Alert the system of unresponsive servers.
 * Mark expired requests on soft mounts as terminated.
 * For UDP, mark/signal requests for retransmission.
 */
void
nfs_request_timer(void *param0, __unused void *param1)
{
	struct nfsreq *req;
	struct nfsmount *nmp, *unmountp = param0;
	int timeo, maxtime, finish_asyncio, error;
	struct timeval now;
	TAILQ_HEAD(nfs_mount_pokeq, nfsmount) nfs_mount_poke_queue;
	TAILQ_INIT(&nfs_mount_poke_queue);

restart:
	lck_mtx_lock(get_lck_mtx(NLM_REQUEST));
	req = TAILQ_FIRST(&nfs_reqq);
	if (req == NULL) {      /* no requests - turn timer off */
		nfs_request_timer_on = 0;
		lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));
		return;
	}

	nfs_reqbusy(req);

	microuptime(&now);
	for (; req != NULL; req = nfs_reqnext(req)) {
		nmp = req->r_nmp;
		if (nmp == NULL) {
			NFS_SOCK_DBG("Found a request with out a mount!\n");
			continue;
		}
		if (req->r_error || req->r_nmrep.nmc_mhead) {
			continue;
		}
		if (unmountp && unmountp != nmp) {
			/* Ignore other mounts during unmount */
			continue;
		}
		if (unmountp && !req->r_callback.rcb_func) {
			/* just wakeup sync RPCs */
			wakeup(req);
			continue;
		}
		if ((error = nfs_sigintr(nmp, req, req->r_thread, 0))) {
			if (req->r_callback.rcb_func != NULL) {
				/* async I/O RPC needs to be finished */
				lck_mtx_lock(&req->r_mtx);
				req->r_error = error;
				finish_asyncio = !(req->r_flags & R_WAITSENT);
				wakeup(req);
				lck_mtx_unlock(&req->r_mtx);
				if (finish_asyncio) {
					nfs_asyncio_finish(req);
				}
			}
			continue;
		}

		if (unmountp) {
			/* Skip request processing */
			continue;
		}

		lck_mtx_lock(&req->r_mtx);

		if (nmp->nm_tprintf_initial_delay &&
		    ((req->r_rexmit > 2) || (req->r_flags & R_RESENDERR)) &&
		    (MAX(nmp->nm_lastrcv, req->r_lastmsg) + nmp->nm_tprintf_delay < now.tv_sec)) {
			req->r_lastmsg = now.tv_sec;
			nfs_down(req->r_nmp, req->r_thread, 0, NFSSTA_TIMEO,
			    "not responding", 1);
			req->r_flags |= R_TPRINTFMSG;
			lck_mtx_lock(&nmp->nm_lock);
			if (!(nmp->nm_state & NFSSTA_MOUNTED)) {
				lck_mtx_unlock(&nmp->nm_lock);
				/* we're not yet completely mounted and */
				/* we can't complete an RPC, so we fail */
				OSAddAtomic64(1, &nfsclntstats.rpctimeouts);
				nfs_softterm(req);
				finish_asyncio = ((req->r_callback.rcb_func != NULL) && !(req->r_flags & R_WAITSENT));
				wakeup(req);
				lck_mtx_unlock(&req->r_mtx);
				if (finish_asyncio) {
					nfs_asyncio_finish(req);
				}
				continue;
			}
			lck_mtx_unlock(&nmp->nm_lock);
		}

		/*
		 * Put a reasonable limit on the maximum timeout,
		 * and reduce that limit when soft mounts get timeouts or are in reconnect.
		 */
		if (!(NMFLAG(nmp, SOFT) || (req->r_flags & R_SOFT)) && !nfs_can_squish(nmp)) {
			maxtime = NFS_MAXTIMEO;
		} else if ((req->r_flags & (R_SETUP | R_RECOVER)) ||
		    ((nmp->nm_reconnect_start <= 0) || ((now.tv_sec - nmp->nm_reconnect_start) < 8))) {
			maxtime = (NFS_MAXTIMEO / (nmp->nm_timeouts + 1)) / 2;
		} else {
			maxtime = NFS_MINTIMEO / 4;
		}

		/*
		 * Check for request timeout.
		 */
		if (req->r_rtt >= 0) {
			req->r_rtt++;
			lck_mtx_lock(&nmp->nm_lock);
			if (req->r_flags & R_RESENDERR) {
				/* with resend errors, retry every few seconds */
				timeo = 4 * hz;
			} else {
				if (req->r_procnum == NFSPROC_NULL && req->r_gss_ctx != NULL) {
					timeo = NFS_MINIDEMTIMEO; // gss context setup
				} else if (NMFLAG(nmp, DUMBTIMER)) {
					timeo = nmp->nm_timeo;
				} else {
					timeo = NFS_RTO(nmp, proct[req->r_procnum]);
				}

				/* ensure 62.5 ms floor */
				while (16 * timeo < hz) {
					timeo *= 2;
				}
				if (nmp->nm_timeouts > 0) {
					timeo *= nfs_backoff[nmp->nm_timeouts - 1];
				}
			}
			/* limit timeout to max */
			if (timeo > maxtime) {
				timeo = maxtime;
			}
			if (req->r_rtt <= timeo) {
				NFS_SOCK_DBG("nfs timeout: req time %d and timeo is %d continue\n", req->r_rtt, timeo);
				lck_mtx_unlock(&nmp->nm_lock);
				lck_mtx_unlock(&req->r_mtx);
				continue;
			}
			/* The request has timed out */
			NFS_SOCK_DBG("nfs timeout: proc %d %d xid %llx rtt %d to %d # %d, t %ld/%d\n",
			    req->r_procnum, proct[req->r_procnum],
			    req->r_xid, req->r_rtt, timeo, nmp->nm_timeouts,
			    (now.tv_sec - req->r_start) * NFS_HZ, maxtime);
			if (nmp->nm_timeouts < 8) {
				nmp->nm_timeouts++;
			}
			if (nfs_mount_check_dead_timeout(nmp)) {
				/* Unbusy this request */
				req->r_lflags &= ~RL_BUSY;
				if (req->r_lflags & RL_WAITING) {
					req->r_lflags &= ~RL_WAITING;
					wakeup(&req->r_lflags);
				}
				lck_mtx_unlock(&req->r_mtx);

				/* No need to poke this mount */
				if (nmp->nm_sockflags & NMSOCK_POKE) {
					nmp->nm_sockflags &= ~NMSOCK_POKE;
					TAILQ_REMOVE(&nfs_mount_poke_queue, nmp, nm_pokeq);
				}
				/* Release our lock state, so we can become a zombie */
				lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));

				/*
				 * Note nfs_mount_make zombie(nmp) must be
				 * called with nm_lock held. After doing some
				 * work we release nm_lock in
				 * nfs_make_mount_zombie with out acquiring any
				 * other locks. (Later, in nfs_mount_zombie we
				 * will acquire get_lck_mtx(NLM_REQUEST), r_mtx,
				 * nm_lock in that order). So we should not be
				 * introducing deadlock here. We take a reference
				 * on the mount so that its still there when we
				 * release the lock.
				 */
				nmp->nm_ref++;
				nfs_mount_make_zombie(nmp);
				lck_mtx_unlock(&nmp->nm_lock);
				nfs_mount_rele(nmp);

				/*
				 * All the request for this mount have now been
				 * removed from the request queue. Restart to
				 * process the remaining mounts
				 */
				goto restart;
			}

			/* if it's been a few seconds, try poking the socket */
			if ((nmp->nm_sotype == SOCK_STREAM) &&
			    ((now.tv_sec - req->r_start) >= 3) &&
			    !(nmp->nm_sockflags & (NMSOCK_POKE | NMSOCK_UNMOUNT)) &&
			    (nmp->nm_sockflags & NMSOCK_READY)) {
				nmp->nm_sockflags |= NMSOCK_POKE;
				/*
				 * We take a ref on the mount so that we know the mount will still be there
				 * when we process the nfs_mount_poke_queue. An unmount request will block
				 * in nfs_mount_drain_and_cleanup until after the poke is finished. We release
				 * the reference after calling nfs_sock_poke below;
				 */
				nmp->nm_ref++;
				TAILQ_INSERT_TAIL(&nfs_mount_poke_queue, nmp, nm_pokeq);
			}
			lck_mtx_unlock(&nmp->nm_lock);
		}

		/* For soft mounts (& SETUPs/RECOVERs), check for too many retransmits/timeout. */
		if ((NMFLAG(nmp, SOFT) || (req->r_flags & (R_SETUP | R_RECOVER | R_SOFT))) &&
		    ((req->r_rexmit >= req->r_retry) || /* too many */
		    ((now.tv_sec - MAX(nmp->nm_lastrcv, req->r_start)) * NFS_HZ > maxtime))) {      /* too long */
			OSAddAtomic64(1, &nfsclntstats.rpctimeouts);
			lck_mtx_lock(&nmp->nm_lock);
			if (!(nmp->nm_state & NFSSTA_TIMEO)) {
				lck_mtx_unlock(&nmp->nm_lock);
				/* make sure we note the unresponsive server */
				/* (maxtime may be less than tprintf delay) */
				nfs_down(req->r_nmp, req->r_thread, 0, NFSSTA_TIMEO,
				    "not responding", 1);
				req->r_lastmsg = now.tv_sec;
				req->r_flags |= R_TPRINTFMSG;
			} else {
				lck_mtx_unlock(&nmp->nm_lock);
			}
			if (req->r_flags & R_NOINTR) {
				/* don't terminate nointr requests on timeout */
				lck_mtx_unlock(&req->r_mtx);
				continue;
			}
			NFS_SOCK_DBG("nfs timer TERMINATE: p %d x 0x%llx f 0x%x rtt %d t %ld\n",
			    req->r_procnum, req->r_xid, req->r_flags, req->r_rtt,
			    now.tv_sec - req->r_start);
			nfs_softterm(req);
			finish_asyncio = ((req->r_callback.rcb_func != NULL) && !(req->r_flags & R_WAITSENT));
			wakeup(req);
			lck_mtx_unlock(&req->r_mtx);
			if (finish_asyncio) {
				nfs_asyncio_finish(req);
			}
			continue;
		}

		/* for TCP, only resend if explicitly requested */
		if ((nmp->nm_sotype == SOCK_STREAM) && !(req->r_flags & R_MUSTRESEND)) {
			if (++req->r_rexmit > NFS_MAXREXMIT) {
				req->r_rexmit = NFS_MAXREXMIT;
			}
			req->r_rtt = 0;
			lck_mtx_unlock(&req->r_mtx);
			continue;
		}

		/*
		 * The request needs to be (re)sent.  Kick the requester to resend it.
		 * (unless it's already marked as needing a resend)
		 */
		if ((req->r_flags & R_MUSTRESEND) && (req->r_rtt == -1)) {
			lck_mtx_unlock(&req->r_mtx);
			continue;
		}
		NFS_SOCK_DBG("nfs timer mark resend: p %d x 0x%llx f 0x%x rtt %d\n",
		    req->r_procnum, req->r_xid, req->r_flags, req->r_rtt);
		req->r_flags |= R_MUSTRESEND;
		req->r_rtt = -1;
		wakeup(req);
		if ((req->r_flags & (R_IOD | R_ASYNC | R_ASYNCWAIT | R_SENDING)) == R_ASYNC) {
			nfs_asyncio_resend(req);
		}
		lck_mtx_unlock(&req->r_mtx);
	}

	lck_mtx_unlock(get_lck_mtx(NLM_REQUEST));

	/* poke any sockets */
	while ((nmp = TAILQ_FIRST(&nfs_mount_poke_queue))) {
		TAILQ_REMOVE(&nfs_mount_poke_queue, nmp, nm_pokeq);
		nfs_sock_poke(nmp);
		nfs_mount_rele(nmp);
	}

	nfs_interval_timer_start(nfs_request_timer_call, NFS_REQUESTDELAY);
}

/*
 * check a thread's proc for the "noremotehang" flag.
 */
int
nfs_noremotehang(thread_t thd)
{
	proc_t p = thd ? nfs_bsdthreadtask_info(thd) : NULL;
	return p && proc_noremotehang(p);
}

/*
 * Test for a termination condition pending on the process.
 * This is used to determine if we need to bail on a mount.
 * ETIMEDOUT is returned if there has been a soft timeout.
 * EINTR is returned if there is a signal pending that is not being ignored
 * ESHUTDOWN is return if the system is in shutdown.
 * and the mount is interruptable, or if we are a thread that is in the process
 * of cancellation (also SIGKILL posted).
 */
int
nfs_sigintr(struct nfsmount *nmp, struct nfsreq *req, thread_t thd, int nmplocked)
{
	proc_t p;
	int error = 0;

	if (!nmp) {
		return ENXIO;
	}

	/*
	 * If the mount is hung and we've requested shutdown, then bail.
	 * If reboot_kernel was called, no need to wait for mount to become unresponsive
	 * because network state may be unknown.
	 */
	if ((IOPMRootDomainGetWillShutdown() && (nmp->nm_state & NFSSTA_TIMEO)) ||
	    get_system_inshutdown()) {
		NFS_SOCK_DBG("Shutdown in progress\n");
		return ESHUTDOWN;
	}

	if (req && (req->r_flags & R_SOFTTERM)) {
		return ETIMEDOUT; /* request has been terminated. */
	}
	if (req && (req->r_flags & R_NOINTR)) {
		thd = NULL; /* don't check for signal on R_NOINTR */
	}
	if (!nmplocked) {
		lck_mtx_lock(&nmp->nm_lock);
	}
	if (nmp->nm_state & NFSSTA_FORCE) {
		/* If a force unmount is in progress then fail. */
		error = EIO;
	} else if (vfs_isforce(nmp->nm_mountp)) {
		/* Someone is unmounting us, go soft and mark it. */
		NFS_BITMAP_SET(nmp->nm_flags, NFS_MFLAG_SOFT);
		nmp->nm_state |= NFSSTA_FORCE;
	}

	/*
	 * Unmount in progress and mount is not responding.
	 * We should abort all "read" requests.
	 */
	if (req && vfs_isunmount(nmp->nm_mountp) &&
	    (nmp->nm_state & NFSSTA_TIMEO) && !(req->r_flags & R_NOUMOUNTINTR)) {
		error = EINTR;
	}
	/* Check if the mount is marked dead. */
	if (!error && (nmp->nm_state & NFSSTA_DEAD)) {
		error = ENXIO;
	}

	/*
	 * If the mount is hung and we've requested not to hang
	 * on remote filesystems, then bail now.
	 */
	if (current_proc() != kernproc &&
	    !error && (nmp->nm_state & NFSSTA_TIMEO) && nfs_noremotehang(thd)) {
		error = EIO;
	}

	if (!nmplocked) {
		lck_mtx_unlock(&nmp->nm_lock);
	}
	if (error) {
		return error;
	}

	/* may not have a thread for async I/O */
	if (thd == NULL || current_proc() == kernproc) {
		return 0;
	}

	/*
	 * Check if the process is aborted, but don't interrupt if we
	 * were killed by a signal and this is the exiting thread which
	 * is attempting to dump core.
	 */
	if (proc_isabortedsignal(current_proc())) {
		return EINTR;
	}

	/* mask off thread and process blocked signals. */
	if (NMFLAG(nmp, INTR) && ((p = nfs_bsdthreadtask_info(thd))) &&
	    proc_issignal(proc_pid(p), NFSINT_SIGMASK)) {
		return EINTR;
	}
	return 0;
}

/*
 * Lock a socket against others.
 * Necessary for STREAM sockets to ensure you get an entire rpc request/reply
 * and also to avoid race conditions between the processes with nfs requests
 * in progress when a reconnect is necessary.
 */
int
nfs_sndlock(struct nfsreq *req)
{
	struct nfsmount *nmp = req->r_nmp;
	u_short *statep;
	int error = 0, slpflag = 0;
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };

	if (nfs_mount_gone(nmp)) {
		return ENXIO;
	}

	NFS_KDBG_ENTRY(NFSDBG_OP_SEND_SNDLOCK_TRY, nmp, req, req->r_thread, nmp->nm_sndstate);
	lck_mtx_lock(&nmp->nm_sndstate_lock);
	statep = &nmp->nm_sndstate;

	if (NMFLAG(nmp, INTR) && req->r_thread && !(req->r_flags & R_NOINTR)) {
		slpflag = PCATCH;
	}
	while (*statep & NFSSENDSTA_SNDLOCK) {
		if ((error = nfs_sigintr(nmp, req, req->r_thread, 1))) {
			break;
		}
		*statep |= NFSSENDSTA_WANTSND;
		if (nfs_noremotehang(req->r_thread)) {
			ts.tv_sec = 1;
		}
		msleep(statep, &nmp->nm_sndstate_lock, slpflag | (PZERO - 1), "nfsndlck", &ts);
		if (slpflag == PCATCH) {
			slpflag = 0;
			ts.tv_sec = 2;
		}
	}
	if (!error) {
		NFS_KDBG_ENTRY(NFSDBG_OP_SEND_SNDLOCK_LOCK, nmp, req, req->r_thread, nmp->nm_sndstate);
		*statep |= NFSSENDSTA_SNDLOCK;
	}
	lck_mtx_unlock(&nmp->nm_sndstate_lock);
	NFS_KDBG_EXIT(NFSDBG_OP_SEND_SNDLOCK_TRY, req, req->r_thread, nmp->nm_sndstate, error);
	return error;
}

/*
 * Unlock the stream socket for others.
 */
void
nfs_sndunlock(struct nfsreq *req)
{
	struct nfsmount *nmp = req->r_nmp;
	u_short *statep, wake = 0;

	if (!nmp) {
		return;
	}
	lck_mtx_lock(&nmp->nm_sndstate_lock);
	statep = &nmp->nm_sndstate;
	if ((*statep & NFSSENDSTA_SNDLOCK) == 0) {
		panic("nfs sndunlock");
	}
	NFS_KDBG_EXIT(NFSDBG_OP_SEND_SNDLOCK_LOCK, nmp, req, req->r_thread, nmp->nm_sndstate);
	*statep &= ~(NFSSENDSTA_SNDLOCK | NFSSENDSTA_SENDING);
	if (*statep & NFSSENDSTA_WANTSND) {
		*statep &= ~NFSSENDSTA_WANTSND;
		wake = 1;
	}
	lck_mtx_unlock(&nmp->nm_sndstate_lock);
	if (wake) {
		wakeup(statep);
	}
}

int
nfs_aux_request(
	struct nfsmount *nmp,
	thread_t thd,
	struct sockaddr *saddr,
	socket_t so,
	int sotype,
	mbuf_t mreq,
	uint32_t xid,
	int bindresv,
	int timeo,
	struct nfsm_chain *nmrep)
{
	int error = 0, on = 1, try, sendat = 2, soproto, recv, optlen, restoreto = 0;
	socket_t newso = NULL;
	struct sockaddr_storage ss;
	struct timeval orig_rcvto, orig_sndto, tv = { .tv_sec = 1, .tv_usec = 0 };
	mbuf_t m, mrep = NULL;
	struct msghdr msg;
	uint32_t rxid = 0, reply = 0, reply_status, rejected_status;
	uint32_t verf_type, verf_len, accepted_status;
	size_t readlen, sentlen;
	struct nfs_rpc_record_state nrrs;

	if (!so) {
		/* create socket and set options */
		if (saddr->sa_family == AF_LOCAL) {
			soproto = 0;
		} else {
			soproto = (sotype == SOCK_DGRAM) ? IPPROTO_UDP : IPPROTO_TCP;
		}
		if ((error = sock_socket(saddr->sa_family, sotype, soproto, NULL, NULL, &newso))) {
			goto nfsmout;
		}

		if (bindresv && saddr->sa_family != AF_LOCAL) {
			int level = (saddr->sa_family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6;
			int optname = (saddr->sa_family == AF_INET) ? IP_PORTRANGE : IPV6_PORTRANGE;
			int portrange = IP_PORTRANGE_LOW;
			error = sock_setsockopt(newso, level, optname, &portrange, sizeof(portrange));
			nfsmout_if(error);
			ss.ss_len = saddr->sa_len;
			ss.ss_family = saddr->sa_family;
			if (ss.ss_family == AF_INET) {
				((struct sockaddr_in*)&ss)->sin_addr.s_addr = INADDR_ANY;
				((struct sockaddr_in*)&ss)->sin_port = htons(0);
			} else if (ss.ss_family == AF_INET6) {
				((struct sockaddr_in6*)&ss)->sin6_addr = in6addr_any;
				((struct sockaddr_in6*)&ss)->sin6_port = htons(0);
			} else {
				error = EINVAL;
			}
			if (!error) {
				error = sock_bind(newso, (struct sockaddr *)&ss);
			}
			nfsmout_if(error);
		}

		if (sotype == SOCK_STREAM) {
#                       define NFS_AUX_CONNECTION_TIMEOUT 4   /* 4 second timeout for connections */
			int count = 0;

			error = sock_connect(newso, saddr, MSG_DONTWAIT);
			if (error == EINPROGRESS) {
				error = 0;
			}
			nfsmout_if(error);

			while ((error = sock_connectwait(newso, &tv)) == EINPROGRESS) {
				/* After NFS_AUX_CONNECTION_TIMEOUT bail */
				if (++count >= NFS_AUX_CONNECTION_TIMEOUT) {
					error = ETIMEDOUT;
					break;
				}
			}
			nfsmout_if(error);
		}
		if (((error = sock_setsockopt(newso, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)))) ||
		    ((error = sock_setsockopt(newso, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)))) ||
		    ((error = sock_setsockopt(newso, SOL_SOCKET, SO_NOADDRERR, &on, sizeof(on))))) {
			goto nfsmout;
		}
		so = newso;
	} else {
		/* make sure socket is using a one second timeout in this function */
		optlen = sizeof(orig_rcvto);
		error = sock_getsockopt(so, SOL_SOCKET, SO_RCVTIMEO, &orig_rcvto, &optlen);
		if (!error) {
			optlen = sizeof(orig_sndto);
			error = sock_getsockopt(so, SOL_SOCKET, SO_SNDTIMEO, &orig_sndto, &optlen);
		}
		if (!error) {
			sock_setsockopt(so, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
			sock_setsockopt(so, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
			restoreto = 1;
		}
	}

	if (sotype == SOCK_STREAM) {
		sendat = 0; /* we only resend the request for UDP */
		nfs_rpc_record_state_init(&nrrs);
	}

	for (try = 0; try < timeo; try++) {
		if ((error = nfs_sigintr(nmp, NULL, !try ? NULL : thd, 0))) {
			break;
		}
		if (!try || (try == sendat)) {
			/* send the request (resending periodically for UDP) */
			if ((error = mbuf_copym(mreq, 0, MBUF_COPYALL, MBUF_WAITOK, &m))) {
				goto nfsmout;
			}
			bzero(&msg, sizeof(msg));
			if ((sotype == SOCK_DGRAM) && !sock_isconnected(so)) {
				msg.msg_name = saddr;
				msg.msg_namelen = saddr->sa_len;
			}
			if ((error = nfs_sendmbuf(saddr, so, &msg, m, 0, &sentlen))) {
				goto nfsmout;
			}
			sendat *= 2;
			if (sendat > 30) {
				sendat = 30;
			}
		}
		/* wait for the response */
		if (sotype == SOCK_STREAM) {
			/* try to read (more of) record */
			error = nfs_rpc_record_read(so, &nrrs, 0, &recv, &mrep);
			/* if we don't have the whole record yet, we'll keep trying */
		} else {
			readlen = 1 << 18;
			bzero(&msg, sizeof(msg));
			error = sock_receivembuf(so, &msg, &mrep, 0, &readlen);
		}
		if (error == EWOULDBLOCK) {
			continue;
		}
		nfsmout_if(error);
		/* parse the response */
		nfsm_chain_dissect_init(error, nmrep, mrep);
		nfsm_chain_get_32(error, nmrep, rxid);
		nfsm_chain_get_32(error, nmrep, reply);
		nfsmout_if(error);
		if ((rxid != xid) || (reply != RPC_REPLY)) {
			error = EBADRPC;
		}
		nfsm_chain_get_32(error, nmrep, reply_status);
		nfsmout_if(error);
		if (reply_status == RPC_MSGDENIED) {
			nfsm_chain_get_32(error, nmrep, rejected_status);
			nfsmout_if(error);
			error = (rejected_status == RPC_MISMATCH) ? ERPCMISMATCH : EACCES;
			goto nfsmout;
		}
		nfsm_chain_get_32(error, nmrep, verf_type); /* verifier flavor */
		nfsm_chain_get_32(error, nmrep, verf_len); /* verifier length */
		nfsmout_if(error);
		if (verf_len) {
			nfsm_chain_adv(error, nmrep, nfsm_rndup(verf_len));
		}
		nfsm_chain_get_32(error, nmrep, accepted_status);
		nfsmout_if(error);
		switch (accepted_status) {
		case RPC_SUCCESS:
			error = 0;
			break;
		case RPC_PROGUNAVAIL:
			error = EPROGUNAVAIL;
			break;
		case RPC_PROGMISMATCH:
			error = EPROGMISMATCH;
			break;
		case RPC_PROCUNAVAIL:
			error = EPROCUNAVAIL;
			break;
		case RPC_GARBAGE:
			error = EBADRPC;
			break;
		case RPC_SYSTEM_ERR:
		default:
			error = EIO;
			break;
		}
		break;
	}
nfsmout:
	if (restoreto) {
		sock_setsockopt(so, SOL_SOCKET, SO_RCVTIMEO, &orig_rcvto, sizeof(tv));
		sock_setsockopt(so, SOL_SOCKET, SO_SNDTIMEO, &orig_sndto, sizeof(tv));
	}
	if (newso) {
		sock_shutdown(newso, SHUT_RDWR);
		sock_close(newso);
	}
	mbuf_freem(mreq);
	return error;
}

int
nfs_portmap_lookup(
	struct nfsmount *nmp,
	vfs_context_t ctx,
	struct sockaddr *sa,
	socket_t so,
	uint32_t protocol,
	uint32_t vers,
	uint32_t stype,
	int timeo)
{
	thread_t thd = vfs_context_thread(ctx);
	kauth_cred_t cred = vfs_context_ucred(ctx);
	struct sockaddr_storage ss;
	struct sockaddr *saddr = (struct sockaddr*)&ss;
	static struct sockaddr_un rpcbind_cots = {
		sizeof(struct sockaddr_un),
		AF_LOCAL,
		RPCB_TICOTSORD_PATH
	};
	static struct sockaddr_un rpcbind_clts = {
		sizeof(struct sockaddr_un),
		AF_LOCAL,
		RPCB_TICLTS_PATH
	};
	struct nfsm_chain nmreq, nmrep;
	mbuf_t mreq;
	int error = 0, ip, pmprog, pmvers, pmproc;
	uint32_t ualen = 0, scopeid = 0, port32;
	uint64_t xid = 0;
	char uaddr[MAX_IPv6_STR_LEN + 16];

	bcopy(sa, saddr, min(sizeof(ss), sa->sa_len));
	if (saddr->sa_family == AF_INET) {
		ip = 4;
		pmprog = PMAPPROG;
		pmvers = PMAPVERS;
		pmproc = PMAPPROC_GETPORT;
	} else if (saddr->sa_family == AF_INET6) {
		ip = 6;
		pmprog = RPCBPROG;
		pmvers = RPCBVERS4;
		pmproc = RPCBPROC_GETVERSADDR;
	} else if (saddr->sa_family == AF_LOCAL) {
		ip = 0;
		pmprog = RPCBPROG;
		pmvers = RPCBVERS4;
		pmproc = RPCBPROC_GETVERSADDR;
		NFS_SOCK_DBG("%s\n", ((struct sockaddr_un*)sa)->sun_path);
		saddr = (struct sockaddr*)((stype == SOCK_STREAM) ? &rpcbind_cots : &rpcbind_clts);
	} else {
		return EINVAL;
	}
	nfsm_chain_null(&nmreq);
	nfsm_chain_null(&nmrep);

tryagain:
	/* send portmapper request to get port/uaddr */
	if (ip == 4) {
		((struct sockaddr_in*)saddr)->sin_port = htons(PMAPPORT);
	} else if (ip == 6) {
		((struct sockaddr_in6*)saddr)->sin6_port = htons(PMAPPORT);
	}
	nfsm_chain_build_alloc_init(error, &nmreq, 8 * NFSX_UNSIGNED);
	nfsm_chain_add_32(error, &nmreq, protocol);
	nfsm_chain_add_32(error, &nmreq, vers);
	if (ip == 4) {
		nfsm_chain_add_32(error, &nmreq, stype == SOCK_STREAM ? IPPROTO_TCP : IPPROTO_UDP);
		nfsm_chain_add_32(error, &nmreq, 0);
	} else {
		if (stype == SOCK_STREAM) {
			if (ip == 6) {
				nfsm_chain_add_string(error, &nmreq, "tcp6", 4);
			} else {
				nfsm_chain_add_string(error, &nmreq, "ticotsord", 9);
			}
		} else {
			if (ip == 6) {
				nfsm_chain_add_string(error, &nmreq, "udp6", 4);
			} else {
				nfsm_chain_add_string(error, &nmreq, "ticlts", 6);
			}
		}
		nfsm_chain_add_string(error, &nmreq, "", 0); /* uaddr */
		nfsm_chain_add_string(error, &nmreq, "", 0); /* owner */
	}
	nfsm_chain_build_done(error, &nmreq);
	nfsmout_if(error);
	error = nfsm_rpchead2(nmp, stype, pmprog, pmvers, pmproc,
	    RPCAUTH_SYS, cred, NULL, nmreq.nmc_mhead, &xid, &mreq);
	nfsmout_if(error);
	nmreq.nmc_mhead = NULL;

	NFS_SOCK_DUMP_MBUF("nfs_portmap_loockup request", mreq);
	error = nfs_aux_request(nmp, thd, saddr, so,
	    stype, mreq, R_XID32(xid), 0, timeo, &nmrep);
	NFS_SOCK_DUMP_MBUF("nfs_portmap_lookup reply", nmrep.nmc_mhead);
	NFS_SOCK_DBG("rpcbind request returned %d for program %u vers %u: %s, socktype %d\n", error, protocol, vers,
	    (saddr->sa_family == AF_LOCAL) ? ((struct sockaddr_un *)saddr)->sun_path :
	    (saddr->sa_family == AF_INET6) ? "INET6 socket" : "INET socket", stype);

	/* grab port from portmap response */
	if (ip == 4) {
		nfsm_chain_get_32(error, &nmrep, port32);
		if (!error) {
			if (NFS_PORT_INVALID(port32)) {
				error = EBADRPC;
			} else {
				((struct sockaddr_in*)sa)->sin_port = htons((in_port_t)port32);
			}
		}
	} else {
		/* get uaddr string and convert to sockaddr */
		nfsm_chain_get_32(error, &nmrep, ualen);
		if (!error) {
			if (ualen > (sizeof(uaddr) - 1)) {
				error = EIO;
			}
			if (ualen < 1) {
				/* program is not available, just return a zero port */
				bcopy(sa, saddr, min(sizeof(ss), sa->sa_len));
				if (ip == 6) {
					((struct sockaddr_in6*)saddr)->sin6_port = htons(0);
				} else {
					((struct sockaddr_un*)saddr)->sun_path[0] = '\0';
				}
				NFS_SOCK_DBG("Program %u version %u unavailable", protocol, vers);
			} else {
				nfsm_chain_get_opaque(error, &nmrep, ualen, uaddr);
				NFS_SOCK_DBG("Got uaddr %s\n", uaddr);
				if (!error) {
					uaddr[ualen] = '\0';
					if (ip == 6) {
						scopeid = ((struct sockaddr_in6*)saddr)->sin6_scope_id;
					}
					if (!nfs_uaddr2sockaddr(uaddr, saddr)) {
						error = EIO;
					}
					if (ip == 6 && scopeid != ((struct sockaddr_in6*)saddr)->sin6_scope_id) {
						NFS_SOCK_DBG("Setting scope_id from %u to %u\n", ((struct sockaddr_in6*)saddr)->sin6_scope_id, scopeid);
						((struct sockaddr_in6*)saddr)->sin6_scope_id = scopeid;
					}
				}
			}
		}
		if ((error == EPROGMISMATCH) || (error == EPROCUNAVAIL) || (error == EIO) || (error == EBADRPC)) {
			/* remote doesn't support rpcbind version or proc (or we couldn't parse uaddr) */
			if (pmvers == RPCBVERS4) {
				/* fall back to v3 and GETADDR */
				pmvers = RPCBVERS3;
				pmproc = RPCBPROC_GETADDR;
				nfsm_chain_cleanup(&nmreq);
				nfsm_chain_cleanup(&nmrep);
				bcopy(sa, saddr, min(sizeof(ss), sa->sa_len));
				xid = 0;
				error = 0;
				goto tryagain;
			}
		}
		if (!error) {
			bcopy(saddr, sa, min(saddr->sa_len, sa->sa_len));
		}
	}
nfsmout:
	nfsm_chain_cleanup(&nmreq);
	nfsm_chain_cleanup(&nmrep);
	NFS_SOCK_DBG("Returned %d\n", error);

	return error;
}

void
nfs_msg(thread_t thd,
    const char *server,
    const char *msg,
    int error)
{
	if (error) {
		tprintf_thd(thd, "nfs server %s: %s, error %d\n", server, msg, error);
	} else {
		tprintf_thd(thd, "nfs server %s: %s\n", server, msg);
	}
}

#define NFS_SQUISH_MOBILE_ONLY          0x0001          /* Squish mounts only on mobile machines */
#define NFS_SQUISH_AUTOMOUNTED_ONLY     0x0002          /* Squish mounts only if the are automounted */
#define NFS_SQUISH_SOFT                 0x0004          /* Treat all soft mounts as though they were on a mobile machine */
#define NFS_SQUISH_QUICK                0x0008          /* Try to squish mounts more quickly. */
#define NFS_SQUISH_SHUTDOWN             0x1000          /* Squish all mounts on shutdown. Currently not implemented */

uint32_t nfs_squishy_flags = NFS_SQUISH_MOBILE_ONLY | NFS_SQUISH_AUTOMOUNTED_ONLY | NFS_SQUISH_QUICK;
int32_t nfs_is_mobile;

/* Default value of tcp_sendspace and tcp_recvspace */
#if TARGET_OS_OSX
uint32_t nfs_tcp_sockbuf = 6 * 1024 * 1024; /* 6MB */
#else
uint32_t nfs_tcp_sockbuf = 128 * 1024;      /* 128K */
#endif /* TARGET_OS_OSX */

#define NFS_SQUISHY_DEADTIMEOUT         8       /* Dead time out for squishy mounts */
#define NFS_SQUISHY_QUICKTIMEOUT        4       /* Quicker dead time out when nfs_squish_flags NFS_SQUISH_QUICK bit is set*/

/*
 * Could this mount be squished?
 */
int
nfs_can_squish(struct nfsmount *nmp)
{
	uint64_t flags = vfs_flags(nmp->nm_mountp);
	int softsquish = ((nfs_squishy_flags & NFS_SQUISH_SOFT) & NMFLAG(nmp, SOFT));

	if (!softsquish && (nfs_squishy_flags & NFS_SQUISH_MOBILE_ONLY) && nfs_is_mobile == 0) {
		return 0;
	}

	if ((nfs_squishy_flags & NFS_SQUISH_AUTOMOUNTED_ONLY) && (flags & MNT_AUTOMOUNTED) == 0) {
		return 0;
	}

	return 1;
}

/*
 * NFS mounts default to "rw,hard" - but frequently on mobile clients
 * the mount may become "not responding".  It's desirable to be able
 * to unmount these dead mounts, but only if there is no risk of
 * losing data or crashing applications.  A "squishy" NFS mount is one
 * that can be force unmounted with little risk of harm.
 *
 * nfs_is_squishy checks if a mount is in a squishy state.  A mount is
 * in a squishy state iff it is allowed to be squishy and there are no
 * dirty pages and there are no mmapped files and there are no files
 * open for write. Mounts are allowed to be squishy is controlled by
 * the settings of the nfs_squishy_flags and its mobility state. These
 * flags can be set by sysctls.
 *
 * If nfs_is_squishy determines that we are in a squishy state we will
 * update the current dead timeout to at least NFS_SQUISHY_DEADTIMEOUT
 * (or NFS_SQUISHY_QUICKTIMEOUT if NFS_SQUISH_QUICK is set) (see
 * above) or 1/8th of the mount's nm_deadtimeout value, otherwise we just
 * update the current dead timeout with the mount's nm_deadtimeout
 * value set at mount time.
 *
 * Assumes that nm_lock is held.
 *
 * Note this routine is racey, but its effects on setting the
 * dead timeout only have effects when we're in trouble and are likely
 * to stay that way. Since by default its only for automounted
 * volumes on mobile machines; this is a reasonable trade off between
 * data integrity and user experience. It can be disabled or set via
 * nfs.conf file.
 */

int
nfs_is_squishy(struct nfsmount *nmp)
{
	mount_t mp = nmp->nm_mountp;
	int squishy = 0;
	int timeo = (nfs_squishy_flags & NFS_SQUISH_QUICK) ? NFS_SQUISHY_QUICKTIMEOUT : NFS_SQUISHY_DEADTIMEOUT;

	NFS_SOCK_DBG("%s: nm_curdeadtimeout = %d, nfs_is_mobile = %d\n",
	    vfs_statfs(mp)->f_mntfromname, nmp->nm_curdeadtimeout, nfs_is_mobile);

	if (!nfs_can_squish(nmp)) {
		goto out;
	}

	timeo =  (nmp->nm_deadtimeout > timeo) ? max(nmp->nm_deadtimeout / 8, timeo) : timeo;
	NFS_SOCK_DBG("nm_writers = %d  nm_mappers = %d timeo = %d\n", nmp->nm_writers, nmp->nm_mappers, timeo);

	if (nmp->nm_writers == 0 && nmp->nm_mappers == 0) {
		uint64_t flags = mp ? vfs_flags(mp) : 0;
		squishy = 1;

		/*
		 * Walk the nfs nodes and check for dirty buffers it we're not
		 * RDONLY and we've not already been declared as squishy since
		 * this can be a bit expensive.
		 */
		if (!(flags & MNT_RDONLY) && !(nmp->nm_state & NFSSTA_SQUISHY)) {
			squishy = !nfs_mount_is_dirty(mp);
		}
	}

out:
	if (squishy) {
		nmp->nm_state |= NFSSTA_SQUISHY;
	} else {
		nmp->nm_state &= ~NFSSTA_SQUISHY;
	}

	nmp->nm_curdeadtimeout = squishy ? timeo : nmp->nm_deadtimeout;

	NFS_SOCK_DBG("nm_curdeadtimeout = %d\n", nmp->nm_curdeadtimeout);

	return squishy;
}

/*
 * On a send operation, if we can't reach the server and we've got only one server to talk to
 * and NFS_SQUISH_QUICK flag is set and we are in a squishy state then mark the mount as dead
 * and ask to be forcibly unmounted. Return 1 if we're dead and 0 otherwise.
 */
int
nfs_is_dead(int error, struct nfsmount *nmp)
{
	fsid_t fsid;

	lck_mtx_lock(&nmp->nm_lock);
	if (nmp->nm_state & NFSSTA_DEAD) {
		lck_mtx_unlock(&nmp->nm_lock);
		return 1;
	}

	if ((error != ENETUNREACH && error != EHOSTUNREACH && error != EADDRNOTAVAIL) ||
	    !(nmp->nm_locations.nl_numlocs == 1 && nmp->nm_locations.nl_locations[0]->nl_servcount == 1)) {
		lck_mtx_unlock(&nmp->nm_lock);
		return 0;
	}

	if ((nfs_squishy_flags & NFS_SQUISH_QUICK) && nfs_is_squishy(nmp)) {
		printf("nfs_is_dead: nfs server %s: unreachable. Squished dead\n", vfs_statfs(nmp->nm_mountp)->f_mntfromname);
		fsid = vfs_statfs(nmp->nm_mountp)->f_fsid;
		lck_mtx_unlock(&nmp->nm_lock);
		nfs_mount_zombie(nmp, NFSSTA_DEAD);
		vfs_event_signal(&fsid, VQ_DEAD, 0);
		return 1;
	}
	lck_mtx_unlock(&nmp->nm_lock);
	return 0;
}

/*
 * If we've experienced timeouts and we're not really a
 * classic hard mount, then just return cached data to
 * the caller instead of likely hanging on an RPC.
 */
int
nfs_use_cache(struct nfsmount *nmp)
{
	/*
	 *%%% We always let mobile users goto the cache,
	 * perhaps we should not even require them to have
	 * a timeout?
	 */
	int cache_ok = (nfs_is_mobile || NMFLAG(nmp, SOFT) ||
	    nfs_can_squish(nmp) || nmp->nm_deadtimeout);

	int timeoutmask = NFSSTA_TIMEO | NFSSTA_LOCKTIMEO | NFSSTA_JUKEBOXTIMEO;

	/*
	 * So if we have a timeout and we're not really a hard hard-mount,
	 * return 1 to not get things out of the cache.
	 */

	return (nmp->nm_state & timeoutmask) && cache_ok;
}

/*
 * Log a message that nfs or lockd server is unresponsive. Check if we
 * can be squished and if we can, or that our dead timeout has
 * expired, and we're not holding state, set our mount as dead, remove
 * our mount state and ask to be unmounted. If we are holding state
 * we're being called from the nfs_request_timer and will soon detect
 * that we need to unmount.
 */
void
nfs_down(struct nfsmount *nmp, thread_t thd, int error, int flags, const char *msg, int holding_state)
{
	int timeoutmask, wasunresponsive, unresponsive, softnobrowse;
	uint32_t do_vfs_signal = 0;
	struct timeval now;

	if (nfs_mount_gone(nmp)) {
		return;
	}

	lck_mtx_lock(&nmp->nm_lock);

	timeoutmask = NFSSTA_TIMEO | NFSSTA_LOCKTIMEO | NFSSTA_JUKEBOXTIMEO;
	if (NMFLAG(nmp, MUTEJUKEBOX)) { /* jukebox timeouts don't count as unresponsive if muted */
		timeoutmask &= ~NFSSTA_JUKEBOXTIMEO;
	}
	wasunresponsive = (nmp->nm_state & timeoutmask);

	/* XXX don't allow users to know about/disconnect unresponsive, soft, nobrowse mounts */
	softnobrowse = (NMFLAG(nmp, SOFT) && (vfs_flags(nmp->nm_mountp) & MNT_DONTBROWSE));

	if ((flags & NFSSTA_TIMEO) && !(nmp->nm_state & NFSSTA_TIMEO)) {
		nmp->nm_state |= NFSSTA_TIMEO;
	}
	if ((flags & NFSSTA_LOCKTIMEO) && !(nmp->nm_state & NFSSTA_LOCKTIMEO)) {
		nmp->nm_state |= NFSSTA_LOCKTIMEO;
	}
	if ((flags & NFSSTA_JUKEBOXTIMEO) && !(nmp->nm_state & NFSSTA_JUKEBOXTIMEO)) {
		nmp->nm_state |= NFSSTA_JUKEBOXTIMEO;
	}

	unresponsive = (nmp->nm_state & timeoutmask);

	nfs_is_squishy(nmp);

	if (unresponsive && (nmp->nm_curdeadtimeout > 0)) {
		microuptime(&now);
		if (!wasunresponsive) {
			nmp->nm_deadto_start = now.tv_sec;
			nfs_mount_sock_thread_wake(nmp);
		} else if ((now.tv_sec - nmp->nm_deadto_start) > nmp->nm_curdeadtimeout && !holding_state) {
			if (!(nmp->nm_state & NFSSTA_DEAD)) {
				printf("nfs server %s: %sdead\n", vfs_statfs(nmp->nm_mountp)->f_mntfromname,
				    (nmp->nm_curdeadtimeout != nmp->nm_deadtimeout) ? "squished " : "");
			}
			do_vfs_signal = VQ_DEAD;
		}
	}
	lck_mtx_unlock(&nmp->nm_lock);

	if (do_vfs_signal == VQ_DEAD && !(nmp->nm_state & NFSSTA_DEAD)) {
		nfs_mount_zombie(nmp, NFSSTA_DEAD);
	} else if (softnobrowse || wasunresponsive || !unresponsive) {
		do_vfs_signal = 0;
	} else {
		do_vfs_signal = VQ_NOTRESP;
	}
	if (do_vfs_signal) {
		vfs_event_signal(&vfs_statfs(nmp->nm_mountp)->f_fsid, do_vfs_signal, 0);
	}

	nfs_msg(thd, vfs_statfs(nmp->nm_mountp)->f_mntfromname, msg, error);
}

void
nfs_up(struct nfsmount *nmp, thread_t thd, int flags, const char *msg)
{
	int timeoutmask, wasunresponsive, unresponsive, softnobrowse;
	int do_vfs_signal;

	if (!nmp || nfs_mount_gone(nmp)) {
		return;
	}

	if (msg) {
		nfs_msg(thd, vfs_statfs(nmp->nm_mountp)->f_mntfromname, msg, 0);
	}

	lck_mtx_lock(&nmp->nm_lock);

	timeoutmask = NFSSTA_TIMEO | NFSSTA_LOCKTIMEO | NFSSTA_JUKEBOXTIMEO;
	if (NMFLAG(nmp, MUTEJUKEBOX)) { /* jukebox timeouts don't count as unresponsive if muted */
		timeoutmask &= ~NFSSTA_JUKEBOXTIMEO;
	}
	wasunresponsive = (nmp->nm_state & timeoutmask);

	/* XXX don't allow users to know about/disconnect unresponsive, soft, nobrowse mounts */
	softnobrowse = (NMFLAG(nmp, SOFT) && (vfs_flags(nmp->nm_mountp) & MNT_DONTBROWSE));

	if ((flags & NFSSTA_TIMEO) && (nmp->nm_state & NFSSTA_TIMEO)) {
		nmp->nm_state &= ~NFSSTA_TIMEO;
	}
	if ((flags & NFSSTA_LOCKTIMEO) && (nmp->nm_state & NFSSTA_LOCKTIMEO)) {
		nmp->nm_state &= ~NFSSTA_LOCKTIMEO;
	}
	if ((flags & NFSSTA_JUKEBOXTIMEO) && (nmp->nm_state & NFSSTA_JUKEBOXTIMEO)) {
		nmp->nm_state &= ~NFSSTA_JUKEBOXTIMEO;
	}

	unresponsive = (nmp->nm_state & timeoutmask);

	/* Reset dead timeout timers only if the state was changed from unresponsive to responsive */
	if (wasunresponsive && !unresponsive) {
		nmp->nm_deadto_start = 0;
		nmp->nm_curdeadtimeout = nmp->nm_deadtimeout;
		nmp->nm_state &= ~NFSSTA_SQUISHY;
	}

	lck_mtx_unlock(&nmp->nm_lock);

	if (softnobrowse) {
		do_vfs_signal = 0;
	} else {
		do_vfs_signal = (wasunresponsive && !unresponsive);
	}
	if (do_vfs_signal) {
		vfs_event_signal(&vfs_statfs(nmp->nm_mountp)->f_fsid, VQ_NOTRESP, 1);
	}
}
