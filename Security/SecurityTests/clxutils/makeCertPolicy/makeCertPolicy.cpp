/* Copyright (c) 1998,2005-2006 Apple Computer, Inc.
 *
 * makeCertPolicy.cpp - create a self signed cert with a Cert Policies extension
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

#define ROOT_KEY_LABEL		"rootKey"
/* default key and signature algorithm */
#define SIG_ALG_DEFAULT		CSSM_ALGID_SHA1WithRSA
#define KEY_ALG_DEFAULT		CSSM_ALGID_RSA

#define CPS_URI				"http://www.foo.com"

static void usage(char **argv)
{
	printf("Usage: %s outFileName\n", argv[0]);
	exit(1);
}

/*
 * RDN components for root, subject
 */
CB_NameOid rootRdn[] = 
{
	{ "Apple Computer DEBUG",			&CSSMOID_OrganizationName },
	{ "Cert Policy Demo",				&CSSMOID_CommonName }
};
#define NUM_ROOT_NAMES	(sizeof(rootRdn) / sizeof(CB_NameOid))

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE	clHand;			// CL handle
	CSSM_X509_NAME	*rootName;
	CSSM_X509_TIME	*notBefore;		// UTC-style "not before" time
	CSSM_X509_TIME	*notAfter;		// UTC-style "not after" time
	CSSM_DATA_PTR	rawCert;		// from CSSM_CL_CertCreateTemplate
	CSSM_DATA		signedRootCert;	// from CSSM_CL_CertSign
	CSSM_CSP_HANDLE	cspHand;		// CSP handle
	CSSM_KEY		rootPubKey;		// root's RSA public key blob
	CSSM_KEY		rootPrivKey;	// root's RSA private key - ref format
	CSSM_RETURN		crtn;
	CSSM_CC_HANDLE	signContext;	// for signing/verifying the cert
	
	/* user-spec'd variables */
	const			char *outFileName;

	if(argc != 2) {
		usage(argv);
	}
	outFileName = argv[1];
	
	/* 
	 * One extensions.
	 */
	CSSM_X509_EXTENSION 	ext;
	CE_CertPolicies			cp;
	CE_PolicyInformation	cpi;
	CE_PolicyQualifierInfo	cpqi;
		
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
	
	/* cook up an RSA key pair */
	crtn = cspGenKeyPair(cspHand,
		KEY_ALG_DEFAULT,
		ROOT_KEY_LABEL,
		strlen(ROOT_KEY_LABEL),
		512,
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
		printf("Error creatingt key pair, aborting.\n");
		exit(1);
	}

	/*
	 * Cook up various cert fields.
	 * First, the RDNs for subject and issuer. 
	 */
	rootName = CB_BuildX509Name(rootRdn, NUM_ROOT_NAMES);
	if(rootName == NULL) {
		printf("CB_BuildX509Name failure");
		exit(1);
	}
	
	/* not before/after in generalized time format */
	notBefore = CB_BuildX509Time(0);
	notAfter  = CB_BuildX509Time(10000);

	/* Here's what we do */
	ext.extnId = CSSMOID_CertificatePolicies;
	ext.critical = CSSM_FALSE;
	ext.format = CSSM_X509_DATAFORMAT_PARSED;
	
	cpqi.policyQualifierId = CSSMOID_QT_CPS;
	cpqi.qualifier.Data = (uint8 *)CPS_URI;
	cpqi.qualifier.Length = strlen(CPS_URI);
	
	cpi.certPolicyId = CSSMOID_APPLE_CERT_POLICY;	/* what I'm testing today */
	cpi.numPolicyQualifiers = 1;
	cpi.policyQualifiers = &cpqi;
	
	cp.numPolicies = 1;
	cp.policies = &cpi;
	
	ext.value.parsedValue = &cp;
	ext.BERvalue.Data = NULL;
	ext.BERvalue.Length = 0;

	/* cook up root cert */
	printf("Creating root cert...\n");
	rawCert = CB_MakeCertTemplate(clHand,
		0x12345678,			// serial number
		rootName,
		rootName,
		notBefore,
		notAfter,
		&rootPubKey,
		SIG_ALG_DEFAULT,
		NULL,				// subjUniqueId
		NULL,				// issuerUniqueId
		&ext,				// extensions
		1);					// numExtensions

	if(rawCert == NULL) {
		printf("CB_MakeCertTemplate failure");
		exit(1);
	}
	/* Self-sign */
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
			SIG_ALG_DEFAULT,
			NULL,			// AccessCred
			&rootPrivKey,
			&signContext);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext", crtn);
		exit(1);
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
		exit(1);
	}
	crtn = CSSM_DeleteContext(signContext);
	if(crtn) {
		printError("CSSM_DeleteContext", crtn);
		exit(1);
	}
	appFreeCssmData(rawCert, CSSM_TRUE);
	writeFile(outFileName, signedRootCert.Data, signedRootCert.Length);
	printf("...wrote %lu bytes to %s\n", signedRootCert.Length, outFileName);

	/* Free the stuff we allocd to get here */
	CB_FreeX509Name(rootName);
	CB_FreeX509Time(notBefore);
	CB_FreeX509Time(notAfter);
	appFreeCssmData(&signedRootCert, CSSM_FALSE);

	cspFreeKey(cspHand, &rootPubKey);
	cspFreeKey(cspHand, &rootPrivKey);
	return 0;
}

