//
//  IOGDiagnoseUtils.cpp
//  IOGDiagnoseUtils
//
//  Created by Jérémy Tran on 8/8/17.
//

#include "IOGDiagnoseUtils.hpp"

#include <cstdint>
#include <cstdio>
#include <memory>

#include <mach/mach_error.h>

// Pick up local headers
#include "IOGraphicsTypes.h"

#define COUNT_OF(x) \
    ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))
#define kResourcePath     kIOServicePlane ":/" kIOResourcesClass "/"

namespace {
kern_return_t openWranglerUC(
        IOConnect* diagConnectP, uint32_t type, const char **errmsgP)
{
    static const char * const sWranglerPath = kResourcePath "IODisplayWrangler";
    char errbuf[128] = "";
    kern_return_t err = kIOReturnInternalError;

    do {
        err = kIOReturnNotFound;
        IOObject wrangler(
                IORegistryEntryFromPath(kIOMasterPortDefault, sWranglerPath));
        if (!static_cast<bool>(wrangler)) {
            snprintf(errbuf, sizeof(errbuf),
                     "IODisplayWrangler '%s' search failed", sWranglerPath);
            continue;
        }

        IOConnect wranglerConnect(wrangler, type);
        err = wranglerConnect.err();
        if (err) {
            const char * typeStr
                = (type == kIOGDiagnoseConnectType) ? "Diagnose"
                : (type == kIOGDiagnoseGTraceType)  ? "Gtrace"
                : "Unknown";
            snprintf(errbuf, sizeof(errbuf),
                     "IOServiceOpen(%s) on IODisplayWrangler failed", typeStr);
            continue;
        }
        *diagConnectP = std::move(wranglerConnect);
    } while(false);

    if (err && errmsgP) {
        char *tmpMsg;
        asprintf(&tmpMsg, "%s - %s(%x)", errbuf, mach_error_string(err), err);
        *errmsgP = tmpMsg;
    }
    return err;
}
} // namespace

kern_return_t openDiagnostics(IOConnect* diagConnectP, const char **errmsgP)
{
    return openWranglerUC(diagConnectP, kIOGDiagnoseConnectType, errmsgP);
}

kern_return_t openGTrace(IOConnect* gtraceConnectP, const char **errmsgP)
{
    return openWranglerUC(gtraceConnectP, kIOGDiagnoseGTraceType, errmsgP);
}
