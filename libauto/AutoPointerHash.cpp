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
 *  AutoPointerHash.cpp
 *  auto
 *
 *  Created by Josh Behnke on 2/13/08.
 *  Copyright 2008-2009 Apple Inc. All rights reserved.
 *
 */

#include "AutoPointerHash.h"
#include "AutoConfiguration.h"
#include <assert.h>

namespace Auto {

    inline uintptr_t AutoPointerHash::hash(void *pointer) const {
        return uintptr_t(pointer) >> allocate_quantum_small_log2;
    }
    
    void AutoPointerHash::add(void *pointer, uint32_t flags) {
        insert(pointer, flags);
        // don't let table fill to capacity - if we did then a rehash would never find a slot with 0
        if (_count*3 > _capacity*2) {
            grow(_capacity * 2);    // double when at 2/3 capacity
        } else if ((_count + _removed + 8) >= _capacity) {
            // rare.  removes are not being eaten up by adds.
            rehash(0x0);    // if deletions are filling the table better rehash (and preserve any flags)
        }
    }
    
    void AutoPointerHash::insert(void *pointer, uint32_t flags) {
        uintptr_t h = hash(pointer);
        uint32_t index = h & _capacityMask;             // <rdar://problem/6544627> don't use integer divide.
        uint32_t run_length = 0;
        vm_address_t probe;
        while (validPointer(probe = _pointers[index])) {
            if (pointerValue(probe) == pointer) return; // already inserted. should the flags be updated? they weren't before.
            index++;
            run_length++;
            if (index == _capacity)
                index = 0;
        }
        if (probe == (vm_address_t)RemovedEntry)
            --_removed;
        _pointers[index] = (vm_address_t)pointer | flags;
        _count++;
        if (index < _firstOccupiedSlot)
            _firstOccupiedSlot = index;
        if (index > _lastOccupiedSlot)
            _lastOccupiedSlot = index;
        if (run_length > _maxRunLength)
            _maxRunLength = run_length;
    }

    int32_t AutoPointerHash::slotIndex(void *pointer) const 
    {
        if (_count > 0) {
            uintptr_t h = hash(pointer);
            const uint32_t kCapacityMask = _capacityMask, kMaxRunLength = _maxRunLength;
            uint32_t i = h & kCapacityMask;
            uint32_t run = 0;
            while (vm_address_t probe = _pointers[i]) {
                if (pointerValue(probe) == pointer)
                    return i;
                if (run >= kMaxRunLength)
                    break;
                run++;
                i = (i + 1) & kCapacityMask;
            }
        }
        return -1;
    }
	
    void AutoPointerHash::remove(uint32_t slot)
    {
        if (slot < _capacity) {
            uint32_t index = slot;
            if (_maxRunLength == 0 || _pointers[(index + 1) & _capacityMask] == 0) {
                // if there are no misaligned pointers or the next slot is NULL then we can just set this slot to NULL
                // we can also clean up the table by NULL-ing slots that contain a now unneeded RemovedEntry value (to improve searching)
                ++_removed;
                do {
                    --_removed;
                    _pointers[index] = NULL;
                    index = (index == 0 ? _capacity - 1 : index - 1);
                } while (_pointers[index] == (vm_address_t)RemovedEntry);
            } else {
                // there is an entry immediately after this one, so have to mark the slot as previously occupied
                _pointers[slot] = (vm_address_t)RemovedEntry;
                ++_removed;
            }
            _count--;
        }
    }
    
    void AutoPointerHash::remove(void *pointer)
    {
        int32_t index = slotIndex(pointer);
        if (index != -1) {
            remove(index);
        }
    }
    
    void AutoPointerHash::grow(uint32_t newCapacity, uint32_t flagMask)
    {
        vm_address_t mask = ~(FlagsMask & flagMask);
        vm_address_t *old_pointers = _pointers;
        uint32_t old_capacity = _capacity;
        uint32_t old_count = _count;
        uint32_t i = _firstOccupiedSlot;
        
        if (newCapacity > 0 && newCapacity < MinCapacity)
            newCapacity = MinCapacity;
            
        if (_capacity != newCapacity) {
            _capacity = newCapacity;
            assert(is_power_of_2(_capacity));
            _capacityMask = _capacity - 1;  // _capacity is ALWAYS a power of two.
            _count = 0;
            _removed = 0;
            _firstOccupiedSlot = _capacity;
            _lastOccupiedSlot = 0;
            _maxRunLength = 0;
            _pointers = NULL;
            
            if (newCapacity > 0) {
                _pointers = (vm_address_t *)aux_calloc(_capacity, sizeof(void *));
                if (old_count > 0) {
                    uint32_t rehashed = 0;
                    while (i<old_capacity && rehashed < old_count) {
                        if (validPointer(old_pointers[i])) {
                            insert(pointerValue(old_pointers[i]), flagsValue(old_pointers[i]) & mask);
                            rehashed++;
                        }
                        i++;
                    }
                }
            }
            if (old_pointers)
                aux_free(old_pointers);
        }
    }
	
    
    void AutoPointerHash::rehash(uint32_t flagMask)
    {
        vm_address_t mask = ~(FlagsMask & flagMask);
        if (_maxRunLength > 0 || flagMask) {
            if (_count > 0) {
                // Must start in gap.
                // Imagine a run that overlaps end, each of which wants to move down one
                // (e.g. item at 0 wants to be at end, item at end wants to be at end-1)
                // item at 0 can't initially move, yet item at end does, leaving item 0
                // in wrong place
                uint32_t start = _capacity;
                for (uint32_t i = 0; i < _capacity; i++) {
                    if (_pointers[i] == 0) {
                        start = i;
                        break;
                    }
                }
                
                if (start == _capacity) {
                    // didn't find any gaps, can't rehash in-place safely.
                    grow(_capacity * 2, flagMask);
                    return;
                }
                
                _count = 0;
                _removed = 0;
                _firstOccupiedSlot = _capacity;
                _lastOccupiedSlot = 0;
                _maxRunLength = 0;
                
                // assert start < _capacity
                for (uint32_t i = start; i < _capacity; i++) {
                    vm_address_t pointer = _pointers[i];
                    _pointers[i] = 0;
                    if (validPointer(pointer))
                        insert(pointerValue(pointer), flagsValue(pointer) & mask);
                }
                for (uint32_t i = 0; i < start; i++) {
                    vm_address_t pointer = _pointers[i];
                    _pointers[i] = 0;
                    if (validPointer(pointer))
                        insert(pointerValue(pointer), flagsValue(pointer) & mask);
                }
            } else {
                bzero(_pointers, _capacity * sizeof(void *));
            }
        }
    }

    void AutoPointerHash::compact(uint32_t flagMask) {
        // If _count < PreferredCapacity / 3, then shrink the table to that size.
        if (_capacity > PreferredCapacity && (_count * 3) < PreferredCapacity)
            grow(PreferredCapacity, flagMask);
        else
            rehash(flagMask);
    }
    
    void AutoPointerHash::clearFlags() {
        for (uint32_t i = _firstOccupiedSlot; i <= _lastOccupiedSlot; ++i) {
            _pointers[i] &= ~FlagsMask;
        }
    }
}
