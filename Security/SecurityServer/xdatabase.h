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
#ifndef _H_DATABASE
#define _H_DATABASE

#include "securityserver.h"
#include "acls.h"
#include "dbcrypto.h"
#include "notifications.h"
#include <Security/utilities.h>
#include <Security/handleobject.h>
#include <Security/cssmdb.h>
#include <Security/machserver.h>
#include <time.h>
#include <string>
#include <map>


class Key;
class Connection;
class Process;
using MachPlusPlus::MachServer;


//
// A Database object represents an Apple CSP/DL open database (DL/DB) object.
// It maintains its protected semantic state (including keys) and provides controlled
// access.
//
class Database : public HandleObject, public SecurityServerAcl {
    static const Listener::Event lockedEvent = Listener::lockedEvent;
    static const Listener::Event unlockedEvent = Listener::unlockedEvent;
    static const Listener::Event passphraseChangedEvent = Listener::passphraseChangedEvent;
    
public:
	class Common; friend class Common;
    
	Database(const DLDbIdentifier &id, const DBParameters &params, Process &proc,
        const AccessCredentials *cred, const AclEntryPrototype *owner);
	virtual ~Database();
    
    Process &process;
	
	static const int maxUnlockTryCount = 3;

public:
    typedef DbBlob::Signature Signature;

    class DbIdentifier {
    public:
        DbIdentifier(const DLDbIdentifier &id, Signature sig)
        : mIdent(id), mSig(sig) { }
        
        operator const DLDbIdentifier &() const { return mIdent; }
        operator const Signature &() const	{ return mSig; }
        const char *dbName() const			{ return mIdent.dbName(); }
        
        bool operator < (const DbIdentifier &id) const	// simple lexicographic
        {
            if (mIdent < id.mIdent) return true;
            if (id.mIdent < mIdent) return false;
            return mSig < id.mSig;
        }
        
    private:
        DLDbIdentifier mIdent;
        Signature mSig;
    };

public:
	//
	// A Database::Common is the "common core" of all Database objects that
	// represent the same client database (on disk, presumably).
	// NOTE: Common obeys exterior locking protocol: the caller (always Database)
	// must lock it before operating on its non-const members. In practice,
	// most Database methods lock down their Common first thing.
	//
    class Common : public DatabaseCryptoCore, public MachServer::Timer, public Mutex {
    public:
        Common(const DbIdentifier &id);
        ~Common();
        
        bool unlock(DbBlob *blob, const CssmData &passphrase,
            void **privateAclBlob = NULL);
        bool unlock(const CssmData &passphrase);
        void lock(bool holdingCommonLock = false, bool forSleep = false); // versatile lock primitive
        bool isLocked() const { return mIsLocked; } // lock status
        void activity();			// reset lock timeout
        
        const DbIdentifier &identifier() const {return mIdentifier; }
        const DLDbIdentifier &dlDbIdent() const { return identifier(); }
        const char *dbName() const { return dlDbIdent().dbName(); }
        
        DbBlob *encode(Database &db);
        void setupKeys(const AccessCredentials *cred);
        
        void notify(Listener::Event event);
		
	protected:
		void action();				// timer queue action to lock keychain

    public:
        DbIdentifier mIdentifier;	// database external identifier [const]
		// all following data locked with object lock
        uint32 sequence;			// change sequence number
        DBParameters mParams;		// database parameters (arbitrated copy)

        CssmAutoData passphrase;	// passphrase if available, or NULL data
        
        uint32 useCount;			// database sessions we belong to
        uint32 version;				// version stamp for change tracking
        
    private:
        bool mIsLocked;				// database is LOGICALLY locked
    };
    
    const DbIdentifier &identifier() const { return common->identifier(); }
    const char *dbName() const { return common->dbName(); }
	
public:
	// encoding/decoding databases
	DbBlob *encode();
	Database(const DLDbIdentifier &id, const DbBlob *blob, Process &proc,
        const AccessCredentials *cred);
    void authenticate(const AccessCredentials *cred);
    void changePassphrase(const AccessCredentials *cred);
	
	// lock/unlock processing
	void lock();											// unconditional lock
	void unlock();											// full-feature unlock
	void unlock(const CssmData &passphrase);				// unlock with passphrase
	bool decode(const CssmData &passphrase);				// try unlock/don't fail
	bool validatePassphrase(const CssmData &passphrase) const; // validate passphrase (no status change)
	bool isLocked() const { return common->isLocked(); }	// lock status
    
    void activity() const { common->activity(); }			// reset timeout clock
    static void lockAllDatabases(bool forSleep = false);	// lock them all
	
	// encoding/decoding keys
    void decodeKey(KeyBlob *blob, CssmKey &key, void * &pubAcl, void * &privAcl);
	KeyBlob *encodeKey(const CssmKey &key, const CssmData &pubAcl, const CssmData &privAcl);

    bool validBlob() const	{ return mBlob && version == common->version; }

	// manage database parameters
	void setParameters(const DBParameters &params);
	void getParameters(DBParameters &params);
    
    // ACL state management hooks
	void instantiateAcl();
	void noticeAclChange();
	const Database *relatedDatabase() const; // "self", for SecurityServerAcl's sake
    
    // notifications
    void notify(Listener::Event event) { common->notify(event); }

    // debugging
    IFDUMP(void debugDump(const char *msg));

protected:
	void makeUnlocked();							// interior version of unlock()
	void makeUnlocked(const CssmData &passphrase);	// interior version of unlock(CssmData)
    static void discard(Common *common);
		
private:
    Common *common;					// shared features of all instances of this database [const]

	// all following data is locked by the common lock
    bool mValidData;				// valid ACL and params (blob decoded)
        
    uint32 version;					// version stamp for blob validity
    DbBlob *mBlob;					// database blob (encoded)
    
    AccessCredentials *mCred;		// local access credentials (always valid)
    
private:
    // @@@ Arguably, this should be a member of the Server or Session.
    // @@@ If we do this, encapsulate it as a DatabaseMap object of sorts.
	static Mutex commonLock;		// lock for commons map (only)
    typedef map<DbIdentifier, Common *> CommonMap;
    static CommonMap commons;		// map of extant database objects
};


#endif //_H_DATABASE
