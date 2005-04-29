/*
 * Copyright (c) 2001-2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _SCPREFERENCESPATHKEY_H
#define _SCPREFERENCESPATHKEY_H

#include <sys/cdefs.h>
#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>


/*!
	@header SCPreferencesPathKey
 */


__BEGIN_DECLS

/*
 * SCPreferencesPathKeyCreate*
 * - convenience routines that create a CFString key for an item in the store
 */

/*!
	@function SCPreferencesPathKeyCreate
	@discussion Creates a preferences path key using the given format.
 */
CFStringRef
SCPreferencesPathKeyCreate			(
						CFAllocatorRef	allocator,
						CFStringRef	fmt,
						...
						)				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
	@function SCPreferencesPathKeyCreateNetworkServices
 */
CFStringRef
SCPreferencesPathKeyCreateNetworkServices	(
						CFAllocatorRef	allocator
						)				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
	@function SCPreferencesPathKeyCreateNetworkServiceEntity
 */
CFStringRef
SCPreferencesPathKeyCreateNetworkServiceEntity	(
						CFAllocatorRef	allocator,
						CFStringRef	service,
						CFStringRef	entity
						)				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
	@function SCPreferencesPathKeyCreateSets
 */
CFStringRef
SCPreferencesPathKeyCreateSets			(
						CFAllocatorRef	allocator
						)				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
	@function SCPreferencesPathKeyCreateSet
 */
CFStringRef
SCPreferencesPathKeyCreateSet			(
						CFAllocatorRef	allocator,
						CFStringRef	set
						)				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
	@function SCPreferencesPathKeyCreateSetNetworkInterfaceEntity
 */
CFStringRef
SCPreferencesPathKeyCreateSetNetworkInterfaceEntity(
						   CFAllocatorRef	allocator,
						   CFStringRef	set,
						   CFStringRef	ifname,
						   CFStringRef	entity
						   )				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
	@function SCPreferencesPathKeyCreateSetNetworkGlobalEntity
 */
CFStringRef
SCPreferencesPathKeyCreateSetNetworkGlobalEntity(
						CFAllocatorRef	allocator,
						CFStringRef	set,
						CFStringRef	entity
						)				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
	@function SCPreferencesPathKeyCreateSetNetworkService
 */
CFStringRef
SCPreferencesPathKeyCreateSetNetworkService     (
						CFAllocatorRef	allocator,
						CFStringRef	set,
						CFStringRef	service
						)				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
	@function SCPreferencesPathKeyCreateSetNetworkServiceEntity
 */
CFStringRef
SCPreferencesPathKeyCreateSetNetworkServiceEntity(
						 CFAllocatorRef	allocator,
						 CFStringRef	set,
						 CFStringRef	service,
						 CFStringRef	entity
						 )				AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

__END_DECLS

#endif /* _SCPREFERENCESPATHKEY_H */
