/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *	Copyright (c) 1998 Apple Computer, Inc. 
 *
 *	The information contained herein is subject to change without
 *	notice and  should not be  construed as a commitment by Apple
 *	Computer, Inc. Apple Computer, Inc. assumes no responsibility
 *	for any errors that may appear.
 *
 *	Confidential and Proprietary to Apple Computer, Inc.
 *
 * at_rwlock.c - AppleTalk private version of read/write locks.
 * These services should be implemented in a system-wide library.
 * Until they are, we'll implement our own in terms of Cthreads.
 *
 * c.f. Butenhof, Programming with POSIX Threads, Addison-Wesley, 1997.
 * ISBN 0-201-63392-2. pp 253 ff.
 *
 * This file implements the "read-write lock" synchronization
 * construct.
 *
 * A read-write lock allows a thread to lock shared data either
 * for shared read access or exclusive write access.
 *
 * The at_rwl_init() and at_rwl_destroy() functions, respectively,
 * allow you to initialize/create and destroy/free the
 * read-write lock.
 *
 * The at_rwl_readlock() function locks a read-write lock for
 * shared read access, and at_rwl_readunlock() releases the
 * lock. at_rwl_readtrylock() attempts to lock a read-write lock
 * for read access, and returns EBUSY instead of blocking.
 *
 * The at_rwl_writelock() function locks a read-write lock for
 * exclusive write access, and at_rwl_writeunlock() releases the
 * lock. at_rwl_writetrylock() attempts to lock a read-write lock
 * for write access, and returns EBUSY instead of blocking.
 */

#include <errno.h>
#include "at_rwlock.h"

/* Note that the rwlock functions have been commented out using
   "#ifdef ASP_LOCKS".  As such time as the rwlock routines are
   converted to use pthreads and the AppleTalk ASP routines can be
   tested with the AppleFileServer, this code should be restored.
*/

#ifdef ASP_LOCKS

/*
 * Initialize a read-write lock
 */
int
at_rwl_init(at_rwlock_t *rwl)
{
    rwl->r_active = 0;
    rwl->r_wait = rwl->w_wait = 0;
    rwl->w_active = 0;
    mutex_init(&rwl->mutex);
    condition_init(&rwl->read);
    condition_init(&rwl->write);
    rwl->valid = RWLOCK_VALID;
    return 0;
}

/*
 * Destroy a read-write lock
 */
int
at_rwl_destroy(at_rwlock_t *rwl)
{
    if (rwl->valid != RWLOCK_VALID)
        return EINVAL;

    mutex_lock(&rwl->mutex);

    /*
     * Check whether any threads own the lock; report "BUSY" if
     * so.
     */
    if (rwl->r_active > 0 || rwl->w_active) {
        mutex_unlock(&rwl->mutex);
        return EBUSY;
    }

    /*
     * Check whether any threads are known to be waiting; report
     * EBUSY if so.
     */
    if (rwl->r_wait != 0 || rwl->w_wait != 0) {
        mutex_unlock(&rwl->mutex);
        return EBUSY;
    }

    rwl->valid = 0;
    mutex_unlock(&rwl->mutex);
    mutex_clear(&rwl->mutex);
    condition_clear(&rwl->read);
    condition_clear(&rwl->write);
    return 0;
}

/*
 * Lock a read-write lock for read access.
 */
int
at_rwl_readlock(at_rwlock_t *rwl)
{
    if (rwl->valid != RWLOCK_VALID)
        return EINVAL;

    mutex_lock(&rwl->mutex);
    if (rwl->w_active) {
        rwl->r_wait++;
        while (rwl->w_active) {
            condition_wait(&rwl->read, &rwl->mutex);
        }
        rwl->r_wait--;
    }
    rwl->r_active++;
    mutex_unlock(&rwl->mutex);
    return 0;
}

/*
 * Attempt to lock a read-write lock for read access (don't
 * block if unavailable).
 */
int
at_rwl_readtrylock(at_rwlock_t *rwl)
{
    int status;

    if (rwl->valid != RWLOCK_VALID)
        return EINVAL;

    mutex_lock(&rwl->mutex);
    if (rwl->w_active)
        status = EBUSY;
    else {
        status = 0;
        rwl->r_active++;
    }
    mutex_unlock(&rwl->mutex);
    return status;
}

/*
 * Unlock a read-write lock from read access.
 */
int
at_rwl_readunlock(at_rwlock_t *rwl)
{
    if (rwl->valid != RWLOCK_VALID)
        return EINVAL;

    mutex_lock(&rwl->mutex);
    rwl->r_active--;
    if (rwl->r_active == 0 && rwl->w_wait > 0)
        condition_signal(&rwl->write);
    mutex_unlock(&rwl->mutex);
    return 0;
}

/*
 * Lock a read-write lock for write access.
 */
int
at_rwl_writelock(at_rwlock_t *rwl)
{
    if (rwl->valid != RWLOCK_VALID)
        return EINVAL;

    mutex_lock(&rwl->mutex);
    if (rwl->w_active || rwl->r_active > 0) {
        rwl->w_wait++;
        while (rwl->w_active || rwl->r_active > 0) {
            condition_wait(&rwl->write, &rwl->mutex);
        }
        rwl->w_wait--;
    }
    rwl->w_active = 1;
    mutex_unlock(&rwl->mutex);
    return 0;
}

/*
 * Attempt to lock a read-write lock for write access. Don't
 * block if unavailable.
 */
int
at_rwl_writetrylock(at_rwlock_t *rwl)
{
    int status;

    if (rwl->valid != RWLOCK_VALID)
        return EINVAL;
    mutex_lock(&rwl->mutex);
    if (rwl->w_active || rwl->r_active > 0)
        status = EBUSY;
    else {
        status = 0;
        rwl->w_active = 1;
    }
    mutex_unlock(&rwl->mutex);
    return status;
}

/*
 * Unlock a read-write lock from write access.
 */
int
at_rwl_writeunlock(at_rwlock_t *rwl)
{
    if (rwl->valid != RWLOCK_VALID)
        return EINVAL;

    mutex_lock(&rwl->mutex);
    rwl->w_active = 0;
    if (rwl->r_wait > 0) {
        condition_broadcast(&rwl->read);
    } else if (rwl->w_wait > 0) {
        condition_signal(&rwl->write);
    }
    mutex_unlock(&rwl->mutex);
    return 0;
}

#endif /* ASP_LOCKS */
