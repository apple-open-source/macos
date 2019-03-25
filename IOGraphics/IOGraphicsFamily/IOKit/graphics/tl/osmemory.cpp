/*
 * Copyright (c) 2018- Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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

// Since IOKit doesn't support stl for good reason, we would like to
// cherry-pick the more useful c++11 and after template. In particular the
// smart pointer stuff. This file is an OSObject specialization of
// std::unique_ptr, std::shared_ptr and std::weak_ptr

#include <osmemory>

_IOG_START_NAMESPACE

// Shared and Weak object helper class
// class _OSSharedWeakCounter
_OSSharedWeakCounter::_OSSharedWeakCounter(OSObject* obj)
    : fObj(obj)
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++11-narrowing"
    // TODO(PR42165698), until I get the refcnt.h narrowing header bug fixed
    os_ref_init(&fUseCount, nullptr);
    os_ref_init(&fWeakCount, nullptr);
#pragma clang diagnostic pop
}

bool _OSSharedWeakCounter::retain_shared()
{
    return os_ref_retain_try(&fUseCount);
}

void _OSSharedWeakCounter::release_shared()
{
    if (0 == os_ref_release(&fUseCount)) {
        // Must be last statement before return as we may deallocate as a
        // result of calling the release_weak
        release_weak();
    }
}

long _OSSharedWeakCounter::count_shared() const
{
    return static_cast<long>(
            os_ref_get_count(const_cast<os_refcnt*>(&fUseCount)));
}

void _OSSharedWeakCounter::retain_weak()
{
    os_ref_retain(&fWeakCount);
}

void _OSSharedWeakCounter::release_weak()
{
    if (0 == os_ref_release(&fWeakCount))
        delete this;
}

_OSSharedWeakCounter::~_OSSharedWeakCounter()
{
    OSSafeReleaseNULL(fObj);
}

_IOG_END_NAMESPACE
