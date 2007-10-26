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
 *  BELPICToken.h
 *  TokendMuscle
 */

#ifndef _BELPICTOKEN_H_
#define _BELPICTOKEN_H_

#include <Token.h>

#include <security_utilities/pcsc++.h>

#define BELPIC_MIN_PIN_LEN	4
#define BELPIC_MAX_PIN_LEN	12

class BELPICSchema;

//
// "The" token
//
class BELPICToken : public Tokend::ISO7816Token
{
	NOCOPY(BELPICToken)
public:
	BELPICToken();
	~BELPICToken();

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
	void _verifyPIN(int pinNum, const unsigned char *pin, size_t pinLength);
	virtual void unverifyPIN(int pinNum);

	void select(const uint8_t *df, const uint8_t *ef);
	void selectKeyForSign(const uint8_t *keyId);
	void readBinary(uint8_t *result, size_t &resultLength);
	uint32_t exchangeAPDU(const uint8_t *apdu, size_t apduLength,
                          uint8_t *result, size_t &resultLength);

	static int usleep(int iTimeVal);
	
protected:
	uint8_t pinDigit(uint8_t digit);
	void populate();

public:
	const uint8_t *mCurrentDF;
	const uint8_t *mCurrentEF;
	unsigned char *mReturnedData;
	uint32_t mPinStatus;

	// temporary ACL cache hack - to be removed
	AutoAclOwnerPrototype mAclOwner;
	AutoAclEntryInfoList mAclEntries;
};


#endif /* !_BELPICTOKEN_H_ */

