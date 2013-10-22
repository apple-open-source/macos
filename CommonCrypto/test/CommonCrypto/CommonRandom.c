/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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
 *  randomTest
 *  CommonCrypto
 */
#include "testmore.h"
#include "capabilities.h"
#include "testbyteBuffer.h"

#if (CCRANDOM == 0)
entryPoint(CommonRandom,"Random Number Generation")
#else
#include <CommonCrypto/CommonRandomSPI.h>

static const int kTestTestCount = 1000;
static const int bufmax = kTestTestCount + 16;

int CommonRandom(int argc, char *const *argv)
{
    int i;
    uint8_t buf1[bufmax], buf2[bufmax], buf3[bufmax], buf4[bufmax], buf5[bufmax], buf6[bufmax];
    CCRandomRef rngref;
    
	plan_tests(kTestTestCount * 6);
    CCRNGCreate(0, &rngref);
    
    struct ccrng_state *devRandom = ccDevRandomGetRngState();
    struct ccrng_state *drbg = ccDRBGGetRngState();
    for(i=0; i < kTestTestCount; i++) {
        size_t len = i+16;
        CCRandomCopyBytes(kCCRandomDefault, buf1, len);
        CCRandomCopyBytes(kCCRandomDevRandom, buf2, len);
        CCRandomCopyBytes(rngref, buf3, len);
        CCRandomCopyBytes(NULL, buf4, len);
        ccrng_generate(devRandom, len, buf5);
        ccrng_generate(drbg, len, buf6);

        ok(memcmp(buf1, buf2, len), "Buffers aren't the same");
        ok(memcmp(buf3, buf4, len), "Buffers aren't the same");
        ok(memcmp(buf2, buf3, len), "Buffers aren't the same");
        ok(memcmp(buf5, buf6, len), "Buffers aren't the same");
        ok(memcmp(buf5, buf2, len), "Buffers aren't the same");
        ok(memcmp(buf6, buf1, len), "Buffers aren't the same");
    }
    CCRNGRelease(rngref);
        
    return 0;
}
#endif

