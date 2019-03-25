//
//  GMetric.hpp
//  IOGraphicsFamily
//
//  Created by Jérémy Tran on 8/16/17.
//

#ifndef GMetric_hpp
#define GMetric_hpp

#include <stdatomic.h>

#include <libkern/c++/OSObject.h>
#include <IOKit/IOReturn.h>

#include "GMetricTypes.h"

#pragma mark - GMetric Macro

#ifdef IOG_GMETRIC
#define GMETRIC(_data_, _type_, _domain_) do{ \
    const auto _gml_domain_ = static_cast<gmetric_domain_t>(_domain_); \
    const auto _gml_event_  = static_cast<gmetric_event_t>(_type_); \
    if (GMetricsRecorder::isDomainActive(_gml_domain_)) { \
        assert(GMETRIC_FUNC_FROM_DATA(_data_)); \
        GMetricsRecorder::record(_gml_domain_, _gml_event_, _data_); \
    } \
} while(0)
#define GMETRICFUNC(_func_, _type_, _domain_) \
    GMETRIC(GMETRIC_DATA_FROM_FUNC(_func_), _type_, _domain_)

#else

// 'Use' arguments to clean compile warnings in release builds
#define GMETRIC(_data_, _type_, _domain_) \
    do { (void) (_data_); (void) (_type_); (void) (_domain_); } while(0)
#define GMETRICFUNC(_func_, _type_, _domain_) \
    do { (void) (_func_); (void) (_type_); (void) (_domain_); } while(0)

#endif // IOG_GMETRIC

#pragma mark - GMetricsRecorder Class

class IODisplayWranglerUserClient;
class IOMemoryDescriptor;
class GMetricsRecorder : public OSObject
{
    OSDeclareDefaultStructors(GMetricsRecorder);

private:
    using super = OSObject;

    static _Atomic(gmetric_domain_t) sDomains;

    static GMetricsRecorder* sRecorder;  // Used for recording data

    static IOReturn prepareForRecording();
    static IOReturn prepareForRecording(const uint64_t userClientEntries);
    static void doneWithRecording();
    static gmetric_domain_t setDomains(const gmetric_domain_t domains);
    static gmetric_domain_t setDomains(const uint64_t domains)
        { return setDomains(static_cast<gmetric_domain_t>(domains)); }

    // APIs for IODisplayWranglerUserClients.cpp
    friend class IOGDiagnosticUserClient;

    static IOReturn enable();
    static IOReturn disable();
    static IOReturn start(const uint64_t domains);
    static IOReturn stop();
    static IOReturn reset();
    static IOReturn fetch(IOMemoryDescriptor* preparedWritableDescriptor);

public:
    // isDomainActive is on the fast path, make as lightweight as possible
    static bool isDomainActive(const gmetric_domain_t domain)
        { return static_cast<bool>(atomic_load(&sDomains) & domain); }
    static bool isReady() { return static_cast<bool>(sRecorder); }
    static void record(const gmetric_domain_t domain,
                       const gmetric_event_t type, const uint64_t arg);

protected:
    void free() override;

    IOReturn initWithCount(const uint32_t lineCount);
    void recordLocked(const gmetric_domain_t domain,
                      const gmetric_event_t type, const uint64_t arg);
    IOReturn copyOutLocked(IOMemoryDescriptor* outDesc);
    IOReturn resetLocked();

private:
    uint32_t         fLineCount = 0;
    uint32_t         fNextLine = 0;

    gmetric_entry_t* fBuffer = nullptr;
    uint32_t         fBufferSize = 0;
};

#endif /* GMetric_hpp */
