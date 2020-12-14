/*
 * Copyright (c) 2008 - 2010 Apple Inc. All rights reserved.
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

#include "memory.hpp"
#include "compiler.h"

#include <cstdlib>
#include <new>

namespace platform {

void
invoke_new_handler(void)
{
    std::new_handler handler;

    // Get the curent new_handler by double-swapping. If multiple threads
    // race over this, then we will end up aborting. Ce la vie.
    handler = std::set_new_handler(::abort);
    std::set_new_handler(handler);

    handler();
}

void *
allocate(
        void * buf,
        std::size_t nbytes)
{
retry:
    if (buf == NULL) {
        if (nbytes % platform::pagesize()) {
            buf = ::malloc(nbytes);
        } else {
            buf = ::valloc(nbytes);
        }
    } else {
        buf = ::realloc(buf, nbytes);
    }

    if (UNLIKELY(buf == NULL)) {
        invoke_new_handler();
        goto retry;
    }

    return buf;
}

} // namespace platform
/* vim: set ts=4 sw=4 tw=79 et cindent : */
