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

#ifndef _OT_FOLLOWUP_H_
#define _OT_FOLLOWUP_H_

#if OCTAGON

#import <Foundation/Foundation.h>
#import <CoreCDP/CDPFollowUpController.h>
#import "keychain/ckks/CKKSAnalytics.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(uint8_t, OTFollowupContextType) {
    OTFollowupContextTypeNone,
    OTFollowupContextTypeRecoveryKeyRepair,
    OTFollowupContextTypeStateRepair,
    OTFollowupContextTypeOfflinePasscodeChange,
};
NSString* OTFollowupContextTypeToString(OTFollowupContextType contextType);

@protocol OctagonFollowUpControllerProtocol
- (BOOL)postFollowUpWithContext:(CDPFollowUpContext *)context error:(NSError **)error;
- (BOOL)clearFollowUpWithContext:(CDPFollowUpContext *)context error:(NSError **)error;
@end
@interface CDPFollowUpController (Octagon) <OctagonFollowUpControllerProtocol>
@end

@interface OTFollowup : NSObject
- (id)initWithFollowupController:(id<OctagonFollowUpControllerProtocol>)cdpFollowupController;

- (BOOL)postFollowUp:(OTFollowupContextType)contextType
               error:(NSError **)error;
- (BOOL)clearFollowUp:(OTFollowupContextType)contextType
                error:(NSError **)error;

- (NSDictionary *)sysdiagnoseStatus;
- (NSDictionary<NSString*,NSNumber*> *)sfaStatus;

@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON

#endif // _OT_FOLLOWUP_H_
