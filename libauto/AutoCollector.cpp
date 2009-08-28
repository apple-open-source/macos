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
    AutoCollector.cpp
    Specialized Memory Scanner
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "AutoCollector.h"
#include "AutoDefs.h"
#include "AutoEnvironment.h"
#include "AutoRange.h"
#include "AutoWriteBarrier.h"
#include "AutoZone.h"


namespace Auto {

    //----- Collector -----//
    
        
    //
    // check_roots
    //
    // Scan root blocks.
    //
    void Collector::check_roots() {
        if (is_partial()) {
            scan_retained_and_old_blocks();
        } else {
            scan_retained_blocks();
        }
       
        scan_root_ranges();
    }


    //
    // collect
    //
    // Collect scans memory for reachable objects.  Unmarked blocks are available to
    // be garbaged.  Returns "uninterrupted"
    //
    void Collector::collect(bool use_pending) {
        // scan memory
        _use_pending = use_pending;
        scan();
    }


    void Collector::scan_barrier() {
        scan_end = auto_date_now();
        _zone->enlivening_barrier(*this);
    }
};
