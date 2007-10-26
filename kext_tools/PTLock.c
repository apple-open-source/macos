/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#include "PTLock.h"
#include <stdlib.h>
#include <mach/message.h>


/*******************************************************************************
*
*******************************************************************************/
typedef struct __PTLock {
    Boolean locked;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
} PTLock;


/*******************************************************************************
*
*******************************************************************************/
PTLockRef PTLockCreate(void)
{
    PTLock * lock;

    lock = (PTLock *)malloc(sizeof(PTLock));
    if (!lock) {
        return NULL;
    }

    lock->locked = false;
    pthread_mutex_init(&lock->mutex, NULL);
    pthread_cond_init(&lock->condition, NULL);

    return (PTLockRef)lock;
}

/*******************************************************************************
*
*******************************************************************************/
void PTLockFree(PTLockRef lock)
{
    if (!lock) {
        return;
    }

    pthread_mutex_destroy(&lock->mutex);
    pthread_cond_destroy(&lock->condition);
    free(lock);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void PTLockTakeLock(PTLockRef lock)
{
    pthread_mutex_lock(&lock->mutex);
    while (lock->locked) {
        pthread_cond_wait(&lock->condition, &lock->mutex);
    }
    lock->locked = true;
    pthread_mutex_unlock(&lock->mutex);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
void PTLockUnlock(PTLockRef lock)
{
    pthread_mutex_lock(&lock->mutex);
    lock->locked = false;
    pthread_cond_signal(&lock->condition);
    pthread_mutex_unlock(&lock->mutex);
    return;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean PTLockTryLock(PTLockRef lock)
{
    Boolean available = false;
    Boolean got_it = false;

    pthread_mutex_lock(&lock->mutex);
    available = !lock->locked;
    if (available) {
        lock->locked = true;
        got_it = true;
    }
    pthread_mutex_unlock(&lock->mutex);

    return got_it;
}

