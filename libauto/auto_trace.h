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
/*
    auto_trace.h
    DTrace support.
    Copyright (c) 2006-2008 Apple Inc. All rights reserved.
 */

#include <sys/cdefs.h>
#include <stddef.h>
#include "auto_zone.h"

__BEGIN_DECLS

typedef enum {
    AUTO_TRACE_SCANNING_PHASE = 0,
    AUTO_TRACE_WEAK_REFERENCE_PHASE,
    AUTO_TRACE_FINALIZING_PHASE,
    AUTO_TRACE_SCAVENGING_PHASE
} auto_collection_phase_t;

typedef enum {
	AUTO_TRACE_FULL = 0,
	AUTO_TRACE_GENERATIONAL = 1,
	AUTO_TRACE_LOCAL = 2
} auto_collection_type_t;

typedef struct {
    uint32_t size; // size of this structure
    void (*auto_trace_collection_begin)(auto_zone_t *zone, boolean_t generational);
    void (*auto_trace_collection_end)(auto_zone_t *zone, boolean_t generational, size_t objectsReclaimed, size_t bytesReclaimed, size_t totalObjectsInUse, size_t totalBytesInUse);
} auto_trace_collection_callouts;

void auto_trace_collection_set_callouts(auto_trace_collection_callouts *new_callouts);

__END_DECLS
