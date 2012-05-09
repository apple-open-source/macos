/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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

#ifndef webdavfs_webdav_cookie_h
#define webdavfs_webdav_cookie_h

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include "webdav.h"

typedef struct cookie_type
{
	// Cookie header for sending, in form "name=val"
	CFMutableStringRef cookie_header;
	
    CFStringRef cookie_name;
    char        *cookie_name_str;
    
    CFStringRef cookie_val;
    char        *cookie_val_str;
    
    CFStringRef cookie_path;
    char        *cookie_path_str;
    
    CFStringRef cookie_domain;
    char        *cookie_domain_str;
    
    boolean_t   has_expire_time;
    time_t      cookie_expire_time;
    
    boolean_t   cookie_secure;
    boolean_t   cookie_httponly;
	
	struct cookie_type *next, *prev;
} WEBDAV_COOKIE;

void cookies_init(void);
void add_cookie_headers(CFHTTPMessageRef message, CFURLRef url);
void handle_cookies(CFStringRef str, CFHTTPMessageRef message);
void purge_expired_cookies(void);

void dump_cookies(struct webdav_request_cookies *req);
void reset_cookies(struct webdav_request_cookies *req);


#endif
