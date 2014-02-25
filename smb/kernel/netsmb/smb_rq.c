/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2013 Apple Inc. All rights reserved.
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
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/kpi_mbuf.h>
#include <sys/mount.h>
#include <libkern/OSAtomic.h>

#include <sys/kauth.h>

#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_packets_2.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_tran.h>

#include <sys/vnode.h>
#include <smbfs/smbfs.h>
#include <netsmb/smb_converter.h>
#include <smbclient/ntstatus.h>

MALLOC_DEFINE(M_SMBRQ, "SMBRQ", "SMB request");

#define ALIGN4(a)	(((a) + 3) & ~3)

/*
 * Given an object return the share or vc associated with that object. If the 
 * object is a share then return the parent which should be the vc. In all cases
 * take a reference on the objects being return.
 */
int
smb_rq_getenv(struct smb_connobj *obj, struct smb_vc **vcp, struct smb_share **share)
{
	int error = 0;
	
	switch (obj->co_level) {
		case SMBL_VC:
			if (obj->co_parent == NULL) {
				SMBERROR("zombie VC %s\n", ((struct smb_vc*)obj)->vc_srvname);
				error = EINVAL;
			} else if (vcp) {
				*vcp = (struct smb_vc*)obj;
				smb_vc_ref(*vcp);
			}
			break;
		case SMBL_SHARE:
			if (obj->co_parent == NULL) {
				SMBERROR("zombie share %s\n", ((struct smb_share*)obj)->ss_name);
				error = EINVAL;
			} else {
				error = smb_rq_getenv(obj->co_parent, vcp, NULL);
				if (!error && share) {
					*share = (struct smb_share *)obj;
					smb_share_ref(*share);
				}
			}
			break;
		default:
			SMBERROR("invalid layer %d passed\n", obj->co_level);
			error = EINVAL;
	}
	return error;
}

static int
smb_rq_new(struct smb_rq *rqp, u_char cmd, uint16_t extraFlags2)
{
	struct mbchain *mbp; 
    struct mdchain *mdp;
	int error;
	
    smb_rq_getrequest(rqp, &mbp);
    smb_rq_getreply(rqp, &mdp);

	rqp->sr_cmd = cmd;
	mb_done(mbp);
	md_done(mdp);
	error = mb_init(mbp);
	if (error)
		return error;
	mb_put_mem(mbp, SMB_SIGNATURE, SMB_SIGLEN, MB_MSYSTEM);
	mb_put_uint8(mbp, cmd);
	mb_put_uint32le(mbp, 0);		/* DosError */
	mb_put_uint8(mbp, rqp->sr_vc->vc_hflags);
	extraFlags2 |= rqp->sr_vc->vc_hflags2;
	if (cmd == SMB_COM_NEGOTIATE)
		extraFlags2 &= ~SMB_FLAGS2_SECURITY_SIGNATURE;
	mb_put_uint16le(mbp, extraFlags2);
	mb_put_uint16le(mbp, rqp->sr_pidHigh);
	/*
	 * The old code would check for SMB_FLAGS2_SECURITY_SIGNATURE, before deciding 
	 * if it need to reserve space for signing. If we are in the middle of reconnect
	 * then SMB_FLAGS2_SECURITY_SIGNATURE may not be set yet. We should always 
	 * reserve and zero the space. It doesn't hurt anything and it means we 
	 * can always handle signing.
	 */
	rqp->sr_rqsig = (uint8_t *)mb_reserve(mbp, 8);
	bzero(rqp->sr_rqsig, 8);
	mb_put_uint16le(mbp, 0);
	
	rqp->sr_rqtid = (uint16_t*)mb_reserve(mbp, sizeof(uint16_t));
	mb_put_uint16le(mbp, rqp->sr_pidLow);
	rqp->sr_rquid = (uint16_t*)mb_reserve(mbp, sizeof(uint16_t));
	mb_put_uint16le(mbp, rqp->sr_mid);
	return 0;
}

/*
 * The object is either a share or a vc in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
static int 
smb_rq_init_internal(struct smb_rq *rqp, struct smb_connobj *obj, u_char cmd, 
					 int rq_flags, uint16_t flags2, vfs_context_t context)
{
	int error;
	uint16_t mid;
	uint16_t low_pid;
	
	bzero(rqp, sizeof(*rqp));
	rqp->sr_flags |= rq_flags;
	lck_mtx_init(&rqp->sr_slock, srs_lck_group, srs_lck_attr);
	error = smb_rq_getenv(obj, &rqp->sr_vc, &rqp->sr_share);
	if (error)
		goto done;
	
	error = smb_vc_access(rqp->sr_vc, context);
	if (error)
		goto done;
	
	rqp->sr_context = context;
    
	/*
	 * We need to get the mid for this request. 
	 */
	mid = OSIncrementAtomic16((int16_t *) &rqp->sr_vc->vc_mid);
	if (mid == 0xffff) {
		/* Reserved for oplock breaks */
		mid = OSIncrementAtomic16((int16_t *) &rqp->sr_vc->vc_mid);
	}
	rqp->sr_mid = mid;

	/*
	 * We need to get the low pid for this request.
	 */
 	if (cmd == SMB_COM_NT_TRANSACT_ASYNC) {
        /* 
         * <14478604> For async SMB_COM_NT_TRANSACT requests, the mid/pid can
         * wrap around and its possible that we will match a reply to the
         * wrong pending request. All other requests will have a pidLow of 1,
         * so make sure that for async requests, the pidLow can never be 1.
         *
         * Example, you have a bunch of Async Change Notifications requests 
         * that have been waiting a long time for a reply.  Then we do a bunch 
         * of ACL requests.  One of those incoming ACL replies could end
         * up matching to a pending Change Notification. The code does figure 
         * out that its not correct, but then it dumps the reply and the ACL 
         * request waits for its reply until the request times out and is
         * retried.  This can cause hangs or long delays during an enumeration. 
         */
        low_pid = OSIncrementAtomic16((int16_t *) &rqp->sr_vc->vc_low_pid);
        if (low_pid == 1) {
            /* Reserved for non async requests */
            low_pid = OSIncrementAtomic16((int16_t *) &rqp->sr_vc->vc_low_pid);
        }

		rqp->sr_pidLow = low_pid;
		rqp->sr_pidHigh = 0;

        cmd = SMB_COM_NT_TRANSACT;
	}
    else {
        /* 
         * All other request just have a hard code value of 1 for PID.
         */
		rqp->sr_pidLow = 1;
		rqp->sr_pidHigh = 0;
	}
   
	error = smb_rq_new(rqp, cmd, flags2);
done:
	if (error) {
		smb_rq_done(rqp);
	}
	return (error);
}

/*
 * The object is either a share or a vc in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
int 
smb_rq_init(struct smb_rq *rqp, struct smb_connobj *obj, u_char cmd, 
				uint16_t flags2, vfs_context_t context)
{
	return smb_rq_init_internal(rqp, obj, cmd, 0, flags2, context);
}

/*
 * The object is either a share or a vc in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
int 
smb_rq_alloc(struct smb_connobj *obj, u_char cmd, uint16_t flags2,
				 vfs_context_t context, struct smb_rq **rqpp)
{
	struct smb_rq *rqp;
	int error;

	SMB_MALLOC(rqp, struct smb_rq *, sizeof(*rqp), M_SMBRQ, M_WAITOK);
	if (rqp == NULL)
		return ENOMEM;
	error = smb_rq_init_internal(rqp, obj, cmd, SMBR_ALLOCED, flags2, context);
	if (! error) {
		/* On error smb_rq_init will clean up rqp memory */
		*rqpp = rqp;
	}
	return error;
}

void
smb_rq_done(struct smb_rq *rqp)
{
	struct mbchain *mbp; 
    struct mdchain *mdp;
	
    smb_rq_getrequest(rqp, &mbp);
    smb_rq_getreply(rqp, &mdp);

    /* 
     * For SMB2, if the request was never sent, then recover the unused credits 
     * If the request was not sent due to reconnect, then no need to recover
     * unused credits as the crediting has been completely reset to start all
     * over.
     */
    if (rqp->sr_extflags & SMB2_REQUEST) {
        if (!(rqp->sr_extflags & SMB2_REQ_SENT) &&
            !(rqp->sr_flags & SMBR_RECONNECTED)) {
            rqp->sr_rspcreditsgranted = rqp->sr_creditcharge;
            smb2_rq_credit_increment(rqp);
        }
    }
    
    if (rqp->sr_share) {
		smb_share_rele(rqp->sr_share, rqp->sr_context);
	}
	if (rqp->sr_vc) {
		smb_vc_rele(rqp->sr_vc, rqp->sr_context);
	}
	rqp->sr_vc = NULL;
	rqp->sr_share = NULL;
	mb_done(mbp);
	md_done(mdp);
	lck_mtx_destroy(&rqp->sr_slock, srs_lck_group);
	if (rqp->sr_flags & SMBR_ALLOCED)
		SMB_FREE(rqp, M_SMBRQ);
}

/*
 * Wait for reply on the request
 * Parse out the SMB Header into the smb_rq response fields
 */
int
smb_rq_reply(struct smb_rq *rqp)
{
	struct mdchain *mdp = NULL;
	uint32_t tdw;
	uint8_t tb;
	int error = 0, rperror = 0;

	/* If an async call then just remove it from the queue, no waiting required */
	if (rqp->sr_flags & SMBR_ASYNC) {
		smb_iod_removerq(rqp);
		error = rqp->sr_lerror;
	} else {
		if (rqp->sr_timo == SMBNOREPLYWAIT) {
            /* Only Echo requests use this */
			return (smb_iod_removerq(rqp));
        }
		error = smb_iod_waitrq(rqp);
	}
	if (error)
		goto done;		
    
    smb_rq_getreply(rqp, &mdp);
    
    if (rqp->sr_extflags & SMB2_RESPONSE) {
        error = smb2_rq_parse_header(rqp, &mdp);
        return (error);
    }
    else {
        /* SMB1 Parsing */
        
        /* Get the Protocol */
        error = md_get_uint32(mdp, &tdw);
        if (error)
            goto done;	
        
        /* Get the Command */
        error = md_get_uint8(mdp, &tb);
        
        /* Get the NT Status */
        if (!error)
            error = md_get_uint32le(mdp, &rqp->sr_ntstatus);
        
        /* Get the Flags */
        if (!error)
            error = md_get_uint8(mdp, &rqp->sr_rpflags);
        
        /* Get the Flags 2 */
        if (!error)
            error = md_get_uint16le(mdp, &rqp->sr_rpflags2);

        /* Convert any error class codes to be NTSTATUS error. */
        if (!(rqp->sr_rpflags2 & SMB_FLAGS2_ERR_STATUS)) {
            uint8_t errClass = rqp->sr_ntstatus & 0xff;
            uint16_t errCode = rqp->sr_ntstatus >> 16;
            rqp->sr_ntstatus = smb_errClassCodes_to_ntstatus(errClass, errCode);
            /* We now have a NTSTATUS error, set the flag */
            rqp->sr_rpflags2 |= SMB_FLAGS2_ERR_STATUS;
        }

        /* Get the PID High */
        if (!error)
            error = md_get_uint16(mdp, &rqp->sr_rppidHigh);

        /* Get the Signature and ignore it */
        if (!error)
            error = md_get_uint32(mdp, NULL);
        if (!error)
            error = md_get_uint32(mdp, NULL);

        /* Get the Reserved bytes and ignore them */
        if (!error)
            error = md_get_uint16(mdp, NULL);

        /* Get the Tree ID */
        if (!error)
            error = md_get_uint16le(mdp, &rqp->sr_rptid);

        /* Get the PID Low */
        if (!error)
            error = md_get_uint16le(mdp, &rqp->sr_rppidLow);

        /* Get the UID */
        if (!error)
            error = md_get_uint16le(mdp, &rqp->sr_rpuid);

        /* Get the Multiplex ID */
        if (!error)
            error = md_get_uint16le(mdp, &rqp->sr_rpmid);
        
        /* If its signed, then verify the signature */
        if (error == 0 &&
            (rqp->sr_vc->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE))
            error = smb_rq_verify(rqp);

        SMBSDEBUG("M:%04x, PHIGH:%04x, PLOW:%04x, U:%04x, T:%04x, E: %d\n",
                  rqp->sr_rpmid, rqp->sr_rppidHigh, rqp->sr_rppidLow, 
                  rqp->sr_rpuid, rqp->sr_rptid, rqp->sr_ntstatus);

        if (error)
            goto done;		
    }

    /* Convert NT Status to an errno value */
    rperror = smb_ntstatus_to_errno(rqp->sr_ntstatus);
    if (rqp->sr_ntstatus == STATUS_INSUFFICIENT_RESOURCES) {
        SMBWARNING("STATUS_INSUFFICIENT_RESOURCES: while attempting cmd %x\n", 
                   rqp->sr_cmd);
    }

	/* The tree has gone away, umount the volume. */
	if ((rperror == ENETRESET) && rqp->sr_share) {
		lck_mtx_lock(&rqp->sr_share->ss_shlock);
		if ( rqp->sr_share->ss_dead)
			rqp->sr_share->ss_dead(rqp->sr_share);
		lck_mtx_unlock(&rqp->sr_share->ss_shlock);
	}
	
    /* Need bigger buffer? */
	if (rperror && (rqp->sr_ntstatus == STATUS_BUFFER_TOO_SMALL)) {
		rqp->sr_flags |= SMBR_MOREDATA;
	} else {
		rqp->sr_flags &= ~SMBR_MOREDATA;
	}
	
done:
	return error ? error : rperror;
}

/*
 * Simple request-reply exchange
 */
int
smb_rq_simple_timed(struct smb_rq *rqp, int timo)
{
	int error;

	rqp->sr_timo = timo;	/* in seconds */
	rqp->sr_state = SMBRQ_NOTSENT;
	error = smb_iod_rq_enqueue(rqp);		
	if (! error)
		error = smb_rq_reply(rqp);
	return (error);
}

int
smb_rq_simple(struct smb_rq *rqp)
{
	return (smb_rq_simple_timed(rqp, rqp->sr_vc->vc_timo));
}

void
smb_rq_wstart(struct smb_rq *rqp)
{
	struct mbchain *mbp;

    smb_rq_getrequest(rqp, &mbp);
	rqp->sr_wcount = (u_char *)mb_reserve(mbp, sizeof(uint8_t));
	rqp->sr_rq.mb_count = 0;
}

void
smb_rq_wend(struct smb_rq *rqp)
{
	if (rqp->sr_wcount == NULL) {
		SMBERROR("no wcount\n");	/* actually panic */
		return;
	}
	if (rqp->sr_rq.mb_count & 1)
		SMBERROR("odd word count\n");
	*rqp->sr_wcount = rqp->sr_rq.mb_count / 2;
}

void
smb_rq_bstart(struct smb_rq *rqp)
{
	struct mbchain *mbp;

    smb_rq_getrequest(rqp, &mbp);
	rqp->sr_bcount = (u_short*)mb_reserve(mbp, sizeof(u_short));
	rqp->sr_rq.mb_count = 0;
}

void
smb_rq_bend(struct smb_rq *rqp)
{
	uint16_t bcnt;

	DBG_ASSERT(rqp->sr_bcount);
	if (rqp->sr_bcount == NULL) {
		SMBERROR("no bcount\n");	/* actually panic */
		return;
	}
	/*
	 * Byte Count field should be ignored when dealing with  SMB_CAP_LARGE_WRITEX 
	 * or SMB_CAP_LARGE_READX messages. So we set it to zero in these cases.
	 */
	if (((VC_CAPS(rqp->sr_vc) & SMB_CAP_LARGE_READX) && (rqp->sr_cmd == SMB_COM_READ_ANDX)) ||
		((VC_CAPS(rqp->sr_vc) & SMB_CAP_LARGE_WRITEX) && (rqp->sr_cmd == SMB_COM_WRITE_ANDX))) {
		/* SAMBA 4 doesn't like the byte count to be zero */
		if (rqp->sr_rq.mb_count > 0x0ffff) 
			bcnt = 0; /* Set the byte count to zero here */
		else
			bcnt = (uint16_t)rqp->sr_rq.mb_count;
	} else if (rqp->sr_rq.mb_count > 0xffff) {
		SMBERROR("byte count too large (%ld)\n", rqp->sr_rq.mb_count);
		bcnt =  0xffff;	/* not sure what else to do here */
	} else
		bcnt = (uint16_t)rqp->sr_rq.mb_count;
	
	*rqp->sr_bcount = htoles(bcnt);
}

int
smb_rq_getrequest(struct smb_rq *rqp, struct mbchain **mbpp)
{
	*mbpp = &rqp->sr_rq;
	return 0;
}

int
smb_rq_getreply(struct smb_rq *rqp, struct mdchain **mbpp)
{
	*mbpp = &rqp->sr_rp;
	return 0;
}

/*
 * The object is either a share or a vc in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
static int 
smb_nt_init(struct smb_ntrq *ntp, struct smb_connobj *obj, int nt_flags, 
			uint16_t fn, vfs_context_t context)
{
	int error;
	
	bzero(ntp, sizeof(*ntp));
	ntp->nt_flags |= nt_flags;
	error = smb_rq_getenv(obj, &ntp->nt_vc, &ntp->nt_share);
	if (error)
		goto done;
	
	ntp->nt_context = context;
	/* obj is either the share or the vc, so we already have a reference on it */
	ntp->nt_obj = obj;
	ntp->nt_function = fn;
done:
	if (error) {
		smb_nt_done(ntp);
	}
	return error;
}

/*
 * The object is either a share or a vc in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
int 
smb_nt_alloc(struct smb_connobj *obj, u_short fn, 
				 vfs_context_t context, struct smb_ntrq **ntpp)
{
	struct smb_ntrq *ntp;
	int error;
	
	SMB_MALLOC(ntp, struct smb_ntrq *, sizeof(*ntp), M_SMBRQ, M_WAITOK);
	if (ntp == NULL)
		return ENOMEM;
	error = smb_nt_init(ntp, obj, SMBT2_ALLOCED, fn, context);
	if (!error) {
		/* On error smb_nt_init will clean up ntp memory */
		*ntpp = ntp;
	}
	return error;
}

void
smb_nt_done(struct smb_ntrq *ntp)
{
	if (ntp->nt_share) {
		smb_share_rele(ntp->nt_share, ntp->nt_context);
	}
	if (ntp->nt_vc) {
		smb_vc_rele(ntp->nt_vc, ntp->nt_context);
	}
	ntp->nt_share = NULL;
	ntp->nt_vc = NULL;
	mb_done(&ntp->nt_tsetup);
	mb_done(&ntp->nt_tparam);
	mb_done(&ntp->nt_tdata);
	md_done(&ntp->nt_rparam);
	md_done(&ntp->nt_rdata);
	if (ntp->nt_flags & SMBT2_ALLOCED)
		SMB_FREE(ntp, M_SMBRQ);
}

/*
 * The object is either a share or a vc in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
static int
smb_t2_init_internal(struct smb_t2rq *t2p, struct smb_connobj *obj, int t2_flags,
					 uint16_t *setup, int setupcnt, vfs_context_t context)
{
	int ii;
	int error;
	
	bzero(t2p, sizeof(*t2p));
	t2p->t2_flags |= t2_flags;
	error = smb_rq_getenv(obj, &t2p->t2_vc, &t2p->t2_share);
	if (error)
		goto done;

	t2p->t2_context = context;
	/* obj is either the share or the vc, so we already have a reference on it */
	t2p->t2_obj = obj;
	t2p->t2_setupcount = setupcnt;
	t2p->t2_setupdata = t2p->t2_setup;
	for (ii = 0; ii < setupcnt; ii++)
		t2p->t2_setup[ii] = setup[ii];
	t2p->t2_fid = 0xffff;
done:
	if (error) {
		smb_t2_done(t2p);
	}
	return error;
}

/*
 * The object is either a share or a vc in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
int
smb_t2_init(struct smb_t2rq *t2p, struct smb_connobj *obj, u_short *setup,
			int setupcnt, vfs_context_t context)
{
	return smb_t2_init_internal(t2p, obj, 0, setup, setupcnt, context);
	
}

/*
 * TRANS2 request implementation
 * TRANS implementation is in the "t2" routines
 * NT_TRANSACTION implementation is the separate "nt" stuff
 *
 * The object is either a share or a vc in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
int 
smb_t2_alloc(struct smb_connobj *obj, u_short setup, int setupcnt, 
				 vfs_context_t context, struct smb_t2rq **t2pp)
{
	struct smb_t2rq *t2p;
	int error;

	SMB_MALLOC(t2p, struct smb_t2rq *, sizeof(*t2p), M_SMBRQ, M_WAITOK);
	if (t2p == NULL)
		return ENOMEM;
	error = smb_t2_init_internal(t2p, obj, SMBT2_ALLOCED, &setup, setupcnt, context);
	if (!error) {
		/* On error smb_t2_init_internal will clean up ntp memory */
		*t2pp = t2p;
	}
	return error;
}

void
smb_t2_done(struct smb_t2rq *t2p)
{
	if (t2p->t2_share) {
		smb_share_rele(t2p->t2_share, t2p->t2_context);
	}
	if (t2p->t2_vc) {
		smb_vc_rele(t2p->t2_vc, t2p->t2_context);
	}
	t2p->t2_share = NULL;
	t2p->t2_vc = NULL;
	mb_done(&t2p->t2_tparam);
	mb_done(&t2p->t2_tdata);
	md_done(&t2p->t2_rparam);
	md_done(&t2p->t2_rdata);
	if (t2p->t2_flags & SMBT2_ALLOCED)
		SMB_FREE(t2p, M_SMBRQ);
}

static int 
smb_t2_placedata(mbuf_t mtop, uint16_t offset, uint16_t count, 
							struct mdchain *mdp)
{
	mbuf_t m, m0;
	uint16_t len = 0;
	size_t check_len = 0;
	

	if (mbuf_split(mtop, offset, MBUF_WAITOK, &m0))
		return EBADRPC;
	/*
	 * We really just wanted to make sure that the chain does not have more 
	 * than count bytes. So count up the bytes and then adjust the mbuf
	 * chain.
	 */ 
	for(m = m0; m; m = mbuf_next(m))
		check_len += mbuf_len(m);

	if (check_len > 0xffff)
		return EINVAL;
	else
		len = (uint16_t)check_len;
	
	if (len > count) {
		/* passing negative value to mbuf_adj trims off the end of the chain. */
		mbuf_adj(m0, count - len);
	}
	else if (len < count) {
		return EINVAL;
	}
	
	if (mdp->md_top == NULL) {
		md_initm(mdp, m0);
	} else
		mbuf_cat_internal(mdp->md_top, m0);
	return 0;
}

static int
smb_t2_reply(struct smb_t2rq *t2p)
{
	struct mdchain *mdp;
	struct smb_rq *rqp = t2p->t2_rq;
	int error, error2, totpgot, totdgot;
	uint16_t totpcount, totdcount, pcount, poff, doff, pdisp, ddisp;
	uint16_t tmp, bc, dcount;
	uint8_t wc;

	t2p->t2_flags &= ~SMBT2_MOREDATA;

	error = smb_rq_reply(rqp);
	if (rqp->sr_flags & SMBR_MOREDATA)
		t2p->t2_flags |= SMBT2_MOREDATA;
	t2p->t2_ntstatus = rqp->sr_ntstatus;
	t2p->t2_sr_rpflags2 = rqp->sr_rpflags2;
	if (error && !(rqp->sr_flags & SMBR_MOREDATA)) {
		return error;
	}
	/*
	 * Now we have to get all subseqent responses, if any.
	 * The CIFS specification says that they can be misordered,
	 * which is weird.
	 * TODO: timo
	 */
	totpgot = totdgot = 0;
	totpcount = totdcount = 0xffff;
    smb_rq_getreply(rqp, &mdp);
	for (;;) {
		m_dumpm(mdp->md_top);
		if ((error2 = md_get_uint8(mdp, &wc)) != 0) {
			break;
		}
		/*
		 * We got a STATUS_BUFFER_OVERFLOW warning and word count is zero, so no
		 * data exist in the buffer. We are done so just return. This should all 
		 * be cleaned up with <rdar://problem/7082077>.
		 */
		if ((wc == 0) && (rqp->sr_rpflags2 & SMB_FLAGS2_ERR_STATUS) && 
			(rqp->sr_ntstatus == STATUS_BUFFER_OVERFLOW)) {
			break;
		}
		
		if (wc < 10) {
			error2 = ENOENT;
			break;
		}
		if ((error2 = md_get_uint16le(mdp, &tmp)) != 0)
			break;
		if (totpcount > tmp)
			totpcount = tmp;
		md_get_uint16le(mdp, &tmp);
		if (totdcount > tmp)
			totdcount = tmp;
		if ((error2 = md_get_uint16le(mdp, &tmp)) != 0 || /* reserved */
		    (error2 = md_get_uint16le(mdp, &pcount)) != 0 ||
		    (error2 = md_get_uint16le(mdp, &poff)) != 0 ||
		    (error2 = md_get_uint16le(mdp, &pdisp)) != 0)
			break;
		if (pcount != 0 && pdisp != totpgot) {
			SMBERROR("Can't handle misordered parameters %d:%d\n",
			    pdisp, totpgot);
			error2 = EINVAL;
			break;
		}
		if ((error2 = md_get_uint16le(mdp, &dcount)) != 0 ||
		    (error2 = md_get_uint16le(mdp, &doff)) != 0 ||
		    (error2 = md_get_uint16le(mdp, &ddisp)) != 0)
			break;
		if (dcount != 0 && ddisp != totdgot) {
			SMBERROR("Can't handle misordered data\n");
			error2 = EINVAL;
			break;
		}
		md_get_uint8(mdp, &wc);
		md_get_uint8(mdp, NULL);
		tmp = wc;
		while (tmp--)
			md_get_uint16(mdp, NULL);
		if ((error2 = md_get_uint16le(mdp, &bc)) != 0)
			break;
		if (dcount) {
			error2 = smb_t2_placedata(mdp->md_top, doff, dcount, &t2p->t2_rdata);
			if (error2)
				break;
		}
		if (pcount) {
			error2 = smb_t2_placedata(mdp->md_top, poff, pcount, &t2p->t2_rparam);
			if (error2)
				break;
		}
		totpgot += pcount;
		totdgot += dcount;
		if (totpgot >= totpcount && totdgot >= totdcount) {
			error2 = 0;
			t2p->t2_flags |= SMBT2_ALLRECV;
			break;
		}
		/*
		 * We're done with this reply, look for the next one.
		 */
		SMBRQ_SLOCK(rqp);
		md_next_record(mdp);
		SMBRQ_SUNLOCK(rqp);
		error2 = smb_rq_reply(rqp);
		if (rqp->sr_flags & SMBR_MOREDATA)
			t2p->t2_flags |= SMBT2_MOREDATA;
		if (!error2)
			continue;
		t2p->t2_ntstatus = rqp->sr_ntstatus;
		t2p->t2_sr_rpflags2 = rqp->sr_rpflags2;
		error = error2;
		if (!(rqp->sr_flags & SMBR_MOREDATA))
			break;
	}
	return (error ? error : error2);
}

int 
smb_nt_reply(struct smb_ntrq *ntp)
{
	struct mdchain *mdp;
	struct smb_rq *rqp = ntp->nt_rq;
	int error, error2;
	uint32_t totpcount, totdcount, pcount, poff, doff, pdisp, ddisp;
	uint32_t tmp, dcount, totpgot, totdgot;
	uint16_t bc;
	uint8_t wc;

	ntp->nt_flags &= ~SMBT2_MOREDATA;

	error = smb_rq_reply(rqp);
	if (rqp->sr_flags & SMBR_MOREDATA)
		ntp->nt_flags |= SMBT2_MOREDATA;
	ntp->nt_status = rqp->sr_ntstatus;
	if (ntp->nt_status == STATUS_NOTIFY_ENUM_DIR) {
		/* STATUS_NOTIFY_ENUM_DIR is not a real error */
		return 0;
	}
	ntp->nt_sr_rpflags2 = rqp->sr_rpflags2;
	if (error && !(rqp->sr_flags & SMBR_MOREDATA))
		return error;
	/*
	 * Now we have to get all subseqent responses. The CIFS specification
	 * says that they can be misordered which is weird.
	 * TODO: timo
	 */
	totpgot = totdgot = 0;
	totpcount = totdcount = 0xffffffff;
    smb_rq_getreply(rqp, &mdp);
	for (;;) {
		m_dumpm(mdp->md_top);
		if ((error2 = md_get_uint8(mdp, &wc)) != 0)
			break;
		if (wc < 18) {
			error2 = ENOENT;
			break;
		}
		md_get_mem(mdp, NULL, 3, MB_MSYSTEM); /* reserved */
		if ((error2 = md_get_uint32le(mdp, &tmp)) != 0)
			break;
		if (totpcount > tmp)
			totpcount = tmp;
		if ((error2 = md_get_uint32le(mdp, &tmp)) != 0)
			break;
		if (totdcount > tmp)
			totdcount = tmp;
		if ((error2 = md_get_uint32le(mdp, &pcount)) != 0 ||
		    (error2 = md_get_uint32le(mdp, &poff)) != 0 ||
		    (error2 = md_get_uint32le(mdp, &pdisp)) != 0)
			break;
		if (pcount != 0 && pdisp != totpgot) {
			SMBERROR("Can't handle misordered parameters %d:%d\n",
			    pdisp, totpgot);
			error2 = EINVAL;
			break;
		}
		if ((error2 = md_get_uint32le(mdp, &dcount)) != 0 ||
		    (error2 = md_get_uint32le(mdp, &doff)) != 0 ||
		    (error2 = md_get_uint32le(mdp, &ddisp)) != 0)
			break;
		if (dcount != 0 && ddisp != totdgot) {
			SMBERROR("Can't handle misordered data\n");
			error2 = EINVAL;
			break;
		}
		md_get_uint8(mdp, &wc);
		tmp = wc;
		while (tmp--)
			md_get_uint16(mdp, NULL);
		if ((error2 = md_get_uint16le(mdp, &bc)) != 0)
			break;
		if (dcount) {
			error2 = smb_t2_placedata(mdp->md_top, doff, dcount,
			    &ntp->nt_rdata);
			if (error2)
				break;
		}
		if (pcount) {
			error2 = smb_t2_placedata(mdp->md_top, poff, pcount,
			    &ntp->nt_rparam);
			if (error2)
				break;
		}
		totpgot += pcount;
		totdgot += dcount;
		if (totpgot >= totpcount && totdgot >= totdcount) {
			error2 = 0;
			ntp->nt_flags |= SMBT2_ALLRECV;
			break;
		}
		DBG_ASSERT((rqp->sr_flags & SMBR_ASYNC) != SMBR_ASYNC)
		/*
		 * We are not doing multiple packets and all the data didn't fit in
		 * this message. Should never happen, but just to make sure.
		 */
		if (!(rqp->sr_flags & SMBR_MULTIPACKET)) {
			SMBWARNING("Not doing multiple message, yet we didn't get all the data?\n");
			error2 = EINVAL;
			break;
		}
		/*
		 * We're done with this reply, look for the next one.
		 */
		SMBRQ_SLOCK(rqp);
		md_next_record(mdp);
		SMBRQ_SUNLOCK(rqp);
		error2 = smb_rq_reply(rqp);
		if (rqp->sr_flags & SMBR_MOREDATA)
			ntp->nt_flags |= SMBT2_MOREDATA;
		if (!error2)
			continue;
		ntp->nt_status = rqp->sr_ntstatus;
		ntp->nt_sr_rpflags2 = rqp->sr_rpflags2;
		error = error2;
		if (!(rqp->sr_flags & SMBR_MOREDATA))
			break;
	}
	return (error ? error : error2);
}


/*
 * Perform a full round of TRANS2 request
 */
int smb_t2_request(struct smb_t2rq *t2p)
{
	uint16_t * txpcountp = NULL;
	uint16_t * txpoffsetp = NULL;
	uint16_t * txdcountp = NULL;
	uint16_t * txdoffsetp = NULL;		
	struct smb_vc *vcp = t2p->t2_vc;
	struct mbchain *mbp;
	struct mdchain *mdp, mbparam, mbdata;
	mbuf_t m;
	struct smb_rq *rqp;
	uint16_t  ii;
	int error;
	size_t check_len;	
	uint16_t totpcount, totdcount, leftpcount, leftdcount;
	uint16_t doff, poff, len, txdcount, txpcount, txmax;

	m = t2p->t2_tparam.mb_top;
	if (m) {
		md_initm(&mbparam, m);	/* do not free it! */
		check_len = m_fixhdr(m);
		if (check_len > 0xffff)		/* maxvalue for u_short */
			return EINVAL;
		totpcount = (uint16_t)check_len;
	} else
		totpcount = 0;
	
	m = t2p->t2_tdata.mb_top;
	if (m) {
		md_initm(&mbdata, m);	/* do not free it! */
		check_len =  m_fixhdr(m);
		if (check_len > 0xffff)
			return EINVAL;
		totdcount = (uint16_t)check_len;
	} else
		totdcount = 0;
	
	leftdcount = totdcount;
	leftpcount = totpcount;
	if (vcp->vc_txmax > 0xffff)		/* maxvalue for u_short */
		txmax = 0xffff;
	else
		txmax = (uint16_t)vcp->vc_txmax;
	
	error = smb_rq_alloc(t2p->t2_obj, t2p->t_name ?
	    SMB_COM_TRANSACTION : SMB_COM_TRANSACTION2, 0, t2p->t2_context, &rqp);
	if (error)
		return error;
	rqp->sr_timo = vcp->vc_timo;
	rqp->sr_flags |= SMBR_MULTIPACKET;
	t2p->t2_rq = rqp;
	rqp->sr_t2 = t2p;
    smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, totpcount);
	mb_put_uint16le(mbp, totdcount);
	mb_put_uint16le(mbp, t2p->t2_maxpcount);
	mb_put_uint16le(mbp, t2p->t2_maxdcount);
	mb_put_uint8(mbp, t2p->t2_maxscount);
	mb_put_uint8(mbp, 0);			/* reserved */
	mb_put_uint16le(mbp, 0);			/* flags */
	mb_put_uint32le(mbp, 0);			/* Timeout */
	mb_put_uint16le(mbp, 0);			/* reserved 2 */

	/* Reserve these field so we can fill them in correctly later */
	txpcountp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));
	txpoffsetp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));
	txdcountp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));
	txdoffsetp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));

	mb_put_uint8(mbp, t2p->t2_setupcount);
	mb_put_uint8(mbp, 0);	/* Reserved */
	for (ii = 0; ii < t2p->t2_setupcount; ii++)
		mb_put_uint16le(mbp, t2p->t2_setupdata[ii]);
	smb_rq_wend(rqp);
	

	smb_rq_bstart(rqp);
	if (t2p->t_name)
		smb_put_dstring(mbp, SMB_UNICODE_STRINGS(vcp), t2p->t_name, PATH_MAX, NO_SFM_CONVERSIONS);
	else	
		mb_put_uint8(mbp, 0);	/* No name so set it to null */

	/* Make sure we are on a four byte alignment, see MS-SMB Section 2.2.12.9 */
	len = mb_fixhdr(mbp);
	mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
	poff = len = mb_fixhdr(mbp);	/* We now have the correct offsets */
	
	if (len + leftpcount > txmax) {
		/* Too much param data, only send what will fit  */
		txpcount = MIN(leftpcount, txmax - len);
		txdcount = 0;	/* No room for data in this message */ 
	} else {
		txpcount = leftpcount;	/* Send all the param data */
		len = ALIGN4(len + txpcount);	/* Make sure the alignment fits */
		txdcount = MIN(leftdcount, txmax - len);
	}
	/*
	 * We now have the amount we are going to send in this message. Update the
	 * left over amount and fill in the param and data count.
	 */
	leftpcount -= txpcount;
	leftdcount -= txdcount;
	*txpcountp = htoles(txpcount);
	*txdcountp = htoles(txdcount);
	
	/* We have the correct parameter offset , fill it in */
	*txpoffsetp = htoles(poff);
	/* Default data offset must be equal to or greater than param offset plus param count */
	doff = poff + txpcount;
	
	if (txpcount) {
		error = md_get_mbuf(&mbparam, txpcount, &m);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
			
		if (txdcount) {
			/* Make sure we are on a four byte alignment, see MS-SMB Section 2.2.12.9 */
			len = mb_fixhdr(mbp);
			mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
			doff = mb_fixhdr(mbp); /* Now get the new data offset */		
		}
	}

	/* We have the correct data offset , fill it in */
	*txdoffsetp = htoles(doff);
	
	if (txdcount) {
		error = md_get_mbuf(&mbdata, txdcount, &m);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
	}
	
	smb_rq_bend(rqp);	/* incredible, but thats it... */
	error = smb_iod_rq_enqueue(rqp);
	if (error) {
		goto freerq;
	}
	if (leftpcount || leftdcount) {
		error = smb_rq_reply(rqp);
		if (error)
			goto bad;
		/* 
		 * this is an interim response, ignore it.
		 */
		SMBRQ_SLOCK(rqp);
        smb_rq_getreply(rqp, &mdp);
		md_next_record(mdp);
		SMBRQ_SUNLOCK(rqp);
	}
	while (leftpcount || leftdcount) {
		t2p->t2_flags |= SMBT2_SECONDARY;
		error = smb_rq_new(rqp, t2p->t_name ? 
		    SMB_COM_TRANSACTION_SECONDARY : SMB_COM_TRANSACTION2_SECONDARY, 0);
		if (error)
			goto bad;
        smb_rq_getrequest(rqp, &mbp);
		smb_rq_wstart(rqp);
		mb_put_uint16le(mbp, totpcount);
		mb_put_uint16le(mbp, totdcount);
		len = mb_fixhdr(mbp);
		/*
		 * now we have known packet size as
		 * ALIGN4(len + 7 * 2 + 2) for T2 request, and -2 for T one,
		 * and need to decide which parts should go into request
		 */
		len = ALIGN4(len + 6 * 2 + 2);
		if (t2p->t_name == NULL)
			len += 2;
		if (len + leftpcount > txmax) {
			txpcount = MIN(leftpcount, txmax - len);
			poff = len;
			txdcount = 0;
			doff = 0;
		} else {
			txpcount = leftpcount;
			poff = txpcount ? len : 0;
			len = ALIGN4(len + txpcount);
			txdcount = MIN(leftdcount, txmax - len);
			doff = txdcount ? len : 0;
		}
		mb_put_uint16le(mbp, txpcount);
		mb_put_uint16le(mbp, poff);
		mb_put_uint16le(mbp, totpcount - leftpcount);
		mb_put_uint16le(mbp, txdcount);
		mb_put_uint16le(mbp, doff);
		mb_put_uint16le(mbp, totdcount - leftdcount);
		leftpcount -= txpcount;
		leftdcount -= txdcount;
		if (t2p->t_name == NULL)
			mb_put_uint16le(mbp, t2p->t2_fid);
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		mb_put_uint8(mbp, 0);	/* name */
		len = mb_fixhdr(mbp);
		if (txpcount) {
			mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
			error = md_get_mbuf(&mbparam, txpcount, &m);
			if (error)
				goto bad;
			mb_put_mbuf(mbp, m);
		}
		len = mb_fixhdr(mbp);
		if (txdcount) {
			mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
			error = md_get_mbuf(&mbdata, txdcount, &m);
			if (error)
				goto bad;
			mb_put_mbuf(mbp, m);
		}
		smb_rq_bend(rqp);
		rqp->sr_state = SMBRQ_NOTSENT;
		error = smb_iod_request(vcp->vc_iod, SMBIOD_EV_NEWRQ, NULL);
		if (error)
			goto bad;
	}	/* while left params or data */
	error = smb_t2_reply(t2p);
	if (error && !(t2p->t2_flags & SMBT2_MOREDATA))
		goto bad;
	mdp = &t2p->t2_rdata;
	if (mdp->md_top) {
		m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
	}
	mdp = &t2p->t2_rparam;
	if (mdp->md_top) {
		m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
	}
bad:
	smb_iod_removerq(rqp);
freerq:
	smb_rq_done(rqp);
	if (error && !(t2p->t2_flags & SMBT2_MOREDATA)) {
		md_done(&t2p->t2_rparam);
		md_done(&t2p->t2_rdata);
	}
	return error;
}


/*
 * Perform a full round of NT_TRANSACTION request
 */
int
smb_nt_request(struct smb_ntrq *ntp)
{
	struct smb_vc *vcp = ntp->nt_vc;
	struct mbchain *mbp;
	struct mdchain *mdp, mbsetup, mbparam, mbdata;
	mbuf_t m;
	struct smb_rq *rqp;
	int error;
	size_t check_len;	
	uint32_t doff, poff, len, txdcount, txpcount;
	uint32_t totscount, totpcount, totdcount;
	uint32_t leftdcount, leftpcount;

	m = ntp->nt_tsetup.mb_top;
	if (m) {
		md_initm(&mbsetup, m);	/* do not free it! */
		check_len = m_fixhdr(m);
		if (check_len > 2 * 0xff)
			return EINVAL;
		totscount = (uint32_t)check_len;
	} else
		totscount = 0;
	
	m = ntp->nt_tparam.mb_top;
	if (m) {
		md_initm(&mbparam, m);	/* do not free it! */
		check_len = m_fixhdr(m);
		if (check_len > 0x7fffffff)
			return EINVAL;
		totpcount = (uint32_t)check_len;
	} else
		totpcount = 0;
	
	m = ntp->nt_tdata.mb_top;
	if (m) {
		md_initm(&mbdata, m);	/* do not free it! */
		check_len = m_fixhdr(m);
		if (check_len > 0x7fffffff)
			return EINVAL;
		totdcount = (uint32_t)check_len;
	} else
		totdcount = 0;
	
	leftdcount = totdcount;
	leftpcount = totpcount;
	error = smb_rq_alloc(ntp->nt_obj, SMB_COM_NT_TRANSACT, 0, ntp->nt_context, &rqp);
	if (error)
		return error;
	rqp->sr_timo = vcp->vc_timo;
	rqp->sr_flags |= SMBR_MULTIPACKET;
	ntp->nt_rq = rqp;
    smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, ntp->nt_maxscount);
	mb_put_uint16le(mbp, 0);	/* reserved (flags?) */
	mb_put_uint32le(mbp, totpcount);
	mb_put_uint32le(mbp, totdcount);
	mb_put_uint32le(mbp, ntp->nt_maxpcount);
	mb_put_uint32le(mbp, ntp->nt_maxdcount);
	check_len = mb_fixhdr(mbp);
	if (check_len > 0x7fffffff) {
		error =  EINVAL;
		goto freerq;
	}
	len = (uint32_t)check_len;
	/*
	 * now we have known packet size as
	 * ALIGN4(len + 4 * 4 + 1 + 2 + ((totscount+1)&~1) + 2),
	 * and need to decide which parts should go into the first request
	 */
	len = ALIGN4(len + 4 * 4 + 1 + 2 + ((totscount+1)&~1) + 2);
	if (len + leftpcount > vcp->vc_txmax) {
		txpcount = MIN(leftpcount, vcp->vc_txmax - len);
		poff = len;
		txdcount = 0;
		doff = 0;
	} else {
		txpcount = leftpcount;
		poff = txpcount ? (uint32_t)len : 0;
		len = ALIGN4(len + txpcount);
		txdcount = MIN(leftdcount, vcp->vc_txmax - len);
		doff = txdcount ? len : 0;
	}
	leftpcount -= txpcount;
	leftdcount -= txdcount;
	mb_put_uint32le(mbp, txpcount);
	mb_put_uint32le(mbp, poff);
	mb_put_uint32le(mbp, txdcount);
	mb_put_uint32le(mbp, doff);
	mb_put_uint8(mbp, (uint8_t)(totscount+1)/2);
	mb_put_uint16le(mbp, ntp->nt_function);
	if (totscount) {
		error = md_get_mbuf(&mbsetup, totscount, &m);
		SMBSDEBUG("%d:%d:%d\n", error, totscount, vcp->vc_txmax);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
		if (totscount & 1)
			mb_put_uint8(mbp, 0); /* setup is in words */
	}
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	check_len = mb_fixhdr(mbp);
	if (txpcount) {
		mb_put_mem(mbp, NULL, ALIGN4(check_len) - check_len, MB_MZERO);
		error = md_get_mbuf(&mbparam, txpcount, &m);
		SMBSDEBUG("%d:%d:%d\n", error, txpcount, vcp->vc_txmax);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
	}
	check_len = mb_fixhdr(mbp);
	if (txdcount) {
		mb_put_mem(mbp, NULL, ALIGN4(check_len) - check_len, MB_MZERO);
		error = md_get_mbuf(&mbdata, txdcount, &m);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
	}
	smb_rq_bend(rqp);	/* incredible, but thats it... */
	error = smb_iod_rq_enqueue(rqp);
	if (error)
		goto freerq;
	if (leftpcount || leftdcount) {
		error = smb_rq_reply(rqp);
		if (error)
			goto bad;
		/* 
		 * this is an interim response, ignore it.
		 */
		SMBRQ_SLOCK(rqp);
        smb_rq_getreply(rqp, &mdp);
		md_next_record(mdp);
		SMBRQ_SUNLOCK(rqp);
	}
	while (leftpcount || leftdcount) {
		mbp = NULL;	/* Just to shutup the static analyze */
		error = smb_rq_new(rqp, SMB_COM_NT_TRANSACT_SECONDARY, 0);
		if (error)
			goto bad;
        smb_rq_getrequest(rqp, &mbp);
		smb_rq_wstart(rqp);
		mb_put_mem(mbp, NULL, 3, MB_MZERO);
		mb_put_uint32le(mbp, totpcount);
		mb_put_uint32le(mbp, totdcount);
		check_len = mb_fixhdr(mbp);
		/*
		 * now we have known packet size as
		 * ALIGN4(len + 6 * 4  + 2)
		 * and need to decide which parts should go into request
		 */
		check_len = ALIGN4(check_len + 6 * 4 + 2);
		if (check_len > 0x7fffffff) {
			error =  EINVAL;
			goto bad;
		}
		len = (uint32_t)check_len;
		if (len + leftpcount > vcp->vc_txmax) {
			txpcount = (uint32_t)MIN(leftpcount, vcp->vc_txmax - len);
			poff = (uint32_t)len;
			txdcount = 0;
			doff = 0;
		} else {
			txpcount = leftpcount;
			poff = txpcount ? len : 0;
			len = ALIGN4(len + txpcount);
			txdcount = MIN(leftdcount, vcp->vc_txmax - len);
			doff = txdcount ? len : 0;
		}
		mb_put_uint32le(mbp, txpcount);
		mb_put_uint32le(mbp, poff);
		mb_put_uint32le(mbp, (uint32_t)(totpcount - leftpcount));
		mb_put_uint32le(mbp, txdcount);
		mb_put_uint32le(mbp, doff);
		mb_put_uint32le(mbp, (uint32_t)(totdcount - leftdcount));
		leftpcount -= txpcount;
		leftdcount -= txdcount;
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		check_len = mb_fixhdr(mbp);
		if (txpcount) {
			mb_put_mem(mbp, NULL, ALIGN4(check_len) - check_len, MB_MZERO);
			error = md_get_mbuf(&mbparam, txpcount, &m);
			if (error)
				goto bad;
			mb_put_mbuf(mbp, m);
		}
		check_len = mb_fixhdr(mbp);
		if (txdcount) {
			mb_put_mem(mbp, NULL, ALIGN4(check_len) - check_len, MB_MZERO);
			error = md_get_mbuf(&mbdata, txdcount, &m);
			if (error)
				goto bad;
			mb_put_mbuf(mbp, m);
		}
		smb_rq_bend(rqp);
		rqp->sr_state = SMBRQ_NOTSENT;
		error = smb_iod_request(vcp->vc_iod, SMBIOD_EV_NEWRQ, NULL);
		if (error)
			goto bad;
	}	/* while left params or data */
	error = smb_nt_reply(ntp);
	if (error && !(ntp->nt_flags & SMBT2_MOREDATA))
		goto bad;
	mdp = &ntp->nt_rdata;
	if (mdp->md_top) {
		m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
	}
	mdp = &ntp->nt_rparam;
	if (mdp->md_top) {
		m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
	}
bad:
	smb_iod_removerq(rqp);
freerq:
	smb_rq_done(rqp);
	if (error && !(ntp->nt_flags & SMBT2_MOREDATA)) {
		md_done(&ntp->nt_rparam);
		md_done(&ntp->nt_rdata);
	}
	return error;
}

/*
 * Perform an Async NT_TRANSACTION request. We only support one message at
 * a time whne doing NT_TRANSACTION async.
 *
 * This is use for NT Notify Change request only currently
 */
int 
smb_nt_async_request(struct smb_ntrq *ntp, void *nt_callback, 
					 void *nt_callback_args)
{
	struct smb_vc *vcp = ntp->nt_vc;
	struct mbchain *mbp;
	struct mdchain mbsetup, mbparam, mbdata;
	mbuf_t m;
	struct smb_rq *rqp;
	int error;
	size_t check_len;	
	uint32_t doff, poff, len, txdcount, txpcount;
	uint32_t totscount, totpcount, totdcount;
	
	m = ntp->nt_tsetup.mb_top;
	if (m) {
		md_initm(&mbsetup, m);	/* do not free it! */
		check_len = m_fixhdr(m);
		if (check_len > 2 * 0xff)
			return EINVAL;
		totscount = (uint32_t)check_len;
	} else
		totscount = 0;
	
	m = ntp->nt_tparam.mb_top;
	if (m) {
		md_initm(&mbparam, m);	/* do not free it! */
		check_len = m_fixhdr(m);
		if (check_len > 0x7fffffff)
			return EINVAL;
		totpcount = (uint32_t)check_len;
	} else
		totpcount = 0;
	
	m = ntp->nt_tdata.mb_top;
	if (m) {
		md_initm(&mbdata, m);	/* do not free it! */
		check_len = m_fixhdr(m);
		if (check_len > 0x7fffffff)
			return EINVAL;
		totdcount = (uint32_t)check_len;
	} else
		totdcount = 0;
	
	error = smb_rq_alloc(ntp->nt_obj, SMB_COM_NT_TRANSACT_ASYNC, 0,
                         ntp->nt_context, &rqp);
	if (error) {
		return error;
    }
    
	rqp->sr_timo = vcp->vc_timo;
    smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, ntp->nt_maxscount);
	mb_put_uint16le(mbp, 0);	/* reserved */
	mb_put_uint32le(mbp, totpcount);
	mb_put_uint32le(mbp, totdcount);
	mb_put_uint32le(mbp, ntp->nt_maxpcount);
	mb_put_uint32le(mbp, ntp->nt_maxdcount);
	check_len = mb_fixhdr(mbp);
	if (check_len > 0x7fffffff) {
		error = EINVAL;
		goto free_rqp;
	}
	len = (uint32_t)check_len;
	/*
	 * now we have known packet size as
	 * ALIGN4(len + 4 * 4 + 1 + 2 + ((totscount+1)&~1) + 2),
	 * now make sure it will all fit in the message
	 */
	len = ALIGN4(len + 4 * 4 + 1 + 2 + ((totscount+1)&~1) + 2);
	if ((len + totpcount + totdcount) > vcp->vc_txmax) {
		error = EINVAL;
		goto free_rqp;
	} 
	txpcount = totpcount;
	poff = txpcount ? (uint32_t)len : 0;
	len = ALIGN4(len + txpcount);
	txdcount = totdcount;
	doff = txdcount ? len : 0;

	mb_put_uint32le(mbp, txpcount);
	mb_put_uint32le(mbp, poff);
	mb_put_uint32le(mbp, txdcount);
	mb_put_uint32le(mbp, doff);
	mb_put_uint8(mbp, (uint8_t)(totscount+1)/2);
	mb_put_uint16le(mbp, ntp->nt_function);
	if (totscount) {
		error = md_get_mbuf(&mbsetup, totscount, &m);
		SMBSDEBUG("%d:%d:%d\n", error, totscount, vcp->vc_txmax);
		if (error)
			goto free_rqp;
		mb_put_mbuf(mbp, m);
		if (totscount & 1)
			mb_put_uint8(mbp, 0); /* setup is in words */
	}
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	check_len = mb_fixhdr(mbp);
	if (txpcount) {
		mb_put_mem(mbp, NULL, ALIGN4(check_len) - check_len, MB_MZERO);
		error = md_get_mbuf(&mbparam, txpcount, &m);
		SMBSDEBUG("%d:%d:%d\n", error, txpcount, vcp->vc_txmax);
		if (error)
			goto free_rqp;
		mb_put_mbuf(mbp, m);
	}
	check_len = mb_fixhdr(mbp);
	if (txdcount) {
		mb_put_mem(mbp, NULL, ALIGN4(check_len) - check_len, MB_MZERO);
		error = md_get_mbuf(&mbdata, txdcount, &m);
		if (error)
			goto free_rqp;
		mb_put_mbuf(mbp, m);
	}
	smb_rq_bend(rqp);	/* incredible, but thats it... */
	rqp->sr_flags |= SMBR_ASYNC;
	rqp->sr_callback_args = nt_callback_args;
	rqp->sr_callback = nt_callback;
	ntp->nt_rq = rqp;
	error = smb_iod_rq_enqueue(rqp);
	if (!error)
		return 0;
	/* else fall through and clean up */
free_rqp:
	ntp->nt_rq = NULL;
	smb_rq_done(rqp);
	return error;
}

