/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import "OctagonTrustTests.h"

#if OCTAGON

NS_ASSUME_NONNULL_BEGIN


@implementation OctagonTrustTests (OctagonTrustTestsErrors)
- (void) testDepthCountOnErrors
{
    SecErrorSetOverrideNestedErrorCappingIsEnabled(true);
    //previousError == nil case
    CFErrorRef errorWithoutPreviousSet = NULL;
    SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, &errorWithoutPreviousSet, NULL, CFSTR("no previous error"));
    XCTAssertNotNil((__bridge NSError*) errorWithoutPreviousSet, "errorWithoutPreviousSet should not be nil");
    NSDictionary *userInfo = [(__bridge NSError*)errorWithoutPreviousSet userInfo];
    XCTAssertNotNil(userInfo, "userInfo should not be nil");
    NSNumber *depth = userInfo[@"numberOfErrorsDeep"];
    XCTAssertNotNil(depth, "depth should not be nil");
    XCTAssertEqual([depth longValue], 0, "depth should be 0");

    //previousError is set with a depth of 0
    CFErrorRef newError = NULL;
    SOSCreateErrorWithFormat(kSOSErrorNoCircle, errorWithoutPreviousSet, &newError, NULL, CFSTR("previousError exists!"));
    XCTAssertNotNil((__bridge NSError*) newError, "newError should not be nil");
    userInfo = [(__bridge NSError*)newError userInfo];
    XCTAssertNotNil(userInfo, "userInfo should not be nil");
    depth = userInfo[@"numberOfErrorsDeep"];
    XCTAssertNotNil(depth, "depth should not be nil");
    XCTAssertEqual([depth longValue], 1, "depth should be 1");

    //previousError is set with a dpeth of 1
    CFErrorRef secondError = NULL;
    SOSCreateErrorWithFormat(kSOSErrorNoCircle, newError, &secondError, NULL, CFSTR("using previous error that already has a chain"));
    XCTAssertNotNil((__bridge NSError*) secondError, "secondError should not be nil");
    userInfo = [(__bridge NSError*)secondError userInfo];
    XCTAssertNotNil(userInfo, "userInfo should not be nil");
    depth = userInfo[@"numberOfErrorsDeep"];
    XCTAssertNotNil(depth, "depth should not be nil");
    XCTAssertEqual([depth longValue], 2, "depth should be 2");
}
- (void) testErrorCap
{
    SecErrorSetOverrideNestedErrorCappingIsEnabled(true);
    //previousError == nil case
    CFErrorRef previous= NULL;
    SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, &previous, NULL, CFSTR("no previous error"));
    XCTAssertNotNil((__bridge NSError*) previous, "errorWithoutPreviousSet should not be nil");
    NSDictionary *userInfo = [(__bridge NSError*)previous userInfo];
    XCTAssertNotNil(userInfo, "userInfo should not be nil");
    NSNumber *depth = userInfo[@"numberOfErrorsDeep"];
    XCTAssertNotNil(depth, "depth should not be nil");
    XCTAssertEqual([depth longValue], 0, "depth should be 0");

    //stay within the cap limit
    CFErrorRef newError = NULL;
    for(int i = 0; i<200; i++) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, previous, &newError, NULL, CFSTR("previousError exists %d!"), i);
        XCTAssertNotNil((__bridge NSError*) newError, "newError should not be nil");
        userInfo = [(__bridge NSError*)newError userInfo];
        XCTAssertNotNil(userInfo, "userInfo should not be nil");
        depth = userInfo[@"numberOfErrorsDeep"];
        XCTAssertNotNil(depth, "depth should not be nil");
        XCTAssertEqual([depth longValue], i+1, "depth should be i+1");
        previous = newError;
        newError = nil;
    }

    //now blow the cap limit
    previous = NULL;
    newError = NULL;
    int depthCounter = 0;

    for(int i = 0; i < 500; i++) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, previous, &newError, NULL, CFSTR("previousError exists %d!"), i);
        XCTAssertNotNil((__bridge NSError*) newError, "newError should not be nil");
        userInfo = [(__bridge NSError*)newError userInfo];
        XCTAssertNotNil(userInfo, "userInfo should not be nil");
        depth = userInfo[@"numberOfErrorsDeep"];
        if (i >= 201) {
            NSDictionary* previousUserInfo = [(__bridge NSError*)previous userInfo];
            NSDictionary* newErrorUserInfo = [(__bridge NSError*)newError userInfo];
            XCTAssertTrue([previousUserInfo isEqualToDictionary:newErrorUserInfo], "newError should equal previous error");
        }

        if (i <= 199) {
            XCTAssertNotNil(depth, "depth should not be nil");
            XCTAssertEqual([depth longValue], depthCounter, "depth should be %d", depthCounter);
            depthCounter++;
        } else {
            userInfo = [(__bridge NSError*)newError userInfo];
            depth = userInfo[@"numberOfErrorsDeep"];
            XCTAssertNotNil(depth, "depth should not be nil");
            XCTAssertEqual([depth longValue], 200, "depth should be 200");
        }
        
        previous = newError;
        newError = nil;
    }
}

- (void) testErrorCapWithFeatureFlagDisable
{
    SecErrorSetOverrideNestedErrorCappingIsEnabled(false);
    //previousError == nil case
    CFErrorRef previous= NULL;
    SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, &previous, NULL, CFSTR("no previous error"));
    XCTAssertNotNil((__bridge NSError*) previous, "errorWithoutPreviousSet should not be nil");
    NSDictionary *userInfo = [(__bridge NSError*)previous userInfo];
    XCTAssertNotNil(userInfo, "userInfo should not be nil");
    NSNumber *depth = userInfo[@"numberOfErrorsDeep"];
    XCTAssertNotNil(depth, "depth should not be nil");
    XCTAssertEqual([depth longValue], 0, "depth should be 0");

    //stay within the cap limit
    CFErrorRef newError = NULL;
    for(int i = 0; i<200; i++) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, previous, &newError, NULL, CFSTR("previousError exists %d!"), i);
        XCTAssertNotNil((__bridge NSError*) newError, "newError should not be nil");
        userInfo = [(__bridge NSError*)newError userInfo];
        XCTAssertNotNil(userInfo, "userInfo should not be nil");
        depth = userInfo[@"numberOfErrorsDeep"];
        XCTAssertNotNil(depth, "depth should not be nil");
        XCTAssertEqual([depth longValue], i+1, "depth should be i+1");
        previous = newError;
        newError = nil;
    }

    //now blow the cap limit
    previous = NULL;
    newError = NULL;

    for(int i = 0; i < 500; i++) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, previous, &newError, NULL, CFSTR("previousError exists %d!"), i);
        XCTAssertNotNil((__bridge NSError*) newError, "newError should not be nil");
        userInfo = [(__bridge NSError*)newError userInfo];
        XCTAssertNotNil(userInfo, "userInfo should not be nil");
        depth = userInfo[@"numberOfErrorsDeep"];
        XCTAssertNotNil(depth, "depth should not be nil");
        XCTAssertEqual([depth longValue], i, "depth should be %d", i);
        previous = newError;
        newError = nil;
    }
}

- (void) testNestedErrorCreationUsingSameErrorParameters
{
    SecErrorSetOverrideNestedErrorCappingIsEnabled(true);
    //previousError == nil case
    CFErrorRef previous= NULL;
    SOSCreateErrorWithFormat(kSOSErrorNoCircle, NULL, &previous, NULL, CFSTR("no previous error"));
    XCTAssertNotNil((__bridge NSError*) previous, "previous should not be nil");
    NSDictionary *userInfo = [(__bridge NSError*)previous userInfo];
    XCTAssertNotNil(userInfo, "userInfo should not be nil");
    NSNumber *depth = userInfo[@"numberOfErrorsDeep"];
    XCTAssertNotNil(depth, "depth should not be nil");
    XCTAssertEqual([depth longValue], 0, "depth should be 0");

    //now pass the exact same error
    CFErrorRef newError = NULL;
    SOSCreateErrorWithFormat(kSOSErrorNoCircle, previous, &newError, NULL, CFSTR("no previous error"));
    XCTAssertNotNil((__bridge NSError*) newError, "newError should not be nil");
    userInfo = [(__bridge NSError*)newError userInfo];
    XCTAssertNotNil(userInfo, "userInfo should not be nil");
    depth = userInfo[@"numberOfErrorsDeep"];
    XCTAssertNotNil(depth, "depth should not be nil");
    XCTAssertEqual([depth longValue], 0, "depth should be 0");

    previous = newError;
    newError = NULL;
    SOSCreateErrorWithFormat(kSOSErrorNoCircle, previous, &newError, NULL, CFSTR("no previous error1"));
    XCTAssertNotNil((__bridge NSError*) newError, "newError should not be nil");
    userInfo = [(__bridge NSError*)newError userInfo];
    XCTAssertNotNil(userInfo, "userInfo should not be nil");
    depth = userInfo[@"numberOfErrorsDeep"];
    XCTAssertNotNil(depth, "depth should not be nil");
    XCTAssertEqual([depth longValue], 1, "depth should be 1");

    previous = newError;
    newError = NULL;
    for(int i = 0; i < 500; i++) {
        SOSCreateErrorWithFormat(kSOSErrorNoCircle, previous, &newError, NULL, CFSTR("no previous error1"));
        XCTAssertNotNil((__bridge NSError*) newError, "newError should not be nil");
        userInfo = [(__bridge NSError*)newError userInfo];
        XCTAssertNotNil(userInfo, "userInfo should not be nil");
        depth = userInfo[@"numberOfErrorsDeep"];
        XCTAssertNotNil(depth, "depth should not be nil");
        XCTAssertEqual([depth longValue], 1, "depth should be 1");
        previous = newError;
        newError = nil;
    }
}

@end
NS_ASSUME_NONNULL_END

#endif
