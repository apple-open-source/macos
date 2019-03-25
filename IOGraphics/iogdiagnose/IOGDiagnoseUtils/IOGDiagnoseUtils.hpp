//
//  IOGDiagnoseUtils.h
//  IOGDiagnoseUtils
//
//  Created by Jérémy Tran on 8/8/17.
//

#ifndef IOGDiagnoseUtils_hpp
#define IOGDiagnoseUtils_hpp

#include <cstdlib>
#include <vector>

#include "iokit"

#include "IOGraphicsDiagnose.h"
#include "GTrace/GTraceTypes.hpp"

kern_return_t openDiagnostics(IOConnect* diagConnectP, char **errmsgP);

IOReturn fetchGTraceBuffers(const IOConnect& diag,
                            std::vector<GTraceBuffer>* traceBuffersP);
/*!
 @brief Populate a pre-allocated IOGDiagnose pointer with data coming from
        IOGraphics. Also populates a vector of GTraceEntry tables.

 @param reportP An allocated pointer to a IOGDiagnose struct.
 @param reportLength The memory length of the passed pointer. Should be greater
                     or equal to @c sizeof(IOGDiagnose).
 @param version Version of the IOGDiagnose report to request. Use
                @em IOGRAPHICS_DIAGNOSE_VERSION for the latest version.
 @param errmsgP Pointer to a string used by the function to return error
                messages. Must be freed by callee.
 */

// If errmsgP is set then the caller is required to free returned string
kern_return_t iogDiagnose(const IOConnect& diag,
                          IOGDiagnose* reportP, size_t reportLength,
                          std::vector<GTraceBuffer>* traceBuffersP,
                          char** errmsgP);

#endif // IOGDiagnoseUtils_hpp
