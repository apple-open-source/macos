/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

/*
 * Copyright (c) 2004 Suleiman Souhlal
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(__ppc__)

#include <sys/cdefs.h>
#include <sys/param.h>

#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ucontext.h>
#include <unistd.h>

void _ctx_done(ucontext_t *ucp);
void _ctx_start(void);

void
_ctx_done(ucontext_t *ucp)
{
	if (ucp->uc_link == NULL)
		exit(0);
	else {
		/* invalidate context */
		ucp->uc_mcsize = 0;

		setcontext((const ucontext_t *)ucp->uc_link);

		abort(); /* should never return from above call */
	}
}

void
makecontext(ucontext_t *ucp, void (*start)(void), int argc, ...)
{
	mcontext_t mc;
	char *sp;
	va_list ap;
	int i, regargs, stackargs;
	uint32_t args[8];

	/* Sanity checks */
	if ((ucp == NULL) || (argc < 0) || (argc > NCARGS)
	    || (ucp->uc_stack.ss_sp == NULL)
	    || (ucp->uc_stack.ss_size < 8192)) {
		/* invalidate context */
		ucp->uc_mcsize = 0;
		return;
	}

	/*
	 * The stack must have space for the frame pointer, saved
	 * link register, overflow arguments, and be 16-byte
	 * aligned.
	 */
	stackargs = (argc > 8) ? argc - 8 : 0;
	sp = (char *) ucp->uc_stack.ss_sp + ucp->uc_stack.ss_size
		- sizeof(uint32_t)*(stackargs + 2);
	sp = (char *)((uint32_t)sp & ~0x1f);

	mc = ucp->uc_mcontext;

	/*
	 * Up to 8 register args. Assumes all args are 32-bit and
	 * integer only. Not sure how to cater for floating point,
	 * although 64-bit args will work if aligned correctly
	 * in the arg list.
	 */
	regargs = (argc > 8) ? 8 : argc;
	va_start(ap, argc);
	for (i = 0; i < regargs; i++)
		args[i] = va_arg(ap, uint32_t);

	switch (regargs) {
		/*
		 * Hi Tom!
		 */
		case 8 : mc->ss.r10 = args[7];
		case 7 : mc->ss.r9  = args[6];
		case 6 : mc->ss.r8  = args[5];
		case 5 : mc->ss.r7  = args[4];
		case 4 : mc->ss.r6  = args[3];
		case 3 : mc->ss.r5  = args[2];
		case 2 : mc->ss.r4  = args[1];
		case 1 : mc->ss.r3  = args[0];
		default: break;
	}

	/*
	 * Overflow args go onto the stack
	 */
	if (argc > 8) {
		uint32_t *argp;

		/* Skip past frame pointer and saved LR */
		argp = (uint32_t *)sp + 2;

		for (i = 0; i < stackargs; i++)
			*argp++ = va_arg(ap, uint32_t);
	}
	va_end(ap);

	/*
	 * Use caller-saved regs 14/15 to hold params that _ctx_start
	 * will use to invoke the user-supplied func
	 */
	mc->ss.srr0 = (uint32_t) _ctx_start;
	mc->ss.r1   = (uint32_t) sp;	/* new stack pointer */
	mc->ss.r14  = (uint32_t) start;	/* r14 <- start */
	mc->ss.r15  = (uint32_t) ucp;	/* r15 <- ucp */
}

#endif /* __ppc__ */
