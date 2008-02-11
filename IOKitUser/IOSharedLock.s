/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

// These functions have migrated to the comm page.

#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#undef	__APPLE_API_PRIVATE



#if defined (__ppc__) || defined(__ppc64__)

#define COMM_PAGE_BRANCH(_symbol_, _address_)	 \
	.align	4				@\
	.globl	_symbol_			@\
_symbol_:					@\
	ba	_address_

#elif defined (__i386__)

#define COMM_PAGE_BRANCH(_symbol_, _address_)	 \
	.align	4, 0x90				 ;\
	.globl	_symbol_			 ;\
_symbol_:					 ;\
        movl    $(_address_), %eax		 ;\
        jmpl    %eax

#elif defined (__x86_64__)

#define COMM_PAGE_BRANCH(_symbol_, _address_)	 ;\
	.align	4, 0x90				 ;\
	.globl	_symbol_			 ;\
_symbol_:					 ;\
        mov     $(_address_), %rax		 ;\
        jmp    *%rax

#elif defined (__arm__)
#warn PWC_TODO see radar 4273615
#else
#error architecture not supported
#endif

#ifdef COMM_PAGE_BRANCH
    	.text
	COMM_PAGE_BRANCH(_IOTrySpinLock, _COMM_PAGE_SPINLOCK_TRY)
	COMM_PAGE_BRANCH(_ev_try_lock,   _COMM_PAGE_SPINLOCK_TRY)
	COMM_PAGE_BRANCH(_IOSpinLock,    _COMM_PAGE_SPINLOCK_LOCK)
	COMM_PAGE_BRANCH(_ev_lock,       _COMM_PAGE_SPINLOCK_LOCK)
	COMM_PAGE_BRANCH(_IOSpinUnlock,  _COMM_PAGE_SPINLOCK_UNLOCK)
	COMM_PAGE_BRANCH(_ev_unlock,     _COMM_PAGE_SPINLOCK_UNLOCK)
#endif
