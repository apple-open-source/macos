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
    AllocationCache.h
    Automatic Garbage Collection
    Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 */

#pragma once
#ifndef __AUTO_ALLOCATION_CACHE__
#define __AUTO_ALLOCATION_CACHE__

#include "Configuration.h"
#include "FreeList.h"

namespace Auto {

    class AllocationCache {
    public:
        enum {
            cache_size = maximum_quanta + 1     // size of the allocation cache
        };
    private:
        FreeList    _lists[cache_size];         // free lists, one for each quanta size, slot 0 is for large clumps
    
    public:
        FreeList    &operator[] (usword_t index) { return _lists[index]; }
    };

}

#endif
