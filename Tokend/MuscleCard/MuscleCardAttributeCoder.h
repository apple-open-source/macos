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
 *  MuscleCardAttributeCoder.h
 *  TokendMuscle
 */

#ifndef _MUSCLECARDATTRIBUTECODER_H_
#define _MUSCLECARDATTRIBUTECODER_H_

#include "AttributeCoder.h"
#include <string>

#include <PCSC/musclecard.h>

//
// A coder that produces a boolean value based on whether a key is extractable
//
class KeyExtractableAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(KeyExtractableAttributeCoder)
public:
	KeyExtractableAttributeCoder() {}
	virtual ~KeyExtractableAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute,
		Tokend::Record &record);
};


//
// A coder that produces a boolean value based on whether a key is sensitive
//
class KeySensitiveAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(KeySensitiveAttributeCoder)
public:
	KeySensitiveAttributeCoder() {}
	virtual ~KeySensitiveAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute,
		Tokend::Record &record);
};


//
// A coder that produces a boolean value based on whether a key is modifiable
//
class KeyModifiableAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(KeyModifiableAttributeCoder)
public:
	KeyModifiableAttributeCoder() {}
	virtual ~KeyModifiableAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute,
		Tokend::Record &record);
};


//
// A coder that produces a boolean value based on whether a key is private
//
class KeyPrivateAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(KeyPrivateAttributeCoder)
public:
	KeyPrivateAttributeCoder() {}
	virtual ~KeyPrivateAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute,
		Tokend::Record &record);
};


//
// A coder that produces a boolean value based on an AND of mask and the direction of a key
//
class KeyDirectionAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(KeyDirectionAttributeCoder)
public:
	KeyDirectionAttributeCoder(MSCUShort16 mask) : mMask(mask) {}
	virtual ~KeyDirectionAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute,
		Tokend::Record &record);
private:
	MSCUShort16 mMask;
};


//
// A coder that produces the LogicalKeySizeInBits of a key
//
class KeySizeAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(KeySizeAttributeCoder)
public:
	KeySizeAttributeCoder() {}
	virtual ~KeySizeAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record);
};


//
// A coder produces a CSSM_ALGID from a key
//
class KeyAlgorithmAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(KeyAlgorithmAttributeCoder)
public:
	KeyAlgorithmAttributeCoder() {}
	virtual ~KeyAlgorithmAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record);
};


//
// A coder that reads the name of a key
//
class KeyNameAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(KeyNameAttributeCoder)
public:

	KeyNameAttributeCoder() {}
	virtual ~KeyNameAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record);
};


//
// A coder that reads the object id of an object
//
class ObjectIDAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(ObjectIDAttributeCoder)
public:

	ObjectIDAttributeCoder() {}
	virtual ~ObjectIDAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record);
};

//
// A coder that reads the data of an object
//
class MscDataAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(MscDataAttributeCoder)
public:

	MscDataAttributeCoder() {}
	virtual ~MscDataAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record);
};


#endif /* !_MUSCLECARDATTRIBUTECODER_H_ */

