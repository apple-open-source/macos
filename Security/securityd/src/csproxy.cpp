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
// csproxy - Code Signing Hosting Proxy
//
#include "csproxy.h"
#include "server.h"
#include <Security/SecStaticCode.h>
#include <securityd_client/cshosting.h>
#include <security_utilities/cfmunge.h>
#include <security_utilities/casts.h>


//
// Construct a CodeSigningHost
//
CodeSigningHost::CodeSigningHost()
	: mLock(Mutex::recursive), mHostingState(noHosting)
{
}


//
// Cleanup code.
//
CodeSigningHost::~CodeSigningHost()
{
	reset();
}


//
// Reset Code Signing Hosting state.
// This turns hosting off and clears all children.
//
void CodeSigningHost::reset()
{
	StLock<Mutex> _(mLock);
	switch (mHostingState) {
	case noHosting:
		break;	// nothing to do
	case dynamicHosting:
		mHostingPort.destroy();
		mHostingPort = MACH_PORT_NULL;
        secnotice("SS", "%p host unregister", this);
		break;
	case proxyHosting:
		Server::active().remove(*this);	// unhook service handler
		mHostingPort.destroy();	// destroy receive right
		mHostingState = noHosting;
		mHostingPort = MACH_PORT_NULL;
		mGuests.erase(mGuests.begin(), mGuests.end());
        secnotice("SS", "%p host unregister", this);
		break;
	}
}


//
// Given a host reference (possibly NULL for the process itself), locate
// its most dedicated guest. This descends a contiguous chain of dedicated
// guests until it find a host that either has no guests, or whose guests
// are not dedicated.
//
CodeSigningHost::Guest *CodeSigningHost::findHost(SecGuestRef hostRef)
{
	Guest *host = findGuest(hostRef, true);
	for (;;) {
		if (Guest *guest = findGuest(host))
			if (guest->dedicated) {
				host = guest;
				continue;
			}
		return host;
	}
}


//
// Look up guest by guestRef.
// Throws if we don't have a guest by that ref.
//
CodeSigningHost::Guest *CodeSigningHost::findGuest(SecGuestRef guestRef, bool hostOk /* = false */)
{
	GuestMap::iterator it = mGuests.find(guestRef);
    if (it == mGuests.end()) {
        if (hostOk) {
			return NULL;
        } else {
			MacOSError::throwMe(errSecCSNoSuchCode);
        }
    }
	assert(it->first == it->second->guestRef());
	return it->second;
}


//
// Look up guest by attribute set.
// Returns the host if the attributes can't be found (*loose* interpretation).
// Throws if multiple guests are found (ambiguity).
// Implicitly matches dedicated guests no matter what attributes are requested.
//
CodeSigningHost::Guest *CodeSigningHost::findGuest(Guest *host, const CssmData &attrData)
{
	CFRef<CFDictionaryRef> attrDict = attrData
		? makeCFDictionaryFrom(attrData.data(), attrData.length())
		: makeCFDictionary(0);
	CFDictionary attrs(attrDict, errSecCSInvalidAttributeValues);
	
	// if a guest handle was provided, start with that - it must be valid or we fail
	if (CFNumberRef canonical = attrs.get<CFNumberRef>(kSecGuestAttributeCanonical)) {
		// direct lookup by SecGuestRef (canonical guest handle)
		SecGuestRef guestRef = cfNumber<SecGuestRef>(canonical);
		if (Guest *guest = findGuest(guestRef, true))	// found guest handle
			if (guest->isGuestOf(host, loose))
				host = guest;		// new starting point
			else
				MacOSError::throwMe(errSecCSNoSuchCode); // not a guest of given host
		else
			MacOSError::throwMe(errSecCSNoSuchCode); // not there at all
	}
	
	// now take the rest of the attrs
	CFIndex count = CFDictionaryGetCount(attrs);
	CFTypeRef keys[count], values[count];
	CFDictionaryGetKeysAndValues(attrs, keys, values);
	for (;;) {
		Guest *match = NULL;	// previous match found
        for (GuestMap::const_iterator it = mGuests.begin(); it != mGuests.end(); ++it) {
            if (it->second->isGuestOf(host, strict)) {
                if (it->second->matches(count, keys, values)) {
                    if (match) {
						MacOSError::throwMe(errSecCSMultipleGuests);	// ambiguous
                    } else {
						match = it->second;
                    }
                }
            }
        }
		if (!match)		// nothing found
			return host;
		else
			host = match;	// and repeat
	}
}


//
// Find any guest of a given host.
// This will return a randomly chosen guest of this host if it has any,
// or NULL if it has none (i.e. it is not a host).
//
CodeSigningHost::Guest *CodeSigningHost::findGuest(Guest *host)
{
	for (GuestMap::const_iterator it = mGuests.begin(); it != mGuests.end(); ++it)
		if (it->second->isGuestOf(host, strict))
			return it->second;
	return NULL;
}


//
// Register a hosting API service port where the host will dynamically
// answer hosting queries from interested parties. This switches the process
// to dynamic hosting mode, and is incompatible with proxy hosting.
//
void CodeSigningHost::registerCodeSigning(mach_port_t hostingPort, SecCSFlags flags)
{
	StLock<Mutex> _(mLock);
	switch (mHostingState) {
	case noHosting:
		mHostingPort = hostingPort;
		mHostingState = dynamicHosting;
        secnotice("SS", "%p host register: %d", this, mHostingPort.port());
		break;
	default:
		MacOSError::throwMe(errSecCSHostProtocolContradiction);
	}
}


//
// Create a guest entry for the given host and prepare to answer for it
// when dynamic hosting queries are received for it.
// This engages proxy hosting mode, and is incompatible with dynamic hosting mode.
//
SecGuestRef CodeSigningHost::createGuest(SecGuestRef hostRef,
		uint32_t status, const char *path,
		const CssmData &cdhash, const CssmData &attributes, SecCSFlags flags)
{
	StLock<Mutex> _(mLock);
	if (path[0] != '/')		// relative path (relative to what? :-)
		MacOSError::throwMe(errSecCSHostProtocolRelativePath);
	if (cdhash.length() > maxUcspHashLength)
		MacOSError::throwMe(errSecCSHostProtocolInvalidHash);
	
	// set up for hosting proxy services if nothing's there yet
	switch (mHostingState) {
	case noHosting:										// first hosting call, this host
		// set up proxy hosting
		mHostingPort.allocate();						// allocate service port
		MachServer::Handler::port(mHostingPort);		// put into Handler
		MachServer::active().add(*this);				// start listening
		mHostingState = proxyHosting;					// now proxying for this host
        secnotice("SS", "%p host proxy: %d", this, mHostingPort.port());
		break;
	case proxyHosting:									// already proxying
		break;
	case dynamicHosting:								// in dynamic mode, can't switch
		MacOSError::throwMe(errSecCSHostProtocolContradiction);
	}
	
	RefPointer<Guest> host = findHost(hostRef);
    if (RefPointer<Guest> knownGuest = findGuest(host)) {	// got a guest already
        if (flags & kSecCSDedicatedHost) {
			MacOSError::throwMe(errSecCSHostProtocolDedicationError);	// can't dedicate with other guests
        } else if (knownGuest->dedicated) {
			MacOSError::throwMe(errSecCSHostProtocolDedicationError);	// other guest is already dedicated
        }
    }

	// create the new guest
	RefPointer<Guest> guest = new Guest;
	if (host)
		guest->guestPath = host->guestPath;
	guest->guestPath.push_back(int_cast<CSSM_HANDLE,SecGuestRef>(guest->handle()));
	guest->status = status;
	guest->path = path;
	guest->setAttributes(attributes);
	guest->setHash(cdhash, flags & kSecCSGenerateGuestHash);
	guest->dedicated = (flags & kSecCSDedicatedHost);
	mGuests[guest->guestRef()] = guest;
    secnotice("SS", "%p guest create %d %d status:%d %d %s", this, hostRef, guest->guestRef(), guest->status, flags, guest->path.c_str());
	return guest->guestRef();
}


void CodeSigningHost::setGuestStatus(SecGuestRef guestRef, uint32_t status, const CssmData &attributes)
{
	StLock<Mutex> _(mLock);
	if (mHostingState != proxyHosting)
		MacOSError::throwMe(errSecCSHostProtocolNotProxy);
	Guest *guest = findGuest(guestRef);

	// state modification machine
	if ((status & ~guest->status) & kSecCodeStatusValid)
		MacOSError::throwMe(errSecCSHostProtocolStateError); // can't set
	if ((~status & guest->status) & (kSecCodeStatusHard | kSecCodeStatusKill))
		MacOSError::throwMe(errSecCSHostProtocolStateError); // can't clear
	guest->status = status;
    secnotice("SS", "%p guest change %d %d", this, guestRef, status);

	// replace attributes if requested
	if (attributes)
		guest->setAttributes(attributes);
}


//
// Remove a guest previously introduced via createGuest().
//
void CodeSigningHost::removeGuest(SecGuestRef hostRef, SecGuestRef guestRef)
{
	StLock<Mutex> _(mLock);
	if (mHostingState != proxyHosting) 
		MacOSError::throwMe(errSecCSHostProtocolNotProxy);
	RefPointer<Guest> host = findHost(hostRef);
	RefPointer<Guest> guest = findGuest(guestRef);
	if (guest->dedicated)	// can't remove a dedicated guest
		MacOSError::throwMe(errSecCSHostProtocolDedicationError);
	if (!guest->isGuestOf(host, strict))
		MacOSError::throwMe(errSecCSHostProtocolUnrelated);
	for (GuestMap::iterator it = mGuests.begin(); it != mGuests.end(); ++it)
		if (it->second->isGuestOf(guest, loose)) {
            secnotice("SS", "%p guest destroy %d", this, it->first);
			mGuests.erase(it);
		}
}


//
// The internal Guest object
//
CodeSigningHost::Guest::~Guest()
{ }

void CodeSigningHost::Guest::setAttributes(const CssmData &attrData)
{
	CFRef<CFNumberRef> guest = makeCFNumber(guestRef());
	if (attrData) {
		attributes.take(cfmake<CFDictionaryRef>("{+%O,%O=%O}",
			makeCFDictionaryFrom(attrData.data(), attrData.length()), kSecGuestAttributeCanonical, guest.get()));
	} else {
		attributes.take(makeCFDictionary(1, kSecGuestAttributeCanonical, guest.get()));
	}
}

CFDataRef CodeSigningHost::Guest::attrData() const
{
	if (!mAttrData)
		mAttrData = makeCFData(this->attributes.get());
	return mAttrData;
}


void CodeSigningHost::Guest::setHash(const CssmData &given, bool generate)
{
	if (given.length())		// explicitly given
		this->cdhash.take(makeCFData(given));
	else if (CFTypeRef hash = CFDictionaryGetValue(this->attributes, kSecGuestAttributeHash))
		if (CFGetTypeID(hash) == CFDataGetTypeID())
			this->cdhash = CFDataRef(hash);
		else
			MacOSError::throwMe(errSecCSHostProtocolInvalidHash);
	else if (generate) {		// generate from path (well, try)
		CFRef<SecStaticCodeRef> code;
		MacOSError::check(SecStaticCodeCreateWithPath(CFTempURL(this->path), kSecCSDefaultFlags, &code.aref()));
		CFRef<CFDictionaryRef> info;
		MacOSError::check(SecCodeCopySigningInformation(code, kSecCSDefaultFlags, &info.aref()));
		this->cdhash = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
	}
}


bool CodeSigningHost::Guest::isGuestOf(Guest *host, GuestCheck check) const
{
	vector<SecGuestRef> hostPath;
	if (host)
		hostPath = host->guestPath;
	if (hostPath.size() <= guestPath.size()
			&& !memcmp(&hostPath[0], &guestPath[0], sizeof(SecGuestRef) * hostPath.size()))
		// hostPath is a prefix of guestPath
		switch (check) {
		case loose:
			return true;
		case strict:
			return guestPath.size() == hostPath.size() + 1;	// immediate guest
		}
	return false;
}


//
// Check to see if a given guest matches the (possibly empty) attribute set provided
// (in broken-open form, for efficiency). A dedicated guest will match all attribute
// specifications, even empty ones. A non-dedicated guest matches if at least one
// attribute value requested matches exactly (in the sense of CFEqual) that given
// by the host for this guest.
// 
bool CodeSigningHost::Guest::matches(CFIndex count, CFTypeRef keys[], CFTypeRef values[]) const
{
	if (dedicated)
		return true;
	for (CFIndex n = 0; n < count; n++) {
		CFStringRef key = CFStringRef(keys[n]);
		if (CFEqual(key, kSecGuestAttributeCanonical))	// ignore canonical attribute (handled earlier)
			continue;
		if (CFTypeRef value = CFDictionaryGetValue(attributes, key))
			if (CFEqual(value, values[n]))
				return true;
	}
	return false;
}


//
// The MachServer dispatch handler for proxy hosting.
//

// give MIG handlers access to the object lock
class CodeSigningHost::Lock : private StLock<Mutex> {
public:
	Lock(CodeSigningHost *host) : StLock<Mutex>(host->mLock) { }
};


boolean_t cshosting_server(mach_msg_header_t *, mach_msg_header_t *);

static ThreadNexus<CodeSigningHost *> context;

boolean_t CodeSigningHost::handle(mach_msg_header_t *in, mach_msg_header_t *out)
{
	CodeSigningHost::Lock _(this);
	context() = this;
	return cshosting_server(in, out);
}


//
// Proxy implementation of Code Signing Hosting protocol
//
#define CSH_ARGS	mach_port_t servicePort, mach_port_t replyPort, OSStatus *rcode
	
#define BEGIN_IPC	try {
#define END_IPC		*rcode = noErr; } \
	catch (const CommonError &err) { *rcode = err.osStatus(); } \
	catch (...) { *rcode = errSecCSInternalError; } \
	return KERN_SUCCESS;

#define DATA_IN(base)	void *base, mach_msg_type_number_t base##Length
#define DATA_OUT(base)	void **base, mach_msg_type_number_t *base##Length
#define DATA(base)		CssmData(base, base##Length)


//
// Find a guest by arbitrary attribute set.
//
// This returns an array of canonical guest references describing the path
// from the host given to the guest found. If the host itself is returned
// as a guest, this will be an empty array (zero length).
//
// The subhost return argument may in the future return the hosting port for
// a guest who dynamically manages its hosting (thus breaking out of proxy mode),
// but this is not yet implemented.
//
kern_return_t cshosting_server_findGuest(CSH_ARGS, SecGuestRef hostRef,
	DATA_IN(attributes),
	GuestChain *foundGuest, mach_msg_type_number_t *depth, mach_port_t *subhost)
{
	BEGIN_IPC
		
	*subhost = MACH_PORT_NULL;	// preset no sub-hosting port returned
	
	Process::Guest *host = context()->findGuest(hostRef, true);
	if (Process::Guest *guest = context()->findGuest(host, DATA(attributes))) {
		*foundGuest = &guest->guestPath[0];
		*depth = int_cast<size_t, mach_msg_type_number_t>(guest->guestPath.size());
	} else {
		*foundGuest = NULL;
		*depth = 0;
	}
	END_IPC
}


//
// Retrieve the path to a guest specified by canonical reference.
//
kern_return_t cshosting_server_identifyGuest(CSH_ARGS, SecGuestRef guestRef,
	char *path, char *hash, uint32_t *hashLength, DATA_OUT(attributes))
{
	BEGIN_IPC
	CodeSigningHost::Guest *guest = context()->findGuest(guestRef);
	strncpy(path, guest->path.c_str(), MAXPATHLEN);

	// canonical cdhash
	if (guest->cdhash) {
		*hashLength = int_cast<size_t, uint32_t>(CFDataGetLength(guest->cdhash));
		assert(*hashLength <= maxUcspHashLength);
		memcpy(hash, CFDataGetBytePtr(guest->cdhash), *hashLength);
	} else
		*hashLength = 0;	// unavailable

	// visible attributes. This proxy returns all attributes set by the host
	CFDataRef attrData = guest->attrData();	// (the guest will cache this until it dies)
	*attributes = (void *)CFDataGetBytePtr(attrData);	// MIG botch (it doesn't need a writable pointer)
	*attributesLength = int_cast<CFIndex, mach_msg_type_number_t>(CFDataGetLength(attrData));
	
	END_IPC
}


//
// Retrieve the status word for a guest specified by canonical reference.
//
kern_return_t cshosting_server_guestStatus(CSH_ARGS, SecGuestRef guestRef, uint32_t *status)
{
	BEGIN_IPC
	*status = context()->findGuest(guestRef)->status;
	END_IPC
}


//
// Debug support
//
#if defined(DEBUGDUMP)

void CodeSigningHost::dump() const
{
	StLock<Mutex> _(mLock);
	switch (mHostingState) {
	case noHosting:
		break;
	case dynamicHosting:
		Debug::dump(" dynamic host port=%d", mHostingPort.port());
		break;
	case proxyHosting:
		Debug::dump(" proxy-host port=%d", mHostingPort.port());
		if (!mGuests.empty()) {
			Debug::dump(" %d guests={", int(mGuests.size()));
			for (GuestMap::const_iterator it = mGuests.begin(); it != mGuests.end(); ++it) {
				if (it != mGuests.begin())
					Debug::dump(", ");
				it->second->dump();
			}
			Debug::dump("}");
		}
		break;
	}
}

void CodeSigningHost::Guest::dump() const
{
	Debug::dump("%s[", path.c_str());
	for (vector<SecGuestRef>::const_iterator it = guestPath.begin(); it != guestPath.end(); ++it) {
		if (it != guestPath.begin())
			Debug::dump("/");
		Debug::dump("0x%x", *it);
	}
	Debug::dump("; status=0x%x attrs=%s]",
		status, cfString(CFCopyDescription(attributes), true).c_str());
}

#endif //DEBUGDUMP
