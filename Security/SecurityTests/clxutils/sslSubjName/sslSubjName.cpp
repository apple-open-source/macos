/* Copyright (c) 2002-2004,2006,2008 Apple Inc.
 *
 * sslSubjName.c
 *
 * Verify comparision of app-specified host name vs. various
 * forms of hostname in a cert.
 *
 */

#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/certVerify.h>
#include <clAppUtils/BlobList.h>
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
#include <security_cdsa_utils/cuFileIo.h>

/* key labels */
#define SUBJ_KEY_LABEL		"subjectKey"
#define ROOT_KEY_LABEL		"rootKey"

/* key and signature algorithm - shouldn't matter for this test */
#define SIG_ALG_DEFAULT		CSSM_ALGID_SHA1WithRSA
#define SIG_OID_DEFAULT		CSSMOID_SHA1WithRSA
#define KEY_ALG_DEFAULT		CSSM_ALGID_RSA

#define KEY_SIZE_DEFAULT	512

#define CERT_FILE		"sslCert.cer"

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("    w(write certs)\n");
	printf("    q(uiet)\n");
	printf("    v(erbose)\n");
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

#define SUBJ_COMMON_NAME	"something.org"

CSSM_APPLE_TP_NAME_OID subjRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	/* overridden when creating the cert */
	{ NULL,								&CSSMOID_CommonName }
};
#define SUBJ_COMMON_NAME_DEX	1

#define NUM_SUBJ_NAMES	(sizeof(subjRdn) / sizeof(CSSM_APPLE_TP_NAME_OID))


/*
 * Test cases
 */
typedef struct {
	/* test description */
	const char		*testDesc;
	
	/* host names for leaf cert - zero or one of these */
	const char		*certDnsName;
	const char		*certIpAddr;
	
	/* subject common name */
	const char		*commonName;
	
	/* host name for CertGroupVerify */
	const char		*vfyHostName;
	
	/* expected error - NULL or e.g. "CSSMERR_APPLETP_CRL_NOT_TRUSTED" */
	const char		*expectErrStr;
	
	/* one optional per-cert error string */
	const char		*certErrorStr;
	
} SSN_TestCase;

SSN_TestCase testCases[] = 
{
	{
		"DNS Name foo.bar, vfyName foo.bar",
		"foo.bar", NULL, SUBJ_COMMON_NAME, "foo.bar",
		NULL, 
		NULL
	},
	{
		"DNS Name foo.bar, vfyName something.org, expect fail due to "
			"DNS present",
		"foo.bar", NULL, SUBJ_COMMON_NAME, "something.org",
		"CSSMERR_APPLETP_HOSTNAME_MISMATCH", 
		"0:CSSMERR_APPLETP_HOSTNAME_MISMATCH"
	},
	{
		"DNS Name foo.bar, vfyName foo.foo.bar, expect fail",
		"foo.bar", NULL, SUBJ_COMMON_NAME, "foo.foo.bar",
		"CSSMERR_APPLETP_HOSTNAME_MISMATCH", 
		"0:CSSMERR_APPLETP_HOSTNAME_MISMATCH"
	},
	{
		"IP Name 1.0.5.8, vfyName 1.0.5.8",
		NULL, "1.0.5.8", SUBJ_COMMON_NAME, "1.0.5.8",
		NULL, 
		NULL
	},
	{
		"IP Name 1.0.5.8, vfyName 1.00.5.008",
		NULL, "1.0.5.8", SUBJ_COMMON_NAME, "1.00.5.008",
		NULL, 
		NULL
	},
	{
		"IP Name 1.0.5.8, vfyName something.org",
		NULL, "1.0.5.8", SUBJ_COMMON_NAME, "something.org",
		NULL, 
		NULL
	},
	{
		"IP Name 1.0.5.8, vfyName 2.0.5.8, expect fail",
		NULL, "1.0.5.8", SUBJ_COMMON_NAME, "2.0.5.8",
		"CSSMERR_APPLETP_HOSTNAME_MISMATCH", 
		"0:CSSMERR_APPLETP_HOSTNAME_MISMATCH"
	},
	{
		"DNS Name *.foo.bar, vfyName bar.foo.bar",
		"*.foo.bar", NULL, SUBJ_COMMON_NAME, "bar.foo.bar",
		NULL, 
		NULL
	},
	{
		"DNS Name *.foo.bar, vfyName foo.bar, expect fail",
		"*.foo.bar", NULL, SUBJ_COMMON_NAME, "foo.bar",
		"CSSMERR_APPLETP_HOSTNAME_MISMATCH", 
		"0:CSSMERR_APPLETP_HOSTNAME_MISMATCH"
	},
	{
		"DNS Name *foo.bar, vfyName barfoo.bar",
		"*foo.bar", NULL, SUBJ_COMMON_NAME, "barfoo.bar",
		NULL, 
		NULL
	},
	{
		"DNS Name *foo*.bar, vfyName barfoo.bar",
		"*foo*.bar", NULL, SUBJ_COMMON_NAME, "barfoo.bar",
		NULL, 
		NULL
	},
	{
		"DNS Name *foo*.bar, vfyName foobar.bar",
		"*foo*.bar", NULL, SUBJ_COMMON_NAME, "foobar.bar",
		NULL, 
		NULL
	},
	{
		"DNS Name *foo*.bar, vfyName foo.bar",
		"*foo*.bar", NULL, SUBJ_COMMON_NAME, "foo.bar",
		NULL, 
		NULL
	},
	{
		"DNS Name *foo.bar, vfyName bar.foo.bar, should fail",
		"*foo.bar", NULL, SUBJ_COMMON_NAME, "bar.foo.bar",
		"CSSMERR_APPLETP_HOSTNAME_MISMATCH", 
		"0:CSSMERR_APPLETP_HOSTNAME_MISMATCH"
	},
	{
		"DNS Name *foo.bar, vfyName foobar.bar, should fail",
		"*foo.bar", NULL, SUBJ_COMMON_NAME, "foobar.bar",
		"CSSMERR_APPLETP_HOSTNAME_MISMATCH", 
		"0:CSSMERR_APPLETP_HOSTNAME_MISMATCH"
	},
	{
		"No DNS or IP name, commonName = vfyName = 1.0.5.8",
		NULL, NULL, "1.0.5.8", "1.0.5.8",
		"CSSMERR_APPLETP_HOSTNAME_MISMATCH", 
		"0:CSSMERR_APPLETP_HOSTNAME_MISMATCH"
	},
};

#define NUM_TEST_CASES	(sizeof(testCases) / sizeof(SSN_TestCase))

/*
 * Convert a string containing a dotted IP address to 4 bytes.
 * Returns nonzero on error.
 * FIXME - should handle 16-byte IP addresses. 
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
		memset(cbuf, 0, sizeof(cbuf));
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

/*
 * Generate a pair of certs.
 */
static CSSM_RETURN genCerts(
	CSSM_CL_HANDLE	clHand,
	CSSM_CSP_HANDLE	cspHand,	
	CSSM_TP_HANDLE	tpHand,	
	CSSM_KEY_PTR	rootPrivKey,
	CSSM_KEY_PTR	rootPubKey,
	CSSM_KEY_PTR	subjPubKey,
	/* one of these goes into leaf's subjectAltName */
	const char		*subjIpAddr,
	const char		*subjDnsName,
	const char		*commonName,
	CSSM_DATA		&rootCert,		// RETURNED
	CSSM_DATA		&subjCert)		// RETURNED
	
{
	CSSM_DATA					refId;	
								// mallocd by CSSM_TP_SubmitCredRequest
	CSSM_RETURN					crtn;
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
	/* 
	 * Two extensions. Subject has two (KeyUsage and possibly 
	 * subjectAltName); root has KeyUsage and  BasicConstraints.
	 */
	CE_DataAndType 				rootExts[2];
	CE_DataAndType 				leafExts[2];
	unsigned					numLeafExts;
	
	if(subjIpAddr && subjDnsName) {
		printf("***Max of one of {subjIpAddr, subjDnsName} at a "
			"time, please.\n");
		exit(1);
	}
	if(subjIpAddr) {
		if(convertIp(subjIpAddr, ipNameBuf)) {
			printf("**Malformed IP address. Aborting.\n");
			exit(1);
		}
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
	if(subjIpAddr || subjDnsName) {
		numLeafExts++;
		leafExts[1].type = DT_SubjectAltName;
		leafExts[1].critical = CSSM_TRUE;
		
		genName.berEncoded = CSSM_FALSE;
		if(subjIpAddr) {
			genName.name.Data = (uint8 *)ipNameBuf;
			genName.name.Length = 4;
			genName.nameType = GNT_IPAddress;
		}
		else {
			genName.name.Data = (uint8 *)subjDnsName;
			genName.nameType = GNT_DNSName;
			genName.name.Length = strlen(subjDnsName);
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
	certReq.certPublicKey = rootPubKey;
	certReq.issuerPrivateKey = rootPrivKey;
	certReq.signatureAlg = SIG_ALG_DEFAULT;
	certReq.signatureOid = SIG_OID_DEFAULT;
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
		return crtn;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult", crtn);
		return crtn;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult returned NULL result set.\n");
		return crtn;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	rootCert = encCert->CertBlob;

	/* now a subject cert signed by the root cert */
	certReq.serialNumber = 0x8765;
	certReq.numSubjectNames = NUM_SUBJ_NAMES;
	subjRdn[SUBJ_COMMON_NAME_DEX].string = commonName;
	certReq.subjectNames = subjRdn;
	certReq.numIssuerNames = NUM_ROOT_NAMES;
	certReq.issuerNames = rootRdn;
	certReq.certPublicKey = subjPubKey;
	certReq.issuerPrivateKey = rootPrivKey;
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
		return crtn;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);		// leaks.....
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult (2)", crtn);
		return crtn;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult (2) returned NULL "
				"result set.\n");
		return crtn;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	subjCert = encCert->CertBlob;

	return CSSM_OK;
}

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE	clHand;			// CL handle
	CSSM_CSP_HANDLE	cspHand;		// CSP handle
	CSSM_TP_HANDLE	tpHand;			// TP handle
	CSSM_DATA		rootCert;		
	CSSM_DATA		subjCert;	
	CSSM_KEY		subjPubKey;		// subject's RSA public key blob
	CSSM_KEY		subjPrivKey;	// subject's RSA private key - ref format
	CSSM_KEY		rootPubKey;		// root's RSA public key blob
	CSSM_KEY		rootPrivKey;	// root's RSA private key - ref format
	CSSM_RETURN		crtn = CSSM_OK;
	int				vfyRtn = 0;
	int				arg;
	SSN_TestCase	*testCase;
	unsigned		testNum;
	
	CSSM_BOOL		quiet = CSSM_FALSE;
	CSSM_BOOL		verbose = CSSM_FALSE;
	CSSM_BOOL		writeCerts = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		char *argp = argv[arg];
		switch(argp[0]) {
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
			case 'w':
				writeCerts = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	
	testStartBanner("sslSubjName", argc, argv);
	
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
		KEY_ALG_DEFAULT,
		SUBJ_KEY_LABEL,
		strlen(SUBJ_KEY_LABEL),
		KEY_SIZE_DEFAULT,
		&subjPubKey,
		CSSM_FALSE,			// pubIsRef
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&subjPrivKey,
		CSSM_TRUE,			// privIsRef - doesn't matter
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		return crtn;
	}

	/* and the root */
	crtn = cspGenKeyPair(cspHand,
		KEY_ALG_DEFAULT,
		ROOT_KEY_LABEL,
		strlen(ROOT_KEY_LABEL),
		KEY_SIZE_DEFAULT,
		&rootPubKey,
		CSSM_FALSE,			// pubIsRef
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&rootPrivKey,
		CSSM_TRUE,			// privIsRef - doesn't matter
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		goto abort;
	}

	for(testNum=0; testNum<NUM_TEST_CASES; testNum++) {
		testCase = &testCases[testNum];
		if(!quiet) {
			printf("%s\n", testCase->testDesc);
		}
		crtn = genCerts(clHand, cspHand, tpHand,
			&rootPrivKey, &rootPubKey, &subjPubKey,
			testCase->certIpAddr, testCase->certDnsName, testCase->commonName,
			rootCert, subjCert);
		BlobList leaf;
		BlobList root;
		/* BlobList uses regular free() on the referent of the blobs */
		leaf.addBlob(subjCert, CSSM_TRUE);
		root.addBlob(rootCert, CSSM_TRUE);
		if(crtn) {
			if(testError(quiet)) {
				break;
			}
		}
		if(writeCerts) {
			if(writeFile(CERT_FILE, subjCert.Data, subjCert.Length)) {
				printf("***Error writing cert to %s\n", CERT_FILE);
			}
			else {
				printf("...wrote %lu bytes to %s\n", subjCert.Length, CERT_FILE);
			}
		}
		vfyRtn = certVerifySimple(tpHand, clHand, cspHand,
			leaf, root,
			CSSM_FALSE,		// useSystemAnchors
			CSSM_FALSE,		// leafCertIsCA
			CSSM_FALSE,		// allow expired root
			CVP_SSL,
			testCase->vfyHostName,
			CSSM_FALSE,		// sslClient
			NULL,
			NULL,
			testCase->expectErrStr,
			testCase->certErrorStr ? 1 : 0,
			testCase->certErrorStr ? (const char **)&testCase->certErrorStr :
				NULL,
			0, NULL,		// certStatus
			CSSM_FALSE,		// trustSettings
			quiet,
			verbose);
		if(vfyRtn) {
			if(testError(quiet)) {
				break;
			}
		}
		/* cert data freed by ~BlobList */
	}

	/* free keys */
	cspFreeKey(cspHand, &rootPubKey);
	cspFreeKey(cspHand, &rootPrivKey);
	cspFreeKey(cspHand, &subjPubKey);
	cspFreeKey(cspHand, &subjPrivKey);

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
	if(!vfyRtn && !crtn && !quiet) {
		printf("...test passed\n");
	}
	return 0;
}


