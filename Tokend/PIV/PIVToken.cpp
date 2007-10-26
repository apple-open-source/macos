/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
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
 *  PIVToken.cpp
 *  TokendPIV
 */

#include "PIVToken.h"
#include "PIVDefines.h"
#include "PIVCCC.h"

#include "Adornment.h"
#include "AttributeCoder.h"
#include "PIVError.h"
#include "PIVRecord.h"
#include "PIVSchema.h"
#include <security_cdsa_client/aclclient.h>
#include <map>
#include <vector>
#include <zlib.h>
#include <CoreFoundation/CFString.h>

using CssmClient::AclFactory;

/*
		APDU: 00 A4 04 00 06 A0 00 00 00 01 01 
		APDU: 6A 82		==> applet not found (NISTIR6887 5.3.3.2/ISO 7816-4)
*/

#pragma mark ---------- PIV defines ----------


// Result codes [Ref NISTIR6887 5.1.1.1 Get Response APDU]

#define PIV_RESULT_SUCCESS_SW1		0x90	//[ref SCARD_SUCCESS]
#define PIV_RESULT_SUCCESS_SW2		(unsigned char )0x00
#define PIV_RESULT_CONTINUATION_SW1	(unsigned char )0x61

/*
	00 A4 04 00 07 A0 00 00 01 51 00 00		[A0000001510000]
	00 A4 04 00 06 A0 00 00 00 01 01 

	00 A4 04 00 0B A0 00 00 03 08 00 00 10 00 01 00		
		Select applet/object	(00 A4 )
		select by AID			(04)
		P2						(00)
		Lc (length of data)		(0B)
		Applet id				A0 00 00 03 08 00 00 10 00 01 00 (A000000308000010000100)
								A0 00 00 03 08 00 00 10 00 01 00
	1. Send SELECT card command with, 
	 
	2. Send SELECT card command without the version number, 
	0 10 00 
	...
	AID == A0 00 00 03 08 00 00 10 00 01 00 
	...
	AID == A0 00 00 03 08 00 00 
*/

static const unsigned char kSelectPIVApplet[] = { SELECT_PIV_APPLET_LONG };	// or SELECT_PIV_APPLET_SHORT

static const unsigned char kUniversalAID[] = { 0xA0, 0x00, 0x00, 0x01, 0x16, 0xDB, 0x00 };

#pragma mark ---------- Object IDs ----------

static const unsigned char oidCardCapabilityContainer[] = { PIV_OBJECT_ID_CARD_CAPABILITY_CONTAINER };
static const unsigned char oidCardHolderUniqueIdentifier[] = { PIV_OBJECT_ID_CARDHOLDER_UNIQUEID };
static const unsigned char oidCardHolderFingerprints[] = { PIV_OBJECT_ID_CARDHOLDER_FINGERPRINTS };
static const unsigned char oidPrintedInformation[] = { PIV_OBJECT_ID_PRINTED_INFORMATION };
static const unsigned char oidCardHolderFacialImage[] = { PIV_OBJECT_ID_CARDHOLDER_FACIAL_IMAGE };
static const unsigned char oidX509CertificatePIVAuthentication[] = { PIV_OBJECT_ID_X509_CERTIFICATE_PIV_AUTHENTICATION };
static const unsigned char oidX509CertificateDigitalSignature[] = { PIV_OBJECT_ID_X509_CERTIFICATE_DIGITAL_SIGNATURE };
static const unsigned char oidX509CertificateKeyManagement[] = { PIV_OBJECT_ID_X509_CERTIFICATE_KEY_MANAGEMENT };
static const unsigned char oidX509CertificateCardAuthentication[] = { PIV_OBJECT_ID_X509_CERTIFICATE_CARD_AUTHENTICATION };


#pragma mark ---------- NO/MINOR MODIFICATION NEEDED ----------

PIVToken::PIVToken() :
	mReturnedData(NULL), mUncompressedData(NULL), mCardCapabilitiesContainer(NULL),
	mCurrentApplet(NULL), mPinStatus(0)
{
	mTokenContext = this;
	mSession.open();
}

PIVToken::~PIVToken()
{
	delete mSchema;
	delete mUncompressedData;
	delete mReturnedData;
	delete mCardCapabilitiesContainer;
}


void PIVToken::didDisconnect()
{
	PCSC::Card::didDisconnect();
	mCurrentApplet = NULL;
	mPinStatus = 0;
}

void PIVToken::didEnd()
{
	PCSC::Card::didEnd();
	mCurrentApplet = NULL;
	mPinStatus = 0;
}

void PIVToken::unverifyPIN(int pinNum)
{
	if (pinNum != -1)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	end(SCARD_RESET_CARD);
}

void PIVToken::establish(const CSSM_GUID *guid, uint32 subserviceId,
	SecTokendEstablishFlags flags, const char *cacheDirectory,
	const char *workDirectory, char mdsDirectory[PATH_MAX],
	char printName[PATH_MAX])
{
#ifdef _USECERTIFICATECOMMONNAME
	std::string commonName = authCertCommonName();
	::snprintf(printName, 40, "PIV-%s", commonName.c_str());
#else
	::snprintf(printName, 40, "PIV-%s", mCardCapabilitiesContainer->hexidentifier().c_str());
#endif	/* _USECERTIFICATECOMMONNAME */
	Tokend::ISO7816Token::name(printName);
	secdebug("pivtoken", "name: %s", printName);

	Tokend::ISO7816Token::establish(guid, subserviceId, flags,
		cacheDirectory, workDirectory, mdsDirectory, printName);

	mSchema = new PIVSchema();
	mSchema->create();

	populate();
}

//
// Database-level ACLs
//
void PIVToken::getOwner(AclOwnerPrototype &owner)
{
	// we don't really know (right now), so claim we're owned by PIN #0
	if (!mAclOwner)
	{
		mAclOwner.allocator(Allocator::standard());
		mAclOwner = AclFactory::PinSubject(Allocator::standard(), 0);
	}
	owner = mAclOwner;
}


void PIVToken::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	//uint32_t cacresult = pinStatus();
	Allocator &alloc = Allocator::standard();
	
	if (unsigned pin = pinFromAclTag(tag, "?")) {
		static AutoAclEntryInfoList acl;
		acl.clear();
		acl.allocator(alloc);
		uint32_t status = this->pinStatus(pin);
		if (status == SCARD_SUCCESS)
			acl.addPinState(pin, CSSM_ACL_PREAUTH_TRACKING_AUTHORIZED);
		else if (status >= PIV_AUTHENTICATION_FAILED_0 && status <= PIV_AUTHENTICATION_FAILED_3)
			acl.addPinState(pin, 0, status - PIV_AUTHENTICATION_FAILED_0);
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


#pragma mark ---------- MODIFICATION REQUIRED ----------

/* ---------------------------------------------------------------------------
 *
 *		The methods in this section should be usable with very minor or no
 *		modifications. For example, for a PKCS#11 based tokend, replace 
 *		mCurrentApplet with mObjectID or the like.
 *
 * ---------------------------------------------------------------------------
*/

uint32 PIVToken::probe(SecTokendProbeFlags flags, char tokenUid[TOKEND_MAX_UID])	// MODIFY
{
	/*
		In probe, try to figure out if this is your token. If it is, return
		a good score (e.g. 100-200) and set the tokenUid to something
		unique-ish. It can be completely token-specific information.
		If not, disconnect from the token and return 0.
	*/
	uint32 score = Tokend::ISO7816Token::probe(flags, tokenUid);

	bool doDisconnect = false; /*!(flags & kSecTokendProbeKeepToken); */

	try
	{
		if (!identify())
			doDisconnect = true;
		else
		{	
			if (!mCardCapabilitiesContainer)
				mCardCapabilitiesContainer = new PIVCCC();
#ifndef _USEFALLBACKTOKENUID
			CssmData cccdata;
			getDataCore(oidCardCapabilityContainer, sizeof(oidCardCapabilityContainer),
				"CCC", false, true, cccdata);
			mCardCapabilitiesContainer->set(cccdata);
			snprintf(tokenUid, TOKEND_MAX_UID, "PIV-%s", mCardCapabilitiesContainer->hexidentifier().c_str());

#else
			// You should put something to uniquely identify the token into
			// tokenUid if possible, since then caching of large items such
			// as certificates will be possible. Here we just put in some
			// random junk.
			unsigned char buffer[80];
			time_t now;
			struct tm* timestruct = localtime(&now);
			strftime(reinterpret_cast<char *>(buffer), 80, "%+", timestruct);			// like "date" output in shell
			snprintf(tokenUid, TOKEND_MAX_UID, "PIV-%s", buffer);
#endif
			score = 110;
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

void PIVToken::populate()
{
	/*
		@@@ To do:
		read and parse CCC record to find out if the card has all of the optional records
		before adding them
	*/
	
	secdebug("populate", "PIVToken::populate() begin");
	
	// These lines will be the same for any token with certs, keys, and
	// data records.
	Tokend::Relation &certRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE);
	Tokend::Relation &privateKeyRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_PRIVATE_KEY);
	Tokend::Relation &dataRelation =
		mSchema->findRelation(CSSM_DL_DB_RECORD_GENERIC);
	
	/*
		Table 1.  SP 800-73 Data Model Containers 

		RID 'A0 00 00 00 01 16' - ContainerID - Access Rule - Contact/Contactless - M/O 
		Card Capability Container				0xDB00 Read Always Contact Mandatory 
		CHUID Buffer							0x3000 Read Always Contact & Contactless Mandatory 
		PIV Authentication Certificate Buffer	0x0101 Read Always Contact Mandatory 
		Fingerprint Buffer						0x6010 PIN			Contact Mandatory 
		Printed Information Buffer				0x3001 PIN			Contact Optional 
		Facial Image Buffer						0x6030 PIN			Contact Optional 
		Digital Signature Certificate Buffer	0x0100 Read Always Contact Optional 
		Key Management Certificate Buffer		0x0102 Read Always Contact Optional 
		Card Authentication Certificate Buffer	0x0500 Read Always Contact  Optional 
		Security Object Buffer					0x9000 Read Always Contact Mandatory 
	*/

	// Since every object ID is 3 bytes long, this works
	const size_t sz = sizeof(oidCardCapabilityContainer);
	
	//	Card Capability Container 2.16.840.1.101.3.7.1.219.0 '5FC107' [Mandatory]
	dataRelation.insertRecord(new PIVDataRecord(oidCardCapabilityContainer, sz, "CCC"));

	//	Card Holder Unique Identifier 2.16.840.1.101.3.7.2.48.0 '5FC102'  [Mandatory] [CHUID]
	dataRelation.insertRecord(new PIVDataRecord(oidCardHolderUniqueIdentifier, sz, "CHUID"));

	//	Card Holder Fingerprints 2.16.840.1.101.3.7.2.96.16 '5FC103' [Mandatory]
	dataRelation.insertRecord(new PIVProtectedRecord(oidCardHolderFingerprints, sz, "FINGERPRINTS"));

	//	Printed Information 2.16.840.1.101.3.7.2.48.1 '5FC109' [Optional]
	dataRelation.insertRecord(new PIVProtectedRecord(oidPrintedInformation, sz, "PRINTDATA"));

	//	Card Holder Facial Image 2.16.840.1.101.3.7.2.96.48 '5FC108' O
	dataRelation.insertRecord(new PIVProtectedRecord(oidCardHolderFacialImage, sz, "FACIALIMAGE"));

	// Now describe the keys and certificates
	
	const unsigned char *certids[] = 
	{
		oidX509CertificatePIVAuthentication,
		oidX509CertificateDigitalSignature,
		oidX509CertificateKeyManagement,
		oidX509CertificateCardAuthentication
	};
	
	const char *certNames[] = 
	{
		"PIV Authentication Certificate",
		"Digital Signature Certificate",
		"Key Management Certificate",
		"Card Authentication Certificate"
	};
	
	const char *keyNames[] = 
	{
		"PIV Authentication Private Key",
		"Digital Signature Private Key",
		"Key Management Private Key",
		"Card Authentication Private Key"
	};
	
	for (unsigned int ix=0;ix<sizeof(certids);++ix)
	{
		RefPointer<Tokend::Record> cert(new PIVCertificateRecord(certids[ix], sz, certNames[ix]));
		certRelation.insertRecord(cert);

		RefPointer<Tokend::Record> key(new PIVKeyRecord(certids[ix], sz, keyNames[ix], privateKeyRelation.metaRecord(), true));
		privateKeyRelation.insertRecord(key);

		// The Adornment class links a particular PIVCertificateRecord 
		// with its corresponding PIVKeyRecord record
		key->setAdornment(mSchema->publicKeyHashCoder().certificateKey(),
							new Tokend::LinkedRecordAdornment(cert));
	}

	secdebug("populate", "PIVToken::populate() end");
}

bool PIVToken::identify()
{
	//	For the PIV identify function, just try to select the PIV applet.
	//	If it fails, this is not a PIV card.

	try
	{
//		PCSC::Transaction _(*this);
		select(kSelectPIVApplet, sizeof(kSelectPIVApplet));
		return true;
	}
	catch (const PCSC::Error &error)
	{
		if (error.error == SCARD_E_PROTO_MISMATCH)
			return false;
		throw;
	}
}

void PIVToken::changePIN(int pinNum,
	const unsigned char *oldPin, size_t oldPinLength,
	const unsigned char *newPin, size_t newPinLength)
{
	/*
		References:
		- 7.2.2 CHANGE REFERENCE DATA Card Command [SP800731]
	*/
	if (pinNum < PIV_VERIFY_KEY_NUMBER_DEFAULT || pinNum > PIV_VERIFY_KEY_NUMBER_MAX)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	if (oldPinLength < PIV_VERIFY_PIN_LENGTH_MIN || oldPinLength > PIV_VERIFY_PIN_LENGTH_MAX ||
		newPinLength < PIV_VERIFY_PIN_LENGTH_MIN || newPinLength > PIV_VERIFY_PIN_LENGTH_MAX)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);

	PCSC::Transaction _(*this);
	// Change pin requires that we select the default applet first
	select(kSelectPIVApplet, sizeof(kSelectPIVApplet));

	const unsigned char dataFieldLen = 0x10;	// doc says must be 16 (= 2x8)
	unsigned char apdu[] = { PIV_CHANGE_REFERENCE_DATA_APDU_TEMPLATE };

	apdu[PIV_VERIFY_APDU_INDEX_KEY] = static_cast<unsigned char>(pinNum & 0xFF);
	apdu[PIV_VERIFY_APDU_INDEX_LEN] = dataFieldLen;

	memcpy(apdu + PIV_VERIFY_APDU_INDEX_DATA, oldPin, oldPinLength);
	memcpy(apdu + PIV_CHANGE_REFERENCE_DATA_APDU_INDEX_DATA2, newPin, newPinLength);

	unsigned char result[2];
	size_t resultLength = sizeof(result);

	mPinStatus = exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	memset(apdu + PIV_VERIFY_APDU_INDEX_DATA, 0, dataFieldLen);
	PIVError::check(mPinStatus);
}

uint32_t PIVToken::pinStatus(int pinNum)
{
	/*
		Ref 5.1.2.4 Verify APDU  [NISTIR6887]

		Processing State returned in the Response Message 
		SW1 SW2	Meaning 
		63  00	Verification failed 
		63  CX	Verification failed, X indicates the number of further allowed retries 
		69  83	Authentication method blocked		[SCARD_AUTHENTICATION_BLOCKED]
		69  84	Referenced data deactivated			[SCARD_REFERENCED_DATA_INVALIDATED]
		6A  86	Incorrect parameters P1-P2			[SCARD_INCORRECT_P1_P2]
		6A  88	Reference data not found			[SCARD_REFERENCED_DATA_NOT_FOUND]
		90  00	Successful execution				[SCARD_SUCCESS]
	*/
	if (pinNum < PIV_VERIFY_KEY_NUMBER_DEFAULT || pinNum > PIV_VERIFY_KEY_NUMBER_MAX)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	if (mPinStatus && isInTransaction())
		return mPinStatus;

	PCSC::Transaction _(*this);
	// Verify pin requires that we select the default applet first
	if (mCurrentApplet != kSelectPIVApplet)
		select(kSelectPIVApplet, sizeof(kSelectPIVApplet));

	unsigned char apdu[] = { PIV_VERIFY_APDU_STATUS };
	apdu[PIV_VERIFY_APDU_INDEX_KEY] = 0x80;//static_cast<unsigned char>(pinNum & 0xFF);

	unsigned char result[2];
	size_t resultLength = sizeof(result);

	mPinStatus = exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	if (((mPinStatus & 0xFF00) != SCARD_AUTHENTICATION_FAILED) &&
		(mPinStatus != SCARD_AUTHENTICATION_BLOCKED))
		PIVError::check(mPinStatus);

	if ((mPinStatus & 0xFF00) == SCARD_AUTHENTICATION_FAILED)
		secdebug("pivtoken", "pinStatus: %d authentication attempts remaining", (mPinStatus & 0x000F));
	else
	if	(mPinStatus == SCARD_AUTHENTICATION_BLOCKED)
		secdebug("pivtoken", "pinStatus: CARD IS BLOCKED");

	return mPinStatus;
}

//      00 20 00 80 08 31 32 33 34 35 36 FF FF
//APDU: 00 20 00 01 08 31 32 33 34 35 36 FF FF 
//APDU: 6A 88 

void PIVToken::verifyPIN(int pinNum,
	const unsigned char *pin, size_t pinLength)
{
	// 5.1.2.4 Verify APDU [NISTIR6887]
	
	if (pinNum < PIV_VERIFY_KEY_NUMBER_DEFAULT || pinNum > PIV_VERIFY_KEY_NUMBER_MAX)
		CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);

	if (pinLength < PIV_VERIFY_PIN_LENGTH_MIN || pinLength > PIV_VERIFY_PIN_LENGTH_MAX)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);

	PCSC::Transaction _(*this);
	// Verify pin requires that we select the default applet first
	if (mCurrentApplet != kSelectPIVApplet)
		select(kSelectPIVApplet, sizeof(kSelectPIVApplet));

	const unsigned char dataFieldLen = 8;	// doc says must be 8
	
	unsigned char apdu[] = { PIV_VERIFY_APDU_TEMPLATE };

	apdu[PIV_VERIFY_APDU_INDEX_KEY] = 0x80;//static_cast<unsigned char>(pinNum & 0xFF);
	apdu[PIV_VERIFY_APDU_INDEX_LEN] = dataFieldLen;

	memcpy(apdu + PIV_VERIFY_APDU_INDEX_DATA, pin, pinLength);

	unsigned char result[2];
	size_t resultLength = sizeof(result);

	mPinStatus = exchangeAPDU(apdu, sizeof(apdu), result, resultLength);
	memset(apdu + PIV_VERIFY_APDU_INDEX_DATA, 0, dataFieldLen);
	PIVError::check(mPinStatus);
	// Start a new transaction which we never get rid of until someone calls
	// unverifyPIN()
	begin();
}


#pragma mark ---------------- TOKEN Specific/Utility --------------


/* ---------------------------------------------------------------------------
 *
 *		The methods in this section are useful utility functions for Java
 *		cards, but may be useful for other tokens as well with appropriate
 *		changes.
 *
 * ---------------------------------------------------------------------------
*/

void PIVToken::select(const unsigned char *applet, size_t appletLength)
{
	/*
		References:
		- 2.3.3.3.1 SELECT APDU [SP800731]
		- 5.1.1.4 Select File APDU [NISTIR6887]
		
		Data Field returned in the Response Message 
		If P2 is set to 0x00, data is returned as per ISO 7816-4 [ISO4]. 
		If P2 is set to 0x0C, no data is returned. 

		Processing State returned in the Response Message 
		
		SW1 SW2	Meaning 
		62  83	Selected file deactivated 
		62  84	FCI not formatted according to ISO 7816-4 Section 5.1.5 
		6A  81	Function not supported 
		6A  82	File not found 
		6A  86	Incorrect parameters P1-P2 
		6A  87	Lc inconsistent with P1-P2 
		90  00	Successful execution
	*/
	
	secdebug("pivtoken", "select BEGIN");
	// If we are already connected and our current applet is already selected we are done.
	if (isInTransaction() && mCurrentApplet == applet)
		return;

//	unsigned char result[2];
	// Result must be large enough for whole response
	unsigned char result[1024];
	size_t resultLength = sizeof(result);
	bool failed = false;

	try
	{
//		transmit(applet, appletLength, result, resultLength);
		exchangeAPDU(applet, appletLength, result, resultLength);
	}
	catch (const PCSC::Error &error)
	{
		secdebug("pivtoken", "select transmit error: %ld (0x%04lX)]", error.error, error.error);
		if (error.error == SCARD_E_PROTO_MISMATCH)
			return;
		failed = true;
	}
	catch (...)
	{
		secdebug("pivtoken", "select transmit unknown failure");
		failed = true;
	}
	//PCSC::Error Transaction failed. (-2146435050) osStatus -2147416063
	// We could return a more specific error based on the codes above
	uint16_t rx = (result[resultLength - 2] << 8) | result[resultLength - 1];

	if (failed || (rx != SCARD_SUCCESS))
	{
		secdebug("pivtoken", "select END [FAILURE %02X %02X]", 
			result[resultLength - 2], result[resultLength - 1]);
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}

	if (isInTransaction())
		mCurrentApplet = applet;
		
	secdebug("pivtoken", "select END [SUCCESS]");
}

void PIVToken::selectDefault()
{
//	PCSC::Transaction _(*this);
	select(kSelectPIVApplet, sizeof(kSelectPIVApplet));
}

uint32_t PIVToken::exchangeAPDU(const unsigned char *apdu, size_t apduLength,
	unsigned char *result, size_t &resultLength)
{
	size_t savedLength = resultLength;

	transmit(apdu, apduLength, result, resultLength);
	if (resultLength == 2 && result[0] == PIV_RESULT_CONTINUATION_SW1)
	{
		resultLength = savedLength;
		uint8 expectedLength = result[1];
		unsigned char getResult[] = { 0x00, 0xC0, 0x00, 0x00, expectedLength, };
		transmit(getResult, sizeof(getResult), result, resultLength);
		if (resultLength - 2 != expectedLength)
        {
            if (resultLength < 2)
                PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
            else
                PIVError::throwMe((result[resultLength - 2] << 8)
					+ result[resultLength - 1]);
        }
	}

	if (resultLength < 2)
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);

    return (result[resultLength - 2] << 8) + result[resultLength - 1];
}

/*
	BER-TLV
	Reference: http://www.cardwerk.com/smartcards/smartcard_standard_ISO7816-4_annex-d.aspx
	
	In short form, the length field consists of a single byte where the bit B8 shall be set to 0 and
	the bits B7-B1 shall encode an integer equal to the number of bytes in the value field. Any length
	from 0-127 can thus be encoded by 1 byte.

	In long form, the length field consists of a leading byte where the bit B8 shall be set to 1 and
	the B7-B1 shall not be all equal, thus encoding a positive integer equal to the number of subsequent
	bytes in the length field. Those subsequent bytes shall encode an integer equal to the number of bytes
	in the value field. Any length within the APDU limit (up to 65535) can thus be encoded by 3 bytes.

	NOTE - ISO/IEC 7816 does not use the indefinite lengths specified by the basic encoding rules of
	ASN.1 (see ISO/IEC 8825).
	
	Sample data (from a certficate GET DATA):
	
	00000000  53 82 04 84 70 82 04 78  78 da 33 68 62 db 61 d0 
	00000010  c4 ba 60 01 33 13 23 13  13 97 e2 dc 88 f7 0c 40
	00000020  20 da 63 c0 cb c6 a9 d5  e6 d1 f6 9d 97 91 91 95 
	....
	00000460  1f 22 27 83 ef fe ed 5e  7a f3 e8 b6 dc 6b 3f dc
	00000470  4c be bc f5 bf f2 70 7e  6b d0 4c 00 80 0d 3f 1f 
	00000480  71 01 80 72 03 49 44 41
	
*/

uint32_t PIVToken::parseBERLength(const unsigned char *&pber, uint32_t &lenlen)
{
	// Parse a BER length field. Return the value of the length and update
	// the pointer to the byte after the length field. "lenlen" is the length
	// in bytes of the length field just parsed.
	uint8_t ch = *pber++;
	if (!(ch & 0x80))	// single byte
	{
		lenlen = 1;
		return static_cast<uint32_t>(ch);
	}
	uint32_t result = 0;
	lenlen = ch & 0x7F;
	for (uint32_t ix=0;ix<lenlen;++ix,pber++)
		result = (result << 8 ) | static_cast<uint32_t>(*pber);
	return result;
}

/*
	This is where the actual data for a certificate or other data is retrieved from the token.
	
	Here is a sample exchange

	APDU: 00 CB 3F FF 05 5C 03 5F C1 05 
	APDU: 61 00 

	APDU: 00 C0 00 00 00 
	APDU: 53 82 04 84 70 82 ... 61 00

	APDU: 00 C0 00 00 00 
	APDU: 68 82 8C 52 65 ... 61 88 

	APDU: 00 C0 00 00 88 
	APDU: 50 D0 B2 A2 EF ... 90 00
*/


void PIVToken::getDataCore(const unsigned char *oid, size_t oidlen, const char *description, bool isCertificate,
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

void PIVToken::processCertificateRecord(const unsigned char *&pber, uint32_t berLen, const unsigned char *oid, const char *description, CssmData &data)
{
	// Assumes that we are pointing to the byte right after PIV_GETDATA_RESPONSE_TAG (0x53)
	// We only use "berLen" for consistency checking
	
	const unsigned char *pCertificateData = NULL;
	uint32_t certificateDataLength = 0;
	uint32_t lenlen = 0;
	bool isCompressed = false;
	
	// 00000000  53 82 04 84 70 82 04 78  78 da 33 68 62 db 61 d0 
	uint32_t overallLength = PIVToken::parseBERLength(pber, lenlen);
	if (overallLength > berLen)
		PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);

	for (int remaining=overallLength-1;remaining>0;)
	{
		uint32_t datalen = 0;
		uint8_t tag = *pber++;
		remaining--;
		switch (tag)
		{
		case PIV_GETDATA_TAG_CERTIFICATE:			// 0x70
			datalen = certificateDataLength = PIVToken::parseBERLength(pber, lenlen);
			pCertificateData = pber;
			break;
		case PIV_GETDATA_TAG_CERTINFO:				// 0x71
			datalen = PIVToken::parseBERLength(pber, lenlen);	// should be 1
			secdebug("pivtokend", "CertInfo byte: %02X", *pber);
			isCompressed = *pber & PIV_GETDATA_COMPRESSION_MASK;
			break;
		case PIV_GETDATA_TAG_MSCUID:				// 0x72
			datalen = PIVToken::parseBERLength(pber, lenlen);	// should be 3
			break;
		case PIV_GETDATA_TAG_ERRORDETECTION:
			break;
		case 0:
		case 0xFF:
			break;
		default:
			PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
			break;
		}
		remaining -= (datalen + lenlen);
		pber += datalen;
	}

	if (isCompressed)
	{
		/* The certificate is compressed */
		secdebug("pivtokend", "uncompressing compressed %s", description);
		dumpDataRecord(pCertificateData, certificateDataLength, oid, "-compressedcert");
		if (!mUncompressedData)
		{
			mUncompressedData = new unsigned char[PIV_MAX_DATA_SIZE];
			if (!mUncompressedData)
				CssmError::throwMe(CSSM_ERRCODE_MEMORY_ERROR);
		}
		size_t uncompressedLength = PIV_MAX_DATA_SIZE;
		int rv = Z_ERRNO;
		int compTyp = compressionType(pCertificateData, certificateDataLength);
		switch (compTyp)
		{
		case kCompressionNone:
		case kCompressionUnknown:
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
			break;
		case kCompressionZlib:
			rv = uncompress(mUncompressedData, &uncompressedLength, pCertificateData,
				certificateDataLength);
			break;
		case kCompressionGzip:
			rv = PIVToken::uncompressGzip(mUncompressedData, &uncompressedLength,
				const_cast<unsigned char *>(pCertificateData), certificateDataLength);
			break;
		}
		if (rv != Z_OK)
		{
			secdebug("zlib", "uncompressing %s failed: %d [type=%d]", description, rv, compTyp);
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
		}

		data.Data = mUncompressedData;
		data.Length = uncompressedLength;
	}
	else
	{
		data.Data = const_cast<uint8 *>(pCertificateData);
		data.Length = certificateDataLength;
	}
	dumpDataRecord(data.Data, data.Length, oid, "-rawcert");
}

int PIVToken::compressionType(const unsigned char *pdata, size_t len)
{
	// Some ad-hoc stuff to guess at compression type
	if (len > 2 && pdata[0] == 0x1F && pdata[1] == 0x8B)
		return kCompressionGzip;
	if (len > 1 /*&& (in[0] & 0x10) == Z_DEFLATED*/)
		return kCompressionZlib;
	else
		return kCompressionUnknown;
}

int PIVToken::uncompressGzip(unsigned char *uncompressedData, size_t* uncompressedDataLength,
	unsigned char *compressedData, size_t compressedDataLength)
{
    z_stream dstream;					// decompression stream
	int windowSize = 15 + 0x20;
    dstream.zalloc = (alloc_func)0;
    dstream.zfree = (free_func)0;
    dstream.opaque = (voidpf)0;
    dstream.next_in  = compressedData;
    dstream.avail_in = compressedDataLength;
	dstream.next_out = uncompressedData;
	dstream.avail_out = *uncompressedDataLength;
    int err = inflateInit2(&dstream, windowSize);
    if (err)
		return err;
	
	err = inflate(&dstream, Z_FINISH);
	if (err != Z_STREAM_END)
	{
		inflateEnd(&dstream);
		return err;
	}
	*uncompressedDataLength = dstream.total_out;
	err = inflateEnd(&dstream);
	return err;
}

void PIVToken::dumpDataRecord(const unsigned char *pReturnedData, size_t returnedDataLength,
	const unsigned char *oid, const char *extraSuffix)
{
#if !defined(NDEBUG)
	FILE *fp;
	char fileName[128]={0,};
	const char *kNamePrefix = "/tmp/pivobj-";
	char suffix[32]={0,};
	memcpy(fileName, kNamePrefix, strlen(kNamePrefix));
	sprintf(suffix,"%02X%02X%02X", *oid, *(oid+1), *(oid+2));
	strncat(fileName, suffix, 3);
	if (extraSuffix)
		strcat(fileName, extraSuffix);
	if ((fp = fopen(fileName, "wb")) != NULL)
	{
		fwrite(pReturnedData, 1, returnedDataLength, fp);
		fclose(fp);
		secdebug("pivtokend", "wrote data of length %ld to %s", returnedDataLength, fileName);
	}
#endif
}	

std::string PIVToken::authCertCommonName()
{
	// Since the PIV Authentication Certificate is mandatory, do the user
	// a favor and find the common name to use as the name of the token
	
	const char *cn = NULL;
	SecCertificateRef certificateRef = NULL;
	CFStringRef commonName = NULL;
	CssmData certData;
	
	getDataCore(oidX509CertificatePIVAuthentication, sizeof(oidX509CertificatePIVAuthentication),
				"AUTHCERT", true, true, certData);

	OSStatus status = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_BER, &certificateRef);
	if (!status)
	{
		CFStringRef commonName = NULL;
		SecCertificateCopyCommonName(certificateRef, &commonName);
		if (commonName)
			cn = CFStringGetCStringPtr(commonName, kCFStringEncodingMacRoman);
	}
	
	if (certificateRef)
		CFRelease(certificateRef);
	if (commonName)
		CFRelease(commonName);

	return std::string(cn?cn:"--unknown--");
}


