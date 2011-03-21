/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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

#ifndef __WEBDAVLIB_H__
#define __WEBDAVLIB_H__

#include <CoreFoundation/CoreFoundation.h>

enum WEBDAVLIBAuthStatus
{
	WEBDAVLIB_Success = 0,
	WEBDAVLIB_ProxyAuth = 1,
	WEBDAVLIB_ServerAuth = 2,
	WEBDAVLIB_UnexpectedStatus = 3,
	WEBDAVLIB_IOError = 4
};

//
// enum WEBDAVLIBAuthStatus queryForProxy(CFURLRef a_url, CFDictionaryRef proxyInfo, int *error)
//
// Sends and OPTIONS request to the server and determines if an http/https authenticating
// porxy server exists.  SSL connections are supported.
//
// Return Values:
//
// WEBDAVLIB_Success    - An OPTIONS reply was received without requiring credentials.
//						  The output arg 'error' is set to zero.
//
// WEBDAVLIB_ServerAuth - The server is requesting credentials (http status 401),
//                        no proxy server was encountered.
//						  The output arg 'error' is set to EAUTH.
//
// WEBDAVLIB_ProxyAuth  - An authenticating proxy server was found (http status 407).
//						  The output arg 'error' is set to EAUTH.
//
//                        The following keys are returned in the 'proxyInfo' dictionary:
//                        kWebDAVLibProxyServerNameKey
//                        kWebDAVLibProxyRealmKey
//                        kWebDAVLibProxyPortKey
//                        kWebDAVLibProxySchemeKey
//
// WEBDAVLIB_UnexpectedStatus  Received an unexpected http status (i.e. not 200, 401, or 407).
//							   The output arg 'error' is set to EIO.
//							   
// WEBDAVLIB_IOError	- This is a catch-all for anything else that went wrong.
//						  The output arg 'error' provides more detail.
//

// Keys defining a proxy server
#define kWebDAVLibProxyServerNameKey	CFSTR("ProxyServer")
#define kWebDAVLibProxyRealmKey			CFSTR("ProxyRealm")
#define kWebDAVLibProxyPortKey			CFSTR("ProxyPort")
#define kWebDAVLibProxySchemeKey		CFSTR("ProxyScheme")	// http or https

extern enum WEBDAVLIBAuthStatus queryForProxy(CFURLRef a_url, CFMutableDictionaryRef proxyInfo, int *error);


//
// enum WEBDAVLIBAuthStatus connectToServer(CFURLRef a_url, CFDictionaryRef creds, boolean_t requireSecureLogin, int *error)
//
// Test if a connection can be established with a server by sending an OPTIONS request,
// performing authentication when required.  SSL connections are supported.  This provides
// a simple way of testing credentials.
//
// A CFDictionary containing credentials (both server and proxy server) can be passed
// in the 'creds' input argument. The input keys are:
//
//			kWebDAVLibUserNameKey		(CFString)
//			kWebDAVLibPasswordKey		(CFString)
//			kWebDAVLibProxyUserNameKey	(CFString)
//			kWebDAVLibProxyPasswordKey	(CFString)
//
// If 'requireSecureLogin' is TRUE, then the connection will fail (EAUTH) if credentials cannot be
// sent securely (fails for Basic Authentication without an SSL connection).  In this case the
// return value is  WEBDAVLIB_ServerAuth or  WEBDAVLIB_ProxyAuth and 'error' is set to EAUTH.
//
// Return Values:
//
// WEBDAVLIB_Success    - Success. The connection attempt was successful.
//						  The output arg 'error' is set to zero.
//
// WEBDAVLIB_ServerAuth - The server requires authentication (http status 401). If server
//						  credentials were passed, then they were not accepted by the server.
//						  The output arg 'error' is set to EAUTH.
//
// WEBDAVLIB_ProxyAuth  - The proxy server requires authentication (http status 407). If proxy server
//						  credentials were passed, then they were not accepted by the proxy server.
//
// WEBDAVLIB_UnexpectedStatus  Received an unexpected http status (i.e. not 200, 401, or 407).
//							   The output arg 'error' is set to EIO.
//							   
// WEBDAVLIB_IOError	- This is a catch-all for anything else that went wrong.
//						  The output arg 'error' provides more detail.
//

// Keys for passing server and proxy server credentials
#define kWebDAVLibUserNameKey	CFSTR("UserName")
#define kWebDAVLibPasswordKey	CFSTR("Password")
#define kWebDAVLibProxyUserNameKey	CFSTR("ProxyUserName")
#define kWebDAVLibProxyPasswordKey	CFSTR("ProxyPassword")

extern enum WEBDAVLIBAuthStatus connectToServer(CFURLRef a_url, CFDictionaryRef creds, boolean_t requireSecureLogin, int *error);

#endif
