/*
 * Copyright (c) 1999-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * HISTORY
 *
 */

#include "CFAdditions.h"

Boolean CFDictionaryMatch(CFDictionaryRef dict1, CFDictionaryRef dict2, CFTypeRef match)
{
    Boolean ret;
    CFTypeID tid;
    UInt32 count;
    UInt32 i;


    if ( match == NULL ) {
        return CFEqual(dict1, dict2);
    }

    tid = CFGetTypeID(match);
    ret = true;
    if ( CFDictionaryGetTypeID() == tid) {
        const void ** keys;

        count = CFDictionaryGetCount(match);
        if ( (count > CFDictionaryGetCount(dict1)) ||
             (count > CFDictionaryGetCount(dict2)) )
            return false;

        keys = malloc(count);
        CFDictionaryGetKeysAndValues(match, keys, NULL);
        for ( i = 0; i < count; i ++ ) {
            const void * val1;
            const void * val2;

            val1 = CFDictionaryGetValue(dict1, keys[i]);
            if ( !val1 ) {
                ret = false;
                break;
            }

            val2 = CFDictionaryGetValue(dict2, keys[i]);
            if ( !val2 ) {
                ret = false;
                break;
            }

            if ( !CFEqual(val1, val2) ) {
                ret = false;
                break;
            }
        }

        free(keys);
    }
    else if ( CFArrayGetTypeID() == tid ) {
        const void * key;
        
        count = CFArrayGetCount(match);
        if ( (count > CFDictionaryGetCount(dict1)) ||
             (count > CFDictionaryGetCount(dict2)) )
            return false;

        for ( i = 0; i < count; i ++ ) {
            const void * val1;
            const void * val2;

            key = CFArrayGetValueAtIndex(match, i);
            val1 = CFDictionaryGetValue(dict1, key);
            if ( !val1 ) {
                ret = false;
                break;
            }

            val2 = CFDictionaryGetValue(dict2, key);
            if ( !val2 ) {
                ret = false;
                break;
            }

            if ( !CFEqual(val1, val2) ) {
                ret = false;
                break;
            }
        }
    }
    else {
        ret = false;
    }

    return ret;
}

static void ArrayMergeFunc(const void * val, void * context)
{
    CFMutableArrayRef array;

    array = context;

    CFArrayAppendValue(array, val);
}

void CFArrayMergeArray(CFMutableArrayRef array1, CFArrayRef array2)
{
    CFRange range;

    if ( !array1 || !array2 ) {
        return;
    }
    
    range = CFRangeMake(0, CFArrayGetCount(array2));
    CFArrayApplyFunction(array2, range, ArrayMergeFunc, array1);
}

CFDataRef CFStringCreateCStringData(CFAllocatorRef allocator, CFStringRef string, CFStringEncoding encoding)
{
    UInt8 * buffer;
    CFDataRef data;
    CFIndex length;

    if ( !string )
        return NULL;
    
    length = CFStringGetLength(string) + 1;
    buffer = (UInt8 *)malloc(sizeof(char) * length);
    if ( !CFStringGetCString(string, buffer, length, encoding) ) {
        return NULL;
    }

    data = CFDataCreate(allocator, (const UInt8 *)buffer, length);
    free(buffer);
    
    return data;
}
