/*
 * Copyright (c) 2002 Apple Computer, Inc. All Rights Reserved.
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

#include <Security/SecTrustedApplication.h>

#include "SecBridge.h"


CFTypeID
SecTrustedApplicationGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().trustedApplication.typeId;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecTrustedApplicationCreateFromPath(const char *path, SecTrustedApplicationRef *appRef)
{
	BEGIN_SECAPI
	RefPointer<TrustedApplication> app =
		path ? new TrustedApplication(path) : new TrustedApplication;
	Required(appRef) = gTypes().trustedApplication.handle(*app);
	END_SECAPI
}

/*!
 */
OSStatus SecTrustedApplicationCopyData(SecTrustedApplicationRef appRef,
	CFDataRef *dataRef)
{
	BEGIN_SECAPI
	const CssmData &data = gTypes().trustedApplication.required(appRef)->data();
	Required(dataRef) = CFDataCreate(NULL, (const UInt8 *)data.data(), data.length());
	END_SECAPI
}

OSStatus SecTrustedApplicationSetData(SecTrustedApplicationRef appRef,
	CFDataRef dataRef)
{
	BEGIN_SECAPI
	gTypes().trustedApplication.required(appRef)->data(cfData(dataRef));
	END_SECAPI
}

