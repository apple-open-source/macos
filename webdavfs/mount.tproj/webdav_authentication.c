/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
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

#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <UNCUserNotification.h>

#include "webdav_authentication.h"
#include "webdav_authcache.h"

/*****************************************************************************/

static int webdav_basic_auth_warning(void)
{
	int						result;
	int						index;
	const char				*contents[11];	/* 11 contents fields including the 0 terminator */
	UNCUserNotificationRef	notification;
	unsigned				response;
	
	index = 0;
	contents[index++] = kUNCLocalizationPathKey;
	contents[index++] = WEBDAV_LOCALIZATION_BUNDLE;
	contents[index++] = kUNCAlertHeaderKey;
	contents[index++] = WEBDAV_BASIC_WARNING_HEADER_KEY;
	contents[index++] = kUNCAlertMessageKey;
	contents[index++] = WEBDAV_BASIC_WARNING_MSG_KEY;
	contents[index++] = kUNCDefaultButtonTitleKey;
	contents[index++] = WEBDAV_CANCEL_KEY;
	contents[index++] = kUNCAlternateButtonTitleKey;
	contents[index++] = WEBDAV_BASIC_WARNING_CONTINUE_KEY;
	contents[index++] = 0;

	notification = UNCUserNotificationCreate(WEBDAV_AUTHENTICATION_TIMEOUT,
		kUNCCautionAlertLevel, &result, contents);
	if ( result == 0 )
	{
		result = UNCUserNotificationReceiveResponse(notification, WEBDAV_AUTHENTICATION_TIMEOUT, &response);
		/* if the UNC notification timed out OR if the user hit Cancel (the default button) */
		if ( (result != 0) || (kUNCDefaultResponse == (response & 0x3)) )
		{
			result = -1;
		}
		else
		{
			result = 0;
		}

		UNCUserNotificationFree(notification);
	}

	return ( result );
}

/*****************************************************************************/

int webdav_get_authentication(namebuff, namebuff_size, passbuff, passbuff_size, urlStr, realmStr, level)
	char *namebuff;
	int namebuff_size;
	char *passbuff;
	int passbuff_size;
	const char *urlStr;
	const char *realmStr;
	int level;
{
	int error = 0, i;
	unsigned response = 0;
	UNCUserNotificationRef notification;
	const char *username;
	const char *password;

	/* *** KEYCHAIN is not defined except for testing purposes *** */
#ifdef KEYCHAIN
	const char *contents[19];
	int save_login = 0;
#else
	const char *contents[17];
#endif

	/* display the Basic authentication warning? */
	if ( level == kChallengeSecurityLevelBasic )
	{
		/* yes */
		error = webdav_basic_auth_warning();
	}

	if (!error)
	{
		i = 0;
		contents[i++] = kUNCLocalizationPathKey;
		contents[i++] = WEBDAV_LOCALIZATION_BUNDLE;
		contents[i++] = kUNCAlertHeaderKey;
		contents[i++] = WEBDAV_HEADER_KEY;
		contents[i++] = kUNCAlertMessageKey;
		contents[i++] = WEBDAV_URL_MSG_KEY;
		contents[i++] = kUNCAlertMessageKey;
		contents[i++] = urlStr;
		contents[i++] = kUNCAlertMessageKey;
		contents[i++] = WEBDAV_REALM_MSG_KEY;
		contents[i++] = kUNCAlertMessageKey;
		contents[i++] = realmStr;
		contents[i++] = kUNCTextFieldTitlesKey;
		contents[i++] = WEBDAV_USERNAME_KEY;
		contents[i++] = kUNCTextFieldTitlesKey;
		contents[i++] = WEBDAV_PASSWORD_KEY;
#ifdef KEYCHAIN	   
		/* *** add flag for saving login in keychain database *** */
		contents[i++] = kUNCCheckBoxTitlesKey;
		contents[i++] = WEBDAV_KEYCHAIN_KEY;
#endif
	
		contents[i++] = kUNCDefaultButtonTitleKey;
		contents[i++] = WEBDAV_OK_KEY;
		contents[i++] = kUNCAlternateButtonTitleKey;
		contents[i++] = WEBDAV_CANCEL_KEY;
		contents[i++] = 0;
	
		notification = UNCUserNotificationCreate(WEBDAV_AUTHENTICATION_TIMEOUT, UNCSecureTextField(1),
			&error, contents);
		if (!error)
		{
	
			/* if the UNC notification timed out OR ... */
			if (UNCUserNotificationReceiveResponse(notification, WEBDAV_AUTHENTICATION_TIMEOUT, &response) ||
				/* ... if the user	hit Cancel */
				(kUNCAlternateResponse == (response & 0x3)))
			{
				error = -1;
			}
			else
			{
				/* fill in save_login, username, and password */
				username = UNCUserNotificationGetResponseValue(notification, kUNCTextFieldValuesKey, 0);
				password = UNCUserNotificationGetResponseValue(notification, kUNCTextFieldValuesKey, 1);
	#ifdef KEYCHAIN
				save_login = (response & UNCCheckBoxChecked(0));
				printf("CheckBox is %s checked.\n", (save_login) ? "" : "NOT");
	#endif
	
				if (!username || !strlen(username))
				{
					bzero(namebuff, namebuff_size);
				}
				else
				{
					strncpy(namebuff, username, namebuff_size);
					namebuff[namebuff_size - 1] = '\0';/* in case it was too long */
				}
	
				if (!password || !strlen(password))
				{
					bzero(passbuff, passbuff_size);
				}
				else
				{
					strncpy(passbuff, password, passbuff_size);
					passbuff[passbuff_size - 1] = '\0';/* in case it was too long */
				}
			}
	
			UNCUserNotificationFree(notification);
		}
	}

	return (error);
}

/*****************************************************************************/
