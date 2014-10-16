/*
 * makeExpiredCerts.cpp - Make expired certs to verify Radar 3622125.
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/CertBuilderApp.h>
#include <clAppUtils/clutils.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/oidsalg.h>

/* 
 * The certs we create and write. 
 * -- GOOD_ROOT and EXPIRED_ROOT use same key pair, subject, issuer name
 * -- GOOD_CA and EXPIRED_CA use same key pair, subject, issuer name; both are 
 *    verifiable  by both GOOD_ROOT and EXPIRED_ROOT (temporal validity aside, 
 *    that is)
 * -- GOOD_LEAF and EXPIRED_LEAF use same key pair, subject, issuer name, both 
 *    are verifiable by both GOOD_CA and EXPIRED_CA (temporal validity aside,
 *    that is)
 */
#define GOOD_ROOT		"ecGoodRoot.cer"
#define EXPIRED_ROOT	"ecExpiredRoot.cer"
#define GOOD_CA			"ecGoodCA.cer"	
#define EXPIRED_CA		"ecExpiredCA.cer"
#define GOOD_LEAF		"ecGoodLeaf.cer"
#define EXPIRED_LEAF	"ecExpiredLeaf.cer"

/*
 * RDN components for root, CA, subject
 */
CB_NameOid rootRdn[] = 
{
	{ "Expired Cert Test Root",			&CSSMOID_CommonName }
};
#define NUM_ROOT_NAMES	(sizeof(rootRdn) / sizeof(CB_NameOid))

CB_NameOid caRdn[] = 
{
	{ "Expired Cert Test CA",			&CSSMOID_CommonName }
};
#define NUM_CA_NAMES	(sizeof(caRdn) / sizeof(CB_NameOid))

CB_NameOid leafRdn[] = 
{
	{ "Expired Cert Test Leaf",			&CSSMOID_CommonName }
};
#define NUM_LEAF_NAMES	(sizeof(leafRdn) / sizeof(CB_NameOid))

/* Key parameters */
#define LEAF_KEY_LABEL		"Expired Cert Leaf"
#define CA_KEY_LABEL		"Expired Cert CA"
#define ROOT_KEY_LABEL		"Expired Cert Root"
#define SIG_ALG				CSSM_ALGID_SHA1WithRSA
#define KEY_ALG				CSSM_ALGID_RSA
#define KEY_SIZE			1024

static void usage(char **argv)
{
	printf("usage: %s dstdir\n", argv[0]);
	exit(1);
}

/* write cert to dstDir/fileName */
static int writeCert(
	const CSSM_DATA *certData,
	const char *fileName,
	const char *dstDir)
{
	unsigned pathLen = strlen(fileName) + strlen(dstDir) + 2;
	char filePath[pathLen];
	sprintf(filePath, "%s/%s", dstDir, fileName);
	if(writeFile(filePath, certData->Data, certData->Length)) {
		printf("***Error writing cert to %s\n", filePath);
		return -1;
	}
	else {
		printf("...wrote %lu bytes to %s.\n", (unsigned long)certData->Length, filePath);
		return 0;
	}
}

static int makeCert(
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	CSSM_X509_NAME		*subject,
	CSSM_X509_NAME		*issuer,
	uint32				serialNum,
	CSSM_X509_TIME		*notBefore,
	CSSM_X509_TIME		*notAfter,
	CSSM_KEY_PTR		privKey,		/* signed with this */
	CSSM_KEY_PTR		pubKey,			/* contains this */
	bool				isCA,
	CSSM_DATA			*certData,		/* signed cert returned here */
	const char			*dstDir,
	const char			*fileName)		/* and written here in dstDir/fileName */
{
	CSSM_DATA_PTR tbsCert;
	CSSM_X509_EXTENSION 	ext;
	CE_BasicConstraints		bc;
	
	ext.extnId = CSSMOID_BasicConstraints;
	ext.critical = CSSM_TRUE;
	ext.format = CSSM_X509_DATAFORMAT_PARSED;
	bc.cA = isCA ? CSSM_TRUE : CSSM_FALSE;
	bc.pathLenConstraintPresent = CSSM_FALSE;
	bc.pathLenConstraint = 0;
	ext.value.parsedValue = &bc;
	ext.BERvalue.Data = NULL;
	ext.BERvalue.Length = 0;

	tbsCert = CB_MakeCertTemplate(clHand,
		serialNum,	
		issuer,
		subject,
		notBefore,
		notAfter,
		pubKey,
		SIG_ALG,
		NULL,				// subjUniqueId
		NULL,				// issuerUniqueId
		&ext,				// extensions
		1);					// numExtensions
	if(tbsCert == NULL) {
		return -1;
	}
	
	CSSM_CC_HANDLE signContext;
	CSSM_RETURN crtn;
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
			SIG_ALG,
			NULL,			// AccessCred
			privKey,
			&signContext);
	if(crtn) {
		cssmPerror("CSSM_CSP_CreateSignatureContext", crtn);
		/* this program is way sloppy about cleanup on errors */
		return -1;
	}
	certData->Data = NULL;
	certData->Length = 0;
	crtn = CSSM_CL_CertSign(clHand,
		signContext,
		tbsCert,			// CertToBeSigned
		NULL,				// SignScope
		0,					// ScopeSize,
		certData);
	if(crtn) {
		cssmPerror("CSSM_CL_CertSign", crtn);
		return -1;
	}
	CSSM_DeleteContext(signContext);
	appFreeCssmData(tbsCert, CSSM_TRUE);
	return writeCert(certData, fileName, dstDir);
}

int main(int argc, char **argv)
{
	if(argc != 2) {
		usage(argv);
	}
	const char *dstDir = argv[1];
	
	CSSM_CL_HANDLE clHand = clStartup();
	if(clHand == 0) {
		return 0;
	}
	CSSM_CSP_HANDLE cspHand = cspStartup();
	if(cspHand == 0) {
		return 0;
	}

	/* Cook up 3 key pairs */
	CSSM_KEY rootPrivKey;
	CSSM_KEY rootPubKey;
	CSSM_KEY caPrivKey;
	CSSM_KEY caPubKey;
	CSSM_KEY leafPrivKey;
	CSSM_KEY leafPubKey;
	
	CSSM_RETURN crtn = cspGenKeyPair(cspHand,
		KEY_ALG,
		ROOT_KEY_LABEL,
		strlen(ROOT_KEY_LABEL),
		KEY_SIZE,
		&rootPubKey,
		CSSM_FALSE,			// pubIsRef
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&rootPrivKey,
		CSSM_FALSE,			// privIsRef
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		exit(1);
	}
	crtn = cspGenKeyPair(cspHand,
		KEY_ALG,
		CA_KEY_LABEL,
		strlen(CA_KEY_LABEL),
		KEY_SIZE,
		&caPubKey,
		CSSM_FALSE,			// pubIsRef
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&caPrivKey,
		CSSM_FALSE,			// privIsRef
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		exit(1);
	}
	crtn = cspGenKeyPair(cspHand,
		KEY_ALG,
		LEAF_KEY_LABEL,
		strlen(LEAF_KEY_LABEL),
		KEY_SIZE,
		&leafPubKey,
		CSSM_FALSE,			// pubIsRef
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&leafPrivKey,
		CSSM_FALSE,			// privIsRef
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		exit(1);
	}

	/* now, subject and issuer names */
	CSSM_X509_NAME *rootSubj = CB_BuildX509Name(rootRdn, NUM_ROOT_NAMES);
	CSSM_X509_NAME *caSubj   = CB_BuildX509Name(caRdn, NUM_CA_NAMES);
	CSSM_X509_NAME *leafSubj = CB_BuildX509Name(leafRdn, NUM_LEAF_NAMES);
	
	/* times: now (for all not before), +10 seconds (for expired), +10 years (for valid not after) */
	CSSM_X509_TIME *nowTime    = CB_BuildX509Time(0, NULL);
	CSSM_X509_TIME *soonTime   = CB_BuildX509Time(10, NULL);
	CSSM_X509_TIME *futureTime = CB_BuildX509Time(60 * 60 * 24 * 365 * 10, NULL);
	
	/* six certs */
	CSSM_DATA goodRoot;
	CSSM_DATA expiredRoot;
	CSSM_DATA goodCA;
	CSSM_DATA expiredCA;
	CSSM_DATA goodLeaf;
	CSSM_DATA expiredLeaf;
	uint32 serialNum = 0;
	
	if(makeCert(clHand, cspHand,
		rootSubj, rootSubj, serialNum++, nowTime, futureTime,
		&rootPrivKey, &rootPubKey, true,
		&goodRoot, dstDir, GOOD_ROOT)) {
		printf("***Error creating good root. Aborting.\n");
		exit(1);
	}
	if(makeCert(clHand, cspHand,
		rootSubj, rootSubj, serialNum++, nowTime, soonTime,
		&rootPrivKey, &rootPubKey, true,
		&expiredRoot, dstDir, EXPIRED_ROOT)) {
		printf("***Error creating expired root. Aborting.\n");
		exit(1);
	}
	
	/* CA signed by root */
	if(makeCert(clHand, cspHand,
		caSubj, rootSubj, serialNum++, nowTime, futureTime,
		&rootPrivKey, &caPubKey, true,
		&goodCA, dstDir, GOOD_CA)) {
		printf("***Error creating good CA. Aborting.\n");
		exit(1);
	}
	if(makeCert(clHand, cspHand,
		caSubj, rootSubj, serialNum++, nowTime, soonTime,
		&rootPrivKey, &caPubKey, true,
		&expiredCA, dstDir, EXPIRED_CA)) {
		printf("***Error creating expired CA. Aborting.\n");
		exit(1);
	}
	
	/* Leaf signed by CA */
	if(makeCert(clHand, cspHand,
		leafSubj, caSubj, serialNum++, nowTime, futureTime,
		&caPrivKey, &leafPubKey, false,
		&goodLeaf, dstDir, GOOD_LEAF)) {
		printf("***Error creating good leaf. Aborting.\n");
		exit(1);
	}
	if(makeCert(clHand, cspHand,
		leafSubj, caSubj, serialNum++, nowTime, soonTime,
		&caPrivKey, &leafPubKey, false,
		&expiredLeaf, dstDir, EXPIRED_LEAF)) {
		printf("***Error creating expired leaf. Aborting.\n");
		exit(1);
	}
	
	/* cleanup if you think you must */
	
	return 0;
}
