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
	secinfo("database", "%p calling unimplemented findFirst", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::findNext(Search *search,
	CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
	CssmData *data, RefPointer<Key> &key, RefPointer<Record> &record,
	CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	secinfo("database", "%p calling unimplemented findNext", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::findRecordHandle(Record *record,
	CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
	CssmData *data, RefPointer<Key> &key,
	CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength)
{
	secinfo("database", "%p calling unimplemented findRecordHandle", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::insertRecord(CSSM_DB_RECORDTYPE recordtype,
	const CssmDbRecordAttributeData *attributes, mach_msg_type_number_t inAttributesLength,
	const CssmData &data, RecordHandle &record)
{
	secinfo("database", "%p calling unimplemented insertRecord", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::modifyRecord(CSSM_DB_RECORDTYPE recordtype, Record *record,
	const CssmDbRecordAttributeData *attributes, mach_msg_type_number_t inAttributesLength,
	const CssmData *data, CSSM_DB_MODIFY_MODE modifyMode)
{
	secinfo("database", "%p calling unimplemented modifyRecord", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::deleteRecord(Database::Record *record)
{
	secinfo("database", "%p calling unimplemented deleteRecord", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void Database::authenticate(CSSM_DB_ACCESS_TYPE, const AccessCredentials *)
{
	secinfo("database", "%p calling unimplemented authenticate", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

bool Database::checkCredentials(const AccessCredentials *)
{
    secinfo("database", "%p calling unimplemented checkCredentials", this);
    CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

SecurityServerAcl &Database::acl()
{
	secinfo("database", "%p has no ACL implementation", this);
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

bool Database::isLocked()
{
	secinfo("database", "%p calling unimplemented isLocked", this);
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

