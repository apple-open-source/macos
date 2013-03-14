/*
 * Copyright (c) 2003-2008 Apple Inc. All Rights Reserved.
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
// codesigdb - code-hash equivalence database
//
#include "codesigdb.h"
#include "process.h"
#include "server.h"
#include "agentquery.h"
#include <security_utilities/memutils.h>
#include <security_utilities/logging.h>
#include <Security/SecRequirementPriv.h>


//
// A self-constructing database key class.
// Key format is <t><uid|S><key data>
//  where
// <t> single ASCII character type code ('H' for hash links)
// <uid|S> decimal userid of owning user, or 'S' for system entries. Followed by null byte.
// <key data> variable length key value (binary).
//
class DbKey : public CssmAutoData {
public:
	DbKey(char type, const CssmData &key, bool perUser = false, uid_t user = 0);
};

DbKey::DbKey(char type, const CssmData &key, bool perUser, uid_t user)
	: CssmAutoData(Allocator::standard())
{
	using namespace LowLevelMemoryUtilities;
	char header[20];
	size_t headerLength;
	if (perUser)
		headerLength = 1 + sprintf(header, "%c%d", type, user);
	else
		headerLength = 1 + sprintf(header, "%cS", type);
	malloc(headerLength + key.length());
	memcpy(this->data(), header, headerLength);
	memcpy(get().at(headerLength), key.data(), key.length());
}


//
// A subclass of Identity made of whole cloth (from a raw CodeSignature ACL information)
//
struct AclIdentity : public CodeSignatures::Identity {
	AclIdentity(const CssmData hash, string path) : mHash(hash), mPath(path) { }
		
	string getPath() const { return mPath; }
	const CssmData getHash() const { return mHash; }

private:
	const CssmData mHash;
	const string mPath;
};


//
// Construct a CodeSignatures objects
//
CodeSignatures::CodeSignatures(const char *path)
{
	try {
		mDb.open(path, O_RDWR | O_CREAT, 0644);
	} catch (const CommonError &err) {
		try {
			mDb.open(path, O_RDONLY, 0644);
			Syslog::warning("database %s opened READONLY (R/W failed errno=%d)", path, err.unixError());
			secdebug("codesign", "database %s opened READONLY (R/W failed errno=%d)", path, err.unixError());
		} catch (...) {
			Syslog::warning("cannot open %s; using no code equivalents", path);
			secdebug("codesign", "unable to open %s; using no code equivalents", path);
		}
	}
	if (mDb)
		mDb.flush();	// in case we just created it
	IFDUMPING("equiv", debugDump("open"));
}

CodeSignatures::~CodeSignatures()
{
}


//
// (Re)open the equivalence database.
// This is useful to switch to database in another volume.
//
void CodeSignatures::open(const char *path)
{
	mDb.open(path, O_RDWR | O_CREAT, 0644);
	mDb.flush();
	IFDUMPING("equiv", debugDump("reopen"));
}


//
// Basic Identity objects
//
CodeSignatures::Identity::Identity() : mState(untried)
{ }

CodeSignatures::Identity::~Identity()
{ }

string CodeSignatures::Identity::canonicalName(const string &path)
{
	string::size_type slash = path.rfind('/');
	if (slash == string::npos)	// bloody unlikely, but whatever...
		return path;
	return path.substr(slash+1);
}


//
// Find and store database objects (primitive layer)
//
bool CodeSignatures::find(Identity &id, uid_t user)
{
	if (id.mState != Identity::untried)
		return id.mState == Identity::valid;
	try {
		DbKey userKey('H', id.getHash(), true, user);
		CssmData linkValue;
		if (mDb.get(userKey, linkValue)) {
			id.mName = string(linkValue.interpretedAs<const char>(), linkValue.length());
			IFDUMPING("equiv", id.debugDump("found/user"));
			id.mState = Identity::valid;
			return true;
		}
		DbKey sysKey('H', id.getHash());
		if (mDb.get(sysKey, linkValue)) {
			id.mName = string(linkValue.interpretedAs<const char>(), linkValue.length());
			IFDUMPING("equiv", id.debugDump("found/system"));
			id.mState = Identity::valid;
			return true;
		}
	} catch (...) {
		secdebug("codesign", "exception validating identity for %s - marking failed", id.path().c_str());
		id.mState = Identity::invalid;
	}
	return id.mState == Identity::valid;
}

void CodeSignatures::makeLink(Identity &id, const string &ident, bool forUser, uid_t user)
{
	DbKey key('H', id.getHash(), forUser, user);
	if (!mDb.put(key, StringData(ident)))
		UnixError::throwMe();
}


//
// Administrative manipulation calls
//
void CodeSignatures::addLink(const CssmData &oldHash, const CssmData &newHash,
	const char *inName, bool forSystem)
{
	string name = Identity::canonicalName(inName);
	uid_t user = Server::process().uid();
	if (forSystem && user)	// only root user can establish forSystem links
		UnixError::throwMe(EACCES);
	if (!forSystem)	// in fact, for now we don't allow per-user calls at all
		UnixError::throwMe(EACCES);
	AclIdentity oldCode(oldHash, name);
	AclIdentity newCode(newHash, name);
	secdebug("codesign", "addlink for name %s", name.c_str());
	StLock<Mutex> _(mDatabaseLock);
	if (oldCode) {
		if (oldCode.trustedName() != name) {
			secdebug("codesign", "addlink does not match existing name %s",
				oldCode.trustedName().c_str());
			MacOSError::throwMe(CSSMERR_CSP_VERIFY_FAILED);
		}
	} else {
		makeLink(oldCode, name, !forSystem, user);
	}
	if (!newCode)
		makeLink(newCode, name, !forSystem, user);
	mDb.flush();
}

void CodeSignatures::removeLink(const CssmData &hash, const char *name, bool forSystem)
{
	AclIdentity code(hash, name);
	uid_t user = Server::process().uid();
	if (forSystem && user)	// only root user can remove forSystem links
		UnixError::throwMe(EACCES);
	DbKey key('H', hash, !forSystem, user);
	StLock<Mutex> _(mDatabaseLock);
	mDb.erase(key);
	mDb.flush();
}


//
// Verify signature matches.
// This ends up getting called when a CodeSignatureAclSubject is validated.
// The OSXVerifier describes what we require of the client code; the process represents
// the requesting client; and the context gives us access to the ACL and its environment
// in case we want to, well, creatively rewrite it for some reason. 
//
bool CodeSignatures::verify(Process &process,
	const OSXVerifier &verifier, const AclValidationContext &context)
{
	secdebug("codesign", "start verify");

	StLock<Mutex> _(process);
	SecCodeRef code = process.currentGuest();
	if (!code) {
		secdebug("codesign", "no code base: fail");
		return false;
	}
	if (SecRequirementRef requirement = verifier.requirement()) {
		// If the ACL contains a code signature (requirement), we won't match against unsigned code at all.
		// The legacy hash is ignored (it's for use by pre-Leopard systems).
		secdebug("codesign", "CS requirement present; ignoring legacy hashes");
		Server::active().longTermActivity();
		switch (OSStatus rc = SecCodeCheckValidity(code, kSecCSDefaultFlags, requirement)) {
		case noErr:
			secdebug("codesign", "CS verify passed");
			return true;
		case errSecCSUnsigned:
			secdebug("codesign", "CS verify against unsigned binary failed");
			return false;
		default:
			secdebug("codesign", "CS verify failed OSStatus=%d", int32_t(rc));
			return false;
		}
	}
	switch (matchSignedClientToLegacyACL(process, code, verifier, context)) {
	case noErr:						// handled, allow access
		return true;
	case errSecCSUnsigned:			// unsigned client, complete legacy case
		secdebug("codesign", "no CS requirement - using legacy hash");
		return verifyLegacy(process,
			CssmData::wrap(verifier.legacyHash(), SHA1::digestLength),
			verifier.path());
	default:						// client unsuitable, reject this match
		return false;
	}
}


//
// See if we can rewrite the ACL from legacy to Code Signing form without losing too much security.
// Returns true if the present validation should succeed (we probably rewrote the ACL).
// Returns false if the present validation shouldn't succeed based on what we did here (we may still
// have rewritten the ACL, in principle).
//
// Note that these checks add nontrivial overhead to ACL processing. We want to eventually phase
// this out, or at least make it an option that doesn't run all the time - perhaps an "extra legacy
// effort" per-client mode bit.
//
static string trim(string s, char delimiter)
{
	string::size_type p = s.rfind(delimiter);
	if (p != string::npos)
		s = s.substr(p + 1);
	return s;
}

static string trim(string s, char delimiter, string suffix)
{
	s = trim(s, delimiter);
	int preLength = s.length() - suffix.length();
	if (preLength > 0 && s.substr(preLength) == suffix)
		s = s.substr(0, preLength);
	return s;
}

OSStatus CodeSignatures::matchSignedClientToLegacyACL(Process &process,
	SecCodeRef code, const OSXVerifier &verifier, const AclValidationContext &context)
{
	//
	// Check whether we seem to be matching a legacy .Mac ACL against a member of the .Mac group
	//
	if (SecurityServerAcl::looksLikeLegacyDotMac(context)) {
		Server::active().longTermActivity();
		CFRef<SecRequirementRef> dotmac;
		MacOSError::check(SecRequirementCreateGroup(CFSTR("dot-mac"), NULL, kSecCSDefaultFlags, &dotmac.aref()));
		if (SecCodeCheckValidity(code, kSecCSDefaultFlags, dotmac) == noErr) {
			secdebug("codesign", "client is a dot-mac application; update the ACL accordingly");

			// create a suitable AclSubject (this is the above-the-API-line way)
			CFRef<CFDataRef> reqdata;
			MacOSError::check(SecRequirementCopyData(dotmac, kSecCSDefaultFlags, &reqdata.aref()));
			RefPointer<CodeSignatureAclSubject> subject = new CodeSignatureAclSubject(NULL, "group://dot-mac");
			subject->add((const BlobCore *)CFDataGetBytePtr(reqdata));

			// add it to the ACL and pass the access check (we just quite literally did it above)
			SecurityServerAcl::addToStandardACL(context, subject);
			return noErr;
		}
	}

	//
	// Get best names for the ACL (legacy) subject and the (signed) client
	//
	CFRef<CFDictionaryRef> info;
	MacOSError::check(SecCodeCopySigningInformation(code, kSecCSSigningInformation, &info.aref()));
	CFStringRef signingIdentity = CFStringRef(CFDictionaryGetValue(info, kSecCodeInfoIdentifier));
	if (!signingIdentity)		// unsigned
		return errSecCSUnsigned;

	string bundleName;	// client
	if (CFDictionaryRef infoList = CFDictionaryRef(CFDictionaryGetValue(info, kSecCodeInfoPList)))
		if (CFStringRef name = CFStringRef(CFDictionaryGetValue(infoList, kCFBundleNameKey)))
			bundleName = trim(cfString(name), '.');
	if (bundleName.empty())	// fall back to signing identifier
		bundleName = trim(cfString(signingIdentity), '.');

	string aclName = trim(verifier.path(), '/', ".app");	// ACL
	
	secdebug("codesign", "matching signed client \"%s\" against legacy ACL \"%s\"",
		bundleName.c_str(), aclName.c_str());
	
	//
	// Check whether we're matching a signed APPLE application against a legacy ACL by the same name
	//
	if (bundleName == aclName) {
		const unsigned char reqData[] = {		// "anchor apple", version 1 blob, embedded here
			0xfa, 0xde, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x10,
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03
		};
		CFRef<SecRequirementRef> apple;
		MacOSError::check(SecRequirementCreateWithData(CFTempData(reqData, sizeof(reqData)),
			kSecCSDefaultFlags, &apple.aref()));
		Server::active().longTermActivity();
		switch (OSStatus rc = SecCodeCheckValidity(code, kSecCSDefaultFlags, apple)) {
		case noErr:
			{
				secdebug("codesign", "withstands strict scrutiny; quietly adding new ACL");
				RefPointer<OSXCode> wrap = new OSXCodeWrap(code);
				RefPointer<AclSubject> subject = new CodeSignatureAclSubject(OSXVerifier(wrap));
				SecurityServerAcl::addToStandardACL(context, subject);
				return noErr;
			}
		default:
			secdebug("codesign", "validation fails with rc=%d, rejecting", int32_t(rc));
			return rc;
		}
		secdebug("codesign", "does not withstand strict scrutiny; ask the user");
		QueryCodeCheck query;
		query.inferHints(process);
		if (!query(verifier.path().c_str())) {
			secdebug("codesign", "user declined equivalence: cancel the access");
			CssmError::throwMe(CSSM_ERRCODE_USER_CANCELED);
		}
		RefPointer<OSXCode> wrap = new OSXCodeWrap(code);
		RefPointer<AclSubject> subject = new CodeSignatureAclSubject(OSXVerifier(wrap));
		SecurityServerAcl::addToStandardACL(context, subject);
		return noErr;
	}

	// not close enough to even ask - this can't match
	return errSecCSReqFailed;
}


//
// Perform legacy hash verification.
// This is the pre-Leopard (Tiger, Panther) code path. Here we only have legacy hashes
// (called, confusingly, "signatures"), which we're matching against suitably computed
// "signatures" (hashes) on the requesting application. We consult the CodeEquivalenceDatabase
// in a doomed attempt to track changes made to applications through updates, and issue
// equivalence dialogs to users if we have a name match (but hash mismatch). That's all
// there was before Code Signing; and that's what you'll continue to get if the requesting
// application is unsigned. Until we throw the whole mess out altogether, hopefully by
// the Next Big Cat After Leopard.
//
bool CodeSignatures::verifyLegacy(Process &process, const CssmData &signature, string path)
{
	// First of all, if the signature directly matches the client's code, we're obviously fine
	// we don't even need the database for that...
	Identity &clientIdentity = process;
	try {
		if (clientIdentity.getHash() == signature) {
			secdebug("codesign", "direct match: pass");
			return true;
		}
	} catch (...) {
		secdebug("codesign", "exception getting client code hash: fail");
		return false;
	}
	
#if CONSULT_LEGACY_CODE_EQUIVALENCE_DATABASE
	
	// Ah well. Establish mediator objects for database signature links
	AclIdentity aclIdentity(signature, path);

	uid_t user = process.uid();
	{
		StLock<Mutex> _(mDatabaseLock);
		find(aclIdentity, user);
		find(clientIdentity, user);
	}

	// if both links exist, we can decide this right now
	if (aclIdentity && clientIdentity) {
		if (aclIdentity.trustedName() == clientIdentity.trustedName()) {
			secdebug("codesign", "app references match: pass");
			return true;
		} else {
			secdebug("codesign", "client/acl links exist but are unequal: fail");
			return false;
		}
	}
	
	// check for name equality
	secdebug("codesign", "matching client %s against acl %s",
		clientIdentity.name().c_str(), aclIdentity.name().c_str());
	if (aclIdentity.name() != clientIdentity.name()) {
		secdebug("codesign", "name/path mismatch: fail");
		return false;
	}
	
	// The names match - we have a possible update.
	
	// Take the UI lock now to serialize "update rushes".
	LongtermStLock uiLocker(mUILock);
	
	// re-read the database in case some other thread beat us to the update
	{
		StLock<Mutex> _(mDatabaseLock);
		find(aclIdentity, user);
		find(clientIdentity, user);
	}
	if (aclIdentity && clientIdentity) {
		if (aclIdentity.trustedName() == clientIdentity.trustedName()) {
			secdebug("codesign", "app references match: pass (on the rematch)");
			return true;
		} else {
			secdebug("codesign", "client/acl links exist but are unequal: fail (on the rematch)");
			return false;
		}
	}
	
	// ask the user
	QueryCodeCheck query;
    query.inferHints(process);
	if (!query(aclIdentity.path().c_str()))
    {
		secdebug("codesign", "user declined equivalence: cancel the access");
		CssmError::throwMe(CSSM_ERRCODE_USER_CANCELED);
	}

	// take the database lock back for real
	StLock<Mutex> _(mDatabaseLock);
	
	// user wants us to go ahead and establish trust (if possible)
	if (aclIdentity) {
		// acl is linked but new client: link the client to this application
		makeLink(clientIdentity, aclIdentity.trustedName(), true, user);
		mDb.flush();
		secdebug("codesign", "client %s linked to application %s: pass",
			clientIdentity.path().c_str(), aclIdentity.trustedName().c_str());
		return true;
	}
	
	if (clientIdentity) {	// code link exists, acl link missing
		// client is linked but ACL (hash) never seen: link the ACL to this app
		makeLink(aclIdentity, clientIdentity.trustedName(), true, user);
		mDb.flush();
		secdebug("codesign", "acl %s linked to client %s: pass",
			aclIdentity.path().c_str(), clientIdentity.trustedName().c_str());
		return true;
	}
	
	// the De Novo case: no links, must create everything
	string ident = clientIdentity.name();
	makeLink(clientIdentity, ident, true, user);
	makeLink(aclIdentity, ident, true, user);
	mDb.flush();
	secdebug("codesign", "new linkages established: pass");
	return true;

#else /* ignore Code Equivalence Database */

	return false;

#endif
}


//
// Debug dumping support
//
#if defined(DEBUGDUMP)

void CodeSignatures::debugDump(const char *how) const
{
	using namespace Debug;
	using namespace LowLevelMemoryUtilities;
	if (!how)
		how = "dump";
	CssmData key, value;
	if (!mDb.first(key, value)) {
		dump("CODE EQUIVALENTS DATABASE IS EMPTY (%s)\n", how);
	} else {
		dump("CODE EQUIVALENTS DATABASE DUMP (%s)\n", how);
		do {
			const char *header = key.interpretedAs<const char>();
			size_t headerLength = strlen(header) + 1;
			dump("%s:", header);
			dumpData(key.at(headerLength), key.length() - headerLength);
			dump(" => ");
			dumpData(value);
			dump("\n");
		} while (mDb.next(key, value));
		dump("END DUMP\n");
	}
}

void CodeSignatures::Identity::debugDump(const char *how) const
{
	using namespace Debug;
	if (!how)
		how = "dump";
	dump("IDENTITY (%s) path=%s", how, getPath().c_str());
	dump(" name=%s hash=", mName.empty() ? "(unset)" : mName.c_str());
	dumpData(getHash());
	dump("\n");
}

#endif //DEBUGDUMP
