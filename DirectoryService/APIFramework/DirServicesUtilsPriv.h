/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
 * @header DirServicesUtilsPriv
 */

#ifndef __DirServicesUtilsPriv_h__
#define	__DirServicesUtilsPriv_h__	1

// App

#include <stdarg.h>

#include <AvailabilityMacros.h>
#include <DirectoryService/DirServicesTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @function dsFillAuthBuffer
 * Pass in a list of buffer items and receive a constructed buffer.
 */
tDirStatus		dsFillAuthBuffer			(	tDataBufferPtr inOutAuthBuffer,
												unsigned long			inCount,
												unsigned long			inLen,
												const void *inData, ... )
AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
 * @function dsAppendAuthBuffer
 * Pass in a list of buffer items and receive a constructed buffer.
 */
tDirStatus		dsAppendAuthBuffer				(	tDataBufferPtr inOutAuthBuffer,
													unsigned long			inCount,
													unsigned long			inLen,
													const void *inData, ... )
AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
    @function dsAppendAuthBufferWithAuthorityAttribute
    @abstract   Inserts a user name with authentication authority data into
				an existing buffer.
    @discussion Use this function for authentication methods that contain user
				or authenticator names and the authentication authority attribute
				has already been retrieved.
    @param      inNodeRef a node reference for the record to parse
    @param      inRecordListBuffPtr the data returned from dsGetDataList
    @param      inAttributePtr an attribute with authentication authority data
    @param      inValueRef the reference for the kDSNAttrAuthenticationAuthority
							attribute.
    @param      inUserName the name of the user to authenticate
    @param      inOutAuthBuffer pass in a preallocated buffer, returns with
				the user data appended.
    @result    tDirStatus code
*/
tDirStatus	dsAppendAuthBufferWithAuthorityAttribute
												(	tDirNodeReference inNodeRef,
													tDataBufferPtr inRecordListBuffPtr,
													tAttributeEntryPtr inAttributePtr,
													tAttributeValueListRef inValueRef,
													const char *inUserName,
													tDataBufferPtr inOutAuthBuffer )
AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

/*!
    @function dsAppendAuthBufferWithAuthorityStrings
    @abstract   Inserts a user name with authentication authority data into
				an existing buffer.
    @discussion Use this function for authentication methods that contain user
				or authenticator names and the authentication authority attribute
				has already been retrieved.
    @param      inUserName the name of the user to authenticate
	@param		inAuthAuthority a NULL terminated array of C strings
    @param      inOutAuthBuffer pass in a preallocated buffer, returns with
				the user data appended.
    @result    tDirStatus code
*/

tDirStatus dsAppendAuthBufferWithAuthorityStrings
												(	const char *inUserName,
													const char *inAuthAuthority[],
													tDataBufferPtr inOutAuthBuffer )
AVAILABLE_MAC_OS_X_VERSION_10_4_AND_LATER;

#ifdef __cplusplus
}
#endif

#endif

