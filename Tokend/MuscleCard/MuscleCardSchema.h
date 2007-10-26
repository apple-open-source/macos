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
 *  MuscleCardSchema.h
 *  TokendMuscle
 */

#ifndef _MUSCLECARDSCHEMA_H_
#define _MUSCLECARDSCHEMA_H_

#include "Schema.h"
#include "MuscleCardAttributeCoder.h"
#include "MuscleCardKeyHandle.h"

namespace Tokend
{
	class Relation;
	class MetaRecord;
	class AttributeCoder;
}

class MuscleCardSchema : public Tokend::Schema
{
	NOCOPY(MuscleCardSchema)
public:
    MuscleCardSchema();
    virtual ~MuscleCardSchema();

	virtual void create();
protected:
	Tokend::Relation *createKeyRelation(CSSM_DB_RECORDTYPE keyType);

private:
	// Coders we need.
	MscDataAttributeCoder mMscDataAttributeCoder;
	ObjectIDAttributeCoder mObjectIDCoder;
	KeyNameAttributeCoder mKeyNameCoder;

	KeyAlgorithmAttributeCoder mKeyAlgorithmCoder;

	// Coders for attributes of keys
	KeyExtractableAttributeCoder mKeyExtractableCoder;
	KeySensitiveAttributeCoder mKeySensitiveCoder;
	KeyModifiableAttributeCoder mKeyModifiableCoder;
	KeyPrivateAttributeCoder mKeyPrivateCoder;

	// Coders for Directions (or usage bits) of keys
	KeyDirectionAttributeCoder mEncryptCoder;
	KeyDirectionAttributeCoder mDecryptCoder;
	KeyDirectionAttributeCoder mSignCoder;
	KeyDirectionAttributeCoder mVerifyCoder;

	KeySizeAttributeCoder mKeySizeCoder;

	MuscleCardKeyHandleFactory mMuscleCardKeyHandleFactory;
};

#endif /* !_MUSCLECARDSCHEMA_H_ */

