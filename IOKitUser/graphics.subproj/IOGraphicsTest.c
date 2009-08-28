/*
 * Copyright (c) 2008 Apple Inc. All rights reserved.
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
cc -o /tmp/iogt graphics.subproj/IOGraphicsTest.c -framework CoreGraphics -Wall -undefined warning
*/

#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/graphics/IOGraphicsLib.h>
#include <stdlib.h>

int main( int argc, char * argv[] )
{
    io_service_t 	service;
    CFDictionaryRef	dict;
    CFMutableArrayRef	array;
    CGSError		err;
    int			i, j;

    CGSInitialize();

    array = CFArrayCreateMutable( kCFAllocatorDefault, 0,
                                    &kCFTypeArrayCallBacks);

    for( i = 0; i < CGSGetNumberOfDisplays(); i++ ) {
        err = CGSServiceForDisplayNumber(i, &service);
        if( err != kCGSErrorSuccess)
            continue;
    
        dict = IOCreateDisplayInfoDictionary( service, kNilOptions );
        if( dict) {
            printf("Display %d\n", i);
            CFShow(dict);
            CFRelease(dict);
        }

        dict = IOCreateDisplayInfoDictionary( service,
                                            kIODisplayMatchingInfo );
        if( dict) {
            printf("Display %d, kIODisplayMatchingInfo\n", i);
            CFShow(dict);

            CFArrayAppendValue(array, dict);
            CFRelease(dict);
			CFIndex count = CFArrayGetCount(array);
            for( j = 0; j < count; j++) {
                printf("%d matches(%ld)\n", j,
                    IODisplayMatchDictionaries( dict,
                        CFArrayGetValueAtIndex(array, j), kNilOptions));
            }
        }
    }

    exit(0);
    return(0);
}

