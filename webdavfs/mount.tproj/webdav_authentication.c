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
/*		@(#)webdav_authentication.c		 *
 *		(c) 2000   Apple Computer, Inc.	 All Rights Reserved
 *
 *
 *		webdav_authentication.c -- Routines for interacting with the user to get credentials
 *				(username,password, etc.)
 *
 *		MODIFICATION HISTORY:
 *				17-MAR-2000		Clark Warner	  File Creation
 */

#include <sys/syslog.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <UNCUserNotification.h>

#include "webdav_authentication.h"
#include "webdav_authcache.h"

/*****************************************************************************/

/* valid responses mask for UNC */
#define	VALID_RESPONSES_MASK (kUNCAlternateResponse | kUNCOtherResponse | kUNCCancelResponse)

/*****************************************************************************/

static void webdav_login_failed_warning(void)
{
	int						result;
	int						index;
	const char				*contents[11];	/* 9 contents fields including the 0 terminator */
	UNCUserNotificationRef	notification;
	unsigned				response;
	
	index = 0;
	contents[index++] = kUNCLocalizationPathKey;
	contents[index++] = WEBDAV_LOCALIZATION_BUNDLE;
	contents[index++] = kUNCIconPathKey;
	contents[index++] = WEBDAV_SERVER_ICON_PATH;
	contents[index++] = kUNCAlertHeaderKey;
	contents[index++] = WEBDAV_LOGIN_FAILED_HEADER_KEY;
	contents[index++] = kUNCAlertMessageKey;
	contents[index++] = WEBDAV_LOGIN_FAILED_MSG_KEY;
	contents[index++] = kUNCDefaultButtonTitleKey;
	contents[index++] = WEBDAV_OK_KEY;
	contents[index++] = 0;

	notification = UNCUserNotificationCreate(WEBDAV_AUTHENTICATION_TIMEOUT,
		kUNCStopAlertLevel, &result, contents);
	if ( result == 0 )
	{
		(void) UNCUserNotificationReceiveResponse(notification, WEBDAV_AUTHENTICATION_TIMEOUT, &response);

		UNCUserNotificationFree(notification);
	}
	else
	{
		syslog(LOG_ERR, "webdav_basic_auth_warning: UNCUserNotificationCreate() failed");
	}
}

/*****************************************************************************/

int webdav_get_authentication(char *namebuff, int namebuff_size, char *passbuff,
	int passbuff_size,const char *urlStr, const char *realmStr, unsigned int level,
	int *addtokeychain, int badlogin)
{
	int error = 0, index;
	unsigned response = 0;
	UNCUserNotificationRef notification;
	const char *username;
	const char *password;
	const char *contents[31];	/* 31 contents fields including the 0 terminator */

	*addtokeychain = 0;

	/* are we asking again because the name and password didn't work? */
	if ( badlogin )
	{
		/* tell them it didn't work */
		webdav_login_failed_warning();
	}
	
	index = 0;
	contents[index++] = kUNCLocalizationPathKey;
	contents[index++] = WEBDAV_LOCALIZATION_BUNDLE;
	contents[index++] = kUNCIconPathKey;
	contents[index++] = WEBDAV_SERVER_ICON_PATH;
	contents[index++] = kUNCAlertHeaderKey;
	contents[index++] = WEBDAV_AUTH_HEADER_KEY;
	contents[index++] = kUNCAlertMessageKey;
	contents[index++] = WEBDAV_AUTH_MSG_KEY1;
	contents[index++] = kUNCAlertMessageKey;
	contents[index++] = urlStr;
	contents[index++] = kUNCAlertMessageKey;
	contents[index++] = WEBDAV_AUTH_MSG_KEY2;
	contents[index++] = kUNCAlertMessageKey;
	contents[index++] = realmStr;
	contents[index++] = kUNCAlertMessageKey;
	contents[index++] = WEBDAV_AUTH_MSG_KEY3;
	contents[index++] = kUNCAlertMessageKey;
	contents[index++] = (level == kChallengeSecurityLevelBasic) ?
						WEBDAV_AUTH_MSG_INSECURE :
						WEBDAV_AUTH_MSG_SECURE;
	contents[index++] = kUNCTextFieldTitlesKey;
	contents[index++] = WEBDAV_USERNAME_KEY;
	contents[index++] = kUNCTextFieldTitlesKey;
	contents[index++] = WEBDAV_PASSWORD_KEY;
	if ( namebuff[0] )
	{
		contents[index++] = kUNCTextFieldValuesKey;
		contents[index++] = namebuff;
	}
	contents[index++] = kUNCCheckBoxTitlesKey;
	contents[index++] = WEBDAV_KEYCHAIN_KEY;
	contents[index++] = kUNCDefaultButtonTitleKey;
	contents[index++] = WEBDAV_OK_KEY;
	contents[index++] = kUNCAlternateButtonTitleKey;
	contents[index++] = WEBDAV_CANCEL_KEY;
	contents[index++] = 0;
	
	notification = UNCUserNotificationCreate(WEBDAV_AUTHENTICATION_TIMEOUT,
		UNCSecureTextField(1) + kUNCPlainAlertLevel, &error, contents);
	if (!error)
	{
		/* if the UNC notification timed out OR ... */
		if (UNCUserNotificationReceiveResponse(notification, WEBDAV_AUTHENTICATION_TIMEOUT, &response) ||
			/* ... if the user	hit Cancel */
			(kUNCAlternateResponse == (response & VALID_RESPONSES_MASK)))
		{
			error = -1;
		}
		else
		{
			/* fill in save_login, username, and password */
			username = UNCUserNotificationGetResponseValue(notification, kUNCTextFieldValuesKey, 0);
			password = UNCUserNotificationGetResponseValue(notification, kUNCTextFieldValuesKey, 1);
			*addtokeychain = (response & UNCCheckBoxChecked(0));
#ifdef DEBUG
			syslog(LOG_INFO,"Keychain checkBox is %s checked.\n", (*addtokeychain != 0) ? "" : "NOT");
#endif

			if (!username || !strlen(username))
			{
				bzero(namebuff, (size_t)namebuff_size);
			}
			else
			{
				strncpy(namebuff, username, (size_t)namebuff_size);
				namebuff[namebuff_size - 1] = '\0';/* in case it was too long */
			}

			if (!password || !strlen(password))
			{
				bzero(passbuff, (size_t)passbuff_size);
			}
			else
			{
				strncpy(passbuff, password, (size_t)passbuff_size);
				passbuff[passbuff_size - 1] = '\0';/* in case it was too long */
			}
		}

		UNCUserNotificationFree(notification);
	}
	else
	{
		syslog(LOG_ERR, "webdav_get_authentication: UNCUserNotificationCreate() failed");
	}

	return (error);
}

/*****************************************************************************/
