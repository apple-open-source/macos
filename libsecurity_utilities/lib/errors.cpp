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
#include <security_utilities/errors.h>
#include <security_utilities/debugging.h>
#include <typeinfo>
#include <stdio.h>


//@@@
// From cssmapple.h - layering break
// Where should this go?
//@@@
#define errSecErrnoBase 100000
#define errSecErrnoLimit 100255

//
// The base of the exception hierarchy.
// Note that the debug output here depends on a particular
// implementation feature of gcc; to wit, that the exception object
// is created and then copied (at least once) via its copy constructor.
// If your compiler does not invoke the copy constructor, you won't get
// debug output, but nothing worse should happen.
//
CommonError::CommonError()
	IFDEBUG(: mCarrier(true))
{
}

CommonError::CommonError(const CommonError &source)
{
#if !defined(NDEBUG)
	if (source.mCarrier)
		source.debugDiagnose(this);
	mCarrier = source.mCarrier;
	source.mCarrier = false;
#endif //NDEBUG
}

CommonError::~CommonError() throw ()
{
#if !defined(NDEBUG)
	if (mCarrier)
		secdebug("exception", "%p handled", this);
#endif //NDEBUG
}

// default debugDiagnose gets what it can (virtually)
void CommonError::debugDiagnose(const void *id) const
{
#if !defined(NDEBUG)
	secdebug("exception", "%p %s 0x%lx osstatus %ld",
		id, Debug::typeName(*this).c_str(),
		osStatus(), osStatus());
#endif //NDEBUG
}


//
// UnixError exceptions
//
UnixError::UnixError() : error(errno)
{
	IFDEBUG(debugDiagnose(this));
}

UnixError::UnixError(int err) : error(err)
{
	IFDEBUG(debugDiagnose(this));
}

const char *UnixError::what() const throw ()
{ return "UNIX error exception"; }


OSStatus UnixError::osStatus() const
{
	return error + errSecErrnoBase;
}

int UnixError::unixError() const
{ return error; }

void UnixError::throwMe(int err) { throw UnixError(err); }

// @@@ This is a hack for the Network protocol state machine
UnixError UnixError::make(int err) { return UnixError(err); }

#if !defined(NDEBUG)
void UnixError::debugDiagnose(const void *id) const
{
    secdebug("exception", "%p UnixError %s (%d) osStatus %ld",
		id, strerror(error), error, osStatus());
}
#endif //NDEBUG


//
// MacOSError exceptions
//
MacOSError::MacOSError(int err) : error(err)
{
	IFDEBUG(debugDiagnose(this));
}

const char *MacOSError::what() const throw ()
{ return "MacOS error"; }

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


//
// CFError exceptions
//
CFError::CFError()
{
	IFDEBUG(debugDiagnose(this));
}

const char *CFError::what() const throw ()
{ return "CoreFoundation error"; }

// can't get this from CarbonCore/MacErrors, but it's too good to pass up
enum {
	coreFoundationUnknownErr      = -4960
};

OSStatus CFError::osStatus() const
{ return coreFoundationUnknownErr; }

int CFError::unixError() const
{
	return EFAULT;		// nothing really matches
}

void CFError::throwMe()
{ throw CFError(); }
