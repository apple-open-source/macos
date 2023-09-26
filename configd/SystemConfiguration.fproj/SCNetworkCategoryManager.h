/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#ifndef _SCNETWORKCATEGORYMANAGER_H
#define _SCNETWORKCATEGORYMANAGER_H

#include <os/availability.h>
#include <TargetConditionals.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <IOKit/IOKitLib.h>
#include <SystemConfiguration/SCNetworkCategoryTypes.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

/*!
	@header SCNetworkCategoryManager
 */

__BEGIN_DECLS

typedef CF_ENUM(uint32_t, SCNetworkCategoryManagerFlags) {
	kSCNetworkCategoryManagerFlagsNone = 0x0,
	kSCNetworkCategoryManagerFlagsKeepConfigured = 0x1,
};

typedef struct CF_BRIDGED_TYPE(id)
	__SCNetworkCategoryManager * SCNetworkCategoryManagerRef;

CFTypeID
SCNetworkCategoryManagerGetTypeID(void)
	API_AVAILABLE(macos(14.0), ios(17.0));

typedef void (^SCNetworkCategoryManagerNotify)(CFStringRef value);

SCNetworkCategoryManagerRef __nullable
SCNetworkCategoryManagerCreateWithInterface(CFStringRef category,
					    SCNetworkInterfaceRef netif,
					    SCNetworkCategoryManagerFlags flags,
					    CFDictionaryRef __nullable options)
	API_AVAILABLE(macos(14.0), ios(17.0));

void
SCNetworkCategoryManagerSetNotifyHandler(SCNetworkCategoryManagerRef manager,
					 dispatch_queue_t __nullable queue,
					 SCNetworkCategoryManagerNotify __nullable notify)
	API_AVAILABLE(macos(14.0), ios(17.0));

Boolean
SCNetworkCategoryManagerActivateValue(SCNetworkCategoryManagerRef manager,
				      CFStringRef __nullable value)
	API_AVAILABLE(macos(14.0), ios(17.0));

__END_DECLS

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif	/* _SCNETWORKCATEGORYMANAGER_H */
