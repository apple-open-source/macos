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

#ifndef _SCNETWORKSETTINGSMANAGER_H
#define _SCNETWORKSETTINGSMANAGER_H

#include <os/availability.h>
#include <TargetConditionals.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>


/*!
	@header SCNetworkSettingsManager.h
 */

/*
 * SCNetworkSettingsManager
 */

/*
 * System Configuration Network Settings (SCNS)
 */

#if	!TARGET_OS_IPHONE
#include <Security/Security.h>
#else	// !TARGET_OS_IPHONE
typedef const struct AuthorizationOpaqueRef *	AuthorizationRef;
#endif	// !TARGET_OS_IPHONE

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

__BEGIN_DECLS

typedef struct CF_BRIDGED_TYPE(id) __SCNSManager * SCNSManagerRef;
typedef struct CF_BRIDGED_TYPE(id) __SCNSService * SCNSServiceRef;

SCNSManagerRef
SCNSManagerCreate(CFStringRef label);

SCNSManagerRef
SCNSManagerCreateWithAuthorization(CFStringRef label,
				   AuthorizationRef __nullable authorization)
	API_AVAILABLE(macos(14.0), ios(17.0));	

void
SCNSManagerRefresh(SCNSManagerRef manager)
	API_AVAILABLE(macos(14.0), ios(17.0));	

typedef uint32_t SCNSManagerEventFlags;

typedef void (^SCNSManagerEventHandler)(SCNSManagerRef manager,
					SCNSManagerEventFlags event_flags);

Boolean
SCNSManagerSetEventHandler(SCNSManagerRef manager, dispatch_queue_t queue,
			   SCNSManagerEventHandler handler)
	API_AVAILABLE(macos(14.0), ios(17.0));	

Boolean
SCNSManagerApplyChanges(SCNSManagerRef manager)
	API_AVAILABLE(macos(14.0), ios(17.0));	

SCNSServiceRef __nullable
SCNSManagerCopyService(SCNSManagerRef manager,
		       SCNetworkInterfaceRef netif,
		       CFStringRef __nullable category_id,
		       CFStringRef __nullable category_value)
	API_AVAILABLE(macos(14.0), ios(17.0));	

SCNSServiceRef __nullable
SCNSManagerCreateService(SCNSManagerRef manager,
			 SCNetworkInterfaceRef netif,
			 CFStringRef __nullable category_id,
			 CFStringRef __nullable category_value)
	API_AVAILABLE(macos(14.0), ios(17.0));	

SCNSServiceRef __nullable
SCNSManagerCopyCurrentService(SCNSManagerRef manager,
			      SCNetworkInterfaceRef netif,
			      CFStringRef __nullable category_id)
		API_AVAILABLE(macos(14.0), ios(17.0));	


void
SCNSManagerRemoveService(SCNSManagerRef manager,
			 SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	

CFStringRef __nullable
SCNSServiceGetCategoryID(SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	

CFStringRef __nullable
SCNSServiceGetCategoryValue(SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	

SCNetworkInterfaceRef
SCNSServiceGetInterface(SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));

CFStringRef
SCNSServiceGetServiceID(SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	

CFStringRef
SCNSServiceGetName(SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	

CFDictionaryRef __nullable
SCNSServiceCopyProtocolEntity(SCNSServiceRef service,
			      CFStringRef entity_type)
	API_AVAILABLE(macos(14.0), ios(17.0));	

Boolean
SCNSServiceSetProtocolEntity(SCNSServiceRef service,
			     CFStringRef entity_type,
			     CFDictionaryRef __nullable entity)
	API_AVAILABLE(macos(14.0), ios(17.0));	

void
SCNSServiceUseDefaultProtocolEntities(SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	

CFDictionaryRef __nullable
SCNSServiceCopyActiveEntity(SCNSServiceRef service,
			    CFStringRef entity_type)
	API_AVAILABLE(macos(14.0), ios(17.0));	

void
SCNSServiceRefreshActiveState(SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));

Boolean
SCNSServiceSetQoSMarkingPolicy(SCNSServiceRef service,
			       CFDictionaryRef __nullable entity)
	API_AVAILABLE(macos(14.0), ios(17.0));

CFDictionaryRef __nullable
SCNSServiceCopyQoSMarkingPolicy(SCNSServiceRef service)
	API_AVAILABLE(macos(14.0), ios(17.0));	

__END_DECLS

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif	/* _SCNETWORKSETTINGSMANAGER_H */
