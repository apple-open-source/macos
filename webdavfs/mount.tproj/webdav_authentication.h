#ifndef _WEBDAV_AUTHENTICATION_H_INCLUDE
#define _WEBDAV_AUTHENTICATION_H_INCLUDE


/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*		@(#)webdav_authentication.h		 *
 *		(c) 2000   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_authentication.h -- Headers for routines that interact with the user to get credentials
 *				(username,password, etc.)
 *
 *		MODIFICATION HISTORY:
 *				17-Mar-2000		Clark Warner	  File Creation
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/vnode.h>
#include <pthread.h>



/* Definitions */

/* Note that if any of these strings are changed, the corresponding key in
 * Localizable.strings, in the webdavfs.bproj subproject, has to be changed
 * as well.
 */

/*
 * Keys for the authentication dialog
 */
#define WEBDAV_AUTH_HEADER_KEY "WebDAV File System Authentication"
#define WEBDAV_AUTH_MSG_KEY1 "Enter your user name and password to access the server at the URL \""
#define WEBDAV_AUTH_MSG_KEY2 "\" in the realm \""
#define WEBDAV_AUTH_MSG_KEY3 "\".\n\n"
#define WEBDAV_AUTH_MSG_SECURE "Your name and password will be sent securely.\n"
#define WEBDAV_AUTH_MSG_INSECURE "Your name and password will not be sent securely.\n"
#define WEBDAV_USERNAME_KEY "Name"
#define WEBDAV_PASSWORD_KEY "Password"
#define WEBDAV_KEYCHAIN_KEY "Remember password (add to keychain)"
#define WEBDAV_OK_KEY "OK"
#define WEBDAV_CANCEL_KEY "Cancel"

/*
 * Keys for the bad name/password dialog
 */
#define WEBDAV_LOGIN_FAILED_HEADER_KEY "The user name or password you entered is not valid."
#define WEBDAV_LOGIN_FAILED_MSG_KEY "Please try again."

/*
 * Paths for the localization bundle and the generic server icon 
 */
#define WEBDAV_LOCALIZATION_BUNDLE "/System/Library/CoreServices/webdavfs.bundle"
#define WEBDAV_SERVER_ICON_PATH "/System/Library/CoreServices/SystemIcons.bundle/Contents/Resources/GenericFileServerIcon.icns"

/*
 * The amount of time to leave an authorization dialog up before auto-dismissing it
 */
#define WEBDAV_AUTHENTICATION_TIMEOUT 300.0

/* functions */

extern int webdav_get_authentication(char *namebuff, int namebuff_size, char *passbuff,
	int passbuff_size,const char *urlStr, const char *realmStr, unsigned int level,
	int *addtokeychain, int badlogin);

#endif
