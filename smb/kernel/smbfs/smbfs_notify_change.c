/*
 * Copyright (c) 2006 - 2012 Apple Inc. All rights reserved.
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

#include <sys/param.h>
#include <sys/kauth.h>
#include <libkern/OSAtomic.h>

#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include "smbfs_notify_change.h"

extern lck_attr_t *smbfs_lock_attr;
extern lck_grp_t *smbfs_mutex_group;

#define NOTIFY_CHANGE_SLEEP_TIMO	15
#define NOTIFY_THROTTLE_SLEEP_TIMO	5
#define SMBFS_MAX_RCVD_NOTIFY		4
#define SMBFS_MAX_RCVD_NOTIFY_TIME	1


/* For now just notify on these item, may want to watch on more in the future */
#define SMBFS_NOTIFY_CHANGE_FILTERS	FILE_NOTIFY_CHANGE_FILE_NAME | \
									FILE_NOTIFY_CHANGE_DIR_NAME | \
									FILE_NOTIFY_CHANGE_ATTRIBUTES | \
									FILE_NOTIFY_CHANGE_CREATION | \
									FILE_NOTIFY_CHANGE_SECURITY | \
									FILE_NOTIFY_CHANGE_STREAM_SIZE | \
									FILE_NOTIFY_CHANGE_STREAM_WRITE

/* For server message notify, we set everything, which is one of the
 * ways the server can tell it's a server message notify, and not
 * a normal notify change type.
 */
#define SMBFS_SVRMSG_NOTIFY_FILTERS	FILE_NOTIFY_CHANGE_FILE_NAME | \
                                    FILE_NOTIFY_CHANGE_DIR_NAME | \
                                    FILE_NOTIFY_CHANGE_ATTRIBUTES | \
                                    FILE_NOTIFY_CHANGE_SIZE	| \
                                    FILE_NOTIFY_CHANGE_LAST_WRITE | \
                                    FILE_NOTIFY_CHANGE_LAST_ACCESS |\
                                    FILE_NOTIFY_CHANGE_CREATION | \
                                    FILE_NOTIFY_CHANGE_EA | \
                                    FILE_NOTIFY_CHANGE_SECURITY | \
                                    FILE_NOTIFY_CHANGE_STREAM_NAME | \
                                    FILE_NOTIFY_CHANGE_STREAM_SIZE | \
                                    FILE_NOTIFY_CHANGE_STREAM_WRITE

/*
 * notify_wakeup
 *
 * Wake up the thread and tell it there is work to be done.
 *
 */
static void 
notify_wakeup(struct smbfs_notify_change * notify)
{
	notify->haveMoreWork = TRUE;		/* we have work to do */
	wakeup(&(notify)->notify_state);
}

/*
 * notify_callback_completion
 */
static void 
notify_callback_completion(void *call_back_args)
{	
	struct watch_item *watchItem = (struct watch_item *)call_back_args;
	
	lck_mtx_lock(&watchItem->watch_statelock);
	if ((watchItem->state != kCancelNotify) && 
		(watchItem->state != kWaitingForRemoval)) {
		watchItem->state = kReceivedNotify;
	}
	lck_mtx_unlock(&watchItem->watch_statelock);
	notify_wakeup(watchItem->notify);	
}

/*
 * reset_notify_change
 *
 * Remove  the request from the network queue. Now cleanup and remove any
 * allocated memory.
 */
static void 
reset_notify_change(struct watch_item *watchItem, int RemoveRQ)
{
	struct smb_ntrq *ntp = watchItem->ntp;
	struct smb_rq *	rqp = (watchItem->ntp) ? watchItem->ntp->nt_rq : NULL;
	
    if (watchItem->flags & SMBV_SMB2) {
        /* Using SMB 2/3 */
        rqp = watchItem->rqp;
    }
    
    if (rqp) {
		if (RemoveRQ) {
            /* Needs to be removed from the queue */
            smb_iod_removerq(rqp);
            if (ntp) {
                watchItem->ntp->nt_rq = NULL;
            }
        }
		smb_rq_done(rqp);
	}
	if (ntp)
		smb_nt_done(ntp);
    
    watchItem->ntp = NULL;

    if (watchItem->flags & SMBV_SMB2) {
        watchItem->rqp = NULL;
    }
}

/*
 * smbfs_notified_vnode
 *
 * See if we can update the node and notify the monitor.
 */
static void 
smbfs_notified_vnode(struct smbnode *np, int throttleBack, uint32_t events, 
					 vfs_context_t context)
{
	struct smb_share *share = NULL;
	struct vnode_attr vattr;
	vnode_t		vp;
	
	if ((np->d_fid == 0) || (smbnode_lock(np, SMBFS_SHARED_LOCK) != 0)) {
		return; /* Nothing to do here */
    }
	
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_NOTIFY | DBG_FUNC_START,
                   throttleBack, events, np->d_fid, 0, 0);

    if (!throttleBack) {
        /*
         * Always reset the cache timer and force a lookup except for ETIMEDOUT
         * where we want to return cached meta data if possible. When we stop
         * throttling, we will do an update at that time.
         */
        np->attribute_cache_timer = 0;
        np->n_symlink_cache_timer = 0;
    }
    
	/* 
	 * The fid changed while we were blocked just unlock and get out. If we are
	 * throttling back then skip this notify.  
	 */  
	if ((np->d_fid == 0) || throttleBack) {
		goto done;
    }

	np->n_lastvop = smbfs_notified_vnode;
	vp = np->n_vnode;
    
    /* If they have a nofication with a smbnode, then we must have a vnode */
    if (vnode_get(vp)) {
        /* The vnode could be going away, skip out nothing to do here */
		goto done;
    }
    /* Should never happen but lets test and make sure */
     if (VTOSMB(vp) != np) {
         SMBWARNING_LOCK(np, "%s vnode_fsnode(vp) and np don't match!\n", np->n_name);
         vnode_put(vp);
         goto done;        
    }
	
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	vfs_get_notify_attributes(&vattr);
	smbfs_attr_cachelookup(share, vp, &vattr, context, TRUE);
	smb_share_rele(share, context);

	vnode_notify(vp, events, &vattr);
	vnode_put(vp);
	events = 0;
	
done:
	if (events == 0)	/* We already process the event */
		np->d_needsUpdate = FALSE;
	else		/* Still need to process the event */
		np->d_needsUpdate = TRUE;
	smbnode_unlock(np);

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_NOTIFY | DBG_FUNC_END, 0, 0, 0, 0, 0);
}

/*
 * process_notify_change
 *
 */
static uint32_t 
process_notify_change(struct smb_ntrq *ntp)
{
	uint32_t events = 0;
	struct mdchain *mdp;
	uint32_t nextoffset = 0, action;
	int error = 0;
	size_t rparam_len = 0;
	
	mdp = &ntp->nt_rdata;
	if (mdp->md_top) {
#ifdef SMB_DEBUG
		size_t rdata_len = m_fixhdr(mdp->md_top);
		SMBDEBUG("rdata_len = %d \n", (int)rdata_len);
#else // SMB_DEBUG
		m_fixhdr(mdp->md_top);
#endif // SMB_DEBUG
		md_initm(mdp, mdp->md_top);
	}
	mdp = &ntp->nt_rparam;
	if (mdp->md_top) {
		rparam_len = m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
		SMBDEBUG("rrparam_len = %d\n", (int)rparam_len);
	}
	/* 
	 * Remeber the md_get_ routines protect us from buffer overruns. Note that
	 * the server doesn't have to return any data, so no next offset field is
	 * not an error.
	 */
	if (rparam_len && (md_get_uint32le(mdp, &nextoffset) == 0))
		do {
			/* since we already moved pass next offset don't count it */
			if (nextoffset >= sizeof(uint32_t))
				nextoffset -= (uint32_t)sizeof(uint32_t);
			
			error = md_get_uint32le(mdp, &action);				
			if (error)
				break;
			
			/* since we already moved pass action don't count it */
			if (nextoffset >= sizeof(uint32_t))
				nextoffset -= (uint32_t)sizeof(uint32_t);
				
			if (nextoffset) {
				error = md_get_mem(mdp, NULL, nextoffset, MB_MSYSTEM);
				if (!error)
					error = md_get_uint32le(mdp, &nextoffset);
				if (error)
					break;			
			}
			
			SMBDEBUG("action = 0x%x \n", action);
			switch (action) {
				case FILE_ACTION_ADDED:
					events |= VNODE_EVENT_FILE_CREATED | VNODE_EVENT_DIR_CREATED;
					break;
				case FILE_ACTION_REMOVED:
					events |= VNODE_EVENT_FILE_REMOVED | VNODE_EVENT_DIR_REMOVED;
					break;
				case FILE_ACTION_MODIFIED:
					events |= VNODE_EVENT_ATTRIB;
					break;
				case FILE_ACTION_RENAMED_OLD_NAME:
				case FILE_ACTION_RENAMED_NEW_NAME:
					events |= VNODE_EVENT_RENAME;
					break;
				case FILE_ACTION_ADDED_STREAM:
				case FILE_ACTION_REMOVED_STREAM:
				case FILE_ACTION_MODIFIED_STREAM:
					/* Should we try to clear all named stream cache? */
					events |= VNODE_EVENT_ATTRIB;
					break;
				default:
					error = ENOTSUP;
					break;
			}
		} while (nextoffset);
	
	if (error || (events == 0))
		events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
	if (error) {
		SMBWARNING("error = %d\n", error);
	}
	return events;
}

/* 
 * Proces a change notify message from the server
 */
static int 
rcvd_notify_change(struct watch_item *watchItem, vfs_context_t context)
{
	struct smbnode *np = watchItem->np;	
	struct smb_ntrq *ntp = watchItem->ntp;
	struct smb_rq *	rqp = (watchItem->ntp) ? watchItem->ntp->nt_rq : NULL;
	int error = 0;
	uint32_t events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
	
    if (watchItem->flags & SMBV_SMB2) {
        /* Using SMB 2/3 */
        rqp = watchItem->rqp;

        if (rqp) {
            error = smb2_smb_parse_change_notify(rqp, &events);
        }
    }
    else {
        if (rqp) {
            /* 
             * NOTE: smb_nt_reply calls smb_rq_reply which will remove the rqp from 
             * the main threads queue. So when we are done here call reset_notify_change 
             * but tell it not to remove the request from the queue.
             */
            error = smb_nt_reply(ntp);
            if (!error)
                events = process_notify_change(ntp);
        }
    }
	
    if (error == ECANCELED) {
        /*
         * Either we close the file descriptor or we canceled the 
         * operation. Nothing else to do here just get out.
         */
        SMBDEBUG_LOCK(np, "Notification for %s was canceled.\n", np->n_name);
        goto done;
    }

    if (error != ETIMEDOUT) {
        /* 
         * Always reset the cache timer and force a lookup except for ETIMEDOUT
         * where we want to return cached meta data if possible
         */
        np->attribute_cache_timer = 0;
        np->n_symlink_cache_timer = 0;
    }
    
	if (error == ENOTSUP) {
		/* This server doesn't support notifications */
		SMBWARNING("Server doesn't support notifications, polling\n");		
		return error;
		
	} else if ((error == ETIMEDOUT) || (error == ENOTCONN)) {
		SMBDEBUG_LOCK(np, "Processing notify for %s error = %d\n", np->n_name, error);
		watchItem->throttleBack = TRUE;
	} else if (error)  {
		SMBWARNING_LOCK(np, "We got an unexpected error: %d for %s\n", error, np->n_name);
		watchItem->throttleBack = TRUE;
	} else {
		struct timespec ts;

		nanouptime(&ts);
		if (timespeccmp(&ts, &watchItem->last_notify_time, >)) {
			watchItem->rcvd_notify_count = 0;
			ts.tv_sec += SMBFS_MAX_RCVD_NOTIFY_TIME;
			watchItem->last_notify_time = ts;
		} else {
			watchItem->rcvd_notify_count++;
			if (watchItem->rcvd_notify_count > SMBFS_MAX_RCVD_NOTIFY)
				watchItem->throttleBack = TRUE;
		}
	}
    
	/* Notify them that something changed */
	smbfs_notified_vnode(np, watchItem->throttleBack, events, context);

done:
	reset_notify_change(watchItem, FALSE);
	return 0;
}

/*
 * Process a svrmsg notify message from the server
 */
static int
rcvd_svrmsg_notify(struct smbmount	*smp, struct watch_item *watchItem)
{
	struct smb_rq *	rqp;
    uint32_t action, delay;
	int error = 0;
	
    /* svrmsg notify always uses SMB 2/3 */
    rqp = watchItem->rqp;
    
    if (rqp == NULL) {
        /* Not good, log an error and punt */
        SMBDEBUG("Received svrmsg, but no rqp\n");
        error = EINVAL;
        goto done;
    }
 
    error = smb2_smb_parse_svrmsg_notify(rqp, &action, &delay);
    
    if (error) {
        SMBDEBUG("parse svrmsg error: %d\n", error);
        goto done;
    }

    /* Here is where we make the call to the Kernel Event Agent and
     * let it know what's going on with the server.
     *
     * Note: SVRMSG_GOING_DOWN and SVRMSG_SHUTDOWN_CANCELLED are mutually exclusive.
     *       Only one can be set at any given time.
     */
    lck_mtx_lock(&smp->sm_svrmsg_lock);
    if (action == SVRMSG_SHUTDOWN_START) {
        /* Clear any pending SVRMSG_RCVD_SHUTDOWN_CANCEL status */
        smp->sm_svrmsg_pending &= SVRMSG_RCVD_SHUTDOWN_CANCEL;
        
        /* Set SVRMSG_RCVD_GOING_DOWN & delay */
        smp->sm_svrmsg_pending |= SVRMSG_RCVD_GOING_DOWN;
        smp->sm_svrmsg_shutdown_delay = delay;
        
    } else if (action == SVRMSG_SHUTDOWN_CANCELLED) {
        /* Clear any pending SVRMSG_RCVD_GOING_DOWN status */
        smp->sm_svrmsg_pending &= ~SVRMSG_RCVD_GOING_DOWN;
        
        /* Set SVRMSG_RCVD_SHUTDOWN_CANCEL */
        smp->sm_svrmsg_pending |= SVRMSG_RCVD_SHUTDOWN_CANCEL;
    }
    lck_mtx_unlock(&smp->sm_svrmsg_lock);
    vfs_event_signal(NULL, VQ_SERVEREVENT, 0);
    
done:
	reset_notify_change(watchItem, FALSE);
	return error;
}

/* 
 * Send a change notify message to the server
 */
static int 
send_notify_change(struct watch_item *watchItem, vfs_context_t context)
{
	struct smbnode *np = watchItem->np;
	struct smb_share *share;
	struct smb_ntrq *ntp;
	struct mbchain *mbp;
	int error;
	uint32_t CompletionFilters;
    uint16_t smb1_fid;
 
	
	share = smb_get_share_with_reference(np->n_mount);
	if (share->ss_flags & SMBS_RECONNECTING) {
		/* While we are in reconnect stop sending */
		error = EAGAIN;
		goto done;
	}

	/* Need to wait for it to be reopened */
	if (np->d_needReopen) {
		error = EBADF;
		goto done;
	}
	
	/* Someone close don't send any more notifies  */
	if (np->d_fid == 0) {
		error = EBADF;
		goto done;
	}
	
	if (watchItem->throttleBack) {
		uint32_t events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
		/* Reset throttle state info */
		watchItem->throttleBack = FALSE;
		watchItem->rcvd_notify_count = 0;	
		/* 
		 * Something could have happen while we were throttle so just say 
		 * something changed 
		 */
		smbfs_notified_vnode(np, watchItem->throttleBack, events, context);
		nanouptime(&watchItem->last_notify_time);
		watchItem->last_notify_time.tv_sec += SMBFS_MAX_RCVD_NOTIFY_TIME;
	}
	
	SMBDEBUG_LOCK(np, "Sending notify for %s with fid = 0x%llx\n", np->n_name, np->d_fid);

	/* Items we want to be notified about. */
	CompletionFilters = SMBFS_NOTIFY_CHANGE_FILTERS;

    /*
    * Let SMB 2/3 handle this
    */
    if (SSTOVC(share)->vc_flags & SMBV_SMB2) {
        /* Set max response size to 64K which should be plenty */
        watchItem->flags |= SMBV_SMB2;
        error = smb2fs_smb_change_notify(share, 64 * 1024, 
                                         CompletionFilters, 
                                         notify_callback_completion, watchItem,
                                         context);
        if (error) {
            SMBWARNING("smb2fs_smb_change_notify return %d\n", error);
            reset_notify_change(watchItem, TRUE);
        }
		goto done;
    }
    
    error = smb_nt_alloc(SSTOCP(share), NT_TRANSACT_NOTIFY_CHANGE, context, &ntp);
	if (error) {
		goto done;	/* Something bad happen, try agian later */
	}
	watchItem->ntp = ntp;
	mbp = &ntp->nt_tsetup;
	mb_init(mbp);
	
	mb_put_uint32le(mbp, CompletionFilters);	/* Completion Filter */
    smb1_fid = (uint16_t) np->d_fid;
	mb_put_uint16le(mbp, smb1_fid);
	/* 
	 * Decide that watch tree should be set per item instead of per mount. So
	 * if we have to poll then watch tree will be true for the parent node or 
	 * root node. This will allow us to handle the case where we have too many 
	 * notifications.
	 *
	 * NOTE: Still concerned about the traffic setting this can cause. Seems 
	 *       finder calls monitor begin on every directory they have open and 
	 *       viewable by the user. Also they never call monitor end, so these 
	 *       notifications hang around until the node goes inactive. So this 
	 *       means if a root is being monitored and some subdirector is being 
	 *       monitored, then we will get double response for everything in the 
	 *       subdirectory. This is exactly whay I have observed with the latest 
	 *		 finder.
	 */
	/* Watch for thing below this item */
	mb_put_uint16le(mbp, watchItem->watchTree);
	
	/* Amount of param data they can return, make sure it fits in one message */
	ntp->nt_maxpcount = SSTOVC(share)->vc_txmax - 
					(SMB_HDRLEN+SMB_COM_NT_TRANS_LEN+SMB_MAX_SETUPCOUNT_LEN+1);
	ntp->nt_maxdcount = 0;
	error = smb_nt_async_request(ntp, notify_callback_completion, watchItem);
	if (error) {
		SMBWARNING("smb_nt_async_request return %d\n", error);
		reset_notify_change(watchItem, TRUE);
	}
done:
	smb_share_rele(share, context);
	return error;
}

static int
send_svrmsg_notify(struct smbmount *smp,
                   struct watch_item *svrItem,
                   vfs_context_t context)
{
	struct smb_share *share;
	int error;
	uint32_t CompletionFilters;
    
	share = smb_get_share_with_reference(smp);
	if (share->ss_flags & SMBS_RECONNECTING) {
		/* While we are in reconnect stop sending */
		error = EAGAIN;
		goto done;
	}
    
	/* Items we want to be notified about. */
	CompletionFilters = SMBFS_SVRMSG_NOTIFY_FILTERS;
    
    /* Set max response size to 64K which should be plenty */
    svrItem->flags |= SMBV_SMB2;
    error = smb2fs_smb_change_notify(share, 64 * 1024,
                                    CompletionFilters,
                                    notify_callback_completion, svrItem,
                                    context);
    if (error) {
        SMBWARNING("smb2fs_smb_change_notify returns %d\n", error);
        reset_notify_change(svrItem, TRUE);
    }
    
done:
	smb_share_rele(share, context);
	return error;
}

static int
VolumeMaxNotification(struct smbmount *smp, vfs_context_t context)
{
	struct smb_share   *share;
	int32_t				vc_volume_cnt;
	int					maxWorkingCnt;

	share = smb_get_share_with_reference(smp);
	vc_volume_cnt = OSAddAtomic(0, &SSTOVC(share)->vc_volume_cnt);
    
	/* 
	 * Did this share just get replaced for Dfs failover, try again
	 */ 
	if (vc_volume_cnt == 0) {
		smb_share_rele(share, context);
		share = smb_get_share_with_reference(smp);
		vc_volume_cnt = OSAddAtomic(0, &SSTOVC(share)->vc_volume_cnt);
	}
    
	/* Just to be safe never let vc_volume_cnt be zero! */
	if (!vc_volume_cnt) {
		vc_volume_cnt = 1;
	}
    
    if (SSTOVC(share)->vc_flags & SMBV_SMB2) {
        /* SMB 2/3 relies on crediting */
        maxWorkingCnt = (SSTOVC(share)->vc_credits_max / 2) / vc_volume_cnt;
    }
    else {
        /* SMB 1 relies on maxmux */
        maxWorkingCnt = (SSTOVC(share)->vc_maxmux / 2) / vc_volume_cnt;
    }
    
	smb_share_rele(share, context);
    
	return maxWorkingCnt;
}

/*
 * process_svrmsg_items
 *
 * Process server message notifications.
 *
 */
static void
process_svrmsg_items(struct smbfs_notify_change *notify, vfs_context_t context)
{
	struct smbmount	*smp = notify->smp;
    struct watch_item *svrItem;
    int error;
    
    svrItem = notify->svrmsg_item;
    if (svrItem == NULL) {
        /* extremely unlikely, but just to be sure */
        return;
    }

    switch (svrItem->state) {
        case kReceivedNotify:
        {
            error = rcvd_svrmsg_notify(smp, svrItem);
            if (error == ENOTSUP) {
                /* Notify not supported, turn off svrmsg notify */
                    
                /* This will effectively disable server messages */
                lck_mtx_lock(&svrItem->watch_statelock);
                SMBERROR("svrmsg notify not supported\n");
                svrItem->state = kWaitingForRemoval;
                lck_mtx_unlock(&svrItem->watch_statelock);
                break;
            } else if (error) {
                lck_mtx_lock(&svrItem->watch_statelock);
                svrItem->rcvd_notify_count++;
                if (svrItem->rcvd_notify_count > SMBFS_MAX_RCVD_NOTIFY) {
                    /* too many errors, turn off svrmsg notify */
                    SMBERROR("disabling svrmsg notify, error: %d\n", error);
                    svrItem->state = kWaitingForRemoval;
                } else {
                    svrItem->state = kSendNotify;
                }
                lck_mtx_unlock(&svrItem->watch_statelock);
                break;
            }
            
            lck_mtx_lock(&svrItem->watch_statelock);
            SMBDEBUG("Receive success, sending next svrmsg notify\n");
            svrItem->state = kSendNotify;
            svrItem->rcvd_notify_count = 0;
            lck_mtx_unlock(&svrItem->watch_statelock);
            
            /* fall through to send another svrmsg notify */
        }
            
        case kSendNotify:
        {
            error = send_svrmsg_notify(smp, svrItem, context);
            if (error == EAGAIN) {
                /* Must be in reconnect, try to send later */
                break;
            }
            if (!error) {
                lck_mtx_lock(&svrItem->watch_statelock);
                svrItem->state = kWaitingOnNotify;
                lck_mtx_unlock(&svrItem->watch_statelock);
            }

            break;
        }
            
        case kCancelNotify:
            reset_notify_change(svrItem, TRUE);
            
            lck_mtx_lock(&svrItem->watch_statelock);
            svrItem->state = kWaitingForRemoval;
            lck_mtx_unlock(&svrItem->watch_statelock);
            wakeup(svrItem);
            break;

        default:
            SMBDEBUG("State %u ignored\n", svrItem->state);
            break;
    }
}

/*
 * process_notify_items
 *
 * Process all watch items on the notify change list. 
 *
 */
static void 
process_notify_items(struct smbfs_notify_change *notify, vfs_context_t context)
{
	struct smbmount	*smp = notify->smp;
	int maxWorkingCnt = VolumeMaxNotification(smp, context);
	struct watch_item *watchItem, *next;
	int	 updatePollingNodes = FALSE;
	int moveToPollCnt = 0, moveFromPollCnt = 0;
	int workingCnt;
	
	lck_mtx_lock(&notify->watch_list_lock);
	/* How many outstanding notification do we have */ 
	workingCnt = notify->watchCnt - notify->watchPollCnt;
	/* Calculate how many need to be move to the polling state */
	if (workingCnt > maxWorkingCnt) {
		moveToPollCnt = workingCnt - maxWorkingCnt;
		SMBDEBUG("moveToPollCnt = %d \n", moveToPollCnt);
	}
	else if (notify->watchPollCnt) {
		/* Calculate how many we can move out of the polling state */
		moveFromPollCnt = maxWorkingCnt - workingCnt;
		if (notify->watchPollCnt < moveFromPollCnt) {
			moveFromPollCnt = notify->watchPollCnt;
			SMBDEBUG("moveFromPollCnt = %d\n", moveFromPollCnt);		
		}
	}
    
    /* Process svrmsg notify messages */
    if (notify->pollOnly != TRUE && (notify->svrmsg_item != NULL)) {
        /* Server message notifications handled separately */
        process_svrmsg_items(notify, context);
    }
	
	STAILQ_FOREACH_SAFE(watchItem, &notify->watch_list, entries, next) {
		switch (watchItem->state) {
			case kCancelNotify:
                if (notify->pollOnly == TRUE) {
                    /* request already removed from the iod queue */
                    reset_notify_change(watchItem, FALSE);
                } else {
                    reset_notify_change(watchItem, TRUE);
                }
                
				lck_mtx_lock(&watchItem->watch_statelock);
				/* Wait for the user process to dequeue and free the item */
				watchItem->state = kWaitingForRemoval;
				lck_mtx_unlock(&watchItem->watch_statelock);
				wakeup(watchItem);
				break;
			case kReceivedNotify:
				/* 
				 * Root is always the first item in the list, so we can set the
				 * flag here and know that all the polling nodes will get updated.
				 */
				if (watchItem->isRoot) {
					updatePollingNodes = TRUE;
					if (moveToPollCnt || (notify->watchPollCnt > moveFromPollCnt)) {
						/* We are polling so turn on watch tree */
						SMBDEBUG("watchTree = TRUE\n");
						watchItem->watchTree = TRUE;
					} else {
						SMBDEBUG("watchTree = FALSE\n");
						watchItem->watchTree = FALSE;
					}
				}
				if (rcvd_notify_change(watchItem, context) == ENOTSUP) {
					notify->pollOnly = TRUE;
					watchItem->state = kUsePollingToNotify;
					break;
				} else {
					watchItem->state = kSendNotify;
					if (watchItem->throttleBack) {
						SMBDEBUG_LOCK(watchItem->np, "Throttling back %s\n", watchItem->np->n_name);
						notify->sleeptimespec.tv_sec = NOTIFY_THROTTLE_SLEEP_TIMO;
						break;	/* Pull back sending notification, until next time */					
					}
				}
				/* Otherwise fall through, so we can send a new request */
			case kSendNotify:
			{
				int sendError;
				sendError = send_notify_change(watchItem, context);
				if (sendError == EAGAIN) {
					/* Must be in reconnect, try to send agian later */
					break;
				} 
				if (!sendError) {
					watchItem->state = kWaitingOnNotify;
					break;
				}
				if (!watchItem->isRoot && moveToPollCnt) {
					watchItem->state = kUsePollingToNotify;
					moveToPollCnt--;
					notify->watchPollCnt++;
					SMBDEBUG_LOCK(watchItem->np, "Moving %s to poll state\n", watchItem->np->n_name);
				} else {
					/* If an error then keep trying */
					watchItem->state = kSendNotify;
				}
				break;
			}
			case kUsePollingToNotify:
				/* We can move some back to notify and turn off polling */
				if ((!notify->pollOnly) && 
                    moveFromPollCnt &&
                    (watchItem->np->d_fid != 0) && 
                    (!watchItem->np->d_needReopen)) {
					watchItem->state = kSendNotify;
					moveFromPollCnt--;
					notify->watchPollCnt--;
					notify->haveMoreWork = TRUE; /* Force us to resend these items */
					SMBDEBUG_LOCK(watchItem->np, "Moving %s from polling to send state\n", watchItem->np->n_name);
				} else if (updatePollingNodes) {
					uint32_t events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
					smbfs_notified_vnode(watchItem->np, FALSE, events, context);
                    SMBDEBUG_LOCK(watchItem->np, "Updating %s using polling\n", watchItem->np->n_name);
				}
				break;
			case kWaitingOnNotify:
				/* Nothing to do here but wait */
				break;
			case kWaitingForRemoval:
				/* Just waiting for it to get removed */
				break;
		}
	}	
	lck_mtx_unlock(&notify->watch_list_lock);
	/* 
	 * Keep track of how many are we over the limit So we can kick them off
	 * in smbfs_restart_change_notify. We need this to keep one volume from
	 * hogging all the kqueue events. So if its zero that means the 
	 * smbfs_restart_change_notify code is done so we can now add the new
	 * value if we have one.
	 */
	if (OSAddAtomic(0, &smp->tooManyNotifies) == 0)
		OSAddAtomic(moveToPollCnt, &smp->tooManyNotifies);
}

/*
 * notify_main
 *
 * Notify thread main routine.
 */
static void 
notify_main(void *arg)
{
	struct smbfs_notify_change	*notify = arg;
	vfs_context_t		context;
	
	context = vfs_context_create((vfs_context_t)0);

	notify->sleeptimespec.tv_nsec = 0;

	lck_mtx_lock(&notify->notify_statelock);
	notify->notify_state = kNotifyThreadRunning;
	lck_mtx_unlock(&notify->notify_statelock);

	while (notify->notify_state == kNotifyThreadRunning) {
		notify->sleeptimespec.tv_sec = NOTIFY_CHANGE_SLEEP_TIMO;
		notify->haveMoreWork = FALSE;
		process_notify_items(notify, context);
		if (!notify->haveMoreWork)
			msleep(&notify->notify_state, 0, PWAIT, "notify change idle", 
				   &notify->sleeptimespec);	
	}
	/* Shouldn't have anything in the queue at this point */
	DBG_ASSERT(STAILQ_EMPTY(&notify->watch_list))		
	
	lck_mtx_lock(&notify->notify_statelock);
	notify->notify_state = kNotifyThreadStop;
	lck_mtx_unlock(&notify->notify_statelock);
	vfs_context_rele(context);
	wakeup(notify);
}

/*
 * smbfs_notify_change_create_thread
 *
 * Create and start the thread used do handle notify change request
 */
void 
smbfs_notify_change_create_thread(struct smbmount *smp)
{
	struct smbfs_notify_change	*notify;
	kern_return_t	result;
	thread_t		thread;

	SMB_MALLOC(notify, struct smbfs_notify_change *, sizeof(*notify), M_TEMP, 
		   M_WAITOK | M_ZERO);
	smp->notify_thread = notify;
	
	notify->smp = smp;
	lck_mtx_init(&notify->notify_statelock, smbfs_mutex_group, smbfs_lock_attr);	
	lck_mtx_init(&notify->watch_list_lock, smbfs_mutex_group, smbfs_lock_attr);	
	STAILQ_INIT(&notify->watch_list);

	notify->notify_state = kNotifyThreadStarting;
	
	result = kernel_thread_start((thread_continue_t)notify_main, notify, &thread);
	if (result != KERN_SUCCESS) {
		SMBERROR("can't start notify change thread: result = %d\n", result);
		smp->notify_thread = NULL;
		SMB_FREE(notify, M_SMBIOD);
		return; 
	}
	thread_deallocate(thread);
	return;
}

/*
 * smbfs_notify_change_destroy_thread
 *
 * Stop the thread used to handle notify change request and remove any memory
 * used by the thread.
 *
 * NOTE: All watch items should have already been remove from the threads list. 
 */
void 
smbfs_notify_change_destroy_thread(struct smbmount *smp)
{
	struct smbfs_notify_change	*notify = smp->notify_thread;

	if (smp->notify_thread == NULL)
		return;
	smp->notify_thread = NULL;
	notify->notify_state = kNotifyThreadStopping;
	wakeup(&notify->notify_state);
	
	for (;;) {
		lck_mtx_lock(&notify->notify_statelock);
		if (notify->notify_state == kNotifyThreadStop) {
			lck_mtx_unlock(&notify->notify_statelock);
			if (STAILQ_EMPTY(&notify->watch_list)) {
				SMBDEBUG("Watch thread going away\n");				
			} else {
				SMBERROR("Watch thread going away with watch items, very bad?\n");								
			}
			break;
		}
		msleep(notify, &notify->notify_statelock, PWAIT | PDROP, "notify change exit", 0);
	}
	lck_mtx_destroy(&notify->notify_statelock, smbfs_mutex_group);
	lck_mtx_destroy(&notify->watch_list_lock, smbfs_mutex_group);
	SMB_FREE(notify, M_TEMP);
}

/*
 * enqueue_notify_change_request
 *
 * Allocate an item and place it on the list. 
 */
static void 
enqueue_notify_change_request(struct smbfs_notify_change *notify, 
							  struct smbnode *np)
{
	struct watch_item *watchItem;
	
	SMB_MALLOC(watchItem, struct watch_item *, sizeof(*watchItem), M_TEMP, M_WAITOK | M_ZERO);
	lck_mtx_init(&watchItem->watch_statelock, smbfs_mutex_group, smbfs_lock_attr);
	watchItem->isRoot = vnode_isvroot(np->n_vnode);		
	watchItem->np = np;
	if (notify->pollOnly) {
		watchItem->state = kUsePollingToNotify;
	} else {
		watchItem->state = kSendNotify;
	}
	watchItem->notify = notify;
	nanouptime(&watchItem->last_notify_time);
	watchItem->last_notify_time.tv_sec += SMBFS_MAX_RCVD_NOTIFY_TIME;
	lck_mtx_lock(&notify->watch_list_lock);
	notify->watchCnt++;

    SMBDEBUG_LOCK(np, "Enqueue %s count = %d poll count = %d\n", np->n_name,
                  notify->watchCnt, notify->watchPollCnt);

	/* Always make sure the root vnode is the first item in the list */
	if (watchItem->isRoot) {
		STAILQ_INSERT_HEAD(&notify->watch_list, watchItem, entries);
	} else {
		STAILQ_INSERT_TAIL(&notify->watch_list, watchItem, entries);
	}
	lck_mtx_unlock(&notify->watch_list_lock);
	notify_wakeup(notify);
}

/*
 * enqueue_notify_svrmsg_request
 *
 * Allocate an item for server messages, and place it
 * in the notify struct.
 */
static void
enqueue_notify_svrmsg_request(struct smbfs_notify_change *notify)
{
	struct watch_item *watchItem;
    
    if (notify->pollOnly) {
        SMBERROR("Server doesn't support notify, not enabling svrmsg notify\n");
        return;
    }
	
	SMB_MALLOC(watchItem, struct watch_item *, sizeof(*watchItem), M_TEMP, M_WAITOK | M_ZERO);
	lck_mtx_init(&watchItem->watch_statelock, smbfs_mutex_group, smbfs_lock_attr);

    watchItem->isServerMsg = TRUE;
    watchItem->state = kSendNotify;

	watchItem->notify = notify;
	nanouptime(&watchItem->last_notify_time);
	lck_mtx_lock(&notify->watch_list_lock);

    notify->svrmsg_item = watchItem;
	lck_mtx_unlock(&notify->watch_list_lock);
	notify_wakeup(notify);
}

/*
 * dequeue_notify_change_request
 *
 * Search the list, if we find a match set the state to cancel. Now wait for the
 * watch thread to say its ok to remove the item.
 */
static void 
dequeue_notify_change_request(struct smbfs_notify_change *notify, 
							  struct smbnode *np)
{
	struct watch_item *watchItem, *next;
		
	lck_mtx_lock(&notify->watch_list_lock);
	STAILQ_FOREACH_SAFE(watchItem, &notify->watch_list, entries, next) {
		if (watchItem->np == np) {
			notify->watchCnt--;
			lck_mtx_lock(&watchItem->watch_statelock);
			if (watchItem->state == kUsePollingToNotify)
				notify->watchPollCnt--;				

            SMBDEBUG_LOCK(np, "Dequeue %s count = %d poll count = %d\n", np->n_name,
                          notify->watchCnt, notify->watchPollCnt);

			watchItem->state = kCancelNotify;
			lck_mtx_unlock(&watchItem->watch_statelock);
			notify_wakeup(notify);
			msleep(watchItem, &notify->watch_list_lock, PWAIT, 
				   "notify watchItem cancel", NULL);
			STAILQ_REMOVE(&notify->watch_list, watchItem, watch_item, entries);
			SMB_FREE(watchItem, M_TEMP);
			watchItem = NULL;
			break;
		}
	}
	lck_mtx_unlock(&notify->watch_list_lock);
}

/*
 * dequeue_notify_svrmsg_request
 *
 * Set the svrmsg_item state to cancel, then wait for the
 * watch thread to say its ok to remove the item.
 */
static void
dequeue_notify_svrmsg_request(struct smbfs_notify_change *notify)
{
	struct watch_item *watchItem = notify->svrmsg_item;
    
    if (watchItem == NULL) {
        return;
    }
    
    lck_mtx_lock(&notify->watch_list_lock);
    
    lck_mtx_lock(&watchItem->watch_statelock);
    watchItem->state = kCancelNotify;
    lck_mtx_unlock(&watchItem->watch_statelock);
    
    notify_wakeup(notify);
    msleep(watchItem, &notify->watch_list_lock, PWAIT,
           "svrmsg watchItem cancel", NULL);
    
    if (watchItem->state != kWaitingForRemoval) {
        SMBERROR("svrmsgItem->state: %d, expected kWaitingForRemoval\n", watchItem->state);
    }

    lck_mtx_lock(&watchItem->watch_statelock);
    notify->svrmsg_item = NULL;
    lck_mtx_unlock(&watchItem->watch_statelock);
    
    SMB_FREE(watchItem, M_TEMP);

	lck_mtx_unlock(&notify->watch_list_lock);
}

/*
 * smbfs_start_change_notify
 *
 * Start the change notify process. Called from the smbfs_vnop_monitor routine.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_start_change_notify(struct smb_share *share, struct smbnode *np, 
						  vfs_context_t context, int *releaseLock)
{
	struct smbmount *smp = np->n_mount;
	int error;
	
	if (smp->notify_thread == NULL) {
		/* This server doesn't support notify change so turn on polling */
		np->n_flag |= N_POLLNOTIFY;
		SMBDEBUG_LOCK(np, "Monitoring %s with polling\n", np->n_name);
	} else {
		if (np->d_kqrefcnt) {
			np->d_kqrefcnt++;	/* Already processing this node, we are done */
			return 0;		
		}
		np->d_kqrefcnt++;
		/* Setting SMB2_SYNCHRONIZE because XP does. */
		error = smbfs_tmpopen(share, np, SMB2_FILE_READ_DATA | SMB2_SYNCHRONIZE,
                              &np->d_fid, context);
		if (error)	{
			/* Open failed so turn on polling */
			np->n_flag |= N_POLLNOTIFY;
			SMBDEBUG_LOCK(np, "Monitoring %s failed to open. %d\n", np->n_name, error);
		} else {
			SMBDEBUG_LOCK(np, "Monitoring %s\n", np->n_name);
            
			/*
			 * We no longer need the node lock. So unlock the node so we have no
			 * lock contention with the notify list lock.
			 *
			 * Make sure we tell the calling routine that we have released the
			 * node lock.
			 */
			*releaseLock = FALSE;
			smbnode_unlock(np);
			enqueue_notify_change_request(smp->notify_thread, np);
		}
	}
	return 0;
}

/*
 * smbfs_start_svrmsg_notify
 *
 * Start the change notify process. Called from the smbfs mount routine.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smbfs_start_svrmsg_notify(struct smbmount *smp)
{
	int error = 0;
	
	if (smp->notify_thread == NULL) {
		/* This server doesn't support notify change, so forget srvmsg
         * notifications
         */
		SMBDEBUG("Server doesn't support notify\n");
        error = ENOTSUP;
	} else {
			SMBDEBUG("Monitoring server messages\n");
			enqueue_notify_svrmsg_request(smp->notify_thread);
	}
	return error;
}

/*
 * smbfs_stop_change_notify
 *
 * Called from  smbfs_vnop_monitor or smb_vnop_inactive routine. If this is the 
 * last close then close the directory and set the fid to zero. This will stop
 * the watch event from doing any further work. Now dequeue the watch item.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_stop_change_notify(struct smb_share *share, struct smbnode *np, 
						 int forceClose, vfs_context_t context, int *releaseLock)
{	
	struct smbmount *smp = np->n_mount;
	SMBFID	fid;
	
	if (forceClose)
		np->d_kqrefcnt = 0;
	else 
		np->d_kqrefcnt--;
	
	/* Still have users monitoring just get out */
	if (np->d_kqrefcnt > 0)
		return 0;
	
	DBG_ASSERT(np->d_kqrefcnt == 0)
	/* If polling was turned on, turn it off */
	np->n_flag &= ~N_POLLNOTIFY;
	fid = np->d_fid;
	/* Stop all notify network traffic */
	np->d_fid = 0;
	/* If marked for reopen, turn it off */
	np->d_needReopen = FALSE; 
	np->d_kqrefcnt = 0;
	/* If we have it open then close it */
	if (fid != 0) {
		(void)smbfs_tmpclose(share, np, fid, context);
    }
    
	SMBDEBUG_LOCK(np, "We are no longer monitoring  %s\n", np->n_name);

	if (smp->notify_thread) {
		/* 
		 * We no longer need the node lock. So unlock the node so we have no
		 * lock contention with the notify list lock.
		 *
		 * Make sure we tell the calling routine that we have released the
		 * node lock.
		 */
		*releaseLock = FALSE;
		smbnode_unlock(np);
		dequeue_notify_change_request(smp->notify_thread, np);
	}
	return 0;
}

int
smbfs_stop_svrmsg_notify(struct smbmount *smp)
{		
	SMBDEBUG("We are no longer monitoring svrmsg notify replies\n");
    
	if (smp->notify_thread) {
		dequeue_notify_svrmsg_request(smp->notify_thread);
	}
	return 0;
}

/*
 * smbfs_restart_change_notify
 *
 * Reopen the directory and wake up the notify queue.
 *
 * The calling routine must hold a reference on the share
 *
 */
void 
smbfs_restart_change_notify(struct smb_share *share, struct smbnode *np, 
							vfs_context_t context)
{
	struct smbmount *smp = np->n_mount;
	int error;
	
	/* This server doesn't support notify change so we are done just return */
	if (smp->notify_thread == NULL) {
		np->d_needReopen = FALSE; 
		return;
	}
	if (!np->d_needReopen) {
		SMBFID	fid = np->d_fid;
		
		if ((vnode_isvroot(np->n_vnode)) || 
			(OSAddAtomic(0, &smp->tooManyNotifies) == 0)) {
			/* Nothing do do here just get out */
			return;
		}
			
		/* We sent something see how long we have been waiting */				
		SMBDEBUG_LOCK(np, "Need to close '%s' so we can force it to use polling\n",
                      np->n_name);

		np->d_needReopen = TRUE;
		np->d_fid = 0;
		/*
		 * Closing it here will cause the server to send a cancel error, which
		 * will cause the notification thread to place this item in the poll 
		 * state.
		 */
		(void)smbfs_tmpclose(share, np, fid, context);
		OSAddAtomic(-1, &smp->tooManyNotifies);
		return;	/* Nothing left to do here, just get out */
	}

    SMBDEBUG_LOCK(np, "%s is being reopened for monitoring\n", np->n_name);

    /*
	 * We set the capabilities VOL_CAP_INT_REMOTE_EVENT for all supported
	 * servers. So if they call us without checking the
	 * capabilities then they get what they get.
	 *
	 * Setting SMB2_SYNCHRONIZE because XP does.
	 *
	 * Now reopen the directory.
	 */
	error = smbfs_tmpopen(share, np, SMB2_FILE_READ_DATA | SMB2_SYNCHRONIZE,  
						  &np->d_fid, context);
	if (error) {
		SMBWARNING_LOCK(np, "Attempting to reopen %s failed %d\n", np->n_name, error);
		return;
	}
	
	np->d_needReopen = FALSE; 
	notify_wakeup(smp->notify_thread);
}
