/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __DCETHREAD_PRIVATE_H__
#define __DCETHREAD_PRIVATE_H__

#include <dce/dcethread.h>

struct _dcethread
{
    /* Handle of associated pthread */
    pthread_t pthread;

    /* Reference count */
    unsigned int refs;

    /* Current state of thread
       Changing state should be done with
       lock held and a signal should be
       broadcast on state_changed */
    enum
    {
	/* Normal, running state */
	DCETHREAD_STATE_ACTIVE = 0,
	/* Blocked */
	DCETHREAD_STATE_BLOCKED = 1,
	/* An interrupt has been requested */
	DCETHREAD_STATE_INTERRUPT = 2,
	/* Thread has exited */
	DCETHREAD_STATE_DEAD = 5,
	/* Thread has been created but is not yet running */
	DCETHREAD_STATE_CREATED = 6
    } state;

    /* Boolean attributes */
    struct
    {
        /* Do we own the underlying pthread?
           (that is, it should be joined/detached when
           this dcethread is deleted)
           FIXME: give this a better name */
	unsigned joinable :1;
        /* Is asychronous cancellation enabled? */
	unsigned async :1;
        /* Is the thread currently interruptible? */
	unsigned interruptible :1;
        /* Is the thread currently locked? (for debugging) */
        unsigned locked :1;
    } flag;

    /* Return status (as set by pthread_exit() or returning from start func) */
    void* status;

    /* Function to perform an interruption on this thread.
       This function is called after setting the state to
       INTERRUPT in order to wake it up as necessary.
       Returns 1 if the thread is definitely interrupted,
       0 if it may not have worked */
    int (*interrupt)(dcethread* thread, void* data);

    /* Data pointer for interruption function */
    void *interrupt_data;

    /* Function to handle an interruption.
       This function is invoked within this thread when
       an interruption is acknowledged.  The default
       behavior is to throw an exception. */
    void (*handle_interrupt)(dcethread* thread, void* data);

    /* Data pointer for interrupt handler function */
    void *handle_interrupt_data;

    /* Lock for atomically updating this structure */
    pthread_mutex_t lock;

    /* Condition variable to wait for a state change */
    pthread_cond_t state_change;
};

typedef struct
{
    dcethread_mutex *mutex;
    dcethread_cond *cond;
} condwait_info;

void dcethread__init (void);
dcethread* dcethread__new (void);
void dcethread__delete(dcethread* thread);
dcethread* dcethread__self(void);
void dcethread__init_self(dcethread* thread);
void dcethread__retain(dcethread* thread);
void dcethread__release(dcethread* thread);
void dcethread__lock(dcethread* thread);
void dcethread__unlock(dcethread* info);
void dcethread__wait(dcethread* thread);
void dcethread__timedwait(dcethread* thread, struct timespec* ts);
void dcethread__change_state(dcethread* thread, int state);
void dcethread__lock_global(void);
void dcethread__unlock_global(void);
void dcethread__dispatchinterrupt(dcethread* thread);
void dcethread__interrupt(dcethread* thread);
void dcethread__set_interrupt_handler(dcethread* thread, void (*handle_interrupt)(dcethread*, void*), void* data);
int dcethread__begin_block(dcethread* thread, int (*interrupt)(dcethread*, void*), void* data, int (**old_interrupt)(dcethread*, void*), void** old_data);
int dcethread__poll_end_block(dcethread* thread, int (*interrupt)(dcethread*, void*), void* data);
int dcethread__end_block(dcethread* thread, int (*interrupt)(dcethread*, void*), void* data);
int dcethread__interrupt_syscall(dcethread* thread, void* data);
int dcethread__interrupt_condwait(dcethread* thread, void* data);
void dcethread__cleanup_self(dcethread* self);

#endif
