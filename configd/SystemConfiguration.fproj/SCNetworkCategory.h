/*
 * Copyright (c) 2022-2023 Apple Inc. All rights reserved.
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

#ifndef _SCNETWORKCATEGORY_H
#define _SCNETWORKCATEGORY_H

#include <os/availability.h>
#include <TargetConditionals.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCNetworkCategoryTypes.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

/*!
	@header SCNetworkCategory
 */

__BEGIN_DECLS

typedef struct CF_BRIDGED_TYPE(id) __SCNetworkCategory * SCNetworkCategoryRef;


CFTypeID
SCNetworkCategoryGetTypeID(void)
	API_AVAILABLE(macos(14.0), ios(17.0));	

CFArrayRef __nullable /* of SCNetworkCategoryRef */
SCNetworkCategoryCopyAll(SCPreferencesRef prefs)
	API_AVAILABLE(macos(14.0), ios(17.0));	

SCNetworkCategoryRef __nullable
SCNetworkCategoryCreate(SCPreferencesRef prefs,
			CFStringRef category)
	API_AVAILABLE(macos(14.0), ios(17.0));	

Boolean
SCNetworkCategoryAddService(SCNetworkCategoryRef category,
			    CFStringRef value,
			    SCNetworkServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	

Boolean
SCNetworkCategoryRemoveService(SCNetworkCategoryRef category,
			       CFStringRef value,
			       SCNetworkServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	


CFArrayRef __nullable /* of SCNetworkServiceRef */
SCNetworkCategoryCopyServices(SCNetworkCategoryRef category,
			      CFStringRef value)
	API_AVAILABLE(macos(14.0), ios(17.0));	

CFArrayRef __nullable /* of CFStringRef category value */
SCNetworkCategoryCopyValues(SCNetworkCategoryRef category)
	API_AVAILABLE(macos(14.0), ios(17.0));	

Boolean
SCNetworkCategorySetServiceQoSMarkingPolicy(SCNetworkCategoryRef category,
					    CFStringRef value,
					    SCNetworkServiceRef service,
					    CFDictionaryRef __nullable entity)
	API_AVAILABLE(macos(14.0), ios(17.0));

CFDictionaryRef __nullable
SCNetworkCategoryGetServiceQoSMarkingPolicy(SCNetworkCategoryRef category,
					    CFStringRef value,
					    SCNetworkServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));

__END_DECLS

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif	/* _SCNETWORKCATEGORY_H */
