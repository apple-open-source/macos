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

#ifndef _SMBFS_NOTIFY_CHANGE_H_
#define _SMBFS_NOTIFY_CHANGE_H_

#define kNotifyThreadStarting	1
#define kNotifyThreadRunning	2
#define kNotifyThreadStopping	3
#define kNotifyThreadStop		4

enum  {
	kSendNotify = 1,
	kReceivedNotify = 2,
	kUsePollingToNotify = 3,
	kWaitingOnNotify = 4,
	kWaitingForRemoval = 5,
	kCancelNotify = 6
	
};

struct watch_item {
	lck_mtx_t		watch_statelock;
	uint32_t		state;
	void			*notify;
	struct smbnode	*np;
	struct smb_ntrq *ntp;
	struct smb_rq   *rqp;           /* SMB 2.x */
	uint64_t		flags;           /* Indicates if using SMB 2.x or not */
	uint32_t		throttleBack;
	uint16_t		watchTree;
	int				isRoot;
    int             isServerMsg;
	struct timespec	last_notify_time;
	uint32_t		rcvd_notify_count;
	STAILQ_ENTRY(watch_item) entries;
};

struct smbfs_notify_change {
	struct smbmount		*smp;
	struct watch_item   *svrmsg_item;   /* SMB2.x, for server messages */
	uint32_t			haveMoreWork;
	struct timespec		sleeptimespec;
	uint32_t			notify_state;
	int					pollOnly;		/* Server doesn't support notifications */
	int					watchCnt;		/* Count of all items on the list */
	int					watchPollCnt;	/* Count of all polling items on the list */
	lck_mtx_t			notify_statelock;
	lck_mtx_t			watch_list_lock;
	STAILQ_HEAD(, watch_item) watch_list;
};

#endif // _SMBFS_NOTIFY_CHANGE_H_
