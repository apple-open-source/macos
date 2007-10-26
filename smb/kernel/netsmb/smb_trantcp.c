/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#define ABSOLUTETIME_SCALAR_TYPE
#define APPLE_PRIVATE 1

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kpi_mbuf.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/smb_apple.h>

#include <sys/mchain.h>

#include <netsmb/netbios.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>
#include <netsmb/smb_subr.h>

#include <netsmb/smb_sleephandler.h>

#define M_NBDATA	M_PCB

static u_int32_t smb_tcpsndbuf = 1024 * 128;
static u_int32_t smb_tcprcvbuf = 1024 * 128;

SYSCTL_DECL(_net_smb_fs);
SYSCTL_INT(_net_smb_fs, OID_AUTO, tcpsndbuf, CTLFLAG_RW, &smb_tcpsndbuf, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, tcprcvbuf, CTLFLAG_RW, &smb_tcprcvbuf, 0, "");

static int  nbssn_recv(struct nbpcb *nbp, mbuf_t *mpp, int *lenp,
	u_int8_t *rpcodep, struct timespec *tsp);
static int  smb_nbst_disconnect(struct smb_vc *vcp);

static int
nb_setsockopt_int(socket_t so, int level, int name, int val)
{
	return (sock_setsockopt(so, level, name, &val, sizeof(val)));
}

static void
nb_upcall(socket_t so, void *arg, int waitflag)
{
	#pragma unused(so, waitflag)
	struct nbpcb *nbp = (struct nbpcb *)arg;

	/* sanity */
	if (arg == NULL)
		return;

	lck_mtx_lock(&nbp->nbp_lock);

	nbp->nbp_flags |= NBF_UPCALLED;
	/*
	 * If there's an upcall, pass it the selectid,
	 * otherwise wakeup on the selectid
	 */
	if (nbp->nbp_upcall) {
		nbp->nbp_upcall(nbp->nbp_selectid);
	} else if (nbp->nbp_selectid)
		wakeup(nbp->nbp_selectid);
	lck_mtx_unlock(&nbp->nbp_lock);
	return;
}

static int
nb_sethdr(mbuf_t m, u_int8_t type, u_int32_t len)
{
	u_int32_t *p = mbuf_data(m);

	*p = htonl((len & 0x1FFFF) | (type << 24));
	return (0);
}

static int
nb_put_name(struct mbchain *mbp, struct sockaddr_nb *snb)
{
	int error;
	u_char seglen, *cp;

	cp = snb->snb_name;
	if (*cp == 0)
		return (EINVAL);
	NBDEBUG("[%s]\n", cp);
	for (;;) {
		seglen = (*cp) + 1;
		error = mb_put_mem(mbp, (c_caddr_t)cp, seglen, MB_MSYSTEM);
		if (error)
			return (error);
		if (seglen == 1)
			break;
		cp += seglen;
	}
	return (0);
}

static int
nb_connect_in(struct nbpcb *nbp, struct sockaddr_in *to)
{
	struct sockaddr_in second_port = *to;
	socket_t second_so = NULL;
	socket_t so;
	int error;
	struct timeval  tv;
	struct smb_vc *vcp = nbp->nbp_vc;
	int tryboth = (vcp->connect_flag && (*(vcp->connect_flag) & NSMBFL_TRYBOTH)) ? TRUE : FALSE;
	
	error = sock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nb_upcall, nbp, &so);
	if (error)
		return (error);
	nbp->nbp_tso = so;
	tv.tv_sec = SMBSBTIMO;
	tv.tv_usec = 0;
	error = sock_setsockopt(so, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (error)
		goto bad;
	error = sock_setsockopt(so, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
	if (error)
		goto bad;

	do {
		error = sock_setsockopt(so, SOL_SOCKET, SO_SNDBUF, &nbp->nbp_sndbuf, sizeof(nbp->nbp_sndbuf));
		if (error) {
			nbp->nbp_sndbuf /= 2;
			SMBDEBUG("sock_setsockopt error = %d nbp_sndbuf = %d\n", error, nbp->nbp_sndbuf);
		}
	}while ((error) && (nbp->nbp_sndbuf > 1024));
	if (error)
		goto bad;

	do {
		error = sock_setsockopt(so, SOL_SOCKET, SO_RCVBUF, &nbp->nbp_rcvbuf, sizeof(nbp->nbp_rcvbuf));
		if (error) {
			nbp->nbp_sndbuf /= 2;
			SMBDEBUG("sock_setsockopt error = %d nbp_rcvbuf = %d\n", error, nbp->nbp_rcvbuf);
		}
	}while ((error) && (nbp->nbp_rcvbuf > 1024));
	if (error)
		goto bad;
	
	error = nb_setsockopt_int(so, SOL_SOCKET, SO_KEEPALIVE, 1);
	if (error)
		goto bad;
	error = nb_setsockopt_int(so, IPPROTO_TCP, TCP_NODELAY, 1);
	if (error)
		goto bad;
	
	/* set SO_NOADDRERR to detect network changes ASAP */
	error = nb_setsockopt_int(so, SOL_SOCKET, SO_NOADDRERR, 1);
	if (error)	/* Should we error out if this fails? */
		goto bad;
	
	error = sock_nointerrupt(so, 0);
	if (error)
		goto bad;
	error = sock_connect(so, (struct sockaddr*)to, MSG_DONTWAIT);
	if (error && error != EINPROGRESS)
		goto bad;
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	while ((error = sock_connectwait(so, &tv)) == EINPROGRESS) {
		if ((error = smb_iod_nb_intr(nbp->nbp_vc)))
			break;
		/*
		 * We need to try both ports, we wait 2 seconds and then
		 * try connecting with port 139. First one to connect wins.
		 * We always give the first port more time since that is the
		 * prefered method of connecting.
		 */
		if (tryboth) {
			second_port.sin_port = htons(NBSS_TCP_PORT_139);
			error = sock_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, &second_so);
			if (!error) 
				error = sock_nointerrupt(second_so, 0);
			if (!error)
				error = sock_connect(second_so, (struct sockaddr*)&second_port, MSG_DONTWAIT);
			/* Done with setup we either have a socket or we don't */
			tryboth = FALSE; 
			/* We failed clean up and just try the first connect */ 
			if (error && error != EINPROGRESS) {
				if (second_so)
					sock_close(second_so);
				second_so = NULL;
			} else
				tv.tv_sec = 1;
		} else  if (second_so && (sock_connectwait(second_so, &tv) == 0)) {
		    	/* We got connect on port 139 tell userland */
			error = ECONNREFUSED;
			break;
		}
	}
	/* If we have a second connection close it. */
	if (second_so)
		sock_close(second_so);
	if (!error)
		return (0);
bad:
	smb_nbst_disconnect(nbp->nbp_vc);
	return (error);
}

static int
nbssn_rq_request(struct nbpcb *nbp)
{
	struct mbchain mb, *mbp = &mb;
	struct mdchain md, *mdp = &md;
	mbuf_t m0;
	struct sockaddr_in sin;
	u_short port;
	u_int8_t rpcode;
	int error, rplen;

	error = mb_init(mbp);
	if (error)
		return (error);
	mb_put_uint32le(mbp, 0);
	nb_put_name(mbp, nbp->nbp_paddr);
	nb_put_name(mbp, nbp->nbp_laddr);
	nb_sethdr(mbp->mb_top, NB_SSN_REQUEST, mb_fixhdr(mbp) - 4);
	error = sock_sendmbuf(nbp->nbp_tso, NULL, (mbuf_t)mbp->mb_top, 0, NULL);
	if (!error)
		nbp->nbp_state = NBST_RQSENT;
	mb_detach(mbp);
	mb_done(mbp);
	if (error)
		return (error);
	error = nbssn_recv(nbp, &m0, &rplen, &rpcode, &nbp->nbp_timo);
	if (error == EWOULDBLOCK) {	/* Timeout */
		NBDEBUG("initial request timeout\n");
		return (ETIMEDOUT);
	}
	if (error) {
		NBDEBUG("recv() error %d\n", error);
		return (error);
	}
	/*
	 * Process NETBIOS reply
	 */
	if (m0)
		md_initm(mdp, m0);
	error = 0;
	do {
		if (rpcode == NB_SSN_POSRESP) {
			lck_mtx_lock(&nbp->nbp_lock);
			nbp->nbp_state = NBST_SESSION;
			nbp->nbp_flags |= NBF_CONNECTED;
			lck_mtx_unlock(&nbp->nbp_lock);
			break;
		}
		if (rpcode != NB_SSN_RTGRESP) {
			error = ECONNABORTED;
			break;
		}
		if (rplen != 6) {
			error = ECONNABORTED;
			break;
		}
		md_get_mem(mdp, (caddr_t)&sin.sin_addr, 4, MB_MSYSTEM);
		md_get_uint16(mdp, &port);
		sin.sin_port = port;
		nbp->nbp_state = NBST_RETARGET;
		smb_nbst_disconnect(nbp->nbp_vc);
		error = nb_connect_in(nbp, &sin);
		if (!error)
			error = nbssn_rq_request(nbp);
		if (error) {
			smb_nbst_disconnect(nbp->nbp_vc);
			break;
		}
	} while(0);
	if (m0)
		md_done(mdp);
	return (error);
}

static int
nbssn_recvhdr(struct nbpcb *nbp, int *lenp,
	u_int8_t *rpcodep, u_int64_t abstime)
{
	struct iovec aio;
	u_int32_t len;
	u_int8_t *bytep;
	int error;
	size_t resid, recvdlen;
	struct msghdr msg;
	struct smbiod *iod = nbp->nbp_vc->vc_iod;

	resid = sizeof(len);
	bytep = (u_int8_t *)&len;
	while (resid != 0) {
		aio.iov_base = bytep;
		aio.iov_len = resid;
		bzero(&msg, sizeof(msg));
		msg.msg_iov = &aio;
		msg.msg_iovlen = 1;
		/*
		 * We don't wait for all the data to be available; we
		 * want to know when the first bit of data arrives, so
		 * we can get the network-layer round-trip time, which
		 * we use as the basis for other timeouts.  (Note
		 * that even if we *did* wait for all the data to
		 * be available, we still might not get all the data,
		 * as there might be a timeout, e.g. due to the machine
		 * going to sleep and waking up later.)
		 */
		error = sock_receive(nbp->nbp_tso, &msg, MSG_DONTWAIT,
				     &recvdlen);
		if (error) {
			/*
			 * If we've gotten a socket timeout, and if we were
			 * given a deadline (which is presumably further in
			 * the future than the socket receive timeout, so
			 * that we'd get a socket receive timeout before the
			 * deadline expires), check whether we've been
			 * upcalled (to deliver more data) and, if not,
			 * wait for the deadline to expire or for an
			 * upcall with more data.
			 */
			if (error == EWOULDBLOCK && abstime) {
				lck_mtx_lock(&nbp->nbp_lock);
				if (!(nbp->nbp_flags & NBF_UPCALLED)) {
					msleep1(&iod->iod_flags,
					    &nbp->nbp_lock, PWAIT, "nbssn",
					    abstime);

					/*
					 * The only reason for the deadline
					 * is to get an initial round-trip
					 * time estimate, so we only want
					 * the deadline for the first sleep
					 * (note that the deadline might
					 * have passed by the time the first
					 * sleep finishes).
					 */
					abstime = 0;
				}
				lck_mtx_unlock(&nbp->nbp_lock);
				continue;
			}
			if (error == EWOULDBLOCK && resid != sizeof(len))
				continue;
			return (error);
		}
		if (recvdlen > resid) {
			/* This "shouldn't happen" */
			SMBERROR("got more data than we asked for!\n");
			return (EPIPE);
		}
		if (!sock_isconnected(nbp->nbp_tso)) {
			nbp->nbp_state = NBST_CLOSED;
			NBDEBUG("session closed by peer\n");
			return (ECONNRESET);
		}
		if (recvdlen == 0) {
			/* This isn't always a bad thing, so make it a warning only */
			SMBWARNING("connection closed out from under us\n");
			return (EPIPE);
		}
		resid -= recvdlen;
		bytep += recvdlen;
	}
	len = ntohl(len);
	if ((len >> 16) & 0xFE) {
		SMBERROR("bad nb header received 0x%x (MBZ flag set)\n", len);
		return (EPIPE);
	}
	*rpcodep = (len >> 24) & 0xFF;
	switch (*rpcodep) {
	    case NB_SSN_MESSAGE:
	    case NB_SSN_REQUEST:
	    case NB_SSN_POSRESP:
	    case NB_SSN_NEGRESP:
	    case NB_SSN_RTGRESP:
	    case NB_SSN_KEEPALIVE:
		break;
	    default:
		SMBERROR("bad nb header received 0x%x (bogus type)\n", len);
		return (EPIPE);
	}
	len &= 0x1ffff;
	if (len > SMB_MAXPKTLEN) {
		SMBERROR("packet too long (%d)\n", len);
		return (EFBIG);
	}
	*lenp = len;
	return (0);
}

static int
nbssn_recv(struct nbpcb *nbp, mbuf_t *mpp, int *lenp,
	u_int8_t *rpcodep, struct timespec *tsp)
{
	socket_t so = nbp->nbp_tso;
	mbuf_t m;
	mbuf_t tm;
	u_int8_t rpcode;
	int len, resid, error;
	size_t recvdlen;
	uint64_t	abstime = 0;

	if (so == NULL)
		return (ENOTCONN);
	if (mpp)
		*mpp = NULL;
	m = NULL;
	if (tsp) {
		nanoseconds_to_absolutetime(tsp->tv_nsec +
					    NSEC_PER_SEC*(uint64_t)tsp->tv_sec,
					    &abstime);
		clock_absolutetime_interval_to_deadline(abstime, &abstime);
	}
	for(;;) {
		/*
		 * Read the response header.
		 */
		lck_mtx_lock(&nbp->nbp_lock);
		nbp->nbp_flags &= ~NBF_UPCALLED;
		lck_mtx_unlock(&nbp->nbp_lock);
		error = nbssn_recvhdr(nbp, &len, &rpcode, abstime);
		if (error)
			return (error);

		/*
		 * Warn about keepalives with data - they're not
		 * supposed to have any, and if we have a problem
		 * at this point, perhaps the length didn't
		 * reflect the actual data in the packet.
		 */
		if (rpcode == NB_SSN_KEEPALIVE && len != 0) {
			SMBERROR("Keepalive received with non-zero length %d\n",
			    len);
		}

		/*
		 * Loop, blocking, for data following the response header,
		 * if any.
		 *
		 * Note that we can't simply block here with MSG_WAITALL for the
		 * entire response size, as it may be larger than the TCP
		 * slow-start window that the sender employs.  This will result
		 * in the sender stalling until the delayed ACK is sent, then
		 * resuming slow-start, resulting in very poor performance.
		 *
		 * Instead, we never request more than NB_SORECEIVE_CHUNK
		 * bytes at a time, resulting in an ack being pushed by
		 * the TCP code at the completion of each call.
		 */
		resid = len;
		while (resid > 0) {
			struct timespec tstart, tend;
			tm = NULL;
			/*
			 * We use to spin until we got a hard error, we no longer wait forever.
			 * We now limit how long we will block receiving any message. This timer only 
			 * starts after we have read the 4 byte header length field. We then try to read
			 * the data in 8K chunks, if any read takes longer that 15 seconds we break the
			 * connection and give up. If we went to sleep then we reset our start timer to when we
			 * woke up. Now for the reason behind the fix. We have the message length, but looks 
			 * like only part of the message has made it in. We went to sleep and the server 
			 * broke the connect while we were still a sleep. Looks like we got the first 
			 * ethernet packet but not the rest. We are in a loop waiting for the rest of the 
			 * message. Since we can't send in this state there is no way for us to know that 
			 * the connect is really down. 
			 */
			nanotime(&tstart);
			do {
				recvdlen = min(resid, NB_SORECEIVE_CHUNK);
				error = sock_receivembuf(so, NULL, &tm, MSG_WAITALL, &recvdlen);
				if (error == EAGAIN) {
					nanotime(&tend);
					/* We fell asleep reset our timer to the wake up timer */
					if (tstart.tv_sec < gWakeTime.tv_sec)
						tstart.tv_sec = gWakeTime.tv_sec;
						/* Ok we have tried hard enough just break the connection and give up. */
					if (tend.tv_sec > (tstart.tv_sec + SMB_SB_RCVTIMEO)) {
						error = EPIPE;					
						SMBERROR("Breaking connection, sock_receivembuf blocked for %d\n", (int)(tend.tv_sec - tstart.tv_sec));
					}
				}
			} while ((error == EAGAIN) || (error == EINTR) || (error == ERESTART));
			if (error)
				goto out;
			if (recvdlen > resid) {
				/* This "shouldn't happen" */
				SMBERROR("got more data than we asked for!\n");
				error = EPIPE;
				goto out;
			}
			resid -= recvdlen;
			/* append received chunk to previous chunk(s) */
			if (!m) {
				m = (mbuf_t )tm;
			} else {
				/*
				 * Just glue the new chain on the end.
				 * Consumer will pullup as required.
				 */
				mbuf_cat_internal(m, (mbuf_t )tm);
			}
		}

		/*
		 * If it's a keepalive, discard any data in it
		 * (there's not supposed to be any, but that
		 * doesn't mean some server won't send some)
		 * and get the next packet.
		 */
		if (rpcode == NB_SSN_KEEPALIVE) {
			if (m) {
				mbuf_freem(m);
				m = NULL;
			}
			continue;
		}

		if (nbp->nbp_state != NBST_SESSION) {
			/*
			 * No session is established.
			 * Return whatever packet we got.
			 */
			break;
		}

		/*
		 * A session is established; the only packets
		 * we should see are session message and
		 * keep-alive packets.
		 */
		if (rpcode == NB_SSN_MESSAGE) {
			/*
			 * Session message.  Does it have any data?
			 */
			if (!m) {
				/*
				 * No - complain and continue.
				 */
				SMBERROR("empty session packet\n");
				continue;
			}

			/*
			 * Yes - return it to our caller.
			 */
			break;
		}

		/*
		 * Ignore other types of packets - drop packet
		 * and try for another.
		 */
		SMBERROR("non-session packet %x\n", rpcode);
		if (m) {
			mbuf_freem(m);
			m = NULL;
		}
	}
out:
	if (error) {
		if (m)
			mbuf_freem(m);
		return (error);
	}
	if (mpp)
		*mpp = m;
	else
		mbuf_freem(m);
	*lenp = len;
	*rpcodep = rpcode;
	return (0);
}

/*
 * SMB transport interface
 */
static int
smb_nbst_create(struct smb_vc *vcp)
{
	struct nbpcb *nbp;

	MALLOC(nbp, struct nbpcb *, sizeof *nbp, M_NBDATA, M_WAITOK);
	bzero(nbp, sizeof *nbp);
	nbp->nbp_timo.tv_sec = SMB_NBTIMO;
	nbp->nbp_state = NBST_CLOSED;
	nbp->nbp_vc = vcp;
	nbp->nbp_sndbuf = smb_tcpsndbuf;
	nbp->nbp_rcvbuf = smb_tcprcvbuf;
	lck_mtx_init(&nbp->nbp_lock, nbp_lck_group, nbp_lck_attr);
	vcp->vc_tdata = nbp;
	return (0);
}

static int
smb_nbst_done(struct smb_vc *vcp)
{
	struct nbpcb *nbp = vcp->vc_tdata;

	if (nbp == NULL)
		return (ENOTCONN);
	smb_nbst_disconnect(vcp);
	if (nbp->nbp_laddr)
		free(nbp->nbp_laddr, M_SONAME);
	if (nbp->nbp_paddr)
		free(nbp->nbp_paddr, M_SONAME);
	lck_mtx_destroy(&nbp->nbp_lock, nbp_lck_group);
	free(nbp, M_NBDATA);
	vcp->vc_tdata = NULL;
	return (0);
}

static int
smb_nbst_bind(struct smb_vc *vcp, struct sockaddr *sap)
{
	struct nbpcb *nbp = vcp->vc_tdata;
	struct sockaddr_nb *snb;
	int error, slen;

	NBDEBUG("\n");
	error = EINVAL;
	do {
		if (nbp->nbp_flags & NBF_LOCADDR)
			break;
		/*
		 * It is possible to create NETBIOS name in the kernel,
		 * but nothing prevents us to do it in the user space.
		 */
		if (sap == NULL)
			break;
		slen = sap->sa_len;
		if (slen < (int)NB_MINSALEN)
			break;
		snb = (struct sockaddr_nb*)smb_dup_sockaddr(sap, 1);
		if (snb == NULL) {
			error = ENOMEM;
			break;
		}
		lck_mtx_lock(&nbp->nbp_lock);
		nbp->nbp_laddr = snb;
		nbp->nbp_flags |= NBF_LOCADDR;
		lck_mtx_unlock(&nbp->nbp_lock);
		error = 0;
	} while(0);
	return (error);
}

static int
smb_nbst_connect(struct smb_vc *vcp, struct sockaddr *sap)
{
	struct nbpcb *nbp = vcp->vc_tdata;
	struct sockaddr_in sin;
	struct sockaddr_nb *snb;
	struct timespec ts1, ts2;
	int error, slen;

	NBDEBUG("\n");
	if (nbp == NULL)
		return (EINVAL);
	if (nbp->nbp_tso != NULL)
		return (EISCONN);
	if (nbp->nbp_laddr == NULL)
		return (EINVAL);
	slen = sap->sa_len;
	if (slen < (int)NB_MINSALEN)
		return (EINVAL);
	if (nbp->nbp_paddr) {
		free(nbp->nbp_paddr, M_SONAME);
		nbp->nbp_paddr = NULL;
	}
	snb = (struct sockaddr_nb*)smb_dup_sockaddr(sap, 1);
	if (snb == NULL)
		return (ENOMEM);
	nbp->nbp_paddr = snb;
	sin = snb->snb_addrin;
	/*
	 * For our general timeout we use the greater of
	 * the default (15 sec) and 4 times the time it
	 * took for the first round trip.  We used to use
	 * just the latter, but sometimes if the first 
	 * round trip is very fast the subsequent 4 sec
	 * timeouts are simply too short.
	 */
	nanotime(&ts1);
	error = nb_connect_in(nbp, &sin);
	if (error)
		return (error);
	nanotime(&ts2);
	timespecsub(&ts2, &ts1);
	timespecadd(&ts2, &ts2);
	timespecadd(&ts2, &ts2);	/*  * 4 */
	if (timespeccmp(&ts2, &nbp->nbp_timo, >))
		nbp->nbp_timo = ts2;
	/* If its not port 139, then we don't need to do a NetBIOS session connect */
	if (betohs(((struct sockaddr_in*)(&sap->sa_data[2]))->sin_port) != NBSS_TCP_PORT_139)
		nbp->nbp_state = NBST_SESSION;
	else {
		error = nbssn_rq_request(nbp);
		if (error)
			smb_nbst_disconnect(vcp);
	}
	return (error);
}

static int
smb_nbst_disconnect(struct smb_vc *vcp)
{
	struct nbpcb *nbp = vcp->vc_tdata;
	socket_t so;

	if (nbp == NULL || nbp->nbp_tso == NULL)
		return (ENOTCONN);
	if ((so = nbp->nbp_tso) != NULL) {
		lck_mtx_lock(&nbp->nbp_lock);
		nbp->nbp_flags &= ~NBF_CONNECTED;
		nbp->nbp_tso = (socket_t) NULL;
		lck_mtx_unlock(&nbp->nbp_lock);
		sock_shutdown(so, 2);
		sock_close(so);
	}
	if (nbp->nbp_state != NBST_RETARGET) {
		nbp->nbp_state = NBST_CLOSED;
	}
	return (0);
}

static int
smb_nbst_send(struct smb_vc *vcp, mbuf_t m0)
{
	struct nbpcb *nbp = vcp->vc_tdata;
	int error;

	/* Should never happen, but just in case */
	DBG_ASSERT(nbp);
	if ((nbp == NULL) || (nbp->nbp_state != NBST_SESSION)) {
		error = ENOTCONN;
		goto abort;
	}
	if (mbuf_prepend(&m0, 4, MBUF_WAITOK))
		return (ENOBUFS);
	nb_sethdr(m0, NB_SSN_MESSAGE, m_fixhdr(m0) - 4);
	error = sock_sendmbuf(nbp->nbp_tso, NULL, (mbuf_t)m0, 0, NULL);
	return (error);
abort:
	if (m0)
		mbuf_freem(m0);
	return (error);
}


static int
smb_nbst_recv(struct smb_vc *vcp, mbuf_t *mpp)
{
	struct nbpcb *nbp = vcp->vc_tdata;
	u_int8_t rpcode;
	int error, rplen;

	/* Should never happen, but just in case */
	DBG_ASSERT(nbp);
	if (nbp == NULL)
		return (ENOTCONN);

	lck_mtx_lock(&nbp->nbp_lock);
	if (nbp->nbp_flags & NBF_RECVLOCK) {
		SMBERROR("attempt to reenter session layer!\n");
		lck_mtx_unlock(&nbp->nbp_lock);
		return (EWOULDBLOCK);
	}
	nbp->nbp_flags |= NBF_RECVLOCK;
	lck_mtx_unlock(&nbp->nbp_lock);

	error = nbssn_recv(nbp, mpp, &rplen, &rpcode, NULL);

	lck_mtx_lock(&nbp->nbp_lock);
	nbp->nbp_flags &= ~NBF_RECVLOCK;
	lck_mtx_unlock(&nbp->nbp_lock);
	return (error);
}

static void
smb_nbst_timo(struct smb_vc *vcp)
{
	#pragma unused(vcp)
	return;
}

static int
smb_nbst_getparam(struct smb_vc *vcp, int param, void *data)
{
	struct nbpcb *nbp = vcp->vc_tdata;

	/* Should never happen, but just in case */
	DBG_ASSERT(nbp);
	if (nbp == NULL)
		return (EINVAL);		
	switch (param) {
	    case SMBTP_SNDSZ:
		*(u_int32_t*)data = nbp->nbp_sndbuf;
		break;
	    case SMBTP_RCVSZ:
		*(u_int32_t*)data = nbp->nbp_rcvbuf;
		break;
	    case SMBTP_TIMEOUT:
		*(struct timespec*)data = nbp->nbp_timo;
		break;
	    case SMBTP_SELECTID:
		*(void **)data = nbp->nbp_selectid;
		break;
	    case SMBTP_UPCALL:
		*(void **)data = nbp->nbp_upcall;
		break;
	    default:
		return (EINVAL);
	}
	return (0);
}

static int
smb_nbst_setparam(struct smb_vc *vcp, int param, void *data)
{
	struct nbpcb *nbp = vcp->vc_tdata;

	/* Should never happen, but just in case */
	DBG_ASSERT(nbp);
	if (nbp == NULL)
		return (EINVAL);		
	switch (param) {
	    case SMBTP_SELECTID:
		nbp->nbp_selectid = data;
		break;
	    case SMBTP_UPCALL:
		nbp->nbp_upcall = data;
		break;
	    default:
		return (EINVAL);
	}
	return (0);
}

/*
 * Check for fatal errors
 */
static int
smb_nbst_fatal(struct smb_vc *vcp, int error)
{
	struct nbpcb *nbp;

	switch (error) {
	    case ENOTCONN:
	    case ENETRESET:
	    case ECONNABORTED:
	    case EPIPE:
		case EADDRNOTAVAIL:
		return 1;
	}
	DBG_ASSERT(vcp);
	nbp = vcp->vc_tdata;
	if ((nbp == NULL) || (nbp->nbp_tso == NULL) || (! sock_isconnected(nbp->nbp_tso)))
			return 1;
	
	return (0);
}

struct smb_tran_desc smb_tran_nbtcp_desc = {
	SMBT_NBTCP,
	smb_nbst_create, smb_nbst_done,
	smb_nbst_bind, smb_nbst_connect, smb_nbst_disconnect,
	smb_nbst_send, smb_nbst_recv,
	smb_nbst_timo,
	smb_nbst_getparam, smb_nbst_setparam,
	smb_nbst_fatal,
	{NULL, NULL}
};
