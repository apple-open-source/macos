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
 * c.f. Butenhof, Programming with POSIX Threads, Addison-Wesley, 1997.
 * ISBN 0-201-63392-2. pp 253 ff.
 *
 * rwlock.h
 *
 * Adapted for cthreads on Apple Rhapsody from original pthreads code
 * included in David Butenhof's "Programming with POSIX Threads" book.
 *
 * This header file describes the "reader/writer lock" synchronization
 * construct. The type at_rwlock_t describes the full state of the lock
 * including the POSIX 1003.1c synchronization objects necessary.
 *
 * A reader/writer lock allows a thread to lock shared data either for shared
 * read access or exclusive write access.
 *
 * The at_rwl_init() and at_rwl_destroy() functions, respectively, allow you to
 * initialize/create and destroy/free the reader/writer lock.
 */

#ifndef _AT_RWLOCK_H_
#define	_AT_RWLOCK_H_

/* Note that the rwlock functions have been commented out using
   "#ifdef ASP_LOCKS".  As such time as the rwlock routines are
   converted to use pthreads and the AppleTalk ASP routines can be
   tested with the AppleFileServer, this code should be restored.
*/

#ifdef ASP_LOCKS

#include <mach/cthreads.h>

/*
 * Structure describing a read-write lock.
 */
typedef struct {
    struct mutex     	mutex;
    struct condition	read;           /* wait for read */
    struct condition	write;          /* wait for write */
    int                 valid;          /* set when valid */
    int                 r_active;       /* readers active */
    int                 w_active;       /* writer active */
    int                 r_wait;         /* readers waiting */
    int                 w_wait;         /* writers waiting */
} at_rwlock_t;

#define RWLOCK_VALID    0xfacade

/*
 * Define read-write lock functions
 */
extern int at_rwl_init(at_rwlock_t *rwlock);
extern int at_rwl_destroy(at_rwlock_t *rwlock);
extern int at_rwl_readlock(at_rwlock_t *rwlock);
extern int at_rwl_readtrylock(at_rwlock_t *rwlock);
extern int at_rwl_readunlock(at_rwlock_t *rwlock);
extern int at_rwl_writelock(at_rwlock_t *rwlock);
extern int at_rwl_writetrylock(at_rwlock_t *rwlock);
extern int at_rwl_writeunlock(at_rwlock_t *rwlock);

#endif /* ASP_LOCKS */

#endif /* not _AT_RWLOCK_H */
