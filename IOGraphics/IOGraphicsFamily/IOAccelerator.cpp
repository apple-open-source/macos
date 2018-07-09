/*
 * Copyright (c) 2017 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/assert.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOUserClient.h>

#include <IOKit/graphics/IOAccelerator.h>
#include <IOKit/graphics/IOGraphicsTypesPrivate.h>

#include "IOGraphicsKTrace.h"

OSDefineMetaClassAndStructors(IOAccelerator, IOService)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// 'static' helpers for IOAccelerator and IOAccelerationUserClient
namespace {
// kUnusedID must be memset-able, that is the uint8_t cast is repeated 4 times
const uint32_t kUnusedID = 0x80808080; // can't ever be returned from createID
const int kInitTableSize = 32 * sizeof(uint32_t);

// Range checked lookup of a uint32_t within a data
inline uint32_t *getID(OSData *data, int index)
{
    const uint32_t *uP = static_cast<const uint32_t*>(
            data->getBytesNoCopy(index * sizeof(*uP), sizeof(*uP)));
    return const_cast<uint32_t*>(uP);
}

bool appendWord(uint32_t value, uint32_t fill, OSData *data)
{
    const int index = data->getLength() / sizeof(value);
    bool ret = data->appendByte(fill, data->getCapacityIncrement());
    if (ret) {
        uint32_t *uP = getID(data, index);
        *uP = value;
    }
    return ret;
}

// RAII lock helper, auto unlocks at end of scope
struct MutexLock {
    MutexLock(IOLock *lock) : fLock(lock) { IOLockLock(fLock); }
    ~MutexLock() { IOLockUnlock(fLock); }
private:
    IOLock * const fLock;
};

// Primary data structure to track IDs both those with specified IDs and those
// that are just allocated with the lower free id.
class IDState {
private:
    IOLock *fLock;

    // Stores requested ID retain counts from -4096 <= id <= 4095 (zzid <= 8191)
    OSData *fSpecifiedIDsData;

    // Stores allocated ID retain counts, add 4096 to user id, max of 68 * 1024
    OSData *fRetainIDsData;

    static const uint32_t kMaxEntries = 64 * 1024;  // arbitary maximum of ID
    // The negative -4096 ID is not a legal request
    static const uint32_t kMaxRequestedZigZag = 8190;     // int2zz(4095)
    static const uint32_t kAllocatedIDBase = 4096;
    static const uint32_t kTaskOwned = (1 << (8*sizeof(kTaskOwned) - 1));
    static const uint32_t kNullRef = 0;

    // Returned from locateID, which decodes userland IDs
    struct IDDataDecode {
        uint32_t *fUP;
        OSData *fIDsData;
        uint32_t fArrayInd;
    };

    // As the requested IOAccelID can be negative we have to be careful with
    // the underlying array of retain counts. Assuming that small ids are more
    // common then larger then zigzag encoding efficiently maps small absolute
    // ids into small unsigned ids, which we use to index into retain array.
    //
    // Zigzag encoding, 0 -1 1 -2 2 -3 3 => 0 1 2 3 4 5 6
    // see http://neurocline.github.io/dev/2015/09/17/zig-zag-encoding.html
    // Encoding, shift up number by one and xor with sign-extended sign bit.
    // TODO(gvdl): C++11 use constexpr and rewrite kMaxRequestedZigZag above.
    inline uint32_t int2zz(int32_t x)  { return (x << 1) ^ (x >> 31); }

    inline IOReturn reserveRequested(uint32_t taskOwned, IOAccelID id,
                                     IOAccelID *idOutP)
    {
        const uint32_t oneRef = 1 | taskOwned;
        const uint32_t zzid = int2zz(id);

        MutexLock locked(fLock);

        if (zzid > kMaxRequestedZigZag)
            return kIOReturnExclusiveAccess;

        const int newLen = (zzid + 1) * sizeof(uint32_t);
        const int oldLen = fSpecifiedIDsData->getLength();
        if (newLen > oldLen
        &&  !fSpecifiedIDsData->appendByte(kNullRef, newLen - oldLen))
            return kIOReturnNoMemory;

        uint32_t *uP = getID(fSpecifiedIDsData, zzid);
        assert(uP); // Must be valid, it fits or we grew the table
        if (*uP)
            return kIOReturnExclusiveAccess;
        *uP = oneRef;
        *idOutP = id;
        return kIOReturnSuccess;
    }

    inline IOReturn allocID(uint32_t taskOwned, IOAccelID *idOutP)
    {
        const uint32_t oneRef = 1 | taskOwned;

        MutexLock locked(fLock);

        // Look for an unused entry
        unsigned i;
        for (i = 0; uint32_t *uP = getID(fRetainIDsData, i); ++i)
            if (!*uP) {
                *uP = oneRef;
                *idOutP = i + kAllocatedIDBase;
                return kIOReturnSuccess;
            }
        // No unused entries found
        if (i >= kMaxEntries)  // Check for a table that is too large
            return kIOReturnNoResources;
        // Append a new reference
        bool res = appendWord(oneRef, kNullRef, fRetainIDsData);
        assert(*getID(fRetainIDsData, i) == oneRef);
        if (res) {
            *idOutP = i + kAllocatedIDBase;
            return kIOReturnSuccess;
        }
        else
            return kIOReturnNoMemory;
    }

    IDDataDecode locateID(IOAccelID id)
    {
        IDDataDecode ret = { NULL, NULL, -1 };
        const uint32_t zzid = int2zz(id);
        const uint32_t entryid = id - kAllocatedIDBase;
        if (zzid <= kMaxRequestedZigZag) {
            ret.fIDsData = fSpecifiedIDsData;
            ret.fArrayInd = zzid;
        } else if (entryid < kMaxEntries) { // < 0 range check uint32
            ret.fIDsData = fRetainIDsData;
            ret.fArrayInd = entryid;
        }
        else
            return ret;
        ret.fUP = getID(ret.fIDsData, ret.fArrayInd);
        return ret;
    }

public:
    IDState() : fLock(IOLockAlloc()),
                fSpecifiedIDsData(OSData::withCapacity(kInitTableSize)),
                fRetainIDsData(OSData::withCapacity(kInitTableSize))
    {
        if (!fLock || !fSpecifiedIDsData || !fRetainIDsData) return;
        if (!fSpecifiedIDsData->appendByte(kNullRef, kInitTableSize)) return;
        // Must be last field to be initialised used by hasInited() below.
        if (!fRetainIDsData->appendByte(kNullRef, kInitTableSize)) return;
    }
    bool hasInited() { return getID(fRetainIDsData, 0); }

    // Destructor only runs once when the IOGraphicsFamily unloads.
    ~IDState()
    {   // TODO(gvdl): What happens to entries in use?
        OSSafeReleaseNULL(fSpecifiedIDsData);
        OSSafeReleaseNULL(fRetainIDsData);
        if (fLock) {
            IOLockFree(fLock);
            fLock = NULL;
        }
    }

    IOLock *lock() const { return fLock; }

    IOReturn createID(const bool taskOwned, const IOOptionBits options,
                      const IOAccelID requestedID, IOAccelID *idOutP)
    {
        const uint32_t taskOwnedMask = (taskOwned) ? kTaskOwned : 0;
        IOReturn ret;
        if (kIOAccelSpecificID & options)
            ret = reserveRequested(taskOwnedMask, requestedID, idOutP);
        else
            ret = allocID(taskOwnedMask, idOutP);
        return ret;
    }

    // Must be locked
    IOReturn validID(int32_t id)
    {
        const IDDataDecode d = locateID(id);
        if (d.fUP && *d.fUP & ~kTaskOwned)  // in range and retained
            return kIOReturnSuccess;
        else
            return kIOReturnBadMessageID;
    }

    // Must be locked and id must have been validated
    void retainID(int32_t id)
    {
        assert(kIOReturnSuccess == validID(id));
        const IDDataDecode d = locateID(id);
        assert(d.fUP);
        const auto value = *d.fUP;
        if ((value + 1) & ~kTaskOwned)
            *d.fUP = value + 1;
        else
            *d.fUP = (value & kTaskOwned) | (-1U >> 1);  // Saturated
    }

    // Must be locked and the id must have been validated
    void releaseID(bool taskOwned, int32_t id)
    {
        assert(kIOReturnSuccess == validID(id));
        const IDDataDecode d = locateID(id);
        assert(d.fUP);
        const auto idOwned = *d.fUP & kTaskOwned;
        const auto value = *d.fUP & ~kTaskOwned;

        if (!taskOwned && idOwned && value == 1)
            panic("IOAccelerator::releaseID still task owned");
        assert(value);
        if (value) {
            // If taskOwned, then remove the flag and reference, otherwise
            // maintian the previous value.
            *d.fUP = (value - 1) | (taskOwned ? 0 : idOwned);
        }
    }
};  // end class IDState

IDState sIDState;  // Global variable, inited at load time
};  // end anonymous namespace

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Check fundamental assumption of data arrays
OSCompileAssert(sizeof(kUnusedID) == sizeof(IOAccelID));

class IOAccelerationUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOAccelerationUserClient);
    using super = IOUserClient;

private:
    OSData *fIDListData;  // list of allocated ids for this task

    IOReturn extCreate(IOOptionBits options,
                       IOAccelID requestedID, IOAccelID *idOutP);
    IOReturn extDestroy(IOOptionBits options, IOAccelID id);

public:
    // OSObject overrides
    virtual void free() APPLE_KEXT_OVERRIDE;

    // IOService overrides
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;

    // IOUserClient overrides
    virtual bool initWithTask(task_t, void*, uint32_t,  OSDictionary*)
        APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientClose() APPLE_KEXT_OVERRIDE;

    virtual IOExternalMethod *
        getTargetAndMethodForIndex(IOService **targetP, uint32_t index)
        APPLE_KEXT_OVERRIDE;
};
OSDefineMetaClassAndStructors(IOAccelerationUserClient, IOUserClient);

bool IOAccelerationUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type,
             OSDictionary *properties)
{
    IOAUC_START(initWithTask,type,0,0);
    if (properties)
        properties->setObject(
                kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
    bool ret = super::initWithTask(owningTask, securityID, type, properties);
    if (ret)
        ret = sIDState.hasInited();
    IOAUC_END(initWithTask,ret,0,0);
    return ret;
}

bool IOAccelerationUserClient::start(IOService *provider)
{
    IOAUC_START(start,0,0,0);
    bool ret = super::start(provider);
    if (ret) {
        fIDListData = OSData::withCapacity(kInitTableSize);
        if (fIDListData)
            ret = fIDListData->appendByte(static_cast<unsigned char>(kUnusedID),
                                          fIDListData->getCapacity());
        else
            ret = false;
    }
    IOAUC_END(start,ret,0,0);
    return ret;
}

void IOAccelerationUserClient::free()
{
    IOAUC_START(free,0,0,0);
    OSSafeReleaseNULL(fIDListData);
    super::free();
    IOAUC_END(free,0,0,0);
}

IOReturn IOAccelerationUserClient::clientClose()
{
    IOAUC_START(clientClose,0,0,0);
    if (!isInactive())
        terminate();
    IOAUC_END(clientClose,kIOReturnSuccess,0,0);
    return kIOReturnSuccess;
}

void IOAccelerationUserClient::stop(IOService *provider)
{
    IOAUC_START(stop,0,0,0);
    MutexLock locked(sIDState.lock());

    for (int i = 0; uint32_t *uP = getID(fIDListData, i); ++i)
        if (*uP != kUnusedID) {
            sIDState.releaseID(/* taskOwned */ true, *uP);
            *uP = kUnusedID;
        }
    super::stop(provider);
    IOAUC_END(stop,0,0,0);
}

// Uses archaic 32bit user client interfaces
IOExternalMethod *IOAccelerationUserClient::getTargetAndMethodForIndex(IOService **targetP, uint32_t index)
{
    IOAUC_START(getTargetAndMethodForIndex,index,0,0);
    static const IOExternalMethod methodTemplate[] =
    {
        /* 0 */  { NULL, (IOMethod) &IOAccelerationUserClient::extCreate,
                    kIOUCScalarIScalarO, 2, 1 },
        /* 1 */  { NULL, (IOMethod) &IOAccelerationUserClient::extDestroy,
                    kIOUCScalarIScalarO, 2, 0 },
    };

    if (index >= (sizeof(methodTemplate) / sizeof(methodTemplate[0])))
    {
        IOAUC_END(getTargetAndMethodForIndex,0,__LINE__,0);
        return NULL;
    }

    *targetP = this;
    IOAUC_END(getTargetAndMethodForIndex,0,0,0);
    return const_cast<IOExternalMethod *>(&methodTemplate[index]);
}

namespace {
IOReturn trackID(const IOAccelID id, OSData *idsData)
{
    for (int i = 0; uint32_t *uP = getID(idsData, i); ++i)
        if (*uP == kUnusedID) {
            *uP = id;
            return kIOReturnSuccess;
        }
    // No free entries in table, grow the table a bit
    if (appendWord(id, kUnusedID, idsData))
        return kIOReturnSuccess;
    else
        return kIOReturnNoMemory;
}
};  // namespace

IOReturn IOAccelerationUserClient::extCreate(IOOptionBits options, IOAccelID requestedID, IOAccelID *idOutP)
{
    IOAUC_START(extCreate,options,requestedID,0);
    IOReturn ret = sIDState.createID(/* taskOwned */ true,
                                     options, requestedID, idOutP);
    if (kIOReturnSuccess == ret) {
        MutexLock locked(sIDState.lock());

        ret = trackID(*idOutP, fIDListData);
        if (ret)
            sIDState.releaseID(/* taskOwned */ true, *idOutP);
    }
    IOAUC_END(extCreate,ret,0,0);
    return ret;
}

IOReturn IOAccelerationUserClient::extDestroy(IOOptionBits /* options */, const IOAccelID id)
{
    IOAUC_START(extDestroy,id,0,0);
    MutexLock locked(sIDState.lock());
    IOReturn ret = sIDState.validID(id);
    if (ret)
    {
        IOAUC_END(extDestroy,kIOReturnBadMessageID,__LINE__,0);
        return kIOReturnBadMessageID;
    }

    for (int i = 0; uint32_t *uP = getID(fIDListData, i); ++i)
        if (static_cast<unsigned>(id) == *uP) {
            // we allocated this id and it is valid
            sIDState.releaseID(/* taskOwned */ true, id);
            *uP = kUnusedID;
            IOAUC_END(extDestroy,kIOReturnSuccess,0,0);
            return kIOReturnSuccess;
        }
    IOAUC_END(extDestroy,kIOReturnBadMessageID,0,0);
    return kIOReturnBadMessageID;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOAccelerator::createAccelID(IOOptionBits options, IOAccelID *idOutP)
{
    IOA_START(createAccelID,options,0,0);
    IOReturn ret = sIDState.createID(/* taskOwned */ false,
                                     options, *idOutP, idOutP);
    IOA_END(createAccelID,ret,0,0);
    return ret;
}

IOReturn
IOAccelerator::retainAccelID(IOOptionBits /* options */, IOAccelID id)
{
    IOA_START(retainAccelID,id,0,0);
    MutexLock locked(sIDState.lock());
    IOReturn ret = sIDState.validID(id);
    if (!ret)
        sIDState.retainID(id);
    IOA_END(retainAccelID,ret,0,0);
    return ret;
}

IOReturn
IOAccelerator::releaseAccelID(IOOptionBits /* options */, IOAccelID id)
{
    IOA_START(releaseAccelID,id,0,0);
    MutexLock locked(sIDState.lock());
    IOReturn ret = sIDState.validID(id);
    if (!ret)
        sIDState.releaseID(/* taskOwned */ false, id);
    IOA_END(releaseAccelID,ret,0,0);
    return ret;
}
