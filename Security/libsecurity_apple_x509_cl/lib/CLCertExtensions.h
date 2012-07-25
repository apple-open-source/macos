/*
 * Copyright (c) 2000-2010 Apple Inc. All Rights Reserved.
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


/*
 * CLCertExtensions.h - extern declarations of get/set/free functions implemented in
 *                    CertExtensions,cpp and used only in CertFields.cpp.
 */

#ifndef	_CL_CERT_EXTENSIONS_H_
#define _CL_CERT_EXTENSIONS_H_

#include "DecodedCert.h"
#include "CLFieldsCommon.h"

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Functions to map OID --> {get,set,free}field
 */
getItemFieldFcn getFieldKeyUsage, getFieldBasicConstraints, 
	getFieldExtKeyUsage,
	getFieldSubjectKeyId, getFieldAuthorityKeyId, getFieldSubjAltName,
	getFieldIssuerAltName,
	getFieldCertPolicies, getFieldNetscapeCertType, getFieldCrlDistPoints,
	getFieldAuthInfoAccess, getFieldSubjInfoAccess, getFieldUnknownExt,
	getFieldQualCertStatements,
	getFieldNameConstraints, getFieldPolicyMappings, getFieldPolicyConstraints,
	getFieldInhibitAnyPolicy;
setItemFieldFcn setFieldKeyUsage, setFieldBasicConstraints, 
	setFieldExtKeyUsage,
	setFieldSubjectKeyId, setFieldAuthorityKeyId, setFieldSubjIssuerAltName,
	setFieldCertPolicies, setFieldNetscapeCertType, setFieldCrlDistPoints,
	setFieldAuthInfoAccess, setFieldUnknownExt, setFieldQualCertStatements,
	setFieldNameConstraints, setFieldPolicyMappings, setFieldPolicyConstraints,
	setFieldInhibitAnyPolicy;
freeFieldFcn freeFieldExtKeyUsage, freeFieldSubjectKeyId,
	freeFieldAuthorityKeyId, freeFieldSubjIssuerAltName, 
	freeFieldCertPolicies, 
	freeFieldCrlDistPoints, freeFieldInfoAccess, freeFieldUnknownExt,
	freeFieldQualCertStatements,
	freeFieldNameConstraints, freeFieldPolicyMappings, freeFieldPolicyConstraints;
	
#ifdef	__cplusplus
}
#endif

#endif	/* _CERT_EXTENSIONS_H_*/
