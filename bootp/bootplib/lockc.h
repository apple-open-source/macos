/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#import "threadcompat.h"

typedef struct {
    MUTEX_STRUCT	lock;
    CONDITION_STRUCT	wakeup;
} lockc_t;

static __inline__ void
lockc_init(lockc_t * l)
{
    mutex_init(&l->lock);
    condition_init(&l->wakeup);
}

static __inline__ void
lockc_signal(lockc_t * l)
{
    mutex_lock(&l->lock);
    condition_signal(&l->wakeup);
    mutex_unlock(&l->lock);
}

static __inline__ void
lockc_signal_nl(lockc_t * l)
{
    condition_signal(&l->wakeup);
}

static __inline__ void
lockc_lock(lockc_t * l)
{
    mutex_lock(&l->lock);
}

static __inline__ void
lockc_unlock(lockc_t * l)
{
    mutex_unlock(&l->lock);
}

static __inline__ void
lockc_wait(lockc_t * l)
{
    condition_wait(&l->wakeup, &l->lock);
}

static __inline__ void
lockc_free(lockc_t * l)
{
    mutex_clear(&l->lock);
    condition_clear(&l->wakeup);
}

