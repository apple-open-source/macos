/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Scooter Morris at Genentech Inc.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)msdosfs_lockf.c
 *	derived from @(#)ufs_lockf.c	8.4 (Berkeley) 10/26/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include "direntry.h"
#include "denode.h"
#include "msdosfs_lockf.h"

/*
 * This variable controls the maximum number of processes that will
 * be checked in doing deadlock detection.
 */
int msdosfsmaxlockdepth = MAXDEPTH;

#ifdef LOCKF_DEBUG
#include <sys/sysctl.h>
int	lockf_debug = 0;
struct ctldebug debug4 = { "lockf_debug", &lockf_debug };
void msdosfs_lprint(char *tag, struct msdosfs_lockf *lock);
void msdosfs_lprintlist(char *tag, struct msdosfs_lockf *lock);
#endif

#define NOLOCKF (struct msdosfs_lockf *)0
#define SELF	0x1
#define OTHERS	0x2

/*
 * Set a byte-range lock.
 */
int
msdosfs_setlock(lock)
	register struct msdosfs_lockf *lock;
{
	register struct msdosfs_lockf *block;
	struct denode *dep = lock->lf_denode;
	struct msdosfs_lockf **prev, *overlap, *ltmp;
	static char lockstr[] = "msdosfs_lockf";
	int ovcase, priority, needtolink, error;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		msdosfs_lprint("msdosfs_setlock", lock);
#endif /* LOCKF_DEBUG */

	/*
	 * Set the priority
	 */
	priority = PLOCK;
	if (lock->lf_type == F_WRLCK)
		priority += 4;
	priority |= PCATCH;
	/*
	 * Scan lock list for this file looking for locks that would block us.
	 */
	while (block = msdosfs_getblock(lock)) {
		/*
		 * Free the structure and return if nonblocking.
		 */
		if ((lock->lf_flags & F_WAIT) == 0) {
			FREE(lock, M_LOCKF);
			return (EAGAIN);
		}
		/*
		 * For flock type locks, we must first remove
		 * any shared locks that we hold before we sleep
		 * waiting for an exclusive lock.
		 */
		if ((lock->lf_flags & F_FLOCK) &&
		    lock->lf_type == F_WRLCK) {
			lock->lf_type = F_UNLCK;
			(void) msdosfs_clearlock(lock);
			lock->lf_type = F_WRLCK;
		}
		/*
		 * Add our lock to the blocked list and sleep until we're free.
		 * Remember who blocked us (for deadlock detection).
		 */
		lock->lf_next = block;
		TAILQ_INSERT_TAIL(&block->lf_blkhd, lock, lf_block);
#ifdef LOCKF_DEBUG
		if (lockf_debug & 1) {
			msdosfs_lprint("msdosfs_setlock: blocking on", block);
			msdosfs_lprintlist("msdosfs_setlock", block);
		}
#endif /* LOCKF_DEBUG */
		if (error = msleep((caddr_t)lock, NULL, priority, lockstr, 0)) {
			/*
			 * We may have been awakened by a signal (in
			 * which case we must remove ourselves from the
			 * blocked list) and/or by another process
			 * releasing a lock (in which case we have already
			 * been removed from the blocked list and our
			 * lf_next field set to NOLOCKF).
			 */
			if (lock->lf_next)
				TAILQ_REMOVE(&lock->lf_next->lf_blkhd, lock,
				    lf_block);
			_FREE(lock, M_LOCKF);
			return (error);
		}
	}
	/*
	 * No blocks!!  Add the lock.  Note that we will
	 * downgrade or upgrade any overlapping locks this
	 * process already owns.
	 *
	 * Skip over locks owned by other processes.
	 * Handle any locks that overlap and are owned by ourselves.
	 */
	prev = &dep->de_lockf;
	block = dep->de_lockf;
	needtolink = 1;
	for (;;) {
		if (ovcase = msdosfs_findoverlap(block, lock, SELF, &prev, &overlap))
			block = overlap->lf_next;
		/*
		 * Six cases:
		 *	0) no overlap
		 *	1) overlap == lock
		 *	2) overlap contains lock
		 *	3) lock contains overlap
		 *	4) overlap starts before lock
		 *	5) overlap ends after lock
		 */
		switch (ovcase) {
		case 0: /* no overlap */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap;
			}
			break;

		case 1: /* overlap == lock */
			/*
			 * If downgrading lock, others may be
			 * able to acquire it.
			 */
			if (lock->lf_type == F_RDLCK &&
			    overlap->lf_type == F_WRLCK)
				msdosfs_wakelock(overlap);
			overlap->lf_type = lock->lf_type;
			FREE(lock, M_LOCKF);
			lock = overlap; /* for debug output below */
			break;

		case 2: /* overlap contains lock */
			/*
			 * Check for common starting point and different types.
			 */
			if (overlap->lf_type == lock->lf_type) {
				_FREE(lock, M_LOCKF);
				lock = overlap; /* for debug output below */
				break;
			}
			if (overlap->lf_start == lock->lf_start) {
				*prev = lock;
				lock->lf_next = overlap;
				overlap->lf_start = lock->lf_end + 1;
			} else
				msdosfs_split(overlap, lock);
			msdosfs_wakelock(overlap);
			break;

		case 3: /* lock contains overlap */
			/*
			 * If downgrading lock, others may be able to
			 * acquire it, otherwise take the list.
			 */
			if (lock->lf_type == F_RDLCK &&
			    overlap->lf_type == F_WRLCK) {
				msdosfs_wakelock(overlap);
			} else {
				while (ltmp = overlap->lf_blkhd.tqh_first) {
					TAILQ_REMOVE(&overlap->lf_blkhd, ltmp,
					    lf_block);
					TAILQ_INSERT_TAIL(&lock->lf_blkhd,
					    ltmp, lf_block);
				}
			}
			/*
			 * Add the new lock if necessary and delete the overlap.
			 */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap->lf_next;
				prev = &lock->lf_next;
				needtolink = 0;
			} else
				*prev = overlap->lf_next;
			_FREE(overlap, M_LOCKF);
			continue;

		case 4: /* overlap starts before lock */
			/*
			 * Add lock after overlap on the list.
			 */
			lock->lf_next = overlap->lf_next;
			overlap->lf_next = lock;
			overlap->lf_end = lock->lf_start - 1;
			prev = &lock->lf_next;
			msdosfs_wakelock(overlap);
			needtolink = 0;
			continue;

		case 5: /* overlap ends after lock */
			/*
			 * Add the new lock before overlap.
			 */
			if (needtolink) {
				*prev = lock;
				lock->lf_next = overlap;
			}
			overlap->lf_start = lock->lf_end + 1;
			msdosfs_wakelock(overlap);
			break;
		}
		break;
	}
#ifdef LOCKF_DEBUG
	if (lockf_debug & 1) {
		msdosfs_lprint("msdosfs_setlock: got the lock", lock);
		msdosfs_lprintlist("msdosfs_setlock", lock);
	}
#endif /* LOCKF_DEBUG */
	return (0);
}

/*
 * Remove a byte-range lock on a denode.
 *
 * Generally, find the lock (or an overlap to that lock)
 * and remove it (or shrink it), then wakeup anyone we can.
 */
int
msdosfs_clearlock(unlock)
	register struct msdosfs_lockf *unlock;
{
	struct denode *dep = unlock->lf_denode;
	register struct msdosfs_lockf *lf = dep->de_lockf;
	struct msdosfs_lockf *overlap, **prev;
	int ovcase;

	if (lf == NOLOCKF)
		return (0);
#ifdef LOCKF_DEBUG
	if (unlock->lf_type != F_UNLCK)
		panic("msdosfs_clearlock: bad type");
	if (lockf_debug & 1)
		msdosfs_lprint("msdosfs_clearlock", unlock);
#endif /* LOCKF_DEBUG */
	prev = &dep->de_lockf;
	while (ovcase = msdosfs_findoverlap(lf, unlock, SELF, &prev, &overlap)) {
		/*
		 * Wakeup the list of locks to be retried.
		 */
		msdosfs_wakelock(overlap);

		switch (ovcase) {

		case 1: /* overlap == lock */
			*prev = overlap->lf_next;
			FREE(overlap, M_LOCKF);
			break;

		case 2: /* overlap contains lock: split it */
			if (overlap->lf_start == unlock->lf_start) {
				overlap->lf_start = unlock->lf_end + 1;
				break;
			}
			msdosfs_split(overlap, unlock);
			overlap->lf_next = unlock->lf_next;
			break;

		case 3: /* lock contains overlap */
			*prev = overlap->lf_next;
			lf = overlap->lf_next;
			_FREE(overlap, M_LOCKF);
			continue;

		case 4: /* overlap starts before lock */
			overlap->lf_end = unlock->lf_start - 1;
			prev = &overlap->lf_next;
			lf = overlap->lf_next;
			continue;

		case 5: /* overlap ends after lock */
			overlap->lf_start = unlock->lf_end + 1;
			break;
		}
		break;
	}
#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		msdosfs_lprintlist("msdosfs_clearlock", unlock);
#endif /* LOCKF_DEBUG */
	return (0);
}

/*
 * Check whether there is a blocking lock,
 * and if so return its process identifier.
 */
int
msdosfs_getlock(lock, fl)
	register struct msdosfs_lockf *lock;
	register struct flock *fl;
{
	register struct msdosfs_lockf *block;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 1)
		msdosfs_lprint("msdosfs_getlock", lock);
#endif /* LOCKF_DEBUG */

	if (block = msdosfs_getblock(lock)) {
		fl->l_type = block->lf_type;
		fl->l_whence = SEEK_SET;
		fl->l_start = block->lf_start;
		if (block->lf_end == -1)
			fl->l_len = 0;
		else
			fl->l_len = block->lf_end - block->lf_start + 1;
		if (block->lf_flags & F_POSIX)
			fl->l_pid = proc_pid((proc_t)(block->lf_id));
		else
			fl->l_pid = -1;
	} else {
		fl->l_type = F_UNLCK;
	}
	return (0);
}

/*
 * Walk the list of locks for a denode and
 * return the first blocking lock.
 */
struct msdosfs_lockf *
msdosfs_getblock(lock)
	register struct msdosfs_lockf *lock;
{
	struct msdosfs_lockf **prev, *overlap, *lf = lock->lf_denode->de_lockf;
	int ovcase;

	prev = &lock->lf_denode->de_lockf;
	while (ovcase = msdosfs_findoverlap(lf, lock, OTHERS, &prev, &overlap)) {
		/*
		 * We've found an overlap, see if it blocks us
		 */
		if ((lock->lf_type == F_WRLCK || overlap->lf_type == F_WRLCK))
			return (overlap);
		/*
		 * Nope, point to the next one on the list and
		 * see if it blocks us
		 */
		lf = overlap->lf_next;
	}
	return (NOLOCKF);
}

/*
 * Walk the list of locks for a denode to
 * find an overlapping lock (if any).
 *
 * NOTE: this returns only the FIRST overlapping lock.  There
 *	 may be more than one.
 */
int
msdosfs_findoverlap(lf, lock, type, prev, overlap)
	register struct msdosfs_lockf *lf;
	struct msdosfs_lockf *lock;
	int type;
	struct msdosfs_lockf ***prev;
	struct msdosfs_lockf **overlap;
{
	off_t start, end;

	*overlap = lf;
	if (lf == NOLOCKF)
		return (0);
#ifdef LOCKF_DEBUG
	if (lockf_debug & 2)
		msdosfs_lprint("msdosfs_findoverlap: looking for overlap in", lock);
#endif /* LOCKF_DEBUG */
	start = lock->lf_start;
	end = lock->lf_end;
	while (lf != NOLOCKF) {
		if (((type & SELF) && lf->lf_id != lock->lf_id) ||
		    ((type & OTHERS) && lf->lf_id == lock->lf_id)) {
			*prev = &lf->lf_next;
			*overlap = lf = lf->lf_next;
			continue;
		}
#ifdef LOCKF_DEBUG
		if (lockf_debug & 2)
			msdosfs_lprint("\tchecking", lf);
#endif /* LOCKF_DEBUG */
		/*
		 * OK, check for overlap
		 *
		 * Six cases:
		 *	0) no overlap
		 *	1) overlap == lock
		 *	2) overlap contains lock
		 *	3) lock contains overlap
		 *	4) overlap starts before lock
		 *	5) overlap ends after lock
		 */
		if ((lf->lf_end != -1 && start > lf->lf_end) ||
		    (end != -1 && lf->lf_start > end)) {
			/* Case 0 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("no overlap\n");
#endif /* LOCKF_DEBUG */
			if ((type & SELF) && end != -1 && lf->lf_start > end)
				return (0);
			*prev = &lf->lf_next;
			*overlap = lf = lf->lf_next;
			continue;
		}
		if ((lf->lf_start == start) && (lf->lf_end == end)) {
			/* Case 1 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap == lock\n");
#endif /* LOCKF_DEBUG */
			return (1);
		}
		if ((lf->lf_start <= start) &&
		    (end != -1) &&
		    ((lf->lf_end >= end) || (lf->lf_end == -1))) {
			/* Case 2 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap contains lock\n");
#endif /* LOCKF_DEBUG */
			return (2);
		}
		if (start <= lf->lf_start &&
		           (end == -1 ||
			   (lf->lf_end != -1 && end >= lf->lf_end))) {
			/* Case 3 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("lock contains overlap\n");
#endif /* LOCKF_DEBUG */
			return (3);
		}
		if ((lf->lf_start < start) &&
			((lf->lf_end >= start) || (lf->lf_end == -1))) {
			/* Case 4 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap starts before lock\n");
#endif /* LOCKF_DEBUG */
			return (4);
		}
		if ((lf->lf_start > start) &&
			(end != -1) &&
			((lf->lf_end > end) || (lf->lf_end == -1))) {
			/* Case 5 */
#ifdef LOCKF_DEBUG
			if (lockf_debug & 2)
				printf("overlap ends after lock\n");
#endif /* LOCKF_DEBUG */
			return (5);
		}
		panic("msdosfs_findoverlap: default");
	}
	return (0);
}

/*
 * Split a lock and a contained region into
 * two or three locks as necessary.
 */
void
msdosfs_split(lock1, lock2)
	register struct msdosfs_lockf *lock1;
	register struct msdosfs_lockf *lock2;
{
	register struct msdosfs_lockf *splitlock;

#ifdef LOCKF_DEBUG
	if (lockf_debug & 2) {
		msdosfs_lprint("msdosfs_split", lock1);
		msdosfs_lprint("splitting from", lock2);
	}
#endif /* LOCKF_DEBUG */
	/*
	 * Check to see if spliting into only two pieces.
	 */
	if (lock1->lf_start == lock2->lf_start) {
		lock1->lf_start = lock2->lf_end + 1;
		lock2->lf_next = lock1;
		return;
	}
	if (lock1->lf_end == lock2->lf_end) {
		lock1->lf_end = lock2->lf_start - 1;
		lock2->lf_next = lock1->lf_next;
		lock1->lf_next = lock2;
		return;
	}
	/*
	 * Make a new lock consisting of the last part of
	 * the encompassing lock
	 */
	MALLOC(splitlock, struct msdosfs_lockf *, sizeof *splitlock, M_LOCKF, M_WAITOK);
	bcopy((caddr_t)lock1, (caddr_t)splitlock, sizeof *splitlock);
	splitlock->lf_start = lock2->lf_end + 1;
	TAILQ_INIT(&splitlock->lf_blkhd);
	lock1->lf_end = lock2->lf_start - 1;
	/*
	 * OK, now link it in
	 */
	splitlock->lf_next = lock1->lf_next;
	lock2->lf_next = splitlock;
	lock1->lf_next = lock2;
}

/*
 * Wakeup a blocklist
 */
void
msdosfs_wakelock(listhead)
	struct msdosfs_lockf *listhead;
{
	register struct msdosfs_lockf *wakelock;

	while (wakelock = listhead->lf_blkhd.tqh_first) {
		TAILQ_REMOVE(&listhead->lf_blkhd, wakelock, lf_block);
		wakelock->lf_next = NOLOCKF;
#ifdef LOCKF_DEBUG
		if (lockf_debug & 2)
			msdosfs_lprint("msdosfs_wakelock: awakening", wakelock);
#endif /* LOCKF_DEBUG */
		wakeup((caddr_t)wakelock);
	}
}
