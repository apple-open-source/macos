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
 *
 * SecImportExportAgg.cpp - private routines used by SecImportExport.h for 
 *						    aggregate (PKCS12 and PKCS7) conversion.
 */

#include "SecImportExportAgg.h"
#include "SecExternalRep.h"
#include "SecImportExportUtils.h"
#include "SecNetscapeTemplates.h"
#include "Certificate.h"
#include <security_pkcs12/SecPkcs12.h>
#include <Security/SecBase.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsSignedData.h>
#include <security_asn1/SecNssCoder.h>	
#include <security_asn1/nssUtils.h>		
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_keychain/Globals.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecKeyPriv.h>

using namespace Security;
using namespace KeychainCore;

#pragma mark --- Aggregate Export routines ---

OSStatus impExpPkcs12Export(
	CFArrayRef							exportReps,		// SecExportReps
	SecItemImportExportFlags			flags,			// kSecItemPemArmour, etc. 	
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableDataRef					outData)		// output appended here
{
	SecPkcs12CoderRef   p12Coder;
	OSStatus			ortn = errSecSuccess;
	CFMutableArrayRef   exportItems;			// SecKeychainItemRefs
	CFDataRef			tmpData = NULL;
	CSSM_CSP_HANDLE		cspHand;
	CSSM_KEY			*passKey = NULL;
	CFStringRef			phraseStr = NULL;
	
	if( (keyParams == NULL) ||
	    ( (keyParams->passphrase == NULL) && 
		  !(keyParams->flags & kSecKeySecurePassphrase) ) ) {
		/* passphrase mandatory */
		return errSecPassphraseRequired;
	}
	CFIndex numReps = CFArrayGetCount(exportReps);
	if(numReps == 0) {
		SecImpExpDbg("impExpPkcs12Export: no items to export");
		return errSecItemNotFound;
	}
	
	/* 
	 * Build an array of SecKeychainItemRefs. 
	 *
	 * Keychain is inferred from the objects to be exported. Some may be
	 * floating certs with no keychain.
	 */
	exportItems = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	SecKeychainRef kcRef = nil;
	for(CFIndex dex=0; dex<numReps; dex++) {
		SecExportRep *exportRep = 
			(SecExportRep *)CFArrayGetValueAtIndex(exportReps, dex);
		SecKeychainItemRef kcItemRef = (SecKeychainItemRef)exportRep->kcItem();
		CFArrayAppendValue(exportItems, kcItemRef);
		if(kcRef == nil) {
			SecKeychainItemCopyKeychain(kcItemRef, &kcRef);
			/* ignore error - we do this 'til we get a kcRef */
		}
	}

	/* Set up a PKCS12 encoder */
	ortn = SecPkcs12CoderCreate(&p12Coder);
	if(ortn) {
		return ortn;
	}
	/* subsequent errors to errOut: */

	ortn = SecPkcs12SetKeychain(p12Coder, kcRef);
	if(ortn) {
		goto errOut;
	}
	
	/* We need a CSPDL handle for possible secure passphrase acquisition */
	ortn = SecKeychainGetCSPHandle(kcRef, &cspHand);
	if(ortn) {
		SecImpExpDbg("SecKeychainGetCSPHandle error");
		goto errOut;
	}
 
	/* explicit passphrase, or get one ourself? */
	ortn = impExpPassphraseCommon(keyParams, cspHand, SPF_String, 
		VP_Export, (CFTypeRef *)&phraseStr, &passKey);
	if(ortn) {
		goto errOut;
	}
	if(phraseStr != NULL) {
		ortn = SecPkcs12SetMACPassphrase(p12Coder, phraseStr); 
		CFRelease(phraseStr);
		if(ortn) {
			SecImpExpDbg("SecPkcs12SetMACPassphrase error");
			goto errOut;
		}
	}
	else {
		assert(passKey != NULL);
		ortn = SecPkcs12SetMACPassKey(p12Coder, passKey);
		if(ortn) {
			SecImpExpDbg("SecPkcs12SetMACPassphrase error");
			goto errOut;
		}
	}

	ortn = SecPkcs12ExportKeychainItems(p12Coder, exportItems);
	if(ortn) {
		SecImpExpDbg("impExpPkcs12Export: SecPkcs12ExportKeychainItems failure");
		goto errOut;
	}   
	
	/* GO */
	ortn = SecPkcs12Encode(p12Coder, &tmpData);
	if(ortn) {
		SecImpExpDbg("impExpPkcs12Export: SecPkcs12Encode failure");
		goto errOut;
	}   
	
	/* append encoded data to output */
	CFDataAppendBytes(outData, CFDataGetBytePtr(tmpData), CFDataGetLength(tmpData));
	
errOut:
	SecPkcs12CoderRelease(p12Coder);
	if(passKey != NULL) {
		CSSM_FreeKey(cspHand, NULL, passKey, CSSM_FALSE);
		free(passKey);
	}
	if(kcRef) {
		CFRelease(kcRef);
	}
	if(exportItems) {
		CFRelease(exportItems);
	}
	if(tmpData) {
		CFRelease(tmpData);
	}
	return ortn;
}

OSStatus impExpPkcs7Export(
	CFArrayRef							exportReps,		// SecExportReps
	SecItemImportExportFlags			flags,			// kSecItemPemArmour, etc. 	
	const SecKeyImportExportParameters	*keyParams,		// optional 
	CFMutableDataRef					outData)		// output appended here
{
	SecCmsSignedDataRef	sigd = NULL;
	SecCertificateRef   certRef;
	OSStatus			ortn;
	CFIndex				numCerts = CFArrayGetCount(exportReps);
	SecExportRep		*exportRep;
	SecCmsContentInfoRef cinfo = NULL;
	SecArenaPoolRef     arena = NULL;
	SecCmsEncoderRef    ecx;
	CSSM_DATA			output = { 0, NULL };
	
	if(numCerts == 0) {
		SecImpExpDbg("impExpPkcs7Export: no certs to export");
		return errSecSuccess;
	}
	
    /* create the message object */
    SecCmsMessageRef cmsg = SecCmsMessageCreate(NULL);
    if(cmsg == NULL) {
		SecImpExpDbg("impExpPkcs7Export: SecCmsMessageCreate failure");
		return errSecInternalComponent;
	}

	/* get first cert */
	exportRep = (SecExportRep *)CFArrayGetValueAtIndex(exportReps, 0);
	assert(exportRep != NULL);
	if(exportRep->externType() != kSecItemTypeCertificate) {
		SecImpExpDbg("impExpPkcs7Export: non-cert item");
		ortn = errSecParam;
		goto errOut;
	}
	certRef = (SecCertificateRef)exportRep->kcItem();

    /* build chain of objects: message->signedData->data */
    sigd = SecCmsSignedDataCreateCertsOnly(cmsg, certRef, false);
	if(sigd == NULL) {
		SecImpExpDbg("impExpPkcs7Export: SecCmsSignedDataCreateCertsOnly failure");
		ortn = errSecInternalComponent;
		goto errOut;
	}

    for (CFIndex dex=1; dex<numCerts; dex++) {
		exportRep = (SecExportRep *)CFArrayGetValueAtIndex(exportReps, dex);
		assert(exportRep != NULL);
		if(exportRep->externType() != kSecItemTypeCertificate) {
			SecImpExpDbg("impExpPkcs7Export: non-cert item");
			ortn = errSecParam;
			goto errOut;
		}
		certRef = (SecCertificateRef)exportRep->kcItem();
        ortn = SecCmsSignedDataAddCertChain(sigd, certRef);
		if(ortn) {
			SecImpExpDbg("impExpPkcs7Export: SecCmsSignedDataAddCertChain error");
			goto errOut;
		}
    }
	
    cinfo = SecCmsMessageGetContentInfo(cmsg);
	if(cinfo == NULL) {
		SecImpExpDbg("impExpPkcs7Export: SecCmsMessageGetContentInfo returned NULL");
		ortn = errSecInternalComponent;
		goto errOut;
	}
    ortn = SecCmsContentInfoSetContentSignedData(cmsg, cinfo, sigd);
	if(ortn) {
		SecImpExpDbg("impExpPkcs7Export: SecCmsContentInfoSetContentSignedData error");
		goto errOut;
    }
    cinfo = SecCmsSignedDataGetContentInfo(sigd);
	if(cinfo == NULL) {
		SecImpExpDbg("impExpPkcs7Export: SecCmsSignedDataGetContentInfo returned NULL");
		ortn = errSecInternalComponent;
		goto errOut;
	}
    ortn = SecCmsContentInfoSetContentData(cmsg, cinfo, NULL, 
				false /* FIXME - what's this? */);
	if(ortn) {
		SecImpExpDbg("impExpPkcs7Export: SecCmsContentInfoSetContentData error");
		goto errOut;
    }

	/* Now encode it */
    ortn = SecArenaPoolCreate(1024, &arena);
	if(ortn) {
		SecImpExpDbg("impExpPkcs7Export: SecArenaPoolCreate error");
		goto errOut;
	}
	ortn = SecCmsEncoderCreate(cmsg, 
		   NULL, NULL,		/* DER output callback  */
		   &output, arena,  /* destination storage  */
		   NULL, NULL,		/* password callback    */
		   NULL, NULL,		/* decrypt key callback */
		   NULL, NULL,
           &ecx );	/* detached digests    */
	if(ortn) {
		SecImpExpDbg("impExpPkcs7Export: SecCmsEncoderCreate error");
		goto errOut;
	}
	ortn = SecCmsEncoderFinish(ecx);
	if(ortn) {
		SecImpExpDbg("impExpPkcs7Export: SecCmsEncoderFinish returned NULL");
		goto errOut;
	}

	/* append encoded data to output */
	CFDataAppendBytes(outData, output.Data, output.Length);


errOut:
    if(cmsg != NULL) {
        SecCmsMessageDestroy(cmsg);
	}
	if(arena != NULL) {
		SecArenaPoolFree(arena, false);
	}
	return ortn;
}

#pragma mark --- Aggregate Import routines ---

/*
 * For all of these, if a cspHand is specified instead of a keychain,
 * the cspHand MUST be a CSPDL handle, not a raw CSP handle. 
 */
OSStatus impExpPkcs12Import(
	CFDataRef							inData,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,			// optional 
	ImpPrivKeyImportState				&keyImportState,	// IN/OUT
	
	/* caller must supply one of these */
	SecKeychainRef						importKeychain, // optional
	CSSM_CSP_HANDLE						cspHand,		// required
	CFMutableArrayRef					outArray)		// optional, append here 
{
	SecPkcs12CoderRef   p12Coder = NULL;
	OSStatus			ortn;
	CFIndex				numCerts;
	CFIndex				numKeys;
	CFIndex				dex;
	CFMutableArrayRef	privKeys = NULL;
	CSSM_KEY			*passKey = NULL;
	CFStringRef			phraseStr = NULL;

	/* 
	 * Optional private key attrs.
	 * Although the PKCS12 library has its own defaults for these, we'll
	 * set them explicitly to the defaults specified in our API if the 
	 * caller doesn't specify any. 
	 */
	CSSM_KEYUSE keyUsage = CSSM_KEYUSE_ANY;
	CSSM_KEYATTR_FLAGS keyAttrs = CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_EXTRACTABLE |
		CSSM_KEYATTR_RETURN_REF;
	
	if( (keyParams == NULL) ||
	    ( (keyParams->passphrase == NULL) && 
		  !(keyParams->flags & kSecKeySecurePassphrase) ) ) {
		/* passphrase mandatory */
		return errSecPassphraseRequired;
	}
	
	/*
	 * Set up a P12 decoder.
	 */
	ortn = SecPkcs12CoderCreate(&p12Coder);
	if(ortn) {
		SecImpExpDbg("SecPkcs12CoderCreate error");
		return ortn;
	}
	/* subsequent errors to errOut: */

	CSSM_CL_HANDLE clHand = cuClStartup();
	CSSM_CSP_HANDLE rawCspHand = cuCspStartup(CSSM_TRUE);   // for CL
	if((clHand == 0) || (rawCspHand == 0)) {
		return CSSMERR_CSSM_ADDIN_LOAD_FAILED;
	}
	
	assert(cspHand != CSSM_INVALID_HANDLE);
	if(importKeychain != NULL) {
		ortn = SecPkcs12SetKeychain(p12Coder, importKeychain);
		if(ortn) {
			SecImpExpDbg("SecPkcs12SetKeychain error");
			goto errOut;
		}
	}
	else {
		if(cspHand == CSSM_INVALID_HANDLE) {
			ortn = errSecParam;
			goto errOut;
		}
		ortn = SecPkcs12SetCspHandle(p12Coder, cspHand);
		if(ortn) {
			SecImpExpDbg("SecPkcs12SetCspHandle error");
			goto errOut;
		}
	}
	
	/* explicit passphrase, or get one ourself? */
	ortn = impExpPassphraseCommon(keyParams, cspHand, SPF_String, 
		VP_Import, (CFTypeRef *)&phraseStr, &passKey);
	if(ortn) {
		goto errOut;
	}
	if(phraseStr != NULL) {
		ortn = SecPkcs12SetMACPassphrase(p12Coder, phraseStr); 
		CFRelease(phraseStr);
		if(ortn) {
			SecImpExpDbg("SecPkcs12SetMACPassphrase error");
			goto errOut;
		}
	}
	else {
		assert(passKey != NULL);
		ortn = SecPkcs12SetMACPassKey(p12Coder, passKey);
		if(ortn) {
			SecImpExpDbg("SecPkcs12SetMACPassphrase error");
			goto errOut;
		}
	}
	
	if(keyImportState != PIS_NoLimit) {
		bool foundOneKey = false;
		
		/* allow either zero or one more private key */
		if(keyImportState == PIS_NoMore) {
			foundOneKey = true;
		}
		ortn = SecPkcs12LimitPrivateKeyImport(p12Coder, foundOneKey);
		if(ortn) {
			SecImpExpDbg("SecPkcs12LimitPrivateKeyImport error");
			goto errOut;
		}
	}
	
	if(keyParams != NULL) {
		if(keyParams->keyUsage != 0) {
			keyUsage = keyParams->keyUsage;
		}
		if(keyParams->keyAttributes != 0) {
			keyAttrs = keyParams->keyAttributes;
		}
		if(keyParams->flags & kSecKeyNoAccessControl) {
			ortn = SecPkcs12SetAccess(p12Coder, NULL);
			if(ortn) {
				SecImpExpDbg("SecPkcs12SetAccess error");
				goto errOut;
			}
		}
		else if(keyParams->accessRef != NULL) {
			ortn = SecPkcs12SetAccess(p12Coder, keyParams->accessRef);
			if(ortn) {
				SecImpExpDbg("SecPkcs12SetAccess error");
				goto errOut;
			}
		}
		/* else default ACL */
	}
	ortn = SecPkcs12SetKeyUsage(p12Coder, keyUsage);
	if(ortn) {
		SecImpExpDbg("SecPkcs12SetKeyUsage error");
		goto errOut;
	}
	ortn = SecPkcs12SetKeyAttrs(p12Coder, keyAttrs);
	if(ortn) {
		SecImpExpDbg("SecPkcs12SetKeyAttrs error");
		goto errOut;
	}
	
	/* GO */
	ortn = SecPkcs12Decode(p12Coder, inData);
	if(ortn) {
		SecImpExpDbg("SecPkcs12Decode error");
		goto errOut;
	}
	
	/*
	 * About to process SecKeychainItemRefs. 
	 * This whole mess is irrelevant if the caller doesn't 
	 * want an array of SecKeychainItemRefs.
	 */
	if(outArray == NULL) {
		goto errOut;
	}
	
	ortn = SecPkcs12CertificateCount(p12Coder, &numCerts);
	if(ortn) {
		SecImpExpDbg("SecPkcs12CertificateCount error");
		goto errOut;
	}
	ortn = SecPkcs12PrivateKeyCount(p12Coder, &numKeys);
	if(ortn) {
		SecImpExpDbg("SecPkcs12PrivateKeyCount error");
		goto errOut;
	}
	
	/* 
	 * Match up certs and keys to create SecIdentityRefs.
	 * First create a temporary array of the private keys
	 * found by the P12 module.
	 *
	 * FIXME we're working with a P12 module which can not
	 * vend SecKeyRefs.....this will hopefully, eventually,
	 * change.
	 */
	privKeys = CFArrayCreateMutable(NULL, numKeys, NULL);
	for(dex=0; dex<numKeys; dex++) {
		CSSM_KEY_PTR privKey;
		ortn = SecPkcs12GetCssmPrivateKey(p12Coder,
			dex, &privKey, NULL, NULL, NULL);
		CFArrayAppendValue(privKeys, privKey);
	}
	
	/*
	 * Now go through all certs, searching for a matching private
	 * key. When we find a match we try to create an identity from the 
	 * cert, which might fail for a number of reasons, currently including 
	 * the fact that there is no way to create an identity with a key
	 * which does not reside on a keychain. (Such is the case here when
	 * caller has not specified a keychain.) If that works we skip the
	 * cert, delete that key from the privKeys array, and append the 
	 * indentity to outArray. If no identity is found we append the 
	 * cert to outArray. At the end of this loop, remaining 
	 * items in privKeys (of which there will typically be none) are
	 * also appended to outArray. 
	 */
	for(dex=0; dex<numCerts; dex++) {
		SecCertificateRef   certRef = NULL;				// created by P12
        SecCertificateRef   importedCertRef = NULL;		// owned by Sec layer
		CSSM_KEY_PTR		pubKey = NULL;				// mallocd by CL
		CSSM_KEY_PTR		privKey = NULL;				// owned by P12
		CSSM_DATA			certData;					// owned by Sec layer
		CSSM_DATA			pubKeyDigest = {0, NULL};   // mallocd by CSP
		CSSM_DATA			privKeyDigest = {0, NULL};  // mallocd by CSP
		bool				foundIdentity = false;
		
		ortn = SecPkcs12CopyCertificate(p12Coder, dex, &certRef,
			NULL, NULL, NULL);
		if(ortn) {
			/* should never happen */
			SecImpExpDbg("SecPkcs12CopyCertificate error");
			goto errOut;
		}
		/* subsequent errors in this loop to loopEnd: */
		
		if(importKeychain == NULL) {
			/* Skip the Identity match - just return keys and certs */
			goto loopEnd;
		}

        /* the SecPkcs12CopyCertificate function returns a floating
         * certificate without a keychain. We must update it now that
         * it has been added to importKeychain.
         */
        {
        StorageManager::KeychainList keychains;
        globals().storageManager.optionalSearchList(importKeychain, keychains);
        SecPointer<Certificate> cert = Certificate::required(certRef);
        importedCertRef = cert->findInKeychain(keychains)->handle();
        }

		/* Get digest of this cert's public key */
		ortn = SecCertificateGetData(importedCertRef, &certData);
		if(ortn) {
			SecImpExpDbg("SecCertificateGetData error");
			goto loopEnd;
		}
		ortn = CSSM_CL_CertGetKeyInfo(clHand, &certData, &pubKey);
		if(ortn) {
			SecImpExpDbg("SecCertificateGetData error");
			goto loopEnd;
		}
		ortn = impExpKeyDigest(rawCspHand, pubKey, &pubKeyDigest);
		if(ortn) {
			goto loopEnd;
		}
		
		/* 
		 * Now search for a private key with this same digest
		 */
		numKeys = CFArrayGetCount(privKeys);
		for(CFIndex privDex=0; privDex<numKeys; privDex++) {
			privKey = (CSSM_KEY_PTR)CFArrayGetValueAtIndex(privKeys, privDex);
			assert(privKey != NULL);
			ortn = impExpKeyDigest(cspHand, privKey, &privKeyDigest);
			if(ortn) {
				goto loopEnd;
			}
			CSSM_BOOL digestMatch = cuCompareCssmData(&pubKeyDigest, &privKeyDigest);
			impExpFreeCssmMemory(cspHand, privKeyDigest.Data);
			if(digestMatch) {
				/* 
				 * MATCH: try to cook up Identity.
				 * TBD: I expect some work will be needed here when 
				 * Sec layer can handle floating keys. One thing 
				 * that would be nice would be if we could create an identity
				 * FROM a given SecCertRef and a SecKeyRef, even if 
				 * the SecKeyRef is floating. 
				 * 
				 * NOTE: you might think that we could do a 
				 * SecIdentityCreateWithCertificate() before, or even without, 
				 * doing a digest match....but that could "work" without
				 * us having imported any keys at all, if the appropriate
				 * private key were already there. Doing the digest match 
				 * guarantees the uniqueness of the key item in the DB.
				 */
				SecIdentityRef idRef = NULL;
				ortn = SecIdentityCreateWithCertificate(importKeychain,
					importedCertRef, &idRef);
				if(ortn == errSecSuccess) {
					/*
					 * Got one!
					 * 
					 * -- add Identity to outArray
					 * -- remove privKey from privKeys array 
					 * -- skip to next cert
					 */
					SecImpExpDbg("P12Import: generating a SecIdentityRef");
					assert(outArray != NULL);
					CFArrayAppendValue(outArray, idRef);
					CFRelease(idRef);		// array holds only ref
					idRef = NULL;
					CFArrayRemoveValueAtIndex(privKeys, privDex);
					foundIdentity = true;
					goto loopEnd;
				}   /* ID create worked, else try next key */
			}		/* digest match */
		}			/* searching thru privKeys */
	loopEnd:
		/* free resources allocated in this loop */
		assert(certRef != NULL);
		if(!foundIdentity ) {
			/* No private key for this cert: give to caller */
			assert(outArray != NULL);
			CFArrayAppendValue(outArray, certRef);
		}
		CFRelease(certRef);				// outArray holds only ref
		certRef = NULL;
        if (importedCertRef) {
            CFRelease(importedCertRef);
            importedCertRef = NULL;
        }
		if(pubKey != NULL) {
			/* technically invalid, the CL used some CSP handle we 
			 * don't have access to to get this... */
			CSSM_FreeKey(rawCspHand, NULL, pubKey, CSSM_FALSE);
			impExpFreeCssmMemory(clHand, pubKey);
			pubKey = NULL;
		}
		if(pubKeyDigest.Data != NULL) {
			impExpFreeCssmMemory(rawCspHand, pubKeyDigest.Data);
			pubKeyDigest.Data = NULL;
		}
		if(ortn) {
			goto errOut;
		}
	}
	
errOut:
	/*
	 * One last thing: pass any remaining (non-identity) keys to caller.
	 * For now, the keys are CSSM_KEYs owned by the P12 coder object, we 
	 * don't have to release them. When P12 can vend SecKeyRefs, we release the 
	 * keys here.
	 */
	 
	 /*
		The code below has no net effect, except for generating a leak. This was
		found while investigating
			<rdar://problem/8799913> SecItemImport() leaking
		Code like this will need to be added when we return SecIdentityRefs in
		the "in memory" case (destKeychain = NULL). Currently, when importing to
		a physical keychain, the returned item array contains SecIdentityRefs,
		whereas the "in memory" case returns SecCertificateRefs. See
			<rdar://problem/8862809> ER: SecItemImport should return SecIdentityRefs in the "in memory" case
	
	*/
#if 0
	if(privKeys) {
		if(ortn == errSecSuccess) {		// TBD OR keys are SecKeyRefs
			numKeys = CFArrayGetCount(privKeys);
			for(dex=0; dex<numKeys; dex++) {
				SecKeyRef keyRef;
				CSSM_KEY_PTR privKey = 
					(CSSM_KEY_PTR)CFArrayGetValueAtIndex(privKeys, dex);
				assert(privKey != NULL);
				if(ortn == errSecSuccess) {
					/* only do this on complete success so far */
					ortn = SecKeyCreateWithCSSMKey(privKey, &keyRef);
					if(ortn) {
						SecImpExpDbg("SecKeyCreateWithCSSMKey error");
					}
					/* keep going for CFRelease */
					if (keyRef)
						CFRelease(keyRef);
				}
				/* TBD CFRelease the SecKeyRef */
			}   /* for each privKey */
		}		/* success so far */
	}
#endif

	SecPkcs12CoderRelease(p12Coder);
	if(passKey != NULL) {
		CSSM_FreeKey(cspHand, NULL, passKey, CSSM_FALSE);
		free(passKey);
	}
	if(privKeys != NULL) {
		CFRelease(privKeys);
	}
	if(clHand != 0) {
		cuClDetachUnload(clHand);
	}
	if(rawCspHand != 0) {
		cuCspDetachUnload(rawCspHand, CSSM_TRUE);
	}
	return ortn;
}

OSStatus impExpPkcs7Import(
	CFDataRef							inData,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	SecKeychainRef						importKeychain, // optional
	CFMutableArrayRef					outArray)		// optional, append here 
{
    SecCmsDecoderRef        decoderContext;
    SecCmsMessageRef        cmsMessage = NULL;
    SecCmsContentInfoRef    contentInfo;
    SecCmsSignedDataRef		signedData;
    int						contentLevelCount;
    int						i;
    SECOidTag				contentTypeTag;
    OSStatus				result;
	OSStatus				ourRtn = errSecSuccess;

    /* decode the message */
    result = SecCmsDecoderCreate (NULL, NULL, NULL, NULL, NULL, NULL, NULL, &decoderContext);
    if (result != 0) {
        ourRtn = result;
        goto errOut;
    }
    result = SecCmsDecoderUpdate(decoderContext, CFDataGetBytePtr(inData),
		CFDataGetLength(inData));
	if (result != 0) {
		/* any useful status here? */
		SecImpExpDbg("SecCmsDecoderUpdate error");
		ourRtn = errSecUnknownFormat;
        SecCmsDecoderDestroy(decoderContext);
		goto errOut;
	}
	
    ourRtn = SecCmsDecoderFinish(decoderContext, &cmsMessage);
    if (ourRtn) {
		SecImpExpDbg("SecCmsDecoderFinish error");
		ourRtn = errSecUnknownFormat;
		goto errOut;
	}

    // process the results
    contentLevelCount = SecCmsMessageContentLevelCount (cmsMessage);
    
    for (i = 0; i < contentLevelCount; ++i)
    {
        // get content information
        contentInfo = SecCmsMessageContentLevel (cmsMessage, i);
        contentTypeTag = SecCmsContentInfoGetContentTypeTag (contentInfo);
        
        switch (contentTypeTag)
        {
            case SEC_OID_PKCS7_SIGNED_DATA:
            {
				/* I guess this the only interesting field */
                signedData = 
					(SecCmsSignedDataRef) SecCmsContentInfoGetContent (contentInfo);
                if (signedData == NULL) {
					SecImpExpDbg("SecCmsContentInfoGetContent returned NULL");
					ourRtn = errSecUnknownFormat;
					goto errOut;
				}
                
                // import the certificates
                CSSM_DATA **outCerts = SecCmsSignedDataGetCertificateList(signedData);
				if(outCerts == NULL) {
					SecImpExpDbg("SecCmsSignedDataGetCertificateList returned NULL");
					ourRtn = errSecUnknownFormat;
					goto errOut;
				}
				
				/* Returned value is NULL-terminated array */
				unsigned count = 0;
				CSSM_DATA **array = outCerts;
				if (array) {
					while (*array++) {
						count++;
					}
				}
				if(count == 0) {
					SecImpExpDbg("No certs found in apparently good PKCS7 blob");
					goto errOut;
				}

				for(unsigned dex=0; dex<count; dex++) {
					ourRtn = impExpImportCertCommon(outCerts[dex], importKeychain,
						outArray);
					if(ourRtn) {
						goto errOut;
					}
				}
                break;
            }            
            default:
                break;
        }
    }
errOut:
	if(cmsMessage) {
		SecCmsMessageDestroy(cmsMessage);
	}
    return ourRtn;

}

/* 
 * Import a netscape-cert-sequence. Suitable for low-cost guessing when neither
 * importKeychain nor outArray is specified.
 */
OSStatus impExpNetscapeCertImport(
	CFDataRef							inData,
	SecItemImportExportFlags			flags,		
	const SecKeyImportExportParameters	*keyParams,		// optional 
	SecKeychainRef						importKeychain, // optional
	CFMutableArrayRef					outArray)		// optional, append here 
{
	SecNssCoder coder;
	NetscapeCertSequence certSeq;

	/* DER-decode */
	memset(&certSeq, 0, sizeof(certSeq));
	PRErrorCode perr = coder.decode(CFDataGetBytePtr(inData),
		CFDataGetLength(inData),
		NetscapeCertSequenceTemplate, 
		&certSeq);
	if(perr) {
		SecImpExpDbg("impExpNetscapeCertImport: DER decode failure");
		return errSecUnknownFormat;
	}

	/* verify (contentType == netscape-cert-sequence) */
	if(!cuCompareOid(&CSSMOID_NetscapeCertSequence, &certSeq.contentType)) {
		SecImpExpDbg("impExpNetscapeCertImport: OID mismatch");
		return errSecUnknownFormat;
	}
	
	/* Extract certs in CSSM_DATA form, return to caller */
	unsigned numCerts = nssArraySize((const void **)certSeq.certs);
	for(unsigned i=0; i<numCerts; i++) {
		CSSM_DATA *cert = certSeq.certs[i];
		OSStatus ortn = impExpImportCertCommon(cert, importKeychain, outArray);
		if(ortn) {
			return ortn;
		}
	} 
	return errSecSuccess;
}

#pragma mark --- Utilities ---

OSStatus impExpImportCertCommon(
	const CSSM_DATA		*cdata,
	SecKeychainRef		importKeychain, // optional
	CFMutableArrayRef	outArray)		// optional, append here 
{
	OSStatus ortn = errSecSuccess;
	SecCertificateRef certRef;
	
	if (!cdata)
		return errSecUnsupportedFormat;

	CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)cdata->Data, (CFIndex)cdata->Length, kCFAllocatorNull);
	/* Pass kCFAllocatorNull as bytesDeallocator to assure the bytes aren't freed */
	if (!data)
		return errSecUnsupportedFormat;

	certRef = SecCertificateCreateWithData(kCFAllocatorDefault, data);
	CFRelease(data); /* certRef has its own copy of the data now */
	if(!certRef) {
		SecImpExpDbg("impExpHandleCert error\n");
		return errSecUnsupportedFormat;
	}

	if(importKeychain != NULL) {
		ortn = SecCertificateAddToKeychain(certRef, importKeychain);
		if(ortn) {
			SecImpExpDbg("SecCertificateAddToKeychain error\n");
			CFRelease(certRef);
			return ortn;
		}
	}
	if(outArray != NULL) {
		CFArrayAppendValue(outArray, certRef);
	}
	CFRelease(certRef);
	return ortn;
}

