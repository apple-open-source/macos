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
/* Copyright 1997 Apple Computer, Inc.
**
** 10 June 1997 - Created by mwatson@apple.com
**
*/

/* We use mode-independent "g" opcodes such as "srgi".  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

MI_ENTRY_POINT(mcount)
        mflr    r0
        stg     r0,SF_RETURN(r1)
        stgu    r1,-SF_MINSIZE(r1)
        mr      r4,r0               /* pass our return address as 2nd argument */
        MI_CALL_EXTERNAL(_moncount)
        addi    r1,r1,SF_MINSIZE
        lg      r0,SF_RETURN(r1)
        mtlr    r0
        blr

