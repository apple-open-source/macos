/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
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
 *  PIVSchema.h
 *  TokendPIV
 */

#ifndef _PIVSCHEMA_H_
#define _PIVSCHEMA_H_

#include "Schema.h"
#include "PIVAttributeCoder.h"
#include "PIVKeyHandle.h"

namespace Tokend
{
	class Relation;
	class MetaRecord;
	class AttributeCoder;
}

class PIVSchema : public Tokend::Schema
{
	NOCOPY(PIVSchema)
public:
    PIVSchema();
	virtual ~PIVSchema();

	virtual void create();

protected:
	Tokend::Relation *createKeyRelation(CSSM_DB_RECORDTYPE keyType);

private:
	// Coders we need.
	PIVDataAttributeCoder mPIVDataAttributeCoder;

	Tokend::ConstAttributeCoder mKeyAlgorithmCoder;
	PIVKeySizeAttributeCoder mKeySizeCoder;

	PIVKeyHandleFactory mPIVKeyHandleFactory;
};

#endif /* !_PIVSCHEMA_H_ */
