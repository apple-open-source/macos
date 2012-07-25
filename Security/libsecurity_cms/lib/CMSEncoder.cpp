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
 * CMSEncoder.cpp - encode, sign, and/or encrypt CMS messages. 
 */
 
#include "CMSEncoder.h"
#include "CMSPrivate.h"
#include "CMSUtils.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsRecipientInfo.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecSMIME.h>
#include <Security/oidsattr.h>
#include <Security/SecAsn1Coder.h>
#include <Security/SecAsn1Types.h>
#include <Security/SecAsn1Templates.h>
#include <CoreFoundation/CFRuntime.h>
#include <pthread.h>

#include <security_smime/tsaSupport.h>
#include <security_smime/cmspriv.h>

#pragma mark --- Private types and definitions ---

/*
 * Encoder state.
 */
typedef enum {
	ES_Init,		/* between CMSEncoderCreate and earlier of CMSEncoderUpdateContent
					 *   and CMSEncodeGetCmsMessage */
	ES_Msg,			/* created cmsMsg in CMSEncodeGetCmsMessage, but no encoder yet */
	ES_Updating,	/* between first CMSEncoderUpdateContent and CMSEncoderCopyEncodedContent */
	ES_Final		/* CMSEncoderCopyEncodedContent has been called */
} CMSEncoderState;

/* 
 * High-level operation: what are we doing? 
 */
typedef enum {
	EO_Sign,
	EO_Encrypt,
	EO_SignEncrypt
} CMSEncoderOp;

/*
 * Caller's CMSEncoderRef points to one of these.
 */
struct _CMSEncoder {
	CFRuntimeBase		base;
	CMSEncoderState		encState;
	CMSEncoderOp		op;
	Boolean				detachedContent;
	CSSM_OID			eContentType;
	CFMutableArrayRef	signers;
	CFMutableArrayRef	recipients;
	CFMutableArrayRef	otherCerts;
	CMSSignedAttributes	signedAttributes;
	CFAbsoluteTime		signingTime;
	SecCmsMessageRef	cmsMsg;
	SecArenaPoolRef		arena;					/* the encoder's arena */
	SecCmsEncoderRef	encoder;
	CSSM_DATA			encoderOut;				/* output goes here... */
	bool				customCoder;			/* unless this is set by 
												 *    CMSEncoderSetEncoder */
	CMSCertificateChainMode chainMode;
};

static void cmsEncoderInit(CFTypeRef enc);
static void cmsEncoderFinalize(CFTypeRef enc);
    
static CFRuntimeClass cmsEncoderRuntimeClass = 
{
	0,			/* version */
	"CMSEncoder",
	cmsEncoderInit,
	NULL,		/* copy */
	cmsEncoderFinalize,
	NULL,		/* equal - just use pointer equality */
	NULL,		/* hash, ditto */
	NULL,		/* copyFormattingDesc */
	NULL		/* copyDebugDesc */
};

void
CmsMessageSetTSACallback(CMSEncoderRef cmsEncoder, SecCmsTSACallback tsaCallback);

#pragma mark --- Private routines ---

/*
 * Decode a CFStringRef representation of an integer
 */
static int cfStringToNumber(
	CFStringRef inStr)
{
	int max = 32;
	char buf[max];
	if (!inStr || !CFStringGetCString(inStr, buf, max-1, kCFStringEncodingASCII))
		return -1;
	return atoi(buf);
}
	
/*
 * Encode an integer component of an OID, return resulting number of bytes;
 * actual bytes are mallocd and returned in *encodeArray.
 */
static unsigned encodeNumber(
	int num,
	unsigned char **encodeArray)		// mallocd and RETURNED 
{
	unsigned char *result;
	unsigned dex;
	unsigned numDigits = 0;
	unsigned scratch;
	
	/* trival case - 0 maps to 0 */
	if(num == 0) {
		*encodeArray = (unsigned char *)malloc(1);
		**encodeArray = 0;
		return 1;
	}
	
	/* first calculate the number of digits in num, base 128 */
	scratch = (unsigned)num;
	while(scratch != 0) {
		numDigits++;
		scratch >>= 7;
	}
	
	result = (unsigned char *)malloc(numDigits);
	scratch = (unsigned)num;
	for(dex=0; dex<numDigits; dex++) { 
		result[numDigits - dex - 1] = scratch & 0x7f;
		scratch >>= 7;
	}
	
	/* all digits except the last one have m.s. bit set */
	for(dex=0; dex<(numDigits - 1); dex++) {
		result[dex] |= 0x80;
	}
	
	*encodeArray = result;
	return numDigits;
}

/*
 * Given an OID in dotted-decimal string representation, convert to binary
 * DER format. Returns a pointer in outOid which the caller must free(),
 * as well as the length of the data in outLen.
 * Function returns 0 if successful, non-zero otherwise.
 */
static int encodeOid(
	const unsigned char *inStr,
	unsigned char **outOid,
	unsigned int *outLen)
{
	unsigned char **digits = NULL;		/* array of char * from encodeNumber */
	unsigned *numDigits = NULL;			/* array of unsigned from encodeNumber */
	unsigned digit;
	unsigned numDigitBytes;				/* total #of output chars */
	unsigned char firstByte;
	unsigned char *outP;
	unsigned numsToProcess;
	CFStringRef oidStr = NULL;
	CFArrayRef argvRef = NULL;
	int num, argc, result = 1;
	
	/* parse input string into array of substrings */
	if (!inStr || !outOid || !outLen) goto cleanExit;
	oidStr = CFStringCreateWithCString(NULL, (const char *)inStr, kCFStringEncodingASCII);
	if (!oidStr) goto cleanExit;
	argvRef = CFStringCreateArrayBySeparatingStrings(NULL, oidStr, CFSTR("."));
	if (!argvRef) goto cleanExit;
	argc = CFArrayGetCount(argvRef);
	if (argc < 3) goto cleanExit;
	
	/* first two numbers in OID munge together */
	num = cfStringToNumber((CFStringRef)CFArrayGetValueAtIndex(argvRef, 0));
	if (num < 0) goto cleanExit;
	firstByte = (40 * num);
	num = cfStringToNumber((CFStringRef)CFArrayGetValueAtIndex(argvRef, 1));
	if (num < 0) goto cleanExit;
	firstByte += num;
	numDigitBytes = 1;

	numsToProcess = argc - 2;
	if(numsToProcess > 0) {
		/* skip this loop in the unlikely event that input is only two numbers */
		digits = (unsigned char **) malloc(numsToProcess * sizeof(unsigned char *));
		numDigits = (unsigned *) malloc(numsToProcess * sizeof(unsigned));
		for(digit=0; digit<numsToProcess; digit++) {
			num = cfStringToNumber((CFStringRef)CFArrayGetValueAtIndex(argvRef, digit+2));
			if (num < 0) goto cleanExit;
			numDigits[digit] = encodeNumber(num, &digits[digit]);
			numDigitBytes += numDigits[digit];
		}
	}
	*outLen = (2 + numDigitBytes);
	*outOid = outP = (unsigned char *) malloc(*outLen);
	*outP++ = 0x06;
	*outP++ = numDigitBytes;
	*outP++ = firstByte;
	for(digit=0; digit<numsToProcess; digit++) {
		unsigned int byteDex;
		for(byteDex=0; byteDex<numDigits[digit]; byteDex++) {
			*outP++ = digits[digit][byteDex];
		}
	}	
	if(digits) {
		for(digit=0; digit<numsToProcess; digit++) {
			free(digits[digit]);
		}
		free(digits);
		free(numDigits);
	}
	result = 0;

cleanExit:
	if (oidStr) CFRelease(oidStr);
	if (argvRef) CFRelease(argvRef);

	return result;
}

/*
 * Given a CF object reference describing an OID, convert to binary DER format
 * and fill out the CSSM_OID structure provided by the caller. Caller is
 * responsible for freeing the data pointer in outOid->Data.
 *
 * Function returns 0 if successful, non-zero otherwise.
 */

static int convertOid(
	CFTypeRef inRef,
	CSSM_OID *outOid)
{
	if (!inRef || !outOid)
		return paramErr;
	
	unsigned char *oidData = NULL;
	unsigned int oidLen = 0;

	if (CFGetTypeID(inRef) == CFStringGetTypeID()) {
		// CFStringRef: OID representation is a dotted-decimal string
		CFStringRef inStr = (CFStringRef)inRef;
		CFIndex max = CFStringGetLength(inStr) * 3;
		char buf[max];
		if (!CFStringGetCString(inStr, buf, max-1, kCFStringEncodingASCII))
			return paramErr;

		if(encodeOid((unsigned char *)buf, &oidData, &oidLen) != 0)
			return paramErr;
	}
	else if (CFGetTypeID(inRef) == CFDataGetTypeID()) {
		// CFDataRef: OID representation is in binary DER format
		CFDataRef inData = (CFDataRef)inRef;
		oidLen = (unsigned int) CFDataGetLength(inData);
		oidData = (unsigned char *) malloc(oidLen);
		memcpy(oidData, CFDataGetBytePtr(inData), oidLen);
	}
	else {
		// Not in a format we understand
		return paramErr;
	}
	outOid->Length = oidLen;
	outOid->Data = (uint8 *)oidData;
	return 0;
}

static CFTypeID cmsEncoderTypeID = _kCFRuntimeNotATypeID;

/* one time only class init, called via pthread_once() in CMSEncoderGetTypeID() */
static void cmsEncoderClassInitialize(void)
{
	cmsEncoderTypeID = 
		_CFRuntimeRegisterClass((const CFRuntimeClass * const)&cmsEncoderRuntimeClass);
}

/* init called out from _CFRuntimeCreateInstance() */
static void cmsEncoderInit(CFTypeRef enc)
{
	char *start = ((char *)enc) + sizeof(CFRuntimeBase);
	memset(start, 0, sizeof(struct _CMSEncoder) - sizeof(CFRuntimeBase));
}

/*
 * Dispose of a CMSEncoder. Called out from CFRelease().
 */
static void cmsEncoderFinalize(
	CFTypeRef		enc)
{
	CMSEncoderRef cmsEncoder = (CMSEncoderRef)enc;
	if(cmsEncoder == NULL) {
		return;
	}
	if(cmsEncoder->eContentType.Data != NULL) {
		free(cmsEncoder->eContentType.Data);
	}
	CFRELEASE(cmsEncoder->signers);
	CFRELEASE(cmsEncoder->recipients);
	CFRELEASE(cmsEncoder->otherCerts);
	if(cmsEncoder->cmsMsg != NULL) {
		SecCmsMessageDestroy(cmsEncoder->cmsMsg);
	}
	if(cmsEncoder->arena != NULL) {
		SecArenaPoolFree(cmsEncoder->arena, false);
	}
	if(cmsEncoder->encoder != NULL) {
		/* 
		 * Normally this gets freed in SecCmsEncoderFinish - this is 
		 * an error case.
		 */
		SecCmsEncoderDestroy(cmsEncoder->encoder);
	}
}

static OSStatus cmsSetupEncoder(
	CMSEncoderRef		cmsEncoder)
{
	OSStatus ortn;
	
	ASSERT(cmsEncoder->arena == NULL);
	ASSERT(cmsEncoder->encoder == NULL);
	
	ortn = SecArenaPoolCreate(1024, &cmsEncoder->arena);
	if(ortn) {
		return cmsRtnToOSStatus(ortn);
	}
	ortn = SecCmsEncoderCreate(cmsEncoder->cmsMsg, 
		NULL, NULL,					// no callback 
		&cmsEncoder->encoderOut,	// data goes here
		cmsEncoder->arena,	
		NULL, NULL,					// no password callback (right?) 
		NULL, NULL,					// decrypt key callback
		NULL, NULL,					// detached digests
		&cmsEncoder->encoder);
	if(ortn) {
		return cmsRtnToOSStatus(ortn);
	}
	return noErr;
}

/* 
 * Set up a SecCmsMessageRef for a SignedData creation.
 */
static OSStatus cmsSetupForSignedData(
	CMSEncoderRef		cmsEncoder)
{
	ASSERT((cmsEncoder->signers != NULL) || (cmsEncoder->otherCerts != NULL));
	
    SecCmsContentInfoRef contentInfo = NULL;
    SecCmsSignedDataRef signedData = NULL;
	OSStatus ortn;

    /* build chain of objects: message->signedData->data */
	if(cmsEncoder->cmsMsg != NULL) {
		SecCmsMessageDestroy(cmsEncoder->cmsMsg);
	}
	cmsEncoder->cmsMsg = SecCmsMessageCreate(NULL);
	if(cmsEncoder->cmsMsg == NULL) {
		return internalComponentErr;
	}

	signedData = SecCmsSignedDataCreate(cmsEncoder->cmsMsg);
	if(signedData == NULL) {
		return internalComponentErr;
	}
	contentInfo = SecCmsMessageGetContentInfo(cmsEncoder->cmsMsg);
	ortn = SecCmsContentInfoSetContentSignedData(cmsEncoder->cmsMsg, contentInfo, 
			signedData);
	if(ortn) {
		return cmsRtnToOSStatus(ortn);
	}
    contentInfo = SecCmsSignedDataGetContentInfo(signedData);
	if(cmsEncoder->eContentType.Data != NULL) {
		/* Override the default eContentType of id-data */
		ortn = SecCmsContentInfoSetContentOther(cmsEncoder->cmsMsg, 
			contentInfo, 
			NULL,		/* data - provided to encoder, not here */
			cmsEncoder->detachedContent,
			&cmsEncoder->eContentType);
	}
	else {
		ortn = SecCmsContentInfoSetContentData(cmsEncoder->cmsMsg, 
			contentInfo, 
			NULL, /* data - provided to encoder, not here */
			cmsEncoder->detachedContent);
	}
	if(ortn) {
		ortn = cmsRtnToOSStatus(ortn);
		CSSM_PERROR("SecCmsContentInfoSetContent*", ortn);
		return ortn;
	}

	/* optional 'global' (per-SignedData) certs */
	if(cmsEncoder->otherCerts != NULL) {
		ortn = SecCmsSignedDataAddCertList(signedData, cmsEncoder->otherCerts);
		if(ortn) {
			ortn = cmsRtnToOSStatus(ortn);
			CSSM_PERROR("SecCmsSignedDataAddCertList", ortn);
			return ortn;
		}
	}
	
	/* SignerInfos, one per signer */
	CFIndex numSigners = 0;
	if(cmsEncoder->signers != NULL) {
		/* this is optional...in case we're just creating a cert bundle */
		numSigners = CFArrayGetCount(cmsEncoder->signers);
	}
	CFIndex dex;
	SecKeychainRef ourKc = NULL;
	SecCertificateRef ourCert = NULL;
	SecCmsCertChainMode chainMode = SecCmsCMCertChain;

	switch(cmsEncoder->chainMode) {
		case kCMSCertificateNone:
			chainMode = SecCmsCMNone;
			break;
		case kCMSCertificateSignerOnly:
			chainMode = SecCmsCMCertOnly;
			break;
		case kCMSCertificateChainWithRoot:
			chainMode = SecCmsCMCertChainWithRoot;
			break;
		default:
			break;
	}
	for(dex=0; dex<numSigners; dex++) {
		SecCmsSignerInfoRef signerInfo;
		
		SecIdentityRef ourId = 
			(SecIdentityRef)CFArrayGetValueAtIndex(cmsEncoder->signers, dex);
		ortn = SecIdentityCopyCertificate(ourId, &ourCert);
		if(ortn) {
			CSSM_PERROR("SecIdentityCopyCertificate", ortn);
			break;
		}
		ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)ourCert, &ourKc);
		if(ortn) {
			CSSM_PERROR("SecKeychainItemCopyKeychain", ortn);
			break;
		}
		signerInfo = SecCmsSignerInfoCreate(cmsEncoder->cmsMsg, ourId, SEC_OID_SHA1);
		if (signerInfo == NULL) {
			ortn = internalComponentErr;
			break;
		}

		/* we want the cert chain included for this one */
		/* NOTE the usage parameter is currently unused by the SMIME lib */
		ortn = SecCmsSignerInfoIncludeCerts(signerInfo, chainMode, 
			certUsageEmailSigner);
		if(ortn) {
			ortn = cmsRtnToOSStatus(ortn);
			CSSM_PERROR("SecCmsSignerInfoIncludeCerts", ortn);
			break;
		}
		
		/* other options */
		if(cmsEncoder->signedAttributes & kCMSAttrSmimeCapabilities) {
			ortn = SecCmsSignerInfoAddSMIMECaps(signerInfo);
			if(ortn) {
				ortn = cmsRtnToOSStatus(ortn);
				CSSM_PERROR("SecCmsSignerInfoAddSMIMEEncKeyPrefs", ortn);
				break;
			}
		}
		if(cmsEncoder->signedAttributes & kCMSAttrSmimeEncryptionKeyPrefs) {
			ortn = SecCmsSignerInfoAddSMIMEEncKeyPrefs(signerInfo, ourCert, ourKc);
			if(ortn) {
				ortn = cmsRtnToOSStatus(ortn);
				CSSM_PERROR("SecCmsSignerInfoAddSMIMEEncKeyPrefs", ortn);
				break;
			}
		}
		if(cmsEncoder->signedAttributes & kCMSAttrSmimeMSEncryptionKeyPrefs) {
			ortn = SecCmsSignerInfoAddMSSMIMEEncKeyPrefs(signerInfo, ourCert, ourKc);
			if(ortn) {
				ortn = cmsRtnToOSStatus(ortn);
				CSSM_PERROR("SecCmsSignerInfoAddMSSMIMEEncKeyPrefs", ortn);
				break;
			}
		}
		if(cmsEncoder->signedAttributes & kCMSAttrSigningTime) {
			if (cmsEncoder->signingTime == 0)
				cmsEncoder->signingTime = CFAbsoluteTimeGetCurrent();
			ortn = SecCmsSignerInfoAddSigningTime(signerInfo, cmsEncoder->signingTime);
			if(ortn) {
				ortn = cmsRtnToOSStatus(ortn);
				CSSM_PERROR("SecCmsSignerInfoAddSigningTime", ortn);
				break;
			}
		}
		
		ortn = SecCmsSignedDataAddSignerInfo(signedData, signerInfo);
		if(ortn) {
			ortn = cmsRtnToOSStatus(ortn);
			CSSM_PERROR("SecCmsSignedDataAddSignerInfo", ortn);
			break;
		}

		CFRELEASE(ourKc);
		CFRELEASE(ourCert);
		ourKc = NULL;
		ourCert = NULL;
	}
	if(ortn) {
		CFRELEASE(ourKc);
		CFRELEASE(ourCert);
	}
	return ortn;
}

/* 
 * Set up a SecCmsMessageRef for a EnvelopedData creation.
 */
static OSStatus cmsSetupForEnvelopedData(
	CMSEncoderRef		cmsEncoder)
{
	ASSERT(cmsEncoder->op == EO_Encrypt);
	ASSERT(cmsEncoder->recipients != NULL);
	
    SecCmsContentInfoRef contentInfo = NULL;
    SecCmsEnvelopedDataRef envelopedData = NULL;
	SECOidTag algorithmTag;
    int keySize;
	OSStatus ortn;

	/*
	 * Find encryption algorithm...unfortunately we need a NULL-terminated array
	 * of SecCertificateRefs for this.
	 */
	CFIndex numCerts = CFArrayGetCount(cmsEncoder->recipients);
	CFIndex dex;
	SecCertificateRef *certArray = (SecCertificateRef *)malloc(
		(numCerts+1) * sizeof(SecCertificateRef));

	for(dex=0; dex<numCerts; dex++) {
		certArray[dex] = (SecCertificateRef)CFArrayGetValueAtIndex(
			cmsEncoder->recipients, dex);
	}
	certArray[numCerts] = NULL;
	ortn = SecSMIMEFindBulkAlgForRecipients(certArray, &algorithmTag, &keySize);
	free(certArray);
	if(ortn) {
		CSSM_PERROR("SecSMIMEFindBulkAlgForRecipients", ortn);
		return ortn;
	}
	
    /* build chain of objects: message->envelopedData->data */
	if(cmsEncoder->cmsMsg != NULL) {
		SecCmsMessageDestroy(cmsEncoder->cmsMsg);
	}
	cmsEncoder->cmsMsg = SecCmsMessageCreate(NULL);
	if(cmsEncoder->cmsMsg == NULL) {
		return internalComponentErr;
	}
	envelopedData = SecCmsEnvelopedDataCreate(cmsEncoder->cmsMsg, 
		algorithmTag, keySize);
	if(envelopedData == NULL) {
		return internalComponentErr;
	}
	contentInfo = SecCmsMessageGetContentInfo(cmsEncoder->cmsMsg);
	ortn = SecCmsContentInfoSetContentEnvelopedData(cmsEncoder->cmsMsg, 
		contentInfo, envelopedData);
	if(ortn) {
		ortn = cmsRtnToOSStatus(ortn);
		CSSM_PERROR("SecCmsContentInfoSetContentEnvelopedData", ortn);
		return ortn;
	}
    contentInfo = SecCmsEnvelopedDataGetContentInfo(envelopedData);
	if(cmsEncoder->eContentType.Data != NULL) {
		/* Override the default ContentType of id-data */
		ortn = SecCmsContentInfoSetContentOther(cmsEncoder->cmsMsg, 
			contentInfo, 
			NULL,		/* data - provided to encoder, not here */
			FALSE,		/* detachedContent */
			&cmsEncoder->eContentType);
	}
	else {
		ortn = SecCmsContentInfoSetContentData(cmsEncoder->cmsMsg, 
			contentInfo, 
			NULL /* data - provided to encoder, not here */, 
			cmsEncoder->detachedContent);
	}
	if(ortn) {
		ortn = cmsRtnToOSStatus(ortn);
		CSSM_PERROR("SecCmsContentInfoSetContentData*", ortn);
		return ortn;
	}

    /* 
     * create & attach recipient information, one for each recipient
     */
	for(dex=0; dex<numCerts; dex++) {
		SecCmsRecipientInfoRef recipientInfo = NULL;
		
		SecCertificateRef thisRecip = (SecCertificateRef)CFArrayGetValueAtIndex(
			cmsEncoder->recipients, dex);
		recipientInfo = SecCmsRecipientInfoCreate(cmsEncoder->cmsMsg, thisRecip);
		ortn = SecCmsEnvelopedDataAddRecipient(envelopedData, recipientInfo);
		if(ortn) {
			ortn = cmsRtnToOSStatus(ortn);
			CSSM_PERROR("SecCmsEnvelopedDataAddRecipient", ortn);
			return ortn;
		}
	}
	return noErr;
}

/* 
 * Set up cmsMsg. Called from either the first call to CMSEncoderUpdateContent, or
 * from CMSEncodeGetCmsMessage().
 */
static OSStatus cmsSetupCmsMsg(
	CMSEncoderRef		cmsEncoder)
{
	ASSERT(cmsEncoder != NULL);
	ASSERT(cmsEncoder->encState == ES_Init);
	
	/* figure out what high-level operation we're doing */
	if((cmsEncoder->signers != NULL) || (cmsEncoder->otherCerts != NULL)) {
		if(cmsEncoder->recipients != NULL) {
			cmsEncoder->op = EO_SignEncrypt;
		}
		else {
			cmsEncoder->op = EO_Sign;
		}
	}
	else if(cmsEncoder->recipients != NULL) {
		cmsEncoder->op = EO_Encrypt;
	}
	else {
		dprintf("CMSEncoderUpdateContent: nothing to do\n");
		return paramErr;
	}
	
	OSStatus ortn = noErr;
	
	switch(cmsEncoder->op) {
		case EO_Sign:
		case EO_SignEncrypt:
			/* If we're signing & encrypting, do the signing first */
			ortn = cmsSetupForSignedData(cmsEncoder);
			break;
		case EO_Encrypt:
			ortn = cmsSetupForEnvelopedData(cmsEncoder);
			break;
	}
	cmsEncoder->encState = ES_Msg;
	return ortn;
}

/* 
 * ASN.1 template for decoding a ContentInfo.
 */
typedef struct {
    CSSM_OID	contentType;
    CSSM_DATA	content;    
} SimpleContentInfo;

static const SecAsn1Template cmsSimpleContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SimpleContentInfo) },
    { SEC_ASN1_OBJECT_ID, offsetof(SimpleContentInfo, contentType) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0, 
	  offsetof(SimpleContentInfo, content),
	  kSecAsn1AnyTemplate },
    { 0, }
};

/*
 * Obtain the content of a contentInfo, This basically strips off the contentType OID
 * and returns its ASN_ANY content, allocated the provided coder's memory space. 
 */
static OSStatus cmsContentInfoContent(
	SecAsn1CoderRef asn1Coder,
	const CSSM_DATA *contentInfo,
	CSSM_DATA *content)				/* RETURNED */
{
    OSStatus ortn;
    SimpleContentInfo decodedInfo;
    
    memset(&decodedInfo, 0, sizeof(decodedInfo));
    ortn = SecAsn1DecodeData(asn1Coder, contentInfo, 
		cmsSimpleContentInfoTemplate, &decodedInfo);
    if(ortn) {
		return ortn;
    }
    if(decodedInfo.content.Data == NULL) {
		dprintf("***Error decoding contentInfo: no content\n");
		return internalComponentErr;
    }
    *content = decodedInfo.content;
	return noErr;
}

#pragma mark --- Start of Public API ---

CFTypeID CMSEncoderGetTypeID(void)
{
	static pthread_once_t once = PTHREAD_ONCE_INIT;
	
	if(cmsEncoderTypeID == _kCFRuntimeNotATypeID) {
		pthread_once(&once, &cmsEncoderClassInitialize);
	}
	return cmsEncoderTypeID;
}

/*
 * Create a CMSEncoder. Result must eventually be freed via CFRelease().
 */
OSStatus CMSEncoderCreate(
	CMSEncoderRef		*cmsEncoderOut)	/* RETURNED */
{
	CMSEncoderRef cmsEncoder = NULL;
	
	uint32_t extra = sizeof(*cmsEncoder) - sizeof(cmsEncoder->base);
	cmsEncoder = (CMSEncoderRef)_CFRuntimeCreateInstance(NULL, CMSEncoderGetTypeID(),
		extra, NULL);
	if(cmsEncoder == NULL) {
		return memFullErr;
	}
	cmsEncoder->encState = ES_Init;
	cmsEncoder->chainMode = kCMSCertificateChain;
	*cmsEncoderOut = cmsEncoder;
	return noErr;
}
	
#pragma mark --- Getters & Setters ---

/* 
 * Specify signers of the CMS message; implies that the message will be signed. 
 */
OSStatus CMSEncoderAddSigners(
	CMSEncoderRef		cmsEncoder,
	CFTypeRef			signerOrArray)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	if(cmsEncoder->encState != ES_Init) {
		return paramErr;
	}
	return cmsAppendToArray(signerOrArray, &cmsEncoder->signers, SecIdentityGetTypeID());
}

/*
 * Obtain an array of signers as specified in CMSEncoderSetSigners(). 
 */
OSStatus CMSEncoderCopySigners(
	CMSEncoderRef		cmsEncoder,
	CFArrayRef			*signers)
{
	if((cmsEncoder == NULL) || (signers == NULL)) {
		return paramErr;
	}
	if(cmsEncoder->signers != NULL) {
		CFRetain(cmsEncoder->signers);
	}
	*signers = cmsEncoder->signers;
	return noErr;
}

/*
 * Specify recipients of the message. Implies that the message will be encrypted. 
 */
OSStatus CMSEncoderAddRecipients(
	CMSEncoderRef		cmsEncoder,
	CFTypeRef			recipientOrArray)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	if(cmsEncoder->encState != ES_Init) {
		return paramErr;
	}
	return cmsAppendToArray(recipientOrArray, &cmsEncoder->recipients, 
			SecCertificateGetTypeID());
}

/*
 * Obtain an array of recipients as specified in CMSEncoderSetRecipients(). 
 */
OSStatus CMSEncoderCopyRecipients(
	CMSEncoderRef		cmsEncoder,
	CFArrayRef			*recipients)
{
	if((cmsEncoder == NULL) || (recipients == NULL)) {
		return paramErr;
	}
	if(cmsEncoder->recipients != NULL) {
		CFRetain(cmsEncoder->recipients);
	}
	*recipients = cmsEncoder->recipients;
	return noErr;
}

/* 
 * Specify additional certs to include in a signed message. 
 */
OSStatus CMSEncoderAddSupportingCerts(
	CMSEncoderRef		cmsEncoder,
	CFTypeRef			certOrArray)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	if(cmsEncoder->encState != ES_Init) {
		return paramErr;
	}
	return cmsAppendToArray(certOrArray, &cmsEncoder->otherCerts, 
			SecCertificateGetTypeID());
}
	
/*
 * Obtain the SecCertificates provided in CMSEncoderAddSupportingCerts(). 
 */
OSStatus CMSEncoderCopySupportingCerts(
	CMSEncoderRef		cmsEncoder,
	CFArrayRef			*certs)			/* RETURNED */
{
	if((cmsEncoder == NULL) || (certs == NULL)) {
		return paramErr;
	}
	if(cmsEncoder->otherCerts != NULL) {
		CFRetain(cmsEncoder->otherCerts);
	}
	*certs = cmsEncoder->otherCerts;
	return noErr;
}

OSStatus CMSEncoderSetHasDetachedContent(
	CMSEncoderRef		cmsEncoder,
	Boolean				detachedContent)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	if(cmsEncoder->encState != ES_Init) {
		return paramErr;
	}
	cmsEncoder->detachedContent = detachedContent;
	return noErr;
}

OSStatus CMSEncoderGetHasDetachedContent(
	CMSEncoderRef		cmsEncoder,
	Boolean				*detachedContent)	/* RETURNED */
{
	if((cmsEncoder == NULL) || (detachedContent == NULL)) {
		return paramErr;
	}
	*detachedContent = cmsEncoder->detachedContent;
	return noErr;
}

/*
 * Optionally specify an eContentType OID for the inner EncapsulatedData for
 * a signed message. The default eContentType, used of this function is not
 * called, is id-data. 
 */
OSStatus CMSEncoderSetEncapsulatedContentType(
	CMSEncoderRef		cmsEncoder,
	const CSSM_OID	*eContentType)
{
	if((cmsEncoder == NULL) || (eContentType == NULL)) {
		return paramErr;
	}
	if(cmsEncoder->encState != ES_Init) {
		return paramErr;
	}
	
	CSSM_OID *ecOid = &cmsEncoder->eContentType;
	if(ecOid->Data != NULL) {
		free(ecOid->Data);
	}
	cmsCopyCmsData(eContentType, ecOid);
	return noErr;
}

OSStatus CMSEncoderSetEncapsulatedContentTypeOID(
	CMSEncoderRef		cmsEncoder,
	CFTypeRef			eContentTypeOID)
{
	// convert eContentTypeOID to a CSSM_OID
	CSSM_OID contentType = { 0, NULL };
	if (!eContentTypeOID || convertOid(eContentTypeOID, &contentType) != 0)
		return paramErr;
	OSStatus result = CMSEncoderSetEncapsulatedContentType(cmsEncoder, &contentType);
	if (contentType.Data)
		free(contentType.Data);
	return result;
}

/*
 * Obtain the eContentType OID specified in CMSEncoderSetEncapsulatedContentType().
 */
OSStatus CMSEncoderCopyEncapsulatedContentType(
	CMSEncoderRef		cmsEncoder,
	CFDataRef			*eContentType)
{
	if((cmsEncoder == NULL) || (eContentType == NULL)) {
		return paramErr;
	}
	
	CSSM_OID *ecOid = &cmsEncoder->eContentType;
	if(ecOid->Data == NULL) {
		*eContentType = NULL;
	}
	else {
		*eContentType = CFDataCreate(NULL, ecOid->Data, ecOid->Length);
	}
	return noErr;
}

/*
 * Optionally specify signed attributes. Only meaningful when creating a 
 * signed message. If this is called, it must be called before
 * CMSEncoderUpdateContent().
 */
OSStatus CMSEncoderAddSignedAttributes(
	CMSEncoderRef		cmsEncoder,
	CMSSignedAttributes	signedAttributes)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	if(cmsEncoder->encState != ES_Init) {
		return paramErr;
	}
	cmsEncoder->signedAttributes = signedAttributes;
	return noErr;
}

/*
 * Set the signing time for a CMSEncoder.
 * This is only used if the kCMSAttrSigningTime attribute is included.
 */
OSStatus CMSEncoderSetSigningTime(
	CMSEncoderRef		cmsEncoder,
	CFAbsoluteTime		time)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	if(cmsEncoder->encState != ES_Init) {
		return paramErr;
	}
	cmsEncoder->signingTime = time;
	return noErr;
}


OSStatus CMSEncoderSetCertificateChainMode(
	CMSEncoderRef			cmsEncoder,
	CMSCertificateChainMode	chainMode)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	if(cmsEncoder->encState != ES_Init) {
		return paramErr;
	}
	switch(chainMode) {
		case kCMSCertificateNone:
		case kCMSCertificateSignerOnly:
		case kCMSCertificateChain:
		case kCMSCertificateChainWithRoot:
			break;
		default:
			return paramErr;
	}
	cmsEncoder->chainMode = chainMode;
	return noErr;
}

OSStatus CMSEncoderGetCertificateChainMode(
	CMSEncoderRef			cmsEncoder,
	CMSCertificateChainMode	*chainModeOut)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	*chainModeOut = cmsEncoder->chainMode;
	return noErr;
}

void
CmsMessageSetTSACallback(CMSEncoderRef cmsEncoder, SecCmsTSACallback tsaCallback)
{
    if (cmsEncoder->cmsMsg)
        SecCmsMessageSetTSACallback(cmsEncoder->cmsMsg, tsaCallback);
}

void
CmsMessageSetTSAContext(CMSEncoderRef cmsEncoder, CFTypeRef tsaContext)
{
    if (cmsEncoder->cmsMsg)
        SecCmsMessageSetTSAContext(cmsEncoder->cmsMsg, tsaContext);
}

#pragma mark --- Action ---

/*
 * Feed content bytes into the encoder. 
 * Can be called multiple times. 
 * No 'setter' routines can be called after this function has been called. 
 */ 
OSStatus CMSEncoderUpdateContent(
	CMSEncoderRef		cmsEncoder,
	const void			*content,
	size_t				contentLen)
{
	if(cmsEncoder == NULL) {
		return paramErr;
	}
	
	OSStatus ortn = noErr;
	switch(cmsEncoder->encState) {
		case ES_Init:
			/* 
			 * First time thru: do the CmsMsg setup.
			 */
			ortn = cmsSetupCmsMsg(cmsEncoder);
			if(ortn) {
				return ortn;
			}
			/* fall thru to set up the encoder */
			
		case ES_Msg:
			/* We have a cmsMsg but no encoder; create one */
			ASSERT(cmsEncoder->cmsMsg != NULL);
			ASSERT(cmsEncoder->encoder == NULL);
			ortn = cmsSetupEncoder(cmsEncoder);
			if(ortn) {
				return ortn;
			}
			/* only legal calls now are update and finalize */
			cmsEncoder->encState = ES_Updating;
			break;
			
		case ES_Updating:
			ASSERT(cmsEncoder->encoder != NULL);
			break;

		case ES_Final:
			/* Too late for another update */
			return paramErr;
			
		default:
			return internalComponentErr;
	}
	
	/* FIXME - CFIndex same size as size_t on 64bit? */
	ortn = SecCmsEncoderUpdate(cmsEncoder->encoder, content, (CFIndex)contentLen);
	if(ortn) {
		ortn = cmsRtnToOSStatus(ortn);
		CSSM_PERROR("SecCmsEncoderUpdate", ortn);
	}
	return ortn;
}
	
/*
 * Finish encoding the message and obtain the encoded result.
 * Caller must CFRelease the result. 
 */
OSStatus CMSEncoderCopyEncodedContent(
	CMSEncoderRef		cmsEncoder,
	CFDataRef			*encodedContent)
{
	if((cmsEncoder == NULL) || (encodedContent == NULL)) {
		return paramErr;
	}

	OSStatus ortn;

	switch(cmsEncoder->encState) {
		case ES_Updating:
			/* normal termination */
			break;
		case ES_Final:
			/* already been called */
			return paramErr;
		case ES_Msg:
		case ES_Init:
			/*
			 * The only time these are legal is when we're doing a SignedData
			 * with certificates only (no signers, no content).
			 */
			if((cmsEncoder->signers != NULL) ||
			   (cmsEncoder->recipients != NULL) ||
			   (cmsEncoder->otherCerts == NULL)) {
				return paramErr;
			}
			
			/* Set up for certs only */
			ortn = cmsSetupForSignedData(cmsEncoder);
			if(ortn) {
				return ortn;
			}
			/* and an encoder */
			ortn = cmsSetupEncoder(cmsEncoder);
			if(ortn) {
				return ortn;
			}
			break;
	}
	
	
	ASSERT(cmsEncoder->encoder != NULL);
	ortn = SecCmsEncoderFinish(cmsEncoder->encoder);
	/* regardless of the outcome, the encoder itself has been freed */
	cmsEncoder->encoder = NULL;
	if(ortn) {
		return cmsRtnToOSStatus(ortn);
	}
	cmsEncoder->encState = ES_Final;

	if((cmsEncoder->encoderOut.Data == NULL) && !cmsEncoder->customCoder) {
		/* not sure how this could happen... */
		dprintf("Successful encode, but no data\n");
		return internalComponentErr;
	}
	if(cmsEncoder->customCoder) {
		/* we're done */
		*encodedContent = NULL;
		return noErr;
	}
	
	/* in two out of three cases, we're done */
	switch(cmsEncoder->op) {
		case EO_Sign:
		case EO_Encrypt:
			*encodedContent = CFDataCreate(NULL, (const UInt8 *)cmsEncoder->encoderOut.Data,	
				cmsEncoder->encoderOut.Length);
			return noErr;
		case EO_SignEncrypt:
			/* proceed, more work to do */
			break;
	}
	
	/* 
	 * Signing & encrypting.
	 * Due to bugs in the libsecurity_smime encoder, it can't encode nested 
	 * ContentInfos in one shot. So we do another pass, specifying the SignedData
	 * inside of the ContentInfo we just created as the data to encrypt.
	 */
	SecAsn1CoderRef asn1Coder = NULL;
	CSSM_DATA signedData = {0, NULL};

	ortn = SecAsn1CoderCreate(&asn1Coder);
	if(ortn) {
		return ortn;
	}
	ortn = cmsContentInfoContent(asn1Coder, &cmsEncoder->encoderOut, &signedData);
	if(ortn) {
		goto errOut;
	}
	
	/* now just encrypt that, one-shot */
	ortn = CMSEncode(NULL,			/* no signers this time */
		cmsEncoder->recipients,
		&CSSMOID_PKCS7_SignedData,	/* fake out encoder so it doesn't try to actually
									 *   encode the signedData - this asserts the
									 *   SEC_OID_OTHER OID tag in the EnvelopedData's
									 *   ContentInfo */
		FALSE,						/* detachedContent */
		kCMSAttrNone,				/* signedAttributes - none this time */
		signedData.Data, signedData.Length,
		encodedContent);

errOut:
	if(asn1Coder) {
		SecAsn1CoderRelease(asn1Coder);
	}
	return ortn;
}
	
#pragma mark --- High-level API ---

/*
 * High-level, one-shot encoder function.
 */
OSStatus CMSEncode(
	CFTypeRef			signers,
	CFTypeRef			recipients,
	const CSSM_OID		*eContentType,
	Boolean				detachedContent,
	CMSSignedAttributes	signedAttributes,
	const void			*content,
	size_t				contentLen,
	CFDataRef			*encodedContent)	/* RETURNED */
{
	if((signers == NULL) && (recipients == NULL)) {
		return paramErr;
	}
	if(encodedContent == NULL) {
		return paramErr;
	}
	
	CMSEncoderRef cmsEncoder;
	OSStatus ortn;
	
	/* set up the encoder */
	ortn = CMSEncoderCreate(&cmsEncoder);
	if(ortn) {
		return ortn;
	}
	
	/* subsequent errors to errOut: */
	if(signers) {
		ortn = CMSEncoderAddSigners(cmsEncoder, signers);
		if(ortn) {
			goto errOut;
		}
	}
	if(recipients) {
		ortn = CMSEncoderAddRecipients(cmsEncoder, recipients);
		if(ortn) {
			goto errOut;
		}
	}
	if(eContentType) {
		ortn = CMSEncoderSetEncapsulatedContentType(cmsEncoder, eContentType);
		if(ortn) {
			goto errOut;
		}
	}
	if(detachedContent) {
		ortn = CMSEncoderSetHasDetachedContent(cmsEncoder, detachedContent);
		if(ortn) {
			goto errOut;
		}
	}
	if(signedAttributes) {
		ortn = CMSEncoderAddSignedAttributes(cmsEncoder, signedAttributes);
		if(ortn) {
			goto errOut;
		}
	}
	/* GO */
	ortn = CMSEncoderUpdateContent(cmsEncoder, content, contentLen);
	if(ortn) {
		goto errOut;
	}
	ortn = CMSEncoderCopyEncodedContent(cmsEncoder, encodedContent);
	
errOut:
	CFRelease(cmsEncoder);
	return ortn;
}

OSStatus CMSEncodeContent(
	CFTypeRef			signers,
	CFTypeRef			recipients,
	CFTypeRef			eContentTypeOID,
	Boolean				detachedContent,
	CMSSignedAttributes	signedAttributes,
	const void			*content,
	size_t				contentLen,
	CFDataRef			*encodedContentOut)	/* RETURNED */
{
	// convert eContentTypeOID to a CSSM_OID
	CSSM_OID contentType = { 0, NULL };
	if (eContentTypeOID && convertOid(eContentTypeOID, &contentType) != 0)
		return paramErr;
	const CSSM_OID *contentTypePtr = (eContentTypeOID) ? &contentType : NULL;
	OSStatus result = CMSEncode(signers, recipients, contentTypePtr,
									detachedContent, signedAttributes,
									content, contentLen, encodedContentOut);
	if (contentType.Data)
		free(contentType.Data);
	return result;
}

#pragma mark --- SPI routines declared in CMSPrivate.h ---

/*
 * Obtain the SecCmsMessageRef associated with a CMSEncoderRef. 
 * If we don't have a SecCmsMessageRef yet, we create one now.
 * This is the only place where we go to state ES_Msg. 
 */
OSStatus CMSEncoderGetCmsMessage(
	CMSEncoderRef		cmsEncoder,
	SecCmsMessageRef	*cmsMessage)		/* RETURNED */
{
	if((cmsEncoder == NULL) || (cmsMessage == NULL)) {
		return paramErr;
	}
	if(cmsEncoder->cmsMsg != NULL) {
		ASSERT(cmsEncoder->encState != ES_Init);
		*cmsMessage = cmsEncoder->cmsMsg;
		return noErr;
	}

	OSStatus ortn = cmsSetupCmsMsg(cmsEncoder);
	if(ortn) {
		return ortn;
	}
	*cmsMessage = cmsEncoder->cmsMsg;
	
	/* Don't set up encoder yet; caller might do that via CMSEncoderSetEncoder */
	cmsEncoder->encState = ES_Msg;
	return noErr;
}

/* 
 * Optionally specify a SecCmsEncoderRef to use with a CMSEncoderRef.
 * If this is called, it must be called before the first call to 
 * CMSEncoderUpdateContent(). The CMSEncoderRef takes ownership of the
 * incoming SecCmsEncoderRef.
 */
OSStatus CMSEncoderSetEncoder(
	CMSEncoderRef		cmsEncoder,
	SecCmsEncoderRef	encoder)
{
	if((cmsEncoder == NULL) || (encoder == NULL)) {
		return paramErr;
	}
	
	OSStatus ortn;
	
	switch(cmsEncoder->encState) {
		case ES_Init:
			/* No message, no encoder */
			ASSERT(cmsEncoder->cmsMsg == NULL);
			ASSERT(cmsEncoder->encoder == NULL);
			ortn = cmsSetupCmsMsg(cmsEncoder);
			if(ortn) {
				return ortn;
			}
			/* drop thru to set encoder */
		case ES_Msg:
			/* cmsMsg but no encoder */
			ASSERT(cmsEncoder->cmsMsg != NULL);
			ASSERT(cmsEncoder->encoder == NULL);
			cmsEncoder->encoder = encoder;
			cmsEncoder->encState = ES_Updating;
			cmsEncoder->customCoder = true;			/* we won't see data */
			return noErr;
		default:
			/* no can do, too late */
			return paramErr;
	}
}
	
/* 
 * Obtain the SecCmsEncoderRef associated with a CMSEncoderRef. 
 * Returns a NULL SecCmsEncoderRef if neither CMSEncoderSetEncoder nor
 * CMSEncoderUpdateContent() has been called. 
 * The CMSEncoderRef retains ownership of the SecCmsEncoderRef.
 */
OSStatus CMSEncoderGetEncoder(
	CMSEncoderRef		cmsEncoder,
	SecCmsEncoderRef	*encoder)			/* RETURNED */
{
	if((cmsEncoder == NULL) || (encoder == NULL)) {
		return paramErr;
	}
	
	/* any state, whether we have an encoder or not is OK */
	*encoder = cmsEncoder->encoder;
	return noErr;
}

#include <AssertMacros.h>

/*
 * Obtain the timestamp of signer 'signerIndex' of a CMS message, if
 * present. This timestamp is an authenticated timestamp provided by
 * a timestamping authority.
 *
 * Returns paramErr if the CMS message was not signed or if signerIndex
 * is greater than the number of signers of the message minus one. 
 *
 * This cannot be called until after CMSEncoderCopyEncodedContent() is called. 
 */
OSStatus CMSEncoderCopySignerTimestamp(
	CMSEncoderRef		cmsEncoder,
	size_t				signerIndex,        /* usually 0 */
	CFAbsoluteTime      *timestamp)			/* RETURNED */
{
    OSStatus status = paramErr;
	SecCmsMessageRef cmsg;
	SecCmsSignedDataRef signedData = NULL;
    int numContentInfos = 0;

    require(cmsEncoder && timestamp, xit);
	require_noerr(CMSEncoderGetCmsMessage(cmsEncoder, &cmsg), xit);
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
