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
 *  MscKey.h
 *  TokendMuscle
 */

#ifndef _MSCKEY_H_
#define _MSCKEY_H_

#include <PCSC/musclecard.h>
#include "MscWrappers.h"
#include "MscTokenConnection.h"
#include <security_utilities/debugging.h>
#include <security_cdsa_utilities/cssmkey.h>

class MscKey : public MscKeyInfo
{
public:
    MscKey() { }
    MscKey(unsigned int keyNum, MscTokenConnection *connection);
    MscKey(const MSCKeyInfo& keyInfo,MscTokenConnection *connection) :
		MscKeyInfo(keyInfo), mConnection(connection) {}
    virtual ~MscKey() {};

	void importKey(const MSCKeyACL& keyACL,const void *keyBlob,size_t keyBlobSize,
		MSCKeyPolicy& keyPolicy,MSCPVoid32 pAddParams=NULL, MSCUChar8 addParamsSize=0);
	void exportKey(void *keyBlob,size_t keyBlobSize,MSCPVoid32 pAddParams=NULL, MSCUChar8 addParamsSize=0);
	void extAuthenticate(MSCUChar8 cipherMode,MSCUChar8 cipherDirection,const char *pData,size_t dataSize);
	void convert(CssmKey &cssmk);
	void computeCrypt(MSCUChar8 cipherMode, MSCUChar8 cipherDirection,
		const MSCUChar8 *inputData, size_t inputDataSize,
		MSCUChar8 *outputData, size_t &outputDataSize);

	MscTokenConnection &connection() { return *mConnection; }

	IFDUMP(void debugDump());

protected:
	MscTokenConnection *mConnection;
};

#endif /* !_MSCKEY_H_ */

