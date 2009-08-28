/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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
//  auto_tester.m
//  auto
//
//  Created by Josh Behnke on 5/16/08.
//  Copyright 2008 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "AutoTestScript.h"
#import "auto_tester.h"

/* Test probe functions */
#define PROBE_IMPL(message) do { AutoTestScript *script = [AutoTestScript runningScript]; if (script) { @synchronized(script) { if ([script testRunning]) { message; } } [script probeDidComplete]; } } while (0)

void auto_probe_auto_collect(auto_collection_mode_t mode)
{
    PROBE_IMPL([script autoCollect:mode]);
}

void auto_probe_begin_heap_scan(boolean_t generational)
{
    PROBE_IMPL([script beginHeapScan:generational]);
}

void auto_probe_begin_local_scan()
{
    PROBE_IMPL([script beginLocalScan]);
}

void auto_probe_collection_complete()
{
    PROBE_IMPL([script collectionComplete]);
}

void auto_probe_end_heap_scan(size_t garbage_count, vm_address_t *garbage_blocks)
{
    PROBE_IMPL([script endHeapScanWithGarbage:(void **)garbage_blocks count:garbage_count]);
}

void auto_probe_end_local_scan(size_t garbage_count, vm_address_t *garbage_blocks)
{
    PROBE_IMPL([script endLocalScanWithGarbage:(void **)garbage_blocks count:garbage_count]);
}

void auto_scan_barrier()
{
    PROBE_IMPL([script scanBarrier]);
}

void auto_probe_end_thread_scan()
{
    PROBE_IMPL([script endThreadScan]);
}

void auto_probe_heap_collection_complete()
{
    PROBE_IMPL([script heapCollectionComplete]);
}

void auto_probe_local_collection_complete()
{
    PROBE_IMPL([script localCollectionComplete]);
}

void auto_probe_mature(void *address, unsigned char age)
{
    PROBE_IMPL([script blockMatured:address newAge:age]);
}

void auto_probe_make_global(void *address, unsigned char age)
{
    PROBE_IMPL([script blockBecameGlobal:address withAge:age]);
}

void auto_probe_scan_range(void *address, void *end)
{
    PROBE_IMPL([script scanBlock:address endAddress:end]);
}

void auto_probe_scan_with_layout(void *address, void *end, const unsigned char *map)
{
    PROBE_IMPL([script scanBlock:address endAddress:end withLayout:map]);
}

void auto_probe_did_scan_with_layout(void *address, void *end, const unsigned char *map)
{
    PROBE_IMPL([script didScanBlock:address endAddress:end withLayout:map]);
}

void auto_probe_scan_with_weak_layout(void *address, void *end, const unsigned char *map)
{
    PROBE_IMPL([script scanBlock:address endAddress:end withWeakLayout:map]);
}

void auto_probe_set_pending(void *block)
{
    PROBE_IMPL([script setPending:block]);
}

void auto_probe_unregistered_thread_error()
{
    PROBE_IMPL([script threadRegistrationError]);
}

AutoProbeFunctions auto_tester_probe_runctions = {
    auto_probe_auto_collect,
    auto_probe_begin_heap_scan,
    auto_probe_begin_local_scan,
    auto_probe_collection_complete,
    auto_probe_end_heap_scan,
    auto_probe_end_local_scan,
    auto_scan_barrier,
    auto_probe_end_thread_scan,
    auto_probe_heap_collection_complete,
    auto_probe_local_collection_complete,
    auto_probe_mature,
    auto_probe_make_global,
    auto_probe_scan_range,
    auto_probe_scan_with_layout,
    auto_probe_did_scan_with_layout,
    auto_probe_scan_with_weak_layout,
    auto_probe_set_pending,
    auto_probe_unregistered_thread_error,
};

int main(int argc, char *argv[])
{
    int passed = 0, failed = 0, repeat = 1;
    BOOL debug = NO;
    
    auto_zone_t *second_zone = auto_zone_create("auto_tester second zone");
    auto_zone_register_thread(second_zone);
    auto_zone_unregister_thread(second_zone);
    
    NSProcessInfo *pi = [NSProcessInfo processInfo];
    NSMutableArray *args = [[pi arguments] mutableCopy];
    [args removeObjectAtIndex:0];
    while ([args count] > 0) {
        int argCount = 0;
        NSString *arg = [args objectAtIndex:0];
        if ([arg isEqual:@"-debug"]) {
            argCount++;
            debug = YES;
        }
        if ([arg hasPrefix:@"-repeat="]) {
            argCount++;
            repeat = [[arg substringFromIndex:[@"-repeat=" length]] intValue];
            if (repeat < 1)
                repeat = 1;
        }
        [args removeObjectsInRange:NSMakeRange(0, argCount)];
    }
    
    // Simple test driver that just runs all the tests.
    NSArray *tests = [AutoTestScript testClasses:debug];
    
    if (!auto_set_probe_functions(&auto_tester_probe_runctions)) {
        NSLog(@"Probe callouts not supported in loaded version of libauto.");
        exit(1);
    }
    
    for (int i=0; i<repeat; i++) {
        for (Class testClass in tests) {
            AutoTestScript *test = [[testClass alloc] init];
            
            [test runTest];
            [test waitForCompletion];
            
            const char *result = [test failureMessage];
            if (result) {
                NSLog(@"%@ failed: %s", [test className], result);
                failed++;
            } else {
                NSLog(@"%@ passed", [test className]);
                passed++;
            }
        }
    }
    
    NSLog(@"Summary: %d passed, %d failed", passed, failed);
    return failed == 0 ? 0 : 1;
}
