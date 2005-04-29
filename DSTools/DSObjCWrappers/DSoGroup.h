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
 * @header DSoGroup
 */


#import <Foundation/Foundation.h>
#include <DirectoryService/DirectoryService.h>
#include <unistd.h>		// for gid_t

#import "DSoRecord.h"

@class DSoUser;

/*!
 * @class DSoGroup This class represents a record of standard type Group.
 *		It provides convenience methods for retrieving
 *		common types of information found in a group record.
 */
@interface DSoGroup : DSoRecord {

}

/*!
 * @method initInNode:name:
 * @abstract Create a new group in a node.
 * @discussion This method is to initalize a new group.
 *		It will create a record in the specified node
 *		of type kDSStdRecordTypeGroups.
 * @param inParent The node in which to create this record.
 * @param inName The name of the group record.
 */
- (id)initInNode:(DSoNode*)inParent name:(NSString*)inName;

/*!
 * @method getGid
 * @abstract Retrieve the gid number of this group.
 * @discussion This retrieves the GroupID, or gid number
 *		of this group.
 */
- (gid_t) getGid;

/*!
 * @method isMember:
 * @abstract Determine if a user is a member of this group.
 * @discussion This method will determine if the specified
 *		user is listed as a member of this group.
 * @param inUser The user to check.
 */
- (BOOL) isMember:(DSoUser*)inUser;

@end
