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


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <paths.h>
#include <sys/queue.h>
		
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <CoreFoundation/CoreFoundation.h>

#include "cf_utils.h"


// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
Boolean isDictionary (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFDictionaryGetTypeID());
}

Boolean isArray (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFArrayGetTypeID());
}

Boolean isString (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFStringGetTypeID());
}

Boolean isNumber (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFNumberGetTypeID());
}

Boolean isData (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFDataGetTypeID());
}


#define OPT_STR_LEN 256

// ----------------------------------------------------------------------------
//	get_array_option
// ----------------------------------------------------------------------------
int get_array_option(CFPropertyListRef options, CFStringRef entity, CFStringRef property, CFIndex index,
            u_char *opt, u_int32_t optsiz, u_int32_t *outlen, u_char *defaultval)
{
    CFDictionaryRef	dict;
    CFArrayRef		array;
    CFIndex		count;
    CFStringRef		string;

    dict = CFDictionaryGetValue(options, entity);
    if (isDictionary(dict)) {
        
        array = CFDictionaryGetValue(dict, property);
        if (isArray(array)
            && (count = CFArrayGetCount(array)) > index) {
            string = CFArrayGetValueAtIndex(array, index);
            if (isString(string)) {
                opt[0] = 0;
                CFStringGetCString(string, (char*)opt, optsiz, kCFStringEncodingMacRoman);
                *outlen = strlen((char*)opt);
            }
            return (count > (index + 1));
        }
    }
    
    strlcpy((char*)opt, (char*)defaultval, optsiz);
    *outlen = strlen((char*)opt);
    return 0;
}

// ----------------------------------------------------------------------------
//	get_str_option
// ----------------------------------------------------------------------------
void get_str_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property, 
                        u_char *opt, u_int32_t optsiz, u_int32_t *outlen, u_char *defaultval)
{
    CFDictionaryRef	dict;
    CFStringRef		ref;
    
    dict = CFDictionaryGetValue(options, entity);
    if (isDictionary(dict)) {
        opt[0] = 0;
        ref  = CFDictionaryGetValue(dict, property);
        if (isString(ref)) {
            CFStringGetCString(ref, (char*)opt, optsiz, kCFStringEncodingUTF8);
            *outlen = strlen((char*)opt);
            return;
        }
    }

    strlcpy((char*)opt, (char*)defaultval, optsiz);
    *outlen = strlen((char*)opt);
}

// ----------------------------------------------------------------------------
//	get_cfstr_option
// ----------------------------------------------------------------------------
CFStringRef get_cfstr_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property)
{
    CFDictionaryRef	dict;
    CFStringRef		ref;
    
    dict = CFDictionaryGetValue(options, entity);
    if (isDictionary(dict)) {
        ref  = CFDictionaryGetValue(dict, property);
        if (isString(ref))
            return ref;
    }

    return NULL;
}

// ----------------------------------------------------------------------------
//	get_int_option
// ----------------------------------------------------------------------------
void get_int_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property,
        u_int32_t *opt, u_int32_t defaultval)
{
    CFDictionaryRef	dict;
    CFNumberRef		ref;

    dict = CFDictionaryGetValue(options, entity);
    if (isDictionary(dict)) {
        ref  = CFDictionaryGetValue(dict, property);
        if (isNumber(ref)) {
            CFNumberGetValue(ref, kCFNumberSInt32Type, opt);
            return;
        }
    }

    *opt = defaultval;
}

// ----------------------------------------------------------------------------
//	GetIntFromDict
// ----------------------------------------------------------------------------
Boolean GetIntFromDict (CFDictionaryRef dict, CFStringRef property, u_int32_t *outval, u_int32_t defaultval)
{
    CFNumberRef		ref;
	
	ref  = CFDictionaryGetValue(dict, property);
	if (isNumber(ref)
		&&  CFNumberGetValue(ref, kCFNumberSInt32Type, outval))
		return TRUE;
	
	*outval = defaultval;
	return FALSE;
}

// ----------------------------------------------------------------------------
//	GetStrFromDict
// ----------------------------------------------------------------------------
int GetStrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen, char *defaultval)
{
    CFStringRef		ref;

	ref  = CFDictionaryGetValue(dict, property);
	if (!isString(ref)
		|| !CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8))
		strncpy(outstr, defaultval, maxlen);
	
	return strlen(outstr);
}

// ----------------------------------------------------------------------------
//	GetStrAddrFromDict
// ----------------------------------------------------------------------------
Boolean GetStrAddrFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen)
{
    CFStringRef		ref;
	in_addr_t               addr;
	
	ref  = CFDictionaryGetValue(dict, property);
	if (isString(ref)
			&& CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8)) {
					addr = inet_addr(outstr);
					return addr != INADDR_NONE;
	}
	
	return FALSE;
}

// ----------------------------------------------------------------------------
//	GetStrNetFromDict
// ----------------------------------------------------------------------------
Boolean GetStrNetFromDict (CFDictionaryRef dict, CFStringRef property, char *outstr, int maxlen)
{
    CFStringRef		ref;
	in_addr_t               net;

	ref  = CFDictionaryGetValue(dict, property);
	if (isString(ref)
			&& CFStringGetCString(ref, outstr, maxlen, kCFStringEncodingUTF8)) {
			net = inet_network(outstr);
			return net != INADDR_NONE && net != 0;
	}
	
	return FALSE;
}
