#ifndef _WEBDAV_AUTHENTICATION_H_INCLUDE
#define _WEBDAV_AUTHENTICATION_H_INCLUDE


/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 2000 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').	You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
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
   Localizable.strings, in the webdavfs.bproj subproject, has to be changed
   as well. */
#define WEBDAV_HEADER_KEY "WebDAV File System Authentication"
#define WEBDAV_URL_MSG_KEY "Enter Username and Password for URL: "
/* Note: The WEBDAV_REALM_MSG_KEY string must start with a newline so
   that it will start on the line after the URL and not on the same line. */
#define WEBDAV_REALM_MSG_KEY "\nRealm: "
#define WEBDAV_USERNAME_KEY "Username"
#define WEBDAV_PASSWORD_KEY "Password"
#define WEBDAV_KEYCHAIN_KEY "Add to Keychain"
#define WEBDAV_OK_KEY "OK"
#define WEBDAV_CANCEL_KEY "Cancel"

#define WEBDAV_BASIC_WARNING_HEADER_KEY "WebDAV File System Security Notice"
#define WEBDAV_BASIC_WARNING_MSG_KEY "You have been challenged by a WebDAV server which is not secure. If you continue and supply your username and password, they can be read while in transit."
#define WEBDAV_BASIC_WARNING_CONTINUE_KEY "Continue"

#define WEBDAV_LOCALIZATION_BUNDLE "/System/Library/CoreServices/webdavfs.bundle"
#define WEBDAV_LOCALIZATION_FILE "Localizable"

#define WEBDAV_AUTHENTICATION_TIMEOUT 300

/* functions */

extern int webdav_get_authentication(char *namebuff, int namebuff_size, char *passbuff,
	int passbuff_size,const char *urlStr, const char *realmStr, int level);

#endif
