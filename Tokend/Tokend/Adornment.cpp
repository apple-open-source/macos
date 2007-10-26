/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  Adornment.cpp
 *  TokendMuscle
 */

#include "Adornment.h"
#include "MetaAttribute.h"
#include "MetaRecord.h"
#include "Record.h"

namespace Tokend
{


//
// LinkedRecordAdornment
//
//const Adornment::Key LinkedRecordAdornment::key = "LinkedRecordAdornment";

LinkedRecordAdornment::LinkedRecordAdornment(RefPointer<Record> record) :
	mRecord(record)
{
}

LinkedRecordAdornment::~LinkedRecordAdornment()
{
}

Record &LinkedRecordAdornment::record()
{
	return *mRecord;
}


//
// SecCertificateAdornment
//
SecCertificateAdornment::SecCertificateAdornment(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	// Get the data for record (the actual certificate).
	const MetaAttribute &dma =
		metaAttribute.metaRecord().metaAttributeForData();
	const Attribute &data = dma.attribute(tokenContext, record);

	// Data should have exactly one value.
	if (data.size() != 1)
		CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);

	// Create a new adornment using the data from the certificate.
	OSStatus status = SecCertificateCreateFromData(&data[0], CSSM_CERT_X_509v3,
		CSSM_CERT_ENCODING_BER, &mCertificate);
	if (status)
		MacOSError::throwMe(status);
}

SecCertificateAdornment::~SecCertificateAdornment()
{
	CFRelease(mCertificate);
}

SecCertificateRef SecCertificateAdornment::certificate()
{
	return mCertificate; 
}

SecKeychainItemRef SecCertificateAdornment::certificateItem()
{
	return SecKeychainItemRef(mCertificate);
}


}	// end namespace Tokend

