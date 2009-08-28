/*
 *  auto_trace.d
 *  auto
 *
 *  Created by Daniel Delwood on 3/25/09.
 *  Copyright 2009 Apple Inc. All rights reserved.
 *
 */

// First define malloc_zone_t's
#include <malloc/malloc.h>

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
};
