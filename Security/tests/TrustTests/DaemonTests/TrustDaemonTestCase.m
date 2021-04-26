/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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
 *
 */

#import <XCTest/XCTest.h>
#include <sys/stat.h>
#include <utilities/SecFileLocations.h>
#include <utilities/debugging.h>
#include "ipc/securityd_client.h"
#include "trust/trustd/trustd_spi.h"

#import "../TrustEvaluationTestHelpers.h"
#import "TrustDaemonTestCase.h"

@implementation TrustDaemonInitializationTestCase

/* make a new directory for each test case */
static int testNumber = 0;
- (void) setUp {
    NSURL *tmpDirURL = setUpTmpDir();
    tmpDirURL = [tmpDirURL URLByAppendingPathComponent:[NSString stringWithFormat:@"case-%d", testNumber]];

    NSError *error = nil;
    BOOL ok = [[NSFileManager defaultManager] createDirectoryAtURL:tmpDirURL
                                       withIntermediateDirectories:YES
                                                        attributes:NULL
                                                             error:&error];
    if (ok && tmpDirURL) {
        SecSetCustomHomeURL((__bridge CFURLRef)tmpDirURL);
    }
    testNumber++;
    gTrustd = &trustd_spi; // Signal that we're running as (uninitialized) trustd

    /* Because each test case gets a new "home" directory but we only create the data vault hierarchy once per
     * launch, we need to initialize those directories for each test case. */
    WithPathInProtectedDirectory(CFSTR("trustd"), ^(const char *path) {
        mode_t permissions = 0755; // Non-system trustd's create directory with expansive permissions
        int ret = mkpath_np(path, permissions);
        chmod(path, permissions);
        if (!(ret == 0 || ret ==  EEXIST)) {
            secerror("could not create path: %s (%s)", path, strerror(ret));
        }
    });
}
@end

@implementation TrustDaemonTestCase

/* Build in trustd functionality to the tests */
+ (void) setUp {
    NSURL *tmpDirURL = setUpTmpDir();
    trustd_init((__bridge CFURLRef) tmpDirURL);

    // "Disable" evaluation analytics (by making the sampling rate as low as possible)
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];
    [defaults setInteger:UINT32_MAX forKey:@"TrustEvaluationEventAnalyticsRate"];
    [defaults setInteger:UINT32_MAX forKey:@"PinningEventAnalyticsRate"];
    [defaults setInteger:UINT32_MAX forKey:@"SystemRootUsageEventAnalyticsRate"];
    [defaults setInteger:UINT32_MAX forKey:@"TrustFailureEventAnalyticsRate"];
}

@end
