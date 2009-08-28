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
    AutoRegion.cpp
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "AutoConfiguration.h"
#include "AutoDefs.h"
#include "AutoEnvironment.h"
#include "AutoRegion.h"
#include "AutoZone.h"

namespace Auto {


    //----- Region -----//
    
    
    //
    // new_region
    //
    // Construct and initialize a new region.
    // First we get the memory that we will parcel out, then we build up the Region object itself
    //
    Region *Region::new_region(Zone *zone) {
        usword_t allocation_size;                           // size of subzone region
        void *allocation_address = NULL;                    // address of subzone region
        unsigned nzones;

#if UseArena
        // take half the space for small/medium.  A better scheme might, in effect, preallocate the entire space,
        // and then guard crossing into the large space in add_subzone.  The top of the large area would be before
        // the bitmaps - stealing from the top subzones.
        // For now, take half.  Chances are there won't be enough physical memory to exhaust this before swapping.
        nzones = 1 << (arena_size_log2 - subzone_quantum_log2 - 1);
        allocation_size = managed_size(nzones);
        allocation_address = zone->arena_allocate_region(allocation_size);  // only succeeds once
#else
        // try to allocate a region until we get space
        for (unsigned n = initial_subzone_count; n >= initial_subzone_min_count && !allocation_address; n--) {
            // size of next attempt
            allocation_size = managed_size(n);            
            // attempt to allocate data that size
            allocation_address = allocate_memory(allocation_size, subzone_quantum, VM_MEMORY_MALLOC_SMALL);
            nzones = n;
        }
#endif

        // handle error
        if (!allocation_address) {
            error("Can not allocate new region");
            return NULL;
        }
        
        // create region and admin data
        Region *region = new Region(zone, allocation_address, allocation_size, nzones);
        
        if (!region) {
            error("Can not allocate new region");
            zone->arena_deallocate(allocation_address, allocation_size);
        }
        
        return region;
    }


    // 
    // Constructor
    //
    Region::Region(Zone *zone, void *address, usword_t size, usword_t nsubzones) {
        // allocation may fail (and we don't want to use throw)
        if (!this) return;
        
        // initialize
        
        // remember our total size before lopping off pending/stack and mark bit space
        set_range(address, size);
        _next = NULL;
        _zone = zone;
        _subzone_lock = 0;
        
        // grab space for use for scanning/stack
        // We need, worse case, 1 bit per smallest quantum (16 bytes), so this should
        // have an assert of something like size/allocate_quantum_small/8
        unsigned long bytes_per_bitmap = nsubzones << subzone_bitmap_bytes_log2;
        size -= bytes_per_bitmap;
        Statistics &zone_stats = _zone->statistics();
        zone_stats.add_admin(bytes_per_bitmap);
        // we prefer to use the stack, but if it overflows, we have (virtual) space enough
        // to do a pending bit iterative scan as fallback
        _scan_space.set_range(displace(address, size), bytes_per_bitmap);
        // start out as zero length; adding subzones will grow it
        _pending.set_address(_scan_space.address());
        _pending.set_size(0);
        
        // the scanning thread needs exclusive access to the mark bits so they are indpendent
        // of other 'admin' data.  Reserve enough space for the case of all subzones being of smallest quanta.
        size -= bytes_per_bitmap;
        zone_stats.add_admin(bytes_per_bitmap);
        _marks.set_address(displace(address, size));
        _marks.set_size(0);
        
        // track number of subzones
        _i_subzones = 0;
        _n_subzones = size >> subzone_quantum_log2;
        if (_n_subzones != nsubzones) {
            // we could, in principle, compute the 'tax' of the bitmaps as a percentage and then confirm that
            // size-tax is a multiple of subzone size.  Easier to pass in the 'nsubzones' and confirm.
            //printf("size %lx, subzones %d, nsubzones %d\n", size, (int)_n_subzones, (int)nsubzones);
            error("region: size inconsistent with number of subzones");
        }
        _n_quantum = 0;

        // prime the small and medium admins with a subzone each (will handle correctly if there are no subzones)
        add_subzone(zone->small_admin());
        add_subzone(zone->medium_admin());

        // update statistics
        zone_stats.add_admin(Region::bytes_needed());   // XXX counted via aux I think
        zone_stats.add_allocated(size);
        zone_stats.increment_regions_in_use();
    }

        
    //
    // Destructor
    //
    Region::~Region() {
        // update statistics
        _zone->statistics().add_admin(-Region::bytes_needed());
        // XXX never happens, never will
        // update other statistics...
    }
        

    //
    // add_subzone
    //
    // Add a new subzone to one of the admin.
    //
    bool Region::add_subzone(Admin *admin) {
        // BEGIN CRITICAL SECTION
        SpinLock admin_lock(admin->lock());
        
        // There may have been a race to get here. Verify that the admin has no active subzone
        // as a quick check that we still need to add one.
        if (admin->active_subzone()) return true;
        
        Subzone *subzone = NULL;
        {
            SpinLock subzone_lock(&_subzone_lock);

            // if there are no subzones available then not much we can do
            if (_i_subzones == _n_subzones) return false;
            
            // Get next subzone
            subzone = new(subzone_address(_i_subzones++)) Subzone(this, admin, admin->quantum_log2(), _n_quantum);

            // advance quantum count
            _n_quantum += subzone->allocation_limit();
            
            // update pending bitmap to total quanta available to be allocated in this region
            _pending.set_size(Bitmap::bytes_needed(_n_quantum));
            _marks.set_size(Bitmap::bytes_needed(_n_quantum));
        }

        // Add free allocation space to admin 
        admin->set_active_subzone(subzone);

        // update statistics
        Statistics &zone_stats = _zone->statistics();
        zone_stats.add_admin(subzone_write_barrier_max);
        zone_stats.increment_subzones_in_use();
        
        // let the zone know the subzone is active.
        _zone->activate_subzone(subzone);
        
        // END CRITICAL SECTION
        
        return true;
    }
    
    //
    // malloc_statistics
    // add up our contribution to the malloc statistics
    void Region::malloc_statistics(malloc_statistics_t *stats) {
        SpinLock lock(&_subzone_lock);
        //stats->max_size_in_use += subzone_size(_i_subzones);
        //stats->size_allocated += subzone_size(_n_subzones);
        for (uint i=0; i<_i_subzones; i++) {
            Subzone *s = subzone_address(i);
            s->malloc_statistics(stats);
        }
        stats->max_size_in_use += _marks.size();
        stats->max_size_in_use += _pending.size();
        stats->max_size_in_use += subzone_write_barrier_max;
    }
};
