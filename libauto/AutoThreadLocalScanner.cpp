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
 *  AutoThreadLocalScanner.cpp
 *  auto
 *
 *  Created by Josh Behnke on 12/13/07.
 *  Copyright 2008 Apple Inc. All rights reserved.
 *
 */
#include "AutoThreadLocalScanner.h"
#include "AutoSubzone.h"
#include "AutoZone.h"

namespace Auto {
    
    void ThreadLocalScanner::check_block(void **reference, void *block) {
        if (_zone->in_subzone_memory(block)) {
            Subzone *subzone = Subzone::subzone(block);
            if (subzone->is_thread_local(block)) {
                int32_t blockIndex = _localBlocks.slotIndex(block);
                if (blockIndex != -1 && !_localBlocks.testAndSetMarked(blockIndex)) {
                    ++_markedBlocksCounter;
                    if (_markedBlocksBuffer)
                        _markedBlocksBuffer->push(block);
                }
            }
        }
        else {
            // XXX make it work for Large
        }
    }
    
    void ThreadLocalScanner::scan_range(const Range &range, WriteBarrier *wb) {
        // set up the iteration for this range
        void ** reference = (void **)range.address();
        void ** const end = (void **)range.end();
        
        _amount_scanned += (char *)end - (char *)reference;
        
        // local copies of valid address info
        uintptr_t valid_lowest = (uintptr_t)_zone->coverage().address();
        uintptr_t valid_size = (uintptr_t)_zone->coverage().end() - valid_lowest;
        
        void *last_valid_pointer = end - 1;
        // iterate through all the potential references
        for ( ; reference <= last_valid_pointer; ++reference) {
            // get referent 
            void *referent = *reference;
            
            // if is a block then check this block out
            if (((intptr_t)referent - valid_lowest) < valid_size && _zone->block_is_start(referent)) {
                check_block(reference, referent);
            }
        }
    }
    
};
