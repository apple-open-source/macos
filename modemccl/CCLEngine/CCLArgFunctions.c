/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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
 *  CCLArgFunctions.c
 *  CCLLauncher
 *
 *  Created by kevine on 4/11/06.
 *  Copyright 2006-7 Apple, Inc.  All rights reserved.
 *
 */

#include "CCLArgFunctions.h"
#include <CoreFoundation/CoreFoundation.h>

// For compatibility with previous usage, the boolean-return Get*FromDict()
// functions return errors only if the key's value is non-NULL but cannot
// be extracted.  If no value is stored for key or the value is NULL, the
// by-reference return buffers are not modified.

CFDictionaryRef
GetCFDictionaryFromDict(CFDictionaryRef dict, const CFStringRef key)
{
    CFDictionaryRef retVal= (CFDictionaryRef) CFDictionaryGetValue(dict, key);
    if((retVal!= NULL) && (CFDictionaryGetTypeID()== CFGetTypeID(retVal)))
    {
        return retVal;
    }
    return NULL;
}

bool
GetCFStringFromDict(CFDictionaryRef dict, CFStringRef *s, const CFStringRef key)
{
    CFStringRef tempString= (CFStringRef) CFDictionaryGetValue(dict, key);

    if (tempString == NULL)
        return true;

    if (CFStringGetTypeID() == CFGetTypeID(tempString)) {
        *s = tempString;
        return true;
    }

    return false;
}

bool
CopyCStringFromDict(CFDictionaryRef dict, char** string, const CFStringRef key)
{
    CFStringRef str = NULL;
    CFIndex bufSize;
    char *buf = NULL;

    // fallout cases
    if (!GetCFStringFromDict(dict, &str, key))
        return false;
    if (str == NULL)
        return true;

    bufSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str),
                                                kCFStringEncodingUTF8) + 1;
    buf = malloc(bufSize);
    if(buf && CFStringGetCString(str, buf, bufSize, kCFStringEncodingUTF8)) {
        *string = buf;
        return true;
    } 

    if (buf)  free(buf);
    return false;
}

bool
GetCFNumberFromDict(CFDictionaryRef dict, CFNumberRef *n, const CFStringRef key)
{
    CFNumberRef tempNum = (CFNumberRef)CFDictionaryGetValue(dict, key);

    if (tempNum == NULL)
        return true;

    if(CFNumberGetTypeID() == CFGetTypeID(tempNum)) {
        *n = tempNum;
        return true;
    }

    return false;
}

bool
GetIntFromDict(CFDictionaryRef dict, int* intRef, const CFStringRef key)
{
    CFNumberRef num = NULL;

    // fallout cases
    if (!GetCFNumberFromDict(dict, &num, key))
        return false;
    if (num == NULL)
        return true;

    return CFNumberGetValue(num, kCFNumberSInt32Type, intRef);
}
