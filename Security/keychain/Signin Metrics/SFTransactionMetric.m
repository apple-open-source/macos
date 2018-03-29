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

#if __OBJC2__

#import "SFTransactionMetric.h"
#import <os/log_private.h>

@interface SFTransactionMetric ()
@property (nonatomic, copy) NSString *uuid;
@property (nonatomic, copy) NSString *category;
@property (nonatomic, strong) os_log_t logObject;

-(os_log_t) sftmCreateLogCategory:(NSString*) category;
-(os_log_t) sftmObjectForCategory:(NSString*) category;
@end

static NSMutableDictionary *logObjects;
static const NSString* signInLogSpace = @"com.apple.security.wiiss";

@implementation SFTransactionMetric

+ (BOOL)supportsSecureCoding {
    return YES;
}

-(os_log_t) sftmObjectForCategory:(NSString*) category
{
    return logObjects[category];
}

-(os_log_t) sftmCreateLogCategory:(NSString*) category
{
    return os_log_create([signInLogSpace UTF8String], [category UTF8String]);
}

- (instancetype)initWithUUID:(NSString *)uuid category:(NSString *)category
{
    self = [super init];
    if (self) {
        _uuid = uuid;
        _category = category;

        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            logObjects = [NSMutableDictionary dictionary];
        });
        @synchronized(logObjects){
            if(category){
                _logObject = [self sftmObjectForCategory:category];

                if(!_logObject){
                    _logObject = [self sftmCreateLogCategory:category];
                    [logObjects setObject:_logObject forKey:category];
                }
            }
        }
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_uuid forKey:@"UUID"];
    [coder encodeObject:_category forKey:@"category"];
}

- (nullable instancetype)initWithCoder:(NSCoder *)decoder
{
    self = [super init];
    if (self) {
        _uuid = [decoder decodeObjectOfClass:[NSString class] forKey:@"UUID"];
        _category = [decoder decodeObjectOfClass:[NSString class] forKey:@"category"];
    }
    return self;
}

- (void)logEvent:(NSString*)eventName eventAttributes:(NSDictionary<NSString*, id>*)attributes
{
    [attributes enumerateKeysAndObjectsUsingBlock:^(NSString* key, id obj, BOOL * stop) {
        os_log(self.logObject, "event: %@, %@  :  %@", eventName, key, obj);
    }];
}

- (void)timeEvent:(NSString*)eventName blockToTime:(void(^)(void))blockToTime
{
    NSDate *firstTime = [NSDate date];
    
    blockToTime();

    NSDate *SecondTime = [NSDate date];
    
    os_log(self.logObject, "event: %@, Time elapsed: %@", eventName, [[NSString alloc] initWithFormat:@"%f", [SecondTime timeIntervalSinceDate:firstTime]]);
}

- (void)logError:(NSError*)error
{
    os_log_error(self.logObject, "%@", error);
}

- (void)signInCompleted
{
    //print final
    os_log(self.logObject, "sign in complete for %@", self.uuid);
}

@end
#endif
