/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2012 Apple Inc. All rights reserved.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kpi_mbuf.h>
#include <sys/proc.h>
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
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>
#include <netsmb/smb_subr.h>
#include <smbfs/smbfs.h>

#include <netsmb/smb_sleephandler.h>

#define M_NBDATA	M_PCB

/* 
 * Set smb_tcpsndbuf and smb_tcprcvbuf for 2 MB now that we dont set SO_RCVBUF
 * or SO_SNDBUF anymore. Previous code was fine with getting 2 MB...
 *
 * smb_tcpsndbuf gets stored in nbp->nbp_sndbuf where its used for calculating
 * max write and transaction size
 *
 * smb_tcprcvbuf gets stored in nbp->nbp_rcvbuf and nbp->nbp_rcvchunk.
 * nbp_rcvbuf is used for calculating max read size.
 * nbp_rcvchunk is the max read size used when reading data from the TCP socket
 */
static uint32_t smb_tcpsndbuf = 2 * 1024 * 1024;
static uint32_t smb_tcprcvbuf = 2 * 1024 * 1024;

SYSCTL_DECL(_net_smb_fs);
SYSCTL_INT(_net_smb_fs, OID_AUTO, tcpsndbuf, CTLFLAG_RW, &smb_tcpsndbuf, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, tcprcvbuf, CTLFLAG_RW, &smb_tcprcvbuf, 0, "");

static int nbssn_recv(struct nbpcb *nbp, mbuf_t *mpp, int *lenp, uint8_t *rpcodep,
					  struct timespec *wait_time);
static int  smb_nbst_disconnect(struct smbiod *iod);

static int
nb_setsockopt_int(socket_t so, int level, int name, int val)
{
	return (sock_setsockopt(so, level, name, &val, (int)sizeof(val)));
}

static int
nb_sethdr(struct nbpcb *nbp, mbuf_t m, uint8_t type, uint32_t len)
{
	uint32_t *p = mbuf_data(m);

	if (nbp->nbp_flags & NBF_NETBIOS) {
		/* NetBIOS connection the length field is 17 bits */
		*p = htonl((len & SMB_MAXPKTLEN) | (type << 24));
	} else {
		/* NetBIOS-less connection the length field is 24 bits */
		*p = htonl((len & SMB_LARGE_MAXPKTLEN) | (type << 24));
	}

	return (0);
}

/*
 * encoded format is one or more of the following block:
 * | 1 byte: <seg_len> | <seg_len> - 1 characters |
 */
static int
nb_validate_name(struct sockaddr_nb *snb) {
    uint8_t total_bytes = 0;
    u_char seglen, *cp;

    if (!snb) {
        return EINVAL;
    }
    
    cp = snb->snb_name;

    if (*cp == 0)
        return (EINVAL);
    
    while(1) {
        if (*cp == 0) {
            return (0);
        }
        seglen = (*cp) + 1;
        total_bytes += seglen;
        if ((seglen == 0) || (total_bytes > sizeof(snb->snb_name))) {
            /* (seglen == 0) means there was a uint8_t overflow and
             * (seglen = 256 > sizeof(snb->snb_name))
             */
            SMBERROR("snb_name encoding exceeds expected length");
            return (EINVAL);
        }
        cp += seglen;
    }
    return (0);
}

static int
nb_put_name(struct mbchain *mbp, struct sockaddr_nb *snb)
{
	int error;
	u_char seglen, *cp;

    error = nb_validate_name(snb);
    if (error) {
        return (error);
    }
	cp = snb->snb_name;

	NBDEBUG("[%s]\n", cp);
	for (;;) {
		seglen = (*cp) + 1;
		error = mb_put_mem(mbp, (const char *)cp, seglen, MB_MSYSTEM);
		if (error)
			return (error);
		if (seglen == 1)
			break;
		cp += seglen;
	}
	return (0);
}

static int tcp_connect(struct nbpcb *nbp, struct sockaddr *to, struct timeval timeout)
{
	socket_t so;
	int error;
	struct timeval  tv;
	
    if (!nbp->nbp_iod) {
        SMBERROR("no nbp_iod. Abort.\n");
        return ENOENT;
    }

    error = sock_socket(to->sa_family, SOCK_STREAM, IPPROTO_TCP, NULL, nbp, &so);

	if (error)
		return (error);
    
    lck_mtx_lock(&nbp->nbp_iod->iod_tdata_lock);
    nbp->nbp_flags |= NBF_SOCK_OPENED;
    lck_mtx_unlock(&nbp->nbp_iod->iod_tdata_lock);

	nbp->nbp_tso = so;
	tv.tv_sec = SMBSBTIMO;
	tv.tv_usec = 0;

    /*
     * Do not set SO_RCVTIMEO as that slows down read performance in
     * 10 gigE, non jumbo frame setups.
     */

    /*
     * Setting SO_SNDTIMEO means that a blocking send will return with
     * EWOULDBLOCK after that amount of time instead of blocking forever.
     */
	error = sock_setsockopt(so, SOL_SOCKET, SO_SNDTIMEO, &tv, (int)sizeof(tv));
    if (error) {
		goto bad;
	}

    if (nbp->nbp_flags & NBF_BOUND_IF) {
        if (to->sa_family == AF_INET6) {
            error = sock_setsockopt(so, IPPROTO_IPV6, IPV6_BOUND_IF, &nbp->nbp_if_idx, sizeof(nbp->nbp_if_idx));
        } else {
            error = sock_setsockopt(so, IPPROTO_IP, IP_BOUND_IF, &nbp->nbp_if_idx, sizeof(nbp->nbp_if_idx));
        }
        if (error) {
            goto bad;
        }
    }

    /*
     * <14422729> We no longer set the SO_RCV_BUF and SO_SNDBUF sizes as that
     * disables the TCP auto windows scaling.
	 */

	/* ??? Max size we want to read off the buffer at a time, always half the socket size */
	//nbp->nbp_rcvchunk = nbp->nbp_rcvbuf / 2;
	nbp->nbp_rcvchunk = nbp->nbp_rcvbuf;
	
	/* Never let it go below 8K */
	if (nbp->nbp_rcvchunk < NB_SORECEIVE_CHUNK) {
		nbp->nbp_rcvchunk = NB_SORECEIVE_CHUNK;
    }
	
	if ((nbp->nbp_sndbuf < smb_tcpsndbuf) ||
		(nbp->nbp_rcvbuf < smb_tcprcvbuf)) {
		SMBWARNING("nbp_rcvbuf = %d nbp_sndbuf = %d\n", nbp->nbp_rcvbuf, nbp->nbp_sndbuf);
	}
	
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
    
    if (timeout.tv_sec) {
        error = sock_setsockopt(so, IPPROTO_TCP, TCP_CONNECTIONTIMEOUT,
                                &timeout, (int)sizeof(timeout));
        if (error)
            goto bad;
    }

	error = sock_nointerrupt(so, 0);
	if (error)
		goto bad;
    
	error = sock_connect(so, (struct sockaddr*)to, MSG_DONTWAIT);
	if (error && error != EINPROGRESS)
		goto bad;
    
	tv.tv_sec = 2;
	tv.tv_usec = 0;
	while ((error = sock_connectwait(so, &tv)) == EINPROGRESS) {
		if ((error = smb_iod_nb_intr(nbp->nbp_iod)))
			break;

        if (nbp->nbp_iod->iod_flags & SMBIOD_SHUTDOWN){
            error = EINTR;
            break;
        }
        
        /* <71930272> check if new connection trial was aborted */
        if (nbp->nbp_iod->iod_flags & SMBIOD_ABORT_CONNECT){
            error = EINTR;
            break;
        }

	}
    if (!error)
        return (0);

bad:
	smb_nbst_disconnect(nbp->nbp_iod);
	return (error);
}

static int
nbssn_rq_request(struct nbpcb *nbp, struct timeval connection_to)
{
	struct mbchain mb, *mbp = &mb;
	struct mdchain md, *mdp = &md;
	mbuf_t m0;
	struct sockaddr_in sin;
	u_short port;
	uint8_t rpcode;
	int error, rplen;

	error = mb_init(mbp);
	if (error)
		return (error);
	mb_put_uint32le(mbp, 0);
	error = nb_put_name(mbp, nbp->nbp_paddr);
    if (error) {
        return error;
    }
	error = nb_put_name(mbp, nbp->nbp_laddr);
    if (error) {
        return error;
    }
	nb_sethdr(nbp, mbp->mb_top, NB_SSN_REQUEST, (uint32_t)(mb_fixhdr(mbp) - 4));
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
            if (!nbp->nbp_iod) {
                SMBERROR("no nbp_iod. This should not happen. \n");
                error = ENOENT;
                break;
            }
			lck_mtx_lock(&nbp->nbp_iod->iod_tdata_lock);
			nbp->nbp_state = NBST_SESSION;
			nbp->nbp_flags |= NBF_CONNECTED;
			lck_mtx_unlock(&nbp->nbp_iod->iod_tdata_lock);
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
		smb_nbst_disconnect(nbp->nbp_iod);

		error = tcp_connect(nbp, (struct sockaddr *)&sin, connection_to);
		if (!error)
			error = nbssn_rq_request(nbp, connection_to);
		if (error) {
			smb_nbst_disconnect(nbp->nbp_iod);
			break;
		}
	} while(0);
	if (m0)
		md_done(mdp);
	return (error);
}

static int nbssn_recvhdr(struct nbpcb *nbp, uint32_t *lenp, uint8_t *rpcodep,
						 struct timespec *wait_time)
{
	struct iovec aio;
	uint32_t len;
	uint8_t *bytep;
	int error;
	size_t resid, recvdlen;
	struct msghdr msg;
	int flags = MSG_WAITALL;
	resid = sizeof(len);
	bytep = (uint8_t *)&len;
	while (resid != 0) {
		aio.iov_base = bytep;
		aio.iov_len = resid;
		bzero(&msg, sizeof(msg));
		msg.msg_iov = &aio;
		msg.msg_iovlen = 1;
		/*
		 * We are trying to read the NetBIOS header which is 4 bytes long.
		 * Call sock_receive() with flag to be MSG_WAITALL.
         *
         * Note that SO_RCVTIMEO is no longer set, so this will block
         * forever.
		 */
		error = sock_receive(nbp->nbp_tso, &msg, flags, &recvdlen);
        if (error == 0) {
            /*
             * If we didn't get an error and recvdlen is zero then we have
             * reached EOF. So the socket has the SS_CANTRCVMORE flag set. This
             * means the other side has closed their side of the connection.
             */
            if ((recvdlen == 0) && resid) {
                SMBERROR("id %d flags 0x%x Server closed their side of the connection.\n",
                         (nbp->nbp_iod) ? nbp->nbp_iod->iod_id : -1,
                         (nbp->nbp_iod) ? nbp->nbp_iod->iod_flags : -1);
                nbp->nbp_state = NBST_CLOSED;
                error = EPIPE;
                return error;
            }
            /* This should never happen, someday should we make it just a debug assert. */
            if ((recvdlen > resid)) {
                SMBERROR("Got more data than we asked for!\n");
                error = EPIPE;
                return error;
            }
        }
        else {
            if (error != EWOULDBLOCK){
                SMBERROR("sock_receive error %d \n", error);
            }

            /*
             * If we have wait_time then we want to wait here for some amount of time
             * that is determined by wait_time.
             */
            if ((error == EWOULDBLOCK) && wait_time) {
                wait_time = 0;    /* We are suppose to have some data waiting by now */
                flags = MSG_WAITALL; /* Wait for all four bytes */
                continue;
            }
            /*
             * Check if the connect got closed.
             * <78410582> sock_isconnected must not be called if the socket was closed
             */
            if (!nbp->nbp_iod) {
                SMBERROR("no nbp_iod! something went awefully wrong \n");
                return ENOENT;
            }
            lck_mtx_lock(&nbp->nbp_iod->iod_tdata_lock);
            if (((nbp->nbp_flags & NBF_SOCK_OPENED) == 0) || (!sock_isconnected(nbp->nbp_tso))) {
                nbp->nbp_state = NBST_CLOSED;
                SMBERROR("session closed by peer\n");
                error = EPIPE;
            }
            lck_mtx_unlock(&nbp->nbp_iod->iod_tdata_lock);
            if ((error == EWOULDBLOCK) && resid && (resid < sizeof(len))) {
                SMBERROR("Timed out reading the nbt header: missing %ld bytes\n", resid);
            }
            return error;
        }

		/* 
		 * At this point we have received some data, reset the flag to wait. We got
		 * part of the 4 byte length field only wait 5 seconds to get the rest.
		 */
        flags = MSG_WAITALL;
		resid -= recvdlen;
		bytep += recvdlen;
	}

	/*
	 * From http://support.microsoft.com/kb/204279
	 *
	 * Direct hosted "NetBIOS-less" SMB traffic uses port 445 (TCP and UDP). In 
	 * this situation, a four-byte header precedes the SMB traffic. The first 
	 * byte of this header is always 0x00, and the next three bytes are the 
	 * length of the remaining data.
	 */
	len = ntohl(len);
	*rpcodep = (len >> 24) & 0xFF; /* For port 445 this should be zero, NB_SSN_MESSAGE */
	if (nbp->nbp_flags & NBF_NETBIOS) {
		/* Port 139, we can only use the first 17 bits for the length */
		if ((len >> 16) & 0xFE) {
			SMBERROR("bad nb header received 0x%x (MBZ flag set)\n", len);
			return (EPIPE);
		}
		len &= SMB_MAXPKTLEN;
	}
    else {
		/* "NetBIOS-less", we can only use the frist 24 bits for the length */
		len &= SMB_LARGE_MAXPKTLEN;
	}
	switch (*rpcodep) {
	    case NB_SSN_MESSAGE:
	    case NB_SSN_KEEPALIVE:	/* Can "NetBIOS-less" have a keep alive, does hurt anything */
			break;
	    case NB_SSN_REQUEST:
	    case NB_SSN_POSRESP:
	    case NB_SSN_NEGRESP:
	    case NB_SSN_RTGRESP:
			if (nbp->nbp_flags & NBF_NETBIOS) {
				break;
			}
	    default:
            SMBERROR("bad nb header received 0x%x (bogus type)\n", len);
            return (EPIPE);
	}
	*lenp = len;
	return (0);
}

static int nbssn_recv(struct nbpcb *nbp, mbuf_t *mpp, int *lenp, uint8_t *rpcodep,
					  struct timespec *wait_time)
{
	mbuf_t m;
	mbuf_t tm;
	uint8_t rpcode;
	uint32_t len;
	int32_t error;
	size_t recvdlen, resid;
    if (!(nbp->nbp_flags & NBF_CONNECTED)) {
        SMBERROR("nbp_flags 0x%x \n", nbp->nbp_flags);
		return (ENOTCONN);
    }
    if (mpp) {
		*mpp = NULL;
    }
	m = NULL;
    for(;;) {
		/*
		 * Read the response header.
		 */
        error = nbssn_recvhdr(nbp, &len, &rpcode, wait_time);
        if (error) {
            if (error != EWOULDBLOCK){
                SMBERROR("nbssn_recvhdr error %d \n", error);
            }
			return (error);
        }

		/*
		 * Loop, blocking, for data following the response header, if any.
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
        while (resid != 0) {
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
			nanouptime(&tstart);
			do {
                /*
                 * Be careful here. sock_receivembuf() does not return a mbuf
                 * with a header!
                 */
				recvdlen = MIN(resid, nbp->nbp_rcvchunk);
				error = sock_receivembuf(nbp->nbp_tso, NULL, &tm,
                                         MSG_WAITALL, &recvdlen);
				if (error == EAGAIN) {
					nanouptime(&tend);
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
            if (error == 0) {
                /*
                 * If we didn't get an error and recvdlen is zero then we have reached
                 * EOF. So the socket has the SS_CANTRCVMORE flag set. This means the other
                 * side has closed their side of the connection.
                 */
                if ((recvdlen == 0) && resid) {
                    SMBWARNING("Server closed their side of the connection.\n");
                    error = EPIPE;
                    goto out;
                }

                /*
                 * This should never happen, someday should we make it just
                 * a debug assert.
                 */
                if ((recvdlen > resid)) {
                    SMBERROR("Got more data than we asked for!\n");
                    if (tm) {
                        mbuf_freem(tm);
                    }
                    error = EPIPE;
                    goto out;
                }
            }
            else {
                SMBERROR("sock_receivembuf error %d \n", error);
                goto out;
            }
            resid -= recvdlen;
			/*
			 * Append received chunk to previous chunk. Just glue 
			 * the new chain on the end. Consumer will pullup as required.
			 */
			if (!m) {
				m = (mbuf_t )tm;
			} else if (tm) {
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
        if (m) {
			mbuf_freem(m);
        }
		return (error);
	}

    if (mpp) {
		*mpp = m;
    }
    else {
		mbuf_freem(m);
    }
	*lenp = len;
	*rpcodep = rpcode;
	return (0);
}

/*
 * SMB transport interface
 */
static int
smb_nbst_create(struct smbiod *iod)
{
	struct nbpcb *nbp;

    SMB_MALLOC_TYPE(nbp, struct nbpcb, Z_WAITOK);
	bzero(nbp, sizeof *nbp);
	nbp->nbp_timo.tv_sec = SMB_NBTIMO;
	nbp->nbp_state = NBST_CLOSED;
	nbp->nbp_iod = iod;
	nbp->nbp_sndbuf = smb_tcpsndbuf;
	nbp->nbp_rcvbuf = smb_tcprcvbuf;
    lck_mtx_lock(&iod->iod_tdata_lock);
	iod->iod_tdata = nbp;
    lck_mtx_unlock(&iod->iod_tdata_lock);
	return (0);
}

static int
smb_nbst_done(struct smbiod *iod)
{
	struct nbpcb *nbp = iod->iod_tdata;

	if (nbp == NULL)
		return (ENOTCONN);
	smb_nbst_disconnect(iod);
    lck_mtx_lock(&iod->iod_tdata_lock);
    if (nbp->nbp_laddr) {
        SMB_FREE_TYPE(struct sockaddr_nb, nbp->nbp_laddr);
    }
    if (nbp->nbp_paddr) {
        SMB_FREE_TYPE(struct sockaddr_nb, nbp->nbp_paddr);
    }
	/* The session_tdata is no longer valid */
    iod->iod_tdata = NULL;
    lck_mtx_unlock(&iod->iod_tdata_lock);
    SMB_FREE_TYPE(struct nbpcb, nbp);
	return (0);
}

static int
smb_nbst_bind(struct smbiod *iod, struct sockaddr *sap)
{
	struct nbpcb *nbp = iod->iod_tdata;
	struct sockaddr_nb *snb;
	int error, slen;

	DBG_ASSERT(iod->iod_tdata != NULL);
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
		lck_mtx_lock(&iod->iod_tdata_lock);
		nbp->nbp_laddr = snb;
		nbp->nbp_flags |= NBF_LOCADDR;
		lck_mtx_unlock(&iod->iod_tdata_lock);
		error = 0;
	} while(0);
	return (error);
}

static int
smb_nbst_connect(struct smbiod *iod, struct sockaddr *sap)
{
	struct nbpcb *nbp = iod->iod_tdata;
	struct sockaddr *so;
	struct timespec ts1, ts2;
	int error, slen;

	NBDEBUG("id %u\n", iod->iod_id);
	if (nbp == NULL)
		return (EINVAL);

    if (nbp->nbp_flags & NBF_CONNECTED) {
        return (EISCONN);
    }

	if (sap->sa_family == AF_NETBIOS) {
		if (nbp->nbp_laddr == NULL)
			return (EINVAL);
		slen = sap->sa_len;
		if (slen < (int)NB_MINSALEN)
			return (EINVAL);
		if (nbp->nbp_paddr) {
            SMB_FREE_TYPE(struct sockaddr_nb, nbp->nbp_paddr);
		}
		nbp->nbp_paddr = (struct sockaddr_nb*)smb_dup_sockaddr(sap, 1);
		if (nbp->nbp_paddr == NULL)
			return (ENOMEM);
		so = (struct sockaddr*)&(nbp->nbp_paddr)->snb_addrin;
	} else {
		so = sap;
	}
	/*
	 * For our general timeout we use the greater of
	 * the default (15 sec) and 4 times the time it
	 * took for the first round trip.  We used to use
	 * just the latter, but sometimes if the first 
	 * round trip is very fast the subsequent 4 sec
	 * timeouts are simply too short.
	 */
	nanouptime(&ts1);
	error = tcp_connect(nbp, so, iod->iod_connection_to);
    if (error) {
		return (error);
    }
	nanouptime(&ts2);

    /* Set that we are connected */
    nbp->nbp_flags |= NBF_CONNECTED;

    timespecsub(&ts2, &ts1);
	timespecadd(&ts2, &ts2);
	timespecadd(&ts2, &ts2);	/*  * 4 */
    if (timespeccmp(&ts2, &nbp->nbp_timo, >)) {
		nbp->nbp_timo = ts2;
    }

	/* If its not a NetBIOS connection, then we don't need to do a NetBIOS session connect */
	if (sap->sa_family != AF_NETBIOS)
		nbp->nbp_state = NBST_SESSION;
	else {
		nbp->nbp_flags |= NBF_NETBIOS;
		error = nbssn_rq_request(nbp, iod->iod_connection_to);
		if (error)
			smb_nbst_disconnect(iod);
	}
    
    if (!error) {
        memset(&nbp->nbp_sock_addr, 0, sizeof(nbp->nbp_sock_addr));
        error = sock_getsockname(nbp->nbp_tso, (struct sockaddr*)&nbp->nbp_sock_addr, sizeof(nbp->nbp_sock_addr));
        if (error) {
            SMBERROR("sock_getsockname of IP_BOUND_IF returned %d.\n", error);
            goto exit;
        }
#ifdef SMB_DEBUG
        char str[128];
        smb2_sockaddr_to_str((struct sockaddr*)&nbp->nbp_sock_addr, str, sizeof(str)); 
        SMBDEBUG("id %d, sockname: %s.\n", iod->iod_id, str);
#endif
    }
exit:
	return (error);
}

static int
smb_nbst_disconnect(struct smbiod *iod)
{
	struct nbpcb *nbp = iod->iod_tdata;
	socket_t so;
	int flags;

	if (nbp == NULL) {
		return (ENOTCONN);
	}

	if ((so = nbp->nbp_tso) != NULL) {
		lck_mtx_lock(&iod->iod_tdata_lock);
		flags = nbp->nbp_flags;
		nbp->nbp_flags &= ~NBF_CONNECTED;
		nbp->nbp_flags &= ~NBF_SOCK_OPENED;
		/* Do not null out nbp_tso here due to the read thread using it */
		lck_mtx_unlock(&iod->iod_tdata_lock);
		if (flags & NBF_CONNECTED) {
			sock_shutdown(so, SHUT_RDWR);
		}
		if (flags & NBF_SOCK_OPENED) {
			sock_close(so);
		}
	}
	if (nbp->nbp_state != NBST_RETARGET) {
		nbp->nbp_state = NBST_CLOSED;
	}
	return (0);
}

static int
smb_nbst_send(struct smbiod *iod, mbuf_t m0)
{
	struct nbpcb *nbp = iod->iod_tdata;
	int error = 0;
    struct msghdr msg = {0};

    SMB_LOG_KTRACE(SMB_DBG_NBST_SEND | DBG_FUNC_START, iod->iod_id, m0, 0, 0, 0);

	/* Should never happen, but just in case */
	if ((nbp == NULL) || (nbp->nbp_state != NBST_SESSION)) {
		error = ENOTCONN;
		goto abort;
	}
    
    /* Add in the NetBIOS 4 byte header */
	if (mbuf_prepend(&m0, 4, MBUF_WAITOK)) {
		error = ENOBUFS;
		goto exit;
	}
	
	nb_sethdr(nbp, m0, NB_SSN_MESSAGE, (uint32_t)(m_fixhdr(m0) - 4));

    /* These fields get copied by sock_sendmbuf_can_wait() so explicity set them */
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    error = sock_sendmbuf_can_wait(nbp->nbp_tso, &msg, (mbuf_t)m0, 0, NULL);
    goto exit;

abort:
    if (m0) {
		mbuf_freem(m0);
    }
exit:
    SMB_LOG_KTRACE(SMB_DBG_NBST_SEND | DBG_FUNC_END, error, iod->iod_id, 0, 0, 0);
	return (error);
}

static int
smb_nbst_recv(struct smbiod *iod, mbuf_t *mpp)
{
	struct nbpcb *nbp = iod->iod_tdata;
	uint8_t rpcode, *hp;
	int error, rplen;

    SMB_LOG_KTRACE(SMB_DBG_NBST_RECV | DBG_FUNC_START, iod->iod_id, 0, 0, 0, 0);

    if (nbp == NULL) {
		error = ENOTCONN;
		goto exit;
    }

    error = nbssn_recv(nbp, mpp, &rplen, &rpcode, NULL);
    if (!error) {
        /* Handle case when first mbuf is zero-length */
        error = mbuf_pullup(mpp, 1);
    }

    // Check for a transform header (encrypted msg)
    if (!error) {
        hp = mbuf_data(*mpp);
        if (*hp == 0xfd) {
            error = smb3_msg_decrypt(iod->iod_session, mpp);
        }
    }
    
    if (error) {
        *mpp = NULL;
    }
    
exit:
    SMB_LOG_KTRACE(SMB_DBG_NBST_RECV | DBG_FUNC_END, error, iod->iod_id, 0, 0, 0);
    return (error);
}

static void
smb_nbst_timo(struct smbiod *iod)
{
	#pragma unused(iod)
	return;
}

static int
smb_nbst_getparam(struct smbiod *iod, int param, void *data)
{
	struct nbpcb *nbp = iod->iod_tdata;

	/* Should never happen, but just in case */
	if (nbp == NULL) {
		return (ENOTCONN);
	}
	
	switch (param) {
	    case SMBTP_SNDSZ:
			*(uint32_t*)data = nbp->nbp_sndbuf;
			break;
	    case SMBTP_RCVSZ:
			*(uint32_t*)data = nbp->nbp_rcvbuf;
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
		case SMBTP_QOS:
			*(uint32_t*)data = nbp->nbp_qos;
			break;
        case SMBTP_IP_ADDR:
            memcpy(data, &nbp->nbp_sock_addr, sizeof(nbp->nbp_sock_addr));
            break;
	    default:
			return (EINVAL);
	}
	return (0);
}

static int
smb_nbst_setparam(struct smbiod *iod, int param, void *data)
{
	struct nbpcb *nbp = iod->iod_tdata;
	uint32_t option = 0;
	int error = 0;

	/* Should never happen, but just in case */
	if (nbp == NULL) {
		return (ENOTCONN);
	}
    
	switch (param) {
	    case SMBTP_SELECTID:
			nbp->nbp_selectid = data;
			break;
	    case SMBTP_UPCALL:
			nbp->nbp_upcall = data;
			break;
		case SMBTP_QOS:
			option = *(uint32_t*) data;
			SMBDEBUG("Setting IP QoS to 0x%x\n", option);
			if ((error = sock_settclassopt(nbp->nbp_tso, &option, sizeof (option))))
				SMBDEBUG("sock_settclassopt failed %d \n", error);
			else {
				/* Only save the setting if it actually worked */
				nbp->nbp_qos = option;
			}
			break;
        case SMBTP_BOUND_IF:
            nbp->nbp_if_idx = *(uint32_t*)data;
            nbp->nbp_flags |= NBF_BOUND_IF;
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
smb_nbst_fatal(struct smbiod *iod, int error)
{
	struct nbpcb *nbp;

	switch (error) {
	    case EHOSTDOWN:
	    case ENETUNREACH:
	    case ENOTCONN:
	    case ENETRESET:
	    case ECONNABORTED:
	    case EPIPE:
		case EADDRNOTAVAIL:
		return 1;
	}
	DBG_ASSERT(iod);
    if (iod == NULL) return 1;
    lck_mtx_lock(&iod->iod_tdata_lock);
	nbp = iod->iod_tdata;

    /*
     * <78410582> sock_isconnected must not be called if the socket was closed
     */
	if ((nbp == NULL) || (nbp->nbp_tso == NULL) ||
        ((nbp->nbp_flags & NBF_SOCK_OPENED) == 0) ||
        (!sock_isconnected(nbp->nbp_tso))) {
        lck_mtx_unlock(&iod->iod_tdata_lock);
        return 1;
    }
    lck_mtx_unlock(&iod->iod_tdata_lock);
	return (0);
}

struct smb_tran_desc smb_tran_nbtcp_desc = {
    .tr_type       = SMBT_NBTCP,
    .tr_create     = smb_nbst_create,
    .tr_done       = smb_nbst_done,
    .tr_bind       = smb_nbst_bind,
    .tr_connect    = smb_nbst_connect,
    .tr_disconnect = smb_nbst_disconnect,
    .tr_send       = smb_nbst_send,
    .tr_recv       = smb_nbst_recv,
    .tr_timo       = smb_nbst_timo,
    .tr_getparam   = smb_nbst_getparam,
    .tr_setparam   = smb_nbst_setparam,
    .tr_fatal      = smb_nbst_fatal,
    .tr_link       = {NULL, NULL}
};
