/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 * @header DSoException
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>

#import "DSoStatus.h"

/*!
 * @defined kDSoExceptionStatusKey
 * @discussion The NSString key for the status that is stored
 *		in the userInfo dictionary of the Exception.
 *		This value can also be retrieved through the -status
 *		method.
 */
#define kDSoExceptionStatusKey @"status"

/*!
 * @class DSoException This class sub-classes NSException, adding the ability
 *		to easily store and retrieve a DS tDirStatus status number in the NSException's
 *		userInfo dictionary.
 */
@interface DSoException : NSException {
    DSoStatus *_dsStat;
}

/*!
 * @method name:reason:status:
 * @abstract Create an autoreleased exception.
 * @discussion Create an autoreleased exception and initialize it
 *		with the specified name, reason, and Directory Services status.
 * @param name The name of the exception.
 * @param reason A String with the reason for the exception.
 * @param status The tDirStatus enumerated DS status value.
 */
+ (DSoException*) name:(NSString*)name reason:(NSString*)reason status:(tDirStatus)status DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method raiseWithStatus:
 * @abstract Create and raise a simple DS exception.
 * @discussion This method creates and raises a DS exception with
 *		the specified status and no name or reason.
 * @param inStatus The status to use for the excpeption.
 */
+ (void) raiseWithStatus:(tDirStatus)inStatus DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method status
 * @abstract Get the DS status number of the DS exception.
 */
- (tDirStatus) status DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method statusString
 * @abstract Get the enumerated string representation of the status number.
 * @result An NSString of the value.
 */
- (NSString*) statusString DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

/*!
 * @method statusCString
 * @abstract Get the enumerated string representation of the status number.
 * @result A null terminated c-string of the value.
 */
- (const char*) statusCString DEPRECATED_IN_MAC_OS_X_VERSION_10_6_AND_LATER;

@end
