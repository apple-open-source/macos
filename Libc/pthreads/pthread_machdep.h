/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
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
 */
/*
 * MkLinux
 */

/* Machine-dependent definitions for pthread internals. */

#ifndef _POSIX_PTHREAD_MACHDEP_H
#define _POSIX_PTHREAD_MACHDEP_H

#ifdef __LP64__
#define _PTHREAD_TSD_OFFSET 0x60
#else
#define _PTHREAD_TSD_OFFSET 0x48
#endif  /* __LP64__ */

#ifndef __ASSEMBLER__

#include <System/machine/cpu_capabilities.h>

/*
** Define macros for inline pthread_getspecific() usage.
** We reserve a number of slots for Apple internal use.
** This number can grow dynamically, no need to fix it.
*/

/* This header contains pre defined thread specific keys */
/* 0 is used for pthread_self */
#define _PTHREAD_TSD_SLOT_PTHREAD_SELF		0
/* Keys 1- 9 for use by dyld, directly or indirectly */
#define _PTHREAD_TSD_SLOT_DYLD_1		1
#define _PTHREAD_TSD_SLOT_DYLD_2		2
#define _PTHREAD_TSD_SLOT_DYLD_3		3
#define _PTHREAD_TSD_RESERVED_SLOT_COUNT	4

/* Keys 10 - 29 are for Libc/Libsystem internal ussage */
/* used as __pthread_tsd_first + Num  */
#define __PTK_LIBC_LOCALE_KEY		10
#define __PTK_LIBC_TTYNAME_KEY		11
#define __PTK_LIBC_LOCALTIME_KEY	12
#define __PTK_LIBC_GMTIME_KEY		13


/* Keys 30-255 for Non Libsystem usage */
#define _PTHREAD_TSD_SLOT_OPENGL	30	/* backwards compat sake */
#define __PTK_FRAMEWORK_OPENGL_KEY	30

/*
** Define macros for inline pthread_getspecific() usage.
** We reserve a number of slots for Apple internal use.
** This number can grow dynamically, no need to fix it.
*/


#if defined(__cplusplus)
extern "C" {
#endif

extern void *pthread_getspecific(unsigned long);
int       pthread_key_init_np(int, void (*)(void *));

#if defined(__cplusplus)
}
#endif

typedef int pthread_lock_t;

inline static int
_pthread_has_direct_tsd(void)
{
#if defined(__ppc__)
	int *caps = (int *)_COMM_PAGE_CPU_CAPABILITIES;
	if (*caps & kFastThreadLocalStorage) {
		return 1;
	} else {
		return 0;
	}
#else
	return 1;
#endif
}
	
inline static void *
_pthread_getspecific_direct(unsigned long slot)
{
	void *ret;
#if defined(__OPTIMIZE__)
#if defined(__i386__) || defined(__x86_64__)
	asm volatile("mov %%gs:%P1, %0" : "=r" (ret) : "i" (slot * sizeof(void *) + _PTHREAD_TSD_OFFSET));
#elif defined(__ppc__)
	void **__pthread_tsd;
	asm volatile("mfspr %0, 259" : "=r" (__pthread_tsd));
	ret = __pthread_tsd[slot + (_PTHREAD_TSD_OFFSET / sizeof(void *))];
#elif defined(__ppc64__)
	register void **__pthread_tsd asm ("r13");
	ret = __pthread_tsd[slot + (_PTHREAD_TSD_OFFSET / sizeof(void *))];
#elif defined(__arm__)
	register void **__pthread_tsd asm ("r9");
	ret = __pthread_tsd[slot + (_PTHREAD_TSD_OFFSET / sizeof(void *))];
#else
#error no pthread_getspecific_direct implementation for this arch
#endif
#else /* ! __OPTIMIZATION__ */
	ret = pthread_getspecific(slot);
#endif
	return ret;
}

#define LOCK_INIT(l)	((l) = 0)
#define LOCK_INITIALIZER 0

#endif /* ! __ASSEMBLER__ */
#endif /* _POSIX_PTHREAD_MACHDEP_H */
