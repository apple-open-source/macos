/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _SECURITY_SCHEMA_H_
#define _SECURITY_SCHEMA_H_

#include <Security/SecKeychainItem.h>

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

// Certificate attributes and schema
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificateCertType;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificateCertEncoding;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificatePrintName;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificateAlias;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificateSubject;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificateIssuer;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificateSerialNumber;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificateSubjectKeyIdentifier;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CertificatePublicKeyHash;

extern const CSSM_DB_SCHEMA_ATTRIBUTE_INFO X509CertificateSchemaAttributeList[];
extern const CSSM_DB_SCHEMA_INDEX_INFO X509CertificateSchemaIndexList[];
extern const uint32 X509CertificateSchemaAttributeCount;
extern const uint32 X509CertificateSchemaIndexCount;

// UserTrust records attributes and schema
extern const CSSM_DB_ATTRIBUTE_INFO kUserTrustTrustedCertificate;
extern const CSSM_DB_ATTRIBUTE_INFO kUserTrustTrustedPolicy;

extern const CSSM_DB_SCHEMA_ATTRIBUTE_INFO UserTrustSchemaAttributeList[];
extern const CSSM_DB_SCHEMA_INDEX_INFO UserTrustSchemaIndexList[];
extern const uint32 UserTrustSchemaAttributeCount;
extern const uint32 UserTrustSchemaIndexCount;

} // end namespace Schema

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_SCHEMA_H_
