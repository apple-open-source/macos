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

#include <Security/SecTrustedApplicationPriv.h>
#include <Security/TrustedApplication.h>
#include <Security/ssclient.h>

#include "SecBridge.h"


CFTypeID
SecTrustedApplicationGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().TrustedApplication.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


OSStatus
SecTrustedApplicationCreateFromPath(const char *path, SecTrustedApplicationRef *appRef)
{
	BEGIN_SECAPI
	SecPointer<TrustedApplication> app =
		path ? new TrustedApplication(path) : new TrustedApplication;
	Required(appRef) = app->handle();
	END_SECAPI
}

/*!
 */
OSStatus SecTrustedApplicationCopyData(SecTrustedApplicationRef appRef,
	CFDataRef *dataRef)
{
	BEGIN_SECAPI
	const CssmData &data = TrustedApplication::required(appRef)->data();
	Required(dataRef) = CFDataCreate(NULL, (const UInt8 *)data.data(), data.length());
	END_SECAPI
}

OSStatus SecTrustedApplicationSetData(SecTrustedApplicationRef appRef,
	CFDataRef dataRef)
{
	BEGIN_SECAPI
	TrustedApplication::required(appRef)->data(cfData(dataRef));
	END_SECAPI
}


OSStatus
SecTrustedApplicationValidateWithPath(SecTrustedApplicationRef appRef, const char *path)
{
	BEGIN_SECAPI
	TrustedApplication &app = *TrustedApplication::required(appRef);
	if (!app.sameSignature(path ? path : app.path()))
		return CSSMERR_CSP_VERIFY_FAILED;
	END_SECAPI
}


OSStatus
SecTrustedApplicationMakeEquivalent(SecTrustedApplicationRef oldRef,
	SecTrustedApplicationRef newRef, UInt32 flags)
{
	BEGIN_SECAPI
	if (flags & ~kSecApplicationValidFlags)
		return paramErr;
	SecurityServer::ClientSession ss(CssmAllocator::standard(), CssmAllocator::standard());
	TrustedApplication *oldApp = TrustedApplication::required(oldRef);
	TrustedApplication *newApp = TrustedApplication::required(newRef);
	ss.addCodeEquivalence(oldApp->signature(), newApp->signature(), oldApp->path(),
		flags & kSecApplicationFlagSystemwide);
	END_SECAPI
}

OSStatus
SecTrustedApplicationRemoveEquivalence(SecTrustedApplicationRef appRef, UInt32 flags)
{
	BEGIN_SECAPI
	if (flags & ~kSecApplicationValidFlags)
		return paramErr;
	SecurityServer::ClientSession ss(CssmAllocator::standard(), CssmAllocator::standard());
	TrustedApplication *app = TrustedApplication::required(appRef);
	ss.removeCodeEquivalence(app->signature(), app->path(),
		flags & kSecApplicationFlagSystemwide);
	END_SECAPI
}


/*
 * Check to see if an application at a given path is a candidate for
 * pre-emptive code equivalency establishment
 */
OSStatus
SecTrustedApplicationIsUpdateCandidate(const char *installroot, const char *path)
{
    BEGIN_SECAPI
	
	// strip installroot
	if (installroot) {
		size_t rootlen = strlen(installroot);
		if (!strncmp(installroot, path, rootlen))
			path += rootlen - 1;	// keep the slash
	}
		
	// look up in database
	static ModuleNexus<PathDatabase> paths;
	if (!paths()[path])
		return CSSMERR_DL_RECORD_NOT_FOUND;	// whatever
    END_SECAPI
}


/*
 * Point the system at another system root for equivalence use.
 * This is for system update installers (only)!
 */
OSStatus
SecTrustedApplicationUseAlternateSystem(const char *systemRoot)
{
	BEGIN_SECAPI
	Required(systemRoot);
	SecurityServer::ClientSession ss(CssmAllocator::standard(), CssmAllocator::standard());
	ss.setAlternateSystemRoot(systemRoot);
	END_SECAPI
}
