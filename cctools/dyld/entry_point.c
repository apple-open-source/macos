/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#import <mach-o/loader.h>
#ifdef m68k
#import <mach/m68k/thread_status.h>
#endif
#ifdef __i386__
#import <mach/i386/thread_status.h>
#endif
#ifdef hppa
#import <mach/hppa/thread_status.h>
#endif
#ifdef sparc
#import <mach/sparc/thread_status.h>
#endif
#ifdef __ppc__
#import <mach/ppc/thread_status.h>
#endif

/*
 * get_entry_point() is passed a pointer to the LC_UNIXTHREAD command in an
 * executable and returns the entry point address from it.
 */
unsigned long
get_entry_point(
struct thread_command *thread_command)
{
    unsigned long *p, flavor, count;
#ifdef m68k
    struct m68k_thread_state_regs *cpu;
#endif
#ifdef __i386__
    i386_thread_state_t *cpu;
#endif
#ifdef hppa
    struct hp_pa_frame_thread_state *cpu;
#endif
#ifdef sparc
    struct sparc_thread_state_regs *cpu;
#endif
#ifdef __ppc__
    ppc_thread_state_t *cpu;
#endif

	p = (unsigned long *)(((char *)thread_command) +
			      sizeof(struct thread_command));
	for(;;){
	    flavor = *p++;
	    count = *p++;
#ifdef m68k
	    if(flavor == M68K_THREAD_STATE_REGS){
		cpu = (struct m68k_thread_state_regs *)p;
		return(cpu->pc);
	    }
#endif
#ifdef __i386__
	    if(flavor == i386_THREAD_STATE){
		cpu = (i386_thread_state_t *)p;
		return(cpu->eip);
	    }
#endif
#ifdef hppa
	    if(flavor == HPPA_FRAME_THREAD_STATE){
		cpu = (struct hp_pa_frame_thread_state *)p;
		return(cpu->ts_pcoq_front);
	    }
#endif
#ifdef sparc
	    if(flavor == SPARC_THREAD_STATE_REGS){
		cpu = (struct sparc_thread_state_regs *)p;
		return(cpu->regs.r_pc);
	    }
#endif
#ifdef __ppc__
	    if(flavor == PPC_THREAD_STATE){
		cpu = (ppc_thread_state_t *)p;
		return(cpu->srr0);
	    }
#endif
	    p += count;
	}
}
