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
#include "cfnotifier.h"	// legacy
#include "notifications.h"
#include "SecurityAgentClient.h"
#include <Security/acl_any.h>	// for default owner ACLs


//
// The map of database common segments
//
Mutex Database::commonLock;
Database::CommonMap Database::commons;


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
	common = new Common(ident);
	StLock<Mutex> _(*common);
	{	StLock<Mutex> _(commonLock);
		assert(commons.find(ident) == commons.end());	// better be new!
		commons[ident] = common = new Common(ident);
		common->useCount++;
	}
	// new common is now visible but we hold its lock
	
	// obtain initial passphrase and generate keys
	common->mParams = params;
	common->setupKeys(cred);

	// establish initial ACL
	if (owner)
		cssmSetInitial(*owner);
	else
		cssmSetInitial(new AnyAclSubject());
    mValidData = true;
    
    // for now, create the blob immediately
    //@@@ this could be deferred, at the cost of some additional
    //@@@ state monitoring. What happens if it locks before we have a blob?
    encode();
    
    // register with process
    process.addDatabase(this);
    
	IFDEBUG(debug("SSdb", "database %s(%p) created, common at %p",
		common->dbName(), this, common));
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
    switch (blob->version) {
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
	StLock<Mutex> mapLock(commonLock);
    CommonMap::iterator it = commons.find(ident);
    if (it != commons.end()) {
        // already there
        common = it->second;				// reuse common component
        //@@@ arbitrate sequence number here, perhaps update common->mParams
		StLock<Mutex> _(*common);			// lock common against other users
		common->useCount++;
		IFDEBUG(debug("SSdb",
            "open database %s(%p) version %lx at known common %p(%d)",
			common->dbName(), this, blob->version, common, int(common->useCount)));
    } else {
        // newly introduced
        commons[ident] = common = new Common(ident);
		common->mParams = blob->params;
		common->useCount++;
		IFDEBUG(debug("SSdb", "open database %s(%p) version %lx with new common %p",
			common->dbName(), this, blob->version, common));
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
    IFDEBUG(debug("SSdb", "deleting database %s(%p) common %p (%d refs)",
        common->dbName(), this, common, int(common->useCount)));
    IFDUMPING("SSdb", debugDump("deleting database instance"));
    process.removeDatabase(this);
    CssmAllocator::standard().free(mCred);

    // take the commonLock to avoid races against re-use of the common
    StLock<Mutex> __(commonLock);
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
    CssmAllocator::standard().free(mCred);
    mCred = DataWalkers::copy(cred, CssmAllocator::standard());
}


//
// Encode the current database as a blob.
// Note that this returns memory we own and keep.
//
DbBlob *Database::encode()
{
	StLock<Mutex> _(*common);
    if (!validBlob()) {
        // unlock the database
        makeUnlocked();

        // create new up-to-date blob
        DbBlob *blob = common->encode(*this);
        CssmAllocator::standard().free(mBlob);
        mBlob = blob;
        version = common->version;
		debug("SSdb", "encoded database %p(%s) version %ld", this, dbName(), version);
    }
    activity();
	assert(mBlob);
    return mBlob;
}


//
// Change the passphrase on a database
//
void Database::changePassphrase(const AccessCredentials *cred)
{
	StLock<Mutex> _(*common);
	if (isLocked()) {
		CssmAutoData passphrase(CssmAllocator::standard(CssmAllocator::sensitive));
		if (getBatchPassphrase(cred, CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK, passphrase)) {
			// incoming sample contained data for unlock
			makeUnlocked(passphrase);
		} else {
			// perform standard unlock
			makeUnlocked();
		}
    } else if (!mValidData)     // need to decode to get our ACLs, passphrase available
        decode(common->passphrase);

    // get the new passphrase
	// @@@ unstaged version -- revise to filter passphrases
	Process &cltProc = Server::active().connection().process;
        IFDEBUG(debug("SSdb", "New passphrase query from PID %d (UID %d)", cltProc.pid(), cltProc.uid()));
	QueryNewPassphrase query(cltProc.uid(), cltProc.session, *common, SecurityAgent::changePassphrase);
        query(cred, common->passphrase);
	common->version++;	// blob state changed
	IFDEBUG(debug("SSdb", "Database %s(%p) passphrase changed", common->dbName(), this));
	
	// send out a notification
	KeychainNotifier::passphraseChanged(identifier());
    notify(passphraseChangedEvent);

    // I guess this counts as an activity
    activity();
}


//
// Unlock this database (if needed) by obtaining the passphrase in some
// suitable way and then proceeding to unlock with it. Performs retries
// where appropriate. Does absolutely nothing if the database is already unlocked.
//
void Database::unlock()
{
	StLock<Mutex> _(*common);
	makeUnlocked();
}
	
void Database::makeUnlocked()
{
    IFDUMPING("SSdb", debugDump("default procedures unlock"));
    if (isLocked()) {
        assert(mBlob || (mValidData && common->passphrase));

	Process &cltProc = Server::active().connection().process;
        IFDEBUG(debug("SSdb", "Unlock query from process %d (UID %d)", cltProc.pid(), cltProc.uid()));
	QueryUnlock query(cltProc.uid(), cltProc.session, *this);
		query(mCred);
		if (isLocked())		// still locked, unlock failed
			CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
			
		// successfully unlocked
		activity();				// set timeout timer
	} else if (!mValidData)		// need to decode to get our ACLs, passphrase available
		decode(common->passphrase);
}


//
// Perform programmatic unlock of a database, given a passphrase.
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
	} else if (!mValidData)
		decode(common->passphrase);
}


//
// Perform an actual unlock operation given a passphrase.
// Caller must hold common lock.
//
bool Database::decode(const CssmData &passphrase)
{
	if (mValidData && common->passphrase) {	// just check
		return common->unlock(passphrase);
	} else {	// decode our blob
		assert(mBlob);
		void *privateAclBlob;
		if (common->unlock(mBlob, passphrase, &privateAclBlob)) {
			if (!mValidData) {
				importBlob(mBlob->publicAclBlob(), privateAclBlob);
				mValidData = true;
			}
			CssmAllocator::standard().free(privateAclBlob);
			return true;
		}
	}
	return false;
}


//
// Verify a putative database passphrase.
// This requires that the database be already unlocked;
// it will not unlock the database (and will not lock it
// if the proffered phrase is wrong).
//
bool Database::validatePassphrase(const CssmData &passphrase) const
{
	assert(!isLocked());
	return passphrase == common->passphrase;
}


//
// Lock this database
//
void Database::lock()
{
    common->lock();
}


//
// Lock all databases we know of.
// This is an interim stop-gap measure, until we can work out how database
// state should interact with true multi-session operation.
//
void Database::lockAllDatabases(bool forSleep)
{
    StLock<Mutex> _(commonLock);	// hold all changes to Common map
    debug("SSdb", "locking all %d known databases", int(commons.size()));
    for (CommonMap::iterator it = commons.begin(); it != commons.end(); it++)
        it->second->lock(true, forSleep);	// lock, already holding commonLock
}


//
// Given a Key for this database, encode it into a blob and return it.
//
KeyBlob *Database::encodeKey(const CssmKey &key, const CssmData &pubAcl, const CssmData &privAcl)
{
    makeUnlocked();
    
    // tell the cryptocore to form the key blob
    return common->encodeKeyCore(key, pubAcl, privAcl);
}


//
// Given a "blobbed" key for this database, decode it into its real
// key object and (re)populate its ACL.
//
void Database::decodeKey(KeyBlob *blob, CssmKey &key,
    void * &pubAcl, void * &privAcl)
{
    makeUnlocked();							// we need our keys

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

void Database::noticeAclChange()
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
Database::Common::Common(const DbIdentifier &id)
: mIdentifier(id), sequence(0), passphrase(CssmAllocator::standard(CssmAllocator::sensitive)),
	useCount(0), version(1),
    mIsLocked(true)
{ }

Database::Common::~Common()
{
	// explicitly unschedule ourselves
	Server::active().clearTimer(this);
}


void Database::discard(Common *common)
{
    // LOCKING: commonLock held, *common NOT held
    debug("SSdb", "discarding dbcommon %p (no users, locked)", common);
	commons.erase(common->identifier());
    delete common;
}

bool Database::Common::unlock(DbBlob *blob, const CssmData &passphrase,
    void **privateAclBlob)
{
	try {
		// Tell the cryptocore to (try to) decode itself. This will fail
		// in an astonishing variety of ways if the passphrase is wrong.
		decodeCore(blob, passphrase, privateAclBlob);
	} catch (...) {
		//@@@ which errors should we let through? Any?
		return false;
	}

	// save the passphrase (we'll need it for database encoding)
	this->passphrase = passphrase;
	
	// retrieve some public arguments
	mParams = blob->params;
	
	// now successfully unlocked
	mIsLocked = false;
	
	// set timeout
	activity();
	
	// broadcast unlock notification
	KeychainNotifier::unlock(identifier());
    notify(unlockedEvent);
    return true;
}


//
// Fast-path unlock: secrets already valid; just check passphrase and approve.
//
bool Database::Common::unlock(const CssmData &passphrase)
{
    assert(isValid());
    if (isLocked()) {
        if (passphrase == this->passphrase) {
            mIsLocked = false;
			KeychainNotifier::unlock(identifier());
            notify(unlockedEvent);
            return true;	// okay
        } else
            return false;	// failed
    } else
        return true;		// was unlocked; no problem
}

void Database::Common::lock(bool holdingCommonLock, bool forSleep)
{
    StLock<Mutex> locker(*this);
    if (!isLocked()) {
        if (forSleep && !mParams.lockOnSleep)
            return;	// it doesn't want to

        //@@@ discard secrets here? That would make fast-path impossible.
        mIsLocked = true;
        KeychainNotifier::lock(identifier());
        notify(lockedEvent);
		
		// if no database refers to us now, we're history
        StLock<Mutex> _(commonLock, false);
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
    DbBlob *blob = encodeCore(form, passphrase, pubAcl, privAcl);
    
    // clean up and go
    db.allocator.free(pubAcl);
    db.allocator.free(privAcl);
	return blob;
}


//
// Send out database-related notifications
//
void Database::Common::notify(Listener::Event event)
{
    IFDEBUG(debug("SSdb", "common %s(%p) sending event %ld", dbName(), this, event));
    DLDbFlatIdentifier flatId(mIdentifier);	// walkable form of DLDbIdentifier
    CssmAutoData data(CssmAllocator::standard());
    copy(&flatId, CssmAllocator::standard(), data.get());
    Listener::notify(Listener::databaseNotifications, event, data);
}


//
// Initialize a (new) database's key information.
// This acquires the passphrase in the appropriate way.
// When (successfully) done, the database is in the unlocked state.
//
void Database::Common::setupKeys(const AccessCredentials *cred)
{
	// get the new passphrase
	// @@@ Un-staged version of the API - revise with acceptability tests
    Process &cltProc = Server::active().connection().process;
    IFDEBUG(debug("SSdb", "New passphrase request from process %d (UID %d)", cltProc.pid(), cltProc.uid()));
    QueryNewPassphrase query(cltProc.uid(), cltProc.session, *this, SecurityAgent::newDatabase);
	query(cred, passphrase);
		
	// we have the passphrase now
    generateNewSecrets();
	
	// we're unlocked now
	mIsLocked = false;
    activity();
}


//
// Perform deferred lock processing for a database.
//
void Database::Common::action()
{
	IFDEBUG(debug("SSdb", "common %s(%p) locked by timer (%d refs)",
		dbName(), this, int(useCount)));
	lock();
}

void Database::Common::activity()
{
    if (!isLocked())
		Server::active().setTimer(this, int(mParams.idleTimeout));
}
