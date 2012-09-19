/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _H_UTILITIES_ERROR
#define _H_UTILITIES_ERROR

#include <AvailabilityMacros.h>
#include <exception>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
#include <errno.h>

#undef check


namespace Security {


//
// Common base of Security exceptions that represent error conditions.
// All can yield Unix or OSStatus error codes as needed, though *how*
// is up to the subclass implementation.
// CSSM_RETURN conversions are done externally in (???).
//
class CommonError : public std::exception {
protected:
    CommonError();
    CommonError(const CommonError &source);
public:
    virtual ~CommonError() throw ();

    virtual OSStatus osStatus() const = 0;
	virtual int unixError() const = 0;
};


//
// Genuine Unix-originated errors identified by an errno value.
// This includes secondary sources such as pthreads.
//
class UnixError : public CommonError {
protected:
    UnixError();
    UnixError(int err);
public:
    const int error;
    virtual OSStatus osStatus() const;
	virtual int unixError() const;
    virtual const char *what () const throw ();
    
    static void check(int result)		{ if (result == -1) throwMe(); }
    static void throwMe(int err = errno) __attribute__((noreturn));

    // @@@ This is a hack for the Network protocol state machine
    static UnixError make(int err = errno) DEPRECATED_ATTRIBUTE;
};


//
// Genuine MacOS (X) errors identified by an OSStatus value.
// Don't even think of working with OSErr values; use OSStatus.
//
class MacOSError : public CommonError {
protected:
    MacOSError(int err);
public:
    const int error;
    virtual OSStatus osStatus() const;
	virtual int unixError() const;
    virtual const char *what () const throw ();
    
    static void check(OSStatus status)	{ if (status != noErr) throwMe(status); }
    static void throwMe(int err) __attribute__((noreturn));
};


//
// CoreFoundation errors.
// Since CF prefers not to tell us *why* something didn't work, this
// is not very useful - but it's better than faking it into one of the other
// error spaces.
//
class CFError : public CommonError {
protected:
	CFError();
public:
	virtual OSStatus osStatus() const;
	virtual int unixError() const;
	virtual const char *what () const throw ();
	
	template <class T>
	static void check(const T &p)		{ if (!p) throwMe(); }

	static void throwMe() __attribute__((noreturn));
};


} // end namespace Security


#endif //_H_UTILITIES_ERROR
