//
//  IOGDiagnoseUtils.cpp
//  IOGDiagnoseUtils
//
//  Created by Jérémy Tran on 8/8/17.
//

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <mach/mach_error.h>

#include "iokit"

// Pick up local headers
#include "IOGraphicsTypes.h"
#include "IOGDiagnoseUtils.hpp"

using std::vector;
using std::unique_ptr;

#define COUNT_OF(x) \
    ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

kern_return_t openDiagnostics(IOConnect* diagConnectP, char **errmsgP)
{
    static const char * const
        sWranglerPath = kIOServicePlane ":/IOResources/IODisplayWrangler";
    const char*   errmsg;
    kern_return_t err = kIOReturnInternalError;

    do {
        errmsg = "IODisplayWrangler search failed";
        err = kIOReturnNotFound;
        IOObject wrangler(
                IORegistryEntryFromPath(kIOMasterPortDefault, sWranglerPath));
        if (!static_cast<bool>(wrangler))
            continue;

        errmsg = "IOServiceOpen on IODisplayWrangler failed";
        IOConnect wranglerConnect(wrangler, kIOGDiagnoseConnectType);
        err = wranglerConnect.err();
        if (err) continue;
        *diagConnectP = std::move(wranglerConnect);
    } while(false);

    if (err && errmsgP)
        asprintf(errmsgP, "%s - %s(%x)", errmsg, mach_error_string(err), err);
    return err;

}

IOReturn fetchGTraceBuffers(const IOConnect& diag,
                            vector<GTraceBuffer>* traceBuffersP)
{
    uint64_t scalarParams[] = { GTRACE_REVISION, kGTraceCmdFetch, 0 };
    uint32_t scalarParamsCount = COUNT_OF(scalarParams);
    size_t   bufSize;

    // TODO(gvdl): Need to multithread the fetches the problem is that the
    // fetch code makes a subroutine call into the driver.
    // Single thread for the time being.

    // Default vector fo GTraceEntries allocates a MiB of entries
    const size_t kBufSizeBytes = 1024 * 1024;
    const vector<GTraceEntry>::size_type
        kBufSize = kBufSizeBytes / sizeof(GTraceEntry);
    GTraceBuffer::vector_type buf(kBufSize);

    IOReturn err = kIOReturnSuccess;
    for (int i = 0; i < kGTraceMaximumBufferCount; i++) {
        scalarParams[2] = i;  // GTrace buffer index
        bufSize = kBufSizeBytes;
        const IOReturn bufErr = diag.callMethod(
                kIOGDUCInterface_gtrace,
                scalarParams, scalarParamsCount, NULL, 0,  // Inputs
                NULL, NULL, buf.data(), &bufSize);         // Outputs
        if (!bufErr) {
            // Decode the gtrace buffer, massage to get access to header
            const auto* header = reinterpret_cast<GTraceHeader*>(buf.data());
            bufSize = header->fBufferSize / sizeof(GTraceEntry);
            auto cbegin = buf.cbegin();
            traceBuffersP->emplace_back(cbegin, cbegin + bufSize);
        }
        else if (bufErr != kIOReturnNotFound) {
            fprintf(stderr, "Error retrieving gtrace buffer %d - %s[%x]\n",
                    i, mach_error_string(bufErr), bufErr);
            if (!err)
                err = bufErr;  // Record first error
        }
    }
    return err;
}

// If errmsgP is set then the caller is required to free returned string
kern_return_t iogDiagnose(const IOConnect& diag,
                          IOGDiagnose* reportP, size_t reportLength,
                          vector<GTraceBuffer>* traceBuffersP,
                          char** errmsgP)
{
    const char*   errmsg;
    kern_return_t err = kIOReturnInternalError;

    do {
        errmsg = "NULL IOGDiagnose pointer argument passed";
        err = kIOReturnBadArgument;
        if (!reportP)
            continue;
        errmsg = "report too small for an IOGDiagnose header";
        err = kIOReturnBadArgument;
        if (sizeof(*reportP) > reportLength)
            continue;

        // Grab the main IOFB reports first
        errmsg = "Problem getting framebuffer diagnostic data";
        uint64_t       scalarParams[]    = {reportLength};
        const uint32_t scalarParamsCount = COUNT_OF(scalarParams);
        memset(reportP, 0, reportLength);
        err = diag.callMethod(
                kIOGDUCInterface_diagnose,
                scalarParams, scalarParamsCount, NULL, 0,  // input
                NULL, NULL, reportP, &reportLength);       // output
        if (!err) {
            errmsg = "A problem occured fetching gTrace buffers"
                      ", see kernel logs";
            err = fetchGTraceBuffers(diag, traceBuffersP);
        }
    } while(false);

    if (err && errmsgP)
        asprintf(errmsgP, "%s - %s(%x)", errmsg, mach_error_string(err), err);
    return err;
}
