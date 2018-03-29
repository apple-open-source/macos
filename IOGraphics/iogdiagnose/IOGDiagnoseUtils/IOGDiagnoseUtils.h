//
//  IOGDiagnoseUtils.h
//  IOGDiagnoseUtils
//
//  Created by Jérémy Tran on 8/8/17.
//

#ifndef IOGDiagnoseLib_h
#define IOGDiagnoseLib_h

#include <IOKit/IOReturn.h>
#include <sys/cdefs.h>
#include <stdlib.h>

__BEGIN_DECLS

#include "IOGraphicsDiagnose.h"

/*!
 @brief Populate a pre-allocated IOGDiagnose pointer with data coming from IOGraphics.

 @param reportP An allocated pointer to a IOGDiagnose struct.
 @param reportLength The memory length of the passed pointer. Should be greater
                     or equal to @c sizeof(IOGDiagnose).
 @param version Version of the IOGDiagnose report to request. Use
                @em IOGRAPHICS_DIAGNOSE_VERSION for the latest version.
 @param errmsgP Pointer to a string used by the function to return error messages.
 */
kern_return_t iogDiagnose(IOGDiagnose * reportP,
                          size_t        reportLength,
                          uint64_t      version,
                          const char ** errmsgP);

__END_DECLS

#endif /* IOGDiagnoseLib_h */
