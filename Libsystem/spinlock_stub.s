/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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

#define __APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#if defined(__ppc__)
.data
.section __TEXT,__picsymbolstub1,symbol_stubs,pure_instructions,32
        .align 2
L__spin_lock$stub:
        .indirect_symbol __spin_lock
        ba      _COMM_PAGE_SPINLOCK_LOCK
        nop
        nop
        nop
        nop
        nop
        nop
        nop
.data
.lazy_symbol_pointer
L__spin_lock$lazy_ptr:
        .indirect_symbol __spin_lock
        .long dyld_stub_binding_helper
#elif defined(__i386__)
.data
.picsymbol_stub
L__spin_lock$stub:
        .indirect_symbol __spin_lock
        movl    $(_COMM_PAGE_SPINLOCK_LOCK), %eax
        jmp     %eax
        nop
        call    LPC$1
LPC$1:  popl    %eax
L__spin_lock$stub_binder:
        lea     L1$lz-LPC$1(%eax),%eax
        pushl   %eax
        jmp     dyld_stub_binding_helper
.data
.lazy_symbol_pointer
L1$lz:
        .indirect_symbol __spin_lock
        .long L__spin_lock$stub_binder
#endif
