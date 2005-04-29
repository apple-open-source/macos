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
 * @header DSoUser
 */


#import <Foundation/Foundation.h>
#import <DirectoryService/DirectoryService.h>
#import <unistd.h>		// for uid_t and gid_t

#import "DSoRecord.h"

/*!
 * @class DSoUser This class represents a record of standard type User.
 *		It provides convenience methods for retrieving
 *		common types of information found in a user record.
 */
@interface DSoUser : DSoRecord {

}

/*!
 * @method initInNode:name:
 * @abstract Create a new user in a node.
 * @discussion This method is to initalize a new user.
 *		It will create a record in the specified node
 *		of type kDSStdRecordTypeUsers.
 * @param inParent The node in which to create this record.
 * @param inName The name of the user record.
 */
- (id)initInNode:(DSoNode*)inParent name:(NSString*)inName;

/*!
 * @method getUid
 * @abstract Retrieve the uid number of the user.
 * @discussion This retrieves the UniqueID, or uid number
 *		of a user.
 */
- (uid_t) getUid;

/*!
 * @method getGid
 * @abstract Retrieve the primary gid number of the user.
 * @discussion This retrieves the GroupID, or gid number
 *		of the primary group of this user user.
 */
- (gid_t) getGid;

/*!
 * @method authenticate:
 * @abstract Check the user's password.
 * @discussion This takes a password and checks
 * 		it for authenticity.  It does not authenticate the user
 *		to its node.   See DSoNode's
 *		authenticateName:withPassword:authOnly:
 * @result Returns eDSNoErr if the password is correct,
 *		else the status of the authentication attempt.
 */
- (tDirStatus) authenticate:(NSString*)inPassword;

/*!
 * @method setPassword:
 * @abstract Set a new password for the user.
 * @discussion Takes a new password from an NSString and does a
 *		doDirNodeAuth() with the setpassword Auth Method.
 * @result Will raise an exception if it fails.
 */
- (void) setPassword:(NSString*)inNewPassword;

/*!
* @method changePassword:toNewPassword:
 * @abstract Change a user's password from one to another.
 * @discussion Takes the old and new password and does a
 *		doDirNodeAuth() with the changepassword Auth Method.
 * @result Will raise an exception if it fails.
 */
- (void)changePassword:(NSString*)inOldPassword toNewPassword:(NSString*)inNewPassword;

/*!
 * @method isAdmin
 * @abstract Determine if the user is an admin for this node.
 * @discussion This method determines if the user is
 *		in the member list of the admin group for the user's node.
 */
- (BOOL) isAdmin;

@end
