/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
   File:      MDSAttrStrings.h

   Contains:  Static tables to map attribute names to numeric values.

   Copyright (c) 2001,2011,2014 Apple Inc. All Rights Reserved.
*/

#ifndef _MDS_ATTR_STRINGS_H_
#define _MDS_ATTR_STRINGS_H_  1

#include <Security/cssmtype.h>

namespace Security
{

/*
 * Each type of attribute has a name/value pair in a table of these:
 */
typedef struct {
	uint32			value;
	const char 		*name;
} MDSNameValuePair;

/*
 * Various tables.
 */

/* attributes in Object and Common relations */
extern const MDSNameValuePair MDSServiceNames[];		// CSSM_SERVICE_MASK

/* CSP attributes */
extern const MDSNameValuePair MDSContextTypeNames[];	// CSSM_CONTEXT_TYPE
extern const MDSNameValuePair MDSAttributeTypeNames[];	// CSSM_ATTRIBUTE_TYPE
extern const MDSNameValuePair MDSPaddingNames[];		// CSSM_PADDING
extern const MDSNameValuePair MDSCspFlagsNames[];		// CSSM_CSP_FLAGS
extern const MDSNameValuePair MDSAlgorithmNames[];		// CSSM_ALGORITHMS
extern const MDSNameValuePair MDSEncryptModeNames[];	// CSSM_ENCRYPT_MODE
extern const MDSNameValuePair MDSCspTypeNames[];		// CSSM_CSPTYPE
extern const MDSNameValuePair MDSUseeTagsNames[];		// CSSM_USEE_TAG
extern const MDSNameValuePair MDSCspReaderFlagsNames[];	// CSSM_CSP_READER_FLAGS
extern const MDSNameValuePair MDSCspScFlagsNames[];		// CSSM_SC_FLAGS

/* CL attributes */
extern const MDSNameValuePair MDSCertTypeNames[];		// CSSM_CERT_TYPE
extern const MDSNameValuePair MDSCrlTypeNames[];		// CSSM_CRL_TYPE
extern const MDSNameValuePair MDSCertBundleTypeNames[];	// CSSM_CERT_BUNDLE_TYPE
extern const MDSNameValuePair MDSCertTemplateTypeNames[];
														// CSSM_CL_TEMPLATE_TYPE

/* TP attributes */
/* CSSM_TP_AUTHORITY_REQUEST_CERTISSUE */
extern const MDSNameValuePair MDSTpAuthRequestNames[];	
											// CSSM_TP_AUTHORITY_REQUEST_CERTISSUE

/* DL attributes */
extern const MDSNameValuePair MDSDlTypeNames[];			// CSSM_DLTYPE
extern const MDSNameValuePair MDSDbConjunctiveNames[];	// CSSM_DB_CONJUNCTIVE
extern const MDSNameValuePair MDSDbOperatorNames[];		// CSSM_DB_OPERATOR
extern const MDSNameValuePair MDSNetProtocolNames[];	// CSSM_NET_PROTOCOL
extern const MDSNameValuePair MDSDbRetrievalModeNames[];// CSSM_DB_RETRIEVAL_MODES

/* misc. */
extern const MDSNameValuePair MDSAclSubjectTypeNames[];	// CSSM_ACL_SUBJECT_TYPE
extern const MDSNameValuePair MDSAclAuthTagNames[];		// CSSM_ACL_AUTHORIZATION_TAG
extern const MDSNameValuePair MDSSampleTypeNames[];		// CSSM_SAMPLE_TYPE
extern const MDSNameValuePair MDSKrPolicyTypeNames[];	// CSSM_KR_POLICY_TYPE

// extern const MDSNameValuePair MDSRecordTypeNames[];		// CSSM_DB_RECORDTYPE

/*
 * Use this function to convert a name, e.g. "CSSM_ALGCLASS_SIGNATURE", to 
 * its associated value as a uint32. Caller specifies proper lookup table
 * as an optimization to avoid grunging thru entire CDSA namespace on every
 * lookup. 
 *
 * If the specified name is not found, or if no MDSNameValuePair is specified, 
 * an attempt will be made to convert the incoming string to a number as if 
 * it were an ASCII hex (starts with "0x") or decimal (starts with any other numeric
 * string) string. If that fails, CSSMERR_CSSM_MDS_ERROR is returned.
 *
 * Values can be prefixed with "<<" indicating that the indicated
 * value is to be shifted 16 bits. Cf. CL Primary Relation, {Cert,Crl}TypeFormat.
 * This applies to both numeric and string tokens. 
 */
CSSM_RETURN MDSAttrNameToValue(
	const char *name,
	const MDSNameValuePair *table,
	uint32 &value);					// RETURNED
	
} // end namespace Security

#endif /* _MDS_ATTR_STRINGS_H_ */
