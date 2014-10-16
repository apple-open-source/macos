/* Copyright (c) 1998-2003,2005-2006 Apple Computer, Inc.
 *
 * signerAndSubjSsl.c
 *
 * Create two certs - a root, and a subject cert signed by the root. 
 * Includes subjectAltName extension for leaf cert.
 * This version uses CSSM_TP_SubmitCredRequest to create the certs.
 *
 */

#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/clutils.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>
#include <Security/certextensions.h>
#include <Security/cssmapple.h>
#include <string.h>

/* key labels */
#define SUBJ_KEY_LABEL		"subjectKey"
#define ROOT_KEY_LABEL		"rootKey"

/* default key and signature algorithm */
#define SIG_ALG_DEFAULT		CSSM_ALGID_SHA1WithRSA
#define SIG_OID_DEFAULT		CSSMOID_SHA1WithRSA
#define KEY_ALG_DEFAULT		CSSM_ALGID_RSA

/* for write certs option */
#define ROOT_CERT_FILE_NAME		"ssRootCert.der"
#define SUBJ_CERT_FILE_NAME		"ssSubjCert.der"

/* public key in ref form, TP supports this as of 1/30/02 */
#define PUB_KEY_IS_REF			CSSM_TRUE

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("    i=IP_Address for subjectAltName\n");
	printf("    d=dnsName for subjectAltName\n");
	printf("    k=keySizeInBits\n");
	printf("    q(uiet)\n");
	exit(1);
}

/*
 * RDN components for root, subject
 */
CSSM_APPLE_TP_NAME_OID rootRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	{ "The Big Cheese",					&CSSMOID_Title }
};
#define NUM_ROOT_NAMES	(sizeof(rootRdn) / sizeof(CSSM_APPLE_TP_NAME_OID))

CSSM_APPLE_TP_NAME_OID subjRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	{ "something.org",					&CSSMOID_CommonName }
};
#define NUM_SUBJ_NAMES	(sizeof(subjRdn) / sizeof(CSSM_APPLE_TP_NAME_OID))

/*
 * Convert a string containing a dotted IP address to 4 bytes.
 * Returns nonzero on error.
 */
static int convertIp(
	const char 	*str,
	uint8 		*buf)
{
	char cbuf[4];
	for(unsigned dex=0; dex<3; dex++) {
		char *nextDot = strchr(str, '.');
		if(nextDot == NULL) {
			return 1;
		}
		memmove(cbuf, str, nextDot - str);
		*buf = atoi(cbuf);
		buf++;				// next out char
		str = nextDot + 1;	// next in char after dot
		
	}
	/* str points to last char */
	if(str == NULL) {
		return 1;
	}
	*buf = atoi(str);
	return 0;
}

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE	clHand;			// CL handle
	CSSM_CSP_HANDLE	cspHand;		// CSP handle
	CSSM_TP_HANDLE	tpHand;			// TP handle
	CSSM_DATA		signedRootCert;	// from CSSM_CL_CertSign
	CSSM_DATA		signedSubjCert;	// from CSSM_CL_CertSign
	CSSM_KEY		subjPubKey;		// subject's RSA public key blob
	CSSM_KEY		subjPrivKey;	// subject's RSA private key - ref format
	CSSM_KEY		rootPubKey;		// root's RSA public key blob
	CSSM_KEY		rootPrivKey;	// root's RSA private key - ref format
	CSSM_RETURN		crtn;
	int				arg;
	unsigned		errorCount = 0;
	CSSM_DATA		refId;			// mallocd by CSSM_TP_SubmitCredRequest
	CSSM_APPLE_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET			reqSet;
	sint32						estTime;
	CSSM_BOOL					confirmRequired;
	CSSM_TP_RESULT_SET_PTR		resultSet;
	CSSM_ENCODED_CERT			*encCert;
	CSSM_TP_CALLERAUTH_CONTEXT 	CallerAuthContext;
	CSSM_FIELD					policyId;
	CE_GeneralNames				genNames;
	CE_GeneralName				genName;
	uint8						ipNameBuf[4];
	
	/* user-spec'd variables */
	CSSM_ALGORITHMS	keyAlg = KEY_ALG_DEFAULT;
	CSSM_ALGORITHMS sigAlg = SIG_ALG_DEFAULT;
	CSSM_OID		sigOid = SIG_OID_DEFAULT;
	uint32			keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	char			*ipAddrs = NULL;
	char			*dnsName = NULL;
	CSSM_BOOL		quiet = CSSM_FALSE;
	
	/* 
	 * Two extensions. Subject has two (KeyUsage and possibly 
	 * subjectAltName); root has KeyUsage and  BasicConstraints.
	 */
	CE_DataAndType 			rootExts[2];
	CE_DataAndType 			leafExts[2];
	unsigned				numLeafExts;
	
	for(arg=1; arg<argc; arg++) {
		char *argp = argv[arg];
		switch(argp[0]) {
		    case 'k':
				keySizeInBits = atoi(&argp[2]);
				break;
			case 'i':
				ipAddrs = &argp[2];
				break;
			case 'd':
				dnsName = &argp[2];
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	
	if(ipAddrs && dnsName) {
		printf("Max of one of {ipAddrs, dnsName} at a time, please.\n");
		usage(argv);
	}
	if(ipAddrs) {
		if(convertIp(ipAddrs, ipNameBuf)) {
			printf("**Malformed IP address. Aborting.\n");
			exit(1);
		}
	}
	
	/* connect to CL, TP, and CSP */
	clHand = clStartup();
	if(clHand == 0) {
		return 0;
	}
	tpHand = tpStartup();
	if(tpHand == 0) {
		return 0;
	}
	cspHand = cspStartup();
	if(cspHand == 0) {
		return 0;
	}

	/* subsequent errors to abort: to detach */
	
	/* cook up an RSA key pair for the subject */
	crtn = cspGenKeyPair(cspHand,
		keyAlg,
		SUBJ_KEY_LABEL,
		strlen(SUBJ_KEY_LABEL),
		keySizeInBits,
		&subjPubKey,
		#if PUB_KEY_IS_REF
		CSSM_TRUE,
		#else
		CSSM_FALSE,			// pubIsRef - should work both ways, but not yet
		#endif
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&subjPrivKey,
		CSSM_TRUE,			// privIsRef - doesn't matter
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		errorCount++;
		goto abort;
	}

	/* and the root */
	crtn = cspGenKeyPair(cspHand,
		keyAlg,
		ROOT_KEY_LABEL,
		strlen(ROOT_KEY_LABEL),
		keySizeInBits,
		&rootPubKey,
		CSSM_FALSE,			// pubIsRef - should work both ways, but not yet
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&rootPrivKey,
		CSSM_TRUE,			// privIsRef - doesn't matter
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		errorCount++;
		goto abort;
	}

	/* A KeyUsage extension for both certs */
	rootExts[0].type = DT_KeyUsage;
	rootExts[0].critical = CSSM_FALSE;
	rootExts[0].extension.keyUsage = 
		CE_KU_DigitalSignature | CE_KU_KeyCertSign;

	leafExts[0].type = DT_KeyUsage;
	leafExts[0].critical = CSSM_FALSE;
	leafExts[0].extension.keyUsage =  CE_KU_DigitalSignature;

	/* BasicConstraints for root only */
	rootExts[1].type = DT_BasicConstraints;
	rootExts[1].critical = CSSM_TRUE;
	rootExts[1].extension.basicConstraints.cA = CSSM_TRUE;
	rootExts[1].extension.basicConstraints.pathLenConstraintPresent = 
			CSSM_TRUE;
	rootExts[1].extension.basicConstraints.pathLenConstraint = 2;

	/* possible subjectAltName for leaf */
	numLeafExts = 1;
	if(ipAddrs || dnsName) {
		numLeafExts++;
		leafExts[1].type = DT_SubjectAltName;
		leafExts[1].critical = CSSM_TRUE;
		
		genName.berEncoded = CSSM_FALSE;
		if(ipAddrs) {
			genName.name.Data = (uint8 *)ipNameBuf;
			genName.name.Length = 4;
			genName.nameType = GNT_IPAddress;
		}
		else {
			genName.name.Data = (uint8 *)dnsName;
			genName.nameType = GNT_DNSName;
			genName.name.Length = strlen(dnsName);
		}
		genNames.numNames = 1;
		genNames.generalName = &genName;
		leafExts[1].extension.subjectAltName = genNames;
	}
	
	/* certReq for root */
	memset(&certReq, 0, sizeof(CSSM_APPLE_TP_CERT_REQUEST));
	certReq.cspHand = cspHand;
	certReq.clHand = clHand;
	certReq.serialNumber = 0x12345678;
	certReq.numSubjectNames = NUM_ROOT_NAMES;
	certReq.subjectNames = rootRdn;
	certReq.numIssuerNames = 0;
	certReq.issuerNames = NULL;
	certReq.certPublicKey = &rootPubKey;
	certReq.issuerPrivateKey = &rootPrivKey;
	certReq.signatureAlg = sigAlg;
	certReq.signatureOid = sigOid;
	certReq.notBefore = 0;			// now
	certReq.notAfter = 10000;		// seconds from now
	certReq.numExtensions = 2;
	certReq.extensions = rootExts;
	
	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;
	
	/* a big CSSM_TP_CALLERAUTH_CONTEXT just to specify an OID */
	memset(&CallerAuthContext, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	memset(&policyId, 0, sizeof(CSSM_FIELD));
	policyId.FieldOid = CSSMOID_APPLE_TP_LOCAL_CERT_GEN;
	CallerAuthContext.Policy.NumberOfPolicyIds = 1;
	CallerAuthContext.Policy.PolicyIds = &policyId;
	
	/* generate root cert */
	crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,	
		&estTime,
		&refId);
	if(crtn) {
		printError("CSSM_TP_SubmitCredRequest", crtn);
		errorCount++;
		goto abort;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult", crtn);
		errorCount++;
		goto abort;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult returned NULL result set.\n");
		errorCount++;
		goto abort;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	signedRootCert = encCert->CertBlob;

	writeFile(ROOT_CERT_FILE_NAME, signedRootCert.Data, 
		signedRootCert.Length);
	if(!quiet) {
		printf("...wrote %lu bytes to %s\n", signedRootCert.Length, 
			ROOT_CERT_FILE_NAME);
	}

	/* now a subject cert signed by the root cert */
	certReq.serialNumber = 0x8765;
	certReq.numSubjectNames = NUM_SUBJ_NAMES;
	certReq.subjectNames = subjRdn;
	certReq.numIssuerNames = NUM_ROOT_NAMES;
	certReq.issuerNames = rootRdn;
	certReq.certPublicKey = &subjPubKey;
	certReq.issuerPrivateKey = &rootPrivKey;
	certReq.numExtensions = numLeafExts;
	certReq.extensions = leafExts;

	crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,
		&estTime,
		&refId);
	if(crtn) {
		printError("CSSM_TP_SubmitCredRequest (2)", crtn);
		errorCount++;
		goto abort;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);		// leaks.....
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult (2)", crtn);
		errorCount++;
		goto abort;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult (2) returned NULL result set.\n");
		errorCount++;
		goto abort;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	signedSubjCert = encCert->CertBlob;

	writeFile(SUBJ_CERT_FILE_NAME, signedSubjCert.Data, 
		signedSubjCert.Length);
	if(!quiet) {
		printf("...wrote %lu bytes to %s\n", signedSubjCert.Length, 
			SUBJ_CERT_FILE_NAME);
	}
	
	/* free/delete certs and keys */
	appFreeCssmData(&signedSubjCert, CSSM_FALSE);
	appFreeCssmData(&signedRootCert, CSSM_FALSE);

	cspFreeKey(cspHand, &rootPubKey);
	cspFreeKey(cspHand, &subjPubKey);

abort:
	if(cspHand != 0) {
		CSSM_ModuleDetach(cspHand);
	}
	if(clHand != 0) {
		CSSM_ModuleDetach(clHand);
	}
	if(tpHand != 0) {
		CSSM_ModuleDetach(tpHand);
	}

	return 0;
}


