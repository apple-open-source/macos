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


#if defined (__ppc__)

	.text

	.align	4
	.globl	_IOTrySpinLock
_IOTrySpinLock:
	ba	_COMM_PAGE_SPINLOCK_TRY

	.align	4
	.globl _ev_try_lock
_ev_try_lock:
	ba	_COMM_PAGE_SPINLOCK_TRY

	.align	4
	.globl _IOSpinLock
_IOSpinLock:
	ba 	_COMM_PAGE_SPINLOCK_LOCK

	.align	4
	.globl _ev_lock
_ev_lock:
	ba 	_COMM_PAGE_SPINLOCK_LOCK

	.align	4
	.globl _IOSpinUnlock
_IOSpinUnlock:
	ba	_COMM_PAGE_SPINLOCK_UNLOCK

	.align	4
	.globl _ev_unlock
_ev_unlock:
	ba	_COMM_PAGE_SPINLOCK_UNLOCK

#elif defined (__i386__)

	.text

	.align 4, 0x90
	.globl _IOTrySpinLock
_IOTrySpinLock:
        movl    $(_COMM_PAGE_SPINLOCK_TRY), %eax
        jmpl    %eax

	.align 4, 0x90
	.globl _ev_try_lock
_ev_try_lock:
        movl    $(_COMM_PAGE_SPINLOCK_TRY), %eax
        jmpl    %eax

	.align 4, 0x90
	.globl _IOSpinLock
_IOSpinLock:
        movl    $(_COMM_PAGE_SPINLOCK_LOCK), %eax
        jmpl    %eax

	.align 4, 0x90
	.globl _ev_lock
_ev_lock:
        movl    $(_COMM_PAGE_SPINLOCK_LOCK), %eax
        jmpl    %eax

	.align 4, 0x90
	.globl _IOSpinUnlock
_IOSpinUnlock:
        movl    $(_COMM_PAGE_SPINLOCK_UNLOCK), %eax
        jmpl    %eax

	.align 4, 0x90
	.globl _ev_unlock
_ev_unlock:
        movl    $(_COMM_PAGE_SPINLOCK_UNLOCK), %eax
        jmpl    %eax

#else
#error architecture not supported
#endif
