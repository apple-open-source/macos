/*
 * Copyright (c) 2012 Apple Inc. All Rights Reserved.
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

/*!
	@header AuthorizationStatus
	The data types in AuthorizationStatus define the various states of Authorization for accessing services.
*/

/*!
	@typedef ABAuthorizationStatus
	@abstract Specifies the AuthorizationStatus result when accessing a service.
    @constant kABAuthorizationStatusNotDetermined indicates the user has not yet made a choice if this app can access the service.
	@constant kABAuthorizationStatusRestricted indicates the app is not authorized to use the service. Due to active restrictions on the service the user cannot change this status and may not have personally denied authorization.
	@constant kABAuthorizationStatusDenied indicates the user has explicitly denied authorization for this app or the service is disabled in Settings.
	@constant kABAuthorizationStatusAuthorized indicates the user has authorized this app to access the service. 
 */

typedef enum {
    kABAuthorizationStatusNotDetermined = 0,
    kABAuthorizationStatusRestricted,
    kABAuthorizationStatusDenied,
    kABAuthorizationStatusAuthorized
} ABAuthorizationStatus;

