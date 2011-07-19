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
    BlockRef.cpp
    Block sieve helper classes.
    Copyright (c) 2010-2011 Apple Inc. All rights reserved.
 */

#include "BlockRef.h"

namespace Auto {
    
    // SubzoneBlockRef

    usword_t SubzoneBlockRef::refcount() const {
        int refcount = 0;
        Admin *admin = subzone()->admin();
        SpinLock lock(admin->lock());
        if (has_refcount()) {
            // non-zero reference count, check the overflow table.
            // BlockRef FIXME: use q instead of address in hash map?
            PtrIntHashMap &retains = admin->retains();
            PtrIntHashMap::iterator retain_iter = retains.find(address());
            if (retain_iter != retains.end() && retain_iter->first == address()) {
                refcount = retain_iter->second;
            } else {
                refcount = 1;
            }
        }
        return refcount;
    }
    
    usword_t SubzoneBlockRef::inc_refcount() const { 
        int refcount;
        Admin *admin = subzone()->admin();
        SpinLock lock(admin->lock());
        void *block = address();
        if (has_refcount()) {
            // non-trivial reference count, check the overflow table.
            PtrIntHashMap &retains = admin->retains();
            PtrIntHashMap::iterator retain_iter = retains.find(block);
            if (retain_iter != retains.end() && retain_iter->first == block) {
                refcount = ++retain_iter->second;
            } else {
                // transition from 1 -> 2
                refcount = (retains[block] = 2);
            }
        } else {
            // transition from 0 -> 1
            Thread &thread = admin->zone()->registered_thread();
            thread.block_escaped(*this);
            subzone()->set_has_refcount(q());
            refcount = 1;
        }
        return refcount;
    }
    
    usword_t SubzoneBlockRef::dec_refcount_no_lock() const {
        Admin *admin = subzone()->admin();
        if (has_refcount()) {
            // non-zero reference count, check the overflow table.
            PtrIntHashMap &retains = admin->retains();
            PtrIntHashMap::iterator retain_iter = retains.find(address());
            if (retain_iter != retains.end() && retain_iter->first == address()) {
                if (--retain_iter->second == 1) {
                    // transition from 2 -> 1
                    retains.erase(retain_iter);
                    return 1;
                } else {
                    return retain_iter->second;
                }
            } else {
                // transition from 1 -> 0
                subzone()->clear_has_refcount(q());
                return 0;
            }
        }
        // underflow.
        malloc_printf("reference count underflow for %p, break on auto_refcount_underflow_error to debug.\n", address());
        auto_refcount_underflow_error(address());
        return -1;
    }
    
    usword_t SubzoneBlockRef::dec_refcount() const {
        Admin *admin = subzone()->admin();
        SpinLock lock(admin->lock());
        return dec_refcount_no_lock();
    }
    
    
    
    // LargeBlockRef
    
    usword_t LargeBlockRef::inc_refcount() const { 
        SpinLock lock(zone()->large_lock());
        usword_t refcount = _large->refcount() + 1;
        _large->set_refcount(refcount);
        return refcount;
    }
    
    usword_t LargeBlockRef::dec_refcount_no_lock() const {
        usword_t rc = refcount();
        if (rc <= 0) {
            malloc_printf("reference count underflow for %p, break on auto_refcount_underflow_error to debug\n", address());
            auto_refcount_underflow_error(address());
        } else {
            rc = rc - 1;
            _large->set_refcount(rc);
        }
        return rc;
    }            
    
    usword_t LargeBlockRef::dec_refcount() const {
        SpinLock lock(zone()->large_lock());
        return dec_refcount_no_lock();
    }            
    
}
