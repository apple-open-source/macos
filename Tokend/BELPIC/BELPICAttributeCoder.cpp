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
 *  BELPICAttributeCoder.cpp
 *  TokendMuscle
 */

#include "BELPICAttributeCoder.h"

#include "Adornment.h"
#include "MetaAttribute.h"
#include "MetaRecord.h"
#include "BELPICRecord.h"
#include "BELPICToken.h"

#include <Security/SecKeychainItem.h>
#include <security_cdsa_utilities/cssmkey.h>

using namespace Tokend;


//
// BELPICDataAttributeCoder
//
BELPICDataAttributeCoder::~BELPICDataAttributeCoder()
{
}

void BELPICDataAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	BELPICRecord &belpicRecord = dynamic_cast<BELPICRecord &>(record);
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		belpicRecord.getDataAttribute(tokenContext));
}

