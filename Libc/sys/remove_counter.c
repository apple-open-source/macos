/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

#include <libkern/OSAtomic.h>

#if defined(__ppc64__) || defined(__i386__) || defined(__x86_64__)
static int64_t __remove_counter = 0;
#else
static int32_t __remove_counter = 0;
#endif

uint64_t
__get_remove_counter(void) {
#if defined(__ppc64__) || defined(__i386__) || defined(__x86_64__)
	return (uint64_t)OSAtomicAdd64Barrier(0, &__remove_counter);
#else
	return (uint64_t)OSAtomicAdd32Barrier(0, &__remove_counter);
#endif
}

__private_extern__ void
__inc_remove_counter(void)
{
#if defined(__ppc64__) || defined(__i386__) || defined(__x86_64__)
	(void)OSAtomicAdd64(1, &__remove_counter);
#else
	(void)OSAtomicAdd32(1, &__remove_counter);
#endif
}
