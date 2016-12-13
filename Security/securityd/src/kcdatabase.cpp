/*
 * Copyright (c) 2000-2009,2012-2014 Apple Inc. All Rights Reserved.
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
// kcdatabase - software database container implementation.
//
// General implementation notes:
// This leverages LocalDatabase/LocalKey for cryptography, and adds the
//  storage coder/decoder logic that implements "keychain" databases in their
//  intricately choreographed dance between securityd and the AppleCSPDL.
// As always, Database objects are lifetime-bound to their Process referent;
//  they can also be destroyed explicitly with a client release call.
// DbCommons are reference-held by their Databases, with one extra special
//  reference (from the Session) introduced when the database unlocks, and
//  removed when it locks again. That way, an unused DbCommon dies when it
//  is locked or when the Session dies, whichever happens earlier.
// There is (as yet) no global-scope Database object for Keychain databases.
//
#include "kcdatabase.h"
#include "agentquery.h"
#include "kckey.h"
#include "server.h"
#include "session.h"
#include "notifications.h"
#include <vector>           // @@@  4003540 workaround
#include <security_cdsa_utilities/acl_any.h>	// for default owner ACLs
#include <security_cdsa_utilities/cssmendian.h>
#include <security_cdsa_client/wrapkey.h>
#include <security_cdsa_client/genkey.h>
#include <security_cdsa_client/signclient.h>
#include <security_cdsa_client/cryptoclient.h>
#include <security_cdsa_client/macclient.h>
#include <securityd_client/dictionary.h>
#include <security_utilities/endian.h>
#include "securityd_service/securityd_service/securityd_service_client.h"
#include <AssertMacros.h>
#include <syslog.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/csr.h>

void unflattenKey(const CssmData &flatKey, CssmKey &rawKey);	//>> make static method on KeychainDatabase

//
// Static members
//
KeychainDbCommon::CommonSet KeychainDbCommon::mCommonSet;
ReadWriteLock KeychainDbCommon::mRWCommonLock;

// Process is using a cached effective uid, login window switches uid after the intial connection
static void get_process_euid(pid_t pid, uid_t * out_euid)
{
    if (!out_euid) return;

    struct kinfo_proc proc_info = {};
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};
    size_t len = sizeof(struct kinfo_proc);
    int ret;
    ret = sysctl(mib, (sizeof(mib)/sizeof(int)), &proc_info, &len, NULL, 0);

    // don't allow root
    if ((ret == 0) && (proc_info.kp_eproc.e_ucred.cr_uid != 0)) {
        *out_euid = proc_info.kp_eproc.e_ucred.cr_uid;
    }
}

static int
unlock_keybag(KeychainDatabase & db, const void * secret, int secret_len)
{
    int rc = -1;

    if (!db.common().isLoginKeychain()) return 0;

    service_context_t context = db.common().session().get_current_service_context();

    // login window attempts to change the password before a session has a uid
    if (context.s_uid == AU_DEFAUDITID) {
        get_process_euid(db.process().pid(), &context.s_uid);
    }

    // try to unlock first if not found then load/create or unlock
    // loading should happen when the kb common object is created
    // if it doesn't exist yet then the unlock will fail and we'll create everything
    rc = service_client_kb_unlock(&context, secret, secret_len);
    if (rc == KB_BagNotLoaded) {
        if (service_client_kb_load(&context) == KB_BagNotFound) {
            rc = service_client_kb_create(&context, secret, secret_len);
        } else {
            rc = service_client_kb_unlock(&context, secret, secret_len);
        }
    }

    if (rc != 0) { // if we just upgraded make sure we swap the encryption key to the password
        if (!db.common().session().keybagGetState(session_keybag_check_master_key)) {
            CssmAutoData encKey(Allocator::standard(Allocator::sensitive));
            db.common().get_encryption_key(encKey);
            if ((rc = service_client_kb_unlock(&context, encKey.data(), (int)encKey.length())) == 0) {
                rc = service_client_kb_change_secret(&context, encKey.data(), (int)encKey.length(), secret, secret_len);
            }

            if (rc != 0) { // if a login.keychain password exists but doesnt on the keybag update it
                bool no_pin = false;
                if ((secret_len > 0) && service_client_kb_is_locked(&context, NULL, &no_pin) == 0) {
                    if (no_pin) {
                        syslog(LOG_ERR, "Updating iCloud keychain passphrase for uid %d", context.s_uid);
                        service_client_kb_change_secret(&context, NULL, 0, secret, secret_len);
                    }
                }
            }
        } // session_keybag_check_master_key
    }

    if (rc == 0) {
        db.common().session().keybagSetState(session_keybag_unlocked|session_keybag_loaded|session_keybag_check_master_key);
    } else {
        syslog(LOG_ERR, "Failed to unlock iCloud keychain for uid %d", context.s_uid);
    }

    return rc;
}

static void
change_secret_on_keybag(KeychainDatabase & db, const void * secret, int secret_len, const void * new_secret, int new_secret_len)
{
    if (!db.common().isLoginKeychain()) return;

    service_context_t context = db.common().session().get_current_service_context();

    // login window attempts to change the password before a session has a uid
    if (context.s_uid == AU_DEFAUDITID) {
        get_process_euid(db.process().pid(), &context.s_uid);
    }

    // if a login.keychain doesn't exist yet it comes into securityd as a create then change_secret
    // we need to create the keybag in this case if it doesn't exist
    int rc = service_client_kb_change_secret(&context, secret, secret_len, new_secret, new_secret_len);
    if (rc == KB_BagNotLoaded) {
        if (service_client_kb_load(&context) == KB_BagNotFound) {
            rc = service_client_kb_create(&context, new_secret, new_secret_len);
        } else {
            rc = service_client_kb_change_secret(&context, secret, secret_len, new_secret, new_secret_len);
        }
    }

    // this makes it possible to restore a deleted keybag on condition it still exists in kernel
    if (rc != KB_Success) {
        service_client_kb_save(&context);
    }

    // if for some reason we are locked lets unlock so later we don't try and throw up SecurityAgent dialog
    bool locked = false;
    if ((service_client_kb_is_locked(&context, &locked, NULL) == KB_Success) && locked) {
        rc = service_client_kb_unlock(&context, new_secret, new_secret_len);
        if (rc != KB_Success) {
            syslog(LOG_ERR, "Failed to unlock iCloud keychain for uid %d (%d)", context.s_uid, (int)rc);
        }
    }
}

// Attempt to unlock the keybag with a AccessCredentials password.
// Honors UI disabled flags from clients set in the cred before prompt.
static bool
unlock_keybag_with_cred(KeychainDatabase &db, const AccessCredentials *cred){
    list<CssmSample> samples;
    if (cred && cred->samples().collect(CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK, samples)) {
        for (list<CssmSample>::iterator it = samples.begin(); it != samples.end(); it++) {
            TypedList &sample = *it;
            sample.checkProper();
            switch (sample.type()) {
                // interactively prompt the user - no additional data
                case CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT: {
                    StSyncLock<Mutex, Mutex> uisync(db.common().uiLock(), db.common());
                    // Once we get the ui lock, check whether another thread has already unlocked keybag
                    bool locked = false;
                    service_context_t context = db.common().session().get_current_service_context();
                    if ((service_client_kb_is_locked(&context, &locked, NULL) == 0) && locked) {
                        QueryKeybagPassphrase keybagQuery(db.common().session(), 3);
                        keybagQuery.inferHints(Server::process());
                        if (keybagQuery.query() == SecurityAgent::noReason) {
                            return true;
                        }
                    }
                    else {
                        // another thread already unlocked the keybag
                        return true;
                    }
                    break;
                }
                // try to use an explicitly given passphrase - Data:passphrase
                case CSSM_SAMPLE_TYPE_PASSWORD: {
                    if (sample.length() != 2)
                        CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
                    secinfo("KCdb", "attempting passphrase unlock of keybag");
                    if (unlock_keybag(db, sample[1].data().data(), (int)sample[1].data().length())) {
                        return true;
                    }
                    break;
                }
                default: {
                    // Unknown sub-sample for unlocking.
                    secinfo("KCdb", "keybag: unknown sub-sample unlock (%d) ignored", sample.type());
                    break;
                }
            }
        }
    }
    return false;
}

//
// Create a Database object from initial parameters (create operation)
//
KeychainDatabase::KeychainDatabase(const DLDbIdentifier &id, const DBParameters &params, Process &proc,
            const AccessCredentials *cred, const AclEntryPrototype *owner)
    : LocalDatabase(proc), mValidData(false), mSecret(Allocator::standard(Allocator::sensitive)), mSaveSecret(false), version(0), mBlob(NULL), mRecoded(false)
{
    // save a copy of the credentials for later access control
    mCred = DataWalkers::copy(cred, Allocator::standard());

    // create a new random signature to complete the DLDbIdentifier
    DbBlob::Signature newSig;
    Server::active().random(newSig.bytes);
    DbIdentifier ident(id, newSig);
	
    // create common block and initialize
    // Since this is a creation step, figure out the correct blob version for this database
    RefPointer<KeychainDbCommon> newCommon = new KeychainDbCommon(proc.session(), ident, CommonBlob::getCurrentVersionForDb(ident.dbName()));
    newCommon->initializeKeybag();

	StLock<Mutex> _(*newCommon);
	parent(*newCommon);
	newCommon->insert();
	// new common is now visible (in ident-map) but we hold its lock

	// establish the new master secret
	establishNewSecrets(cred, SecurityAgent::newDatabase);
	
	// set initial database parameters
	common().mParams = params;
		
	// the common is "unlocked" now
	common().makeNewSecrets();

	// establish initial ACL
	if (owner)
		acl().cssmSetInitial(*owner);
	else
		acl().cssmSetInitial(new AnyAclSubject());
    mValidData = true;
    
    // for now, create the blob immediately
    encode();
    
	proc.addReference(*this);
	
	// this new keychain is unlocked; make it so
	activity();
	
    secinfo("KCdb", "creating keychain %p %s with common %p", this, (char*)this->dbName(), &common());
}


//
// Create a Database object from a database blob (decoding)
//
KeychainDatabase::KeychainDatabase(const DLDbIdentifier &id, const DbBlob *blob, Process &proc,
    const AccessCredentials *cred)
	: LocalDatabase(proc), mValidData(false), mSecret(Allocator::standard(Allocator::sensitive)), mSaveSecret(false), version(0), mBlob(NULL), mRecoded(false)
{
	validateBlob(blob);

    // save a copy of the credentials for later access control
    mCred = DataWalkers::copy(cred, Allocator::standard());
    mBlob = blob->copy();
    
    // check to see if we already know about this database
    DbIdentifier ident(id, blob->randomSignature);
	Session &session = process().session();
	RefPointer<KeychainDbCommon> com;
    secinfo("kccommon", "looking for a common at %s", ident.dbName());
	if (KeychainDbCommon::find(ident, session, com)) {
		parent(*com);
        secinfo("KCdb", "joining keychain %p %s with common %p", this, (char*)this->dbName(), &common());
	} else {
		// DbCommon not present; make a new one
        secinfo("kccommon", "no common found");
		parent(*com);
		common().mParams = blob->params;
        secinfo("KCdb", "making keychain %p %s with common %p", this, (char*)this->dbName(), &common());
		// this DbCommon is locked; no timer or reference setting
	}
	proc.addReference(*this);
}

void KeychainDbCommon::insert()
{
    StReadWriteLock _(mRWCommonLock, StReadWriteLock::Write);
    insertHoldingLock();
}

void KeychainDbCommon::insertHoldingLock()
{
    mCommonSet.insert(this);
}



// find or make a DbCommon. Returns true if an existing one was found and used.
bool KeychainDbCommon::find(const DbIdentifier &ident, Session &session, RefPointer<KeychainDbCommon> &common, uint32 requestedVersion, KeychainDbCommon* cloneFrom)
{
    // Prepare to drop the mRWCommonLock.
    {
        StReadWriteLock _(mRWCommonLock, StReadWriteLock::Read);
        for (CommonSet::const_iterator it = mCommonSet.begin(); it != mCommonSet.end(); ++it) {
            if (&session == &(*it)->session() && ident == (*it)->identifier()) {
                common = *it;
                secinfo("kccommon", "found a common for %s at %p", ident.dbName(), common.get());
                return true;
            }
        }
    }

    // not found. Grab the write lock, ensure that nobody has beaten us to adding,
    // and then create a DbCommon and add it to the map.
    {
        StReadWriteLock _(mRWCommonLock, StReadWriteLock::Write);
        for (CommonSet::const_iterator it = mCommonSet.begin(); it != mCommonSet.end(); ++it) {
            if (&session == &(*it)->session() && ident == (*it)->identifier()) {
                common = *it;
                secinfo("kccommon", "found a common for %s at %p", ident.dbName(), common.get());
                return true;
            }
        }

        // not found
        if(cloneFrom) {
            common = new KeychainDbCommon(session, ident, *cloneFrom);
        } else if(requestedVersion != CommonBlob::version_none) {
            common = new KeychainDbCommon(session, ident, requestedVersion);
        } else {
            common = new KeychainDbCommon(session, ident);
        }

        secinfo("kccommon", "made a new common for %s at %p", ident.dbName(), common.get());

        // Can't call insert() here, because it grabs the write lock (which we have).
        common->insertHoldingLock();
    }
    common->initializeKeybag();
    return false;
}

// recode/clone:
//
// Special-purpose constructor for keychain synchronization.  Copies an
// existing keychain but uses the operational keys from secretsBlob.  The 
// new KeychainDatabase will silently replace the existing KeychainDatabase
// as soon as the client declares that re-encoding of all keychain items is
// finished.  This is a little perilous since it allows a client to dictate
// securityd state, but we try to ensure that only the client that started 
// the re-encoding can declare it done.  
//
KeychainDatabase::KeychainDatabase(KeychainDatabase &src, Process &proc, DbHandle dbToClone)
	: LocalDatabase(proc), mValidData(false), mSecret(Allocator::standard(Allocator::sensitive)), mSaveSecret(false), version(0), mBlob(NULL), mRecoded(false)
{
	mCred = DataWalkers::copy(src.mCred, Allocator::standard());

	// Give this KeychainDatabase a temporary name
	std::string newDbName = std::string("////") + std::string(src.identifier().dbName());
	DLDbIdentifier newDLDbIdent(src.identifier().dlDbIdentifier().ssuid(), newDbName.c_str(), src.identifier().dlDbIdentifier().dbLocation());
	DbIdentifier ident(newDLDbIdent, src.identifier());

    // create common block and initialize
	RefPointer<KeychainDbCommon> newCommon = new KeychainDbCommon(proc.session(), ident);
    newCommon->initializeKeybag();
	StLock<Mutex> _(*newCommon);
	parent(*newCommon);
	newCommon->insert();

	// set initial database parameters from the source keychain
	common().mParams = src.common().mParams;
	
	// establish the source keychain's master secret as ours
	// @@@  NB: this is a v. 0.1 assumption.  We *should* trigger new UI 
	//      that offers the user the option of using the existing password 
	//      or choosing a new one.  That would require a new 
	//      SecurityAgentQuery type, new UI, and--possibly--modifications to
	//      ensure that the new password is available here to generate the 
	//      new master secret.  
	src.unlockDb(false);		// precaution for masterKey()
	common().setup(src.blob(), src.common().masterKey());
	
    // import the operational secrets
	RefPointer<KeychainDatabase> srcKC = Server::keychain(dbToClone);
	common().importSecrets(srcKC->common());
	
	// import source keychain's ACL  
	CssmData pubAcl, privAcl;
	src.acl().exportBlob(pubAcl, privAcl);
	importBlob(pubAcl.data(), privAcl.data());
	src.acl().allocator.free(pubAcl);
	src.acl().allocator.free(privAcl);
	
	// indicate that this keychain should be allowed to do some otherwise
	// risky things required for copying, like re-encoding keys
	mRecodingSource = &src;
	
	common().setUnlocked();
	mValidData = true;
	
    encode();

	proc.addReference(*this);
	secinfo("SSdb", "database %s(%p) created as copy, common at %p",
			 common().dbName(), this, &common());
}

// Make a new KeychainDatabase from an old one, but have a completely different location
KeychainDatabase::KeychainDatabase(const DLDbIdentifier& id, KeychainDatabase &src, Process &proc)
: LocalDatabase(proc), mValidData(false), mSecret(Allocator::standard(Allocator::sensitive)), mSaveSecret(false), version(0), mBlob(NULL), mRecoded(false)
{
    mCred = DataWalkers::copy(src.mCred, Allocator::standard());

    DbIdentifier ident(id, src.identifier());

    // create common block and initialize
    RefPointer<KeychainDbCommon> newCommon;
    if(KeychainDbCommon::find(ident, process().session(), newCommon, CommonBlob::version_none, &src.common())) {
        // A common already existed. Write over it, but note that everything may go horribly from here on out.
        secinfo("kccommon", "Found common where we didn't expect. Possible strange behavior ahead.");
        newCommon->cloneFrom(src.common());
    }

    StLock<Mutex> _(*newCommon);
    parent(*newCommon);

    // set initial database parameters from the source keychain
    common().mParams = src.common().mParams;

    // import source keychain's ACL
    CssmData pubAcl, privAcl;
    src.acl().exportBlob(pubAcl, privAcl);
    importBlob(pubAcl.data(), privAcl.data());
    src.acl().allocator.free(pubAcl);
    src.acl().allocator.free(privAcl);

    // Copy the source database's blob, if possible
    if(src.mBlob) {
        mBlob = src.mBlob->copy();
        version = src.version;
    }

    // We've copied everything we can from our source. If they were valid, so are we.
    mValidData = src.mValidData;

    proc.addReference(*this);
    secinfo("SSdb", "database %s(%p) created as expected clone, common at %p", common().dbName(), this, &common());
}


// Make a new KeychainDatabase from an old one, but have entirely new operational secrets
KeychainDatabase::KeychainDatabase(uint32 requestedVersion, KeychainDatabase &src, Process &proc)
: LocalDatabase(proc), mValidData(false), mSecret(Allocator::standard(Allocator::sensitive)), mSaveSecret(false), version(0), mBlob(NULL), mRecoded(false)
{
    mCred = DataWalkers::copy(src.mCred, Allocator::standard());

    // Give this KeychainDatabase a temporary name
    // this must canonicalize to a different path than the original DB, otherwise another process opening the existing DB wil find this new KeychainDbCommon
    // and call decodeCore with the old blob, overwriting the new secrets and wreaking havoc
    std::string newDbName = std::string("////") + std::string(src.identifier().dbName()) + std::string("_com.apple.security.keychain.migrating");
    DLDbIdentifier newDLDbIdent(src.identifier().dlDbIdentifier().ssuid(), newDbName.c_str(), src.identifier().dlDbIdentifier().dbLocation());
    DbIdentifier ident(newDLDbIdent, src.identifier());

    // hold the lock for src's common during this operation (to match locking common locking order with KeychainDatabase::recodeKey)
    StLock<Mutex> __(src.common());

    // create common block and initialize
    RefPointer<KeychainDbCommon> newCommon;
    if(KeychainDbCommon::find(ident, process().session(), newCommon, requestedVersion)) {
        // A common already existed here. Write over it, but note that everything may go horribly from here on out.
        secinfo("kccommon", "Found common where we didn't expect. Possible strange behavior ahead.");
        newCommon->cloneFrom(src.common(), requestedVersion);
    }
    newCommon->initializeKeybag();
    StLock<Mutex> _(*newCommon);
    parent(*newCommon);

    // We want to re-use the master secrets from the source database (and so the
    // same password), but reroll new operational secrets.

    // Copy the master secret over...
    src.unlockDb(false); // precaution

    common().setup(src.blob(), src.common().masterKey(), false); // keep the new common's version intact

    // set initial database parameters from the source keychain
    common().mParams = src.common().mParams;

    // and make new operational secrets
    common().makeNewSecrets();

    // import source keychain's ACL
    CssmData pubAcl, privAcl;
    src.acl().exportBlob(pubAcl, privAcl);
    importBlob(pubAcl.data(), privAcl.data());
    src.acl().allocator.free(pubAcl);
    src.acl().allocator.free(privAcl);

    // indicate that this keychain should be allowed to do some otherwise
    // risky things required for copying, like re-encoding keys
    mRecodingSource = &src;

    common().setUnlocked();
    mValidData = true;

    encode();

    proc.addReference(*this);
    secinfo("SSdb", "database %s(%p) created as expected copy, common at %p",
             common().dbName(), this, &common());
}

//
// Destroy a Database
//
KeychainDatabase::~KeychainDatabase()
{
    secinfo("KCdb", "deleting database %s(%p) common %p",
        common().dbName(), this, &common());
    Allocator::standard().free(mCred);
	Allocator::standard().free(mBlob);
}


//
// Basic Database virtual implementations
//
KeychainDbCommon &KeychainDatabase::common() const
{
	return parent<KeychainDbCommon>();
}

const char *KeychainDatabase::dbName() const
{
	return common().dbName();
}

bool KeychainDatabase::transient() const
{
	return false;	// has permanent store
}

AclKind KeychainDatabase::aclKind() const
{
	return dbAcl;
}

Database *KeychainDatabase::relatedDatabase()
{
	return this;
}

//
// (Re-)Authenticate the database. This changes the stored credentials.
//
void KeychainDatabase::authenticate(CSSM_DB_ACCESS_TYPE mode,
	const AccessCredentials *cred)
{
	StLock<Mutex> _(common());
	
	// the (Apple specific) RESET bit means "lock the database now"
	switch (mode) {
	case CSSM_DB_ACCESS_RESET:
		secinfo("KCdb", "%p ACCESS_RESET triggers keychain lock", this);
		common().lockDb();
		break;
	default:
		//  store the new credentials for future use
		secinfo("KCdb", "%p authenticate stores new database credentials", this);
		AccessCredentials *newCred = DataWalkers::copy(cred, Allocator::standard());
		Allocator::standard().free(mCred);
		mCred = newCred;
	}
}


//
// Make a new KeychainKey.
// If PERMANENT is off, make a temporary key instead.
// The db argument allows you to create for another KeychainDatabase (only);
// it defaults to ourselves.
//
RefPointer<Key> KeychainDatabase::makeKey(Database &db, const CssmKey &newKey,
	uint32 moreAttributes, const AclEntryPrototype *owner)
{

	if (moreAttributes & CSSM_KEYATTR_PERMANENT)
		return new KeychainKey(db, newKey, moreAttributes, owner);
	else
		return process().makeTemporaryKey(newKey, moreAttributes, owner);
}

RefPointer<Key> KeychainDatabase::makeKey(const CssmKey &newKey,
	uint32 moreAttributes, const AclEntryPrototype *owner)
{
	return makeKey(*this, newKey, moreAttributes, owner);
}


//
// Return the database blob, recalculating it as needed.
//
DbBlob *KeychainDatabase::blob()
{
	StLock<Mutex> _(common());
    if (!validBlob()) {
        makeUnlocked(false);	// unlock to get master secret
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
void KeychainDatabase::encode()
{
	DbBlob *blob = common().encode(*this);
	Allocator::standard().free(mBlob);
	mBlob = blob;
	version = common().version;
	secinfo("KCdb", "encoded database %p common %p(%s) version %u params=(%u,%u)",
		this, &common(), dbName(), version,
		common().mParams.idleTimeout, common().mParams.lockOnSleep);
}


//
// Change the passphrase on a database
//
void KeychainDatabase::changePassphrase(const AccessCredentials *cred)
{
	// get and hold the common lock (don't let other threads break in here)
	StLock<Mutex> _(common());
	
	// establish OLD secret - i.e. unlock the database
	//@@@ do we want to leave the final lock state alone?
    if (common().isLoginKeychain()) mSaveSecret = true;
	makeUnlocked(cred, false);
	
    // establish NEW secret
    if(!establishNewSecrets(cred, SecurityAgent::changePassphrase)) {
        secinfo("KCdb", "Old and new passphrases are the same. Database %s(%p) master secret did not change.",
                 common().dbName(), this);
        return;
    }
    if (mSecret) { mSecret.reset(); }
    mSaveSecret = false;
	common().invalidateBlob();	// blob state changed
	secinfo("KCdb", "Database %s(%p) master secret changed", common().dbName(), this);
	encode();			// force rebuild of local blob
	
	// send out a notification
	notify(kNotificationEventPassphraseChanged);

    // I guess this counts as an activity
    activity();
}

//
// Second stage of keychain synchronization: overwrite the original keychain's
// (this KeychainDatabase's) operational secrets
//
void KeychainDatabase::commitSecretsForSync(KeychainDatabase &cloneDb)
{
    StLock<Mutex> _(common());
    
	// try to detect spoofing
	if (cloneDb.mRecodingSource != this) 
        CssmError::throwMe(CSSM_ERRCODE_INVALID_DB_HANDLE);
	
    // in case we autolocked since starting the sync
    makeUnlocked(false); // call this because we already own the lock
	cloneDb.unlockDb(false); // we may not own the lock here, so calling unlockDb will lock the cloneDb's common lock

    // Decode all keys whose handles refer to this on-disk keychain so that
    // if the holding client commits the key back to disk, it's encoded with
    // the new operational secrets.  The recoding client *must* hold a write
    // lock for the on-disk keychain from the moment it starts recoding key
    // items until after this call.  
    // 
	// @@@  This specific implementation is a workaround for 4003540.  
	std::vector<U32HandleObject::Handle> handleList;
	U32HandleObject::findAllRefs<KeychainKey>(handleList);
    size_t count = handleList.size();
	if (count > 0) {
        for (unsigned int n = 0; n < count; ++n) {
            RefPointer<KeychainKey> kckey = 
                U32HandleObject::findRefAndLock<KeychainKey>(handleList[n], CSSMERR_CSP_INVALID_KEY_REFERENCE);
            StLock<Mutex> _(*kckey/*, true*/);
            if (kckey->database().global().identifier() == identifier()) {
                kckey->key();               // force decode
                kckey->invalidateBlob();
				secinfo("kcrecode", "changed extant key %p (proc %d)",
						 &*kckey, kckey->process().pid());
            }
        }
	}

    // mark down that we just recoded
    mRecoded = true;

    // it is now safe to replace the old op secrets
    common().importSecrets(cloneDb.common());
	common().invalidateBlob();
}


//
// Extract the database master key as a proper Key object.
//
RefPointer<Key> KeychainDatabase::extractMasterKey(Database &db,
	const AccessCredentials *cred, const AclEntryPrototype *owner,
	uint32 usage, uint32 attrs)
{
	// get and hold common lock
	StLock<Mutex> _(common());
	
	// force lock to require re-validation of credentials
	lockDb();
	
	// unlock to establish master secret
	makeUnlocked(false);
	
	// extract the raw cryptographic key
	CssmClient::WrapKey wrap(Server::csp(), CSSM_ALGID_NONE);
	CssmKey key;
	wrap(common().masterKey(), key);
	
	// make the key object and return it
	return makeKey(db, key, attrs & LocalKey::managedAttributes, owner);
}


//
// Unlock this database (if needed) by obtaining the master secret in some
// suitable way and then proceeding to unlock with it.
// Does absolutely nothing if the database is already unlocked.
// The makeUnlocked forms are identical except the assume the caller already
// holds the common lock.
//
void KeychainDatabase::unlockDb(bool unlockKeybag)
{
	StLock<Mutex> _(common());
	makeUnlocked(unlockKeybag);
}

void KeychainDatabase::makeUnlocked(bool unlockKeybag)
{
	return makeUnlocked(mCred, unlockKeybag);
}

void KeychainDatabase::makeUnlocked(const AccessCredentials *cred, bool unlockKeybag)
{
    if (isLocked()) {
		secnotice("KCdb", "%p(%p) unlocking for makeUnlocked()", this, &common());
        assert(mBlob || (mValidData && common().hasMaster()));
		establishOldSecrets(cred);
		common().setUnlocked(); // mark unlocked
        if (common().isLoginKeychain()) {
            CssmKey master = common().masterKey();
            CssmKey rawMaster;
            CssmClient::WrapKey wrap(Server::csp(), CSSM_ALGID_NONE);
            wrap(master, rawMaster);
            
            service_context_t context = common().session().get_current_service_context();
            service_client_stash_load_key(&context, rawMaster.keyData(), (int)rawMaster.length());
        }
	} else if (unlockKeybag && common().isLoginKeychain()) {
        bool locked = false;
        service_context_t context = common().session().get_current_service_context();
        if ((service_client_kb_is_locked(&context, &locked, NULL) == 0) && locked) {
            if (!unlock_keybag_with_cred(*this, cred)) {
                syslog(LOG_NOTICE, "failed to unlock iCloud keychain");
            }
        }
    }
	if (!mValidData) {	// need to decode to get our ACLs, master secret available
		secnotice("KCdb", "%p(%p) is unlocked; decoding for makeUnlocked()", this, &common());
		if (!decode())
			CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
	}
	assert(!isLocked());
	assert(mValidData);
}

//
// Invoke the securityd_service to retrieve the keychain master
// key from the AppleFDEKeyStore.
//
void KeychainDatabase::stashDbCheck()
{    
    CssmAutoData masterKey(Allocator::standard(Allocator::sensitive));
    CssmAutoData encKey(Allocator::standard(Allocator::sensitive));

    // Fetch the key
    int rc = 0;
    void * stash_key = NULL;
    int stash_key_len = 0;
    service_context_t context = common().session().get_current_service_context();
    rc = service_client_stash_get_key(&context, &stash_key, &stash_key_len);
    if (rc == 0) {
        if (stash_key) {
            masterKey.copy(CssmData((void *)stash_key,stash_key_len));
            memset(stash_key, 0, stash_key_len);
            free(stash_key);
        }
    } else {
        CssmError::throwMe(rc);
    }
    
    {
        StLock<Mutex> _(common());

        // Now establish it as the keychain master key
        CssmClient::Key key(Server::csp(), masterKey.get());
        CssmKey::Header &hdr = key.header();
        hdr.keyClass(CSSM_KEYCLASS_SESSION_KEY);
        hdr.algorithm(CSSM_ALGID_3DES_3KEY_EDE);
        hdr.usage(CSSM_KEYUSE_ANY);
        hdr.blobType(CSSM_KEYBLOB_RAW);
        hdr.blobFormat(CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING);
        common().setup(mBlob, key);

        if (!decode())
            CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);

        common().get_encryption_key(encKey);
    }

    // when upgrading from pre-10.9 create a keybag if it doesn't exist with the encryption key
    // only do this after we have verified the master key unlocks the login.keychain
    if (service_client_kb_load(&context) == KB_BagNotFound) {
        service_client_kb_create(&context, encKey.data(), (int)encKey.length());
    }
}

//
// Get the keychain master key and invoke the securityd_service
// to stash it in the AppleFDEKeyStore ready for commit to the
// NVRAM blob.
//
void KeychainDatabase::stashDb()
{
    CssmAutoData data(Allocator::standard(Allocator::sensitive));
    
    {
        StLock<Mutex> _(common());

        if (!common().isValid()) {
            CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
        }
        
        CssmKey key = common().masterKey();
        data.copy(key.keyData());
    }
    
    service_context_t context = common().session().get_current_service_context();
    int rc = service_client_stash_set_key(&context, data.data(), (int)data.length());
    if (rc != 0) CssmError::throwMe(rc);
}

//
// The following unlock given an explicit passphrase, rather than using
// (special cred sample based) default procedures.
//
void KeychainDatabase::unlockDb(const CssmData &passphrase, bool unlockKeybag)
{
	StLock<Mutex> _(common());
	makeUnlocked(passphrase, unlockKeybag);
}

void KeychainDatabase::makeUnlocked(const CssmData &passphrase, bool unlockKeybag)
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

    if (unlockKeybag && common().isLoginKeychain()) {
        bool locked = false;
        service_context_t context = common().session().get_current_service_context();
        if (!common().session().keybagGetState(session_keybag_check_master_key) || ((service_client_kb_is_locked(&context, &locked, NULL) == 0) && locked)) {
            unlock_keybag(*this, passphrase.data(), (int)passphrase.length());
        }
    }

	assert(!isLocked());
	assert(mValidData);
}


//
// Nonthrowing passphrase-based unlock. This returns false if unlock failed.
// Note that this requires an explicitly given passphrase.
// Caller must hold common lock.
//
bool KeychainDatabase::decode(const CssmData &passphrase)
{
	assert(mBlob);
	common().setup(mBlob, passphrase);
	bool success = decode();
    if (success && common().isLoginKeychain()) {
        unlock_keybag(*this, passphrase.data(), (int)passphrase.length());
    }
    return success;
}


//
// Given the established master secret, decode the working keys and other
// functional secrets for this database. Return false (do NOT throw) if
// the decode fails. Call this in low(er) level code once you established
// the master key.
//
bool KeychainDatabase::decode()
{
	assert(mBlob);
	assert(common().hasMaster());
	void *privateAclBlob;
	if (common().unlockDb(mBlob, &privateAclBlob)) {
		if (!mValidData) {
			acl().importBlob(mBlob->publicAclBlob(), privateAclBlob);
			mValidData = true;
		}
		Allocator::standard().free(privateAclBlob);
		return true;
	}
	secinfo("KCdb", "%p decode failed", this);
	return false;
}


//
// Given an AccessCredentials for this database, wring out the existing primary
// database secret by whatever means necessary.
// On entry, caller must hold the database common lock. It will be held
// throughout except when user interaction is required. User interaction 
// requires relinquishing the database common lock and taking the UI lock. On
// return from user interaction, the UI lock is relinquished and the database
// common lock must be reacquired. At no time may the caller hold both locks.
// On exit, the crypto core has its master secret. If things go wrong,
// we will throw a suitable exception. Note that encountering any malformed
// credential sample will throw, but this is not guaranteed -- don't assume
// that NOT throwing means creds is entirely well-formed (it may just be good
// enough to work THIS time).
//
// How this works:
// Walk through the creds. Fish out those credentials (in order) that
// are for unlock processing (they have no ACL subject correspondents),
// and (try to) obey each in turn, until one produces a valid secret
// or you run out. If no special samples are found at all, interpret that as
// "use the system global default," which happens to be hard-coded right here.
//
void KeychainDatabase::establishOldSecrets(const AccessCredentials *creds)
{
	bool forSystem = this->belongsToSystem();	// this keychain belongs to the system security domain

	// attempt system-keychain unlock
	if (forSystem) {
		SystemKeychainKey systemKeychain(kSystemUnlockFile);
		if (systemKeychain.matches(mBlob->randomSignature)) {
			secinfo("KCdb", "%p attempting system unlock", this);
			common().setup(mBlob, CssmClient::Key(Server::csp(), systemKeychain.key(), true));
			if (decode())
				return;
		}
	}
    
	list<CssmSample> samples;
	if (creds && creds->samples().collect(CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK, samples)) {
		for (list<CssmSample>::iterator it = samples.begin(); it != samples.end(); it++) {
			TypedList &sample = *it;
			sample.checkProper();
			switch (sample.type()) {
			// interactively prompt the user - no additional data
			case CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT:
				if (!forSystem) {
					if (interactiveUnlock())
						return;
				}
                break;
			// try to use an explicitly given passphrase - Data:passphrase
			case CSSM_SAMPLE_TYPE_PASSWORD:
				if (sample.length() != 2)
					CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
				secinfo("KCdb", "%p attempting passphrase unlock", this);
				if (decode(sample[1]))
					return;
				break;
			// try to open with a given master key - Data:CSP or KeyHandle, Data:CssmKey
			case CSSM_SAMPLE_TYPE_SYMMETRIC_KEY:
			case CSSM_SAMPLE_TYPE_ASYMMETRIC_KEY:
				assert(mBlob);
				secinfo("KCdb", "%p attempting explicit key unlock", this);
				common().setup(mBlob, keyFromCreds(sample, 4));
                if (decode()) {
					return;
                }
				break;
			// explicitly defeat the default action but don't try anything in particular
			case CSSM_WORDID_CANCELED:
				secinfo("KCdb", "%p defeat default action", this);
				break;
			default:
				// Unknown sub-sample for unlocking.
				// If we wanted to be fascist, we could now do
				//  CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
				// But instead we try to be tolerant and continue on.
				// This DOES however count as an explicit attempt at specifying unlock,
				// so we will no longer try the default case below...
				secinfo("KCdb", "%p unknown sub-sample unlock (%d) ignored", this, sample.type());
				break;
			}
		}
	} else {
		// default action
		assert(mBlob);

		if (!forSystem) {
			if (interactiveUnlock())
				return;
		}
	}
	
	// out of options - no secret obtained
	CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
}

//
// This function is almost identical to establishOldSecrets, but:
//   1. It will never prompt the user; these credentials either work or they don't
//   2. It will not change the secrets of this database
//
// TODO: These two functions should probably be refactored to something nicer.
bool KeychainDatabase::checkCredentials(const AccessCredentials *creds) {

    list<CssmSample> samples;
    if (creds && creds->samples().collect(CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK, samples)) {
        for (list<CssmSample>::iterator it = samples.begin(); it != samples.end(); it++) {
            TypedList &sample = *it;
            sample.checkProper();
            switch (sample.type()) {
                // interactively prompt the user - no additional data
                case CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT:
                    // do nothing, because this function will never prompt the user
                    secinfo("integrity", "%p ignoring keychain prompt", this);
                    break;
                // try to use an explicitly given passphrase - Data:passphrase
                case CSSM_SAMPLE_TYPE_PASSWORD:
                    if (sample.length() != 2)
                        CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
                    secinfo("integrity", "%p checking passphrase", this);
                    if(validatePassphrase(sample[1])) {
                        return true;
                    }
                    break;
                // try to open with a given master key - Data:CSP or KeyHandle, Data:CssmKey
                case CSSM_SAMPLE_TYPE_SYMMETRIC_KEY:
                case CSSM_SAMPLE_TYPE_ASYMMETRIC_KEY:
                    assert(mBlob);
                    secinfo("integrity", "%p attempting explicit key unlock", this);
                    try {
                        CssmClient::Key checkKey = keyFromCreds(sample, 4);
                        if(common().validateKey(checkKey)) {
                            return true;
                        }
                    } catch(...) {
                        // ignore all problems in keyFromCreds
                        secinfo("integrity", "%p caught error", this);
                    }
                    break;
            }
        }
    }

    // out of options - credentials don't match
    return false;
}

uint32_t KeychainDatabase::interactiveUnlockAttempts = 0;

bool KeychainDatabase::interactiveUnlock()
{
	secinfo("KCdb", "%p attempting interactive unlock", this);
    interactiveUnlockAttempts++;

	SecurityAgent::Reason reason = SecurityAgent::noReason;
    QueryUnlock query(*this);
	// take UI interlock and release DbCommon lock (to avoid deadlocks)
	StSyncLock<Mutex, Mutex> uisync(common().uiLock(), common());
	
	// now that we have the UI lock, interact unless another thread unlocked us first
	if (isLocked()) {
		query.inferHints(Server::process());
        reason = query();
        if (mSaveSecret && reason == SecurityAgent::noReason) {
            query.retrievePassword(mSecret);
        }
        query.disconnect();
	} else {
		secinfo("KCdb", "%p was unlocked during uiLock delay", this);
	}

    if (common().isLoginKeychain()) {
        bool locked = false;
        service_context_t context = common().session().get_current_service_context();
        if ((service_client_kb_is_locked(&context, &locked, NULL) == 0) && locked) {
            QueryKeybagNewPassphrase keybagQuery(common().session());
            keybagQuery.inferHints(Server::process());
            CssmAutoData pass(Allocator::standard(Allocator::sensitive));
            CssmAutoData oldPass(Allocator::standard(Allocator::sensitive));
            SecurityAgent::Reason queryReason = keybagQuery.query(oldPass, pass);
            if (queryReason == SecurityAgent::noReason) {
                service_client_kb_change_secret(&context, oldPass.data(), (int)oldPass.length(), pass.data(), (int)pass.length());
            } else if (queryReason == SecurityAgent::resettingPassword) {
                query.retrievePassword(pass);
                service_client_kb_reset(&context, pass.data(), (int)pass.length());
            }

        }
    }

    return reason == SecurityAgent::noReason;
}

uint32_t KeychainDatabase::getInteractiveUnlockAttempts() {
    if (csr_check(CSR_ALLOW_APPLE_INTERNAL)) {
        // Not an internal install; don't answer
        return 0;
    } else {
        return interactiveUnlockAttempts;
    }
}


//
// Same thing, but obtain a new secret somehow and set it into the common.
//
bool KeychainDatabase::establishNewSecrets(const AccessCredentials *creds, SecurityAgent::Reason reason)
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
				secinfo("KCdb", "%p specified interactive passphrase", this);
				QueryNewPassphrase query(*this, reason);
				StSyncLock<Mutex, Mutex> uisync(common().uiLock(), common());
				query.inferHints(Server::process());
				CssmAutoData passphrase(Allocator::standard(Allocator::sensitive));
                CssmAutoData oldPassphrase(Allocator::standard(Allocator::sensitive));
				if (query(oldPassphrase, passphrase) == SecurityAgent::noReason) {
					common().setup(NULL, passphrase);
                    change_secret_on_keybag(*this, oldPassphrase.data(), (int)oldPassphrase.length(), passphrase.data(), (int)passphrase.length());
					return true;
				}
                }
				break;
			// try to use an explicitly given passphrase
			case CSSM_SAMPLE_TYPE_PASSWORD:
				{
                    secinfo("KCdb", "%p specified explicit passphrase", this);
                    if (sample.length() != 2)
                        CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
                    if (common().isLoginKeychain()) {
                        CssmAutoData oldPassphrase(Allocator::standard(Allocator::sensitive));
                        list<CssmSample> oldSamples;
                        creds->samples().collect(CSSM_SAMPLE_TYPE_KEYCHAIN_LOCK, oldSamples);
                        for (list<CssmSample>::iterator oit = oldSamples.begin(); oit != oldSamples.end(); oit++) {
                            TypedList &tmpList = *oit;
                            tmpList.checkProper();
                            if (tmpList.type() == CSSM_SAMPLE_TYPE_PASSWORD) {
                                if (tmpList.length() == 2) {
                                    oldPassphrase = tmpList[1].data();
                                }
                            }
                        }
                        if (!oldPassphrase.length() && mSecret && mSecret.length()) {
                            oldPassphrase = mSecret;
                        }
                        if ((oldPassphrase.length() == sample[1].data().length()) &&
                            !memcmp(oldPassphrase.data(), sample[1].data().data(), oldPassphrase.length()) &&
                            oldPassphrase.length()) {
                            // don't change master key if the passwords are the same
                            return false;
                        }
                        common().setup(NULL, sample[1]);
                        change_secret_on_keybag(*this, oldPassphrase.data(), (int)oldPassphrase.length(), sample[1].data().data(), (int)sample[1].data().length());
                    }
                    else {
                        common().setup(NULL, sample[1]);
                    }
                    return true;
                }
			// try to open with a given master key
			case CSSM_WORDID_SYMMETRIC_KEY:
			case CSSM_SAMPLE_TYPE_ASYMMETRIC_KEY:
				secinfo("KCdb", "%p specified explicit master key", this);
				common().setup(NULL, keyFromCreds(sample, 3));
				return true;
			// explicitly defeat the default action but don't try anything in particular
			case CSSM_WORDID_CANCELED:
				secinfo("KCdb", "%p defeat default action", this);
				break;
			default:
				// Unknown sub-sample for acquiring new secret.
				// If we wanted to be fascist, we could now do
				//  CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
				// But instead we try to be tolerant and continue on.
				// This DOES however count as an explicit attempt at specifying unlock,
				// so we will no longer try the default case below...
				secinfo("KCdb", "%p unknown sub-sample acquisition (%d) ignored",
					this, sample.type());
				break;
			}
		}
	} else {
		// default action -- interactive (only)
		QueryNewPassphrase query(*this, reason);
		StSyncLock<Mutex, Mutex> uisync(common().uiLock(), common());
        query.inferHints(Server::process());
		CssmAutoData passphrase(Allocator::standard(Allocator::sensitive));
        CssmAutoData oldPassphrase(Allocator::standard(Allocator::sensitive));
		if (query(oldPassphrase, passphrase) == SecurityAgent::noReason) {
			common().setup(NULL, passphrase);
            change_secret_on_keybag(*this, oldPassphrase.data(), (int)oldPassphrase.length(), passphrase.data(), (int)passphrase.length());
			return true;
		}
	}
	
	// out of options - no secret obtained
	CssmError::throwMe(CSSM_ERRCODE_OPERATION_AUTH_DENIED);
}


//
// Given a (truncated) Database credentials TypedList specifying a master key,
// locate the key and return a reference to it.
//
CssmClient::Key KeychainDatabase::keyFromCreds(const TypedList &sample, unsigned int requiredLength)
{
	// decode TypedList structure (sample type; Data:CSPHandle; Data:CSSM_KEY)
	assert(sample.type() == CSSM_SAMPLE_TYPE_SYMMETRIC_KEY || sample.type() == CSSM_SAMPLE_TYPE_ASYMMETRIC_KEY);
	if (sample.length() != requiredLength
		|| sample[1].type() != CSSM_LIST_ELEMENT_DATUM
		|| sample[2].type() != CSSM_LIST_ELEMENT_DATUM
		|| (requiredLength == 4 && sample[3].type() != CSSM_LIST_ELEMENT_DATUM))
			CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
	KeyHandle &handle = *sample[1].data().interpretedAs<KeyHandle>(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
    // We used to be able to check the length but supporting multiple client
    // architectures dishes that (sizeof(CSSM_KEY) varies due to alignment and
    // field-size differences).  The decoding in the transition layer should 
    // serve as a sufficient garbling check anyway.  
    if (sample[2].data().data() == NULL)
        CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
    CssmKey &key = *sample[2].data().interpretedAs<CssmKey>();

	if (key.header().cspGuid() == gGuidAppleCSPDL) {
		// handleOrKey is a SecurityServer KeyHandle; ignore key argument
		return safer_cast<LocalKey &>(*Server::key(handle));
	} else 
	if (sample.type() == CSSM_SAMPLE_TYPE_ASYMMETRIC_KEY) {
		/*
			Contents (see DefaultCredentials::unlockKey in libsecurity_keychain/defaultcreds.cpp)
			
			sample[0]	sample type
			sample[1]	csp handle for master or wrapping key; is really a keyhandle
			sample[2]	masterKey [not used since securityd cannot interpret; use sample[1] handle instead]
			sample[3]	UnlockReferralRecord data, in this case the flattened symmetric key
		*/

		// RefPointer<Key> Server::key(KeyHandle key)
		KeyHandle keyhandle = *sample[1].data().interpretedAs<KeyHandle>(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
		CssmData &flattenedKey = sample[3].data();
		RefPointer<Key> unwrappingKey = Server::key(keyhandle);
		Database &db=unwrappingKey->database();
		
		CssmKey rawWrappedKey;
		unflattenKey(flattenedKey, rawWrappedKey);

		RefPointer<Key> masterKey;
		CssmData emptyDescriptiveData;
		const AccessCredentials *cred = NULL;
		const AclEntryPrototype *owner = NULL;
		CSSM_KEYUSE usage = CSSM_KEYUSE_ANY;
		CSSM_KEYATTR_FLAGS attrs = CSSM_KEYATTR_EXTRACTABLE;	//CSSM_KEYATTR_RETURN_REF | 

		// Get default credentials for unwrappingKey (the one on the token)
		// Copied from Statics::Statics() in libsecurity_keychain/aclclient.cpp
		// Following KeyItem::getCredentials, one sees that the "operation" parameter
		// e.g. "CSSM_ACL_AUTHORIZATION_DECRYPT" is ignored
		Allocator &alloc = Allocator::standard();
		AutoCredentials promptCred(alloc, 3);// enable interactive prompting
	
		// promptCred: a credential permitting user prompt confirmations
		// contains:
		//  a KEYCHAIN_PROMPT sample, both by itself and in a THRESHOLD
		//  a PROMPTED_PASSWORD sample
		promptCred.sample(0) = TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT);
		promptCred.sample(1) = TypedList(alloc, CSSM_SAMPLE_TYPE_THRESHOLD,
			new(alloc) ListElement(TypedList(alloc, CSSM_SAMPLE_TYPE_KEYCHAIN_PROMPT)));
		promptCred.sample(2) = TypedList(alloc, CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD,
			new(alloc) ListElement(alloc, CssmData()));

		// This unwrap object is here just to provide a context
		CssmClient::UnwrapKey unwrap(Server::csp(), CSSM_ALGID_NONE);	//ok to lie about csp here
		unwrap.mode(CSSM_ALGMODE_NONE);
		unwrap.padding(CSSM_PADDING_PKCS1);
		unwrap.cred(promptCred);
		unwrap.add(CSSM_ATTRIBUTE_WRAPPED_KEY_FORMAT, uint32(CSSM_KEYBLOB_WRAPPED_FORMAT_PKCS7));
		Security::Context *tmpContext;
		CSSM_CC_HANDLE CCHandle = unwrap.handle();
		/*CSSM_RETURN rx = */ CSSM_GetContext (CCHandle, (CSSM_CONTEXT_PTR *)&tmpContext);
		
		// OK, this is skanky but necessary. We overwrite fields in the context struct

		tmpContext->ContextType = CSSM_ALGCLASS_ASYMMETRIC;
		tmpContext->AlgorithmType = CSSM_ALGID_RSA;
		
		db.unwrapKey(*tmpContext, cred, owner, unwrappingKey, NULL, usage, attrs,
			rawWrappedKey, masterKey, emptyDescriptiveData);

	    Allocator::standard().free(rawWrappedKey.KeyData.Data);

		return safer_cast<LocalKey &>(*masterKey).key();
	}
	else if (sample.type() == CSSM_SAMPLE_TYPE_SYMMETRIC_KEY && sample.length() == 4 && sample[3].data().length() > 0) {
        /*
         Contents (see MasterKeyUnlockCredentials in libsecurity_cdsa_client/lib/aclclient.cpp)

         sample[0]  sample type
         sample[1]  0, since we don't have a valid handle
         sample[2]  CssmKey of the masterKey [can't immediately use since it includes a CSSM_DATA struct with pointers]
         sample[3]  flattened symmetric master key, including the key data
         */

        // Fix up key to include actual data
        CssmData &flattenedKey = sample[3].data();
        unflattenKey(flattenedKey, key);

        // Check that we have a reasonable key
        if (key.header().blobType() != CSSM_KEYBLOB_RAW) {
            CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
        }
        if (key.header().keyClass() != CSSM_KEYCLASS_SESSION_KEY) {
            CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
        }

        // bring the key into the CSP and return it
        return CssmClient::Key(Server::csp(), key, true);
    } else {
		// not a KeyHandle reference; use key as a raw key
		if (key.header().blobType() != CSSM_KEYBLOB_RAW)
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_REFERENCE);
		if (key.header().keyClass() != CSSM_KEYCLASS_SESSION_KEY)
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
		return CssmClient::Key(Server::csp(), key, true);
	}
}

void unflattenKey(const CssmData &flatKey, CssmKey &rawKey)
{
    // The format we're expecting is a CSSM_KEY followed by the actual key data:
    //    CSSM_KEY : KEY DATA
    // which is approximately:
    //    h2ni(CSSM_KEYHEADER) : 4 bytes padding : CSSM_DATA{?:?} : KEY BYTES
    //
    // Note that CSSM_KEY includes a CSSM_DATA struct, which we will ignore as it has pointers.
    // The pointer and length will be set to whatever key data follows the CSSM_KEY in rawKey.

	// unflatten the raw input key naively: key header then key data
	// We also convert it back to host byte order
	// A CSSM_KEY is a CSSM_KEYHEADER followed by a CSSM_DATA

	// Now copy: header, then key struct, then key data
	memcpy(&rawKey.KeyHeader, flatKey.Data, sizeof(CSSM_KEYHEADER));
	memcpy(&rawKey.KeyData, flatKey.Data + sizeof(CSSM_KEYHEADER), sizeof(CSSM_DATA));
	size_t keyDataLength = flatKey.length() - sizeof(CSSM_KEY);
	rawKey.KeyData.Data = Allocator::standard().malloc<uint8>(keyDataLength);
	rawKey.KeyData.Length = keyDataLength;
	memcpy(rawKey.KeyData.Data, flatKey.Data + sizeof(CSSM_KEY), keyDataLength);
	Security::n2hi(rawKey.KeyHeader);	// convert it to host byte order
}


//
// Verify a putative database passphrase.
// If the database is already unlocked, just check the passphrase.
// Otherwise, unlock with that passphrase and report success.
// Caller must hold the common lock.
//
bool KeychainDatabase::validatePassphrase(const CssmData &passphrase) const
{
	if (common().hasMaster()) {
		// verify against known secret
		return common().validatePassphrase(passphrase);
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
void KeychainDatabase::lockDb()
{
    common().lockDb();
}


//
// Given a Key for this database, encode it into a blob and return it.
//
KeyBlob *KeychainDatabase::encodeKey(const CssmKey &key, const CssmData &pubAcl, const CssmData &privAcl)
{
	bool inTheClear = false;
	if((key.keyClass() == CSSM_KEYCLASS_PUBLIC_KEY) &&
	   !(key.attribute(CSSM_KEYATTR_PUBLIC_KEY_ENCRYPT))) {
		inTheClear = true;
	}
	StLock<Mutex> _(common());
	if(!inTheClear)
		makeUnlocked(false);
	
    // tell the cryptocore to form the key blob
    return common().encodeKeyCore(key, pubAcl, privAcl, inTheClear);
}


//
// Given a "blobbed" key for this database, decode it into its real
// key object and (re)populate its ACL.
//
void KeychainDatabase::decodeKey(KeyBlob *blob, CssmKey &key, void * &pubAcl, void * &privAcl)
{
	StLock<Mutex> _(common());

	if(!blob->isClearText())
		makeUnlocked(false);							// we need our keys

	common().decodeKeyCore(blob, key, pubAcl, privAcl);
	// memory protocol: pubAcl points into blob; privAcl was allocated
	
    activity();
}

//
// Given a KeychainKey (that implicitly belongs to another keychain), 
// return it encoded using this keychain's operational secrets.  
//
KeyBlob *KeychainDatabase::recodeKey(KeychainKey &oldKey)
{
	if (mRecodingSource != &oldKey.referent<KeychainDatabase>()) {
        CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
    }

    // To protect this operation, we need to take the mutex for both our common and the remote key's common in some defined order.
    // Grab the common being cloned (oldKey's) first, and then the common receiving the recoding (ours).
    StLock<Mutex> _ (oldKey.referent<KeychainDatabase>().common());
    StLock<Mutex> __(common());

	oldKey.instantiateAcl();	// make sure key is decoded
	CssmData publicAcl, privateAcl;
	oldKey.exportBlob(publicAcl, privateAcl);
	// NB: blob's memory belongs to caller, not the common

	/* 
	 * Make sure the new key is in the same cleartext/encrypted state.
	 */
	bool inTheClear = false;
	assert(oldKey.blob());
	if(oldKey.blob() && oldKey.blob()->isClearText()) {
		/* careful....*/
		inTheClear = true;
	}
	KeyBlob *blob = common().encodeKeyCore(oldKey.cssmKey(), publicAcl, privateAcl, inTheClear);
	oldKey.acl().allocator.free(publicAcl);
	oldKey.acl().allocator.free(privateAcl);
	return blob;
}


//
// Modify database parameters
//
void KeychainDatabase::setParameters(const DBParameters &params)
{
	StLock<Mutex> _(common());
    makeUnlocked(false);
	common().mParams = params;
    common().invalidateBlob();		// invalidate old blobs
    activity();				// (also resets the timeout timer)
	secinfo("KCdb", "%p common %p(%s) set params=(%u,%u)",
		this, &common(), dbName(), params.idleTimeout, params.lockOnSleep);
}


//
// Retrieve database parameters
//
void KeychainDatabase::getParameters(DBParameters &params)
{
	StLock<Mutex> _(common());
    makeUnlocked(false);
	params = common().mParams;
    //activity();		// getting parameters does not reset the idle timer
}


//
// RIGHT NOW, database ACLs are attached to the database.
// This will soon move upstairs.
//
SecurityServerAcl &KeychainDatabase::acl()
{
	return *this;
}


//
// Intercept ACL change requests and reset blob validity
//
void KeychainDatabase::instantiateAcl()
{
	StLock<Mutex> _(common());
	makeUnlocked(false);
}

void KeychainDatabase::changedAcl()
{
	StLock<Mutex> _(common());
	version = 0;
}


//
// Check an incoming DbBlob for basic viability
//
void KeychainDatabase::validateBlob(const DbBlob *blob)
{
    // perform basic validation on the blob
	assert(blob);
	blob->validate(CSSMERR_APPLEDL_INVALID_DATABASE_BLOB);
	switch (blob->version()) {
#if defined(COMPAT_OSX_10_0)
		case DbBlob::version_MacOS_10_0:
			break;
#endif
		case DbBlob::version_MacOS_10_1:
			break;
		case DbBlob::version_partition:
			break;
		default:
			CssmError::throwMe(CSSMERR_APPLEDL_INCOMPATIBLE_DATABASE_BLOB);
	}
}

//
// Check if this database is currently recoding
//
bool KeychainDatabase::isRecoding()
{
    secnotice("integrity", "recoding source: %p", mRecodingSource.get());
    return (mRecodingSource.get() != NULL || mRecoded);
}

//
// Mark ourselves as no longer recoding
//
void KeychainDatabase::recodeFinished()
{
    secnotice("integrity", "recoding finished");
    mRecodingSource = NULL;
    mRecoded = false;
}


//
// Debugging support
//
#if defined(DEBUGDUMP)

void KeychainDbCommon::dumpNode()
{
	PerSession::dumpNode();
	uint32 sig; memcpy(&sig, &mIdentifier.signature(), sizeof(sig));
	Debug::dump(" %s[%8.8x]", mIdentifier.dbName(), sig);
	if (isLocked()) {
		Debug::dump(" locked");
	} else {
		time_t whenTime = time_t(when());
		Debug::dump(" unlocked(%24.24s/%.2g)", ctime(&whenTime),
			(when() - Time::now()).seconds());
	}
	Debug::dump(" params=(%u,%u)", mParams.idleTimeout, mParams.lockOnSleep);
}

void KeychainDatabase::dumpNode()
{
	PerProcess::dumpNode();
	Debug::dump(" %s vers=%u",
		mValidData ? " data" : " nodata", version);
	if (mBlob) {
		uint32 sig; memcpy(&sig, &mBlob->randomSignature, sizeof(sig));
		Debug::dump(" blob=%p[%8.8x]", mBlob, sig);
	} else {
		Debug::dump(" noblob");
	}
}

#endif //DEBUGDUMP


//
// DbCommon basic features
//
KeychainDbCommon::KeychainDbCommon(Session &ssn, const DbIdentifier &id, uint32 requestedVersion)
	: LocalDbCommon(ssn), DatabaseCryptoCore(requestedVersion), sequence(0), version(1), mIdentifier(id),
      mIsLocked(true), mValidParams(false), mLoginKeychain(false)
{
    // match existing DbGlobal or create a new one
	{
        Server &server = Server::active();
        StLock<Mutex> _(server);
        if (KeychainDbGlobal *dbglobal =
                server.findFirst<KeychainDbGlobal, const DbIdentifier &>(&KeychainDbGlobal::identifier, identifier())) {
            parent(*dbglobal);
            secinfo("KCdb", "%p linking to existing DbGlobal %p", this, dbglobal);
        } else {
            // DbGlobal not present; make a new one
            parent(*new KeychainDbGlobal(identifier()));
            secinfo("KCdb", "%p linking to new DbGlobal %p", this, &global());
        }

        // link lifetime to the Session
        session().addReference(*this);
        
        if (strcasestr(id.dbName(), "login.keychain") != NULL) {
            mLoginKeychain = true;
        }
    }
}

void KeychainDbCommon::initializeKeybag() {
    if (mLoginKeychain && !session().keybagGetState(session_keybag_loaded)) {
        service_context_t context = session().get_current_service_context();
        if (service_client_kb_load(&context) == 0) {
            session().keybagSetState(session_keybag_loaded);
        }
    }
}

KeychainDbCommon::KeychainDbCommon(Session &ssn, const DbIdentifier &id, KeychainDbCommon& toClone)
    : LocalDbCommon(ssn), DatabaseCryptoCore(toClone.mBlobVersion), sequence(toClone.sequence), mParams(toClone.mParams), version(toClone.version),
    mIdentifier(id), mIsLocked(toClone.mIsLocked), mValidParams(toClone.mValidParams), mLoginKeychain(toClone.mLoginKeychain)
{
    cloneFrom(toClone);

    {
        Server &server = Server::active();
        StLock<Mutex> _(server);
        if (KeychainDbGlobal *dbglobal =
            server.findFirst<KeychainDbGlobal, const DbIdentifier &>(&KeychainDbGlobal::identifier, identifier())) {
            parent(*dbglobal);
            secinfo("KCdb", "%p linking to existing DbGlobal %p", this, dbglobal);
        } else {
            // DbGlobal not present; make a new one
            parent(*new KeychainDbGlobal(identifier()));
            secinfo("KCdb", "%p linking to new DbGlobal %p", this, &global());
        }
        session().addReference(*this);
    }
}

KeychainDbCommon::~KeychainDbCommon()
{
    secinfo("KCdb", "releasing keychain %p %s", this, (char*)this->dbName());

	// explicitly unschedule ourselves
	Server::active().clearTimer(this);
    if (mLoginKeychain) {
        session().keybagClearState(session_keybag_unlocked);
    }
    // remove ourselves from mCommonSet
    kill();
}

void KeychainDbCommon::cloneFrom(KeychainDbCommon& toClone, uint32 requestedVersion) {
    // don't clone the mIdentifier
    sequence = toClone.sequence;
    mParams = toClone.mParams;
    version = toClone.version;
    mIsLocked = toClone.mIsLocked;
    mValidParams = toClone.mValidParams;
    mLoginKeychain = toClone.mLoginKeychain;

    DatabaseCryptoCore::initializeFrom(toClone, requestedVersion);
}

void KeychainDbCommon::kill()
{
    StReadWriteLock _(mRWCommonLock, StReadWriteLock::Write);
    mCommonSet.erase(this);
}

KeychainDbGlobal &KeychainDbCommon::global() const
{
	return parent<KeychainDbGlobal>();
}


void KeychainDbCommon::select()
{ this->ref(); }

void KeychainDbCommon::unselect()
{ this->unref(); }



void KeychainDbCommon::makeNewSecrets()
{
	// we already have a master key (right?)
	assert(hasMaster());

	// tell crypto core to generate the use keys
	DatabaseCryptoCore::generateNewSecrets();
	
	// we're now officially "unlocked"; set the timer
	setUnlocked();
}


//
// All unlocking activity ultimately funnels through this method.
// This unlocks a DbCommon using the secrets setup in its crypto core
// component, and performs all the housekeeping needed to represent
// the state change.
// Returns true if unlock was successful, false if it failed due to
// invalid/insufficient secrets. Throws on other errors.
//
bool KeychainDbCommon::unlockDb(DbBlob *blob, void **privateAclBlob)
{
	try {
		// Tell the cryptocore to (try to) decode itself. This will fail
		// in an astonishing variety of ways if the passphrase is wrong.
		assert(hasMaster());
		decodeCore(blob, privateAclBlob);
		secinfo("KCdb", "%p unlock successful", this);
	} catch (...) {
		secinfo("KCdb", "%p unlock failed", this);
		return false;
	}
	
	// get the database parameters only if we haven't got them yet
	if (!mValidParams) {
		mParams = blob->params;
		n2hi(mParams.idleTimeout);
		mValidParams = true;	// sticky
	}

	bool isLocked = mIsLocked;
	
	setUnlocked();		// mark unlocked
	
	if (isLocked) {
		// broadcast unlock notification, but only if we were previously locked
		notify(kNotificationEventUnlocked);
        secinfo("KCdb", "unlocking keychain %p %s", this, (char*)this->dbName());
	}
    return true;
}

void KeychainDbCommon::setUnlocked()
{
	session().addReference(*this);	// active/held
	mIsLocked = false;				// mark unlocked
	activity();						// set timeout timer
}


void KeychainDbCommon::lockDb()
{
    bool lock = false;
    {
        StLock<Mutex> _(*this);
        if (!isLocked()) {
            DatabaseCryptoCore::invalidate();
            notify(kNotificationEventLocked);
            secinfo("KCdb", "locking keychain %p %s", this, (char*)this->dbName());
            Server::active().clearTimer(this);

            mIsLocked = true;		// mark locked
            lock = true;
            
            // this call may destroy us if we have no databases anymore
            session().removeReference(*this);
        }
    }
}


DbBlob *KeychainDbCommon::encode(KeychainDatabase &db)
{
    assert(!isLocked());	// must have been unlocked by caller
    
    // export database ACL to blob form
    CssmData pubAcl, privAcl;
    db.acl().exportBlob(pubAcl, privAcl);
    
    // tell the cryptocore to form the blob
    DbBlob form;
    form.randomSignature = identifier();
    form.sequence = sequence;
    form.params = mParams;
	h2ni(form.params.idleTimeout);
	
	assert(hasMaster());
    DbBlob *blob = encodeCore(form, pubAcl, privAcl);
    
    // clean up and go
    db.acl().allocator.free(pubAcl);
    db.acl().allocator.free(privAcl);
	return blob;
}


//
// Perform deferred lock processing for a database.
//
void KeychainDbCommon::action()
{
	secinfo("KCdb", "common %s(%p) locked by timer", dbName(), this);
	lockDb();
}

void KeychainDbCommon::activity()
{
    if (!isLocked()) {
		secinfo("KCdb", "setting DbCommon %p timer to %d",
			this, int(mParams.idleTimeout));
		Server::active().setTimer(this, Time::Interval(int(mParams.idleTimeout)));
	}
}

void KeychainDbCommon::sleepProcessing()
{
	secinfo("KCdb", "common %s(%p) sleep-lock processing", dbName(), this);
    if (mParams.lockOnSleep && !isDefaultSystemKeychain()) {
        StLock<Mutex> _(*this);
		lockDb();
    }
}

void KeychainDbCommon::lockProcessing()
{
	lockDb();
}


//
// We consider a keychain to belong to the system domain if it resides
// in /Library/Keychains. That's not exactly fool-proof, but we don't
// currently have any internal markers to interrogate.
//
bool KeychainDbCommon::belongsToSystem() const
{
    if (const char *name = this->dbName())
        return !strncmp(name, "/Library/Keychains/", 19);
    return false;
}

bool KeychainDbCommon::isDefaultSystemKeychain() const
{
    // /Library/Keychains/System.keychain (34)
    if (const char *name = this->dbName())
        return !strncmp(name, "/Library/Keychains/System.keychain", 34);
    return false;
}

//
// Keychain global objects
//
KeychainDbGlobal::KeychainDbGlobal(const DbIdentifier &id)
	: mIdentifier(id)
{
}

KeychainDbGlobal::~KeychainDbGlobal()
{
	secinfo("KCdb", "DbGlobal %p destroyed", this);
}
