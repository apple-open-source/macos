/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991  
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */

#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#undef	__APPLE_API_PRIVATE

#if defined(__ppc__)

#import	<architecture/ppc/asm_help.h>
#import	<architecture/ppc/pseudo_inst.h>

/* void spin_lock(int *p);
 *
 * Lock the lock pointed to by `p'.  Spin (possibly forever) until
 * the lock is available.  Test and test and set logic used.
 * These routines have moved to the comm page.
 */

.text
.align 2
LEAF(__spin_lock_try)
    ba		_COMM_PAGE_SPINLOCK_TRY
END(__spin_lock_try)

.globl _spin_lock
LEAF(__spin_lock)
_spin_lock:
    ba		_COMM_PAGE_SPINLOCK_LOCK
END(__spin_lock)

/* void spin_unlock(int *p);
 *
 *	Unlock the lock pointed to by p.
 */
.globl _spin_unlock
LEAF(__spin_unlock)
_spin_unlock:
    ba		_COMM_PAGE_SPINLOCK_UNLOCK
END(__spin_unlock)

#elif defined(__i386__)

#include <architecture/i386/asm_help.h>  

/*    
 * void
 * _spin_lock(p)
 *      int *p;
 *
 * Lock the lock pointed to by p.  Spin (possibly forever) until the next
 * lock is available.
 */
        TEXT
	ALIGN

.globl _spin_lock_try
LEAF(__spin_lock_try, 0)
_spin_lock_try:
	movl    $(_COMM_PAGE_SPINLOCK_TRY), %eax
	jmpl	%eax

	ALIGN

.globl _spin_lock
LEAF(__spin_lock, 0)
_spin_lock:
	movl    $(_COMM_PAGE_SPINLOCK_LOCK), %eax
	jmpl	%eax

/*
 * void
 * _spin_unlock(p)
 *      int *p;
 *
 * Unlock the lock pointed to by p.
 */
	ALIGN

.globl _spin_unlock
LEAF(__spin_unlock, 0)
_spin_unlock:
	movl    $(_COMM_PAGE_SPINLOCK_UNLOCK), %eax
	jmpl	%eax

#else
#error spin_locks not defined for this architecture
#endif
