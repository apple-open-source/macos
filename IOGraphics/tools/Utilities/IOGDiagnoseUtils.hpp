//
//  IOGDiagnoseUtils.hpp
//  IOGDiagnoseUtils
//
//  Created by Jérémy Tran on 8/8/17.
//

#ifndef IOGDiagnoseUtils_hpp
#define IOGDiagnoseUtils_hpp

#include "iokit"

// Callers responsibility to free the errmsg if any.
kern_return_t openDiagnostics(IOConnect* diagConnectP, const char **errmsgP);
kern_return_t openGTrace(IOConnect* gtraceConnectP, const char **errmsgP);
#endif // IOGDiagnoseUtils_hpp
