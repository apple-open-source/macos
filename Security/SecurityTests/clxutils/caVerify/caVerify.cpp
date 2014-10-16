/* Copyright (c) 1998,2003-2006,2008 Apple Inc.
 *
 * caVerify.cpp
 *
 * Verify proper detection of basicConstraints.cA
 */

#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/CertBuilderApp.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/timeStr.h>
#include <clAppUtils/tpUtils.h>
#include <security_cdsa_utils/cuPrintCert.h>
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

/* define nonzero to build on Puma */
#define PUMA_BUILD			0

/* default key and signature algorithm */
#define SIG_ALG_DEFAULT		CSSM_ALGID_SHA1WithRSA
#define KEY_ALG_DEFAULT		CSSM_ALGID_RSA

#define NUM_CERTS_DEF		5		/* default is random from 2 to this */
#define NUM_LOOPS_DEF		100

#if		PUMA_BUILD
extern "C" {
	void cssmPerror(const char *how, CSSM_RETURN error);
}
#endif	/* PUMA_BUILD */

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("   n=numCerts (default=random from 2 to %d)\n", NUM_CERTS_DEF);
	printf("   a=alg  where alg is s(RSA/SHA1), 5(RSA/MD5), f(FEE/MD5), "
			"F(FEE/SHA1), e(ECDSA), E(ANSI/ECDSA), 6(ECDSA/SHA256)\n");
	printf("   k=keySizeInBits\n");
	printf("   c (dump certs on error)\n");
	printf("   l=numLoops (default = %d)\n", NUM_LOOPS_DEF);
	exit(1);
}

void showCerts(
	const CSSM_DATA *certs,
	unsigned numCerts)
{
	unsigned i;
	
	for(i=0; i<numCerts; i++) {
		printf("======== cert %d ========\n", i);
		printCert(certs[i].Data, certs[i].Length, CSSM_FALSE);
		printf("\n\n");
	}
}

void writeCerts(
	const CSSM_DATA *certs,
	unsigned numCerts)
{
	unsigned i;
	char fileName[80];
	
	for(i=0; i<numCerts; i++) {
		sprintf(fileName, "caVerifyCert%u.cer", i);
		writeFile(fileName, certs[i].Data, certs[i].Length);
		printf("....wrote %lu bytes to %s\n", certs[i].Length, fileName);
	}
}

/* 
 * Generate a cert chain using specified key pairs and extensions.
 * The last cert in the chain (certs[numCerts-1]) is a root cert, 
 * self-signed. 
 */
static CSSM_RETURN tpGenCertsExten(
	CSSM_CSP_HANDLE		cspHand,
	CSSM_CL_HANDLE		clHand,
	CSSM_ALGORITHMS		sigAlg,			/* CSSM_ALGID_SHA1WithRSA, etc. */
	unsigned			numCerts,
	/* per-cert arrays */
	const char 			*nameBase,		/* C string */
	CSSM_KEY_PTR		pubKeys,		/* array of public keys */
	CSSM_KEY_PTR		privKeys,		/* array of private keys */
	CSSM_X509_EXTENSION	**extensions,
	unsigned			*numExtensions,
	CSSM_DATA_PTR		certs,			/* array of certs RETURNED here */
	const char			*notBeforeStr,	/* from genTimeAtNowPlus() */
	const char			*notAfterStr)	/* from genTimeAtNowPlus() */

{
	int 				dex;
	CSSM_RETURN			crtn;
	CSSM_X509_NAME		*issuerName = NULL;
	CSSM_X509_NAME		*subjectName = NULL;
	CSSM_X509_TIME		*notBefore;			// UTC-style "not before" time
	CSSM_X509_TIME		*notAfter;			// UTC-style "not after" time
	CSSM_DATA_PTR		rawCert = NULL;		// from CSSM_CL_CertCreateTemplate
	CSSM_DATA			signedCert;			// from CSSM_CL_CertSign	
	uint32				rtn;
	CSSM_KEY_PTR		signerKey;			// signs the cert
	CSSM_CC_HANDLE		signContext;
	char				nameStr[100];
	CSSM_DATA_PTR		thisCert;			// ptr into certs[]
	CB_NameOid			nameOid;
	
	nameOid.oid = &CSSMOID_OrganizationName;	// const
	nameOid.string = nameStr;
	
	/* main loop - once per keypair/cert - starting at end/root */
	for(dex=numCerts-1; dex>=0; dex--) {
		thisCert = &certs[dex];
		
		thisCert->Data = NULL;
		thisCert->Length = 0;
		
		sprintf(nameStr, "%s%04d", nameBase, dex);
		if(issuerName == NULL) {
			/* last (root) cert - subject same as issuer */
			issuerName = CB_BuildX509Name(&nameOid, 1); 
			/* self-signed */
			signerKey = &privKeys[dex];
		}
		else {
			/* previous subject becomes current issuer */
			CB_FreeX509Name(issuerName);
			issuerName = subjectName;
			signerKey = &privKeys[dex+1];
		}
		subjectName = CB_BuildX509Name(&nameOid, 1);
		if((subjectName == NULL) || (issuerName == NULL)) {
			printf("Error creating X509Names\n");
			crtn = CSSMERR_CSSM_MEMORY_ERROR;
			break;
		}
		
		/* 
		 * not before/after in Y2k-compliant generalized time format.
		 * These come preformatted from our caller. 
		 */
		notBefore = CB_BuildX509Time(0, notBeforeStr);
		notAfter  = CB_BuildX509Time(0, notAfterStr);

		/* 
		 * Cook up cert template 
		 * Note serial number would be app-specified in real world
		 */
		rawCert = CB_MakeCertTemplate(clHand,
			0x12345 + dex,	// serial number
			issuerName,
			subjectName,
			notBefore,
			notAfter,
			&pubKeys[dex],
			sigAlg,
			NULL,						// subj unique ID
			NULL,						// issuer unique ID
			extensions[dex],			// extensions
			numExtensions[dex]);		// numExtensions
	
		if(rawCert == NULL) {
			crtn = CSSM_ERRCODE_INTERNAL_ERROR;
			break;
		}

		/* Free the stuff we allocd to get here */
		CB_FreeX509Time(notBefore);
		CB_FreeX509Time(notAfter);

		/**** sign the cert ****/
		/* 1. get a signing context */
		crtn = CSSM_CSP_CreateSignatureContext(cspHand,
				sigAlg,
				NULL,			// no passphrase for now
				signerKey,
				&signContext);
		if(crtn) {
			printError("CreateSignatureContext", crtn);
			break;
		}
		
		/* 2. use CL to sign the cert */ 
		signedCert.Data = NULL;
		signedCert.Length = 0;
		crtn = CSSM_CL_CertSign(clHand,
			signContext,
			rawCert,			// CertToBeSigned
			NULL,				// SignScope per spec
			0,					// ScopeSize per spec
			&signedCert);
		if(crtn) {
			printError("CSSM_CL_CertSign", crtn);
			break;
		}
		
		/* 3. delete signing context */
		crtn = CSSM_DeleteContext(signContext);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			break;
		}

		/* 
		 * CSSM_CL_CertSign() returned us a mallocd CSSM_DATA. Copy
		 * its fields to caller's cert. 
		 */
		certs[dex] = signedCert;
		
		/* and the raw unsigned cert as well */
		appFreeCssmData(rawCert, CSSM_TRUE);
		rtn = 0;
	}
	
	/* free resources */
	if(issuerName != NULL) {
		CB_FreeX509Name(issuerName);
	}
	if(subjectName != NULL) {
		CB_FreeX509Name(subjectName);
	}
	return crtn;
}

static int doTest(
	CSSM_CSP_HANDLE	cspHand,		// CSP handle
	CSSM_CL_HANDLE	clHand,			// CL handle
	CSSM_TP_HANDLE	tpHand,			// TP handle
	unsigned		numCerts,		// >= 2
	CSSM_KEY_PTR	pubKeys,
	CSSM_KEY_PTR	privKeys,
	CSSM_ALGORITHMS	sigAlg,
	CSSM_BOOL		expectFail,
	CSSM_BOOL		dumpCerts,
	CSSM_BOOL		quiet)
{
	CSSM_DATA_PTR		certs;
	CSSM_X509_EXTENSION	**extens;
	unsigned			*numExtens;
	char 				*notBeforeStr = genTimeAtNowPlus(0);
	char 				*notAfterStr = genTimeAtNowPlus(10000);
	unsigned			certDex;
	CE_BasicConstraints	*bc;
	CSSM_X509_EXTENSION *thisExten;
	CE_BasicConstraints *thisBc;
	const char			*failMode = "not set - internal error";
	CSSM_RETURN			crtn;
	unsigned 			badCertDex = 0;
	
	certs = (CSSM_DATA_PTR)malloc(sizeof(CSSM_DATA) * numCerts);
	memset(certs, 0, sizeof(CSSM_DATA) * numCerts);
	
	/*
	 * For now just zero or one extension per cert - basicConstraints. 
	 * Eventually we'll want to test keyUsage as well.
	 */
	extens = (CSSM_X509_EXTENSION **)malloc(sizeof(CSSM_X509_EXTENSION *) * numCerts);
	memset(extens, 0, sizeof(CSSM_X509_EXTENSION *) * numCerts);
	numExtens = (unsigned *)malloc(sizeof(unsigned) * numCerts);
	bc = (CE_BasicConstraints *)malloc(sizeof(CE_BasicConstraints) * numCerts);
	
	/*
	 * Set up all extensions for success
	 */
	for(certDex=0; certDex<numCerts; certDex++) {
		extens[certDex] = (CSSM_X509_EXTENSION *)malloc(sizeof(CSSM_X509_EXTENSION));
		numExtens[certDex] = 1;
		thisExten = extens[certDex];
		thisBc = &bc[certDex];
		
		thisExten->extnId = CSSMOID_BasicConstraints;
		thisExten->critical = CSSM_TRUE;
		thisExten->format = CSSM_X509_DATAFORMAT_PARSED;
		thisExten->value.parsedValue = thisBc;
		thisExten->BERvalue.Data = NULL;
		thisExten->BERvalue.Length = 0;

		if(certDex == 0) {
			/* leaf - flip coin to determine presence of basicConstraints */
			int coin = genRand(1,2);
			if(coin == 1) {
				/* basicConstraints, !cA */
				thisBc->cA = CSSM_FALSE;
				thisBc->pathLenConstraintPresent = CSSM_FALSE;
				thisBc->pathLenConstraint = 0;
			}
			else {
				/* !basicConstraints, !cA by default */
				numExtens[certDex] = 0;
			}
		}
		else if(certDex == (numCerts-1)) {
			/* root - flip coin to determine presence of basicConstraints */
			int coin = genRand(1,2);
			if(coin == 1) {
				/* basicConstraints, cA */
				thisBc->cA = CSSM_TRUE;
				/* flip coin to determine present of pathLenConstraint */
				coin = genRand(1,2);
				if(coin == 1) {
					thisBc->pathLenConstraintPresent = CSSM_FALSE;
					thisBc->pathLenConstraint = 0;
				}
				else {
					thisBc->pathLenConstraintPresent = CSSM_TRUE;
					thisBc->pathLenConstraint = genRand(certDex-1, numCerts+1);
				}
			}
			else {
				/* !basicConstraints, cA by default */
				numExtens[certDex] = 0;
			}
		}
		else {
			/* intermediate = cA required */
			thisBc->cA = CSSM_TRUE;
			/* flip coin to determine presence of pathLenConstraint */
			int coin = genRand(1,2);
			if(coin == 1) {
				thisBc->pathLenConstraintPresent = CSSM_FALSE;
				thisBc->pathLenConstraint = 0;
			}
			else {
				thisBc->pathLenConstraintPresent = CSSM_TRUE;
				thisBc->pathLenConstraint = genRand(certDex-1, numCerts+1);
			}
		}
	}
	
	if(expectFail) {
		/* introduce a failure */
		if(numCerts == 2) {
			/* only possible failure is explicit !cA in root */
			/* don't assume presence of BC exten */
			badCertDex = 1;
			thisExten = extens[badCertDex];
			thisBc = &bc[badCertDex];
			thisBc->cA = CSSM_FALSE;
			thisBc->pathLenConstraintPresent = CSSM_FALSE;
			bc->pathLenConstraint = 0;
			numExtens[badCertDex] = 1;
			failMode = "Explicit !cA in root";
		}
		else {
			/* roll the dice to select an intermediate cert */
			badCertDex = genRand(1, numCerts-2);
			thisExten = extens[badCertDex];
			if((thisExten == NULL) || (numExtens[badCertDex] == 0)) {
				printf("***INTERNAL SCREWUP\n");
				exit(1);
			}
			thisBc = &bc[badCertDex];
			
			/* 
			 * roll die: fail by 
			 *   -- no BasicConstraints
			 *   -- !cA
			 *   -- bad pathLenConstraint 
			 */
			int die = genRand(1,3);
			if((die == 1) &&
			   (badCertDex != 1)) {		// last cA doesn't need pathLenConstraint 
				thisBc->pathLenConstraintPresent = CSSM_TRUE;
				thisBc->pathLenConstraint = badCertDex - 2;	// one short
				failMode = "Short pathLenConstraint";
			}
			else if(die == 2) {
				thisBc->cA = CSSM_FALSE;
				failMode = "Explicit !cA in intermediate";
			}
			else {
				/* no extension */
				numExtens[badCertDex] = 0;
				failMode = "No BasicConstraints in intermediate";
			}
		}
	}
	if(!quiet && expectFail) {
		printf("   ...bad cert at index %d: %s\n", badCertDex, failMode);
	}
	
	/* here we go - create cert chain */
	crtn = tpGenCertsExten(cspHand,
		clHand,
		sigAlg,
		numCerts,
		"caVerify",		// nameBase
		pubKeys,
		privKeys,
		extens,
		numExtens,
		certs,
		notBeforeStr,
		notAfterStr);
	if(crtn) {
		printError("tpGenCertsExten", crtn);
		return crtn;	// and leak like crazy 
	}
	
	CSSM_CERTGROUP  cgrp;
	memset(&cgrp, 0, sizeof(CSSM_CERTGROUP));
	cgrp.NumCerts = numCerts;
	#if		PUMA_BUILD
	cgrp.CertGroupType = CSSM_CERTGROUP_ENCODED_CERT;
	#else
	/* Jaguar */
	cgrp.CertGroupType = CSSM_CERTGROUP_DATA;
	#endif	/* PUMA_BUILD */
	cgrp.CertType = CSSM_CERT_X_509v3;
	cgrp.CertEncoding = CSSM_CERT_ENCODING_DER; 
	cgrp.GroupList.CertList = certs;
	
	#if		PUMA_BUILD
	crtn = tpCertGroupVerify(tpHand,
		clHand,
		cspHand,
		NULL,						// DlDbList
		&CSSMOID_APPLE_X509_BASIC,	// SSL requires built-in root match 
		&cgrp,
		/* pass in OUR ROOT as anchors */
		(CSSM_DATA_PTR)&certs[numCerts-1],		// anchorCerts
		1,
		CSSM_TP_STOP_ON_POLICY, 
		CSSM_FALSE,					// allowExpired
		NULL);						// vfyResult
	#else
	/* Jaguar */
	crtn = tpCertGroupVerify(tpHand,
		clHand,
		cspHand,
		NULL,						// DlDbList
		&CSSMOID_APPLE_TP_SSL,		// may want to parameterize this
		NULL,						// fieldOpts for server name
		NULL,						// actionDataPtr for allow expired
		NULL,						// policyOpts
		&cgrp,
		/* pass in OUR ROOT as anchors */
		(CSSM_DATA_PTR)&certs[numCerts-1],		// anchorCerts
		1,
		CSSM_TP_STOP_ON_POLICY, 
		NULL,						// cssmTimeStr
		NULL);						// vfyResult
	#endif	/* PUMA_BUILD */
	if(expectFail) {
		if(crtn != CSSMERR_TP_VERIFY_ACTION_FAILED) {
			cssmPerror("***Expected error TP_VERIFY_ACTION_FAILED; got ", crtn);
			printf("   Expected failure due to %s\n", failMode);
			if(dumpCerts) {
				showCerts(certs, numCerts);
				writeCerts(certs, numCerts);
			}
			return testError(quiet);
		}
	}
	else if(crtn) {
		cssmPerror("Unexpected failure on tpCertGroupVerify", crtn);
		if(dumpCerts) {
			showCerts(certs, numCerts);
		}
		return testError(quiet);
	}
		
	/* clean up */
	return 0;
}

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE	clHand;			// CL handle
	CSSM_CSP_HANDLE	cspHand;		// CSP handle
	CSSM_TP_HANDLE	tpHand;
	CSSM_KEY_PTR	pubKeys;
	CSSM_KEY_PTR	privKeys;
	CSSM_RETURN		crtn;
	int				arg;
	unsigned		certDex;
	unsigned		loopNum;
	unsigned		maxCerts;
	
	/* user-spec'd variables */
	CSSM_ALGORITHMS	keyAlg = KEY_ALG_DEFAULT;
	CSSM_ALGORITHMS sigAlg = SIG_ALG_DEFAULT;
	uint32			keySizeInBits = CSP_KEY_SIZE_DEFAULT;
	unsigned 		numCerts = 0;			// means random per loop
	unsigned 		numLoops = NUM_LOOPS_DEF;
	CSSM_BOOL		quiet = CSSM_FALSE;
	CSSM_BOOL		dumpCerts = CSSM_FALSE;
	
	for(arg=1; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'a':
				if((argv[arg][1] == '\0') || (argv[arg][2] == '\0')) {
					usage(argv);
				}
				switch(argv[arg][2]) {
					case 's':
						keyAlg = CSSM_ALGID_RSA;
						sigAlg = CSSM_ALGID_SHA1WithRSA;
						break;
					case '5':
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
					case '6':
						keyAlg = CSSM_ALGID_ECDSA;
						sigAlg = CSSM_ALGID_SHA256WithECDSA;
						break;
					default:
						usage(argv);
				}
				break;
		    case 'k':
				keySizeInBits = atoi(&argv[arg][2]);
				break;
		    case 'l':
				numLoops = atoi(&argv[arg][2]);
				break;
		    case 'n':
				numCerts = atoi(&argv[arg][2]);
				break;
		    case 'c':
				dumpCerts = CSSM_TRUE;
				break;
		    case 'q':
				quiet = CSSM_TRUE;
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

	if(numCerts == 0) {
		maxCerts = NUM_CERTS_DEF;	// random, this is the max $
	}
	else {
		maxCerts = numCerts;		// user-specd
	}
	
	printf("Starting caVerify; args: ");
	for(unsigned i=1; i<(unsigned)argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");

	/* cook up maxCerts key pairs */
	if(!quiet) {
		printf("generating key pairs...\n");
	}
	pubKeys  = (CSSM_KEY_PTR)malloc(sizeof(CSSM_KEY) * maxCerts);
	privKeys = (CSSM_KEY_PTR)malloc(sizeof(CSSM_KEY) * maxCerts);
	for(certDex=0; certDex<maxCerts; certDex++) {
		crtn = cspGenKeyPair(cspHand,
			keyAlg,
			"a key pair",
			9,
			keySizeInBits,
			&pubKeys[certDex],
			CSSM_FALSE,			// pubIsRef - should work both ways, but not yet
			CSSM_KEYUSE_VERIFY,
			CSSM_KEYBLOB_RAW_FORMAT_NONE,
			&privKeys[certDex],
			CSSM_TRUE,			// privIsRef - doesn't matter
			CSSM_KEYUSE_SIGN,
			CSSM_KEYBLOB_RAW_FORMAT_NONE,
			CSSM_FALSE);
		if(crtn) {
			printError("cspGenKeyPair", crtn);
			printf("***error generating key pair. Aborting.\n");
			exit(1);
		}
	}

	for(loopNum=0; loopNum<numLoops; loopNum++) {
		unsigned thisNumCerts;
		
		/* random: num certs and whether this loop is to test a failure */
		if(numCerts) {
			thisNumCerts = numCerts;
		}
		else {
			thisNumCerts = genRand(2, NUM_CERTS_DEF);
		}
		int coin = genRand(1,2);
		CSSM_BOOL expectFail = (coin == 1) ? CSSM_TRUE : CSSM_FALSE;
		if(!quiet) {
			printf("...loop %d numCerts %u expectFail %s\n", loopNum,
				thisNumCerts, expectFail ? "TRUE" : "FALSE");
		}
		if(doTest(cspHand,
			clHand,
			tpHand,
			thisNumCerts,
			pubKeys,
			privKeys,
			sigAlg,
			expectFail,
			dumpCerts,
			quiet)) {
				break;
		}
	}
	/* clean up */
	return 0;
}


