/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#ifndef __CFOPENDIRECTORYPRIV_H
#define __CFOPENDIRECTORYPRIV_H

#include <CFOpenDirectory/CFOpenDirectory.h>
#include <DirectoryService/DirServices.h>

/*!
    @const      kODSessionLocalPath
    @abstract   is the ability to open a targetted local path
    @discussion is the ability to open a targetted local path
*/
CF_EXPORT CFStringRef kODSessionLocalPath;

__BEGIN_DECLS

/*!
    @function   ODNodeCreateWithDSRef
    @abstract   Private call to create an object from existing DS references
    @discussion Recovery if DS invalidates the references, or something fails outside of the plugins ability cannot
                be recovered for DSProxy tDirReference types.  Other types will re-open a DS Reference and re-open the
                node if the references become invalid.  That does mean the ODNode will "own" the reference at that point.  
                If the flag inCloseOnRelease is set to TRUE, then the last release on ODNodeRef will cause the 
                tDirNodeReference and tDirReference to be closed, otherwise, it's up to the caller to close the references
                accordingly (except after the failure/reopen).
    @param      inAllocator the CFAllocatorRef to use
    @param      inDirRef the tDirReference to use.  An ODSessionRef will be created internally for this reference.
    @param      inNodeRef the existing tDirNodeReference to use.
    @param      inCloseOnRelease a Boolean TRUE or FALSE signifying whether the ODNodeRef and underlying ODSessionRef should
                close the references after the last release.
    @result     a valid ODNodeRef or NULL if failure occurs and ODGetError() can be called for detailed error
*/
CF_EXPORT
ODNodeRef ODNodeCreateWithDSRef( CFAllocatorRef inAllocator, tDirReference inDirRef, tDirNodeReference inNodeRef, 
                                 Boolean inCloseOnRelease );


/*!
    @function   ODSessionCreateWithDSRef
    @abstract   Private call to create an object from existing DS references
    @discussion Recovery if DS invalidates the references, or something fails outside of the plugins ability cannot
                be recovered for DSProxy tDirReference types.  If the flag inCloseOnRelease is set to TRUE, then the last release 
                on ODSessionRef will cause the tDirReference to be closed, otherwise, it's up to the caller to close the references
                accordingly (except after the failure/reopen).
    @param      inAllocator the CFAllocatorRef to use
    @param      inDirRef the tDirReference to use.  An ODSessionRef will be created internally for this reference.
    @param      inCloseOnRelease a Boolean TRUE or FALSE signifying whether the ODNodeRef and underlying ODSessionRef should
                close the references after the last release.
    @result     a valid ODSessionRef or NULL if failure occurs and ODGetError() can be called for detailed error
*/
CF_EXPORT
ODSessionRef ODSessionCreateWithDSRef( CFAllocatorRef inAllocator, tDirReference inDirRef, Boolean inCloseOnRelease );

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

__END_DECLS

#endif
