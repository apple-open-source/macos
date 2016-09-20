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

/* Purpose: This header defines the generic Translocator interface, implemented by the client and server,
    and the Translocator factory method to make a client or server object
 */

#ifndef SecTranslocateInterface_h
#define SecTranslocateInterface_h

#include <string>
#include <unistd.h>

#include "SecTranslocateShared.hpp"

namespace Security {
namespace SecTranslocate {

using namespace std;

#define SECTRANSLOCATE_XPC_SERVICE_NAME "com.apple.security.translocation"

class Translocator
{
public:
    virtual ~Translocator() {};
    virtual string translocatePathForUser(const TranslocationPath &originalPath, const string &destPath) = 0;
    virtual bool destroyTranslocatedPathForUser(const string &translocatedPath) = 0;
    virtual void appLaunchCheckin(pid_t pid) = 0;
};

Translocator* getTranslocator(bool isServer=false);

} //namespace SecTranslocate
} //namespace Security
#endif /* SecTranslocateInterface_h */
