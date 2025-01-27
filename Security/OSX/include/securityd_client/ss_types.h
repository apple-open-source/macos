/*
 * Copyright (c) 2000-2007,2011 Apple Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _H_SS_TYPES
#define _H_SS_TYPES

#include <sys/syslimits.h>
#include <securityd_client/handletypes.h>

//
// ss_types - common type definitions for securityd-related IPC services
//
#include "ssclient.h"

#define __MigTypeCheck 1


typedef void *Data;
typedef void *XMLBlob;
typedef void *XMLBlobOut;
typedef void *Pointer;
typedef void *BasePointer;

typedef const char *CssmString;

typedef const char *FilePath;
typedef char FilePathOut[PATH_MAX];
typedef void *HashData;
typedef char HashDataOut[maxUcspHashLength];
typedef const char *RelationName;


#ifdef __cplusplus

namespace Security {

using namespace SecurityServer;


// @@@ OBSOLETE BEYOND THIS POINT (SecurityTokend uses this still)

typedef void *ContextAttributes;
typedef Context::Attr *ContextAttributesPointer;

typedef CssmKey *CssmKeyPtr;
typedef AclEntryPrototype *AclEntryPrototypePtr;
typedef AclEntryInput *AclEntryInputPtr;
typedef AclEntryInfo *AclEntryInfoPtr;
typedef AclOwnerPrototype *AclOwnerPrototypePtr;
typedef AccessCredentials *AccessCredentialsPtr;
typedef CssmDeriveData *CssmDeriveDataPtr;

typedef CssmDbRecordAttributeData *CssmDbRecordAttributeDataPtr;
typedef CssmNetAddress *CssmNetAddressPtr;
typedef CssmQuery *CssmQueryPtr;
typedef CssmSubserviceUid *CssmSubserviceUidPtr;
typedef CSSM_DBINFO *CSSM_DBINFOPtr;
typedef CSSM_DB_SCHEMA_ATTRIBUTE_INFO *CSSM_DB_SCHEMA_ATTRIBUTE_INFOPtr;
typedef CSSM_DB_SCHEMA_INDEX_INFO *CSSM_DB_SCHEMA_INDEX_INFOPtr;
typedef CSSM_NAME_LIST *CSSM_NAME_LISTPtr;
typedef void *VoidPtr;

typedef CssmKey::Header CssmKeyHeader;

typedef SecGuestRef *GuestChain;


//
// MIG-used translation functions
//
inline Context &inTrans(CSSM_CONTEXT &arg) { return Context::overlay(arg); }
inline CssmKey &inTrans(CSSM_KEY &arg) { return CssmKey::overlay(arg); }
inline CSSM_KEY &outTrans(CssmKey &key) { return key; }

} // end namespace Security

#endif //__cplusplus


//
// MIG-used byte swapping macros
//
#define __NDR_convert__int_rep__BasePointer__defined
#define __NDR_convert__int_rep__BasePointer(a, f)	/* do not flip */

#endif //_H_SS_TYPES
