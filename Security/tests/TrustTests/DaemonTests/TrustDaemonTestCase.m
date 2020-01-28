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
#include "trust/trustd/trustd_spi.h"

#import "TrustDaemonTestCase.h"

@implementation TrustDaemonTestCase

static int current_dir = -1;
static char *home_var = NULL;

/* Build in trustd functionality to the tests */
+ (void) setUp {
    /* Set up TMP directory for trustd's files */
    int ok = 0;
    NSError* error = nil;
    NSString* pid = [NSString stringWithFormat: @"tst-%d", [[NSProcessInfo processInfo] processIdentifier]];
    NSURL* tmpDirURL = [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:pid];
    ok = (bool)tmpDirURL;

    if (current_dir == -1 && home_var == NULL) {
        ok = ok && [[NSFileManager defaultManager] createDirectoryAtURL:tmpDirURL
                                            withIntermediateDirectories:NO
                                                             attributes:NULL
                                                                  error:&error];

        NSURL* libraryURL = [tmpDirURL URLByAppendingPathComponent:@"Library"];
        NSURL* preferencesURL = [tmpDirURL URLByAppendingPathComponent:@"Preferences"];

        ok =  (ok && (current_dir = open(".", O_RDONLY) >= 0)
               && (chdir([tmpDirURL fileSystemRepresentation]) >= 0)
               && (setenv("HOME", [tmpDirURL fileSystemRepresentation], 1) >= 0)
               && (bool)(home_var = getenv("HOME")));

        ok = ok && [[NSFileManager defaultManager] createDirectoryAtURL:libraryURL
                                            withIntermediateDirectories:NO
                                                             attributes:NULL
                                                                  error:&error];

        ok = ok && [[NSFileManager defaultManager] createDirectoryAtURL:preferencesURL
                                            withIntermediateDirectories:NO
                                                             attributes:NULL
                                                                  error:&error];
    }

    if (ok > 0) {
        /* Be trustd */
        trustd_init((__bridge CFURLRef) tmpDirURL);
    }
}

@end
