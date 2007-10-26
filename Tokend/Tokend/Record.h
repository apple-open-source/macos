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
 *  Record.h
 *  TokendMuscle
 */

#ifndef _TOKEND_RECORD_H_
#define _TOKEND_RECORD_H_

#include "AttributeCoder.h"
#include "MetaRecord.h"
#include "Attribute.h"
#include <security_utilities/refcount.h>
#include <security_utilities/adornments.h>
#include <security_cdsa_utilities/cssmaclpod.h>
#include <security_cdsa_utilities/cssmcred.h>
#include <SecurityTokend/SecTokend.h>

namespace Tokend
{

class Record : public RefCount, public Security::Adornable
{
	NOCOPY(Record)
public:
	Record();
	virtual ~Record();

	bool hasAttributeAtIndex(uint32 attributeIndex) const;
	const Attribute &attributeAtIndex(uint32 attributeIndex) const;
	void attributeAtIndex(uint32 attributeIndex, Attribute *attribute);

    virtual void getOwner(AclOwnerPrototype &owner);
    virtual void getAcl(const char *tag, uint32 &count,
		AclEntryInfo *&aclList);
	virtual void changeOwner(const AclOwnerPrototype &owner);
	virtual void changeAcl(const AccessCredentials &cred, const AclEdit &edit);

	virtual const char *description();
	virtual Attribute *getDataAttribute(TokenContext *tokenContext);

protected:
	typedef std::vector<Attribute *> Attributes;
    typedef Attributes::iterator AttributesIterator;
    typedef Attributes::const_iterator ConstAttributesIterator;

	Attributes mAttributes;

	// temporary ACL cache hack - to be removed
	static AutoAclOwnerPrototype gNobodyAclOwner;
	static AutoAclEntryInfoList gAnyReadAclEntries;
};

} // end namespace Tokend

#endif /* !_TOKEND_RECORD_H_ */

