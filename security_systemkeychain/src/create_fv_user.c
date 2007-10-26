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

#include "keychain_utilities.h"
#include "readline.h"
#include "security.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecKeychain.h>

#include <Admin/LoginPrefs.h>

static int
do_lock_all(void)
{
	OSStatus result = SecKeychainLockAll();
    if (result)
        sec_perror("SecKeychainLockAll", result);

	return result;
}

static int
do_lock(const char *keychainName)
{
	SecKeychainRef keychain = NULL;
	OSStatus result;

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto loser;
		}
	}

	result = SecKeychainLock(keychain);
	if (result)
	{
		sec_error("SecKeychainLock %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
	}

loser:
	if (keychain)
		CFRelease(keychain);

	return result;
}

int
create_fv_user(int argc, char * const *argv)
{
	char *keychainName = NULL;
	int ch, result = 0;
	Boolean lockAll = FALSE;
	while ((ch = getopt(argc, argv, "ah")) != -1)
	{
		switch  (ch)
		{
		case 'a':
			lockAll = TRUE;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 1 && !lockAll)
	{
		keychainName = argv[0];
		if (*keychainName == '\0')
		{
			result = 2;
			goto loser;
		}
	}
	else if (argc != 0)
		return 2;

	if (lockAll)
		result = do_lock_all();
	else
		result = do_lock(keychainName);

loser:

	return result;
}


	//
	// Verify user Full name
	//
	if([[mNewUserFullName stringValue] length] == 0)
	{
		// NSRunAlertPanel(LOCSTRING(@"USERNAME_IS_EMPTY"), LOCSTRING(@"USERNAME_IS_EMPTY_DESC"), LOCSTRING(@"OK"), NULL, NULL);
		[mNewUserFullNameWarn setStringValue:LOCSTRING(@"USERNAME_IS_EMPTY_SHORT")];
		[self _showWarningForField:mNewUserFullName];
		return;
	}
	
	{
		if(([[mNewUserFullName stringValue] caseInsensitiveCompare:@"admin"] != NSOrderedSame) && ([Group findGroupByName:[mNewUserFullName stringValue]] != NULL))
		{
			// NSRunAlertPanel([NSString stringWithFormat:LOCSTRING(@"USERNAME_IS_NOT_AVAILABLE"), [mNewUserFullName stringValue]], LOCSTRING(@"USERNAME_IS_NOT_AVAILABLE_DESC"), LOCSTRING(@"OK"), NULL, NULL);
			[mNewUserFullNameWarn setStringValue:LOCSTRING(@"USERNAME_IS_NOT_AVAILABLE_SHORT")];
			[self _showWarningForField:mNewUserFullName];
			return;
		}

		if(![User isUserNameUnique:[mNewUserFullName stringValue] searchParent:NO])
		{
			// NSRunAlertPanel([NSString stringWithFormat:LOCSTRING(@"USERNAME_IS_NOT_UNIQUE"), [mNewUserFullName stringValue]], LOCSTRING(@"USERNAME_IS_NOT_UNIQUE_DESC"), LOCSTRING(@"OK"), NULL, NULL);
			[mNewUserFullNameWarn setStringValue:LOCSTRING(@"USERNAME_IS_NOT_UNIQUE_SHORT")];
			[self _showWarningForField:mNewUserFullName];
			return;
		}
	}
	
	
	//
	// Verify unix-user name
	//
	if([[mNewUserName stringValue] length] == 0))
	{
		// HACK to have the same behavior when user presses "Enter" and clicked "Save"
		// right after entering full name (without even leaving the field)
		[mNewUserName setStringValue:[User generateUnixNameUsingString:[mNewUserFullName stringValue]]];
		[[mNewUserName window] display];
	}

	if([[mNewUserName stringValue] isEqualToString:@"ftp"])
	{
		// NSRunAlertPanel(LOCSTRING(@"UNIXNAME_IS_PUBLIC"), LOCSTRING(@"UNIXNAME_IS_PUBLIC_DESC"), LOCSTRING(@"OK"), NULL, NULL);
		[mNewUserNameWarn setStringValue:LOCSTRING(@"UNIXNAME_IS_FTP_SHORT")];
		[self _showWarningForField:mNewUserName];
		return;
	}

	if([[mNewUserName stringValue] isEqualToString:@"public"])
	{
		// NSRunAlertPanel(LOCSTRING(@"UNIXNAME_IS_PUBLIC"), LOCSTRING(@"UNIXNAME_IS_PUBLIC_DESC"), LOCSTRING(@"OK"), NULL, NULL);
		[mNewUserNameWarn setStringValue:LOCSTRING(@"UNIXNAME_IS_PUBLIC_SHORT")];
		[self _showWarningForField:mNewUserName];
		return;
	}
		
	if([[mNewUserName stringValue] length] == 0)
	{
		// NSRunAlertPanel(LOCSTRING(@"UNIXNAME_IS_EMPTY"), LOCSTRING(@"UNIXNAME_IS_EMPTY_DESC"), LOCSTRING(@"OK"), NULL, NULL);
		[mNewUserNameWarn setStringValue:LOCSTRING(@"UNIXNAME_IS_EMPTY_SHORT")];
		[self _showWarningForField:mNewUserName];
		return;
	}

	if(![User isUserNameUnique:[mNewUserName stringValue] searchParent:NO])
	{
		// NSRunAlertPanel([NSString stringWithFormat:LOCSTRING(@"UNIXNAME_IS_NOT_UNIQUE"), [mNewUserName stringValue]], LOCSTRING(@"UNIXNAME_IS_NOT_UNIQUE_DESC"), LOCSTRING(@"OK"), NULL, NULL);
		[mNewUserNameWarn setStringValue:LOCSTRING(@"UNIXNAME_IS_NOT_UNIQUE_SHORT")];
		[self _showWarningForField:mNewUserName];
		return;
	}

	if(![User isUnixNameValid:[mNewUserName stringValue]])
	{
		// NSRunAlertPanel([NSString stringWithFormat:LOCSTRING(@"UNIXNAME_IS_NOT_VALID"), [mNewUserName stringValue]], LOCSTRING(@"UNIXNAME_IS_NOT_VALID_DESC"), LOCSTRING(@"OK"), NULL, NULL);
		[mNewUserNameWarn setStringValue:LOCSTRING(@"UNIXNAME_IS_NOT_VALID_SHORT")];
		[self _showWarningForField:mNewUserName];
		return;
	}

	if(([[mNewUserName stringValue] caseInsensitiveCompare:@"admin"] != NSOrderedSame) && ([Group findGroupByName:[mNewUserName stringValue]] != NULL))
	{
		// NSRunAlertPanel([NSString stringWithFormat:LOCSTRING(@"USERNAME_IS_NOT_AVAILABLE"), [mNewUserName stringValue]], LOCSTRING(@"USERNAME_IS_NOT_AVAILABLE_DESC"), LOCSTRING(@"OK"), NULL, NULL);
		[mNewUserNameWarn setStringValue:LOCSTRING(@"USERNAME_IS_NOT_AVAILABLE_SHORT")];
		[self _showWarningForField:mNewUserName];
		return;
	}
	
	//
	// Verify Password
	//
	if(![[mNewUserPassword stringValue] isEqualToString:[mNewUserPasswordVerify stringValue]])
		{
			// NSRunAlertPanel(LOCSTRING(@"PASS_VERIFY_ERR"), LOCSTRING(@"PASS_VERIFY_ERR_DESC"), LOCSTRING(@"OK"), NULL, NULL);				
			[mNewUserPassword setStringValue:@""];
			[mNewUserPasswordVerify setStringValue:@""];
			[mNewUserPasswordWarn setStringValue:LOCSTRING(@"PASS_VERIFY_ERR_SHORT")];
			[self _showWarningForField:mNewUserPassword];
			return;
		}
		
		// Warn about empty passowrd
		if(![[mNewUserPassword stringValue] length])
		{
			if(NSRunAlertPanel(LOCSTRING(@"PASS_IS_EMPTY_WARN"), LOCSTRING(@"PASS_IS_EMPTY_WARN_DESC"), LOCSTRING(@"CANCEL"), LOCSTRING(@"OK"), NULL) == NSOKButton)
			{
				[[mNewUserPassword window] makeFirstResponder:mNewUserPassword];
				return;
			}
		}
	
	// Fix for 3707901
	// Warn user if user's short name is admin
	if([[mNewUserName stringValue] caseInsensitiveCompare:@"admin"] == NSOrderedSame)
	{
		if(NSRunAlertPanel(LOCSTRING(@"USERNAME_IS_ADMIN"), LOCSTRING(@"USERNAME_IS_ADMIN_DESCR"), LOCSTRING(@"OK"), LOCSTRING(@"CANCEL"), NULL) != NSOKButton)
		{
			[[mNewUserName window] makeFirstResponder:mNewUserName];
			return;
		}
		
		[mNewUserAdmin setState:NSOnState];
	}
	
	// Check if home already exists
	{
		NSFileManager *		fm = [NSFileManager defaultManager];
		BOOL				directory;
		NSString *			username = [mNewUserName stringValue];
		
		if([fm fileExistsAtPath:[@"/Users/" stringByAppendingPathComponent:username] isDirectory:&directory])
		{
			if(directory)
			{
				if(NSRunAlertPanel([NSString stringWithFormat:LOCSTRING(@"HOME_EXISTS_WARN"), username], LOCSTRING(@"HOME_EXISTS_WARN_DESCR"), LOCSTRING(@"CANCEL"), LOCSTRING(@"OK"), NULL) != NSCancelButton) return;
			}
			else
			{
				NSRunAlertPanel([NSString stringWithFormat:LOCSTRING(@"HOME_EXISTS_ERR"), username], LOCSTRING(@"HOME_EXISTS_ERR_DESCR"), LOCSTRING(@"OK"), NULL, NULL);
				return;
			}
		}
	}
	
	if([mNewUserIsFV state] == NSOnState && (!SecFileVaultMasterPasswordEnabled(NULL)))
	{
		[mMasterPassword setStringValue:@""];
		[mMasterPasswordVerify setStringValue:@""];
		[mMasterPasswordHint setStringValue:@""];
		[mMasterPasswordWarn setStringValue:@""];
		[mNewUserWarningSign setHidden:YES];

		[self _setContentView:mMasterPasswordView displayAndAnimate:YES];
		[mNewUserSheet performSelector:@selector(makeFirstResponder:) withObject:mMasterPassword afterDelay:0.1];
	}
	
	[[NSApplication sharedApplication] endSheet:[inSender window] returnCode:NSOKButton];
	[[inSender window] orderOut:[inSender window]];
