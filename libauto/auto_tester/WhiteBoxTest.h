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
//  WhiteBoxTest.h
//  Copyright (c) 2009-2011 Apple Inc. All rights reserved.
//

#import "TestCase.h"
#import "auto_tester.h"

@interface WhiteBoxTest : TestCase {

}

/*
 Methods invoked via probe points in the collector, in alphabetical order.
 Concrete subclasses override these to observe whichever probe points are needed.
 Subclasses should invoke the AutoTestScript implementations, as some invariants may be checked for all tests.
 */

/* Invoked from the auto_zpme_collect probe at the beginning of auto_zone_collect(). */
- (void)autoZoneCollect:(auto_zone_options_t)options;

/* Invoked from the admin deallocator */
- (void)adminDeallocate:(void *)address;

/* Invoked from the begin_heap_scan probe near the beginning of auto_collect_internal(). */
- (void)beginHeapScan:(BOOL)generational;

/* Invoked from the begin_local_scan probe near the beginning of ThreadLocalCollector::collect(). */
- (void)beginLocalScan;

/* Invoked from the make_global probe in Subzone::make_global(). */
- (void)blockBecameGlobal:(void *)block withAge:(uint32_t)age;

/* Invoked from the mature probe in Subzone::mature(). */
- (void)blockMatured:(void *)block newAge:(uint32_t)age;

/* Invoked at the end of collection, immediately after threads blocked waiting on synchronous collection are woken. */
- (void)collectionComplete;

/* Invoked from the end_heap_scan probe in auto_collect_internal(). Occurs after scanning is complete and the garbage list has been determined. */
- (void)endHeapScanWithGarbage:(void **)garbage_list count:(size_t)count;

/* Invoked from the end_local_scan probe in ThreadLocalCollector::process_local_garbage(). Occurs after scanning is complete and the garbage list has been determined. */
- (void)endLocalScanWithGarbage:(void **)garbage_list count:(size_t)count;

/* Invoked from the scan_barrier probe in MemoryScanner::scan(), right before the scan_barrier() is performed. */
- (void)scanBarrier;

/* Invoked from the end_thread_scan probe in MemoryScanner::scan(). Occurs after locked down thread scanning is complete. */
- (void)endThreadScan;

/* Invoked from the heap_collection_complete probe near the end of auto_collect_internal(). */
/* This is invoked _after_ the collector is reenabled so it can be used to trigger a new collection. */
- (void)heapCollectionComplete;

/* Invoked from the local_collection_complete probe at the end of ThreadLocalCollector::collect(). */
- (void)localCollectionComplete;

/* Invoked from the scan_range probe at the beginning of MemoryScanner::scan_range(). */
- (void)scanBlock:(void *)block endAddress:(void *)end;

/* Invoked from the scan_with_layout probe at the beginning of MemoryScanner::scan_with_layout(). */
- (void)scanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map;

/* Invoked from the did_scan_with_layout probe at the end of MemoryScanner::scan_with_layout(). */
- (void)didScanBlock:(void *)block endAddress:(void *)end withLayout:(const unsigned char *)map;

/* Invoked from the set_pending probe at the beginning of Zone::set_pending(). */
- (void)setPending:(void *)block;

/* Invoked from auto_zone_thread_registration_error */
- (void)threadRegistrationError;


@end
