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
// Schema.h
//
#ifndef _H_SCHEMA
#define _H_SCHEMA

//#include <Security/dlclient.h>
#include <Security/SecKeychainAPI.h>

namespace Security {

namespace KeychainCore {

namespace Schema {

CSSM_DB_RECORDTYPE recordTypeFor(SecItemClass itemClass);
SecItemClass itemClassFor(CSSM_DB_RECORDTYPE recordType);
const CSSM_DB_ATTRIBUTE_INFO &attributeInfo(SecKeychainAttrType attrType);

extern const CSSM_DB_ATTRIBUTE_INFO RelationID;
extern const CSSM_DB_ATTRIBUTE_INFO RelationName;
extern const CSSM_DB_ATTRIBUTE_INFO AttributeID;
extern const CSSM_DB_ATTRIBUTE_INFO AttributeNameFormat;
extern const CSSM_DB_ATTRIBUTE_INFO AttributeName;
extern const CSSM_DB_ATTRIBUTE_INFO AttributeNameID;
extern const CSSM_DB_ATTRIBUTE_INFO AttributeFormat;
extern const CSSM_DB_ATTRIBUTE_INFO IndexType;

extern const CSSM_DBINFO DBInfo;

} // end namespace Schema

} // end namespace KeychainCore

} // end namespace Security

#endif // _H_SCHEMA

