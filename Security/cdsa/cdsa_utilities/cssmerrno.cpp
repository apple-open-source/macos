/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// cssmerrno - number-to-string translation for CSSM error codes
//
#include <Security/cssmerrno.h>
#include <Security/SecBase.h>
#include <Security/Authorization.h>
#include <Security/globalizer.h>
#include <stdio.h>
#include <map>


namespace Security {


//
// The compiled database of error codes
//
struct ErrorMap : public map<CSSM_RETURN, const char *> {
    ErrorMap();
};
static ModuleNexus<ErrorMap> errorMap;

struct Mapping {
    CSSM_RETURN error;
    const char *name;
};

#include "errorcodes.gen"	// include generated error tables


//
// Create the error map (the first time)
//
ErrorMap::ErrorMap()
{
    for (unsigned n = 0; n < sizeof(errorList) / sizeof(errorList[0]); n++)
        (*this)[errorList[n].error] = errorList[n].name;
}


//
// A perror-like form usable from C (and C++)
//
extern "C"
void cssmPerror(const char *how, CSSM_RETURN error)
{
    if (how)
        fprintf(stderr, "%s: %s\n", how, cssmErrorString(error).c_str());
    else
        fprintf(stderr, "%s\n", cssmErrorString(error).c_str());
}


//
// Produce a diagnostic string from a CSSM error number or exception.
// Small numbers are tried as convertible errors first.
// Unknown errors are output numerically with a note.
//
string cssmErrorString(CSSM_RETURN error)
{
    if (error == CSSM_OK) {
    	return "[ok]";
	} else if (error >= errSecErrnoBase && error <= errSecErrnoLimit) {
		return string("UNIX[") + strerror(error - errSecErrnoBase) + "]";
    } else if (error > 0 &&
               int(error) < int(sizeof(convErrorList) / sizeof(convErrorList[0])) &&
			   convErrorList[error]) {
        return string("COMMON[") + convErrorList[error] + "]";
    } else {
        ErrorMap::const_iterator it = errorMap().find(error);
        if (it == errorMap().end()) {
			char msg[80];
			snprintf(msg, sizeof(msg), "[UNKNOWN:0x%lx=%ld]", error, error);
            return msg;
        } else {
            return it->second;
		}
    }
}

string cssmErrorString(const CssmCommonError &error)
{ return cssmErrorString(error.cssmError()); }

} // end namespace Security
