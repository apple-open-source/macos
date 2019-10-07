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

#import "CKKSPowerCollection.h"
#import "CKKSOutgoingQueueEntry.h"
#import "SecPLWrappers.h"

#if OCTAGON

CKKSPowerEvent* const kCKKSPowerEventOutgoingQueue = (CKKSPowerEvent*)@"processOutgoingQueue";
CKKSPowerEvent* const kCKKSPowerEventIncommingQueue = (CKKSPowerEvent*)@"processIncomingQueue";
CKKSPowerEvent* const kCKKSPowerEventTLKShareProcessing = (CKKSPowerEvent*)@"TLKShareProcessing";
CKKSPowerEvent* const kCKKSPowerEventScanLocalItems = (CKKSPowerEvent*)@"scanLocalItems";
CKKSPowerEvent* const kCKKSPowerEventFetchAllChanges = (CKKSPowerEvent*)@"fetchAllChanges";
CKKSPowerEvent* const kCKKSPowerEventReencryptOutgoing = (CKKSPowerEvent *)@"reencryptOutgoing";

OTPowerEvent* const kOTPowerEventRestore = (OTPowerEvent *)@"restoreBottledPeer";
OTPowerEvent* const kOTPowerEventEnroll = (OTPowerEvent *)@"enrollBottledPeer";


@interface CKKSPowerCollection ()
@property (strong) NSMutableDictionary<NSString *,NSNumber *> *store;
@property (strong) NSMutableDictionary<NSString *,NSNumber *> *delete;
@end

@implementation CKKSPowerCollection

+ (void)CKKSPowerEvent:(CKKSPowerEvent *)operation zone:(NSString *)zone
{
    SecPLLogRegisteredEvent(@"CKKSSyncing", @{
        @"operation" : operation,
        @"zone" : zone
    });
}

+ (void)CKKSPowerEvent:(CKKSPowerEvent *)operation zone:(NSString *)zone count:(NSUInteger)count
{
    SecPLLogRegisteredEvent(@"CKKSSyncing", @{
        @"operation" : operation,
        @"zone" : zone,
        @"count" : @(count)
    });
}

+ (void)CKKSPowerEvent:(CKKSPowerEvent *)operation count:(NSUInteger)count
{
    SecPLLogRegisteredEvent(@"CKKSSyncing", @{
                                              @"operation" : operation,
                                              @"count" : @(count)
                                              });
}

+ (void)OTPowerEvent:(NSString *)operation
{
    SecPLLogRegisteredEvent(@"OctagonTrust", @{
        @"operation" : operation
    });
}

- (instancetype)init
{
    if ((self = [super init]) != nil) {
        _store = [NSMutableDictionary dictionary];
        _delete = [NSMutableDictionary dictionary];
    }
    return self;
}

- (void)addToStatsDictionary:(NSMutableDictionary *)stats key:(NSString *)key
{
    if(!key) {
        key = @"access-group-missing";
    }
    NSNumber *number = stats[key];
    stats[key] = @([number longValue] + 1);
}

- (void)storedOQE:(CKKSOutgoingQueueEntry *)oqe
{
    [self addToStatsDictionary:_store key:oqe.accessgroup];
}
- (void)deletedOQE:(CKKSOutgoingQueueEntry *)oqe
{
    [self addToStatsDictionary:_delete key:oqe.accessgroup];
}

-(void)summary:(NSString *)operation stats:(NSDictionary<NSString *,NSNumber *> *)stats
{
    for (NSString *accessGroup in stats) {
        SecPLLogRegisteredEvent(@"CKKSSyncing", @{
            @"operation" : operation,
            @"accessgroup" : accessGroup,
            @"items" : stats[accessGroup]
        });
    }
}

- (void)commit
{
    [self summary:@"store" stats:_store];
    [self summary:@"delete" stats:_delete];
}

@end

#endif
