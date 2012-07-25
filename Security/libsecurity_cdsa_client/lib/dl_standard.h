/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
// dl_standard - standard-defined DL record types.
//
// These are the C++ record types corresponding to standard and Apple-defined
// DL relations. Note that not all standard fields are included; only those
// of particular interest to the implementation. Feel free to add field functions
// as needed.
//

#ifndef _H_CDSA_CLIENT_DL_STANDARD
#define _H_CDSA_CLIENT_DL_STANDARD

#include <security_cdsa_client/dlclient.h>


namespace Security {
namespace CssmClient {


//
// All CDSA standard DL schemas contain these fields
//
class DLCommonFields : public Record {
public:
	DLCommonFields(const char * const * names);

	string printName() const;
	string alias() const;
};


//
// A record type for all records in a DL, with PrintName (only)
//
class AllDLRecords : public DLCommonFields {
public:
	AllDLRecords();
};


//
// The CDSA-standard "generic record" table
//
class GenericRecord : public DLCommonFields {
public:
	GenericRecord();
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_GENERIC;
};


//
// Generic password records (Apple specific)
//
class GenericPasswordRecord : public DLCommonFields {
public:
	GenericPasswordRecord();
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_GENERIC_PASSWORD;
};


//
// Key records
//
class KeyRecord : public DLCommonFields {
public:
	KeyRecord();
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_ALL_KEYS;

	uint32 keyClass() const;
	uint32 type() const;
	uint32 size() const;
	uint32 effectiveSize() const;
	const CssmData &label() const;
	const CssmData &applicationTag() const;
	
	// boolean attributes for classification
	bool isPermanent() const;
	bool isPrivate() const;
	bool isModifiable() const;
	bool isSensitive() const;
	bool wasAlwaysSensitive() const;
	bool isExtractable() const;
	bool wasNeverExtractable() const;
	bool canEncrypt() const;
	bool canDecrypt() const;
	bool canDerive() const;
	bool canSign() const;
	bool canVerify() const;
	bool canWrap() const;
	bool canUnwrap() const;
};

class PrivateKeyRecord : public KeyRecord {
public:
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
};

class PublicKeyRecord : public KeyRecord {
public:
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_PUBLIC_KEY;
};

class SymmetricKeyRecord : public KeyRecord {
public:
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;
};


//
// X509 Certificate records
//
class X509CertRecord : public DLCommonFields {
public:
	X509CertRecord();
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_X509_CERTIFICATE;
	
	CSSM_CERT_TYPE type() const;
	CSSM_CERT_ENCODING encoding() const;
	const CssmData &subject() const;
	const CssmData &issuer() const;
	const CssmData &serial() const;
	const CssmData &subjectKeyIdentifier() const;
	const CssmData &publicKeyHash() const;
};


//
// Unlock referral records
//
class UnlockReferralRecord : public DLCommonFields {
public:
	UnlockReferralRecord();
	static const CSSM_DB_RECORDTYPE recordType = CSSM_DL_DB_RECORD_UNLOCK_REFERRAL;
	
	uint32 type() const;
	string dbName() const;
	const CssmData &dbNetname() const;
	const Guid &dbGuid() const;
	uint32 dbSSID() const;
	uint32 dbSSType() const;
	const CssmData &keyLabel() const;
	const CssmData &keyApplicationTag() const;
};


} // end namespace CssmClient
} // end namespace Security

#endif // _H_CDSA_CLIENT_DL_STANDARD
