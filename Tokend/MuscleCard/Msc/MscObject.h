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
 *  MscObject.h
 *  TokendMuscle
 */

#ifndef _MSCOBJECT_H_
#define _MSCOBJECT_H_

#include "MscWrappers.h"
#include "MscTokenConnection.h"
#include <PCSC/musclecard.h>

class MscObject
{
	NOCOPY(MscObject)
public:
    MscObject(const char *objectID,MscTokenConnection *connection);
    MscObject(const MSCObjectInfo& info,MscTokenConnection *connection);
    virtual ~MscObject();
    
	virtual void create(const char *objectID,u_int32_t objectSize,const MscObjectACL& objectACL=MscObjectACL());
	virtual void deleteobj(const char *objectID,bool zeroFlag);
	virtual void write(const char *dataToWrite,size_t dataSize);
	virtual void read();

	virtual const void *data() { if (!mDataLoaded) read(); return reinterpret_cast<const void *>(mData); }
	virtual uint32 size() const	{ return mInfo.size(); }
    virtual const char *objid() const	{ return mInfo.objid(); }

#ifdef _DEBUG_OSTREAM
	friend std::ostream& operator << (std::ostream& strm, const MscObject& obj);
#endif

protected:
	MscObjectInfo mInfo;
	MscTokenConnection *mConnection;
	char *mData;
	mutable bool mDataLoaded;
	mutable bool mAttributesLoaded;
	
	void getAttributes(bool refresh=false);
};

#ifdef _DEBUG_OSTREAM
std::ostream& operator << (std::ostream& strm, const MscObject& ee);
#endif

#endif /* !_MSCOBJECT_H_ */

