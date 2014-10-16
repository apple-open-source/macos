/*
 * Copyright (c) 2013 Apple Inc. All Rights Reserved.
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

//
//  utilities.c
//

#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>

#include <CKBridge/SOSCloudKeychainClient.h>
#include "utilities.h"

bool testPutObjectInCloud(CFStringRef key, CFTypeRef object, CFErrorRef *error, dispatch_group_t dgroup, dispatch_queue_t processQueue)
{
    //FIXME: The error set in the block is never returned here.
    secerror("testPutObjectInCloud: key: %@, %@", key, object);
    CFDictionaryRef objects = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, key, object, NULL);
    if (objects)
    {
        dispatch_group_enter(dgroup);
        SOSCloudKeychainPutObjectsInCloud(objects, processQueue, ^ (CFDictionaryRef returnedValues, CFErrorRef error2)
        {
            secerror("testPutObjectInCloud returned: %@", returnedValues);
            if (error2)
            {
                secerror("testPutObjectInCloud returned: %@", error2);
            }
            dispatch_group_leave(dgroup);
        });
        CFRelease(objects);
    }
    return true; // Never returns an error
}
