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

/*
**
**  NAME
**
**      rpctimer.h
**
**  FACILITY:
**
**      Remote Procedure Call (RPC)
**
**  ABSTRACT:
**
**  Interface to NCK timer functions.
**
**  Suggested uasge:  These routines are tailored for the situation
**  in which a monitor routine is needed to keep track of the state
**  of a data structure.  In this case, one of the fields in the data
**  structure should be a rpc_timer_t (defined below).  The user of the
**  timer service would then call the rpc__timer_set routine with the
**  following arguments:
**
**      1) the address of the rpc_timer_t variable
**      2) the address of the routine to run
**      3) a dce_pointer_t value to be sent to the routine when run
**      4) the frequency with which the routine should be run
**
**  The pointer value used will most often be the address of the data
**  structure being monitored.  (However, it also possible to declare
**  a global rpc_timer_t to be used to periodically run a routine not
**  associated with any particular data object.  N.B.  It is necessary
**  that the rpc_timer_t variable exist during the entire time that the
**  periodic routine is registered to run;  that is, don't declare it on
**  the stack if the current routine will return before unregistering the
**  periodic routine.)
**
**  Users of this service should not keep track of time by the frequency
**  with which their periodic routines are run.  Aside from the lack of
**  accuracy of user space time functions, it is also possible that the
**  system's idea of the current time may be changed at any time.  When
**  necessary, the routines currently err in favor of running a periodic
**  routine too early rather than too late.  For this reason, data
**  struture monitoring should follow this outline:
**
**      struct {
**          .....
**          rpc_clock_t        timestamp;
**          rpc_timer_t        timer;
**          .....
**      } foo;
**
**      rpc__clock_stamp( &foo.timestamp );
**
**      rpc__timer_set( &foo.timer, foo_mon, &foo, rpc_timer_sec(1) );
**          ...
**
**      void foo_mon( parg )
**      dce_pointer_t parg;
**      {
**          if( rpc__clock_aged( parg->timestamp, rpc_timer_sec( 1 ) )
**          {
**            ...
**          }
**      }
**
**
**
*/

#ifndef _RPCTIMER_H
#define _RPCTIMER_H

typedef void (*rpc_timer_proc_p_t) ( dce_pointer_t );

/*
 * This type is used to create a list of periodic functions ordered by
 * trigger time.
 */
typedef struct rpc_timer_t
{
    struct rpc_timer_t   *next;
    rpc_clock_t          trigger;
    rpc_clock_t          frequency;
    rpc_timer_proc_p_t   proc;
    dce_pointer_t            parg;
} rpc_timer_t, *rpc_timer_p_t;

/*
 * Initialize the timer package.
 */
PRIVATE void rpc__timer_init (void);

/*
 * Timer package fork handling routine
 */
PRIVATE void rpc__timer_fork_handler (
    rpc_fork_stage_id_t  /*stage*/
);

/*
 * Shutdown the timer package.
 */
PRIVATE void rpc__timer_shutdown (void);

/*
 * Register a routine to be run at a specific interval.
 */
PRIVATE void rpc__timer_set (
    rpc_timer_p_t /*t*/,
    rpc_timer_proc_p_t /*proc*/,
    dce_pointer_t /*parg*/,
    rpc_clock_t  /*freq*/
);

/*
 * Change one or more of the characteristics of a periodic routine.
 */
PRIVATE void rpc__timer_adjust (
    rpc_timer_p_t /*t*/,
    rpc_clock_t /*freq*/
);

/*
 * Discontinue running a previously registered periodic routine.
 */
PRIVATE void rpc__timer_clear (rpc_timer_p_t /*t*/);

/*
 * Run any periodic routines that are ready.  Return the amount of time
 * until the next scheduled routine should be run.
 */
PRIVATE rpc_clock_t rpc__timer_callout (void);

#endif /* _RPCTIMER_H */
