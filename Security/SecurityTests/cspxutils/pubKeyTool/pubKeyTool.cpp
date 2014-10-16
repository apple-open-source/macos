/*
 * pubKeyTool.cpp - calculate public key hash of arbitrary keys and certs; derive
 *                  public key from a private key or a cert. 
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/Security.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <security_cdsa_utils/cuCdsaUtils.h>
#include "cspwrap.h"
#include "common.h"

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("   -k priv_key_file      -- private key file to read\n");
	printf("   -b pub_key_file       -- public key file to read\n");
	printf("   -c cert_file          -- cert file to read\n");
	printf("   -d                    -- print public key digest\n");
	printf("   -o out_file           -- write public key to out_file\n");
	printf("   -f pkcs1|pkcs8|x509   -- input key format\n");
	printf("                         -- default is PKCS8 for private key, PKCS1 for"
											" public\n");
	printf("   -K keychain           -- import pub key to this keychain; workaround "
											"for Radar 4191851)\n");
	exit(1);
}

/* Convert raw key blob into a respectable CSSM_KEY. */
static CSSM_RETURN inferCssmKey(
	const CSSM_DATA &keyBlob,
	bool isPrivKey,
	CSSM_KEYBLOB_FORMAT keyForm,
	CSSM_CSP_HANDLE cspHand,
	CSSM_KEY &outKey)
{
	memset(&outKey, 0, sizeof(CSSM_KEY));
	outKey.KeyData = keyBlob;
	CSSM_KEYHEADER &hdr = outKey.KeyHeader;
	hdr.HeaderVersion = CSSM_KEYHEADER_VERSION;
	/* CspId blank */
	hdr.BlobType = CSSM_KEYBLOB_RAW;
	hdr.AlgorithmId = CSSM_ALGID_RSA;
	hdr.KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	hdr.Format = keyForm;
	hdr.KeyClass = isPrivKey ? CSSM_KEYCLASS_PRIVATE_KEY : CSSM_KEYCLASS_PUBLIC_KEY;
	hdr.KeyUsage = CSSM_KEYUSE_ANY;
	hdr.WrapAlgorithmId = CSSM_ALGID_NONE;
	hdr.WrapMode = CSSM_ALGMODE_NONE;
	/*
	 * LogicalKeySizeInBits - ask the CSP
	 */
	CSSM_KEY_SIZE keySize;
	CSSM_RETURN crtn;
	crtn = CSSM_QueryKeySizeInBits(cspHand, CSSM_INVALID_HANDLE, &outKey,
		&keySize);
	if(crtn) {
		cssmPerror("CSSM_QueryKeySizeInBits", crtn);
		return crtn;
	}
	hdr.LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;
	return CSSM_OK;
}

/*
 * Given any key in either blob or reference format,
 * obtain the associated public key's SHA-1 hash. 
 */
static CSSM_RETURN keyDigest(
	CSSM_CSP_HANDLE		cspHand,	
	const CSSM_KEY		*key,		
	CSSM_DATA_PTR		*hashData)	/* struct and contents cuAppMalloc'd and RETURNED */
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_DATA_PTR		dp;
	
	*hashData = NULL;
	
	/* validate input params */
	if((key == NULL) ||
	   (hashData == NULL)) {
	   	printf("keyHash: bogus args\n");
		return CSSMERR_CSSM_INTERNAL_ERROR;				
	}
	
	/* cook up a context for a passthrough op */
	crtn = CSSM_CSP_CreatePassThroughContext(cspHand,
	 	key,
		&ccHand);
	if(ccHand == 0) {
		cssmPerror("CSSM_CSP_CreatePassThroughContext", crtn);
		return crtn;
	}
	
	/* now it's up to the CSP */
	crtn = CSSM_CSP_PassThrough(ccHand,
		CSSM_APPLECSP_KEYDIGEST,
		NULL,
		(void **)&dp);
	if(crtn) {
		cssmPerror("CSSM_CSP_PassThrough(KEYDIGEST)", crtn);
	}
	else {
		*hashData = dp;
		crtn = CSSM_OK;
	}
	CSSM_DeleteContext(ccHand);
	return crtn;
}

/* 
 * Here's a tricky one. Given a private key, obtain the correspoding public key. 
 * This uses a private key blob format that's used internally in the CSP
 * to generate key digests. 
 */
 
/* 
 * this magic const copied from BinaryKey.h 
 */
#define CSSM_KEYBLOB_RAW_FORMAT_DIGEST	\
	(CSSM_KEYBLOB_RAW_FORMAT_VENDOR_DEFINED + 0x12345)

static CSSM_RETURN pubKeyFromPrivKey(
	CSSM_CSP_HANDLE cspHand, 
	const CSSM_KEY *privKey,			// assumed to be raw format
	CSSM_KEY *pubKey)			
{
	/* first convert to reference key */
	CSSM_KEY refKey;
	CSSM_RETURN crtn;
	crtn = cspRawKeyToRef(cspHand, privKey, &refKey);
	if(crtn) {
		return crtn;
	}
	
	/* now a NULL wrap with the magic format attribute */
	CSSM_CC_HANDLE ccHand;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_DATA descData = {0, 0};
	
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			CSSM_ALGID_NONE,
			CSSM_ALGMODE_NONE,
			NULL,			// passPhrase,
			NULL,			// key
			NULL,			// initVector,
			CSSM_PADDING_NONE,	
			NULL,			// Reserved
			&ccHand);
	if(crtn) {
		cssmPerror("CSSM_CSP_CreateSymmetricContext", crtn);
		return crtn;
	}
	crtn = AddContextAttribute(ccHand,
		/* 
		 * The output of the WrapKey is a private key as far as the CSP is 
		 * concerned, at the level that this attribute is used anyway.... 
		 */
		CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT,
		sizeof(uint32),
		CAT_Uint32,
		NULL,
		CSSM_KEYBLOB_RAW_FORMAT_DIGEST);
	if(crtn) {
		cssmPerror("CSSM_CSP_CreateSymmetricContext", crtn);
		goto errOut;
	}
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		&refKey,
		&descData,	
		pubKey);
	if(crtn) {
		cssmPerror("CSSM_WrapKey", crtn);
		goto errOut;
	}
	
	/* now: presto chango - don't do this at home! */
	pubKey->KeyHeader.KeyClass = CSSM_KEYCLASS_PUBLIC_KEY;
errOut:
	CSSM_FreeKey(cspHand, NULL, &refKey, CSSM_FALSE);
	CSSM_DeleteContext(ccHand);
	return crtn;
}

/* 
 * Import a key into a DLDB.
 */
static CSSM_RETURN importToDlDb(
	CSSM_CSP_HANDLE	cspHand,
	CSSM_DL_DB_HANDLE_PTR dlDbHand,
	const CSSM_KEY *rawPubKey,
	CSSM_DATA_PTR labelData,
	CSSM_KEY_PTR importedKey)
{
	CSSM_CC_HANDLE			ccHand = 0;
	CSSM_RETURN				crtn;
	uint32					keyAttr;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_CONTEXT_ATTRIBUTE	newAttr;	
	CSSM_DATA				descData = {0, 0};
	
	memset(importedKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				CSSM_ALGID_NONE,
				CSSM_ALGMODE_NONE,
				&creds,
				NULL,			// unwrappingKey
				NULL,			// initVector
				CSSM_PADDING_NONE,
				0,				// Params
				&ccHand);
	if(crtn) {
		cssmPerror("CSSM_CSP_CreateSymmetricContext", crtn);
		return crtn;
	}
	keyAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_PERMANENT;
	
	/* Add DLDB to context */
	newAttr.AttributeType     = CSSM_ATTRIBUTE_DL_DB_HANDLE;
	newAttr.AttributeLength   = sizeof(CSSM_ATTRIBUTE_DL_DB_HANDLE);
	newAttr.Attribute.Data    = (CSSM_DATA_PTR)dlDbHand;
	crtn = CSSM_UpdateContextAttributes(ccHand, 1, &newAttr);
	if(crtn) {
		cssmPerror("CSSM_UpdateContextAttributes", crtn);
		goto errOut;
	}
	
	/* import */
	crtn = CSSM_UnwrapKey(ccHand,
		NULL,				// PublicKey
		rawPubKey,
		CSSM_KEYUSE_ANY,
		keyAttr,
		labelData,
		NULL,				// CredAndAclEntry
		importedKey,
		&descData);			// required
	if(crtn) {
		cssmPerror("CSSM_UnwrapKey", crtn);
	}
errOut:
	if(ccHand) {
		CSSM_DeleteContext(ccHand);
	}
	return crtn;
}

/*
 * Free memory via specified plugin's app-level allocator
 */
void impExpFreeCssmMemory(
	CSSM_HANDLE		hand,
	void 			*p)
{
	CSSM_API_MEMORY_FUNCS memFuncs;
	CSSM_RETURN crtn = CSSM_GetAPIMemoryFunctions(hand, &memFuncs);
	if(crtn) {
		return;
	}
	memFuncs.free_func(p, memFuncs.AllocRef);
}

/*
 * Key attrribute names and values.
 *
 * This is where the public key hash goes.
 */
#define SEC_KEY_HASH_ATTR_NAME			"Label"

/*
 * This is where the publicly visible name goes.
 */
#define SEC_KEY_PRINT_NAME_ATTR_NAME	"PrintName"

/* 
 * Look up public key by label 
 * Set label to new specified label (SHA1 digest)
 * Set print name to new specified user-visible name
 */
static CSSM_RETURN setPubKeyLabel(
	CSSM_CSP_HANDLE 	cspHand,		// where the key lives
	CSSM_DL_DB_HANDLE 	*dlDbHand,		// ditto
	const CSSM_DATA		*existKeyLabel,	// existing label, a random string, for lookup
	const CSSM_DATA		*keyDigest,		// SHA1 digest, the new label
	const CSSM_DATA		*newPrintName)	// new user-visible name
{
	CSSM_QUERY						query;
	CSSM_SELECTION_PREDICATE		predicate;
	CSSM_DB_UNIQUE_RECORD_PTR		record = NULL;
	CSSM_RETURN						crtn;
	CSSM_HANDLE						resultHand = 0;
	
	/*
	 * Look up the key in the DL.
	 */
	query.RecordType = CSSM_DL_DB_RECORD_PUBLIC_KEY;
	query.Conjunctive = CSSM_DB_NONE;
	query.NumSelectionPredicates = 1;
	predicate.DbOperator = CSSM_DB_EQUAL;
	
	predicate.Attribute.Info.AttributeNameFormat = 
		CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	predicate.Attribute.Info.Label.AttributeName = (char *)"Label";
	predicate.Attribute.Info.AttributeFormat = 
		CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	/* hope this cast is OK */
	predicate.Attribute.Value = (CSSM_DATA_PTR)existKeyLabel;
	query.SelectionPredicate = &predicate;
	
	query.QueryLimits.TimeLimit = 0;	// FIXME - meaningful?
	query.QueryLimits.SizeLimit = 1;	// FIXME - meaningful?
	query.QueryFlags = 0; // CSSM_QUERY_RETURN_DATA;	// FIXME - used?

	/* build Record attribute with two attrs */
	CSSM_DB_RECORD_ATTRIBUTE_DATA recordAttrs;
	CSSM_DB_ATTRIBUTE_DATA attr[2];
	
	attr[0].Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr[0].Info.Label.AttributeName = (char *)SEC_KEY_HASH_ATTR_NAME;
	attr[0].Info.AttributeFormat     = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	attr[1].Info.AttributeNameFormat = CSSM_DB_ATTRIBUTE_NAME_AS_STRING;
	attr[1].Info.Label.AttributeName = (char *)SEC_KEY_PRINT_NAME_ATTR_NAME;
	attr[1].Info.AttributeFormat     = CSSM_DB_ATTRIBUTE_FORMAT_BLOB;
	
	recordAttrs.DataRecordType		 = CSSM_DL_DB_RECORD_PUBLIC_KEY;
	recordAttrs.NumberOfAttributes   = 2;
	recordAttrs.AttributeData        = attr;
	
	crtn = CSSM_DL_DataGetFirst(*dlDbHand,
		&query,
		&resultHand,
		&recordAttrs,
		NULL,			// theData
		&record);
	/* abort only on success */
	if(crtn != CSSM_OK) {
		cssmPerror("CSSM_DL_DataGetFirst", crtn);
		goto errOut;
	}
	
	/* 
	 * Update existing attr data.
	 * NOTE: the module which allocated this attribute data - a DL -
	 * was loaded and attached by the keychain layer, not by us. Thus 
	 * we can't use the memory allocator functions *we* used when 
	 * attaching to the CSP - we have to use the ones
	 * which the client registered with the DL.
	 */
	impExpFreeCssmMemory(dlDbHand->DLHandle, attr[0].Value->Data);
	impExpFreeCssmMemory(dlDbHand->DLHandle, attr[0].Value);
	impExpFreeCssmMemory(dlDbHand->DLHandle, attr[1].Value->Data);
	impExpFreeCssmMemory(dlDbHand->DLHandle, attr[1].Value);
	attr[0].Value = const_cast<CSSM_DATA *>(keyDigest);
	attr[1].Value = const_cast<CSSM_DATA *>(newPrintName);
	
	crtn = CSSM_DL_DataModify(*dlDbHand,
			CSSM_DL_DB_RECORD_PUBLIC_KEY,
			record,
			&recordAttrs,
            NULL,				// DataToBeModified
			CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);
	if(crtn) {
		cssmPerror("CSSM_DL_DataModify", crtn);
	}
errOut:
	/* free resources */
	if(resultHand) {
		CSSM_DL_DataAbortQuery(*dlDbHand, resultHand);
	}
	if(record) {
		CSSM_DL_FreeUniqueRecord(*dlDbHand, record);
	}
	return crtn;
}

#define SHA1_LABEL_LEN	20
#define IMPORTED_KEY_NAME "Imported Public Key"

/* 
 * Import a public key into a keychain, with proper Label attribute setting. 
 * A workaround for Radar 4191851.
 */
static int pubKeyImport(
	const char *kcName, 
	const CSSM_KEY *pubKey,
	CSSM_CSP_HANDLE rawCspHand)		/* raw CSP handle for calculating digest */
{
	CSSM_CSP_HANDLE cspHand;
	CSSM_DL_DB_HANDLE dlDbHand;
	OSStatus ortn;
	CSSM_RETURN crtn;
	SecKeychainRef kcRef = NULL;
	int ourRtn = 0;
	CSSM_DATA_PTR digest = NULL;
	CSSM_KEY importedKey;
	CSSM_DATA newPrintName = 
		{ (uint32)strlen(IMPORTED_KEY_NAME), (uint8 *)IMPORTED_KEY_NAME};
		
	/* NULL unwrap stuff */
	uint8 tempLabel[SHA1_LABEL_LEN];
	CSSM_DATA labelData = {SHA1_LABEL_LEN, tempLabel};
	
	ortn = SecKeychainOpen(kcName, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainOpen", ortn);
		return -1;
	}
	/* subsequent errors to errOut: */
	
	/* Get CSSM handles */
	ortn = SecKeychainGetCSPHandle(kcRef, &cspHand);
	if(ortn) {
		cssmPerror("SecKeychainGetCSPHandle", ortn);
		ourRtn = -1;
		goto errOut;
	}
	ortn = SecKeychainGetDLDBHandle(kcRef, &dlDbHand);
	if(ortn) {
		cssmPerror("SecKeychainGetCSPHandle", ortn);
		ourRtn = -1;
		goto errOut;
	}
	
	/* public key hash from raw CSP */
	crtn = keyDigest(rawCspHand, pubKey, &digest);
	if(crtn) {
		ourRtn = -1;
		goto errOut;
	}
	
	/* random label for initial storage and later retrieval */
	appGetRandomBytes(tempLabel, SHA1_LABEL_LEN);
	
	/* import the key into the keychain's DLDB */
	memset(&importedKey, 0, sizeof(CSSM_KEY));
	crtn = importToDlDb(cspHand, &dlDbHand, pubKey, &labelData, &importedKey);
	if(crtn) {
		ourRtn = -1;
		goto errOut;
	}
	
	/* don't need this */
	CSSM_FreeKey(cspHand, NULL, &importedKey, CSSM_FALSE);
	
	/* update the label and printName attributes */
	crtn = setPubKeyLabel(cspHand, &dlDbHand, &labelData, digest, &newPrintName);
	if(crtn) {
		ourRtn = -1;
	}
errOut:
	CFRelease(kcRef);
	if(digest) {
		APP_FREE(digest->Data);
		APP_FREE(digest);
	}
	return ourRtn;
}

int main(int argc, char **argv)
{
	char *privKeyFile = NULL;
	char *pubKeyFile = NULL;
	char *certFile = NULL;
	char *outFile = NULL;
	bool printDigest = false;
	CSSM_KEYBLOB_FORMAT keyForm = CSSM_KEYBLOB_RAW_FORMAT_NONE;
	char *kcName = NULL;
	
	if(argc < 3) {
		usage(argv);
	}
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "k:b:c:do:f:K:h")) != -1) {
		switch (arg) {
			case 'k':
				privKeyFile = optarg;
				break;
			case 'b':
				pubKeyFile = optarg;
				break;
			case 'c':
				certFile = optarg;
				break;
			case 'd':
				printDigest = true;
				break;
			case 'o':
				outFile = optarg;
				break;
			case 'f':
				if(!strcmp("pkcs1", optarg)) {
					keyForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
				}
				else if(!strcmp("pkcs8", optarg)) {
					keyForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
				}
				else if(!strcmp("x509", optarg)) {
					keyForm = CSSM_KEYBLOB_RAW_FORMAT_X509;
				}
				break;
			case 'K':
				kcName = optarg;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	CSSM_DATA privKeyBlob = {0, NULL};
	CSSM_DATA pubKeyBlob = {0, NULL};
	CSSM_KEY thePrivKey;			// constructed
	CSSM_KEY thePubKey;				// null-wrapped
	CSSM_KEY_PTR pubKey = NULL;
	CSSM_KEY_PTR privKey = NULL;
	CSSM_RETURN crtn;
	CSSM_CL_HANDLE clHand = 0;
	CSSM_CSP_HANDLE cspHand = cuCspStartup(CSSM_TRUE);

	/* gather input */
	if(privKeyFile) {
		/* key blob from a file ==> a private CSSM_KEY */

		if(pubKeyFile || certFile) {
			printf("****Specify exactly one of {cert_file, priv_key_file, "
					"pub_key_file}.\n");
			exit(1);
		}
		unsigned len;
		if(readFile(privKeyFile, &privKeyBlob.Data, &len)) {
			printf("***Error reading private key from %s. Aborting.\n", privKeyFile);
			exit(1);
		}
		privKeyBlob.Length = len;
		if(keyForm == CSSM_KEYBLOB_RAW_FORMAT_NONE) {
			/* default for private keys */
			keyForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
		}
		crtn = inferCssmKey(privKeyBlob, true, keyForm, cspHand, thePrivKey);
		if(crtn) {
			goto errOut;
		}
		privKey = &thePrivKey;
	}
	if(pubKeyFile) {
		/* key blob from a file ==> a public CSSM_KEY */

		if(privKeyFile || certFile) {
			printf("****Specify exactly one of {cert_file, priv_key_file, "
					"pub_key_file}.\n");
			exit(1);
		}
		
		unsigned len;
		if(readFile(pubKeyFile, &pubKeyBlob.Data, &len)) {
			printf("***Error reading public key from %s. Aborting.\n", pubKeyFile);
			exit(1);
		}
		pubKeyBlob.Length = len;
		if(keyForm == CSSM_KEYBLOB_RAW_FORMAT_NONE) {
			/* default for public keys */
			keyForm = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
		}
		crtn = inferCssmKey(pubKeyBlob, false, keyForm, cspHand, thePubKey);
		if(crtn) {
			goto errOut;
		}
		pubKey = &thePubKey;
	}
	if(certFile) {
		/* cert from a file ==> a public CSSM_KEY */

		if(privKeyFile || pubKeyFile) {
			printf("****Specify exactly one of {cert_file, priv_key_file, "
					"pub_key_file}.\n");
			exit(1);
		}
		
		CSSM_DATA certData = {0, NULL};
		unsigned len;
		if(readFile(certFile, &certData.Data, &len)) {
			printf("***Error reading cert from %s. Aborting.\n", certFile);
			exit(1);
		}
		certData.Length = len;
		
		/* Extract public key - that's what we will be using later */
		clHand = cuClStartup();
		crtn = CSSM_CL_CertGetKeyInfo(clHand, &certData, &pubKey);
		if(crtn) {
			cssmPerror("CSSM_CL_CertGetKeyInfo", crtn);
			goto errOut;
		}
	}
	
	/* now do something useful */
	if(printDigest) {
		CSSM_KEY_PTR theKey = privKey;
		if(theKey == NULL) {
			/* maybe we got public key from a cert */
			theKey = pubKey;
		}
		if(theKey == NULL) {
			printf("***Can't calculate digest because I don't have a key or a clue.\n");
			goto errOut;
		}
		CSSM_DATA_PTR dig = NULL;
		crtn = keyDigest(cspHand, theKey, &dig);
		if(crtn) {
			printf("Sorry, can't get the digest for this key.\n");
			goto errOut;
		}
		if((dig == NULL) || (dig->Length == 0)) {
			printf("Screwup calculating digest.\n");
			goto errOut;
		}
		printf("Key Digest:\n");
		for(unsigned dex=0; dex<dig->Length; dex++) {
			printf("%02X ", dig->Data[dex]);
		}
		printf("\n");
		APP_FREE(dig->Data);
		APP_FREE(dig);
	}
	
	if(outFile || kcName) {
		/* get a public key if we don't already have one */
		if(pubKey == NULL) {
			if(privKey == NULL) {
				printf("***PubKey file name specified but no privKey or cert. "	
						"Aborting.\n");
				goto errOut;
			}
			crtn = pubKeyFromPrivKey(cspHand, privKey, &thePubKey);
			if(crtn) {
				goto errOut;
			}
			pubKey = &thePubKey;
		}
	}
	if(outFile) {
		if(writeFile(outFile, pubKey->KeyData.Data, pubKey->KeyData.Length)) {
			printf("***Error writing to %s.\n", outFile);
		}
		else {
			printf("...%lu bytes written to %s.\n", pubKey->KeyData.Length, outFile);
		}
	}
	if(kcName) {
		if(pubKeyImport(kcName, pubKey, cspHand) == 0) {
			printf("....public key %s imported to %s\n", pubKeyFile, kcName);
		}
		else {
			printf("***Error importing public key %s to %s\n", pubKeyFile, kcName);
		}
	}
errOut:
	/* clean up here if you must */
	return 0;
}
