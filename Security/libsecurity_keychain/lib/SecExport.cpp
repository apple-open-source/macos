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
 * SecExport.cpp - high-level facility for exporting Sec layer objects. 
 */

#include "SecImportExport.h"
#include "SecImportExportAgg.h"
#include "SecImportExportPem.h"
#include "SecExternalRep.h"
#include "SecImportExportUtils.h"
#include <security_utilities/errors.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecItem.h>
#include <Security/SecBase.h>
using namespace Security;
using namespace KeychainCore;

/*
 * Convert Sec item to one or two SecExportReps, append to exportReps array.
 * The "one or two" clause exists for SecIdentityRefs, which we split into 
 * a cert and a key.
 * Throws a MacOSError if incoming CFTypeRef is of type other than SecKeyRef,
 * SecCertRef, or SecIdentityRef.
 */
static void impExpAddToExportReps(
	CFTypeRef			thing,				// Key, Cert, Identity
	CFMutableArrayRef   exportReps,
	unsigned			&numCerts,			// IN/OUT - accumulated
	unsigned			&numKeys)			// IN/OUT - accumulated
{
	if(CFGetTypeID(thing) == SecIdentityGetTypeID()) {
		/* special case for SecIdentities, creates two SecExportReps */
		OSStatus ortn;
		SecIdentityRef idRef = (SecIdentityRef)thing;
		SecCertificateRef certRef;
		SecKeyRef keyRef;
		SecExportRep *rep;
		
		/* cert */
		SecImpExpDbg("impExpAddToExportReps: adding identity cert and key");
		ortn = SecIdentityCopyCertificate(idRef, &certRef);
		if(ortn) {
			Security::MacOSError::throwMe(ortn);
		}
		rep = SecExportRep::vend(certRef);
		CFArrayAppendValue(exportReps, rep);
		CFRelease(certRef);			// SecExportRep holds a reference
		numCerts++;
		
		/* private key */
		ortn = SecIdentityCopyPrivateKey(idRef, &keyRef);
		if(ortn) {
			Security::MacOSError::throwMe(ortn);
		}
		rep = SecExportRep::vend(keyRef);
		CFArrayAppendValue(exportReps, rep);
		CFRelease(keyRef);			// SecExportRep holds a reference
		numKeys++;
	}
	else {
		/* this throws if 'thing' is an unacceptable type */
		SecExportRep *rep = SecExportRep::vend(thing);
		SecImpExpDbg("impExpAddToExportReps: adding single type %d",
			(int)rep->externType());
		CFArrayAppendValue(exportReps, rep);
		if(rep->externType() == kSecItemTypeCertificate) {
			numCerts++;
		}
		else {
			numKeys++;
		}
	}
}

#pragma mark --- public export function ---

OSStatus SecKeychainItemExport(
	CFTypeRef							keychainItemOrArray,
	SecExternalFormat					outputFormat,	// a SecExternalFormat 
	SecItemImportExportFlags			flags,			// kSecItemPemArmour, etc. 	
	const SecKeyImportExportParameters  *keyParams,		// optional 
	CFDataRef							*exportedData)	// external representation 
														//    returned here
{
	BEGIN_IMP_EXP_SECAPI
	
	/* some basic input validation */
	if(keychainItemOrArray == NULL) {
		return errSecParam;
	}
	if(keyParams != NULL) {
		/* can't specify explicit passphrase and ask for secure one */
		if( (keyParams->passphrase != NULL) &&
		    ((keyParams->flags & kSecKeySecurePassphrase) != 0)) {
			return errSecParam;
		}
	}
	
	unsigned numKeys			= 0;
	unsigned numCerts			= 0;
	unsigned numTotalExports	= 0;
	OSStatus ortn				= errSecSuccess;
	SecExportRep *rep			= NULL;				// common temp variable
	CFMutableDataRef outputData = NULL;
	const char *pemHeader		= "UNKNOWN";
	
	/* convert keychainItemOrArray to CFArray of SecExportReps */
	CFMutableArrayRef exportReps = CFArrayCreateMutable(NULL, 0, NULL);
	/* subsequent errors to errOut: */
	
	try {
		if(CFGetTypeID(keychainItemOrArray) == CFArrayGetTypeID()) {
			CFArrayRef arr = (CFArrayRef)keychainItemOrArray;
			CFIndex arraySize = CFArrayGetCount(arr);
			for(CFIndex dex=0; dex<arraySize; dex++) {
				impExpAddToExportReps(CFArrayGetValueAtIndex(arr, dex), 
					exportReps, numCerts, numKeys);
			}
		}
		else {
			impExpAddToExportReps(keychainItemOrArray, exportReps, numCerts, numKeys);
		}
	}
	catch(const Security::MacOSError osErr) {
		ortn = osErr.error;
		goto errOut;
	}
	catch(...) {
		ortn = errSecParam;
		goto errOut;
	}
	numTotalExports = (unsigned int)CFArrayGetCount(exportReps);
	assert((numCerts + numKeys) == numTotalExports);
	if((numTotalExports > 1) && (outputFormat == kSecFormatUnknown)) {
		/* default aggregate format is PEM sequence */
		outputFormat = kSecFormatPEMSequence;
	}
	
	/*
	 * Break out to SecExternalFormat-specific code, appending all data to outputData 
	 */
	outputData = CFDataCreateMutable(NULL, 0);
	switch(outputFormat) {
		case kSecFormatPKCS7:
			ortn = impExpPkcs7Export(exportReps, flags, keyParams, outputData);
			pemHeader = PEM_STRING_PKCS7;
			break;
		case kSecFormatPKCS12:
			ortn = impExpPkcs12Export(exportReps, flags, keyParams, outputData);
			pemHeader = PEM_STRING_PKCS12;
			break;
		case kSecFormatPEMSequence:
			{
				/* 
				 * A bit of a special case. Create an intermediate DER encoding 
				 * of each SecExportRef, in the default format for that item;
				 * PEM encode the result, and append the PEM encoding to 
				 * outputData.
				 */
				CFIndex numReps = CFArrayGetCount(exportReps);
				for(CFIndex dex=0; dex<numReps; dex++) {
				
					rep = (SecExportRep *)CFArrayGetValueAtIndex(exportReps, dex);
					
					/* default DER encoding */
					CFMutableDataRef tmpData = CFDataCreateMutable(NULL, 0);
					ortn = rep->exportRep(kSecFormatUnknown, flags, keyParams,
						tmpData, &pemHeader);
					if(ortn) {
						SecImpExpDbg("ItemExport: releasing tmpData %p", tmpData);
						CFRelease(tmpData);
						goto errOut;
					}
					
					/* PEM to accumulating output */
					assert(rep->pemParamLines() == NULL);
					ortn = impExpPemEncodeExportRep((CFDataRef)tmpData, 
							pemHeader, NULL,		/* no pemParamLines, right? */
							outputData);
					CFRelease(tmpData);
					if(ortn) {
						goto errOut;
					}
				}
				break;
			}
		
		/* Enumerate remainder explicitly for clarity; all are single-item forms */
		case kSecFormatOpenSSL:
		case kSecFormatSSH:
		case kSecFormatSSHv2:
		case kSecFormatBSAFE:
		case kSecFormatRawKey:
		case kSecFormatWrappedPKCS8:
		case kSecFormatWrappedOpenSSL:
		case kSecFormatWrappedSSH:
		case kSecFormatWrappedLSH:
		case kSecFormatX509Cert:
		case kSecFormatUnknown:		// i.e., default, handled by SecExportRep
			{
				unsigned foundCount = 0;
				
				/* verify that we have exactly one of specified item */
				if(outputFormat == kSecFormatX509Cert) {
					foundCount = numCerts;
				}
				else if(outputFormat == kSecFormatUnknown) {
					/* can't go wrong */
					foundCount = numTotalExports;
				}
				else {
					foundCount = numKeys;
				}
				if((numTotalExports != 1) || (foundCount != 1)) {
					SecImpExpDbg("Export single item format with other than one item");
					ortn = errSecParam;
					goto errOut;
				}
				assert(CFArrayGetCount(exportReps) == 1);
				rep = (SecExportRep *)CFArrayGetValueAtIndex(exportReps, 0);
				ortn = rep->exportRep(outputFormat, flags, 
						keyParams, outputData, &pemHeader);
				break;
			}
		default:
			SecImpExpDbg("SecKeychainItemExport: bad format (%u)", 
				(unsigned)outputFormat);
			ortn = errSecParam;
			goto errOut;
	}
	
	/* 
	 * Final step: possible PEM encode. Skip for kSecFormatPEMSequence (in which
	 * case outputData is all ready to ship out to the caller); mandatory
	 * if exportRep has a non-NULL pemParamLines (which can only happen if we're
	 * exporting a single item). 
	 */
	if(ortn == errSecSuccess) {
		if(outputFormat == kSecFormatPEMSequence) {
			*exportedData = outputData;
			outputData = NULL;		
		}
		else {
			rep = (SecExportRep *)CFArrayGetValueAtIndex(exportReps, 0);
			if((flags & kSecItemPemArmour) || (rep->pemParamLines() != NULL)) {
				/* PEM encode a single item */
				CFMutableDataRef tmpData = CFDataCreateMutable(NULL, 0);
				ortn = impExpPemEncodeExportRep((CFDataRef)outputData, pemHeader, 
					rep->pemParamLines(), tmpData);
				CFRelease(outputData);		// done with this
				outputData = NULL;			
				*exportedData = tmpData;	// caller gets PEM
			}
			else {
				*exportedData = outputData;
				outputData = NULL;		
			}
		}
	}
errOut:
	if(exportReps != NULL) {
		/* CFArray of our own classes, no auto release */
		CFIndex num = CFArrayGetCount(exportReps);
		for(CFIndex dex=0; dex<num; dex++) {
			rep = (SecExportRep *)CFArrayGetValueAtIndex(exportReps, dex);
			delete rep;
		}
		CFRelease(exportReps);
	}
	if(outputData != NULL) {
		CFRelease(outputData);
		outputData = NULL;
	}
	if(ortn) {
		return SecKeychainErrFromOSStatus(ortn);
	}
	else {
		return errSecSuccess;
	}
	
	END_IMP_EXP_SECAPI
}


OSStatus SecItemExport(CFTypeRef secItemOrArray, SecExternalFormat outputFormat,	
	SecItemImportExportFlags			flags,				/* kSecItemPemArmor, etc. */
	const SecItemImportExportKeyParameters  *keyParams,			/* optional */
	CFDataRef							*exportedData)	
{
	SecKeyImportExportParameters* oldStructPtr = NULL;
	SecKeyImportExportParameters oldStruct;
	memset(&oldStruct, 0, sizeof(oldStruct));
	
	if (NULL != keyParams)
	{
		
		SecKeyRef tempKey = NULL;
		
		if (SecKeyGetTypeID() == CFGetTypeID(secItemOrArray))
		{
			tempKey = (SecKeyRef)secItemOrArray;
		}
		
		if (ConvertSecKeyImportExportParametersToSecImportExportKeyParameters(tempKey,
			keyParams, &oldStruct))
		{
			oldStructPtr = &oldStruct;
		}
	}
	
	return SecKeychainItemExport(secItemOrArray, outputFormat, flags, oldStructPtr, exportedData);
}







