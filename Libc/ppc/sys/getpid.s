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
#include "SYS.h"
	.data
	EXPORT(__current_pid)
	.long 0

TEXT
LEAF(_getpid)
#if defined(__DYNAMIC__)
        mflr    r0
        bcl    20,31,1f
1:
        mflr    r5
        mtlr    r0
        addis   r5, r5, ha16(__current_pid - 1b)
        addi    r5, r5, lo16(__current_pid - 1b)
#else
	lis	r5,ha16(__current_pid)
	ori	r5,r5,lo16(__current_pid)
#endif
	lwz	r3,0(r5)		// get the cached pid
	cmpwi 	r3,0			// if positive,
	bgtlr+				// return it
	
        SYSCALL_NONAME(getpid, 0)

        lwarx	r4,0,r5			// see if we can cache it
	cmpwi	r4,0			// we cant if there are any
	bltlr-				// vforks in progress

	stwcx.	r3,0,r5			// ignore cache conflicts
	blr
END(_getpid)
