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
 *  MscTokenConnection.cpp
 *  TokendMuscle
 */

#include <iostream>
#include "MscTokenConnection.h"
#include "MscError.h"

MscTokenConnection::MscTokenConnection(const MSCTokenInfo &rTokenInfo)
{
	// @@@ assume that we will call MSCEstablishConnection, which should set us up
	secdebug("connection", "Calling MscTokenConnection::MscTokenConnection");
	clearPod();
//	::memcpy(&tokenInfo,&rTokenInfo,sizeof(MSCTokenInfo));
//	std::cout << "Dump: \n" << tokenInfo << std::endl;
	::memcpy(&mLocalTokenInfo,&rTokenInfo,sizeof(MSCTokenInfo));
#ifdef _DEBUG_OSTREAM
	std::cout << "Dump: \n" << mLocalTokenInfo << std::endl;
#endif
}

MscTokenConnection::MscTokenConnection(const MSCTokenConnection &rTokenConnection)
{
	// Set basic fields
	hContext = rTokenConnection.hContext;		// Handle to resource manager
	hCard = rTokenConnection.hCard;				// Handle to the connection
	ioType->dwProtocol = rTokenConnection.ioType->dwProtocol;		// Protocol identifier
	ioType->cbPciLength = rTokenConnection.ioType->cbPciLength;   // Protocol Control Inf Length
	macSize = rTokenConnection.macSize;				// Size of the MAC code
	loggedIDs = rTokenConnection.loggedIDs;				// Verification bit mask
	shareMode = rTokenConnection.shareMode;				// Sharing mode for this

	// Now copy the strings
	::strncpy(reinterpret_cast<char *>(pMac), reinterpret_cast<const char *>(rTokenConnection.pMac), 
		min(static_cast<size_t>(rTokenConnection.macSize),sizeof(pMac)));		// Token name
}

// strncpy(char * restrict dst, const char * restrict src, size_t len);

MscTokenConnection &MscTokenConnection::operator = (const MSCTokenConnection &rTokenConnection)
{
	// how do we avoid duplication of copy constructor code?

	// Set basic fields
	hContext = rTokenConnection.hContext;		// Handle to resource manager
	hCard = rTokenConnection.hCard;				// Handle to the connection
	ioType->dwProtocol = rTokenConnection.ioType->dwProtocol;		// Protocol identifier
	ioType->cbPciLength = rTokenConnection.ioType->cbPciLength;   // Protocol Control Inf Length
	macSize = rTokenConnection.macSize;				// Size of the MAC code
	loggedIDs = rTokenConnection.loggedIDs;				// Verification bit mask
	shareMode = rTokenConnection.shareMode;				// Sharing mode for this

	// Now copy the strings
	::strncpy(reinterpret_cast<char *>(pMac), reinterpret_cast<const char *>(rTokenConnection.pMac), 
		min(static_cast<size_t>(rTokenConnection.macSize),sizeof(pMac)));		// Token name

	return *this;
}

void MscTokenConnection::connect(const char *applicationName,MSCULong32 sharingMode)
{
	// Establishes a connection to the specified token
	MSC_RV rv = MSCReleaseConnection(this, MSC_RESET_TOKEN);

	rv = MSCEstablishConnection(&mLocalTokenInfo, sharingMode,
		reinterpret_cast<unsigned char *>(const_cast<char *>(applicationName)),
		applicationName?strlen(applicationName):0, this); // NULL,0 => use default applet
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::release(MSCULong32 endAction)
{
	// Releases a connection to the specified token 
	MSC_RV rv = MSCReleaseConnection(this,endAction);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::beginTransaction()
{
	// Locks a transaction to the token 
	MSC_RV rv = MSCBeginTransaction(this);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::endTransaction(MSCULong32 endAction)
{
	// Releases a locked transaction to the token 
	MSC_RV rv = MSCEndTransaction(this,endAction);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::logoutAll()
{
	// Releases a connection to the specified token 
	MSC_RV rv = MSCLogoutAll(this);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::verifyPIN(MSCUChar8 pinNum,std::string pin)
{
	// Releases a locked transaction to the token 
	MSC_RV rv = MSCVerifyPIN(this,pinNum,reinterpret_cast<unsigned char *>(const_cast<char *>(pin.c_str())),pin.length());
	if (rv!=MSC_SUCCESS)
	{
		std::cout << "*** PIN verify failed!!! ***" << std::endl;
		MscError::throwMe(rv);
	}
}

unsigned int MscTokenConnection::listPins()
{
	MSCUShort16 mask;
	MSC_RV rv = MSCListPINs(this, &mask);
	if (rv != MSC_SUCCESS)
		MscError::throwMe(rv);
	return mask;
}

void MscTokenConnection::selectAID(std::string aid)
{
//	selectAID(reinterpret_cast<MSCUChar8 *>(aid.c_str()), aid.length());
	selectAID(aid.c_str(), aid.length());
}

void MscTokenConnection::selectAID(const char */* aidValue */, MSCULong32 /* aidSize */)
{
	// Selects applet - Not to be used by applications
	// MSCSelectAID is not exported!!
//	MSC_RV rv = MSCSelectAID(this,reinterpret_cast<unsigned char *>(const_cast<char *>(aidValue)),aidSize); /* MSC_SUCCESS */
	MSC_RV rv = MSC_UNSUPPORTED_FEATURE;	//MSCSelectAID(this,reinterpret_cast<unsigned char *>(const_cast<char *>(aidValue)),aidSize); /*  */
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::writeFramework(const MSCInitTokenParams& initParams)
{
	// Pre-personalization function
	MSC_RV rv = MSCWriteFramework(this,const_cast<MSCInitTokenParams *>(&initParams));
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::getKeyAttributes(MSCUChar8 keyNumber,MSCKeyInfo& keyInfo)
{
	// 
	MSC_RV rv = MSCGetKeyAttributes(this,keyNumber,&keyInfo);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::getObjectAttributes(std::string objectID,MSCObjectInfo& objectInfo)
{
	// 
	MSC_RV rv = MSCGetObjectAttributes(this,const_cast<char *>(objectID.c_str()),&objectInfo);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscTokenConnection::getStatus(MSCStatusInfo& statusInfo)
{
	// Pre-personalization function
	MSC_RV rv = MSCGetStatus(this,&statusInfo);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

#pragma mark ---------------- Token state methods --------------

bool MscTokenConnection::tokenWasReset()
{
	// Was the token reset ? 
	return MSCIsTokenReset(this);
}

bool MscTokenConnection::clearReset()
{
	// Clear the Reset state 
	return MSCClearReset(this);
}

bool MscTokenConnection::moved()
{
	// Was the token moved (removed, removed/inserted) ?
	return MSCIsTokenMoved(this);
}

bool MscTokenConnection::changed()
{
	// Did any state change with the token ?
	return MSCIsTokenChanged(this);
}

bool MscTokenConnection::known()
{
	// Did any state change with the token ?
	return MSCIsTokenKnown(this);
}

#pragma mark ---------------- Capability methods --------------

MSCULong32 MscTokenConnection::getCapabilities(MSCULong32 tag)
{
	MSCULong32 cap;
	MSCULong32 size;
	MSC_RV rv = MSCGetCapabilities(this, tag,
		reinterpret_cast<MSCPUChar8>(&cap), &size);
	if (rv != MSC_SUCCESS)
		MscError::throwMe(rv);

	if (size == 1)
		return *reinterpret_cast<uint8_t *>(&cap);
	else if (size == 2)
		return *reinterpret_cast<uint16_t *>(&cap);
	else
		return cap;
}

void MscTokenConnection::extendedFeature(MSCULong32 extFeature,MSCPUChar8 outData,MSCULong32 outLength,
	MSCPUChar8 inData, MSCPULong32 inLength)
{
	MSC_RV rv = MSCExtendedFeature(this,extFeature,outData,outLength,inData,inLength);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

#pragma mark ---------------- Key methods --------------

void MscTokenConnection::generateKeys(MSCUChar8 prvKeyNum,MSCUChar8 pubKeyNum,MSCGenKeyParams& params)
{
	MSC_RV rv = MSCGenerateKeys(this,prvKeyNum,pubKeyNum,&params);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

#pragma mark ---------------- Misc methods --------------

void MscTokenConnection::getChallenge(const char *seed,size_t seedSize,const char *randomData,size_t randomDataSize)
{
	MSC_RV rv = MSCGetChallenge(this,reinterpret_cast<unsigned char *>(const_cast<char *>(seed)),seedSize,
		reinterpret_cast<unsigned char *>(const_cast<char *>(randomData)),randomDataSize);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

