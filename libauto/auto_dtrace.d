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
/*
    auto_trace.d
    DTrace support.
    Copyright (c) 2006-2011 Apple Inc. All rights reserved.
 */

// First define malloc_zone_t's
//#include <malloc/malloc.h> // don't include directly <rdar://problem/7823490>
struct _malloc_zone_t;
typedef struct _malloc_zone_t malloc_zone_t;

// Don't actually let "auto_zone.h" be included becuase it would bring in <sys/types>;
// An auto_zone_t is typedef'd to a malloc_zone_t anyways.
#define	__AUTO_ZONE__
#define auto_zone_t malloc_zone_t

// Include the collection_phase_t flags
#include <stdint.h>
#include "auto_trace.h"

provider garbage_collection {
	probe collection_begin(auto_zone_t *zone, auto_collection_type_t collection_type);
	probe collection_end(auto_zone_t *zone, uint64_t objects_reclaimed, uint64_t bytes_reclaimed, uint64_t total_objects_in_use, uint64_t total_bytes_in_use);
	
	probe collection_phase_begin(auto_zone_t *zone, auto_collection_phase_t phase);
	probe collection_phase_end(auto_zone_t *zone, auto_collection_phase_t phase, uint64_t number_affected, uint64_t bytes_affected);
    probe auto_refcount_one_allocation(uint64_t size);
    probe auto_block_lost_thread_locality(void *block, uint64_t size);
};
