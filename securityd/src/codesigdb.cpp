/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
class AclIdentity : public CodeSignatures::Identity {
public:
	AclIdentity(const CodeSigning::Signature *sig, const char *comment)
		: mHash(*sig), mPath(comment ? comment : "") { }
	AclIdentity(const CssmData &hash, const char *comment)
		: mHash(hash), mPath(comment ? comment : "") { }
		
protected:
	std::string getPath() const { return mPath; }
	const CssmData getHash(CodeSigning::OSXSigner &) const { return mHash; }
	
private:
	const CssmData mHash;
	std::string mPath;
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
		DbKey userKey('H', id.getHash(mSigner), true, user);
		CssmData linkValue;
		if (mDb.get(userKey, linkValue)) {
			id.mName = string(linkValue.interpretedAs<const char>(), linkValue.length());
			IFDUMPING("equiv", id.debugDump("found/user"));
			id.mState = Identity::valid;
			return true;
		}
		DbKey sysKey('H', id.getHash(mSigner));
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
	DbKey key('H', id.getHash(mSigner), forUser, user);
	if (!mDb.put(key, StringData(ident)))
		UnixError::throwMe();
}

void CodeSignatures::makeApplication(const std::string &name, const std::string &path)
{
	//@@@ create app record and fill (later)
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
	AclIdentity oldCode(oldHash, name.c_str());
	AclIdentity newCode(newHash, name.c_str());
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
// Verify signature matches
//
bool CodeSignatures::verify(Process &process,
	const CodeSigning::Signature *trustedSignature, const CssmData *comment)
{
	secdebug("codesign", "start verify");

	// if we have no client code, we cannot possibly match this
	if (!process.clientCode()) {
		secdebug("codesign", "no code base: fail");
		return false;
	}

	// first of all, if the signature directly matches the client's code, we're obviously fine
	// we don't even need the database for that...
	Identity &clientIdentity = process;
	try {
		if (clientIdentity.getHash(mSigner) == CssmData(*trustedSignature)) {
			secdebug("codesign", "direct match: pass");
			return true;
		}
	} catch (...) {
		secdebug("codesign", "exception getting client code hash: fail");
		return false;
	}
	
	// ah well. Establish mediator objects for database signature links
	AclIdentity aclIdentity(trustedSignature, comment ? comment->interpretedAs<const char>() : NULL);

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
	Server::active().longTermActivity();
	StLock<Mutex> uiLocker(mUILock);
	
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
		secdebug("codesign", "user declined equivalence: fail");
		return false;
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
	makeApplication(ident, clientIdentity.path());
	makeLink(clientIdentity, ident, true, user);
	makeLink(aclIdentity, ident, true, user);
	mDb.flush();
	secdebug("codesign", "new linkages established: pass");
	return true;
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
	CodeSigning::OSXSigner signer;
	dumpData(getHash(signer));
	dump("\n");
}

#endif //DEBUGDUMP