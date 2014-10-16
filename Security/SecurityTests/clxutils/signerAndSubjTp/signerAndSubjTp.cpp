/* Copyright (c) 1998-2003,2005-2006,2008 Apple Inc.
 *
 * signerAndSubjTp.c
 *
 * Create two certs - a root, and a subject cert signed by the root. Includes
 * extension construction. Verify certs every which way, including various expected
 * failures. This version uses CSSM_TP_SubmitCredRequest to create the certs.
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
#define ROOT_KEY_FILE_NAME		"ssRootKey.der"
#define SUBJ_KEY_FILE_NAME		"ssSubjKey.der"

/* public key in ref form, TP supports this as of 1/30/02 */
#define PUB_KEY_IS_REF			CSSM_TRUE

#define SERIAL_DEFAULT			0x12345678

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("    w[rite certs and keys]\n");
	printf("    a=alg  where alg is s(RSA/SHA1), m(RSA/MD5), f(FEE/MD5), F(FEE/SHA1),\n");
	printf("           2(RSA/SHA224), 6(RSA/SHA256), 3(RSA/SHA384) 5=RSA/SHA512,\n");
	printf("           e(ECDSA), E(ANSI/ECDSA), 7(ECDSA/SHA256), 8(ECDSA/SHA384), 9(ECDSA/SHA512)\n");
	printf("    k=keySizeInBits\n");
	printf("    c=commonName (for SSL compatible subject name)\n");
	printf("    P (loop and pause for malloc debug)\n");
	printf("Extension options:\n");
	printf("    t=authorityKeyName     -- AuthorityKeyID, generalNames, DNSName plus s/n variant\n");
	printf("    s                      -- SubjectKey, data = aabbccddeeff\n");
	printf("    e=emailAddress         -- subjectAltName, RFC822Name variant\n");
	printf("    i=issuerAltName        -- DNSName variant\n");
	printf("    r=crlDistributionPoint -- dpn, URI variant\n");
	printf("    u=authorityInfoAccess  -- OCSP, DNSName variant\n");
	printf("    p=certPolicyString     -- CertPolicies, id_cps variant\n");
	printf("    n=netscapeCertType     -- NetscapeCertType, specify an integer\n");
	printf("    N=serialNumber         -- in decimal, default is 0x%x\n", SERIAL_DEFAULT);
	exit(1);
}

/*
 * RDN components for root, subject
 */
static CSSM_APPLE_TP_NAME_OID rootRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	{ "The Big Cheesy Debug Root",		&CSSMOID_CommonName }
};
#define NUM_ROOT_NAMES	(sizeof(rootRdn) / sizeof(CSSM_APPLE_TP_NAME_OID))

static CSSM_APPLE_TP_NAME_OID subjRdn[] = 
{
	/* note extra space for normalize test */
	{ "Apple  Computer",				&CSSMOID_OrganizationName },
	/* this can get overridden by cmd line */
	{ "Doug Mitchell",					&CSSMOID_CommonName }
};
#define NUM_SUBJ_NAMES	(sizeof(subjRdn) / sizeof(CSSM_APPLE_TP_NAME_OID))

static CSSM_BOOL compareKeyData(const CSSM_KEY *key1, const CSSM_KEY *key2);
static CSSM_RETURN verifyCert(CSSM_CL_HANDLE clHand,
	CSSM_CSP_HANDLE	cspHand,
	CSSM_DATA_PTR	cert,
	CSSM_DATA_PTR	signerCert,
	CSSM_KEY_PTR	key,
	CSSM_ALGORITHMS	sigAlg,
	CSSM_RETURN		expectResult,
	const char 		*opString);

/* 
 * Cook up trivial CE_GeneralName, one component of specified NameType.
 */
static void makeGeneralName(
	CE_GeneralName		*genName,		/* locally declared, persistent */
	char				*str,
	CE_GeneralNameType	nameType)		/* GNT_RFC822Name, etc. */
{
	genName->nameType = nameType;
	genName->berEncoded = CSSM_FALSE;
	genName->name.Data = (uint8 *)str;
	genName->name.Length = strlen(str);
}

/*
 * Cook up a trivial CE_GeneralNames, one component of specified NameType.
 */
static void makeGeneralNames(
	CE_GeneralNames		*genNames,		/* pointer from CE_DataAndType */
	CE_GeneralName		*genName,		/* locally declared, persistent */
	char				*str,
	CE_GeneralNameType	nameType)		/* GNT_RFC822Name, etc. */
{
	genNames->numNames = 1;
	genNames->generalName = genName;
	makeGeneralName(genName, str, nameType);
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
	CSSM_KEY_PTR	extractRootKey;	// from CSSM_CL_CertGetKeyInfo()
	CSSM_KEY_PTR	extractSubjKey;	// ditto
	unsigned		badByte;
	int				arg;
	unsigned		errorCount = 0;
	CSSM_DATA		refId;			// mallocd by CSSM_TP_SubmitCredRequest
	CSSM_APPLE_TP_CERT_REQUEST	certReq;
	CSSM_TP_REQUEST_SET			reqSet;
	sint32						estTime;
	CSSM_BOOL					confirmRequired;
	CSSM_TP_RESULT_SET_PTR		rootResultSet;
	CSSM_TP_RESULT_SET_PTR		subjResultSet;
	CSSM_ENCODED_CERT			*rootEncCert;
	CSSM_ENCODED_CERT			*subjEncCert;
	CSSM_TP_CALLERAUTH_CONTEXT 	CallerAuthContext;
	CSSM_FIELD					policyId;
	
	/* extension support */
	CE_GeneralName				sanGenName;		/* subjectAltName */
	CE_GeneralName				ianGenName;		/* issuerAltName */
	CE_DistributionPointName	distPointName;
	CE_GeneralName				crlGenName;
	CE_GeneralNames				crlGenNames;
	CE_CRLDistributionPoint		cdp;
	CE_AccessDescription		accessDescr;
	CE_GeneralNames				authKeyIdGenNames;
	CE_GeneralName				authKeyIdGenName;
	uint8						authKeyIdSerial[4] = {0x22, 0x33, 0x44, 0x55 };
	uint8						subjKeyIdData[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
	CE_PolicyQualifierInfo		polQualInfo;
	CE_PolicyInformation		polInfo;
	
	/* user-spec'd variables */
	CSSM_BOOL		writeBlobs = CSSM_FALSE;
	CSSM_ALGORITHMS	keyAlg = KEY_ALG_DEFAULT;
	CSSM_ALGORITHMS sigAlg = SIG_ALG_DEFAULT;
	CSSM_OID		sigOid = SIG_OID_DEFAULT;
	uint32			keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	char			*subjectEmail = NULL;			// for S/MIME subjectAltName
	char			*issuerAltName = NULL;
	char			*crlDistPoint = NULL;
	char			*authorityInfoAccess = NULL;
	char			*authKeyIdName = NULL;
	bool			subjectKeyId = false;
	char			*certPoliciesStr = NULL;
	bool			netscapeTypeSpec = false;
	uint16			netscapeType = 0;
	uint32			serialNumber = SERIAL_DEFAULT;
	bool			loopPause = false;
	
	/* 
	 * Extensions. Subject at least one (KeyUsage).
	 * Root has KeyUsage and BasicConstraints.
	 */
	CE_DataAndType 			exts[8];
	CE_DataAndType			*extp = &exts[0];
	
	for(arg=1; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'w':
				writeBlobs = CSSM_TRUE;
				break;
			case 'a':
				if((argv[arg][1] == '\0') || (argv[arg][2] == '\0')) {
					usage(argv);
				}
				switch(argv[arg][2]) {
					case 's':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA1WithRSA;
						sigOid = CSSMOID_SHA1WithRSA;
						break;
					case 'm':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_MD5WithRSA;
						sigOid = CSSMOID_MD5WithRSA;
						break;
					case '2':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA224WithRSA;
						sigOid = CSSMOID_SHA224WithRSA;
						break;
					case '6':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA256WithRSA;
						sigOid = CSSMOID_SHA256WithRSA;
						break;
					case '3':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA384WithRSA;
						sigOid = CSSMOID_SHA384WithRSA;
						break;
					case '5':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA512WithRSA;
						sigOid = CSSMOID_SHA512WithRSA;
						break;
					case 'f':
						keyAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_FEE_MD5;
						sigOid = CSSMOID_APPLE_FEE_MD5;
						break;
					case 'F':
						keyAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_FEE_SHA1;
						sigOid = CSSMOID_APPLE_FEE_SHA1;
						break;
					case 'e':
						keyAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_SHA1WithECDSA;
						sigOid = CSSMOID_APPLE_ECDSA;
						break;
					case 'E':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA1WithECDSA;
						sigOid = CSSMOID_ECDSA_WithSHA1;
						break;
					case '7':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA256WithECDSA;
						sigOid = CSSMOID_ECDSA_WithSHA256;
						break;
					case '8':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA384WithECDSA;
						sigOid = CSSMOID_ECDSA_WithSHA384;
						break;
					case '9':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA512WithECDSA;
						sigOid = CSSMOID_ECDSA_WithSHA512;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'k':
				keySizeInBits = atoi(&argv[arg][2]);
				break;
			case 'e':
				subjectEmail = &argv[arg][2];
				break;
			case 'i':
				issuerAltName = &argv[arg][2];
				break;
			case 'r':
				crlDistPoint = &argv[arg][2];
				break;
			case 'u':
				authorityInfoAccess = &argv[arg][2];
				break;
			case 't':
				authKeyIdName = &argv[arg][2];
				break;
			case 's':
				subjectKeyId = true;
				break;
			case 'p':
				certPoliciesStr =  &argv[arg][2];
				break;
			case 'n':
				netscapeTypeSpec = true;
				netscapeType = atoi(&argv[arg][2]);
				break;
			case 'c':
				subjRdn[NUM_SUBJ_NAMES-1].string = &argv[arg][2];
				break;
		    case 'N':
				serialNumber = atoi(&argv[arg][2]);
				break;
			case 'P':
				loopPause = true;
				break;
			default:
				usage(argv);
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
		CSSM_FALSE,								// pubIsRef 
		#endif
		CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_ENCRYPT,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&subjPrivKey,
		writeBlobs ? CSSM_FALSE : CSSM_TRUE,	// privIsRef
		CSSM_KEYUSE_SIGN | CSSM_KEYUSE_DECRYPT,
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
		writeBlobs ? CSSM_FALSE : CSSM_TRUE,	// privIsRef
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		errorCount++;
		goto abort;
	}

	if(compareKeyData(&rootPubKey, &subjPubKey)) {
	 	printf("**WARNING: Identical root and subj keys!\n");
	}
	
	if(writeBlobs) {
		writeFile(ROOT_KEY_FILE_NAME, rootPrivKey.KeyData.Data, 
			rootPrivKey.KeyData.Length);
		printf("...wrote %lu bytes to %s\n", rootPrivKey.KeyData.Length, 
			ROOT_KEY_FILE_NAME);
		writeFile(SUBJ_KEY_FILE_NAME, subjPrivKey.KeyData.Data, 
			subjPrivKey.KeyData.Length);
		printf("...wrote %lu bytes to %s\n", subjPrivKey.KeyData.Length, 
			SUBJ_KEY_FILE_NAME);
	}
	
	if(loopPause) {
		fpurge(stdin);
		printf("pausing before root CSSM_TP_SubmitCredRequest; CR to continue:");
		getchar();
	}
loopTop:
	
	/* A KeyUsage extension for both certs */
	exts[0].type = DT_KeyUsage;
	exts[0].critical = CSSM_FALSE;
	exts[0].extension.keyUsage = CE_KU_DigitalSignature | CE_KU_KeyCertSign;

	/* root - BasicConstraints extensions */
	exts[1].type = DT_BasicConstraints;
	exts[1].critical = CSSM_TRUE;
	exts[1].extension.basicConstraints.cA = CSSM_TRUE;
	exts[1].extension.basicConstraints.pathLenConstraintPresent = CSSM_TRUE;
	exts[1].extension.basicConstraints.pathLenConstraint = 2;

	/* certReq for root */
	memset(&certReq, 0, sizeof(CSSM_APPLE_TP_CERT_REQUEST));
	certReq.cspHand = cspHand;
	certReq.clHand = clHand;
	certReq.serialNumber = serialNumber;
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
	certReq.extensions = exts;
	
	reqSet.NumberOfRequests = 1;
	reqSet.Requests = &certReq;
	
	/* a big CSSM_TP_CALLERAUTH_CONTEXT just to specify an OID */
	memset(&CallerAuthContext, 0, sizeof(CSSM_TP_CALLERAUTH_CONTEXT));
	memset(&policyId, 0, sizeof(CSSM_FIELD));
	policyId.FieldOid = CSSMOID_APPLE_TP_LOCAL_CERT_GEN;
	CallerAuthContext.Policy.NumberOfPolicyIds = 1;
	CallerAuthContext.Policy.PolicyIds = &policyId;
	
	/* generate root cert */
	printf("Creating root cert...\n");
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
		&rootResultSet);
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult", crtn);
		errorCount++;
		goto abort;
	}
	CSSM_FREE(refId.Data);
	if(rootResultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult returned NULL result set.\n");
		errorCount++;
		goto abort;
	}
	rootEncCert = (CSSM_ENCODED_CERT *)rootResultSet->Results;
	signedRootCert = rootEncCert->CertBlob;
	if(writeBlobs) {
		writeFile(ROOT_CERT_FILE_NAME, signedRootCert.Data, signedRootCert.Length);
		printf("...wrote %lu bytes to %s\n", signedRootCert.Length, 
			ROOT_CERT_FILE_NAME);
	}

	if(loopPause) {
		fpurge(stdin);
		printf("pausing before subject CSSM_TP_SubmitCredRequest; CR to continue:");
		getchar();
	}

	/* now a subject cert signed by the root cert */
	printf("Creating subject cert...\n");
	certReq.serialNumber = serialNumber + 1;
	certReq.numSubjectNames = NUM_SUBJ_NAMES;
	certReq.subjectNames = subjRdn;
	certReq.numIssuerNames = NUM_ROOT_NAMES;
	certReq.issuerNames = rootRdn;
	certReq.certPublicKey = &subjPubKey;
	certReq.issuerPrivateKey = &rootPrivKey;
	certReq.numExtensions = 1;
	certReq.extensions = exts;
	
	/* subject cert extensions - at least KeyUsage, maybe more */
	exts[0].type = DT_KeyUsage;
	exts[0].critical = CSSM_FALSE;
	exts[0].extension.keyUsage = 
		CE_KU_DigitalSignature | CE_KU_DataEncipherment | CE_KU_KeyAgreement;
	extp = &exts[1];
	
	if(subjectEmail) {
		/* subjectAltName extension */
		makeGeneralNames(&extp->extension.subjectAltName, &sanGenName,
			subjectEmail, GNT_RFC822Name);
		extp->type = DT_SubjectAltName;
		extp->critical = CSSM_FALSE;
		certReq.numExtensions++;
		extp++;
	}

	if(authKeyIdName) {
		/* AuthorityKeyID extension */
		makeGeneralNames(&authKeyIdGenNames, &authKeyIdGenName,
			authKeyIdName, GNT_DNSName);
		CE_AuthorityKeyID *akid = &extp->extension.authorityKeyID;
		memset(akid, 0, sizeof(*akid));
		akid->generalNamesPresent = CSSM_TRUE;
		akid->generalNames = &authKeyIdGenNames;
		akid->serialNumberPresent = CSSM_TRUE;
		akid->serialNumber.Data = authKeyIdSerial;
		akid->serialNumber.Length = sizeof(authKeyIdSerial);

		extp->type = DT_AuthorityKeyID;
		extp->critical = CSSM_FALSE;
		certReq.numExtensions++;
		extp++;
	}
	
	if(subjectKeyId) {
		/* SubjectKeyID extension */
		CSSM_DATA *skid = &extp->extension.subjectKeyID;
		skid->Data = subjKeyIdData;
		skid->Length = sizeof(subjKeyIdData);
		
		extp->type = DT_SubjectKeyID;
		extp->critical = CSSM_FALSE;
		certReq.numExtensions++;
		extp++;
	}
	
	if(issuerAltName) {
		/* issuerAltName extension */
		makeGeneralNames(&extp->extension.issuerAltName, &ianGenName,
			issuerAltName, GNT_DNSName);
		extp->type = DT_IssuerAltName;
		extp->critical = CSSM_FALSE;
		certReq.numExtensions++;
		extp++;
	}
	
	if(crlDistPoint) {
		/* CRLDistributionPoints extension */
		memset(&distPointName, 0, sizeof(distPointName));
		makeGeneralNames(&crlGenNames, &crlGenName,
			crlDistPoint, GNT_URI);
		distPointName.nameType = CE_CDNT_FullName;
		distPointName.dpn.fullName = &crlGenNames;
		
		cdp.distPointName = &distPointName;
		cdp.reasonsPresent = CSSM_FALSE;
		cdp.reasons = 0;
		cdp.crlIssuer = NULL;

		CE_CRLDistPointsSyntax	*dps = &extp->extension.crlDistPoints;
		dps->numDistPoints = 1;
		dps->distPoints = &cdp;
		extp->type = DT_CrlDistributionPoints;
		extp->critical = CSSM_FALSE;
		
		certReq.numExtensions++;
		extp++;
	}

	if(authorityInfoAccess) {
		/* AuthorityInfoAccess extension */
		CE_AuthorityInfoAccess *cad = &extp->extension.authorityInfoAccess;
		cad->numAccessDescriptions = 1;
		cad->accessDescriptions = &accessDescr;
		makeGeneralName(&accessDescr.accessLocation, authorityInfoAccess,
			GNT_DNSName);
		accessDescr.accessMethod = CSSMOID_AD_OCSP;
		extp->type = DT_AuthorityInfoAccess;
		extp->critical = CSSM_FALSE;
		certReq.numExtensions++;
		extp++;
	}

	if(certPoliciesStr) {
		/* Cert Policies extension */
		CE_CertPolicies *cp = &extp->extension.certPolicies;
		cp->numPolicies = 1;
		cp->policies = &polInfo;
		/* just make this policy OID up */
		polInfo.certPolicyId = CSSMOID_APPLE_TP_PKINIT_CLIENT;
		polInfo.numPolicyQualifiers = 1;
		polInfo.policyQualifiers = &polQualInfo;
		polQualInfo.policyQualifierId = CSSMOID_QT_CPS;
		polQualInfo.qualifier.Data = (uint8 *)certPoliciesStr;
		polQualInfo.qualifier.Length = strlen(certPoliciesStr);

		extp->type = DT_CertPolicies;
		extp->critical = CSSM_FALSE;
		certReq.numExtensions++;
		extp++;
	}
	
	if(netscapeTypeSpec) {
		/* NetscapeCertType extension */
		extp->extension.netscapeCertType = netscapeType;
		extp->type = DT_NetscapeCertType;
		extp->critical = CSSM_FALSE;
		certReq.numExtensions++;
		extp++;
	}
	
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
		&subjResultSet);
	if(crtn) {
		printError("CSSM_TP_RetrieveCredResult (2)", crtn);
		errorCount++;
		goto abort;
	}
	CSSM_FREE(refId.Data);
	if(subjResultSet == NULL) {
		printf("***CSSM_TP_RetrieveCredResult (2) returned NULL result set.\n");
		errorCount++;
		goto abort;
	}
	subjEncCert = (CSSM_ENCODED_CERT *)subjResultSet->Results;
	signedSubjCert = subjEncCert->CertBlob;

	if(writeBlobs) {
		writeFile(SUBJ_CERT_FILE_NAME, signedSubjCert.Data, signedSubjCert.Length);
		printf("...wrote %lu bytes to %s\n", signedSubjCert.Length, 
			SUBJ_CERT_FILE_NAME);
	}

	/*
	 * Extract public keys from the two certs, verify.
	 */
	crtn = CSSM_CL_CertGetKeyInfo(clHand, &signedSubjCert, &extractSubjKey);
	if(crtn) {
		printError("CSSM_CL_CertGetKeyInfo", crtn);
	}
	else {
		/* compare key data - header is different.
		 * Known header differences:
		 *  -- CspID - CSSM_CL_CertGetKeyInfo returns a key with NULL for
		 *     this field
		 * --  Format. rootPubKey      : 6 (CSSM_KEYBLOB_RAW_FORMAT_BSAFE)
		 *             extractRootKey  : 1 (CSSM_KEYBLOB_RAW_FORMAT_PKCS1)
		 * --  KeyAttr. rootPubKey     : 0x20 (CSSM_KEYATTR_EXTRACTABLE)
		 *              extractRootKey : 0x0
		 */
		if(!compareKeyData(extractSubjKey, &subjPubKey)) {
			printf("***CSSM_CL_CertGetKeyInfo(signedSubjCert) returned bad key data\n");
		}
		if(extractSubjKey->KeyHeader.LogicalKeySizeInBits !=
				subjPubKey.KeyHeader.LogicalKeySizeInBits) {
			printf("***EffectiveKeySizeInBits mismatch: extract %u subj %u\n",
				(unsigned)extractSubjKey->KeyHeader.LogicalKeySizeInBits,
				(unsigned)subjPubKey.KeyHeader.LogicalKeySizeInBits);
		}
	}
	crtn = CSSM_CL_CertGetKeyInfo(clHand, &signedRootCert, &extractRootKey);
	if(crtn) {
		printError("CSSM_CL_CertGetKeyInfo", crtn);
	}
	else {
		if(!compareKeyData(extractRootKey, &rootPubKey)) {
			printf("***CSSM_CL_CertGetKeyInfo(signedRootCert) returned bad key data\n");
		}
	}

	/*
	 * Verify:
	 */
	printf("Verifying certificates...\n");
	
	/*
	 *  Verify root cert by root pub key, should succeed.
	 */
	if(verifyCert(clHand,
			cspHand,
			&signedRootCert,
			NULL,
			&rootPubKey,
			sigAlg,
			CSSM_OK,
			"Verify(root by root key)")) {
		errorCount++;
		/* continue */
	}
	
	/*
	 *  Verify root cert by root cert, should succeed.
	 */
	if(verifyCert(clHand,
			cspHand,
			&signedRootCert,
			&signedRootCert,
			NULL,
			CSSM_ALGID_NONE,			// sigAlg not used here
			CSSM_OK,
			"Verify(root by root cert)")) {
		errorCount++;
		/* continue */
	}


	/*
	 *  Verify subject cert by root pub key, should succeed.
	 */
	if(verifyCert(clHand,
			cspHand,
			&signedSubjCert,
			NULL,
			&rootPubKey,
			sigAlg,
			CSSM_OK,
			"Verify(subj by root key)")) {
		errorCount++;
		/* continue */
	}

	/*
	 *  Verify subject cert by root cert, should succeed.
	 */
	if(verifyCert(clHand,
			cspHand,
			&signedSubjCert,
			&signedRootCert,
			NULL,
			CSSM_ALGID_NONE,			// sigAlg not used here
			CSSM_OK,
			"Verify(subj by root cert)")) {
		errorCount++;
		/* continue */
	}

	/*
	 *  Verify subject cert by root cert AND key, should succeed.
	 */
	if(verifyCert(clHand,
			cspHand,
			&signedSubjCert,
			&signedRootCert,
			&rootPubKey,
			sigAlg,
			CSSM_OK,
			"Verify(subj by root cert and key)")) {
		errorCount++;
		/* continue */
	}

	/*
	 *  Verify subject cert by extracted root pub key, should succeed.
	 */
	if(verifyCert(clHand,
			cspHand,
			&signedSubjCert,
			NULL,
			extractRootKey,
			sigAlg,
			CSSM_OK,
			"Verify(subj by extracted root key)")) {
		errorCount++;
		/* continue */
	}

	/*
	 *  Verify subject cert by subject pub key, should fail.
	 */
	if(verifyCert(clHand,
			cspHand,
			&signedSubjCert,
			NULL,
			&subjPubKey,
			sigAlg,
			CSSMERR_CL_VERIFICATION_FAILURE,
			"Verify(subj by subj key)")) {
		errorCount++;
		/* continue */
	}

	/*
	 *  Verify subject cert by subject cert, should fail.
	 */
	if(verifyCert(clHand,
			cspHand,
			&signedSubjCert,
			&signedSubjCert,
			NULL,
			CSSM_ALGID_NONE,			// sigAlg not used here
			CSSMERR_CL_VERIFICATION_FAILURE,
			"Verify(subj by subj cert)")) {
		errorCount++;
		/* continue */
	}

	/*
	 *  Verify erroneous subject cert by root pub key, should fail.
	 */
	badByte = genRand(1, signedSubjCert.Length - 1);
	signedSubjCert.Data[badByte] ^= 0x55;
	if(verifyCert(clHand,
			cspHand,
			&signedSubjCert,
			NULL,
			&rootPubKey,
			sigAlg,
			CSSMERR_CL_VERIFICATION_FAILURE,
			"Verify(bad subj by root key)")) {
		errorCount++;
		/* continue */
	}


	/* free/delete certs and keys */
	CSSM_FREE(signedSubjCert.Data);
	CSSM_FREE(signedRootCert.Data);
	CSSM_FREE(rootEncCert);
	CSSM_FREE(subjEncCert);
	CSSM_FREE(rootResultSet);
	CSSM_FREE(subjResultSet);

	/* These don't work because CSSM_CL_CertGetKeyInfo() gives keys with
	 * a bogus GUID. This may be a problem with the Apple CSP...
	 *
	cspFreeKey(cspHand, extractRootKey);
	cspFreeKey(cspHand, extractSubjKey);
	 *
	 * do it this way instead...*/
	CSSM_FREE(extractRootKey->KeyData.Data);
	CSSM_FREE(extractSubjKey->KeyData.Data);

	/* need to do this regardless...*/
	CSSM_FREE(extractRootKey);
	CSSM_FREE(extractSubjKey);

	if(loopPause) {
		fpurge(stdin);
		printf("pausing at end of loop; CR to continue:");
		getchar();
		goto loopTop;
	}

abort:
	cspFreeKey(cspHand, &rootPubKey);
	cspFreeKey(cspHand, &subjPubKey);

	if(cspHand != 0) {
		CSSM_ModuleDetach(cspHand);
	}
	if(clHand != 0) {
		CSSM_ModuleDetach(clHand);
	}
	if(tpHand != 0) {
		CSSM_ModuleDetach(tpHand);
	}

	if(errorCount) {
		printf("Signer/Subject test failed with %d errors\n", errorCount);
	}
	else {
		printf("Signer/Subject test succeeded\n");
	}
	return 0;
}


/* compare KeyData for two keys. */
static CSSM_BOOL compareKeyData(const CSSM_KEY *key1, const CSSM_KEY *key2)
{
	if(key1->KeyData.Length != key2->KeyData.Length) {
		return CSSM_FALSE;
	}
	if(memcmp(key1->KeyData.Data,
			key2->KeyData.Data,
			key1->KeyData.Length)) {
		return CSSM_FALSE;
	}
	return CSSM_TRUE;
}

/* verify a cert using specified key and/or signerCert */
static CSSM_RETURN verifyCert(CSSM_CL_HANDLE clHand,
	CSSM_CSP_HANDLE	cspHand,
	CSSM_DATA_PTR	cert,
	CSSM_DATA_PTR	signerCert,		// optional
	CSSM_KEY_PTR	key,			// ditto, to work spec one, other, or both
	CSSM_ALGORITHMS	sigAlg,			// CSSM_ALGID_SHA1WithRSA, etc. 
	CSSM_RETURN		expectResult,
	const char 		*opString)
{
	CSSM_RETURN		crtn;
	CSSM_CC_HANDLE	signContext = CSSM_INVALID_HANDLE;

	if(key) {
		crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				sigAlg,
				NULL,				// AccessCred
				key,
				&signContext);
		if(crtn) {
			printf("Failure during %s\n", opString);
			printError("CSSM_CSP_CreateSignatureContext", crtn);
			return crtn;
		}
	}
	crtn = CSSM_CL_CertVerify(clHand,
		signContext,
		cert,					// CertToBeVerified
		signerCert,				// SignerCert
		NULL,					// VerifyScope 
		0);						// ScopeSize
		
	/* Hack to accomodate ECDSA returning CSSMERR_CSP_INVALID_SIGNATURE - a more detailed */
	if(crtn != expectResult) {
		printf("Failure during %s\n", opString);
		if(crtn == CSSM_OK) {
			printf("Unexpected CSSM_CL_CertVerify success\n");
		}
		else if(expectResult == CSSM_OK) {
			printError("CSSM_CL_CertVerify", crtn);
		}
		else {
			printError("CSSM_CL_CertVerify: expected", expectResult);
			printError("CSSM_CL_CertVerify: got     ", crtn);
		}
		return CSSMERR_CL_VERIFICATION_FAILURE;
	}
	if(signContext != CSSM_INVALID_HANDLE) {
		crtn = CSSM_DeleteContext(signContext);
		if(crtn) {
			printf("Failure during %s\n", opString);
			printError("CSSM_DeleteContext", crtn);
			return crtn;
		}
	}
	return CSSM_OK;
}
