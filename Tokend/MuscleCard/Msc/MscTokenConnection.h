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
 *  MscTokenConnection.h
 *  TokendMuscle
 */

#ifndef _MSCTOKENCONNECTION_H_
#define _MSCTOKENCONNECTION_H_

#include <PCSC/musclecard.h>
#include <security_utilities/utilities.h>
//#include <sstream>
#include <map>
#include <set>
#include "MscWrappers.h"

class MscTokenConnection : public Security::PodWrapper<MscTokenConnection, MSCTokenConnection>
{
public:
    MscTokenConnection() { memset(this, 0, sizeof(*this)); }
	MscTokenConnection(const MSCTokenInfo &rTokenInfo);
    MscTokenConnection(const MSCTokenConnection &rTokenConnection);

    MscTokenConnection &operator = (const MSCTokenConnection &rTokenInfo);

	// Accessors
	MSCULong32 context() const			{ return hContext; }	// Handle to resource manager
	const MSCTokenInfo& tinfo() const	{ return tokenInfo; }	// token information
	const MSCTokenInfo& info() const	{ return mLocalTokenInfo; }	// token information

	const MSCUChar8 *mac() const		{ return pMac; }		// MAC code
	MSCULong32 macsize() const			{ return macSize; }		// Size of the MAC code

	// calls to muscle layer
	
	void connect(const char *applicationName=NULL,MSCULong32 sharingMode=MSC_SHARE_SHARED);
	void release(MSCULong32 endAction=SCARD_LEAVE_CARD);
	void beginTransaction();
	void endTransaction(MSCULong32 endAction=SCARD_LEAVE_CARD);
	void logoutAll();

	void verifyPIN(MSCUChar8 pinNum,std::string pin);
	unsigned int listPins();

	void selectAID(std::string aid);
	void selectAID(const char *aidValue, MSCULong32 aidSize);
	void writeFramework(const MSCInitTokenParams& initParams);

	void getKeyAttributes(MSCUChar8 keyNumber,MSCKeyInfo& keyInfo);
	void getObjectAttributes(std::string objectID,MSCObjectInfo& objectInfo);

	void getStatus(MSCStatusInfo& statusInfo);

	bool tokenWasReset();
	bool clearReset();
	bool moved();
	bool changed();
	bool known();
	
	MSCULong32 getCapabilities(MSCULong32 Tag);
	void extendedFeature(MSCULong32 extFeature,MSCPUChar8 outData,MSCULong32 outLength,
		MSCPUChar8 inData, MSCPULong32 inLength);

	void generateKeys(MSCUChar8 prvKeyNum,MSCUChar8 pubKeyNum, MSCGenKeyParams& params);
	void getChallenge(const char *seed,size_t seedSize,const char *randomData,size_t randomDataSize);

protected:
	MSCTokenInfo mLocalTokenInfo;
};

#if 0
	typedef struct
	{
		MSCLong32 hContext;	      /*  */
		MSCLong32 hCard;	      /* Handle to the connection */
		LPSCARD_IO_REQUEST ioType;    /* Type of protocol */
		MSCPVoid32 tokenLibHandle;    /* Handle to token library */
		CFDyLibPointers libPointers;  /* Function pointers */
		MSCTokenInfo tokenInfo;	/*  */
		MSCUChar8 loggedIDs;	/* Verification bit mask */
		MSCULong32 shareMode;	/* Sharing mode for this */
		LPRWEventCallback rwCallback;	/* Registered callback */
	}
	MSCTokenConnection, *MSCLPTokenConnection;
#endif

#endif /* !_MSCTOKENCONNECTION_H_ */

