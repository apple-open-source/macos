/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <utilities/SecBuffer.h>

#include "utilities_regressions.h"


#define kTestCount (2 * 12 + 3 * 12)

const uint8_t testBytes[] = { 0xD0, 0xD0, 0xBA, 0xAD };

static void
tests(void) {
    for(size_t testSize = 1023; testSize < 2 * 1024 * 1024; testSize *= 2) {
        PerformWithBuffer(testSize, ^(size_t size, uint8_t *buffer) {
            ok(buffer, "got buffer");
            ok(size == testSize, "buffer size");

            // Scribble on the end, make sure we can.
            uint64_t *scribbleLocation = (uint64_t *) (buffer + testSize - sizeof(testBytes));
            bcopy(testBytes, scribbleLocation, sizeof(testBytes));
        });
    }
    
    for(size_t testSize = 1023; testSize < 2 * 1024 * 1024; testSize *= 2) {
        __block uint64_t *scribbleLocation = NULL;
        PerformWithBufferAndClear(testSize, ^(size_t size, uint8_t *buffer) {
            ok(buffer, "got buffer");
            ok(size == testSize, "buffer size");
            
            scribbleLocation = (uint64_t *) (buffer + testSize - sizeof(testBytes));
            bcopy(testBytes, scribbleLocation, sizeof(testBytes));
        });
        SKIP: {
            skip("memory might be unmapped leading to a crash", 1, false);
            ok(*scribbleLocation == 0, "Was erased");
        }
    }    
}


int
su_08_secbuffer(int argc, char *const *argv) {
    plan_tests(kTestCount);
    
    tests();
    
    return 0;
}
