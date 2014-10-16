/*
 * makeCrl.cpp - create a CRL revoking a given cert
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/Security.h>
#include <Security/SecAsn1Coder.h>
#include <Security/SecAsn1Types.h>
#include <Security/X509Templates.h>
#include <Security/keyTemplates.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecIdentityPriv.h>
#include <utilLib/common.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/timeStr.h>

#define THIS_UPDATE_DEF		0
#define NEXT_UPDATE_DEF		(60 * 60 * 24)
#define REVOKE_TIME_DEF		(60 * 60 * 12)

static void usage(char **argv)
{
	printf("usage: %s [requiredParams...] [options...]\n", argv[0]);
	printf("Required parameters:\n");
	printf("  -s subjectCert\n");
	printf("  -i issuerCert\n");
	printf("  -o outputFile\n");
	printf("Options:\n");
	printf("  -k keychain     -- contains issuerCert identity; default is default KC list\n");
	printf("  -r revokeTime   -- seconds after 'now' cert is revoked; default is %d\n",
		REVOKE_TIME_DEF);
	printf("  -t thisUpdate   -- CRL thisUpdate, seconds after 'now'; default is %d\n",
		THIS_UPDATE_DEF);
	printf("  -n thisUpdate   -- CRL nextUpdate, seconds after 'now'; default is %d\n",
		NEXT_UPDATE_DEF);
	/* etc. */
	exit(1);
}

/* seconds from now --> NSS_Time */
/* caller must eventually free nssTime.item.Data */
static void secondsToNssTime(
	int seconds,
	NSS_Time *nssTime)
{
	char *revocationDate = genTimeAtNowPlus(seconds);
	nssTime->item.Data = (uint8 *)revocationDate;
	nssTime->item.Length = strlen(revocationDate);
	nssTime->tag = SEC_ASN1_GENERALIZED_TIME; 
}

/* sign some data using a SecKeyRef */
static OSStatus secSign(
	SecKeyRef signingKey,
	CSSM_ALGORITHMS sigAlg,
	const CSSM_DATA *ptext,
	CSSM_DATA *sig)
{
	const CSSM_KEY *cssmKey;
	CSSM_CSP_HANDLE cspHand;
	const CSSM_ACCESS_CREDENTIALS *creds;
	CSSM_CC_HANDLE sigHand = 0;
	CSSM_RETURN crtn;
	OSStatus ortn;

	ortn = SecKeyGetCSSMKey(signingKey, &cssmKey);
	if(ortn) {
		cssmPerror("SecKeyGetCSSMKey", ortn);
		return ortn;
	} 
	ortn = SecKeyGetCSPHandle(signingKey, &cspHand);
	if(ortn) {
		cssmPerror("SecKeyGetCSPHandle", ortn);
		return ortn;
	} 
	ortn = SecKeyGetCredentials(signingKey,
		CSSM_ACL_AUTHORIZATION_SIGN,
		kSecCredentialTypeDefault,
		&creds);
	if(ortn) {
		cssmPerror("SecKeyGetCredentials", ortn);
		return ortn;
	} 
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		sigAlg,
		creds,	
		cssmKey,
		&sigHand);
	if(crtn) {
		cssmPerror("CSSM_CSP_CreateSignatureContext", crtn);
		return crtn;
	} 
	sig->Data = NULL;
	sig->Length = 0;
	crtn = CSSM_SignData(sigHand,
		ptext,
		1,
		CSSM_ALGID_NONE,	// digestAlg for raw sign
		sig);
	CSSM_DeleteContext(sigHand);
	return crtn;
}

/* basic "create and sign a CRL" routine - the CL is not capable of this */
static int makeCrl(
	const unsigned char *subjectCert,
	unsigned subjectCertLen,
	SecKeyRef signingKey,
	int revokeTime,					// revocation time, seconds from now (+/-)
	int crlThisUpdate,				// CRL thisUpdate, seconds from now (+/-)
	int crlNextUpdate,				// CRL nextUpdate, seconds from now (+/-)
	unsigned char **crlOut,			// mallocd and returned
	unsigned *crlOutLen)			// returned
{
	SecAsn1CoderRef coder = NULL;
	OSStatus ortn;
	int ourRtn = -1;
	NSS_Certificate subject;
	NSS_RevokedCert revokedCert;
	NSS_TBSCrl tbsCrl;
	NSS_SignedCertOrCRL crl;
	CSSM_DATA encodedCrl = {0, NULL};
	uint8 nullEnc[2] = {5, 0};
	CSSM_DATA nullEncData = {2, nullEnc};

	ortn = SecAsn1CoderCreate(&coder);
	if(ortn) {
		cssmPerror("SecAsn1CoderCreate", ortn);
		goto errOut;
	}

	/* decode subject to get serial number and issuer */
	memset(&subject, 0, sizeof(subject));
	ortn = SecAsn1Decode(coder, subjectCert, subjectCertLen, kSecAsn1SignedCertTemplate,
		&subject);
	if(ortn) {
		cssmPerror("SecAsn1Decode(subjectCert)", ortn);
		goto errOut;
	}

	/* revoked cert entry - no extensions */
	revokedCert.userCertificate = subject.tbs.serialNumber;
	secondsToNssTime(revokeTime, &revokedCert.revocationDate);
	revokedCert.extensions = NULL;

	/* TBS CRL - assume RSA signing key for now */
	memset(&tbsCrl, 0, sizeof(tbsCrl));
	tbsCrl.signature.algorithm = CSSMOID_SHA1WithRSA;
	tbsCrl.signature.parameters = nullEncData;
	tbsCrl.issuer = subject.tbs.issuer;
	secondsToNssTime(crlThisUpdate, &tbsCrl.thisUpdate);
	secondsToNssTime(crlNextUpdate, &tbsCrl.nextUpdate);
	tbsCrl.revokedCerts = (NSS_RevokedCert **)(SecAsn1Malloc(coder, sizeof(void *) * 2));
	tbsCrl.revokedCerts[0] = &revokedCert;
	tbsCrl.revokedCerts[1] = NULL;
	tbsCrl.extensions = NULL;

	/* encode TBS */
	memset(&crl, 0, sizeof(crl));
	ortn = SecAsn1EncodeItem(coder, &tbsCrl, kSecAsn1TBSCrlTemplate, &crl.tbsBlob);
	if(ortn) {
		cssmPerror("SecAsn1EncodeItem(tbsCrl)", ortn);
		goto errOut;
	} 

	/* encode top-level algid */
	ortn = SecAsn1EncodeItem(coder, &tbsCrl.signature,
		kSecAsn1AlgorithmIDTemplate, &crl.signatureAlgorithm);
	if(ortn) {
		cssmPerror("SecAsn1EncodeItem(signatureAlgorithm)", ortn);
		goto errOut;
	} 

	/* sign TBS */
	ortn = secSign(signingKey, CSSM_ALGID_SHA1WithRSA, &crl.tbsBlob, 
		&crl.signature);
	if(ortn) {
		goto errOut;
	}

	/* Encode result. Signature is bit string... */
	crl.signature.Length *= 8;
	ortn = SecAsn1EncodeItem(coder, &crl,
		kSecAsn1SignedCertOrCRLTemplate, &encodedCrl);
	if(ortn) {
		cssmPerror("SecAsn1EncodeItem(encodedCrl)", ortn);
		goto errOut;
	} 
	*crlOut = (unsigned char *)malloc(encodedCrl.Length);
	*crlOut = (unsigned char *)encodedCrl.Data;
	*crlOutLen = encodedCrl.Length;
	ourRtn = 0;
	
errOut:
	if(coder) {
		SecAsn1CoderRelease(coder);
	}
	if(revokedCert.revocationDate.item.Data) {
		CSSM_FREE(revokedCert.revocationDate.item.Data);
	}	
	if(tbsCrl.thisUpdate.item.Data) {
		CSSM_FREE(tbsCrl.thisUpdate.item.Data);
	}	
	if(revokedCert.revocationDate.item.Data) {
		CSSM_FREE(tbsCrl.nextUpdate.item.Data);
	}	
	return ourRtn;
}

int main(int argc, char **argv)
{
	char *subjectName = NULL;
	char *issuerName = NULL;
	char *outFileName = NULL;
	char *kcName = NULL;
	int revokeTime = REVOKE_TIME_DEF;
	int thisUpdate = THIS_UPDATE_DEF;
	int nextUpdate = NEXT_UPDATE_DEF;
	
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "s:i:o:k:r:t:n:h")) != -1) {
		switch (arg) {
			case 's':
				subjectName = optarg;
				break;
			case 'i':
				issuerName = optarg;
				break;	
			case 'o':
				outFileName = optarg;
				break;
			case 'k':
				kcName = optarg;
				break;
			case 'r':
				revokeTime = atoi(optarg);
				break;
			case 't':
				thisUpdate = atoi(optarg);
				break;
			case 'n':
				nextUpdate = atoi(optarg);
				break;
			default:
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	if((subjectName == NULL) || (issuerName == NULL) || (outFileName == NULL)) {
		usage(argv);
	}

	/* get input files */
	unsigned char *subjectCert;
	unsigned subjectCertLen;
	unsigned char *issuerCert;
	unsigned issuerCertLen;
	if(readFile(subjectName, &subjectCert, &subjectCertLen)) {
		printf("***Error reading %s. \n", subjectName);
		exit(1);
	}
	if(readFile(issuerName, &issuerCert, &issuerCertLen)) {
		printf("***Error reading %s. \n", issuerName);
		exit(1);
	}

	/* get issuer identity and signing key */
	SecKeychainRef kcRef = NULL;
	OSStatus ortn;
	if(kcName) {
		ortn = SecKeychainOpen(kcName, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainOpen", ortn);
			exit(1);
		}
	}
	SecCertificateRef certRef = NULL;
	SecIdentityRef idRef = NULL;
	SecKeyRef signingKey = NULL;
	CSSM_DATA issuerCData = {issuerCertLen, (uint8 *)issuerCert};
	ortn = SecCertificateCreateFromData(&issuerCData,	
		CSSM_CERT_X_509v3,
		CSSM_CERT_ENCODING_DER, 
		&certRef);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		exit(1);
	}
	ortn = SecIdentityCreateWithCertificate(kcRef, certRef, &idRef);
	if(ortn) {
		cssmPerror("SecIdentityCreateWithCertificate", ortn);
		exit(1);
	}
	ortn = SecIdentityCopyPrivateKey(idRef, &signingKey);
	if(ortn) {
		cssmPerror("SecIdentityCopyPrivateKey", ortn);
		exit(1);
	}

	/* create and sign the CRL */
	unsigned char *crlOut = NULL;
	unsigned crlOutLen = 0;
	if(makeCrl(subjectCert, subjectCertLen,
		signingKey,
		revokeTime, thisUpdate, nextUpdate,
		&crlOut, &crlOutLen)) {
		printf("***Error creating CRL. Aborting.\n");
		exit(1);
	}

	/* ==> outFile */
	if(writeFile(outFileName, crlOut, crlOutLen)) {
		printf("***Error writing CRL to %s\n", outFileName);
	}
	else {
		printf("...wrote %u bytes to %s.\n", crlOutLen, outFileName);
	}
	/* cleanup if you must */
	
	return 0;
}
