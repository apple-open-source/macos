/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// database - database session management
//
#include "xdatabase.h"
#include "agentquery.h"
#include "key.h"
#include "server.h"
#include "session.h"
#include "cfnotifier.h"	// legacy
#include "notifications.h"
#include "SecurityAgentClient.h"
#include <Security/acl_any.h>	// for default owner ACLs
#include <Security/wrapkey.h>
#include <Security/endian.h>


//
// Create a Database object from initial parameters (create operation)
//
Database::Database(const DLDbIdentifier &id, const DBParameters &params, Process &proc,
            const AccessCredentials *cred, const AclEntryPrototype *owner)
	: SecurityServerAcl(dbAcl, CssmAllocator::standard()), process(proc),
    mValidData(false), version(0), mBlob(NULL)
{
    // save a copy of the credentials for later access control
    mCred = DataWalkers::copy(cred, CssmAllocator::standard());

    // create a new random signature to complete the DLDbIdentifier
    Signature newSig;
    Server::active().random(newSig.bytes);
    DbIdentifier ident(id, newSig);
    
    // create common block and initialize
	CommonMap &commons = proc.session.databases();
	common = new Common(ident, commons);
	StLock<Mutex> _(*common);
	{	StLock<Mutex> _(commons);
		assert(commons.find(ident) == commons.end());	// better be new!
		commons[ident] = common;
		common->useCount++;
	}
	// new common is now visible but we hold its lock

	// establish the new master secret
	establishNewSecrets(cred, SecurityAgent::newDatabase);
	
	// set initial database parameters
	common->mParams = params;
		
	// we're unlocked now
	common->makeNewSecrets();

	// establish initial ACL
	if (owner)
		cssmSetInitial(*owner);
	else
		cssmSetInitial(new AnyAclSubject());
    mValidData = true;
    
    // for now, create the blob immediately
    encode();
    
    // register with process
    process.addDatabase(this);
    
	secdebug("SSdb", "database %s(%p) created, common at %p",
		common->dbName(), this, common);
	IFDUMPING("SSdb", debugDump("creation complete"));
}


//
// Create a Database object from a database blob (decoding)
//
Database::Database(const DLDbIdentifier &id, const DbBlob *blob, Process &proc,
    const AccessCredentials *cred)
	: SecurityServerAcl(dbAcl, CssmAllocator::standard()), process(proc),
    mValidData(false), version(0)
{
    // perform basic validation on the incoming blob
	assert(blob);
    blob->validate(CSSMERR_APPLEDL_INVALID_DATABASE_BLOB);
    switch (blob->version()) {
#if defined(COMPAT_OSX_10_0)
    case blob->version_MacOS_10_0:
        break;
#endif
    case blob->version_MacOS_10_1:
        break;
    default:
        CssmError::throwMe(CSSMERR_APPLEDL_INCOMPATIBLE_DATABASE_BLOB);
    }

    // save a copy of the credentials for later access control
    mCred = DataWalkers::copy(cred, CssmAllocator::standard());
    
    // check to see if we already know about this database
    DbIdentifier ident(id, blob->randomSignature);
	CommonMap &commons = proc.session.databases();
	StLock<Mutex> mapLock(commons);
    CommonMap::iterator it = commons.find(ident);
    if (it != commons.end()) {
        // already there
        common = it->second;				// reuse common component
        //@@@ arbitrate sequence number here, perhaps update common->mParams
		StLock<Mutex> _(*common);			// lock common against other users
		common->useCount++;
		secdebug("SSdb",
            "open database %s(%p) version %lx at known common %p(%d)",
			common->dbName(), this, blob->version(), common, int(common->useCount));
    } else {
        // newly introduced
        commons[ident] = common = new Common(ident, commons);
		common->mParams = blob->params;
		common->useCount++;
		secdebug("SSdb", "open database %s(%p) version %lx with new common %p",
			common->dbName(), this, blob->version(), common);
    }
    
    // register with process
    process.addDatabase(this);

    mBlob = blob->copy();
    IFDUMPING("SSdb", debugDump("end of decode"));
}


//
// Destroy a Database
//
Database::~Database()
{
    secdebug("SSdb", "deleting database %s(%p) common %p (%d refs)",
        common->dbName(), this, common, int(common->useCount));
    IFDUMPING("SSdb", debugDump("deleting database instance"));
    process.removeDatabase(this);
    CssmAllocator::standard().free(mCred);
	CssmAllocator::standard().free(mBlob);

    // take the commonLock to avoid races against re-use of the common
	CommonMap &commons = process.session.databases();
    StLock<Mutex> __(commons);
    if (--common->useCount == 0 && common->isLocked()) {
        // last use of this database, and it's locked - discard
		IFDUMPING("SSdb", debugDump("discarding common"));
        discard(common);
    } else if (common->useCount == 0)
		IFDUMPING("SSdb", debugDump("retained because it's unlocked"));
}


//
// (Re-)Authenticate the database. This changes the stored credentials.
//
void Database::authenticate(const AccessCredentials *cred)
{
	StLock<Mutex> _(*common);
	AccessCredentials *newCred = DataWalkers::copy(cred, CssmAllocator::standard());
    CssmAllocator::standard().free(mCred);
    mCred = newCred;
}


//
// Return the database blob, recalculating it as needed.
//
DbBlob *Database::blob()
{
	StLock<Mutex> _(*common);
    if (!validBlob()) {
        makeUnlocked();			// unlock to get master secret
		encode();				// (re)encode blob if needed
    }
    activity();					// reset timeout
	assert(validBlob());		// better have a valid blob now...
    return mBlob;
}


//
// Encode the current database as a blob.
// Note that this returns memory we own and keep.
// Caller must hold common lock.
//
void Database::encode()
{
	DbBlob *blob = common->encode(*this);
	CssmAllocator::standard().free(mBlob);
	mBlob = blob;
	version = common->version;
	secdebug("SSdb", "encoded database %p common %p(%s) version %ld params=(%ld,%d)",
		this, common, dbName(), version,
		common->mParams.idleTimeout, common->mParams.lockOnSleep);
}


//
// Change the passphrase on a database
//
void Database::changePassphrase(const AccessCredentials *cred)
{
	// get and hold the common lock (don't let other threads break in here)
	StLock<Mutex> _(*common);
	
	// establish OLD secret - i.e. unlock the database
	//@@@ do we want to leave the final lock state alone?
	makeUnlocked(cred);
	
    // establish NEW secret
	establishNewSecrets(cred, SecurityAgent::changePassphrase);
	common->version++;	// blob state changed
	secdebug("SSdb", "Database %s(%p) master secret changed", common->dbName(), this);
	encode();			// force rebuild of local blob
	
	// send out a notification
	KeychainNotifier::passphraseChanged(identifier());

    // I guess this counts as an activity
    activity();
}


//
// Extract the database master key as a proper Key object.
//
Key *Database::extractMasterKey(Database *db,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	uint32 usage, uint32 attrs)
{
	// get and hold common lock
	StLock<Mutex> _(*common);
	
	// unlock to establish master secret
	makeUnlocked(cred);
	
	// extract the raw cryptographic key
	CssmClient::WrapKey wrap(Server::csp(), CSSM_ALGID_NONE);
	CssmKey key;
	wrap(common->masterKey(), key);
	
	// make the key object and return it
	return new Key(db, key, attrs & Key::managedAttributes, owner);
}


//
// Construct a binary blob of moderate size that is suitable for constructing
// an index identifying this database.
// We construct this from the database's marker blob, which is created with
// the database is made, and stays stable thereafter.
// Note: Ownership of the index blob passes to the caller.
// @@@ This means that physical copies share this index.
//
void Database::getDbIndex(CssmData &indexData)
{
	if (!mBlob)
		encode();	// force blob creation
	assert(mBlob);
	CssmData signature = CssmData::wrap(mBlob->randomSignature);
	indexData = CssmAutoData(CssmAllocator::standard(), signature).release();
}


//
// Unlock this database (if needed) by obtaining the master secret in some
// suitable way and then proceeding to unlock with it.
// Does absolutely nothing if the database is already unlocked.
// The makeUnlocked forms are identical except the assume the caller already
// holds the common lock.
//
void Database::unlock()
{
	StLock<Mutex> _(*common);
	makeUnlocked();
}

void Database::makeUnlocked()
{
	return makeUnlocked(mCred);
}

void Database::makeUnlocked(const AccessCredentials *cred)
{
    IFDUMPING("SSdb", debugDump("default procedures unlock"));
    if (isLocked()) {
        assert(mBlob || (mValidData && common->hasMaster()));
		establishOldSecrets(cred);
		activity();				// set timeout timer
	} else if (!mValidData)	{	// need to decode to get our ACLs, passphrase available
		if (!decode())
			CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
	}
	assert(!isLocked());
	assert(mValidData);
}


//
// The following unlock given an explicit passphrase, rather than using
// (special cred sample based) default procedures.
//
void Database::unlock(const CssmData &passphrase)
{
	StLock<Mutex> _(*common);
	makeUnlocked(passphrase);
}

void Database::makeUnlocked(const CssmData &passphrase)
{
	if (isLocked()) {
		if (decode(passphrase))
			return;
		else
			CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
	} else if (!mValidData)	{	// need to decode to get our ACLs, passphrase available
		if (!decode())
			CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
	}
	assert(!isLocked());
	assert(mValidData);
}


//
// Nonthrowing passphrase-based unlock. This returns false if unlock failed.
// Note that this requires an explicitly given passphrase.
// Caller must hold common lock.
//
bool Database::decode(const CssmData &passphrase)
{
	assert(mBlob);
	common->setup(mBlob, passphrase);
	return decode();
}


//
// Given the established master secret, decode the working keys and other
// functional secrets for this database. Return false (do NOT throw) if
// the decode fails. Call this in low(er) level code once you established
// the master key.
//
bool Database::decode()
{
	assert(mBlob);
	assert(common->hasMaster());
	void *privateAclBlob;
	if (common->unlock(mBlob, &privateAclBlob)) {
		if (!mValidData) {
			importBlob(mBlob->publicAclBlob(), privateAclBlob);
			mValidData = true;
		}
		CssmAllocator::standard().free(privateAclBlob);
		return true;
	}
	secdebug("SSdb", "%p decode failed", this);
	return false;
}


//
// Given an AccessCredentials for this database, wring out the existing primary
// database secret by whatever means necessary.
// On entry, caller must hold the database common lock. It will be held throughout.
// On exit, the crypto core has its master secret. If things go wrong,
// we will throw a suitable exception. Note that encountering any malformed
// credential sample will throw, but this is not guaranteed -- don't assume
// that NOT throwing means creds is entirely well-formed.
//
// How this works:
// Walk through the creds. Fish out those credentials (in order) that
// are for unlock processing (they have no ACL subject correspondents),
// and (try to) obey each in turn, until one produces a valid secret
// or you run out. If no special samples are found at all, interpret that as
// "use the system global default," which happens to be hard-coded right here.
//
void Database::establishOldSecrets(const AccessCredentials *creds)
{
	list<CssmSample> samples;
	if (creds && creds->samples().collect(CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK, samples)) {
		for (list<CssmSample>::iterator it = samples.begin(); it != samples.end(); it++) {
			TypedList &sample = *it;
			sample.checkProper();
			switch (sample.type()) {
			// interactively prompt the user - no additional data
			case CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT:
				{
				secdebug("SSdb", "%p attempting interactive unlock", this);
				QueryUnlock query(*this);
				if (query() == SecurityAgent::noReason)
					return;
				}
				break;
			// try to use an explicitly given passphrase - Data:passphrase
			case CSSM_SAMPLE_TYPE_PASSWORD:
				if (sample.length() != 2)
					CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
				secdebug("SSdb", "%p attempting passphrase unlock", this);
				if (decode(sample[1]))
					return;
				break;
			// try to open with a given master key - Data:CSP or KeyHandle, Data:CssmKey
			case CSSM_WORDID_SYMMETRIC_KEY:
				assert(mBlob);
				secdebug("SSdb", "%p attempting explicit key unlock", this);
				common->setup(mBlob, keyFromCreds(sample));
				if (decode())
					return;
				break;
			// explicitly defeat the default action but don't try anything in particular
			case CSSM_WORDID_CANCELED:
				secdebug("SSdb", "%p defeat default action", this);
				break;
			default:
				// Unknown sub-sample for unlocking.
				// If we wanted to be fascist, we could now do
				//  CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
				// But instead we try to be tolerant and continue on.
				// This DOES however count as an explicit attempt at specifying unlock,
				// so we will no longer try the default case below...
				secdebug("SSdb", "%p unknown sub-sample unlock (%ld) ignored", this, sample.type());
				break;
			}
		}
	} else {
		// default action
		assert(mBlob);
		SystemKeychainKey systemKeychain(kSystemUnlockFile);
		if (systemKeychain.matches(mBlob->randomSignature)) {
			secdebug("SSdb", "%p attempting system unlock", this);
			common->setup(mBlob, CssmClient::Key(Server::csp(), systemKeychain.key(), true));
			if (decode())
				return;
		}
		
		QueryUnlock query(*this);
		if (query() == SecurityAgent::noReason)
			return;
	}
	
	// out of options - no secret obtained
	CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
}


//
// Same thing, but obtain a new secret somehow and set it into the common.
//
void Database::establishNewSecrets(const AccessCredentials *creds, SecurityAgent::Reason reason)
{
	list<CssmSample> samples;
	if (creds && creds->samples().collect(CSSM_SAMPLE_TYPE_KEYCHAIN_CHANGE_LOCK, samples)) {
		for (list<CssmSample>::iterator it = samples.begin(); it != samples.end(); it++) {
			TypedList &sample = *it;
			sample.checkProper();
			switch (sample.type()) {
			// interactively prompt the user
			case CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT:
				{
				secdebug("SSdb", "%p specified interactive passphrase", this);
				QueryNewPassphrase query(*this, reason);
				CssmAutoData passphrase(CssmAllocator::standard(CssmAllocator::sensitive));
				if (query(passphrase) == SecurityAgent::noReason) {
					common->setup(NULL, passphrase);
					return;
				}
				}
				break;
			// try to use an explicitly given passphrase
			case CSSM_SAMPLE_TYPE_PASSWORD:
				secdebug("SSdb", "%p specified explicit passphrase", this);
				if (sample.length() != 2)
					CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
				common->setup(NULL, sample[1]);
				return;
			// try to open with a given master key
			case CSSM_WORDID_SYMMETRIC_KEY:
				secdebug("SSdb", "%p specified explicit master key", this);
				common->setup(NULL, keyFromCreds(sample));
				return;
			// explicitly defeat the default action but don't try anything in particular
			case CSSM_WORDID_CANCELED:
				secdebug("SSdb", "%p defeat default action", this);
				break;
			default:
				// Unknown sub-sample for acquiring new secret.
				// If we wanted to be fascist, we could now do
				//  CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
				// But instead we try to be tolerant and continue on.
				// This DOES however count as an explicit attempt at specifying unlock,
				// so we will no longer try the default case below...
				secdebug("SSdb", "%p unknown sub-sample acquisition (%ld) ignored",
					this, sample.type());
				break;
			}
		}
	} else {
		// default action -- interactive (only)
		QueryNewPassphrase query(*this, reason);
		CssmAutoData passphrase(CssmAllocator::standard(CssmAllocator::sensitive));
		if (query(passphrase) == SecurityAgent::noReason) {
			common->setup(NULL, passphrase);
			return;
		}
	}
	
	// out of options - no secret obtained
	CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
}


//
// Given a (truncated) Database credentials TypedList specifying a master key,
// locate the key and return a reference to it.
//
CssmClient::Key Database::keyFromCreds(const TypedList &sample)
{
	// decode TypedList structure (sample type; Data:CSPHandle; Data:CSSM_KEY)
	assert(sample.type() == CSSM_WORDID_SYMMETRIC_KEY);
	if (sample.length() != 3
		|| sample[1].type() != CSSM_LIST_ELEMENT_DATUM
		|| sample[2].type() != CSSM_LIST_ELEMENT_DATUM)
			CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
	CSSM_CSP_HANDLE &handle = *sample[1].data().interpretedAs<CSSM_CSP_HANDLE>(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
	CssmKey &key = *sample[2].data().interpretedAs<CssmKey>(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);

	if (key.header().cspGuid() == gGuidAppleCSPDL) {
		// handleOrKey is a SecurityServer KeyHandle; ignore key argument
		return Server::key(handle);
	} else {
		// not a KeyHandle reference; use key as a raw key
		if (key.header().blobType() != CSSM_KEYBLOB_RAW)
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
		if (key.header().keyClass() != CSSM_KEYCLASS_SESSION_KEY)
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
		return CssmClient::Key(Server::csp(), key, true);
	}
}


//
// Verify a putative database passphrase.
// If the database is already unlocked, just check the passphrase.
// Otherwise, unlock with that passphrase and report success.
// Caller must hold the common lock.
//
bool Database::validatePassphrase(const CssmData &passphrase) const
{
	if (common->hasMaster()) {
		// verify against known secret
		return common->validatePassphrase(passphrase);
	} else {
		// no master secret - perform "blind" unlock to avoid actual unlock
		try {
			DatabaseCryptoCore test;
			test.setup(mBlob, passphrase);
			test.decodeCore(mBlob, NULL);
			return true;
		} catch (...) {
			return false;
		}
	}
}


//
// Lock this database
//
void Database::lock()
{
    common->lock(false);
}


//
// Lock all databases we know of.
// This is an interim stop-gap measure, until we can work out how database
// state should interact with true multi-session operation.
//
void Database::lockAllDatabases(CommonMap &commons, bool forSleep)
{
    StLock<Mutex> _(commons);	// hold all changes to Common map
    for (CommonMap::iterator it = commons.begin(); it != commons.end(); it++)
        it->second->lock(true, forSleep);	// lock, already holding commonLock
}


//
// Given a Key for this database, encode it into a blob and return it.
//
KeyBlob *Database::encodeKey(const CssmKey &key, const CssmData &pubAcl, const CssmData &privAcl)
{
    unlock();
    
    // tell the cryptocore to form the key blob
    return common->encodeKeyCore(key, pubAcl, privAcl);
}


//
// Given a "blobbed" key for this database, decode it into its real
// key object and (re)populate its ACL.
//
void Database::decodeKey(KeyBlob *blob, CssmKey &key, void * &pubAcl, void * &privAcl)
{
    unlock();							// we need our keys

    common->decodeKeyCore(blob, key, pubAcl, privAcl);
    // memory protocol: pubAcl points into blob; privAcl was allocated
	
    activity();
}


//
// Modify database parameters
//
void Database::setParameters(const DBParameters &params)
{
	StLock<Mutex> _(*common);
    makeUnlocked();
	common->mParams = params;
    common->version++;		// invalidate old blobs
    activity();
	secdebug("SSdb", "%p common %p(%s) set params=(%ld,%d)",
		this, common, dbName(), params.idleTimeout, params.lockOnSleep);
}


//
// Retrieve database parameters
//
void Database::getParameters(DBParameters &params)
{
	StLock<Mutex> _(*common);
    makeUnlocked();
	params = common->mParams;
    //activity();		// getting parameters does not reset the idle timer
}


//
// Intercept ACL change requests and reset blob validity
//
void Database::instantiateAcl()
{
	StLock<Mutex> _(*common);
	makeUnlocked();
}

void Database::changedAcl()
{
	StLock<Mutex> _(*common);
	version = 0;
}

const Database *Database::relatedDatabase() const
{ return this; }


//
// Debugging support
//
#if defined(DEBUGDUMP)

void Database::debugDump(const char *msg)
{
    assert(common);
	const Signature &sig = common->identifier();
	uint32 sig4; memcpy(&sig4, sig.bytes, sizeof(sig4));
	Debug::dump("** %s(%8.8lx) common=%p(%ld) %s\n",
		common->dbName(), sig4, common, common->useCount, msg);
	if (isLocked())
		Debug::dump("  locked");
	else {
		Time::Absolute when = common->when();
		time_t whenTime = time_t(when);
		Debug::dump("  UNLOCKED(%24.24s/%.2g)", ctime(&whenTime),
            (when - Time::now()).seconds());
	}
	Debug::dump(" %s blobversion=%ld/%ld %svalidData",
		(common->isValid() ? "validkeys" : "!validkeys"),
		version, common->version,
		(mValidData ? "" : "!"));
	Debug::dump(" Params=(%ld %d)\n",
		common->mParams.idleTimeout, common->mParams.lockOnSleep);
}

#endif //DEBUGDUMP


//
// Database::Common basic features
//
Database::Common::Common(const DbIdentifier &id, CommonMap &commonPool)
: pool(commonPool), mIdentifier(id), sequence(0), useCount(0), version(1),
    mIsLocked(true), mValidParams(false)
{ }

Database::Common::~Common()
{
	// explicitly unschedule ourselves
	Server::active().clearTimer(this);
	pool.erase(identifier());
}


void Database::Common::makeNewSecrets()
{
	// we already have a master key (right?)
	assert(hasMaster());

	// tell crypto core to generate the use keys
	DatabaseCryptoCore::generateNewSecrets();
	
	// we're now officially "unlocked"; set the timer
	mIsLocked = false;
	activity();
}


void Database::discard(Common *common)
{
    // LOCKING: pool lock held, *common NOT held
    secdebug("SSdb", "discarding dbcommon %p (no users, locked)", common);
    delete common;
}


//
// All unlocking activity ultimately funnels through this method.
// This unlocks a Common using the secrets setup in its crypto core
// component, and performs all the housekeeping needed to represent
// the state change.
//
bool Database::Common::unlock(DbBlob *blob, void **privateAclBlob)
{
	try {
		// Tell the cryptocore to (try to) decode itself. This will fail
		// in an astonishing variety of ways if the passphrase is wrong.
		assert(hasMaster());
		decodeCore(blob, privateAclBlob);
		secdebug("SSdb", "%p unlock successful", this);
	} catch (...) {
		secdebug("SSdb", "%p unlock failed", this);
		return false;
	}
	
	// get the database parameters only if we haven't got them yet
	if (!mValidParams) {
		mParams = blob->params;
		n2hi(mParams.idleTimeout);
		mValidParams = true;	// sticky
	}
	
	// now successfully unlocked
	mIsLocked = false;
	
	// set timeout
	activity();
	
	// broadcast unlock notification
	KeychainNotifier::unlock(identifier());
    return true;
}


void Database::Common::lock(bool holdingCommonLock, bool forSleep)
{
    StLock<Mutex> locker(*this);
    if (!isLocked()) {
        if (forSleep && !mParams.lockOnSleep)
            return;	// it doesn't want to

        mIsLocked = true;
		DatabaseCryptoCore::invalidate();
        KeychainNotifier::lock(identifier());
		Server::active().clearTimer(this);
		
		// if no database refers to us now, we're history
        StLock<Mutex> _(pool, false);
        if (!holdingCommonLock)
            _.lock();
		if (useCount == 0) {
            locker.unlock();	// release object lock
			discard(this);
        }
    }
}

DbBlob *Database::Common::encode(Database &db)
{
    assert(!isLocked());	// must have been unlocked by caller
    
    // export database ACL to blob form
    CssmData pubAcl, privAcl;
    db.exportBlob(pubAcl, privAcl);
    
    // tell the cryptocore to form the blob
    DbBlob form;
    form.randomSignature = identifier();
    form.sequence = sequence;
    form.params = mParams;
	h2ni(form.params.idleTimeout);
	
	assert(hasMaster());
    DbBlob *blob = encodeCore(form, pubAcl, privAcl);
    
    // clean up and go
    db.allocator.free(pubAcl);
    db.allocator.free(privAcl);
	return blob;
}


//
// Perform deferred lock processing for a database.
//
void Database::Common::action()
{
	secdebug("SSdb", "common %s(%p) locked by timer (%d refs)",
		dbName(), this, int(useCount));
	lock(false);
}

void Database::Common::activity()
{
    if (!isLocked())
		Server::active().setTimer(this, Time::Interval(int(mParams.idleTimeout)));
}


//
// Implementation of a "system keychain unlock key store"
//
SystemKeychainKey::SystemKeychainKey(const char *path)
	: mPath(path)
{
	// explicitly set up a key header for a raw 3DES key
	CssmKey::Header &hdr = mKey.header();
	hdr.blobType(CSSM_KEYBLOB_RAW);
	hdr.blobFormat(CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING);
	hdr.keyClass(CSSM_KEYCLASS_SESSION_KEY);
	hdr.algorithm(CSSM_ALGID_3DES_3KEY_EDE);
	hdr.KeyAttr = 0;
	hdr.KeyUsage = CSSM_KEYUSE_ANY;
	mKey = CssmData::wrap(mBlob.masterKey);
}

SystemKeychainKey::~SystemKeychainKey()
{
}

bool SystemKeychainKey::matches(const DbBlob::Signature &signature)
{
	return update() && signature == mBlob.signature;
}

bool SystemKeychainKey::update()
{
	// if we checked recently, just assume it's okay
	if (mUpdateThreshold > Time::now())
		return mValid;
		
	// check the file
	struct stat st;
	if (::stat(mPath.c_str(), &st)) {
		// something wrong with the file; can't use it
		mUpdateThreshold = Time::now() + Time::Interval(checkDelay);
		return mValid = false;
	}
	if (mValid && Time::Absolute(st.st_mtimespec) == mCachedDate)
		return true;
	mUpdateThreshold = Time::now() + Time::Interval(checkDelay);
	
	try {
		secdebug("syskc", "reading system unlock record from %s", mPath.c_str());
		AutoFileDesc fd(mPath, O_RDONLY);
		if (fd.read(mBlob) != sizeof(mBlob))
			return false;
		if (mBlob.isValid()) {
			mCachedDate = st.st_mtimespec;
			return mValid = true;
		} else
			return mValid = false;
	} catch (...) {
		secdebug("syskc", "system unlock record not available");
		return false;
	}
}
