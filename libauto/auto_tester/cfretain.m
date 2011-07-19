/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */

#import "BlackBoxTest.h"

/* This test monitors stderr for unexpected output during CFRetain/CFRelease */


@interface _CFRetainTestObject : NSObject {
    NSString *string;
}
- (void)makeGlobal;
@end

static id global;

@implementation _CFRetainTestObject

- init {
    string = [@"string" mutableCopy];
    return self;
}

- (void)finalize {
    for (int i = 0; i < 10; ++i) {
        CFRetain((CFTypeRef)self);
    }
    for (int i = 0; i < 10; ++i) {
        CFRelease((CFTypeRef)self);
    }
    for (int i = 0; i < 10; ++i) {
        CFRetain((CFTypeRef)string);
    }
    for (int i = 0; i < 10; ++i) {
        CFRelease((CFTypeRef)string);
    }
    [super finalize];
}

- (void)makeGlobal {
    global = self;
    global = nil;
}

@end


@interface CFRetainTest : BlackBoxTest
{
}
@end

@implementation CFRetainTest

- (id)init
{
    self = [super init];
    if (self) {
    }
    return self;
}

- (void)performTest
{
    // make sure generic CF style objects can CFRetain/CFRelease
    // This test just looks for console warning messages.
    
    NSObject *string = (NSObject *)CFStringCreateCopy(NULL,(CFStringRef)@"hello");
    for (int i = 0; i < 101; ++i) {
        CFRetain((CFTypeRef)string);
    }
    for (int i = 0; i < 101; ++i) {
        CFRelease((CFTypeRef)string);
    }
    CFRelease(string);
    
    for (int i = 0; i < 101; ++i) {
        _CFRetainTestObject *to = [[_CFRetainTestObject alloc] init];
    }
    for (int i = 0; i < 0; ++i) {
        _CFRetainTestObject *to = [[_CFRetainTestObject alloc] init];
        [to makeGlobal];
    }
        
    [self passed];
    [self testFinished];
}
    
@end
