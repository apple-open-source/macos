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
/* Copyright 1997 Apple Computer, Inc.
**
** 10 June 1997 - Created by mwatson@apple.com
**
*/

#include <architecture/ppc/asm_help.h>

        .text
        .align 2
        .globl mcount
mcount:
        mflr r0
        stw r0,8(r1)
        stwu r1,-64(r1)
        mflr r4
        CALL_EXTERN(_moncount)
        addi r1,r1,64
        lwz r0,8(r1)
        mtlr r0
        blr

