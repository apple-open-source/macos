/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#include "secd_regressions.h"

#include "keychain/securityd/SecDbItem.h"
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecFileLocations.h>
#include <utilities/fileIo.h>

#include "keychain/securityd/SOSCloudCircleServer.h"
#include "keychain/securityd/SecItemServer.h"

#include <Security/SecBasePriv.h>

#include <TargetConditionals.h>
#include <AssertMacros.h>

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include "SecdTestKeychainUtilities.h"
#include <Security/OTConstants.h>

#include "SOSAccountTesting.h"

#if TARGET_OS_SIMULATOR || TARGET_OS_IPHONE
#if SOS_ENABLED

static void enableCompatibilityMode()
{
    CFErrorRef localError = NULL;
    bool setResult = SOSCCSetCompatibilityMode(true, &localError);
    ok(setResult == true, "result should be true");
    CFReleaseNull(localError);
    
    
    bool fetchResult = SOSCCFetchCompatibilityMode(&localError);
    ok(fetchResult == true, "result should be enabled");
    CFReleaseNull(localError);
}

static void disableCompatibilityMode()
{
    CFErrorRef localError = NULL;
    bool setResult = SOSCCSetCompatibilityMode(false, &localError);
    ok(setResult == true, "result should be true");
    CFReleaseNull(localError);
    
    
    bool fetchResult = SOSCCFetchCompatibilityMode(&localError);
    ok(fetchResult == false, "result should be disabled");
    ok(localError == nil, "error should be nil");
    CFReleaseNull(localError);
}

static void tests(void)
{
    CFErrorRef error = NULL;

    disableCompatibilityMode();
    
    bool joinResult = SOSCCRequestToJoinCircle(&error);
    ok(joinResult == false, "join result should be false");
    ok(error != nil, "error should not be nil");
    ok(CFErrorGetCode(error) == kSOSErrorPlatformNoSOS, "error code should be kSOSErrorPlatformNoSOS");
    
    NSString* description = CFBridgingRelease(CFErrorCopyDescription(error));
    ok(description, "description should not be nil");
    ok([description isEqualToString:@"The operation couldn’t be completed. (com.apple.security.sos.error error 1050 - SOS Disabled for this platform)"], "error description should be SOS Disabled for this platform");
    description = nil;

    CFReleaseNull(error);
    
    enableCompatibilityMode();
    
    joinResult = SOSCCRequestToJoinCircle(&error);
    ok(joinResult == false, "join result should be false");
    ok (error != nil, "error should not be nil");
    
    ok(CFErrorGetCode(error) == kSOSErrorPrivateKeyAbsent, "error code should be kSOSErrorPrivateKeyAbsent");
    description = CFBridgingRelease(CFErrorCopyDescription(error));
    ok(description, "description should not be nil");
    
    ok([description isEqualToString:@"The operation couldn’t be completed. (com.apple.security.sos.error error 1 - Private Key not available - failed to prompt user recently)"], "error description should be The operation couldn’t be completed. (com.apple.security.sos.error error 1 - Private Key not available - failed to prompt user recently)");
    CFReleaseNull(error);
    
}
#endif

int secd_232_sos_compatibility_mode(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(16);
    secd_test_setup_temp_keychain(__FUNCTION__, NULL);
    tests();
    secd_test_teardown_delete_temp_keychain(__FUNCTION__);
#else
    plan_tests(0);
#endif
    return 0;
}
#endif
