/*
 * Copyright (c) 2012 Apple Inc. All rights reserved.
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

#ifndef __PTHREAD_PRIVATE_H__
#define __PTHREAD_PRIVATE_H__

#include <sys/cdefs.h>
#include <Availability.h>
#include <pthread/tsd_private.h>

/* get the thread specific errno value */
__header_always_inline int
_pthread_get_errno_direct(void)
{
	return *(int*)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_ERRNO);
}

/* set the thread specific errno value */
__header_always_inline void
_pthread_set_errno_direct(int value)
{
	*((int*)_pthread_getspecific_direct(_PTHREAD_TSD_SLOT_ERRNO)) = value;
}

__OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0)
pthread_t pthread_main_thread_np(void);

struct _libpthread_functions {
	unsigned long version;
	void (*exit)(int); // added with version=1
	void *(*malloc)(size_t); // added with version=2
	void (*free)(void *); // added with version=2
};

#endif // __PTHREAD_PRIVATE_H__
