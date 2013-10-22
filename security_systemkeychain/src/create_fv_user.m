/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
 *
 * create_fv_user.c
 */

#include "create_fv_user.h"

#include "readline.h"
#include "tokenadmin.h"
#include "TokenIDHelper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecKeychain.h>
#include <SecurityFoundation/FileVaultPriv.h>
#include <SecurityFoundation/SFAuthorization.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>

#if 0
#import <Admin/LoginPrefs.h>
#import <Admin/User.h>
#import <Admin/UserAdditions.h>
#import <Admin/AdminConst.h>
#import <Admin/Group.h>
#import <Admin/Authenticator.h>
#import <Admin/DSAuthenticator.h>
#else
#import <SystemAdministration/SystemAdministration.h>
#endif

static int do_create_fv_user(const char *userShortName, const char *userFullName, const char *kcpassword);
static BOOL verify_userFullName(const char *userFullName);
static BOOL verify_userUnixName(const char *userShortName);
static BOOL checkHomedirExistence();
static void _createUserAccount(NSArray *inCertificates);
static BOOL authorize_me();

NSString *mNewUserFullName;
NSString *mNewUserName;
NSString *mNewUserNameWarn;
NSString *mNewUserPassword;

// Fix build failure. Remove this when
//	<rdar://problem/4874550> Admin support functions for creation of token protected FileVault users
// is submitted
#ifndef kHomeDirectoryCertificates
#define kHomeDirectoryCertificates @"HomeDirectoryCertificates"
#endif

SFAuthorization *mAuthorization;
/*
	-p optional-keychain-password
	-u usershortname
	-l user-long-name
	-h hash-of-encryption-key
*/

int
create_fv_user(int argc, char * const *argv)
{
	char *userShortName = NULL;
	char *userFullName = NULL;
	char *kcpassword = NULL;
	char *encryptionHash = NULL;
	int ch, result = 0;
	while ((ch = getopt(argc, argv, "u:l:p:h:")) != -1)
	{
		switch  (ch)
		{
		case 'u':
			userShortName = optarg;
			break;
		case 'l':
			userFullName = optarg;
			break;
		case 'p':
			kcpassword = optarg;
			break;
		case 'h':
			encryptionHash = optarg;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		return 2;

	result = do_create_fv_user(userShortName, userFullName, kcpassword);

	return result;
}

static int do_create_fv_user(const char *userShortName, const char *userFullName, const char *kcpassword)
{
	OSStatus status;
	CFTypeRef identityOrArray = NULL;
	CFArrayRef wrappingCertificates = NULL;

	if (!authorize_me())
		return 1;
		
	printf("Connecting to writeconfig...\n");
	AdminAuthenticator *sharedAuthenticator = [AdminAuthenticator sharedAuthenticator];
	[sharedAuthenticator authenticateUsingAuthorization:mAuthorization];
	if (![sharedAuthenticator isAuthenticated])
	{
		sec_error("Unable to connect to ToolLiason");
		return 1;
	}
	printf("Connected\n");
	
	if (!SecFileVaultMasterPasswordEnabled(NULL))
	{
		sec_error("FileVault master password not enabled");
		return 1;
	}
		
	if (!verify_userFullName(userFullName))
		return 1;
	if (!verify_userUnixName(userShortName))
		return 1;
	if (!checkHomedirExistence())
		return 1;

	NS_DURING
		mNewUserPassword = [NSString stringWithUTF8String:kcpassword];
	NS_HANDLER
		NSLog(@"failed to convert string");
	NS_ENDHANDLER

	status = findEncryptionIdentities(&identityOrArray);
	if (status)// || !identityArray || CFArrayGetCount(identityArray)==0)
	{
		sec_perror("Error when searching for encryption identities", status);
		return status;
	}
	extractCertificatesFromIdentities(identityOrArray, &wrappingCertificates);
	if (!wrappingCertificates || CFArrayGetCount(wrappingCertificates)==0)
	{
		sec_error("Error processing encryption identities");
		return status;
	}
	
	// add code here to append the cert from the master fv password keychain
	
	sec_error("Creating user \"%s\" (%s)",userFullName,userShortName);
	// what do we do now??
	_createUserAccount((NSArray *)wrappingCertificates);

	return 0;
}

static BOOL authorize_me()
{
	const char *kRightName = "system.preferences.accounts";
	AuthorizationFlags flags = kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed; // XXX remove kAuthorizationFlagInteractionAllowed
	AuthorizationItem myAction = { kRightName, 0, 0, 0 };
	AuthorizationRights myRights = {1, &myAction};
//	AuthorizationEnvironment myEnvironment = {0,};

	printf("Authorizing right %s\n", kRightName);
	mAuthorization = [SFAuthorization authorizationWithFlags:flags
		rights:&myRights environment:kAuthorizationEmptyEnvironment]; //&myEnvironment

	if (!mAuthorization)
		sec_error("Unable to obtain authorization for %s", kRightName);

	return (mAuthorization!=0);
//	if (![myauth obtainWithRights:(const AuthorizationRights *)rights flags:(AuthorizationFlags)flags environment:(const AuthorizationEnvironment *)environment authorizedRights:(AuthorizationRights **)authorizedRights error:(NSError**)error;

//	[self setAuthenticator:[DSAuthenticator sharedDSAuthenticator]];
}

static BOOL verify_userFullName(const char *userFullName)
{
	// Verify user Full name
	// Return true if OK to use
	printf("Validating full name: %s\n", userFullName);

	if ((userFullName == 0) || (strlen(userFullName) == 0))
	{
		sec_error("User full name is required");
		return NO;
	}
	NS_DURING
		mNewUserFullName = [NSString stringWithUTF8String:userFullName];
	NS_HANDLER
		NSLog(@"failed to convert string");
	NS_ENDHANDLER

	if(([mNewUserFullName caseInsensitiveCompare:@"admin"] != NSOrderedSame) && 
		([ADMGroup findGroupByName:mNewUserFullName] != NULL))
	{
		sec_error("User full name is not available (a group by that name exists)");	//USERNAME_IS_NOT_UNIQUE_SHORT
		return NO;
	}

	if(![ADMUser isUserNameUnique:mNewUserFullName searchParent:NO])
	{
		sec_error("User full name is not unique");	//USERNAME_IS_NOT_UNIQUE_SHORT
		return NO;
	}
	return YES;
}
	
static BOOL verify_userUnixName(const char *userShortName)
{
	// Verify unix-user name
	// Return true if OK to use

	if ((userShortName == 0) || (strlen(userShortName) == 0))
	{
		sec_error("User short name is required");
		return NO;
	}
	
	printf("Validating short name: %s\n", userShortName);
	NS_DURING
		mNewUserName = [NSString stringWithUTF8String:userShortName];
	NS_HANDLER
		NSLog(@"failed to convert string");
	NS_ENDHANDLER

	if([mNewUserName isEqualToString:@"ftp"])
		return NO;

	if([mNewUserName isEqualToString:@"public"])
		return NO;
		
	if([mNewUserName length] == 0)
		return NO;

	if(![ADMUser isUserNameUnique:mNewUserName searchParent:NO])
	{
		sec_error("User short name is not unique");	//USERNAME_IS_NOT_UNIQUE_SHORT
		return NO;
	}

	if(![ADMUser isUnixNameValid:mNewUserName])
	{
		sec_error("User short name is not a valid unix name");	//UNIXNAME_IS_NOT_VALID
		return NO;
	}

	if(([mNewUserName caseInsensitiveCompare:@"admin"] != NSOrderedSame) && 
		([ADMGroup findGroupByName:mNewUserName] != NULL))
	{
		sec_error("User short name is not available (a group by that name exists)");	//USERNAME_IS_NOT_UNIQUE_SHORT
		return NO;
	}
	
	// Orig:  Fix for 3707901; Warn user if user's short name is admin
	// We do not permit user's short name to be "admin"
	if([mNewUserName caseInsensitiveCompare:@"admin"] == NSOrderedSame)
	{
		sec_error("User short name may not be \"admin\"");
		return NO;
	}
	
	return YES;
}

static BOOL checkHomedirExistence()
{
	// Check if home already exists - we disallow
	NSFileManager *		fm = [NSFileManager defaultManager];
	BOOL				directory;
			
	if([fm fileExistsAtPath:[@"/Users/" stringByAppendingPathComponent:mNewUserName] isDirectory:&directory])
	{
		sec_error("Home directory already exists");
		return NO;
	}
	
	return YES;
}

static void _createUserAccount(NSArray *inCertificates)
{
	// local version of [AddRecordController _createUserAccount:]

	printf("Creating new user account: %s\n", [mNewUserName UTF8String]);
	ADMUser *		newUser = [ADMUser newUser];
	NSDictionary *inParameters =[NSDictionary dictionaryWithObject:inCertificates 
		forKey:kHomeDirectoryCertificates];

	[newUser setName:mNewUserName];
	[newUser setHomeDirectory:[newUser defaultHomeDirectory]];
	
	[newUser setUserCanChangePassword:YES];
	[newUser setUserCanChangeHint:YES];
	[newUser setUserCanChangePicture:YES];
	[newUser setUserCanChangeFullName:YES];
	[newUser setFullName:mNewUserFullName];

	[newUser setHint:@""];
	[newUser setPassword:mNewUserPassword];

	[newUser setHomeDirEncrypted:YES];
	[newUser commitChanges];
//	[newUser createHomeDirectory];
	printf("Creating home directory...\n");
	if (![newUser createHomeDirectoryWithParameters:inParameters])
		sec_error("Failed to create home directory");

	[newUser setAdministrator:NO];

	[newUser createHTTPConfig];
	printf("New user account created and configured\n");
//	[Utilities syncAFPIfNeeded];
}
//>>>>>>>>>>>> don't forget to add the hash to "dsAttrTypeStandard:AuthenticationAuthority"
/*
	NSArray *forbiddenUserNames = [NSArray arrayWithObjects:
		@"admin", @"ftp", @"public", 
		nil];
tolower
- (NSUInteger)[forbiddenUserNames indexOfObject:(id)anObject;
NSString 
*/
