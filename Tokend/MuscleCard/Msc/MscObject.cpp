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
 *  MscObject.cpp
 *  TokendMuscle
 */

#include "MscObject.h"
#include "MscError.h"

MscObject::MscObject(const char *objectID,MscTokenConnection *connection) :
	mConnection(connection), mData(NULL), mDataLoaded(false), mAttributesLoaded(false)
{
	::memcpy(mInfo.objectID,objectID,sizeof(mInfo.objectID));
}

MscObject::MscObject(const MSCObjectInfo& info,MscTokenConnection *connection) :
	mInfo(info), mConnection(connection), mData(NULL), mDataLoaded(false), mAttributesLoaded(true)
{
	// Note: if we are constructed with an MSCObjectInfo, we already have our attributes
}

MscObject::~MscObject()
{
	if (mData)
		free(mData);
}

void MscObject::create(const char *objectID,u_int32_t objectSize,const MscObjectACL& objectACL)
{
	// This reserves space on the card for a new object
	// It must be called before the object can be written
	MSC_RV rv = MSCCreateObject(mConnection,const_cast<char *>(&Required(objectID)),objectSize,
		const_cast<MSCObjectACL *>((MSCObjectACL *)&objectACL));
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscObject::deleteobj(const char *objectID,bool zeroFlag)
{
	// This deletes an object on the card
	MSC_RV rv = MSCDeleteObject(mConnection,const_cast<char *>(&Required(objectID)),zeroFlag);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
}

void MscObject::read()
{
	LPRWEventCallback rwCallback = NULL;
	MSCPVoid32 addParams = NULL;
	getAttributes();

	if (mDataLoaded)
		return;
		
	MSCULong32 readSz = mInfo.size();
	MSC_RV rv = MSCReadAllocateObject(mConnection, const_cast<char *>(mInfo.objid()),
		reinterpret_cast<MSCPUChar8 *>(&mData),&readSz, rwCallback, addParams);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);

	mDataLoaded = true;
}

void MscObject::write(const char *dataToWrite,size_t dataSize)
{
	MSCULong32 offset = 0;
	LPRWEventCallback rwCallback = NULL;
	MSCPVoid32 addParams = NULL;

	MSC_RV rv = MSCWriteObject(mConnection, const_cast<char *>(mInfo.objid()), offset,
		reinterpret_cast<unsigned char *>(const_cast<char *>(dataToWrite)),dataSize, rwCallback, addParams);
	if (rv!=MSC_SUCCESS)
		MscError::throwMe(rv);
	mDataLoaded = false;
}

#ifdef _DEBUG_OSTREAM
std::ostream& operator << (std::ostream& strm, const MscObject& obj)
{
	strm << "Obj: " << obj.mInfo;
	return strm;
}
#endif

#pragma mark ---------------- Utility methods --------------

void MscObject::getAttributes(bool refresh)
{
	if (refresh || !mAttributesLoaded)
	{
		(Required(mConnection)).getObjectAttributes(mInfo.objid(),mInfo);
		mAttributesLoaded = true;
	}
}

