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
// Apple X.509 CRL-related session functions.
//

#include "AppleX509CLSession.h"

void
AppleX509CLSession::CrlDescribeFormat(
	uint32 &NumberOfFields,
	CSSM_OID_PTR &OidList)
{
	unimplemented();
}


void
AppleX509CLSession::CrlGetAllFields(
	const CssmData &Crl,
	uint32 &NumberOfCrlFields,
	CSSM_FIELD_PTR &CrlFields)
{
	unimplemented();
}


CSSM_HANDLE
AppleX509CLSession::CrlGetFirstFieldValue(
	const CssmData &Crl,
	const CssmData &CrlField,
	uint32 &NumberOfMatchedFields,
	CSSM_DATA_PTR &Value)
{
	unimplemented();
	return CSSM_INVALID_HANDLE;
}

	
bool
AppleX509CLSession::CrlGetNextFieldValue(
	CSSM_HANDLE ResultsHandle,
	CSSM_DATA_PTR &Value)
{
	unimplemented();
	return false;
}


void
AppleX509CLSession::IsCertInCrl(
	const CssmData &Cert,
	const CssmData &Crl,
	CSSM_BOOL &CertFound)
{
	unimplemented();
}

	
	
#if __MWERKS__
#pragma mark Cached
#endif
	
void
AppleX509CLSession::CrlCache(
	const CssmData &Crl,
	CSSM_HANDLE &CrlHandle)
{
	unimplemented();
}


CSSM_HANDLE
AppleX509CLSession::CrlGetFirstCachedFieldValue(
	CSSM_HANDLE CrlHandle,
	const CssmData *CrlRecordIndex,
	const CssmData &CrlField,
	uint32 &NumberOfMatchedFields,
	CSSM_DATA_PTR &Value)
{
	unimplemented();
	return CSSM_INVALID_HANDLE;
}


bool
AppleX509CLSession::CrlGetNextCachedFieldValue(
	CSSM_HANDLE ResultsHandle,
	CSSM_DATA_PTR &Value)
{
	unimplemented();
	return false;
}


void
AppleX509CLSession::IsCertInCachedCrl(
	const CssmData &Cert,
	CSSM_HANDLE CrlHandle,
	CSSM_BOOL &CertFound,
	CssmData &CrlRecordIndex)
{
	unimplemented();
}


void
AppleX509CLSession::CrlAbortCache(
	CSSM_HANDLE CrlHandle)
{
	unimplemented();
}


void
AppleX509CLSession::CrlAbortQuery(
	CSSM_HANDLE ResultsHandle)
{
	unimplemented();
}



#if __MWERKS__
#pragma mark Template
#endif

void
AppleX509CLSession::CrlCreateTemplate(
	uint32 NumberOfFields,
	const CSSM_FIELD *CrlTemplate,
	CssmData &NewCrl)
{
	unimplemented();
}


void
AppleX509CLSession::CrlSetFields(
	uint32 NumberOfFields,
	const CSSM_FIELD *CrlTemplate,
	const CssmData &OldCrl,
	CssmData &ModifiedCrl)
{
	unimplemented();
}


void
AppleX509CLSession::CrlAddCert(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &Cert,
	uint32 NumberOfFields,
	const CSSM_FIELD CrlEntryFields[],
	const CssmData &OldCrl,
	CssmData &NewCrl)
{
	unimplemented();
}


void
AppleX509CLSession::CrlRemoveCert(
	const CssmData &Cert,
	const CssmData &OldCrl,
	CssmData &NewCrl)
{
	unimplemented();
}


void
AppleX509CLSession::CrlGetAllCachedRecordFields(
	CSSM_HANDLE CrlHandle,
	const CssmData &CrlRecordIndex,
	uint32 &NumberOfFields,
	CSSM_FIELD_PTR &CrlFields)
{
	unimplemented();
}

void
AppleX509CLSession::CrlVerifyWithKey(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &CrlToBeVerified)
{
	unimplemented();
}


void
AppleX509CLSession::CrlVerify(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &CrlToBeVerified,
	const CssmData &SignerCert,
	const CSSM_FIELD *VerifyScope,
	uint32 ScopeSize)
{
	unimplemented();
}

void
AppleX509CLSession::CrlSign(
	CSSM_CC_HANDLE CCHandle,
	const CssmData &UnsignedCrl,
	const CSSM_FIELD *SignScope,
	uint32 ScopeSize,
	CssmData &SignedCrl)
{
	unimplemented();
}




