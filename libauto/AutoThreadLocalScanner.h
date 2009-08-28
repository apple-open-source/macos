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
 *  AutoThreadLocalScanner.h
 *  auto
 *
 *  Created by Josh Behnke on 12/13/07.
 *  Copyright 2008 Apple Inc. All rights reserved.
 *
 */

#include "AutoMemoryScanner.h"
#include "AutoPointerHash.h"
#include "AutoThread.h"

namespace Auto {
    
    //----- ThreadLocalScanner -----//
    
    //
    // Responsible for performing memory (stack) scanning for thread local collections.
    // Conservatively found blocks are simply accumulated.
    //
    
    // FIXME:  We don't need this class. Merge it with ThreadLocalCollector.
    
    class ThreadLocalScanner : public MemoryScanner {

    protected:
        LocalBlocksHash &_localBlocks;
        SimplePointerBuffer *_markedBlocksBuffer;
        size_t _markedBlocksCounter;

        // if a buffer/size is provided then the block must live at least as long as the new scanner object
        ThreadLocalScanner(Zone *zone, void *current_stack_bottom, LocalBlocksHash &localBlocks)
            : MemoryScanner(zone, current_stack_bottom, true),
              _localBlocks(localBlocks), _markedBlocksBuffer(NULL), _markedBlocksCounter(0)
        {
        }
        
        // overridden to get rid of MemoryScanner interaction with the zone (pending stuff) that we don't need
        virtual void scan_range(const Range &range, WriteBarrier *wb = NULL);
        
        virtual void check_block(void **reference, void *block);
        
        // if a SimplePointerBuffer is set then add blocks to it as they are marked
        void set_marked_blocks_buffer(SimplePointerBuffer *buffer) { _markedBlocksBuffer = buffer; }
    };
};
