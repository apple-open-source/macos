/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2008 Likewise Software, Inc. All rights reserved.
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

#include <config.h>
#include <errno.h>

#include "dcethread-private.h"
#include "dcethread-util.h"
#include "dcethread-debug.h"

#ifdef API

int
dcethread_interrupt(dcethread* thread)
{
    dcethread__lock(thread);
    dcethread__interrupt(thread);
    dcethread__unlock(thread);

    return dcethread__set_errno(0);
}

int
dcethread_interrupt_throw(dcethread* thread)
{
    DCETHREAD_WRAP_THROW(dcethread_interrupt(thread));
}

#endif /* API */

#ifdef TEST

#include "dcethread-test.h"

static void*
basic_thread(void* data)
{
    volatile int interrupt_caught = 0;

    DCETHREAD_TRY
    {
	MU_ASSERT(!interrupt_caught);
	while (1)
	{
	    dcethread_checkinterrupt();
	    dcethread_yield();
	}
    }
    DCETHREAD_CATCH(dcethread_interrupt_e)
    {
	MU_ASSERT(!interrupt_caught);
	interrupt_caught = 1;
    }
    DCETHREAD_ENDTRY;

    MU_ASSERT(interrupt_caught);

    return NULL;
}

MU_TEST(dcethread_interrupt, basic)
{
    dcethread* thread;

    MU_TRY_DCETHREAD( dcethread_create(&thread, NULL, basic_thread, NULL) );
    MU_TRY_DCETHREAD( dcethread_interrupt(thread) );
    MU_TRY_DCETHREAD( dcethread_join(thread, NULL) );
}

MU_TEST(dcethread_interrupt, self)
{
    volatile int interrupt_caught = 0;

    DCETHREAD_TRY
    {
        MU_ASSERT(!interrupt_caught);
        dcethread_interrupt(dcethread_self());
        dcethread_checkinterrupt();
    }
    DCETHREAD_CATCH(dcethread_interrupt_e)
    {
        MU_ASSERT(!interrupt_caught);
        interrupt_caught = 1;
    }
    DCETHREAD_ENDTRY;

    MU_ASSERT(interrupt_caught);
}

MU_TEST(dcethread_interrupt, disable)
{
    DCETHREAD_TRY
    {
        dcethread_enableinterrupt(0);
        dcethread_interrupt(dcethread_self());
        dcethread_checkinterrupt();
    }
    DCETHREAD_CATCH(dcethread_interrupt_e)
    {
        MU_ASSERT_NOT_REACHED();
    }
    DCETHREAD_ENDTRY;
}

MU_TEST(dcethread_interrupt, disable_enable)
{
    volatile int interrupt_caught = 0;

    DCETHREAD_TRY
    {
        dcethread_enableinterrupt(0);
        dcethread_interrupt(dcethread_self());
        dcethread_enableinterrupt(1);
        dcethread_checkinterrupt();
    }
    DCETHREAD_CATCH(dcethread_interrupt_e)
    {
        MU_ASSERT(!interrupt_caught);
        interrupt_caught = 1;
    }
    DCETHREAD_ENDTRY;

    MU_ASSERT(interrupt_caught);
}

static dcethread_mutex bug_6386_mutex = DCETHREAD_MUTEX_INITIALIZER;

static void*
bug_6386_thread(void* data)
{
    MU_TRY_DCETHREAD(dcethread_mutex_lock(&bug_6386_mutex));
    MU_TRY_DCETHREAD(dcethread_mutex_unlock(&bug_6386_mutex));

    return data;
}

/* Test for regression of bug 6386, which causes
   deadlock when a thread interrupted during a
   dcethread_mutex_lock of a mutex held by
   the interrupting thread */
MU_TEST(dcethread_interrupt, bug_6386)
{
    dcethread* thread = NULL;

    MU_TRY_DCETHREAD(dcethread_mutex_lock(&bug_6386_mutex));
    MU_TRY_DCETHREAD(dcethread_create(&thread, NULL, bug_6386_thread, NULL));
    MU_TRY_DCETHREAD(dcethread_interrupt(thread));
    MU_TRY_DCETHREAD(dcethread_mutex_unlock(&bug_6386_mutex));
    MU_TRY_DCETHREAD(dcethread_join(thread, NULL));
}

#endif /* TEST */
