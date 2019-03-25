//
//  GTrace.cpp
//  IOGraphicsFamily
//
//  Created by bparke on 3/17/17.
//  Rewritten by gvdl 2018-10-24
//

#include <sys/cdefs.h>

#include <osmemory>
#include <osutility>
#include <iolocks>

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOLib.h>

#include "GTrace.hpp"
#include "GMetricTypes.h"

#if KERNEL

#include <IOKit/assert.h>

#include <mach/vm_param.h>
#include <mach/mach_types.h>
__BEGIN_DECLS
#include <machine/cpu_number.h>
__END_DECLS
#include <IOKit/graphics/IOGraphicsPrivate.h>  // Debug logging macros

#else // !KERNEL

#include <mutex>

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include <mach/mach_time.h>
#include <IOKit/IOReturn.h>

#include <pthread.h>
#define cpu_number() (-1)

#define D(categ, name, args...) do{}while(0)  // Not needed in userland

#endif // KERNEL

#define DGT(args...) D(TRACE, "GTraceBuffer", args)

OSDefineMetaClassAndFinalStructors(GTraceBuffer, OSObject);

using iog::LockGuard;
using iog::OSUniqueObject;

// Give this variable a well known name that can be found in a corefile.
extern GTraceBuffer::shared_type gGTraceArray[kGTraceMaximumBufferCount];
GTraceBuffer::shared_type gGTraceArray[kGTraceMaximumBufferCount];

namespace {

enum ThreadWakeups : bool {
    kOneThreadWakeup  = true,
    kManyThreadWakeup = false,
};

struct Globals {
    IOLock* fLock;
    uint32_t fCurBufferID;
    Globals() : fLock(IOLockAlloc())
        { DGT(" lckOK%d\n", static_cast<bool>(fLock)); }
    ~Globals() { IOLockFree(fLock); }
} sGlobals;
#define sLock        (sGlobals.fLock)
#define sCurBufferID (sGlobals.fCurBufferID)

// Grabbed from somewhere on the inter-tubes
uint32_t nextPowerOf2(uint32_t x)
{
    const uint64_t max32bit = UINT64_C(1) << 32;
    assert(x <= (max32bit-1)/2);
    const auto shift = __builtin_clz(x-1);
    return (x <= 2) ? x : static_cast<uint32_t>(max32bit >> shift);
}

constexpr bool isPowerOf2(const uint64_t x) { return 0 == (x & (x-1)); }

template <typename T>
T bound(const T min, const T x, const T max)
{
    assert(min <= max);
    return (min > x) ? min : ((max < x) ? max : x);
}

// define a KERNEL and test environment dependant wrapper
uint64_t threadID()
{
    uint64_t tid;
#if KERNEL
    tid = thread_tid(current_thread());
#else
    pthread_threadid_np(NULL, &tid);
#endif
    return tid;
}

uint8_t cpu()
{
    return static_cast<uint8_t>(cpu_number());
}

};  // namespace

// GTraceBuffer implementation

// Check basic assumptions, power of two assumptions
static_assert(isPowerOf2(kGTraceEntrySize), "Entry size not a power of two");
static_assert(isPowerOf2(kGTraceMinimumLineCount), "Min not a power of two");
static_assert(isPowerOf2(kGTraceMaximumLineCount), "Max not a power of two");

bool GTraceBuffer::init()
{
    // Always call super::init even if we are going to fail later
    return super::init() && static_cast<bool>(sLock);
}

void GTraceBuffer::free()
{
    DGT("[%d]\n", bufferID());
    if (fBuffer) {
        IODelete(fBuffer, GTraceEntry, fLineCount);
        fBuffer = nullptr;
        fLineCount = 0;
    }
    super::free();
}

/* static */ GTraceBuffer::shared_type GTraceBuffer::make(
        const char* decoderName, const char* bufferName,
        const uint32_t lineCount, breadcrumb_func bcf, void* context)
{
    shared_type ret(new GTraceBuffer);
    if (static_cast<bool>(ret)) {
        IOReturn err
            = ret->init(decoderName, bufferName, lineCount, bcf, context);
        if (!err) {
            // try to find a spare index
            LockGuard<IOLock> locked(sLock);
            for (int i = 0; i < kGTraceMaximumBufferCount; ++i) {
                if ( !static_cast<bool>(gGTraceArray[i]) ) {
                    gGTraceArray[i] = ret;  // Cache a shared copy
                    ret->fHeader.fBufferIndex = i;
                    DGT("[%d] (d=%s,b=%s,lc=%u, bcf=%s) {ind=%d}\n",
                        ret->bufferID(), decoderName, bufferName,
                        lineCount, static_cast<bool>(bcf) ? "yes" : "no", i);
                    return ret;
                }
            }
            err = kIOReturnNoResources;
        }
        DGT("[%d] (d=%s,b=%s,lc=%u, %s bcf) -> %x\n",
            ret->bufferID(), decoderName, bufferName, lineCount,
            static_cast<bool>(bcf) ? "has" : "no", err);
        IOLog("Failed %x to create "
              "GTraceBuffer %s for %s decoder and count %d\n",
              err, bufferName, decoderName, lineCount);
        ret.reset();  // Release our shared_object reference
    }

    return ret;
}

IOReturn GTraceBuffer::init(
        const char* decoderName, const char* bufferName, const uint32_t count,
        breadcrumb_func bcf, const void* context)
{
    if (!init()) {
        IOLog("GTraceBuffer init() failed\n");
        return kIOReturnNoMemory;
    }

    if ( !static_cast<bool>(decoderName) || !*decoderName) {
        IOLog("GTraceBuffer bad decoder name\n");
        return kIOReturnBadArgument;
    }

    memset(&fHeader, '\0', sizeof(fHeader));
    fHeader.fBufferIndex = static_cast<uint16_t>(-1);  // Sentinel, never valid

    const auto roundedCount = nextPowerOf2(count);
    const auto lines = bound(
            kGTraceMinimumLineCount, roundedCount, kGTraceMaximumLineCount);
    fBuffer = IONew(GTraceEntry, lines);
    if (fBuffer)
        memset(fBuffer, '\0', lines * sizeof(GTraceEntry));
    else
        return kIOReturnNoMemory;

    // From here on I expect the code to run quickly
    fLineCount = lines;
    fLineMask = lines - 1;  // It must be a power of 2, enforced by nextPowerOf2
    fNextLine = 0;
    fBreadcrumbFunc = bcf;
    fBCFContext = context;
    fBCData = nullptr;
    fBCActiveCount = 0;

    fHeader.fVersion = GTRACE_REVISION;
    fHeader.fCreationTime = mach_continuous_time();
    if (bufferName)
        GPACKSTRING(fHeader.fBufferName, bufferName);
    GPACKSTRING(fHeader.fDecoderName, decoderName);
    fHeader.fTokensMask = static_cast<uint16_t>(fLineMask);

    {
        LockGuard<IOLock> locked(sLock);
        fHeader.fBufferID = sCurBufferID++;
    }

    DGT("[%d] {lc=%x lm=%x}\n", bufferID(), fLineCount, fLineMask);
    return kIOReturnSuccess;
}

#define namecpy(dst, buf, len) \
    snprintf(dst, len, "%.*s", \
             static_cast<int>(sizeof(buf)), reinterpret_cast<const char*>(buf))
size_t GTraceBuffer::decoderName(char* outName, const int outLen) const
{
    return namecpy(outName, fHeader.fDecoderName, outLen);
}

size_t GTraceBuffer::bufferName(char* outName, const int outLen) const
{
    return namecpy(outName, fHeader.fBufferName, outLen);
}
#undef namecpy

// Carefully formed to use copy elision. In theory the target will be
// initialised directly by recordToken, but also allows us to create a defered
// start and store the value on the stack.
GTraceEntry
GTraceBuffer::formatToken(const uint16_t line,
                          const uint64_t tag1, const uint64_t arg1,
                          const uint64_t tag2, const uint64_t arg2,
                          const uint64_t tag3, const uint64_t arg3,
                          const uint64_t tag4, const uint64_t arg4,
                          const uint64_t timestamp)
{
    return GTraceEntry(timestamp, line, bufferID(),
                       cpu(), threadID(), /* objectID */ 0,
                       MAKEGTRACETAG(tag1), MAKEGTRACEARG(arg1),
                       MAKEGTRACETAG(tag2), MAKEGTRACEARG(arg2),
                       MAKEGTRACETAG(tag3), MAKEGTRACEARG(arg3),
                       MAKEGTRACETAG(tag4), MAKEGTRACEARG(arg4));
}

void GTraceBuffer::recordToken(const GTraceEntry& entry)
{
    fBuffer[getNextLine()] = entry;
}

namespace {
IOReturn fetchValidateAndMap(IOMemoryDescriptor* outDesc,
                             OSUniqueObject<IOMemoryMap>* mapP)
{
    IOReturn err = kIOReturnSuccess;
    const auto len = outDesc->getLength();

    // Minimal length required to copy out header, range check
    if (len < sizeof(IOGTraceBuffer))
        err = kIOReturnBadArgument;
    else if (len > INT_MAX)
        err = kIOReturnBadArgument;
    else {
        OSUniqueObject<IOMemoryMap> map(outDesc->map());
        if (static_cast<bool>(map))
            *mapP = iog::move(map);
        else
            err = kIOReturnVMError;
    }
    return kIOReturnSuccess;
}
} // namespace

// Call from an unlocked environment
IOReturn GTraceBuffer::copyOut(
        OSUniqueObject<IOMemoryMap> map, OSData* bcData) const
{
    iog::locking_primitives::assertUnlocked(sLock);

    GTraceHeader header = fHeader;
    const auto outLen = static_cast<int>(map->getLength());
    void* outP = reinterpret_cast<void*>(map->getVirtualAddress());
    auto* outBufP = static_cast<IOGTraceBuffer*>(outP);
    uint16_t bcTokens = 0;

    int remaining = outLen - sizeof(IOGTraceBuffer);
    assert(remaining >= 0);  // Guaranteed by fetchValidateAndMap

    const uint32_t bcDataLen
        = static_cast<bool>(bcData) ? bcData->getLength() : 0;
    const bool hasBCFunc = static_cast<bool>(fBreadcrumbFunc);
    if (bcDataLen || hasBCFunc) {
        const uint16_t kBCLen = static_cast<uint16_t>(
            (remaining < kGTraceMaxBreadcrumbSize)
                ? remaining : kGTraceMaxBreadcrumbSize);
        uint16_t bcCopied = 0;
        if (bcDataLen) {
            bcCopied = (bcDataLen < kBCLen) ? bcDataLen : kBCLen;
            memcpy(&outBufP->fTokens[0], bcData->getBytesNoCopy(), bcCopied);
        }
        else {
            assert(hasBCFunc);
            void* buf = IOMalloc(kGTraceMaxBreadcrumbSize);
            if (buf) {
                // Cast the const away, internally we don't touch the data but
                // the breadcrumb func can modify the context if it chooses.
                void* context = const_cast<void*>(fBCFContext);
                bcCopied = kBCLen;
                IOReturn err = (*fBreadcrumbFunc)(context, buf, &bcCopied);
                if (!err) {
                    if (bcCopied > kBCLen) {
                        // Client's bcFunc may cause a buffer overrun!
                        err = kIOReturnInternalError;
                    } else if (bcCopied)
                        memcpy(&outBufP->fTokens[0], buf, bcCopied);
                }
                if (err) {
                    bcCopied = 0;
                    IOLog("GTrace[%u] bad breadcrumb %x, dropping\n",
                            bufferID(), err);
                }
                IOFree(buf, kGTraceMaxBreadcrumbSize);
            }
        }

        // Round bcCopied to number of fTokens used
        bcTokens = (bcCopied + (kGTraceEntrySize - 1)) / kGTraceEntrySize;
        header.fBreadcrumbTokens = bcTokens;
        remaining -= bcTokens * kGTraceEntrySize;  // Might be negative now
    }
    auto* const outTokensP = &outBufP->fTokens[bcTokens];
    header.fTokenLine = nextLine();
    if (remaining >= kGTraceEntrySize) {
        // We have room to copy at least one entry
        const int outNumEntries = remaining / kGTraceEntrySize;
        const int availLines = fLineCount;
        const int copyEntries
            = (availLines < outNumEntries) ? availLines : outNumEntries;

        // Copy and obfuscate trace entries into client's buffer.
        int curToken = 0;
        for (int i = 0; i < copyEntries; i++) {
            GTraceEntry entry = fBuffer[i];
            if (!static_cast<bool>(entry.timestamp()))  // Skip unused slots
                continue;

            const auto& tags = entry.fArgsTag;
            for (int j = 0; tags.tag() && j < GTraceEntry::ArgsTag::kNum; ++j) {
                if (kGTRACE_ARGUMENT_POINTER & tags.tag(j)) {
                    vm_offset_t outArg = 0;
                    vm_kernel_addrperm_external(
                        static_cast<vm_offset_t>(entry.arg64(j)), &outArg);
                    entry.arg64(j) = outArg;
                }
            }
            outTokensP[curToken++] = entry;
        }
        header.fTokensCopied = curToken;
    }
    // Add breadcrumb size to copied tokens if any.
    header.fTokensCopied += bcTokens;
    header.fBufferSize = kGTraceHeaderSize
                       + header.fTokensCopied * sizeof(GTraceEntry);
    outBufP->fHeader = header;  // Copy out the header

    DGT("[%d] sz%d bc%d tkn%d\n", bufferID(),
        header.fBufferSize, bcTokens, header.fTokensCopied - bcTokens);
    return kIOReturnSuccess;
}

/* static */ IOReturn
GTraceBuffer::fetch(const uint32_t index, IOMemoryDescriptor* outDesc)
{
    if (index >= kGTraceMaximumBufferCount)
        return kIOReturnNotFound;

    shared_type traceBuffer;
    bool releaseBuffer = false;
    {
        // Locked while we copy cached GTraceBuffer to a local OSSharedObject
        LockGuard<IOLock> locked(sLock);

        auto& so = gGTraceArray[index];  // alias
        if (!static_cast<bool>(so))
            return kIOReturnNotFound;
        DGT("[%d] cached buffer %u, uc=%ld\n",
            so->bufferID(), index, so.use_count());

        releaseBuffer = (1 == so.use_count());  // Release if unique
        traceBuffer = so; // Copy cached buffer locally, adds reference
    }
    __unused const auto bufferID = traceBuffer->bufferID();

    // Drop lock while creating a kernel mapping
    OSUniqueObject<IOMemoryMap> map;
    IOReturn err = fetchValidateAndMap(outDesc, &map);
    if (err) return err;

    OSData* bcData = nullptr;
    bool isBCActive = false;
    {
        // Locked to check for fetch()s on other threads.
        LockGuard<IOLock> locked(sLock);

        if (static_cast<bool>(traceBuffer->fBCData))
            iog::swap(bcData, traceBuffer->fBCData);
        auto& activeCount = traceBuffer->fBCActiveCount;
        while ((isBCActive = static_cast<bool>(traceBuffer->fBreadcrumbFunc))) {
            if (activeCount < 8) {  // Arbitary max thread count
                ++activeCount;
                break;
            }
            IOLockSleep(sLock, &activeCount, THREAD_UNINT);
        }
    }

    err = traceBuffer->copyOut(iog::move(map), bcData);
    OSSafeReleaseNULL(bcData);
    // Well that was quick, time for more boilerplate

    {
        // Lock again to clear out buffer cache entry and decrement active count
        LockGuard<IOLock> locked(sLock);
        if (!err && releaseBuffer) {
            // This line invalidates our cached gGTraceArray entry and replaces
            // out traceBuffer reference with the previously cached entry. The
            // net effect is to hold the last reference to the buffer in
            // traceBuffer.
            traceBuffer = iog::move(gGTraceArray[index]);

            // Invariant: A use_count of 1 means that only the gGTraceArray
            // entry is valid as this code NEVER exports an OSSharedObject from
            // the cache entry it must still be one at this point, just before
            // we release it.
            assert(1 == traceBuffer.use_count());
        }
        if (isBCActive) {
            auto& activeCount = traceBuffer->fBCActiveCount;
            --activeCount;
            IOLockWakeup(sLock, &activeCount, kManyThreadWakeup);
        }
    }

    DGT("[%d] {tuc=%ld} -> %x\n", bufferID, traceBuffer.use_count(), err);
    return err;
}

// Note at the end of this function only the cached reference will remain until
// the next run of iogdiagnose.
/* static */ void GTraceBuffer::destroy(shared_type&& inBso)
{
    DGT("[%d]\n", inBso->bufferID());
    shared_type bso(iog::move(inBso));  // Grab a move construct copy

    breadcrumb_func breadcrumbFunc = nullptr;
    const void* ccontext = nullptr; // Context to be passed to fBreadcrumbFunc
    {
        LockGuard<IOLock> locked(sLock);
        while (bso->fBCActiveCount)
            IOLockSleep(sLock, &bso->fBCActiveCount, THREAD_UNINT);
        iog::swap(breadcrumbFunc, bso->fBreadcrumbFunc);
        iog::swap(ccontext, bso->fBCFContext);
        // Can safely drop the lock now, as the client must still be around as
        // it is calling us now and fetcher will not call out anymore.
    }

    if (!static_cast<bool>(breadcrumbFunc))
        return;

    // Copy the breadcrumb data on first call to destroy()
    assert(!bso->fBCData);

    void* buf = IOMalloc(kGTraceMaxBreadcrumbSize);
    if (buf) {
        // Cast the const away, internally we don't touch the data but the
        // breadcrumb func can modify the context if it chooses.
        void* context = const_cast<void*>(ccontext);

        uint16_t bcCopied = kGTraceMaxBreadcrumbSize;
        const IOReturn err = (*breadcrumbFunc)(context, buf, &bcCopied);
        if (!err && bcCopied)
            bso->fBCData = OSData::withBytes(buf, bcCopied);
        IOFree(buf, kGTraceMaxBreadcrumbSize);
    }

    // bso now goes out of scope and its reference dropped.
}

/* static */ IOReturn
GTraceBuffer::fetch(GTraceBuffer::shared_type bso, IOMemoryDescriptor* outDesc)
{
    OSUniqueObject<IOMemoryMap> map;
    IOReturn err = fetchValidateAndMap(outDesc, &map);
    if (!err)
        err = bso->copyOut(iog::move(map), nullptr);
    DGT("[%d] {buc=%ld} -> %x\n", bso->bufferID(), bso.use_count(), err);
    return err;
}

/* static */ GTraceBuffer* GTraceBuffer::makeArchaicCpp(
        const char* decoderName, const char* bufferName,
        const uint32_t lineCount, breadcrumb_func bcf, void* context)
{
    shared_type bso = make(decoderName, bufferName, lineCount, bcf, context);
    if (!static_cast<bool>(bso)) return nullptr;
    bso->fArchaicCPPSharedObjectHack = bso;
    return bso.get();
}

/* static */ void
GTraceBuffer::destroyArchaicCpp(GTraceBuffer *buffer)
{
    destroy(iog::move(buffer->fArchaicCPPSharedObjectHack)); // 🤮
}
