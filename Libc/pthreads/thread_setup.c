/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991  
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * MkLinux
 */

/*
 * Machine specific support for thread initialization
 */

#if defined(__ppc__)
#include <architecture/ppc/cframe.h>
#endif

#include "pthread_internals.h"

/*
 * Set up the initial state of a MACH thread
 */
void
_pthread_setup(pthread_t thread, 
	       void (*routine)(pthread_t), 
	       void *vsp, int suspended,
	       int needresume)
{
        kern_return_t r;
        unsigned int count;
#if defined(__ppc__)
        struct ppc_thread_state state = {0};
	struct ppc_thread_state *ts = &state;

	/*
	 * Set up PowerPC registers.
	 */
	count = PPC_THREAD_STATE_COUNT;
	if (suspended) {
		PTHREAD_MACH_CALL(thread_get_state(thread->kernel_thread,
					   PPC_THREAD_STATE,
					   (thread_state_t) &state,
					   &count),
			  r);
	}
	ts->srr0 = (int) routine;
        ts->r1 = (uintptr_t)vsp - C_ARGSAVE_LEN - C_RED_ZONE;
	ts->r3 = (int)thread;
	/* Incase of needresume, suspend is always set */
	if (suspended) {
		PTHREAD_MACH_CALL(thread_set_state(thread->kernel_thread,
					   PPC_THREAD_STATE,
					   (thread_state_t) &state,
					   PPC_THREAD_STATE_COUNT),
			  r);
		if (needresume)
			PTHREAD_MACH_CALL(thread_resume(thread->kernel_thread),
				r);
	} else {
		PTHREAD_MACH_CALL(thread_create_running(mach_task_self(),
					PPC_THREAD_STATE,
					(thread_state_t) ts,
					PPC_THREAD_STATE_COUNT,
					&thread->kernel_thread),
			r);
	}
#elif defined(__i386__)
        i386_thread_state_t state = {0};
        i386_thread_state_t *ts = &state;
        int *sp = vsp;

        /*
         * Set up i386 registers & function call.
         */
        count = i386_THREAD_STATE_COUNT;
	if (suspended) {
	        PTHREAD_MACH_CALL(thread_get_state(thread->kernel_thread,
					   i386_THREAD_STATE,
					   (thread_state_t) &state,
					   &count),
			  r);
	}
        ts->eip = (int) routine;
        *--sp = (int) thread;	/* argument to function */
        *--sp = 0;		/* fake return address */
        ts->esp = (int) sp;	/* set stack pointer */
	/* Incase of needresume, suspend is always set */
        if (suspended) {
		PTHREAD_MACH_CALL(thread_set_state(thread->kernel_thread,
					   i386_THREAD_STATE,
					   (thread_state_t) &state,
					   i386_THREAD_STATE_COUNT),
			  r);
		if (needresume)
			PTHREAD_MACH_CALL(thread_resume(thread->kernel_thread),
				r);
	} else {
		PTHREAD_MACH_CALL(thread_create_running(mach_task_self(),
					i386_THREAD_STATE,
					(thread_state_t) ts,
					i386_THREAD_STATE_COUNT,
					&thread->kernel_thread),
			r);
	}

#else
#error _pthread_setup not defined for this architecture
#endif
}
