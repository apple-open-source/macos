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
    SubzonePartition.h
    Subzone partitioning scheme.
    Copyright (c) 2009-2011 Apple Inc. All rights reserved.
 */

#ifndef __AUTO_SUBZONE_PARTITION__
#define __AUTO_SUBZONE_PARTITION__

#include "Admin.h"
#include "Locks.h"

namespace Auto {

    // Forward declarations
    
    //----- SubzonePartition -----//
    
    class SubzonePartition {
        enum {
            kPartitionUnscanned  = 1,
            kPartitionRetained   = 2,
            kPartitionCount      = 4
        };
    
        Admin _small[kPartitionCount];
        Admin _medium[kPartitionCount];

    public:
        void initialize(Zone *zone);
        
        Admin &admin(const size_t size, const usword_t layout, bool refcount_is_one) {
            usword_t partion = ((layout & AUTO_UNSCANNED) ? kPartitionUnscanned : 0) | (refcount_is_one ? kPartitionRetained : 0);
            return (size < allocate_quantum_medium ? _small[partion] : _medium[partion]);
        }

        void lock() {
            for (usword_t i = 0; i < kPartitionCount; ++i) {
                spin_lock(_small[i].lock());
                spin_lock(_medium[i].lock());
            }
        }
        
        void unlock() {
            for (usword_t i = kPartitionCount; i > 0; --i) {
                spin_unlock(_medium[i - 1].lock());
                spin_unlock(_small[i - 1].lock());
            }
        }
        
        bool locked() {
            for (usword_t i = 0; i < kPartitionCount; ++i) {
                TrySpinLock smallAttempt(_small[i].lock());
                if (!smallAttempt) return true;
                TrySpinLock mediumAttempt(_medium[i].lock());
                if (!mediumAttempt) return true;
            }
            return false;
        }
        
        //
        // for_each
        //
        // Applies block to all partitioned admins.
        //
        void for_each(void (^block) (Admin &admin)) {
            for (usword_t i = 0; i < kPartitionCount; ++i) {
                block(_small[i]);
                block(_medium[i]);
            }
        }
        
        usword_t purge_free_space() {
            usword_t bytes_purged = 0;
            for (usword_t i = 0; i < kPartitionCount; ++i) {
                bytes_purged += _small[i].purge_free_space();
                bytes_purged += _medium[i].purge_free_space();
            }
            return bytes_purged;
        }
        
        usword_t purge_free_space_no_lock() {
            usword_t bytes_purged = 0;
            for (usword_t i = 0; i < kPartitionCount; ++i) {
                bytes_purged += _small[i].purge_free_space_no_lock();
                bytes_purged += _medium[i].purge_free_space_no_lock();
            }
            return bytes_purged;
        }
        
        class Lock {
            SubzonePartition &_partition;
        public:
            Lock(SubzonePartition &partition) : _partition(partition) { _partition.lock(); }
            ~Lock() { _partition.unlock(); }
        };
    };
    
};

#endif /* __AUTO_SUBZONE_PARTITION__ */
