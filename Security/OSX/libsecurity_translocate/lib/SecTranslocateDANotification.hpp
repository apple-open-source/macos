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

/* Purpose: This header defines the Disk Arbitration monitor for translocation */

#ifndef SecTranslocateDANotification_hpp
#define SecTranslocateDANotification_hpp

#include <dispatch/dispatch.h>
#include <DiskArbitration/DiskArbitration.h>

namespace Security {
namespace SecTranslocate {
    
class DANotificationMonitor {
public:
    DANotificationMonitor(dispatch_queue_t q); //throws
    ~DANotificationMonitor();
private:
    DANotificationMonitor() = delete;
    DANotificationMonitor(const DANotificationMonitor& that) = delete;
    
    DASessionRef diskArbitrationSession;
};
    
} //namespace SecTranslocate
} //namespace Security


#endif /* SecTranslocateDANotification_hpp */
