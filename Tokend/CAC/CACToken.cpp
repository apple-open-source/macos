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
	unsigned char result[MAX_BUFFER_SIZE];
	size_t resultLength = sizeof(result);

	transmit(applet, applet_length, result, resultLength);
	// If the select command failed this isn't a cac card, so we are done.
	if (resultLength < 2 || result[resultLength - 2] != 0x90 &&
		result[resultLength - 2] != 0x61 /* || result[resultLength - 1] != 0x0D */)
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
{ secdebug("adhoc", "returning cached PIN status 0x%x", mPinStatus);
		return mPinStatus;
}

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

secdebug("adhoc", "new PIN status=0x%x", mPinStatus);
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

/*
	See NIST IR 6887 Ð 2003 EDITION, GSC-IS VERSION 2.1
	5.3.4 Generic Container Provider Virtual Machine Card Edge Interface
	for a description of how this command works
	
	READ BUFFER 0x80 0x52 Off/H Off/L 0x02 <buffer & number bytes to read> Ð 

*/

#if 0
        unsigned char toread = bytes_left > MAX_READ ? MAX_READ : bytes_left;
	unsigned char apdu[] = { 0x80, 0x52,
            offset >> 8, offset & 0xFF,
            0x02, (getTB ? 0x01 : 0x02),
            toread };

#define TBD_ZERO						0x00

#define CAC_CLA_STANDARD				CLA_STANDARD	// 00
#define CAC_INS_GET_DATA				INS_GET_DATA	0xCB	// [SP800731 7.1.2]

//										0x00				0xCB
#define CAC_GETDATA_APDU			CAC_CLA_STANDARD, CAC_INS_GET_DATA, 0x3F, 0xFF
// Template for getting data
//									 00 CB 3F FF		Lc		Tag	  Len	    OID1	  OID2	  OID3
#define PIV_GETDATA_APDU_TEMPLATE	PIV_GETDATA_APDU, TBD_ZERO, 0x5C, TBD_ZERO, TBD_FF, TBD_FF, TBD_FF

#define PIV_GETDATA_APDU_INDEX_LEN		4	// Index into APDU for APDU data length (this is TLV<OID>) [Lc]
#define PIV_GETDATA_APDU_INDEX_OIDLEN	6	// Index into APDU for requested length of data
#define PIV_GETDATA_APDU_INDEX_OID		7	// Index into APDU for object ID

#define CAC_GETDATA_CONT_APDU_TEMPLATE	0x00, 0xC0, 0x00, 0x00, TBD_ZERO

#define CAC_GETDATA_CONT_APDU_INDEX_LEN	4	// Index into CONT APDU for requested length of data

void CACToken::getDataCore(const unsigned char *oid, size_t oidlen, const char *description, bool isCertificate,
	bool allowCaching, CssmData &data)
{
	unsigned char result[MAX_BUFFER_SIZE];
	size_t resultLength = sizeof(result);
	size_t returnedDataLength = 0;

	// The APDU only has space for a 3 byte OID
	if (oidlen != 3)
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	
	if (!mReturnedData)
	{
		mReturnedData = new unsigned char[PIV_MAX_DATA_SIZE];
		if (!mReturnedData)
			CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR);
	}
	
	const unsigned char dataFieldLen = 0x05;	// doc says must be 16, but in pratice it is 5
	unsigned char initialapdu[] = { PIV_GETDATA_APDU_TEMPLATE };

	initialapdu[PIV_GETDATA_APDU_INDEX_LEN] = dataFieldLen;
	initialapdu[PIV_GETDATA_APDU_INDEX_OIDLEN] = oidlen;
	memcpy(initialapdu + PIV_GETDATA_APDU_INDEX_OID, oid, oidlen);

	unsigned char continuationapdu[] = { PIV_GETDATA_CONT_APDU_TEMPLATE };
	
	unsigned char *apdu = initialapdu;
	size_t apduSize = sizeof(initialapdu);

	selectDefault();
	// Talk to token here to get data
	{
		PCSC::Transaction _(*this);

		uint32_t rx;
		do
		{
			resultLength = sizeof(result);	// must reset each time
			transmit(apdu, apduSize, result, resultLength);
			if (resultLength < 2)
				break;
			rx = (result[resultLength - 2] << 8) + result[resultLength - 1];
			secdebug("pivtokend", "exchangeAPDU result %02X", rx);

			if ((rx & 0xFF00) != SCARD_BYTES_LEFT_IN_SW2 &&
				(rx & 0xFF00) != SCARD_SUCCESS)
				PIVError::check(rx);

			// Switch to the continuation APDU after first exchange
			apdu = continuationapdu;
			apduSize = sizeof(continuationapdu);
			
			memcpy(mReturnedData + returnedDataLength, result, resultLength - 2);
			returnedDataLength += resultLength - 2;
			
			// Number of bytes to fetch next time around is in the last byte returned.
			// For all except the penultimate read, this is 0, indicating that the
			// token should read all bytes.
			
			*(apdu + PIV_GETDATA_CONT_APDU_INDEX_LEN) = static_cast<unsigned char>(rx & 0xFF);
			
		} while ((rx & 0xFF00) == SCARD_BYTES_LEFT_IN_SW2);
	}

	dumpDataRecord(mReturnedData, returnedDataLength, oid);
	
	// Start to parse the BER-TLV encoded data. In the end, we only return the
	// main data part of this but we need to step through the rest first
	// The certficates are the only types we parse here

	if (returnedDataLength>0)
	{
		const unsigned char *pd = &mReturnedData[0];
		if (*pd != PIV_GETDATA_RESPONSE_TAG)
			PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
		pd++;

		if (isCertificate)
			processCertificateRecord(pd, returnedDataLength, oid, description, data);
		else
		{
			data.Data = mReturnedData;
			data.Length = returnedDataLength;
		}

		if (allowCaching)
			cacheObject(0, description, data);
	}
	else
	{
		data.Data = mReturnedData;
		data.Length = 0;
	}
}
#endif


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
		/*	uint32_t cacreturn = */ getData(result, resultLength);

			/* Score of 200 to ensure that CAC "wins" for Hybrid CAC/PIV cards */
			score = 200;
			// Now stick in the bytes returned by getData into the
			// tokenUid.
			if(resultLength > 20)
			{
				sprintf(tokenUid,
					"CAC-%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X",
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
				snprintf(tokenUid, TOKEND_MAX_UID, "CAC-%s", buffer);
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
	Allocator &alloc = Allocator::standard();
	
	if (unsigned pin = pinFromAclTag(tag, "?")) {
		static AutoAclEntryInfoList acl;
		acl.clear();
		acl.allocator(alloc);
		uint32_t status = this->pinStatus(pin);
		if (status == SCARD_SUCCESS)
			acl.addPinState(pin, CSSM_ACL_PREAUTH_TRACKING_AUTHORIZED);
		else if (status >= CAC_AUTHENTICATION_FAILED_0 && status <= CAC_AUTHENTICATION_FAILED_3)
			acl.addPinState(pin, 0, status - CAC_AUTHENTICATION_FAILED_0);
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
		privateKeyRelation.metaRecord()));
	RefPointer<Tokend::Record> eSigKey(new CACKeyRecord(
		kSelectCACAppletPKIESig, "Email Signing Private Key",
		privateKeyRelation.metaRecord()));
	RefPointer<Tokend::Record> eCryKey(new CACKeyRecord(
		kSelectCACAppletPKIECry, "Email Encryption Private Key",
		privateKeyRelation.metaRecord()));

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

