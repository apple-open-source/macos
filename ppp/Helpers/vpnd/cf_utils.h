/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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


#ifndef __CF_UTILS_H__
#define __CF_UTILS_H__


Boolean isDictionary (CFTypeRef obj);
Boolean isArray (CFTypeRef obj);
Boolean isString (CFTypeRef obj);
Boolean isNumber (CFTypeRef obj);
Boolean isData (CFTypeRef obj);

int get_array_option(CFPropertyListRef options, CFStringRef entity, CFStringRef property, CFIndex index,
            u_char *opt, u_int32_t optsiz, u_int32_t *outlen, u_char *defaultval);

void get_str_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property, 
                        u_char *opt, u_int32_t optsiz, u_int32_t *outlen, u_char *defaultval);

CFStringRef get_cfstr_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property);

void get_int_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property,
        u_int32_t *opt, u_int32_t defaultval);

Boolean GetIntFromDict (CFDictionaryRef dict, CFStringRef property, u_int32_t *outval, u_int32_t defaultval);

int GetStrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen, char *defaultval);

Boolean GetStrAddrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen);

Boolean GetStrNetFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen);

#endif
