/*
 * Copyright (c) 2002-2017 Apple Inc. All Rights Reserved.
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
// CertificateValues.h - Objects in a Certificate
//
#ifndef _SECURITY_CERTIFICATEVALUES_H_
#define _SECURITY_CERTIFICATEVALUES_H_

#include <Security/SecBase.h>


namespace Security
{

namespace KeychainCore
{

class CertificateValues
{
	NOCOPY(CertificateValues)

public:

	CertificateValues(SecCertificateRef certificateRef);
	virtual ~CertificateValues() _NOEXCEPT;

	static CFStringRef remapLabelToKey(CFStringRef label);
	CFArrayRef copyPropertyValues(CFErrorRef *error);
	CFDictionaryRef copyFieldValues(CFArrayRef keys, CFErrorRef *error);
	CFDataRef copySerialNumber(CFErrorRef *error);
	CFDataRef copyNormalizedIssuerContent(CFErrorRef *error);
	CFDataRef copyNormalizedSubjectContent(CFErrorRef *error);
	CFDataRef copyIssuerSequence(CFErrorRef *error);
	CFDataRef copySubjectSequence(CFErrorRef *error);
	CFStringRef copyIssuerSummary(CFErrorRef *error);
	CFStringRef copySubjectSummary(CFErrorRef *error);
	CFDictionaryRef copyAttributeDictionary(CFErrorRef *error);
	bool isValid(CFAbsoluteTime verifyTime, CFErrorRef *error);
	CFAbsoluteTime notValidBefore(CFErrorRef *error);
	CFAbsoluteTime notValidAfter(CFErrorRef *error);

private:

	SecCertificateRef copySecCertificateRef(CFErrorRef *error);

	SecCertificateRef mCertificateRef;
	CFDataRef mCertificateData;
	CFArrayRef mCertificateProperties;
	static CFDictionaryRef mOIDRemap;
};


} // end namespace KeychainCore

} // end namespace Security

#endif // !_SECURITY_CERTIFICATEVALUES_H_
