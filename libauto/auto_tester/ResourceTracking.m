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
//
//  ResourceTracking.m
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#import "BlackBoxTest.h"

static BOOL shouldCollect;

@interface ResourceTracking : BlackBoxTest
{
    BOOL _finalized;
    BOOL _collected;
    BOOL _resourceTrackerQueried;
}
- (void)verifyNoCollection;
- (void)verifyCollection;
@end

@implementation ResourceTracking

static char *tracker_description = "resource tracker unit test";

- (BOOL)shouldCollect
{
    boolean_t collect = shouldCollect;
    _resourceTrackerQueried = YES;
    shouldCollect = NO;
    return collect;
}

- (void)processOutputLine:(NSString *)line
{
    NSString *expectedString = @"triggering collection due to external resource tracker: resource tracker unit test";
    NSRange r = [line rangeOfString:expectedString];
    if (r.location == NSNotFound) {
        [super processOutputLine:line];
    }
}

- (void)allocate
{
    // force test object out of thread local set
    CFRelease(CFRetain([TestFinalizer new]));
}

- (void)performTest
{
    auto_zone_register_resource_tracker([self auto_zone], tracker_description, ^{ 
        return (boolean_t)[self shouldCollect];
    });
    shouldCollect = NO;
    _finalized = NO;
    _resourceTrackerQueried = NO;
    
    [self allocate];
    
    [NSThread sleepForTimeInterval:0.3]; // wait long enough that the collector will poll the resource tracker
    auto_zone_collect_and_notify([self auto_zone], AUTO_ZONE_COLLECT_NO_OPTIONS, _testQueue, ^{ [self verifyNoCollection]; } ); // should not collect
}

- (void)verifyNoCollection
{
    if (_finalized) {
        [self fail:@"unexpected collection"];
    }
    if (!_resourceTrackerQueried) {
        [self fail:@"resource tracker was not queried (no collection)"];
    }
    
    shouldCollect = YES;
    _resourceTrackerQueried = NO;
    [NSThread sleepForTimeInterval:0.3]; // wait long enough that the collector will poll the resource tracker
    auto_zone_collect_and_notify([self auto_zone], AUTO_ZONE_COLLECT_NO_OPTIONS, _testQueue, ^{ [self verifyCollection]; } ); // should collect
}

- (void)verifyCollection
{
    if (!_resourceTrackerQueried) {
        [self fail:@"resource tracker was not queried (collection)"];
    }
    if (_finalized) {
        [self passed];
    } else {
        [self fail:@"did not collect"];
    }
    auto_zone_unregister_resource_tracker([self auto_zone], tracker_description);
    [self testFinished];
}

- (void)didFinalize:(TestFinalizer *)finalizer
{
    _finalized = YES;
}

@end
