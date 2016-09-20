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

/* Purpose:
    This header defines the client side interface (xpc client for translocation)
 */

#ifndef SecTranslocateClient_hpp
#define SecTranslocateClient_hpp

#include <string>
#include <dispatch/dispatch.h>

#include "SecTranslocateInterface.hpp"
#include "SecTranslocateShared.hpp"

namespace Security {

namespace SecTranslocate {

using namespace std;

class TranslocatorClient: public Translocator
{
public:
    TranslocatorClient(dispatch_queue_t q);
    ~TranslocatorClient();

    string translocatePathForUser(const TranslocationPath &originalPath, const string &destPath) override;
    bool destroyTranslocatedPathForUser(const string &translocatedPath) override;
    void appLaunchCheckin(pid_t pid) override;

private:
    TranslocatorClient() = delete;
    TranslocatorClient(const TranslocatorClient &that) = delete;
    dispatch_queue_t syncQ;
    xpc_connection_t service;
};

} //namespace SecTranslocate
} //namespace Security

#endif /* SecTranslocateClient_hpp */
