//
//  GMetric.cpp
//  IOGraphicsFamily
//
//  Created by Jérémy Tran on 8/16/17.
//

#include <sys/cdefs.h>

#include <stddef.h>

#include <iolocks>

#include "GMetric.hpp"

#include <mach/mach_time.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>

#if KERNEL

__BEGIN_DECLS
#include <machine/cpu_number.h>
__END_DECLS
#include <IOKit/graphics/IOGraphicsPrivate.h>  // Debug logging macros

#else  // !KERNEL

#include <pthread.h>
#define cpu_number() (-1)

#define D(categ, name, args...) do{}while(0)  // Not needed in userland

#endif // KERNEL

#define DGM(args...) D(TRACE, "GMetric", args)

OSDefineMetaClassAndStructors(GMetricsRecorder, OSObject);

using iog::LockGuard;

namespace {

const uint64_t kThreadIDMask = 0xffffff;  // 24 bits

struct Globals {
    IOLock *fLock;
    Globals() : fLock(IOLockAlloc()) { }
    ~Globals() { IOLockFree(fLock); }
} sGlobals;

constexpr bool isPowerOf2(const uint64_t x) { return 0 == (x & (x-1)); }
uint32_t nextPowerOf2(uint32_t x)
{
    const uint64_t max32bit = UINT64_C(1) << 32;
    assert(x <= (max32bit-1)/2);
    const auto shift = __builtin_clz(x-1);
    return (x <= 2) ? x : static_cast<uint32_t>(max32bit >> shift);
}
};  // namespace

// GMetricsRecorder static variables
/* static */ GMetricsRecorder*
    GMetricsRecorder::sRecorder;
/* static */ _Atomic(gmetric_domain_t)
    GMetricsRecorder::sDomains = kGMETRICS_DOMAIN_NONE;

// GMetricsRecorder implementation
/* static */ IOReturn
GMetricsRecorder::prepareForRecording()
{
    return prepareForRecording(0);  // Use default size
}

/* static */ IOReturn
GMetricsRecorder::prepareForRecording(const uint64_t userClientEntries)
{
    IOReturn err = kIOReturnNoMemory;
    const uint32_t entriesCount = nextPowerOf2(
            (userClientEntries > kGMetricMaximumLineCount)
            ? kGMetricMaximumLineCount
            : (userClientEntries
                ? static_cast<uint32_t>(userClientEntries)
                : kGMetricDefaultLineCount));

    LockGuard<IOLock> locked(sGlobals.fLock);
    if (isReady()) {
        if (entriesCount == sRecorder->fLineCount)
            return kIOReturnSuccess;
        OSSafeReleaseNULL(sRecorder);
    }
    GMetricsRecorder* me = new GMetricsRecorder;
    if (me && (err = me->initWithCount(entriesCount)))
        OSSafeReleaseNULL(me);
    sRecorder = me;
    DGM("(e=%lld) {re=%u} -> %x\n",
        userClientEntries, entriesCount, err);
    return err;
}

/* static */ void GMetricsRecorder::doneWithRecording()
{
    LockGuard<IOLock> locked(sGlobals.fLock);
    if (isReady()) {
        const bool isEnabled = isDomainActive(kGMETRICS_DOMAIN_ENABLED);
        DGM(" cleanup with %s\n", isEnabled ? "reset" : "release");
        if (isEnabled)
            sRecorder->resetLocked();
        else
            OSSafeReleaseNULL(sRecorder);
    }
}

/* static */ void
GMetricsRecorder::record(const gmetric_domain_t domain,
                         const gmetric_event_t type, const uint64_t arg)
{
    LockGuard<IOLock> locked(sGlobals.fLock);
    if (isReady())
        sRecorder->recordLocked(domain, type, arg);
}

// Setting domain to kGMETRICS_DOMAIN_NONE will halt further data collection
/* static */ gmetric_domain_t
GMetricsRecorder::setDomains(const gmetric_domain_t domains)
{
    const gmetric_domain_t ret = atomic_exchange(&sDomains, domains);
    DGM("(newd=%llx) oldd=%llx\n", domains, ret);
    return ret;
}

void GMetricsRecorder::free()
{
    DGM("\n");
    // Can't be in use as free will only be called if sRecorder is empty
    if (fBuffer)
        IODelete(fBuffer, gmetric_entry_t, fLineCount);
    super::free();
}

#pragma mark - GMetricRecorder

IOReturn GMetricsRecorder::initWithCount(const uint32_t lineCount)
{
    if (!super::init())
        return kIOReturnNotOpen;

    if (!lineCount || lineCount > kGMetricMaximumLineCount
    ||  !isPowerOf2(lineCount))
        return kIOReturnInternalError;  // prepareForRecording screwed up

    // Enforce power of 2-ness for '&' masked modulo
    fLineCount = lineCount;

    fBuffer = IONew(gmetric_entry_t, fLineCount);
    if (static_cast<bool>(fBuffer)) {
        fBufferSize = sizeof(gmetric_entry_t) * fLineCount;
        bzero(fBuffer, fBufferSize);
        return kIOReturnSuccess;
    }
    return kIOReturnNoMemory;
}


// Must be called with the global lock held
void GMetricsRecorder::recordLocked(
        const gmetric_domain_t domain, const gmetric_event_t type,
        const uint64_t arg1)
{
    // define a KERNEL and test environment dependant wrapper
    auto threadID = []{
        uint64_t tid;
#if KERNEL
        tid = thread_tid(current_thread());
#else
        pthread_threadid_np(NULL, &tid);
#endif
        return tid;
    };

    iog::locking_primitives::assertLocked(sGlobals.fLock);
    const auto i = fNextLine++;
    if (i < fLineCount) {
        gmetric_entry_t& metric = fBuffer[i];
        metric.header.type = type;
        metric.header.domain = domain;
        metric.header.cpu = cpu_number();
        metric.tid = threadID() & kThreadIDMask;
        metric.timestamp = mach_continuous_time();
        metric.data = arg1;
    }
}

IOReturn GMetricsRecorder::resetLocked()
{
    iog::locking_primitives::assertLocked(sGlobals.fLock);
    bzero(fBuffer, fBufferSize);
    fNextLine = 0;
    return kIOReturnSuccess;
}

IOReturn GMetricsRecorder::copyOutLocked(IOMemoryDescriptor* outDesc)
{
    // Check that we have a consistent view of metrics
    iog::locking_primitives::assertLocked(sGlobals.fLock);

    gmetric_buffer_t metricBuffer;
    const IOByteCount hdrOffset  = 0;
    const IOByteCount dataOffset = offsetof(gmetric_buffer_t, fEntries);
    const IOByteCount len        = outDesc->getLength();
    const IOByteCount dataLen    = len - dataOffset;

    if (len < dataOffset)
        return kIOReturnBadArgument;  // Out buffer is too small for header

    const IOByteCount filledLen = fNextLine * sizeof(*fBuffer);
    const IOByteCount bufferLen
        = (filledLen < fBufferSize) ? filledLen : fBufferSize;
    const IOByteCount copyLen   = (dataLen < bufferLen) ? dataLen : bufferLen;
    if (copyLen)
        (void) outDesc->writeBytes(dataOffset, fBuffer, copyLen);

    bzero(&metricBuffer, sizeof(metricBuffer));
    metricBuffer.fHeader.fEntriesCount = fNextLine;
    metricBuffer.fHeader.fCopiedCount
        = static_cast<uint32_t>(copyLen / sizeof(gmetric_entry_t));
    (void) outDesc->writeBytes(hdrOffset, &metricBuffer, sizeof(metricBuffer));

    return kIOReturnSuccess;
}

// APIs for IODisplayWranglerUserClient
/* static */ IOReturn GMetricsRecorder::enable()
{
    IOReturn err = kIOReturnNotReady;
    if (isReady()) {
        GMetricsRecorder::setDomains(kGMETRICS_DOMAIN_ENABLED);
        err = kIOReturnSuccess;
    }
    return err;
}

/* static */ IOReturn GMetricsRecorder::disable()
{
    IOReturn err = kIOReturnNotPermitted;
    if (isDomainActive(kGMETRICS_DOMAIN_ENABLED)) {
        const auto wasActive = setDomains(kGMETRICS_DOMAIN_NONE);
        if (static_cast<bool>(wasActive))
            doneWithRecording();
        err = kIOReturnSuccess;
    }
    return err;
}

/* static */ IOReturn GMetricsRecorder::start(const uint64_t inDomains)
{
    // Only look at interesting bits
    const auto domains = inDomains & kGMETRICS_DOMAIN_ALL;

    IOReturn err = kIOReturnNotPermitted;
    if (isDomainActive(kGMETRICS_DOMAIN_ENABLED)) {
        assert(isReady());
        setDomains(kGMETRICS_DOMAIN_ENABLED | domains);
        err = kIOReturnSuccess;
    }

    DGM("d=%llx) -> %x\n", inDomains, err);
    return err;
}

/* static */ IOReturn GMetricsRecorder::stop()
{
    IOReturn err = kIOReturnNotPermitted;
    if (isDomainActive(kGMETRICS_DOMAIN_ENABLED)) {
        setDomains(kGMETRICS_DOMAIN_ENABLED);
        err = kIOReturnSuccess;
    }
    DGM(" -> %x\n", err);
    return err;
}

/* static */ IOReturn GMetricsRecorder::reset()
{
    IOReturn err = kIOReturnNotPermitted;
    if (isDomainActive(kGMETRICS_DOMAIN_ENABLED)) {
        LockGuard<IOLock> locked(sGlobals.fLock);
        if (isReady())
            err = sRecorder->resetLocked();
    }
    DGM(" -> %x\n", err);
    return err;
}

/* static */ IOReturn GMetricsRecorder::fetch(IOMemoryDescriptor* outDesc)
{
    IOReturn err = kIOReturnNotPermitted;
    if ( GMetricsRecorder::isDomainActive(kGMETRICS_DOMAIN_ENABLED)) {
        LockGuard<IOLock> locked(sGlobals.fLock);
        if (isReady())
            err = sRecorder->copyOutLocked(outDesc);
    }
    DGM("(mdl=%llu) -> %x\n", outDesc->getLength(), err);
    return err;
}
