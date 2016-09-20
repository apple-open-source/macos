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

/* Purpose: This header defines the Launch Services Notification Monitor for translocation */

#ifndef SecTranslocateLSNotification_hpp
#define SecTranslocateLSNotification_hpp

#include <security_utilities/cfutilities.h>
#include <CoreServices/CoreServicesPriv.h>
#include <unistd.h>
#include <dispatch/dispatch.h>

namespace Security {
namespace SecTranslocate {

class LSNotificationMonitor {
public:
    LSNotificationMonitor(dispatch_queue_t q); //throws
    void checkIn(pid_t pid);
    ~LSNotificationMonitor();
private:
    LSNotificationMonitor() = delete;
    LSNotificationMonitor(const LSNotificationMonitor& that) = delete;
    
    void asnDied(CFTypeRef data) const;
    static string stringIfTranslocated(CFStringRef appPath);
    
    dispatch_queue_t notificationQ;
    
};

} //namespace SecTranslocate
} //namespace Security

#endif /* SecTranslocateLSNotification_hpp */
