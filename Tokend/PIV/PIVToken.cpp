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

#include <algorithm> /* min */

#include "TLV.h"

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
	mCurrentApplet(NULL), mPinStatus(0)
{
	mTokenContext = this;
	mSession.open();
}

PIVToken::~PIVToken()
{
	delete mSchema;
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
	byte_string cccOid((const unsigned char *)oidCardCapabilityContainer, oidCardCapabilityContainer + sizeof(oidCardCapabilityContainer));
	byte_string cccdata;
	getDataCore(cccOid, "CCC", false, true, cccdata);
	PIVCCC ccc(cccdata);
	::snprintf(printName, 40, "PIV-%s", ccc.hexidentifier().c_str());
#endif	/* _USECERTIFICATECOMMONNAME */
	Tokend::ISO7816Token::name(printName);
	secdebug("pivtoken", "name: %s", printName);

	Tokend::ISO7816Token::establish(guid, subserviceId, flags,
		cacheDirectory, workDirectory, mdsDirectory, printName);

	if(mSchema)
		delete mSchema;
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
#ifndef _USEFALLBACKTOKENUID
			byte_string cccOid((const unsigned char *)oidCardCapabilityContainer, oidCardCapabilityContainer + sizeof(oidCardCapabilityContainer));
			byte_string cccdata;
			getDataCore(cccOid, "CCC", false, true, cccdata);
			PIVCCC ccc(cccdata);
			snprintf(tokenUid, TOKEND_MAX_UID, "PIV-%s", ccc.hexidentifier().c_str());

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
	
	const unsigned char keyRefs[] =
	{
		PIV_KEYREF_PIV_AUTHENTICATION,
		PIV_KEYREF_PIV_DIGITAL_SIGNATURE,
		PIV_KEYREF_PIV_KEY_MANAGEMENT,
		PIV_KEYREF_PIV_CARD_AUTHENTICATION
	};
	
	for (unsigned int ix=0;ix<sizeof(certids)/sizeof(certids[0]);++ix)
	{
		RefPointer<Tokend::Record> cert(new PIVCertificateRecord(certids[ix], sz, certNames[ix]));
		certRelation.insertRecord(cert);

		RefPointer<Tokend::Record> key(new PIVKeyRecord(certids[ix], sz, keyNames[ix], privateKeyRelation.metaRecord(), keyRefs[ix]));
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
		selectDefault();
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
	selectDefault();

	const unsigned char dataFieldLen = 0x10;	// doc says must be 16 (= 2x8)
	const unsigned char APDU_TEMPLATE[] = { PIV_CHANGE_REFERENCE_DATA_APDU_TEMPLATE };
	byte_string apdu(APDU_TEMPLATE, APDU_TEMPLATE + sizeof(APDU_TEMPLATE));

	apdu[PIV_VERIFY_APDU_INDEX_KEY] = static_cast<unsigned char>(pinNum & 0xFF);
	apdu[PIV_VERIFY_APDU_INDEX_LEN] = dataFieldLen;

	copy(oldPin, oldPin + oldPinLength, apdu.begin() + PIV_VERIFY_APDU_INDEX_DATA);
	copy(newPin, newPin + newPinLength, apdu.begin() + PIV_CHANGE_REFERENCE_DATA_APDU_INDEX_DATA2);

	byte_string result;

	mPinStatus = exchangeAPDU(apdu, result);
	/* Clear out pin by forcing zeroes in */
	secure_zero(apdu);
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
	selectDefault();

	const unsigned char APDU_TEMPLATE[] = { PIV_VERIFY_APDU_STATUS };
	byte_string apdu(APDU_TEMPLATE, APDU_TEMPLATE + sizeof(APDU_TEMPLATE));

	apdu[PIV_VERIFY_APDU_INDEX_KEY] = 0x80;//static_cast<unsigned char>(pinNum & 0xFF);

	byte_string result;

	mPinStatus = exchangeAPDU(apdu, result);
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
	selectDefault();

	const unsigned char dataFieldLen = 8;	// doc says must be 8
	
	const unsigned char APDU_TEMPLATE[] = { PIV_VERIFY_APDU_TEMPLATE };
	byte_string apdu(APDU_TEMPLATE, APDU_TEMPLATE + sizeof(APDU_TEMPLATE));

	apdu[PIV_VERIFY_APDU_INDEX_KEY] = 0x80;//static_cast<unsigned char>(pinNum & 0xFF);
	apdu[PIV_VERIFY_APDU_INDEX_LEN] = dataFieldLen;

	copy(pin, pin + pinLength, apdu.begin() + PIV_VERIFY_APDU_INDEX_DATA);

	byte_string result;

	mPinStatus = exchangeAPDU(apdu, result);
	/* Clear out pin */
	secure_zero(apdu);
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

	byte_string apdu(applet, applet + appletLength);
	byte_string result;
	bool failed = false;

	uint16_t rx;
	try
	{
		rx = exchangeAPDU(apdu, result);
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

	if (failed || (rx != SCARD_SUCCESS))
	{
		secdebug("pivtoken", "select END [FAILURE %02X %02X]", 
			result[result.size() - 2], result[result.size() - 1]);
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}

	if (isInTransaction())
		mCurrentApplet = applet;
		
	secdebug("pivtoken", "select END [SUCCESS]");
}

void PIVToken::selectDefault()
{
	select(kSelectPIVApplet, sizeof(kSelectPIVApplet));
}

uint32_t PIVToken::exchangeAPDU(const byte_string &apdu, byte_string &result)
{
	static const uint8_t GET_RESULT_TEMPLATE [] = { 0x00, 0xC0, 0x00, 0x00, 0xFF };
	byte_string getResult(GET_RESULT_TEMPLATE, GET_RESULT_TEMPLATE + sizeof(GET_RESULT_TEMPLATE));
	const int SIZE_INDEX = 4;

	transmit(apdu, result);
	/* Keep pulling more data */
	while (result.size() >= 2 && result[result.size() - 2] == PIV_RESULT_CONTINUATION_SW1)
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
                PIVError::throwMe((result[result.size() - 2] << 8)
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

uint32_t PIVToken::exchangeChainedAPDU(unsigned char cla, unsigned char ins,
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
	const size_t BASE_CHUNK_LENGTH = 256;
	size_t chunkLength;
	byte_string::const_iterator iter;
	/* Chain data and skip last chunk since its in the receiving end */
	for(iter = data.begin(); (iter + BASE_CHUNK_LENGTH) < data.end(); iter += BASE_CHUNK_LENGTH) {
		chunkLength = std::min(BASE_CHUNK_LENGTH, (size_t)(data.end() - iter));
		apdu[5] = chunkLength & 0xFF;
		/* Don't send Le */
		transmit(apdu.begin(), apdu.begin() + 5 + chunkLength, result);
		/* No real data should come back until chaining is complete */
		if(result.size() != 2)
			PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
		else
			PIVError::check(result[result.size() - 2] << 8 | result[result.size() - 1]);
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


void PIVToken::getDataCore(const byte_string &oid, const char *description, bool isCertificate,
	bool allowCaching, byte_string &data)
{
	/* First check the cache */
	CssmData cssmData;
	if(allowCaching && cachedObject(0, description, cssmData)) {
		data.assign(cssmData.Data, cssmData.Data + cssmData.Length);
		free(cssmData.Data);
		return;
	}

	// The APDU only has space for a 3 byte OID
	if (oid.size() != 3)
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	
	const unsigned char dataFieldLen = 0x05;	// doc says must be 16, but in pratice it is 5
	static const unsigned char INITIAL_APDU_TEMPLATE[] = { PIV_GETDATA_APDU_TEMPLATE };
	/* TODO: Build from ground-up */
	byte_string initialApdu(INITIAL_APDU_TEMPLATE, INITIAL_APDU_TEMPLATE + sizeof(INITIAL_APDU_TEMPLATE));

	initialApdu[PIV_GETDATA_APDU_INDEX_LEN] = dataFieldLen;
	initialApdu[PIV_GETDATA_APDU_INDEX_OIDLEN] = oid.size();
	copy(oid.begin(), oid.end(), initialApdu.begin() + PIV_GETDATA_APDU_INDEX_OID);
	initialApdu.resize(PIV_GETDATA_APDU_INDEX_OID + oid.size());

	static const unsigned char CONTINUATION_APDU_TEMPLATE[] = { PIV_GETDATA_CONT_APDU_TEMPLATE };
	byte_string continuationApdu(CONTINUATION_APDU_TEMPLATE, CONTINUATION_APDU_TEMPLATE + sizeof(CONTINUATION_APDU_TEMPLATE));
	
	byte_string *apdu = &initialApdu;

	// Talk to token here to get data
	{
		PCSC::Transaction _(*this);
		selectDefault();
		uint32_t rx;
		do
		{
			rx = exchangeAPDU(*apdu, data);
			secdebug("pivtokend", "exchangeAPDU result %02X", rx);

			if ((rx & 0xFF00) != SCARD_BYTES_LEFT_IN_SW2 &&
				(rx & 0xFF00) != SCARD_SUCCESS)
				PIVError::check(rx);

			// Switch to the continuation APDU after first exchange
			apdu = &continuationApdu;
			
			// Number of bytes to fetch next time around is in the last byte returned.
			// For all except the penultimate read, this is 0, indicating that the
			// token should read all bytes.
			
			apdu->at(PIV_GETDATA_CONT_APDU_INDEX_LEN) = static_cast<unsigned char>(rx & 0xFF);
		} while ((rx & 0xFF00) == SCARD_BYTES_LEFT_IN_SW2);
	}
	if(data.size() > PIV_MAX_DATA_SIZE) {
		PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	}
	dumpDataRecord(data, oid);

	// Start to parse the BER-TLV encoded data. In the end, we only return the
	// main data part of this but we need to step through the rest first
	// The certficates are the only types we parse here

	if (data.size()<=0)
		return;
	if (data[0] != PIV_GETDATA_RESPONSE_TAG)
			PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);

		if (isCertificate)
		processCertificateRecord(data, oid, description);

	if (!allowCaching)
		return;
	cssmData.Data = &data[0];
	cssmData.Length = data.size();
	cacheObject(0, description, cssmData);
	}

void PIVToken::processCertificateRecord(byte_string &data, const byte_string &oid, const char *description)
{
	bool hasCertificateData = false;
	bool isCompressed = false;
	
	// 00000000  53 82 04 84 70 82 04 78  78 da 33 68 62 db 61 d0 
	TLV_ref tlv;
	TLVList list;
	try {
		tlv = TLV::parse(data);
		list = tlv->getInnerValues();
	} catch(...) {
		PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	}

	for(TLVList::const_iterator iter = list.begin(); iter != list.end(); ++iter) {
		const byte_string &tagString = (*iter)->getTag();
		const byte_string &value = (*iter)->getValue();
		if(tagString.size() != 1)
			PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
		uint8_t tag = tagString[0];
		switch (tag)
		{
		case PIV_GETDATA_TAG_CERTIFICATE:			// 0x70
			data = value;
			hasCertificateData = true;
			break;
		case PIV_GETDATA_TAG_CERTINFO:				// 0x71
			if(value.size() != 1)
				PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
			secdebug("pivtokend", "CertInfo byte: %02X", value[0]);
			isCompressed = value[0] & PIV_GETDATA_COMPRESSION_MASK;
			break;
		case PIV_GETDATA_TAG_MSCUID:				// 0x72 -- should be of length 3...
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
	}

	/* No cert data ? */
	if(!hasCertificateData)
		PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	if (isCompressed)
	{
		/* The certificate is compressed */
		secdebug("pivtokend", "uncompressing compressed %s", description);
		dumpDataRecord(data, oid, "-compressedcert");

		byte_string uncompressedData;
		uncompressedData.resize(PIV_MAX_DATA_SIZE);
		int rv = Z_ERRNO;
		int compTyp = compressionType(data);
		rv = PIVToken::uncompressData(uncompressedData, data, compTyp);
		if (rv != Z_OK)
		{
			secdebug("zlib", "uncompressing %s failed: %d [type=%d]", description, rv, compTyp);
			CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
		}
		data = uncompressedData;
	}
	else
	{
	}
	dumpDataRecord(data, oid, "-rawcert");
}

int PIVToken::compressionType(const byte_string &data)
{
	// Some ad-hoc stuff to guess at compression type
	if (data.size() > 2 && data[0] == 0x1F && data[1] == 0x8B)
		return kCompressionGzip;
	if (data.size() > 1 /*&& (data[0] & 0x10) == Z_DEFLATED*/)
		return kCompressionZlib;
	else
		return kCompressionUnknown;
}

int PIVToken::uncompressData(byte_string &uncompressedData, const byte_string &compressedData, int compressionType)
{
    z_stream dstream;					// decompression stream
	int windowSize = 15;
	switch(compressionType) {
	case kCompressionGzip:
		windowSize += 0x20;
		break;
	case kCompressionZlib:
		break;
	default:
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);
	}
    dstream.zalloc = (alloc_func)0;
    dstream.zfree = (free_func)0;
    dstream.opaque = (voidpf)0;
	/* Input not altered , so de-const-casting ok*/
    dstream.next_in  = (Bytef*)&compressedData[0];
    dstream.avail_in = compressedData.size();
	dstream.next_out = &uncompressedData[0];
	dstream.avail_out = uncompressedData.size();
    int err = inflateInit2(&dstream, windowSize);
    if (err)
		return err;
	
	err = inflate(&dstream, Z_FINISH);
	if (err != Z_STREAM_END)
	{
		inflateEnd(&dstream);
		return err;
	}
	uncompressedData.resize(dstream.total_out);
	err = inflateEnd(&dstream);
	return err;
}

void PIVToken::dumpDataRecord(const byte_string &data, const byte_string &oid, const char *extraSuffix)
{
#if !defined(NDEBUG)
	FILE *fp;
	char fileName[128]={0,};
	const char *kNamePrefix = "/tmp/pivobj-";
	char suffix[32]={0,};
	memcpy(fileName, kNamePrefix, strlen(kNamePrefix));
	sprintf(suffix,"%02X%02X%02X", oid[0], oid[1], oid[2]);
	strncat(fileName, suffix, 3);
	if (extraSuffix)
		strcat(fileName, extraSuffix);
	if ((fp = fopen(fileName, "wb")) != NULL)
	{
		fwrite(&data[0], 1, data.size(), fp);
		fclose(fp);
		secdebug("pivtokend", "wrote data of length %ld to %s", data.size(), fileName);
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
	
	byte_string data;
	byte_string oidAuthCert(oidX509CertificatePIVAuthentication, oidX509CertificatePIVAuthentication + sizeof(oidX509CertificatePIVAuthentication));
	getDataCore(oidAuthCert, "AUTHCERT", true, true, data);
	CssmData certData(&data[0], data.size());
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

size_t PIVToken::transmit(const byte_string::const_iterator &apduBegin, const byte_string::const_iterator &apduEnd, byte_string &result) {
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

