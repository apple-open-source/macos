/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#import "Authorization.h"


@implementation authorization

+ sharedInstance
{
    static id sharedTask = nil;
    if(sharedTask==nil)
    {
        sharedTask = [[self alloc]init];
    }
    return sharedTask;
}

//============================================================================
//	- (BOOL)isAuthenticated
//============================================================================
// Find out if the user has the appropriate authorization rights.
// This really needs to be called each time you need to know whether the user
// is authorized, since the AuthorizationRef can be invalidated elsewhere, or
// may expire after a short period of time.
// As far as I know, there is no way to be notified when your AuthorizationRef
// expires.
//
- (BOOL)isAuthenticated
{
    AuthorizationRights rights;
    AuthorizationRights *authorizedRights;
    AuthorizationFlags flags;
    AuthorizationItem items[1];
    char sysctlPath[] = "/usr/sbin/kmodstat";
    OSStatus err = 0;
    BOOL authorized = NO;

    if(authorizationRef==NULL)
    {
        //If we haven't created an AuthorizationRef yet, create one now with the
        //kAuthorizationFlagDefaults flags only to get the user's current
        //authorization rights.
        rights.count=0;
        rights.items = NULL;
        
        flags = kAuthorizationFlagDefaults;
    
        err = AuthorizationCreate(&rights,
                                kAuthorizationEmptyEnvironment, flags,
                                &authorizationRef);
        
    }
    
    //There should be one item in the AuthorizationItems array for each
    //type of right you want to acquire.
    //The data in the value and valueLength is dependant on which right you
    //want to acquire. For the right to execute tools as root,
    //kAuthorizationRightExecute, they should hold a pointer to a C string 
    //containing the path to the tool you want to execute, and 
    //the length of the C string path.
    //There needs to be one item for each tool you want to execute.
    items[0].name = kAuthorizationRightExecute;
    items[0].value = sysctlPath;
    items[0].valueLength = strlen(sysctlPath);
    items[0].flags = 0;

    rights.count=1;
    rights.items = items;
    
    flags = kAuthorizationFlagExtendRights;
    
    //Since we've specified kAuthorizationFlagExtendRights and
    //haven't specified kAuthorizationFlagInteractionAllowed, if the
    //user isn't currently authorized to execute tools as root,
    //they won't be asked for a password and err will indicate
    //an authorization failure.
    err = AuthorizationCopyRights(authorizationRef,&rights,
                        kAuthorizationEmptyEnvironment,
                        flags,&authorizedRights);

    authorized = (errAuthorizationSuccess==err);
    if(authorized)
    {
        //we don't need these items, and they need to be disposed of.
        AuthorizationFreeItemSet(authorizedRights);
    }
    return authorized;
}

//============================================================================
//	- (void)deauthenticate
//============================================================================
// Invalidate the user's AuthorizationRef to dispose of any acquired rights.
// It's a good idea to do this before you quit your application, else they
// will stay authorized as of the current time (4K78).
//
- (void)deauthenticate
{
    if(authorizationRef)
    {
        //dispose of any rights our AuthorizationRef has acquired, and null it out
        //so we get a new one next time we need one.
        AuthorizationFree(authorizationRef,kAuthorizationFlagDestroyRights);
        authorizationRef = NULL;
//////////////    //    [[NSNotificationCenter defaultCenter]postNotificationName:SysctlDeauthenticatedNotification
   ///////////                                                         object:self];
    }
}

//============================================================================
//	- (BOOL)fetchPassword
//============================================================================
// Fetch user's password if needed. If the user is already authorized, they
// will not be asked for their password again.
//
- (BOOL)fetchPassword
{
    AuthorizationRights rights;
    AuthorizationRights *authorizedRights;
    AuthorizationFlags flags;
    AuthorizationItem item[1];
    char sysctlPath[] = "/usr/sbin/sysctl";
    OSStatus err = 0;
    BOOL authorized = NO;

    item[0].name = kAuthorizationRightExecute;
    item[0].value = sysctlPath;
    item[0].valueLength = strlen(sysctlPath);
    item[0].flags = 0;
    
    rights.count=1;
    rights.items = item;
    
    flags = kAuthorizationFlagInteractionAllowed 
                | kAuthorizationFlagExtendRights;

    //Here, since we've specified kAuthorizationFlagExtendRights and
    //have also specified kAuthorizationFlagInteractionAllowed, if the
    //user isn't currently authorized to execute tools as root 
    //(kAuthorizationRightExecute),they will be asked for their password. 
    //The err return value will indicate authorization success or failure.
    err = AuthorizationCopyRights(authorizationRef,&rights,
                        kAuthorizationEmptyEnvironment,
                        flags,&authorizedRights);
    authorized = (errAuthorizationSuccess==err);
    if(authorized)
    {
        AuthorizationFreeItemSet(authorizedRights);
/////        [[NSNotificationCenter defaultCenter]postNotificationName:SysctlAuthenticatedNotification
     ///////////                                                   object:self];
    }                                                    
    return authorized;

}

//============================================================================
//	- (BOOL)authenticate
//============================================================================
// if the user is not already authorized, ask them to authenticate.
// return YES if the user is (or becomes) authorized.
//
- (BOOL)authenticate
{
    if(![self isAuthenticated])
    {
        [self fetchPassword];
    }
    return [self isAuthenticated];
}

//============================================================================
//	- (void)setValue:(NSString*)inValue forVariable:(NSString*)inVariable
//============================================================================
// Execute the sysctl tool with the given arguments using
// AuthorizationExecuteWithPrivileges.
// Attempt to authorize the user if necessary.
//
- (void)setValue:(NSString*)inValue forVariable:(NSString*)inVariable
{
    char* args[3];
    OSStatus err = 0;
    NSString* argValue = nil;

    if(![self authenticate])
        return;
        
    argValue = [NSString stringWithFormat:@"%@=%@",inVariable,inValue];    
    
    //the arguments parameter to AuthorizationExecuteWithPrivileges is
    //a NULL terminated array of C string pointers.
    args[0] = "-w";
    args[1]=(char*)[argValue cString];
    args[2]=NULL;

    err = AuthorizationExecuteWithPrivileges(authorizationRef,
                                            "/usr/sbin/sysctl",
                                            0, args, NULL);
    if(err!=0)
    {
        NSBeep();
        NSLog(@"Error %d in AuthorizationExecuteWithPrivileges",err);
    }
}

-(AuthorizationRef)returnAuthorizationRef
{
    return authorizationRef;
}


@end
