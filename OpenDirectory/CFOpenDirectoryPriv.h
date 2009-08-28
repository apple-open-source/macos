/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
    @header		CFOpenDirectoryPriv
    @abstract   Private functions and constants
    @discussion Contains private functions and constants.  These functions can
				change or go away at any time as they are not public API.
*/


#ifndef __CFOPENDIRECTORYPRIV_H
#define __CFOPENDIRECTORYPRIV_H

#include <CFOpenDirectory/CFOpenDirectory.h>
#include <DirectoryService/DirServices.h>

/*!
    @const      kODSessionLocalPath
    @abstract   is the ability to open a targetted local path
    @discussion is the ability to open a targetted local path
*/
CF_EXPORT
CFStringRef kODSessionLocalPath;

/*!
    @const		kODAttributeTypeOperatingSystem
    @abstract   Returns the operating system type where the daemon is running
    @discussion Returns the operating system type where the daemon is running,
				e.g., Mac OS X or Mac OS X Server
*/
CF_EXPORT
const ODAttributeType kODAttributeTypeOperatingSystem;

/*!
	@const		kODAttributeTypeOperatingSystemVersion
	@abstract   Returns the operating system version where the daemon is running
	@discussion Returns the operating system version where the daemon is running,
				e.g., 10.6
 */
CF_EXPORT
const ODAttributeType kODAttributeTypeOperatingSystemVersion;

/*!
	@const		kODAttributeTypeAltSecurityIdentities
	@abstract	Used to store alternate identities for the record
	@discussion Used to store alternate identities for the record. Values will have standardized form as
				specified by Microsoft LDAP schema (1.2.840.113556.1.4.867).

 				Kerberos:user\@REALM
 */
CF_EXPORT
const ODAttributeType kODAttributeTypeAltSecurityIdentities;

/*!
    @const      kODAttributeTypeHardwareUUID
    @abstract   Used to store hardware UUID in string form
    @discussion Used to store hardware UUID in string form for a record.  Typically found in
                kODRecordTypeComputers.
*/
CF_EXPORT
const ODAttributeType kODAttributeTypeHardwareUUID;

__BEGIN_DECLS

/*!
    @function   ODNodeCreateWithDSRef
    @abstract   Private call to create an object from existing DS references
    @discussion Recovery if DS invalidates the references, or something fails outside of the plugins ability cannot
                be recovered for DSProxy tDirReference types.  Other types will re-open a DS Reference and re-open the
                node if the references become invalid.  That does mean the ODNode will "own" the reference at that point.  
                If the flag inCloseOnRelease is set to true, then the last release on ODNodeRef will cause the 
                tDirNodeReference and tDirReference to be closed, otherwise, it's up to the caller to close the references
                accordingly (except after the failure/reopen).
    @param      inAllocator the CFAllocatorRef to use
    @param      inDirRef the tDirReference to use.  An ODSessionRef will be created internally for this reference.
    @param      inNodeRef the existing tDirNodeReference to use.
    @param      inCloseOnRelease a bool true or false signifying whether the ODNodeRef and underlying ODSessionRef should
                close the references after the last release.
    @result     a valid ODNodeRef or NULL if failure occurs and ODGetError() can be called for detailed error
*/
CF_EXPORT
ODNodeRef ODNodeCreateWithDSRef( CFAllocatorRef inAllocator, tDirReference inDirRef, tDirNodeReference inNodeRef, 
                                 bool inCloseOnRelease );


/*!
    @function   ODSessionCreateWithDSRef
    @abstract   Private call to create an object from existing DS references
    @discussion Recovery if DS invalidates the references, or something fails outside of the plugins ability cannot
                be recovered for DSProxy tDirReference types.  If the flag inCloseOnRelease is set to true, then the last release 
                on ODSessionRef will cause the tDirReference to be closed, otherwise, it's up to the caller to close the references
                accordingly (except after the failure/reopen).
    @param      inAllocator the CFAllocatorRef to use
    @param      inDirRef the tDirReference to use.  An ODSessionRef will be created internally for this reference.
    @param      inCloseOnRelease a bool true or false signifying whether the ODNodeRef and underlying ODSessionRef should
                close the references after the last release.
    @result     a valid ODSessionRef or NULL if failure occurs and ODGetError() can be called for detailed error
*/
CF_EXPORT
ODSessionRef ODSessionCreateWithDSRef( CFAllocatorRef inAllocator, tDirReference inDirRef, bool inCloseOnRelease );

/*!
    @function   ODSessionGetDSRef
    @abstract   Returns the internal tDirReference used by the APIs
    @discussion This ref is only guaranteed as long as the ref is usable.  It should not be saved and only used with caution.
                This ref can be closed by the higher-level APIs if ODSessionRefs are deleted, become invalid, etc.
    @param      inSessionRef the ODSessionRef to extract the tDirReference from
    @result     the current tDirReference of the session
*/
CF_EXPORT
tDirReference ODSessionGetDSRef( ODSessionRef inSessionRef );

/*!
    @function   ODNodeGetDSRef
    @abstract   Returns the internal tDirNodeReference used by the APIs
    @discussion This ref is only guaranteed as long as the ref is usable.  It should not be saved and only used with caution.
                This ref can be closed by the higher-level APIs if ODNodeRefs are deleted, become invalid, etc.
    @param      inNodeRef the ODNodeRef to extract the tDirNodeReference from
    @result     the current tDirNodeReference of the node connection
*/
CF_EXPORT
tDirNodeReference ODNodeGetDSRef( ODNodeRef inNodeRef );

/*!
    @function   ODRecordGetDSRef
    @abstract   Returns the internal tRecordReference used by the APIs
    @discussion This ref is only guaranteed as long as the ref is usable.  It should not be saved and only used with caution.
                This ref can be closed by the higher-level APIs if ODRecordRefs are deleted, become invalid, etc
    @param      inRecord the ODRecorRef to extract the tRecordReference from
    @result     the current tRecordReference of the working record
*/
CF_EXPORT
tRecordReference ODRecordGetDSRef( ODRecordRef inRecord );

/*!
    @function	ODConvertToLegacyErrorCode
    @abstract   Converts error code to legacy error
    @discussion Converts the error code to legacy error code.
    @param      inCode is the error code returned by retrieving error code CFErrorGetCode() or [Error code]
    @result     the legacy style error code
*/
CF_EXPORT
tDirStatus ODConvertToLegacyErrorCode( CFIndex code );

/*!
    @function	ODRecordContainsMemberRefresh
	@abstract   Will use membership APIs to resolve group membership based on Group and Member record combination ignoring the cache
	@discussion Will use membership APIs to resolve group membership based on Group and Member record combination ignoring the cache.
				This API does not check attributes values directly, instead uses system APIs to deal with nested
				memberships.
	@param      inGroupRef an ODRecordRef of the group to be checked for membership
	@param      inMemberRef an ODRecordRef of the member to be checked against the group
	@param      outError an optional CFErrorRef reference for error details
	@result     returns true or false depending on result
*/
CF_EXPORT
bool ODRecordContainsMemberRefresh( ODRecordRef inGroupRef, ODRecordRef inMemberRef, CFErrorRef *outError );

__END_DECLS

#endif
