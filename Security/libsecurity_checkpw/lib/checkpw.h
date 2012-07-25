/*
 * Copyright (c) 2000-2010 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

#ifndef __CHKUSRNAMPASSWD_H__
#define __CHKUSRNAMPASSWD_H__

#include <Availability.h>

#include <pwd.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	CHECKPW_SUCCESS = 0,
	CHECKPW_UNKNOWNUSER = -1,
	CHECKPW_BADPASSWORD = -2,
	CHECKPW_FAILURE = -3
};

/*!
	@function checkpw

	checks a username/password combination.

	@param username (input) username as a UTF8 string
	@param password (input) password as a UTF8 string

	@result CHECKPW_SUCCESS username/password correct
	CHECKPW_UNKNOWNUSER no such user
	CHECKPW_BADPASSWORD wrong password
	CHECKPW_FAILURE failed to communicate with DirectoryServices

	@discussion Deprecated and should no longer be used.
	Username/password combinations can be checked in two ways:
	1) PAM(3): with the "checkpw" service.
	2) OpenDirectory: ODRecordVerifyPassword() - if you are
           currently using OpenDirectory.
*/

int checkpw( const char* userName, const char* password )
	__OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_1,__MAC_10_7,__IPHONE_NA,__IPHONE_NA);

#ifdef __cplusplus
}
#endif

#endif // __CHKUSRNAMPASSWD_H__
