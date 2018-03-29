//
//  GMetric.cpp
//  IOGraphicsFamily
//
//  Created by Jérémy Tran on 8/16/17.
//

#include "GMetric.hpp"
#include <mach/mach_time.h>

#if defined(_KERNEL_) || defined(KERNEL)

extern "C" {
#include <machine/cpu_number.h>
}

#pragma mark - OSObject

OSDefineMetaClassAndStructors(GMetricsRecorder, OSObject);

bool GMetricsRecorder::init()
{
    bool bRet = super::init();

    if (bRet)
    {
        fInUseLock = IOLockAlloc();
        if (!fInUseLock)
        {
            bRet = false;
        }
        else
        {
            GMETRIC_RAII_LOCK(fInUseLock);

            fInitialized = false;
            fRecordingEnabled = false;
            fLineCount = 0;
            fLineMask = 0;
            fNextLine = 0;
            fDomains = kGMETRICS_DOMAIN_NONE;
            fBuffer = NULL;
            fBufferSize = 0;
        }
    }

    return bRet;
}


void GMetricsRecorder::free()
{
    if (fInitialized)
    {
        {
            GMETRIC_RAII_LOCK(fInUseLock);

            fInitialized = false;

            if (fBuffer)
            {
                GMETRIC_SAFE_FREE(fBuffer, fBufferSize);
                fBufferSize = 0;
            }
        }
        if (fInUseLock)
        {
            IOLockFree(fInUseLock);
        }
    }

    super::free();
}

#endif /* defined(_KERNEL_) || defined(KERNEL) */


#pragma mark - GMetricRecorder

IOReturn GMetricsRecorder::initWithCount(const uint32_t lineCount)
{
    if (fInitialized)
    {
        return kIOReturnNotPermitted;
    }
#if defined(_KERNEL_) || defined(KERNEL)
    if (!init())
    {
        return kIOReturnNotOpen;
    }
#endif /* defined(_KERNEL_) || defined(KERNEL) */

    GMETRIC_RAII_LOCK(fInUseLock);

    if (!lineCount)
    {
        return kIOReturnBadArgument;
    }
    uint32_t bufSize = linesToSize(lineCount);
    if (!bufSize)
    {
        return kIOReturnBadArgument;
    }

    fBufferSize = bufSize;
    fBuffer = reinterpret_cast<gmetric_entry_t *>(GMETRIC_MALLOC(fBufferSize));
    if (!fBuffer)
    {
        return kIOReturnNoMemory;
    }
    bzero(fBuffer, fBufferSize);

    fRecordingEnabled = false;
    fLineCount = sizeToLines(fBufferSize);
    fLineMask = fLineCount - 1;
    fNextLine = 0;
    fDomains = kGMETRICS_DOMAIN_NONE;

    fInitialized = true;

    return kIOReturnSuccess;
}


IOReturn GMetricsRecorder::recordMetric(const gmetric_domain_t domain,
                                        const gmetric_event_t type,
                                        const uint64_t arg1)
{
    if (!fInitialized) { return kIOReturnNotReady; }
    if (!fRecordingEnabled) { return kIOReturnOffline; }
    // Notice that the buffer is not circular, so when the buffer is full, no
    // more metrics are recorded
    if (fNextLine >= fLineCount) { return kIOReturnNoMemory; }

    // Filtering out a metric we're not interested in is not considered an error
    if (domain & fDomains)
    {
#if defined(_KERNEL_) || defined(KERNEL)
        GMETRIC_RETAIN;
#endif /* defined(_KERNEL_) || defined(KERNEL) */
        GMETRIC_RAII_LOCK(fInUseLock);

        gmetric_entry_t& metric = fBuffer[fNextLine++];
        metric.header.type = type;
        metric.header.domain = domain;
#if defined(_KERNEL_) || defined(KERNEL)
        metric.header.cpu = cpu_number();
        metric.tid = thread_tid(current_thread());
#else
        metric.header.cpu = 0;
        metric.tid = ((uintptr_t)pthread_self()) & kTHREAD_ID_MASK;
#endif /* defined(_KERNEL_) || defined(KERNEL) */
        metric.timestamp = mach_continuous_time();
        metric.data = arg1;

#if defined(_KERNEL_) || defined(KERNEL)
        GMETRIC_RELEASE;
#endif /* defined(_KERNEL_) || defined(KERNEL) */
    }
    return kIOReturnSuccess;
}


IOReturn GMetricsRecorder::copyMetrics(void * outBufferP,
                                       const uint32_t bufferSize,
                                       uint32_t * outEntriesCountP)
{
    if ((NULL == outEntriesCountP) || (NULL == outBufferP))
    {
        return kIOReturnBadArgument;
    }

    // Make sure bytesCount is a multiple of sizeof(gmetric_entry_t)
    uint32_t bytesCount = linesToSize(sizeToLines(bufferSize));
    if (bytesCount > fBufferSize)
    {
        bytesCount = fBufferSize;
    }
    {
        GMETRIC_RAII_LOCK(fInUseLock);
        memcpy(outBufferP, fBuffer, bytesCount);
    }
    *outEntriesCountP = fNextLine;

    return kIOReturnSuccess;
}


IOReturn GMetricsRecorder::resetMetrics()
{
    if (!fInitialized)
    {
        return kIOReturnNotReady;
    }
    {
        GMETRIC_RAII_LOCK(fInUseLock);
        bzero(fBuffer, fBufferSize);
    }
    fNextLine = 0;
    return kIOReturnSuccess;
}
