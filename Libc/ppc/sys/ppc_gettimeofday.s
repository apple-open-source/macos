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
/* Copyright 1998 Apple Computer, Inc. */

#include "SYS.h"

	.globl cerror
LEAF(___ppc_gettimeofday)
        li      r0,SYS_gettimeofday               
	mr	r12,r3				
        sc                                      
        b       1f                              
        b       2f                              
1:      BRANCH_EXTERN(cerror)                   
2:     
	mr.	r12,r12
	beq	3f
	stw	r3,0(r12)
	stw	r4,4(r12)
	li	r3,0
3:
	blr

