/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 *  CACToken.cpp
 *  TokendMuscle
 */

#include "CACToken.h"

#include "Adornment.h"
#include "AttributeCoder.h"
#include "CACError.h"
#include "CACRecord.h"
#include "CACSchema.h"
#include <security_cdsa_client/aclclient.h>
#include <map>
#include <vector>

using CssmClient::AclFactory;

#define CLA_STANDARD      0x00
#define INS_SELECT_FILE   0xA4
#define INS_GET_DATA      0xCA

#define SELECT_APPLET  CLA_STANDARD, INS_SELECT_FILE, 0x04, 0x00

#define SELECT_CAC_APPLET  SELECT_APPLET, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x79

#define SELECT_CAC_APPLET_PKI  SELECT_CAC_APPLET, 0x01
#define SELECT_CAC_APPLET_TLB  SELECT_CAC_APPLET, 0x02
#define SELECT_CAC_APPLET_PIN  SELECT_CAC_APPLET, 0x03

static const unsigned char kSelectCardManagerApplet[] =
	{ SELECT_APPLET, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00 };

static const unsigned char kSelectCACAppletPKIID[]   =
	{ SELECT_CAC_APPLET_PKI, 0x00 };
static const unsigned char kSelectCACAppletPKIESig[] =
	{ SELECT_CAC_APPLET_PKI, 0x01 };
static const unsigned char kSelectCACAppletPKIECry[] =
	{ SELECT_CAC_APPLET_PKI, 0x02 };
static const unsigned char kSelectCACAppletPN[]      =
	{ SELECT_CAC_APPLET_TLB, 0x00 };
static const unsigned char kSelectCACAppletPL[]      =
	{ SELECT_CAC_APPLET_TLB, 0x01 };
static const unsigned char kSelectCACAppletBS[]      =
	{ SELECT_CAC_APPLET_TLB, 0x02 };
static const unsigned char kSelectCACAppletOB[]      =
	{ SELECT_CAC_APPLET_TLB, 0x03 };
static const unsigned char kSelectCACAppletPIN[]     =
	{ SELECT_CAC_APPLET_PIN, 0x00 };


CACToken::CACToken() :
	mCurrentApplet(NULL),
	mPinStatus(0)
{
	mTokenContext = this;
	mSession.open();
}

CACToken::~CACToken()
{
	delete mSchema;
}

bool CACToken::identify()
{
	try
	{
		select(kSelectCACAppletPKIID);
		return true;
	}
	catch (const PCSC::Error &error)
	{
		if (error.error == SCARD_E_PROTO_MISMATCH)
			return false;
		throw;
	}
}

void CACToken::select(const unsigned char *applet)
{
	// If we are already connected and our current applet is already selected
	// we are done.
	if (isInTransaction() && mCurrentApplet == applet)
		return;

	// For CAC all applet selectors have the same size.
	size_t applet_length = sizeof(kSelectCACAppletPKIID);
	unsigned char result[2];
	size_t resultLength = sizeof(result);

	transmit(applet, applet_length, result, resultLength);
	// If the select command failed this isn't a cac card, so we are done.
	if (resultLength != 2 || result[0] != 0x61 /* || result[1] != 0x0D */)
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);

	if (isInTransaction())
		mCurrentApplet = applet;
}

uint32_t CACToken::exchangeAPDU(const unsigned char *apdu, size_t apduLength,
	unsigned char *result, size_t &resultLength)
{
	size_t savedLength = resultLength;

	transmit(apdu, apduLength, result, resultLength);
	if (resultLength == 2 && result[0] == 0x61)
	{
		resultLength = savedLength;
		uint8 expectedLength = result[1];
		unsigned char getResult[] = { 0x00, 0xC0, 0x00, 0x00, expectedLength };
		transmit(getResult, sizeof(getResult), result, resultLength);
		if (resultLength - 2 != expectedLength)
        {
            if (resultLength < 2)
                PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
            else
                CACError::throwMe((result[resultLength - 2] << 8)
					+ result[resultLength - 1]);
        }
	}

	if (resultLength < 2)
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);

    return (result[resultLength - 2] << 8) + result[resultLength - 1];
}

void CACToken::didDisconnect()
{
	PCSC::Card::didDisconnect();
	mCurrentApplet = NULL;
	mPinStatus = 0;
}

void CACToken::didEnd()
{
	PCSC::Card::didEnd();
	mCurrentApplet = NULL;
	mPinStatus = 0;
}

void CACToken::changePIN(int pinNum,
	const unsigned char *oldPin, size_t oldPinLength,
	const unsigned char *newPin, size_t newPinLength)
{
	if (pinNum != 1)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	if (oldPinLength < 4 || oldPinLength > 8 ||
		newPinLength < 4 || newPinLength > 8)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);

	PCSC::Transaction _(*this);
	/* Change pin only works if one of the CAC applets are selected. */
	select(kSelectCACAppletPIN);

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

	mPinStatus = exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	memset(apdu + 5, 0, 16);
	CACError::check(mPinStatus);
}

uint32_t CACToken::pinStatus(int pinNum)
{
	if (pinNum != 1)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	if (mPinStatus && isInTransaction())
		return mPinStatus;

	PCSC::Transaction _(*this);
	/* Verify pin only works if one of the CAC applets are selected. */
	if (mCurrentApplet != kSelectCACAppletPKIID
		&& mCurrentApplet != kSelectCACAppletPKIESig
		&& mCurrentApplet != kSelectCACAppletPKIECry
		&& mCurrentApplet != kSelectCACAppletPN
		&& mCurrentApplet != kSelectCACAppletPL
		&& mCurrentApplet != kSelectCACAppletBS
		&& mCurrentApplet != kSelectCACAppletOB
		&& mCurrentApplet != kSelectCACAppletPIN)
	{
		select(kSelectCACAppletPKIESig);
	}

	unsigned char result[2];
	size_t resultLength = sizeof(result);
	unsigned char apdu[] = { 0x80, 0x20, 0x00, 0x00 };

	mPinStatus = exchangeAPDU(apdu, 4, result, resultLength);
	if ((mPinStatus & 0xFF00) != 0x6300
		&& mPinStatus != SCARD_AUTHENTICATION_BLOCKED)
		CACError::check(mPinStatus);

	return mPinStatus;
}

void CACToken::verifyPIN(int pinNum,
	const unsigned char *pin, size_t pinLength)
{
	if (pinNum != 1)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	if (pinLength < 4 || pinLength > 8)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);

	PCSC::Transaction _(*this);
	/* Verify pin only works if one of the CAC applets are selected. */
	if (mCurrentApplet != kSelectCACAppletPKIID
		&& mCurrentApplet != kSelectCACAppletPKIESig
		&& mCurrentApplet != kSelectCACAppletPKIECry
		&& mCurrentApplet != kSelectCACAppletPN
		&& mCurrentApplet != kSelectCACAppletPL
		&& mCurrentApplet != kSelectCACAppletBS
		&& mCurrentApplet != kSelectCACAppletOB
		&& mCurrentApplet != kSelectCACAppletPIN)
	{
		select(kSelectCACAppletPKIESig);
	}

	unsigned char apdu[] =
	{
		0x80, 0x20, 0x00, 0x00, 0x08,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};

#if defined(CAC_PROTECTED_MODE)
	memcpy(apdu + 5, "77777777", 8);
#else
	memcpy(apdu + 5, pin, pinLength);
#endif

	unsigned char result[2];
	size_t resultLength = sizeof(result);

	mPinStatus = exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	memset(apdu + 5, 0, 8);
	CACError::check(mPinStatus);
	// Start a new transaction which we never get rid of until someone calls
	// unverifyPIN()
	begin();
}

void CACToken::unverifyPIN(int pinNum)
{
	if (pinNum != -1)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	end(SCARD_RESET_CARD);
}

uint32_t CACToken::getData(unsigned char *result, size_t &resultLength)
{
	PCSC::Transaction _(*this);
	try
	{
		select(kSelectCardManagerApplet);
	}
	catch (const PCSC::Error &error)
	{
		return error.error;
	}

	unsigned char apdu[] = { 0x80, INS_GET_DATA, 0x9F, 0x7F, 0x2D };
	return exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
}

uint32 CACToken::probe(SecTokendProbeFlags flags,
	char tokenUid[TOKEND_MAX_UID])
{
	uint32 score = Tokend::ISO7816Token::probe(flags, tokenUid);

	bool doDisconnect = false; /*!(flags & kSecTokendProbeKeepToken); */

	try
	{
		if (!identify())
			doDisconnect = true;
		else
		{
			unsigned char result[0x2F];
			size_t resultLength = sizeof(result);
			uint32_t cacreturn = getData(result, resultLength);

			if (cacreturn != SCARD_SUCCESS || resultLength != 0x2F)
			{
				// This looks like is a CAC card, but somehow we can't get a
				// TokendUid for it.  So perhaps it's a non Java CAC card.
				score = 100;
			}
			else
			{
				score = 200;				
				// Now stick in the bytes returned by getData into the
				// tokenUid.
				for (uint32_t ix = 0x00; ix < 0x0A; ++ix)
				{
					sprintf(tokenUid,
						"CAC-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X",
						result[3], result[4], result[5], result[6], result[19],
						result[20], result[15], result[16], result[17],
						result[18]);
				}
				Tokend::ISO7816Token::name(tokenUid);
				secdebug("probe", "recognized %s", tokenUid);
			}
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

void CACToken::establish(const CSSM_GUID *guid, uint32 subserviceId,
	SecTokendEstablishFlags flags, const char *cacheDirectory,
	const char *workDirectory, char mdsDirectory[PATH_MAX],
	char printName[PATH_MAX])
{
	Tokend::ISO7816Token::establish(guid, subserviceId, flags,
		cacheDirectory, workDirectory, mdsDirectory, printName);

	mSchema = new CACSchema();
	mSchema->create();

	populate();
}

//
// Database-level ACLs
//
void CACToken::getOwner(AclOwnerPrototype &owner)
{
	// we don't really know (right now), so claim we're owned by PIN #0
	if (!mAclOwner)
	{
		mAclOwner.allocator(Allocator::standard());
		mAclOwner = AclFactory::PinSubject(Allocator::standard(), 0);
	}
	owner = mAclOwner;
}


void CACToken::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	//uint32_t cacresult = pinStatus();
	Allocator &alloc = Allocator::standard();

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
		mAclEntries.addPin(AclFactory::PromptPWSubject(alloc, CssmData()), 1);
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}


#pragma mark ---------------- CAC Specific --------------

void CACToken::populate()
{
	secdebug("populate", "CACToken::populate() begin");
	Tokend::Relation &certRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE);
	Tokend::Relation &privateKeyRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_PRIVATE_KEY);
	Tokend::Relation &dataRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_GENERIC);

	RefPointer<Tokend::Record> idCert(new CACCertificateRecord(
		kSelectCACAppletPKIID, "Identity Certificate"));
	RefPointer<Tokend::Record> eSigCert(new CACCertificateRecord(
		kSelectCACAppletPKIESig, "Email Signing Certificate"));
	RefPointer<Tokend::Record> eCryCert(new CACCertificateRecord(
		kSelectCACAppletPKIECry, "Email Encryption Certificate"));

	certRelation.insertRecord(idCert);
	certRelation.insertRecord(eSigCert);
	certRelation.insertRecord(eCryCert);

	RefPointer<Tokend::Record> idKey(new CACKeyRecord(
		kSelectCACAppletPKIID, "Identity Private Key",
		privateKeyRelation.metaRecord(), true));
	RefPointer<Tokend::Record> eSigKey(new CACKeyRecord(
		kSelectCACAppletPKIESig, "Email Signing Private Key",
		privateKeyRelation.metaRecord(), true));
	RefPointer<Tokend::Record> eCryKey(new CACKeyRecord(
		kSelectCACAppletPKIECry, "Email Encryption Private Key",
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

	dataRelation.insertRecord(new CACTBRecord(kSelectCACAppletPN, "PNTB"));
	dataRelation.insertRecord(new CACVBRecord(kSelectCACAppletPN, "PNVB"));
	dataRelation.insertRecord(new CACTBRecord(kSelectCACAppletPL, "PLTB"));
	dataRelation.insertRecord(new CACVBRecord(kSelectCACAppletPL, "PLVB"));
	dataRelation.insertRecord(new CACTBRecord(kSelectCACAppletBS, "BSTB"));
	dataRelation.insertRecord(new CACVBRecord(kSelectCACAppletBS, "BSVB"));
	dataRelation.insertRecord(new CACTBRecord(kSelectCACAppletOB, "OBTB"));
	dataRelation.insertRecord(new CACVBRecord(kSelectCACAppletOB, "OBVB"));

	secdebug("populate", "CACToken::populate() end");
}

/* arch-tag: 36F733B4-0DBC-11D9-914C-000A9595DEEE */
