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


#import "DSoUser.h"

#import "DSoNode.h"
#import "DSoGroup.h"
#import "DSoBuffer.h"
#import "DSoDataNode.h"
#import "DSoException.h"
#import "DSoRecordPriv.h"

@implementation DSoUser
- (id)initInNode:(DSoNode*)inParent recordRef:(tRecordReference)inRecRef
{
	[super initInNode:inParent recordRef:inRecRef];
	mType = [[NSString alloc] initWithCString:kDSStdRecordTypeUsers];
	return self;
}

- (id)initInNode:(DSoNode*)inParent name:(NSString*)inName
{
    return [super initInNode:inParent type:kDSStdRecordTypeUsers name:inName];
}

// ----------------------------------------------------------------------------
//	¥ DSUser Public Instance Methods
// ----------------------------------------------------------------------------
#pragma mark **** DSUser Public Instance Methods ****
- (uid_t) getUid
{
	NSString	   *sUid		= [self getAttribute:kDS1AttrUniqueID] ;
    NSScanner      *uidScanner  = [[NSScanner alloc] initWithString:sUid];
    long long		uid;
    BOOL			success		= [uidScanner scanLongLong:&uid];
    
    [uidScanner release];
    if (success)
        return (uid_t)uid;
    else
        return (uid_t) -2 ; // nobody
}

- (gid_t) getGid
{
	NSString	   *sGid		= [self getAttribute:kDS1AttrPrimaryGroupID] ;
    NSScanner      *gidScanner  = [[NSScanner alloc] initWithString:sGid];
    long long		gid;
    BOOL			success		= [gidScanner scanLongLong:&gid];
    
    [gidScanner release];
    if (success)
        return (gid_t)gid;
    else
        return (gid_t) -1 ; // nogroup
}

- (tDirStatus) authenticate:(NSString*)inPassword
{
	return [mParent authenticateName:[self getName] withPassword:inPassword] ;
}

- (void) setPassword:(NSString*)inNewPassword
{
	NSArray    *nameAndPassword = [[NSArray alloc] initWithObjects:[self getName], inNewPassword, nil];
	tDirStatus  status			= [mParent authenticateWithBufferItems:nameAndPassword
									authType:kDSStdAuthSetPasswdAsRoot authOnly:NO];
	
	[nameAndPassword release];
	if(status != eDSNoErr)
		[DSoException raiseWithStatus:status];
}

- (void)changePassword:(NSString*)inOldPassword toNewPassword:(NSString*)inNewPassword
{
	NSArray    *oldAndNewPassword   = [[NSArray alloc] initWithObjects:[self getName], inOldPassword, inNewPassword, nil];
	tDirStatus  status				= [mParent authenticateWithBufferItems:oldAndNewPassword
                                                                  authType:kDSStdAuthChangePasswd authOnly:NO];
	
	[oldAndNewPassword release];
	if(status != eDSNoErr)
		[DSoException raiseWithStatus:status];
}

- (BOOL) isAdmin
{
    BOOL bAdmin = NO;
    
	if ([self getUid] == 0)
		return YES;
    
    @try
    {
        bAdmin = [[mParent adminGroup] isMember:self];
    } @catch( DSoException *exception ) { // only catching DSoExceptions, let NSException go..
        // If this is not a eDSRecordNotFound (meaning there is no admin group), 
        // rethrow the exception, letting default NO be the return for eDSRecordNotFound.

        if( [exception status] != eDSRecordNotFound ) {
            @throw;
        }
    }
    
	return bAdmin;
}


@end
