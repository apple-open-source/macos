/*
 * certTime - measure performacne of cert parse and build.
 */
 
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/CertBuilderApp.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <Security/cssm.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/certextensions.h>
#include <CoreFoundation/CoreFoundation.h>
#include "extenCooker.h"

#define KEYSIZE_DEF			1024
#define CL_KEY_VIA_GET_KEY	0

static void usage(char **argv)
{
	printf("Usage: %s op loops [options]\n", argv[0]);
	printf("Op:\n");
	printf("   p  parse\n");
	printf("   g  parse & get all fields\n");
	#if CL_KEY_VIA_GET_KEY
	printf("   t  parse & get some fields, emulating TPCertInfo, GetKeyInfo\n");
	#else
	printf("   t  parse & get some fields, emulating TPCertInfo, fetchField(key)\n");
	#endif
	printf("   c  create\n");
	printf("   s  create & sign\n");
	printf("   v  verify\n");
	printf("Options:\n");
	printf("   b  RSA blinding on\n");
	printf("   k=keysize (default = %d)\n", KEYSIZE_DEF);
	exit(1);
}

/*
 * The certs we'll be parsing
 */
static const char *certNames[] = 
{
	"anchor_0",		// GTE CyberTrust Root, no extens
	"anchor_9",		// VeriSign, no extens
	"anchor_34",	// TrustCenter, 6 extens
	"anchor_44",	// USERTRUST, 5 extens, incl. cRLDistributionPoints
	"anchor_76",	// QuoVadis, 6 extens, incl. authorityInfoAccess
	"anchor_80",	// KMD-CA Kvalificeret3 6 extens
};

#define NUM_PARSED_CERTS	(sizeof(certNames) / sizeof(certNames[0]))

/* dummy RDN - subject and issuer - we aren't testing this */
CB_NameOid dummyRdn[] = 
{
	{ "Apple Computer",					&CSSMOID_OrganizationName },
	{ "Doug Mitchell",					&CSSMOID_CommonName }
};
#define NUM_DUMMY_NAMES	(sizeof(dummyRdn) / sizeof(CB_NameOid))

#define KEY_ALG			CSSM_ALGID_RSA
#define SIG_ALG			CSSM_ALGID_SHA1WithRSA
#define SUBJ_KEY_LABEL	"subjectKey"


/*
 * Set of extensions we'll be creating 
 */
/* empty freeFcn means no extension-specific resources to free */
#define NO_FREE		NULL

static ExtenTest extenTests[] = {
	{ kuCreate, kuCompare, NO_FREE, 
	  sizeof(CE_KeyUsage), CSSMOID_KeyUsage, 
	  "KeyUsage", 'k' },
	{ ekuCreate, ekuCompare, NO_FREE,
	  sizeof(CE_ExtendedKeyUsage), CSSMOID_ExtendedKeyUsage,
	  "ExtendedKeyUsage", 'x' },
	{ authKeyIdCreate, authKeyIdCompare, authKeyIdFree,
	  sizeof(CE_AuthorityKeyID), CSSMOID_AuthorityKeyIdentifier, 
	  "AuthorityKeyID", 'a' },
	{ genNamesCreate, genNamesCompare, genNamesFree, 
	  sizeof(CE_GeneralNames), CSSMOID_SubjectAltName,
	  "SubjectAltName", 't' },
};

#define MAX_EXTENSIONS		(sizeof(extenTests) / sizeof(ExtenTest))

static int doParse(
	CSSM_CL_HANDLE	clHand,
	const CSSM_DATA	&cert,
	unsigned 		loops)
{
	CSSM_HANDLE	cacheHand;
	CSSM_RETURN crtn;
	
	for(unsigned loop=0; loop<loops; loop++) {
		crtn = CSSM_CL_CertCache(clHand, &cert, &cacheHand);
		if(crtn) {
			printError("CSSM_CL_CertCache", crtn);
			return 1;
		}
		crtn = CSSM_CL_CertAbortCache(clHand, cacheHand);
		if(crtn) {
			printError("CSSM_CL_CrlAbortCache", crtn);
			return 1;
		}
	}
	return 0;
}

/* Emulate TPCertInfo constructor */

static CSSM_RETURN fetchCertField(
	CSSM_CL_HANDLE	clHand,
	CSSM_HANDLE		certHand,
	const CSSM_OID	*fieldOid,
	CSSM_DATA_PTR	*fieldData)		// mallocd by CL and RETURNED
{
	CSSM_RETURN crtn;
	
	uint32 NumberOfFields = 0;
	CSSM_HANDLE resultHand = 0;
	*fieldData = NULL;
	crtn = CSSM_CL_CertGetFirstCachedFieldValue(
		clHand,
		certHand,
	    fieldOid,
	    &resultHand,
	    &NumberOfFields,
		fieldData);
	if(crtn) {
		printError("fetchCertField", crtn);
		return crtn;
	}
	if(NumberOfFields != 1) {
		printf("***fetchCertField: numFields %d, expected 1\n", 
			(int)NumberOfFields);
	}
  	CSSM_CL_CertAbortQuery(clHand, resultHand);
	return CSSM_OK;
}
	

static int doGetSomeFields(
	CSSM_CL_HANDLE	clHand,
	const CSSM_DATA	&cert,
	unsigned 		loops)
{
	CSSM_HANDLE	cacheHand;
	CSSM_RETURN crtn;
	
	/* fetched by TPClItemInfo constructor */
	CSSM_DATA_PTR issuerName;
	CSSM_DATA_PTR sigAlg;
	CSSM_DATA_PTR notBefore;
	CSSM_DATA_PTR notAfter;
	/* fetched by TPCertInfo */
	CSSM_DATA_PTR subjectName;
	#if CL_KEY_VIA_GET_KEY
	CSSM_KEY_PTR subjPubKey;
	#else
	CSSM_DATA_PTR subjPubKeyData;
	#endif
	
	for(unsigned loop=0; loop<loops; loop++) {
		/* parse and cache */
		crtn = CSSM_CL_CertCache(clHand, &cert, &cacheHand);
		if(crtn) {
			printError("CSSM_CL_CertCache", crtn);
			return 1;
		}
		/* fetch the fields */
		fetchCertField(clHand, cacheHand, &CSSMOID_X509V1IssuerName, &issuerName);
		fetchCertField(clHand, cacheHand, &CSSMOID_X509V1SignatureAlgorithmTBS, 
			&sigAlg);
		fetchCertField(clHand, cacheHand, &CSSMOID_X509V1ValidityNotBefore, 
			&notBefore);
		fetchCertField(clHand, cacheHand, &CSSMOID_X509V1ValidityNotAfter, &notAfter);
		fetchCertField(clHand, cacheHand, &CSSMOID_X509V1SubjectName, &subjectName);
		#if CL_KEY_VIA_GET_KEY
		CSSM_CL_CertGetKeyInfo(clHand, &cert, &subjPubKey);
		#else
		fetchCertField(clHand, cacheHand, &CSSMOID_CSSMKeyStruct, &subjPubKeyData);
		#endif
		
		/* free the fields */
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1IssuerName, issuerName);
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SignatureAlgorithmTBS, sigAlg);
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1ValidityNotBefore, notBefore);
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1ValidityNotAfter, notAfter);
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_X509V1SubjectName, subjectName);
		#if CL_KEY_VIA_GET_KEY
		appFree(subjPubKey->KeyData.Data, 0);
		appFree(subjPubKey, 0);
		#else
		CSSM_CL_FreeFieldValue(clHand, &CSSMOID_CSSMKeyStruct, subjPubKeyData);
		#endif
		
		crtn = CSSM_CL_CertAbortCache(clHand, cacheHand);
		if(crtn) {
			printError("CSSM_CL_CrlAbortCache", crtn);
			return 1;
		}
	}
	return 0;
}

static int doGetFields(
	CSSM_CL_HANDLE	clHand,
	const CSSM_DATA	&cert,
	unsigned 		loops)
{
	uint32 numFields;
	CSSM_FIELD_PTR certFields;
	CSSM_RETURN crtn;
	
	for(unsigned loop=0; loop<loops; loop++) {
		crtn = CSSM_CL_CertGetAllFields(clHand, &cert, &numFields, 
				&certFields);
		if(crtn) {
			printError("CSSM_CL_CertGetAllFields", crtn);
			return 1;
		}
		crtn = CSSM_CL_FreeFields(clHand, numFields, &certFields);
		if(crtn) {
			printError("CSSM_CL_FreeFields", crtn);
			return 1;
		}
	}
	return 0;
}

static int doVerify(
	CSSM_CL_HANDLE	clHand,
	const CSSM_DATA	&cert,
	unsigned 		loops)
{
	CSSM_RETURN crtn;
	
	for(unsigned loop=0; loop<loops; loop++) {
		crtn = CSSM_CL_CertVerify(clHand, 
			CSSM_INVALID_HANDLE,
			&cert, 
			&cert,
			NULL,	// VerifyScope
			0);		// ScopeSize
		if(crtn) {
			printError("CSSM_CL_CertVerify", crtn);
			return 1;
		}
	}
	return 0;
}

/*
 * Stuff to be created before entering the timed cert create routine.
 */
typedef struct {
	CSSM_KEY			privKey;
	CSSM_KEY			pubKey;
	CSSM_X509_NAME		*dummyName;
	CSSM_X509_TIME		*notBefore;
	CSSM_X509_TIME		*notAfter;
	CSSM_X509_EXTENSION	extens[MAX_EXTENSIONS];
} PresetParams;

/*
 * One-time only setup of cert creation params.
 */
 static int createSetup(
	CSSM_CL_HANDLE		clHand,
	CSSM_CSP_HANDLE		cspHand,
	unsigned			keySize,
	PresetParams		&params)
{
	CSSM_RETURN crtn;
	
	crtn = cspGenKeyPair(cspHand,
		KEY_ALG,
		SUBJ_KEY_LABEL,
		strlen(SUBJ_KEY_LABEL),
		keySize,
		&params.pubKey,
		CSSM_FALSE,		// pubIsRef
		CSSM_KEYUSE_VERIFY,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		&params.privKey,
		CSSM_TRUE,			// privIsRef - doesn't matter
		CSSM_KEYUSE_SIGN,
		CSSM_KEYBLOB_RAW_FORMAT_NONE,
		CSSM_FALSE);
	if(crtn) {
		return 1;
	}
	params.dummyName = CB_BuildX509Name(dummyRdn, NUM_DUMMY_NAMES);
	if(params.dummyName == NULL) {
		printf("CB_BuildX509Name failure");
		return 1;
	}
	params.notBefore = CB_BuildX509Time(0);
	params.notAfter  = CB_BuildX509Time(10000);
	
	/* now some extensions */
	for(unsigned dex=0; dex<MAX_EXTENSIONS; dex++) {
		CSSM_X509_EXTENSION &extn = params.extens[dex];
		ExtenTest &etest = extenTests[dex];
		
		void *extVal = CSSM_MALLOC(etest.extenSize);
		memset(extVal, 0, etest.extenSize);
		etest.createFcn(extVal);
		
		extn.extnId   			= etest.extenOid;
		extn.critical 			= randBool();
		extn.format   			= CSSM_X509_DATAFORMAT_PARSED;
		extn.value.parsedValue 	= extVal;
		extn.BERvalue.Data = NULL;
		extn.BERvalue.Length = 0;
	}
	return 0;
}

static int doCreate(
	CSSM_CL_HANDLE 			clHand,
	CSSM_CSP_HANDLE			cspHand,
	unsigned				loops,
	PresetParams			&params,
	bool					doSign,
	bool					rsaBlind)
{
	for(unsigned loop=0; loop<loops; loop++) {
		CSSM_DATA_PTR rawCert = CB_MakeCertTemplate(clHand,
			0x12345678,			// serial number
			params.dummyName,
			params.dummyName,
			params.notBefore,
			params.notAfter,
			&params.pubKey,
			SIG_ALG,
			NULL,				// subjUniqueId
			NULL,				// issuerUniqueId
			params.extens,		// extensions
			/* vary numExtensions per loop */
			loop % MAX_EXTENSIONS);
		if(rawCert == NULL) {
			printf("Error creating cert template.\n");
			return 1;
		}
		if(doSign) {
			CSSM_DATA signedCert = {0, NULL};
			CSSM_CC_HANDLE sigHand;
			CSSM_RETURN crtn = CSSM_CSP_CreateSignatureContext(cspHand,
					SIG_ALG,
					NULL,			// no passphrase for now
					&params.privKey,
					&sigHand);
			if(crtn) {
				printError("CreateSignatureContext", crtn);
				return 1;
			}
			
			if(rsaBlind) {
				CSSM_CONTEXT_ATTRIBUTE	newAttr;	
				newAttr.AttributeType     = CSSM_ATTRIBUTE_RSA_BLINDING;
				newAttr.AttributeLength   = sizeof(uint32);
				newAttr.Attribute.Uint32  = 1;
				crtn = CSSM_UpdateContextAttributes(sigHand, 1, &newAttr);
				if(crtn) {
					printError("CSSM_UpdateContextAttributes", crtn);
					return crtn;
				}
			}

			crtn = CSSM_CL_CertSign(clHand,
				sigHand,
				rawCert,			// CertToBeSigned
				NULL,				// SignScope per spec
				0,					// ScopeSize per spec
				&signedCert);
			if(crtn) {
				printError("CSSM_CL_CertSign", crtn);
				return 1;
			}
			CSSM_DeleteContext(sigHand);
			CSSM_FREE(signedCert.Data);
		}
		CSSM_FREE(rawCert->Data);
		CSSM_FREE(rawCert);
	}
	return 0;
}

typedef enum {
	CTO_Parse,
	CTO_GetFields,
	CTO_GetSomeFields,
	CTO_Create,			// sign is an option for this one
	CTO_Verify
} CT_Op;

int main(int argc, char **argv)
{
	CSSM_CL_HANDLE		clHand;
	CSSM_CSP_HANDLE		cspHand;
	int					arg;
	int					rtn;
	char				*argp;
	unsigned			i;
	PresetParams		params;
	CSSM_DATA			certData[NUM_PARSED_CERTS];
	
	/* user-specificied params */
	CT_Op				op;
	unsigned			loops = 0;
	bool				doSign = false;
	const char			*opStr = NULL;
	bool				rsaBlinding = false;
	unsigned			keySize = KEYSIZE_DEF;
	
	if(argc < 3) {
		usage(argv);
	}
	switch(argv[1][0]) {
		case 'p':
			op = CTO_Parse;
			opStr = "Parsed";
			break;
		case 'g':
			op = CTO_GetFields;
			opStr = "Parsed with GetAllFields";
			break;
		case 't':
			op = CTO_GetSomeFields;
			#if CL_KEY_VIA_GET_KEY
			opStr = "Parsed with some GetFields and GetKeyInfo";
			#else
			opStr = "Parsed with some GetFields";
			#endif
			break;
		case 'c':
			op = CTO_Create;
			opStr = "Created";
			break;
		case 's':
			op = CTO_Create;
			opStr = "Created and Signed";
			doSign = true;
			break;
		case 'v':
			op = CTO_Verify;
			opStr = "Verified";
			break;
		default:
			usage(argv);
	}
	
	loops = atoi(argv[2]);
	for(arg=3; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'b':
				rsaBlinding = true;
				break;
			case 'k':
				keySize = atoi(&argp[2]);
				break;
			default:
				usage(argv);
		}
	}
	
	/* common setup */
	clHand = clStartup();
	if(clHand == 0) {
		return 0;
	}
	cspHand = cspStartup();
	if(cspHand == 0) {
		return 0;
	}
	
	/* per-test setup */
	switch(op) {
		unsigned dex;
		unsigned len;
		
		case CTO_Parse:
		case CTO_GetFields:
		case CTO_GetSomeFields:
		case CTO_Verify:
			/* read in the certs */
			for(dex=0; dex<NUM_PARSED_CERTS; dex++) {
				CSSM_DATA &cdata = certData[dex];
				if(readFile(certNames[dex], 
					(unsigned char **)&cdata.Data, 
					&len)) {
					printf("Error reading cert %s. Aborting.\n",
						certNames[dex]);
					exit(1);
				}
				cdata.Length = len;
			}
			break;
		case CTO_Create:
			/* set up keys, names */
			if(createSetup(clHand, cspHand, keySize, params)) {
				exit(1);
			}
			break;
	}
	
	/* one loop outside of timer to heat up test bed */
	switch(op) {
		case CTO_Parse:
			rtn = doParse(clHand, certData[0], 1);
			break;
		case CTO_GetFields:
			rtn = doGetFields(clHand, certData[0], 1);
			break;
		case CTO_GetSomeFields:
			rtn = doGetSomeFields(clHand, certData[0], 1);
			break;
		case CTO_Verify:
			rtn = doVerify(clHand, certData[0], 1);
			break;
		case CTO_Create:
			rtn = doCreate(clHand, cspHand, 1, params, true, rsaBlinding);
			break;
	}
	if(rtn) {
		printf("This program needs work. Try again.\n");
		return 1;
	}
	
	CFAbsoluteTime startTime, endTime;
	startTime = CFAbsoluteTimeGetCurrent();

	/* begin timed loop */
	switch(op) {
		case CTO_Parse:
			for(i=0; i<NUM_PARSED_CERTS; i++) {
				rtn = doParse(clHand, certData[i], loops);
				if(rtn) {
					break;
				}	
			}
			break;
		case CTO_GetFields:
			for(i=0; i<NUM_PARSED_CERTS; i++) {
				rtn = doGetFields(clHand, certData[i], loops);
				if(rtn) {
					break;
				}	
			}
			break;
		case CTO_GetSomeFields:
			for(i=0; i<NUM_PARSED_CERTS; i++) {
				rtn = doGetSomeFields(clHand, certData[i], loops);
				if(rtn) {
					break;
				}	
			}
			break;
		case CTO_Verify:
			for(i=0; i<NUM_PARSED_CERTS; i++) {
				rtn = doVerify(clHand, certData[i], loops);
				if(rtn) {
					break;
				}	
			}
			break;
		case CTO_Create:
			rtn = doCreate(clHand, cspHand, loops, params, doSign, 
				rsaBlinding);
			break;
	}
	endTime = CFAbsoluteTimeGetCurrent();
	CFAbsoluteTime deltaTime = endTime - startTime;
	
	if(rtn) {
		printf("Error in main loop. Try again.\n");
		return 1;
	}
	
	unsigned numCerts = loops;
	if(op != CTO_Create) {
		numCerts *= NUM_PARSED_CERTS;
	}
	
	printf("=== %u certs %s ===\n", numCerts, opStr);
	printf("Total time %g s\n", deltaTime);
	printf("%g ms per cert\n", (deltaTime / (double)numCerts) * 1000.0);
	
	/* cleanup */
	if(op == CTO_Create) {
		CB_FreeX509Name(params.dummyName);
		CB_FreeX509Time(params.notBefore);
		CB_FreeX509Time(params.notAfter);
		cspFreeKey(cspHand, &params.pubKey);
		cspFreeKey(cspHand, &params.privKey);
	}
	else {
		for(i=0; i<NUM_PARSED_CERTS; i++) {
			free(certData[i].Data);
		}
	}
	CSSM_ModuleDetach(cspHand);
	CSSM_ModuleDetach(clHand);
	return 0;
}

