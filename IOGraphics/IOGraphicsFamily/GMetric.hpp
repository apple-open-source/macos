//
//  GMetric.hpp
//  IOGraphicsFamily
//
//  Created by Jérémy Tran on 8/16/17.
//

#ifndef GMetric_hpp
#define GMetric_hpp

#if defined(_KERNEL_) || defined(KERNEL)

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOLocks.h>
#include <libkern/c++/OSObject.h>

#else /* defined(_KERNEL_) || defined(KERNEL) */

#include <mutex>
#include <stdlib.h>
#include <IOKit/IOReturn.h>

#endif /* defined(_KERNEL_) || defined(KERNEL) */

#include "GMetricTypes.h"


#pragma mark - GMetric Macros

#if defined(_KERNEL_) || defined(KERNEL)

#define GMETRIC_USING_SUPER             using super = OSObject
#define GMETRIC_PUBLIC                  : public OSObject
#define GMETRIC_SERVICE                 IOService
#define GMETRIC_MALLOC(_s_)             IOMalloc(_s_)
#define GMETRIC_SAFE_FREE(_p_,_s_)      do{if(_p_){IOFree(_p_,_s_);_p_=NULL;}}while(0)
#define GMETRIC_SLEEP(_d_)              do{IOSleep(_d_*1000);}while(0)
#define GMETRIC_RAII_LOCK(inLck)        GMetricLock lock(inLck)
#define GMETRIC_RETAIN                  retain()
#define GMETRIC_RELEASE                 release()
#define GMETRIC_PRINTF                  kprintf

#else /* defined(_KERNEL_) || defined(KERNEL) */

#define GMETRIC_USING_SUPER
#define GMETRIC_PUBLIC
#define GMETRIC_SERVICE                 uintptr_t
#define GMETRIC_MALLOC(_s_)             malloc(_s_)
#define GMETRIC_SAFE_FREE(_p_,_s_)      do{if(_p_){free((void *)_p_);_p_=NULL;}}while(0)
#define OSSafeReleaseNULL(_p_)          do{if(_p_){_p_=NULL;}}while(0)
#define GMETRIC_SLEEP(_d_)              do{sleep(_d_);}while(0)
#define GMETRIC_RAII_LOCK(inLck)        std::lock_guard<std::mutex> lock(inLck)
#define GMETRIC_RETAIN                  GMETRIC_RAII_LOCK(fInUseLock)
#define GMETRIC_RELEASE
#define GMETRIC_PRINTF                  printf

#endif /* defined(_KERNEL_) || defined(KERNEL) */

#define GMETRICOBJ                      gGMetrics
#define GMETRICRECORDMETRIC             GMETRICOBJ->recordMetric

#ifdef IOG_GMETRIC
#define GMETRIC(domain, type, arg) \
    do{\
        if(GMETRICOBJ && GMETRICOBJ->recordingEnabled()){\
            GMETRICRECORDMETRIC(domain, type, arg);\
        }\
    }while(0)
#else
#define GMETRIC(domain, type, arg)
#endif

#define kTHREAD_ID_MASK         0x0000000000FFFFFFULL // 24 bits


#pragma mark - GMetricsRecorder Class

class GMetricsRecorder GMETRIC_PUBLIC
{
#if defined(_KERNEL_) || defined(KERNEL)
    OSDeclareDefaultStructors(GMetricsRecorder);
#endif /* defined(_KERNEL_) || defined(KERNEL) */

private:
    GMETRIC_USING_SUPER;

public:
#if defined(_KERNEL_) || defined(KERNEL)
    virtual bool init() APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
#endif /* defined(_KERNEL_) || defined(KERNEL) */

    IOReturn initWithCount(const uint32_t lineCount);

    IOReturn recordMetric(const gmetric_domain_t domain,
                          const gmetric_event_t type,
                          const uint64_t arg1);
    IOReturn copyMetrics(void * outBufferP,
                         const uint32_t bufferSize,
                         uint32_t * outEntriesCountP);
    IOReturn resetMetrics();

    inline uint32_t sizeToLines(const uint32_t bufSize) {
        uint32_t    lines = 0;
        if (bufSize > (kGMetricMaximumLineCount * sizeof(gmetric_entry_t))) {
            lines = kGMetricMaximumLineCount;
        } else if (bufSize >= sizeof(gmetric_entry_t)) {
            lines = (1 << (32 - (__builtin_clz(bufSize) + 1))) / sizeof(gmetric_entry_t);
        }
        return lines;
    }

    inline uint32_t linesToSize(const uint32_t lines) {
        uint32_t    bufSize = 0;
        if (lines > kGMetricMaximumLineCount) {
            bufSize = kGMetricMaximumLineCount * sizeof(gmetric_entry_t);
        } else {
            bufSize = lines * sizeof(gmetric_entry_t);
            bufSize = (sizeToLines(bufSize) * sizeof(gmetric_entry_t));
        }
        return bufSize;
    }

    inline bool isInitialized() const { return fInitialized; }

    inline bool recordingEnabled() const { return fRecordingEnabled; }
    inline void setRecordingEnabled(const bool enabled) { fRecordingEnabled = enabled; }

    inline gmetric_domain_t domains() const { return fDomains; }
    inline void setDomains(const gmetric_domain_t domains) { fDomains = domains; }

    inline uint32_t bufferSize() const { return fBufferSize; }

#if defined(_KERNEL_) || defined(KERNEL)
    struct GMetricLock
    {
        GMetricLock(IOLock *inLock) : fGMetricLock(inLock) { IOLockLock(fGMetricLock); }
        ~GMetricLock() { IOLockUnlock(fGMetricLock); }
    private:
        IOLock * const fGMetricLock;
    };
#endif /* defined(_KERNEL_) || defined(KERNEL) */

private:
#if defined(_KERNEL_) || defined(KERNEL)
    IOLock *            fInUseLock;
#else
    std::mutex          fInUseLock;
#endif /* defined(_KERNEL_) || defined(KERNEL) */
    _Atomic(bool)       fInitialized;
    _Atomic(bool)       fRecordingEnabled;

    uint32_t            fLineCount;
    uint32_t            fLineMask;
    _Atomic(uint32_t)   fNextLine;

    gmetric_domain_t    fDomains;

    gmetric_entry_t *   fBuffer;
    uint32_t            fBufferSize;
};

#endif /* GMetric_hpp */
