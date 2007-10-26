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
 *  Adornment.h
 *  TokendMuscle
 */

#ifndef _TOKEND_ADORNMENT_H_
#define _TOKEND_ADORNMENT_H_

#include <security_utilities/adornments.h>
#include <security_utilities/refcount.h>
#include <Security/SecCertificate.h>

namespace Tokend
{

class TokenContext;
class MetaRecord;
class MetaAttribute;
class Record;

//
// Adornment that refers to another record
//
class LinkedRecordAdornment : public Adornment
{
	NOCOPY(LinkedRecordAdornment)
public:
	LinkedRecordAdornment(RefPointer<Record> record);
	~LinkedRecordAdornment();
	Record &record();

private:
	RefPointer<Record> mRecord;
};


class SecCertificateAdornment : public Adornment
{
	NOCOPY(SecCertificateAdornment)
public:
	SecCertificateAdornment(TokenContext *tokenContext,
		const MetaAttribute &metaAttribute, Record &record);
	~SecCertificateAdornment();
	SecCertificateRef certificate();
	SecKeychainItemRef certificateItem();

private:
	SecCertificateRef mCertificate;
};

} // end namespace Tokend

#endif /* !_TOKEND_ADORNMENT_H_ */

