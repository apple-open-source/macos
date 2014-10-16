/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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


#include "testpolicy.h"

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE

#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>
#include <stdlib.h>
#include <unistd.h>

#include "testmore.h"

/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
 */

#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>
#include <stdlib.h>
#include <unistd.h>

#include "testmore.h"

/* Those tests were originally written around that date. */
CFGiblisGetSingleton(CFDateRef, GetFrozenTime, frozenTime, ^{
    *frozenTime = CFDateCreateForGregorianZuluDay(NULL, 2011, 9, 1);
});

static void runOneLeafTest(SecPolicyRef policy,
                           NSArray* anchors,
                           NSArray* intermediates,
                           NSString* path,
                           bool expectedResult,
                           NSObject* expectations,
                           CFDateRef date)
{
    NSString* fileName = [path lastPathComponent];
    const char *reason = NULL;
    SecTrustRef trustRef = NULL;
    CFStringRef failReason = NULL;
    NSMutableArray* certArray = NULL;
    SecCertificateRef certRef = NULL;

    if (expectations) {
        if ([expectations isKindOfClass: [NSString class]]) {
            reason = [(NSString *)expectations UTF8String];
        } else if ([expectations isKindOfClass: [NSDictionary class]]) {
            NSDictionary *dict = (NSDictionary *)expectations;
            NSObject *value = [dict valueForKey:@"valid"];
            if (value) {
                if ([value isKindOfClass: [NSNumber class]]) {
                    expectedResult = [(NSNumber *)value boolValue];
                } else {
                    NSLog(@"Unexpected valid value %@ in dict for key %@", value, fileName);
                }
            }
            value = [dict valueForKey:@"reason"];
            if (value) {
                if ([value isKindOfClass: [NSString class]]) {
                    reason = [(NSString *)value UTF8String];
                } else {
                    NSLog(@"Unexpected reason value %@ in dict for key %@", value, fileName);
                }
            }
        } else if ([expectations isKindOfClass: [NSNumber class]]) {
            expectedResult = [(NSNumber *)expectations boolValue];
        } else {
            NSLog(@"Unexpected class %@ value %@ for key %@", [expectations class], expectations, fileName);
        }
    }

    certRef = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
    if (!certRef) {
        if (reason) {
            todo(reason);
            fail("%@ unable to create certificate", fileName);
        } else {
            fail("PARSE %@ unable to create certificate", fileName);
        }
        goto exit;
    }

    certArray = [NSMutableArray arrayWithArray:intermediates];
    [certArray insertObject:(id)certRef atIndex:0]; //The certificate to be verified must be the first in the array.

    OSStatus err;
    err = SecTrustCreateWithCertificates(certArray, policy, &trustRef);
    if (err) {
        ok_status(err, "SecTrustCreateWithCertificates");
        goto exit;
    }
    if ([anchors count])
        SecTrustSetAnchorCertificates(trustRef, (CFArrayRef)anchors);

    SecTrustSetVerifyDate(trustRef, date ? date : GetFrozenTime());

    SecTrustResultType evalRes = 0;
    //NSLog(@"Evaluating: %@",certRef);
    err = SecTrustEvaluate(trustRef, &evalRes);
    if (err) {
        ok_status(err, "SecTrustCreateWithCertificates");
        goto exit;
    }
    BOOL isValid = (evalRes == kSecTrustResultProceed || evalRes == kSecTrustResultUnspecified);
    if (!isValid && expectedResult) {
        failReason = SecTrustCopyFailureDescription(trustRef);
    }
    if (reason) {
        todo(reason);
        ok(isValid == expectedResult, "%@%@",
           fileName,
           (expectedResult
            ? (failReason ? failReason : CFSTR(""))
            : CFSTR(" valid")));
    } else {
        ok(isValid == expectedResult, "%s %@%@",
           expectedResult ? "REGRESSION" : "SECURITY", fileName,
           failReason ? failReason : CFSTR(""));
    }

exit:
    CFReleaseSafe(failReason);
    CFReleaseSafe(trustRef);
    CFReleaseSafe(certRef);
}

// TODO: Export this interface in a better way.
static void runCertificateTestFor(SecPolicyRef policy,
                           NSArray* anchors,
                           NSArray* intermediates,
                           NSMutableArray* leafPaths,
                           NSDictionary* expect,
                           CFDateRef date)
{
    /* Sort the tests by name. */
    [leafPaths sortUsingSelector:@selector(compare:)];

	for (NSString* path in leafPaths) {
        NSString* fileName = [path lastPathComponent];
        runOneLeafTest(policy, anchors, intermediates, path, ![fileName hasPrefix:@"Invalid"], [expect objectForKey:fileName], date);
	}
}

void runCertificateTestForDirectory(SecPolicyRef policy, CFStringRef resourceSubDirectory, CFDateRef date)
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
	NSMutableArray* allRoots = [NSMutableArray array];
	NSMutableArray* allCAs = [NSMutableArray array];
	NSMutableArray* certTests = [NSMutableArray array];
    NSDictionary* expect = NULL;

    /* Iterate though the nist-certs resources dir. */
    NSURL* filesDirectory = [[[NSBundle mainBundle] resourceURL] URLByAppendingPathComponent:(NSString*)resourceSubDirectory];
    for (NSURL* fileURL in [[NSFileManager defaultManager] contentsOfDirectoryAtURL:filesDirectory includingPropertiesForKeys:[NSArray array] options:NSDirectoryEnumerationSkipsSubdirectoryDescendants error:nil]) {
        NSString* path = [fileURL path];
		if ([path hasSuffix:@"Cert.crt"]) {
            SecCertificateRef certRef = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
            [allCAs addObject:(id)certRef];
        } else if ([path hasSuffix:@"RootCertificate.crt"]) {
            SecCertificateRef certRef = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
            [allRoots addObject:(id)certRef];
        } else if ([path hasSuffix:@".crt"]) {
                [certTests addObject:path];
        } else if ([path hasSuffix:@".plist"]) {
            if (expect) {
                fail("Multiple .plist files found in %@", filesDirectory);
            } else {
                expect = [NSDictionary dictionaryWithContentsOfFile:path];
            }
        }
	}

    runCertificateTestFor(policy, allRoots, allCAs, certTests, expect, date);

    [pool release];
}

#endif /* TARGET_OS_IPHONE */
