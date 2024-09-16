/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _OPTIMIZER_OBJC_H_
#define _OPTIMIZER_OBJC_H_

#include <mach-o/loader.h>
#include <optional>

#include "Diagnostics.h"
#include "MachOAnalyzer.h"
#include "PerfectHash.h"

namespace objc {

struct objc_image_info {
    int32_t version;
    uint32_t flags;
};

// Precomputed perfect hash table of strings.
// Base class for precomputed selector, class and protocol tables.
class VIS_HIDDEN StringHashTable
{
protected:
    typedef uint8_t  CheckByteType;
    typedef int32_t  StringOffset;

    uint32_t version;
    uint32_t capacity;
    uint32_t occupied;
    uint32_t shift;
    uint32_t mask;
    uint64_t salt;

    uint32_t scramble[256];
    uint8_t  tab[0]; /* tab[mask+1] (always power-of-2) */
    // uint8_t checkbytes[capacity];  /* check byte for each string */
    // int32_t offsets[capacity];     /* offsets from &capacity to cstrings */

    CheckByteType* checkbytes()
    {
        return (CheckByteType*)&tab[mask + 1];
    }
    const CheckByteType* checkbytes() const
    {
        return (const CheckByteType*)&tab[mask + 1];
    }

    StringOffset* offsets()
    {
        return (StringOffset*)&checkbytes()[capacity];
    }
    const StringOffset* offsets() const
    {
        return (const StringOffset*)&checkbytes()[capacity];
    }

    uint32_t hash(const char* key, size_t keylen) const
    {
        uint64_t val   = lookup8((uint8_t*)key, keylen, salt);
        uint32_t index = (uint32_t)((shift == 64) ? 0 : (val >> shift)) ^ scramble[tab[val & mask]];
        return index;
    }

    uint32_t hash(const char* key) const
    {
        return hash(key, strlen(key));
    }

    // The check bytes are used to reject strings that aren't in the table
    // without paging in the table's cstring data. This checkbyte calculation
    // catches 4785/4815 rejects when launching Safari; a perfect checkbyte
    // would catch 4796/4815.
    CheckByteType checkbyte(const char* key, size_t keylen) const
    {
        return ((key[0] & 0x7) << 5) | ((uint8_t)keylen & 0x1f);
    }

    CheckByteType checkbyte(const char* key) const
    {
        return checkbyte(key, strlen(key));
    }

    std::optional<uint32_t> tryGetIndex(const char* key) const
    {
        size_t   keylen = strlen(key);
        uint32_t h      = hash(key, keylen);

        // Use check byte to reject without paging in the table's cstrings
        CheckByteType h_check   = checkbytes()[h];
        CheckByteType key_check = checkbyte(key, keylen);
        if ( h_check != key_check )
            return {};

        StringOffset offset = offsets()[h];
        if ( offset == 0 )
            return {};

        const char* result = (const char*)this + offset;
        if ( 0 != strcmp(key, result) )
            return {};

        return h;
    }

    void forEachString(void (^callback)(const char* str)) const {
        for ( unsigned i = 0; i != capacity; ++i ) {
            StringOffset offset = offsets()[i];
            if ( offset == 0 )
                continue;

            const char* result = (const char*)this + offset;
            callback(result);
        }
    }

#if BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS || BUILDING_CACHE_BUILDER_UNIT_TESTS

    size_t size()
    {
        return sizeof(StringHashTable) + mask + 1 + (capacity * sizeof(CheckByteType)) + (capacity * sizeof(StringOffset));
    }

    // Take an array of strings and turn it in to a perfect hash map of offsets to those strings
    // Note the strings are going to be emitted relative to stringBaseVMAddr, but class/protocol maps
    // want to look them up relative to offsetsBaseVMAddr.  So adjust the offsets to account for that
    void write(Diagnostics& diag, uint64_t stringBaseVMAddr, uint64_t offsetsBaseVMAddr,
               size_t remaining, const std::vector<ObjCString>& strings)
    {
        if ( sizeof(StringHashTable) > remaining ) {
            diag.error("selector section too small (metadata not optimized)");
            return;
        }

        if ( strings.size() == 0 ) {
            bzero(this, sizeof(StringHashTable));
            return;
        }

        objc::PerfectHash phash;
        objc::PerfectHash::make_perfect(strings, phash);
        if ( phash.capacity == 0 ) {
            diag.error("perfect hash failed (metadata not optimized)");
            return;
        }

        // Set header
        capacity = phash.capacity;
        occupied = phash.occupied;
        shift    = phash.shift;
        mask     = phash.mask;
        salt     = phash.salt;

        if ( size() > remaining ) {
            diag.error("class section too small (metadata not optimized)");
            return;
        }

        // Set hash data
        for ( uint32_t i = 0; i < 256; i++ ) {
            scramble[i] = phash.scramble[i];
        }
        for ( uint32_t i = 0; i < phash.mask + 1; i++ ) {
            tab[i] = phash.tab[i];
        }

        // Set offsets to 0
        for ( uint32_t i = 0; i < phash.capacity; i++ ) {
            offsets()[i] = 0;
        }
        // Set checkbytes to 0
        for ( uint32_t i = 0; i < phash.capacity; i++ ) {
            checkbytes()[i] = 0;
        }

        // Set real string offsets and checkbytes
        // We get the strings in the same order they will be in memory.  So we
        // can iterate over them in the same order to get the offsets
        for ( const ObjCString& stringAndOffset : strings ) {
            const std::string_view& str = stringAndOffset.first;
            const uint32_t stringBufferOffset = stringAndOffset.second;
            int64_t stringOffset = (stringBaseVMAddr + stringBufferOffset) - offsetsBaseVMAddr;

            StringOffset encodedOffset = (StringOffset)stringOffset;
            if ( (uint64_t)encodedOffset != stringOffset ) {
                diag.error("selector offset too big (metadata not optimized)");
                return;
            }

            uint32_t h      = hash(str.data());
            offsets()[h]    = encodedOffset;
            checkbytes()[h] = checkbyte(str.data());
        }
    }

#endif // BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS

public:
    uint32_t occupancy() const {
        return occupied;
    }
};

// Precomputed selector table.
class SelectorHashTable : public StringHashTable
{
public:
    using StringHashTable::forEachString;
    using StringHashTable::tryGetIndex;

#if BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS || BUILDING_CACHE_BUILDER_UNIT_TESTS
    using StringHashTable::size;
#endif

    const char* get(const char *key) const
    {
        if ( std::optional<uint32_t> index = tryGetIndex(key) )
            return getEntryForIndex(*index);
        return nullptr;
    }

#if BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS || BUILDING_CACHE_BUILDER_UNIT_TESTS
    template<typename StringArray>
    void write(Diagnostics& diag, uint64_t stringsBaseVMAddr, uint64_t mapBaseAddress,
               size_t remaining, const StringArray& strings)
    {
        StringHashTable::write(diag, stringsBaseVMAddr, mapBaseAddress, remaining, strings);
        if ( diag.hasError() )
            return;
#if BUILDING_CACHE_BUILDER
        diag.verbose("  selector table occupancy %u/%u (%u%%)\n",
                     occupied, capacity,
                     (unsigned)(occupied/(double)capacity*100));
#endif
    }
#endif

private:
    const char* getEntryForIndex(uint32_t index) const {
        return (const char *)this + offsets()[index];
    }
};

// This is used for classes, protocols and categories
// The keys are strings, eg, class/protocol  names.  Those are encoded as 32-bit offsets from the
// 'this' pointer of the map.
// Given this, all keys must be within 32-bits of the map, even if that requires copying strings in to
// nearby memory.
// Values are at offsets relative to the shared cache base address.  They are not offsets from the map itself.
class VIS_HIDDEN ObjectHashTable : public StringHashTable
{
protected:
    // ...StringHashTable fields...
    // ObjectData objectOffsets[capacity]; /* offset from &capacity to Object, and dylib index */
    // uint32_t duplicateCount;
    // ObjectData duplicateOffsets[duplicatedClasses];

    struct ObjectData {
        struct Object {
            uint64_t isDuplicate        : 1,    // == 0
                     objectCacheOffset  : 47,   // Offset from the shared cache base to the Class/Protocol
                     dylibObjCIndex    : 16;   // Index in to the HeaderInfoRW dylibs for this Class/Protocol
        };
        struct Duplicate {
            uint64_t isDuplicate        : 1,    // == 1
                     index              : 47,
                     count              : 16;
        };
        union {
            Object      object;
            Duplicate   duplicate;
            uint64_t    raw;
        };

        // For duplicate class names:
        // duplicated classes are duplicateOffsets[duplicateIndex..duplicateIndex+duplicateCount-1]
        bool isDuplicate() const
        {
            return duplicate.isDuplicate;
        }
        uint32_t duplicateCount() const
        {
            return (uint32_t)duplicate.count;
        }
        uint32_t duplicateIndex() const
        {
            return (uint32_t)duplicate.index;
        }
    };

    ObjectData*       objectOffsets() { return (ObjectData*)&offsets()[capacity]; }
    const ObjectData* objectOffsets() const { return (const ObjectData*)&offsets()[capacity]; }

    uint32_t&       duplicateCount() { return *(uint32_t*)&objectOffsets()[capacity]; }
    const uint32_t& duplicateCount() const { return *(const uint32_t*)&objectOffsets()[capacity]; }

    ObjectData*       duplicateOffsets() { return (ObjectData*)(&duplicateCount() + 1); }
    const ObjectData* duplicateOffsets() const { return (const ObjectData*)(&duplicateCount() + 1); }

    const char* getObjectNameForIndex(uint32_t index) const
    {
        return (const char*)this + offsets()[index];
    }

#if 0
    void* getClassForIndex(uint32_t index, uint32_t duplicateIndex) const
    {
        const ObjectData& clshi = classOffsets()[index];
        if ( !clshi.isDuplicate() ) {
            // class appears in exactly one header
            return (void*)((const char*)this + clshi.clsOffset);
        }
        else {
            // class appears in more than one header - use getClassesAndHeaders
            const ObjectData* list = &duplicateOffsets()[clshi.duplicateIndex()];
            return (void*)((const char*)this + list[duplicateIndex].clsOffset);
        }
    }

    // 0/NULL/NULL: not found
    // 1/ptr/ptr: found exactly one
    // n/NULL/NULL:  found N - use getClassesAndHeaders() instead
    uint32_t getClassHeaderAndIndex(const char* key, void*& cls, void*& hi, uint32_t& index) const
    {
        uint32_t h = getIndex(key);
        if ( h == INDEX_NOT_FOUND ) {
            cls   = NULL;
            hi    = NULL;
            index = 0;
            return 0;
        }

        index = h;

        const ObjectData& clshi = classOffsets()[h];
        if ( !clshi.isDuplicate() ) {
            // class appears in exactly one header
            cls = (void*)((const char*)this + clshi.clsOffset);
            hi  = (void*)((const char*)this + clshi.hiOffset);
            return 1;
        }
        else {
            // class appears in more than one header - use getClassesAndHeaders
            cls = NULL;
            hi  = NULL;
            return clshi.duplicateCount();
        }
    }

    void getClassesAndHeaders(const char* key, void** cls, void** hi) const
    {
        uint32_t h = getIndex(key);
        if ( h == INDEX_NOT_FOUND )
            return;

        const ObjectData& clshi = classOffsets()[h];
        if ( !clshi.isDuplicate() ) {
            // class appears in exactly one header
            cls[0] = (void*)((const char*)this + clshi.clsOffset);
            hi[0]  = (void*)((const char*)this + clshi.hiOffset);
        }
        else {
            // class appears in more than one header
            uint32_t                  count = clshi.duplicateCount();
            const ObjectData* list  = &duplicateOffsets()[clshi.duplicateIndex()];
            for ( uint32_t i = 0; i < count; i++ ) {
                cls[i] = (void*)((const char*)this + list[i].clsOffset);
                hi[i]  = (void*)((const char*)this + list[i].hiOffset);
            }
        }
    }

    // 0/NULL/NULL: not found
    // 1/ptr/ptr: found exactly one
    // n/NULL/NULL:  found N - use getClassesAndHeaders() instead
    uint32_t getClassAndHeader(const char* key, void*& cls, void*& hi) const
    {
        uint32_t unusedIndex = 0;
        return getClassHeaderAndIndex(key, cls, hi, unusedIndex);
    }
#endif

protected:
    typedef void (^ObjectCallback)(uint64_t objectCacheOffset, uint16_t dylibObjCIndex,
                                   bool& stopObjects);
    void forEachObject(const char* key, ObjectCallback callback) const
    {
        std::optional<uint32_t> index = tryGetIndex(key);
        if ( !index.has_value() )
            return;

        const ObjectData& data = objectOffsets()[*index];
        if ( !data.isDuplicate() ) {
            // object appears in exactly one header
            bool stopObjects = false;
            callback(data.object.objectCacheOffset, data.object.dylibObjCIndex, stopObjects);
        }
        else {
            // object appears in more than one header
            uint32_t count = data.duplicate.count;
            const ObjectData* list  = &duplicateOffsets()[data.duplicate.index];
            for ( uint32_t i = 0; i < count; i++ ) {
                bool stopObjects = false;
                callback(list[i].object.objectCacheOffset, list[i].object.dylibObjCIndex, stopObjects);
                if ( stopObjects )
                    break;
            }
        }
    }

    typedef std::pair<uint64_t, uint16_t> ObjectAndDylibIndex;
    void forEachObject(void (^callback)(uint32_t bucketIndex,
                                        const char* objectName,
                                        const dyld3::Array<ObjectAndDylibIndex>& implCacheInfos)) const {
        for ( unsigned i = 0; i != capacity; ++i ) {
            StringOffset nameOffset = offsets()[i];
            if ( nameOffset == 0 )
                continue;

            const char* objectName = getObjectNameForIndex(i);

            // Walk each class for this key
            const ObjectData& data = objectOffsets()[i];
            if ( !data.isDuplicate() ) {
                // This class/protocol has a single implementation
                ObjectAndDylibIndex objectInfo = {
                    data.object.objectCacheOffset,
                    data.object.dylibObjCIndex
                };
                const dyld3::Array<ObjectAndDylibIndex> implTarget(&objectInfo, 1, 1);
                callback(i, objectName, implTarget);
            }
            else {
                // This class/protocol  has mulitple implementations.
                uint32_t count = data.duplicate.count;
                ObjectAndDylibIndex objectInfos[count];
                const ObjectData* list  = &duplicateOffsets()[data.duplicate.index];
                for (uint32_t duplicateIndex = 0; duplicateIndex < count; duplicateIndex++) {
                    ObjectAndDylibIndex objectInfo = {
                        list[duplicateIndex].object.objectCacheOffset,
                        list[duplicateIndex].object.dylibObjCIndex
                    };
                    objectInfos[duplicateIndex] = objectInfo;
                }
                const dyld3::Array<ObjectAndDylibIndex> implTargets(&objectInfos[0], count, count);
                callback(i, objectName, implTargets);
            }
        }
    }

#if BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS || BUILDING_CACHE_BUILDER_UNIT_TESTS

    size_t size()
    {
        size_t totalSize = 0;
        totalSize += StringHashTable::size();
        totalSize += capacity * sizeof(ObjectData);
        totalSize += sizeof(duplicateCount());
        totalSize += duplicateCount() * sizeof(ObjectData);
        return totalSize;
    }

    size_t sizeWithoutDups()
    {
        return StringHashTable::size() + (capacity * sizeof(ObjectData));
    }

    template<typename ObjectMapType>
    void write(Diagnostics& diag, uint64_t stringsBaseAddress, uint64_t mapBaseAddress,
               uint64_t cacheBaseAddress, size_t remaining,
               const std::vector<ObjCString>& strings, const ObjectMapType& objects)
    {
        StringHashTable::write(diag, stringsBaseAddress, mapBaseAddress, remaining, strings);
        if ( diag.hasError() )
            return;

        if ( sizeWithoutDups() > remaining ) {
            diag.error("class/protocol section too small (metadata not optimized)");
            return;
        }
        if ( size() > remaining ) {
            diag.error("class/protocol section too small (metadata not optimized)");
            return;
        }

        // Set object offsets to 0
        for ( uint32_t i = 0; i < capacity; i++ ) {
            objectOffsets()[i].raw = 0;
        }

        // Set real object offsets
        typename ObjectMapType::const_iterator c;
        for ( c = objects.begin(); c != objects.end(); ++c ) {
            std::optional<uint32_t> index = tryGetIndex(c->first);
            if ( !index.has_value() ) {
                diag.error("class/protocol list busted (metadata not optimized)");
                return;
            }

            uint32_t h = *index;

            if ( objectOffsets()[h].raw != 0 ) {
                // already did this object
                continue;
            }

            uint32_t count = (uint32_t)objects.count(c->first);
            if ( count == 1 ) {
                // only one object with this name
                uint64_t objectCacheOffset = c->second.first - cacheBaseAddress;
                uint16_t dylibIndex = c->second.second;

                objectOffsets()[h].object = { 0, objectCacheOffset, dylibIndex };
                if ( objectOffsets()[h].object.objectCacheOffset != objectCacheOffset ) {
                    diag.error("class/protocol offset too big (metadata not optimized)");
                    return;
                }
            }
            else {
                // object name has duplicates - write them all now

                uint32_t dest = duplicateCount();
                duplicateCount() += count;
                if ( size() > remaining ) {
                    diag.error("class/protocol section too small (metadata not optimized)");
                    return;
                }

                // objectOffsets() instead contains count and array index
                objectOffsets()[h].duplicate = { 1, dest, count };

                auto duplicates = objects.equal_range(c->first);
                typename ObjectMapType::const_iterator dup;
                for ( dup = duplicates.first; dup != duplicates.second; ++dup ) {
                    uint64_t objectCacheOffset = dup->second.first - cacheBaseAddress;
                    uint16_t dylibIndex = dup->second.second;

                    duplicateOffsets()[dest].object = { 0, objectCacheOffset, dylibIndex };
                    if ( duplicateOffsets()[dest].object.objectCacheOffset != objectCacheOffset ) {
                        diag.error("class/protocol offset too big (metadata not optimized)");
                        return;
                    }
                    dest++;
                }
            }
        }
    }

#endif // BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS
};

class VIS_HIDDEN ClassHashTable : public ObjectHashTable
{
public:
#if BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS || BUILDING_CACHE_BUILDER_UNIT_TESTS
    using ObjectHashTable::size;
#endif

    void forEachClass(const char* key, ObjectCallback callback) const {
        forEachObject(key, callback);
    }

    using ObjectHashTable::ObjectAndDylibIndex;
    void forEachClass(void (^callback)(uint32_t bucketIndex,
                                       const char* className,
                                       const dyld3::Array<ObjectAndDylibIndex>& implCacheInfos)) const {
        forEachObject(callback);
    }

    uint32_t classCount() const {
        return occupied + duplicateCount();
    }

#if BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS || BUILDING_CACHE_BUILDER_UNIT_TESTS
    template<typename StringArray, typename ObjectMapType>
    void write(Diagnostics& diag, uint64_t stringsBaseAddress, uint64_t mapBaseAddress,
               uint64_t cacheBaseAddress, size_t remaining,
               const StringArray& strings, const ObjectMapType& objects)
    {
        ObjectHashTable::write(diag, stringsBaseAddress, mapBaseAddress,
                               cacheBaseAddress, remaining,
                               strings, objects);
        if ( diag.hasError() )
            return;

#if BUILDING_CACHE_BUILDER
        diag.verbose("  found    %u duplicate classes\n",
                   duplicateCount());
        diag.verbose("  class table occupancy %u/%u (%u%%)\n",
                     occupied, capacity,
                     (unsigned)(occupied/(double)capacity*100));
#endif
    }
#endif // BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS
};

class VIS_HIDDEN ProtocolHashTable : public ObjectHashTable
{
public:
#if BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS || BUILDING_CACHE_BUILDER_UNIT_TESTS
    using ObjectHashTable::size;
#endif

    void forEachProtocol(const char* key, ObjectCallback callback) const {
        forEachObject(key, callback);
    }

    using ObjectHashTable::ObjectAndDylibIndex;
    void forEachProtocol(void (^callback)(uint32_t bucketIndex,
                                          const char* protocolName,
                                          const dyld3::Array<ObjectAndDylibIndex>& implCacheInfos)) const {
        forEachObject(callback);
    }

#if BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS || BUILDING_CACHE_BUILDER_UNIT_TESTS
    template<typename StringArray, typename ObjectMapType>
    void write(Diagnostics& diag, uint64_t stringsBaseAddress, uint64_t mapBaseAddress,
               uint64_t cacheBaseAddress, size_t remaining,
               const StringArray& strings, const ObjectMapType& objects)
    {
        ObjectHashTable::write(diag, stringsBaseAddress, mapBaseAddress,
                               cacheBaseAddress, remaining,
                               strings, objects);
        if ( diag.hasError() )
            return;

#if BUILDING_CACHE_BUILDER
        diag.verbose("  protocol table occupancy %u/%u (%u%%)\n",
                     occupied, capacity,
                     (unsigned)(occupied/(double)capacity*100));
#endif
    }
#endif // BUILDING_CACHE_BUILDER || BUILDING_UNIT_TESTS
};

template <typename PointerType>
struct header_info_rw {
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
template<>
struct header_info_rw<uint64_t> {

    bool getLoaded() const {
        return isLoaded;
    }

    void setLoaded() {
        isLoaded = true;
    }

private:
    uint64_t isLoaded              : 1;
    uint64_t allClassesRealized    : 1;
    uint64_t next                  : 62;
};

#if __LP64__ && (BUILDING_DYLD || BUILDING_UNIT_TESTS)
template<>
struct header_info_rw<uintptr_t> {

    bool getLoaded() const {
        return isLoaded;
    }

    void setLoaded() {
        isLoaded = true;
    }

private:
    uint64_t isLoaded              : 1;
    uint64_t allClassesRealized    : 1;
    uint64_t next                  : 62;
};
#endif

template<>
struct header_info_rw<uint32_t> {

    bool getLoaded() const {
        return isLoaded;
    }

    void setLoaded() {
        isLoaded = true;
    }

private:
    uint32_t isLoaded              : 1;
    uint32_t allClassesRealized    : 1;
    uint32_t next                  : 30;
};

#if !__LP64__ && (BUILDING_DYLD || BUILDING_UNIT_TESTS)
template<>
struct header_info_rw<uintptr_t> {

    bool getLoaded() const {
        return isLoaded;
    }

    void setLoaded() {
        isLoaded = true;
    }

private:
    uint32_t isLoaded              : 1;
    uint32_t allClassesRealized    : 1;
    uint32_t next                  : 30;
};
#endif

template <typename PointerType>
struct objc_header_info_ro_t {
};

template<>
class objc_header_info_ro_t<uint32_t> {
private:
    int32_t mhdr_offset;     // offset to mach_header or mach_header_64
    int32_t info_offset;     // offset to objc_image_info *

public:
    const uint64_t mhdrVMAddr(uint64_t baseVMAddr) const {
        return baseVMAddr + mhdr_offset;
    }

    const void* imageInfo() const {
        return (uint8_t*)&info_offset + info_offset;
    }
};

template<>
class objc_header_info_ro_t<uint64_t> {
private:
    int64_t mhdr_offset;     // offset to mach_header or mach_header_64
    int64_t info_offset;     // offset to objc_image_info *

public:
    const uint64_t mhdrVMAddr(uint64_t baseVMAddr) const {
        return baseVMAddr + mhdr_offset;
    }

    const void* imageInfo() const {
        return (uint8_t*)&info_offset + info_offset;
    }
};

#pragma clang diagnostic pop // "-Wunused-private-field"

template <typename PointerType>
struct objc_headeropt_ro_t {
    uint32_t count;
    uint32_t entsize;
    objc_header_info_ro_t<PointerType> headers[0];  // sorted by mhdr address

    objc_header_info_ro_t<PointerType>& getOrEnd(uint32_t i) const {
        assert(i <= count);
        return *(objc_header_info_ro_t<PointerType>*)((uint8_t *)&headers + (i * entsize));
    }

    objc_header_info_ro_t<PointerType>& get(uint32_t i) const {
        assert(i < count);
        return *(objc_header_info_ro_t<PointerType>*)((uint8_t *)&headers + (i * entsize));
    }

    uint32_t index(const objc_header_info_ro_t<PointerType>* hi) const {
        const objc_header_info_ro_t<PointerType>* begin = &get(0);
        const objc_header_info_ro_t<PointerType>* end = &getOrEnd(count);
        assert(hi >= begin && hi < end);
        return (uint32_t)(((uintptr_t)hi - (uintptr_t)begin) / entsize);
    }

    const objc_header_info_ro_t<PointerType>* get(uint64_t headerInfoROVMAddr, uint64_t machoVMAddr) const
    {
        int32_t start = 0;
        int32_t end = count;
        while (start <= end) {
            int32_t i = (start+end)/2;
            objc_header_info_ro_t<PointerType> &hi = get(i);
            uint64_t elementVMOffset = (uint64_t)&hi - (uint64_t)this;
            uint64_t elementVMAddr = headerInfoROVMAddr + elementVMOffset;
            uint64_t elementTargetVMAddr = hi.mhdrVMAddr(elementVMAddr);
            if ( machoVMAddr == elementTargetVMAddr )
                return &hi;
            if ( machoVMAddr < elementTargetVMAddr ) {
                end = i-1;
            } else {
                start = i+1;
            }
        }

        return nullptr;
    }
};

template <typename PointerType>
struct objc_headeropt_rw_t {
    uint32_t count;
    uint32_t entsize;
    header_info_rw<PointerType> headers[0];  // sorted by mhdr address

    uint32_t getCount() const {
        return count;
    }

    void* get(uint32_t i) const {
        assert(i < count);
        return (void*)((uint8_t *)&headers + (i * entsize));
    }

    bool isLoaded(uint32_t i) const {
        return ((header_info_rw<PointerType>*)get(i))->getLoaded();
    }

    void setLoaded(uint32_t i) const {
       ((header_info_rw<PointerType>*)get(i))->setLoaded();
    }
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
static void *getPreoptimizedHeaderRW(const void* headerInfoRO, const void* headerInfoRW,
                                     uint64_t headerInfoROVMAddr, uint64_t machoVMAddr,
                                     bool is64)
{
    if ( is64 ) {
        typedef uint64_t PointerType;
        objc_headeropt_ro_t<PointerType>* hinfoRO = (objc_headeropt_ro_t<PointerType>*)headerInfoRO;
        objc_headeropt_rw_t<PointerType>* hinfoRW = (objc_headeropt_rw_t<PointerType>*)headerInfoRW;
        if ( (hinfoRO == nullptr) || (hinfoRW == nullptr) ) {
            return nullptr;
        }

        const objc_header_info_ro_t<PointerType>* hdr = hinfoRO->get(headerInfoROVMAddr, machoVMAddr);
        if ( hdr == nullptr )
            return nullptr;
        int32_t index = hinfoRO->index(hdr);
        assert(hinfoRW->entsize == sizeof(header_info_rw<PointerType>));
        return &hinfoRW->headers[index];
    } else {
        typedef uint32_t PointerType;
        objc_headeropt_ro_t<PointerType>* hinfoRO = (objc_headeropt_ro_t<PointerType>*)headerInfoRO;
        objc_headeropt_rw_t<PointerType>* hinfoRW = (objc_headeropt_rw_t<PointerType>*)headerInfoRW;
        if ( (hinfoRO == nullptr) || (hinfoRW == nullptr) ) {
            return nullptr;
        }

        const objc_header_info_ro_t<PointerType>* hdr = hinfoRO->get(headerInfoROVMAddr, machoVMAddr);
        if ( hdr == nullptr )
            return nullptr;
        int32_t index = hinfoRO->index(hdr);
        assert(hinfoRW->entsize == sizeof(header_info_rw<PointerType>));
        return &hinfoRW->headers[index];
    }
}

static std::optional<uint16_t> getPreoptimizedHeaderROIndex(const void* headerInfoRO, const void* headerInfoRW,
                                                            uint64_t headerInfoROVMAddr, uint64_t machoVMAddr,
                                                            bool is64)
{
    assert(headerInfoRO != nullptr);
    assert(headerInfoRW != nullptr);
    if ( is64 ) {
        typedef uint64_t PointerType;
        objc_headeropt_ro_t<PointerType>* hinfoRO = (objc_headeropt_ro_t<PointerType>*)headerInfoRO;
        objc_headeropt_rw_t<PointerType>* hinfoRW = (objc_headeropt_rw_t<PointerType>*)headerInfoRW;

        const objc_header_info_ro_t<PointerType>* hdr = hinfoRO->get(headerInfoROVMAddr, machoVMAddr);
        if ( hdr == nullptr )
            return {};
        int32_t index = hinfoRO->index(hdr);
        assert(hinfoRW->entsize == sizeof(header_info_rw<PointerType>));
        return (uint16_t)index;
    } else {
        typedef uint32_t PointerType;
        objc_headeropt_ro_t<PointerType>* hinfoRO = (objc_headeropt_ro_t<PointerType>*)headerInfoRO;
        objc_headeropt_rw_t<PointerType>* hinfoRW = (objc_headeropt_rw_t<PointerType>*)headerInfoRW;

        const objc_header_info_ro_t<PointerType>* hdr = hinfoRO->get(headerInfoROVMAddr, machoVMAddr);
        if ( hdr == nullptr )
            return {};
        int32_t index = hinfoRO->index(hdr);
        assert(hinfoRW->entsize == sizeof(header_info_rw<PointerType>));
        return (uint16_t)index;
    }
}
#pragma clang diagnostic pop // "-Wunused-function"

} // namespace objc

// relative_list_list_t in objc is equivalent to a contiguous array of ListOfListsEntry,
// where the first entry consists in both the entry size and list count.
struct ListOfListsEntry {
    union {
        struct {
            uint64_t imageIndex: 16;
            int64_t  offset: 48;
        };
        struct {
            uint32_t entsize;
            uint32_t count;
        };
    };
};

struct ImpCacheHeader_v1 {
    int32_t  fallback_class_offset;
    uint32_t cache_shift :  5;
    uint32_t cache_mask  : 11;
    uint32_t occupied    : 14;
    uint32_t has_inlines :  1;
    uint32_t bit_one     :  1;
};

/// Added with objc_opt_preopt_caches_version = 3
struct ImpCacheHeader_v2 {
    int64_t  fallback_class_offset;
    uint32_t cache_shift :  5;
    uint32_t cache_mask  : 11;
    uint32_t occupied    : 14;
    uint32_t has_inlines :  1;
    uint32_t padding     :  1;
    uint32_t unused      :  31;
    uint32_t bit_one     :  1;
};

struct ImpCacheEntry_v1 {
    uint32_t selOffset;
    uint32_t impOffset;
};

// Added with objc_opt_preopt_caches_version = 2
struct ImpCacheEntry_v2 {
    int64_t impOffset : 38;
    uint64_t selOffset : 26;
};

#endif /* _OPTIMIZER_OBJC_H_ */
