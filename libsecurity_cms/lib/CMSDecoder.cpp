/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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

/*
 * CMSDecoder.cpp - Interface for decoding CMS messages.
 */
 
#include "CMSDecoder.h"
#include "CMSPrivate.h"
#include "CMSUtils.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDigestContext.h>
#include <Security/SecCertificate.h>
#include <Security/SecSMIME.h>
#include <Security/oidsattr.h>
#include <Security/SecTrustPriv.h>
#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>
#include <AssertMacros.h>

#pragma mark --- Private types and definitions ---

/*
 * Decoder state.
 */
typedef enum {
	DS_Init,		/* between CMSDecoderCreate and CMSDecoderUpdateMessage */
	DS_Updating,	/* between first CMSDecoderUpdateMessage and CMSDecoderFinalizeMessage */
	DS_Final		/* CMSDecoderFinalizeMessage has been called */
} CMSDecoderState;

/*
 * Caller's CMSDecoderRef points to one of these. 
 */
struct _CMSDecoder {
	CFRuntimeBase		base;
	CMSDecoderState		decState;
	SecArenaPoolRef		arena;				/* the decoder's arena */
	SecCmsDecoderRef	decoder;
	CFDataRef			detachedContent;
	CFTypeRef			keychainOrArray;	/* from CMSDecoderSetSearchKeychain() */
	
	/* 
	 * The following are valid (and quiescent) after CMSDecoderFinalizeMessage().
	 */
	SecCmsMessageRef	cmsMsg;
	Boolean				wasEncrypted;	/* valid after CMSDecoderFinalizeMessage() */
	SecCmsSignedDataRef	signedData;		/* if there is one... */
	/* only non-NULL if we found a signedData */
	size_t				numSigners;
	CSSM_OID			*eContentType;
	/* etc. */
};

static void cmsDecoderInit(CFTypeRef dec);
static void cmsDecoderFinalize(CFTypeRef dec);

static CFRuntimeClass cmsDecoderRuntimeClass = 
{
	0,			/* version */
	"CMSDecoder",
	cmsDecoderInit,
	NULL,		/* copy */
	cmsDecoderFinalize,
	NULL,		/* equal - just use pointer equality */
	NULL,		/* hash, ditto */
	NULL,		/* copyFormattingDesc */
	NULL		/* copyDebugDesc */
};

#pragma mark --- Private Routines ---

static CFTypeID cmsDecoderTypeID = _kCFRuntimeNotATypeID;

/* one time only class init, called via pthread_once() in CMSDecoderGetTypeID() */
static void cmsDecoderClassInitialize(void)
{
	cmsDecoderTypeID = 
		_CFRuntimeRegisterClass((const CFRuntimeClass * const)&cmsDecoderRuntimeClass);
}

/* init called out from _CFRuntimeCreateInstance() */
static void cmsDecoderInit(CFTypeRef dec)
{
	char *start = ((char *)dec) + sizeof(CFRuntimeBase);
	memset(start, 0, sizeof(struct _CMSDecoder) - sizeof(CFRuntimeBase));
}

/*
 * Dispose of a CMSDecoder. Called out from CFRelease().
 */
static void cmsDecoderFinalize(
	CFTypeRef		dec)
{
	CMSDecoderRef cmsDecoder = (CMSDecoderRef)dec;
	if(cmsDecoder == NULL) {
		return;
	}
	if(cmsDecoder->decoder != NULL) {
		/* 
		 * Normally this gets freed in SecCmsDecoderFinish - this is 
		 * an error case.
		 * FIXME: SecCmsDecoderDestroy() appears to destroy the
		 * cmsMsg too! Plus there's a comment there re: a leak...
		 */
		SecCmsDecoderDestroy(cmsDecoder->decoder);
	}
	CFRELEASE(cmsDecoder->detachedContent);
	CFRELEASE(cmsDecoder->keychainOrArray);
	if(cmsDecoder->cmsMsg != NULL) {
		SecCmsMessageDestroy(cmsDecoder->cmsMsg);
	}
	if(cmsDecoder->arena != NULL) {
		SecArenaPoolFree(cmsDecoder->arena, false);
	}
}


/*
 * Given detached content and a valid (decoded) SignedData, digest the detached 
 * content. This occurs at the later of {CMSDecoderFinalizeMessage() finding a 
 * SignedData when already have detachedContent, or CMSDecoderSetDetachedContent() 
 * when we already have a SignedData).
 */
static OSStatus cmsDigestDetachedContent(
	CMSDecoderRef cmsDecoder)
{
	ASSERT((cmsDecoder->signedData != NULL) && (cmsDecoder->detachedContent != NULL));
	
	SECAlgorithmID **digestAlgorithms = SecCmsSignedDataGetDigestAlgs(cmsDecoder->signedData);
	if(digestAlgorithms == NULL) {
		return errSecUnknownFormat;
	}
	SecCmsDigestContextRef digcx = SecCmsDigestContextStartMultiple(digestAlgorithms);
	if(digcx == NULL) {
		return memFullErr;
	}
	CSSM_DATA **digests = NULL;
	
	SecCmsDigestContextUpdate(digcx, CFDataGetBytePtr(cmsDecoder->detachedContent), 
		CFDataGetLength(cmsDecoder->detachedContent));
	/* note this frees the digest content regardless */
	OSStatus ortn = SecCmsDigestContextFinishMultiple(digcx, cmsDecoder->arena, &digests);
	if(ortn) {
		ortn = cmsRtnToOSStatus(ortn);
		CSSM_PERROR("SecCmsDigestContextFinishMultiple", ortn);
		return ortn;
	}
	ortn = SecCmsSignedDataSetDigests(cmsDecoder->signedData, digestAlgorithms, digests);
	if(ortn) {
		ortn = cmsRtnToOSStatus(ortn);
		CSSM_PERROR("SecCmsSignedDataSetDigests", ortn);
	}
	return ortn;
}

#pragma mark --- Start of Public API ---

CFTypeID CMSDecoderGetTypeID(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	
	if(cmsDecoderTypeID == _kCFRuntimeNotATypeID) {
		pthread_once(&once, &cmsDecoderClassInitialize);
	}
	return cmsDecoderTypeID;
}

/*
 * Create a CMSDecoder. Result must eventually be freed via CFRelease().
 */
OSStatus CMSDecoderCreate(
	CMSDecoderRef		*cmsDecoderOut)	/* RETURNED */
{
	CMSDecoderRef cmsDecoder = NULL;

	uint32_t extra = sizeof(*cmsDecoder) - sizeof(cmsDecoder->base);
	cmsDecoder = (CMSDecoderRef)_CFRuntimeCreateInstance(NULL, CMSDecoderGetTypeID(),
		extra, NULL);
	if(cmsDecoder == NULL) {
		return memFullErr;
	}
	cmsDecoder->decState = DS_Init;
	*cmsDecoderOut = cmsDecoder;
	return noErr;
}

/* 
 * Feed raw bytes of the message to be decoded into the decoder. Can be called
 * multiple times. 
 */
OSStatus CMSDecoderUpdateMessage(
	CMSDecoderRef		cmsDecoder,
	const void			*msgBytes,
	size_t				msgBytesLen)
{
	if(cmsDecoder == NULL) {
		return paramErr;
	}
	
	OSStatus ortn;
	switch(cmsDecoder->decState) {
		case DS_Init:
			/* First time through; set up */
			ASSERT(cmsDecoder->decoder == NULL);
			ASSERT(cmsDecoder->arena == NULL);
			ortn = SecArenaPoolCreate(1024, &cmsDecoder->arena);
			if(ortn) {
				return cmsRtnToOSStatus(ortn);
			}
			ortn = SecCmsDecoderCreate(cmsDecoder->arena, 
				NULL, NULL, NULL, NULL, NULL, NULL, &cmsDecoder->decoder);
			if(ortn) {
				ortn = cmsRtnToOSStatus(ortn);
				CSSM_PERROR("SecCmsDecoderCreate", ortn);
				return ortn;
			}
			cmsDecoder->decState = DS_Updating;
			break;
		
		case DS_Updating:
			ASSERT(cmsDecoder->decoder != NULL);
			break;
			
		case DS_Final:
			/* Too late for another update */
			return paramErr;
			
		default:
			dprintf("CMSDecoderUpdateMessage: bad decState\n");
			return internalComponentErr;
	}
	
	/* FIXME - CFIndex same size as size_t on 64bit? */
	ortn = SecCmsDecoderUpdate(cmsDecoder->decoder, msgBytes, (CFIndex)msgBytesLen);
	if(ortn) {
		ortn = cmsRtnToOSStatus(ortn, errSecUnknownFormat);
		CSSM_PERROR("SecCmsDecoderUpdate", ortn);
	}
	return ortn;
}
	
/* 
 * Indicate that no more CMSDecoderUpdateMessage() calls are forthcoming;
 * finish decoding the message. We parse the message as best we can, up to 
 * but not including verifying individual signerInfos. 
 */
OSStatus CMSDecoderFinalizeMessage(
	CMSDecoderRef		cmsDecoder)
{
	if(cmsDecoder == NULL) {
		return paramErr;
	}
	if(cmsDecoder->decState != DS_Updating) {
		return paramErr;
	}
	ASSERT(cmsDecoder->decoder != NULL);
	OSStatus ortn = SecCmsDecoderFinish(cmsDecoder->decoder, &cmsDecoder->cmsMsg);
	cmsDecoder->decState = DS_Final;
	
	/* SecCmsDecoderFinish destroyed the decoder even on failure */
	cmsDecoder->decoder = NULL;
	
	if(ortn) {
		ortn = cmsRtnToOSStatus(ortn, errSecUnknownFormat);
		CSSM_PERROR("SecCmsDecoderFinish", ortn);
		return ortn;
	}
	
	ASSERT(cmsDecoder->cmsMsg != NULL);
	cmsDecoder->wasEncrypted = SecCmsMessageIsEncrypted(cmsDecoder->cmsMsg);
	
	/* Look for a SignedData */
	int numContentInfos = SecCmsMessageContentLevelCount(cmsDecoder->cmsMsg);
	int dex;
	for(dex=0; dex<numContentInfos; dex++) {
		SecCmsContentInfoRef ci = SecCmsMessageContentLevel(cmsDecoder->cmsMsg, dex);
		SECOidTag tag = SecCmsContentInfoGetContentTypeTag(ci);
		switch(tag) {
			case SEC_OID_PKCS7_SIGNED_DATA:
				cmsDecoder->signedData = 
					(SecCmsSignedDataRef)SecCmsContentInfoGetContent(ci);
				/* dig down one more layer for eContentType */
				ci = SecCmsSignedDataGetContentInfo(cmsDecoder->signedData);
				cmsDecoder->eContentType = SecCmsContentInfoGetContentTypeOID(ci);
				break;
			default:
				break;
		}
		if(cmsDecoder->signedData != NULL) {
			break;
		}
					
	}
	
	/* minimal processing of optional signedData... */
	if(cmsDecoder->signedData != NULL) {
		cmsDecoder->numSigners = (size_t)
			SecCmsSignedDataSignerInfoCount(cmsDecoder->signedData);
		if(cmsDecoder->detachedContent != NULL) {
			/* time to calculate digests from detached content */
			ortn = cmsDigestDetachedContent(cmsDecoder);
		}
	}
	return ortn;
}

/*
 * A signed CMS message optionally includes the data which was signed. If the
 * message does not include the signed data, caller specifies the signed data
 * (the "detached content") here. 
 *
 * This can be called either before or after the actual decoding of the message
 * (via CMSDecoderUpdateMessage() and CMSDecoderFinalizeMessage()); the only
 * restriction is that, if detached content is required, this function must 
 * be called befoere successfully ascertaining the signature status via 
 * CMSDecoderCopySignerStatus().
 */
OSStatus CMSDecoderSetDetachedContent(
	CMSDecoderRef		cmsDecoder,
	CFDataRef			detachedContent)
{
	if((cmsDecoder == NULL) || (detachedContent == NULL)) {
		return paramErr;
	}
	cmsDecoder->detachedContent = detachedContent;
	CFRetain(detachedContent);
	
	if(cmsDecoder->signedData != NULL) {
		/* time to calculate digests from detached content */
		ASSERT(cmsDecoder->decState == DS_Final);
		return cmsDigestDetachedContent(cmsDecoder);
	}
	return noErr;
}

/*
 * Obtain the detached content specified in CMSDecoderSetDetachedContent().
 * Returns a NULL detachedContent if no detached content has been specified. 
 * Caller must CFRelease() the result.
 */
OSStatus CMSDecoderCopyDetachedContent(
	CMSDecoderRef		cmsDecoder,
	CFDataRef			*detachedContent)		/* RETURNED */
{
	if((cmsDecoder == NULL) || (detachedContent == NULL)) {
		return paramErr;
	}
	if(cmsDecoder->detachedContent != NULL) {
		CFRetain(cmsDecoder->detachedContent);
	}
	*detachedContent = cmsDecoder->detachedContent;
	return noErr;
}

/*
 * Optionally specify a SecKeychainRef, or an array of them, containing
 * intermediate certs to be used in verifying a signed message's signer
 * certs. By default, the default keychain search list is used for this. 
 * Specify an empty CFArrayRef to search *no* keychains for intermediate
 * certs. 
 * IF this is called, it must be called before CMSDecoderCopySignerStatus().
 */
OSStatus CMSDecoderSetSearchKeychain(
	CMSDecoderRef		cmsDecoder,
	CFTypeRef			keychainOrArray)
{
	if(cmsDecoder == NULL) {
		return paramErr;
	}
	cmsDecoder->keychainOrArray = keychainOrArray;
	if(keychainOrArray) {
		CFRetain(keychainOrArray);
	}
	return noErr;
}

/*
 * Obtain the number of signers of a message. A result of zero indicates that
 * the message was not signed. 
 */
OSStatus CMSDecoderGetNumSigners(
	CMSDecoderRef		cmsDecoder,
	size_t				*numSigners)			/* RETURNED */
{
	if((cmsDecoder == NULL) || (numSigners == NULL)) {
		return paramErr;
	}
	if(cmsDecoder->decState != DS_Final) {
		return paramErr;
	}
	*numSigners = cmsDecoder->numSigners;
	return noErr;
}

/*
 * Obtain the status of a CMS message's signature. A CMS message can 
 * be signed my multiple signers; this function returns the status
 * associated with signer 'n' as indicated by the signerIndex parameter. 
 */
OSStatus CMSDecoderCopySignerStatus(
	CMSDecoderRef		cmsDecoder,
	size_t				signerIndex,
	CFTypeRef			policyOrArray,
	Boolean				evaluateSecTrust,
	CMSSignerStatus		*signerStatus,			/* optional; RETURNED */
	SecTrustRef			*secTrust,				/* optional; RETURNED */
	OSStatus			*certVerifyResultCode)	/* optional; RETURNED */
{
	if((cmsDecoder == NULL) || (cmsDecoder->decState != DS_Final)) {
		return paramErr;
	}
	
	/* initialize return values */
	if(signerStatus) {
		*signerStatus = kCMSSignerUnsigned;
	}
	if(secTrust) {
		*secTrust = NULL;
	}
	if(certVerifyResultCode) {
		*certVerifyResultCode = 0;
	}
	
	if(cmsDecoder->signedData == NULL) {
		*signerStatus = kCMSSignerUnsigned;	/* redundant, I know, but explicit */
		return noErr;
	}
	ASSERT(cmsDecoder->numSigners > 0);
	if(signerIndex >= cmsDecoder->numSigners) {
		*signerStatus = kCMSSignerInvalidIndex;
		return noErr;
	}
	if(!SecCmsSignedDataHasDigests(cmsDecoder->signedData)) {
		*signerStatus = kCMSSignerNeedsDetachedContent;	
		return noErr;
	}
	
	/*
	 * OK, we should be able to verify this signerInfo.
	 * I think we have to do the SecCmsSignedDataVerifySignerInfo first 
	 * in order get all the cert pieces into place before returning them 
	 * to the caller. 
	 */
	SecTrustRef theTrust = NULL;
	OSStatus vfyRtn = SecCmsSignedDataVerifySignerInfo(cmsDecoder->signedData, 
		signerIndex, 
		/* 
		 * FIXME this cast should not be necessary, but libsecurity_smime 
		 * declares this argument as a SecKeychainRef
		 */
		(SecKeychainRef)cmsDecoder->keychainOrArray,
		policyOrArray, 
		&theTrust);
	 /* Subsequent errors to errOut: */
	 
	/*
	 * NOTE the smime lib did NOT evaluate that SecTrust - it only does 
	 * SecTrustEvaluate() if we don't ask for a copy.
	 * 
	 * FIXME deal with multitudes of status returns here...for now, proceed with
	 * obtaining components the caller wants and assume that a nonzero vfyRtn
	 * means "bad signature". 
	 */
	OSStatus ortn = noErr;
	SecTrustResultType secTrustResult;
	CSSM_RETURN tpVfyStatus = CSSM_OK;
	OSStatus evalRtn;
	
	if(secTrust != NULL) {
		*secTrust = theTrust;
		/* we'll release our reference at the end */
		if (theTrust)
			CFRetain(theTrust);
	}
	SecCmsSignerInfoRef signerInfo = 
		SecCmsSignedDataGetSignerInfo(cmsDecoder->signedData, signerIndex);
	if(signerInfo == NULL) {
		/* should never happen */
		ASSERT(0);
		dprintf("CMSDecoderCopySignerStatus: no signerInfo\n");
		ortn = internalComponentErr;
		goto errOut;
	}

	/* now do the actual cert verify */
	if(evaluateSecTrust) {
		evalRtn = SecTrustEvaluate(theTrust, &secTrustResult);
		if(evalRtn) {
			/* should never happen */
			CSSM_PERROR("SecTrustEvaluate", evalRtn);
			dprintf("CMSDecoderCopySignerStatus: SecTrustEvaluate error\n");
			ortn = internalComponentErr;
			goto errOut;
		}
		switch(secTrustResult) {
			case kSecTrustResultUnspecified:
				/* cert chain valid, no special UserTrust assignments */
			case kSecTrustResultProceed:
				/* cert chain valid AND user explicitly trusts this */
				break;
			case kSecTrustResultDeny:
				tpVfyStatus = CSSMERR_APPLETP_TRUST_SETTING_DENY;
				break;
			case kSecTrustResultConfirm:
				dprintf("SecTrustEvaluate reported confirm\n");
				tpVfyStatus = CSSMERR_TP_NOT_TRUSTED;
				break;
			default:
			{
				/* get low-level TP error */
				OSStatus tpStatus;
				ortn = SecTrustGetCssmResultCode(theTrust, &tpStatus);
				if(ortn) {
					CSSM_PERROR("SecTrustGetCssmResultCode", ortn);
				}
				else {
					tpVfyStatus = tpStatus;
				}
				CSSM_PERROR("TP status after SecTrustEvaluate", tpVfyStatus);
				break;
			}
		} 	/* switch(secTrustResult) */
	}		/* evaluateSecTrust true */
	if(certVerifyResultCode != NULL) {
		*certVerifyResultCode = tpVfyStatus;
	}
	
	/* cook up global status based on vfyRtn and tpVfyStatus */
	if(signerStatus != NULL) {
		if((vfyRtn == noErr) && (tpVfyStatus == CSSM_OK))  {
			*signerStatus = kCMSSignerValid;
		}
		else if(vfyRtn != noErr) {
			/* this could mean other things, but for now... */
			*signerStatus = kCMSSignerInvalidSignature;
		}
		else {
			*signerStatus = kCMSSignerInvalidCert;
		}
	}
errOut:
	CFRELEASE(theTrust);
	return ortn;
}

/*
 * Obtain the email address of signer 'signerIndex' of a CMS message, if
 * present. 
 *
 * This cannot be called until after CMSDecoderFinalizeMessage() is called. 
 */
OSStatus CMSDecoderCopySignerEmailAddress(
	CMSDecoderRef		cmsDecoder,
	size_t				signerIndex,
	CFStringRef			*signerEmailAddress)	/* RETURNED */
{
	if((cmsDecoder == NULL) || 
	   (signerEmailAddress == NULL) ||
	   (cmsDecoder->signedData == NULL) ||			/* not signed */
	   (signerIndex >= cmsDecoder->numSigners) ||	/* index out of range */
	   (cmsDecoder->decState != DS_Final)) {
		return paramErr;
	}
	
	SecCmsSignerInfoRef signerInfo = 
		SecCmsSignedDataGetSignerInfo(cmsDecoder->signedData, signerIndex);
	if(signerInfo == NULL) {
		/* should never happen */
		ASSERT(0);
		dprintf("CMSDecoderCopySignerEmailAddress: no signerInfo\n");
		return internalComponentErr;
	}
	
	/* 
	 * This is leaking memory in libsecurityKeychain per Radar 4412699. 
	 */
	*signerEmailAddress = SecCmsSignerInfoGetSignerEmailAddress(signerInfo);
	return noErr;
}

/*
 * Obtain the certificate of signer 'signerIndex' of a CMS message, if
 * present. 
 *
 * This cannot be called until after CMSDecoderFinalizeMessage() is called. 
 */
OSStatus CMSDecoderCopySignerCert(
	CMSDecoderRef		cmsDecoder,
	size_t				signerIndex,
	SecCertificateRef	*signerCert)			/* RETURNED */
{
	if((cmsDecoder == NULL) || 
	   (signerCert == NULL) ||
	   (cmsDecoder->signedData == NULL) ||			/* not signed */
	   (signerIndex >= cmsDecoder->numSigners) ||	/* index out of range */
	   (cmsDecoder->decState != DS_Final)) {
		return paramErr;
	}

	SecCmsSignerInfoRef signerInfo = 
		SecCmsSignedDataGetSignerInfo(cmsDecoder->signedData, signerIndex);
	if(signerInfo == NULL) {
		/* should never happen */
		ASSERT(0);
		dprintf("CMSDecoderCopySignerCertificate: no signerInfo\n");
		return internalComponentErr;
	}
	*signerCert = SecCmsSignerInfoGetSigningCertificate(signerInfo, NULL);
	/* libsecurity_smime does NOT retain that */
	if(*signerCert == NULL) {
		/* should never happen */
		ASSERT(0);
		dprintf("CMSDecoderCopySignerCertificate: no signerCert\n");
		return internalComponentErr;
	}
	CFRetain(*signerCert);
	return noErr;
}

/* 
 * Determine whether a CMS message was encrypted, and if so, whether we were
 * able to decrypt it. 
 */
OSStatus CMSDecoderIsContentEncrypted(
	CMSDecoderRef	cmsDecoder,
	Boolean			*wasEncrypted)
{
	if((cmsDecoder == NULL) || (wasEncrypted == NULL)) {
		return paramErr;
	}
	if(cmsDecoder->decState != DS_Final) {
		return paramErr;
	}
	*wasEncrypted = cmsDecoder->wasEncrypted;
	return noErr;
}

/*
 * Obtain the eContentType OID for a SignedData's EncapsulatedContentType, if 
 * present.
 */
OSStatus CMSDecoderCopyEncapsulatedContentType(
	CMSDecoderRef		cmsDecoder,
	CFDataRef			*eContentType)		/* RETURNED */
{
	if((cmsDecoder == NULL) || (eContentType == NULL)) {
		return paramErr;
	}
	if(cmsDecoder->decState != DS_Final) {
		return paramErr;
	}
	if(cmsDecoder->signedData == NULL) {
		*eContentType = NULL;
	}
	else {
		CSSM_OID *ecOid = cmsDecoder->eContentType;
		*eContentType = CFDataCreate(NULL, ecOid->Data, ecOid->Length);
	}
	return noErr;
}

/*
 * Obtain an array of all of the certificates in a message. Elements of the 
 * returned array are SecCertificateRefs. The caller must CFRelease the returned 
 * array.
 * This cannot be called until after CMSDecoderFinalizeMessage() is called. 
 */
OSStatus CMSDecoderCopyAllCerts(
	CMSDecoderRef		cmsDecoder,
	CFArrayRef			*certs)					/* RETURNED */
{
	if((cmsDecoder == NULL) || (certs == NULL)) {
		return paramErr;
	}
	if(cmsDecoder->decState != DS_Final) {
		return paramErr;
	}
	if(cmsDecoder->signedData == NULL) {
		/* message wasn't signed */
		*certs = NULL;
		return noErr;
	}
	
	/* NULL_terminated array of CSSM_DATA ptrs */
	CSSM_DATA_PTR *cssmCerts = SecCmsSignedDataGetCertificateList(cmsDecoder->signedData);
	if((cssmCerts == NULL) || (*cssmCerts == NULL)) {
		*certs = NULL;
		return noErr;
	}
	
	CFMutableArrayRef allCerts = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CSSM_DATA_PTR *cssmCert;
	for(cssmCert=cssmCerts; *cssmCert!=NULL; cssmCert++) {
		OSStatus ortn;
		SecCertificateRef cfCert;
		ortn = SecCertificateCreateFromData(*cssmCert, 
			CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
			&cfCert);
		if(ortn) {
			CFRelease(allCerts);
			return ortn;
		}
		CFArrayAppendValue(allCerts, cfCert);
		/* the array holds the only needed refcount */
		CFRelease(cfCert);
	}
	*certs = allCerts;
	return noErr;
}

/*
 * Obtain the actual message content (payload), if any. If the message was
 * signed with detached content this will return NULL.
 * Caller must CFRelease the result. 
 */
OSStatus CMSDecoderCopyContent(
	CMSDecoderRef		cmsDecoder,
	CFDataRef			*content)				/* RETURNED */
{
	if((cmsDecoder == NULL) || (content == NULL)) {
		return paramErr;
	}
	if(cmsDecoder->decState != DS_Final) {
		return paramErr;
	}
	if(cmsDecoder->cmsMsg == NULL) {
		/* Hmmm....looks like the finalize call failed */
		return paramErr;
	}
	CSSM_DATA_PTR odata = SecCmsMessageGetContent(cmsDecoder->cmsMsg);
	if((odata == NULL) || (odata->Length == 0)) {
		/* i.e., detached content */
		*content = NULL;
		return noErr;
	}
	*content = CFDataCreate(NULL, (const UInt8 *)odata->Data, odata->Length);
	return noErr;
}

#pragma mark --- SPI declared in CMSPrivate.h ---

/*
 * Obtain the SecCmsMessageRef associated with a CMSDecoderRef. Intended 
 * to be called after decoding the message (i.e., after 
 * CMSDecoderFinalizeMessage() to gain finer access to the contents of the
 * SecCmsMessageRef than is otherwise available via the CMSDecoder interface. 
 * Returns a NULL SecCmsMessageRef if CMSDecoderFinalizeMessage() has not been
 * called. 
 *
 * The CMSDecoder retains ownership of the returned SecCmsMessageRef.
 */
OSStatus CMSDecoderGetCmsMessage(
	CMSDecoderRef		cmsDecoder,
	SecCmsMessageRef	*cmsMessage)		/* RETURNED */
{
	if((cmsDecoder == NULL) || (cmsMessage == NULL)) {
		return paramErr;
	}
	/* any state, whether we have a msg or not is OK */
	*cmsMessage = cmsDecoder->cmsMsg;
	return noErr;
}

/* 
 * Optionally specify a SecCmsDecoderRef to use with a CMSDecoderRef.
 * If this is called, it must be called before the first call to 
 * CMSDecoderUpdateMessage(). The CMSDecoderRef takes ownership of the
 * incoming SecCmsDecoderRef.
 */
OSStatus CMSDecoderSetDecoder(
	CMSDecoderRef		cmsDecoder,
	SecCmsDecoderRef	decoder)
{
	if((cmsDecoder == NULL) || (decoder == NULL)) {
		return paramErr;
	}
	switch(cmsDecoder->decState) {
		case DS_Init:
			ASSERT(cmsDecoder->decoder == NULL);
			cmsDecoder->decoder = decoder;
			cmsDecoder->decState = DS_Updating;
			return noErr;
		case DS_Updating:
		case DS_Final:
			return paramErr;
	}
	return noErr;
}		
	
/* 
 * Obtain the SecCmsDecoderRef associated with a CMSDecoderRef. 
 * Returns a NULL SecCmsDecoderRef if neither CMSDecoderSetDecoder() nor
 * CMSDecoderUpdateMessage() has been called. 
 * The CMSDecoderRef retains ownership of the SecCmsDecoderRef.
 */
OSStatus CMSDecoderGetDecoder(
	CMSDecoderRef		cmsDecoder,
	SecCmsDecoderRef	*decoder)			/* RETURNED */
{
	if((cmsDecoder == NULL) || (decoder == NULL)) {
		return paramErr;
	}
	/* any state, whether we have a decoder or not is OK */
	*decoder = cmsDecoder->decoder;
	return noErr;
}

/*
 * Obtain the signing time of signer 'signerIndex' of a CMS message, if
 * present. This is an unauthenticate time, although it is part of the
 * signed attributes of the message.
 *
 * Returns paramErr if the CMS message was not signed or if signerIndex
 * is greater than the number of signers of the message minus one. 
 *
 * This cannot be called until after CMSDecoderFinalizeMessage() is called. 
 */
OSStatus CMSDecoderCopySignerSigningTime(
       CMSDecoderRef           cmsDecoder,
       size_t                          signerIndex,            /* usually 0 */
       CFAbsoluteTime      *signingTime)                       /* RETURNED */
{
    OSStatus status = paramErr;
       SecCmsMessageRef cmsg;
       SecCmsSignedDataRef signedData = NULL;
    int numContentInfos = 0;
    
    require(cmsDecoder && signingTime, xit);
       require_noerr(CMSDecoderGetCmsMessage(cmsDecoder, &cmsg), xit);
    numContentInfos = SecCmsMessageContentLevelCount(cmsg);
    for (int dex = 0; !signedData && dex < numContentInfos; dex++)
    {
        SecCmsContentInfoRef ci = SecCmsMessageContentLevel(cmsg, dex);
        SECOidTag tag = SecCmsContentInfoGetContentTypeTag(ci);
        if (tag == SEC_OID_PKCS7_SIGNED_DATA)
            if ((signedData = SecCmsSignedDataRef(SecCmsContentInfoGetContent(ci))))
                if (SecCmsSignerInfoRef signerInfo = SecCmsSignedDataGetSignerInfo(signedData, (int)signerIndex))
                {
                    status = SecCmsSignerInfoGetSigningTime(signerInfo, signingTime);
                    break;
                }
    }
xit:
    return status;
}

/*
 * Obtain the timestamp of signer 'signerIndex' of a CMS message, if
 * present. This timestamp is an authenticated timestamp provided by
 * a timestamping authority.
 *
 * Returns paramErr if the CMS message was not signed or if signerIndex
 * is greater than the number of signers of the message minus one. 
 *
 * This cannot be called until after CMSDecoderFinalizeMessage() is called. 
 */
OSStatus CMSDecoderCopySignerTimestamp(
       CMSDecoderRef           cmsDecoder,
       size_t                          signerIndex,        /* usually 0 */
       CFAbsoluteTime      *timestamp)                 /* RETURNED */
{
    OSStatus status = paramErr;
       SecCmsMessageRef cmsg;
       SecCmsSignedDataRef signedData = NULL;
    int numContentInfos = 0;

    require(cmsDecoder && timestamp, xit);
       require_noerr(CMSDecoderGetCmsMessage(cmsDecoder, &cmsg), xit);
    numContentInfos = SecCmsMessageContentLevelCount(cmsg);
    for (int dex = 0; !signedData && dex < numContentInfos; dex++)
    {
        SecCmsContentInfoRef ci = SecCmsMessageContentLevel(cmsg, dex);
        SECOidTag tag = SecCmsContentInfoGetContentTypeTag(ci);
        if (tag == SEC_OID_PKCS7_SIGNED_DATA)
            if ((signedData = SecCmsSignedDataRef(SecCmsContentInfoGetContent(ci))))
                if (SecCmsSignerInfoRef signerInfo = SecCmsSignedDataGetSignerInfo(signedData, (int)signerIndex))
                {
                    status = SecCmsSignerInfoGetTimestampTime(signerInfo, timestamp);
                    break;
                }
    }

xit:
    return status;
}

/*
 * Obtain an array of the certificates in a timestamp response. Elements of the 
 * returned array are SecCertificateRefs. The caller must CFRelease the returned
 * array. This timestamp is an authenticated timestamp provided by
 * a timestamping authority.
 *
 * Returns paramErr if the CMS message was not signed or if signerIndex
 * is greater than the number of signers of the message minus one. It returns
 * errSecItemNotFound if no certificates were found.
 *
 * This cannot be called until after CMSDecoderFinalizeMessage() is called. 
 */
OSStatus CMSDecoderCopySignerTimestampCertificates(
       CMSDecoderRef           cmsDecoder,
       size_t                          signerIndex,            /* usually 0 */
       CFArrayRef          *certificateRefs)       /* RETURNED */
{
    OSStatus status = paramErr;
       SecCmsMessageRef cmsg;
       SecCmsSignedDataRef signedData = NULL;
    int numContentInfos = 0;

    require(cmsDecoder && certificateRefs, xit);
       require_noerr(CMSDecoderGetCmsMessage(cmsDecoder, &cmsg), xit);
    numContentInfos = SecCmsMessageContentLevelCount(cmsg);
    for (int dex = 0; !signedData && dex < numContentInfos; dex++)
    {
        SecCmsContentInfoRef ci = SecCmsMessageContentLevel(cmsg, dex);
        SECOidTag tag = SecCmsContentInfoGetContentTypeTag(ci);
        if (tag == SEC_OID_PKCS7_SIGNED_DATA)
            if ((signedData = SecCmsSignedDataRef(SecCmsContentInfoGetContent(ci))))
                if (SecCmsSignerInfoRef signerInfo = SecCmsSignedDataGetSignerInfo(signedData, (int)signerIndex))
                {
                    CFArrayRef certList = SecCmsSignerInfoGetTimestampCertList(signerInfo);
                    require_action(certList, xit, status = errSecItemNotFound);
                    *certificateRefs = CFArrayCreateCopy(kCFAllocatorDefault, certList);
                    status = noErr;
                    break;
                }
    }

xit:
    return status;
}
