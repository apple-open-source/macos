#include "SecManifest.h"
#include <security_utilities/security_utilities.h>
#include "Manifest.h"
#include <security_utilities/seccfobject.h>
#include <security_cdsa_utilities/cssmbridge.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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

#define API_BEGIN \
	try {

#define API_END \
	} \
	catch (const MacOSError &err) { return err.osStatus(); } \
	catch (const std::bad_alloc &) { return memFullErr; } \
	catch (...) { return internalComponentErr; } \
    return noErr;

#define API_END_GENERIC_CATCH		} catch (...) { return; }

#define API_END_ERROR_CATCH(bad)	} catch (...) { return bad; }



OSStatus SecManifestGetVersion (UInt32 *version)
{
	secdebug ("manifest", "SecManifestGetVersion");
	*version = 0x01000000;
	return noErr;
}



OSStatus SecManifestCreate(SecManifestRef *manifest)
{
	API_BEGIN
	
	Manifest* manifestPtr = new Manifest ();
	*manifest = (SecManifestRef) manifestPtr;
	
	secdebug ("manifest", "SecManifestCreate(%p)", manifest);
	
	API_END
}



void SecManifestRelease (SecManifestRef manifest)
{
	delete (Manifest*) manifest;
}



static const char* GetDescription (CFTypeRef object)
{
	return CFStringGetCStringPtr (CFCopyDescription (object), kCFStringEncodingMacRoman);
}



OSStatus SecManifestVerifySignature (CFDataRef data,
									 SecManifestTrustSetupCallback setupCallback,
									 void* setupContext,
									 SecManifestTrustEvaluateCallback evaluateCallback,
									 void* evaluateContext,
									 SecManifestRef *manifest)
{
	return SecManifestVerifySignatureWithPolicy (data, setupCallback, setupContext, evaluateCallback,
												 evaluateContext, NULL, manifest);
}



OSStatus SecManifestVerifySignatureWithPolicy (CFDataRef data,
											   SecManifestTrustSetupCallback setupCallback,
											   void* setupContext,
											   SecManifestTrustEvaluateCallback evaluateCallback,
											   void* evaluateContext,
											   SecPolicyRef policyRef,
											   SecManifestRef *manifest)
{
	API_BEGIN
	
	secdebug ("manifest", "SecManifestVerifySignature (%s, %p, %p, %p, %p)", GetDescription (data), setupCallback, setupContext, evaluateCallback, evaluateContext);
	
	Required (setupCallback);
	Required (evaluateCallback);

	Manifest* mp = new Manifest ();
	
	// make a temporary manifest for this operation
	Manifest tm;
	tm.MakeSigner (kAppleSigner);
	
	try
	{

		tm.GetSigner ()->Verify (data, setupCallback, setupContext, evaluateCallback, evaluateContext,
								 policyRef, manifest == NULL ? NULL : &mp->GetManifestInternal ());
		if (manifest == NULL)
		{
			delete mp;
		}
		else
		{
			*manifest = (SecManifestRef) mp;
		}
	}
	catch (...)
	{
		delete mp;
		throw;
	}
	
	API_END
}



OSStatus SecManifestCreateSignature(SecManifestRef manifest, UInt32 options, CFDataRef *data)
{
	API_BEGIN
	
	secdebug ("manifest", "SecManifestCreateSignature(%p, %ul, %p)", manifest, (unsigned int) options, data);
	Manifest* manifestPtr = (Manifest*) manifest;
	
	if (options != 0)
	{
		return unimpErr;
	}
	
	// check to see if there is a serializer present
	const ManifestSigner* signer = manifestPtr->GetSigner ();
	
	if (signer == NULL) // no serializer?
	{
		manifestPtr->MakeSigner (kAppleSigner);
	}
	
	*data = manifestPtr->GetSigner ()->Export (manifestPtr->GetManifestInternal ());
	
	API_END
}



OSStatus SecManifestAddObject(SecManifestRef manifest, CFTypeRef object, CFArrayRef exceptionList)
{
	API_BEGIN

	secdebug ("manifest", "SecManifestAddObject(%p), %s, %s",
						  manifest, GetDescription (object),
						  exceptionList ? GetDescription (exceptionList) : "NULL");
	
	Manifest* manifestPtr = (Manifest*) manifest;
	manifestPtr->GetManifestInternal ().GetItemList ().AddObject (object, exceptionList);
	
	API_END
}



OSStatus SecManifestCompare(SecManifestRef manifest1, SecManifestRef manifest2, SecManifestCompareOptions options)
{
	API_BEGIN
	
	secdebug ("manifest", "SecManifestVerify(%p, %p, %d)", manifest1, manifest2, (int) options);

	ManifestInternal &m1 = ((Manifest*) (manifest1))->GetManifestInternal ();
	ManifestInternal &m2 = ((Manifest*) (manifest2))->GetManifestInternal ();
	
	ManifestInternal::CompareManifests (m1, m2, options);
	
	API_END
}



OSStatus SecManifestAddSigner(SecManifestRef manifest, SecIdentityRef identity)
{
	API_BEGIN
	
	secdebug ("manifest", "SecManifestAddSigner(%p, %p)", manifest, identity);
	Manifest* manifestPtr = (Manifest*) (manifest);
	
	// check to see if there is a serializer present
	const ManifestSigner* signer = manifestPtr->GetSigner ();
	
	if (signer == NULL) // no serializer?
	{
		manifestPtr->MakeSigner (kAppleSigner);
	}

	manifestPtr->GetSigner ()->AddSigner (identity);
	
	API_END
}



