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

#if OCTAGON

#import <Foundation/Foundation.h>
#import "CKKSOutgoingQueueEntry.h"

NS_ASSUME_NONNULL_BEGIN

@interface CKKSRateLimiter : NSObject <NSSecureCoding>

@property (readonly) NSDictionary* config;  // of NSString : NSNumber

/*!
 * @brief Find out whether outgoing items are okay to send.
 * @param entry The outgoing object being judged.
 * @param time Current time.
 * @param limitTime In case of badness, this will contain the time at which the object may be sent.
 * @return Badness score from 0 (fine to send immediately) to 5 (overload, keep back), or -1 in case caller does not provide an NSDate object.
 *
 * judge:at: will set the limitTime object to nil in case of 0 badness. For badnesses 1-4 the time object will indicate when it is okay to send the entry.
 * At badness 5 judge:at: has determined there is too much activity so the caller should hold off altogether. The limitTime object will indicate when
 * this overloaded state will end.
 */
- (int)judge:(CKKSOutgoingQueueEntry* const)entry
           at:(NSDate*)time
    limitTime:(NSDate* _Nonnull __autoreleasing* _Nonnull)limitTime;

- (instancetype)init;
- (instancetype _Nullable)initWithCoder:(NSCoder* _Nullable)coder NS_DESIGNATED_INITIALIZER;
- (NSUInteger)stateSize;
- (void)reset;
- (NSString*)diagnostics;

+ (BOOL)supportsSecureCoding;

@end

NS_ASSUME_NONNULL_END
#endif
