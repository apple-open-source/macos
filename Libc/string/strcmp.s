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
.text
.globl _strcmp
_strcmp:
#if defined(__i386__)
        movl    0x04(%esp),%eax
        movl    0x08(%esp),%edx
        jmp     L2                      /* Jump into the loop! */

        .align  2,0x90
L1:     incl    %eax
        incl    %edx
L2:     movb    (%eax),%cl
        testb   %cl,%cl                 /* null terminator??? */
        jz      L3
        cmpb    %cl,(%edx)              /* chars match??? */
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        jz      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        jz      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        jz      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        jz      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        jz      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        jz      L3
        cmpb    %cl,(%edx)
        jne     L3
        incl    %eax
        incl    %edx
        movb    (%eax),%cl
        testb   %cl,%cl
        jz      L3
        cmpb    %cl,(%edx)
        je      L1
        .align 2, 0x90
L3:     movzbl  (%eax),%eax             /* unsigned comparison */
        movzbl  (%edx),%edx
        subl    %edx,%eax
        ret
#elif defined(__ppc__)
	mr	r5,r3
1:	lbz	r3,0(r5)
	addi	r5,r5,1
	cmpwi	cr1,r3,0
	lbz	r0,0(r4)
	addi	r4,r4,1
	subf.	r3,r0,r3
	beqlr+	cr1
	beq-	1b
	blr
#else
#error strcmp is not defined for this architecture
#endif
