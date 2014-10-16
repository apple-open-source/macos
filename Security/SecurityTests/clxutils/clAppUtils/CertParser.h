/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */

/*
 * CertParser.h - cert parser with autorelease of fetched fields
 *
 * Created 24 October 2003 by Doug Mitchell
 */
 
#ifndef	_CERT_PARSER_H_
#define _CERT_PARSER_H_

#include <Security/Security.h>
#include <vector>

using std::vector;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * We store an vector<> of these as an "autorelease" pool of fetched fields. 
 */
class CP_FetchedField;

class CertParser
{
public:
	/* 
	 * Construct with or without data - you can add the data later with 
	 * initWithData() to parse without exceptions 
	 */
	 
	CertParser();					// must be used with initWithSecCert to get clHand
	CertParser(						// use with initWithData
		CSSM_CL_HANDLE		clHand);
	CertParser(
		CSSM_CL_HANDLE		clHand,
		const CSSM_DATA 	&certData);
	CertParser(
		SecCertificateRef 	secCert);
	
	/* frees all the fields we fetched */
	~CertParser();
	
	/*
	 * No cert- or CDSA-related exceptions thrown by remainder
	 */
	CSSM_RETURN initWithData(
		const CSSM_DATA 	&certData);
	OSStatus	initWithSecCert(
		SecCertificateRef 	secCert);
	CSSM_RETURN	initWithCFData(
		CFDataRef			cfData);
	
	/*
	 * Obtain atrbitrary field from cached cert. This class takes care of freeing
	 * the field in its destructor. 
	 *
	 * Returns NULL if field not found (not exception). 
	 *
	 * Caller optionally specifies field length to check - specifying zero means
	 * "don't care, don't check". Actual field length always returned in fieldLength. 
	 */
	const void *fieldForOid(
		const CSSM_OID		&oid,
		CSSM_SIZE			&fieldLength);		// IN/OUT
		
	/*
	* Conveneince routine to fetch an extension we "know" the CL can parse.
	* The return value gets cast to one of the CE_Data types.
	*/
	const void *extensionForOid(
		const CSSM_OID		&oid);

private:
	void initFields();
	
	CSSM_CL_HANDLE			mClHand;
	CSSM_HANDLE				mCacheHand;			// the parsed & cached cert
	vector<CP_FetchedField *> mFetchedFields;
};

#ifdef __cplusplus
}
#endif

#endif	/* _CERT_PARSER_H_ */

