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

#include <exception>
#include <dispatch/dispatch.h>

#include <security_utilities/unix++.h>
#include <security_utilities/logging.h>

#include "SecTranslocateInterface.hpp"
#include "SecTranslocateServer.hpp"
#include "SecTranslocateClient.hpp"

namespace Security {
namespace SecTranslocate {

using namespace std;

Translocator* getTranslocator(bool isServer)
{
    static dispatch_once_t initialized;
    static Translocator* me = NULL;
    static dispatch_queue_t q;
    __block exception_ptr exception(0);

    if(isServer && me)
    {
        Syslog::critical("SecTranslocate: getTranslocator, asked for server but previously intialized as client");
        UnixError::throwMe(EINVAL);
    }

    dispatch_once(&initialized, ^{
        try
        {
            q = dispatch_queue_create(isServer?"com.apple.security.translocate":"com.apple.security.translocate-client", DISPATCH_QUEUE_SERIAL);
            if(q == NULL)
            {
                Syslog::critical("SecTranslocate: getTranslocator, failed to create queue");
                UnixError::throwMe(ENOMEM);
            }

            if(isServer)
            {
                me = new TranslocatorServer(q);
            }
            else
            {
                me = new TranslocatorClient(q);
            }
        }
        catch (...)
        {
            Syslog::critical("SecTranslocate: error while creating Translocator");
            exception = current_exception();
        }
    });

    if (me == NULL)
    {
        if (exception)
        {
            rethrow_exception(exception); //we already logged in this case.
        }
        else
        {
            Syslog::critical("SecTranslocate: Translocator initialization failed");
            UnixError::throwMe(EINVAL);
        }
    }

    return me;
}

} //namespace SecTranslocate
} //namespace Security
