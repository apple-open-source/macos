/*
 * Copyright (c) 2009-2010 Apple Inc. All Rights Reserved.
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

/*!
    @header asynchttp
    The functions provided in asynchttp.h provide an interface to
    an asynchronous http get/post engine.
*/

#ifndef _SECURITYD_ASYNCHTTP_H_
#define _SECURITYD_ASYNCHTTP_H_

#include <stdbool.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFDate.h>
#include <CFNetwork/CFHTTPMessage.h>
#include <CFNetwork/CFHTTPStream.h>
#include <dispatch/dispatch.h>

__BEGIN_DECLS

typedef struct asynchttp_s {
    void(*completed)(struct asynchttp_s *http, CFTimeInterval maxAge);
    void *info;
    CFHTTPMessageRef request;
    CFHTTPMessageRef response;
    dispatch_queue_t queue;
    /* The fields below should be considered private. */
    CFMutableDataRef data;
    CFReadStreamRef stream;
    dispatch_source_t timer;
} asynchttp_t;

/* Return false if work was scheduled and the callback will be invoked,
   true if it wasn't or the callback was already called. */
bool asyncHttpPost(CFURLRef cfUrl, CFDataRef postData, asynchttp_t *http);

/* Caller owns struct pointed to by http, but is responsible for calling
   asynchttp_free() when it's done with it. */
bool asynchttp_request(CFHTTPMessageRef request, asynchttp_t *http);
void asynchttp_free(asynchttp_t *http);

/* */


__END_DECLS

#endif /* !_SECURITYD_ASYNCHTTP_H_ */
