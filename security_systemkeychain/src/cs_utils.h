/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

//
// cs_utils - 
//
#ifndef _H_CSUTILS
#define _H_CSUTILS

#include <cstdio>
#include <string>
#include <getopt.h>
#include <Security/CodeSigning.h>
#include <security_utilities/unix++.h>
#include <security_utilities/errors.h>
#include <security_codesigning/requirement.h>


//
// Exit codes
//
enum {
	exitSuccess = 0,
	exitFailure = 1,
	exitUsage = 2,
	exitNoverify = 3
};


//
// Diagnostics and utility helpers
//
void fail(const char *format, ...);
void note(unsigned level, const char *format, ...);
void diagnose(const char *context = NULL, int stop = 0);
void diagnose(const char *context, CFErrorRef err);
void diagnose(const char *context, OSStatus rc, CFDictionaryRef info);

struct Fail : public std::exception {	// thrown by fail() if continueOnErrors
	Fail(int c) : code(c) { }
	const int code;
};


//
// A helper class to handle CFError-returning API calls
//
class ErrorCheck {
public:
	ErrorCheck() : mError(NULL) { }
	
	operator CFErrorRef * () { return &mError; }
	void operator () (OSStatus rc);
	
public:
	struct Error {
		Error(CFErrorRef e) : error(e) { }
		CFErrorRef error;
	};
	
private:
	CFErrorRef mError;
};


//
// Miscellaneous helpers and assistants
//
std::string keychainPath(CFTypeRef whatever);
SecIdentityRef findIdentity(SecKeychainRef keychain, const char *name);

template <class ReqType> const ReqType *readRequirement(const std::string &source);

uint32_t parseCdFlags(const char *string);
CFDateRef parseDate(const char *string);

std::string hashString(SHA1::Digest hash);
std::string hashString(SHA1 &hash);

void writeFileList(CFArrayRef list, const char *destination, const char *mode);
void writeData(CFDataRef data, const char *destination, const char *mode);

CFRef<SecCodeRef> codePath(const char *target);



//
// Program arguments (shared)
//
extern unsigned verbose;				// verbosity level
extern bool force;						// force overwrite flag
extern bool continueOnError;			// continue processing targets on error(s)

extern int exitcode;					// cumulative exit code


#endif //_H_CSUTILS
