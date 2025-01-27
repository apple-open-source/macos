/*
 * Copyright (c) 2000-2004,2011,2014 Apple Inc. All Rights Reserved.
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
// mds_standard - standard-defined MDS record types
//
#include <security_cdsa_client/dl_standard.h>
#include <security_cdsa_client/dlquery.h>


namespace Security {
namespace CssmClient {


//
// CDSA Common relation (one record per module)
//
static const char * const commonAttributes[] = {
	"PrintName",
	"Alias",
	NULL
};
DLCommonFields::DLCommonFields(const char * const * names)
	: Record(commonAttributes)
{
	addAttributes(names);
}

string DLCommonFields::printName() const			{ return mAttributes[0]; }
string DLCommonFields::alias() const
	{ return mAttributes[1].size() ? string(mAttributes[1]) : "(no value)"; }


//
// The all-record-types pseudo-record
//
AllDLRecords::AllDLRecords()
	: DLCommonFields(NULL)
{ }


//
// CDSA Generic record attributes
//
static const char * const genericAttributes[] = {
	NULL
};
GenericRecord::GenericRecord()
	: DLCommonFields(genericAttributes)
{
}


//
// Apple "Generic Password" records
//
static const char * const genericPasswordAttributes[] = {
	// if you find yourself here, you should add the attributes and their functions
	NULL
};
GenericPasswordRecord::GenericPasswordRecord()
	: DLCommonFields(genericPasswordAttributes)
{
}


//
// Common key attributes
//
static const char * const keyAttributes[] = {
	"KeyClass",
	"KeyType",
	"KeySizeInBits",
	"EffectiveKeySize",
	"Label",
	"ApplicationTag",
	"Permanent",
	"Private",
	"Modifiable",
	"Sensitive",
	"AlwaysSensitive",
	"Extractable",
	"NeverExtractable",
	"Encrypt",
	"Decrypt",
	"Derive",
	"Sign",
	"Verify",
	"Wrap",
	"Unwrap",
	NULL
};

KeyRecord::KeyRecord()
	: DLCommonFields(keyAttributes)
{
}

uint32 KeyRecord::keyClass() const				{ return mAttributes[2]; }
uint32 KeyRecord::type() const					{ return mAttributes[3]; }
uint32 KeyRecord::size() const					{ return mAttributes[4]; }
uint32 KeyRecord::effectiveSize() const			{ return mAttributes[5]; }
const CssmData &KeyRecord::label() const		{ return mAttributes[6]; }
const CssmData &KeyRecord::applicationTag() const { return mAttributes[7]; }
bool KeyRecord::isPermanent() const				{ return mAttributes[8]; }
bool KeyRecord::isPrivate() const				{ return mAttributes[9]; }
bool KeyRecord::isModifiable() const			{ return mAttributes[10]; }
bool KeyRecord::isSensitive() const				{ return mAttributes[11]; }
bool KeyRecord::wasAlwaysSensitive() const		{ return mAttributes[12]; }
bool KeyRecord::isExtractable() const			{ return mAttributes[13]; }
bool KeyRecord::wasNeverExtractable() const		{ return mAttributes[14]; }
bool KeyRecord::canEncrypt() const				{ return mAttributes[15]; }
bool KeyRecord::canDecrypt() const				{ return mAttributes[16]; }
bool KeyRecord::canDerive() const				{ return mAttributes[17]; }
bool KeyRecord::canSign() const					{ return mAttributes[18]; }
bool KeyRecord::canVerify() const				{ return mAttributes[19]; }
bool KeyRecord::canWrap() const					{ return mAttributes[20]; }
bool KeyRecord::canUnwrap() const				{ return mAttributes[21]; }


//
// Certificate attributes
//
static const char * const certAttributes[] = {
	"CertType",
	"CertEncoding",
	"Subject",
	"Issuer",
	"SerialNumber",
	"SubjectKeyIdentifier",
	"PublicKeyHash",
	NULL
};

X509CertRecord::X509CertRecord()
	: DLCommonFields(certAttributes)
{
}

CSSM_CERT_TYPE X509CertRecord::type() const	{ return mAttributes[2]; }
CSSM_CERT_ENCODING X509CertRecord::encoding() const	{ return mAttributes[3]; }
const CssmData &X509CertRecord::subject() const	{ return mAttributes[4]; }
const CssmData &X509CertRecord::issuer() const	{ return mAttributes[5]; }
const CssmData &X509CertRecord::serial() const	{ return mAttributes[6]; }
const CssmData &X509CertRecord::subjectKeyIdentifier() const { return mAttributes[7]; }
const CssmData &X509CertRecord::publicKeyHash() const { return mAttributes[8]; }


//
// UnlockReferral attributes
//
static const char * const unlockReferralAttributes[] = {
	"Type",
	"DbName",
	"DbNetname",
	"DbGuid",
	"DbSSID",
	"DbSSType",
	"KeyLabel",
	"KeyAppTag",
	NULL
};

UnlockReferralRecord::UnlockReferralRecord()
	: DLCommonFields(unlockReferralAttributes)
{
}

uint32 UnlockReferralRecord::type() const		{ return mAttributes[2]; }
string UnlockReferralRecord::dbName() const		{ return mAttributes[3]; }
const CssmData &UnlockReferralRecord::dbNetname() const { return mAttributes[4]; }
const Guid &UnlockReferralRecord::dbGuid() const { return mAttributes[5]; }
uint32 UnlockReferralRecord::dbSSID() const		{ return mAttributes[6]; }
uint32 UnlockReferralRecord::dbSSType() const	{ return mAttributes[7]; }
const CssmData &UnlockReferralRecord::keyLabel() const { return mAttributes[8]; }
const CssmData &UnlockReferralRecord::keyApplicationTag() const { return mAttributes[9]; }


} // end namespace CssmClient
} // end namespace Security
