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


#import "DSoGroup.h"

#import "DSoDirectory.h"
#import "DSoNode.h"
#import "DSoUser.h"
#import "DSoException.h"
#import "DSoRecordPriv.h"

@implementation DSoGroup

// ----------------------------------------------------------------------------
//	¥ DSGroup Public Class Methods
// ----------------------------------------------------------------------------
#pragma mark **** DSGroup Public Class Methods ****

- (id)initInNode:(DSoNode*)inParent recordRef:(tRecordReference)inRecRef
{
	[super initInNode:inParent recordRef:inRecRef];
	mType = [[NSString alloc] initWithCString:kDSStdRecordTypeGroups];
	return self;
}

- (id)initInNode:(DSoNode*)inParent name:(NSString*)inName
{
    return [super initInNode:inParent type:kDSStdRecordTypeGroups name:inName];
}

// ----------------------------------------------------------------------------
//	¥ DSGroup Public Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** DSGroup Public Instance Methods ****

- (gid_t) getGid
{
    NSString	   *sGid		= [self getAttribute:kDS1AttrPrimaryGroupID] ;

    NSScanner      *gidScanner  = [[NSScanner alloc] initWithString:sGid];
    long long		gid			= -1; //nogroup

	[gidScanner scanLongLong:&gid];
    [gidScanner release];
	return (gid_t)gid;
}

- (BOOL) isMember:(DSoUser*)inUser
{
#warning need to use memberd here, should not be checking if users is a direct member
    NSString				   *szpUser = nil;
	NSString				   *sValue  = nil;
    unsigned long               ulIndex = 1;

    if ([inUser getGid] == [self getGid])
        return YES;

    szpUser = [inUser getName];
    
    @try
    {
        while (1)
		{
            sValue = [self getAttribute:kDSNAttrGroupMembership index:ulIndex++];
            if ([sValue isEqualToString:szpUser])
                return YES;
        }
    } @catch( DSoException *exception ) {
        // just let the default return occur
    } @catch( NSException *exception ) {
        // if not a DSoException, throw it..
        @throw;
    }

    return NO;
}

@end
