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
/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 */
#import	"sigcatch.h"
#import	<signal.h>
#import	<sys/signal.h>
#import	<ucontext.h>

/*
 * sigvec registers _sigtramp as the handler for any signal requiring
 * user-mode intervention.  All _sigtramp does is find the real handler,
 * calls it, then sigreturn's.
 *
 * Note that the kernel saves/restores all of our register state.
 */
#if defined(__DYNAMIC__)
int __in_sigtramp = 0;
#endif

/* These defn should match the kernel one */
#define UC_TRAD			1
#ifdef __ppc__
#define UC_TRAD64		20
#define UC_TRAD64_VEC		25
#define UC_FLAVOR		30
#define UC_FLAVOR_VEC		35
#define UC_FLAVOR64		40
#define UC_FLAVOR64_VEC		45
#define UC_DUAL			50
#define UC_DUAL_VEC		55

 /* The following are valid mcontext sizes */
#define UC_FLAVOR_SIZE ((PPC_THREAD_STATE_COUNT + PPC_EXCEPTION_STATE_COUNT + PPC_FLOAT_STATE_COUNT) * sizeof(int))

#define UC_FLAVOR_VEC_SIZE ((PPC_THREAD_STATE_COUNT + PPC_EXCEPTION_STATE_COUNT + PPC_FLOAT_STATE_COUNT + PPC_VECTOR_STATE_COUNT) * sizeof(int))

#define UC_FLAVOR64_SIZE ((PPC_THREAD_STATE64_COUNT + PPC_EXCEPTION_STATE64_COUNT + PPC_FLOAT_STATE_COUNT) * sizeof(int))

#define UC_FLAVOR64_VEC_SIZE ((PPC_THREAD_STATE64_COUNT + PPC_EXCEPTION_STATE64_COUNT + PPC_FLOAT_STATE_COUNT + PPC_VECTOR_STATE_COUNT) * sizeof(int))
#endif

#ifdef __ppc__
/* This routine will be replaced by an assembly soon */
static  int
restore64_state(mcontext_t mctx, mcontext64_t mctx64)
{
	if (mctx->ss.srr0 != (unsigned int)mctx64->ss.srr0)
		return(0);	
	if (mctx->ss.srr1 != (unsigned int)mctx64->ss.srr1)
		return(0);	
	if (mctx->ss.r0 != (unsigned int)mctx64->ss.r0)
		return(0);	
	if (mctx->ss.r1 != (unsigned int)mctx->ss.r1)
		return(0);	
	if (mctx->ss.r2 != (unsigned int)mctx->ss.r2)
		return(0);	
	if (mctx->ss.r3 != (unsigned int)mctx->ss.r3)
		return(0);	
	if (mctx->ss.r4 != (unsigned int)mctx->ss.r4)
		return(0);	
	if (mctx->ss.r5 != (unsigned int)mctx->ss.r5)
		return(0);	
	if (mctx->ss.r6 != (unsigned int)mctx->ss.r6)
		return(0);	
	if (mctx->ss.r7 != (unsigned int)mctx->ss.r7)
		return(0);	
	if (mctx->ss.r8 != (unsigned int)mctx->ss.r8)
		return(0);	
	if (mctx->ss.r9 != (unsigned int)mctx->ss.r9)
		return(0);	
	if (mctx->ss.r10 != (unsigned int)mctx->ss.r10)
		return(0);	
	if (mctx->ss.r11 != (unsigned int)mctx->ss.r11)
		return(0);	
	if (mctx->ss.r12 != (unsigned int)mctx->ss.r12)
		return(0);	
	if (mctx->ss.r13 != (unsigned int)mctx->ss.r13)
		return(0);	
	if (mctx->ss.r14 != (unsigned int)mctx->ss.r14)
		return(0);	
	if (mctx->ss.r15 != (unsigned int)mctx->ss.r15)
		return(0);	
	if (mctx->ss.r16 != (unsigned int)mctx->ss.r16)
		return(0);	
	if (mctx->ss.r17 != (unsigned int)mctx->ss.r17)
		return(0);	
	if (mctx->ss.r18 != (unsigned int)mctx->ss.r18)
		return(0);	
	if (mctx->ss.r19 != (unsigned int)mctx->ss.r19)
		return(0);	
	if (mctx->ss.r20 != (unsigned int)mctx->ss.r20)
		return(0);	
	if (mctx->ss.r21 != (unsigned int)mctx->ss.r21)
		return(0);	
	if (mctx->ss.r22 != (unsigned int)mctx64->ss.r22)
		return(0);	
	if (mctx->ss.r23 != (unsigned int)mctx64->ss.r23)
		return(0);	
	if (mctx->ss.r24 != (unsigned int)mctx64->ss.r24)
		return(0);	
	if (mctx->ss.r25 != (unsigned int)mctx64->ss.r25)
		return(0);	
	if (mctx->ss.r26 != (unsigned int)mctx64->ss.r26)
		return(0);	
	if (mctx->ss.r27 != (unsigned int)mctx64->ss.r27)
		return(0);	
	if (mctx->ss.r28 != (unsigned int)mctx64->ss.r28)
		return(0);	
	if (mctx->ss.r29 != (unsigned int)mctx64->ss.r29)
		return(0);	
	if (mctx->ss.r30 != (unsigned int)mctx64->ss.r30)
		return(0);	
	if (mctx->ss.r31 != (unsigned int)mctx64->ss.r31)
		return(0);	

	if (mctx->ss.cr != mctx64->ss.cr)
		return(0);	
	if (mctx->ss.xer != (unsigned int)mctx64->ss.xer)
		return(0);	
	if (mctx->ss.lr != (unsigned int)mctx64->ss.lr)
		return(0);	
	if (mctx->ss.ctr != (unsigned int)mctx64->ss.ctr)
		return(0);	

	return(1);

}

#endif 

void
_sigtramp(
	union __sigaction_u __sigaction_u,
	int 			sigstyle,
	int 			sig,
	siginfo_t		*sinfo,
	struct ucontext		*uctx
) {
#ifdef __ppc__
	int ctxstyle = UC_FLAVOR;
#endif
	mcontext_t mctx;
	mcontext64_t mctx64;

#if defined(__DYNAMIC__)
        __in_sigtramp++;
#endif
#ifndef __ppc__
	if (sigstyle == UC_TRAD)
        	sa_handler(sig);
#else /* __ppc__ */
	if ((sigstyle == UC_TRAD) || (sigstyle == UC_TRAD64) || (sigstyle == UC_TRAD64_VEC))
        	sa_handler(sig);

	else
        	sa_sigaction(sig, sinfo, uctx);
		
	 if ((sigstyle == UC_DUAL) || (sigstyle == UC_DUAL_VEC)) {
		mctx = uctx->uc_mcontext;
		mctx64 = (mcontext64_t)((char *)(uctx->uc_mcontext) + sizeof(struct mcontext));
		/* restore 64bit state ? */
		if (restore64_state(mctx, mctx64)) {
			uctx->uc_mcontext = (void *)mctx64;
			if (sigstyle == UC_DUAL)  {
				uctx->uc_mcsize = UC_FLAVOR64_SIZE;
				ctxstyle = UC_FLAVOR64;
			 } else {
				uctx->uc_mcsize = UC_FLAVOR64_VEC_SIZE;
				ctxstyle = UC_FLAVOR64_VEC;
			}
		} else {
			if (sigstyle == UC_DUAL) 
				ctxstyle = UC_FLAVOR;
			 else
				ctxstyle = UC_FLAVOR_VEC;
		}
	} else
		ctxstyle = sigstyle;
#endif /* __ppc__ */

#if defined(__DYNAMIC__)
        __in_sigtramp--;
#endif
#ifdef __ppc__
	{
        /* sigreturn(uctx, ctxstyle); */
	/* syscall (SYS_SIGRETURN, uctx, ctxstyle); */
	syscall (184, uctx, ctxstyle);
	}
#else
	sigreturn(uctx);
#endif /* __ppc__ */
}

