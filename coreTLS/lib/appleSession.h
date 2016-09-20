/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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

/*
 * appleSession.h - Session storage module.
 */

#ifndef	_APPLE_SESSION_H_
#define _APPLE_SESSION_H_

#include <tls_types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SessionCacheEntry SessionCacheEntry;

typedef struct SessionCache {
    SessionCacheEntry *head;
    time_t mTimeToLive;		/* default time-to-live in seconds */
} SessionCache;

void SessionCacheInit(SessionCache *cache);
void SessionCacheEmpty(SessionCache *cache);


int SessionCacheAddEntry(SessionCache *cache,
                         const tls_buffer *sessionKey,
                         const tls_buffer *sessionData,
                         time_t timeToLive);			/* optional time-to-live in seconds; 0 ==> default */

int SessionCacheLookupEntry(SessionCache *cache,
                            const tls_buffer *sessionKey,
                            tls_buffer *sessionData);

int SessionCacheDeleteEntry(SessionCache *cache,
                            const tls_buffer *sessionKey);

bool SessionCacheCleanup(SessionCache *cache);


#ifdef __cplusplus
}
#endif

#endif	/* _APPLE_SESSION_H_ */
