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
 *  PIVToken.h
 *  TokendPIV
 */

#ifndef _PIVTOKEN_H_
#define _PIVTOKEN_H_

#define _USECERTIFICATECOMMONNAME	1

#include <Token.h>
#include "TokenContext.h"
#include "PIVDefines.h"

#include <security_utilities/pcsc++.h>

#include "byte_string.h"

#pragma mark ---------- PIV defines ----------

#define CLA_STANDARD				0x00
#define INS_SELECT_FILE				0xA4
#define INS_VERIFY_APDU				0x20	// SP800731 Section 2.3.3.2.1
#define INS_CHANGE_REFERENCE_DATA	0x24	// [SP800731 7.2.2]

// Placeholders for fields in the APDU to be filled in programmatically
#define TBD_ZERO			0x00
#define TBD_FF				0xFF

// These are from NISTIR6887 5.1.1.4 Select File APDU
// They are the values for the P1 field
#define SELECT_P1_EXPLICIT	0x00
#define SELECT_P1_CHILDDF	0x01
#define SELECT_P1_CHILDEF	0x02
#define SELECT_P1_PARENTDF	0x03

#define SELECT_APPLET  PIV_CLA_STANDARD, PIV_INS_SELECT_FILE, 0x04, 0x00	// Select application by AID

#define SELECT_PIV_APPLET_VERS	0x10, 0x00, 0x01, 0x00
#define SELECT_PIV_APPLET_SHORT	SELECT_APPLET, 0x07, 0xA0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00
#define SELECT_PIV_APPLET_LONG  SELECT_APPLET, 0x0B, 0xA0, 0x00, 0x00, 0x03, 0x08, 0x00, 0x00, SELECT_PIV_APPLET_VERS

#pragma mark ---------- Object IDs on Token ----------

/*
	Object IDs for objects on token. All currently 3 hex bytes.
	See 4.2 OIDs and Tags of PIV Card Application Data Objects [SP800731]

	4.1 PIV Card Application Data Objects [SP800731]
	A PIV Card Application shall contain six mandatory data objects and five optional data object for 
	interoperable use.  The six mandatory data objects for interoperable use are as follows: 

	1. Card Capability Container 
	2. Card Holder Unique Identifier  
	3. X.509 Certificate for PIV Authentication  
	4. Card Holder Fingerprint I 
	5. Card Holder Fingerprint II2 
	6. Security Object 
 
	The five optional data objects for interoperable use are as follows: 
 
	1. Card Holder Facial Image 
	2. Printed Information 
	3. X.509 Certificate for PIV Digital Signature 
	4. X.509 Certificate for PIV Key Management 
	5. X.509 Certificate for Card Authentication 
*/

//	Card Capability Container 2.16.840.1.101.3.7.1.219.0				0x5FC107	M
#define PIV_OBJECT_ID_CARD_CAPABILITY_CONTAINER				0x5F, 0xC1, 0x07

//	Card Holder Unique Identifier 2.16.840.1.101.3.7.2.48.0				0x5FC102	M [CHUID]
#define PIV_OBJECT_ID_CARDHOLDER_UNIQUEID					0x5F, 0xC1, 0x02

//	Card Holder Fingerprints 2.16.840.1.101.3.7.2.96.16					0x5FC103	M
#define PIV_OBJECT_ID_CARDHOLDER_FINGERPRINTS				0x5F, 0xC1, 0x03

//	Printed Information 2.16.840.1.101.3.7.2.48.1						0x5FC109	O
#define PIV_OBJECT_ID_PRINTED_INFORMATION					0x5F, 0xC1, 0x09

//	Card Holder Facial Image 2.16.840.1.101.3.7.2.96.48					0x5FC108	O
#define PIV_OBJECT_ID_CARDHOLDER_FACIAL_IMAGE				0x5F, 0xC1, 0x08

//	X.509 Certificate for PIV Authentication 2.16.840.1.101.3.7.2.1.1	0x5FC105	M
#define PIV_OBJECT_ID_X509_CERTIFICATE_PIV_AUTHENTICATION	0x5F, 0xC1, 0x05

//	X.509 Certificate for Digital Signature 2.16.840.1.101.3.7.2.1.0	0x5FC10A	O
#define PIV_OBJECT_ID_X509_CERTIFICATE_DIGITAL_SIGNATURE	0x5F, 0xC1, 0x0A

//	X.509 Certificate for Key Management 2.16.840.1.101.3.7.2.1.2		0x5FC10B	O
#define PIV_OBJECT_ID_X509_CERTIFICATE_KEY_MANAGEMENT		0x5F, 0xC1, 0x0B

//	X.509 Certificate for Card Authentication 2.16.840.1.101.3.7.2.5.0	0x5FC101	O
#define PIV_OBJECT_ID_X509_CERTIFICATE_CARD_AUTHENTICATION	0x5F, 0xC1, 0x01


class PIVSchema;
class PIVCCC;

#pragma mark ---------- The Token Class ----------

//
// "The" token
//
class PIVToken : public Tokend::ISO7816Token
{
	NOCOPY(PIVToken)
public:
	PIVToken();
	~PIVToken();

	virtual void didDisconnect();
	virtual void didEnd();

    virtual uint32 probe(SecTokendProbeFlags flags,
		char tokenUid[TOKEND_MAX_UID]);
	virtual void establish(const CSSM_GUID *guid, uint32 subserviceId,
		SecTokendEstablishFlags flags, const char *cacheDirectory,
		const char *workDirectory, char mdsDirectory[PATH_MAX],
		char printName[PATH_MAX]);
	virtual void getOwner(AclOwnerPrototype &owner);
	virtual void getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls);

	virtual void changePIN(int pinNum,
		const unsigned char *oldPin, size_t oldPinLength,
		const unsigned char *newPin, size_t newPinLength);
	virtual uint32_t pinStatus(int pinNum);
	virtual void verifyPIN(int pinNum, const unsigned char *pin, size_t pinLength);
	virtual void unverifyPIN(int pinNum);

	bool identify();
	
	// These methods are convenient for Java card, but would be replace by calls
	// to the PKCS#11 library for a for a PKCS#11 based tokend

	/* NOTE: Using pointers for applet selection rather than byte_strings to permit simple selection detection */
	void select(const unsigned char *applet, size_t appletLength);
	void selectDefault();
	uint32_t exchangeAPDU(const byte_string& apdu,
                          byte_string &result);
	uint32_t exchangeChainedAPDU(unsigned char cla, unsigned char ins,
	                             unsigned char p1, unsigned char p2,
	                             const byte_string &data,
	                             byte_string &result);

	uint32_t getData(byte_string &result);

	void getDataCore(const byte_string &oid, const char *description, bool isCertificate,
		bool allowCaching, byte_string &data);
	std::string authCertCommonName();

protected:
	void populate();

	void processCertificateRecord(byte_string &data, const byte_string &oid, const char *description);
	void dumpDataRecord(const byte_string &data, const byte_string &oid, const char *extraSuffix = NULL);
	static int compressionType(const byte_string &data);
	static int uncompressData(byte_string &uncompressedData, const byte_string &compressedData, int compressionType);
	
	enum			//arbitrary values
	{
		kCompressionNone = 0,
		kCompressionZlib = 1,
		kCompressionGzip = 2,
		kCompressionUnknown = 9
	};
	
	size_t transmit(const byte_string &apdu, byte_string &result) {
		return transmit(apdu.begin(), apdu.end(), result);
	}
	size_t transmit(const byte_string::const_iterator &apduBegin, const byte_string::const_iterator &apduEnd, byte_string &result);
public:
	const unsigned char *mCurrentApplet;
	uint32_t mPinStatus;
	
	// temporary ACL cache hack - to be removed
	AutoAclOwnerPrototype mAclOwner;
	AutoAclEntryInfoList mAclEntries;
};


#endif /* !_PIVTOKEN_H_ */
