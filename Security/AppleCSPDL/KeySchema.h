/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// KeySchema.h
//
#ifndef _H_KEYSCHEMA
#define _H_KEYSCHEMA

#include <Security/dlclient.h>

namespace KeySchema
{
	extern const CSSM_DB_ATTRIBUTE_INFO KeyClass;
	extern const CSSM_DB_ATTRIBUTE_INFO PrintName;
	extern const CSSM_DB_ATTRIBUTE_INFO Alias;
	extern const CSSM_DB_ATTRIBUTE_INFO Permanent;
	extern const CSSM_DB_ATTRIBUTE_INFO Private;
	extern const CSSM_DB_ATTRIBUTE_INFO Modifiable;
	extern const CSSM_DB_ATTRIBUTE_INFO Label;
	extern const CSSM_DB_ATTRIBUTE_INFO ApplicationTag;
	extern const CSSM_DB_ATTRIBUTE_INFO KeyCreator;
	extern const CSSM_DB_ATTRIBUTE_INFO KeyType;
	extern const CSSM_DB_ATTRIBUTE_INFO KeySizeInBits;
	extern const CSSM_DB_ATTRIBUTE_INFO EffectiveKeySize;
	extern const CSSM_DB_ATTRIBUTE_INFO StartDate;
	extern const CSSM_DB_ATTRIBUTE_INFO EndDate;
	extern const CSSM_DB_ATTRIBUTE_INFO Sensitive;
	extern const CSSM_DB_ATTRIBUTE_INFO AlwaysSensitive;
	extern const CSSM_DB_ATTRIBUTE_INFO Extractable;
	extern const CSSM_DB_ATTRIBUTE_INFO NeverExtractable;
	extern const CSSM_DB_ATTRIBUTE_INFO Encrypt;
	extern const CSSM_DB_ATTRIBUTE_INFO Decrypt;
	extern const CSSM_DB_ATTRIBUTE_INFO Derive;
	extern const CSSM_DB_ATTRIBUTE_INFO Sign;
	extern const CSSM_DB_ATTRIBUTE_INFO Verify;
	extern const CSSM_DB_ATTRIBUTE_INFO SignRecover;
	extern const CSSM_DB_ATTRIBUTE_INFO VerifyRecover;
	extern const CSSM_DB_ATTRIBUTE_INFO Wrap;
	extern const CSSM_DB_ATTRIBUTE_INFO Unwrap;

	extern const CSSM_DB_SCHEMA_ATTRIBUTE_INFO KeySchemaAttributeList[];
	extern const CSSM_DB_SCHEMA_INDEX_INFO KeySchemaIndexList[];
	extern const uint32 KeySchemaAttributeCount;
	extern const uint32 KeySchemaIndexCount;
};


#endif // _H_KEYSCHEMA
