/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


/*
	File:		appleSession.h

	Contains:	Session storage module, Apple CDSA version. 

	Written by:	Doug Mitchell, based on Netscape SSLRef 3.0

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

/* 
 * This file replaces the caller-specified SSLAddSessionFunc,
 * SSLGetSessionFunc, and SSLDeleteSessionFunc callbacks in the 
 * original SSLRef 3.0.
 */
 
#ifndef	_APPLE_SESSION_H_
#define _APPLE_SESSION_H_

#ifndef	_SSL_H_
#include "ssl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern SSLErr sslAddSession (
	const SSLBuffer sessionKey, 
	const SSLBuffer sessionData);

extern SSLErr sslGetSession (
	const SSLBuffer sessionKey, 
	SSLBuffer *sessionData);

extern SSLErr sslDeleteSession (
	const SSLBuffer sessionKey);

extern SSLErr sslCleanupSession();

#ifdef __cplusplus
}
#endif

#endif	/* _APPLE_SESSION_H_ */
