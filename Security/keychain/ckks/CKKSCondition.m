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

#import "keychain/ckks/CKKSCondition.h"

@interface CKKSCondition ()
@property dispatch_semaphore_t semaphore;
@property CKKSCondition* chain;
@end

@implementation CKKSCondition

-(instancetype)init {
    return [self initToChain:nil];
}

-(instancetype)initToChain:(CKKSCondition*)chain
{
    if((self = [super init])) {
        _semaphore = dispatch_semaphore_create(0);
        _chain = chain;
    }
    return self;
}

-(void)fulfill {
    dispatch_semaphore_signal(self.semaphore);
    [self.chain fulfill];
    self.chain = nil; // break the retain, since that condition is filled
}

-(long)wait:(uint64_t)timeout {
    long result = dispatch_semaphore_wait(self.semaphore, dispatch_time(DISPATCH_TIME_NOW, timeout));

    // If we received a go-ahead from the semaphore, replace the signal
    if(0 == result) {
        dispatch_semaphore_signal(self.semaphore);
    }

    return result;
}

@end

