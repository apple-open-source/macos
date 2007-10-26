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
 *  RecordHandle.cpp
 *  TokendMuscle
 */

#include "RecordHandle.h"

#include "MetaRecord.h"
#include "Record.h"

namespace Tokend
{

RecordHandle::RecordHandle(const MetaRecord &metaRecord,
	const RefPointer<Record> &record) :
	mMetaRecord(metaRecord), mRecord(record)
{
}

RecordHandle::~RecordHandle()
{
}

void RecordHandle::get(TokenContext *tokenContext, TOKEND_RETURN_DATA &data)
{
	mMetaRecord.get(tokenContext, *mRecord, data);
	data.record = handle();
}

void RecordHandle::getOwner(AclOwnerPrototype &owner)
{
	mRecord->getOwner(owner);
}

void RecordHandle::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	mRecord->getAcl(tag, count, acls);
}

void RecordHandle::changeOwner(const AclOwnerPrototype &owner)
{
	mRecord->changeOwner(owner);
}

void RecordHandle::changeAcl(const AccessCredentials &cred,
	const AclEdit &edit)
{
	mRecord->changeAcl(cred, edit);
}


} // end namespace Tokend

