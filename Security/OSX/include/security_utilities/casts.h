/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#ifndef casts_h
#define casts_h

#include <stdexcept>
#include <security_utilities/debugging.h>
#include <syslog.h>

template<typename TSource, typename TResult>
static inline TResult int_cast(TSource value) {
    // TODO: if we're using C++11, we should do some static_asserts on the signedness of these types
    TResult result = static_cast<TResult>(value);

    if (static_cast<TSource>(result) != value) {
#ifndef NDEBUG
        syslog(LOG_ERR, "%s: casted value out of range", __PRETTY_FUNCTION__);
#endif
        secnotice("int_cast", "casted value out of range");
        throw std::out_of_range("int_cast: casted value out of range");
    }
    return result;
}

#endif /* casts_h */
