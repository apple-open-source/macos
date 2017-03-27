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
#include <Availability.h>
#if (CCRANDOM == 0)
entryPoint(CommonRandom,"Random Number Generation")
#else
#include <CommonCrypto/CommonRandomSPI.h>

static const int kTestTestCount = 1000;
static const int bufmax = kTestTestCount + 16;

// Number of file which can be open by CCRegression application
// This value need to be acceptable for all CCRegression since default value can only be restored by Super User.
#define CCREGRESSION_MAX_FILE_OPEN_LIMIT 10
#if defined(_WIN32)
#include <Windows.h>
#endif
static void print_os(char *msg){
    
#if defined(IPHONE_SIMULATOR_HOST_MIN_VERSION_REQUIRED)
    int v  = IPHONE_SIMULATOR_HOST_MIN_VERSION_REQUIRED;
    char s = "simulator";
#elif defined(__MAC_OS_X_VERSION_MIN_REQUIRED)
    int v = __MAC_OS_X_VERSION_MIN_REQUIRED;
    char *s = "macOs";
#elif defined(__IPHONE_OS_VERSION_MIN_REQUIRED)
    int v = __IPHONE_OS_VERSION_MIN_REQUIRED;
    char *s = "iOS";
#elif defined(__TV_OS_VERSION_MIN_REQUIRED)
    int v = __TV_OS_VERSION_MIN_REQUIRED;
    char *s = "tvOs";
#elif defined(__WATCH_OS_VERSION_MIN_REQUIRED)
    int v = __WATCH_OS_VERSION_MIN_REQUIRED;
    char *s = "watchOs";
#elif defined(_WIN32)
    int v = WINVER;
    char *s = "Windows";
#else
    int v = 0;
	char *s = "unknown OS";
#endif
    
    diag("%s %s=%i", msg, s, v);
    
}


//This if statment needs to be expressed in a positive form.
//We use the current negative form temporarily until we find the correct OS versiona that
//need this test. rdar://problem/28064281
#if  !((defined(IPHONE_SIMULATOR_HOST_MIN_VERSION_REQUIRED) && IPHONE_SIMULATOR_HOST_MIN_VERSION_REQUIRED >= 100000) \
|| (defined(__MAC_OS_X_VERSION_MIN_REQUIRED)  && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200)   \
|| (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED>= 100000)   \
|| (defined(__TV_OS_VERSION_MIN_REQUIRED) &&     __TV_OS_VERSION_MIN_REQUIRED    >= 100000)   \
|| (defined(__WATCH_OS_VERSION_MIN_REQUIRED) &&  __WATCH_OS_VERSION_MIN_REQUIRED >= 30000)) && !defined(_WIN32)

#include <sys/resource.h>

static void CommonRandom_OS10_11_earlier()
{
    // This is for 10.11 / iOS9.0 and earlier.
    // From now on, we use a syscall so that there is no file descriptor issue.
    
    print_os("/dev/random exhaustion");
    // ============================================
    //          Negative testing first
    // ============================================
    struct ccrng_system_state rng_array[CCREGRESSION_MAX_FILE_OPEN_LIMIT];
    int rng_array_valid_cnt=0;
    int rng_status=0;
    
    // Reduce number to CCREGRESSION_MAX_FILE_OPEN_LIMIT
    // Only SU can increase this value so we just don't restore it to default.
    const struct rlimit rlp= { CCREGRESSION_MAX_FILE_OPEN_LIMIT, CCREGRESSION_MAX_FILE_OPEN_LIMIT };
    is(setrlimit(RLIMIT_NOFILE, &rlp),0, "Set max number of open files");
    
    // Exhaust opening of /dev/random
    for(int i=0; ((i < CCREGRESSION_MAX_FILE_OPEN_LIMIT) && (rng_status>=0)); i++) {
        rng_status=ccrng_system_init(&rng_array[i]);
        rng_array_valid_cnt++;
    }
    cmp_ok(rng_array_valid_cnt,<,CCREGRESSION_MAX_FILE_OPEN_LIMIT, "/dev/random exhaustion");
    
    // Any random initialization will cause abort below.
    // Since we can't recover, this can't be permanently in the test
#if 0
    isnt(CCRNGCreate(0, &rngref),kCCSuccess, "CCRNGCreate with /dev/random exhaustion");
    devRandom = ccDevRandomGetRngState();
    is(devRandom,NULL, "ccDevRandomGetRngState /dev/random exhaustion");
    drbg = ccDRBGGetRngState();
    is(drbg,NULL, "ccDRBGGetRngState /dev/random exhaustion");
    
    // Get random under exhausted /dev/random
    isnt(CCRandomCopyBytes(kCCRandomDefault, buf1, 1),0, "Exhausted /dev/random failure");
    isnt(CCRandomCopyBytes(kCCRandomDevRandom, buf2, 1),0, "Exhausted /dev/random failure");
    isnt(CCRandomCopyBytes(rngref, buf3, 1),0, "Exhausted /dev/random failure");
    isnt(CCRandomCopyBytes(NULL, buf4, 1),0, "Exhausted /dev/random failure");
    isnt(CCRandomGenerateBytes(buf7, 1),0, "Exhausted /dev/random failure");
#endif
    // Close to allow again the use of /dev/random
    for(int i=0; (i < rng_array_valid_cnt); i++) {
        ccrng_system_done(&rng_array[i]);
    }
}
#else
static void CommonRandom_OS10_11_earlier(){
    //do nothing
    print_os("");
}
#endif
                 
int CommonRandom(int __unused argc, char *const * __unused argv)
{
    int i;
    uint8_t buf1[bufmax], buf2[bufmax], buf3[bufmax], buf4[bufmax], buf5[bufmax], buf6[bufmax], buf7[bufmax];
    CCRandomRef rngref;
    
	plan_tests(kTestTestCount * 14 + 12);

    struct ccrng_state *devRandom = NULL;
    struct ccrng_state *drbg = NULL;
    
    CommonRandom_OS10_11_earlier();

    // ============================================
    //          Positive testing
    // ============================================

    is(CCRNGCreate(0, &rngref),kCCSuccess, "CCRNGCreate success");
    devRandom = ccDevRandomGetRngState();
    isnt(devRandom,NULL, "Dev random first state ok");
    drbg = ccDRBGGetRngState();
    isnt(drbg,NULL, "DRBG first state ok");
    if (devRandom==NULL || drbg==NULL) return 1;
    for(i=0; i < kTestTestCount; i++) {
        size_t len = i+16;
        is(CCRandomCopyBytes(kCCRandomDefault, buf1, len),0, "Random success");
        is(CCRandomCopyBytes(kCCRandomDevRandom, buf2, len),0, "Random success");
        is(CCRandomCopyBytes(rngref, buf3, len),0, "Random success");
        is(CCRandomCopyBytes(NULL, buf4, len),0, "Random success");
        is(ccrng_generate(devRandom, len, buf5),0, "Random success");
        is(ccrng_generate(drbg, len, buf6),0, "Random success");
        is(CCRandomGenerateBytes(buf7, len),0, "Random success");
        
        ok(memcmp(buf1, buf2, len), "Buffers aren't the same");
        ok(memcmp(buf3, buf4, len), "Buffers aren't the same");
        ok(memcmp(buf2, buf3, len), "Buffers aren't the same");
        ok(memcmp(buf5, buf6, len), "Buffers aren't the same");
        ok(memcmp(buf5, buf2, len), "Buffers aren't the same");
        ok(memcmp(buf6, buf1, len), "Buffers aren't the same");
        ok(memcmp(buf7, buf1, len), "Buffers aren't the same");
    }


    // Bad inputs
    is(CCRandomCopyBytes(kCCRandomDefault, buf1, 0),0, "Zero Length");
    is(CCRandomCopyBytes(kCCRandomDevRandom, buf2, 0),0, "Zero Length");
    is(CCRandomGenerateBytes(buf7, 0),0, "Zero Length");

    isnt(CCRandomCopyBytes(kCCRandomDefault, NULL, 1),0, "NULL pointer");
    isnt(CCRandomCopyBytes(kCCRandomDevRandom, NULL, 1),0, "NULL pointer");
    isnt(CCRandomGenerateBytes(NULL, 1),0, "NULL pointer");

    is(CCRandomCopyBytes(kCCRandomDefault, NULL, 0),0, "Zero Length, NULL pointer");
    is(CCRandomCopyBytes(kCCRandomDevRandom, NULL, 0),0, "Zero Length, NULL pointer");
    is(CCRandomGenerateBytes(NULL, 0),0, "Zero Length, NULL pointer");

    CCRNGRelease(rngref);
        
    return 0;
}
#endif //CCRANDOM

