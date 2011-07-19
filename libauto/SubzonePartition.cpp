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
    SubzonePartition.cpp
    Subzone partitioning scheme.
    Copyright (c) 2009-2011 Apple Inc. All rights reserved.
 */

#include "SubzonePartition.h"

namespace Auto {
    void SubzonePartition::initialize(Zone *zone) {
        for (usword_t i = 0; i < kPartitionCount; ++i) {
            _small[i].initialize(zone, allocate_quantum_small_log2, i & kPartitionUnscanned);
            _medium[i].initialize(zone, allocate_quantum_medium_log2, i & kPartitionUnscanned);
        }
    }
};
