/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

#include "SecEncryptTransform.h"
#include "EncryptTransform.h"

/* --------------------------------------------------------------------------
 Create the declared CFStringRefs
 -------------------------------------------------------------------------- */

CFStringRef kSecPaddingNoneKey = CFSTR("SecPaddingNoneKey");
CFStringRef kSecPaddingPKCS1Key = CFSTR("SecPaddingPKCS1Key");
CFStringRef kSecPaddingPKCS5Key = CFSTR("SecPaddingPKCS5Key");
CFStringRef kSecPaddingPKCS7Key = CFSTR("SecPaddingPKCS7Key");
CFStringRef kSecPaddingOAEPKey = CFSTR("OAEPPadding");
CFStringRef kSecOAEPMGF1DigestAlgorithmAttributeName = CFSTR("OAEPMGF1DigestAlgo");

CFStringRef kSecModeNoneKey = CFSTR("SecModeNoneKey");
CFStringRef kSecModeECBKey = CFSTR("SecModeECBKey");
CFStringRef kSecModeCBCKey = CFSTR("SecModeCBCKey");
CFStringRef kSecModeCFBKey = CFSTR("SecModeCFBKey");
CFStringRef kSecModeOFBKey = CFSTR("SecModeOFBKey");

CFStringRef kSecOAEPMessageLengthAttributeName = CFSTR("OAEPMessageLength");
CFStringRef kSecOAEPEncodingParametersAttributeName = CFSTR("OAEPEncodingParameters");

CFStringRef kSecEncryptKey = CFSTR("SecEncryptKey");
CFStringRef kSecPaddingKey = CFSTR("SecPaddingKey");
CFStringRef kSecIVKey = CFSTR("SecIVKey");
CFStringRef kSecEncryptionMode = CFSTR("SecEncryptionMode");


SecTransformRef SecEncryptTransformCreate(SecKeyRef keyRef, CFErrorRef* error)
{
	SecTransformRef etRef = EncryptTransform::Make();
	EncryptTransform* et = (EncryptTransform*) CoreFoundationHolder::ObjectFromCFType(etRef);
	if (et->InitializeObject(keyRef, error))
	{
		return etRef;
	}
	else
	{
		
		CFRelease(etRef);
		return NULL;
	}
}


CFTypeID SecEncryptTransformGetTypeID()
{
	return Transform::GetCFTypeID();
}

SecTransformRef SecDecryptTransformCreate(SecKeyRef keyRef, CFErrorRef* error)
{
	
	SecTransformRef dtRef = DecryptTransform::Make();
	DecryptTransform* dt = (DecryptTransform*) CoreFoundationHolder::ObjectFromCFType(dtRef);
	if (dt->InitializeObject(keyRef, error))
	{
		return dtRef;
	}
	else
	{
		CFRelease(dtRef);
		return NULL;
	}
}

CFTypeID SecDecryptTransformGetTypeID()
{
	return Transform::GetCFTypeID();
}
