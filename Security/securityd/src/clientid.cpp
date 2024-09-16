/*
 * Copyright (c) 2006-2009,2012,2016 Apple Inc. All Rights Reserved.
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
#include <Security/oidsattr.h>
#include <Security/SecCertificatePriv.h>
#include <CrashReporterClient.h>
#include <utilities/SecInternalReleasePriv.h>


//
// Constructing a ClientIdentification doesn't do much.
// We're waiting for setup(), which should be called by the child class's
// constructor.
//
ClientIdentification::ClientIdentification()
	: mGotPartitionId(false)
{
}


//
// Initialize the ClientIdentification.
// This creates a process-level code object for the client.
//
void ClientIdentification::setup(Security::CommonCriteria::AuditToken const &audit)
{
    StLock<Mutex> _(mLock);
    StLock<Mutex> __(mValidityCheckLock);
    
    audit_token_t const token = audit.auditToken();
    OSStatus rc = SecCodeCreateWithAuditToken(&token, kSecCSDefaultFlags, &mClientProcess.aref());
    
    if (rc) {
        secerror("could not get code for process %d: OSStatus=%d",
                audit.pid(), int32_t(rc));
    }
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
		if (it != mGuests.end()) {
			it->second.lastTouchTime = dispatch_time(DISPATCH_TIME_NOW, 0);
			return &it->second;
		}

		while (mGuests.size() >= kMaxGuestMapSize) {
			it = mGuests.begin();
			dispatch_time_t oldestTime = it->second.lastTouchTime;
			SecGuestRef oldest = it->first;
			while (++it != mGuests.end()) {
				if (it->second.lastTouchTime < oldestTime) {
					oldestTime = it->second.lastTouchTime;
					oldest = it->first;
				}
			}
			mGuests.erase(oldest);
		}
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
		[[fallthrough]];		//  ... the guest we expected to be there isn't
	default:
		MacOSError::throwMe(rc);
	}
	StLock<Mutex> _(mLock);
	GuestState &slot = mGuests[guestRef];
	if (!slot.code)	// if another thread didn't get here first...
		slot.code = code;

	slot.lastTouchTime = dispatch_time(DISPATCH_TIME_NOW, 0);
	return &slot;
}


//
// Return the partition id ascribed to this client.
// This is assigned to the whole client process - it is not per-guest.
//
std::string ClientIdentification::partitionId() const
{
	if (!mGotPartitionId) {
		StLock<Mutex> _(mValidityCheckLock);
		mClientPartitionId = partitionIdForProcess(processCode());
		mGotPartitionId = true;
	}
	return mClientPartitionId;
}


static std::string hashString(CFDataRef data)
{
	CFIndex length = CFDataGetLength(data);
	const unsigned char *hash = CFDataGetBytePtr(data);
	char s[2 * length + 1];
	for (CFIndex n = 0; n < length; n++)
		snprintf(&s[2*n], 2 * length + 1 - (2 * n), "%2.2x", hash[n]);
	return s;
}


std::string ClientIdentification::partitionIdForProcess(SecStaticCodeRef code)
{
	static CFStringRef const appleReq = CFSTR("anchor apple");
	static CFStringRef const masReq = CFSTR("anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.9]");
    // TestFlight cert chain, which should be treated like MAS
    static CFStringRef const tfReq = CFSTR("anchor apple generic"
                                           " and "
                                           "certificate 1[field.1.2.840.113635.100.6.2.1] exists"
                                           " and "
                                           "certificate leaf[field.1.2.840.113635.100.6.1.25.1] exists");
    // On internal systems, we also allow the QA version of the TestFlight hierarchy to act like MAS
    static CFStringRef const tfQaInternalReq = CFSTR("anchor apple generic"
                                                     " and "
                                                     "certificate 1[field.1.2.840.113635.100.6.2.1] exists"
                                                     " and "
                                                     "("
                                                        "certificate leaf[field.1.2.840.113635.100.6.1.25.1] exists"
                                                        " or "
                                                        "certificate leaf[field.1.2.840.113635.100.6.1.25.2] exists"
                                                     ")");
    static CFStringRef const developmentOrDevIDReq = CFSTR("anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.6] and certificate leaf[field.1.2.840.113635.100.6.1.13]" // Developer ID CA and Leaf
                                                           " or "
                                                           "anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.1] and certificate leaf[field.1.2.840.113635.100.6.1.12]" // WWDR CA and Mac Development Leaf
                                                           " or "
                                                           "anchor apple generic and certificate 1[field.1.2.840.113635.100.6.2.1] and certificate leaf[field.1.2.840.113635.100.6.1.7]"); // WWDR CA and  Mac Distribution Leaf
	static SecRequirementRef apple;
	static SecRequirementRef mas;
	static SecRequirementRef tf;
	static SecRequirementRef tfQaInternal;
	static SecRequirementRef developmentOrDevID;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (noErr != SecRequirementCreateWithString(appleReq, kSecCSDefaultFlags, &apple)) {
            secerror("Unable to create SecRequirement for 'anchor apple'");
            CRSetCrashLogMessage("Unable to create SecRequirement for 'anchor apple'");
            abort();
        }
        if(noErr != SecRequirementCreateWithString(masReq, kSecCSDefaultFlags, &mas)) {
            secerror("Unable to create SecRequirement for mac app store");
            CRSetCrashLogMessage("Unable to create SecRequirement for mac app store");
            abort();
        }
        if(noErr != SecRequirementCreateWithString(tfReq, kSecCSDefaultFlags, &tf)) {
            secerror("Unable to create SecRequirement for testflight");
            CRSetCrashLogMessage("Unable to create SecRequirement for testflight");
            abort();
        }
        if(noErr != SecRequirementCreateWithString(tfQaInternalReq, kSecCSDefaultFlags, &tfQaInternal)) {
            secerror("Unable to create SecRequirement for internal testflight");
            CRSetCrashLogMessage("Unable to create SecRequirement for internal testflight");
            abort();
        }
        if(noErr != SecRequirementCreateWithString(developmentOrDevIDReq, kSecCSDefaultFlags, &developmentOrDevID)) {
            secerror("Unable to create SecRequirement for development or dev ID");
            CRSetCrashLogMessage("Unable to create SecRequirement for development or dev ID");
            abort();
        }
    });

	OSStatus rc;
	switch (rc = SecStaticCodeCheckValidity(code, kSecCSBasicValidateOnly, apple)) {
	case noErr:
	case errSecCSReqFailed:
		break;
	case errSecCSUnsigned:
		return "unsigned:";
	default:
		MacOSError::throwMe(rc);
	}
	CFRef<CFDictionaryRef> info;
	if (OSStatus irc = SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()))
		MacOSError::throwMe(irc);

	if (rc == noErr) {
        // for apple-signed code, make it canonical apple
        if (CFEqual(CFDictionaryGetValue(info, kSecCodeInfoIdentifier), CFSTR("com.apple.security"))) {
			return "apple-tool:";	// take security(1) into a separate partition so it can't automatically peek into Apple's own
        } else {
			return "apple:";
        }
	} else if (noErr == SecStaticCodeCheckValidity(code, kSecCSBasicValidateOnly, mas) ||
               noErr == SecStaticCodeCheckValidity(code, kSecCSBasicValidateOnly, tf) ||
               (SecIsInternalRelease() && noErr == SecStaticCodeCheckValidity(code, kSecCSBasicValidateOnly, tfQaInternal))) {
		// for MAS- or TestFlight-signed code, we take the embedded team identifier (verified by Apple)
		return "teamid:" + cfString(CFStringRef(CFDictionaryGetValue(info, kSecCodeInfoTeamIdentifier)));
	} else if (noErr == SecStaticCodeCheckValidity(code, kSecCSBasicValidateOnly, developmentOrDevID)) {
		// for developer-signed code, we take the team identifier from the signing certificate's OU field
		CFRef<CFDictionaryRef> info;
		if (noErr != (rc = SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref())))
			MacOSError::throwMe(rc);
		CFArrayRef certChain = CFArrayRef(CFDictionaryGetValue(info, kSecCodeInfoCertificates));
		SecCertificateRef signingCert = SecCertificateRef(CFArrayGetValueAtIndex(certChain, 0));
		CFRef<CFStringRef> ou;
		SecCertificateCopySubjectComponent(signingCert, &CSSMOID_OrganizationalUnitName, &ou.aref());
		return "teamid:" + cfString(ou);
	} else {
		// cannot positively classify this code, but it's signed
		CFDataRef cdhashData = CFDataRef(CFDictionaryGetValue(info, kSecCodeInfoUnique));
		assert(cdhashData);
		return "cdhash:" + hashString(cdhashData);
	}
}


//
// Support for the legacy hash identification mechanism.
// The legacy machinery deals exclusively in terms of processes.
// It knows nothing about guests and their identities.
//
string ClientIdentification::getPath() const
{
	assert(mClientProcess);
	StLock<Mutex> _(mValidityCheckLock);
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

AclSubject* ClientIdentification::copyAclSubject() const
{
	StLock<Mutex> _(mValidityCheckLock);
	RefPointer<OSXCode> clientXCode = new OSXCodeWrap(currentGuest());
	return new CodeSignatureAclSubject(OSXVerifier(clientXCode));
}

OSStatus ClientIdentification::copySigningInfo(SecCSFlags flags,
	CFDictionaryRef *info) const
{
	StLock<Mutex> _(mValidityCheckLock);
	return SecCodeCopySigningInformation(currentGuest(), flags, info);
}

OSStatus ClientIdentification::checkValidity(SecCSFlags flags,
	SecRequirementRef requirement) const
{
	// Make sure more than one thread cannot be evaluating this code signature concurrently
	StLock<Mutex> _(mValidityCheckLock);
	return SecCodeCheckValidityWithErrors(currentGuest(), flags, requirement, NULL);
}

bool ClientIdentification::checkAppleSigned() const
{
	// This is the clownfish supported way to check for a Mac App Store or B&I signed build
	static CFStringRef const requirementString = CFSTR("(anchor apple) or (anchor apple generic and certificate leaf[field.1.2.840.113635.100.6.1.9])");
	CFRef<SecRequirementRef> secRequirementRef = NULL;
	OSStatus status = SecRequirementCreateWithString(requirementString, kSecCSDefaultFlags, &secRequirementRef.aref());
	if (status == errSecSuccess) {
		status = checkValidity(kSecCSDefaultFlags, secRequirementRef);
		if (status != errSecSuccess) {
			secnotice("clientid", "code requirement check failed (%d), client is not Apple-signed", (int32_t)status);
		} else {
			return true;
		}
	}
	return false;
}


bool ClientIdentification::hasEntitlement(const char *name) const
{
	CFRef<CFDictionaryRef> info;
	{
		StLock<Mutex> _(mValidityCheckLock);
		MacOSError::check(SecCodeCopySigningInformation(processCode(), kSecCSDefaultFlags, &info.aref()));
	}
	CFCopyRef<CFDictionaryRef> entitlements = (CFDictionaryRef)CFDictionaryGetValue(info, kSecCodeInfoEntitlementsDict);
	if (entitlements && entitlements.is<CFDictionaryRef>()) {
		CFTypeRef value = CFDictionaryGetValue(entitlements, CFTempString(name));
		if (value && value != kCFBooleanFalse)
			return true;		// have entitlement, it's not <false/> - bypass partition construction
	}
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
