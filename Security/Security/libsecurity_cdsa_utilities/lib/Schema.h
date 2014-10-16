/*
 * Copyright (c) 2000-2004,2006,2011,2014 Apple Inc. All Rights Reserved.
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

// CRL attributes and schema
extern const CSSM_DB_ATTRIBUTE_INFO kX509CrlCrlType;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CrlCrlEncoding;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CrlPrintName;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CrlAlias;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CrlIssuer;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CrlSerialNumber;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CrlThisUpdate;
extern const CSSM_DB_ATTRIBUTE_INFO kX509CrlNextUpdate;

extern const CSSM_DB_SCHEMA_ATTRIBUTE_INFO X509CrlSchemaAttributeList[];
extern const CSSM_DB_SCHEMA_INDEX_INFO X509CrlSchemaIndexList[];
extern const uint32 X509CrlSchemaAttributeCount;
extern const uint32 X509CrlSchemaIndexCount;

// UserTrust records attributes and schema
extern const CSSM_DB_ATTRIBUTE_INFO kUserTrustTrustedCertificate;
extern const CSSM_DB_ATTRIBUTE_INFO kUserTrustTrustedPolicy;

extern const CSSM_DB_SCHEMA_ATTRIBUTE_INFO UserTrustSchemaAttributeList[];
extern const CSSM_DB_SCHEMA_INDEX_INFO UserTrustSchemaIndexList[];
extern const uint32 UserTrustSchemaAttributeCount;
extern const uint32 UserTrustSchemaIndexCount;

// UnlockReferral records attributes and schema
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralType;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralDbName;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralDbGuid;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralDbSSID;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralDbSSType;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralDbNetname;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralKeyLabel;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralKeyAppTag;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralPrintName;
extern const CSSM_DB_ATTRIBUTE_INFO kUnlockReferralAlias;

extern const CSSM_DB_SCHEMA_ATTRIBUTE_INFO UnlockReferralSchemaAttributeList[];
extern const CSSM_DB_SCHEMA_INDEX_INFO UnlockReferralSchemaIndexList[];
extern const uint32 UnlockReferralSchemaAttributeCount;
extern const uint32 UnlockReferralSchemaIndexCount;

// Extended Attribute record attributes and schema
extern const CSSM_DB_ATTRIBUTE_INFO kExtendedAttributeRecordType;
extern const CSSM_DB_ATTRIBUTE_INFO kExtendedAttributeItemID;
extern const CSSM_DB_ATTRIBUTE_INFO kExtendedAttributeAttributeName;
extern const CSSM_DB_ATTRIBUTE_INFO kExtendedAttributeModDate;
extern const CSSM_DB_ATTRIBUTE_INFO kExtendedAttributeAttributeValue;

extern const CSSM_DB_SCHEMA_ATTRIBUTE_INFO ExtendedAttributeSchemaAttributeList[];
extern const CSSM_DB_SCHEMA_INDEX_INFO ExtendedAttributeSchemaIndexList[];
extern const uint32 ExtendedAttributeSchemaAttributeCount;
extern const uint32 ExtendedAttributeSchemaIndexCount;

} // end namespace Schema

} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_SCHEMA_H_
