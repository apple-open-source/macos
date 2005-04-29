/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
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
/* Copyright (c) 1992 NeXT Computer, Inc.  All rights reserved.
 *
 *	File:	libc/ppc/sys/fork.s
 *
 * HISTORY
 * 18-Nov-92  Ben Fathi (benf@next.com)
 *	Created from M88K sources
 *
 * 11-Jan-92  Peter King (king@next.com)
 *	Created from M68K sources
 */

/* We use mode-independent "g" opcodes such as "srgi".  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

#include "SYS.h"


MI_ENTRY_POINT(_fork)
    MI_PUSH_STACK_FRAME
    
    MI_CALL_EXTERNAL(__cthread_fork_prepare)
    
#if defined(__DYNAMIC__)
    .cstring
LC1:
	.ascii	"__dyld_fork_prepare\0"
    .text
	.align 2
	mflr	r0
	bcl     20,31,1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC1-1b)
	addi	r3,r3,lo16(LC1-1b)
	addi 	r4,r1,SF_LOCAL1
	bl      __dyld_func_lookup
	lg      r3,SF_LOCAL1(r1)
	mtspr 	ctr,r3
	bctrl	
#endif

	li      r0,SYS_fork
	sc                      // do the fork
	b       Lbotch			// error return

	cmpwi	r4,0            // parent (r4==0) or child (r4==1) ?
	beq     Lparent         // parent, since r4==0

                            
/* Here if we are the child.  */

#if defined(__DYNAMIC__)
    .cstring
LC3:
	.ascii	"__dyld_fork_child\0"
    .text
	.align 2
	mflr	r0
	bcl     20,31,1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC3-1b)
	addi	r3,r3,lo16(LC3-1b)
	addi 	r4,r1,SF_LOCAL1
	bl      __dyld_func_lookup
	lg      r3,SF_LOCAL1(r1)
	mtspr 	ctr,r3
	bctrl	
#endif

    li      r9,0
    MI_GET_ADDRESS(r8,__current_pid)
    stw     r9,0(r8)            // clear cached pid in child
    
	MI_CALL_EXTERNAL(__cthread_fork_child)
    
#if defined(__DYNAMIC__)
    .cstring
LC4:
	.ascii	"__dyld_fork_child_final\0"
    .text
	.align 2
	mflr	r0
	bcl     20,31,1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC4-1b)
	addi	r3,r3,lo16(LC4-1b)
	addi 	r4,r1,SF_LOCAL1
	bl      __dyld_func_lookup
	lg      r3,SF_LOCAL1(r1)
	mtspr 	ctr,r3
	bctrl	
#endif

	li	r3,0        // flag for "we are the child"
	b	Lreturn


/* Here if we are the parent, with:
 *  r3 = child's pid
 */
Lparent:
	stg     r3,SF_LOCAL2(r1)	// save child pid in stack
    
#if defined(__DYNAMIC__)
	mflr	r0
	bcl     20,31,1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC2-1b)
	addi	r3,r3,lo16(LC2-1b)
	addi 	r4,r1,SF_LOCAL1
	bl      __dyld_func_lookup
	lg      r3,SF_LOCAL1(r1)
	mtspr 	ctr,r3
	bctrl		
#endif
    
    b       Lparent_return      // clean up and return child's pid


/* Here if the fork() syscall failed.  We're still the parent.  */

Lbotch:	

#if defined(__DYNAMIC__)
    .cstring
LC2:
	.ascii	"__dyld_fork_parent\0"
    .text
	.align 2
	stg     r3,SF_LOCAL2(r1)	// save error return in stack
	mflr	r0
	bcl     20,31,1f
1:	mflr	r3
	mtlr	r0
	addis	r3,r3,ha16(LC2-1b)
	addi	r3,r3,lo16(LC2-1b)
	addi 	r4,r1,SF_LOCAL1
	bl      __dyld_func_lookup
	lg      r3,SF_LOCAL1(r1)
	mtspr 	ctr,r3
	bctrl
	lg      r3,SF_LOCAL2(r1)    // restore error code
#endif

	MI_CALL_EXTERNAL(cerror)
    li      r3,-1               // get an error return code
	stg     r3,SF_LOCAL2(r1)	// save return code in stack
    
	/*
	 * We use cthread_fork_parent() to clean up after a fork error
	 * (unlock cthreads and mailloc packages) so the parent
	 * process can Malloc() after fork() errors without
	 * deadlocking.
	 */
     
Lparent_return:
	MI_CALL_EXTERNAL(__cthread_fork_parent)
	lg      r3,SF_LOCAL2(r1)    // return -1 on error, child's pid on success
    
Lreturn:
    MI_POP_STACK_FRAME_AND_RETURN

