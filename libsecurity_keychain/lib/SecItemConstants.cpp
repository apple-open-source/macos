/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

#include <CoreFoundation/CFString.h>

/* String constant declarations */

#define SEC_CONST_DECL(k,v) const CFTypeRef (k) = (CFTypeRef)(CFSTR(v));

/* Class Key Constant */
SEC_CONST_DECL (kSecClass, "_kSecClass");

/* Class Value Constants */
SEC_CONST_DECL (kSecClassGenericPassword, "_kSecClassGenericPassword");
SEC_CONST_DECL (kSecClassInternetPassword, "_kSecClassInternetPassword");
SEC_CONST_DECL (kSecClassAppleSharePassword, "_kSecClassAppleSharePassword");
SEC_CONST_DECL (kSecClassCertificate, "_kSecClassCertificate");
SEC_CONST_DECL (kSecClassPublicKey, "_kSecClassPublicKey");
SEC_CONST_DECL (kSecClassPrivateKey, "_kSecClassPrivateKey");
SEC_CONST_DECL (kSecClassSymmetricKey, "_kSecClassSymmetricKey");
SEC_CONST_DECL (kSecClassIdentity, "_kSecClassIdentity");

/* Attribute Key Constants */
SEC_CONST_DECL (kSecAttrCreationDate, "_kSecAttrCreationDate");
SEC_CONST_DECL (kSecAttrModifcationDate, "_kSecAttrModifcationDate");
SEC_CONST_DECL (kSecAttrDescription, "_kSecAttrDescription");
SEC_CONST_DECL (kSecAttrComment, "_kSecAttrComment");
SEC_CONST_DECL (kSecAttrCreator, "_kSecAttrCreator");
SEC_CONST_DECL (kSecAttrType, "_kSecAttrType");
SEC_CONST_DECL (kSecAttrLabel, "_kSecAttrLabel");
SEC_CONST_DECL (kSecAttrInvisible, "_kSecAttrInvisible");
SEC_CONST_DECL (kSecAttrNegative, "_kSecAttrNegative");
SEC_CONST_DECL (kSecAttrAccount, "_kSecAttrAccount");
SEC_CONST_DECL (kSecAttrService, "_kSecAttrService");
SEC_CONST_DECL (kSecAttrGeneric, "_kSecAttrGeneric");
SEC_CONST_DECL (kSecAttrSecurityDomain, "_kSecAttrSecurityDomain");
SEC_CONST_DECL (kSecAttrServer, "_kSecAttrServer");
SEC_CONST_DECL (kSecAttrProtocol, "_kSecAttrProtocol");
SEC_CONST_DECL (kSecAttrAuthenticationType, "_kSecAttrAuthenticationType");
SEC_CONST_DECL (kSecAttrPort, "_kSecAttrPort");
SEC_CONST_DECL (kSecAttrPath, "_kSecAttrPath");
SEC_CONST_DECL (kSecAttrVolume, "_kSecAttrVolume");
SEC_CONST_DECL (kSecAttrAddress, "_kSecAttrAddress");
SEC_CONST_DECL (kSecAttrAFPServerSignature, "_kSecAttrAFPServerSignature");
SEC_CONST_DECL (kSecAttrAlias, "_kSecAttrAlias");
SEC_CONST_DECL (kSecAttrSubject, "_kSecAttrSubject");
SEC_CONST_DECL (kSecAttrIssuer, "_kSecAttrIssuer");
SEC_CONST_DECL (kSecAttrSerialNumber, "_kSecAttrSerialNumber");
SEC_CONST_DECL (kSecAttrSubjectKeyID, "_kSecAttrSubjectKeyID");
SEC_CONST_DECL (kSecAttrPublicKeyHash, "_kSecAttrPublicKeyHash");
SEC_CONST_DECL (kSecAttrCertificateType, "_kSecAttrCertificateType");
SEC_CONST_DECL (kSecAttrCertificateEncoding, "_kSecAttrCertificateEncoding");
SEC_CONST_DECL (kSecAttrKeyClass, "_kSecAttrKeyClass");
SEC_CONST_DECL (kSecAttrApplicationLabel, "_kSecAttrApplicationLabel");
SEC_CONST_DECL (kSecAttrPermanent, "_kSecAttrPermanent");
SEC_CONST_DECL (kSecAttrPrivate, "_kSecAttrPrivate");
SEC_CONST_DECL (kSecAttrModifiable, "_kSecAttrModifiable");
SEC_CONST_DECL (kSecAttrApplicationTag, "_kSecAttrApplicationTag");
SEC_CONST_DECL (kSecAttrKeyCreator, "_kSecAttrKeyCreator");
SEC_CONST_DECL (kSecAttrKeyType, "_kSecAttrKeyType");
SEC_CONST_DECL (kSecAttrKeySizeInBits, "kSecAttrKeySizeInBits");
SEC_CONST_DECL (kSecAttrEffectiveKeySize, "_kSecAttrEffectiveKeySize");
SEC_CONST_DECL (kSecAttrStartDate, "_kSecAttrStartDate");
SEC_CONST_DECL (kSecAttrEndDate, "_kSecAttrEndDate");
SEC_CONST_DECL (kSecAttrSensitive, "_kSecAttrSensitive");
SEC_CONST_DECL (kSecAttrAlwaysSensitive, "_kSecAttrAlwaysSensitive");
SEC_CONST_DECL (kSecAttrExtractable, "_kSecAttrExtractable");
SEC_CONST_DECL (kSecAttrNeverExtractable, "_kSecAttrNeverExtractable");
SEC_CONST_DECL (kSecAttrEncrypt, "_kSecAttrEncrypt");
SEC_CONST_DECL (kSecAttrDecrypt, "_kSecAttrDecrypt");
SEC_CONST_DECL (kSecAttrDerive, "_kSecAttrDerive");
SEC_CONST_DECL (kSecAttrSign, "_kSecAttrSign");
SEC_CONST_DECL (kSecAttrVerify, "_kSecAttrVerify");
SEC_CONST_DECL (kSecAttrSignRecover, "_kSecAttrSignRecover");
SEC_CONST_DECL (kSecAttrVerifyRecover, "_kSecAttrVerifyRecover");
SEC_CONST_DECL (kSecAttrWrap, "_kSecAttrWrap");
SEC_CONST_DECL (kSecAttrUnwrap, "_kSecAttrUnwrap");

/* Search Constants */
SEC_CONST_DECL (kSecMatchKeyUsage, "_kSecMatchKeyUsage");
SEC_CONST_DECL (kSecMatchPolicy, "_kSecMatchPolicy");
SEC_CONST_DECL (kSecMatchItemList, "_kSecMatchItemList");
SEC_CONST_DECL (kSecMatchSearchList, "_kSecMatchSearchList");
SEC_CONST_DECL (kSecMatchIssuers, "_kSecMatchIssuers");
SEC_CONST_DECL (kSecMatchEmailAddressIfPresent, "_kSecMatchEmailAddressIfPresent");
SEC_CONST_DECL (kSecMatchSubjectContains, "_kSecMatchSubjectContains");
SEC_CONST_DECL (kSecMatchCaseInsensitive, "_kSecMatchCaseInsensitive");
SEC_CONST_DECL (kSecMatchTrustedOnly, "_kSecMatchTrustedOnly");
SEC_CONST_DECL (kSecMatchValidOnDate, "_kSecMatchValidOnDate");
SEC_CONST_DECL (kSecMatchLimit, "_kSecMatchLimit");
SEC_CONST_DECL (kSecMatchLimitOne, "_kSecMatchLimitOne");
SEC_CONST_DECL (kSecMatchLimitAll, "_kSecMatchLimitAll");

/* Return Type Constants */
SEC_CONST_DECL (kSecReturnData, "_kSecReturnData");
SEC_CONST_DECL (kSecReturnAttributes, "_kSecReturnAttributes");
SEC_CONST_DECL (kSecReturnRef, "_kSecReturnRef");
SEC_CONST_DECL (kSecReturnPersistentRef, "_kSecReturnPersistentRef");

/* Other Constants */
SEC_CONST_DECL (kSecUseItemList, "_kSecUseItemList");

/* Attribute Constants (Private) */
SEC_CONST_DECL (kSecAttrScriptCode, "_kSecAttrScriptCode");
SEC_CONST_DECL (kSecAttrCustomIcon, "_kSecAttrCustomIcon");
SEC_CONST_DECL (kSecAttrCRLType, "_kSecAttrCRLType");
SEC_CONST_DECL (kSecAttrCRLEncoding, "_kSecAttrCRLEncoding");

/* Other Constants (Private) */
SEC_CONST_DECL (kSecUseKeychain, "_kSecUseKeychain");
SEC_CONST_DECL (kSecUseKeychainList, "_kSecUseKeychainList");

