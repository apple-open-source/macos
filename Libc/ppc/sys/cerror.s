/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 */

/* We use mode-independent "g" opcodes such as "srgi", and/or
 * mode-independent macros such as MI_GET_ADDRESS.  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

    .globl  _errno

#if 0
MI_ENTRY_POINT(cerror_cvt)
    MI_PUSH_STACK_FRAME
    MI_GET_ADDRESS(r12,_errno)
    cmplwi  r3,102		    /* EOPNOTSUPP? */
    bne     1f
    li	    r3,45		    /* Yes; make ENOTSUP for compatibility */
1:
    stw     r3,0(r12)               /* save syscall return code in global */
    MI_CALL_EXTERNAL(_cthread_set_errno_self)
    li      r3,-1                   /* then bug return value */
    li      r4,-1                   /* in case we're returning a long-long in 32-bit mode, etc */
    MI_POP_STACK_FRAME_AND_RETURN
#endif

MI_ENTRY_POINT(cerror_cvt)
    cmplwi  r3,102		    /* EOPNOTSUPP? */
    bne     1f
    li	    r3,45		    /* Yes; make ENOTSUP for compatibility */
1:
MI_ENTRY_POINT(cerror)
    MI_PUSH_STACK_FRAME
    MI_GET_ADDRESS(r12,_errno)
    stw     r3,0(r12)               /* save syscall return code in global */
    MI_CALL_EXTERNAL(_cthread_set_errno_self)
    li      r3,-1                   /* then bug return value */
    li      r4,-1                   /* in case we're returning a long-long in 32-bit mode, etc */
    MI_POP_STACK_FRAME_AND_RETURN
