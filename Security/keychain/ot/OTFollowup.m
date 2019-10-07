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
 */

#if OCTAGON

#import "OTFollowup.h"

#if __has_include(<CoreFollowUp/CoreFollowUp.h>) && !TARGET_OS_SIMULATOR
#import <CoreFollowUp/CoreFollowUp.h>
#define HAVE_COREFOLLOW_UP 1
#endif

#undef HAVE_COREFOLLOW_UP // XXX

#import <CoreCDP/CDPFollowUpController.h>
#import <CoreCDP/CDPFollowUpContext.h>

#include "utilities/debugging.h"

static NSString * const kOTFollowupEventCompleteKey = @"OTFollowupContextType";

@interface OTFollowup()
@property id<OctagonFollowUpControllerProtocol> cdpd;
@property NSTimeInterval previousFollowupEnd;
@property NSTimeInterval followupStart;
@property NSTimeInterval followupEnd;
@end

@implementation OTFollowup : NSObject

- (id)initWithFollowupController:(id<OctagonFollowUpControllerProtocol>)cdpFollowupController
{
    if (self = [super init]) {
        self.cdpd = cdpFollowupController;
    }
    return self;
}

- (CDPFollowUpContext *)createCDPFollowupContext:(OTFollowupContextType)contextType
{
    switch (contextType) {
        case OTFollowupContextTypeStateRepair: {
            return [CDPFollowUpContext contextForStateRepair];
        }
        case OTFollowupContextTypeRecoveryKeyRepair: {
            return [CDPFollowUpContext contextForRecoveryKeyRepair];
        }
        case OTFollowupContextTypeOfflinePasscodeChange: {
            return [CDPFollowUpContext contextForOfflinePasscodeChange];
        }
        default: {
            return nil;
        }
    }
}

- (BOOL)postFollowUp:(OTFollowupContextType)contextType
               error:(NSError **)error
{
    CDPFollowUpContext *context = [self createCDPFollowupContext:contextType];
    if (!context) {
        return NO;
    }

    NSError *followupError = nil;
    BOOL result = [self.cdpd postFollowUpWithContext:context error:&followupError];
    if (error) {
        *error = followupError;
    }

    return result;
}

- (BOOL)clearFollowUp:(OTFollowupContextType)contextType
                error:(NSError **)error
{
    // Note(caw): we don't track metrics for clearing CFU prompts.
    CDPFollowUpContext *context = [self createCDPFollowupContext:contextType];
    if (!context) {
        return NO;
    }

    return [self.cdpd clearFollowUpWithContext:context error:error];
}


- (NSDictionary *)sysdiagnoseStatus
{
    NSMutableDictionary *pendingCFUs = nil;

#if HAVE_COREFOLLOW_UP
    if ([FLFollowUpController class]) {
        NSError *error = nil;
        pendingCFUs = [NSMutableDictionary dictionary];

        FLFollowUpController *followUpController = [[FLFollowUpController alloc] initWithClientIdentifier:@"com.apple.corecdp"];
        NSArray <FLFollowUpItem*>* followUps = [followUpController pendingFollowUpItems:&error];
        if (error) {
            secnotice("octagon", "Fetching pending follow ups failed with: %@", error);
            pendingCFUs[@"error"] = [error description];
        }
        for (FLFollowUpItem *followUp in followUps) {
            NSDate *creationDate = followUp.notification.creationDate;
            pendingCFUs[followUp.uniqueIdentifier] = creationDate;
        }
    }
#endif
    return pendingCFUs;
}

- (NSDictionary<NSString*,NSNumber *> *)sfaStatus {
    NSMutableDictionary<NSString*, NSNumber*>* values = [NSMutableDictionary dictionary];
#if HAVE_COREFOLLOW_UP
    if ([FLFollowUpController class]) {
        NSError *error = nil;

        //pretend to be CDP
        FLFollowUpController *followUpController = [[FLFollowUpController alloc] initWithClientIdentifier:@"com.apple.corecdp"];

        NSArray <FLFollowUpItem*>* followUps = [followUpController pendingFollowUpItems:&error];
        if (error) {
            secnotice("octagon", "Fetching pending follow ups failed with: %@", error);
        }
        for (FLFollowUpItem *followUp in followUps) {
            NSInteger created = 10000;

            NSDate *creationDate = followUp.notification.creationDate;
            if (creationDate) {
                created = [CKKSAnalytics fuzzyDaysSinceDate:creationDate];
            }
            NSString *key = [NSString stringWithFormat:@"OACFU-%@", followUp.uniqueIdentifier];
            values[key] = @(created);
        }
    }
#endif
    return values;
}


@end

#endif // OCTAGON
