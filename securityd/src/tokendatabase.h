/*
 * Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
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
// tokendatabase - software database container implementation.
//
// A TokenDatabase represents access to an external (secure) storage container
// of some kind (usually a smartcard token).
//
#ifndef _H_TOKENDATABASE
#define _H_TOKENDATABASE

#include "database.h"
#include "tokenacl.h"
#include "session.h"
#include "token.h"
#include <security_utilities/adornments.h>

class TokenDatabase;
class TokenDbCommon;
class TokenKey;
class TokenDaemon;


//
// The global per-system object for a TokenDatabase (the TokenDbGlobal so to
// speak) is the Token object itself (from token.h).
//


//
// TokenDatabase DbCommons
//
class TokenDbCommon : public DbCommon, public Adornable {
public:
	TokenDbCommon(Session &ssn, Token &tk, const char *name);
	~TokenDbCommon();

	Token &token() const;
	
	uint32 subservice() const { return token().subservice(); }
	std::string dbName() const;
	
	Adornable &store();
	void resetAcls();

	void notify(NotificationEvent event);
	
	void lockProcessing();

	typedef Token::ResetGeneration ResetGeneration;

private:
	std::string mDbName;			// name given during open
	bool mHasAclState;				// Adornment is carrying active ACL state

	ResetGeneration mResetLevel;	// validity tag
};


//
// A Database object represents a SC/CSPDL per-process access to a token.
//
class TokenDatabase : public Database {
	friend class TokenDbCommon;
public:
	TokenDatabase(uint32 ssid, Process &proc, const char *name, const AccessCredentials *cred);
	~TokenDatabase();

	TokenDbCommon &common() const;
	Token &token() const { return common().token(); }
	TokenDaemon &tokend();
	uint32 subservice() const { return common().subservice(); }
	const char *dbName() const;
	void dbName(const char *name);
	bool transient() const;
	
	SecurityServerAcl &acl();		// it's our Token
	void getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls);	// post-processing

	bool isLocked();
	bool pinState(uint32 pin, int *count = NULL);

    void notify(NotificationEvent event) { return common().notify(event); }

	bool validateSecret(const AclSubject *subject, const AccessCredentials *cred);
	
	const AccessCredentials *openCreds() const { return mOpenCreds; }
	
protected:
	// any Process-referent concept handle we hand out to the client
	class Handler {
	public:
		Handler() : mHandle(0) { }
		GenericHandle &tokenHandle() { return mHandle; }
		GenericHandle tokenHandle() const { return mHandle; }

	protected:
		GenericHandle mHandle;
	};
	
	// CSSM-style search handles (returned by findFirst)
	struct Search : public Database::Search, public Handler {
		Search(TokenDatabase &db) : Database::Search(db) { }
		TokenDatabase &database() const { return referent<TokenDatabase>(); }
		~Search();
		
		Search *commit()	{ database().addReference(*this); return this; }
	};
	
	// CSSM-style record handles (returned by findFirst/findNext et al)
	struct Record : public Database::Record, public Handler, public TokenAcl {
		Record(TokenDatabase &db) : Database::Record(db) { }
		TokenDatabase &database() const { return referent<TokenDatabase>(); }
		~Record();
		
		Record *commit()	{ database().addReference(*this); return this; }
		
		void validate(AclAuthorization auth, const AccessCredentials *cred)
		{ TokenAcl::validate(auth, cred, &database()); }
		
		// TokenAcl personality
		AclKind aclKind() const;
		Token &token();
		using Handler::tokenHandle;
		GenericHandle tokenHandle() const;
	};

public:
	//
	// Cryptographic service calls
	//
    void queryKeySizeInBits(Key &key, CssmKeySize &result);
    void getOutputSize(const Context &context, Key &key, uint32 inputSize, bool encrypt, uint32 &result);
	
	// service calls
	void generateSignature(const Context &context, Key &key, CSSM_ALGORITHMS signOnlyAlgorithm,
		const CssmData &data, CssmData &signature);
	void verifySignature(const Context &context, Key &key, CSSM_ALGORITHMS verifyOnlyAlgorithm,
		const CssmData &data, const CssmData &signature);
	void generateMac(const Context &context, Key &key,
		const CssmData &data, CssmData &mac);
	void verifyMac(const Context &context, Key &key,
		const CssmData &data, const CssmData &mac);
	
	void encrypt(const Context &context, Key &key, const CssmData &clear, CssmData &cipher);
	void decrypt(const Context &context, Key &key, const CssmData &cipher, CssmData &clear);
	
	void generateKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		uint32 usage, uint32 attrs, RefPointer<Key> &newKey);
	void generateKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		uint32 pubUsage, uint32 pubAttrs, uint32 privUsage, uint32 privAttrs,
		RefPointer<Key> &publicKey, RefPointer<Key> &privateKey);
	void deriveKey(const Context &context, Key *key,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		CssmData *param, uint32 usage, uint32 attrs, RefPointer<Key> &derivedKey);

    void wrapKey(const Context &context, const AccessCredentials *cred,
		Key *hWrappingKey, Key &keyToBeWrapped,
        const CssmData &descriptiveData, CssmKey &wrappedKey);
	void unwrapKey(const Context &context,
		const AccessCredentials *cred, const AclEntryPrototype *owner,
		Key *wrappingKey, Key *publicKey, CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs,
		const CssmKey wrappedKey, RefPointer<Key> &unwrappedKey, CssmData &descriptiveData);

public:
	//
	// Data-access calls
	//
	void findFirst(const CssmQuery &query,
		CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
		CssmData *data, RefPointer<Key> &key,
		 RefPointer<Database::Search> &search, RefPointer<Database::Record> &record,
		CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength);
	void findNext(Database::Search *search,
		CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
		CssmData *data, RefPointer<Key> &key, RefPointer<Database::Record> &record,
		CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength);
	void findRecordHandle(Database::Record *record,
		CssmDbRecordAttributeData *inAttributes, mach_msg_type_number_t inAttributesLength,
		CssmData *data, RefPointer<Key> &key,
		CssmDbRecordAttributeData * &outAttributes, mach_msg_type_number_t &outAttributesLength);
	void insertRecord(CSSM_DB_RECORDTYPE recordtype,
		const CssmDbRecordAttributeData *attributes, mach_msg_type_number_t inAttributesLength,
		const CssmData &data, RefPointer<Database::Record> &record);
	void modifyRecord(CSSM_DB_RECORDTYPE recordtype, Record *record,
		const CssmDbRecordAttributeData *attributes, mach_msg_type_number_t inAttributesLength,
		const CssmData *data, CSSM_DB_MODIFY_MODE modifyMode);
	void deleteRecord(Database::Record *record);

	// authenticate to database
    void authenticate(CSSM_DB_ACCESS_TYPE mode, const AccessCredentials *cred);
	
private:
	// internal utilities
	RefPointer<Key> makeKey(KeyHandle hKey, const CssmKey *key,
		uint32 moreAttributes, const AclEntryPrototype *owner);
	
	class InputKey {
	public:
		InputKey(Key *key)					{ setup(key); }
		InputKey(Key &key)					{ setup(&key); }
		~InputKey();

		operator KeyHandle () const			{ return mKeyHandle; }
		operator const CssmKey * () const	{ return mKeyPtr; }
	
	private:
		KeyHandle mKeyHandle;
		CssmKey mKey;
		CssmKey *mKeyPtr;
		
		void setup(Key *key);
	};
	
private:
	AccessCredentials *mOpenCreds;			// credentials passed during open
};


#endif //_H_TOKENDATABASE
