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

#include <dce/dcethread.h>
#include <moonunit/interface.h>

static dcethread_exc dummy_e;
static dcethread_exc dummy2_e;

MU_FIXTURE_SETUP(exception)
{
    DCETHREAD_EXC_INIT(dummy_e);
    DCETHREAD_EXC_INIT(dummy2_e);
}

MU_TEST(exception, nothrow)
{
    volatile int reached_finally = 0;

    DCETHREAD_TRY
    {
	MU_ASSERT(!reached_finally);
    }
    DCETHREAD_CATCH(dummy_e)
    {
	MU_FAILURE("Reached catch block");
    }
    DCETHREAD_FINALLY
    {
	MU_ASSERT(!reached_finally);
	reached_finally = 1;
    }
    DCETHREAD_ENDTRY;

    MU_ASSERT(reached_finally);
}

MU_TEST(exception, throw_catch)
{
    volatile int reached_finally = 0;
    volatile int caught = 0;

    DCETHREAD_TRY
    {
	DCETHREAD_RAISE(dummy_e);
    }
    DCETHREAD_CATCH(dummy_e)
    {
	MU_ASSERT(!reached_finally);
	caught = 1;
    }
    DCETHREAD_FINALLY
    {
	reached_finally = 1;
    }
    DCETHREAD_ENDTRY;

    MU_ASSERT(caught);
    MU_ASSERT(reached_finally);
}

MU_TEST(exception, throw_catch_throw_catch)
{
    volatile int caught_inner = 0;
    volatile int caught_outer = 0;
    volatile int reached_inner_finally = 0;
    volatile int reached_outer_finally = 0;

    DCETHREAD_TRY
    {
	DCETHREAD_TRY
	{
	    MU_ASSERT(!caught_inner);
	    MU_ASSERT(!reached_inner_finally);
	    MU_ASSERT(!caught_outer);
	    MU_ASSERT(!reached_outer_finally);
	    DCETHREAD_RAISE(dummy2_e);
	}
	DCETHREAD_CATCH(dummy2_e)
	{
	    MU_ASSERT(!caught_inner);
	    MU_ASSERT(!reached_inner_finally);
	    MU_ASSERT(!caught_outer);
	    MU_ASSERT(!reached_outer_finally);
	    caught_inner = 1;
	    DCETHREAD_RAISE(dummy_e);
	}
	DCETHREAD_FINALLY
	{
	    MU_ASSERT(caught_inner);
	    MU_ASSERT(!reached_inner_finally);
	    MU_ASSERT(!caught_outer);
	    MU_ASSERT(!reached_outer_finally);
	    reached_inner_finally = 1;
	}
	DCETHREAD_ENDTRY;
    }
    DCETHREAD_CATCH(dummy_e)
    {
	MU_ASSERT(caught_inner);
	MU_ASSERT(reached_inner_finally);
	MU_ASSERT(!caught_outer);
	MU_ASSERT(!reached_outer_finally);
	caught_outer = 1;
    }
    DCETHREAD_FINALLY
    {
	MU_ASSERT(caught_inner);
	MU_ASSERT(reached_inner_finally);
	MU_ASSERT(caught_outer);
	MU_ASSERT(!reached_outer_finally);
	reached_outer_finally = 1;
    }
    DCETHREAD_ENDTRY;

    MU_ASSERT(caught_inner);
    MU_ASSERT(reached_inner_finally);
    MU_ASSERT(caught_outer);
    MU_ASSERT(reached_outer_finally);
}

MU_TEST(exception, throw_catch_finally_throw_catch)
{
    volatile int caught_inner = 0;
    volatile int caught_outer = 0;
    volatile int reached_inner_finally = 0;
    volatile int reached_outer_finally = 0;

    DCETHREAD_TRY
    {
	DCETHREAD_TRY
	{
	    MU_ASSERT(!caught_inner);
	    MU_ASSERT(!reached_inner_finally);
	    MU_ASSERT(!caught_outer);
	    MU_ASSERT(!reached_outer_finally);
	    DCETHREAD_RAISE(dummy2_e);
	}
	DCETHREAD_CATCH(dummy2_e)
	{
	    MU_ASSERT(!caught_inner);
	    MU_ASSERT(!reached_inner_finally);
	    MU_ASSERT(!caught_outer);
	    MU_ASSERT(!reached_outer_finally);
	    caught_inner = 1;
	}
	DCETHREAD_FINALLY
	{
	    MU_ASSERT(caught_inner);
	    MU_ASSERT(!reached_inner_finally);
	    MU_ASSERT(!caught_outer);
	    MU_ASSERT(!reached_outer_finally);
	    reached_inner_finally = 1;
	    DCETHREAD_RAISE(dummy_e);
	}
	DCETHREAD_ENDTRY;
    }
    DCETHREAD_CATCH(dummy_e)
    {
	MU_ASSERT(caught_inner);
	MU_ASSERT(reached_inner_finally);
	MU_ASSERT(!caught_outer);
	MU_ASSERT(!reached_outer_finally);
	caught_outer = 1;
    }
    DCETHREAD_FINALLY
    {
	MU_ASSERT(caught_inner);
	MU_ASSERT(reached_inner_finally);
	MU_ASSERT(caught_outer);
	MU_ASSERT(!reached_outer_finally);
	reached_outer_finally = 1;
    }
    DCETHREAD_ENDTRY;

    MU_ASSERT(caught_inner);
    MU_ASSERT(reached_inner_finally);
    MU_ASSERT(caught_outer);
    MU_ASSERT(reached_outer_finally);
}

MU_TEST(exception, throw_finally_throw_catch)
{
    volatile int caught_outer = 0;
    volatile int reached_inner_finally = 0;
    volatile int reached_outer_finally = 0;

    DCETHREAD_TRY
    {
	DCETHREAD_TRY
	{
	    MU_ASSERT(!reached_inner_finally);
	    MU_ASSERT(!caught_outer);
	    MU_ASSERT(!reached_outer_finally);
	    DCETHREAD_RAISE(dummy2_e);
	}
	DCETHREAD_FINALLY
	{
	    MU_ASSERT(!reached_inner_finally);
	    MU_ASSERT(!caught_outer);
	    MU_ASSERT(!reached_outer_finally);
	    reached_inner_finally = 1;
	    DCETHREAD_RAISE(dummy_e);
	}
	DCETHREAD_ENDTRY;
    }
    DCETHREAD_CATCH(dummy_e)
    {
	MU_ASSERT(reached_inner_finally);
	MU_ASSERT(!caught_outer);
	MU_ASSERT(!reached_outer_finally);
	caught_outer = 1;
    }
    DCETHREAD_FINALLY
    {
	MU_ASSERT(reached_inner_finally);
	MU_ASSERT(caught_outer);
	MU_ASSERT(!reached_outer_finally);
	reached_outer_finally = 1;
    }
    DCETHREAD_ENDTRY;

    MU_ASSERT(reached_inner_finally);
    MU_ASSERT(caught_outer);
    MU_ASSERT(reached_outer_finally);
}

MU_TEST(exception, uncaught)
{
    MU_EXPECT(MU_STATUS_EXCEPTION);

    DCETHREAD_RAISE(dummy_e);
}
