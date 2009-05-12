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
// csgeneric - generic Code representative
//
#include "csgeneric.h"
#include "cs.h"
#include "StaticCode.h"
#include <securityd_client/cshosting.h>
#include <sys/param.h>

namespace Security {
namespace CodeSigning {

using MachPlusPlus::Port;


//
// Common call-out code for cshosting RPC service
//
#define CALL(host, name, args...) \
	OSStatus result; \
	if (cshosting_client_ ## name (host, mig_get_reply_port(), &result, args)) \
		MacOSError::throwMe(errSecCSNotAHost); \
	MacOSError::check(result);


//
// Construct a running process representation
//
GenericCode::GenericCode(SecCode *host, SecGuestRef guestRef)
	: SecCode(host), mGuestRef(guestRef)
{
}


//
// Identify a guest by attribute set, and return a new GenericCode representing it.
// This uses cshosting RPCs to ask the host (or its proxy).
//
SecCode *GenericCode::locateGuest(CFDictionaryRef attributes)
{
	if (Port host = hostingPort()) {
		CFRef<CFDataRef> attrData;
		void *attrPtr = NULL; size_t attrLength = 0;
		if (attributes) {
			attrData.take(CFPropertyListCreateXMLData(NULL, attributes));
			attrPtr = (void *)CFDataGetBytePtr(attrData);
			attrLength = CFDataGetLength(attrData);
		}
		GuestChain guestPath;
		mach_msg_type_number_t guestPathLength;
		mach_port_t subport;
		CALL(host, findGuest, guestRef(), attrPtr, attrLength,
			&guestPath, &guestPathLength, &subport);
		secdebug("genericcode", "%p found guest chain length=%d",
			this, guestPathLength);
		SecPointer<SecCode> code = this;
		for (unsigned n = 0; n < guestPathLength; n++)
			code = new GenericCode(code, guestPath[n]);
		return code.yield();
	} else
		return NULL;		// not found, no error
}


//
// Map a guest to its StaticCode.
// This uses cshosting RPCs to ask the host (or its proxy).
//
SecStaticCode *GenericCode::mapGuestToStatic(SecCode *guest)
{
	if (Port host = hostingPort()) {
		char path[MAXPATHLEN];
		CALL(host, guestPath, safe_cast<GenericCode *>(guest)->guestRef(), path);
		return (new GenericStaticCode(DiskRep::bestGuess(path)))->retain();
	} else
		MacOSError::throwMe(errSecCSNotAHost);
}


//
// Get the Code Signing Status Word for a Code.
// This uses cshosting RPCs to ask the host (or its proxy).
//
uint32_t GenericCode::getGuestStatus(SecCode *guest)
{
	if (Port host = hostingPort()) {
		uint32_t status;
		CALL(host, guestStatus, safe_cast<GenericCode *>(guest)->guestRef(), &status);
		return status;
	} else
		MacOSError::throwMe(errSecCSNotAHost);
}


//
// Return the Hosting Port for this Code.
// May return MACH_PORT_NULL if the code is not a code host.
// Throws if we can't get the hosting port for some reason (and can't positively
// determine that there is none).
//
// Note that we do NOT cache negative outcomes. Being a host is a dynamic property,
// and this Code may not have commenced hosting operations yet. For non- (or not-yet-)hosts
// we simply return NULL.
//
Port GenericCode::hostingPort()
{
	if (!mHostingPort) {
		if (staticCode()->codeDirectory()->flags & kSecCodeSignatureHost)
			mHostingPort = getHostingPort();
	}
	return mHostingPort;	
}


//
// A pure GenericHost has no idea where to get a hosting port from.
// This method must be overridden to get one.
// However, we do handle a contiguous chain of GenericCodes by deferring
// to our next-higher host for it.
//
mach_port_t GenericCode::getHostingPort()
{
	if (GenericCode *genericHost = dynamic_cast<GenericCode *>(host()))
		return genericHost->getHostingPort();
	else
		MacOSError::throwMe(errSecCSNotAHost);
}


} // CodeSigning
} // Security
