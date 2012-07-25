/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
// Utilities
//
#include <security_utilities/utilities.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>

namespace Security
{

static CFMutableDictionaryRef gCacheDictionaryRef = NULL;
static dispatch_once_t gDictionaryCreated = 0;
static dispatch_queue_t gSerializeQueue;
    
char *cached_realpath(const char * file_name, char * resolved_name)
{
    dispatch_once(&gDictionaryCreated,
        ^{
            gCacheDictionaryRef = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            gSerializeQueue = dispatch_queue_create("com.apple.SecurityLookupCacheQueue", DISPATCH_QUEUE_SERIAL);
        });

    
    __block char* valueToReturn = NULL;
    
    dispatch_sync(gSerializeQueue,
    ^{
        // Put a maximum limit on the size of this cache.
        CFIndex entryCount = CFDictionaryGetCount(gCacheDictionaryRef);
        
        // make the incoming name a string
        CFStringRef input = CFStringCreateWithCString(NULL, file_name, kCFStringEncodingUTF8);
        if (entryCount < OPEN_MAX && input != NULL) // is it acceptable to use the cache?
        {
            // see if we can find that string in our dictionary
            CFStringRef output = (CFStringRef) CFDictionaryGetValue(gCacheDictionaryRef, input);
            
            if (output == NULL)
            {
                // the string is not in our cache, so use realpath
                valueToReturn = realpath(file_name, resolved_name);
                if (valueToReturn != NULL) // no error, so continue
                {
                    // make a new entry in the dictionary for our string
                    output = CFStringCreateWithCString(NULL, valueToReturn, kCFStringEncodingUTF8);
                    CFDictionaryAddValue(gCacheDictionaryRef, input, output);
                    CFRelease(output);
                }
            }
            else
            {
                char* valueToFree = NULL;

                // we need to extract the value from the output
                
                // figure out how big to make our buffer
                CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(output), kCFStringEncodingUTF8) + 1; // account for NULL termination
                
                // if the user has passed in a buffer, use it.  If not, allocate our own
                
                // technically, we don't know the size of the buffer that the
                // user has passed in.  It has to be large enough to hold the
                // string, however, so we can use size as an estimator.  The
                // result will be the same, however: If the user didn't supply
                // enough memory, he will crash.  We behave exactly the same
                // as realpath, which is the idea.
                char *buffer = resolved_name;

                // allocate a buffer if none was passed in
                if (buffer == NULL) {
                    valueToFree = buffer = (char*) malloc(size);
                }

                if (buffer != NULL) // check to see if malloc failed earlier
                {
                    if (!CFStringGetCString(output, buffer, size, kCFStringEncodingUTF8))
                    {
                        free((void*) valueToFree);
                        valueToReturn = NULL;
                    }
                    else
                    {
                        valueToReturn = buffer;
                    }
                }
            }
        }
        else
        {
            valueToReturn = realpath(file_name, resolved_name);
        }

        CFRelease(input);
    });

    return valueToReturn;
}

}


