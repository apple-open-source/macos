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
//  WhiteBoxTest.m
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#import "WhiteBoxTest.h"

@implementation WhiteBoxTest

static BOOL _skip;

/* Test probe functions */
#define PROBE_IMPL(message) do { WhiteBoxTest *script = [TestCase currentTestCase]; if (script) { message;}} while (0)

static void auto_probe_auto_zone_collect(auto_zone_options_t options)
{
    PROBE_IMPL([script autoZoneCollect:options]);
}

static void auto_probe_admin_deallocate(void *address)
{
    PROBE_IMPL([script adminDeallocate:address]);
}

static void auto_probe_begin_heap_scan(boolean_t generational)
{
    PROBE_IMPL([script beginHeapScan:generational]);
}

static void auto_probe_begin_local_scan()
{
    PROBE_IMPL([script beginLocalScan]);
}

static void auto_probe_collection_complete()
{
    PROBE_IMPL([script collectionComplete]);
}

static void auto_probe_end_heap_scan(size_t garbage_count, void **garbage_blocks)
{
    PROBE_IMPL([script endHeapScanWithGarbage:garbage_blocks count:garbage_count]);
}

static void auto_probe_end_local_scan(size_t garbage_count, void **garbage_blocks)
{
    PROBE_IMPL([script endLocalScanWithGarbage:garbage_blocks count:garbage_count]);
}

static void auto_scan_barrier()
{
    PROBE_IMPL([script scanBarrier]);
}

static void auto_probe_end_thread_scan()
{
    PROBE_IMPL([script endThreadScan]);
}

static void auto_probe_heap_collection_complete()
{
    PROBE_IMPL([script heapCollectionComplete]);
}

static void auto_probe_local_collection_complete()
{
    PROBE_IMPL([script localCollectionComplete]);
}

static void auto_probe_mature(void *address, unsigned char age)
{
    PROBE_IMPL([script blockMatured:address newAge:age]);
}

static void auto_probe_make_global(void *address, unsigned char age)
{
    PROBE_IMPL([script blockBecameGlobal:address withAge:age]);
}

static void auto_probe_scan_range(void *address, void *end)
{
    PROBE_IMPL([script scanBlock:address endAddress:end]);
}

static void auto_probe_scan_with_layout(void *address, void *end, const unsigned char *map)
{
    PROBE_IMPL([script scanBlock:address endAddress:end withLayout:map]);
}

static void auto_probe_did_scan_with_layout(void *address, void *end, const unsigned char *map)
{
    PROBE_IMPL([script didScanBlock:address endAddress:end withLayout:map]);
}

static void auto_probe_set_pending(void *block)
{
    PROBE_IMPL([script setPending:block]);
}

static void auto_probe_unregistered_thread_error()
{
    PROBE_IMPL([script threadRegistrationError]);
}

AutoProbeFunctions auto_tester_probe_runctions = {
    auto_probe_auto_zone_collect,
    auto_probe_admin_deallocate,
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
    auto_probe_set_pending,
    auto_probe_unregistered_thread_error,
};


+ (void)initialize
{
    if ([WhiteBoxTest class] == self) {
        if (!auto_set_probe_functions(&auto_tester_probe_runctions)) {
            _skip = YES;
        } else {
            auto_set_probe_functions(NULL);
        }
    }
}

- (NSString *)shouldSkip
{
    if (_skip)
        return @"Probe callouts not present in loaded version of libauto.";
    else
        return [super shouldSkip];
}

- (void)startTest
{
    auto_set_probe_functions(&auto_tester_probe_runctions);
    [super startTest];
}

- (void)testFinished
{
    auto_set_probe_functions(NULL);
    [super testFinished];
}

/*
 Methods invoked via probe points in the collector, in alphabetical order.
 */
- (void)autoZoneCollect:(auto_zone_options_t)options
{
}

- (void)adminDeallocate:(void *)address
{
}

- (void)beginHeapScan:(BOOL)generational
{
}

- (void)beginLocalScan
{
}

- (void)blockBecameGlobal:(void *)block withAge:(uint32_t)age
{
}

- (void)blockMatured:(void *)block newAge:(uint32_t)age
{
}

- (void)collectionComplete
{
}

- (void)endHeapScanWithGarbage:(void **)garbage_list count:(size_t)count
{
}

- (void)endLocalScanWithGarbage:(void **)garbage_list count:(size_t)count
{
}

- (void)scanBarrier
{
}

- (void)endThreadScan
{
}

- (void)heapCollectionComplete
{
}

- (void)localCollectionComplete
{
}

- (void)scanBlock:(void *)block endAddress:(void *)end
{
}

- (void)scanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map
{
}

- (void)didScanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map
{
}

- (void)setPending:(void *)block
{
}

- (void)threadRegistrationError
{
}

@end
