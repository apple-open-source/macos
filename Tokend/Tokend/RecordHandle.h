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
 *  RecordHandle.h
 *  TokendMuscle
 */

#ifndef _TOKEND_RECORDHANDLE_H_
#define _TOKEND_RECORDHANDLE_H_

#include <security_cdsa_utilities/handleobject.h>
#include <security_utilities/refcount.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/cssmcred.h>
#include <SecurityTokend/SecTokend.h>

namespace Tokend
{

class MetaRecord;
class Record;
class TokenContext;

class RecordHandle: public HandleObject
{
	NOCOPY(RecordHandle)
public:
	RecordHandle(const MetaRecord &metaRecord,
		const RefPointer<Record> &record);
	virtual ~RecordHandle();
	virtual void get(TokenContext *tokenContext, TOKEND_RETURN_DATA &data);

    virtual void getOwner(AclOwnerPrototype &owner);
    virtual void getAcl(const char *tag, uint32 &count,
		AclEntryInfo *&aclList);
	virtual void changeOwner(const AclOwnerPrototype &owner);
	virtual void changeAcl(const AccessCredentials &cred, const AclEdit &edit);

private:
	const MetaRecord &mMetaRecord;
	RefPointer<Record> mRecord;
};

} // end namespace Tokend

#endif /* !_TOKEND_RECORDHANDLE_H_ */


