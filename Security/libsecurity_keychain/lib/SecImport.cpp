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
 * SecImport.cpp - high-level facility for importing Sec layer objects. 
 */

#include "SecImportExport.h"
#include "SecExternalRep.h"
#include "SecImportExportPem.h"
#include "SecImportExportUtils.h"
#include <security_cdsa_utils/cuCdsaUtils.h>
#include <security_utilities/globalizer.h>

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#define SecImpInferDbg(args...)	secdebug("SecImpInfer", ## args)

using namespace Security;
using namespace KeychainCore;

/*
 * Do our best to ensure that a SecImportRep's type and format are known.
 * A return of true means that both format and type (and, if the item
 * is a raw public or private key, the algorithm) are known. 
 */
static bool impExpInferTypeAndFormat(
	SecImportRep		*rep,
	CFStringRef			fileStr,	
	SecExternalFormat   inputFormat,
	SecExternalItemType	itemType)
{
	/* fill in blanks if caller knows them */
	if((rep->mExternType == kSecItemTypeUnknown) && (itemType != kSecItemTypeUnknown)) {
		rep->mExternType = itemType;
	}
	if((rep->mExternFormat == kSecFormatUnknown) && (inputFormat != kSecFormatUnknown)) {
		rep->mExternFormat = inputFormat;
	}

	/* some types can be inferred from format */
	if(rep->mExternType == kSecItemTypeUnknown) {
		SecExternalFormat format;
		if(rep->mExternFormat == kSecFormatUnknown) {
			/* caller specified */
			format = inputFormat;
		}
		else {
			/* maybe this is already set */
			format = rep->mExternFormat;
		}
		switch(format) {
			case kSecFormatUnknown:
				break;
			case kSecFormatPKCS7:
			case kSecFormatPKCS12:		
			case kSecFormatPEMSequence:
			case kSecFormatNetscapeCertSequence:
				rep->mExternType = kSecItemTypeAggregate;
				break;
			case kSecFormatRawKey:
				rep->mExternType = kSecItemTypeSessionKey;
				break;
			case kSecFormatX509Cert:	
				rep->mExternType = kSecItemTypeCertificate;
				break;
			case kSecFormatWrappedPKCS8:
			case kSecFormatWrappedOpenSSL:
			case kSecFormatWrappedSSH:
				rep->mExternType = kSecItemTypePrivateKey;
				break;
			case kSecFormatSSHv2:
				rep->mExternType = kSecItemTypePublicKey;
				break;
			case kSecFormatOpenSSL:
			case kSecFormatBSAFE:
			case kSecFormatWrappedLSH:
			default:
				/* can be private or session (right? */
				break;
		}
	}
	   
	/* some formats can be inferred from type */
	if(rep->mExternFormat == kSecFormatUnknown) {
		SecExternalItemType thisType;
		if(rep->mExternType == kSecItemTypeUnknown) {
			/* caller specified */
			thisType = itemType;
		}
		else {
			/* maybe this is already set */
			thisType = rep->mExternType;
		}
		switch(thisType) {
			case kSecItemTypeCertificate:
				rep->mExternFormat = kSecFormatX509Cert;
				break;
			/* any others? */
			default:
				break;
		}
	}
	
	/* 
	 * Wrapped private keys don't need algorithm 
	 * Some formats implies algorithm
	 */
	bool isWrapped = false;
	switch(rep->mExternFormat) {
		case kSecFormatWrappedPKCS8:
		case kSecFormatWrappedOpenSSL:
		case kSecFormatWrappedLSH:
			isWrapped = true;
			break;
		case kSecFormatWrappedSSH:
			isWrapped = true;
			rep->mKeyAlg = CSSM_ALGID_RSA;
			break;
		case kSecFormatSSH:
			rep->mKeyAlg = CSSM_ALGID_RSA;
			break;
		default:
			break;
	}
	
	/* Are we there yet? */
	bool done = true;
	if((rep->mExternType   == kSecItemTypeUnknown) ||
	   (rep->mExternFormat == kSecFormatUnknown)) {
		done = false;
	}
	if(done) {
		switch(rep->mExternType) {
			case kSecItemTypePrivateKey:
			case kSecItemTypePublicKey:
				if(!isWrapped && (rep->mKeyAlg == CSSM_ALGID_NONE)) {
					/* gotta know this too */
					done = false;
				}
				break;
			default:
				break;
		}
	}
	if(!done) {
		/* infer from filename if possible */
		done = impExpImportParseFileExten(fileStr, &rep->mExternFormat, 
			&rep->mExternType);
	}
	if(done) {
	   return true;
	}

	/* invoke black magic: try decoding various forms */
	return impExpImportGuessByExamination(rep->mExternal, &rep->mExternFormat,
		&rep->mExternType, &rep->mKeyAlg);	
}
	
class CSPDLMaker
{
protected:
	CSSM_CSP_HANDLE mHandle;

public:
	CSPDLMaker() : mHandle(cuCspStartup(CSSM_FALSE)) {}
	operator CSSM_CSP_HANDLE() {return mHandle;}
};

static ModuleNexus<CSPDLMaker> gCSPHandle;

OSStatus SecKeychainItemImport(
	CFDataRef							importedData,
	CFStringRef							fileNameOrExtension,	// optional
	SecExternalFormat					*inputFormat,			// optional, IN/OUT
	SecExternalItemType					*itemType,				// optional, IN/OUT
	SecItemImportExportFlags			flags, 
	const SecKeyImportExportParameters  *keyParams,				// optional
	SecKeychainRef						importKeychain,			// optional
	CFArrayRef							*outItems)				/* optional */
{
	BEGIN_IMP_EXP_SECAPI
	
	bool				isPem;
	OSStatus			ortn = noErr;
	OSStatus			pem_ortn = noErr;
	SecImportRep		*rep = NULL;
	SecExternalFormat   callerInputFormat;
	SecExternalItemType callerItemType;
	CSSM_CSP_HANDLE		cspHand = 0;
	CFIndex				dex;
	CFStringRef			ourFileStr = NULL;
	
	if((importedData == NULL) || (CFDataGetLength(importedData) == 0)) {
		return paramErr;
	}
	/* all other args are optional */
	
	if(inputFormat) {
		callerInputFormat = *inputFormat;
	}
	else {
		callerInputFormat = kSecFormatUnknown;
	}
	if(itemType) {
		callerItemType = *itemType;
	}
	else {
		callerItemType = kSecItemTypeUnknown;
	}
	
	CFIndex numReps = 0;
	SecExternalFormat tempFormat = callerInputFormat;
	SecExternalItemType tempType = callerItemType;
	ImpPrivKeyImportState keyImportState = PIS_NoLimit;

	CFMutableArrayRef importReps = CFArrayCreateMutable(NULL, 0, NULL);
	CFMutableArrayRef createdKcItems = CFArrayCreateMutable(NULL, 0, 
		&kCFTypeArrayCallBacks);
	/* subsequent errors to errOut: */
	
	/* 
	 * importedData --> one or more SecImportReps.
	 * Note successful PEM decode can override caller's inputFormat and/or itemType.
	 */
	pem_ortn = impExpParsePemToImportRefs(importedData, importReps, &isPem);
	/* remember how PEM decode failed, but continue to examine other possibilities */
	if(!isPem) {
		/* incoming blob is one binary item, type possibly unknown */
		rep = new SecImportRep(importedData, callerItemType, callerInputFormat,
			CSSM_ALGID_NONE);
		CFArrayAppendValue(importReps, rep);
		if(fileNameOrExtension) {
			ourFileStr = fileNameOrExtension;
			CFRetain(ourFileStr);
		}
	}
	else {
		/* 
		 * Strip off possible .pem extension in case there's another one in 
		 * front of it 
		 */
		assert(CFArrayGetCount(importReps) >= 1);
		if(fileNameOrExtension) {
			if(CFStringHasSuffix(fileNameOrExtension, CFSTR(".pem"))) {
				ourFileStr = impExpImportDeleteExtension(fileNameOrExtension);
			}
			else {
				ourFileStr = fileNameOrExtension;
				CFRetain(ourFileStr);
			}
		}
	}
	
	/* 
	 * Ensure we know type and format (and, for raw keys, algorithm) of each item. 
	 */
	numReps = CFArrayGetCount(importReps);	
	if(numReps > 1) {
		/* 
		 * Incoming kSecFormatPEMSequence, caller specs are useless now.
		 * Hopefully the PEM parsing disclosed the info we'll need.
		 */
		if(ourFileStr) {
			CFRelease(ourFileStr);
			ourFileStr = NULL;
		}
		tempFormat = kSecFormatUnknown;
		tempType = kSecItemTypeUnknown;
	}
	for(dex=0; dex<numReps; dex++) {
		rep = (SecImportRep *)CFArrayGetValueAtIndex(importReps, dex);
		bool ok = impExpInferTypeAndFormat(rep, ourFileStr, tempFormat, tempType);
		if(!ok) {
			ortn = errSecUnknownFormat;
			goto errOut;
		}
	}

	/* Get a CSPDL handle, somehow, as convenience for lower level code */
	if(importKeychain != NULL) {
		ortn = SecKeychainGetCSPHandle(importKeychain, &cspHand);
		if(ortn) {
			goto errOut;
		}
	}
	else {
		cspHand = gCSPHandle();
	}
	
	if(keyParams && (keyParams->flags & kSecKeyImportOnlyOne)) {
		keyImportState = PIS_AllowOne;
	}
	
	/* Everything looks good: Go */ 
	for(CFIndex dex=0; dex<numReps; dex++) {
		rep = (SecImportRep *)CFArrayGetValueAtIndex(importReps, dex);
		ortn = rep->importRep(importKeychain, cspHand, flags, keyParams, 
			keyImportState, createdKcItems);
		if(ortn) {
			goto errOut;
		}
	}

	/* Give as much info to caller as we can even if we got an error on import */
	if(inputFormat != NULL) {
		if(numReps > 1) {
			assert(isPem);
			*inputFormat = kSecFormatPEMSequence;
		}
		else {
			/* format from sole item in importReps */
			assert(numReps != 0);
			rep = (SecImportRep *)CFArrayGetValueAtIndex(importReps, 0);
			*inputFormat = rep->mExternFormat;
		}
	}
	if(itemType != NULL) {
		if(numReps > 1) {
			assert(isPem);
			*itemType = kSecItemTypeAggregate;
		}
		else {
			/* itemType from sole item in importReps */
			assert(numReps != 0);
			rep = (SecImportRep *)CFArrayGetValueAtIndex(importReps, 0);
			*itemType = rep->mExternType;
		}
	}
	if((ortn == noErr) && (outItems != NULL)) {
		/* return the array */
		*outItems = createdKcItems;
		createdKcItems = NULL;
	}
	/* else caller doesn't want SecKeychainItemsRefs; we'll release below */

errOut:
	if(createdKcItems) {
		CFRelease(createdKcItems);
	}
	if(importReps != NULL) {
		/* CFArray of our own classes, no auto release */
		CFIndex num = CFArrayGetCount(importReps);
		for(dex=0; dex<num; dex++) {
			rep = (SecImportRep *)CFArrayGetValueAtIndex(importReps, dex);
			delete rep;
		}
		CFRelease(importReps);
	}
	if(ourFileStr) {
		CFRelease(ourFileStr);
	}
	if(ortn) {
		/* error occurred importing non-PEM representation */
		return SecKeychainErrFromOSStatus(ortn);
	}
	if(pem_ortn == errSecUnsupportedFormat && numReps == 0) {
		/* error occurred importing as PEM, and no other rep was imported */
		return SecKeychainErrFromOSStatus(pem_ortn);
	}
	return noErr;
	
	END_IMP_EXP_SECAPI
}

OSStatus SecItemImport(
	CFDataRef							importedData,
	CFStringRef							fileNameOrExtension,	/* optional */
	SecExternalFormat					*inputFormat,			/* optional, IN/OUT */
	SecExternalItemType					*itemType,				/* optional, IN/OUT */
	SecItemImportExportFlags			flags, 
	const SecItemImportExportKeyParameters  *keyParams,				/* optional */
	SecKeychainRef						importKeychain,			/* optional */
	CFArrayRef							*outItems)
{
	
	SecKeyImportExportParameters* oldStructPtr = NULL;
	SecKeyImportExportParameters oldStruct;
	memset(&oldStruct, 0, sizeof(oldStruct));
	
	
	if (NULL != keyParams)
	{
		if (ConvertSecKeyImportExportParametersToSecImportExportKeyParameters(NULL,
			keyParams, &oldStruct))
		{
			oldStructPtr = &oldStruct;
		}
	}
	
	return SecKeychainItemImport(importedData, fileNameOrExtension, inputFormat, 
		itemType, flags, oldStructPtr, importKeychain, outItems);
}

