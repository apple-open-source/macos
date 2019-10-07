/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#if OCTAGON


@protocol CKKSPowerEventType <NSObject>
@end
typedef NSString<CKKSPowerEventType> CKKSPowerEvent;

extern CKKSPowerEvent* const kCKKSPowerEventOutgoingQueue;
extern CKKSPowerEvent* const kCKKSPowerEventIncommingQueue;
extern CKKSPowerEvent* const kCKKSPowerEventTLKShareProcessing;
extern CKKSPowerEvent* const kCKKSPowerEventScanLocalItems;
extern CKKSPowerEvent* const kCKKSPowerEventFetchAllChanges;
extern CKKSPowerEvent* const kCKKSPowerEventReencryptOutgoing;

@protocol OTPowerEventType <NSObject>
@end
typedef NSString<OTPowerEventType> OTPowerEvent;

extern OTPowerEvent* const kOTPowerEventRestore;
extern OTPowerEvent* const kOTPowerEventEnroll;

@class CKKSOutgoingQueueEntry;

@interface CKKSPowerCollection : NSOperation

+ (void)CKKSPowerEvent:(CKKSPowerEvent *)operation zone:(NSString *)zone;
+ (void)CKKSPowerEvent:(CKKSPowerEvent *)operation zone:(NSString *)zone count:(NSUInteger)count;
+ (void)CKKSPowerEvent:(CKKSPowerEvent *)operation count:(NSUInteger)count;

+ (void)OTPowerEvent:(NSString *)operation;

- (void)storedOQE:(CKKSOutgoingQueueEntry *)oqe;
- (void)deletedOQE:(CKKSOutgoingQueueEntry *)oqe;

- (void)commit;

@end

#endif
