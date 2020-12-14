/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2019 Apple Inc. All rights reserved.
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
#include <sys/reboot.h>

#include <sys/kauth.h>

#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>
#include <netsmb/smb_subr.h>
#include <smbfs/smbfs.h>
#include <netsmb/smb_packets_2.h>
#include <smbclient/ntstatus.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr_2.h>

#include <IOKit/IOLib.h>
#include <netsmb/smb_sleephandler.h>

struct smb_reconnect_stats smb_reconn_stats;

static int smb_iod_next;

int smb_iod_sendall(struct smbiod *iod);
int smb_iod_get_interface_info(struct smbiod *iod);
static void smb_iod_read_thread(void *arg);


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


static int smb_iod_check_non_idempotent(struct smbiod *iod)
{
	int error = 0;
	struct smb_rq *rqp, *trqp;
	struct smb_rq *tmp_rqp;
	int found_create = 0;
	int found_close = 0;
	int found_non_idempotent = 0;

	/* Search through the request list and check for non idempotent requests */
	SMB_IOD_RQLOCK(iod);
	
	TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
		SMBRQ_SLOCK(rqp);
		
		if (!(rqp->sr_flags & SMBR_COMPOUND_RQ)) {
			/* Single Request to be checked */
			if (rqp->sr_extflags & SMB2_NON_IDEMPOTENT) {
				/* Its a non idempotent request so return error */
				switch (rqp->sr_command) {
					case SMB2_CREATE:
						smb_reconn_stats.fail_create_cnt += 1;
						break;
						
					case SMB2_CLOSE:
						smb_reconn_stats.fail_close_cnt += 1;
						break;
						
					case SMB2_LOCK:
						smb_reconn_stats.fail_lock_cnt += 1;
						break;
						
					case SMB2_IOCTL:
						/* Only ioctl that has flag set is for set reparse points */
						smb_reconn_stats.fail_set_reparse_cnt += 1;
						break;
						
					case SMB2_SET_INFO:
						smb_reconn_stats.fail_set_info_cnt += 1;
						break;
						
					default:
						SMBERROR("Unknown non idempotent command 0x%x \n", rqp->sr_command);
						break;
				}
				error = ENETDOWN;
				smb_reconn_stats.fail_non_idempotent_cnt += 1;
			}
		}
		else {
			/* 
			 * Have to check all request in cmpd chain.
			 * The typical compound request would be Create/SomeOp/Close
			 * We have to check for
			 * 1. Whether a middle request is non idempotent
			 * 2. If there is an unmatched Create/Close pair
			 * Note that Create and Close have the SMB2_NON_IDEMPOTENT flag
			 * set on them by default. Its only non idempotent if its an 
			 * unmatched Create/Close pair.
			 */
			found_create = 0;
			found_close = 0;
			found_non_idempotent = 0;
			
			tmp_rqp = rqp;
			while (tmp_rqp != NULL) {
				switch (tmp_rqp->sr_command) {
					case SMB2_CREATE:
						found_create = 1;
						break;
						
					case SMB2_CLOSE:
						found_close = 1;
						break;
						
					case SMB2_LOCK:
						found_non_idempotent = 1;
						smb_reconn_stats.fail_lock_cnt += 1;
						break;
						
					case SMB2_IOCTL:
						/* Only ioctl that has flag set is for set reparse points */
						found_non_idempotent = 1;
						smb_reconn_stats.fail_set_reparse_cnt += 1;
						break;
						
					case SMB2_SET_INFO:
						found_non_idempotent = 1;
						smb_reconn_stats.fail_set_info_cnt += 1;
						break;
						
					default:
						/* Just ignore other commands */
						break;
				}
				
				if (found_non_idempotent == 1) {
					smb_reconn_stats.fail_non_idempotent_cnt += 1;
					error = ENETDOWN;
					break;
				}
				
				tmp_rqp = tmp_rqp->sr_next_rqp;
			}
			
			/* 
			 * Did we find a matching Create/Close pair?
			 * IE either both Create and Close are present or
			 * neither Create or Close are present
			 */
			if (found_create != found_close) {
				smb_reconn_stats.fail_cmpd_create_cnt += 1;
				error = ENETDOWN;
			}
		}

		SMBRQ_SUNLOCK(rqp);
		
		if (error) {
			/* If we found non idempotent req, then we are done */
			break;
		}
	}
	
	SMB_IOD_RQUNLOCK(iod);

	return (error);
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
	}
    else {
		wakeup(&rqp->sr_state);
    }
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
smb_iod_closetran(struct smbiod *iod, int from_read_thread)
{
	struct smb_session *sessionp = iod->iod_session;

    if (sessionp->session_tdata == NULL) {
        return;
    }

    /*
     * Need to halt the read thread before we free the tcp transport.
     * Read thread may be blocked in a socket read, so do a disconnect to
     * unblock it.
     */
	SMB_TRAN_DISCONNECT(sessionp);

    if (from_read_thread == 1) {
        /*
         * Being called from the read thread, just set the state and do not
         * wait for it to exit else you will deadlock
         */
        SMB_IOD_FLAGSLOCK(iod);
        iod->iod_flags |= SMBIOD_READ_THREAD_STOP;
        SMB_IOD_FLAGSUNLOCK(iod);

        /*
         * Dont call SMB_TRAN_DONE() from the read thread as the main
         * iod_thread may still be using the tcp transport. Let the main
         * iod_thread clean up the tcp transport.
         */
    }
    else {
        /*
         * Wait for read thread to exit
         */
        SMB_IOD_FLAGSLOCK(iod);
        for (;;) {
            if (!(iod->iod_flags & SMBIOD_READ_THREAD_RUNNING)) {
                SMB_IOD_FLAGSUNLOCK(iod);
                break;
            }

            /* Tell read thread to exit */
            iod->iod_flags |= SMBIOD_READ_THREAD_STOP;
            wakeup(&(iod->iod_flags));

            msleep(iod, SMB_IOD_FLAGSLOCKPTR(iod), PWAIT,
                   "iod-wait-read-exit", 0);
        }

        /* This will free the tcp transport! */
        SMB_TRAN_DONE(sessionp);
    }
}

static void
smb_iod_dead(struct smbiod *iod, int from_read_thread)
{
	struct smb_rq *rqp, *trqp;

	iod->iod_state = SMBIOD_ST_DEAD;

	smb_iod_closetran(iod, from_read_thread);
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
 * We lost the connection. Set the session flag saying we need to do a reconnect and
 * tell all the shares we are starting reconnect. At this point all non reconnect messages 
 * should block until the reconnect process is completed. This routine is always excuted
 * from the main thread.
 */
static void smb_iod_start_reconnect(struct smbiod *iod, int from_read_thread)
{
	struct smb_share *share, *tshare;
	struct smb_rq *rqp, *trqp;

	/* This should never happen, but for testing lets leave it in */
	if (iod->iod_flags & SMBIOD_START_RECONNECT) {
		SMBWARNING("Already in start reconnect with %s\n", iod->iod_session->session_srvname);
		return; /* Nothing to do here we are already in start reconnect mode */
	}
	
    /*
     * Needed for soft mount timeouts. It will get reset when reconnect
     * actually starts.
     */
    nanouptime(&iod->reconnectStartTime);

    /*
	 * Only start a reconnect on an active sessions or when a reconnect failed because we
	 * went to sleep. If we are in the middle of a connection then mark the connection
	 * as dead and get out.
	 */
	switch (iod->iod_state) {
        case SMBIOD_ST_SESSION_ACTIVE:  /* SMB session established */
        case SMBIOD_ST_RECONNECT_AGAIN: /* betweeen reconnect attempts; sleep happened. */
        case SMBIOD_ST_RECONNECT:       /* reconnect has been started */
            break;
        case SMBIOD_ST_NOTCONN:         /* no connect request was made */
        case SMBIOD_ST_CONNECT:         /* a connect attempt is in progress */
        case SMBIOD_ST_TRANACTIVE:      /* TCP transport level is connected, but SMB session not up yet */
        case SMBIOD_ST_NEGOACTIVE:      /* completed negotiation */
        case SMBIOD_ST_SSNSETUP:        /* started (a) session setup */
        case SMBIOD_ST_DEAD:            /* connection broken, transport is down */
            SMBDEBUG("%s: iod->iod_state = %x iod->iod_flags = 0x%x\n",
                     iod->iod_session->session_srvname, iod->iod_state, iod->iod_flags);
            if (!(iod->iod_flags & SMBIOD_RECONNECT)) {
                smb_iod_dead(iod, from_read_thread);
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
            /* pretend like it did not get sent to recover SMB 2/3 credits */
            rqp->sr_extflags &= ~SMB2_REQ_SENT;
            
			SMBRQ_SUNLOCK(rqp);
			if (rqp->sr_flags & SMBR_ASYNC)
				smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
			else
				smb_iod_rqprocessed(rqp, ENOTCONN, SMBR_DEAD); 
		}
        else {
            /* If SMB 2/3 and soft mount, cancel all requests with ETIMEDOUT */
            if ((rqp->sr_share) &&
                (rqp->sr_extflags & SMB2_REQUEST) &&
                (rqp->sr_share->ss_soft_timer) &&
                (smb_iod_check_timeout(&iod->reconnectStartTime, rqp->sr_share->ss_soft_timer))) {
                /* 
                 * Pretend like it did not get sent to recover SMB 2/3 credits
                 */
                rqp->sr_extflags &= ~SMB2_REQ_SENT;
                
                SMBRQ_SUNLOCK(rqp);
                
                SMBDEBUG("Soft Mount timed out! cmd 0x%x message_id %lld \n",
                         (UInt32) rqp->sr_command, rqp->sr_messageid);
                smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
            }
            else {
                /*
                 * Let the upper layer know that this message was processed 
                 * while we were in reconnect mode. If they receive an error 
                 * they may want to handle this message differently.
                 */
                rqp->sr_flags |= SMBR_RECONNECTED;
                
                /* If we have not received a reply set the state to reconnect */
                if (rqp->sr_state != SMBRQ_NOTIFIED) {
                    rqp->sr_extflags &= ~SMB2_REQ_SENT; /* clear the SMB 2/3 sent flag */
                    rqp->sr_state = SMBRQ_RECONNECT; /* Wait for reconnect to complete */
                    rqp->sr_flags |= SMBR_REXMIT;	/* Tell the upper layer this message was resent */
                    rqp->sr_lerror = 0;		/* We are going to resend clear the error */
                }
                
                SMBRQ_SUNLOCK(rqp);
            }
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
	 * We have the session list locked so the shares can't be remove and they can't
	 * go away. If the share is not gone then mark that we are in reconnect mode.
	 */
	smb_session_lock(iod->iod_session);
	SMBCO_FOREACH_SAFE(share, SESSION_TO_CP(iod->iod_session), tshare) {
		lck_mtx_lock(&share->ss_stlock);
		if (!(share->ss_flags & SMBO_GONE)) {
			share->ss_flags |= SMBS_RECONNECTING;
		}
		lck_mtx_unlock(&(share)->ss_stlock);
	}
	smb_session_unlock(iod->iod_session);
done:
	/* Ok now we can do the reconnect */
	SMB_IOD_FLAGSLOCK(iod);
    iod->iod_state = SMBIOD_ST_RECONNECT;
	iod->iod_flags &= ~SMBIOD_START_RECONNECT;
	SMB_IOD_FLAGSUNLOCK(iod);
}

static int
smb_iod_negotiate(struct smbiod *iod, vfs_context_t user_context)
{
	struct smb_session *sessionp = iod->iod_session;
	int error;
    kern_return_t result;
    thread_t thread;

	SMBIODEBUG("%d\n", iod->iod_state);
	switch(iod->iod_state) {
	    case SMBIOD_ST_TRANACTIVE:
	    case SMBIOD_ST_NEGOACTIVE:
	    case SMBIOD_ST_SSNSETUP:
            SMBERROR("smb_iod_negotiate is invalid now, state=%d\n", iod->iod_state);
            return EINVAL;
	    case SMBIOD_ST_SESSION_ACTIVE:
            SMBERROR("smb_iod_negotiate called when connected\n");
            return EISCONN;
	    case SMBIOD_ST_DEAD:
            return ENOTCONN;	/* XXX: last error code ? */
	    default:
            break;
	}

    iod->iod_state = SMBIOD_ST_CONNECT;

    error = SMB_TRAN_CREATE(sessionp);
	if (error) {
		goto errorOut;
	}
	SMBIODEBUG("tcreate\n");

    /*
     * Start up read thread if not already running
     */
    SMB_IOD_FLAGSLOCK(iod);
    if (!(iod->iod_flags & SMBIOD_READ_THREAD_RUNNING)) {
        SMBIODEBUG("Starting read thread \n");
        result = kernel_thread_start((thread_continue_t)smb_iod_read_thread,
                                     iod, &thread);
        if (result != KERN_SUCCESS) {
            /* Should never happen */
            SMB_IOD_FLAGSUNLOCK(iod);
            SMBERROR("can't start read thread. result = %d\n", result);
            error = ENOTCONN;
            goto errorOut;
        }
        else {
            thread_deallocate(thread);
        }
    }
    SMB_IOD_FLAGSUNLOCK(iod);

	/* We only bind when doing a NetBIOS connection */
	if (sessionp->session_saddr->sa_family == AF_NETBIOS) {
		error = SMB_TRAN_BIND(sessionp, sessionp->session_laddr);
		if (error) {
			goto errorOut;
		}
		SMBIODEBUG("tbind\n");		
	}

	error = SMB_TRAN_CONNECT(sessionp, sessionp->session_saddr);
	if (error == 0) {
		iod->iod_state = SMBIOD_ST_TRANACTIVE;
		SMBIODEBUG("tconnect\n");

        /* Wake up the read thread */
        smb_iod_wakeup(iod);

        error = smb_smb_negotiate(sessionp, user_context, FALSE, iod->iod_context);
	}
	if (error) {
		goto errorOut;
	}
	iod->iod_state = SMBIOD_ST_NEGOACTIVE;
	SMBIODEBUG("completed\n");
	smb_iod_invrq(iod);
	return 0;
	
errorOut:
	smb_iod_dead(iod, 0);
	return error;
}

static int
smb_iod_ssnsetup(struct smbiod *iod, int inReconnect)
{
	struct smb_session *sessionp = iod->iod_session;
	int error;

	SMBIODEBUG("%d\n", iod->iod_state);
	switch(iod->iod_state) {
	    case SMBIOD_ST_NEGOACTIVE:
            break;
	    case SMBIOD_ST_DEAD:
            return ENOTCONN;	/* XXX: last error code ? */
	    case SMBIOD_ST_SESSION_ACTIVE:
            SMBERROR("smb_iod_ssnsetup called when connected\n");
            return EISCONN;
	    default:
            SMBERROR("smb_iod_ssnsetup is invalid now, state=%d\n",
                     iod->iod_state);
		return EINVAL;
	}
	iod->iod_state = SMBIOD_ST_SSNSETUP;
	error = smb_smb_ssnsetup(sessionp, inReconnect, iod->iod_context);
	if (error) {
		/* 
		 * We no longer call smb_io_dead here, the session could still be
		 * alive. Allow for other attempt to authenticate on this same 
		 * session. If the connect went down let the call process
		 * decide what to do with the session.
		 *
		 * Now all we do is reset the iod state back to what it was, but only if
		 * it hasn't change from the time we came in here. If the connection goes
		 * down(server dies) then we shouldn't change the state. 
		 */
		if (iod->iod_state == SMBIOD_ST_SSNSETUP)
			iod->iod_state = SMBIOD_ST_NEGOACTIVE;
	} else {
		iod->iod_state = SMBIOD_ST_SESSION_ACTIVE;
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
	struct smb_session *sessionp = iod->iod_session;

	SMBIODEBUG("\n");
	if (iod->iod_state == SMBIOD_ST_SESSION_ACTIVE) {
		smb_smb_ssnclose(sessionp, iod->iod_context);
        /*
         * Instead of going to SMBIOD_ST_TRANACTIVE, set SMBIOD_ST_NOTCONN
         * as we are going to disconnect now anyways. This keeps the read
         * thread from running inbetween now and setting SMBIOD_ST_NOTCONN
         */
        iod->iod_state = SMBIOD_ST_NOTCONN;
	}
	sessionp->session_smbuid = SMB_UID_UNKNOWN;

	smb_iod_closetran(iod, 0);

	iod->iod_state = SMBIOD_ST_NOTCONN;
	return 0;
}

static int
smb_iod_sendrq(struct smbiod *iod, struct smb_rq *rqp)
{
	struct smb_session *sessionp = iod->iod_session;
	mbuf_t m, m2;
	int error = 0;
    struct smb_rq *tmp_rqp;
	struct mbchain *mbp;

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

    if (rqp->sr_extflags & SMB2_REQUEST) {
        /* filled in by smb2_rq_init_internal */

        smb_rq_getrequest(rqp, &mbp);
        mb_fixhdr(mbp);
        
        if (rqp->sr_flags & SMBR_COMPOUND_RQ) {
            /* 
             * Compound request to send. The first rqp has its sr_next_rq set to 
             * point to the next request to send and so on. The last request will
             * have sr_next_rq set to NULL. The next_command fields should already
             * be filled in with correct offsets. Have to copy all the requests 
             * into a single mbuf chain before sending it.
             *
             * ONLY the first rqp in the chain will have its sr_ fields updated.
             */
            DBG_ASSERT(rqp->sr_next_rqp != NULL);
 #if 0      
 			/* Message ID and credit checking debugging code */     
            if (rqp->sr_creditcharge != letohs(*rqp->sr_creditchargep)) {
                SMBERROR("MID: cmpd Credit charge mismatch %d != %d \n",
                         rqp->sr_creditcharge,
                         letohs(rqp->sr_creditchargep));
                return ENOTCONN;
            }

            if (rqp->sr_messageid != letohq(*rqp->sr_messageidp)) {
                SMBERROR("MID: cmpd mid mismatch %llu != %llu \n",
                         rqp->sr_messageid,
                         letohq(rqp->sr_messageidp));
                return ENOTCONN;
            }

            lck_mtx_lock(&sessionp->session_mid_lock);
            if (sessionp->session_expected_mid != rqp->sr_messageid) {
                SMBERROR("MID: cmpd not expected mid %llu != %llu cmd %d \n",
                         sessionp->session_expected_mid, rqp->sr_messageid,
                         rqp->sr_command);
                SMBERROR("MID: cmpd last mid %llu, charge %d, cmd %d \n",
                         sessionp->session_last_sent_mid,
                         sessionp->session_last_credit_charge,
                         sessionp->session_last_command);
                return ENOTCONN;
            }
            else {
                /* Set the next expected mid */
                sessionp->session_last_sent_mid = rqp->sr_messageid;
                sessionp->session_last_credit_charge = rqp->sr_creditcharge;
                sessionp->session_last_command = rqp->sr_command;
                sessionp->session_expected_mid = rqp->sr_messageid + rqp->sr_creditcharge;
            }
            lck_mtx_unlock(&sessionp->session_mid_lock);

            SMBERROR("Send cmpd MID:%llu chg %d cmd %d \n", rqp->sr_messageid, rqp->sr_creditcharge, rqp->sr_command);
 #endif

          /*
             * Create the first chain
             * Save current sr_rq.mp_top into "m", set sr_rq.mp_top to NULL, 
             * then send "m"
             */
            m = mb_detach(mbp);
            
            /* Concatenate the other requests into the mbuf chain */
            tmp_rqp = rqp->sr_next_rqp;
            while (tmp_rqp != NULL) {
 #if 0      
 				/* Message ID and credit checking debugging code */     
                if (tmp_rqp->sr_creditcharge != letohs(*tmp_rqp->sr_creditchargep)) {
                    SMBERROR("MID: tcmpd Credit charge mismatch %d != %d \n",
                             tmp_rqp->sr_creditcharge,
                             letohs(tmp_rqp->sr_creditchargep));
                    return ENOTCONN;
                }

                if (tmp_rqp->sr_messageid != letohq(*tmp_rqp->sr_messageidp)) {
                    SMBERROR("MID: tcmpd mid mismatch %llu != %llu \n",
                             tmp_rqp->sr_messageid,
                             letohq(tmp_rqp->sr_messageidp));
                    return ENOTCONN;
                }

                lck_mtx_lock(&sessionp->session_mid_lock);
                if (sessionp->session_expected_mid != tmp_rqp->sr_messageid) {
                    SMBERROR("MID: tcmpd not expected mid %llu != %llu cmd %d \n",
                             sessionp->session_expected_mid, tmp_rqp->sr_messageid,
                             tmp_rqp->sr_command);
                    SMBERROR("MID: tcmpd last mid %llu, charge %d, cmd %d \n",
                             sessionp->session_last_sent_mid,
                             sessionp->session_last_credit_charge,
                             sessionp->session_last_command);
                    return ENOTCONN;
                }
                else {
                    /* Set the next expected mid */
                    sessionp->session_last_sent_mid = tmp_rqp->sr_messageid;
                    sessionp->session_last_credit_charge = tmp_rqp->sr_creditcharge;
                    sessionp->session_last_command = tmp_rqp->sr_command;
                    sessionp->session_expected_mid = tmp_rqp->sr_messageid + tmp_rqp->sr_creditcharge;
                }
                lck_mtx_unlock(&sessionp->session_mid_lock);

                SMBERROR("Send tcmpd MID:%llu chg %d cmd %d \n", tmp_rqp->sr_messageid, tmp_rqp->sr_creditcharge, tmp_rqp->sr_command);
#endif
                /* copy next request into new mbuf m2 */
                smb_rq_getrequest(tmp_rqp, &mbp);
                m2 = mb_detach(mbp);
                
                if (m2 != NULL) {
                    /* concatenate m2 to m */
                    m = mbuf_concatenate(m, m2);
                }

                tmp_rqp = tmp_rqp->sr_next_rqp;
            }
            
            /* fix up the mbuf packet header */
            m_fixhdr(m);
        }
        else {
 #if 0      
 			/* Message ID and credit checking debugging code */     
            if (rqp->sr_creditcharge != letohs(*rqp->sr_creditchargep)) {
                SMBERROR("MID: Credit charge mismatch %d != %d \n",
                         rqp->sr_creditcharge,
                         letohs(rqp->sr_creditchargep));
                return ENOTCONN;
            }

            if (rqp->sr_messageid != letohq(*rqp->sr_messageidp)) {
                SMBERROR("MID: mid mismatch %llu != %llu \n",
                         rqp->sr_messageid,
                         letohq(rqp->sr_messageidp));
                return ENOTCONN;
            }

            lck_mtx_lock(&sessionp->session_mid_lock);
            if (rqp->sr_command == SMB2_NEGOTIATE) {
                sessionp->session_expected_mid = 2;
            }
            else {
                if (sessionp->session_expected_mid != rqp->sr_messageid) {
                    SMBERROR("MID: not expected mid %llu != %llu cmd %d \n",
                             sessionp->session_expected_mid, rqp->sr_messageid,
                             rqp->sr_command);
                    SMBERROR("MID: last mid %llu, charge %d, cmd %d \n",
                             sessionp->session_last_sent_mid,
                             sessionp->session_last_credit_charge,
                             sessionp->session_last_command);
                   // return ENOTCONN;
                }
                else {
                    /* Set the next expected mid */
                    sessionp->session_last_sent_mid = rqp->sr_messageid;
                    sessionp->session_last_credit_charge = rqp->sr_creditcharge;
                    sessionp->session_last_command = rqp->sr_command;
                    sessionp->session_expected_mid = rqp->sr_messageid + rqp->sr_creditcharge;
                }
            }
            lck_mtx_unlock(&sessionp->session_mid_lock);

            SMBERROR("Send MID:%llu chg %d cmd %d \n", rqp->sr_messageid, rqp->sr_creditcharge, rqp->sr_command);
#endif
            /* 
             * Not a compound request 
             * Save current sr_rq.mp_top into "m", set sr_rq.mp_top to NULL, 
             * then send "m"
             */
            m = mb_detach(mbp);
        }
    }
    else {
        /* Use to check for session, can't have an iod without a session */
        *rqp->sr_rquid = htoles(sessionp->session_smbuid);
        
        /* If the request has a share then it has a reference on it */
        *rqp->sr_rqtid = htoles(rqp->sr_share ? 
                                rqp->sr_share->ss_tid : SMB_TID_UNKNOWN);

        /*
         * SMB 1
         */
        if (sessionp->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) {
            smb_rq_sign(rqp);
        }
        
        SMBSDEBUG("M:%04x, P:%04x, U:%04x, T:%04x\n", rqp->sr_mid, 0, 0, 0);
        m_dumpm(rqp->sr_rq.mb_top);

        /* SMB 1 always duplicates the sr_rq.mb_top and sends the dupe */
        error = mbuf_copym(rqp->sr_rq.mb_top, 0, MBUF_COPYALL, MBUF_WAITOK, &m);
        DBG_ASSERT(error == 0);
    }
	
    /*
     * NOTE:
     *
     * SMB 1 calls mbuf_copym to create a duplicate mbuf of sr_rq.mp_top
     * to send. If a reconnect happens, then its easy to resend the exact same 
     * packet again by just duplicating sr_rq.mp_top again and sending it again.
     * 
     * For SMB 2/3, the exact same packet can not be sent. After a reconnect
     * the credits reset to 0 and the volatile part of the FID can also change.
     * Thus, the entire packet has to be rebuilt and then resent. Thus, for 
     * SMB 2/3, we do not bother creating a duplicate of the mbuf before
     * sending. This will allow SMB 2/3 to use fewer mbufs.
     */
    
    /* Record the current thread for VFS_CTL_NSTATUS */
    rqp->sr_threadId = thread_tid(current_thread());

    
    /* Call SMB_TRAN_SEND to send the mbufs in "m" */
    error = rqp->sr_lerror = (error) ? error : SMB_TRAN_SEND(sessionp, m);
	if (error == 0) {
		nanouptime(&rqp->sr_timesent);
		rqp->sr_credit_timesent = rqp->sr_timesent;
        iod->iod_lastrqsent = rqp->sr_timesent;
        rqp->sr_state = SMBRQ_SENT;
        
        /* 
         * For SMB 2/3, set flag indicating this request was sent. Used for 
         * keeping track of credits.  
         */
        if (rqp->sr_flags & SMBR_COMPOUND_RQ) {
            rqp->sr_extflags |= SMB2_REQ_SENT;
            tmp_rqp = rqp->sr_next_rqp;
            while (tmp_rqp != NULL) {
                tmp_rqp->sr_extflags |= SMB2_REQ_SENT;
                tmp_rqp = tmp_rqp->sr_next_rqp;
            }
        }
        else {
            rqp->sr_extflags |= SMB2_REQ_SENT;
        }
        
		return 0;
	}
    
	/* Did the connection go down, we may need to reconnect. */
	if (SMB_TRAN_FATAL(sessionp, error)) {
		return ENOTCONN;
    }
	else if (error) {	
		/* 
		* Either the send failed or the mbuf_copym? The mbuf chain got freed and
		* we also failed to send those message IDs for SMB 2, thus for SMB2, just
		* go into reconnect.
		*/
        if (rqp->sr_extflags & SMB2_REQUEST) {
            /* SMB v2/3 */
            SMBERROR("TRAN_SEND returned non-fatal error %d sr_command = 0x%x\n",
                     error, rqp->sr_command);
            error = EIO; /* Couldn't send not much else we can do */
            smb_iod_rqprocessed(rqp, error, 0);
            return ENOTCONN;    /* go into reconnect */
        }
        else {
            /* SMB v1 */
            SMBERROR("TRAN_SEND returned non-fatal error %d sr_cmd = 0x%x\n",
                     error, rqp->sr_cmd);
            error = EIO; /* Couldn't send not much else we can do */
            smb_iod_rqprocessed(rqp, error, 0);
        }
	}
	return 0;
}

// Interogate Server for available interfaces
int
smb_iod_get_interface_info(struct smbiod *iod)
{
    struct smb_session *sessionp     = iod->iod_session;
    struct vfs_context *piod_context = iod->iod_context;
    int error = 0;
    
    if (!(sessionp->session_flags & SMBV_MULTICHANNEL_ON)) {
        SMBERROR("incorrect sessionp->session_flags (0x%x) ", sessionp->session_flags);
        return EINVAL;
    }

    if (iod->iod_state != SMBIOD_ST_SESSION_ACTIVE) {
        SMBERROR("incorrect iod->iod_state mode (%d) ", iod->iod_state);
        return EINVAL;
    }

    struct smb_share *share = NULL;
    error = smb_get_share(sessionp, &share);
    if (error || (share == NULL)) {
        SMBERROR("share is null \n");
        return EINVAL;
    }

    error = smb2fs_smb_validate_neg_info(share, piod_context);
    if (error) {
        SMBERROR("smb2fs_smb_validate_neg_info returned %d\n", error);
        goto exit;
    }
    
    error = smb2fs_smb_query_network_interface_info(share, piod_context);
    if (error) {
        SMBERROR("smb2fs_smb_query_network_interface_info returned %d\n", error);
        goto exit;
    }

exit:
    if (share) {
        smb_share_rele(share, piod_context);
    }
    return error;
}

    
/*
 * Process incoming packets
 */
static int
smb_iod_recvall(struct smbiod *iod)
{
	struct smb_session *sessionp = iod->iod_session;
	struct smb_rq *rqp, *trqp, *temp_rqp, *cmpd_rqp;
	mbuf_t m;
	u_char *hp;
	uint16_t mid = 0;
	uint16_t pidHigh = 0;
	uint16_t pidLow = 0;
	uint8_t cmd = 0;
	int error;
    struct smb2_header* smb2_hdr;
    boolean_t smb2_packet;
    uint64_t message_id = 0;
    boolean_t smb1_allowed = true;
	struct mdchain *mdp = NULL;
    int skip_wakeup = 0;
    int drop_req_lock = 0;

	for (;;) {
        m = NULL;
        smb2_packet = false;
        cmpd_rqp = NULL;
        temp_rqp = NULL;
        skip_wakeup = 0;

        /* this reads in the entire response packet based on the NetBIOS hdr */
		error = SMB_TRAN_RECV(sessionp, &m);
        if (error) {
            if (error == EWOULDBLOCK) {
                break;
            }
            else {
                SMBERROR("SMB_TRAN_RECV failed %d\n", error);
                if (SMB_TRAN_FATAL(sessionp, error)) {
                    /* Tell send thread to start reconnect */
                    SMBWARNING("Calling smb_iod_start_reconnect. iod_state %d \n",
                               iod->iod_state);
                    smb_iod_start_reconnect(iod, 1);
                }
                break;
            }
        }

		if (m == NULL) {
			SMBERROR("SMB_TRAN_RECV returned NULL without error\n");
			continue;
		}
        
        /*
         * It's possible the first mbuf in the chain
         * has zero length, so we will do the pullup
         * now, fixes <rdar://problem/17166274>.
         *
         * Note: Ideally we would simply pullup SMB2_HDRLEN bytes,
         * here, but we still have to support SMB 1, which has
         * messages less than 64 bytes (SMB2_HDRLEN). Once we
         * remove SMB 1 support, we can change this pullup to
         * SMB2_HDRLEN bytes, and remove the additional pullup
         * in the "SMB 2/3 Response packet" block below.
         */
        if (mbuf_pullup(&m, SMB_HDRLEN)) {
            continue;
        }
        
        /*
         * Parse out enough of the response to be able to match it with an 
         * existing smb_rq in the queue.
         */

        /* 
         * For SMB 2/3, client sends out a SMB 1 Negotiate request, but the
         * server replies with a SMB 2/3 Negotiate response that has no mid
         * and a pid of 0.  Have to just match it to any Negotiate request
         * waiting for a response. 
         */

        m_dumpm(m);
        hp = mbuf_data(m);
        
		if (*hp == 0xfe) {
            /* 
             * SMB 2/3 Response packet
             */

            /* Wait for entire header to be read in */
            if (mbuf_pullup(&m, SMB2_HDRLEN))
                continue;
            
            hp = mbuf_data(m);

            /* Verify SMB 2/3 signature */
            if (bcmp(hp, SMB2_SIGNATURE, SMB2_SIGLEN) != 0) {
                SMBERROR("dumping non SMB 2/3 packet\n");
                mbuf_freem(m);
                continue;
            }

            /* this response is an SMB 2/3 response */
            smb2_packet = true;
            
            /* 
             * Once using SMB 2/3, ignore any more SMB 1 responses
             */
            if (smb1_allowed)
                smb1_allowed = false;
            
            /*
             * At this point we have the SMB 2/3 Header and packet data read in
             * Get the Message ID so we can find the matching smb_rq
             */
            m_dumpm(m);
            smb2_hdr = mbuf_data(m);

            cmd = letohs(smb2_hdr->command);
            message_id = letohq(smb2_hdr->message_id);
            SMBSDEBUG("message_id %lld cmd = %d\n", letohq(message_id), cmd);
		}
        else {            
            /* 
             * SMB 1 Response packet
             */
            
            /*
             * We don't need to call mbuf_pullup(&m, SMB_HDRLEN),
             * since it was already done above.
             */
            
            hp = mbuf_data(m);

            /* Verify SMB 1 signature */
            if (bcmp(hp, SMB_SIGNATURE, SMB_SIGLEN) != 0) {
                SMBERROR("dumping non SMB 1 packet\n");
                mbuf_freem(m);
                continue;
            }

            /* if no more SMB 1 packets allowed, then ignore this packet */
            if (!smb1_allowed) {
                SMBERROR("No more SMB 1 packets allowed, dumping request\n");
                mbuf_freem(m);
                continue;
            }
            
            /*
             * At this point we have the SMB 1 Header and packet data read in
             * Get the cmd, mid, pid so we can find the matching smb_rq
             */
            mid = SMB_HDRMID(hp);
            cmd = SMB_HDRCMD(hp);
            pidHigh = SMB_HDRPIDHIGH(hp);
            pidLow = SMB_HDRPIDLOW(hp);
            SMBSDEBUG("mid %04x cmd = 0x%x\n", (unsigned)mid, cmd);
        }

        /*
         * Search queue of smb_rq to find a match for this reply
         *
         * If we find a matching rqp, wake up the thread waiting for the reply
         * and break out of the TAILQ_FOREACH_SAFE loop as we are done.
         *
         * Otherwise we search all rqp's, not find any matches, and exit the
         * TAILQ_FOREACH_SAFE loop with rqp = null.
         */
        SMB_IOD_RQLOCK(iod);
        drop_req_lock = 1;

        nanouptime(&iod->iod_lastrecv);
		TAILQ_FOREACH_SAFE(rqp, &iod->iod_rqlist, sr_link, trqp) {
            if (smb2_packet) {
                if (rqp->sr_messageid == message_id) {
                    /*
                     * Matched non compound rqp or matched first rqp in a
                     * compound rqp.
                     */
                    
                    /*
                     * If sent compound req, and this is not an 
                     * Async/STATUS_PENDING reply, then we should have gotten 
                     * a compound response
                     */
                    if ((rqp->sr_flags & SMBR_COMPOUND_RQ) &&
                        (smb2_hdr->next_command == 0) &&
                        !((smb2_hdr->flags & SMBR_ASYNC) && (smb2_hdr->status == STATUS_PENDING))) {
                        
                        if (!(sessionp->session_misc_flags & SMBV_NON_COMPOUND_REPLIES)) {
                            /*
                             * <14227703> Some NetApp servers send back non
                             * compound replies to compound requests. Sigh.
                             */
                            SMBWARNING("Non compound reply to compound req. message_id %lld, cmd %d\n",
                                       message_id, cmd);
                            
                            /* Once set, this remains set forever */
                            sessionp->session_misc_flags |= SMBV_NON_COMPOUND_REPLIES;
                        }
                        
                        /*
                         * Must be first non compound reply to a compound 
                         * request, thus there must be more replies pending.
                         */
                        cmpd_rqp = rqp; /* save start of compound rqp */
                    }
                }
                else {
                    /*
                     * Did not match non compound rqp or did not match
                     * first rqp in a compound rqp
                     */
                    if (!(rqp->sr_flags & SMBR_COMPOUND_RQ) ||
                        !(sessionp->session_misc_flags & SMBV_NON_COMPOUND_REPLIES)) {
                        /* 
                         * Not a compound request or server supports compound 
                         * replies, go check next rqp 
                         */
                        continue;
                    }
                    
                    /* 
                     * <14227703> Server is using non compound replies, so have
                     * to search each request chain to see if this reply matches 
                     * a middle or last request in the chain.
                     */
                    cmpd_rqp = rqp;     /* save start of compound rqp */
                    
                    temp_rqp = rqp->sr_next_rqp;
                    while (temp_rqp != NULL) {
                        if (temp_rqp->sr_messageid == message_id) {
                            /* Matches a rqp in this chain */
                            rqp = temp_rqp;
                            break;
                        }
                        temp_rqp = temp_rqp->sr_next_rqp;
                    }
                    
                    if (temp_rqp == NULL) {
                        /* Not found in this compound rqp */
                        cmpd_rqp = NULL;
                        continue;
                    }
                }
                
                /* Verify that found smb_rq is a SMB 2/3 request */
                if (!(rqp->sr_extflags & SMB2_REQUEST) &&
                    (cmd != SMB2_NEGOTIATE)) {
                    SMBERROR("Found non SMB 2/3 request? message_id %lld, cmd %d\n",
                             message_id, cmd);
                }
                
                rqp->sr_extflags |= SMB2_RESPONSE;

                /* Dropping the RQ lock here helps performance */
                SMB_IOD_RQUNLOCK(iod);
                /* Indicate that we are not holding the lock */
                drop_req_lock = 0;
            }
            else {
                /* 
                 * <12071582>
                 * We now use the mid and the low pid as a single mid, this gives
                 * us a larger mid and helps prevent finding the wrong item. So we
                 * need to make sure the messages match up, so use the cmd to confirm
                 * we have the correct message.
                 *
                 * NOTE: SMB 2/3 does not have this issue.
                 */
                if ((rqp->sr_mid != mid) ||
                    (rqp->sr_cmd != cmd) ||
                    (rqp->sr_pidHigh != pidHigh) ||
                    (rqp->sr_pidLow != pidLow)) {
                    continue;
                }
            }
            
            /*
             * We received a packet on the session, make sure main iod thread
             * is awake and ready to go. This improves read performance on
             * 10 gigE non jumbo setups.
             */
            iod->iod_workflag = 1;

            /*
             * Found a matching smb_rq
             */
            if ((rqp->sr_share) &&
                (rqp->sr_share->ss_mount) &&
                (rqp->sr_share->ss_mount->sm_status & SM_STATUS_DOWN)) {
				lck_mtx_lock(&rqp->sr_share->ss_shlock);
				if (rqp->sr_share->ss_up)
					rqp->sr_share->ss_up(rqp->sr_share, FALSE);
				lck_mtx_unlock(&rqp->sr_share->ss_shlock);
			}

            if (smb2_packet) {
                /* 
                 * Check for Async and STATUS_PENDING responses.
                 * Ignore this response and wait for the real one 
                 * to arrive later
                 */
                if ((smb2_hdr->flags & SMBR_ASYNC) && 
                    (smb2_hdr->status == STATUS_PENDING)) {
                    rqp->sr_rspasyncid = letohq(smb2_hdr->async.async_id);
                    rqp->sr_rspcreditsgranted = letohs(smb2_hdr->credit_reqrsp);
                    
                    /* Get granted credits from this response */
                    smb2_rq_credit_increment(rqp);
					
					/* Since we got STATUS_PENDING, reset send timeout */
					nanouptime(&rqp->sr_timesent);
					
                    rqp = NULL;
                    break;
                }
            } 

            /* 
             * For compound replies received,
             * ONLY the first rqp in the chain will have ALL the reply data
             * in its mbuf chains. Its up to the upper layers to parse out
             * the extra SMB 2/3 headers and know how to parse the SMB 2/3 reply
             * data.
             *
             * Note: an alternate way to do this would be to somehow split up
             * each of the replies into the seperate rqp's. 
             *
             * <14227703> Some NetApp servers do not support compound replies.  
             * Those replies are in each rqp in the chain.
             */

            SMBRQ_SLOCK(rqp);

            smb_rq_getreply(rqp, &mdp);
            if (rqp->sr_rp.md_top == NULL) {
                md_initm(mdp, m);
            } 
            else {
                if (rqp->sr_flags & SMBR_MULTIPACKET) {
                    md_append_record(mdp, m);
                } 
                else {
                    SMBRQ_SUNLOCK(rqp);
                    SMBERROR("duplicate response %d (ignored)\n", mid);
                    break;
                }
            }
            
            /* 
             * <14227703> For servers that do not support compound replies, 
             * check to see if entire reply has arrived.
             */
            if (sessionp->session_misc_flags & SMBV_NON_COMPOUND_REPLIES) {
                if (cmpd_rqp != NULL) {
                    temp_rqp = cmpd_rqp;
                    while (temp_rqp != NULL) {
                        if (!(temp_rqp->sr_extflags & SMB2_RESPONSE)) {
                            /* Still missing a reply */
                            skip_wakeup = 1;
                            break;
                        }
                        temp_rqp = temp_rqp->sr_next_rqp;
                    }
                }
            }
            
            SMBRQ_SUNLOCK(rqp);
            
            /* Wake up thread waiting for this response */
            if (skip_wakeup == 0) {
                if (cmpd_rqp != NULL) {
                    /* 
                     * <14227703> Have to wake up the head of the compound 
                     * chain 
                     */
                    smb_iod_rqprocessed(cmpd_rqp, 0, 0);
                }
                else {
                    smb_iod_rqprocessed(rqp, 0, 0);
                }

                /*
                 * NOTE: Be careful here as rqp can now be freed at this point
                 */
            }

            /* Fall out of the TAILQ_FOREACH_SAFE loop */
            break;
		}

        if (drop_req_lock) {
            SMB_IOD_RQUNLOCK(iod);
        }

		if (rqp == NULL) {		
            if (smb2_packet) {
                /*
                 * Is it a lease break?
                 * Some servers are not setting session ID to be 0, so do not
                 * check for that.
                 */
                if ((cmd == SMB2_OPLOCK_BREAK) &&
                    (smb2_hdr->message_id == 0xffffffffffffffff) &&
                    (smb2_hdr->sync.tree_id == 0)) {
					/* Note this will call mbuf_freem */
                    (void) smb2_smb_parse_lease_break(iod, m);
                    continue;
                }
                
                /* Ignore Echo or (Async and STATUS_PENDING) responses */
                if ((cmd != SMB2_ECHO) &&
                    !((smb2_hdr->flags & SMBR_ASYNC) && (smb2_hdr->status == STATUS_PENDING))) {
                    SMBWARNING("drop resp: message_id %lld, cmd %d status 0x%x\n", 
                               smb2_hdr->message_id,
                               smb2_hdr->command,
                               smb2_hdr->status);
                }
            }
            else {
                /* Ignore ECHO and NTNotify dropped messages */
                if ((cmd != SMB_COM_ECHO) && (cmd != SMB_COM_NT_TRANSACT)) {
                    SMBWARNING("drop resp: mid %d, cmd %d\n", (unsigned)mid, cmd);
                }
            }
			mbuf_freem(m);
		}
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

static int
smb_iod_rq_sign(struct smb_rq *rqp)
{
    struct smb_session *sessionp = rqp->sr_session;
    uint32_t do_encrypt;
    struct mbchain *mbp;
    int error = 0;

    smb_rq_getrequest(rqp, &mbp);
    mb_fixhdr(mbp);

    /*
     * SMB 2/3
     *
     * Set the message_id right before we sent the request
     *
     * This is because Windows based servers will stop granting credits
     * if the difference between the smallest available sequence number and
     * the largest available sequence number exceeds 2 times the number
     * of granted credits.  Refer to Secion 3.3.1.1 in SMB Version 2
     * Protocol Specification.
     */
    smb2_rq_message_id_increment(rqp);

    //SMBERROR("MID:%llu chg %d cmd %d \n", rqp->sr_messageid, rqp->sr_creditcharge, rqp->sr_command);

    /* Determine if outgoing request(s) must be encrypted */
    do_encrypt = 0;

    if (SMBV_SMB3_OR_LATER(sessionp)) {
        /* Check if session is encrypted */
        if (sessionp->session_sopt.sv_sessflags & SMB2_SESSION_FLAG_ENCRYPT_DATA) {
            if ((rqp->sr_command != SMB2_NEGOTIATE) &&
                (rqp->sr_command != SMB2_SESSION_SETUP)) {
                do_encrypt = 1;
            }
        } else if (rqp->sr_share != NULL) {
            if ( (rqp->sr_command != SMB2_NEGOTIATE) &&
                (rqp->sr_command != SMB2_SESSION_SETUP) &&
                (rqp->sr_command != SMB2_TREE_CONNECT) &&
                (rqp->sr_share->ss_share_flags & SMB2_SHAREFLAG_ENCRYPT_DATA) ){
                do_encrypt = 1;
            }
        }
    }

    if ( !(do_encrypt) &&
        ((sessionp->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) ||
         ((rqp->sr_flags & SMBR_SIGNED)))) {
            /* Only sign if not encrypting */
            smb2_rq_sign(rqp);
        }

    if (do_encrypt) {
        error = smb3_rq_encrypt(rqp);
        if (error) {
            /* Should never happen */
            SMBERROR("SMB3 transform failed, error: %d\n", error);
            return error;
        }
    }

    return(error);
}

/*
 * Place request in the queue.
 * Request from smbiod have a high priority.
 */
int
smb_iod_rq_enqueue(struct smb_rq *rqp)
{
	struct smb_session *sessionp = rqp->sr_session;
	struct smbiod *iod = sessionp->session_iod;
	struct timespec ts;
    struct smb_rq *tmp_rqp;
    int return_error = 0;
    int error = 0;

    if (rqp->sr_context == iod->iod_context) {
        /*
         * This is a special request like reconnect or Echo.
         * Note that the request gets enqueued on head to be sent right away
         */
		DBG_ASSERT((rqp->sr_flags & SMBR_ASYNC) != SMBR_ASYNC);
		rqp->sr_flags |= SMBR_INTERNAL;
		
        /* Hold RQ lock while we assign the Message ID and sign/seal */
		SMB_IOD_RQLOCK(iod);

        if (rqp->sr_extflags & SMB2_REQUEST) {
            error = smb_iod_rq_sign(rqp);
            if (error) {
                SMBERROR("smb_iod_rq_sign failed %d \n", error);
                SMB_IOD_RQUNLOCK(iod);
                return(error);
            }
        }

		TAILQ_INSERT_HEAD(&iod->iod_rqlist, rqp, sr_link);
		SMB_IOD_RQUNLOCK(iod);
		
		for (;;) {
			if (smb_iod_sendrq(iod, rqp) != 0) {
				smb_iod_start_reconnect(iod, 0);
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
            /*
             * If this is not an internal message and we are in reconnect then
             * check for softmount timouts.
             */
            if ((!(rqp->sr_flags & SMBR_INTERNAL)) && (iod->iod_flags & SMBIOD_RECONNECT) &&
                (rqp->sr_share) && (rqp->sr_share->ss_soft_timer)) {
                /* It soft mounted, check to see if should return ETIMEDOUT */
                if (rqp->sr_extflags & SMB2_REQUEST) {
                    /*
                     * If its SMB 2/3 and its not part of reconnect, return
                     * ETIMEDOUT.
                     */
                    if (!(rqp->sr_context == iod->iod_context)) {
                        if (smb_iod_check_timeout(&iod->reconnectStartTime,
                                                  rqp->sr_share->ss_soft_timer)) {
                            SMBDEBUG("Soft Mount timed out! cmd = 0x%x message_id %lld \n",
                                     (UInt32) rqp->sr_command, rqp->sr_messageid);
                            /*
                             * Dont have to clear SMB2_REQ_SENT because at this
                             * time, it has not been sent.
                             */
                            return ETIMEDOUT;
                        }
                        else {
                            /*
                             * Let the request get put on the queue and wait
                             * there until reconnect finishes. If reconnect
                             * works, then the request will be sent back up to
                             * get rebuilt and resent.
                             */
                            rqp->sr_flags |= SMBR_RECONNECTED;
                            rqp->sr_state = SMBRQ_RECONNECT; /* Wait for reconnect to complete */
                            rqp->sr_flags |= SMBR_REXMIT;    /* Tell the upper layer this message was resent */
                        }
                    }
                }
                else {
                    /* For SMB 1, see if soft mount timer has expired or not */
                    if (smb_iod_check_timeout(&iod->reconnectStartTime,
                                              rqp->sr_share->ss_soft_timer)) {
                        SMBDEBUG("Soft Mount timed out! cmd = 0x%x\n",
                                 (UInt32) rqp->sr_cmd);
                        return ETIMEDOUT;
                    }
                }
            }
			break;
	}

    /*
     * Regular request send code
     */
    
    if (!(rqp->sr_extflags & SMB2_REQUEST)) {
        /* SMB v1 request */
        if (sessionp->session_flags & SMBV_SMB2) {
            /* 
             * Huh? Why are we trying to send SMB 1 request on SMB 2/3
             * connection. This is not allowed. Need to find the code path
             * that got to here and fix it.
             */
            SMBERROR("SMB 1 not allowed on SMB 2/3 connection. cmd = %x\n", rqp->sr_cmd);
            return ERPCMISMATCH;
        }

        /* SMB 1 Flow Control */
        for (;;) {
            if (iod->iod_muxcnt < sessionp->session_maxmux) {
                break;
            }
            iod->iod_muxwant++;
            msleep(&iod->iod_muxwant, 0, PWAIT, "iod-rq-mux", 0);
        }
    }
    else {
        /* 
         * Check for SMB 2/3 request that got partially built when
         * reconnect occurred. Example, Create got built with SessionID 1, 
         * QueryInfo was blocked waiting on credits and after reconnect, it 
         * gets built with SessionID 2 and so does the Close. Then the
         * compound request gets sent with an invalid SessionID in the Create.
         *
         * Check sr_rqsessionid and make sure its correct for each rqp. If any
         * are old, then mark that rqp with Reconnect (so no credits are 
         * recovered since they are from previous session) and return an error.
         */
        tmp_rqp = rqp;
        while (tmp_rqp != NULL) {
            if (tmp_rqp->sr_rqsessionid != rqp->sr_session->session_session_id) {
                tmp_rqp->sr_flags |= SMBR_RECONNECTED;
                return_error = 1;
            }
            tmp_rqp = tmp_rqp->sr_next_rqp;
        }

        if (return_error == 1) {
            return (EAGAIN);
        }
    }
    
	/* 
     * SMB 1
	 * Should be noted here Window 2003 and Samba don't seem to care about going
	 * over the maxmux count when doing notification messages. XPhome does for sure,
	 * they will actual break the connection. SMB 2/3 will solve this issue and some
	 * day I would like to see which server care and which don't. Should we do 
	 * something special for Samba or Apple, since they don't care?
	 *
	 * So for now we never use more than two thirds, if session_maxmux is less than
	 * three then don't allow any. Should never happen, but just to be safe.
	 */
	if (rqp->sr_flags & SMBR_ASYNC) {
        if (!(rqp->sr_extflags & SMB2_REQUEST)) {
            /* SMB 1 Flow Control */
            if (iod->iod_asynccnt >= ((sessionp->session_maxmux / 3) * 2)) {
                SMBWARNING("Max out on session async notify request %d\n", iod->iod_asynccnt);
                return EWOULDBLOCK;
            }
        }
        
        /* Number of pending async requests */
		iod->iod_asynccnt++;
	} else if (iod->iod_state == SMBIOD_ST_SESSION_ACTIVE) {
		if (sessionp->throttle_info)
			throttle_info_update(sessionp->throttle_info, 0);
	}
    
    /* Number of pending requests (sync and async) */
    iod->iod_muxcnt++;
    
    SMB_LOG_KTRACE(SMB_DBG_IOD_MUXCNT | DBG_FUNC_NONE,
                   /* channelID */ 0, iod->iod_muxcnt, 0, 0, 0);

    /* Hold RQ lock while we assign the Message ID and sign/seal */
    SMB_IOD_RQLOCK(iod);

    if (rqp->sr_extflags & SMB2_REQUEST) {
        error = smb_iod_rq_sign(rqp);
        if (error) {
            SMBERROR("smb_iod_rq_sign failed %d \n", error);
            SMB_IOD_RQUNLOCK(iod);
            return(error);
        }
    }

	TAILQ_INSERT_TAIL(&iod->iod_rqlist, rqp, sr_link);
	SMB_IOD_RQUNLOCK(iod);

	iod->iod_workflag = 1;
	smb_iod_wakeup(iod);
	return 0;
}

int
smb_iod_removerq(struct smb_rq *rqp)
{
	struct smb_session *sessionp = rqp->sr_session;
	struct smbiod *iod = sessionp->session_iod;

	SMBIODEBUG("\n");
	SMB_IOD_RQLOCK(iod);
    
	if (rqp->sr_flags & SMBR_INTERNAL) {
		TAILQ_REMOVE(&iod->iod_rqlist, rqp, sr_link);
		SMB_IOD_RQUNLOCK(iod);
		return 0;
	}
    
	TAILQ_REMOVE(&iod->iod_rqlist, rqp, sr_link);
    
    SMB_IOD_RQUNLOCK(iod);

    if (rqp->sr_flags & SMBR_ASYNC) {
        /* decrement number of pending async requests */
		iod->iod_asynccnt--;
    }
    
    /* Decrement number of pending requests (sync and async) */
    if (!(rqp->sr_flags & SMBR_RECONNECTED)) {
        /* 
         * Reconnect resets muxcnt to 0, so any request that was in progress
         * at that time should not decrement muxcnt else it will go negative
         */
        iod->iod_muxcnt--;
        
        SMB_LOG_KTRACE(SMB_DBG_IOD_MUXCNT | DBG_FUNC_NONE,
                       /* channelID */ 0, iod->iod_muxcnt, 0, 0, 0);
    }
    
    if (!(rqp->sr_extflags & SMB2_REQUEST)) {
        /* SMB 1 Flow Control */
        if (iod->iod_muxwant) {
            iod->iod_muxwant--;
            wakeup(&iod->iod_muxwant);
        }
    }

	return 0;
}

int
smb_iod_waitrq(struct smb_rq *rqp)
{
	struct smbiod *iod = rqp->sr_session->session_iod;
	int error;
	struct timespec ts;

	SMBIODEBUG("\n");
	if (rqp->sr_flags & SMBR_INTERNAL) {
		for (;;) {
			smb_iod_sendall(iod);
            /* iod_read_thread calls smb_iod_recvall() */

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
         * First thing to note here is that the transaction messages can have
         * multiple replies. We have to watch out for this if a reconnect 
         * happens. So if we sent the message and received at least one reply 
         * make sure a reconnect hasn't happen in between. So we check for 
         * SMBR_MULTIPACKET flag because it tells us this is a transaction 
         * message, we also check for the SMBR_RECONNECTED flag because it 
         * tells us that a reconnect happen and we also check to make sure the 
         * SMBR_REXMIT flags isn't set because that would mean we resent the 
         * whole message over. If the sr_rplast is set then we have received 
         * at least one response, so there is not much we can do with this 
         * transaction. So just treat it like a softmount happened and return 
         * ETIMEDOUT.
         *
         * Make sure we didn't get reconnect while we were asleep waiting on the next response.
         */
		do {
			ts.tv_sec = 15;
			ts.tv_nsec = 0;
			msleep(&rqp->sr_state, SMBRQ_SLOCKPTR(rqp), PWAIT, "wait for reply", &ts);
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
 * Error out any outstanding requests on the session that belong to the specified
 * share. The calling routine should hold a reference on this share before 
 * calling this routine.
 */
void
smb_iod_errorout_share_request(struct smb_share *share, int error)
{
	struct smbiod *iod = SS_TO_SESSION(share)->session_iod;
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
	struct smb_session *sessionp = iod->iod_session;
	struct smb_rq *rqp, *trqp;
	struct timespec now, ts, uetimeout;
	int herror, echo, drop_req_lock;
	uint64_t oldest_message_id = 0;
	struct timespec oldest_timesent = {0, 0};
    uint32_t pending_reply = 0;
    uint32_t need_wakeup = 0;

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
			/* session is down, just time out any message on the list */
			smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
			continue;
		}
		
		/* If the share is going away then just timeout the request. */ 
		if ((rqp->sr_share) && (isShareGoingAway(rqp->sr_share))) {
			smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
			continue;
		}
		
        /* Are we currently in reconnect? */
		if ((iod->iod_flags & SMBIOD_RECONNECT) &&
            (!(rqp->sr_flags & SMBR_INTERNAL))) {
            /*
             * If SMB 2/3 and soft mounted, then cancel the request (unless
             * its a reconnect request) with ETIMEDOUT
             */
            if ((rqp->sr_share) &&
                (rqp->sr_extflags & SMB2_REQUEST) &&
                !(rqp->sr_context == iod->iod_context) &&
                (rqp->sr_share->ss_soft_timer) &&
                smb_iod_check_timeout(&iod->reconnectStartTime, rqp->sr_share->ss_soft_timer)) {
                /* 
                 * Pretend like it did not get sent to recover SMB 2/3 credits 
                 */
                rqp->sr_extflags &= ~SMB2_REQ_SENT;
				smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
                
                SMBDEBUG("Soft Mount timed out! cmd = 0x%x message_id %lld \n",
                         (UInt32) rqp->sr_cmd, rqp->sr_messageid);
                continue;
            }
            
			if (rqp->sr_flags & SMBR_ASYNC) {
				smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
				continue;							
			}
            
            /* Should never be in the sent state at this point */
			DBG_ASSERT(rqp->sr_state != SMBRQ_SENT)

			if (rqp->sr_state == SMBRQ_NOTSENT) {
                rqp->sr_extflags &= ~SMB2_REQ_SENT; /* clear the SMB 2/3 sent flag */
				rqp->sr_state = SMBRQ_RECONNECT;
            }

			/* 
             * Tell the upper layer that any error may have been the result 
             * of a reconnect. 
             */
			rqp->sr_flags |= SMBR_RECONNECTED;
		}
		
		switch (rqp->sr_state) {
			case SMBRQ_RECONNECT:
				if (iod->iod_flags & SMBIOD_RECONNECT)
					break;		/* Do nothing to do but wait for reconnect to end. */
				rqp->sr_state = SMBRQ_NOTSENT;
				/*
				 * Make sure this is not a bad server. If we reconnected more
				 * than MAX_SR_RECONNECT_CNT (5) times trying to send this
				 * message, then just give up and kill the mount.
				 */
				rqp->sr_reconnect_cnt += 1; 
				if (rqp->sr_reconnect_cnt > MAX_SR_RECONNECT_CNT) {
					SMBERROR("Looks like we are in a reconnect loop with server %s, canceling the reconnect. (cmd = %x)\n", 
						sessionp->session_srvname, rqp->sr_cmd);
					iod->iod_state = SMBIOD_ST_DEAD;
					smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
					continue;
				}
                
                /* 
                 * Due to crediting issues, have to rebuild the entire request.
                 * Send the request back with error of EAGAIN to indicate that 
                 * it needs to be rebuilt. The SMBR_RECONNECTED flag will also
                 * be set in rqp->sr_flags.
                 */
                if (rqp->sr_extflags & SMB2_REQUEST) {
					smb_iod_rqprocessed(rqp, EAGAIN, 0);
                    continue;
                }
                
                /* Fall through here and send it */
			case SMBRQ_NOTSENT:
                /* Dropping the RQ lock here helps performance */
				SMB_IOD_RQUNLOCK(iod);
                /* Indicate that we are not holding the lock */
                drop_req_lock = 0;
				herror = smb_iod_sendrq(iod, rqp);
                if (herror == 0) {
                    /*
                     * We will need to go back and reaquire the request queue lock 
                     * and start over, since dropping the lock before sending the 
                     * request, the queue could be in a completely differen state.
                     */
                    goto retry;
                }
				break;
			case SMBRQ_SENT:
                if (sessionp->session_flags & SMBV_SMB2) {
                    /*
                     * Keep track of oldest pending Message ID as this is used
                     * in crediting.
                     */
                    if (!(rqp->sr_flags & SMBR_ASYNC)) {
                        /* 
                         * Ignore async requests as they can take an indefinite
                         * amount of time.
                         */
                        if (pending_reply == 0) {
                            /* first pending reply found */
                            pending_reply = 1;
                            oldest_message_id = rqp->sr_messageid;
                            oldest_timesent = rqp->sr_credit_timesent;
                        }
                        else {
                            if (timespeccmp(&oldest_timesent, &rqp->sr_credit_timesent, >)) {
                                oldest_message_id = rqp->sr_messageid;
                                oldest_timesent = rqp->sr_credit_timesent;
                            }
                        }
                    }
                }
        
				/*
				 * If this is an async call or a long-running request then it
				 * can't timeout so we are done.
				 */
				if ((rqp->sr_flags & SMBR_ASYNC) ||
					(rqp->sr_flags & SMBR_NO_TIMEOUT)) {
					break;
				}
				nanouptime(&now);
				if (rqp->sr_share) {
					/*
					 * If its been over session_resp_wait_timeout since
                     * the last time we received a message from the server and 
                     * its been over session_resp_wait_timeout since we sent this
                     * message break the connection. Let the reconnect code 
                     * handle breaking the connection and cleaning up.
                     *
                     * We check both the iod->iod_lastrecv and rqp->sr_timesent
                     * because when the client has no work to do, then no
                     * requests are sent and thus nothing received. Then
                     * iod_lastrecv could exceed the timeout by quite a bit. By
                     * checking both the iod_lastrecv and sr_timesent, we are 
                     * only checking when we know we are actually doing work.
                     *
                     * The rqp->sr_timo field was intended to have variable time
                     * out lengths, but never implemented. This code handles 
                     * time outs on a share. Negotiate, SessionSetup, Logout,
                     * etc, timeouts are handled below with the 
                     * SMB_SEND_WAIT_TIMO check.
					 */
					ts = now;
					uetimeout.tv_sec = sessionp->session_resp_wait_timeout;
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
				 * How long should we wait for a response. In theory forever, 
                 * but what if we never get a response. Should we break the 
                 * connection or just return an error. The old code would wait 
                 * from 12 to 60 seconds depending on the smb message. This 
                 * seems crazy to me, why should the type of smb message 
                 * matter. I know some writes can take a long time, but if the 
                 * server is busy couldn't that happen with any message. We now 
                 * wait for 2 minutes, if time expires we time out the call and 
                 * log a message to the system log.
                 */

                /*
                 * <55757983> dont call smb_iod_nb_intr() to check for
                 * system shutdown as you could deadlock on the SMB_IOD_RQLOCK
                 */
                if (get_system_inshutdown()) {
                    /* If forced unmount or shutdown, dont wait very long */
                    ts.tv_sec = SMB_FAST_SEND_WAIT_TIMO;
                }
                else {
                    ts.tv_sec = SMB_SEND_WAIT_TIMO;
                }

                ts.tv_nsec = 0;
				timespecadd(&ts, &rqp->sr_timesent);
				if (timespeccmp(&now, &ts, >)) {
                    if (rqp->sr_extflags & SMB2_REQUEST) {
                        /* pretend like it did not get sent to recover SMB 2/3 credits */
                        rqp->sr_extflags &= ~SMB2_REQ_SENT;

                        SMBERROR("Timed out waiting on the response for 0x%x message_id = %lld state 0x%x\n",
                                 (UInt32) rqp->sr_cmd, rqp->sr_messageid, (UInt32) rqp->sr_state);
                   }
                    else {
                        SMBERROR("Timed out waiting on the response for 0x%x mid = 0x%x state 0x%x\n",
                                 (UInt32) rqp->sr_cmd, rqp->sr_mid, (UInt32) rqp->sr_state);
                    }
					smb_iod_rqprocessed(rqp, ETIMEDOUT, 0);
				} else if (rqp->sr_cmd != SMB_COM_ECHO) {
					ts = now;
					uetimeout.tv_sec = SMBUETIMEOUT;
					uetimeout.tv_nsec = 0;
					timespecsub(&ts, &uetimeout);
					/* 
                     * Its been SMBUETIMEOUT seconds since we sent this message
                     * send an echo ping
                     */
					if (timespeccmp(&ts, &rqp->sr_timesent, >)) {
						echo++;
                    }
				}
				break;
		    default:
				break;
		}
		if (herror)
			break;
	}
    
    if (drop_req_lock) {
        SMB_IOD_RQUNLOCK(iod);
    }
    
	if (herror == ENOTCONN) {
		smb_iod_start_reconnect(iod, 0);
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
		    timespeccmp(&ts, &iod->iod_lastrqsent, >)) {
			(void)smb_smb_echo(sessionp, SMBNOREPLYWAIT, 1, iod->iod_context);
        }
	}

    if (sessionp->session_flags & SMBV_SMB2) {
        /* Update oldest pending request Message ID */
        SMBC_CREDIT_LOCK(sessionp);
        if (pending_reply == 0) {
            /* No pending reply found */
            sessionp->session_req_pending = 0;
            
            if (sessionp->session_oldest_message_id != 0) {
                sessionp->session_oldest_message_id = 0;
                need_wakeup = 1;
            }
        }
        else {
            /* A pending reply was found */
            sessionp->session_req_pending = 1;
            
            if (oldest_message_id != sessionp->session_oldest_message_id) {
                sessionp->session_oldest_message_id = oldest_message_id;
                need_wakeup = 1;
            }
        }

        /* Wake up any requests waiting for more credits */
        if ((need_wakeup == 1) && (sessionp->session_credits_wait)) {
            OSAddAtomic(-1, &sessionp->session_credits_wait);
            wakeup(&sessionp->session_credits_wait);
        }

        SMBC_CREDIT_UNLOCK(sessionp);
    }

	return 0;
}

/*
 * Count the number of active shares on the session, handle a dead shares,
 * timeout share or notification of unresponsive shares.
 */
static int 
smb_iod_check_for_active_shares(struct smb_session *sessionp)
{
	struct smbiod *	iod = sessionp->session_iod;
	struct smb_share *share, *tshare;
	int treecnt = 0;

	smb_session_lock(sessionp);	/* lock the session so we can search the list */
	SMBCO_FOREACH_SAFE(share, SESSION_TO_CP(sessionp), tshare) {
		/*
		 * Since we have the session lock we know the share can't be freed, but
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
			treecnt += share->ss_down(share, (iod->iod_flags & SMBIOD_SESSION_NOTRESP));
		} else {
			treecnt++;
		}
		lck_mtx_unlock(&share->ss_shlock);
		smb_share_rele(share, iod->iod_context);
	}
	smb_session_unlock(sessionp);
	return treecnt;			

}

/*
 * This is called from tcp_connect and smb_iod_reconnect. During a reconnect if the volume
 * goes away or someone tries to unmount it then we need to break out of the reconnect. We 
 * may want to use this for normal connections in the future.
 */
int smb_iod_nb_intr(struct smb_session *sessionp)
{
	struct smbiod *	iod = sessionp->session_iod;

    if (get_system_inshutdown()) {
        /* If system is shutting down, then cancel reconnect */
        return EINTR;
    }

    /*
	 * If not in reconnect then see if the user applications wants to cancel the
	 * connection.
	 */
	if ((iod->iod_flags & SMBIOD_RECONNECT) != SMBIOD_RECONNECT) {
		if (sessionp->connect_flag && (*(sessionp->connect_flag) & NSMBFL_CANCEL))
			return EINTR;
		else return 0;
	}
	/* 
	 * We must be in reconnect, check to see if we are offically unresponsive. 
	 * XXX - We should really rework this in the future, if the session was having
	 * issues before we got here, we may want to have SMBIOD_SESSION_NOTRESP arlready
	 * set. See <rdar://problem/8124132>
	 */
	if (((iod->iod_flags & SMBIOD_SESSION_NOTRESP) == 0) &&
		(smb_iod_check_timeout(&iod->reconnectStartTime, NOTIFY_USER_TIMEOUT))) {
			SMB_IOD_FLAGSLOCK(iod);		/* Mark that the session is not responsive. */
			iod->iod_flags |= SMBIOD_SESSION_NOTRESP;
			SMB_IOD_FLAGSUNLOCK(iod);						
	}
	/* Don't keep reconnecting if there are no active shares */
	return (smb_iod_check_for_active_shares(sessionp)) ? 0 : EINTR;
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
	struct smb_session *sessionp = iod->iod_session;
	int tree_cnt = 0;
	int error = 0;
	int sleepcnt = 0;
	struct smb_share *share = NULL, *tshare;
	struct timespec waittime, sleeptime, tsnow;
	int ii;

    /* See if we can get a reference on this session */
	if (smb_session_reconnect_ref(iod->iod_session, iod->iod_context)) {
		/* The session is either gone or going away */
        SMB_IOD_FLAGSLOCK(iod);
		iod->iod_flags &= ~SMBIOD_RECONNECT;
        SMB_IOD_FLAGSUNLOCK(iod);
        /* Wake anyone waiting for reconnect to finish */
        wakeup(&iod->iod_flags);

        /* This function is called by smb_iod_thread so no need for wakeup */
		iod->iod_workflag = 1;
		SMBERROR("The session is going aways while we are in reconnect?\n");
		return;
	}

    SMB_LOG_KTRACE(SMB_DBG_IOD_RECONNECT | DBG_FUNC_START,
                   /* channelID */ 0, 0, 0, 0, 0);

    SMBWARNING("Starting reconnect with %s\n", sessionp->session_srvname);
	SMB_TRAN_DISCONNECT(sessionp); /* Make sure the connection is close first */
	iod->iod_state = SMBIOD_ST_CONNECT;
	
	/* Start the reconnect timers */
	sleepcnt = 1;
	sleeptime.tv_sec = 1;
	sleeptime.tv_nsec = 0;
	nanouptime(&iod->reconnectStartTime);
	/* The number of seconds to wait on a reconnect */
	waittime.tv_sec = sessionp->reconnect_wait_time;	
	waittime.tv_nsec = 0;
	timespecadd(&waittime, &iod->reconnectStartTime);
	
	/* Is reconnect even allowed? */
	if (sessionp->reconnect_wait_time == 0) {
		SMBWARNING("Reconnect is disabled \n");
		smb_reconn_stats.disabled_cnt += 1;
		error = EINTR;
		goto exit;
	}
	
	/* 
	 * If its Time Machine, are there any non idempotent requests "in flight"?
	 * If so, then cancel the reconnect attempt.
	 */
	if (sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) {
		error = smb_iod_check_non_idempotent(iod);
		if (error) {
			SMBWARNING("Non idempotent requests found, failing reconnect \n");
			error = EINTR;
			goto exit;
		}
	}
	
	do {
		/* 
		 * The tcp connect will cause smb_iod_nb_intr to be called every two 
		 * seconds. So we always wait 2 two seconds to see if the connection
		 * comes back quickly before attempting any other types of action.
		 */
        if (smb_iod_nb_intr(sessionp) == EINTR) {
            error = EINTR;
            SMBWARNING("Reconnect to %s was canceled\n", sessionp->session_srvname);
			smb_reconn_stats.cancelled_cnt += 1;
            goto exit;
        }

		error = SMB_TRAN_CONNECT(sessionp, sessionp->session_saddr);
		if (error == EINTR)	{
			SMBWARNING("Reconnect to %s was canceled\n", sessionp->session_srvname);
			smb_reconn_stats.cancelled_cnt += 1;
			goto exit;
		}

		DBG_ASSERT(sessionp->session_tdata != NULL);
		DBG_ASSERT(error != EISCONN);
		DBG_ASSERT(error != EINVAL);
		if (error) {			
			/* 
			 * Never sleep longer that 1 second at a time, but we can wait up 
			 * to 5 seconds between tcp connections. 
			 */ 
			for (ii= 1; ii <= sleepcnt; ii++) {
				msleep(&iod->iod_flags, 0, PWAIT, "reconnect connect retry", &sleeptime);
                
				if (smb_iod_nb_intr(sessionp) == EINTR) {
					error = EINTR;
					SMBWARNING("Reconnect to %s was canceled\n", sessionp->session_srvname);
					smb_reconn_stats.cancelled_cnt += 1;
					goto exit;
				}
			}
			/* Never wait more than 5 seconds between connection attempts */
			if (sleepcnt < SMB_MAX_SLEEP_CNT )
				sleepcnt++;
			SMBWARNING("Retrying connection to %s error = %d\n", sessionp->session_srvname, 
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
			waittime.tv_sec = sessionp->reconnect_wait_time;
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
            
			/* Reset the session to a reconnect state */
			smb_session_reset(sessionp);
            
			/* Start the session */
			iod->iod_state = SMBIOD_ST_TRANACTIVE;

            /* Wake up the read thread */
            smb_iod_wakeup(iod);

			error = smb_smb_negotiate(sessionp, NULL, TRUE, iod->iod_context);
			if ((error == ENOTCONN) || (error == ETIMEDOUT)) {
				SMBWARNING("The negotiate timed out to %s trying again: error = %d\n", 
						   sessionp->session_srvname, error);
				SMB_TRAN_DISCONNECT(sessionp); /* Make sure the connection is close first */
				iod->iod_state = SMBIOD_ST_CONNECT;
			} else if (error) {
				SMBWARNING("The negotiate failed to %s with an error of %d\n", 
						   sessionp->session_srvname, error);
				break;				
			} else {
				SMBDEBUG("The negotiate succeeded to %s\n", sessionp->session_srvname);
				iod->iod_state = SMBIOD_ST_NEGOACTIVE;				
				/*
				 * We now do the authentication inside the connect loop. This 
				 * way if the authentication fails because we don't have a 
				 * creditials yet or the creditials have expired, then we can 
				 * keep trying.
				 */
				error = smb_iod_ssnsetup(iod, TRUE);
				if (error)
					SMBWARNING("The authentication failed to %s with an error of %d\n", 
							   sessionp->session_srvname, error);
				/* If the error isn't EAGAIN then nothing else to do here, we have success or failure */
				if (error != EAGAIN) {
					break;
				}
				
				/* Try four more times and see if the user has update the Kerberos Creds */
				for (ii = 1; ii < SMB_MAX_SLEEP_CNT; ii++) {
					msleep(&iod->iod_flags, 0, PWAIT, "reconnect auth retry", &sleeptime);
                    
                    if (smb_iod_nb_intr(sessionp) == EINTR) {
                        error = EINTR;
                        SMBWARNING("Reconnect to %s was canceled\n",
                                 sessionp->session_srvname);
						smb_reconn_stats.cancelled_cnt += 1;
                        goto exit;
                    }
					
                    error = smb_iod_ssnsetup(iod, TRUE);
					if (error)
						SMBWARNING("Retrying authentication count %d failed to %s with an error of %d\n", 
								   ii, sessionp->session_srvname, error);
					if (error != EAGAIN)
						break;
				}
				/* If no error then we are done, otherwise break the connection and try again */
				if (error == 0)
					break;
				
				SMB_TRAN_DISCONNECT(sessionp); /* Make sure the connection is close first */
				iod->iod_state = SMBIOD_ST_CONNECT;
				error = EAUTH;
			}
		}
		nanouptime(&tsnow);
	} while (error && (timespeccmp(&waittime, &tsnow, >)));
		
	/* reconnect failed or we timed out, nothing left to do cancel the reconnect */
	if (error) {
		SMBWARNING("The connection failed to %s with an error of %d\n", sessionp->session_srvname, error);
		smb_reconn_stats.error_cnt += 1;
		goto exit;
	}
	
    /* We logged back into the session, clear the not responsive flag */
    iod->iod_flags &= ~SMBIOD_SESSION_NOTRESP;

	/*
	 * We now need to reconnect each share. Since the current code only has one share
	 * per session there is no problem with locking the list down here. Need
	 * to look at this in the future. If at least one mount point succeeds then do not
	 * close the whole session.
	 * We do not wake up smbfs_smb_reopen_file, wait till the very end.
	 */
	tree_cnt = 0;
	smb_session_lock(sessionp);	/* lock the session so we can search the list */
	SMBCO_FOREACH_SAFE(share, SESSION_TO_CP(sessionp), tshare) {		
		/*
		 * Since we have the session lock we know the share can't be freed, but
		 * it could be going away (Tree Disconnect is in process). Take a 
		 * reference so no one else can disconnect it out from underneath 
		 * us.
		 */
		smb_share_ref(share);
		if (share->ss_flags & SMBO_GONE) {
			/* Skip any shares that are being disconnected */
			smb_share_rele(share, iod->iod_context);
			continue;
		}
			
		/* Always reconnect the tree, even if its not a mount point */
		error = smb_smb_treeconnect(share, iod->iod_context);
		if (error) {
			SMBERROR("Reconnect failed to share %s on server %s error = %d\n", 
					 share->ss_name, sessionp->session_srvname, error);
			
			if (sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) {
				smb_share_rele(share, iod->iod_context);
				
				smb_iod_disconnect(iod);
				
				smb_session_unlock(sessionp);
				error = ENOTCONN;
				smb_reconn_stats.tree_conn_fail_cnt += 1;
				goto exit;
			}
			else {
				/* For non Time Machine mounts, we dont care about an error */
				error = 0; /* reset the error, only used for logging */
			}
		}
		else {
			tree_cnt++;
			lck_mtx_lock(&share->ss_shlock);
			if (share->ss_up) {
                /* 
                 * Tell upper layers that reconnect has been done. Right now
                 * this marks all open files that they need to be reopened.
                 */
				error = share->ss_up(share, TRUE);
                if (error == 0) {
                    /* ss_up() worked for this share */
                    SMBERROR("Reconnected share %s with server %s\n",
                             share->ss_name, sessionp->session_srvname);
                }
                else {
                    /* ss_up() failed for this share */
                    if (sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) {
                        SMBERROR("Failed to reconnect TM share %s with server %s\n",
                                 share->ss_name, sessionp->session_srvname);
                        lck_mtx_unlock(&share->ss_shlock);
                        smb_share_rele(share, iod->iod_context);
                        
                        smb_iod_disconnect(iod);
                        
                        smb_session_unlock(sessionp);
                        error = ENOTCONN;
                        smb_reconn_stats.reopen_fail_cnt += 1;
                        goto exit;
                    }
                    else {
                        /*
                         * For non Time Machine mounts, we dont care about
                         * an error for one share. Hopefully another share will
                         * get reconnected. If all shares fail, then the
                         * tree_cnt of 0 will cause the reconnect to fail.
                         */
                        SMBERROR("Failed to reconnect share %s with server %s\n",
                                 share->ss_name, sessionp->session_srvname);
                        tree_cnt -= 1;
                        error = 0; /* reset the error, only used for logging */
                   }
                }
			}
            else {
                /*
                 * No ss_up() function to call, so share must be reconnected.
                 * I dont think we can ever have this case...
                 */
				SMBWARNING("Reconnected share %s with server %s\n",
                           share->ss_name, sessionp->session_srvname);
			}
			lck_mtx_unlock(&share->ss_shlock);	
		}
		smb_share_rele(share, iod->iod_context);			
	}
	smb_session_unlock(sessionp);
	
	/* If we have no shares on this connect then kill the whole session. */
	if (!tree_cnt) {
		SMBWARNING("No mounted volumes in reconnect, closing connection to server %s\n",
                   sessionp->session_srvname);
		error = ENOTCONN;
		smb_reconn_stats.no_vol_cnt += 1;
	}

exit:	
	/*
	 * We only want to wake up the shares if we are not trying to do another
	 * reconnect. So if we have no error or the reconnect time is pass the
	 * wake time, then wake up any volumes that are waiting
	 */
	if ((error == 0) || (iod->reconnectStartTime.tv_sec >= gWakeTime.tv_sec)) {
		smb_session_lock(sessionp);	/* lock the session so we can search the list */
		SMBCO_FOREACH_SAFE(share, SESSION_TO_CP(sessionp), tshare) {
			smb_share_ref(share);
			lck_mtx_lock(&share->ss_stlock);
			share->ss_flags &= ~SMBS_RECONNECTING;	/* Turn off reconnecting flag */
			lck_mtx_unlock(&share->ss_stlock);
			wakeup(&share->ss_flags);	/* Wakeup the volumes. */
			smb_share_rele(share, iod->iod_context);			
		}					
		smb_session_unlock(sessionp);
	}
	/* 
	 * Remember we are the main thread, turning off the flag will start the process 
	 * going only after we leave this routine.
	 */
	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags &= ~SMBIOD_RECONNECT;
	SMB_IOD_FLAGSUNLOCK(iod);
    /* Wake anyone waiting for reconnect to finish */
    wakeup(&iod->iod_flags);

    if (error) {
		SMB_TRAN_DISCONNECT(sessionp);
    }
		
	smb_session_reconnect_rel(sessionp);	/* We are done release the reference */
	
	if (error) {
		if (iod->reconnectStartTime.tv_sec < gWakeTime.tv_sec) {
			/*
			 * We went to sleep after the connection, but before the reconnect
			 * completed. Start the whole process over now and see if we can
			 * reconnect.
			 */
			SMBWARNING("The reconnect failed because we went to sleep retrying! %d\n", error);
			iod->iod_state = SMBIOD_ST_RECONNECT_AGAIN;
			smb_iod_start_reconnect(iod, 0); /* Retry the reconnect */
		}
        else {
			/* We failed; tell the user and have the volume unmounted */
			smb_iod_dead(iod, 0);
			
            /* 
             * Reconnect failed, but iod layer is all set now to deny any new 
             * requests. Tell above layer that we now have a ton of credits to 
             * allow any requests waiting for credits to error out.
             */
			smb_reconn_stats.fail_cnt += 1;
            smb2_rq_credit_start(sessionp, kCREDIT_MAX_AMT);
		}
	}
    else {
        /* Reconnect worked, its now safe to start up crediting again */
        smb2_rq_credit_start(sessionp, 0);
		smb_reconn_stats.success_cnt += 1;
    }

    /* This function is called by smb_iod_thread so no need for wakeup */
	iod->iod_workflag = 1;

    SMB_LOG_KTRACE(SMB_DBG_IOD_RECONNECT | DBG_FUNC_END,
                   /* channelID */ 0, error, 0, 0, 0);
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
				evp->ev_error = smb_iod_ssnsetup(iod, FALSE);
				break;
            case SMBIOD_EV_QUERY_IF_INFO:
                // Interogate Server for available interfaces
                evp->ev_error = smb_iod_get_interface_info(iod);
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
		    case SMBIOD_EV_FORCE_RECONNECT:
                smb_iod_start_reconnect(iod, 0);
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
    /* iod_read_thread calls smb_iod_recvall() */
	return;
}

static void smb_iod_thread(void *arg)
{
	struct smbiod *iod = arg;
	vfs_context_t context;

	/* 
	 * The iod sets the iod_p to kernproc when launching smb_iod_thread in
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

        if (iod->iod_flags & SMBIOD_SHUTDOWN) {
			break;
        }

		/* First see if we need to try a reconnect. If not see the session is not responsive. */
        if ((iod->iod_flags & (SMBIOD_START_RECONNECT | SMBIOD_RECONNECT)) == SMBIOD_RECONNECT) {
			smb_iod_reconnect(iod);
        }

		/*
		 * In order to prevent a race here, this should really be locked
		 * with a mutex on which we would subsequently msleep, and
		 * which should be acquired before changing the flag.
		 * Or should this be another flag in iod_flags, using its
		 * mutex?
		 */
        if (iod->iod_workflag) {
			continue;
        }

#if 0
        SMBERROR("going to sleep for %ld secs %ld nsecs\n",
                 iod->iod_sleeptimespec.tv_sec,
                 iod->iod_sleeptimespec.tv_nsec);
#endif
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

void
smb_iod_lease_enqueue(struct smbiod *iod,
					  uint16_t server_epoch, uint32_t flags,
					  uint64_t lease_key_hi, uint64_t lease_key_low,
					  uint32_t curr_lease_state, uint32_t new_lease_state)
{
	struct lease_rq *lease_rqp = NULL;
	
	/* Malloc a lease struct */
	SMB_MALLOC(lease_rqp, struct lease_rq *, sizeof(*lease_rqp), M_SMBIOD,
			   M_WAITOK | M_ZERO);

	lease_rqp->server_epoch = server_epoch;
	lease_rqp->flags = flags;
	lease_rqp->lease_key_hi = lease_key_hi;
	lease_rqp->lease_key_low = lease_key_low;
	lease_rqp->curr_lease_state = curr_lease_state;
	lease_rqp->new_lease_state = new_lease_state;
	
	/* Enqueue it to be handled by lease thread */
	SMB_IOD_LEASELOCK(iod);
	TAILQ_INSERT_HEAD(&iod->iod_lease_list, lease_rqp, link);
	SMB_IOD_LEASEUNLOCK(iod);
	
	/* Wake up the lease thread and tell it that theres work to do */
	iod->iod_lease_work_flag = 1;
	wakeup(&(iod->iod_lease_work_flag));
}

static void 
smb_iod_lease_thread(void *arg)
{
	vfs_context_t context;
	struct smbiod *iod = arg;
	struct lease_rq *lease_rqp, *tmp_lease_rqp;
	
	/*
	 * This thread handles any incoming lease breaks from the server
	 */
	
	/* 
	 * Its important to use a different context than the iod_context
	 * as those send/rcv expect one request at a time.
	 */
	context = vfs_context_create((vfs_context_t)0);

	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags |= SMBIOD_LEASE_THREAD_RUNNING;
	SMB_IOD_FLAGSUNLOCK(iod);
	
	/*
	 * Loop until the iod is shutdown
	 */
	while ((iod->iod_flags & SMBIOD_SHUTDOWN) == 0) {
		iod->iod_lease_work_flag = 0;
		
		SMB_IOD_LEASELOCK(iod);
		
		TAILQ_FOREACH_SAFE(lease_rqp, &iod->iod_lease_list, link, tmp_lease_rqp) {
            /* Remove the lease from the queue and drop the lock while we process it. */
            TAILQ_REMOVE(&iod->iod_lease_list, lease_rqp, link);
            SMB_IOD_LEASEUNLOCK(iod);

			/* Handle the lease break */
			smbfs_handle_lease_break(lease_rqp, context);

            /* Done with this lease break; free it */
			SMB_FREE(lease_rqp, M_SMBIOD);

            /* ...and reacquire the lock to check for more. */
            SMB_IOD_LEASELOCK(iod);
		}
		
		SMB_IOD_LEASEUNLOCK(iod);
		
		/* Do we need to exit this thread? */
		if (iod->iod_flags & SMBIOD_SHUTDOWN) {
			break;
		}
		
		/* Did more work come in? */
		if (iod->iod_lease_work_flag) {
			continue;
		}
		
		/* No work left to do, sleep until we get more work */
		msleep(&iod->iod_lease_work_flag, 0, PWAIT, "iod lease idle",
			   &iod->iod_sleeptimespec);
	}
	
	/*
	 * Clear the running flag, and wake up anybody waiting for us to quit.
	 */
	SMB_IOD_FLAGSLOCK(iod);
	iod->iod_flags &= ~SMBIOD_LEASE_THREAD_RUNNING;
	wakeup(iod);
	SMB_IOD_FLAGSUNLOCK(iod);

	vfs_context_rele(context);
}

static void
smb_iod_read_thread(void *arg)
{
    struct smbiod *iod = arg;

    /*
     * This thread does all the reading on the socket
     */

    SMB_IOD_FLAGSLOCK(iod);
    iod->iod_flags |= SMBIOD_READ_THREAD_RUNNING;
    wakeup(iod);
    SMB_IOD_FLAGSUNLOCK(iod);

    /*
     * Loop until the iod read thread is shutdown
     * Run states:
     * 1. Before the connection, thread checks iod_state and goes to sleep.
     *    It will wake up and check the iod_state for changes and go back to
     *    sleep.
     * 2. After the TCP connect is done, a wake is done on iod_state and the
     *    read thread now does blocking reads by calling smb_iod_recvall().
     * 3. If the client or server disconnects, then smb_iod_recvall() gets an
     *    error and returns. The iod_state is checked and the read thread
     *    goes back to sleep.
     * 4. If a reconnect is started, state moves to SMBIOD_ST_RECONNECT and
     *    thread goes to sleep. In reconnect, the send thread will do the
     *    the actual reconnect by calling smb_iod_reconnect(). It will
     *    disconnect and change state to SMBIOD_ST_CONNECT. After a succesful
     *    connect, the read thread is woken up. If reconnect fails, then
     *    read thread should be exited.
     * 5. If we log off, then the read thread is told to exit.
     */
    while ((iod->iod_flags & SMBIOD_READ_THREAD_STOP) == 0) {
        /* Sleep until we are in a state where we need to start reading */
        switch (iod->iod_state) {
            case SMBIOD_ST_NOTCONN:
                /* This state is set after we have shut down this thread */
            case SMBIOD_ST_CONNECT:
                /* This state is set before we started this thread */
            case SMBIOD_ST_DEAD:
                /* This state is set right before we shut down this thread */
            case SMBIOD_ST_RECONNECT_AGAIN:
                /* reconnect is being tried again */
            case SMBIOD_ST_RECONNECT:
                /* reconnect has been started */
#if 0
                SMBERROR("going to sleep for %ld secs %ld nsecs\n",
                         iod->iod_sleeptimespec.tv_sec,
                         iod->iod_sleeptimespec.tv_nsec);
#endif
                msleep(&iod->iod_flags, 0, PWAIT, "iod read thread idle", &iod->iod_sleeptimespec);
                /* Loop around and recheck status */
                continue;
            default:
                break;
        }

        smb_iod_recvall(iod);

        /* Do we need to exit this thread? */
        if (iod->iod_flags & SMBIOD_SHUTDOWN) {
            break;
        }
    }

    /*
     * Clear the running flag, and wake up anybody waiting for us to quit.
     */
    SMB_IOD_FLAGSLOCK(iod);
    iod->iod_flags &= ~SMBIOD_READ_THREAD_RUNNING;
    wakeup(iod);
    SMB_IOD_FLAGSUNLOCK(iod);
}

int
smb_iod_create(struct smb_session *sessionp)
{
	struct smbiod	*iod;
	kern_return_t	result;
	thread_t		thread;

	SMB_MALLOC(iod, struct smbiod *, sizeof(*iod), M_SMBIOD, M_WAITOK | M_ZERO);
	iod->iod_id = smb_iod_next++;
	iod->iod_state = SMBIOD_ST_NOTCONN;
	lck_mtx_init(&iod->iod_flagslock, iodflags_lck_group, iodflags_lck_attr);
	iod->iod_session = sessionp;
	iod->iod_sleeptimespec.tv_sec = SMBIOD_SLEEP_TIMO;
	iod->iod_sleeptimespec.tv_nsec = 0;
	nanouptime(&iod->iod_lastrqsent);
	sessionp->session_iod = iod;
	lck_mtx_init(&iod->iod_rqlock, iodrq_lck_group, iodrq_lck_attr);
	TAILQ_INIT(&iod->iod_rqlist);
	lck_mtx_init(&iod->iod_evlock, iodev_lck_group, iodev_lck_attr);
	STAILQ_INIT(&iod->iod_evlist);
	lck_mtx_init(&iod->iod_lease_lock, iodrq_lck_group, iodrq_lck_attr);
	TAILQ_INIT(&iod->iod_lease_list);

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
	
    /* Start up a thread to handle lease breaks */
	result = kernel_thread_start((thread_continue_t)smb_iod_lease_thread, iod, &thread);
	if (result != KERN_SUCCESS) {
		/* Should never happen */
		SMBERROR("can't start lease break thread. result = %d\n", result);
	}
	else {
		thread_deallocate(thread);
	}

	return (0);
}

int
smb_iod_destroy(struct smbiod *iod)
{
	struct lease_rq *lease_rqp, *tmp_lease_rqp;
	
	/*
	 * We don't post this synchronously, as that causes a wakeup
	 * when the SMBIOD_SHUTDOWN flag is set, but that happens
	 * before the iod actually terminates, and we have to wait
	 * until it terminates before we can free its locks and
	 * its data structure.
	 */
	smb_iod_request(iod, SMBIOD_EV_SHUTDOWN, NULL);

    /* Make sure the tcp transport was freed */
    smb_iod_closetran(iod, 0);

    /*
	 * Wait for the iod to exit.
	 */
    SMB_IOD_FLAGSLOCK(iod);
	for (;;) {
		if (!(iod->iod_flags & SMBIOD_RUNNING)) {
			SMB_IOD_FLAGSUNLOCK(iod);
			break;
		}

        msleep(iod, SMB_IOD_FLAGSLOCKPTR(iod), PWAIT,
               "iod-exit", 0);
	}

    /*
     * Tell read thread to exit
     */
    SMB_IOD_FLAGSLOCK(iod);
    for (;;) {
        if (!(iod->iod_flags & SMBIOD_READ_THREAD_RUNNING)) {
            SMB_IOD_FLAGSUNLOCK(iod);
            break;
        }

        /* Tell read thread to exit */
        iod->iod_flags |= SMBIOD_READ_THREAD_STOP;
        wakeup(&(iod->iod_flags));

        msleep(iod, SMB_IOD_FLAGSLOCKPTR(iod), PWAIT,
               "iod-read-exit", 0);
    }

    /*
	 * Wait for the iod lease thread to exit.
	 */
	wakeup(&(iod->iod_lease_work_flag));
    SMB_IOD_FLAGSLOCK(iod);
	for (;;) {
		if (!(iod->iod_flags & SMBIOD_LEASE_THREAD_RUNNING)) {
			SMB_IOD_FLAGSUNLOCK(iod);
			break;
		}

        msleep(iod, SMB_IOD_FLAGSLOCKPTR(iod), PWAIT,
               "iod-lease-exit", 0);
	}
	
	/* free any leftover leases */
	SMB_IOD_LEASELOCK(iod);
	TAILQ_FOREACH_SAFE(lease_rqp, &iod->iod_lease_list, link, tmp_lease_rqp) {
		TAILQ_REMOVE(&iod->iod_lease_list, lease_rqp, link);
		SMB_FREE(lease_rqp, M_SMBIOD);
	}
	SMB_IOD_LEASEUNLOCK(iod);

	lck_mtx_destroy(&iod->iod_flagslock, iodflags_lck_group);
	lck_mtx_destroy(&iod->iod_rqlock, iodrq_lck_group);
	lck_mtx_destroy(&iod->iod_evlock, iodev_lck_group);
	lck_mtx_destroy(&iod->iod_lease_lock, iodrq_lck_group);

	SMB_FREE(iod, M_SMBIOD);
	return 0;
}

int
smb_iod_init(void)
{
	bzero(&smb_reconn_stats, sizeof(smb_reconn_stats));
	return 0;
}

int
smb_iod_done(void)
{
	return 0;
}

int
smb_iod_get_qos(struct smb_session *sessionp, void *data)
{
	int error;
	
	error = SMB_TRAN_GETPARAM(sessionp, SMBTP_QOS, data);
	return(error);
}

int
smb_iod_set_qos(struct smb_session *sessionp, void *data)
{
	int error;
	
	error = SMB_TRAN_SETPARAM(sessionp, SMBTP_QOS, data);
	return(error);
}

