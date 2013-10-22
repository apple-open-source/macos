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
// clientid - track and manage identity of securityd clients
//
#include "clientid.h"
#include "server.h"
#include <Security/SecCodePriv.h>


//
// Constructing a ClientIdentification doesn't do much.
// We're waiting for setup(), which should be called by the child class's
// constructor.
//
ClientIdentification::ClientIdentification()
{
}


//
// Initialize the ClientIdentification.
// This creates a process-level code object for the client.
//
void ClientIdentification::setup(pid_t pid)
{
	StLock<Mutex> _(mLock);
	if (OSStatus rc = SecCodeCreateWithPID(pid, kSecCSDefaultFlags,
			&mClientProcess.aref()))
		secdebug("clientid", "could not get code for process %d: OSStatus=%d",
			pid, int32_t(rc));
	mGuests.erase(mGuests.begin(), mGuests.end());
}


//
// Return a SecCodeRef for the client process itself, regardless of
// which guest within it is currently active.
// Think twice before using this.
//
SecCodeRef ClientIdentification::processCode() const
{
	return mClientProcess;
}


//
// Return a SecCodeRef for the currently active guest within the client
// process.
//
// We make a fair effort to cache client guest identities without over-growing
// the cache. Note that there's currently no protocol for being notified of
// a guest's death or disappearance (independent from the host process's death),
// so we couldn't track guests live even if we tried.
//
// Note that this consults Server::connection for the currently serviced
// Connection object, so this is not entirely a function of ClientIdentification state.
//
SecCodeRef ClientIdentification::currentGuest() const
{
	if (GuestState *guest = current())
		return guest->code;
	else
		return mClientProcess;
}

ClientIdentification::GuestState *ClientIdentification::current() const
{
	// if we have no client identification, we can't find a current guest either
	if (!processCode())
		return NULL;

	SecGuestRef guestRef = Server::connection().guestRef();
	
	// try to deliver an already-cached entry
	{
		StLock<Mutex> _(mLock);
		GuestMap::iterator it = mGuests.find(guestRef);
		if (it != mGuests.end())
			return &it->second;
	}

	// okay, make a new one (this may take a while)
	CFRef<CFDictionaryRef> attributes = (guestRef == kSecNoGuest)
		? NULL
		: makeCFDictionary(1, kSecGuestAttributeCanonical, CFTempNumber(guestRef).get());
	Server::active().longTermActivity();
	CFRef<SecCodeRef> code;
	switch (OSStatus rc = SecCodeCopyGuestWithAttributes(processCode(),
		attributes, kSecCSDefaultFlags, &code.aref())) {
	case noErr:
		break;
	case errSecCSUnsigned:			// not signed; clearly not a host
	case errSecCSNotAHost:			// signed but not marked as a (potential) host
		code = mClientProcess;
		break;
	case errSecCSNoSuchCode:		// potential host, but...
		if (guestRef == kSecNoGuest) {	//  ... no guests (yet), so return the process
			code = mClientProcess;
			break;
		}
		// else fall through		//  ... the guest we expected to be there isn't
	default:
		MacOSError::throwMe(rc);
	}
	StLock<Mutex> _(mLock);
	GuestState &slot = mGuests[guestRef];
	if (!slot.code)	// if another thread didn't get here first...
		slot.code = code;
	return &slot;
}


//
// Support for the legacy hash identification mechanism.
// The legacy machinery deals exclusively in terms of processes.
// It knows nothing about guests and their identities.
//
string ClientIdentification::getPath() const
{
	assert(mClientProcess);
	return codePath(currentGuest());
}

const CssmData ClientIdentification::getHash() const
{
	if (GuestState *guest = current()) {
		if (!guest->gotHash) {
			RefPointer<OSXCode> clientCode = new OSXCodeWrap(guest->code);
			OSXVerifier::makeLegacyHash(clientCode, guest->legacyHash);
			guest->gotHash = true;
		}
		return CssmData::wrap(guest->legacyHash, SHA1::digestLength);
	} else
		return CssmData();
}

const bool ClientIdentification::checkAppleSigned() const
{
	if (GuestState *guest = current()) {
		if (!guest->checkedSignature) {
            // This is the clownfish supported way to check for a Mac App Store or B&I signed build
            CFStringRef requirementString = CFSTR("(anchor apple) or (anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.9])");
            SecRequirementRef  secRequirementRef = NULL;
            OSStatus status = SecRequirementCreateWithString(requirementString, kSecCSDefaultFlags, &secRequirementRef);
            if (status == errSecSuccess) {
                OSStatus status = SecCodeCheckValidity(guest->code, kSecCSDefaultFlags, secRequirementRef);
                if (status != errSecSuccess) {
                    secdebug("SecurityAgentXPCQuery", "code requirement check failed (%d)", (int32_t)status);
                } else {
                    guest->appleSigned = true;
                }
                guest->checkedSignature = true;
            }
            CFRelease(secRequirementRef);
		}
		return guest->appleSigned;
	} else
		return false;
}


//
// Bonus function: get the path out of a SecCodeRef
//
std::string codePath(SecStaticCodeRef code)
{
	CFRef<CFURLRef> path;
	MacOSError::check(SecCodeCopyPath(code, kSecCSDefaultFlags, &path.aref()));
	return cfString(path);
}


//
// Debug dump support
//
#if defined(DEBUGDUMP)

static void dumpCode(SecCodeRef code)
{
	CFRef<CFURLRef> path;
	if (OSStatus rc = SecCodeCopyPath(code, kSecCSDefaultFlags, &path.aref()))
		Debug::dump("unknown(rc=%d)", int32_t(rc));
	else
		Debug::dump("%s", cfString(path).c_str());
}

void ClientIdentification::dump()
{
	Debug::dump(" client=");
	dumpCode(mClientProcess);
	for (GuestMap::const_iterator it = mGuests.begin(); it != mGuests.end(); ++it) {
		Debug::dump(" guest(0x%x)=", it->first);
		dumpCode(it->second.code);
		if (it->second.gotHash)
			Debug::dump(" [got hash]");
	}
}

#endif //DEBUGDUMP
