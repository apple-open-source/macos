/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
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
 * $Id: smb_iod.c,v 1.32.80.1 2005/07/20 05:27:00 lindak Exp $
 */
 
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
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

#include <netsmb/smb_compat4.h>

/* XXX make sure they fix IOLib.h */
#include <IOKit/IOLib.h>

#define SMBIOD_SLEEP_TIMO	2
#define	SMBIOD_PING_TIMO	60	/* seconds */

/*
 * After this many seconds we want an unresponded-to request to trigger
 * some sort of UE (dialogue).  If the connection hasn't responded at all
 * in this many seconds then the dialogue is of the "connection isn't
 * responding would you like to force unmount" variety.  If the connection
 * has been responding (to other requests that is) then we need a dialogue
 * of the "operation is still pending do you want to cancel it" variety.
 * At present this latter dialogue does not exist so we have no UE and
 * just keep waiting for the slow operation.
 */
#define SMBUETIMEOUT 8 /* seconds */

#define	SMB_IOD_EVLOCKPTR(iod)	(&((iod)->iod_evlock))
#define	SMB_IOD_EVLOCK(iod)	smb_sl_lock(&((iod)->iod_evlock))
#define	SMB_IOD_EVUNLOCK(iod)	smb_sl_unlock(&((iod)->iod_evlock))

#define	SMB_IOD_RQLOCKPTR(iod)	(&((iod)->iod_rqlock))
#define	SMB_IOD_RQLOCK(iod)	smb_sl_lock(&((iod)->iod_rqlock))
#define	SMB_IOD_RQUNLOCK(iod)	smb_sl_unlock(&((iod)->iod_rqlock))

#define	SMB_IOD_FLAGSLOCKPTR(iod)	(&((iod)->iod_flagslock))
#define	SMB_IOD_FLAGSLOCK(iod)		smb_sl_lock(&((iod)->iod_flagslock))
#define	SMB_IOD_FLAGSUNLOCK(iod)	smb_sl_unlock(&((iod)->iod_flagslock))

#define	smb_iod_wakeup(iod)	wakeup(&(iod)->iod_flags)


MALLOC_DEFINE(M_SMBIOD, "SMBIOD", "SMB network io daemon");

static int smb_iod_next;

static void smb_iod_sockwakeup(struct smbiod *iod);
static int  smb_iod_sendall(struct smbiod *iod);
static int  smb_iod_disconnect(struct smbiod *iod);
static void smb_iod_thread(void *);

static __inline void
smb_iod_rqprocessed(struct smb_rq *rqp, int error, int flags)
{
	SMBRQ_SLOCK(rqp);
	rqp->sr_flags |= flags;
	rqp->sr_lerror = error;
	rqp->sr_rpgen++;
	rqp->sr_state = SMBRQ_NOTIFIED;
	wakeup(&rqp->sr_state);
	SMBRQ_SUNLOCK(rqp);
}

static void
smb_iod_invrq(struct smbiod *iod)
{
	struct smb_rq *rqp;

	/*
	 * Invalidate all outstanding requests for this connection
	 */
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
		smb_iod_rqprocessed(rqp, ENOTCONN, SMBR_RESTART);
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
	struct proc *p = iod->iod_p;

	if (vcp->vc_tdata == NULL)
		return;
	SMB_TRAN_DISCONNECT(vcp, p);
	SMB_TRAN_DONE(vcp, p);
	vcp->vc_tdata = NULL;
}

static void
smb_iod_dead(struct smbiod *iod)
{
	struct smb_rq *rqp;

	iod->iod_state = SMBIOD_ST_DEAD;
	smb_iod_closetran(iod);
	smb_iod_invrq(iod);
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
		if (rqp->sr_share)
			smbfs_dead(rqp->sr_share->ss_mount);
	}
	SMB_IOD_RQUNLOCK(iod);
}

/* XXX */
static int
smb_iod_connect(struct smbiod *iod)
{
	SMBERROR("smb_iod_connect is defunct, FIX CALLER\n");
	return (ENOTCONN);
}

static int
smb_iod_negotiate(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct proc *p = iod->iod_p;
	int error;

	SMBIODEBUG("%d\n", iod->iod_state);
	switch(iod->iod_state) {
	    case SMBIOD_ST_TRANACTIVE:
	    case SMBIOD_ST_NEGOACTIVE:
	    case SMBIOD_ST_SSNSETUP:
		SMBERROR("smb_iod_negotiate is invalid now, state=%d\n",
			 iod->iod_state);
		return EINVAL;
	    case SMBIOD_ST_VCACTIVE:
		SMBERROR("smb_iod_negotiate called when connected\n");
		return EISCONN;
	    case SMBIOD_ST_DEAD:
		return ENOTCONN;	/* XXX: last error code ? */
	    default:
		break;
	}
	iod->iod_state = SMBIOD_ST_RECONNECT;
	vcp->vc_genid++;
	error = 0;
	itry {
		ithrow(SMB_TRAN_CREATE(vcp, p));
		SMBIODEBUG("tcreate\n");
		if (vcp->vc_laddr) {
			ithrow(SMB_TRAN_BIND(vcp, vcp->vc_laddr, p));
		}
		SMBIODEBUG("tbind\n");
		SMB_TRAN_SETPARAM(vcp, SMBTP_SELECTID, iod);
		SMB_TRAN_SETPARAM(vcp, SMBTP_UPCALL, smb_iod_sockwakeup);
		ithrow(SMB_TRAN_CONNECT(vcp, vcp->vc_paddr, p));
		iod->iod_state = SMBIOD_ST_TRANACTIVE;
		SMBIODEBUG("tconnect\n");
/*		vcp->vc_mid = 0;*/
		ithrow(smb_smb_negotiate(vcp, &iod->iod_scred));
		iod->iod_state = SMBIOD_ST_NEGOACTIVE;
		SMBIODEBUG("completed\n");
		smb_iod_invrq(iod);
	} icatch(error) {
		smb_iod_dead(iod);
	} ifinally {
	} iendtry;
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
	error = 0;
	iod->iod_state = SMBIOD_ST_SSNSETUP;
	itry {
		ithrow(smb_smb_ssnsetup(vcp, &iod->iod_scred));
		iod->iod_state = SMBIOD_ST_VCACTIVE;
		SMBIODEBUG("completed\n");
		smb_iod_invrq(iod);
	} icatch(error) {
		/* XXX undo any smb_smb_negotiate side effects? */
		smb_iod_dead(iod);
	} ifinally {
	} iendtry;
	return error;
}

static int
smb_iod_disconnect(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;

	SMBIODEBUG("\n");
	if (iod->iod_state == SMBIOD_ST_VCACTIVE) {
		smb_smb_ssnclose(vcp, &iod->iod_scred);
		iod->iod_state = SMBIOD_ST_TRANACTIVE;
	}
	vcp->vc_smbuid = SMB_UID_UNKNOWN;
	smb_iod_closetran(iod);
	iod->iod_state = SMBIOD_ST_NOTCONN;
	return 0;
}

static int
smb_iod_treeconnect(struct smbiod *iod, struct smb_share *ssp)
{
	int error;

	if (iod->iod_state != SMBIOD_ST_VCACTIVE) {
#if 0
		if (iod->iod_state != SMBIOD_ST_DEAD)
#endif
			return ENOTCONN;
#if 0
		error = smb_iod_connect(iod);
		if (error)
			return error;
#endif
	}
	SMBIODEBUG("tree reconnect\n");
	SMBS_ST_LOCK(ssp);
	ssp->ss_flags |= SMBS_RECONNECTING;
	SMBS_ST_UNLOCK(ssp);
	error = smb_smb_treeconnect(ssp, &iod->iod_scred);
	SMBS_ST_LOCK(ssp);
	ssp->ss_flags &= ~SMBS_RECONNECTING;
	SMBS_ST_UNLOCK(ssp);
	wakeup(&ssp->ss_vcgenid);
	return error;
}

static int
smb_iod_sendrq(struct smbiod *iod, struct smb_rq *rqp)
{
	struct proc *p = iod->iod_p;
	struct smb_vc *vcp = iod->iod_vc;
	struct smb_share *ssp = rqp->sr_share;
	struct mbuf *m;
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
	    case SMBIOD_ST_RECONNECT:
		return 0;
	    case SMBIOD_ST_NEGOACTIVE:
		SMBERROR("smb_iod_sendrq in unexpected state(%d)\n",
			 iod->iod_state);
	    default:
		break;
	}
	if (rqp->sr_sendcnt == 0) {
#ifdef movedtoanotherplace
		if (vcp->vc_maxmux != 0 && iod->iod_muxcnt >= vcp->vc_maxmux)
			return 0;
#endif
		*rqp->sr_rquid = htoles(vcp ? vcp->vc_smbuid : 0);
		/*
		 * This is checking for the case where
		 * "vc_smbuid" was set to 0 in "smb_smb_ssnsetup()";
		 * that happens for requests that occur
		 * after that's done but before we get back the final
		 * session setup reply, where the latter is what
		 * gives us the UID.  (There can be an arbitrary # of
		 * session setup packet exchanges to complete
		 * "extended security" authentication.)
		 *
		 * However, if the server gave us a UID of 0 in a
		 * Session Setup andX reply, and we then do a
		 * Tree Connect andX and get back a TID, we should
		 * use that TID, not 0, in subsequent references to
		 * that tree (e.g., in NetShareEnum RAP requests).
		 *
		 * So, for now, we forcibly zero out the TID only if we're
		 * doing extended security, as that's the only time
		 * that "vc_smbuid" should be explicitly zeroed.
		 *
		 * note we must and do use SMB_TID_UNKNOWN for SMB_COM_ECHO
		 */
		if (vcp && !vcp->vc_smbuid && vcp->vc_hflags2 & SMB_FLAGS2_EXT_SEC)
			*rqp->sr_rqtid = htoles(0);
		else
			*rqp->sr_rqtid = htoles(ssp ? ssp->ss_tid : SMB_TID_UNKNOWN);
		mb_fixhdr(&rqp->sr_rq);
	}
	if (rqp->sr_sendcnt++ >= 60/SMBSBTIMO) { /* one minute */
		smb_iod_rqprocessed(rqp, rqp->sr_lerror, SMBR_RESTART);
		/*
		 * If all attempts to send a request failed, then
		 * something is seriously hosed.
		 */
		return ENOTCONN;
	}
	SMBSDEBUG("M:%04x, P:%04x, U:%04x, T:%04x\n", rqp->sr_mid, 0, 0, 0);
	m_dumpm(rqp->sr_rq.mb_top);
	m = m_copym(rqp->sr_rq.mb_top, 0, M_COPYALL, M_WAIT);
	error = rqp->sr_lerror = m ? SMB_TRAN_SEND(vcp, m, p) : ENOBUFS;
	if (error == 0) {
		nanotime(&rqp->sr_timesent);
		iod->iod_lastrqsent = rqp->sr_timesent;
		rqp->sr_flags |= SMBR_SENT;
		rqp->sr_state = SMBRQ_SENT;
		return 0;
	}
	/*
	 * Check for fatal errors
	 */
	if (SMB_TRAN_FATAL(vcp, error)) {
		/*
		 * No further attempts should be made
		 */
		SMBERROR("TRAN_SEND returned fatal error %d\n", error);
		return ENOTCONN;
	}
	if (error)
		SMBERROR("TRAN_SEND returned non-fatal error %d\n", error);
	if (smb_rq_intr(rqp))
		smb_iod_rqprocessed(rqp, EINTR, 0);
	return 0;
}

/*
 * Process incoming packets
 */
static int
smb_iod_recvall(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct proc *p = iod->iod_p;
	struct smb_rq *rqp;
	struct mbuf *m;
	u_char *hp;
	u_short mid;
	int error;

	switch (iod->iod_state) {
	    case SMBIOD_ST_NOTCONN:
	    case SMBIOD_ST_DEAD:
	    case SMBIOD_ST_RECONNECT:
		return 0;
	    default:
		break;
	}
	for (;;) {
		m = NULL;
		error = SMB_TRAN_RECV(vcp, &m, p);
		if (error == EWOULDBLOCK)
			break;
		if (SMB_TRAN_FATAL(vcp, error)) {
			smb_iod_dead(iod);
			break;
		}
		if (error)
			break;
		if (m == NULL) {
			SMBERROR("tran return NULL without error\n");
			error = EPIPE;
			continue;
		}
		m = m_pullup(m, SMB_HDRLEN);
		if (m == NULL)
			continue;	/* wait for a good packet */
		/*
		 * Now we got an entire and possibly invalid SMB packet.
		 * Be careful while parsing it.
		 */
		m_dumpm(m);
		hp = mtod(m, u_char*);
		if (bcmp(hp, SMB_SIGNATURE, SMB_SIGLEN) != 0) {
			m_freem(m);
			continue;
		}
		mid = SMB_HDRMID(hp);
		SMBSDEBUG("mid %04x\n", (u_int)mid);
		SMB_IOD_RQLOCK(iod);
		nanotime(&iod->iod_lastrecv);
		TAILQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
			if (rqp->sr_mid != mid)
				continue;
			if (rqp->sr_share)
				smbfs_up(rqp->sr_share->ss_mount);
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
			smb_iod_rqprocessed(rqp, 0, 0);
			break;
		}
		SMB_IOD_RQUNLOCK(iod);
		if (rqp == NULL) {
			int cmd = SMB_HDRCMD(hp);

			if (cmd != SMB_COM_ECHO)
				SMBERROR("drop resp: mid %d, cmd %d\n",
					 (u_int)mid, cmd);
/*			smb_printrqlist(vcp);*/
			m_freem(m);
		}
	}
	/*
	 * check for interrupts
	 */
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
		if (smb_sigintr(rqp->sr_cred->scr_vfsctx))
			smb_iod_rqprocessed(rqp, EINTR, 0);
	}
	SMB_IOD_RQUNLOCK(iod);
	return 0;
}

int
smb_iod_request(struct smbiod *iod, int event, void *ident)
{
	struct smbiod_event *evp;
	int error;

	SMBIODEBUG("\n");
	evp = smb_zmalloc(sizeof(*evp), M_SMBIOD, M_WAITOK);
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
	free(evp, M_SMBIOD);
	return error;
}

/*
 * Place request in the queue.
 * Request from smbiod have a high priority.
 */
int
smb_iod_addrq(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct smbiod *iod = vcp->vc_iod;
	struct timespec ts;

	SMBIODEBUG("\n");
	if (rqp->sr_cred == &iod->iod_scred) {
		rqp->sr_flags |= SMBR_INTERNAL;
		SMB_IOD_RQLOCK(iod);
		TAILQ_INSERT_HEAD(&iod->iod_rqlist, rqp, sr_link);
		SMB_IOD_RQUNLOCK(iod);
		for (;;) {
			if (smb_iod_sendrq(iod, rqp) != 0) {
				smb_iod_dead(iod);
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
#ifdef XXX
		error = smb_iod_request(vcp->vc_iod, SMBIOD_EV_CONNECT | SMBIOD_EV_SYNC, NULL);
		if (error)
			return error;
		return EXDEV;
#endif
		if (rqp->sr_share)
			smbfs_dead(rqp->sr_share->ss_mount);
	    case SMBIOD_ST_NOTCONN:
		return ENOTCONN;
	    case SMBIOD_ST_TRANACTIVE:
	    case SMBIOD_ST_NEGOACTIVE:
	    case SMBIOD_ST_SSNSETUP:
		SMBERROR("smb_iod_addrq in unexpected state(%d)\n",
			 iod->iod_state);
	    default:
		break;
	}

	SMB_IOD_RQLOCK(iod);
	for (;;) {
		if (vcp->vc_maxmux == 0) {
			SMBERROR("maxmux == 0\n");
			break;
		}
		if (iod->iod_muxcnt < vcp->vc_maxmux)
			break;
		iod->iod_muxwant++;
		msleep(&iod->iod_muxwant, SMB_IOD_RQLOCKPTR(iod), PWAIT,
		       "iod-rq-mux", 0);
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
	while (rqp->sr_flags & SMBR_XLOCK) {
		rqp->sr_flags |= SMBR_XLOCKWANT;
		msleep(rqp, SMB_IOD_RQLOCKPTR(iod), PWAIT, "iod-rq-rm", 0);
	}
	TAILQ_REMOVE(&iod->iod_rqlist, rqp, sr_link);
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
	if (rqp->sr_rpgen == rqp->sr_rplast)
		msleep(&rqp->sr_state, SMBRQ_SLOCKPTR(rqp), PWAIT, "srs-rq",
			   0);
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
 * Shutdown all outstanding I/O requests on the specified share with
 * ENXIO; used when unmounting a share.  (There shouldn't be any for a
 * non-forced unmount; if this is a forced unmount, we have to shutdown
 * the requests as part of the unmount process.)
 */
void
smb_iod_shutdown_share(struct smb_share *ssp)
{
	struct smbiod *iod = SSTOVC(ssp)->vc_iod;
	struct smb_rq *rqp;

	/*
	 * Loop through the list of requests and shutdown the ones
	 * that are for the specified share.
	 */
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
		if (rqp->sr_state != SMBRQ_NOTIFIED && rqp->sr_share == ssp)
			smb_iod_rqprocessed(rqp, ENXIO, 0);
	}
	SMB_IOD_RQUNLOCK(iod);
}

static int
smb_iod_sendall(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct smb_rq *rqp;
	struct timespec now, ts, tstimeout, uetimeout;
	int herror, echo;

	herror = 0;
	echo = 0;
	/*
	 * Loop through the list of requests and send them if possible
	 */
	SMB_IOD_RQLOCK(iod);
	TAILQ_FOREACH(rqp, &iod->iod_rqlist, sr_link) {
		if (iod->iod_state == SMBIOD_ST_DEAD) {
			herror = ENOTCONN; /* stop everything! */
			break;
		}
		switch (rqp->sr_state) {
		    case SMBRQ_NOTSENT:
			rqp->sr_flags |= SMBR_XLOCK;
			SMB_IOD_RQUNLOCK(iod);
			herror = smb_iod_sendrq(iod, rqp);
			SMB_IOD_RQLOCK(iod);
			rqp->sr_flags &= ~SMBR_XLOCK;
			if (rqp->sr_flags & SMBR_XLOCKWANT) {
				rqp->sr_flags &= ~SMBR_XLOCKWANT;
				wakeup(rqp);
			}
			break;
		    case SMBRQ_SENT:
			SMB_TRAN_GETPARAM(vcp, SMBTP_TIMEOUT, &tstimeout);
			/*
			 * Negative & 0 timeouts on request are overrides.
			 * Otherwise we use the larger of the requested
			 * and the TRAN layer timeout.
			 */
			ts.tv_sec = rqp->sr_timo;
			ts.tv_nsec = 0;
			if (rqp->sr_timo > 0) {
				if (timespeccmp(&ts, &tstimeout, >))
					tstimeout = ts;
			} else {
				if (rqp->sr_timo < 0)
					ts.tv_sec = -rqp->sr_timo;
				tstimeout = ts;
			}
			nanotime(&now);
			if (rqp->sr_share) {
				ts = now;
				uetimeout.tv_sec = SMBUETIMEOUT;
				uetimeout.tv_nsec = 0;
				timespecsub(&ts, &uetimeout);
				if (timespeccmp(&ts, &iod->iod_lastrecv, >) &&
				    timespeccmp(&ts, &rqp->sr_timesent, >))
					smbfs_down(rqp->sr_share->ss_mount);
			}
			timespecadd(&tstimeout, &rqp->sr_timesent);
			if (timespeccmp(&now, &tstimeout, >)) {
				smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
			} else if (rqp->sr_cmd != SMB_COM_ECHO) {
				ts = now;
				uetimeout.tv_sec = SMBUETIMEOUT/2;
				uetimeout.tv_nsec = 0;
				timespecsub(&ts, &uetimeout);
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
	SMB_IOD_RQUNLOCK(iod);
	if (herror == ENOTCONN)
		smb_iod_dead(iod);
	else if (echo) {
		/*
		 * If 1/2 the UE timeout has passed since last packet i/o,
		 * nudge connection with "echo".  If server
		 * responds iod_lastrecv gets set so
		 * we'll avoid doing another smbfs_down above.
		 */
		nanotime(&ts);
		uetimeout.tv_sec = SMBUETIMEOUT/2;
		uetimeout.tv_nsec = 0;
		timespecsub(&ts, &uetimeout);
		if (timespeccmp(&ts, &iod->iod_lastrecv, >) &&
		    timespeccmp(&ts, &iod->iod_lastrqsent, >))
			(void)smb_smb_echo(vcp, &iod->iod_scred,
					   SMBNOREPLYWAIT);
	}
	return 0;
}

static void
smb_tickle(struct smbiod *iod)
{
	struct smb_vc *vcp = iod->iod_vc;
	struct smb_share *ssp = NULL;
	int error;

	/*
	 * Tickle the connection by checking root dir on one of
	 * the shares.  One success is good enough - needn't
	 * tickle all the shares.
	 */
	SMBCO_FOREACH(ssp, VCTOCP(vcp)) {
		/*
		 * Make sure nobody deletes the share out from under
		 * us.
		 */
		smb_share_ref(ssp);
		error = smb_smb_checkdir(ssp, NULL, "", 0, &iod->iod_scred);
		smb_share_rele(ssp, &iod->iod_scred);
		if (!error)
			break;
	}
}

/*
 * "main" function for smbiod daemon
 */
static __inline void
smb_iod_main(struct smbiod *iod)
{
	struct smbiod_event *evp;
	struct timespec tsnow;
	int error;

	SMBIODEBUG("\n");
	error = 0;

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
		STAILQ_REMOVE_HEAD(&iod->iod_evlist, ev_link);
		evp->ev_type |= SMBIOD_EV_PROCESSING;
		SMB_IOD_EVUNLOCK(iod);
		switch (evp->ev_type & SMBIOD_EV_MASK) {
		    case SMBIOD_EV_CONNECT:
			evp->ev_error = smb_iod_connect(iod);
			break;
		    case SMBIOD_EV_NEGOTIATE:
			evp->ev_error = smb_iod_negotiate(iod);
			break;
		    case SMBIOD_EV_SSNSETUP:
			evp->ev_error = smb_iod_ssnsetup(iod);
			break;
		    case SMBIOD_EV_DISCONNECT:
			evp->ev_error = smb_iod_disconnect(iod);
			break;
		    case SMBIOD_EV_TREECONNECT:
			evp->ev_error = smb_iod_treeconnect(iod, evp->ev_ident);
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
		}
		if (evp->ev_type & SMBIOD_EV_SYNC) {
			SMB_IOD_EVLOCK(iod);
			wakeup(evp);
			SMB_IOD_EVUNLOCK(iod);
		} else
			free(evp, M_SMBIOD);
	}
	if (iod->iod_state == SMBIOD_ST_VCACTIVE) {
		nanotime(&tsnow);
		timespecsub(&tsnow, &iod->iod_pingtimo);
		if (timespeccmp(&tsnow, &iod->iod_lastrqsent, >))
			smb_tickle(iod);
	}
	smb_iod_sendall(iod);
	smb_iod_recvall(iod);
	return;
}


void
smb_iod_thread(void *arg)
{
	struct smbiod *iod = arg;
	vfs_context_t      vfsctx;
	boolean_t	funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);

#if 0
	vfsctx.vc_proc = iod->iod_p;
	vfsctx.vc_ucred = vfsctx.vc_proc->p_ucred;
#else
	/* the iod sets the iod_p to kernproc when launching smb_iod_thread in 
	 * smb_iod_create. Current kpis to cvfscontext support to build a 
	 * context from the current context or from some other context and
	 * not from proc only. So Since the kernel threads run under kernel 
	 * task and kernproc it should be fine to create the context from \
	 * from current thread 
	 */
	
	vfsctx = vfs_context_create((vfs_context_t)0);

#endif
	smb_scred_init(&iod->iod_scred, vfsctx);

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
		msleep(&iod->iod_flags, 0, PWAIT, "90idle", &iod->iod_sleeptimespec);
	}

	/*
	 * Clear the running flag, and wake up anybody waiting for us to quit.
	 */
	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags &= ~SMBIOD_RUNNING;
	wakeup(iod);
	SMB_IOD_FLAGSUNLOCK(iod);

	vfs_context_rele(vfsctx);
	(void) thread_funnel_set(kernel_flock, funnel_state);
}

int
smb_iod_create(struct smb_vc *vcp)
{
	struct smbiod *iod;

	iod = smb_zmalloc(sizeof(*iod), M_SMBIOD, M_WAITOK);
	iod->iod_id = smb_iod_next++;
	iod->iod_state = SMBIOD_ST_NOTCONN;
	smb_sl_init(&iod->iod_flagslock, iodflags_lck_group, iodflags_lck_attr);
	iod->iod_vc = vcp;
	iod->iod_sleeptimespec.tv_sec = SMBIOD_SLEEP_TIMO;
	iod->iod_sleeptimespec.tv_nsec = 0;
	iod->iod_pingtimo.tv_sec = SMBIOD_PING_TIMO;
	iod->iod_p = kernproc;
	nanotime(&iod->iod_lastrqsent);
	vcp->vc_iod = iod;
	smb_sl_init(&iod->iod_rqlock, iodrq_lck_group, iodrq_lck_attr);
	TAILQ_INIT(&iod->iod_rqlist);
	smb_sl_init(&iod->iod_evlock, iodev_lck_group, iodev_lck_attr);
	STAILQ_INIT(&iod->iod_evlist);
	if (IOCreateThread(smb_iod_thread, iod) == NULL) {
		SMBERROR("can't start smbiod\n");
		free(iod, M_SMBIOD);
		return (ENOMEM); /* true cause lost in mach interfaces */
	}
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
	smb_sl_destroy(&iod->iod_flagslock, iodflags_lck_group);
	smb_sl_destroy(&iod->iod_rqlock, iodrq_lck_group);
	smb_sl_destroy(&iod->iod_evlock, iodev_lck_group);
	free(iod, M_SMBIOD);
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

