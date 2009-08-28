/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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

#ifndef _WEBDAV_AUTHCACHE_H_INCLUDE
#define _WEBDAV_AUTHCACHE_H_INCLUDE

#include <sys/types.h>
#include <sys/queue.h>
#include <CoreServices/CoreServices.h>

/*****************************************************************************/

/*
 * Paths for the localization bundle and the generic server icon 
 */
#define WEBDAV_LOCALIZATION_BUNDLE "/System/Library/Filesystems/webdav.fs"
#define WEBDAV_SERVER_ICON_PATH "/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/GenericFileServerIcon.icns"

/*
 * The amount of time to leave an authorization dialog up before auto-dismissing it
 */
#define WEBDAV_AUTHENTICATION_TIMEOUT 300.0

/*****************************************************************************/

/*
 * Structure used to store context
 * while the mount is being authenticated.
 *
 */
struct authcache_request_ctx {
	UInt32 count;		/* tracks retries for a request */
	UInt32 generation;	/* the generation count of the cached entry */
};

typedef struct authcache_request_ctx AuthRequestContext;

int authcache_apply(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the request to apply authentication to */
	UInt32 statusCode,					/* -> the status code (401, 407), or 0 if no challenge */
	CFHTTPMessageRef response,			/* -> the response containing the challenge, or NULL if no challenge */
	UInt32 *generation);				/* <- the generation count of the cache entry */

int authcache_valid(
	uid_t uid,							/* -> uid of the user making the request */
	CFHTTPMessageRef request,			/* -> the message of the successful request */
	UInt32 generation);					/* -> the generation count of the cache entry */

int authcache_proxy_invalidate(void);

int authcache_init(
	char *username,				/* -> username to attempt to use on first server challenge, or NULL */
	char *password,				/* -> password to attempt to use on first server challenge, or NULL */
	char *proxy_username,		/* -> username to attempt to use on first proxy server challenge, or NULL */
	char *proxy_password,		/* -> password to attempt to use on first proxy server challenge, or NULL */
	char *domain);				/* -> account domain to attempt to use on first server challenge, or NULL */

/*****************************************************************************/

#endif
