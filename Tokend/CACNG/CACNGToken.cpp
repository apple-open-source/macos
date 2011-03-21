/*
 *  Copyright (c) 2004,2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  CACNGToken.cpp
 *  TokendMuscle
 */

#include "CACNGToken.h"

#include "Adornment.h"
#include "AttributeCoder.h"
#include "CACNGError.h"
#include "CACNGRecord.h"
#include "CACNGSchema.h"
#include <security_cdsa_client/aclclient.h>
#include <map>
#include <vector>

using CssmClient::AclFactory;

#define PIV_CLA_STANDARD      0x00
#define CLA_STANDARD      0x00
#define INS_SELECT_FILE   0xA4
#define INS_GET_DATA      0xCA

#define SELECT_APPLET  CLA_STANDARD, INS_SELECT_FILE, 0x04, 0x00

#define SELECT_CACNG_APPLET  SELECT_APPLET, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x79

#define SELECT_CACNG_OBJECT  CLA_STANDARD, INS_SELECT_FILE, 0x02, 0x00, 0x02

#define SELECT_CACNG_APPLET_PKI  SELECT_CACNG_APPLET, 0x01
#define SELECT_CACNG_APPLET_PIN  SELECT_CACNG_APPLET, 0x03, 0x00

static const unsigned char kSelectCardManagerApplet[] =
	{ SELECT_APPLET, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00 };

static const unsigned char kSelectCACNGAppletPKI[]   =
	{ SELECT_CACNG_APPLET_PKI, 0x00 };

static const unsigned char kSelectCACNGObjectPKIID[] =
	{ SELECT_CACNG_OBJECT, 0x01, 0x00 };
static const unsigned char kSelectCACNGObjectPKIESig[] =
	{ SELECT_CACNG_OBJECT, 0x01, 0x01 };
static const unsigned char kSelectCACNGObjectPKIECry[] =
	{ SELECT_CACNG_OBJECT, 0x01, 0x02 };

static const unsigned char kSelectCACNGObjectPN[]      =
	{ SELECT_CACNG_OBJECT, 0x02, 0x00 };
static const unsigned char kSelectCACNGObjectPL[]      =
	{ SELECT_CACNG_OBJECT, 0x02, 0x01 };
/* Unknown objects... */
static const unsigned char kSelectCACNGObjectBS[]      =
	{ SELECT_CACNG_OBJECT, 0x02, 0x02 };
static const unsigned char kSelectCACNGObjectOB[]      =
	{ SELECT_CACNG_OBJECT, 0x02, 0x03 };

static const unsigned char kSelectCACNGAppletPIN[]     =
	{ SELECT_CACNG_APPLET_PIN };


#define SELECT_PIV_APPLET_VERS	0x10, 0x00, 0x01, 0x00
#define SELECT_PIV_APPLET_SHORT	SELECT_APPLET, 0x07, 0xA0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00
#define SELECT_PIV_APPLET_LONG  SELECT_APPLET, 0x0B, 0xA0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, SELECT_PIV_APPLET_VERS

static const unsigned char kSelectPIVApplet[] =
	{ SELECT_PIV_APPLET_LONG };

//	X.509 Certificate for PIV Authentication 2.16.840.1.101.3.7.2.1.1 '5FC105' M
#define PIV_OBJECT_ID_X509_CERTIFICATE_PIV_AUTHENTICATION	0x5F, 0xC1, 0x05

static const unsigned char oidX509CertificatePIVAuthentication[] = { PIV_OBJECT_ID_X509_CERTIFICATE_PIV_AUTHENTICATION };

#define PIV_KEYREF_PIV_AUTHENTICATION      0x9A

CACNGToken::CACNGToken() :
	mCacPinStatus(0),mPivPinStatus(0)
{
	mTokenContext = this;
	mSession.open();

	/* Change pin only works if one of the CACNG applets are selected. */
	byte_string pinAppletId(kSelectCACNGAppletPIN, kSelectCACNGAppletPIN + sizeof(kSelectCACNGAppletPIN));
	shared_ptr<CACNGSelectable> cacPinApplet(new CACNGCacApplet(*this, pinAppletId, byte_string()));
	this->cacPinApplet = cacPinApplet;

	byte_string cardManagerAppletId(kSelectCardManagerApplet, kSelectCardManagerApplet + sizeof(kSelectCardManagerApplet));
	shared_ptr<CACNGSelectable> cardManagerApplet(new CACNGCacApplet(*this, cardManagerAppletId, byte_string()));
	this->cardManagerApplet = cardManagerApplet;

	byte_string selectPivApplet(kSelectPIVApplet, kSelectPIVApplet + sizeof(kSelectPIVApplet));
	shared_ptr<CACNGSelectable> pivApplet(new CACNGPivApplet(*this, selectPivApplet));
	this->pivApplet = pivApplet;
}

CACNGToken::~CACNGToken()
{
	delete mSchema;
	/* XXX: Wipe out cached pin */
	secure_resize(cached_piv_pin, 0);
}

bool CACNGToken::identify()
{
	try
	{
		byte_string pkiApplet(kSelectCACNGAppletPKI, kSelectCACNGAppletPKI + sizeof(kSelectCACNGAppletPKI));
		byte_string pkiIdObject(kSelectCACNGObjectPKIID, kSelectCACNGObjectPKIID + sizeof(kSelectCACNGObjectPKIID));
		byte_string pkiESigObject(kSelectCACNGObjectPKIESig, kSelectCACNGObjectPKIESig + sizeof(kSelectCACNGObjectPKIESig));
		shared_ptr<CACNGSelectable> idApplet(new CACNGCacApplet(*this, pkiApplet, pkiIdObject));
		shared_ptr<CACNGSelectable> eSigApplet(new CACNGCacApplet(*this, pkiApplet, pkiESigObject));
		select(idApplet);
		select(eSigApplet);
		return true;
	}
	catch (const PCSC::Error &error)
	{
		if (error.error == SCARD_E_PROTO_MISMATCH)
			return false;
		throw;
	}
}

void CACNGToken::select(shared_ptr<CACNGSelectable> &selectable)
{
	if (isInTransaction() &&
		(currentSelectable == selectable))
		return;
	 /* XXX: Resets PIV pin status to match card behavior */
//	if (selectable != pivApplet)
		mPivPinStatus = 0;
	selectable->select();
	if (isInTransaction()) {
		currentSelectable = selectable;
	}
}

uint32_t CACNGToken::exchangeAPDU(const unsigned char *apdu, size_t apduLength,
	unsigned char *result, size_t &resultLength)
{
	size_t savedLength = resultLength;

	ISO7816Token::transmit(apdu, apduLength, result, resultLength);
	if (resultLength == 2 && result[0] == 0x61)
	{
		resultLength = savedLength;
		size_t expectedLength = result[1];
		unsigned char getResult[] = { 0x00, 0xC0, 0x00, 0x00, expectedLength };
		if (expectedLength == 0) expectedLength = 256;
		ISO7816Token::transmit(getResult, sizeof(getResult), result, resultLength);
		if (resultLength - 2 != expectedLength)
        {
            if (resultLength < 2)
                PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
            else
                CACNGError::throwMe((result[resultLength - 2] << 8)
					+ result[resultLength - 1]);
        }
	}

	if (resultLength < 2)
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);

    return (result[resultLength - 2] << 8) + result[resultLength - 1];
}

void CACNGToken::didDisconnect()
{
	PCSC::Card::didDisconnect();
	currentSelectable.reset();
	mCacPinStatus = 0;
	mPivPinStatus = 0;
	/* XXX: Wipe out cached pin */
	secure_resize(cached_piv_pin, 0);
}

void CACNGToken::didEnd()
{
	PCSC::Card::didEnd();
	currentSelectable.reset();
	mCacPinStatus = 0;
	mPivPinStatus = 0;
	/* XXX: Wipe out cached pin */
	secure_resize(cached_piv_pin, 0);
}

void CACNGToken::changePIN(int pinNum,
	const unsigned char *oldPin, size_t oldPinLength,
	const unsigned char *newPin, size_t newPinLength)
{
	if (pinNum != 1)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	if (oldPinLength < 4 || oldPinLength > 8 ||
		newPinLength < 4 || newPinLength > 8)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);

	PCSC::Transaction _(*this);
	select(cacPinApplet);

	unsigned char apdu[] =
	{
		0x80, 0x24, 0x01, 0x00, 0x10,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};

	memcpy(apdu + 5, oldPin, oldPinLength);
	memcpy(apdu + 13, newPin, newPinLength);

	unsigned char result[2];
	size_t resultLength = sizeof(result);

	mCacPinStatus = exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	memset(apdu + 5, 0, 16);
	CACNGError::check(mCacPinStatus);

	/* XXX: Wipe out cached pin */
	secure_resize(cached_piv_pin, 0);
}

uint32_t CACNGToken::cacPinStatus()
{
	if (mCacPinStatus && isInTransaction()) {
		secdebug("adhoc", "returning cached PIN status 0x%x", mCacPinStatus);
		return mCacPinStatus;
	}
	
	PCSC::Transaction _(*this);
	/* Verify pin only works if one of the CACNG applets are selected. */
	select(cacPinApplet);

	unsigned char result[2];
	size_t resultLength = sizeof(result);
	unsigned char apdu[] = { 0x00, 0x20, 0x00, 0x00 };
	
	mCacPinStatus = exchangeAPDU(apdu, 4, result, resultLength);
	if ((mCacPinStatus & 0xFF00) != 0x6300
		&& mCacPinStatus != SCARD_AUTHENTICATION_BLOCKED)
		CACNGError::check(mCacPinStatus);
	
	secdebug("adhoc", "new PIN status=0x%x", mCacPinStatus);
	return mCacPinStatus;
}

uint32_t CACNGToken::pivPinStatus()
{
	if (mPivPinStatus && isInTransaction()) {
		secdebug("adhoc", "returning cached PIN status 0x%x", mPivPinStatus);
		return mPivPinStatus;
	}
	if (currentSelectable != pivApplet)
		return SCARD_NOT_AUTHORIZED;
	PCSC::Transaction _(*this);
	/* Check PIV pin only works if one of the PIV applets are selected. */
	select(pivApplet);
	
	unsigned char result[2];
	size_t resultLength = sizeof(result);
	unsigned char apdu[] = { 0x00, 0x20, 0x00, 0x00 };
	
	mPivPinStatus = exchangeAPDU(apdu, 4, result, resultLength);
	if ((mPivPinStatus & 0xFF00) != 0x6300
		&& mPivPinStatus != SCARD_AUTHENTICATION_BLOCKED)
		CACNGError::check(mPivPinStatus);
	
	secdebug("adhoc", "new PIN status=0x%x", mPivPinStatus);
	return mPivPinStatus;
}

uint32_t CACNGToken::pinStatus(int pinNum)
{
	switch (pinNum) {
	case 1:
		return cacPinStatus();
	case 2:
		return pivPinStatus();
	default:
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
	}
}

static void verify_cac(CACNGToken &token, const unsigned char *pin, size_t pinLength)
{
	token.select(token.cacPinApplet);
	
	unsigned char apdu[] =
	{
		0x00, 0x20, 0x00, 0x00, 0x08,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	
#if defined(CACNG_PROTECTED_MODE)
	memcpy(apdu + 5, "77777777", 8);
#else
	memcpy(apdu + 5, pin, pinLength);
#endif
	
	unsigned char result[2];
	size_t resultLength = sizeof(result);
	
	token.mCacPinStatus = token.exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	memset(apdu + 5, 0, 8);
	CACNGError::check(token.mCacPinStatus);	
}



static void verify_piv(CACNGToken &token, const unsigned char *pin, size_t pinLength)
{
	unsigned char apdu[] =
	{
		0x00, 0x20, 0x00, 0x80, 0x08,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	
#if defined(CACNG_PROTECTED_MODE)
	memcpy(apdu + 5, "77777777", 8);
#else
	memcpy(apdu + 5, pin, pinLength);
#endif
	
	unsigned char result[2];
	size_t resultLength = sizeof(result);
	token.select(token.pivApplet);
	token.mPivPinStatus = token.exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	memset(apdu + 5, 0, 8);
	CACNGError::check(token.mPivPinStatus);
}

void CACNGToken::verifyPIN(int pinNum,
	const unsigned char *pin, size_t pinLength)
{
	if (pinNum != 1 && pinNum != 2)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
	PCSC::Transaction _(*this);
	switch (pinNum) {
	case 1:
		if (pinLength < 4 || pinLength > 8)
			CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);

		/* Verify pin only works if one of the CACNG applets are selected. */
		verify_cac(*this, pin, pinLength);

		// Start a new transaction which we never get rid of until someone calls
		// unverifyPIN()
		begin();
		break;
	case 2:
		if (pinLength < 1 || pinLength > 8)
			CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);
		/* Verify pin only works if one of the CACNG applets are selected. */
		verify_piv(*this, pin, pinLength);
		/* XXX: CACHED PIN */
		cached_piv_pin.assign(pin, pin + pinLength);
		// Start a new transaction which we never get rid of until someone calls
		// unverifyPIN()
		begin();
		break;
	}
}

void CACNGToken::verifyCachedPin(int pinNum)
{
	if (pinNum != 2)
		return;
	/* XXX: PIN CACHE */
	if (cached_piv_pin.empty())
		return;
	try {
		verify_piv(*this, &cached_piv_pin[0], cached_piv_pin.size());
	} catch (...) {
		/* XXX: Wipe out cache if anything goes wrong */
		secure_resize(cached_piv_pin, 0);
		throw;
	}
}

void CACNGToken::unverifyPIN(int pinNum)
{
	if (pinNum != -1)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
	/* XXX: Wipe out cached pin */
	secure_resize(cached_piv_pin, 0);
	end(SCARD_RESET_CARD);
}

uint32_t CACNGToken::getData(unsigned char *result, size_t &resultLength)
{
	PCSC::Transaction _(*this);
	try
	{
		select(cardManagerApplet);
	}
	catch (const PCSC::Error &error)
	{
		return error.error;
	}

	unsigned char apdu[] = { 0x80, INS_GET_DATA, 0x9F, 0x7F, 0x2D };
	return exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
}

uint32 CACNGToken::probe(SecTokendProbeFlags flags,
	char tokenUid[TOKEND_MAX_UID])
{
	uint32 score = Tokend::ISO7816Token::probe(flags, tokenUid);

	bool doDisconnect = false; /*!(flags & kSecTokendProbeKeepToken); */

	try
	{
//		PCSC::Card::reconnect(SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1);
		if (!identify())
			doDisconnect = true;
		else
		{
			unsigned char result[0x2F];
			size_t resultLength = sizeof(result);
			(void)getData(result, resultLength);
			/* Score of 200 to ensure that CACNG "wins" for Hybrid CACNG/PIV cards */
				score = 300;
				// Now stick in the bytes returned by getData into the
				// tokenUid.
			if(resultLength > 20)
				{
					sprintf(tokenUid,
						"CACNG-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X",
						result[3], result[4], result[5], result[6], result[19],
						result[20], result[15], result[16], result[17],
						result[18]);
				}
			else
			{
				/* Cannot generated a tokenUid given the returned data.
				 * Generate time-based tokenUid to permit basic caching */
				unsigned char buffer[80];
				time_t now;
				struct tm* timestruct = localtime(&now);
				/* Print out the # of seconds since EPOCH UTF */
				strftime(reinterpret_cast<char *>(buffer), 80, "%s", timestruct);
				snprintf(tokenUid, TOKEND_MAX_UID, "CACNG-%s", buffer);
			}
			Tokend::ISO7816Token::name(tokenUid);
			secdebug("probe", "recognized %s", tokenUid);
		}
	}
	catch (...)
	{
		doDisconnect = true;
		score = 0;
	}

	if (doDisconnect)
		disconnect();

	return score;
}

void CACNGToken::establish(const CSSM_GUID *guid, uint32 subserviceId,
	SecTokendEstablishFlags flags, const char *cacheDirectory,
	const char *workDirectory, char mdsDirectory[PATH_MAX],
	char printName[PATH_MAX])
{
	Tokend::ISO7816Token::establish(guid, subserviceId, flags,
		cacheDirectory, workDirectory, mdsDirectory, printName);

	mSchema = new CACNGSchema();
	mSchema->create();

	populate();
}

//
// Database-level ACLs
//
void CACNGToken::getOwner(AclOwnerPrototype &owner)
{
	// we don't really know (right now), so claim we're owned by PIN #0
	if (!mAclOwner)
	{
		mAclOwner.allocator(Allocator::standard());
		mAclOwner = AclFactory::PinSubject(Allocator::standard(), 0);
	}
	owner = mAclOwner;
}


void CACNGToken::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	Allocator &alloc = Allocator::standard();
	
	if (unsigned pin = pinFromAclTag(tag, "?")) {
		static AutoAclEntryInfoList acl;
		acl.clear();
		acl.allocator(alloc);
		uint32_t status = this->pinStatus(pin);
		if (status == SCARD_SUCCESS)
			acl.addPinState(pin, CSSM_ACL_PREAUTH_TRACKING_AUTHORIZED);
		else if (status >= CACNG_AUTHENTICATION_FAILED_0 && status <= CACNG_AUTHENTICATION_FAILED_3)
			acl.addPinState(pin, 0, status - CACNG_AUTHENTICATION_FAILED_0);
		else
			acl.addPinState(pin, CSSM_ACL_PREAUTH_TRACKING_UNKNOWN);
		count = acl.size();
		acls = acl.entries();
		return;
	}

	// mAclEntries sets the handle of each AclEntryInfo to the
	// offset in the array.

	// get pin list, then for each pin
	if (!mAclEntries) {
		mAclEntries.allocator(alloc);
        // Anyone can read the attributes and data of any record on this token
        // (it's further limited by the object itself).
		mAclEntries.add(CssmClient::AclFactory::AnySubject(
			mAclEntries.allocator()),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
        // We support PIN1 with either a passed in password
        // subject or a prompted password subject.
		mAclEntries.addPin(AclFactory::PWSubject(alloc), 1);
		mAclEntries.addPin(AclFactory::PWSubject(alloc), 2);
		mAclEntries.addPin(AclFactory::PromptPWSubject(alloc, CssmData()), 1);
		mAclEntries.addPin(AclFactory::PromptPWSubject(alloc, CssmData()), 2);
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}


#pragma mark ---------------- CACNG Specific --------------

uint32_t CACNGToken::exchangeAPDU(const byte_string &apdu, byte_string &result)
{
	static const uint8_t GET_RESULT_TEMPLATE [] = { 0x00, 0xC0, 0x00, 0x00, 0xFF };
	byte_string getResult(GET_RESULT_TEMPLATE, GET_RESULT_TEMPLATE + sizeof(GET_RESULT_TEMPLATE));
	const int SIZE_INDEX = 4;
	
	transmit(apdu, result);
	/* Keep pulling more data */
	while (result.size() >= 2 && result[result.size() - 2] == 0x61)
	{
		size_t expectedLength = result[result.size() - 1];
		if(expectedLength == 0) /* 256-byte case .. */
			expectedLength = 256;
		getResult[SIZE_INDEX] = expectedLength;
		// Trim off status bytes
		result.resize(result.size() - 2);
		size_t appended = transmit(getResult, result);
		if (appended != (expectedLength + 2))
        {
            if (appended < 2)
                PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
            else
                CACNGError::throwMe((result[result.size() - 2] << 8)
								  + result[result.size() - 1]);
        }
	}
	
	if (result.size() < 2)
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	uint16_t ret = (result[result.size() - 2] << 8) + result[result.size() - 1];
	// Trim off status bytes
	result.resize(result.size() - 2);
    return ret;
}

size_t CACNGToken::transmit(const byte_string::const_iterator &apduBegin, const byte_string::const_iterator &apduEnd, byte_string &result) {
	const size_t BUFFER_SIZE = 1024;
	size_t resultLength = BUFFER_SIZE;
	size_t index = result.size();
	/* To prevent data leaking, secure byte_string resize takes place */
	secure_resize(result, result.size() + BUFFER_SIZE);
	ISO7816Token::transmit(&(*apduBegin), (size_t)(apduEnd - apduBegin), &result[0]+ index, resultLength);
	/* Trims the data, no expansion occurs */
	result.resize(index + resultLength);
	return resultLength;
}


uint32_t CACNGToken::exchangeChainedAPDU(
	unsigned char cla, unsigned char ins,
	unsigned char p1, unsigned char p2,
	const byte_string &data,
	byte_string &result)
{
	byte_string apdu;
	apdu.reserve(5 + data.size());
	apdu.resize(5);
	apdu[0] = cla;
	apdu[1] = ins;
	apdu[2] = p1;
	apdu[3] = p2;
	
	apdu[0] |= 0x10;
	apdu += data;
	const size_t BASE_CHUNK_LENGTH = 255;
	size_t chunkLength;
	byte_string::const_iterator iter;
	/* Chain data and skip last chunk since its in the receiving end */
	for(iter = data.begin(); (iter + BASE_CHUNK_LENGTH) < data.end(); iter += BASE_CHUNK_LENGTH) {
		chunkLength = std::min(BASE_CHUNK_LENGTH, (size_t)(data.end() - iter));
		apdu[4] = chunkLength & 0xFF;
		/* Don't send Le */
		transmit(apdu.begin(), apdu.begin() + 5 + chunkLength, result);
		/* No real data should come back until chaining is complete */
		if(result.size() != 2)
			PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
		else
			CACNGError::check(result[result.size() - 2] << 8 | result[result.size() - 1]);
		/* Trim off result SW */
		result.resize(result.size() - 2);
		// Trim off old data
		apdu.erase(apdu.begin() + 5, apdu.begin() + 5 + chunkLength);
	}
	apdu[0] &= ~0x10;
	apdu[4] = (apdu.size() - 5) & 0xFF;
	/* LE BYTE? */
	return exchangeAPDU(apdu, result);
}


void CACNGToken::populate()
{
	secdebug("populate", "CACNGToken::populate() begin");
	Tokend::Relation &certRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE);
	Tokend::Relation &privateKeyRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_PRIVATE_KEY);
	Tokend::Relation &dataRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_GENERIC);

	byte_string pkiApplet(kSelectCACNGAppletPKI, kSelectCACNGAppletPKI + sizeof(kSelectCACNGAppletPKI));

	shared_ptr<CACNGSelectable> idApplet(new CACNGCacApplet(*this, pkiApplet,
		byte_string(kSelectCACNGObjectPKIID, kSelectCACNGObjectPKIID + sizeof(kSelectCACNGObjectPKIID))));
	shared_ptr<CACNGSelectable> sigApplet(new CACNGCacApplet(*this, pkiApplet,
		byte_string(kSelectCACNGObjectPKIESig, kSelectCACNGObjectPKIESig + sizeof(kSelectCACNGObjectPKIESig))));
	shared_ptr<CACNGSelectable> encApplet(new CACNGCacApplet(*this, pkiApplet,
		byte_string(kSelectCACNGObjectPKIECry, kSelectCACNGObjectPKIECry + sizeof(kSelectCACNGObjectPKIECry))));

	shared_ptr<CACNGIDObject> idObject(new CACNGCacIDObject(*this, idApplet, "Identity Certificate"));
	shared_ptr<CACNGIDObject> sigObject(new CACNGCacIDObject(*this, sigApplet, "Email Signature Certificate"));
	shared_ptr<CACNGIDObject> encObject(new CACNGCacIDObject(*this, encApplet, "Email Encryption Certificate"));
	RefPointer<Tokend::Record> idCert(new CACNGCertificateRecord(idObject, "Identity Certificate"));
	RefPointer<Tokend::Record> eSigCert(new CACNGCertificateRecord(sigObject, "Email Signing Certificate"));
	RefPointer<Tokend::Record> eCryCert(new CACNGCertificateRecord(encObject, "Email Encryption Certificate"));

#if 1
	certRelation.insertRecord(idCert);
	certRelation.insertRecord(eSigCert);
	certRelation.insertRecord(eCryCert);

	RefPointer<Tokend::Record> idKey(new CACNGKeyRecord(idObject, "Identity Private Key",
		privateKeyRelation.metaRecord(), true));
	RefPointer<Tokend::Record> eSigKey(new CACNGKeyRecord(sigObject, "Email Signing Private Key",
		privateKeyRelation.metaRecord(), true));
	RefPointer<Tokend::Record> eCryKey(new CACNGKeyRecord(encObject, "Email Encryption Private Key",
		privateKeyRelation.metaRecord(), false));

	privateKeyRelation.insertRecord(idKey);
	privateKeyRelation.insertRecord(eSigKey);
	privateKeyRelation.insertRecord(eCryKey);

	idKey->setAdornment(mSchema->publicKeyHashCoder().certificateKey(),
                        new Tokend::LinkedRecordAdornment(idCert));
	eSigKey->setAdornment(mSchema->publicKeyHashCoder().certificateKey(),
                          new Tokend::LinkedRecordAdornment(eSigCert));
	eCryKey->setAdornment(mSchema->publicKeyHashCoder().certificateKey(),
                          new Tokend::LinkedRecordAdornment(eCryCert));
#endif
	static const char *applets[][3] = {
		{(char*)kSelectCACNGObjectPN, "PNTB", "PNVB"},
		{(char*)kSelectCACNGObjectPL, "PLTB", "PLVB"},
		{(char*)kSelectCACNGObjectBS, "BSTB", "BSVB"},
		{(char*)kSelectCACNGObjectOB, "OBTB", "OBVB"},
		{NULL, NULL, NULL}
	};
	for (int i = 0; applets[i][0]; i++) {
		shared_ptr<CACNGSelectable> applet(new CACNGCacApplet(
			*this,
			pkiApplet,
			byte_string(applets[i][0], applets[i][0] + 7)));
		shared_ptr<CACNGReadable> tbuffer(new CACNGCacBufferObject(*this, applet, true));
		shared_ptr<CACNGReadable> vbuffer(new CACNGCacBufferObject(*this, applet, false));
		dataRelation.insertRecord(new CACNGDataRecord(tbuffer, applets[i][1]));
		dataRelation.insertRecord(new CACNGDataRecord(vbuffer, applets[i][2]));
	}

	/* PIV AUTH KEY */
	byte_string pivAuthOid(oidX509CertificatePIVAuthentication, oidX509CertificatePIVAuthentication + sizeof(oidX509CertificatePIVAuthentication));
	
	shared_ptr<CACNGIDObject> pivAuthObject(new CACNGPivIDObject(*this, pivApplet, "Piv Authentication Certificate", pivAuthOid, PIV_KEYREF_PIV_AUTHENTICATION));
	RefPointer<Tokend::Record> pivAuthCert(new CACNGCertificateRecord(pivAuthObject, "Piv Authentication Certificate"));

	certRelation.insertRecord(pivAuthCert);
	
	RefPointer<Tokend::Record> pivAuthKey(new CACNGKeyRecord(pivAuthObject, "Piv Authentication Private Key",
		privateKeyRelation.metaRecord(), true, true));
	privateKeyRelation.insertRecord(pivAuthKey);

	pivAuthKey->setAdornment(mSchema->publicKeyHashCoder().certificateKey(),
                        new Tokend::LinkedRecordAdornment(pivAuthCert));
	
	secdebug("populate", "CACNGToken::populate() end");
}

