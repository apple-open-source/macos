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

#ifndef _SCNETWORKCATEGORYMANAGERINTERNAL_H
#define _SCNETWORKCATEGORYMANAGERINTERNAL_H

#include <os/availability.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>

CF_IMPLICIT_BRIDGING_ENABLED
CF_ASSUME_NONNULL_BEGIN

/*!
	@header SCNetworkCategoryManagerInternal
 */

__BEGIN_DECLS

CFStringRef __nullable
__SCNetworkCategoryManagerCopyActiveValueNoSession(CFStringRef category,
						   SCNetworkInterfaceRef __nullable netif);

__END_DECLS

CF_ASSUME_NONNULL_END
CF_IMPLICIT_BRIDGING_DISABLED

#endif	/* _SCNETWORKCATEGORYMANAGERINTERNAL_H */
