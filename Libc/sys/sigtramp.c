/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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

void
_sigtramp(
	union __sigaction_u __sigaction_u,
	int 			sigstyle,
	int 			sig,
	siginfo_t		*sinfo,
	struct ucontext		*uctx
) {

#if defined(__DYNAMIC__)
        __in_sigtramp++;
#endif
	if (sigstyle == 1)
        	sa_handler(sig);
#ifdef __ppc__
	else
        	sa_sigaction(sig, sinfo, uctx);
#endif /* __ppc__ */
		
#if defined(__DYNAMIC__)
        __in_sigtramp--;
#endif
        sigreturn(uctx);
}
