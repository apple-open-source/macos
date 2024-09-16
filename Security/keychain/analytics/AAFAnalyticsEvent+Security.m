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

#import <SoftLinking/SoftLinking.h>
#if __has_include(<AAAFoundation/AAAFoundation.h>)
#import <AAAFoundation/AAAFoundation.h>
#endif

#if __has_include(<AuthKit/AuthKit.h>)
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>
#endif

#if __has_include(<AppleAccount/AppleAccount.h>)
#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/AppleAccount_Private.h>
#endif

#import "AAFAnalyticsEvent+Security.h"
#import "utilities/debugging.h"

#if __has_include(<AAAFoundation/AAAFoundation.h>)
SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, AAAFoundation);
SOFT_LINK_CLASS(AAAFoundation, AAFAnalyticsEvent);
SOFT_LINK_CONSTANT(AAAFoundation, kAAFDeviceSessionId, NSString*);
SOFT_LINK_CONSTANT(AAAFoundation, kAAFFlowId, NSString*)
#endif

#if __has_include(<AuthKit/AuthKit.h>)
SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, AuthKit);
SOFT_LINK_CLASS(AuthKit, AKAccountManager);
#endif

@interface AAFAnalyticsEventSecurity()
#if __has_include(<AAAFoundation/AAAFoundation.h>)
@property AAFAnalyticsEvent* event;
#endif
@property BOOL areTestsEnabled;
@property BOOL canSendMetrics;
@property BOOL isAAAFoundationAvailable;
@property BOOL isAuthKitAvailable;

@end


@implementation AAFAnalyticsEventSecurity

+ (BOOL)isAAAFoundationAvailable
{
    static BOOL available = NO;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if __has_include(<AAAFoundation/AAAFoundation.h>)
        if (isAAAFoundationAvailable()) {
            available = YES;
        } else {
            secerror("aafanalyticsevent-security: failed to softlink AAAFoundation");
        }
#else
        secnotice("aafanalyticsevent-security", "AAAFoundation unavailable on this platform");
#endif
    });

    return available;
}

+ (BOOL)isAuthKitAvailable
{
    static BOOL available = NO;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
#if __has_include(<AuthKit/AuthKit.h>)
        if (isAuthKitAvailable()) {
            available = YES;
        } else {
            secerror("aafanalyticsevent-security: failed to softlink AuthKit");
        }
#else
        secnotice("aafanalyticsevent-security", "AuthKit unavailable on this platform");
#endif
    });

    return available;
}

- (BOOL)permittedToSendMetrics
{
    if (self.isAAAFoundationAvailable == NO ||
        self.isAuthKitAvailable == NO ||
        self.areTestsEnabled ||
        self.canSendMetrics == NO) {
        return NO;
    }

    return YES;
}

#if __has_include(<AAAFoundation/AAAFoundation.h>)
+ (NSString* _Nullable)fetchDeviceSessionIDFromAuthKit:(NSString*)altDSID
{
    AKAccountManager *accountManager = [getAKAccountManagerClass() sharedInstance];

    ACAccount* acAccount = nil;
    if (altDSID == nil) {
        acAccount = [accountManager primaryAuthKitAccount];
    } else {
        NSError* localError = nil;
        acAccount = [accountManager authKitAccountWithAltDSID:altDSID error:&localError];
        if (localError) {
            secerror("authKitAccountWithAltDSID returned error: %@", localError);
        }
    }
    if ([accountManager accountAccessTelemetryOptInForAccount:acAccount]) {
        NSString* deviceSessionIDFromAuthKit = [accountManager telemetryDeviceSessionIDForAccount:acAccount];
        return deviceSessionIDFromAuthKit;
    }

    return nil;
}
#endif

- (instancetype)initWithKeychainCircleMetrics:(NSDictionary * _Nullable)metrics
                                      altDSID:(NSString * _Nullable)altDSID
                                       flowID:(NSString * _Nullable)flowID
                              deviceSessionID:(NSString * _Nullable)deviceSessionID
                                    eventName:(NSString *)eventName
                              testsAreEnabled:(BOOL)testsAreEnabled
                               canSendMetrics:(BOOL)canSendMetrics
                                     category:(NSNumber *)category
{
    // if AAAFoundation is not available or
    // if AuthKit is not available or
    // if tests are running or
    // if the device finished signing in, don't create metrics
    if ([AAFAnalyticsEventSecurity isAAAFoundationAvailable] == NO ||
        [AAFAnalyticsEventSecurity isAuthKitAvailable] == NO ||
        testsAreEnabled ||
        canSendMetrics == NO) {
        if (self = [super init]) {
            _isAAAFoundationAvailable = [AAFAnalyticsEventSecurity isAAAFoundationAvailable];
            _isAuthKitAvailable = [AAFAnalyticsEventSecurity isAuthKitAvailable];
            _areTestsEnabled = testsAreEnabled;
            _canSendMetrics = canSendMetrics;
        }
        return self;
    }

    if (self = [super init]) {
        _queue = dispatch_queue_create("com.apple.security.aafanalyticsevent.queue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _areTestsEnabled = testsAreEnabled;
        _canSendMetrics = canSendMetrics;
        _isAAAFoundationAvailable = YES;
        _isAuthKitAvailable = YES;

#if __has_include(<AAAFoundation/AAAFoundation.h>)
        AAFAnalyticsEvent *analyticsEvent = [[getAAFAnalyticsEventClass() alloc] initWithEventName:eventName
                                                                                     eventCategory:category
                                                                                          initData:nil];
        if (flowID && [flowID isEqualToString:@""] == NO) {
            analyticsEvent[getkAAFFlowId()] = flowID;
        }

        if (deviceSessionID && [deviceSessionID isEqualToString:@""] == NO) {
            analyticsEvent[getkAAFDeviceSessionId()] = deviceSessionID;
        } else {
            analyticsEvent[getkAAFDeviceSessionId()] = [AAFAnalyticsEventSecurity fetchDeviceSessionIDFromAuthKit:altDSID];
        }
        
        if (metrics) {
            for (NSString* key in metrics.allKeys) {
                analyticsEvent[key] = metrics[key];
            }
        }

        _event = analyticsEvent;
#endif
    }
    return self;
}

- (instancetype)initWithCKKSMetrics:(NSDictionary * _Nullable)metrics
                            altDSID:(NSString *)altDSID
                          eventName:(NSString *)eventName
                    testsAreEnabled:(BOOL)testsAreEnabled
                           category:(NSNumber *)category
                         sendMetric:(BOOL)sendMetric
{
    return [self initWithKeychainCircleMetrics:metrics
                                       altDSID:altDSID
                                        flowID:nil
                               deviceSessionID:nil
                                     eventName:eventName
                               testsAreEnabled:testsAreEnabled
                                canSendMetrics:sendMetric
                                      category:category];
}

- (instancetype)initWithKeychainCircleMetrics:(NSDictionary * _Nullable)metrics
                                      altDSID:(NSString * _Nullable)altDSID
                                    eventName:(NSString *)eventName
                                     category:(NSNumber *)category
{
    return [self initWithKeychainCircleMetrics:metrics 
                                       altDSID:altDSID
                                        flowID:nil 
                               deviceSessionID:nil
                                     eventName:eventName
                               testsAreEnabled:NO
                                canSendMetrics:YES
                                      category:category];
}

- (void)populateUnderlyingErrorsStartingWithRootError:(NSError* _Nullable)error 
{
    if ([self permittedToSendMetrics] == NO) {
        return;
    }

#if __has_include(<AAAFoundation/AAAFoundation.h>)
    dispatch_sync(self.queue, ^{
        [self.event populateUnderlyingErrorsStartingWithRootError:error];
    });
#endif
}

- (void)addMetrics:(NSDictionary*)metrics
{
    if ([self permittedToSendMetrics] == NO) {
        return;
    }

#if __has_include(<AAAFoundation/AAAFoundation.h>)
    dispatch_async(self.queue, ^{
        for (NSString* key in metrics.allKeys) {
            self.event[key] = metrics[key];
        }
    });
#endif
}

- (id)getEvent
{
#if __has_include(<AAAFoundation/AAAFoundation.h>)
    return self.event;
#else
    return nil;
#endif
}

@end


