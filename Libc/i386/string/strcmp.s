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
.text
.globl _strcmp
_strcmp:
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
