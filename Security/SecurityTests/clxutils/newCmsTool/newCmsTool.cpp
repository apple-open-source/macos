/*
 * cmstool.cpp - manipulate CMS messages, CMSEncoder/CMSDecoder version
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

#include <Security/SecTrustPriv.h>		/* SecTrustGetCssmResultCode */
#include <Security/SecIdentityPriv.h>	/* SecIdentityCreateWithCertificate */
#include <Security/CMSEncoder.h>
#include <Security/CMSDecoder.h>
#include <Security/CMSPrivate.h>

#include <Security/SecCertificate.h>
#include <Security/oidsattr.h>

#define CFRELEASE(cfr)	if(cfr != NULL) { CFRelease(cfr); }

static SecKeychainRef keychain_open(const char *name);

static void usage(char **argv)
{
    printf("Usage: %s cmd [option ...]\n", argv[0]);
	printf("cmd values:\n");
	printf("   sign                 -- create signedData\n");
	printf("   envel                -- create envelopedData\n");
	printf("   signEnv              -- create nested EnvelopedData(signedData(data))\n");
	printf("   certs                -- create certs-only CMS msg\n");
	printf("   parse                -- parse a CMS message file\n");
	printf("Input/output options:\n");
	printf("   -i infile\n");
	printf("   -o outfile\n");
	printf("   -D detachedContent   -- detached content (parse only)\n");
	printf("   -d detached          -- infile contains detached content (sign only)\n");
	printf("   -f certFileBase      -- dump all certs to certFileBase\n");
	printf("Signer and recipient options:\n");
	printf("   -k keychain          -- Keychain to search for certs\n");
	printf("   -p                   -- Use identity picker\n");
	printf("   -r recipient         -- add recipient (via email address) of enveloped data\n");
	printf("   -R recipCertFile     -- add recipient (via cert from file) of enveloped data\n");
	printf("   -S signerEmail       -- add signer email address\n");
	printf("   -C cert              -- add (general) signedData cert\n"); 
	printf("Misc. options:\n");
	printf("   -e eContentType      -- a(uthData)|r(keyData)\n");
	printf("   -m                   -- multi updates; default is one-shot\n");
	printf("   -1 (one)             -- custom encoder/decoder\n");
	printf("   -2                   -- fetch SecCmsMessageRef\n");
	printf("   -c                   -- parse signer certs\n");
	printf("   -a [ceEt]            -- Signed Attributes: c=SmimeCaps,\n");
	printf("                           e=EncrPrefs, E=MSEncrPrefs, t=signingTime\n");
	printf("   -A anchorFile        -- Verify certs using specified anchor cert\n");
	printf("   -M                   -- Do SecTrustEvaluate manually\n");
	printf("   -t certChainMode     -- none|signer|chain|chainWithRoot; default is chain\n");
	printf("   -l                   -- loop & pause for malloc debug\n");
	printf("   -q                   -- quiet\n");
	printf("   -Z                   -- silent, no output at all except for errors\n");
	printf("Verification options:\n");
	printf("   -v sign|encr|signEnv -- verify message is signed/encrypted/both\n");
	printf("   -s numSigners        -- verify msg has specified number of signers\n");
	printf("   -E eContentType      -- verify a(authData)|r(keyData)|d(data)\n");
	printf("   -N numCerts          -- verify number of certs\n");
	exit(1);
}

/* high level op */
typedef enum {
	CTO_Sign,
	CTO_Envelop,
	CTO_SignEnvelop,
	CTO_CertsOnly,
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

/*
 * Find a cert in specified keychain or keychain list matching specified 
 * email address. We happen to know that the email address is stored with the 
 * kSecAlias attribute. 
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
	
	attr.tag = kSecAlias;
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
		printf("***No certs found matching recipient %s. Aborting.\n",
			emailAddress);
		return ortn;
	}
	CFRelease(srch);
	return noErr;
}

/* create a SecCertificateRef from a file */
static SecCertificateRef readCertFile(
	const char *fileName)
{
	unsigned char *certData = NULL;
	unsigned certDataLen;
	SecCertificateRef rtnCert = NULL;
	
	if(readFile(fileName, &certData, &certDataLen)) {
		printf("***Error reading %s. Aborting.\n", fileName);
		return NULL;
	}
	CSSM_DATA cssmCert = {certDataLen, (uint8 *)certData};
	OSStatus ortn = SecCertificateCreateFromData(&cssmCert, 
		CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
		&rtnCert);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		printf("***Error creating cert fromn %s. Aborting.\n", fileName);
	}
	free(certData);
	return rtnCert;
}

static int dumpCertFiles(
	CFArrayRef allCerts,
	const char *fileBase,
	bool quiet)
{
	char fileName[200];
	
	if(allCerts == NULL) {
		printf("...no certs to write.\n");
		return 0;
	}
	CFIndex numCerts = CFArrayGetCount(allCerts);
	if(numCerts == 0) {
		printf("...no certs to write.\n");
		return 0;
	}
	for(CFIndex dex=0; dex<numCerts; dex++) {
		SecCertificateRef secCert = 
			(SecCertificateRef)CFArrayGetValueAtIndex(allCerts, dex);
		CSSM_DATA certData;
		OSStatus ortn;
		
		ortn = SecCertificateGetData(secCert, &certData);
		if(ortn) {
			cssmPerror("SecCertificateGetData", ortn);
			return -1;
		}	
		sprintf(fileName, "%s_%u.cer", fileBase, (unsigned)dex);
		if(writeFile(fileName, certData.Data, certData.Length)) {
			printf("***Error writing cert data to %s. Aborting.\n", fileName);
			return 1;
		}
		else if(!quiet) {
			printf("...wrote %u bytes to %s.\n",
				(unsigned)certData.Length, fileName);
		}
	}
	return 0;
}

/*
 * Do a random number of random-sized updates on a CMSEncoder.
 */
static OSStatus updateEncoder(
	CMSEncoderRef cmsEncoder,
	const unsigned char *inData,
	unsigned inDataLen)
{
	unsigned toMove = inDataLen;
	unsigned thisMove;
	
	while(toMove != 0) {
		thisMove = genRand(1, toMove);
		OSStatus ortn = CMSEncoderUpdateContent(cmsEncoder, inData, thisMove);
		if(ortn) {
			cssmPerror("CMSEncoderUpdateContent", ortn);
			return ortn;
		}
		toMove -= thisMove;
		inData += thisMove;
	}
	return noErr;
}

/*
 * Do a random number of random-sized updates on a CMSDecoder.
 */
static OSStatus updateDecoder(
	CMSDecoderRef cmsDecoder,
	const unsigned char *inData,
	unsigned inDataLen)
{
	unsigned toMove = inDataLen;
	unsigned thisMove;
	
	while(toMove != 0) {
		thisMove = genRand(1, toMove);
		OSStatus ortn = CMSDecoderUpdateMessage(cmsDecoder, inData, thisMove);
		if(ortn) {
			cssmPerror("CMSDecoderUpdateMessage", ortn);
			return ortn;
		}
		toMove -= thisMove;
		inData += thisMove;
	}
	return noErr;
}

#define TRUST_STRING_MAX	128

static OSStatus evalSecTrust(
	SecTrustRef			secTrust,
	CFMutableArrayRef	anchorArray,		// optional
	char				*trustStr,			// caller-mallocd, TRUST_STRING_MAX chars
	bool				quiet)
{
	OSStatus ortn;
	SecTrustResultType			secTrustResult;
	
	if(anchorArray) {
		ortn  = SecTrustSetAnchorCertificates(secTrust, anchorArray);
		if(ortn) {
			/* should never happen */
			cssmPerror("SecTrustSetAnchorCertificates", ortn);
			return ortn;
		}
	}
	ortn = SecTrustEvaluate(secTrust, &secTrustResult);
	if(ortn) {
		/* should never happen */
		cssmPerror("SecTrustEvaluate", ortn);
		return ortn;
	}
	switch(secTrustResult) {
		case kSecTrustResultUnspecified:
			/* cert chain valid, no special UserTrust assignments */
		case kSecTrustResultProceed:
			/* cert chain valid AND user explicitly trusts this */
			sprintf(trustStr, "Successful\n");
			return noErr;
		case kSecTrustResultDeny:
		case kSecTrustResultConfirm:
			/*
			 * Cert chain may well have verified OK, but user has flagged
			 * one of these certs as untrustable.
			 */
			sprintf(trustStr, "Not trusted per user-specified Trust level\n");
			/* bogus return code I know */
			return errSecInvalidTrustSetting;
		default:
		{
			/* get low-level TP error */
			OSStatus tpStatus;
			ortn = SecTrustGetCssmResultCode(secTrust, &tpStatus);
			if(ortn) {
				cssmPerror("SecTrustGetCssmResultCode", ortn);
				return ortn;
			}
			switch(tpStatus) {
				case CSSMERR_TP_INVALID_ANCHOR_CERT: 
					sprintf(trustStr, "Untrusted root\n");
					break;
				case CSSMERR_TP_NOT_TRUSTED:
					/* no root, not even in implicit SSL roots */
					sprintf(trustStr, "No root cert found\n");
					break;
				case CSSMERR_TP_CERT_EXPIRED:
					sprintf(trustStr, "Expired cert\n");
					break;
				case CSSMERR_TP_CERT_NOT_VALID_YET:
					sprintf(trustStr, "Cert not valid yet\n");
					break;
				default:
					sprintf(trustStr, "Other cert failure (%s)",
						cssmErrToStr(tpStatus));
					break;
			}
			return tpStatus;
		}
	} 	/* SecTrustEvaluate error */
	/* NOT REACHED */
}

static OSStatus doParse(
	const unsigned char *data,
	unsigned			dataLen,
	const unsigned char *detachedContent,
	unsigned			detachedContentLen,
	bool				multiUpdate,
	CT_Vfy				vfyOp,
	const CSSM_OID		*eContentVfy,	// optional to verify
	int					numSignersVfy,	// optional (>=0) to verify
	int					numCertsVfy,	// optionjal (>= 0) to verify
	bool				parseSignerCert,
	const char			*certFileBase,	// optionally write certs here
	bool				customDecoder,	// invoke CMSDecoderSetDecoder()
	bool				manTrustEval,	// evaluate SecTrust ourself
	CFMutableArrayRef	anchorArray,	// optional, and only for manTrustEval
	bool				quiet,
	CFDataRef			*outData)		// RETURNED 
{
	if((data == NULL) || (dataLen == 0)) {
		fprintf(stderr, "***Parse requires input file. Aborting.\n");
		return paramErr;
	}
	
	CMSDecoderRef cmsDecoder;
	size_t numSigners;
	Boolean isEncrypted;
	CFArrayRef allCerts = NULL;
	unsigned signerDex;
	SecPolicyRef policy = NULL;
	SecPolicySearchRef policySearch = NULL;
	CFIndex numCerts = 0;
	int addDetachedAfterDecode = 0;
	SecArenaPoolRef arena = NULL;
	
	/* 
	 * Four different return codes:
	 * -- ortn used for function returns; if nonzero, bail immediately and goto errOut
	 * -- ourRtn is manually set per the output of CMSDecoderCopySignerStatus and 
	 *    evalSecTrust
	 * -- trustRtn is the output of manual SecTrustEvaluate (via evalSecTrust())
	 * -- vfyErr indicates mismatch in caller-specified error params
	 *
	 * All four have to be zero fgor us to return zero.
	 */
	OSStatus ourRtn = noErr;
	OSStatus ortn = noErr;
	OSStatus trustRtn = noErr;
	int vfyErr = 0;
	
	ortn = CMSDecoderCreate(&cmsDecoder);
	if(ortn) {
		cssmPerror("CMSDecoderCreate", ortn);
		return ortn;
	}
	
	/* subsequent errors to errOut: */
	
	if(detachedContent != NULL) {
		/* 
		 * We can add detached content either before or after the 
		 * update/finalize; to test and verify, flip a coin to decide 
		 * when to do it.
		 */
		addDetachedAfterDecode = genRand(0, 1);
		if(!addDetachedAfterDecode) {
			CFDataRef cfDetach = CFDataCreate(NULL, detachedContent, detachedContentLen);
			ortn = CMSDecoderSetDetachedContent(cmsDecoder, cfDetach);
			CFRelease(cfDetach);
			if(ortn) {
				cssmPerror("CMSDecoderSetDetachedContent", ortn);
				goto errOut;
			}
		}
	}
	
	if(customDecoder) {
		/* Create a decoder; we don't have to free it, but we do have to free the
		 * arena pool */
		SecCmsDecoderRef coder = NULL;
		
		ortn = SecArenaPoolCreate(1024, &arena);
		if(ortn) {
			cssmPerror("SecArenaPoolCreate", ortn);
			goto errOut;
		}
		ortn = SecCmsDecoderCreate(arena, 
			NULL, NULL, NULL, NULL, NULL, NULL, &coder);
		if(ortn) {
			cssmPerror("SecCmsDecoderCreate", ortn);
			goto errOut;
		}
		ortn = CMSDecoderSetDecoder(cmsDecoder, coder);
		if(ortn) {
			cssmPerror("CMSDecoderSetDecoder", ortn);
			goto errOut;
		}
		else if(!quiet) {
			printf("...set up custom SecCmsDecoderRef\n");
		}
	}
	if(multiUpdate) {
		ortn = updateDecoder(cmsDecoder, data, dataLen);
		if(ortn) {
			goto errOut;
		}
	}
	else {
		ortn = CMSDecoderUpdateMessage(cmsDecoder, data, dataLen);
		if(ortn) {
			cssmPerror("CMSDecoderUpdateMessage", ortn);
			goto errOut;
		}
	}
	ortn = CMSDecoderFinalizeMessage(cmsDecoder);
	if(ortn) {
		cssmPerror("CMSDecoderFinalizeMessage", ortn);
		goto errOut;
	}
	if(addDetachedAfterDecode) {
		CFDataRef cfDetach = CFDataCreate(NULL, detachedContent, detachedContentLen);
		ortn = CMSDecoderSetDetachedContent(cmsDecoder, cfDetach);
		CFRelease(cfDetach);
		if(ortn) {
			cssmPerror("CMSDecoderSetDetachedContent", ortn);
			goto errOut;
		}
	}
	ortn = CMSDecoderGetNumSigners(cmsDecoder, &numSigners);
	if(ortn) {
		cssmPerror("CMSDecoderGetNumSigners", ortn);
		goto errOut;
	}
	ortn = CMSDecoderIsContentEncrypted(cmsDecoder, &isEncrypted);
	if(ortn) {
		cssmPerror("CMSDecoderIsContentEncrypted", ortn);
		goto errOut;
	}
	ortn = CMSDecoderCopyAllCerts(cmsDecoder, &allCerts);
	if(ortn) {
		cssmPerror("CMSDecoderCopyAllCerts", ortn);
		goto errOut;
	}
	if(allCerts) {
		numCerts = CFArrayGetCount(allCerts);
	}
	
	/* optional verify of expected message type */
	switch(vfyOp) {
		case CTV_None:
			break;
		case CTV_Sign: 
			if((numSigners == 0) && (allCerts == NULL)) {
				fprintf(stderr, "***Expected SignedData, but no signersFound\n");
				vfyErr = 1;
				/* but keep going */
			}
			if(isEncrypted) {
				fprintf(stderr, "***Expected SignedData, but msg IS encrypted\n");
				vfyErr = 1;
			}
			break;
		case CTV_SignEnvelop:
			if(numSigners == 0) {
				fprintf(stderr, "***Expected Signed&Enveloped, but no signersFound\n");
				vfyErr = 1;
			}
			if(!isEncrypted) {
				fprintf(stderr, "***Expected Signed&Enveloped, but msg not encrypted\n");
				vfyErr = 1;
			}
			break;
		case CTV_Envelop:
			if(numSigners != 0) {
				fprintf(stderr, "***Expected EnvelopedData, but signers found\n");
				vfyErr = 1;
			}
			if(!isEncrypted) {
				fprintf(stderr, "***Expected EnvelopedData, but msg not encrypted\n");
				vfyErr = 1;
			}
			break;
	}

	if(numSignersVfy >= 0) {
		if((unsigned)numSignersVfy != numSigners) {
			fprintf(stderr, "***Expected %d signers; found %lu\n",
				numSignersVfy, numSigners);
			vfyErr = 1;
		}
	}
	if(numCertsVfy >= 0) {
		if((int)numCerts != numCertsVfy) {
			fprintf(stderr, "***Expected %d certs; found %d\n",
				numCertsVfy, (int)numCerts);
			vfyErr = 1;
		}
	}
	if(!quiet) {
		fprintf(stderr, "=== CMS message info ===\n");
		fprintf(stderr, "   Num Signers      : %lu\n", (unsigned long)numSigners);
		fprintf(stderr, "   Encrypted        : %s\n", isEncrypted ? "true" : "false");
		fprintf(stderr, "   Num Certs        : %lu\n", 
			allCerts ? (unsigned long)CFArrayGetCount(allCerts) : 0);
	}
	
	if((certFileBase != NULL) & (allCerts != NULL)) {
		dumpCertFiles(allCerts, certFileBase, quiet);
	}


	if(numSigners) {
		CSSM_OID eContentType = {0, NULL};
		CFDataRef eContentData = NULL;
		OidParser oidParser;
		char str[OID_PARSER_STRING_SIZE];
		
		ortn = CMSDecoderCopyEncapsulatedContentType(cmsDecoder, &eContentData);
		if(ortn) {
			cssmPerror("CMSDecoderCopyEncapsulatedContentType", ortn);
			goto errOut;
		}
		if(eContentData != NULL) {
			eContentType.Data = (uint8 *)CFDataGetBytePtr(eContentData);
			eContentType.Length = CFDataGetLength(eContentData);
		}
		if(!quiet) {
			/* can't use stderr - oidparser is fixed w/stdout */
			printf("   eContentType     : ");
			if(eContentType.Data == NULL) {
				printf("***NONE FOUND***\n");
			}
			else if(eContentType.Length == 0) {
				printf("***EMPTY***\n");
			}
			else {
				oidParser.oidParse(eContentType.Data, eContentType.Length, str);
				printf("%s\n", str);
			}
		}
		
		if(eContentVfy != NULL) {
			if(eContentType.Data == NULL) {
				fprintf(stderr, "***Tried to verify eContentType, but none found\n");
				vfyErr = 1;
			}
			else if(!appCompareCssmData(eContentVfy, &eContentType)) {
				fprintf(stderr, "***eContentType verify error\n");
				fprintf(stderr, "   Expected: ");
				oidParser.oidParse(eContentVfy->Data, eContentVfy->Length, str);
				printf("%s\n", str);
				fprintf(stderr, "   Found   : ");
				oidParser.oidParse(eContentType.Data, eContentType.Length, str);
				printf("%s\n", str);
				vfyErr = 1;
			}
		}
		CFRELEASE(eContentData);
		
		/* get a policy for cert evaluation */
		ortn = SecPolicySearchCreate(CSSM_CERT_X_509v3,
			&CSSMOID_APPLE_X509_BASIC,
			NULL,
			&policySearch);
		if(ortn) {
			cssmPerror("SecPolicySearchCreate", ortn);
			goto errOut;
		}
		ortn = SecPolicySearchCopyNext(policySearch, &policy);
		if(ortn) {
			cssmPerror("SecPolicySearchCopyNext", ortn);
			goto errOut;
		}
	}
	for(signerDex=0; signerDex<numSigners; signerDex++) {
		CMSSignerStatus		signerStatus = kCMSSignerInvalidIndex;
		SecCertificateRef	signerCert;
		CFStringRef			signerEmailAddress;
		SecTrustRef			secTrust;
		OSStatus			certVerifyResultCode;
		char				trustStr[TRUST_STRING_MAX];
		
		ortn = CMSDecoderCopySignerStatus(cmsDecoder, signerDex,
			policy, 
			manTrustEval ? FALSE : TRUE,	/* evaluateSecTrust */
			&signerStatus,
			&secTrust,
			&certVerifyResultCode);
		if(ortn) {
			cssmPerror("CMSDecoderCopySignerStatus", ortn);
			goto errOut;
		}
		if(ourRtn == noErr) {
			if((signerStatus != kCMSSignerValid) ||
			   (certVerifyResultCode != CSSM_OK)) {
				ourRtn = -1;
			}
		}
		ortn = CMSDecoderCopySignerEmailAddress(cmsDecoder, signerDex, 
			&signerEmailAddress);
		if(ortn) {
			cssmPerror("CMSDecoderCopySignerEmailAddress", ortn);
			goto errOut;
		}
		ortn = CMSDecoderCopySignerCert(cmsDecoder, signerDex, 
			&signerCert);
		if(ortn) {
			cssmPerror("CMSDecoderCopySignerCertificate", ortn);
			goto errOut;
		}
		if(manTrustEval) {
			trustRtn = evalSecTrust(secTrust, anchorArray, trustStr, quiet);
		}
		
		/* display nothing here if quiet true and status is copacetic */
		if(!quiet || (signerStatus != kCMSSignerValid) || (trustRtn != noErr)) {
			fprintf(stderr, "   Signer %u:\n", signerDex);
			fprintf(stderr, "      signerStatus  : ");
			switch(signerStatus) {
				case kCMSSignerUnsigned: 
					fprintf(stderr, "kCMSSignerUnsigned\n"); break;
				case kCMSSignerValid: 
					fprintf(stderr, "kCMSSignerValid\n"); break;
				case kCMSSignerNeedsDetachedContent: 
					fprintf(stderr, "kCMSSignerNeedsDetachedContent\n"); break;
				case kCMSSignerInvalidSignature: 
					fprintf(stderr, "kCMSSignerInvalidSignature\n"); break;
				case kCMSSignerInvalidCert: 
					fprintf(stderr, "kCMSSignerInvalidCert\n"); break;
				case kCMSSignerInvalidIndex: 
					fprintf(stderr, "kCMSSignerInvalidIndex\n"); break;
			}
			if(manTrustEval) {
				fprintf(stderr, "      Trust Eval    : %s\n", trustStr);
			}
			fprintf(stderr, "      emailAddrs    : ");
			if(signerEmailAddress == NULL) {
				fprintf(stderr, "<<none found>>\n");
			}
			else {
				char emailStr[1000];
				if(!CFStringGetCString(signerEmailAddress, 
						emailStr, 1000, kCFStringEncodingASCII)) {
					fprintf(stderr, "<<<Error converting email address to C string>>>\n");
				}
				else {
					fprintf(stderr, "%s\n", emailStr);
				}
			}
			
			fprintf(stderr, "      vfyResult     : %s\n",
				certVerifyResultCode ? 
					cssmErrToStr(certVerifyResultCode) : "Success");
		
			/* TBD: optionally manually verify the SecTrust object */
			
			if(parseSignerCert) {
				
				if(signerCert == NULL) {
					fprintf(stderr, "      <<<Unable to obtain signer cert>>>\n");
				}
				else {
					CSSM_DATA certData;
					ortn = SecCertificateGetData(signerCert, &certData);
					if(ortn) {
						fprintf(stderr, "      <<<Unable to obtain signer cert>>>\n");
						cssmPerror("SecCertificateGetData", ortn);
					}
					else {
						printf("========== Signer Cert==========\n\n");
						printCert(certData.Data, certData.Length, CSSM_FALSE);
						printf("========== End Signer Cert==========\n\n");
					}
				}
			}	/* parseSignerCert */
		}	/* displaying per-signer info */
		
		CFRELEASE(signerCert); 
		signerCert = NULL;
		CFRELEASE(signerEmailAddress); 
		signerEmailAddress = NULL;
		CFRELEASE(secTrust); 
		secTrust = NULL;
	}	/* for signerDex */
	
	if(ortn == noErr) {
		ortn = CMSDecoderCopyContent(cmsDecoder, outData);
		if(ortn) {
			cssmPerror("CMSDecoderCopyContent", ortn);
		}
	}

errOut:
	CFRelease(cmsDecoder);
	if(arena != NULL) {
		SecArenaPoolFree(arena, false);
	}

	CFRELEASE(allCerts);
	CFRELEASE(policySearch);
	CFRELEASE(policy);
	if(ourRtn) {
		return ourRtn;
	}
	else if(trustRtn) {
		return trustRtn;
	}
	else if(vfyErr) {
		return vfyErr;
	}
	else {
		return ortn;
	}
}

static OSStatus doSign(
	CFTypeRef			signerOrArray,
	const unsigned char *inData,
	unsigned			inDataLen,
	bool				multiUpdate,
	bool				detachedContent,
	const CSSM_OID		*eContentType,	// OPTIONAL 
	CMSSignedAttributes	attrs,
	CFTypeRef			otherCerts,		// OPTIONAL
	bool				customCoder,
	bool				getCmsMsg,
	CMSCertificateChainMode chainMode,
	bool				quiet,
	CFDataRef			*outData)		// RETURNED
{
	if((inData == NULL) || (inDataLen == 0) || (outData == NULL)) {	
		fprintf(stderr, "***Sign requires input file. Aborting.\n");
		return paramErr;
	}
	if(signerOrArray == NULL) {
		fprintf(stderr, "***Sign requires a signing identity. Aborting.\n");
		return paramErr;
	}
	
	OSStatus ortn;
	CMSEncoderRef cmsEncoder = NULL;
	SecCmsMessageRef msg = NULL;		/* for optional CMSEncoderGetCmsMessage */
	SecArenaPoolRef arena = NULL;
	CSSM_DATA encoderOut = {0, NULL};
	
	if(multiUpdate || otherCerts || getCmsMsg || customCoder ||
			(chainMode != kCMSCertificateChain)) {
		/* one-shot encode doesn't support otherCerts or chainOptions*/
		
		ortn = CMSEncoderCreate(&cmsEncoder);
		if(ortn) {
			cssmPerror("CMSEncoderCreate", ortn);
			return ortn;
		}
		/* subsequent errors to errOut: */
		if(signerOrArray != NULL) {
			ortn = CMSEncoderAddSigners(cmsEncoder, signerOrArray);
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
		if(otherCerts) {
			ortn = CMSEncoderAddSupportingCerts(cmsEncoder, otherCerts);
			if(ortn) {
				goto errOut;
			}
		}
		if(attrs) {
			ortn = CMSEncoderAddSignedAttributes(cmsEncoder, attrs);
			if(ortn) {
				goto errOut;
			}
		}
		if(chainMode != kCMSCertificateChain) {
			ortn = CMSEncoderSetCertificateChainMode(cmsEncoder, chainMode);
			if(ortn) {
				goto errOut;
			}
		}
		if(getCmsMsg || customCoder) {
			/* 
			 * We just want to trigger the state transition 
			 * that we know should happen. We also might need
			 * the msg to create a custom coder. 
			 */
			ortn = CMSEncoderGetCmsMessage(cmsEncoder, &msg);
			if(ortn) {
				cssmPerror("CMSEncoderGetCmsMessage", ortn);
				goto errOut;
			}
		}
		
		if(customCoder) {
			SecCmsEncoderRef coder = NULL;
			ortn = SecArenaPoolCreate(1024, &arena);
			if(ortn) {
				cssmPerror("SecArenaPoolCreate", ortn);
				goto errOut;
			}
			ortn = SecCmsEncoderCreate(msg, 
				NULL, NULL,		// no callback 
				&encoderOut,	// data goes here
				arena,	
				NULL, NULL,		// no password callback (right?) 
				NULL, NULL,		// decrypt key callback
				NULL, NULL,		// detached digests
				&coder);
			if(ortn) {
				cssmPerror("SecCmsEncoderCreate", ortn);
				goto errOut;
			}
			ortn = CMSEncoderSetEncoder(cmsEncoder, coder);
			if(ortn) {
				cssmPerror("CMSEncoderSetEncoder", ortn);
				goto errOut;
			}
			else if(!quiet) {
				printf("...set up custom SecCmsEncoderRef\n");
			}
		}
		/* random number of random-sized updates */
		ortn = updateEncoder(cmsEncoder, inData, inDataLen);
		if(ortn) {
			goto errOut;
		}

		ortn = CMSEncoderCopyEncodedContent(cmsEncoder, outData);
		if(ortn) {
			cssmPerror("CMSEncoderCopyEncodedContent", ortn);
		}
		if(customCoder) {
			/* we have the data right here */
			*outData = 	CFDataCreate(NULL,
				(const UInt8 *)encoderOut.Data,	encoderOut.Length);
		}
	}
	else {
		ortn = CMSEncode(signerOrArray, 
			NULL,			/* recipients */
			eContentType,
			detachedContent,
			attrs,
			inData, inDataLen,
			outData);
		if(ortn) {
			printf("***CMSEncode returned %ld\n", (long)ortn);
			cssmPerror("CMSEncode", ortn);
		}
	}
errOut:
	if(cmsEncoder) {
		CFRelease(cmsEncoder);
	}
	if(arena) {
		SecArenaPoolFree(arena, false);
	}
	return ortn;
}

static OSStatus doEncrypt(
	CFTypeRef			recipOrArray,
	const unsigned char *inData,
	unsigned			inDataLen,
	bool				multiUpdate,
	CFDataRef			*outData)		// RETURNED
{
	if((inData == NULL) || (inDataLen == 0) || (outData == NULL)) {	
		fprintf(stderr, "***Encrypt requires input file. Aborting.\n");
		return paramErr;
	}
	if(recipOrArray == NULL) {
		fprintf(stderr, "***Encrypt requires a recipient certificate. Aborting.\n");
		return paramErr;
	}
	
	OSStatus ortn;
	CMSEncoderRef cmsEncoder = NULL;
	
	if(multiUpdate) {
		ortn = CMSEncoderCreate(&cmsEncoder);
		if(ortn) {
			cssmPerror("CMSEncoderCreate", ortn);
			return ortn;
		}
		/* subsequent errors to errOut: */
		ortn = CMSEncoderAddRecipients(cmsEncoder, recipOrArray);
		if(ortn) {
			goto errOut;
		}
		
		/* random number of random-sized updates */
		ortn = updateEncoder(cmsEncoder, inData, inDataLen);
		if(ortn) {
			goto errOut;
		}
		ortn = CMSEncoderCopyEncodedContent(cmsEncoder, outData);
		if(ortn) {
			cssmPerror("CMSEncoderCopyEncodedContent", ortn);
		}
	}
	else {
		/* one-shot */
		ortn = CMSEncode(NULL,	/* signers */
			recipOrArray,		
			NULL,				/* eContentType */
			FALSE,				/* detachedContent */
			kCMSAttrNone,
			inData, inDataLen,
			outData);
		if(ortn) {
			printf("***CMSEncode returned %ld\n", (long)ortn);
			cssmPerror("CMSEncode", ortn);
		}
	}
errOut:
	if(cmsEncoder) {
		CFRelease(cmsEncoder);
	}
	return ortn;
}

/* create nested message: msg = EnvelopedData(SignedData(inData)) */
static OSStatus doSignEncrypt(
	CFTypeRef			recipOrArray,	// encryption recipients
	CFTypeRef			signerOrArray,	// signers
	const CSSM_OID		*eContentType,	// OPTIONAL - for signedData
	CMSSignedAttributes	attrs,
	const unsigned char *inData,
	unsigned			inDataLen,
	bool				multiUpdate,
	CFTypeRef			otherCerts,		// OPTIONAL
	CFDataRef			*outData)		// RETURNED
{
	if((inData == NULL) || (inDataLen == 0) || (outData == NULL)) {	
		fprintf(stderr, "***Sign/Encrypt requires input file. Aborting.\n");
		return paramErr;
	}
	if(recipOrArray == NULL) {
		fprintf(stderr, "***Sign/Encrypt requires a recipient certificate. Aborting.\n");
		return paramErr;
	}
	if(signerOrArray == NULL) {
		fprintf(stderr, "***Sign/Encrypt requires a signer Identity. Aborting.\n");
		return paramErr;
	}
	
	OSStatus ortn;
	CMSEncoderRef cmsEncoder = NULL;
	
	if(multiUpdate || otherCerts) {
		ortn = CMSEncoderCreate(&cmsEncoder);
		if(ortn) {
			cssmPerror("CMSEncoderCreate", ortn);
			return ortn;
		}
		/* subsequent errors to errOut: */
		ortn = CMSEncoderAddRecipients(cmsEncoder, recipOrArray);
		if(ortn) {
			goto errOut;
		}
		ortn = CMSEncoderAddSigners(cmsEncoder, signerOrArray);
		if(ortn) {
			goto errOut;
		}
		if(eContentType) {
			ortn = CMSEncoderSetEncapsulatedContentType(cmsEncoder, eContentType);
			if(ortn) {
				goto errOut;
			}
		}
		if(otherCerts) {
			ortn = CMSEncoderAddSupportingCerts(cmsEncoder, otherCerts);
			if(ortn) {
				goto errOut;
			}
		}
		if(attrs) {
			ortn = CMSEncoderAddSignedAttributes(cmsEncoder, attrs);
			if(ortn) {
				goto errOut;
			}
		}

		/* random number of random-sized updates */
		ortn = updateEncoder(cmsEncoder, inData, inDataLen);
		if(ortn) {
			goto errOut;
		}
		ortn = CMSEncoderCopyEncodedContent(cmsEncoder, outData);
		if(ortn) {
			cssmPerror("CMSEncoderCopyEncodedContent", ortn);
		}
	}
	else {
		ortn = CMSEncode(signerOrArray,	
			recipOrArray,		
			eContentType,	
			FALSE,				/* detachedContent */
			attrs,
			inData, inDataLen,
			outData);
		if(ortn) {
			printf("***CMSEncode returned %ld\n", (long)ortn);
			cssmPerror("CMSEncode", ortn);
		}
	}
	
errOut:
	if(cmsEncoder) {
		CFRelease(cmsEncoder);
	}
	return ortn;
}

/*
 * Create a CMS message containing only certs.
 */
static OSStatus makeCertBag(
	CFTypeRef	certsOrArray,
	CFDataRef	*outData)
{
	if(certsOrArray == NULL) {
		printf("***Need some certs to generate this type of message.\n");
		return -1;
	}
	
	OSStatus ortn;
	CMSEncoderRef cmsEncoder = NULL;
	
	ortn = CMSEncoderCreate(&cmsEncoder);
	if(ortn) {
		cssmPerror("CMSEncoderCreate", ortn);
		return ortn;
	}
	/* subsequent errors to errOut: */
	ortn = CMSEncoderAddSupportingCerts(cmsEncoder, certsOrArray);
	if(ortn) {
		goto errOut;
	}
	ortn = CMSEncoderCopyEncodedContent(cmsEncoder, outData);
	if(ortn) {
		cssmPerror("CMSEncoderCopyEncodedContent", ortn);
	}
errOut:
	CFRelease(cmsEncoder);
	return ortn;
}

/*
 * Support maintanance of single item or array of them.
 *
 * Given new incoming 'newThing':
 *   if both *thingArray and *currThing are NULL
 *		*currThing = newThing;
 *		done;
 *   create *thingArray;
 *   add *currThing to *thingArray if present;
 *   add newThing to *thingArray;
 */
static void addThing(
	CFTypeRef newThing,
	CFTypeRef *currThing,
	CFMutableArrayRef *thingArray)
{
	if((*currThing == NULL) && (*thingArray == NULL)) {
		/* first occurrence of a thing */
		*currThing = newThing;
		return;
	}
	
	/* at least two things - prepare array */
	if(*thingArray == NULL) {
		*thingArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	}
	if(*currThing != NULL) {
		/* move current thing to array */
		CFArrayAppendValue(*thingArray, *currThing);
		CFRelease(*currThing);
		*currThing = NULL;
	}
	CFArrayAppendValue(*thingArray, newThing);
	CFRelease(newThing);
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
	else if(!strcmp(argv[1], "certs")) {
		op = CTO_CertsOnly;
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
	OSStatus ortn;
	
	/* optional args */
	char *inFileName = NULL;
	char *outFileName = NULL;
	bool detachedContent = false;
	char *detachedFile = NULL;
	bool useIdPicker = false;
	bool quiet = false;
	bool silent = false;
	bool parseSignerCert = false;
	const CSSM_OID *eContentType = NULL;
	bool multiUpdate = false;
	bool loopPause = false;
	char *certFileBase = NULL;
	CMSSignedAttributes signedAttrs = 0;
	char *anchorFile = NULL;
	bool manTrustEval = false;
	CMSCertificateChainMode chainMode = kCMSCertificateChain;

	/* for verification, usually in quiet/script mode */
	CT_Vfy vfyOp = CTV_None;
	const CSSM_OID *eContentVfy = NULL;
	int numSignersVfy = -1;
	int numCertsVfy = -1;
	
	/* for verifying functions in CMSPrivate.h */
	bool customCoder = false;
	bool fetchSecCmsMsg = false;
	
	/* 
	 * Signer/recipient items and arrays - use one item if possible, 
	 * else array (to test both paths)
	 */
	SecIdentityRef signerId = NULL;
	CFMutableArrayRef signerArray = NULL;
	SecCertificateRef recipCert = NULL;
	CFMutableArrayRef recipArray = NULL;
	SecCertificateRef generalCert = NULL;
	CFMutableArrayRef generalCertArray = NULL;
	SecKeychainRef kcRef = NULL;
	CFMutableArrayRef anchorArray = NULL;
	
	optind = 2;
	while ((arg = getopt(argc, argv, "i:o:k:pr:R:dD:e:mlqcv:s:E:S:C:f:N:a:A:M12t:Z")) != -1) {
		switch (arg) {
			case 'i':
				inFileName = optarg;
				break;
			case 'o':
				outFileName = optarg;
				break;
			case 'k':
				kcRef = keychain_open(optarg);
				if(!kcRef) {
				//	cssmPerror("SecKeychainOpen", ortn);
					exit(1);
				}
				break;
			case 'p':
				useIdPicker = true;
				break;
			case 'r':
			{
				SecCertificateRef newCert = NULL;
				char *recipient = optarg;
				ortn = findCert(recipient, kcRef, &newCert);
				if(ortn) {
					exit(1);
				}
				addThing(newCert, (CFTypeRef *)&recipCert, &recipArray);
				break;
			}
			case 'R':
			{
				SecCertificateRef certRef = readCertFile(optarg);
				if(certRef == NULL) {
					exit(1);
				}
				addThing(certRef, (CFTypeRef *)&recipCert, &recipArray);
				break;
			}
			case 'S':
			{
				SecIdentityRef newId = NULL;
				SecCertificateRef newCert = NULL;
				char *signerEmail = optarg;
				
				/* 
				 * first find the cert, optionally in the keychain already 
				 * specified via -k
				 */
				ortn = findCert(signerEmail, kcRef, &newCert);
				if(ortn) {
					exit(1);
				}
				
				/* map cert to an identity */
				ortn = SecIdentityCreateWithCertificate(kcRef, newCert, &newId);
				if(ortn) {
					cssmPerror("SecIdentityCreateWithCertificate", ortn);
					exit(1);
				}
				
				addThing(newId, (CFTypeRef *)&signerId, &signerArray);
				CFRelease(newCert);
				break;
			}
			case 'C':
			{
				SecCertificateRef newCert = readCertFile(optarg);
				if(newCert == NULL) {
					exit(1);
				}
				addThing(newCert, (CFTypeRef *)&generalCert, &generalCertArray);
				break;
			}	
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
			case 'E':
				switch(optarg[0]) {
					case 'a':
						eContentVfy = &CSSMOID_PKINIT_AUTH_DATA;
						break;
					case 'r':
						eContentVfy = &CSSMOID_PKINIT_RKEY_DATA;
						break;
					case 'd':
						eContentVfy = &CSSMOID_PKCS7_Data;
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
			case 'l':
				loopPause = true;
				break;
			case 'm':
				multiUpdate = true;
				break;
			case 's':
				numSignersVfy = atoi(optarg);
				break;
			case 'f':
				certFileBase = optarg;
				break;
			case 'N':
				numCertsVfy = atoi(optarg);
				break;
			case '1':
				customCoder = true;
				break;
			case '2':
				fetchSecCmsMsg = true;
				break;
			case 'a':
				for(; *optarg; optarg++) {
					switch(*optarg) {
						case 'c':
							signedAttrs |= kCMSAttrSmimeCapabilities;
							break;
						case 'e':
							signedAttrs |= kCMSAttrSmimeEncryptionKeyPrefs;
							break;
						case 'E':
							signedAttrs |= kCMSAttrSmimeMSEncryptionKeyPrefs;
							break;
						case 't':
							signedAttrs |= kCMSAttrSigningTime;
							break;
						default:
							usage(argv);
					}
				}
				break;
			case 'A':
				anchorFile = optarg;
				break;
			case 'M':
				manTrustEval = true;
				break;
			case 't':
				if(!strcmp(optarg, "none")) {
					chainMode = kCMSCertificateNone;
				}
				else if(!strcmp(optarg, "signer")) {
					chainMode = kCMSCertificateSignerOnly;
				}
				else if(!strcmp(optarg, "chain")) {
					chainMode = kCMSCertificateChain;
				}
				else if(!strcmp(optarg, "chainWithRoot")) {
					chainMode = kCMSCertificateChainWithRoot;
				}
				else {
					printf("***Bogus cert chain spec***\n");
					usage(argv);
				}
				break;
			case 'q':
				quiet = true;
				break;
			case 'Z':
				quiet = true;
				silent = true;
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
	
	unsigned char *inData = NULL;
	unsigned inDataLen = 0;
	unsigned char *detachedData = NULL;
	unsigned detachedDataLen = 0;
	CFDataRef outData = NULL;
	CFIndex byteCount = 0; 
	if(!silent) {
		testStartBanner((char *)"newCmsTool", argc, argv);
	}
	
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
	
	/* signer IDs */
	if(useIdPicker) {
		ortn = sslSimpleIdentPicker(kcRef, &signerId);
		if(ortn) {
			fprintf(stderr, "***Error obtaining identity via picker. Aborting.\n");
			exit(1);
		}
	}
	if(anchorFile) {
		SecCertificateRef secCert = readCertFile(anchorFile);
		if(secCert == NULL) {
			exit(1);
		}
		anchorArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayAppendValue(anchorArray, secCert);
		CFRelease(secCert);
	}
	
	/* 
	 * In order for signed blobs to contain a full cert chain, and to find
	 * certs matching a specific recipient cert email address, the keychain 
	 * containing intermediates must be in the user's keychain search list 
	 * (or, alternatively, the intermedaite certs must be added manually).
	 * To alleviate the burden on test scripts, we'll ensure that an optionally 
	 * specified keychain is in factin the search list when we sign a message 
	 * here. 
	 * Careful - make sure to ALWAYS restore the search list!
	 */
	CFArrayRef originalSearchList = NULL;
	if(kcRef != NULL) {
		ortn = SecKeychainCopySearchList(&originalSearchList);
		if(ortn) {
			cssmPerror("SecKeychainCopySearchList", ortn);
			exit(1);
		}
		CFMutableArrayRef newList = CFArrayCreateMutableCopy(
			NULL, 0, originalSearchList);
		CFArrayAppendValue(newList, kcRef);
		ortn = SecKeychainSetSearchList(newList);
		if(ortn) {
			cssmPerror("SecKeychainSetSearchList", ortn);
			exit(1);
		}
		/* DO NOT EXIT WITHOUT RESTORING TO originalSearchList */
	}
	do {
		switch(op) {
			case CTO_Sign:
				ortn = doSign((signerArray != NULL) ? 
						(CFTypeRef)signerArray : (CFTypeRef)signerId, 
					inData, inDataLen, 
					multiUpdate, detachedContent, eContentType, signedAttrs, 
					(generalCertArray != NULL) ?
						(CFTypeRef)generalCertArray : (CFTypeRef)generalCert,
					customCoder, fetchSecCmsMsg, chainMode, quiet,
					&outData);
				break;
			case CTO_Envelop:
				ortn = doEncrypt(recipArray ? 
						(CFTypeRef)recipArray : (CFTypeRef)recipCert,
					inData, inDataLen, 
					multiUpdate, &outData);
				break;
			case CTO_SignEnvelop:
				ortn = doSignEncrypt(recipArray ?
						(CFTypeRef)recipArray : (CFTypeRef)recipCert, 
					signerArray ? 
						(CFTypeRef)signerArray : (CFTypeRef)signerId, 
					eContentType, signedAttrs,
					inData, inDataLen, multiUpdate, 
					(generalCertArray != NULL) ?
						(CFTypeRef)generalCertArray : (CFTypeRef)generalCert,
					&outData);
				break;
			case CTO_CertsOnly:
				ortn = makeCertBag((generalCertArray != NULL) ?
						(CFTypeRef)generalCertArray : (CFTypeRef)generalCert,
						&outData);
				break;
			case CTO_Parse:
				ortn = doParse(inData, inDataLen, 
					detachedData, detachedDataLen,
					multiUpdate, 
					vfyOp, eContentVfy, numSignersVfy, numCertsVfy,
					parseSignerCert, certFileBase, customCoder, 
					manTrustEval, anchorArray,
					quiet, &outData);
				break;
		}
		
		if(loopPause) {
			if(outData) {
				printf("...generated %u bytes of data.\n",
					(unsigned)CFDataGetLength(outData));
			}
			fpurge(stdin);
			printf("q to quit, anything else to loop again: ");
			char resp = getchar();
			if(resp == 'q') {
				break;
			}
			else {
				CFRELEASE(outData);
				outData = NULL;
			}
		}
	} while (loopPause);
	
	if(originalSearchList) {
		ortn = SecKeychainSetSearchList(originalSearchList);
		if(ortn) {
			cssmPerror("SecKeychainSetSearchList", ortn);
			/* keep going */
		}
	}
	
	if(ortn) {
		goto errOut;
	}
	
	byteCount = outData ? CFDataGetLength(outData) : 0;
	if(outData && outFileName) {
		if(writeFile(outFileName, CFDataGetBytePtr(outData), byteCount)) {
			fprintf(stderr, "***Error writing to %s.\n", outFileName);
			ortn = -1;
		}
		else {
			if(!quiet) {
				fprintf(stderr, "...wrote %u bytes to %s.\n", 
					(unsigned)byteCount, outFileName);
			}
		}
	}
	else if(byteCount) {
		fprintf(stderr, "...generated %u bytes but no place to write it.\n", 
			(unsigned)byteCount);
	}
	else if(outFileName) {
		fprintf(stderr, "...nothing to write to file %s.\n", outFileName);
		/* assume this is an error, caller wanted something */
		ortn = -1;
	}
errOut:
	return ortn;
}

/*
	From SecurityTool/keychain_utilites.cpp
	This properly supports dynamic keychains, i.e. smartcards
*/

SecKeychainRef keychain_open(const char *name)
{
	SecKeychainRef keychain = NULL;
	OSStatus result;
	
//	check_obsolete_keychain(name);
	if (name && name[0] != '/')
	{
		CFArrayRef dynamic = NULL;
		result = SecKeychainCopyDomainSearchList(
												 kSecPreferencesDomainDynamic, &dynamic);
		if (result)
		{
			//	cssmPerror("SecKeychainOpen", ortn);
		//	sec_error("SecKeychainCopyDomainSearchList %s: %s", 
		//			  name, sec_errstr(result));
			cssmPerror("SecKeychainCopyDomainSearchList", result);
			return NULL;
		}
		else
		{
			uint32_t i;
			uint32_t count = dynamic ? CFArrayGetCount(dynamic) : 0;
			
			for (i = 0; i < count; ++i)
			{
				char pathName[PATH_MAX];
				UInt32 ioPathLength = sizeof(pathName);
				bzero(pathName, ioPathLength);
				keychain = (SecKeychainRef)CFArrayGetValueAtIndex(dynamic, i);
				result = SecKeychainGetPath(keychain, &ioPathLength, pathName);
				if (result)
				{
				//	sec_error("SecKeychainGetPath %s: %s",  name, sec_errstr(result));
					cssmPerror("SecKeychainCopyDomainSearchList", result);
					return NULL;
				}
				if (!strncmp(pathName, name, ioPathLength))
				{
					CFRetain(keychain);
					CFRelease(dynamic);
					return keychain;
				}
			}
			CFRelease(dynamic);
		}
	}
	
	result = SecKeychainOpen(name, &keychain);
	if (result)
	{
	//	sec_error("SecKeychainOpen %s: %s", name, sec_errstr(result));
		cssmPerror("SecKeychainOpen", result);
	}
	
	return keychain;
}

