/*
 * Copyright (c) 2000-2008 Apple Inc. All Rights Reserved.
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
// database - database session management
//
#include "database.h"
#include "agentquery.h"
#include "key.h"
#include "server.h"
#include "session.h"
#include "notifications.h"
#include <security_agent_client/agentclient.h>
#include <securityd_client/dictionary.h>
#include <security_cdsa_utilities/acl_any.h>	// for default owner ACLs
#include <security_cdsa_client/wrapkey.h>
#include <security_utilities/endian.h>

using namespace UnixPlusPlus;


//
// DbCommon basics
//
DbCommon::DbCommon(Session &session)
{
	referent(session);
}

Session &DbCommon::session() const
{
	return referent<Session>();
}


//
// Database basics
//
Database::Database(Process &proc)
{
	referent(proc);
}


Process& Database::process() const
{
	return referent<Process>();
}
	

//
// Send a keychain-related notification event about this database
//
void DbCommon::notify(NotificationEvent event, const DLDbIdentifier &ident)
{
	// form the data (encoded DLDbIdentifier)
    NameValueDictionary nvd;
    NameValueDictionary::MakeNameValueDictionaryFromDLDbIdentifier(ident, nvd);
    CssmData data;
    nvd.Export(data);

	// inject notification into Security event system
    Listener::notify(kNotificationDomainDatabase, event, data);
	
	// clean up
    free (data.data());
}


//
// Default behaviors
//
void DbCommon::sleepProcessing()
{
	// nothing
}

void DbCommon::lockProcessing()
{
	// nothing
}

bool DbCommon::belongsToSystem() const
{
	return false;
}


void Database::releaseKey(Key &key)
{
	kill(key);
}

void Database::releaseSearch(Search &search)
{
	kill(search);
}

void Database::releaseRecord(Record &record)
{
	kill(record);
}

void Database::dbName(const char *name)
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}


//
// Functions that aren't implemented at the Database level but can stay that way
//
void Database::findFirst(const CssmQuery &query,
	CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
	CssmData *data, RefPointer<Key> &key, RefPointer<Search> &search, RefPointer<Record> &record,
	CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	secdebug("database", "%p calling unimplemented findFirst", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::findNext(Search *search,
	CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
	CssmData *data, RefPointer<Key> &key, RefPointer<Record> &record,
	CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	secdebug("database", "%p calling unimplemented findNext", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::findRecordHandle(Record *record,
	CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
	CssmData *data, RefPointer<Key> &key,
	CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	secdebug("database", "%p calling unimplemented findRecordHandle", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::insertRecord(CSSM_DB_RECORDTYPE recordtype,
	const CssmDbRecordAttributeData *attributes, mach_msg_type_number_t inAttributesLength,
	const CssmData &data, RecordHandle &record)
{
	secdebug("database", "%p calling unimplemented insertRecord", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::modifyRecord(CSSM_DB_RECORDTYPE recordtype, Record *record,
	const CssmDbRecordAttributeData *attributes, mach_msg_type_number_t inAttributesLength,
	const CssmData *data, CSSM_DB_MODIFY_MODE modifyMode)
{
	secdebug("database", "%p calling unimplemented modifyRecord", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::deleteRecord(Database::Record *record)
{
	secdebug("database", "%p calling unimplemented deleteRecord", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::authenticate(CSSM_DB_ACCESS_TYPE, const AccessCredentials *)
{
	secdebug("database", "%p calling unimplemented authenticate", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

SecurityServerAcl &Database::acl()
{
	secdebug("database", "%p has no ACL implementation", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

bool Database::isLocked()
{
	secdebug("database", "%p calling unimplemented isLocked", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}


//
// SecurityServerAcl personality implementation.
// This is the trivial (type coding) stuff. The hard stuff is virtually mixed in.
//
Database *Database::relatedDatabase()
{
	return this;
}

AclKind Database::aclKind() const
{
	return dbAcl;
}


//
// Remote validation is not, by default, supported
//
bool Database::validateSecret(const AclSubject *, const AccessCredentials *)
{
	return false;
}


//
// Implementation of a "system keychain unlock key store"
//
SystemKeychainKey::SystemKeychainKey(const char *path)
	: mPath(path), mValid(false)
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
	if (mValid && mUpdateThreshold > Time::now())
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
