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
    AutoSubzone.cpp
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "AutoSubzone.h"
#include "AutoZone.h"

namespace Auto {

    //----- Subzone -----//
    
    //
    // malloc_statistics
    //
    // compute the malloc statistics
    // XXX known not to be accurate - missing blocks allocated
    //
    void Subzone::malloc_statistics(malloc_statistics_t *stats) {
        SpinLock lock(_admin->lock());
        for (usword_t q=0; q<_in_use; q++) {
            if (is_start(q)) {
                stats->blocks_in_use++;
                stats->size_in_use += size(q);
                q += length(q);
            }
        }
        // admin costs
        // XXX we should add in the size of the base + write-barriers
        stats->max_size_in_use += quantum_size(_in_use);
        stats->size_allocated += quantum_size(_in_use);
    }

    
};

