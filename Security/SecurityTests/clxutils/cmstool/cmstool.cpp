/*
 * cmstool.cpp - manipluate CMS messages, intended to be an alternate for the 
 *		 currently useless cms command in /usr/bin/security
 */
 
#include <Security/Security.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utilLib/common.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <clAppUtils/identPicker.h>
#include <clAppUtils/sslAppUtils.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <Security/SecCmsEncoder.h>
#include <Security/SecCmsDecoder.h>
#include <Security/SecCmsEncryptedData.h>
#include <Security/SecCmsEnvelopedData.h>
#include <Security/SecCmsMessage.h>
#include <Security/SecCmsRecipientInfo.h>
#include <Security/SecCmsSignedData.h>
#include <Security/SecCmsSignerInfo.h>
#include <Security/SecCmsContentInfo.h>
#include <Security/SecCmsDigestContext.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecKeychainItemPriv.h>
#include <Security/SecCertificate.h>
#include <Security/SecSMIME.h>
#include <Security/oidsattr.h>
#include <Security/SecAsn1Coder.h>
#include <Security/secasn1t.h>
#include <Security/SecAsn1Templates.h>

static void usage(char **argv)
{
    printf("Usage: %s cmd [option ...]\n", argv[0]);
	printf("cmd values:\n");
	printf("   sign       -- create signedData\n");
	printf("   envel      -- create envelopedData\n");
	printf("   signEnv    -- create nested EnvelopedData(signedData(data))\n");
	printf("   parse      -- parse a CMS message file\n");
	printf("Options:\n");
	printf("   -i infile\n");
	printf("   -o outfile\n");
	printf("   -k keychain        -- Keychain to search for certs\n");
	printf("   -p                 -- Use identity picker\n");
	printf("   -r recipient       -- specify recipient of enveloped data\n");
	printf("   -c                 -- parse signer cert\n");
	printf("   -v sign|encr       -- verify message is signed/encrypted\n");
	printf("   -e eContentType    -- a(uthData)|r(keyData)\n");
	printf("   -d detached        -- infile contains detached content (sign only)\n");
	printf("   -D detachedContent -- detached content (parse only)\n");
	printf("   -q                 -- quiet\n");
	exit(1);
}

/* high level op */
typedef enum {
	CTO_Sign,
	CTO_Envelop,
	CTO_SignEnvelop,
	CTO_Parse
} CT_Op;

/* to verify */
typedef enum {
	CTV_None,
	CTV_Sign,
	CTV_Envelop,
	CTV_SignEnvelop
} CT_Vfy;

/* additional OIDS to specify as eContentType */
#define OID_PKINIT	0x2B, 6, 1, 5, 2, 3
#define OID_PKINIT_LEN	6

static const uint8	OID_PKINIT_AUTH_DATA[]	    = {OID_PKINIT, 1};
static const uint8	OID_PKINIT_DH_KEY_DATA[]    = {OID_PKINIT, 2};
static const uint8	OID_PKINIT_RKEY_DATA[]	    = {OID_PKINIT, 3};
static const uint8	OID_PKINIT_KP_CLIENTAUTH[]  = {OID_PKINIT, 3};
static const uint8	OID_PKINIT_KPKDC[]			= {OID_PKINIT, 5};

static const CSSM_OID	CSSMOID_PKINIT_AUTH_DATA = 
	{OID_PKINIT_LEN+1, (uint8 *)OID_PKINIT_AUTH_DATA};
static const CSSM_OID	CSSMOID_PKINIT_DH_KEY_DATA =
	{OID_PKINIT_LEN+1, (uint8 *)OID_PKINIT_DH_KEY_DATA};
static const CSSM_OID	CSSMOID_PKINIT_RKEY_DATA = 
	{OID_PKINIT_LEN+1, (uint8 *)OID_PKINIT_RKEY_DATA};
static const CSSM_OID	CSSMOID_PKINIT_KP_CLIENTAUTH =
	{OID_PKINIT_LEN+1, (uint8 *)OID_PKINIT_KP_CLIENTAUTH};
static const CSSM_OID	CSSMOID_PKINIT_KPKDC = 
	{OID_PKINIT_LEN+1, (uint8 *)OID_PKINIT_KPKDC};

typedef struct {
    CSSM_OID	contentType;
    CSSM_DATA	content;    
} SimpleContentInfo;

const SecAsn1Template SimpleContentInfoTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SimpleContentInfo) },
    { SEC_ASN1_OBJECT_ID, offsetof(SimpleContentInfo, contentType) },
    { SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | SEC_ASN1_CONTEXT_SPECIFIC | 0, 
	  offsetof(SimpleContentInfo, content),
	  kSecAsn1AnyTemplate },
    { 0, }
};

/*
 * Obtain the content of a contentInfo, This basically strips off the contentType OID
 * and returns a mallocd copy of the ASN_ANY content.
 */
static OSStatus ContentInfoContent(
    const unsigned char *contentInfo,
    unsigned contentInfoLen,
    unsigned char **content,	    /* mallocd and RETURNED */
    unsigned *contentLen)	    /* RETURNED */
{
    SecAsn1CoderRef coder = NULL;
    OSStatus ortn;
    SimpleContentInfo decodedInfo;
    
    ortn = SecAsn1CoderCreate(&coder);
    if(ortn) {
		return ortn;
    }
    memset(&decodedInfo, 0, sizeof(decodedInfo));
    ortn = SecAsn1Decode(coder, contentInfo, contentInfoLen, 
		SimpleContentInfoTemplate, &decodedInfo);
    if(ortn) {
		goto errOut;
    }
    if(decodedInfo.content.Data == NULL) {
	printf("***Error decoding contentInfo: no content\n");
	ortn = internalComponentErr;
		goto errOut;
    }
    *content = (unsigned char *)malloc(decodedInfo.content.Length);
    memmove(*content, decodedInfo.content.Data, decodedInfo.content.Length);
    *contentLen = decodedInfo.content.Length;
errOut:
    SecAsn1CoderRelease(coder);
    return ortn;
}

/*
 * Find a cert in specified keychain or keychain list matching specified 
 * email address. We happen to knopw that the email address is stored with the 
 * kSecKeyAlias attribute. 
 */
static OSStatus findCert(
	const char *emailAddress,
	CFTypeRef kcArArray,			// kc, array, or even NULL
	SecCertificateRef *cert)
{
	OSStatus					ortn;
	SecKeychainSearchRef		srch;
	SecKeychainAttributeList	attrList;
	SecKeychainAttribute		attr;
	
	attr.tag = kSecKeyAlias;
	attr.length = strlen(emailAddress);
	attr.data = (void *)emailAddress;
	attrList.count = 1;
	attrList.attr = &attr;
	
	ortn = SecKeychainSearchCreateFromAttributes(kcArArray,
		kSecCertificateItemClass,
		&attrList,
		&srch);
	if(ortn) {
		cssmPerror("SecKeychainSearchCreateFromAttributes", ortn);
		return ortn;
	}
	
	ortn = SecKeychainSearchCopyNext(srch, (SecKeychainItemRef *)cert);
	if(ortn) {
		printf("***No certs founmd matching recipient %s. Aborting.\n",
			emailAddress);
		return ortn;
	}
	CFRelease(srch);
	return noErr;
}

static void evalSecTrust(
	SecTrustRef secTrust,
	bool quiet)
{
	OSStatus ortn;
	SecTrustResultType			secTrustResult;
	
	ortn = SecTrustEvaluate(secTrust, &secTrustResult);
	if(ortn) {
		/* should never happen */
		cssmPerror("SecTrustEvaluate", ortn);
		return;
	}
	switch(secTrustResult) {
		case kSecTrustResultUnspecified:
			/* cert chain valid, no special UserTrust assignments */
		case kSecTrustResultProceed:
			/* cert chain valid AND user explicitly trusts this */
			if(!quiet) {
				fprintf(stderr, "Successful\n");
			}
			return;
		case kSecTrustResultDeny:
		case kSecTrustResultConfirm:
			/*
			 * Cert chain may well have verified OK, but user has flagged
			 * one of these certs as untrustable.
			 */
			printf("Not trusted per user-specified Trust level\n");
			return;
		default:
		{
			/* get low-level TP error */
			OSStatus tpStatus;
			ortn = SecTrustGetCssmResultCode(secTrust, &tpStatus);
			if(ortn) {
				cssmPerror("SecTrustGetCssmResultCode", ortn);
				return;
			}
			switch(tpStatus) {
				case CSSMERR_TP_INVALID_ANCHOR_CERT: 
					fprintf(stderr, "Untrusted root\n");
					return;
				case CSSMERR_TP_NOT_TRUSTED:
					/* no root, not even in implicit SSL roots */
					fprintf(stderr, "No root cert found\n");
					return;
				case CSSMERR_TP_CERT_EXPIRED:
					fprintf(stderr, "Expired cert\n");
					return;
				case CSSMERR_TP_CERT_NOT_VALID_YET:
					fprintf(stderr, "Cert not valid yet\n");
					break;
				default:
					printf("Other cert failure: ");
					cssmPerror("", tpStatus);
					return;
			}
		}
	} 	/* SecTrustEvaluate error */

}
static OSStatus parseSignedData(
	SecCmsSignedDataRef		signedData,
	SecArenaPoolRef			arena,			/* used for detached content only */
	const unsigned char		*detachedData,
	unsigned				detachedDataLen,
	CT_Vfy					vfyOp,
	bool					quiet,
	bool					parseSignerCert)
{
	Boolean b; 
	b = SecCmsSignedDataHasDigests(signedData);
	if(!quiet) {
		printf("      has digests   : %s\n", b ? "true" : "false");
	}
	
	SecTrustRef secTrust = NULL;
	OSStatus ortn;
	SecPolicyRef policy = NULL;
	SecPolicySearchRef policySearch = NULL;
	
	ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
		&CSSMOID_APPLE_X509_BASIC,
		NULL,
		&policySearch);
	if(ortn) {
		cssmPerror("SecPolicySearchCreate", ortn);
		return ortn;
	}
	ortn = SecPolicySearchCopyNext(policySearch, &policy);
	if(ortn) {
		cssmPerror("SecPolicySearchCopyNext", ortn);
		return ortn;
	}
	
	int numSigners = SecCmsSignedDataSignerInfoCount(signedData);
	if(!quiet) {
		printf("      num signers   : %d\n", numSigners);
	}
	for(int dex=0; dex<numSigners; dex++) {
		if(!quiet) {
			fprintf(stderr, "      signer %d      :\n", dex);
			fprintf(stderr, "         vfy status : ");
		}
		Boolean b = SecCmsSignedDataHasDigests(signedData);
		if(b) {
			if(detachedData != NULL) {
				fprintf(stderr, "<provided detachedContent, but msg has digests> ");
				/* FIXME - does this make sense? Error? */
			}
		}
		else if(detachedData != NULL) {
			/* digest the detached content */
			SECAlgorithmID **digestAlgorithms = SecCmsSignedDataGetDigestAlgs(signedData);
			SecCmsDigestContextRef digcx = SecCmsDigestContextStartMultiple(digestAlgorithms);
			CSSM_DATA **digests = NULL;
			
			SecCmsDigestContextUpdate(digcx, detachedData, detachedDataLen);
			ortn = SecCmsDigestContextFinishMultiple(digcx, arena, &digests);
			if(ortn) {
				fprintf(stderr, "SecCmsDigestContextFinishMultiple() returned %d\n", (int)ortn);
			}
			else {
				SecCmsSignedDataSetDigests(signedData, digestAlgorithms, digests);
			}
		}
		else {
			fprintf(stderr, "<Msg has no digest: need detachedContent> ");
		}
		ortn = SecCmsSignedDataVerifySignerInfo(signedData, dex, NULL, 
			policy, &secTrust);
		if(ortn) {
			fprintf(stderr, "vfSignerInfo() returned %d\n", (int)ortn);
			fprintf(stderr, "         vfy status : ");
		}
		if(secTrust == NULL) {
			fprintf(stderr, "***NO SecTrust available!\n");
		}
		else {
			evalSecTrust(secTrust, quiet);
		}

		SecCmsSignerInfoRef signerInfo = SecCmsSignedDataGetSignerInfo(signedData, dex);
		CFStringRef emailAddrs = SecCmsSignerInfoGetSignerCommonName(signerInfo);
		char emailStr[1000];
		if(!quiet) {
			fprintf(stderr, "         signer     : ");
		}
		if(emailAddrs == NULL) {
			fprintf(stderr, "<<SecCmsSignerInfoGetSignerCommonName returned NULL)>>\n");
		}
		else {
			if(!CFStringGetCString(emailAddrs, emailStr, 1000, kCFStringEncodingASCII)) {
				fprintf(stderr, "*** Error converting email address to C string\n");
			}
			else if(!quiet) {
				
				fprintf(stderr, "%s\n", emailStr);
			}
		}
		if(parseSignerCert) {
			SecCertificateRef signer;
			signer = SecCmsSignerInfoGetSigningCertificate(signerInfo, NULL);
			if(signer) {
				CSSM_DATA certData;
				ortn = SecCertificateGetData(signer, &certData);
				if(ortn) {
					fprintf(stderr, "***Error getting signing cert data***\n");
					cssmPerror("SecCertificateGetData", ortn);
				}
				else {
					printf("========== Signer Cert==========\n\n");
					printCert(certData.Data, certData.Length, CSSM_FALSE);
					printf("========== End Signer Cert==========\n\n");
				}
			}
			else {
				fprintf(stderr, "***Error getting signing cert ***\n");
			}
		}
	}
	return ortn;
}

static OSStatus doParse(
	const unsigned char *data,
	unsigned			dataLen,
	const unsigned char *detachedData,
	unsigned			detachedDataLen,
	CT_Vfy				vfyOp,
	bool				parseSignerCert,
	bool				quiet,
	unsigned char		**outData,		// mallocd and RETURNED
	unsigned			*outDataLen)	// RETURNED
{
	if((data == NULL) || (dataLen == 0)) {
		fprintf(stderr, "***Parse requires input file. Aborting.\n");
		return paramErr;
	}
	
	SecArenaPoolRef arena = NULL;
	SecArenaPoolCreate(1024, &arena);
	SecCmsMessageRef cmsMsg = NULL;
	SecCmsDecoderRef decoder;
	OSStatus ortn;
	OSStatus ourRtn = noErr;
	bool foundOneSigned = false;
	bool foundOneEnveloped = false;
	
	ortn = SecCmsDecoderCreate(arena, NULL, NULL, NULL, NULL, NULL, NULL, &decoder);
	if(ortn) {
		cssmPerror("SecCmsDecoderCreate", ortn);
		return ortn;
	}
	ortn = SecCmsDecoderUpdate(decoder, data, dataLen);
	if(ortn) {
		cssmPerror("SecCmsDecoderUpdate", ortn);
		return ortn;
	}
	ortn = SecCmsDecoderFinish(decoder, &cmsMsg);
	if(ortn) {
		cssmPerror("SecCmsDecoderFinish", ortn);
		return ortn;
	}
	
	Boolean b = SecCmsMessageIsSigned(cmsMsg);
	switch(vfyOp) {
		case CTV_None:
			break;
		case CTV_Sign: 
			if(!b) {
				fprintf(stderr, "***Expected SignedData, but !SecCmsMessageIsSigned()\n");
				ourRtn = -1;
			}
			break;
		case CTV_SignEnvelop:
			if(!b) {
				fprintf(stderr, "***Expected Signed&Enveloped, but !SecCmsMessageIsSigned()\n");
				ourRtn = -1;
			}
			break;
		case CTV_Envelop:
			if(b) {
				fprintf(stderr, "***Expected EnvelopedData, but SecCmsMessageIsSigned() "
						"TRUE\n");
				ourRtn = -1;
			}
			break;
	}
	int numContentInfos = SecCmsMessageContentLevelCount(cmsMsg);
	if(!quiet) {
		fprintf(stderr, "=== CMS message info ===\n");
		fprintf(stderr, "   Signed           : %s\n", b ? "true" : "false");
		b = SecCmsMessageIsEncrypted(cmsMsg);
		fprintf(stderr, "   Encrypted        : %s\n", b ? "true" : "false");
		b = SecCmsMessageContainsCertsOrCrls(cmsMsg);
		fprintf(stderr, "   certs/crls       : %s\n", b ? "present" : "not present");
		fprintf(stderr, "   Num ContentInfos : %d\n", numContentInfos);
	}
	
	/* FIXME needs work for CTV_SignEnvelop */
	OidParser oidParser;
	for(int dex=0; dex<numContentInfos; dex++) {
		SecCmsContentInfoRef ci = SecCmsMessageContentLevel(cmsMsg, dex);
		if(!quiet) {
			/* can't use stderr - oidparser is fixed w/stdout */
			printf("   Content Info %d   :\n", dex);
			CSSM_OID *typeOid = SecCmsContentInfoGetContentTypeOID(ci);
			printf("      OID Tag       : ");
			if(typeOid == NULL) {
				printf("***NONE FOUND***]n");
			}
			else if(typeOid->Length == 0) {
				printf("***EMPTY***\n");
			}
			else {
				char str[OID_PARSER_STRING_SIZE];
				oidParser.oidParse(typeOid->Data, typeOid->Length, str);
				printf("%s\n", str);
			}
		}
		SECOidTag tag = SecCmsContentInfoGetContentTypeTag(ci);
		switch(tag) {
			case SEC_OID_PKCS7_SIGNED_DATA:
			{
				switch(vfyOp) {
					case CTV_None:		// caller doesn't care
					case CTV_Sign:		// got what we wanted
						break;
					case CTV_Envelop:
						fprintf(stderr, "***Expected EnvelopedData, got SignedData\n");
						ourRtn = -1;
						break;
					case CTV_SignEnvelop:
						printf("CTV_SignEnvelop code on demand\n");
						break;
				}
				foundOneSigned = true;
				SecCmsSignedDataRef sd = 
					(SecCmsSignedDataRef) SecCmsContentInfoGetContent(ci);
				parseSignedData(sd, arena, 
					detachedData, detachedDataLen,
					vfyOp, quiet, parseSignerCert);
				break;
			}
			case SEC_OID_PKCS7_DATA:
			case SEC_OID_OTHER:
			    break;
			case SEC_OID_PKCS7_ENVELOPED_DATA:
				foundOneEnveloped = true;
				if(vfyOp == CTV_Sign) {
					fprintf(stderr, "***Expected SignedData, EnvelopedData\n");
					ourRtn = -1;
					break;
				}
			case SEC_OID_PKCS7_ENCRYPTED_DATA:
				switch(vfyOp) {
					case CTV_None:
						break;
					case CTV_Sign:
						fprintf(stderr, "***Expected SignedData, got EncryptedData\n");
						ourRtn = -1;
						break;
					case CTV_Envelop:
						fprintf(stderr, "***Expected EnvelopedData, got EncryptedData\n");
						ourRtn = -1;
						break;
					case CTV_SignEnvelop:
						printf("CTV_SignEnvelop code on demand\n");
						break;
				}
				break;
			default:
				fprintf(stderr, "      other content type TBD\n");
		}
	}
	if(outData) {
		CSSM_DATA_PTR odata = SecCmsMessageGetContent(cmsMsg);
		if(odata == NULL) {
			fprintf(stderr, "***No inner content available\n");
		}
		else {
			*outData = (unsigned char *)malloc(odata->Length);
			memmove(*outData, odata->Data, odata->Length);
			*outDataLen = odata->Length;
		}
	}
	if(arena) {
		SecArenaPoolFree(arena, false);
	}
	switch(vfyOp) {
		case CTV_None:
			break;
		case CTV_Sign:
			if(!foundOneSigned) {
				fprintf(stderr, "Expected signed, never saw a SignedData\n");
				ourRtn = -1;
			}
			break;
		case CTV_Envelop:
			if(!foundOneEnveloped) {
				fprintf(stderr, "Expected enveloped, never saw an EnvelopedData\n");
				ourRtn = -1;
			}
			break;
		case CTV_SignEnvelop:
			if(!foundOneSigned) {
				fprintf(stderr, "Expected signed, never saw a SignedData\n");
				ourRtn = -1;
			}
			if(!foundOneEnveloped) {
				fprintf(stderr, "Expected enveloped, never saw an EnvelopedData\n");
				ourRtn = -1;
			}
			break;
	}
	/* free decoder? cmsMsg? */
	return ourRtn;
}

/*
 * Common encode routine.
 */
#if 1
/* the simple way, when 3655861 is fixed */
static OSStatus encodeCms(
	SecCmsMessageRef	cmsMsg,
	const unsigned char *inData,		// add in this
	unsigned			inDataLen,
	unsigned char		**outData,		// mallocd and RETURNED
	unsigned			*outDataLen)	// RETURNED
{
	SecArenaPoolRef arena = NULL;
	SecArenaPoolCreate(1024, &arena);
	CSSM_DATA cdataIn = {inDataLen, (uint8 *)inData};
	CSSM_DATA cdataOut = {0, NULL};

	OSStatus ortn = SecCmsMessageEncode(cmsMsg, &cdataIn, arena, &cdataOut);
	if((ortn == noErr) && (cdataOut.Length != 0)) {
		*outData = (unsigned char *)malloc(cdataOut.Length);
		memmove(*outData, cdataOut.Data, cdataOut.Length);
		*outDataLen = cdataOut.Length;
	}
	else {
		cssmPerror("SecCmsMessageEncode", ortn);
		*outData = NULL;
		*outDataLen = 0;
	}
	SecArenaPoolFree(arena, false);
	return ortn;
}

#else

/* the hard way back when SecCmsMessageEncode() didn't work */
static OSStatus encodeCms(
	SecCmsMessageRef	cmsMsg,
	const unsigned char *inData,		// add in this
	unsigned			inDataLen,
	unsigned char		**outData,		// mallocd and RETURNED
	unsigned			*outDataLen)	// RETURNED
{
	SecArenaPoolRef arena = NULL;
	SecArenaPoolCreate(1024, &arena);
	SecCmsEncoderRef cmsEnc = NULL;
	CSSM_DATA output = { 0, NULL };
	OSStatus ortn;
	
	ortn = SecCmsEncoderCreate(cmsMsg, 
		NULL, NULL,			// no callback 
		&output, arena,		// data goes here
		NULL, NULL,			// no password callback (right?) 
		NULL, NULL,			// decrypt key callback
		NULL, NULL,			// detached digests
		&cmsEnc);
	if(ortn) {
		cssmPerror("SecKeychainItemCopyKeychain", ortn);
		goto errOut;
	}
	ortn = SecCmsEncoderUpdate(cmsEnc, (char *)inData, inDataLen);
	if(ortn) {
		cssmPerror("SecCmsEncoderUpdate", ortn);
		goto errOut;
	}
	ortn = SecCmsEncoderFinish(cmsEnc);
	if(ortn) {
		cssmPerror("SecCMsEncoderFinish", ortn);
		goto errOut;
	}
	
	/* Did we get any data? */
	if(output.Length) {
		*outData = (unsigned char *)malloc(output.Length);
		memmove(*outData, output.Data, output.Length);
		*outDataLen = output.Length;
	}
	else {
		*outData = NULL;
		*outDataLen = 0;
	}
errOut:
	if(arena) {
		SecArenaPoolFree(arena, false);
	}
	return ortn;
}

#endif

static OSStatus doSign(
	SecIdentityRef		signerId,
	const unsigned char *inData,
	unsigned			inDataLen,
	bool				detachedContent,
	const CSSM_OID		*eContentType,	// OPTIONAL 
	unsigned char		**outData,		// mallocd and RETURNED
	unsigned			*outDataLen)	// RETURNED
{
	if((inData == NULL) || (inDataLen == 0) || (outData == NULL)) {	
		fprintf(stderr, "***Sign requires input file. Aborting.\n");
		return paramErr;
	}
	if(signerId == NULL) {
		fprintf(stderr, "***Sign requires a signing identity. Aborting.\n");
		return paramErr;
	}
	
	SecCmsMessageRef cmsMsg = NULL;
    SecCmsContentInfoRef contentInfo = NULL;
    SecCmsSignedDataRef signedData = NULL;
	SecCertificateRef ourCert = NULL;
    SecCmsSignerInfoRef signerInfo;
	OSStatus ortn;
	SecKeychainRef ourKc = NULL;
	
	ortn = SecIdentityCopyCertificate(signerId, &ourCert);
	if(ortn) {
		cssmPerror("SecIdentityCopyCertificate", ortn);
		return ortn;
	}
	ortn = SecKeychainItemCopyKeychain((SecKeychainItemRef)ourCert, &ourKc);
	if(ortn) {
		cssmPerror("SecKeychainItemCopyKeychain", ortn);
		goto errOut;
	}
	
    // build chain of objects: message->signedData->data
	cmsMsg = SecCmsMessageCreate(NULL);
	if(cmsMsg == NULL) {
		fprintf(stderr, "***Error creating SecCmsMessageRef\n");
		ortn = -1;
		goto errOut;
	}
	signedData = SecCmsSignedDataCreate(cmsMsg);
	if(signedData == NULL) {
		printf("***Error creating SecCmsSignedDataRef\n");
		ortn = -1;
		goto errOut;
	}
	contentInfo = SecCmsMessageGetContentInfo(cmsMsg);
	ortn = SecCmsContentInfoSetContentSignedData(cmsMsg, contentInfo, signedData);
	if(ortn) {
		cssmPerror("SecCmsContentInfoSetContentSignedData", ortn);
		goto errOut;
	}
    contentInfo = SecCmsSignedDataGetContentInfo(signedData);
	if(eContentType != NULL) {
		ortn = SecCmsContentInfoSetContentOther(cmsMsg, contentInfo, 
			NULL /* data */, 
			detachedContent,
			eContentType);
		if(ortn) {
			cssmPerror("SecCmsContentInfoSetContentData", ortn);
			goto errOut;
		}
	}
	else {
		ortn = SecCmsContentInfoSetContentData(cmsMsg, contentInfo, NULL /* data */, 
			detachedContent);
		if(ortn) {
			cssmPerror("SecCmsContentInfoSetContentData", ortn);
			goto errOut;
		}
	}
	
    /* 
     * create & attach signer information
     */
    signerInfo = SecCmsSignerInfoCreate(cmsMsg, signerId, SEC_OID_SHA1);
    if (signerInfo == NULL) {
		fprintf(stderr, "***Error on SecCmsSignerInfoCreate\n");
		ortn = -1;
		goto errOut;
	}
    /* we want the cert chain included for this one */
	/* FIXME - what's the significance of the usage? */
	ortn = SecCmsSignerInfoIncludeCerts(signerInfo, SecCmsCMCertChain, certUsageEmailSigner);
	if(ortn) {
		cssmPerror("SecCmsSignerInfoIncludeCerts", ortn);
		goto errOut;
	}
	
	/* other options go here - signing time, etc. */

	ortn = SecCmsSignerInfoAddSMIMEEncKeyPrefs(signerInfo, ourCert, ourKc);
	if(ortn) {
		cssmPerror("SecCmsSignerInfoAddSMIMEEncKeyPrefs", ortn);
		goto errOut;
	}
	ortn = SecCmsSignedDataAddCertificate(signedData, ourCert);
	if(ortn) {
		cssmPerror("SecCmsSignedDataAddCertificate", ortn);
		goto errOut;
	}

	ortn = SecCmsSignedDataAddSignerInfo(signedData, signerInfo);
	if(ortn) {
		cssmPerror("SecCmsSignedDataAddSignerInfo", ortn);
		goto errOut;
	}

	/* go */
	ortn = encodeCms(cmsMsg, inData, inDataLen, outData, outDataLen);
errOut:
	/* free resources */
	if(cmsMsg) {
		SecCmsMessageDestroy(cmsMsg);
	}
	if(ourCert) {
		CFRelease(ourCert);
	}
	if(ourKc) {
		CFRelease(ourKc);
	}
	return ortn;
}

static OSStatus doEncrypt(
	SecCertificateRef   recipCert,		// eventually more than one
	const unsigned char *inData,
	unsigned			inDataLen,
	unsigned char		**outData,		// mallocd and RETURNED
	unsigned			*outDataLen)	// RETURNED
{
	if((inData == NULL) || (inDataLen == 0) || (outData == NULL)) {	
		fprintf(stderr, "***Encrypt requires input file. Aborting.\n");
		return paramErr;
	}
	if(recipCert == NULL) {
		fprintf(stderr, "***Encrypt requires a recipient certificate. Aborting.\n");
		return paramErr;
	}
	
	SecCmsMessageRef cmsMsg = NULL;
    SecCmsContentInfoRef contentInfo = NULL;
    SecCmsEnvelopedDataRef envelopedData = NULL;
	SecCmsRecipientInfoRef recipientInfo = NULL;
	OSStatus ortn;
	SecCertificateRef allCerts[2] = { recipCert, NULL};
	
	SECOidTag algorithmTag;
    int keySize;
	
	ortn = SecSMIMEFindBulkAlgForRecipients(allCerts, &algorithmTag, &keySize);
	if(ortn) {
		cssmPerror("SecSMIMEFindBulkAlgForRecipients", ortn);
		return ortn;
	}
	
    // build chain of objects: message->envelopedData->data
	cmsMsg = SecCmsMessageCreate(NULL);
	if(cmsMsg == NULL) {
		fprintf(stderr, "***Error creating SecCmsMessageRef\n");
		ortn = -1;
		goto errOut;
	}
	envelopedData = SecCmsEnvelopedDataCreate(cmsMsg, algorithmTag, keySize);
	if(envelopedData == NULL) {
		fprintf(stderr, "***Error creating SecCmsEnvelopedDataRef\n");
		ortn = -1;
		goto errOut;
	}
	contentInfo = SecCmsMessageGetContentInfo(cmsMsg);
	ortn = SecCmsContentInfoSetContentEnvelopedData(cmsMsg, contentInfo, envelopedData);
	if(ortn) {
		cssmPerror("SecCmsContentInfoSetContentEnvelopedData", ortn);
		goto errOut;
	}
    contentInfo = SecCmsEnvelopedDataGetContentInfo(envelopedData);
	ortn = SecCmsContentInfoSetContentData(cmsMsg, contentInfo, NULL /* data */, false);
	if(ortn) {
		cssmPerror("SecCmsContentInfoSetContentData", ortn);
		goto errOut;
	}
	
    /* 
     * create & attach recipient information
     */
	recipientInfo = SecCmsRecipientInfoCreate(cmsMsg, recipCert);
	ortn = SecCmsEnvelopedDataAddRecipient(envelopedData, recipientInfo);
	if(ortn) {
		cssmPerror("SecCmsEnvelopedDataAddRecipient", ortn);
		goto errOut;
	}


	/* go */
	ortn = encodeCms(cmsMsg, inData, inDataLen, outData, outDataLen);
errOut:
	/* free resources */
	if(cmsMsg) {
		SecCmsMessageDestroy(cmsMsg);
	}
	return ortn;
}

/* create nested message: msg = EnvelopedData(SignedData(inData)) */
static OSStatus doSignEncrypt(
	SecCertificateRef   recipCert,		// encryption recipient
	SecIdentityRef		signerId,		// signer
	const CSSM_OID		*eContentType,	// OPTIONAL - for signedData
	const unsigned char *inData,
	unsigned			inDataLen,
	unsigned char		**outData,		// mallocd and RETURNED
	unsigned			*outDataLen)	// RETURNED
{
	if((inData == NULL) || (inDataLen == 0) || (outData == NULL)) {	
		fprintf(stderr, "***Sign/Encrypt requires input file. Aborting.\n");
		return paramErr;
	}
	if(recipCert == NULL) {
		fprintf(stderr, "***Sign/Encrypt requires a recipient certificate. Aborting.\n");
		return paramErr;
	}
	if(signerId == NULL) {
		fprintf(stderr, "***Sign/Encrypt requires a signer Identity. Aborting.\n");
		return paramErr;
	}
	
	OSStatus ortn;
	unsigned char *signedData = NULL;
	unsigned signedDataLen = 0;
	SecCmsMessageRef cmsMsg = NULL;
    SecCmsContentInfoRef contentInfo = NULL;
    SecCmsEnvelopedDataRef envelopedData = NULL;
	SecCmsRecipientInfoRef recipientInfo = NULL;
	SecCertificateRef allCerts[2] = { recipCert, NULL};
	SECOidTag algorithmTag;
    int keySize;
	
	/* first get a SignedData */	
	ortn = doSign(signerId, inData, inDataLen, 
		false,		/* can't do detached content here */
		eContentType, 
		&signedData, &signedDataLen);
	if(ortn) {
		printf("***Error generating inner signedData. Aborting.\n");
		return ortn;
	}
	
	/* extract just the content - don't need the whole ContentINfo */
	unsigned char *signedDataContent = NULL;
	unsigned signedDataContentLen = 0;
	ortn = ContentInfoContent(signedData, signedDataLen, &signedDataContent, &signedDataContentLen);
	if(ortn) {
		goto errOut;
	}
	
	/* now wrap that in an EnvelopedData */
	ortn = SecSMIMEFindBulkAlgForRecipients(allCerts, &algorithmTag, &keySize);
	if(ortn) {
		cssmPerror("SecSMIMEFindBulkAlgForRecipients", ortn);
		return ortn;
	}
	
    // build chain of objects: message->envelopedData->data
	cmsMsg = SecCmsMessageCreate(NULL);
	if(cmsMsg == NULL) {
		fprintf(stderr, "***Error creating SecCmsMessageRef\n");
		ortn = -1;
		goto errOut;
	}
	envelopedData = SecCmsEnvelopedDataCreate(cmsMsg, algorithmTag, keySize);
	if(envelopedData == NULL) {
		fprintf(stderr, "***Error creating SecCmsEnvelopedDataRef\n");
		ortn = -1;
		goto errOut;
	}
	contentInfo = SecCmsMessageGetContentInfo(cmsMsg);
	ortn = SecCmsContentInfoSetContentEnvelopedData(cmsMsg, contentInfo, envelopedData);
	if(ortn) {
		cssmPerror("SecCmsContentInfoSetContentEnvelopedData", ortn);
		goto errOut;
	}
    contentInfo = SecCmsEnvelopedDataGetContentInfo(envelopedData);
	
	/* here's the difference: we override the 'data' content with a SignedData type,
	 * but we fool the smime lib into thinking it's a plain old data so it doesn't try
	 * to encode the SignedData */
	ortn = SecCmsContentInfoSetContentOther(cmsMsg, contentInfo, 
		NULL /* data */, 
		false,
		&CSSMOID_PKCS7_SignedData);
	if(ortn) {
		cssmPerror("SecCmsContentInfoSetContentData", ortn);
		goto errOut;
	}
	
    /* 
     * create & attach recipient information
     */
	recipientInfo = SecCmsRecipientInfoCreate(cmsMsg, recipCert);
	ortn = SecCmsEnvelopedDataAddRecipient(envelopedData, recipientInfo);
	if(ortn) {
		cssmPerror("SecCmsEnvelopedDataAddRecipient", ortn);
		goto errOut;
	}

	 
	/* go */
	ortn = encodeCms(cmsMsg, signedDataContent, signedDataContentLen, outData, outDataLen);
errOut:
	/* free resources */
	if(cmsMsg) {
		SecCmsMessageDestroy(cmsMsg);
	}
	if(signedData) {
		free(signedData);
	}
	if(signedDataContent) {
		free(signedDataContent);
	}
	return ortn;
}

int main(int argc, char **argv)
{
	if(argc < 2) {
		usage(argv);
	}
	
	CT_Op op;
	bool needId = false;
	if(!strcmp(argv[1], "sign")) {
		op = CTO_Sign;
		needId = true;
	}
	else if(!strcmp(argv[1], "envel")) {
		op = CTO_Envelop;
	}
	else if(!strcmp(argv[1], "signEnv")) {
		op = CTO_SignEnvelop;
		needId = true;
	}
	else if(!strcmp(argv[1], "parse")) {
		op = CTO_Parse;
	}
	else {
		fprintf(stderr, "***Unrecognized cmd.\n");
		usage(argv);
	}

	extern int optind;
	extern char *optarg;
	int arg;
	
	/* optional args */
	const char *keychainName = NULL;
	char *inFileName = NULL;
	char *outFileName = NULL;
	bool detachedContent = false;
	char *detachedFile = NULL;
	bool useIdPicker = false;
	char *recipient = NULL;
	bool quiet = false;
	bool parseSignerCert = false;
	CT_Vfy vfyOp = CTV_None;
	const CSSM_OID *eContentType = NULL;
	
	optind = 2;
	while ((arg = getopt(argc, argv, "i:o:k:pr:e:dD:qcv:")) != -1) {
		switch (arg) {
			case 'i':
				inFileName = optarg;
				break;
			case 'o':
				outFileName = optarg;
				break;
			case 'k':
				keychainName = optarg;
				break;
			case 'p':
				useIdPicker = true;
				break;
			case 'r':
				recipient = optarg;
				break;
			case 'c':
				parseSignerCert = true;
				break;
			case 'v':
				if(!strcmp(optarg, "sign")) {
					vfyOp = CTV_Sign;
				}
				else if(!strcmp(optarg, "encr")) {
					vfyOp = CTV_Envelop;
				}
				else if(!strcmp(optarg, "signEnv")) {
					vfyOp = CTV_SignEnvelop;
				}
				else {
					usage(argv);
				}
				break;
			case 'e':
				switch(optarg[0]) {
					case 'a':
						eContentType = &CSSMOID_PKINIT_AUTH_DATA;
						break;
					case 'r':
						eContentType = &CSSMOID_PKINIT_RKEY_DATA;
						break;
					default:
						usage(argv);
				}
				break;
			case 'd':
				if(op != CTO_Sign) {
					printf("-d only valid for op sign\n");
					exit(1);
				}
				detachedContent = true;
				break;
			case 'D':
				if(op != CTO_Parse) {
					printf("-D only valid for op sign\n");
					exit(1);
				}
				detachedFile = optarg;
				break;
			case 'q':
				quiet = true;
				break;
			default:
			case '?':
				usage(argv);
		}
	}
	if(optind != argc) {
		/* getopt does not return '?' */
		usage(argv);
	}
	
	SecIdentityRef idRef = NULL;
	SecKeychainRef kcRef = NULL;
	SecCertificateRef recipientCert = NULL;
	unsigned char *inData = NULL;
	unsigned inDataLen = 0;
	unsigned char *outData = NULL;
	unsigned outDataLen = 0;
	unsigned char *detachedData = NULL;
	unsigned detachedDataLen = 0;
	OSStatus ortn;
	
	if(inFileName) {
		if(readFile(inFileName, &inData, &inDataLen)) {
			fprintf(stderr, "***Error reading infile %s. Aborting.\n", inFileName);
			exit(1);
		}
	}
	if(detachedFile) {
		if(readFile(detachedFile, &detachedData, &detachedDataLen)) {
			fprintf(stderr, "***Error reading detachedFile %s. Aborting.\n", detachedFile);
			exit(1);
		}
	}
	if(keychainName) {
		ortn = SecKeychainOpen(keychainName, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainOpen", ortn);
			exit(1);
		}
	}
	if(useIdPicker) {
		ortn = sslSimpleIdentPicker(kcRef, &idRef);
		if(ortn) {
			fprintf(stderr, "***Error obtaining identity via picker. Aborting.\n");
			exit(1);
		}
	}
	else if(needId) {
		/* use first identity in specified keychain */
		CFArrayRef array = sslKcRefToCertArray(kcRef, CSSM_FALSE, CSSM_FALSE, 
			NULL,		// no verify policy
			NULL);
		if(array == NULL) {
			fprintf(stderr, "***Error finding a signing cert. Aborting.\n");
			exit(1);
		}
		idRef = (SecIdentityRef)CFArrayGetValueAtIndex(array, 0);
		if(idRef == NULL) {
			fprintf(stderr, "***No identities found. Aborting.\n");
			exit(1);
		}
		CFRetain(idRef);
		CFRelease(array);
	}
	if(recipient) {
		ortn = findCert(recipient, kcRef, &recipientCert);
		if(ortn) {
			exit(1);
		}
	}
	
	switch(op) {
		case CTO_Sign:
			ortn = doSign(idRef, inData, inDataLen, 
				detachedContent, eContentType, 
				&outData, &outDataLen);
			break;
		case CTO_Envelop:
			if(recipientCert == NULL) {
				if(idRef == NULL) {
					printf("***Need a recipient or an identity to encrypt\n");
					exit(1);
				}
				ortn = SecIdentityCopyCertificate(idRef, &recipientCert);
				if(ortn) {
					cssmPerror("SecIdentityCopyCertificate", ortn);
					exit(1);
				}
			}
			ortn = doEncrypt(recipientCert, inData, inDataLen, &outData, &outDataLen);
			break;
		case CTO_SignEnvelop:
			ortn = doSignEncrypt(recipientCert, idRef, eContentType,
				inData, inDataLen, &outData, &outDataLen);
			break;
		case CTO_Parse:
			ortn = doParse(inData, inDataLen, 
				detachedData, detachedDataLen,
				vfyOp, parseSignerCert, quiet,
				&outData, &outDataLen);
			break;
	}
	if(ortn) {
		goto errOut;
	}
	if(outData && outFileName) {
		if(writeFile(outFileName, outData, outDataLen)) {
			fprintf(stderr, "***Error writing to %s.\n", outFileName);
			ortn = -1;
		}
		else {
			if(!quiet) {
				fprintf(stderr, "...wrote %u bytes to %s.\n", outDataLen, outFileName);
			}
		}
	}
	else if(outData) {
		fprintf(stderr, "...generated %u bytes but no place to write it.\n", outDataLen);
	}
	else if(outFileName) {
		fprintf(stderr, "...nothing to write to file %s.\n", outFileName);
		/* assume this is an error, caller wanted something */
		ortn = -1;
	}
errOut:
	return ortn;
}
