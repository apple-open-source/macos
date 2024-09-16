/*
 * Copyright (c) 2011  Apple Inc. All rights reserved.
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

#include <libkern/OSAtomic.h>
#include <sys/smb_apple.h>
#include <sys/kauth.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_packets_2.h>
#include <smbclient/ntstatus.h>
#include <smbfs/smbfs.h>

static int smb2_rq_init_internal(struct smb_rq *rqp, struct smb_connobj *obj, 
                                 u_char cmd, uint32_t *rq_len, int rq_flags, 
                                 vfs_context_t context, struct smbiod *iod);
static int smb2_rq_new(struct smb_rq *rqp);


/* 
 * Adds padding to 8 byte boundary for SMB 2/3 compound requests
 */
void
smb2_rq_align8(struct smb_rq *rqp)
{
    size_t pad_bytes = 0;
	struct mbchain *mbp;
    
    if ((rqp->sr_rq.mb_len % 8) != 0) {
        /* Next request MUST start on next 8 byte boundary! */
        pad_bytes = 8 - (rqp->sr_rq.mb_len % 8);
        smb_rq_getrequest(rqp, &mbp);
        mb_put_mem(mbp, NULL, pad_bytes, MB_MZERO);
    }
}

/*
 * The object is either a share or a session in either case the calling routine
 * must have a reference on the object before calling this routine.
 *
 * Be careful to use the correct maco SSTOCP or SESSION_TO_CP to get the correct
 * smb_connobj to pass in which depends on the type of packet you are 
 * sending.  If you pass in the wrong obj, odd behavior will result.
 */
int 
smb2_rq_alloc(struct smb_connobj *obj, u_char cmd, uint32_t *rq_len,
              vfs_context_t context, struct smbiod *iod, struct smb_rq **rqpp)
{
	struct smb_rq *rqp;
	int error;
    
    /* 
     * This function allocates smb_rq and creates the SMB 2/3 header
     */
    SMB_MALLOC_TYPE(rqp, struct smb_rq, Z_WAITOK);
	if (rqp == NULL)
		return ENOMEM;
    
	error = smb2_rq_init_internal(rqp, obj, cmd, rq_len, SMBR_ALLOCED, context, iod);
	if (!error) {
		/* On error, smb2_rq_init_internal will free the rqp */
		*rqpp = rqp;
	}
    
	return error;
}

/*
 * Similar to smb_rq_bend except matches with smb2_rq_bstart32 meaning it 
 * assumes the use of rqp->sr_lcount
 */
void
smb_rq_bend32(struct smb_rq *rqp)
{
	uint32_t bcnt;
    
	DBG_ASSERT(rqp->sr_lcount);
	if (rqp->sr_lcount == NULL) {
		SMBERROR("no lcount\n");	/* actually panic */
		return;
	}
	/*
	 * Byte Count field should be ignored when dealing with SMB_CAP_LARGE_WRITEX 
	 * or SMB_CAP_LARGE_READX messages. So we set it to zero in these cases.
	 */
	if (((SESSION_CAPS(rqp->sr_session) & SMB_CAP_LARGE_READX) && (rqp->sr_cmd == SMB_COM_READ_ANDX)) ||
		((SESSION_CAPS(rqp->sr_session) & SMB_CAP_LARGE_WRITEX) && (rqp->sr_cmd == SMB_COM_WRITE_ANDX))) {
		/* SAMBA 4 doesn't like the byte count to be zero */
		if (rqp->sr_rq.mb_count > 0xffffffff) 
			bcnt = 0; /* Set the byte count to zero here */
		else
			bcnt = (uint32_t)rqp->sr_rq.mb_count;
	} else if (rqp->sr_rq.mb_count > 0xffffffff) {
		SMBERROR("byte count too large (%ld)\n", rqp->sr_rq.mb_count);
		bcnt =  0xfffffff;	/* not sure what else to do here */
	} else
		bcnt = (uint32_t)rqp->sr_rq.mb_count;
	
	*rqp->sr_lcount = htolel(bcnt);
}

/* 
 * Similar to smb_rq_bstart except uses an uint16 len field
 */
void
smb2_rq_bstart(struct smb_rq *rqp, uint16_t *len_ptr)
{
	rqp->sr_bcount = len_ptr;
	rqp->sr_rq.mb_count = 0;
}

/* 
 * Similar to smb_rq_bstart except uses any uint32 len field
 */
void
smb2_rq_bstart32(struct smb_rq *rqp, uint32_t *len_ptr)
{
	rqp->sr_lcount = len_ptr;
	rqp->sr_rq.mb_count = 0;
}

/*
 * SMB 2/3 crediting
 *
 * CreditCharge - number of credits consumed by response
 * CreditRequest/CreditResponse - used to request more credits.
 *      >1 = requesting more credits
 *      1 = maintain current number of credits
 *      0 = decrease number of current credits
 *
 * Calls that can use multi credits are
 *      Read/Write - len is the IO size
 *      IOCTL - dont think we support any of these specific ones
 *      Query Dir - output buffer length
 *      Change Notify - output buffer length
 *      Query Info - output buffer length
 *      Set Info - output buffer length
 *
 * CreditCharge = ((RequestLength - 1) / (64 * 1024)) + 1;
 * Dont forget to increment the MessageIDs by the same amount 
 */
uint32_t
smb2_rq_credit_check(struct smb_rq *rqp, uint32_t len)
{
    /* This function returns the allowable length based on avail credits */
    int16_t credit_charge16;
    int32_t credit_charge32;
    int32_t curr_credits;
    uint32_t ret_len = len;
    
    /*
     * Session Credit lock must be held before calling this function
     */
    
    /* Do simple checks first */
    if (len <= (64 * 1024)) {
        /* default of one credit is fine and length is fine as is */
        return ret_len;
    }
    
    /* How many credits do we have? */
    curr_credits = OSAddAtomic(0, &rqp->sr_iod->iod_credits_granted);
    
    if (curr_credits <= kCREDIT_LOW_WATER) {
        /* 
         * Running low on credits, so stay with just default of one credit.
         * Adjust len to be just 64K to fit in one credit
         */
        ret_len = 64 * 1024;
        return ret_len;
    }
    
    /* how many credits are needed? */
    credit_charge16 = ((len - 1) / (64 * 1024)) + 1;
    credit_charge32 = credit_charge16;
    
    if ((credit_charge32 + kCREDIT_LOW_WATER) > curr_credits) {
        /* Not enough credits left, so reduce len of request */
        ret_len = (curr_credits - kCREDIT_LOW_WATER) * (64 * 1024);
        credit_charge16 = curr_credits - kCREDIT_LOW_WATER;
    }
    
    /* Update credit charge */
    rqp->sr_creditcharge = credit_charge16;
    
    DBG_ASSERT(ret_len != 0);
    return ret_len;
}

/*
 * Decrement session_credits_granted by number of credits charged in request
 * See smb2_rq_credit_check() for longer description
 */
static int
smb2_rq_credit_decrement(struct smb_rq *rqp, uint32_t *rq_len)
{
    int32_t curr_credits = 0;
    int16_t credit_charge = 0;
    struct timespec ts = {0};
    int ret = 0;
    int error = 0;
    struct smb_session *sessionp = NULL;
    uint32_t sleep_cnt = 0, i = 0;
    uint64_t message_id_diff = 0;
    struct smbiod *iod = NULL;
    
	if (rqp == NULL) {
        SMBERROR("rqp is NULL\n");
		return ENOMEM;
    }
    iod = rqp->sr_iod;

	if (rqp->sr_session == NULL) {
        SMBERROR("rqp->sr_session is NULL\n");
		return ENOMEM;
    }

    SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_DECREMENT | DBG_FUNC_START, smb_hideaddr(rqp), iod->iod_id, iod->iod_credits_max, rq_len != NULL ? *rq_len : 0, 0);

    sessionp = rqp->sr_session;

    SMBC_CREDIT_LOCK(iod);

    switch (rqp->sr_command) {
        case SMB2_NEGOTIATE:
            /*
             * Negotiate is a special case where credit charge and credits
             * requested are both 0. Its expected server will grant at least 1
             * credit in Neg rsp
             */
            rqp->sr_creditcharge = 0;
            rqp->sr_creditsrequested = 0;
            
            /* Reset starting credits to be 0 in case of reconnect */
            iod->iod_credits_granted = 0;
            iod->iod_credits_ss_granted = 0;
            iod->iod_credits_max = 0;
            
            goto out;
            
        case SMB2_SESSION_SETUP:
            /*
             * We need more than a minimum of 3 credits in order to send
             * at least one compound request instead of hanging waiting for 
             * more credits.
             */
            rqp->sr_creditsrequested = kCREDIT_REQUEST_AMT;
            goto out;

        case SMB2_TREE_CONNECT:
            rqp->sr_creditsrequested = kCREDIT_REQUEST_AMT;
            /* Go ahead and fall through */
        case SMB2_LOGOFF:
            /*
             * These commands are used during reconnect and we assume that 
             * we always have one credit to use.
             */
            goto out;
            
		case SMB2_OPLOCK_BREAK:
			/*
			 * <32083736> This command has the iod_context but needs to still
			 * go through the regular crediting code
			 */
			break;

		default:
            if (!(sessionp->session_flags & SMBV_SMB2)) {
                /*
                 * Huh? Why are we trying to send SMB 2/3 request on SMB 1
                 * connection. This is not allowed. Need to find the code path
                 * that got to here and fix it.
                 */
                SMBERROR("id %u SMB 2/3 not allowed on SMB 1 connection. cmd = %x\n",
                         iod->iod_id, rqp->sr_command);
                error = ERPCMISMATCH;
                goto out;
            }
            
            if (rqp->sr_context == sessionp->session_iod->iod_context) {
                /* 
                 * More reconnect commands, assume that we always have one
                 * credit to use 
                 */
                goto out;
            }
            
            /* All other requests must go through regular crediting code */
            break;
    }
    
    /*
     * Check to see if need to pause sending until we get more credits 
     * Two ways to run out of credits.
     * 1) Just have no credits left
     * 2) (curr message ID) - (oldest pending message ID) > current credits
     */
 	for (;;) {
        curr_credits = OSAddAtomic(0, &iod->iod_credits_granted);

        SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_DECREMENT | DBG_FUNC_NONE, 0xabc001, iod->iod_id, curr_credits, 0, 0);

        if (curr_credits >= kCREDIT_MIN_AMT) {
            if (iod->iod_req_pending == 0) {
                /* Have enough credits and no pending reqs, so go send it */
                break;
            }
        
            /* Have a pending request, see if send window is open */
            if (iod->iod_message_id > iod->iod_oldest_message_id) {
                message_id_diff = iod->iod_message_id - iod->iod_oldest_message_id;
            }
            else {
                /* Must have wrapped around */
                message_id_diff = UINT64_MAX - iod->iod_oldest_message_id;
                message_id_diff += iod->iod_message_id;
            }
            
            if (message_id_diff <= (uint64_t) curr_credits) {
                /* Send window still open, so go send it */
                break;
            }
        }
        
        if ((rqp->sr_command == SMB2_ECHO) ||
            (rqp->sr_extflags & SMB2_REQ_NO_BLOCK)) {
            /*
             * Can not block waiting here for credits for an Echo request.
             * Echo request is sent by smb_iod_sendall() and that is the same
             * function that times out requests that have not gotten their
             * replies. Just skip sending the Echo request and return an error.
             */
            error = ENOBUFS;
            goto out;
        }
        
        /* If the share is going away, just return immediately */
        if ((rqp->sr_share) &&
            (rqp->sr_share->ss_going_away) &&
            (rqp->sr_share->ss_going_away(rqp->sr_share))) {
            error = ENXIO;
            goto out;
        }

        /* If the channel is going away, just return immediately */
        if (iod->iod_flags & SMBIOD_SHUTDOWN) {  // TBD: This may not be the appropriate handling. Need to re-enque on a valid iod
            SMBDEBUG("id %u channel is shutting down", iod->iod_id);
            error = ENXIO;
            goto out;
        }

        /* Block until we get more credits */
        SMBDEBUG("id %d Wait for credits curr %d max %d curr ID %lld pending ID %lld session_credits_wait %d iod_credits_granted %d\n",
                 iod->iod_id, curr_credits, iod->iod_credits_max,
                 iod->iod_message_id, iod->iod_oldest_message_id,
                 iod->iod_credits_wait, iod->iod_credits_granted);

        SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_DECREMENT | DBG_FUNC_NONE, 0xabc002, iod->iod_id, iod->iod_message_id, iod->iod_oldest_message_id, 0);

        /* Only wait a max of 60 seconds waiting for credits */
        sleep_cnt = 60;
        ts.tv_sec = 1;
        ts.tv_nsec = 0;
        
        for (i = 1; i <= sleep_cnt; i++) {
            /* Indicate that we are sleeping by incrementing wait counter */
            OSAddAtomic(1, &iod->iod_credits_wait);
            
            SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_DECREMENT | DBG_FUNC_NONE, 0xabc003, iod->iod_id, iod->iod_credits_wait, 0, 0);

            ret = msleep(&iod->iod_credits_wait, SMBC_CREDIT_LOCKPTR(iod),
                         PWAIT, "session-credits-wait", &ts);

            if (ret == EWOULDBLOCK) {
                /* Timed out, so decrement wait counter */
                if (iod->iod_credits_wait) {
                    OSAddAtomic(-1, &iod->iod_credits_wait);
                }

                SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_DECREMENT | DBG_FUNC_NONE, 0xabc004, iod->iod_id, iod->iod_credits_wait, 0, 0);

                /* If the share is going away, just return immediately */
                if ((rqp->sr_share) &&
                    (rqp->sr_share->ss_going_away) &&
                    (rqp->sr_share->ss_going_away(rqp->sr_share))) {
                    error = ENXIO;
                    goto out;
                }
                
                /* If have credits, go and check to see if we can send now */
                curr_credits = OSAddAtomic(0, &iod->iod_credits_granted);
                if (curr_credits > 0) {
                    ret = 0;
                    break;
                }
                
                /* Loop again and wait some more */
            }
            else {
                /* 
                 * If we are woken up and its not EWOULDBLOCK, then must
                 * have credits now, go and check.
                 */
                break;
            }
        }

        if (ret == EWOULDBLOCK) {
            /* 
             * Something really went wrong here. We should not have to wait
             * this long to get any credits. Force a reconnect.
             */
            curr_credits = OSAddAtomic(0, &iod->iod_credits_granted);
            SMBERROR("id %u Timed out waiting for credits curr %d max %d curr ID %lld pending ID %lld session_credits_wait %d\n",
                     iod->iod_id,
                     curr_credits, iod->iod_credits_max,
                     iod->iod_message_id, iod->iod_oldest_message_id,
                     iod->iod_credits_wait);
            
            SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_DECREMENT | DBG_FUNC_NONE, 0xabc005, iod->iod_id, curr_credits, iod->iod_oldest_message_id, 0);

            /* Reconnect requests will need the credit lock so free it */
            SMBC_CREDIT_UNLOCK(iod);

            /*
             * Force this particular iod to reconnect to try to get crediting
             * back in sync with the server. It will either fail over or if
             * its the last channel, then it will do a reconnect.
             */
            (void) smb_session_force_reconnect(iod);
            
            /* Reconnect is done now, reacquire credit lock and try again */
            SMBC_CREDIT_LOCK(iod);
        }
	}
   
    if (rq_len != NULL) {
        /* Possible multi credit request */
        *rq_len = smb2_rq_credit_check(rqp, *rq_len);
    }
    
    credit_charge = rqp->sr_creditcharge;

    /* Decrement number of credits we have left */
    OSAddAtomic(-(credit_charge), &iod->iod_credits_granted);
    
    /* Message IDs are set right before the request is sent */
    
    /* 
     * Check to see if need to request more credits.
     */
    curr_credits = OSAddAtomic(0, &iod->iod_credits_granted);

    SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_DECREMENT | DBG_FUNC_NONE, 0xabc006, iod->iod_id, curr_credits, 0, 0);

    if (curr_credits < kCREDIT_MAX_AMT) {
        /*
         * Currently the client keeps trying to get more and more credits
         * until get kCREDIT_MAX_AMT. Essentially its up to the server to 
         * throttle back our client.
         * Optionally, we could change kCREDIT_MAX_AMT to be kCREDIT_LOW_WATER 
         * so that we only request more credits when we need them. 
         */
        rqp->sr_creditsrequested = kCREDIT_REQUEST_AMT;

        /* Dont access sr_creditreqp as its not set up yet */
    }

    if (curr_credits < 0) {
        SMBERROR("credit count %d < 0 \n", curr_credits);
    }

out:
    SMBC_CREDIT_UNLOCK(iod);

    SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_DECREMENT | DBG_FUNC_END, error, smb_hideaddr(rqp), iod->iod_id, rq_len != NULL ? *rq_len : 0, 0);
    return error;
}

/*
 * Increment session_credits_granted by number of credits granted by server
 * See smb2_rq_credit_check() for longer description
 */
int
smb2_rq_credit_increment(struct smb_rq *rqp)
{
    int32_t curr_credits = 0;
    struct smb_session *sessionp = NULL;
    int error = 0;
    
	if (rqp == NULL) {
        SMBERROR("rqp is NULL\n");
		return (ENOMEM);
    }

    if (rqp->sr_rspcreditsgranted == 0) {
        /* Nothing to do */
        return (0);
    }

	if (rqp->sr_session == NULL) {
        SMBERROR("rqp->sr_session is NULL in %lld \n", rqp->sr_messageid);
		return (ENOMEM);
    }
    sessionp = rqp->sr_session;
    
    struct smbiod *iod = rqp->sr_iod;
    if (iod == NULL) {
        SMBERROR("rqp->sr_iod is NULL in %lld \n", rqp->sr_messageid);
        return (ENOMEM);
    }

    SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_INCREMENT | DBG_FUNC_START, smb_hideaddr(rqp), iod->iod_id, rqp->sr_rspcreditsgranted, 0, 0);

    switch (rqp->sr_command) {
        case SMB2_NEGOTIATE:
        case SMB2_LOGOFF:
            /*
             * These commands are used during reconnect and we assume
             * they are always granted one credit back.
             */
            error = 0;
            goto exit;
            
        case SMB2_SESSION_SETUP:
            /*
             * Save credits granted from Session Setup in separate variable.
             * Once we are ready to start tracking credits then we will set
             * this number as our starting number of credits.
             */
            SMBC_CREDIT_LOCK(iod);
            OSAddAtomic(rqp->sr_rspcreditsgranted, &iod->iod_credits_ss_granted);
            SMBC_CREDIT_UNLOCK(iod);
            error = 0;
            goto exit;
            
        case SMB2_TREE_CONNECT:
            if (rqp->sr_context == sessionp->session_iod->iod_context) {
                /*
                 * <63029037> Tree Connect also requests more credits during
                 * reconnect to work around a bug where the Session Setups
                 * do not grant us enough credits for reconnect.
                 * Note that in reconnect crediting does not start up until
                 * after the tree connects are done.
                 */
                SMBC_CREDIT_LOCK(iod);
                OSAddAtomic(rqp->sr_rspcreditsgranted, &iod->iod_credits_ss_granted);
                SMBC_CREDIT_UNLOCK(iod);
                error = 0;
                goto exit;
            }

            /*
             * If not in reconnect, then let Tree Connect response go through
             * regular crediting code
             */
            break;

        case SMB2_OPLOCK_BREAK:
			/*
			 * <32083736> This command has the iod_context but needs to still 
			 * go through the regular crediting code
			 */
			break;

		default:
            if (rqp->sr_context == sessionp->session_iod->iod_context) {
                /*
                 * More reconnect commands, assume that we always are granted
                 * one credit back
                 */
                error = 0;
                goto exit;
            }

            /* All other responses must go through regular crediting code */
            break;
    }

    SMBC_CREDIT_LOCK(iod);

    OSAddAtomic(rqp->sr_rspcreditsgranted, &iod->iod_credits_granted);

    /* Keep track of max number of credits that server has granted us */
    curr_credits = OSAddAtomic(0, &iod->iod_credits_granted);

    SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_INCREMENT | DBG_FUNC_NONE, 0xabc001, iod->iod_id, curr_credits, 0, 0);

    if ((iod->iod_credits_max < (uint32_t) curr_credits) &&
        (curr_credits < kCREDIT_MAX_AMT)) {
        
        iod->iod_credits_max = curr_credits;
        
        SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_INCREMENT | DBG_FUNC_NONE, 0xabc002, iod->iod_id, curr_credits, 0, 0);
    }
    
	/* Wake up any requests waiting for more credits */
    if (iod->iod_credits_wait) {
        SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_INCREMENT | DBG_FUNC_NONE, 0xabc003, iod->iod_id, iod->iod_credits_wait, 0, 0);

        OSAddAtomic(-1, &iod->iod_credits_wait);
        wakeup(&iod->iod_credits_wait);
	}
    
    SMBC_CREDIT_UNLOCK(iod);

exit:
    SMB_LOG_KTRACE(SMB_DBG_RQ_CREDIT_INCREMENT | DBG_FUNC_END, error, smb_hideaddr(rqp), iod->iod_id, 0, 0);
    return error;
}

void
smb2_rq_credit_preprocess_reply(struct smb_rq *rqp)
{
    int error = 0;
    struct mdchain *mdp = NULL, *shadow_mdp = NULL;
    struct mdchain md_context_shadow = {0};

    /* 
     * We try to parse the SMB2 header now so that we can get the granted
     * server credits sooner. Dont do the signing verification here as it
     * would be single threaded and potentially slower.
     */
    
    if (!(rqp->sr_extflags & SMB2_RESPONSE)) {
        /* Only for SMB v2/3 */
        return;
    }
    
    if (rqp->sr_flags & SMBR_COMPOUND_RQ) {
        /* 
         * Skip If its a compound as they are usually small replies that do
         * not use many credits. Maybe future enhancement will be to handle
         * compound replies.
         */
        return;
    }

    /* Get pointer to response data */
    smb_rq_getreply(rqp, &mdp);

    /* Create a shadow copy so we dont alter original mdp */
    md_shadow_copy(mdp, &md_context_shadow);
    shadow_mdp = &md_context_shadow;

    /* Try to parse out the header just enough to get granted credits */
    error = smb2_rq_parse_header(rqp, &shadow_mdp, 1);
    if (error) {
        /* If it failed, then let regular reply code handle the error */
        SMBERROR("smb2_rq_parse_header failed %d \n", error);
    }
    else {
        /*
         * smb2_rq_parse_header() got the credits granted and incremented
         * current credits. Set flag so we dont double increment the granted
         * credits.
         */
        rqp->sr_extflags |= SMB2_HDR_PREPARSED;
    }
}

/*
 * Set starting number of credits.
 * For the Login/Reconnect commands of Negotiate, Session Setup, Tree Connect,
 * and Log Off, we assume we always have 1 credit and that we always get 1
 * credit back. The credit count is not incremented during this time.
 * During a Login, this is fine as its a synchronous operation
 * with just one request being done at a time. During Reconnect, this is 
 * important as you may have several threads waiting for credits when we go
 * into reconnect. In Reconnect, the credit count goes back to 0. We dont want 
 * to increment our credit count on the replies to these commands as then the 
 * other threads may grab all the credits and then the reconnect commands would 
 * be stuck waiting for more credits that will not arrive. Once Reconnect is 
 * done, then we set the credit count to be the number of credits that we got
 * back from the Session Setup replies.  
 *
 * Each Session Setup request will ask for more credits. The credits granted 
 * in those replies are saved in a separate counter. When crediting is allowed
 * to start up, smb2_rq_credit_start() is called.
 * smb2_rq_credit_start() is called in two cases:
 * 1) Not in reconnect, this is called after Session Setup is done
 * 2) In reconnect, called after reconnect finishes
 *
 * If credits is 0, then set credits to the ones granted by the Session Setup
 * responses.  If credits is non zero, then set it to that value (failed
 * reconnect situation).
 * 
 * The very next commands sent after Logging in or after Reconnect may be 
 * compound request, we need to start with at least > 3 credits.
 */
void
smb2_rq_credit_start(struct smbiod *iod, uint16_t credits)
{
    DBG_ASSERT(iod != NULL);
    
    SMBC_CREDIT_LOCK(iod);
    
    if (credits == 0) {
        /*
         * Set our starting number of credits to the number granted to us from
         * the Session Setup Responses.
         */
        OSAddAtomic(iod->iod_credits_ss_granted, &iod->iod_credits_granted);
    }
    else {
        /* Set to passed in value. Should be due to failed reconnect */
        OSAddAtomic(credits, &iod->iod_credits_granted);
    }
    
    /* Set max credits to same value */
    iod->iod_credits_max = iod->iod_credits_granted;
    
    /* Clear out oldest message ID to open send window */
    iod->iod_req_pending = 0;
    iod->iod_oldest_message_id = 0;

	/* Wake up any requests waiting for more credits */
    if (iod->iod_credits_wait) { 
        OSAddAtomic(-1, &iod->iod_credits_wait);
        wakeup(&iod->iod_credits_wait);
	}

    SMBC_CREDIT_UNLOCK(iod);
}

/*
 * The object is either a share or a session in either case the calling routine
 * must have a reference on the object before calling this routine.
 */
static int 
smb2_rq_init_internal(struct smb_rq *rqp, struct smb_connobj *obj, u_char cmd, 
                      uint32_t *rq_len, int rq_flags, vfs_context_t context, struct smbiod *iod)
{
	int error;
	
    /* Fill in the smb_rq */
	bzero(rqp, sizeof(*rqp));
	rqp->sr_flags |= rq_flags;
	lck_mtx_init(&rqp->sr_slock, srs_lck_group, srs_lck_attr);
	error = smb_rq_getenv(obj, &rqp->sr_session, &rqp->sr_share);
	if (error)
		goto done;
	
	error = smb_session_access(rqp->sr_session, context);
	if (error)
		goto done;
	
    if (cmd & SMB2_NO_BLOCK) {
        cmd &= ~SMB2_NO_BLOCK;
        rqp->sr_extflags |= SMB2_REQ_NO_BLOCK;
    }
    
	rqp->sr_command = cmd;
    rqp->sr_creditcharge = 1;
    rqp->sr_creditsrequested = 1;
    rqp->sr_rqtreeid = 0;
    rqp->sr_rqsessionid = rqp->sr_session->session_session_id;
    
	rqp->sr_context = context;
    rqp->sr_iod = iod;
	rqp->sr_extflags |= SMB2_REQUEST;
    
    /* 
     * Decrement current credit count 
     * ASSUMPTION - a built request will always get sent, otherwise the credit
     * counts and message ids will get out of sync.
     */
    error = smb2_rq_credit_decrement(rqp, rq_len);
    if (error) {
        /* 
         * if got an error, then must not have used any credits and we did not
         * send the request. Set sr_creditcharge to 0 so we do not try to 
         * recover any credits in smb_rq_done()
         */
        rqp->sr_creditcharge = 0;
        goto done;
    }
    
    switch (rqp->sr_command) {
        case SMB2_NEGOTIATE:
            rqp->sr_rqsessionid = 0;
            break;

        case SMB2_SESSION_SETUP:
        case SMB2_LOGOFF:
        case SMB2_ECHO:
        case SMB2_CANCEL:
        case SMB2_TREE_CONNECT:
            break;
            
        default:
            /* If the request has a share then it has a reference on it */
            rqp->sr_rqtreeid = htolel(rqp->sr_share ? 
                                      rqp->sr_share->ss_tree_id : 
                                      SMB2_TID_UNKNOWN);
            break;
    }
    
    /* Create the SMB 2/3 Header */
    error = smb2_rq_new(rqp);
done:
	if (error) {
        rqp->sr_iod = NULL; // smb2_rq_init_internal() should not rel iod
		smb_rq_done(rqp);
	}
	return (error);
}

/*
 * Returns uint32_t length of the SMB 2/3 request
 */
uint32_t
smb2_rq_length(struct smb_rq *rqp)
{
    uint32_t length;
    
    length = (uint32_t) rqp->sr_rq.mb_len;
    return (length);
}

/*
 * Set Message ID in the request and increment iod_message_id by the number
 * of credits used in the request.
 */
int
smb2_rq_message_id_increment(struct smb_rq *rqp)
{
    int64_t message_id = 0;
    struct smb_rq *tmp_rqp;

	if (rqp == NULL) {
        SMBERROR("rqp is NULL\n");
		return ENOMEM;
    }
    
	if (rqp->sr_session == NULL) {
        SMBERROR("rqp->sr_session is NULL\n");
		return ENOMEM;
    }
    
    SMBC_CREDIT_LOCK(rqp->sr_iod);
    
    if (rqp->sr_command == SMB2_NEGOTIATE) {
        /* 
         * Negotiate is a special case where credit charge is 0.
         * Increment message_id by 1.
         */
        message_id = OSAddAtomic64(1, (int64_t*) &rqp->sr_iod->iod_message_id);
        rqp->sr_messageid = message_id;
        *rqp->sr_messageidp = htoleq(rqp->sr_messageid);
        
        goto out;
    }
    
    if (rqp->sr_flags & SMBR_COMPOUND_RQ) {
        /* 
         * Compound Request
         * Set first rqp's message_id 
         * Increment message_id by same amount as the credit charge 
         */
        message_id = OSAddAtomic64(rqp->sr_creditcharge,
                                   (int64_t*) &rqp->sr_iod->iod_message_id);
        rqp->sr_messageid = message_id;
        *rqp->sr_messageidp = htoleq(rqp->sr_messageid);
        
        /* Set message_id in the other requests */
        tmp_rqp = rqp->sr_next_rqp;
        while (tmp_rqp != NULL) {
            /* Increment message_id by same amount as the credit charge */
            message_id = OSAddAtomic64(tmp_rqp->sr_creditcharge,
                                       (int64_t*) &tmp_rqp->sr_iod->iod_message_id);
            tmp_rqp->sr_messageid = message_id;
            *tmp_rqp->sr_messageidp = htoleq(tmp_rqp->sr_messageid);
            
            tmp_rqp = tmp_rqp->sr_next_rqp;
        }
    }
    else {
        /* 
         * Single Request
         * Increment message_id by same amount as the credit charge 
         */
        message_id = OSAddAtomic64(rqp->sr_creditcharge,
                                   (int64_t*) &rqp->sr_iod->iod_message_id);
        rqp->sr_messageid = message_id;
        *rqp->sr_messageidp = htoleq(rqp->sr_messageid);
    }

out:
    SMBC_CREDIT_UNLOCK(rqp->sr_iod);
    
    return 0;
}

/*
 * next_cmd_offset is the total of all next_cmd_offset offsets. Subtract the 
 * bytes that we have parsed and that should leave the number of pad bytes to 
 * consume.
 */
int
smb2_rq_next_command(struct smb_rq *rqp, size_t *next_cmd_offset, 
                     struct mdchain *mdp)
{
    ssize_t pad_bytes = 0;
    int error = 0;
    
    *next_cmd_offset += rqp->sr_rspnextcmd;

    /* take total of next_cmd_offset and subtract what we have parsed */ 
    pad_bytes = *next_cmd_offset - mdp->md_len; 

    if (pad_bytes > 0) {
        error = md_get_mem(mdp, NULL, pad_bytes, MB_MSYSTEM);
        if (error) {
            SMBERROR("md_get_mem failed %d\n", error);
        }
    }
    return(error);
}

static int
smb2_rq_new(struct smb_rq *rqp)
{
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	
    smb_rq_getrequest(rqp, &mbp);
    smb_rq_getreply(rqp, &mdp);
	
	mb_done(mbp);
	md_done(mdp);
	error = mb_init(mbp);
	if (error) {
		return error;
    }
    
    if (!(rqp->sr_flags & SMBR_ASYNC)) {
        /* 
         * Build SMB 2/3 Sync Header
         */
        mb_put_mem(mbp, SMB2_SIGNATURE, SMB2_SIGLEN, MB_MSYSTEM); /* Protocol ID */
        mb_put_uint16le(mbp, SMB2_HDRLEN);                      /* Struct Size */
        rqp->sr_creditchargep = (uint16_t *)mb_reserve(mbp, 2); /* Credit Charge */
        *rqp->sr_creditchargep = htoles(rqp->sr_creditcharge);
        mb_put_uint32le(mbp, 0);                                /* Status */
        mb_put_uint16le(mbp, rqp->sr_command);                  /* Command */
        rqp->sr_creditreqp = (uint16_t *)mb_reserve(mbp, 2);    /* Credit Req/Rsp */
        *rqp->sr_creditreqp = htoles(rqp->sr_creditsrequested);
        rqp->sr_flagsp = (uint32_t *)mb_reserve(mbp, 4);        /* Flags */
        *rqp->sr_flagsp = htolel(rqp->sr_rqflags);
        rqp->sr_nextcmdp = (uint32_t *)mb_reserve(mbp, 4);      /* Next command */
        *rqp->sr_nextcmdp = htolel(rqp->sr_nextcmd);
        rqp->sr_messageidp = (uint64_t *)mb_reserve(mbp, 8);    /* Message ID */
        bzero(rqp->sr_messageidp, 8);
        mb_put_uint32le(mbp, 0xFEFF);                           /* Process ID */
        mb_put_uint32le(mbp, rqp->sr_rqtreeid);                 /* Tree ID */
        mb_put_uint64le(mbp, rqp->sr_rqsessionid);              /* Session ID */
        rqp->sr_rqsig = (uint8_t *)mb_reserve(mbp, 16);         /* Signature */
        bzero(rqp->sr_rqsig, 16);
    }
    else {
        /* 
         * Build SMB 2/3 Async Header
         */
        mb_put_mem(mbp, SMB2_SIGNATURE, SMB2_SIGLEN, MB_MSYSTEM); /* Protocol ID */
        mb_put_uint16le(mbp, SMB2_HDRLEN);                      /* Struct Size */
        rqp->sr_creditchargep = (uint16_t *)mb_reserve(mbp, 2); /* Credit Charge */
        *rqp->sr_creditchargep = htoles(rqp->sr_creditcharge);
        mb_put_uint32le(mbp, 0);                                /* Status */
        mb_put_uint16le(mbp, rqp->sr_command);                  /* Command */
        rqp->sr_creditreqp = (uint16_t *)mb_reserve(mbp, 2);    /* Credit Req/Rsp */
        *rqp->sr_creditreqp = htoles(rqp->sr_creditsrequested);
        rqp->sr_flagsp = (uint32_t *)mb_reserve(mbp, 4);        /* Flags */
        rqp->sr_rqflags |= SMB2_FLAGS_ASYNC_COMMAND;
        *rqp->sr_flagsp = htolel(rqp->sr_rqflags);
        rqp->sr_nextcmdp = (uint32_t *)mb_reserve(mbp, 4);      /* Next command */
        *rqp->sr_nextcmdp = htolel(rqp->sr_nextcmd);
        rqp->sr_messageidp = (uint64_t *)mb_reserve(mbp, 8);    /* Message ID */
        bzero(rqp->sr_messageidp, 8);
        mb_put_uint64le(mbp, 0);                                /* Async ID */
        mb_put_uint64le(mbp, rqp->sr_rqsessionid);              /* Session ID */
        rqp->sr_rqsig = (uint8_t *)mb_reserve(mbp, 16);         /* Signature */
        bzero(rqp->sr_rqsig, 16);
    }
    
	return 0;
}

/*
 * Parses SMB 2/3 Response Header
 * For non compound responses, the mdp is from the rqp.
 * For compound responses, the mdp may be from the first rqp in the chain and 
 * not from the rqp passed into this function. 
 */
uint32_t
smb2_rq_parse_header(struct smb_rq *rqp, struct mdchain **mdp, uint32_t parse_for_credits)
{
	int error = 0, rperror = 0;
    uint32_t protocol_id = 0;
	uint16_t length = 0, credit_charge = 0, command = 0;
    uint64_t message_id = 0;
    uint64_t async_id = 0;
    struct mdchain md_sign = {0};
    uint32_t encryption_on = 0;
    uint8_t signature[16] = {0};

    /* 
     * Parse SMB 2/3 Header
     * We are already pointing to begining of header data
     */

	if (rqp == NULL) {
        SMBERROR("rqp is NULL\n");
		error = ENOMEM;
        goto bad;
    }
    
    /*
     * <14227703> If SMB2_RESPONSE is set, then the reply is in the rqp
     * Must be a server that does not support compound replies
     */
    if ((rqp->sr_extflags & SMB2_RESPONSE) &&
        (parse_for_credits == 0)) {
        /* Get pointer to response data */
        smb_rq_getreply(rqp, mdp);
    }

    /*
     * Hold on to a copy of the mdchain before we touch it,
     * since we will need to verify the signature
     * if signing is turned on
     */
    md_sign = **mdp;

    /* Get Protocol ID */
    error = md_get_uint32le(*mdp, &protocol_id);
    if (error) {
        goto bad;
    }
    
    /* Check structure size is 64 */
    error = md_get_uint16le(*mdp, &length);
    if (error) {
        goto bad;
    }
    if (length != 64) {
        SMBERROR("Bad struct size: %u\n", (uint32_t) length);
        error = EBADRPC;
        goto bad;
    }
    
    /* Get Credit Charge */
    error = md_get_uint16le(*mdp, &credit_charge);
    if (error) {
        goto bad;
    }
   
    /* Get Status */
    error = md_get_uint32le(*mdp, &rqp->sr_ntstatus);
    if (error) {
        goto bad;
    }
    
    /* Get Command */
    error = md_get_uint16le(*mdp, &command);
    if (error) {
        goto bad;
    }
    
    /* Get Credits Granted */
    error = md_get_uint16le(*mdp, &rqp->sr_rspcreditsgranted);
    if (error) {
        goto bad;
    }

    if (rqp->sr_rspcreditsgranted != 0) {
        /* Increment current credits granted */
        if (!(rqp->sr_extflags & SMB2_HDR_PREPARSED)) {
            /* Has not been preprocessed, so increment granted credits */
            smb2_rq_credit_increment(rqp);
        }
    }
    
    if (parse_for_credits == 1) {
        /* Thats all far as we need to parse for now, just leave */
        goto bad;
    }

    /* Get Flags */
    error = md_get_uint32le(*mdp, &rqp->sr_rspflags);
    if (error) {
        goto bad;
    }
    
    /* Get Next Command offset */
    error = md_get_uint32le(*mdp, &rqp->sr_rspnextcmd);
    if (error) {
        goto bad;
    }
   
    /* Get Message ID */
    error = md_get_uint64le(*mdp, &message_id);
    if (error) {
        goto bad;
    }
    
    if (!(rqp->sr_rspflags & SMB2_FLAGS_ASYNC_COMMAND)) {
        /* 
         * Sync Header 
         */
        
        /* Get Process ID */
        error = md_get_uint32le(*mdp, &rqp->sr_rsppid);
        if (error) {
            goto bad;
        }
        
        /* Get Tree ID */
        error = md_get_uint32le(*mdp, &rqp->sr_rsptreeid);
        if (error) {
            goto bad;
        }
    }
    else {
        /* 
         * Async Header 
         */
        
        /* Get Async ID */
        error = md_get_uint64le(*mdp, &async_id);
        if (error) {
            goto bad;
        }

        if (async_id != rqp->sr_rspasyncid) {
            SMBERROR("Async rsp ids do not match: id %lld async_id %lld ! = %lld\n", 
                     message_id, async_id, rqp->sr_rspasyncid);
            error = EBADRPC;
            goto bad;
        }
    }

    /* Get Session ID */
    error = md_get_uint64le(*mdp, &rqp->sr_rspsessionid);
    if (error) {
        goto bad;
    }

    /* Get Signature */
	error = md_get_mem(*mdp, (caddr_t) &signature, sizeof(signature), MB_MSYSTEM);
    if (error) {
        goto bad;
    }

    /* Can skip signature verification if we're encrypting */
    encryption_on = 0;
    
	if (SMBV_SMB3_OR_LATER(rqp->sr_session)) {
        /* Check if session is encrypted */
        if (rqp->sr_session->session_sopt.sv_sessflags & SMB2_SESSION_FLAG_ENCRYPT_DATA) {
            if (rqp->sr_command != SMB2_NEGOTIATE) {
                encryption_on = 1;
            }
        } else if (rqp->sr_share != NULL) {
            /* Check if share is encrypted */
            if ( (rqp->sr_command != SMB2_NEGOTIATE) &&
                (rqp->sr_command != SMB2_SESSION_SETUP) &&
                (rqp->sr_command != SMB2_TREE_CONNECT) &&
                (rqp->sr_share->ss_share_flags & SMB2_SHAREFLAG_ENCRYPT_DATA) ){
                encryption_on = 1;
            }
        }
    }
    
    /* If it's signed and encryption is off, then verify the signature */
    if ( (error == 0) && (encryption_on == 0) &&
        ((rqp->sr_session->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) ||
         (rqp->sr_flags & SMBR_SIGNED))) {
        error = smb2_rq_verify(rqp, &md_sign, signature);
    }
    
    /*
     * If the signature failed verification, do not bother with
     * looking at sr_ntstatus, just punt.
     */
    if (error) {
        goto bad;
    }
    
    /* Convert NT Status to an errno value */
    rperror = smb_ntstatus_to_errno(rqp->sr_ntstatus);
    
    switch (rqp->sr_ntstatus) {
        case STATUS_INSUFFICIENT_RESOURCES:
            SMBWARNING("id %u: STATUS_INSUFFICIENT_RESOURCES: while attempting cmd %x\n",
                       (rqp->sr_iod)?(rqp->sr_iod->iod_id):(-1), rqp->sr_cmd);
            break;
            
        case STATUS_NETWORK_SESSION_EXPIRED:
        case STATUS_USER_SESSION_DELETED:
            /*
             * This error check is for the situation where a share is mounted
             * for a while and the server has expired the session. All requests
             * will get these errors until we do a reconnect which essentially
             * logs us back in.
             *
             * With multichannel, all channels will be getting
             * these errors and we need to fail them all over until we get to
             * just one main channel and have that channel do a reconnect.
             *
             * <71930272> Check to see if its an alt channel or if its the main
             * channel (which could be doing the reconnect).
             *      1. If its an alt channel, force reconnect.
             *          a. If the session is established, the channel will try
             *             to fail over in the reconnect code.
             *          b. If in trials (context is iod_context), then just
             *             return so we dont deadlock against ourselves.
             *      2. If its the main channel and a reconnect request, return
             *         an error so we dont recurse into the reconnect code
             *         again. Use special error code to cause the share to be
             *         unmounted since reconnect has obviously failed.
             *      3. If its the main channel and not a reconnect request, try
             *         to force reconnect. Main will either fail over or if
             *         its the last channel, it will start reconnect.
             */
            if (rqp->sr_iod == NULL) {
                /* Should be impossible and make static analyzer happy */
                SMBERROR("rqp->sr_iod is null??? cmd 0x%x", rqp->sr_cmd);
                break;
            }

            if (rqp->sr_iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
                /* Its an alt channel, is it in trials? */
                if (rqp->sr_context == rqp->sr_iod->iod_context) {
                    /*
                     * Alt channel is in trials so dont try to force reconnect
                     * to avoid a deadlock. The iod cant process an iod event of
                     * force reconnect if the iod is currently busy right here
                     * waiting for the sync iod event to finish.
                     */
                    SMBWARNING("id %u Session Expired or Deleted on Alt Channel while in trials cmd 0x%x. Disconnecting.\n",
                               rqp->sr_iod->iod_id, rqp->sr_cmd);
                    /* Leave rperror code unchanged */
                }
                else {
                    /*
                     * Its established alt channel, force it to reconnect/fail
                     * over
                     */
                    SMBWARNING("id %u Session Expired or Deleted on Alt Channel while attempting cmd 0x%x. Reconnecting.\n",
                               rqp->sr_iod->iod_id, rqp->sr_cmd);
                    (void) smb_session_force_reconnect(rqp->sr_iod);
                    /* Leave rperror code unchanged */
                }
            }
            else {
                /* Its the main channel, is it in reconnect? */
                if (rqp->sr_context == rqp->sr_session->session_iod->iod_context) {
                    /*
                     * Its a reconnect command so dont recurse into reconnect
                     * again. Just fail reconnect and unmount this share.
                     */
                    SMBWARNING("id %u Session Expired or Deleted on Main Channel while reconnecting cmd 0x%x. Disconnecting.\n",
                               rqp->sr_iod->iod_id, rqp->sr_cmd);
                    /* Set rperror code to unmount this share */
                    rperror = ENETRESET;
                }
                else {
                    /*
                     * Either single channel, or last channel remaining, try
                     * to do a reconnect
                     */
                    SMBWARNING("id %u Session Expired or Deleted on Main Channel while attempting cmd 0x%x. Reconnecting.\n",
                               rqp->sr_iod->iod_id, rqp->sr_cmd);

                   (void) smb_session_force_reconnect(rqp->sr_iod);
                    /* Leave rperror code unchanged */
                }
            }
            break;
            
        default:
            break;
    }
    
    /* The tree has gone away, umount the volume. */
	if ((rperror == ENETRESET) && rqp->sr_share) {
		lck_mtx_lock(&rqp->sr_share->ss_shlock);
        if (rqp->sr_share->ss_dead) {
            rqp->sr_share->ss_dead(rqp->sr_share);
        }
		lck_mtx_unlock(&rqp->sr_share->ss_shlock);
	}
	
    /* Need bigger buffer? */
	if (rperror && (rqp->sr_ntstatus == STATUS_BUFFER_TOO_SMALL)) {
		rqp->sr_flags |= SMBR_MOREDATA;
	} 
    else {
		rqp->sr_flags &= ~SMBR_MOREDATA;
	}

bad:
	return error ? error : rperror;
}

/*
 * Sets SMB 2/3 Header Flags and NextCommand for Compound req chains
 */
int
smb2_rq_update_cmpd_hdr(struct smb_rq *rqp, uint32_t position_flag)
{    
	if (rqp == NULL)
		return ENOMEM;
    
	if (rqp->sr_nextcmdp == NULL)
		return ENOMEM;
    
    if (rqp->sr_flagsp == NULL)
        return ENOMEM;
    
    switch (position_flag) {
        case SMB2_CMPD_FIRST:
            /* 
             * Do not set SMB2_FLAGS_RELATED_OPERATIONS
             * Set next command offset
             */
            *rqp->sr_nextcmdp = htolel(smb2_rq_length(rqp));
            break;
        case SMB2_CMPD_MIDDLE:
            /* 
             * Set SMB2_FLAGS_RELATED_OPERATIONS
             * Set next command offset
             */
            *rqp->sr_nextcmdp = htolel(smb2_rq_length(rqp));
            *rqp->sr_flagsp |= htolel(SMB2_FLAGS_RELATED_OPERATIONS);
            break;
        case SMB2_CMPD_LAST:
            /* 
             * Set SMB2_FLAGS_RELATED_OPERATIONS
             * Set next command offset to be 0
             */
            *rqp->sr_nextcmdp = htolel(0);
            *rqp->sr_flagsp |= htolel(SMB2_FLAGS_RELATED_OPERATIONS);
            break;
            
        default:
            SMBERROR("Unknown postion_flag %d\n", position_flag);
            return ENOMEM;
            break;
    }
    
    return 0;
}





