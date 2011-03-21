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
 *  CACNGAttributeCoder.h
 *  TokendMuscle
 */

#ifndef _CACNGATTRIBUTECODER_H_
#define _CACNGATTRIBUTECODER_H_

#include "AttributeCoder.h"
#include <string>

#include <PCSC/musclecard.h>


//
// A coder that reads the data of an object
//
class CACNGDataAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(CACNGDataAttributeCoder)
public:

	CACNGDataAttributeCoder() {}
	virtual ~CACNGDataAttributeCoder();

	virtual void decode(Tokend::TokenContext *tokenContext,
		const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record);
};

//
// A coder that produces the LogicalKeySizeInBits of a key
//
class CACNGKeySizeAttributeCoder : public Tokend::AttributeCoder
{
	NOCOPY(CACNGKeySizeAttributeCoder)
public:
	CACNGKeySizeAttributeCoder() {}
	virtual ~CACNGKeySizeAttributeCoder();
	
	virtual void decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record);
};

#endif /* !_CACNGATTRIBUTECODER_H_ */

