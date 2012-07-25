/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kpi_mbuf.h>
#include <sys/unistd.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include <sys/kauth.h>

#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>
#include <smbfs/smbfs.h>

#include <IOKit/IOLib.h>
#include <netsmb/smb_sleephandler.h>

static int smb_iod_next;

int smb_iod_sendall(struct smbiod *iod);

/*
 * Check to see if the share has a routine to handle going away, if so.
 */
static int isShareGoingAway(struct smb_share* share)
{
	int goingAway = FALSE;
	
	lck_mtx_lock(&share->ss_shlock);
	if (share->ss_going_away) {
		goingAway = share->ss_going_away(share);
	}
	lck_mtx_unlock(&share->ss_shlock);
	return goingAway;
}

static int smb_iod_check_timeout(struct timespec *starttime, int SecondsTillTimeout)
{
	struct timespec waittime, tsnow;
				
	waittime.tv_sec = SecondsTillTimeout;
	waittime.tv_nsec = 0;
	timespecadd(&waittime, starttime);
	nanouptime(&tsnow);
	if (timespeccmp(&tsnow, &waittime, >))
		return TRUE;
	else return FALSE;
}


static __inline void
smb_iod_rqprocessed(struct smb_rq *rqp, int error, int flags)
{
	SMBRQ_SLOCK(rqp);
	rqp->sr_flags |= flags;
	rqp->sr_lerror = error;
	rqp->sr_rpgen++;
	rqp->sr_state = SMBRQ_NOTIFIED;
	if (rqp->sr_flags & SMBR_ASYNC) {
		DBG_ASSERT(rqp->sr_callback);
		rqp->sr_callback(rqp->sr_callback_args);
	} else 
		wakeup(&rqp->sr_state);
	SMBRQ_SUNLOCK(rqp);
}

/* 
 * Gets called from smb_iod_dead, smb_iod_negotiate and smb_iod_ssnsetup. This routine
 * should never get called while we are in reconnect state. This routine just flushes 
 * any old messages left after a connection went down. 
 */
static void smb_iod_invrq(struct smbiod *iod)
{
	struct smb_rq *rqp, *trqp;
	
	/*
	 * Invalidate all outstanding requests for this connection
	 */
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
		smb_iod_rqprocessed(rqp, ENOTCONN, SMBR_DEAD);
	}
	SMB_IOD_RQUNLOCK(iod);
}

static void
smb_iod_sockwakeup(struct smbiod *iod)
{
	/* note: called from socket upcall... */
	iod->iod_workflag = 1;		/* new work to do */
	
	wakeup(&(iod)->iod_flags);
}

static void
smb_iod_closetran(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;

	if (vcp->vc_tdata == NULL)
		return;
	SMB_TRAN_DISCONNECT(vcp);
	SMB_TRAN_DONE(vcp);
}

static void
smb_iod_dead(struct smbiod *iod)
{
	struct smb_rq *rqp, *trqp;

	iod->iod_state = SMBIOD_ST_DEAD;
	smb_iod_closetran(iod);
	smb_iod_invrq(iod);
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
		if (rqp->sr_share) {
			lck_mtx_lock(&rqp->sr_share->ss_shlock);
			if (rqp->sr_share->ss_dead)
				rqp->sr_share->ss_dead(rqp->sr_share);
			lck_mtx_unlock(&rqp->sr_share->ss_shlock);
		}
	}
	SMB_IOD_RQUNLOCK(iod);
}

/*
 * We lost the connection. Set the vc flag saying we need to do a reconnect and
 * tell all the shares we are starting reconnect. At this point all non reconnect messages 
 * should block until the reconnect process is completed. This routine is always excuted
 * from the main thread.
 */
static void smb_iod_start_reconnect(struct smbiod *iod)
{
	struct smb_share *share, *tshare;
	struct smb_rq *rqp, *trqp;

	/* This should never happen, but for testing lets leave it in */
	if (iod->iod_flags & SMBIOD_START_RECONNECT) {
		SMBWARNING("Already in start reconnect with %s\n", iod->iod_vc->vc_srvname);
		return; /* Nothing to do here we are already in start reconnect mode */
	}
	
	/*
	 * Only  start a reconnect on an active sessions or when a reconnect failed because we
	 * went to sleep. If we are in the middle of a connection then mark the connection
	 * as dead and get out.
	 */
	switch (iod->iod_state) {
	case SMBIOD_ST_VCACTIVE:	/* session established */
	case SMBIOD_ST_RECONNECT:	/* betweeen reconnect attempts; sleep happened. */
		break; 
	case SMBIOD_ST_NOTCONN:	/* no connect request was made */
	case SMBIOD_ST_CONNECT:	/* a connect attempt is in progress */
	case SMBIOD_ST_TRANACTIVE:	/* transport level is up */
	case SMBIOD_ST_NEGOACTIVE:	/* completed negotiation */
	case SMBIOD_ST_SSNSETUP:	/* started (a) session setup */
	case SMBIOD_ST_DEAD:		/* connection broken, transport is down */
		SMBDEBUG("%s: iod->iod_state = %x iod->iod_flags = 0x%x\n", 
				 iod->iod_vc->vc_srvname, iod->iod_state, iod->iod_flags);
		if (!(iod->iod_flags & SMBIOD_RECONNECT)) {
			smb_iod_dead(iod);
			return;
		}
		break;
	}

	/* Set the flag saying we are starting reconnect. */
	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags |= SMBIOD_START_RECONNECT;
	SMB_IOD_FLAGSUNLOCK(iod);
	
	/* Search through the request list and set them to the correct state */
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
		SMBRQ_SLOCK(rqp);
		/* Clear any internal or async request out of the queue */
		if (rqp->sr_flags & (SMBR_INTERNAL | SMBR_ASYNC)) {
			SMBRQ_SUNLOCK(rqp);
			if (rqp->sr_flags & SMBR_ASYNC)
				smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
			else
				smb_iod_rqprocessed(rqp, ENOTCONN, SMBR_DEAD); 
		} else {
			/*
			 * Let the upper layer know that this message was process while we were in
			 * reconnect mode. If they receive an error they may want to handle this 
			 * message differently.
			 */
			rqp->sr_flags |= SMBR_RECONNECTED;
				/* If we have not received a reply set the state to reconnect */
			if (rqp->sr_state != SMBRQ_NOTIFIED) {
				rqp->sr_state = SMBRQ_RECONNECT; /* Wait for reconnect to complete */
				rqp->sr_flags |= SMBR_REXMIT;	/* Tell the upper layer this message was resent */
				rqp->sr_lerror = 0;		/* We are going to resend clear the error */
			}
			SMBRQ_SUNLOCK(rqp);
		}
	}
	SMB_IOD_RQUNLOCK(iod);
	
	/* We are already in reconnect, so we are done */
	if (iod->iod_flags & SMBIOD_RECONNECT) {
		goto done;
	}
	/* Set our flag saying we need to do a reconnect, but not until we finish the work in this routine. */
	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags |= SMBIOD_RECONNECT;
	SMB_IOD_FLAGSUNLOCK(iod);
	
	/*
	 * We have the vc list locked so the shares can't be remove and they can't
	 * go away. If the share is not gone then mark that we are in reconnect mode.
	 */
	smb_vc_lock(iod->iod_vc);
	SMBCO_FOREACH_SAFE(share, VCTOCP(iod->iod_vc), tshare) {
		lck_mtx_lock(&share->ss_stlock);
		if (!(share->ss_flags & SMBO_GONE)) {
			share->ss_flags |= SMBS_RECONNECTING;
		}
		lck_mtx_unlock(&(share)->ss_stlock);
	}
	smb_vc_unlock(iod->iod_vc);
done:
	/* Ok now we can do the reconnect */
	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags &= ~SMBIOD_START_RECONNECT;
	SMB_IOD_FLAGSUNLOCK(iod);
}

static int
smb_iod_negotiate(struct smbiod *iod, vfs_context_t user_context)
{
	struct smb_vc *vcp = iod->iod_vc;
	int error;

	SMBIODEBUG("%d\n", iod->iod_state);
	switch(iod->iod_state) {
	    case SMBIOD_ST_TRANACTIVE:
	    case SMBIOD_ST_NEGOACTIVE:
	    case SMBIOD_ST_SSNSETUP:
            SMBERROR("smb_iod_negotiate is invalid now, state=%d\n", iod->iod_state);
            return EINVAL;
	    case SMBIOD_ST_VCACTIVE:
            SMBERROR("smb_iod_negotiate called when connected\n");
            return EISCONN;
	    case SMBIOD_ST_DEAD:
            return ENOTCONN;	/* XXX: last error code ? */
	    default:
            break;
	}
	iod->iod_state = SMBIOD_ST_CONNECT;
	error = SMB_TRAN_CREATE(vcp);
	if (error) {
		goto errorOut;
	}
	SMBIODEBUG("tcreate\n");
	/* We only bind when doing a NetBIOS connection */
	if (vcp->vc_saddr->sa_family == AF_NETBIOS) {
		error = SMB_TRAN_BIND(vcp, vcp->vc_laddr);
		if (error) {
			goto errorOut;
		}
		SMBIODEBUG("tbind\n");		
	}
	SMB_TRAN_SETPARAM(vcp, SMBTP_SELECTID, iod);
	SMB_TRAN_SETPARAM(vcp, SMBTP_UPCALL, smb_iod_sockwakeup);
	error = SMB_TRAN_CONNECT(vcp, vcp->vc_saddr);
	if (error == 0) {
		iod->iod_state = SMBIOD_ST_TRANACTIVE;
		SMBIODEBUG("tconnect\n");
		error = smb_smb_negotiate(vcp, user_context, FALSE, iod->iod_context);
	}
	if (error) {
		goto errorOut;
	}
	iod->iod_state = SMBIOD_ST_NEGOACTIVE;
	SMBIODEBUG("completed\n");
	smb_iod_invrq(iod);
	return 0;
	
errorOut:
	smb_iod_dead(iod);
	return error;
}

static int
smb_iod_ssnsetup(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	int error;

	SMBIODEBUG("%d\n", iod->iod_state);
	switch(iod->iod_state) {
	    case SMBIOD_ST_NEGOACTIVE:
            break;
	    case SMBIOD_ST_DEAD:
            return ENOTCONN;	/* XXX: last error code ? */
	    case SMBIOD_ST_VCACTIVE:
            SMBERROR("smb_iod_ssnsetup called when connected\n");
            return EISCONN;
	    default:
            SMBERROR("smb_iod_ssnsetup is invalid now, state=%d\n",
                     iod->iod_state);
		return EINVAL;
	}
	iod->iod_state = SMBIOD_ST_SSNSETUP;
	error = smb_smb_ssnsetup(vcp, iod->iod_context);
	if (error) {
		/* 
		 * We no longer call smb_io_dead here, the vc could still be
		 * alive. Allow for other attempt to authenticate on this same 
		 * circuit. If the connect went down let the call process 
		 * decide what to do with the circcuit. 
		 *
		 * Now all we do is reset the iod state back to what it was, but only if
		 * it hasn't change from the time we came in here. If the connection goes
		 * down(server dies) then we shouldn't change the state. 
		 */
		if (iod->iod_state == SMBIOD_ST_SSNSETUP)
			iod->iod_state = SMBIOD_ST_NEGOACTIVE;
	} else {
		iod->iod_state = SMBIOD_ST_VCACTIVE;
		SMBIODEBUG("completed\n");
		/* Don't flush the queue if we are in reconnect state. We need to resend those messages. */
		if ((iod->iod_flags & SMBIOD_RECONNECT) != SMBIOD_RECONNECT)
			smb_iod_invrq(iod);
	}
	return error;
}

static int
smb_iod_disconnect(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;

	SMBIODEBUG("\n");
	if (iod->iod_state == SMBIOD_ST_VCACTIVE) {
		smb_smb_ssnclose(vcp, iod->iod_context);
		iod->iod_state = SMBIOD_ST_TRANACTIVE;
	}
	vcp->vc_smbuid = SMB_UID_UNKNOWN;
	smb_iod_closetran(iod);
	iod->iod_state = SMBIOD_ST_NOTCONN;
	return 0;
}

static int
smb_iod_sendrq(struct smbiod *iod, struct smb_rq *rqp)
{
	struct smb_vc *vcp = iod->iod_vc;
	mbuf_t m;
	int error;

	SMBIODEBUG("iod_state = %d\n", iod->iod_state);
	switch (iod->iod_state) {
	    case SMBIOD_ST_NOTCONN:
            smb_iod_rqprocessed(rqp, ENOTCONN, 0);
            return 0;
	    case SMBIOD_ST_DEAD:
            /* This is what keeps the iod itself from sending more */
            smb_iod_rqprocessed(rqp, ENOTCONN, 0);
            return 0;
	    case SMBIOD_ST_CONNECT:
            return 0;
	    case SMBIOD_ST_NEGOACTIVE:
            SMBERROR("smb_iod_sendrq in unexpected state(%d)\n",
                     iod->iod_state);
	    default:
            break;
	}

	/* Use to check for vc, can't have an iod without a vc */
	*rqp->sr_rquid = htoles(vcp->vc_smbuid);
	/* If the request has a share then it has a reference on it */
	*rqp->sr_rqtid = htoles(rqp->sr_share ? rqp->sr_share->ss_tid : SMB_TID_UNKNOWN);
	
	mb_fixhdr(&rqp->sr_rq);
	if (vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE)
		smb_rq_sign(rqp);

	SMBSDEBUG("M:%04x, P:%04x, U:%04x, T:%04x\n", rqp->sr_mid, 0, 0, 0);
	m_dumpm(rqp->sr_rq.mb_top);
	error = mbuf_copym(rqp->sr_rq.mb_top, 0, MBUF_COPYALL, MBUF_WAITOK, &m);
	DBG_ASSERT(error == 0);
	error = rqp->sr_lerror = (error) ? error : SMB_TRAN_SEND(vcp, m);
	if (error == 0) {
		nanouptime(&rqp->sr_timesent);
		iod->iod_lastrqsent = rqp->sr_timesent;
		rqp->sr_state = SMBRQ_SENT;
		return 0;
	}
	/* Did the connection go down, we may need to reconnect. */
	if (SMB_TRAN_FATAL(vcp, error)) 
		return ENOTCONN;
	else if (error) {	/* Either the send failed or the mbuf_copym? */
		SMBERROR("TRAN_SEND returned non-fatal error %d sr_cmd = 0x%x\n", error, rqp->sr_cmd);
		error = EIO; /* Couldn't send not much else we can do */
		smb_iod_rqprocessed(rqp, error, 0);
	}
	return 0;
}

/*
 * Process incoming packets
 */
static int
smb_iod_recvall(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct smb_rq *rqp, *trqp;
	mbuf_t m;
	u_char *hp;
	uint16_t mid;
	uint16_t pidHigh;
	uint16_t pidLow;
	uint8_t cmd;
	int error;

	switch (iod->iod_state) {
	    case SMBIOD_ST_NOTCONN:
	    case SMBIOD_ST_DEAD:
	    case SMBIOD_ST_CONNECT:
            return 0;
	    default:
            break;
	}

	for (;;) {
		m = NULL;

        /* this reads in the entire response packet based on the NetBIOS hdr */
		error = SMB_TRAN_RECV(vcp, &m);
		if (error == EWOULDBLOCK)
			break;
		if (SMB_TRAN_FATAL(vcp, error)) {
			smb_iod_start_reconnect(iod);
			break;
		}
		if (error)
			break;
		if (m == NULL) {
			SMBERROR("tran return NULL without error\n");
			continue;
		}

        /* 
         * Parse out enough of the response to be able to match it with an 
         * existing smb_rq in the queue.
         */

		if (mbuf_pullup(&m, SMB_HDRLEN))
			continue;	/* wait for a good packet */
		/*
		 * Now we got an entire and possibly invalid SMB packet.
		 * Be careful while parsing it.
		 */
		m_dumpm(m);
		hp = mbuf_data(m);
		if (bcmp(hp, SMB_SIGNATURE, SMB_SIGLEN) != 0) {
			mbuf_freem(m);
			continue;
		}
		mid = SMB_HDRMID(hp);
		cmd = SMB_HDRCMD(hp);
		pidHigh = SMB_HDRPIDHIGH(hp);
		pidLow = SMB_HDRPIDLOW(hp);
		SMBSDEBUG("mid %04x cmd = 0x%x\n", (unsigned)mid, cmd);

        /*
         * Search queue of smb_rq to find a match
         */
		SMB_IOD_RQLOCK(iod);
		nanouptime(&iod->iod_lastrecv);
		TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
			/* 
			 * We now use the mid and the high pid as a single mid, this gives
			 * us a larger mid and helps prevent finding the wrong item. So we
			 * need to make sure the messages match up, so use the cmd to confirm
			 * we have the correct message.
			 * NOTE: SMB2 does not have this issue.
			 *
			 * We need to test and make sure the server supports returning the 
			 * high pid field correctly. We always set the high pid to one when
			 * sending the negotiate message, this way we can check to make 
			 * sure they return it back correctly. This is safe since we should 
			 * only have one negotiate message on the list at time. 
			 */
			if ((cmd == SMB_COM_NEGOTIATE) &&  
				(rqp->sr_cmd == SMB_COM_NEGOTIATE) &&  (rqp->sr_mid == mid)) {
				if (rqp->sr_pidHigh == pidHigh) {
					vcp->supportHighPID = TRUE;
				} else {
					vcp->supportHighPID = FALSE;
				}

			} else if ((rqp->sr_mid != mid) ||  (rqp->sr_cmd != cmd) || 
					   (rqp->sr_pidHigh != pidHigh) || (rqp->sr_pidLow != pidLow)) {
				continue;
			}

            /*
             * Found a matching smb_rq
             */
            
            /* We received a packet on the vc, clear the not responsive flag */
			SMB_IOD_FLAGSLOCK(iod);
			iod->iod_flags &= ~SMBIOD_VC_NOTRESP;
			SMB_IOD_FLAGSUNLOCK(iod);

			if (rqp->sr_share) {
				lck_mtx_lock(&rqp->sr_share->ss_shlock);
				if (rqp->sr_share->ss_up)
					rqp->sr_share->ss_up(rqp->sr_share, FALSE);
				lck_mtx_unlock(&rqp->sr_share->ss_shlock);
			}

			SMBRQ_SLOCK(rqp);
			if (rqp->sr_rp.md_top == NULL) {
				md_initm(&rqp->sr_rp, m);
			} else {
				if (rqp->sr_flags & SMBR_MULTIPACKET) {
					md_append_record(&rqp->sr_rp, m);
				} else {
					SMBRQ_SUNLOCK(rqp);
					SMBERROR("duplicate response %d (ignored)\n", mid);
					break;
				}
			}
			SMBRQ_SUNLOCK(rqp);
            
            /* Wake up thread waiting for this response */
			smb_iod_rqprocessed(rqp, 0, 0);
			break;
		}
		SMB_IOD_RQUNLOCK(iod);

		if (rqp == NULL) {			
			/* Ignore ECHO and NTNotify dropped messages */
			if ((cmd != SMB_COM_ECHO) && (cmd != SMB_COM_NT_TRANSACT))
				SMBWARNING("drop resp: mid %d, cmd %d\n", (unsigned)mid, cmd);	
			mbuf_freem(m);
		}
	}

	if ((iod->iod_flags & SMBIOD_RECONNECT) != SMBIOD_RECONNECT) {
		SMB_IOD_RQLOCK(iod);	/* check for interrupts */
		TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
			if (smb_sigintr(rqp->sr_context))
				rqp->sr_timo = SMBIOD_INTR_TIMO;	/* Wait a little longer before timing out */
		}
		SMB_IOD_RQUNLOCK(iod);
	}
	return 0;
}

int
smb_iod_request(struct smbiod *iod, int event, void *ident)
{
	struct smbiod_event *evp;
	int error;

	SMBIODEBUG("\n");
	SMB_MALLOC(evp, struct smbiod_event *, sizeof(*evp), M_SMBIOD, M_WAITOK | M_ZERO);
	evp->ev_type = event;
	evp->ev_ident = ident;
	SMB_IOD_EVLOCK(iod);
	STAILQ_INSERT_TAIL(&iod->iod_evlist, evp, ev_link);
	if ((event & SMBIOD_EV_SYNC) == 0) {
		SMB_IOD_EVUNLOCK(iod);
		smb_iod_wakeup(iod);
		return 0;
	}
	smb_iod_wakeup(iod);
	msleep(evp, SMB_IOD_EVLOCKPTR(iod), PWAIT | PDROP, "iod-ev", 0);
	error = evp->ev_error;
	SMB_FREE(evp, M_SMBIOD);
	return error;
}

/*
 * Place request in the queue.
 * Request from smbiod have a high priority.
 */
int
smb_iod_rq_enqueue(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct smbiod *iod = vcp->vc_iod;
	struct timespec ts;

	if (rqp->sr_context == iod->iod_context) {
		DBG_ASSERT((rqp->sr_flags & SMBR_ASYNC) != SMBR_ASYNC);
		rqp->sr_flags |= SMBR_INTERNAL;
		SMB_IOD_RQLOCK(iod);
		TAILQ_INSERT_HEAD(&iod->iod_rqlist, rqp, sr_link);
		SMB_IOD_RQUNLOCK(iod);
		for (;;) {
			if (smb_iod_sendrq(iod, rqp) != 0) {
				smb_iod_start_reconnect(iod);
				break;
			}
			/*
			 * we don't need to lock state field here
			 */
			if (rqp->sr_state != SMBRQ_NOTSENT)
				break;
			ts.tv_sec = 1;
			ts.tv_nsec = 0;
			msleep(&iod->iod_flags, 0, PWAIT, "90sndw", &ts);
		}
		if (rqp->sr_lerror)
			smb_iod_removerq(rqp);
		return rqp->sr_lerror;
	}

	switch (iod->iod_state) {
		case SMBIOD_ST_DEAD:
			if (rqp->sr_share) {
				lck_mtx_lock(&rqp->sr_share->ss_shlock);
				if (rqp->sr_share->ss_dead)
					rqp->sr_share->ss_dead(rqp->sr_share);
				lck_mtx_unlock(&rqp->sr_share->ss_shlock);
			}
	    case SMBIOD_ST_NOTCONN:
			return ENOTCONN;
	    case SMBIOD_ST_TRANACTIVE:
	    case SMBIOD_ST_NEGOACTIVE:
	    case SMBIOD_ST_SSNSETUP:
			/* This can happen if we are doing a reconnect */
	    default:
			/* If this is not an internal message and we are in reconnect then check for softmount timouts. */
			if ((!(rqp->sr_flags & SMBR_INTERNAL)) && (iod->iod_flags & SMBIOD_RECONNECT) && 
				(rqp->sr_share) && (rqp->sr_share->ss_soft_timer)) {
				if (smb_iod_check_timeout(&iod->reconnectStartTime, rqp->sr_share->ss_soft_timer)) {
					SMBDEBUG("Soft Mount timed out! cmd = %x\n", rqp->sr_cmd);
					return ETIMEDOUT;			
				}
			}
			break;
	}

	SMB_IOD_RQLOCK(iod);
	for (;;) {
		if (iod->iod_muxcnt < vcp->vc_maxmux)
			break;
		iod->iod_muxwant++;
		msleep(&iod->iod_muxwant, SMB_IOD_RQLOCKPTR(iod), PWAIT, "iod-rq-mux", 0);
	}
	/* 
	 * Should be noted here Window 2003 and Samba don't seem to care about going
	 * over the maxmux count when doing notification messages. XPhome does for sure,
	 * they will actual break the connection. SMB2 will solve this issue and some
	 * day I would like to see which server care and which don't. Should we do 
	 * something special for Samba or Apple, since they don't care?
	 *
	 * So for now we never use more than two thirds, if vc_maxmux is less than
	 * three then don't allow any. Should never happen, but just to be safe.
	 */
	if (rqp->sr_flags & SMBR_ASYNC) {
		if (iod->iod_asynccnt >= ((vcp->vc_maxmux / 3) * 2)) {
			SMBWARNING("Max out on VC async notify request %d\n", iod->iod_asynccnt);
			SMB_IOD_RQUNLOCK(iod);
			return EWOULDBLOCK;			
		}
		iod->iod_asynccnt++;
	} else if (iod->iod_state == SMBIOD_ST_VCACTIVE) {
		if (vcp->throttle_info)
			throttle_info_update(vcp->throttle_info, 0);
	}
	iod->iod_muxcnt++;
	TAILQ_INSERT_TAIL(&iod->iod_rqlist, rqp, sr_link);
	SMB_IOD_RQUNLOCK(iod);
	iod->iod_workflag = 1;
	smb_iod_wakeup(iod);
	return 0;
}

int
smb_iod_removerq(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct smbiod *iod = vcp->vc_iod;

	SMBIODEBUG("\n");
	SMB_IOD_RQLOCK(iod);
	if (rqp->sr_flags & SMBR_INTERNAL) {
		TAILQ_REMOVE(&iod->iod_rqlist, rqp, sr_link);
		SMB_IOD_RQUNLOCK(iod);
		return 0;
	}
	TAILQ_REMOVE(&iod->iod_rqlist, rqp, sr_link);
	if (rqp->sr_flags & SMBR_ASYNC)
		iod->iod_asynccnt--;
	iod->iod_muxcnt--;
	if (iod->iod_muxwant) {
		iod->iod_muxwant--;
		wakeup(&iod->iod_muxwant);
	}
	SMB_IOD_RQUNLOCK(iod);
	return 0;
}

int
smb_iod_waitrq(struct smb_rq *rqp)
{
	struct smbiod *iod = rqp->sr_vc->vc_iod;
	int error;
	struct timespec ts;

	SMBIODEBUG("\n");
	if (rqp->sr_flags & SMBR_INTERNAL) {
		for (;;) {
			smb_iod_sendall(iod);
			smb_iod_recvall(iod);
			if (rqp->sr_rpgen != rqp->sr_rplast)
				break;
			ts.tv_sec = 1;
			ts.tv_nsec = 0;
			msleep(&iod->iod_flags, 0, PWAIT, "90irq", &ts);
		}
		smb_iod_removerq(rqp);
		return rqp->sr_lerror;

	}

	SMBRQ_SLOCK(rqp);
	if (rqp->sr_rpgen == rqp->sr_rplast) {
		/*
		  * First thing to note here is that the transaction messages can have multiple replies. We have 
		  * to watch out for this if a reconnect happens. So if we sent the message and received at least
		  * one reply make sure a reconnect hasn't happen in between. So we check for SMBR_MULTIPACKET 
		  * flag because it tells us this is a transaction message, we also check for the SMBR_RECONNECTED 
		  * flag because it tells us that a reconnect happen and we also check to make sure the SMBR_REXMIT 
		  * flags isn't set because that would mean we resent the whole message over. If the sr_rplast is set 
		  * then we have received at least one response, so there is not much we can do with this transaction. 
		  * So just treat it like a softmount happened and return ETIMEDOUT.
		  *
		  * Make sure we didn't get reconnect while we were asleep waiting on the next response.
		 */ 
		do {
			ts.tv_sec = 15;
			ts.tv_nsec = 0;
			msleep(&rqp->sr_state, SMBRQ_SLOCKPTR(rqp), PWAIT, "srs-rq", &ts);
			if ((rqp->sr_rplast) && (rqp->sr_rpgen == rqp->sr_rplast) && 
				 ((rqp->sr_flags & (SMBR_MULTIPACKET | SMBR_RECONNECTED | SMBR_REXMIT)) == (SMBR_MULTIPACKET | SMBR_RECONNECTED))) {
				SMBERROR("Reconnect in the middle of a transaction messages, just return ETIMEDOUT\n");
				rqp->sr_lerror = ETIMEDOUT;
			}
		} while ((rqp->sr_lerror == 0) && (rqp->sr_rpgen == rqp->sr_rplast));
	}
	rqp->sr_rplast++;
	SMBRQ_SUNLOCK(rqp);
	error = rqp->sr_lerror;
	if (rqp->sr_flags & SMBR_MULTIPACKET) {
		/*
		 * If request should stay in the list, then reinsert it
		 * at the end of queue so other waiters have chance to concur
		 */
		SMB_IOD_RQLOCK(iod);
		TAILQ_REMOVE(&iod->iod_rqlist, rqp, sr_link);
		TAILQ_INSERT_TAIL(&iod->iod_rqlist, rqp, sr_link);
		SMB_IOD_RQUNLOCK(iod);
	} else
		smb_iod_removerq(rqp);
	return error;
}

/*
 * Error out any outstanding requests on the VC that belong to the specified 
 * share. The calling routine should hold a reference on this share before 
 * calling this routine.
 */
void
smb_iod_errorout_share_request(struct smb_share *share, int error)
{
	struct smbiod *iod = SSTOVC(share)->vc_iod;
	struct smb_rq *rqp, *trqp;

	/*
	 * Loop through the list of requests and error out all those that belong
	 * to the specified share.
	 */
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
		if (rqp->sr_share && (rqp->sr_share == share) && 
			(rqp->sr_state != SMBRQ_NOTIFIED))
			smb_iod_rqprocessed(rqp, error, 0);
	}
	SMB_IOD_RQUNLOCK(iod);
}

int
smb_iod_sendall(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct smb_rq *rqp, *trqp;
	struct timespec now, ts, uetimeout;
	int herror, echo, drop_req_lock;

	herror = 0;
	echo = 0;
	/*
	 * Loop through the list of requests and send them if possible
	 */
retry:
	SMB_IOD_RQLOCK(iod);
    drop_req_lock = 1;
	TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
		if (iod->iod_state == SMBIOD_ST_DEAD) {
			/* VC is down, just time out any message on the list */
			smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
			continue;
		}
		
		/* If the share is going away then just timeout the request. */ 
		if ((rqp->sr_share) && (isShareGoingAway(rqp->sr_share))) {
			smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
			continue;
		}
		
		if ((iod->iod_flags & SMBIOD_RECONNECT) && (!(rqp->sr_flags & SMBR_INTERNAL))) {
			if (rqp->sr_flags & SMBR_ASYNC) {
				smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
				continue;							
			}
			DBG_ASSERT(rqp->sr_state != SMBRQ_SENT) /* Should never be in the sent state at this point */
			if (rqp->sr_state == SMBRQ_NOTSENT)
				rqp->sr_state = SMBRQ_RECONNECT;
			/* Tell the upper layer that any error may have been the result of a reconnect. */
			rqp->sr_flags |= SMBR_RECONNECTED;
		}
		
		switch (rqp->sr_state) {
			case SMBRQ_RECONNECT:
				if (iod->iod_flags & SMBIOD_RECONNECT)
					break;		/* Do nothing to do but wait for reconnect to end. */
				rqp->sr_state = SMBRQ_NOTSENT;
				/*
				 * Make sure this is no a bad server. If we reconnect more than
				 * MAX_SR_RECONNECT_CNT (5) times trying to send this message
				 * then just give up and kill the mount.
				 */
				rqp->sr_reconnect_cnt += 1; 
				if (rqp->sr_reconnect_cnt > MAX_SR_RECONNECT_CNT) {
					SMBERROR("Looks like we are in a reconnect loop with  server %s, canceling the reconnect. (cmd = %x)\n", 
						vcp->vc_srvname, rqp->sr_cmd);
					iod->iod_state = SMBIOD_ST_DEAD;
					smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
					continue;
				}				
				/* Fall through here and send it */
			case SMBRQ_NOTSENT:
				SMB_IOD_RQUNLOCK(iod);
                /* Indicate that we are not holding the lock */
                drop_req_lock = 0;
				herror = smb_iod_sendrq(iod, rqp);
                if (herror == 0)
                    /*
                     * We will need to go back and reaquire the request queue lock 
                     * and start over, since dropping the lock before sending the 
                     * request, the queue could be in a completely differen state.
                     */
                    goto retry;
				break;
			case SMBRQ_SENT:
				/* if this is an async call then it can't timeout so we are done */
				if (rqp->sr_flags & SMBR_ASYNC)
					break;
				nanouptime(&now);
				if (rqp->sr_share) {
					/*
					 * If its been over SMB_RESP_WAIT_TIMO (60 seconds) since 
                     * the last time we received a message from the server and 
                     * its been over SMB_RESP_WAIT_TIMO since we sent this 
                     * message break the connection. Let the reconnect code 
                     * handle breaking the connection and cleaning up.
                     *
                     * The rqp->sr_timo field was intended to have variable time
                     * out lengths, but never implemented. This code handles 
                     * time outs on a share. Negotiate, SessionSetup, Logout,
                     * etc, timeouts are handled below with the 
                     * SMB_SEND_WAIT_TIMO check.
					 */
					ts = now;
					uetimeout.tv_sec = SMB_RESP_WAIT_TIMO;
					uetimeout.tv_nsec = 0;
					timespecsub(&ts, &uetimeout);
					if (timespeccmp(&ts, &iod->iod_lastrecv, >) && 
                        timespeccmp(&ts, &rqp->sr_timesent, >)) {
						/* See if the connection went down */
						herror = ENOTCONN;
						break;
					}
				}
				/*
				 * Here is the state of things at this point.
				 * 1. We believe the connection is still up.
				 * 2. The server is still responsive.
				 * 3. We have a sent message that has not received a response yet. 
				 *
				 * How long should we wait for a response. In theory forever, but what if 
				 * we never get a response. Should we break the connection or just return 
				 * an error. The old code would wait from 12 to 60 seconds depending 
				 * on the smb message. This seems crazy to me, why should the type of smb 
				 * message matter. I know some writes can take a long time, but if the server
				 * is busy couldn't that happen with any message. We now wait for 2 minutes, if 
				 * time expires we time out the call and log a message to the system log.
				 */
				ts.tv_sec = SMB_SEND_WAIT_TIMO;
				ts.tv_nsec = 0;
				timespecadd(&ts, &rqp->sr_timesent);
				if (timespeccmp(&now, &ts, >)) {
					SMBERROR("Timed out waiting on the response for 0x%x mid = 0x%x\n", rqp->sr_cmd, rqp->sr_mid);
					smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
				} else if (rqp->sr_cmd != SMB_COM_ECHO) {
					ts = now;
					uetimeout.tv_sec = SMBUETIMEOUT;
					uetimeout.tv_nsec = 0;
					timespecsub(&ts, &uetimeout);
					/* Its been 12 seconds since we sent this message send an echo ping */
					if (timespeccmp(&ts, &rqp->sr_timesent, >))
						echo++;
				}
				break;
		    default:
				break;
		}
		if (herror)
			break;
	}
    if (drop_req_lock)
        SMB_IOD_RQUNLOCK(iod);
	if (herror == ENOTCONN) {
		smb_iod_start_reconnect(iod);		
	}	/* no echo message while we are reconnecting */
	else if (echo && ((iod->iod_flags & SMBIOD_RECONNECT) != SMBIOD_RECONNECT)) {
		/*
		 * If the UE timeout has passed since last packet i/o, nudge connection with "echo".  If server
		 * responds iod_lastrecv gets set so we'll avoid bring the connection down.
		 */
		nanouptime(&ts);
		uetimeout.tv_sec = SMBUETIMEOUT;
		uetimeout.tv_nsec = 0;
		timespecsub(&ts, &uetimeout);
		if (timespeccmp(&ts, &iod->iod_lastrecv, >) &&
		    timespeccmp(&ts, &iod->iod_lastrqsent, >))
			(void)smb_echo(vcp, SMBNOREPLYWAIT, 1, iod->iod_context);
	}
	return 0;
}

/*
 * Count the number of active shares on the VC, handle a dead shares,
 * timeout share or notification of unresponsive shares.
 */
static int 
smb_iod_check_for_active_shares(struct smb_vc *vcp)
{
	struct smbiod *	iod = vcp->vc_iod;
	struct smb_share *share, *tshare;
	int treecnt = 0;

	smb_vc_lock(vcp);	/* lock the vc so we can search the list */
	SMBCO_FOREACH_SAFE(share, VCTOCP(vcp), tshare) {
		/*
		 * Since we have the vc lock we know the share can't be freed, but 
		 * it could be going away (Tree Disconnect is in process). Take a 
		 * reference so no one else can disconnect it out from underneath 
		 * us.
		 */
		smb_share_ref(share);
		if (share->ss_flags & SMBO_GONE) {
			/* Skip any shares that are being disconnect */
			smb_share_rele(share, iod->iod_context);
			continue;
		}
		/* This share has a dead timeout value see if they want to disconnect */
		if (share->ss_dead_timer) {
			if (smb_iod_check_timeout(&iod->reconnectStartTime, share->ss_dead_timer)) {
				lck_mtx_lock(&share->ss_shlock);
				if (share->ss_dead) {
					share->ss_dead(share);
					lck_mtx_unlock(&share->ss_shlock);
				} else {
					lck_mtx_unlock(&share->ss_shlock);
					/* Shutdown all outstanding I/O requests on this share. */
					smb_iod_errorout_share_request(share, EPIPE);
				}
				smb_share_rele(share, iod->iod_context);
				continue;
			}
		}
		/* Check for soft mount timeouts */
		if ((share->ss_soft_timer) &&
			(smb_iod_check_timeout(&iod->reconnectStartTime, share->ss_soft_timer))) {
			smb_iod_errorout_share_request(share, ETIMEDOUT);
		}
		
		lck_mtx_lock(&share->ss_shlock);
		if (share->ss_down) {
			treecnt += share->ss_down(share, (iod->iod_flags & SMBIOD_VC_NOTRESP));
		} else {
			treecnt++;
		}
		lck_mtx_unlock(&share->ss_shlock);
		smb_share_rele(share, iod->iod_context);
	}
	smb_vc_unlock(vcp);
	return treecnt;			

}

/*
 * This is called from tcp_connect and smb_iod_reconnect. Durring a reconnect if the volume
 * goes away or someone tries to unmount it then we need to break out of the reconnect. We 
 * may want to use this for normal connections in the future.
 */
int smb_iod_nb_intr(struct smb_vc *vcp)
{
	struct smbiod *	iod = vcp->vc_iod;
	
	/*
	 * If not in reconnect then see if the user applications wants to cancel the
	 * connection.
	 */
	if ((iod->iod_flags & SMBIOD_RECONNECT) != SMBIOD_RECONNECT) {
		if (vcp->connect_flag && (*(vcp->connect_flag) & NSMBFL_CANCEL))
			return EINTR;
		else return 0;
	}
	/* 
	 * We must be in reconnect, check to see if we are offically unresponsive. 
	 * XXX - We should really rework this in the future, if the VC was having
	 * issues before we got here, we may want to have SMBIOD_VC_NOTRESP arlready
	 * set. See <rdar://problem/8124132>
	 */
	if (((iod->iod_flags & SMBIOD_VC_NOTRESP) == 0) &&
		(smb_iod_check_timeout(&iod->reconnectStartTime, NOTIFY_USER_TIMEOUT))) {
			SMB_IOD_FLAGSLOCK(iod);		/* Mark that the VC is not responsive. */
			iod->iod_flags |= SMBIOD_VC_NOTRESP;
			SMB_IOD_FLAGSUNLOCK(iod);						
	}
	/* Don't keep reconnecting if there are no active shares */
	return (smb_iod_check_for_active_shares(vcp)) ? 0 : EINTR;
}

/*
 * The connection went down for some reason. We need to try and reconnect. We need to
 * do a TCP connection and if on port 139 a NetBIOS connection. Any error from the negotiate,
 * setup, or tree connect message is fatal and will bring down the whole process. This routine
 * is run from the main thread, so any message comming down from the file system will block
 * until we are done.
 */
static void smb_iod_reconnect(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	int tree_cnt = 0;
	int error = 0;
	int sleepcnt = 0;
	struct smb_share *share = NULL, *tshare;
	struct timespec waittime, sleeptime, tsnow;
	int ii;

	/* See if we can get a reference on this VC */
	if (smb_vc_reconnect_ref(iod->iod_vc, iod->iod_context)) {
		/* The vc is either gone or going away */
		iod->iod_flags &= ~SMBIOD_RECONNECT;
		iod->iod_workflag = 1;
		SMBERROR("The vc is going aways while we are in reconnect?\n");
		return;
	}

	SMBWARNING("Starting reconnect with %s\n", vcp->vc_srvname);
	SMB_TRAN_DISCONNECT(vcp); /* Make sure the connection is close first */
	iod->iod_state = SMBIOD_ST_CONNECT;
	/* Start the reconnect timers */
	sleepcnt = 1;
	sleeptime.tv_sec = 1;
	sleeptime.tv_nsec = 0;
	nanouptime(&iod->reconnectStartTime);
	/* The number of seconds to wait on a reconnect */
	waittime.tv_sec = vcp->reconnect_wait_time;	
	waittime.tv_nsec = 0;
	timespecadd(&waittime, &iod->reconnectStartTime);
	
	do {
		/* 
		 * The tcp connect will cause smb_iod_nb_intr to be called every two 
		 * seconds. So we always wait 2 two seconds to see if the connection
		 * comes back quickly before attempting any other types of action.
		 */
		error = SMB_TRAN_CONNECT(vcp, vcp->vc_saddr);
		if (error == EINTR)	{
			SMBDEBUG("The reconnection to %s, was canceled\n", vcp->vc_srvname);
			goto exit;
		}

		DBG_ASSERT(vcp->vc_tdata != NULL);
		DBG_ASSERT(error != EISCONN);
		DBG_ASSERT(error != EINVAL);
		if (error) {			
			/* 
			 * Never sleep longer that 1 second at a time, but we can wait up 
			 * to 5 seconds between tcp connections. 
			 */ 
			for (ii= 1; ii <= sleepcnt; ii++) {
				msleep(&iod->iod_flags, 0, PWAIT, "smb_iod_reconnect", &sleeptime);
				if (smb_iod_nb_intr(vcp) == EINTR) {
					error = EINTR;
					SMBDEBUG("The reconnection to %s, was canceled\n", 
							 vcp->vc_srvname);
					goto exit;
				}
			}
			/* Never wait more than 5 seconds between connection attempts */
			if (sleepcnt < SMB_MAX_SLEEP_CNT )
				sleepcnt++;
			SMBWARNING("Retrying connection to %s error = %d\n", vcp->vc_srvname, 
					 error);
		}			
		/* 
		 * We went to sleep during the reconnect and we just woke up. Start the 
		 * reconnect process over again. Reset our start time to now. Reset our 
		 * wait time to be based on the current time. Reset the time we sleep between
		 * reconnect. Just act like we just entered this routine
		 */
		if (iod->reconnectStartTime.tv_sec < gWakeTime.tv_sec) {
			sleepcnt = 1;
			nanouptime(&iod->reconnectStartTime);
			/* The number of seconds to wait on a reconnect */
			waittime.tv_sec = vcp->reconnect_wait_time;
			waittime.tv_nsec = 0;
			timespecadd(&waittime, &iod->reconnectStartTime);
		}
		/*
		 * We now do the negotiate message inside the connect loop. This way if
		 * the negotiate message times out we can keep trying the connection. This
		 * solves the problem of getting disconnected from a server that is going
		 * down, but stays up long enough for us to do a tcp connection.
		 */
		if (!error) {
			/* Clear out the outstanding request counter, everything is going to get resent */
			iod->iod_muxcnt = 0;
			/* Reset the virtual circuit to a reconnect state */
			smb_vc_reset(vcp);
			/* Start the virtual circuit */
			iod->iod_state = SMBIOD_ST_TRANACTIVE;
			error = smb_smb_negotiate(vcp, NULL, TRUE, iod->iod_context);
			if ((error == ENOTCONN) || (error == ETIMEDOUT)) {
				SMBWARNING("The negotiate timed out to %s trying again: error = %d\n", 
						   vcp->vc_srvname, error);
				SMB_TRAN_DISCONNECT(vcp); /* Make sure the connection is close first */
				iod->iod_state = SMBIOD_ST_CONNECT;
			} else if (error) {
				SMBWARNING("The negotiate failed to %s with an error of %d\n", 
						   vcp->vc_srvname, error);
				break;				
			} else {
				SMBDEBUG("The negotiate succeed to %s\n", vcp->vc_srvname);
				iod->iod_state = SMBIOD_ST_NEGOACTIVE;				
				/*
				 * We now do the authentication inside the connect loop. This 
				 * way if the authentication fails because we don't have a 
				 * creditials yet or the creditials have expired, then we can 
				 * keep trying.
				 */
				error = smb_iod_ssnsetup(iod);
				if (error)
					SMBWARNING("The authentication failed to %s with an error of %d\n", 
							   vcp->vc_srvname, error);
				/* If the error isn't EAGAIN then nothing else to do here, we have success or failure */
				if (error != EAGAIN)
					break;
					
				/* Try four more times and see if the user has update the Kerberos Creds */
				for (ii=1; ii < SMB_MAX_SLEEP_CNT; ii++) {
					msleep(&iod->iod_flags, 0, PWAIT, "smb_iod_reconnect", &sleeptime);
					error = smb_iod_ssnsetup(iod);
					if (error)
						SMBWARNING("Retring authentication count %d failed to %s with an error of %d\n", 
								   ii, vcp->vc_srvname, error);
					if (error != EAGAIN)
						break;
				}
				/* If no error then we are done, otherwise break the connection and try again */
				if (error == 0)
					break;
				
				SMB_TRAN_DISCONNECT(vcp); /* Make sure the connection is close first */
				iod->iod_state = SMBIOD_ST_CONNECT;
				error = EAUTH;
			}
		}
		nanouptime(&tsnow);
	} while (error && (timespeccmp(&waittime, &tsnow, >)));
		
	/* reconnect failed or we timed out, nothing left to do cancel the reconnect */
	if (error) {
		SMBWARNING("The connection failed to %s with an error of %d\n", vcp->vc_srvname, error);
		goto exit;
	}
	/*
	 * We now need to reconnect each share. Since the current code only has one share
	 * per virtual circuit there is no problem with locking the list down here. Need
	 * to look at this in the future. If at least one mount point succeeds then do not
	 * close the whole circuit.
	 * We do not wake up smbfs_smb_reopen_file, wait till the very end.
	 */
	tree_cnt = 0;
	smb_vc_lock(vcp);	/* lock the vc so we can search the list */
	SMBCO_FOREACH_SAFE(share, VCTOCP(vcp), tshare) {		
		/*
		 * Since we have the vc lock we know the share can't be freed, but 
		 * it could be going away (Tree Disconnect is in process). Take a 
		 * reference so no one else can disconnect it out from underneath 
		 * us.
		 */
		smb_share_ref(share);
		if (share->ss_flags & SMBO_GONE) {
			/* Skip any shares that are being disconnect */
			smb_share_rele(share, iod->iod_context);
			continue;
		}
			
		/* Always reconnect the tree, even if its not a mount point */
		error = smb_smb_treeconnect(share, iod->iod_context);
		if (error) {
			SMBERROR("Reconnection failed to share %s on server %s error = %d\n", 
					 share->ss_name, vcp->vc_srvname, error);
			error = 0; /* reset the error, only used for logging */
		} else {
			tree_cnt++;
			lck_mtx_lock(&share->ss_shlock);
			if (share->ss_up) {
				share->ss_up(share, TRUE);
				SMBERROR("Reconnected share %s with server %s\n", share->ss_name, vcp->vc_srvname);
			} else {
				SMBWARNING("Reconnected share %s with server %s\n", share->ss_name, vcp->vc_srvname);
			}
			lck_mtx_unlock(&share->ss_shlock);	
		}
		smb_share_rele(share, iod->iod_context);			
	}
	smb_vc_unlock(vcp);
	/* If we have no shares on this connect then kill the whole virtual circuit. */
	if (!tree_cnt) {
		SMBWARNING("No mounted volumes in reconnect, closing connection to server %s\n",vcp->vc_srvname);		
		error = ENOTCONN;
	}

exit:	
	/*
	 * We only want to wake up the shares if we are not trying to do another
	 * reconnect. So if we have no error or the reconnect time is pass the
	 * wake time, then wake up any volumes that are waiting
	 */
	if ((error == 0) || (iod->reconnectStartTime.tv_sec >= gWakeTime.tv_sec)) {
		smb_vc_lock(vcp);	/* lock the vc so we can search the list */
		SMBCO_FOREACH_SAFE(share, VCTOCP(vcp), tshare) {
			smb_share_ref(share);
			lck_mtx_lock(&share->ss_stlock);
			share->ss_flags &= ~SMBS_RECONNECTING;	/* Turn off reconnecting flag */
			lck_mtx_unlock(&share->ss_stlock);
			wakeup(&share->ss_flags);	/* Wakeup the volumes. */
			smb_share_rele(share, iod->iod_context);			
		}					
		smb_vc_unlock(vcp);
	}
	/* 
	 * Remember we are the main thread, turning off the flag will start the process 
	 * going only after we leave this routine.
	 */
	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags &= ~SMBIOD_RECONNECT;
	SMB_IOD_FLAGSUNLOCK(iod);
	if (error)
		SMB_TRAN_DISCONNECT(vcp);
		
	smb_vc_reconnect_rel(vcp);	/* We are done release the reference */
	
	if (error) {
		if (iod->reconnectStartTime.tv_sec < gWakeTime.tv_sec) {
			/*
			 * We went to sleep after the connection, but before the reconnect
			 * completed. Start the whole process over now and see if we can
			 * reconnect.
			 */
			SMBWARNING("The reconnect failed because we went to sleep retrying! %d\n", error);
			iod->iod_state = SMBIOD_ST_RECONNECT;
			smb_iod_start_reconnect(iod); /* Retry the reconnect */
		} else {
			/* We failed; tell the user and have the volume unmounted */
			smb_iod_dead(iod);			
		}
	}

	iod->iod_workflag = 1;
}

/*
 * "main" function for smbiod daemon
 */
static __inline void
smb_iod_main(struct smbiod *iod)
{
	struct smbiod_event *evp;

	SMBIODEBUG("\n");
	/*
	 * Check all interesting events
	 */
	for (;;) {
		SMB_IOD_EVLOCK(iod);
		evp = STAILQ_FIRST(&iod->iod_evlist);
		if (evp == NULL) {
			SMB_IOD_EVUNLOCK(iod);
			break;
		}
		else if (iod->iod_flags & SMBIOD_RECONNECT) {
			/* Ignore any events until reconnect is done */
		    SMB_IOD_EVUNLOCK(iod);
		    break;
		}
		STAILQ_REMOVE_HEAD(&iod->iod_evlist, ev_link);
		evp->ev_type |= SMBIOD_EV_PROCESSING;
		SMB_IOD_EVUNLOCK(iod);
		switch (evp->ev_type & SMBIOD_EV_MASK) {
		    case SMBIOD_EV_NEGOTIATE:
				evp->ev_error = smb_iod_negotiate(iod, evp->ev_ident);
				break;
		    case SMBIOD_EV_SSNSETUP:
				evp->ev_error = smb_iod_ssnsetup(iod);
				break;
		    case SMBIOD_EV_DISCONNECT:
				evp->ev_error = smb_iod_disconnect(iod);
				break;
		    case SMBIOD_EV_SHUTDOWN:
				/*
				 * Flags in iod_flags are only set within the iod,
				 * so we don't need the mutex to protect
				 * setting or clearing them, and SMBIOD_SHUTDOWN
				 * is only tested within the iod, so we don't
				 * need the mutex to protect against other
				 * threads testing it.
				 */
				iod->iod_flags |= SMBIOD_SHUTDOWN;
				break;
		    case SMBIOD_EV_NEWRQ:
				break;
			default:
				break;
		}
		if (evp->ev_type & SMBIOD_EV_SYNC) {
			SMB_IOD_EVLOCK(iod);
			wakeup(evp);
			SMB_IOD_EVUNLOCK(iod);
		} else
			SMB_FREE(evp, M_SMBIOD);
	}
	smb_iod_sendall(iod);
	smb_iod_recvall(iod);
	return;
}

static void smb_iod_thread(void *arg)
{
	struct smbiod *iod = arg;
	vfs_context_t      context;


	/* the iod sets the iod_p to kernproc when launching smb_iod_thread in 
	 * smb_iod_create. Current kpis to cvfscontext support to build a 
	 * context from the current context or from some other context and
	 * not from proc only. So Since the kernel threads run under kernel 
	 * task and kernproc it should be fine to create the context from 
	 * from current thread 
	 */
	
	context = iod->iod_context = vfs_context_create((vfs_context_t)0);

	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags |= SMBIOD_RUNNING;
	SMB_IOD_FLAGSUNLOCK(iod);

	/*
	 * SMBIOD_SHUTDOWN is only set within the iod, so we don't need
	 * the mutex to protect testing it.
	 */
	while ((iod->iod_flags & SMBIOD_SHUTDOWN) == 0) {
		iod->iod_workflag = 0;
		smb_iod_main(iod);
		if (iod->iod_flags & SMBIOD_SHUTDOWN)
			break;
		/* First see if we need to try a reconnect. If not see the VC is not responsive. */
		if ((iod->iod_flags & (SMBIOD_START_RECONNECT | SMBIOD_RECONNECT)) == SMBIOD_RECONNECT)
			smb_iod_reconnect(iod);
		/*
		 * In order to prevent a race here, this should really be locked
		 * with a mutex on which we would subsequently msleep, and
		 * which should be acquired before changing the flag.
		 * Or should this be another flag in iod_flags, using its
		 * mutex?
		 */
		if (iod->iod_workflag)
			continue;
		SMBIODEBUG("going to sleep for %d secs %d nsecs\n", iod->iod_sleeptimespec.tv_sec,
				iod->iod_sleeptimespec.tv_nsec);
		msleep(&iod->iod_flags, 0, PWAIT, "iod thread idle", &iod->iod_sleeptimespec);
	}

	/*
	 * Clear the running flag, and wake up anybody waiting for us to quit.
	 */
	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags &= ~SMBIOD_RUNNING;
	wakeup(iod);
	SMB_IOD_FLAGSUNLOCK(iod);

	vfs_context_rele(context);
}

int
smb_iod_create(struct smb_vc *vcp)
{
	struct smbiod	*iod;
	kern_return_t	result;
	thread_t		thread;

	SMB_MALLOC(iod, struct smbiod *, sizeof(*iod), M_SMBIOD, M_WAITOK | M_ZERO);
	iod->iod_id = smb_iod_next++;
	iod->iod_state = SMBIOD_ST_NOTCONN;
	lck_mtx_init(&iod->iod_flagslock, iodflags_lck_group, iodflags_lck_attr);
	iod->iod_vc = vcp;
	iod->iod_sleeptimespec.tv_sec = SMBIOD_SLEEP_TIMO;
	iod->iod_sleeptimespec.tv_nsec = 0;
	nanouptime(&iod->iod_lastrqsent);
	vcp->vc_iod = iod;
	lck_mtx_init(&iod->iod_rqlock, iodrq_lck_group, iodrq_lck_attr);
	TAILQ_INIT(&iod->iod_rqlist);
	lck_mtx_init(&iod->iod_evlock, iodev_lck_group, iodev_lck_attr);
	STAILQ_INIT(&iod->iod_evlist);
	/* 
	 * The IOCreateThread routine has been depricated. Just copied
	 * that code here
	 */
	result = kernel_thread_start((thread_continue_t)smb_iod_thread, iod, &thread);
	if (result != KERN_SUCCESS) {
		SMBERROR("can't start smbiod result = %d\n", result);
		SMB_FREE(iod, M_SMBIOD);
		return (ENOMEM); 
	}
	thread_deallocate(thread);
	return (0);
}

int
smb_iod_destroy(struct smbiod *iod)
{
	/*
	 * We don't post this synchronously, as that causes a wakeup
	 * when the SMBIOD_SHUTDOWN flag is set, but that happens
	 * before the iod actually terminates, and we have to wait
	 * until it terminates before we can free its locks and
	 * its data structure.
	 */
	smb_iod_request(iod, SMBIOD_EV_SHUTDOWN, NULL);

	/*
	 * Wait for the iod to exit.
	 */
	for (;;) {
		SMB_IOD_FLAGSLOCK(iod);
		if (!(iod->iod_flags & SMBIOD_RUNNING)) {
			SMB_IOD_FLAGSUNLOCK(iod);
			break;
		}
		msleep(iod, SMB_IOD_FLAGSLOCKPTR(iod), PWAIT | PDROP,
		    "iod-exit", 0);
	}
	lck_mtx_destroy(&iod->iod_flagslock, iodflags_lck_group);
	lck_mtx_destroy(&iod->iod_rqlock, iodrq_lck_group);
	lck_mtx_destroy(&iod->iod_evlock, iodev_lck_group);
	SMB_FREE(iod, M_SMBIOD);
	return 0;
}

int
smb_iod_init(void)
{
	return 0;
}

int
smb_iod_done(void)
{
	return 0;
}

