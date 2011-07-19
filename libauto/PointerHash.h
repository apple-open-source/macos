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
    PointerHash.h
    Pointer Hash Set for TLC.
    Copyright (c) 2008-2011 Apple Inc. All rights reserved.
 */

#ifndef __AUTO_POINTER_HASH__
#define __AUTO_POINTER_HASH__

#include <stdint.h>
#include <stddef.h>
#include "auto_impl_utilities.h"

namespace Auto {

    class PointerHash {
    protected:
        enum {
            FlagsMask = 0x7,
            RemovedEntry = ~FlagsMask,
            MaxRunLength = 16,
            MinCapacity = MaxRunLength * 2,
            PreferredCapacity = 512, // arbitrarily chosen such that this many 64 bit pointers fit in a single page
        };
        
        vm_address_t *_pointers;
        uint32_t _capacity;
        uint32_t _capacityMask;
        uint32_t _count;
        uint32_t _removed;
        uint32_t _firstOccupiedSlot;
        uint32_t _lastOccupiedSlot;
        uint32_t _maxRunLength;
        
        inline uint32_t flagsValue(vm_address_t storedValue) const { return storedValue & FlagsMask; }
        inline void *pointerValue(vm_address_t storedValue) const { return (void *)(storedValue & ~FlagsMask); }
        inline bool validPointer(vm_address_t value) const { return value != 0 && value != (vm_address_t)RemovedEntry; }

    public:
        PointerHash(uint32_t initialCapacity) : _pointers(NULL), _capacity(0), _capacityMask(0), _count(0), _removed(0), _firstOccupiedSlot(0), _lastOccupiedSlot(0), _maxRunLength(0) { if (initialCapacity > 0) grow(initialCapacity * 4); };
        ~PointerHash() { if (_pointers) aux_free(_pointers); }
        
        int32_t slotIndex(void *pointer) const; // returns the index of the slot containing pointer, or -1 if pointer is not in the set
		
        void add(void *pointer, uint32_t flags = 0);
        void remove(uint32_t slot);
        void remove(void *pointer);
        inline bool contains(void *pointer) const { return slotIndex(pointer) != -1; }
        
        void rehash(uint32_t flagMask = 0); // bits which are set in flagMask will be cleared in all entries
        void grow(uint32_t newCapacity = PreferredCapacity, uint32_t flagMask = 0); // bits which are set in flagMask will be cleared in all entries
        void compact(uint32_t flagMask = 0);
        void clearFlags();

        inline uint32_t capacity() const        { return _capacity; }
        inline uint32_t count() const           { return _count; }
        inline uint32_t firstOccupiedSlot() const { return _firstOccupiedSlot; }
        inline uint32_t lastOccupiedSlot() const { return _lastOccupiedSlot; }

        inline bool validPointerAtIndex(uint32_t index) const { return validPointer(_pointers[index]); }
        
        inline void *operator[](uint32_t index) const { return validPointerAtIndex(index) ? pointerValue(_pointers[index]) : NULL; }
        
        inline void setFlag(uint32_t index, uint32_t flag) { _pointers[index] |= flag; }
        inline void clearFlag(uint32_t index, uint32_t flag) { if (_pointers[index] != (vm_address_t)RemovedEntry) _pointers[index] &= ~flag; }
        inline bool flagSet(uint32_t index, uint32_t flag) { return (_pointers[index] & flag) != 0; }
        
    private:
        uintptr_t hash(void *pointer) const;
        void insert(void *pointer, uint32_t flags);
    };

};

#endif // __AUTO_POINTER_HASH__
