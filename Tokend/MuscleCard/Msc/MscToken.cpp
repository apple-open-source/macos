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
 *  MscToken.cpp
 *  TokendMuscle
 */

#include <iostream>
#include "MscToken.h"
#include "MscError.h"

#include <Security/cssmtype.h>
#include <PCSC/pcsclite.h>
#include <PCSC/musclecard.h>

#include <security_cdsa_utilities/cssmdb.h>

void MscToken::loadobjects()
{
	for (MSCUChar8 seqOption = MSC_SEQUENCE_RESET;;)
	{
		MSCObjectInfo objInfo;
		MSC_RV rv = MSCListObjects(mConnection, seqOption, &objInfo);
		if (rv!=MSC_SUCCESS)
			break;	//MscError::throwMe(rv);
		const char *objid = MscObjectInfo::overlay(&objInfo)->objid();
		MscObject *obj = new MscObject(objInfo,mConnection);
		mObjects.insert(pair<std::string,MscObject *>(std::string(objid),obj));
		seqOption = MSC_SEQUENCE_NEXT;
	}

	for (MSCUChar8 seqOption = MSC_SEQUENCE_RESET;;)
	{
		MSCKeyInfo keyInfo;
		MSC_RV rv = MSCListKeys(mConnection, seqOption, &keyInfo);
		if (rv!=MSC_SUCCESS)
			break;	//MscError::throwMe(rv);
		MscKey *xkey = new MscKey(keyInfo,mConnection);
		mKeys.insert(pair<MSCUChar8,MscKey *>(xkey->number(),xkey));
		seqOption = MSC_SEQUENCE_NEXT;
	}
}

void MscToken::dumpobjects()
{
    ConstObjIterator obji = mObjects.begin();
	for (;obji!=mObjects.end();obji++)
		std::cout << (*obji).second << std::endl;
		
#if 0
    ConstKeyIterator keyi = mKeys.begin();
	for (;keyi!=mKeys.end();keyi++)
		std::cout << (*keyi).second << std::endl;
#endif
}

MscObject &MscToken::getObject(const std::string &objID)
{
	ConstObjIterator obji = mObjects.find(objID);
	if (obji==mObjects.end())
        CssmError::throwMe(CSSM_ERRCODE_INVALID_CONTEXT_HANDLE);
	return *(obji->second);
}

MscKey &MscToken::getKey(MSCUChar8 keyNum)
{
	ConstKeyIterator keyi = mKeys.find(keyNum);
	if (keyi==mKeys.end())
        CssmError::throwMe(CSSM_ERRCODE_INVALID_CONTEXT_HANDLE);
	return *(keyi->second);
}

