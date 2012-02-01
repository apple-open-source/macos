/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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
#include <Security/Security.h>
#include <Security/CodeSigning.h>
#include <Security/CSCommonPriv.h>
#include <Security/SecRequirementPriv.h>
#include <security_utilities/unix++.h>
#include <security_utilities/macho++.h>
#include <security_utilities/errors.h>
#include <security_utilities/hashing.h>
#include <security_utilities/cfutilities.h>


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
void fail(const char *format, ...) __attribute((noreturn));
void note(unsigned level, const char *format, ...);
void diagnose(const char *context = NULL, int stop = 0);
void diagnose(const char *context, CFErrorRef err);
void diagnose(const char *context, OSStatus rc, CFDictionaryRef info);

struct Fail : public std::exception {	// thrown by fail() if continueOnErrors
	Fail(int c) : code(c) { }
	const int code;
};


//
// Helper classes to handle CFError-returning API calls
//
class ErrorCheck {
public:
	ErrorCheck() : mError(NULL) { }
	
	operator CFErrorRef * () { return &mError; }
	void operator () (OSStatus rc)
		{ if (rc != noErr) throwError(); }
	void operator () (Boolean success)
		{ if (!success) throwError(); }
	
public:
	struct Error {
		Error(CFErrorRef e) : error(e) { }
		CFErrorRef error;
	};
	
protected:
	CFErrorRef mError;
	void throwError();
};

template <class T>
class CheckedRef : public CFRef<T>, public ErrorCheck {
public:
	void check(T value)
	{
		this->take(value);
		if (value == NULL)
			throwError();
	}
};


//
// Convert between hash code numbers and human-readable form
//
struct HashType {
	const char *name;
	uint32_t code;
	unsigned size;
};

const HashType *findHashType(const char *hashName);
const HashType *findHashType(uint32_t hashCode);


//
// Miscellaneous helpers and assistants
//
std::string keychainPath(CFTypeRef whatever);
SecIdentityRef findIdentity(SecKeychainRef keychain, const char *name);

CFTypeRef readRequirement(const std::string &source, SecCSFlags flags);
inline SecRequirementRef readRequirement(const std::string &source)
{ return SecRequirementRef(readRequirement(source, kSecCSParseRequirement)); }
inline CFDataRef readRequirements(const std::string &source)
{ return CFDataRef(readRequirement(source, kSecCSParseRequirementSet)); }

uint32_t parseOptionTable(const char *arg, const SecCodeDirectoryFlagTable *options);
uint32_t parseCdFlags(const char *string);
CFDateRef parseDate(const char *string);

std::string cleanPath(const char *path);

std::string hashString(const SHA1::Byte *hash);
std::string hashString(SHA1 &hash);
void stringHash(const char *string, SHA1::Digest digest);
CFDataRef certificateHash(SecCertificateRef cert);

void writeFileList(CFArrayRef list, const char *destination, const char *mode);
void writeDictionary(CFDictionaryRef dict, const char *destination, const char *mode);
void writeData(CFDataRef data, const char *destination, const char *mode);

SecCodeRef dynamicCodePath(const char *target);
SecStaticCodeRef staticCodePath(const char *target, const Architecture &arch, const char *version);


//
// Program arguments (shared)
//
extern unsigned verbose;				// verbosity level
extern bool force;						// force overwrite flag
extern bool continueOnError;			// continue processing targets on error(s)

extern int exitcode;					// cumulative exit code


#endif //_H_CSUTILS
