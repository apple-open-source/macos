/* Copyright (c) 2006 Apple Computer, Inc.
 *
 * certSerialEncodeTest.cpp
 *
 * Verify proper encoding of unsigned integer as a DER_encoded signed integer.
 * Verifies Radar 4471281.
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

#define SUBJ_KEY_LABEL		"subjectKey"
#define ROOT_KEY_LABEL		"rootKey"
/* default key and signature algorithm */
#define SIG_ALG_DEFAULT		CSSM_ALGID_SHA1WithRSA
#define SIG_OID_DEFAULT		CSSMOID_SHA1WithRSA
#define KEY_ALG_DEFAULT		CSSM_ALGID_RSA

/* for write certs/keys option */
#define ROOT_CERT_FILE_NAME		"ssRootCert.cer"
#define SUBJ_CERT_FILE_NAME		"ssSubjCert.cer"

/* public key in ref form, TP supports this as of 1/30/02 */
#define PUB_KEY_IS_REF			CSSM_TRUE

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("    w[rite certs]\n");
	printf("    p(ause for MallocDebug)\n");
	printf("    q(uiet)\n");
	exit(1);
}

/*
 * RDN components
 */
static CSSM_APPLE_TP_NAME_OID rootRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	{ "The Big Cheesy Debug Root",		&CSSMOID_CommonName }
};
#define NUM_ROOT_NAMES	(sizeof(rootRdn) / sizeof(CSSM_APPLE_TP_NAME_OID))

/* test cases */
typedef struct {
	uint32		serialIn;		/* --> CSSM_TP_SubmitCredRequest */
	CSSM_SIZE	expectLen;	
	const uint8	*expect;
} SerialNumber;

/* 0x7f */
static const uint8 sn0_Data[1] = {0x7f};
static const SerialNumber sn0 = {0x7f, 1, sn0_Data };

/* 0x80 */
static const uint8 sn1_Data[2] = {0x00, 0x80};
static const SerialNumber sn1 = {0x80, 2, sn1_Data };

/* 0x7ff */
static const uint8 sn2_Data[2] = {0x07, 0xff};
static const SerialNumber sn2 = {0x7ff, 2, sn2_Data };

/* 0x80ff */
static const uint8 sn3_Data[3] = {0x00, 0x80, 0xff};
static const SerialNumber sn3 = {0x80ff, 3, sn3_Data };

/* 0xfffffff */
static const uint8 sn4_Data[4] = {0x0f, 0xff, 0xff, 0xff};
static const SerialNumber sn4 = {0xfffffff, 4, sn4_Data };

/* 0x0fffffff */
static const uint8 sn5_Data[4] = {0x0f, 0xff, 0xff, 0xff};
static const SerialNumber sn5 = {0x0fffffff, 4, sn5_Data };

/* 0x80000000 */
static const uint8 sn6_Data[5] = {0x00, 0x80, 0x00, 0x00, 0x00};
static const SerialNumber sn6 = {0x80000000, 5, sn6_Data };

static const SerialNumber *serialNumbers[] = {
	&sn0, &sn1, &sn2, &sn3, &sn4, &sn5, &sn6
};
#define NUM_SERIAL_NUMS (sizeof(serialNumbers) / sizeof(serialNumbers[0]))

static int doTest(
	CSSM_CL_HANDLE	clHand,			// CL handle
	CSSM_CSP_HANDLE	cspHand,		// CSP handle
	CSSM_TP_HANDLE	tpHand,			// TP handle
	CSSM_KEY_PTR	subjPubKey,
	CSSM_KEY_PTR	signerPrivKey,
	uint32			serialNumIn,
	CSSM_SIZE		serialNumExpLen,
	const uint8		*serialNumExp,
	CSSM_BOOL		quiet,
	CSSM_BOOL 		writeBlobs)
{
	CSSM_DATA					refId;			// mallocd by CSSM_TP_SubmitCredRequest
	CSSM_APPLE_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET			reqSet;
	sint32						estTime;
	CSSM_BOOL					confirmRequired;
	CSSM_TP_RESULT_SET_PTR		resultSet;
	CSSM_ENCODED_CERT			*encCert;
	CSSM_TP_CALLERAUTH_CONTEXT 	CallerAuthContext;
	CSSM_FIELD					policyId;
	CSSM_RETURN					crtn;
	CSSM_DATA					*signedRootCert;
	int							ourRtn = 0;
	CSSM_DATA_PTR 				foundSerial = NULL;
	CSSM_HANDLE 				resultHand = 0;
	uint32 						numFields;

	/* certReq for root */
	memset(&certReq, 0, sizeof(CSSM_APPLE_TP_CERT_REQUEST));
	certReq.cspHand = cspHand;
	certReq.clHand = clHand;
	certReq.serialNumber = serialNumIn;
	certReq.numSubjectNames = NUM_ROOT_NAMES;
	certReq.subjectNames = rootRdn;
	certReq.numIssuerNames = 0;
	certReq.issuerNames = NULL;
	certReq.certPublicKey = subjPubKey;
	certReq.issuerPrivateKey = signerPrivKey;
	certReq.signatureAlg = CSSM_ALGID_SHA1WithRSA;
	certReq.signatureOid = CSSMOID_SHA1WithRSA;
	certReq.notBefore = 0;			// now
	certReq.notAfter = 10000;		// seconds from now
	certReq.numExtensions = 0;
	certReq.extensions = NULL;
	
	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;
	
	/* a big CSSM_TP_CALLERAUTH_CONTEXT just to specify an OID */
	memset(&CallerAuthContext, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	memset(&policyId, 0, sizeof(CSSM_FIELD));
	policyId.FieldOid = CSSMOID_APPLE_TP_LOCAL_CERT_GEN;
	CallerAuthContext.Policy.NumberOfPolicyIds = 1;
	CallerAuthContext.Policy.PolicyIds = &policyId;
	
	/* generate root cert */
	if(!quiet) {
		printf("Creating root cert...\n");
	}
	crtn = CSSM_TP_SubmitCredRequest(tpHand,
		NULL,				// PreferredAuthority
		CSSM_TP_AUTHORITY_REQUEST_CERTISSUE,
		&reqSet,
		&CallerAuthContext,	
		&estTime,
		&refId);
	if(crtn) {
		printError("CSSM_TP_SubmitCredRequest", crtn);
		ourRtn = -1;
		goto errOut;
	}
	crtn = CSSM_TP_RetrieveCredResult(tpHand,
		&refId,
		NULL,				// CallerAuthCredentials
		&estTime,
		&confirmRequired,
		&resultSet);
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult", crtn);
		ourRtn = -1;
		goto errOut;
	}
	if(resultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult returned NULL result set.\n");
		ourRtn = -1;
		goto errOut;
	}
	encCert = (CSSM_ENCODED_CERT *)resultSet->Results;
	signedRootCert = &encCert->CertBlob;
	if(writeBlobs) {
		writeFile(ROOT_CERT_FILE_NAME, signedRootCert->Data, signedRootCert->Length);
		printf("...wrote %lu bytes to %s\n", signedRootCert->Length, 
			ROOT_CERT_FILE_NAME);
	}

	/* make sure it self-verifies */
	crtn = CSSM_CL_CertVerify(clHand, 0 /* CCHandle */,
		signedRootCert, signedRootCert,
		NULL, 0);
	if(crtn) {
		cssmPerror("CSSM_CL_CertVerify", crtn);
		printf("***Created cert does not self-verify\n");
		ourRtn = -1;
		goto errOut;
	}

	/* extract the field we're interested in verifying */
	crtn = CSSM_CL_CertGetFirstFieldValue(clHand, signedRootCert,
		&CSSMOID_X509V1SerialNumber, &resultHand, &numFields, &foundSerial);
	if(crtn) {
		cssmPerror("CSSM_CL_CertGetFirstFieldValue(serialNumber)", crtn);
		printf("***Can't obtain serial number\n");
		ourRtn = -1;
		goto errOut;
	}
	CSSM_CL_CertAbortQuery(clHand, resultHand);
	if(foundSerial->Length != serialNumExpLen) {
		printf("***expected serialNumber len 0x%lu, got 0x%lu\n",
			(unsigned long)serialNumExpLen, (unsigned long)foundSerial->Length);
		ourRtn = -1;
		goto errOut;
	}
	for(unsigned dex=0; dex<serialNumExpLen; dex++) {
		if(foundSerial->Data[dex] != serialNumExp[dex]) {
			printf("***SerialNumber mismatch at index %u: exp %02X got %02X\n",
				dex, (unsigned)serialNumExp[dex], 
				(unsigned)foundSerial->Data[dex]);
			ourRtn = -1;
		}
	}
	/* free retrieved serial number and the result set itself */
	CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SerialNumber, foundSerial);
	CSSM_FREE(signedRootCert->Data);
	CSSM_FREE(encCert);
	CSSM_FREE(resultSet);
	/* Per the spec, this is supposed to be Opaque to us and the TP is supposed to free
	 * it when it goes out of scope...but libsecurity_keychains's 
	 * CertificateRequest::submitDotMac() frees this...that would have to change
	 * in order for the TP to free this properly. Someday maybe. No big deal. 
	 */
	CSSM_FREE(refId.Data);
errOut:
	return ourRtn;
}

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE	clHand;			// CL handle
	CSSM_CSP_HANDLE	cspHand;		// CSP handle
	CSSM_TP_HANDLE	tpHand;			// TP handle
	CSSM_KEY		rootPubKey;		// root's RSA public key blob
	CSSM_KEY		rootPrivKey;	// root's RSA private key - ref format
	CSSM_RETURN		crtn;
	int				arg;
	unsigned 		dex;
	int 			ourRtn = 0;
	uint32			keySizeInBits = 512;
	CSSM_BOOL		doPause = CSSM_FALSE;

	/* user-spec'd variables */
	CSSM_BOOL		writeBlobs = CSSM_FALSE;
	CSSM_BOOL		quiet = CSSM_FALSE;

	for(arg=1; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'w':
				writeBlobs = CSSM_TRUE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'p':
				doPause = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	
	testStartBanner("certSerialEncodeTest", argc, argv);
	
	
	/* connect to CL, TP, and CSP */
	clHand = clStartup();
	if(clHand == 0) {
		return -1;
	}
	tpHand = tpStartup();
	if(tpHand == 0) {
		return -1;
	}
	cspHand = cspStartup();
	if(cspHand == 0) {
		return -1;
	}

	/* cook up key pair for self-signed cert */
	crtn = cspGenKeyPair(cspHand,
		CSSM_ALGID_RSA,
		ROOT_KEY_LABEL,
		strlen(ROOT_KEY_LABEL),
		keySizeInBits,
		&rootPubKey,
		CSSM_FALSE,			// pubIsRef - should work both ways, but not yet
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&rootPrivKey,
		writeBlobs ? CSSM_FALSE : CSSM_TRUE,	// privIsRef
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		ourRtn = -1;
		goto abort;
	}

	for(dex=0; dex<NUM_SERIAL_NUMS; dex++) {
		const SerialNumber *sn = serialNumbers[dex];
		if(!quiet) {
			printf("...testing serial number 0x%lx\n", (unsigned long)sn->serialIn);
		}
		ourRtn = doTest(clHand, cspHand, tpHand,
			&rootPubKey, &rootPrivKey,
			sn->serialIn, sn->expectLen, sn->expect,
			quiet, writeBlobs);
		if(ourRtn) {
			break;
		}
		if(doPause) {
			fpurge(stdin);
			printf("Pausing for MallocDebug. a to abort, anything else to continue: ");
			if(getchar() == 'a') {
				break;
			}
		}
	}

	cspFreeKey(cspHand, &rootPubKey);
	cspFreeKey(cspHand, &rootPrivKey);

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

	if((ourRtn == 0) && !quiet) {
		printf("certSerialEncodeTest test succeeded\n");
	}
	return ourRtn;
}


