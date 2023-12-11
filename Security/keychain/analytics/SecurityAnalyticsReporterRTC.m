/*
 * Copyright (c) 2023 Apple Inc. All Rights Reserved.
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

#import "SecurityAnalyticsReporterRTC.h"
#import "SecurityAnalyticsConstants.h"
#import <SoftLinking/SoftLinking.h>

#if __has_include(<AAAFoundation/AAAFoundation.h>)
SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, AAAFoundation);
SOFT_LINK_CLASS(AAAFoundation, AAFAnalyticsTransportRTC);
SOFT_LINK_CLASS(AAAFoundation, AAFAnalyticsReporter);
#endif

@implementation SecurityAnalyticsReporterRTC

#if __has_include(<AAAFoundation/AAAFoundation.h>)
+ (AAFAnalyticsReporter *)rtcAnalyticsReporter {
    static AAFAnalyticsReporter *rtcReporter = nil;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        AAFAnalyticsTransportRTC *transport = [getAAFAnalyticsTransportRTCClass() analyticsTransportRTCWithClientType:kSecurityRTCClientType
                                                                                                       clientBundleId:kSecurityRTCClientBundleIdentifier
                                                                                                           clientName:kSecurityRTCClientNameDNU];
        rtcReporter = [getAAFAnalyticsReporterClass() analyticsReporterWithTransport:transport];
    });
    return rtcReporter;
}
#endif

+ (void)sendMetricWithEvent:(AAFAnalyticsEventSecurity*)eventS success:(BOOL)success error:(NSError* _Nullable)error
{
#if __has_include(<AAAFoundation/AAAFoundation.h>)
    
    if (eventS.isAAAFoundationAvailable == NO) {
        return;
    }

    if (eventS.isAuthKitAvailable == NO) {
        return;
    }
    
    if (eventS.areTestsEnabled) {
        return;
    }

    dispatch_sync(eventS.queue, ^{
        AAFAnalyticsEvent* event = (AAFAnalyticsEvent*)[eventS getEvent];
        event[kSecurityRTCFieldDidSucceed] = @(success);
        [event populateUnderlyingErrorsStartingWithRootError:error];
        [[SecurityAnalyticsReporterRTC rtcAnalyticsReporter] sendEvent:event];
    });
#endif
}

@end
