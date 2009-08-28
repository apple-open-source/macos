/*-
 * Copyright (c) 2008 Apple, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 *
 */
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include "amt.h"

/* 
 * This code verifies that privileged instructions don't work outside of
 * supervisor mode for the i386, x86_64, ppc, and ppc_64.
 */ 

#if !defined(__i386__) && !defined(__x86_64__) && !defined(__ppc__) && \
    !defined(__ppc64__)
# warning Architecture not supported.
#endif

struct priv_test {
	void		(*pt_fp)(void *);
	char		*pt_inst; 
	char		*pt_desc;
	u_char		 pt_result;
};

#if defined(__i386__) || defined(__x86_64__)
static void	inst_hlt(void *arg);
static void	inst_rdpmc(void *arg);
static void	inst_clts(void *arg);
static void	inst_lgdt(void *arg);
static void	inst_lidt(void *arg);
static void	inst_ltr(void *arg);
static void	inst_lldt(void *arg);
#if 0
static void	inst_cpu(void *arg);
#endif
#endif /* __i386__ || __x86_64__ */
#if defined(__x86_64__)
static void	inst_lmsw(void *arg);
static void	inst_rdmsr(void *arg);
static void	inst_wrmsr(void *arg);
#endif /* __x86_64__ */
#if defined(__ppc__) || defined(__ppc64__)
static void	inst_mfmsr(void *arg);
static void	inst_tlbsync(void *arg);
static void	inst_mfspr(void *arg);
static void	inst_mfsprintr(void *arg);
#endif /* __ppc__ || __ppc64__ */

static struct priv_test priv_tests[] = {
#if defined(__i386__) || defined(__x86_64__)
	{ inst_hlt, "HLT", "Halt", SIGSEGV },
	{ inst_rdpmc, "RDPMC", "Read Performance Monitoring Counters", SIGSEGV},
	{ inst_clts, "CLTS", "Clear Task-Switched Flag", SIGSEGV },
#if 0 
	{ inst_cpu, "CPU", "Write to CPU (CS) control Register", SIGILL },
#endif
#endif /* __i386__ || __x86_64__ */
#if defined(__i386__)
	{ inst_lgdt, "LGDT", "Load Global Descriptor Tables", SIGBUS },
	{ inst_lidt, "LIDT", "Load Interrupt Descriptor Table", SIGBUS },
	{ inst_ltr, "LTR", "Load Task Register", SIGBUS },
	{ inst_lldt, "LLDT", "Load Local Descriptor Table", SIGBUS },
#endif /* __i386__ */
#if defined(__x86_64__)
	{ inst_lgdt, "LGDT", "Load Global Descriptor Tables", SIGSEGV },
	{ inst_lidt, "LIDT", "Load Interrupt Descriptor Table", SIGSEGV },
	{ inst_ltr, "LTR", "Load Task Register", SIGSEGV },
	{ inst_lldt, "LLDT", "Load Local Descriptor Table", SIGSEGV },
	{ inst_lmsw, "LMSW", "Load Machine Status Word", SIGSEGV },
	{ inst_rdmsr, "RDMSR", "Read from Model Specific Register", SIGSEGV },
	{ inst_wrmsr, "WRMSR", "Write to Model Specific Register", SIGSEGV },
#endif /* __x86_64__ */
#if defined (__ppc__) || defined(__ppc64__)
	{ inst_mfmsr, "MFMSR", "Move from Segment Register", SIGILL },
	{ inst_tlbsync, "TLBSYNC", "TLB Synchronize", SIGILL },
	{ inst_mfspr, "MFSPR", "Move From Special Purpose Register", SIGILL },
	{ inst_mfsprintr, "MFSPR(intr)", 
	    "Move From Special Purpose Register", SIGILL },
#endif /* __ppc__ || __ppc64__ */
	{ NULL, NULL, NULL, 0 }
};

#if defined(__i386__) || defined(__x86_64__)
static void
inst_hlt(__unused void *arg)
{

	__asm__("HLT\n\t");
}

static void
inst_rdpmc(__unused void *arg)
{

	__asm__("RDPMC\n\t");
}

static void
inst_clts(__unused void *arg)
{

	__asm__("CLTS\n\t");
}

static void
inst_lgdt(__unused void *arg)
{

	__asm__("SGDT 4\n\t");
	__asm__("LGDT 4\n\t");
}

static void
inst_lidt(__unused void *arg)
{

	__asm__("SIDT 4\n\t");
	__asm__("LIDT 4\n\t");
}

static void
inst_ltr(__unused void *arg)
{

	__asm__("STR 4\n\t");
	__asm__("LTR 4\n\t");
}

static void
inst_lldt(__unused void *arg)
{

	__asm__("SLDT 4\n\t");
	__asm__("LLDT 4\n\t");
}

#if 0
static void 
inst_cpu(__unused void *arg)
{
	
	__asm__("MOVL %cs,28(%esp)\n\t");
}
#endif

#if defined(__x86_64__)
static void
inst_lmsw(__unused void *arg)
{

	__asm__("LMSW 4\n\t");
}

static void
inst_rdmsr(__unused void *arg)
{
	
	__asm__("RDMSR\n\t");
}

static void
inst_wrmsr(__unused void *arg)
{

	__asm__("WRMSR\n\t");
}
#endif /* __x86_64__ */
#endif /* __i386__ || __x86_64__ */

#if defined (__ppc__) || defined(__ppc64__)
static void
inst_mfmsr(__unused void *arg)
{

	__asm__("mfmsr r4\n\t");
}

static void
inst_tlbsync(__unused void *arg)
{

	__asm__("tlbsync\n\t");
}

static void
inst_mfspr(__unused void *arg)
{

	__asm__("mfspr r6,17\n\t");
}

static void
inst_mfsprintr(__unused void *arg)
{

	__asm__("mfspr r6,18\n\t");
}
#endif /* __ppc__ || __ppc64__ */


/*
 *  Test a set of privileged instructions.  Returns:
 *    -2   if fork() failed.
 *    -1   if at least one test failed.  (TEST FAILURE) 
 *     0   if there were no tests for the compiled architecture.  (Not Tested.)
 *     n   where n (> 0) is the number of tests done.  (SUCCESS)
 */
int
amt_priv(int verbose)
{
	struct priv_test *ptp;
	int tcnt;

	for (tcnt = 0, ptp = &priv_tests[0]; ptp->pt_fp != NULL; ptp++) {
		if (verbose)
			printf("\tSubtest %d. %s (%s): ", tcnt + 1, 
			    ptp->pt_inst, ptp->pt_desc);
		if (fault_test(verbose, ptp->pt_fp, NULL, ptp->pt_result) == 1)
			tcnt++;
		else
			/* This test failed. No need to test more. */
			return (-1);
	}

	/* Return the number of tests done. */
	return (tcnt);
}
