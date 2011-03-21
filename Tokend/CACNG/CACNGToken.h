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
 *  CACNGToken.h
 *  TokendMuscle
 */

#ifndef _CACNGTOKEN_H_
#define _CACNGTOKEN_H_

#include <Token.h>
#include "TokenContext.h"

#include <security_utilities/pcsc++.h>

#include "byte_string.h"

#include "CACNGApplet.h"

class CACNGSchema;

//
// "The" token
//
class CACNGToken : public Tokend::ISO7816Token
{
	NOCOPY(CACNGToken)
public:
	CACNGToken();
	~CACNGToken();

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
	uint32_t pivPinStatus();
	uint32_t cacPinStatus();
	virtual uint32_t pinStatus(int pinNum);
	virtual void verifyPIN(int pinNum, const unsigned char *pin, size_t pinLength);
	void verifyCachedPin(int pinNum);
	virtual void unverifyPIN(int pinNum);

	bool identify();
	void select(shared_ptr<CACNGSelectable> &obj);

	uint32_t exchangeAPDU(const unsigned char *apdu, size_t apduLength,
                          unsigned char *result, size_t &resultLength);

	uint32_t getData(unsigned char *result, size_t &resultLength);

	uint32_t exchangeAPDU(const byte_string& apdu, byte_string &result);
	uint32_t exchangeChainedAPDU(
		unsigned char cla, unsigned char ins,
		unsigned char p1, unsigned char p2,
		const byte_string &data,
		byte_string &result);
protected:
	void populate();

	size_t transmit(const byte_string &apdu, byte_string &result) {
		return transmit(apdu.begin(), apdu.end(), result);
	}
	size_t transmit(const byte_string::const_iterator &apduBegin, const byte_string::const_iterator &apduEnd, byte_string &result);
	
public:
	shared_ptr<CACNGSelectable> currentSelectable;
	uint32_t mCacPinStatus;
	uint32_t mPivPinStatus;
	shared_ptr<CACNGSelectable> cacPinApplet;
	shared_ptr<CACNGSelectable> cardManagerApplet;
	shared_ptr<CACNGSelectable> pivApplet;

	// temporary ACL cache hack - to be removed
	AutoAclOwnerPrototype mAclOwner;
	AutoAclEntryInfoList mAclEntries;

	byte_string cached_piv_pin;
};


#endif /* !_CACNGTOKEN_H_ */

