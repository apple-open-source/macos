/*
 * certLabelTest.cpp - test SecCertificateInferLabel(), in particular, Radar 
 *					   3529689 (teletex strings) and 4746055 (add Description 
 *					   in parentheses)
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/CertBuilderApp.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <security_cdsa_utils/cuFileIo.h>

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -p              -- pause for leaks check\n");
	printf("  -q              -- quiet\n");
	/* etc. */
	exit(1);
}

#define KEY_SIZE		1024
#define KEY_ALG			CSSM_ALGID_RSA
#define SIG_ALG			CSSM_ALGID_SHA1WithRSA
#define CERT_FILE_OUT	"/tmp/certLabelTest.cer"

/* 
 * Here's the definitive string for Radar 3529689.
 * BER tag = Teletex/T61, encoding = kCFStringEncodingISOLatin1.
 * I hope Herr Petersen does not mind. 
 */
static const unsigned char JurgenPetersen[] = 
{
	0x4a, 0xf8, 0x72, 0x67, 0x65, 0x6e, 0x20, 0x4e,
	0xf8, 0x72, 0x67, 0x61, 0x61, 0x72, 0x64, 0x20,
	0x50, 0x65, 0x74, 0x65, 0x72, 0x73, 0x65, 0x6e
};

/*
 * Name/OID pair used in buildX509Name().
 * This logic is like the CB_BuildX509Name() code in clAppUtils/CertBuilderApp.cpp,
 * with the addition of the berTag specification, and data is specified as a void *
 * and size_t. 
 */
typedef struct {
	const void			*nameVal;
	CSSM_SIZE			nameLen;
	const CSSM_OID 		*oid;
	CSSM_BER_TAG		berTag;
} NameOid;

/*
 * Build up a CSSM_X509_NAME from an arbitrary list of name/OID/tag triplets.
 * We do one a/v pair per RDN.
 */
static CSSM_X509_NAME *buildX509Name(
	const NameOid		*nameArray,
	unsigned 			numNames)
{
	CSSM_X509_NAME *top = (CSSM_X509_NAME *)appMalloc(sizeof(CSSM_X509_NAME), 0);
	if(top == NULL) {
		return NULL;
	}
	top->numberOfRDNs = numNames;
	top->RelativeDistinguishedName = 
		(CSSM_X509_RDN_PTR)appMalloc(sizeof(CSSM_X509_RDN) * numNames, 0);
	if(top->RelativeDistinguishedName == NULL) {
		return NULL;
	}
	CSSM_X509_RDN_PTR rdn;
	const NameOid *nameOid;
	unsigned nameDex;
	for(nameDex=0; nameDex<numNames; nameDex++) {
		rdn = &top->RelativeDistinguishedName[nameDex];
		nameOid = &nameArray[nameDex];
		rdn->numberOfPairs = 1;
		rdn->AttributeTypeAndValue = (CSSM_X509_TYPE_VALUE_PAIR_PTR)
			appMalloc(sizeof(CSSM_X509_TYPE_VALUE_PAIR), 0);
		CSSM_X509_TYPE_VALUE_PAIR_PTR atvp = rdn->AttributeTypeAndValue;
		if(atvp == NULL) {
			return NULL;
		}
		appCopyCssmData(nameOid->oid, &atvp->type);
		atvp->valueType = nameOid->berTag;
		atvp->value.Length = nameOid->nameLen;
		atvp->value.Data = (uint8 *)CSSM_MALLOC(nameOid->nameLen);
		memmove(atvp->value.Data, nameOid->nameVal, nameOid->nameLen);
	}
	return top;
}

/* just make these static and reuse them */
static CSSM_X509_TIME *notBefore;
static CSSM_X509_TIME *notAfter;

/* 
 * Core test routine.
 * -- build a cert with issuer and subject as per specified name components
 * -- extract inferred label 
 * -- compare inferred label to expected value 
 * -- if labelIsCommonName true, verify that SecCertificateCopyCommonName() yields
 *    the same string as inferred label
 */
static int doTest(
	const char			*testName,
	bool				quiet,
	CSSM_CSP_HANDLE		cspHand,
	CSSM_CL_HANDLE		clHand,
	CSSM_KEY_PTR		privKey,
	CSSM_KEY_PTR		pubKey,
	
	/* input names - one or two */
	const void			*name1Val,
	CSSM_SIZE			name1Len,
	CSSM_BER_TAG		berTag1,
	const CSSM_OID		*name1Oid,
	const void			*name2Val,		// optional 
	CSSM_SIZE			name2Len,
	CSSM_BER_TAG		berTag2,
	const CSSM_OID		*name2Oid,
	
	/* expected label */
	CFStringRef			expectedLabel,
	bool				labelIsCommonName)
{
	if(!quiet) {
		printf("...%s\n", testName);
	}
	
	/* build the subject/issuer name */
	NameOid nameArray[2] = { {name1Val, name1Len, name1Oid, berTag1 },
							 {name2Val, name2Len, name2Oid, berTag2 } };
	unsigned numNames = name2Val ? 2 : 1;
	
	CSSM_X509_NAME *name = buildX509Name(nameArray, numNames);
	if(name == NULL) {
		printf("***buildX509Name screwup\n");
		return -1;
	}
	
	/* build the cert template */
	CSSM_DATA_PTR certTemp = CB_MakeCertTemplate(
		clHand, 0x123456,
		name, name,
		notBefore, notAfter,
		pubKey, SIG_ALG,
		NULL, NULL,		// subject/issuer UniqueID
		NULL, 0);		// extensions 
	if(certTemp == NULL) {
		printf("***CB_MakeCertTemplate screwup\n");
		return -1;
	}
	
	/* sign the cert */
	CSSM_DATA signedCert = {0, NULL};
	CSSM_CC_HANDLE sigHand;
	CSSM_RETURN crtn = CSSM_CSP_CreateSignatureContext(cspHand,
			SIG_ALG,
			NULL,			// no passphrase for now
			privKey,
			&sigHand);
	if(crtn) {
		/* should never happen */
		cssmPerror("CSSM_CSP_CreateSignatureContext", crtn);
		return 1;
	}
	crtn = CSSM_CL_CertSign(clHand,
		sigHand,
		certTemp,			// CertToBeSigned
		NULL,				// SignScope per spec
		0,					// ScopeSize per spec
		&signedCert);
	if(crtn) {
		cssmPerror("CSSM_CL_CertSign", crtn);
		return 1;
	}
	CSSM_DeleteContext(sigHand);
	CSSM_FREE(certTemp->Data);
	CSSM_FREE(certTemp);

	/* 
	 * OK, we have a signed cert. 
	 * Turn it into a SecCertificateRef and get the inferred label.
	 */
	OSStatus ortn;
	SecCertificateRef certRef;
	ortn = SecCertificateCreateFromData(&signedCert, 
		CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER,
		&certRef);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		return -1;
	}
	CFStringRef inferredLabel;
	ortn = SecCertificateInferLabel(certRef, &inferredLabel);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		return -1;
	}
	CFComparisonResult res = CFStringCompare(inferredLabel, expectedLabel, 0);
	if(res != kCFCompareEqualTo) {
		fprintf(stderr, "*** label miscompare in test '%s' ***\n", testName);
		fprintf(stderr, "expected label : ");
		CFShow(expectedLabel);
		fprintf(stderr, "inferred label : ");
		CFShow(inferredLabel);
		if(writeFile(CERT_FILE_OUT, signedCert.Data, signedCert.Length)) {
			fprintf(stderr, "***Error writing cert to %s\n", CERT_FILE_OUT);
		}
		else {
			fprintf(stderr, "...write %lu bytes to %s\n", (unsigned long)signedCert.Length, 
			CERT_FILE_OUT);
		}
		return -1;
	}
	
	if(labelIsCommonName) {
		CFStringRef commonName = NULL;
		ortn = SecCertificateCopyCommonName(certRef, &commonName);
		if(ortn) {
			cssmPerror("SecCertificateCopyCommonName", ortn);
			return -1;
		}
		res = CFStringCompare(inferredLabel, commonName, 0);
		if(res != kCFCompareEqualTo) {
			printf("*** CommonName miscompare in test '%s' ***\n", testName);
			printf("Common Name    : '");
			CFShow(commonName);
			printf("'\n");
			printf("inferred label : '");
			CFShow(inferredLabel);
			printf("'\n");
			if(writeFile(CERT_FILE_OUT, signedCert.Data, signedCert.Length)) {
				printf("***Error writing cert to %s\n", CERT_FILE_OUT);
			}
			else {
				printf("...write %lu bytes to %s\n", (unsigned long)signedCert.Length, 
				CERT_FILE_OUT);
			}
			return -1;
		}
		CFRelease(commonName);
	}
	CFRelease(certRef);
	CSSM_FREE(signedCert.Data);
	CB_FreeX509Name(name);
	CFRelease(inferredLabel);
	return 0;
}

int main(int argc, char **argv)
{
	bool quiet = false;
	bool doPause = false;
	
	int arg;
	while ((arg = getopt(argc, argv, "pqh")) != -1) {
		switch (arg) {
			case 'q':
				quiet = true;
				break;
			case 'p':
				doPause = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	testStartBanner("certLabelTest", argc, argv);
	
	CSSM_CL_HANDLE	clHand = clStartup();
	CSSM_CSP_HANDLE cspHand = cspStartup();
	
	/* create a key pair */
	CSSM_RETURN crtn;
	CSSM_KEY pubKey;
	CSSM_KEY privKey;
	
	crtn = cspGenKeyPair(cspHand, KEY_ALG, 
		"someLabel", 8,
		KEY_SIZE,
		&pubKey, CSSM_FALSE, CSSM_KEYUSE_ANY, CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&privKey, CSSM_FALSE, CSSM_KEYUSE_ANY, CSSM_KEYBLOB_RAW_FORMAT_NONE, 
		CSSM_FALSE);
	if(crtn) {
		printf("***Error generating RSA key pair. Aborting.\n");
		exit(1);
	}
	
	/* common params, reused for each test */
	notBefore = CB_BuildX509Time(0);
	notAfter = CB_BuildX509Time(100000);
	
	/* 
	 * Grind thru test cases.
	 */
	int ourRtn;
	
	/* very basic */
	ourRtn = doTest("simple ASCII common name", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		"Simple Name", strlen("Simple Name"), BER_TAG_PRINTABLE_STRING, &CSSMOID_CommonName,
		NULL, 0, BER_TAG_UNKNOWN, NULL, 
		CFSTR("Simple Name"), true);
	if(ourRtn) {
		exit(1);
	}
	
	/* test concatentation of description */
	ourRtn = doTest("ASCII common name plus ASCII description", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		"Simple Name", strlen("Simple Name"), BER_TAG_PRINTABLE_STRING, &CSSMOID_CommonName,
		"Description", strlen("Description"), BER_TAG_PRINTABLE_STRING, &CSSMOID_Description, 
		CFSTR("Simple Name (Description)"), false);
	if(ourRtn) {
		exit(1);
	}

	/* basic, specifying UTF8 (should be same as PRINTABLE) */
	ourRtn = doTest("simple UTF8 common name", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		"Simple Name", strlen("Simple Name"), BER_TAG_PKIX_UTF8_STRING, &CSSMOID_CommonName,
		NULL, 0, BER_TAG_UNKNOWN, NULL, 
		CFSTR("Simple Name"), true);
	if(ourRtn) {
		exit(1);
	}

	/* label from org name instead of common name */
	ourRtn = doTest("label from OrgName", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		"Simple Name", strlen("Simple Name"), BER_TAG_PRINTABLE_STRING, &CSSMOID_OrganizationName,
		NULL, 0, BER_TAG_UNKNOWN, NULL, 
		CFSTR("Simple Name"), false);
	if(ourRtn) {
		exit(1);
	}

	/* label from orgUnit name instead of common name */
	ourRtn = doTest("label from OrgUnit", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		"Simple Name", strlen("Simple Name"), BER_TAG_PRINTABLE_STRING, &CSSMOID_OrganizationalUnitName,
		NULL, 0, BER_TAG_UNKNOWN, NULL, 
		CFSTR("Simple Name"), false);
	if(ourRtn) {
		exit(1);
	}

	/* label from orgUnit name, description is ignored (it's only used if the 
	 * label comes from CommonName) */
	ourRtn = doTest("label from OrgUnit, description is ignored", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		"Simple Name", strlen("Simple Name"), BER_TAG_PRINTABLE_STRING, &CSSMOID_OrganizationalUnitName,
		"Description", strlen("Description"), BER_TAG_PRINTABLE_STRING, &CSSMOID_Description, 
		CFSTR("Simple Name"), false);
	if(ourRtn) {
		exit(1);
	}

	/* Radar 3529689: T61/Teletex, ISOLatin encoding, commonName only */
	CFStringRef t61Str = CFStringCreateWithBytes(NULL, JurgenPetersen, sizeof(JurgenPetersen),
				kCFStringEncodingISOLatin1, true);
	ourRtn = doTest("T61/Teletex name from Radar 3529689", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		JurgenPetersen, sizeof(JurgenPetersen), BER_TAG_TELETEX_STRING, &CSSMOID_CommonName,
		NULL, 0, BER_TAG_UNKNOWN, NULL, 
		t61Str, true);
	if(ourRtn) {
		exit(1);
	}
	
	/* Now convert that ISOLatin into Unicode and try with that */
	CFDataRef unicodeStr = CFStringCreateExternalRepresentation(NULL, t61Str,
			kCFStringEncodingUnicode, 0);
	if(unicodeStr == NULL) {
		printf("***Error converting to Unicode\n");
		exit(1);
	}
	ourRtn = doTest("Unicode CommonName", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		CFDataGetBytePtr(unicodeStr), CFDataGetLength(unicodeStr), 
				BER_TAG_PKIX_BMP_STRING, &CSSMOID_CommonName,
		NULL, 0, BER_TAG_UNKNOWN, NULL, 
		t61Str, true);
	if(ourRtn) {
		exit(1);
	}
	CFRelease(unicodeStr);
	
	/* Mix up ISOLatin Common Name and ASCII Description to ensure that the encodings
	 * of the two components are handled separately */
	CFMutableStringRef combo = CFStringCreateMutable(NULL, 0);
	CFStringAppend(combo, t61Str);
	CFStringAppendCString(combo, " (Description)", kCFStringEncodingASCII);
	ourRtn = doTest("ISOLatin Common Name and ASCII Description", quiet, 
		cspHand, clHand, &privKey, &pubKey,
		JurgenPetersen, sizeof(JurgenPetersen), BER_TAG_TELETEX_STRING, &CSSMOID_CommonName,
		"Description", strlen("Description"), BER_TAG_PRINTABLE_STRING, &CSSMOID_Description, 
		combo, false);
	if(ourRtn) {
		exit(1);
	}
	CFRelease(combo);
	CFRelease(t61Str);
				
	if(doPause) {
		fpurge(stdin);
		printf("Pausing for leaks testing; CR to continue: ");
		getchar();
	}
	if(!quiet) {
		printf("...success\n");
	}
	return 0;
}
