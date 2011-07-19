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
    Subzone.cpp
    Quantized Memory Allocation
    Copyright (c) 2004-2011 Apple Inc. All rights reserved.
 */

#include "Subzone.h"
#include "Zone.h"
#include "Thread.h"

namespace Auto {

    //----- Subzone -----//

    Subzone::~Subzone() {
        reset_collection_checking();
    }

#ifdef MEASURE_TLC_STATS
    void Subzone::update_block_escaped_stats() {
        admin()->zone()->statistics().add_escaped(1);
    }
#endif
    
    
    Subzone::PendingCountAccumulator::PendingCountAccumulator(Thread &thread) : _thread(thread), _last_pended_subzone(NULL), _pended_count(0) {
        _thread.set_pending_count_accumulator(this);
    }
    
    Subzone::PendingCountAccumulator::~PendingCountAccumulator() {
        flush_count();
        _thread.set_pending_count_accumulator(NULL);
    }
};

