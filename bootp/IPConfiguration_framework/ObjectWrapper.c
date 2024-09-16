/*
 * Copyright (c) 2011-2024 Apple Inc. All rights reserved.
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

#include <stdatomic.h>
#include "symbol_scope.h"
#include "cfutil.h"
#include "IPConfigurationLog.h"
#include "ObjectWrapper.h"

struct ObjectWrapper {
    const void *	obj;
    int32_t		retain_count;
};

PRIVATE_EXTERN const void *
ObjectWrapperRetain(const void * info)
{
#ifdef DEBUG
    int32_t		new_val;
#endif
    ObjectWrapperRef 	wrapper = (ObjectWrapperRef)info;

#ifdef DEBUG
    new_val =
#endif
    atomic_fetch_add_explicit((_Atomic int32_t *)&wrapper->retain_count, 1,
			      memory_order_relaxed);
#ifdef DEBUG
    /* Apparently, atomic_fetch_add_explicit() returns the old value,
     * not the new one. A +1 is needed here.
     */
    printf("wrapper retain (%d)\n", (new_val + 1));
#endif
    return (info);
}

PRIVATE_EXTERN const void *
ObjectWrapperGetObject(ObjectWrapperRef wrapper)
{
    return (wrapper->obj);
}

PRIVATE_EXTERN void
ObjectWrapperClearObject(ObjectWrapperRef wrapper)
{
    wrapper->obj = NULL;
}

PRIVATE_EXTERN ObjectWrapperRef
ObjectWrapperAlloc(const void * obj)
{
    ObjectWrapperRef	wrapper;

    wrapper = (ObjectWrapperRef)malloc(sizeof(*wrapper));
    wrapper->obj = obj;
    wrapper->retain_count = 1;
    return (wrapper);
}

PRIVATE_EXTERN void
ObjectWrapperRelease(const void * info)
{
    int32_t		new_val;
    ObjectWrapperRef 	wrapper = (ObjectWrapperRef)info;

    new_val = atomic_fetch_sub_explicit((_Atomic int32_t *)&wrapper->retain_count, 1,
					memory_order_relaxed) - 1;
#ifdef DEBUG
    printf("wrapper release (%d)\n", new_val);
#endif
    if (new_val == 0) {
#ifdef DEBUG
	printf("wrapper free\n");
#endif
	free(wrapper);
    }
    else if (new_val < 0) {
	IPConfigLogFL(LOG_NOTICE,
		      "IPConfigurationService: retain count already zero");
	abort();
    }
    return;
}
