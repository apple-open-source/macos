/* Copyright (c) 1998,2003,2005-2006,2008 Apple Inc.
 *
 * signerAndSubj.c
 *
 * Create two certs - a root, and a subject cert signed by the root. Includes
 * extension construction. Verify certs every which way, including various expected
 * failures.
 *
 * Revision History
 * ----------------
 *  31 Aug 2000 Doug Mitchell at Apple
 *		Ported to X/CDSA2.
 *  20 Jul 1998	Doug Mitchell at NeXT
 *		Created.
 */

#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/CertBuilderApp.h>
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
#define KEY_ALG_DEFAULT		CSSM_ALGID_RSA

/* for write certs/components option */
#define ROOT_CERT_FILE_NAME		"ssRootCert.der"
#define ROOT_TBS_FILE_NAME		"ssRootTBS.der"
#define SUBJ_CERT_FILE_NAME		"ssSubjCert.der"
#define SUBJ_TBS_FILE_NAME		"ssSubjTBS.der"
#define ROOT_PRIV_KEY_FILE		"ssRootPriv.der"
#define SUBJ_PRIV_KEY_FILE		"ssSubjPriv.der"

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("    w[rite certs and components]\n");
	printf("    a=alg  where alg is s(RSA/SHA1), m(RSA/MD5), f(FEE/MD5), F(FEE/SHA1),\n");
	printf("           2(RSA/SHA224), 6(RSA/SHA256), 3(RSA/SHA384) 5=RSA/SHA512,\n");
	printf("           e(ECDSA), E(ANSI/ECDSA), 7(ECDSA/SHA256), 8(ECDSA/SHA384), 9(ECDSA/SHA512)\n");
	printf("    k=keySizeInBits\n");
	exit(1);
}

/*
 * RDN components for root, subject
 */
CB_NameOid rootRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	{ "The Big Cheese",					&CSSMOID_Title }
};
#define NUM_ROOT_NAMES	(sizeof(rootRdn) / sizeof(CB_NameOid))

CB_NameOid subjRdn[] = 
{
	/* note extra space for normalize test */
	{ "Apple  Computer",				&CSSMOID_OrganizationName },
	{ "Doug Mitchell",					&CSSMOID_CommonName }
};
#define NUM_SUBJ_NAMES	(sizeof(subjRdn) / sizeof(CB_NameOid))

static CSSM_BOOL compareKeyData(const CSSM_KEY *key1, const CSSM_KEY *key2);
static CSSM_RETURN verifyCert(CSSM_CL_HANDLE clHand,
	CSSM_CSP_HANDLE	cspHand,
	CSSM_DATA_PTR	cert,
	CSSM_DATA_PTR	signerCert,
	CSSM_KEY_PTR	key,
	CSSM_ALGORITHMS	sigAlg,
	CSSM_RETURN		expectResult,
	const char 		*opString);


int main(int argc, char **argv)
{
	CSSM_CL_HANDLE	clHand;			// CL handle
	CSSM_X509_NAME	*subjName;
	CSSM_X509_NAME	*rootName;
	CSSM_X509_TIME	*notBefore;		// UTC-style "not before" time
	CSSM_X509_TIME	*notAfter;		// UTC-style "not after" time
	CSSM_DATA_PTR	rawCert;		// from CSSM_CL_CertCreateTemplate
	CSSM_DATA		signedRootCert;	// from CSSM_CL_CertSign
	CSSM_DATA		signedSubjCert;	// from CSSM_CL_CertSign
	CSSM_CSP_HANDLE	cspHand;		// CSP handle
	CSSM_KEY		subjPubKey;		// subject's RSA public key blob
	CSSM_KEY		subjPrivKey;	// subject's RSA private key - ref format
	CSSM_KEY		rootPubKey;		// root's RSA public key blob
	CSSM_KEY		rootPrivKey;	// root's RSA private key - ref format
	CSSM_RETURN		crtn;
	CSSM_KEY_PTR	extractRootKey;	// from CSSM_CL_CertGetKeyInfo()
	CSSM_KEY_PTR	extractSubjKey;	// ditto
	CSSM_CC_HANDLE	signContext;	// for signing/verifying the cert
	unsigned		badByte;
	int				arg;
	unsigned		errorCount = 0;
	
	/* user-spec'd variables */
	CSSM_BOOL		writeBlobs = CSSM_FALSE;
	CSSM_ALGORITHMS	keyAlg = KEY_ALG_DEFAULT;
	CSSM_ALGORITHMS sigAlg = SIG_ALG_DEFAULT;
	uint32			keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	
	/* 
	 * Two extensions. Subject has one (KeyUsage); root has KeyUsage and 
	 * BasicConstraints.
	 */
	CSSM_X509_EXTENSION 	exts[2];
	CE_KeyUsage				keyUsage;
	CE_BasicConstraints		bc;
	
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
						break;
					case 'm':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_MD5WithRSA;
						break;
					case 'f':
						keyAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_FEE_MD5;
						break;
					case 'F':
						keyAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_FEE_SHA1;
						break;
					case 'e':
						keyAlg = CSSM_ALGID_FEE;
						sigAlg = CSSM_ALGID_SHA1WithECDSA;
						break;
					case 'E':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA1WithECDSA;
						break;
					case '7':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA256WithECDSA;
						break;
					case '8':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA384WithECDSA;
						break;
					case '9':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA512WithECDSA;
						break;
					case '2':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA224WithRSA;
						break;
					case '6':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA256WithRSA;
						break;
					case '3':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA384WithRSA;
						break;
					case '5':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA512WithRSA;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'k':
				keySizeInBits = atoi(&argv[arg][2]);
				break;
			default:
				usage(argv);
		}
	}
	
	/* connect to CL and CSP */
	clHand = clStartup();
	if(clHand == 0) {
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
		CSSM_FALSE,			// pubIsRef - should work both ways, but not yet
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&subjPrivKey,
		CSSM_FALSE,			// privIsRef
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		errorCount++;
		goto abort;
	}
	if(writeBlobs) {
		writeFile(SUBJ_PRIV_KEY_FILE, subjPrivKey.KeyData.Data,
			subjPrivKey.KeyData.Length);
		printf("...wrote %lu bytes to %s\n", subjPrivKey.KeyData.Length,
			SUBJ_PRIV_KEY_FILE);
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
		CSSM_FALSE,			// privIsRef
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		errorCount++;
		goto abort;
	}
	if(writeBlobs) {
		writeFile(ROOT_PRIV_KEY_FILE, rootPrivKey.KeyData.Data,
			rootPrivKey.KeyData.Length);
		printf("...wrote %lu bytes to %s\n", rootPrivKey.KeyData.Length,
			ROOT_PRIV_KEY_FILE);
	}

	if(compareKeyData(&rootPubKey, &subjPubKey)) {
	 	printf("**WARNING: Identical root and subj keys!\n");
	}

	/*
	 * Cook up various cert fields.
	 * First, the RDNs for subject and issuer. 
	 */
	rootName = CB_BuildX509Name(rootRdn, NUM_ROOT_NAMES);
	subjName = CB_BuildX509Name(subjRdn, NUM_SUBJ_NAMES);
	if((rootName == NULL) || (subjName == NULL)) {
		printf("CB_BuildX509Name failure");
		errorCount++;
		goto abort;
	}
	
	/* not before/after in generalized time format */
	notBefore = CB_BuildX509Time(0);
	notAfter  = CB_BuildX509Time(10000);

	/* A KeyUsage extension for both certs */
	exts[0].extnId = CSSMOID_KeyUsage;
	exts[0].critical = CSSM_FALSE;
	exts[0].format = CSSM_X509_DATAFORMAT_PARSED;
	keyUsage = CE_KU_DigitalSignature | CE_KU_KeyCertSign |
			   CE_KU_KeyEncipherment | CE_KU_DataEncipherment;
	exts[0].value.parsedValue = &keyUsage;
	exts[0].BERvalue.Data = NULL;
	exts[0].BERvalue.Length = 0;

	/* BasicConstraints for root only */
	exts[1].extnId = CSSMOID_BasicConstraints;
	exts[1].critical = CSSM_TRUE;
	exts[1].format = CSSM_X509_DATAFORMAT_PARSED;
	bc.cA = CSSM_TRUE;
	bc.pathLenConstraintPresent = CSSM_TRUE;
	bc.pathLenConstraint = 2;
	exts[1].value.parsedValue = &bc;
	exts[1].BERvalue.Data = NULL;
	exts[1].BERvalue.Length = 0;
	
	/* cook up root cert */
	printf("Creating root cert...\n");
	rawCert = CB_MakeCertTemplate(clHand,
		0x12345678,			// serial number
		rootName,
		rootName,
		notBefore,
		notAfter,
		&rootPubKey,
		sigAlg,
		NULL,				// subjUniqueId
		NULL,				// issuerUniqueId
		exts,				// extensions
		2);					// numExtensions

	if(rawCert == NULL) {
		errorCount++;
		goto abort;
	}
	if(writeBlobs) {
		writeFile(ROOT_TBS_FILE_NAME, rawCert->Data, rawCert->Length);
		printf("...wrote %lu bytes to %s\n", rawCert->Length, ROOT_TBS_FILE_NAME);
	}
	
	/* Self-sign; this is a root cert */
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
			sigAlg,
			NULL,			// AccessCred
			&rootPrivKey,
			&signContext);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext", crtn);
		errorCount++;
		goto abort;
	}
	signedRootCert.Data = NULL;
	signedRootCert.Length = 0;
	crtn = CSSM_CL_CertSign(clHand,
		signContext,
		rawCert,			// CertToBeSigned
		NULL,				// SignScope
		0,					// ScopeSize,
		&signedRootCert);
	if(crtn) {
		printError("CSSM_CL_CertSign", crtn);
		errorCount++;
		goto abort;
	}
	crtn = CSSM_DeleteContext(signContext);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		errorCount++;
		goto abort;
	}
	appFreeCssmData(rawCert, CSSM_TRUE);
	if(writeBlobs) {
		writeFile(ROOT_CERT_FILE_NAME, signedRootCert.Data, signedRootCert.Length);
		printf("...wrote %lu bytes to %s\n", signedRootCert.Length, 
			ROOT_CERT_FILE_NAME);
	}

	/* now a subject cert signed by the root cert */
	printf("Creating subject cert...\n");
	rawCert = CB_MakeCertTemplate(clHand,
		0x8765,				// serial number
		rootName,
		subjName,
		notBefore,
		notAfter,
		&subjPubKey,
		sigAlg,
		NULL,				// subjUniqueId
		NULL,				// issuerUniqueId
		exts,				// extensions
		1);					// numExtensions
	if(rawCert == NULL) {
		errorCount++;
		goto abort;
	}
	if(writeBlobs) {
		writeFile(SUBJ_TBS_FILE_NAME, rawCert->Data, rawCert->Length);
		printf("...wrote %lu bytes to %s\n", rawCert->Length, SUBJ_TBS_FILE_NAME);
	}

	/* sign by root */
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
			sigAlg,
			NULL,			// AccessCred
			&rootPrivKey,
			&signContext);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext", crtn);
		errorCount++;
		goto abort;
	}
	signedSubjCert.Data = NULL;
	signedSubjCert.Length = 0;
	crtn = CSSM_CL_CertSign(clHand,
		signContext,
		rawCert,			// CertToBeSigned
		NULL,				// SignScope
		0,					// ScopeSize,
		&signedSubjCert);
	if(crtn) {
		printError("CSSM_CL_CertSign", crtn);
		errorCount++;
		goto abort;
	}
	crtn = CSSM_DeleteContext(signContext);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		errorCount++;
		goto abort;
	}
	appFreeCssmData(rawCert, CSSM_TRUE);
	if(writeBlobs) {
		writeFile(SUBJ_CERT_FILE_NAME, signedSubjCert.Data, signedSubjCert.Length);
		printf("...wrote %lu bytes to %s\n", signedSubjCert.Length, 
			SUBJ_CERT_FILE_NAME);
	}

	/* Free the stuff we allocd to get here */
	CB_FreeX509Name(rootName);
	CB_FreeX509Name(subjName);
	CB_FreeX509Time(notBefore);
	CB_FreeX509Time(notAfter);

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
	appFreeCssmData(&signedSubjCert, CSSM_FALSE);
	appFreeCssmData(&signedRootCert, CSSM_FALSE);

	cspFreeKey(cspHand, &rootPubKey);
	cspFreeKey(cspHand, &subjPubKey);

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

abort:
	if(cspHand != 0) {
		CSSM_ModuleDetach(cspHand);
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
