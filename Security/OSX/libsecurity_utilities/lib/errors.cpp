/*
 * Copyright (c) 2000-2004,2011-2014 Apple Inc. All Rights Reserved.
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
// Error hierarchy
//
#include <security_utilities/errors.h>
#include <security_utilities/debugging.h>
#include <typeinfo>
#include <stdio.h>
#include <Security/SecBase.h>
#include <execinfo.h>
#include <cxxabi.h>

//@@@
// From cssmapple.h - layering break
// Where should this go?
//@@@
#define errSecErrnoBase 100000
#define errSecErrnoLimit 100255

//
// The base of the exception hierarchy.
//
CommonError::CommonError() : whatBuffer("CommonError")
{
}


//
// We strongly encourage catching all exceptions by const reference, so the copy
// constructor of our exceptions should never be called.
//
CommonError::CommonError(const CommonError &source)
{
    strlcpy(whatBuffer, source.whatBuffer, whatBufferSize);
}

CommonError::~CommonError() throw ()
{
}

void CommonError::LogBacktrace() {
    // Only do this work if we're actually going to log things
    if(secinfoenabled("security_exception")) {
        const size_t maxsize = 32;
        void* callstack[maxsize];

        int size = backtrace(callstack, maxsize);
        char** names = backtrace_symbols(callstack, size);

        // C++ symbolicate the callstack

        const char* delim = " ";
        string build;
        char * token = NULL;
        char * line = NULL;

        for(int i = 0; i < size; i++) {
            build = "";

            line = names[i];

            while((token = strsep(&line, delim))) {
                if(*token == '\0') {
                    build += " ";
                } else {
                    int status = 0;
                    char * demangled = abi::__cxa_demangle(token, NULL, NULL, &status);
                    if(status == 0) {
                        build += demangled;
                    } else {
                        build += token;
                    }
                    build += " ";

                    if(demangled) {
                        free(demangled);
                    }
                }
            }

            secinfo("security_exception", "%s", build.c_str());
        }
        free(names);
    }
}



//
// UnixError exceptions
//
UnixError::UnixError() : error(errno)
{
    SECURITY_EXCEPTION_THROW_UNIX(this, errno);

    snprintf(whatBuffer, whatBufferSize, "UNIX errno exception: %d", this->error);
    secnotice("security_exception", "%s", what());
    LogBacktrace();
}

UnixError::UnixError(int err) : error(err)
{
    SECURITY_EXCEPTION_THROW_UNIX(this, err);

    snprintf(whatBuffer, whatBufferSize, "UNIX error exception: %d", this->error);
    secnotice("security_exception", "%s", what());
    LogBacktrace();
}

const char *UnixError::what() const throw ()
{
    return whatBuffer;
}


OSStatus UnixError::osStatus() const
{
	return error + errSecErrnoBase;
}

int UnixError::unixError() const
{ return error; }

void UnixError::throwMe(int err) { throw UnixError(err); }

// @@@ This is a hack for the Network protocol state machine
UnixError UnixError::make(int err) { return UnixError(err); }


//
// MacOSError exceptions
//
MacOSError::MacOSError(int err) : error(err)
{
    SECURITY_EXCEPTION_THROW_OSSTATUS(this, err);

    snprintf(whatBuffer, whatBufferSize, "MacOS error: %d", this->error);
    secnotice("security_exception", "%s", what());
    LogBacktrace();
}

const char *MacOSError::what() const throw ()
{
    return whatBuffer;
}

OSStatus MacOSError::osStatus() const
{ return error; }

int MacOSError::unixError() const
{
	// embedded UNIX errno values are returned verbatim
	if (error >= errSecErrnoBase && error <= errSecErrnoLimit)
		return error - errSecErrnoBase;

	switch (error) {
	default:
		// cannot map this to errno space
		return -1;
    }
}

void MacOSError::throwMe(int error)
{ throw MacOSError(error); }

MacOSError MacOSError::make(int error)
{ return MacOSError(error); }


//
// CFError exceptions
//
CFError::CFError()
{
    SECURITY_EXCEPTION_THROW_CF(this);
    secnotice("security_exception", "CFError");
    LogBacktrace();
}

const char *CFError::what() const throw ()
{ return "CoreFoundation error"; }

OSStatus CFError::osStatus() const
{ return errSecCoreFoundationUnknown; }

int CFError::unixError() const
{
	return EFAULT;		// nothing really matches
}

void CFError::throwMe()
{ throw CFError(); }




void ModuleNexusError::throwMe()
{
    throw ModuleNexusError();
}



OSStatus ModuleNexusError::osStatus() const
{
    return errSecParam;
}



int ModuleNexusError::unixError() const
{
    return EINVAL;      
}

