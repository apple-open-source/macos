/*
 * Copyright (c) 2006 - 2008 Apple Inc. All rights reserved.
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
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
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

/*
 * notify_wakeup
 *
 * Wake up the thread and tell it there is work to be done.
 *
 */
static void notify_wakeup(struct smbfs_notify_change * notify)
{
	notify->haveMoreWork = TRUE;		/* we have work to do */
	wakeup(&(notify)->notify_state);
}

/*
 * notify_callback_completion
 */
static void notify_callback_completion(void *call_back_args)
{	
	struct watch_item *watchItem = (struct watch_item *)call_back_args;
	
	lck_mtx_lock(&watchItem->watch_statelock);
	if ((watchItem->state != kCancelNotify) && (watchItem->state != kWaitingForRemoval))
		watchItem->state = kReceivedNotify;
	lck_mtx_unlock(&watchItem->watch_statelock);
	notify_wakeup(watchItem->notify);	
}

/*
 * reset_notify_change
 *
 * Remove  the request from the network queue. Now cleanup and remove any
 * allocated memory.
 */
static void reset_notify_change(struct watch_item *watchItem, int RemoveRQ)
{
	struct smb_ntrq *ntp = watchItem->ntp;
	struct smb_rq *	rqp = (watchItem->ntp) ? watchItem->ntp->nt_rq : NULL;
	
	watchItem->ntp = NULL;
	if (rqp) {
		if (RemoveRQ)	/* Needs to be removed from the queue */
			smb_iod_removerq(rqp);
		smb_rq_done(rqp);
	}
	if (ntp)
		smb_nt_done(ntp);
}

/*
 * smbfs_notified_vnode
 *
 * See if we can update the node and notify the monitor.
 */
static void smbfs_notified_vnode(struct smbnode *np, int throttleBack, 
								 u_int32_t events, vfs_context_t context)
{
	struct vnode_attr vattr;
	vnode_t		vp;
	uint32_t	vid;
	
	if ((np->d_fid == 0) || (smbnode_lock(np, SMBFS_SHARED_LOCK) != 0))
		return; /* Nothing to do here */
	
	np->attribute_cache_timer = 0;
	/* 
	 * The fid changed while we were blocked just unlock and get out. If we are
	 * throttling back then skip this notify.  
	 */  
	if ((np->d_fid == 0) || throttleBack)
		goto done;
	
	np->n_lastvop = smbfs_notified_vnode;
	vp = np->n_vnode;
	vid = vnode_vid(vp);

	if (vnode_getwithvid(vp, vid))
		goto done;

	vfs_get_notify_attributes(&vattr);
	smbfs_attr_cachelookup(vp, &vattr, context, TRUE);
	vnode_notify(vp, events, &vattr);
	vnode_put(vp);
	events = 0;
	
done:
	if (events == 0)	/* We already process the event */
		np->d_needsUpdate = FALSE;
	else		/* Still need to process the event */
		np->d_needsUpdate = TRUE;
	smbnode_unlock(np);
}

/*
 * process_notify_change
 *
 */
static u_int32_t process_notify_change(struct smb_ntrq *ntp)
{
	u_int32_t events = 0;
	struct mdchain *mdp;
	u_int32_t nextoffset = 0, action;
	int error = 0;
	size_t rparam_len = 0;
	size_t rdata_len = 0;
	
	mdp = &ntp->nt_rdata;
	if (mdp->md_top) {
		rdata_len = m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
		SMBDEBUG("rdata_len = %d \n", (int)rdata_len);
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
			if (nextoffset >= sizeof(u_int32_t))
				nextoffset -= (u_int32_t)sizeof(u_int32_t);
			
			error = md_get_uint32le(mdp, &action);				
			if (error)
				break;
			
			/* since we already moved pass action don't count it */
			if (nextoffset >= sizeof(u_int32_t))
				nextoffset -= (u_int32_t)sizeof(u_int32_t);
				
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
					/* %%% Should we try to clear all named stream cache and how would we do that? */
					events |= VNODE_EVENT_ATTRIB;
					break;
				default:
					error = ENOTSUP;
					break;
			}
		} while (nextoffset);
	
	if (error || (events == 0))
		events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
	if (error)
		SMBWARNING("error = %d\n", error);
	return events;
}

/* 
 * Proces a change notify message from the server
 */
static void rcvd_notify_change(struct watch_item *watchItem, vfs_context_t context)
{
	struct smbnode *np = watchItem->np;	
	struct smb_ntrq *ntp = watchItem->ntp;
	struct smb_rq *	rqp = (watchItem->ntp) ? watchItem->ntp->nt_rq : NULL;
	int error = 0;
	u_int32_t events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
	
	if (rqp) {
		/* 
		 * NOTE: smb_nt_reply calls smb_rq_reply which will remove the rqp from 
		 * the main threads queue. So when we are done here call reset_notify_change 
		 * but tell it not to remove the request 
		 * from the queue.
		 */
		error = smb_nt_reply(ntp);
		if (!error)
			events = process_notify_change(ntp);
		if (error == ECANCELED) {
			/*
			 * Either we close the file descriptor or we canceled the 
			 * operation. Nothing else to do here just get out.
			 */
			SMBDEBUG("Notification for %s was canceled.\n", np->n_name);
			goto done;
		}
	}
	
	/* Always reset the cache timer and force a lookup */
	np->attribute_cache_timer = 0;

	if ((error == ETIMEDOUT) || (error == ENOTCONN) || (error == ENOTSUP)) {
		SMBDEBUG("Processing notify for %s error = %d\n", np->n_name, error);	
		watchItem->throttleBack = TRUE;
	} else if (error)  {
		SMBWARNING("We got an unexpected error: %d for %s\n", error, np->n_name);		
		watchItem->throttleBack = TRUE;
	} else {
		struct timespec ts;

		nanouptime(&ts);
		if (timespeccmp(&ts, &watchItem->last_notify_time, >)) {
			watchItem->rcvd_notify_count = 0;
			ts.tv_sec += SMBFS_MAX_RCVD_NOTIFY_TIME;
			watchItem->last_notify_time = ts;
		}
		else {
			watchItem->rcvd_notify_count++;
			if (watchItem->rcvd_notify_count > SMBFS_MAX_RCVD_NOTIFY)
				watchItem->throttleBack = TRUE;
		}
	}
	/* Notify them that something changed */
	smbfs_notified_vnode(np, watchItem->throttleBack, events, context);

done:
	reset_notify_change(watchItem, FALSE);
}

/* 
 * Send a change notify message to the server
 */
static int send_notify_change(struct smbfs_notify_change *notify, struct watch_item *watchItem, vfs_context_t context)
{
	struct smbnode *np = watchItem->np;
	struct smb_share *ssp = notify->smp->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_ntrq *ntp;
	struct mbchain *mbp;
	int error;
	u_int32_t CompletionFilters;
 	
	/* Need to wait for it to be reopened */
	if (np->d_needReopen)
		return EBADF;
	
	/* Someone close don't send any more notifies  */
	if (np->d_fid == 0)
		return EBADF;
	
	if (watchItem->throttleBack) {
		u_int32_t events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
		/* Reset throttle state info */
		watchItem->throttleBack = FALSE;
		watchItem->rcvd_notify_count = 0;	
		/* Something could have happen while we were throttle so just say something changed */
		smbfs_notified_vnode(np, watchItem->throttleBack, events, context);
		nanouptime(&watchItem->last_notify_time);
		watchItem->last_notify_time.tv_sec += SMBFS_MAX_RCVD_NOTIFY_TIME;
	}
	
	SMBDEBUG("Sending notify for %s with fid = %d\n", np->n_name, np->d_fid);

	/* Items we want to be notified about. */
	CompletionFilters = SMBFS_NOTIFY_CHANGE_FILTERS;
		
	error = smb_nt_alloc(SSTOCP(ssp), NT_TRANSACT_NOTIFY_CHANGE, context, &ntp);
	if (error)
		return error;	/* Something bad happen, try agian later */
	
	watchItem->ntp = ntp;
	mbp = &ntp->nt_tsetup;
	mb_init(mbp);
	
	mb_put_uint32le(mbp, CompletionFilters);	/* Completion Filter */
	mb_put_uint16le(mbp, np->d_fid);
	/* 
	 * Decide that watch tree should be set per item instead of per mount. So
	 * if we have to poll then watch tree will be true for the parent node or 
	 * root node. This will allow us to handle the case where we have too many 
	 * notifications.
	 *
	 * NOTE: Still concerned about the traffic setting this can cause. Seems finder calls
	 *       monitor begin on every directory they have open and viewable by the user.
	 *       Also they never call monitor end, so these notifications hang around until the
	 *       node goes inactive. So this means if a root is being monitored and
	 *       some subdirector is being monitored, then we will get double response for
	 *       everything in the subdirectory. This is exactly whay I have observed with
	 *       the latest finder.
	 *
	 */
	mb_put_uint16le(mbp, watchItem->watchTree);	/* WATCH_TREE - watch any for thing below this item */
	
	/* Amount of param data they can return, make sure it fits in one message */
	ntp->nt_maxpcount = vcp->vc_txmax - (SMB_HDRLEN+SMB_COM_NT_TRANS_LEN+SMB_MAX_SETUPCOUNT_LEN+1);
	ntp->nt_maxdcount = 0;
	error = smb_nt_async_request(ntp, notify_callback_completion, watchItem);
	if (error) {
		SMBWARNING("smb_nt_async_request return %d\n", error);
		reset_notify_change(watchItem, TRUE);
	}
	return error;
}

/*
 * process_notify_items
 *
 * Process all watch items on the notify change list. 
 *
 */
static void process_notify_items(struct smbfs_notify_change *notify, vfs_context_t context)
{
	struct smbmount	*smp = notify->smp;
	struct smb_vc   *vcp = SSTOVC(smp->sm_share);
	int32_t vc_volume_cnt = OSAddAtomic(0, &vcp->vc_volume_cnt);
	int maxWorkingCnt = (vcp->vc_maxmux / 2) / vc_volume_cnt;
	struct watch_item *watchItem, *next;
	int	 updatePollingNodes = FALSE;
	int moveToPollCnt = 0, moveFromPollCnt = 0;
	int workingCnt;
	
	lck_mtx_lock(&notify->watch_list_lock);
	/* How many outstanding notification do we have */ 
	workingCnt = notify->watchCnt - notify->watchPollCnt;
	/* Calculate how many need to be move to the polling state */
	if (workingCnt >= maxWorkingCnt) {
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
	
	STAILQ_FOREACH_SAFE(watchItem, &notify->watch_list, entries, next) {
		switch (watchItem->state) {
			case kCancelNotify:
				reset_notify_change(watchItem, TRUE);
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
				if (watchItem->isRoot)
					updatePollingNodes = TRUE;
				rcvd_notify_change(watchItem, context);
				watchItem->state = kSendNotify;
				if (watchItem->throttleBack) {
					SMBDEBUG("Throttling back %s\n", watchItem->np->n_name);
					notify->sleeptimespec.tv_sec = NOTIFY_THROTTLE_SLEEP_TIMO;
					break;	/* Pull back sending notification, until next time */					
				}
				/* Otherwise fall through, so we can send a new request */
			case kSendNotify:
				if (smp->sm_share->ss_flags & SMBS_RECONNECTING)
					break;	/* While we are in reconnect stop sending */
				
				if (send_notify_change(notify, watchItem, context)) {
					if (!watchItem->isRoot && moveToPollCnt) {
						watchItem->state = kUsePollingToNotify;
						moveToPollCnt--;
						notify->watchPollCnt++;
						SMBDEBUG("Moving %s to poll state\n", watchItem->np->n_name);
					} else
						watchItem->state = kSendNotify;	/* If an error then keep trying */
				}
				else
					watchItem->state = kWaitingOnNotify;
				break;
			case kUsePollingToNotify:
				/* We can move some back to notify and off polling */
				if (moveFromPollCnt && (watchItem->np->d_fid) && (!watchItem->np->d_needReopen)) {
					watchItem->state = kSendNotify;
					moveFromPollCnt--;
					notify->watchPollCnt--;
					notify->haveMoreWork = TRUE; /* Force us to resend these items */
					SMBDEBUG("Moving %s from polling to send state\n", watchItem->np->n_name);
				} else if (updatePollingNodes) {
					u_int32_t events = VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
					smbfs_notified_vnode(watchItem->np, FALSE, events, context);
					SMBDEBUG("Updating %s using polling\n", watchItem->np->n_name);
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
static void notify_main(void *arg)
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
			msleep(&notify->notify_state, 0, PWAIT, "notify change idle", &notify->sleeptimespec);	
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
void smbfs_notify_change_create_thread(struct smbmount *smp)
{
	struct smb_vc   *vcp = SSTOVC(smp->sm_share);
	struct smbfs_notify_change	*notify;
	kern_return_t	result;
	thread_t		thread;

	if (!(vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)) {
		SMBWARNING("Server doesn't support Notify Change\n");
		return;
	}

	MALLOC(notify, struct smbfs_notify_change	*, sizeof(*notify), M_TEMP, M_WAITOK | M_ZERO);
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
		free(notify, M_SMBIOD);
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
void smbfs_notify_change_destroy_thread(struct smbmount *smp)
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
	free(notify, M_TEMP);
}

/*
 * enqueue_notify_change_request
 *
 * Allocate an item and place it on the list. 
 */
static void enqueue_notify_change_request(struct smbfs_notify_change *notify, struct smbnode *np)
{
	struct watch_item *watchItem;
	
	MALLOC(watchItem, struct watch_item *, sizeof(*watchItem), M_TEMP, M_WAITOK | M_ZERO);
	lck_mtx_init(&watchItem->watch_statelock, smbfs_mutex_group, smbfs_lock_attr);
	watchItem->isRoot = vnode_isvroot(np->n_vnode);		
	watchItem->np = np;
	watchItem->state = kSendNotify;
	watchItem->notify = notify;
	nanouptime(&watchItem->last_notify_time);
	watchItem->last_notify_time.tv_sec += SMBFS_MAX_RCVD_NOTIFY_TIME;
	lck_mtx_lock(&notify->watch_list_lock);
	notify->watchCnt++;
	SMBDEBUG("Enqueue %s count = %d poll count = %d\n", np->n_name, notify->watchCnt, notify->watchPollCnt);
	/* Always make sure the root vnode is the first item in the list */
	if (watchItem->isRoot) {
		STAILQ_INSERT_HEAD(&notify->watch_list, watchItem, entries);
		watchItem->watchTree = TRUE;	/* Always turn on watch tree, incase we have to poll */
	}
	else
		STAILQ_INSERT_TAIL(&notify->watch_list, watchItem, entries);
	lck_mtx_unlock(&notify->watch_list_lock);
	notify_wakeup(notify);
}

/*
 * dequeue_notify_change_request
 *
 * Search the list, if we find a match set the state to cancel. Now wait for the
 * watch thread to say its ok to remove the item.
 */
static void dequeue_notify_change_request(struct smbfs_notify_change *notify, struct smbnode *np)
{
	struct watch_item *watchItem, *next;
		
	lck_mtx_lock(&notify->watch_list_lock);
	STAILQ_FOREACH_SAFE(watchItem, &notify->watch_list, entries, next) {
		if (watchItem->np == np) {
			notify->watchCnt--;
			lck_mtx_lock(&watchItem->watch_statelock);
			if (watchItem->state == kUsePollingToNotify)
				notify->watchPollCnt--;				
			SMBDEBUG("Dequeue %s count = %d poll count = %d\n", np->n_name, notify->watchCnt, notify->watchPollCnt);
			watchItem->state = kCancelNotify;
			lck_mtx_unlock(&watchItem->watch_statelock);
			notify_wakeup(notify);
			msleep(watchItem, &notify->watch_list_lock, PWAIT, "notify watchItem cancel", NULL);
			STAILQ_REMOVE(&notify->watch_list, watchItem, watch_item, entries);
			FREE(watchItem, M_TEMP);
			watchItem = NULL;
			break;
		}
	}
	lck_mtx_unlock(&notify->watch_list_lock);
}

/*
 * smbfs_start_change_notify
 *
 * Start the change notify process. Called from the smbfs_vnop_monitor
 * routine.
 */
int smbfs_start_change_notify(struct smbnode *np, vfs_context_t context, int *releaseLock)
{
	struct smbmount *smp = np->n_mount;
	int				error;
	
	/* 
	 * We only set the capabilities VOL_CAP_INT_REMOTE_EVENT if the
	 * server supports SMB_CAP_NT_SMBS. So if they calls us without checking the
	 * capabilities then they get what they get.
	 *
	 * Setting STD_RIGHT_SYNCHRONIZE_ACCESS because XP does.
	 */
	if (np->d_kqrefcnt) {
		np->d_kqrefcnt++;	/* Already processing this node, we are done */
		return 0;		
	}
	
	np->d_kqrefcnt++;
	
	if (smp->notify_thread == NULL) {
		/* This server doesn't support notify change so turn on polling */
		np->n_flag |= N_POLLNOTIFY;
		SMBDEBUG("Monitoring %s with polling\n", np->n_name);
	} else {
		error = smbfs_smb_tmpopen(np, SA_RIGHT_FILE_READ_DATA | STD_RIGHT_SYNCHRONIZE_ACCESS,  context, &np->d_fid);
		if (error)	{
			/* Open failed so turn on polling */
			np->n_flag |= N_POLLNOTIFY;
			SMBDEBUG("Monitoring %s failed to open. %d\n", np->n_name, error);
		} else {
			SMBDEBUG("Monitoring %s\n", np->n_name);
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
 * smbfs_stop_change_notify
 *
 * Called from  smbfs_vnop_monitor or smb_vnop_inactive routine. If this is the 
 * last close then close the directory and set the fid to zero. This will stop the
 * watch event from doing any further work. Now dequeue the watch item.
 */
int smbfs_stop_change_notify(struct smbnode *np, int forceClose, vfs_context_t context, int *releaseLock)
{	
	struct smbmount *smp = np->n_mount;
	u_int16_t		fid;
	
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
	if (fid)
		(void)smbfs_smb_tmpclose(np, fid, context);		
	SMBDEBUG("We are no longer monitoring  %s\n", np->n_name);
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
	return 0;
}

/*
 * smbfs_restart_change_notify
 *
 * Reopen the directory and wake up the notify queue.
 */
void smbfs_restart_change_notify(struct smbnode *np, vfs_context_t context)
{
	struct smbmount *smp = np->n_mount;
	int error;
	
	/* This server doesn't support notify change so we are done just return */
	if (smp->notify_thread == NULL) {
		np->d_needReopen = FALSE; 
		return;
	}
	if (!np->d_needReopen) {
		u_int16_t		fid = np->d_fid;
		
		if ((vnode_isvroot(np->n_vnode)) || (OSAddAtomic(0, &smp->tooManyNotifies) == 0)) {
			/* Nothing do do here just get out */
			return;
		}
			
		/* We sent something see how long we have been waiting */				
		SMBDEBUG("Need to close '%s' so we can force it to use polling\n", np->n_name);
		np->d_needReopen = TRUE; 
		np->d_fid = 0;
		/*
		 * Closing it here will cause the server to send a cancel error, which
		 * will cause the notification thread to place this item in the poll state.
		 */
		(void)smbfs_smb_tmpclose(np, fid, context);
		OSAddAtomic(-1, &smp->tooManyNotifies);
		return;	/* Nothing left to do here, just get out */
	}
	SMBDEBUG("%s is being reopened for monitoring\n", np->n_name);
	/* 
	 * We only set the capabilities VOL_CAP_INT_REMOTE_EVENT if the
	 * server supports SMB_CAP_NT_SMBS. So if they calls us without checking the
	 * capabilities then they get what they get.
	 *
	 * Setting STD_RIGHT_SYNCHRONIZE_ACCESS because XP does.
	 *
	 * Now reopen the directory.
	 */
	error = smbfs_smb_tmpopen(np, SA_RIGHT_FILE_READ_DATA | STD_RIGHT_SYNCHRONIZE_ACCESS,  context, &np->d_fid);
	if (error) {
		SMBWARNING("Attempting to reopen %s failed %d\n", np->n_name, error);
		return;
	}
	
	np->d_needReopen = FALSE; 
	notify_wakeup(smp->notify_thread);
}
