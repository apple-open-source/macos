/*
 * rsatool.c - RSA/DSA/ECDSA key pair generator, en/decrypt, sign/verify with file I/O
 */
 
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <Security/cssm.h>
#include "cspwrap.h"
#include "common.h"
#include <security_cdsa_utils/cuFileIo.h>

/* For 3141770 - defined true when PR-3074739 merged to TOT Security */
#define OPENSSL_ENABLE		1

#define USAGE_NAME				"noUsage"
#define USAGE_NAME_LEN			(strlen(USAGE_NAME))
#define DEFAULT_KEY_SIZE_BITS	512

typedef struct {
	CSSM_ALGORITHMS		alg;
	uint32				keySizeInBits;
	CSSM_CSP_HANDLE		cspHand;
	char				*keyFileName;
	char				*outKeyFileName;	// for pub key convert 
	char				*plainFileName;
	char				*sigFileName;
	char				*cipherFileName;
	char				*dsaParamFileIn;
	char				*dsaParamFileOut;
	CSSM_BOOL			swapKeyClass;
	CSSM_BOOL			rawSign;
	CSSM_BOOL			noPad;
	CSSM_BOOL			quiet;
	CSSM_KEYBLOB_FORMAT	pubKeyFormat; 	// FORMAT_NONE ==> default
	CSSM_KEYBLOB_FORMAT	privKeyFormat; 	// FORMAT_NONE ==> default
	CSSM_KEYBLOB_FORMAT	outPubKeyFormat;// FORMAT_NONE ==> default, for pub key convert
	CSSM_ALGORITHMS		digestAlg;		// optional digest alg for raw sign/verify
} opParams;

static void usage(char **argv) 
{
	printf("usage: %s op [options]\n", argv[0]);
	printf("  op:\n");
	printf("     g  generate key pair\n");
	printf("     e  encrypt\n");
	printf("     d  decrypt\n");
	printf("     s  sign\n");
	printf("     v  verify\n");
	printf("     S  SHA-1 digest\n");
	printf("     M  MD5 digest\n");
	printf("     C  convert public key format\n");
	printf("  options:\n");
	printf("     k=keyfileBase keys are keyFileBase_pub.der, "
					"keyFileBase_priv.der)\n");
	printf("     K=outputPublicKey\n");
	printf("     p=plainFile\n");
	printf("     c=cipherFile\n");
	printf("     s=sigfile\n");
	printf("     z=keySizeInBits (default %d)\n", DEFAULT_KEY_SIZE_BITS);
	printf("     w (swap key class)\n");
	printf("     r (raw sign/verify)\n");
	printf("     P (no padding)\n");
	printf("     d=digestAlg   digestAlg: s(SHA1) 5(MD5) for raw signing\n");
	printf("     m=dsaParamFileIn\n");
	printf("     M=dsaParamFileOut (must specify one of dsaParamFile{In,Out}\n");
	printf("     b=[1xboOL] (pub  key in PKCS1/X509/BSAFE/OpenSSH1/OpenSSH2/OpenSSL form)\n");
	printf("          RSA = {PKCS1,X509,OpenSSH1,OpenSSH2}     default = PKCS1\n");
	printf("          DSA = {BSAFE,X509,OpenSSH2}              default = X509\n");
	printf("          ECDSA = {X509, OpenSSL}                  default = X509\n");
	printf("          Note: RSA and DSA public keys in OpenSSL form are X509.\n");
	printf("     v=[18sbo] (priv key in PKCS1/PKCS8/OpenSSH/BSAFE/OpenSSL form)\n");
	printf("          RSA = {PKCS1,PKCS8,OpenSSH1}             default = PKCS8\n");
	printf("          DSA = {BSAFE,OpenSSL,PKCS8}              default = OpenSSL\n");
	printf("          ECDSA = {PKCS8,OpenSSL}                  default = OpenSSL}\n");
	printf("          Note: RSA private key in OpenSSL form is PKCS1.\n");
	printf("     B=[1xboO] output public key format\n");
	printf("     a=alg   d=DSA, r=RSA, e=ECDSA; default = RSA\n");
	printf("     q(uiet)\n");
	exit(1);
}

/* NULL wrap a key to specified format. */
static CSSM_RETURN nullWrapKey(CSSM_CSP_HANDLE cspHand,
	const CSSM_KEY			*refKey,	
	CSSM_KEYBLOB_FORMAT		blobFormat,
	CSSM_KEY_PTR			rawKey)			// RETURNED
{
	CSSM_CC_HANDLE		ccHand;
	CSSM_RETURN			crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	CSSM_DATA descData = {0, 0};
	
	memset(rawKey, 0, sizeof(CSSM_KEY));
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
				CSSM_ALGID_NONE,
				CSSM_ALGMODE_NONE,
				&creds,			// passPhrase
				NULL,			// unwrappingKey
				NULL,			// initVector
				CSSM_PADDING_NONE,
				0,				// Params
				&ccHand);
	if(crtn) {
		printError("cspWrapKey/CreateContext", crtn);
		return crtn;
	}
	if(blobFormat != CSSM_KEYBLOB_WRAPPED_FORMAT_NONE) {
		/* only add this attribute if it's not the default */
		CSSM_ATTRIBUTE_TYPE attrType;
	
		switch(refKey->KeyHeader.KeyClass) {
			case CSSM_KEYCLASS_SESSION_KEY:
				attrType = CSSM_ATTRIBUTE_SYMMETRIC_KEY_FORMAT;
				break;
			case CSSM_KEYCLASS_PUBLIC_KEY:
				attrType = CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT;
				break;
			case CSSM_KEYCLASS_PRIVATE_KEY:
				attrType = CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT;
				break;
			default:
				printf("***Bogus KeyClass in nullWrapKey\n");
				return -1;
		}
		CSSM_CONTEXT_ATTRIBUTE attr;
		attr.AttributeType = attrType;
		attr.AttributeLength = sizeof(uint32);
		attr.Attribute.Uint32 = blobFormat;
		crtn = CSSM_UpdateContextAttributes(
			ccHand,
			1,
			&attr);
		if(crtn) {
			printError("CSSM_UpdateContextAttributes", crtn);
			return crtn;
		}
	}
	crtn = CSSM_WrapKey(ccHand,
		&creds,
		refKey,
		&descData,	
		rawKey);
	if(crtn != CSSM_OK) {
		printError("CSSM_WrapKey", crtn);
	}
	if(CSSM_DeleteContext(ccHand)) {
		printf("CSSM_DeleteContext failure\n");
	}
	return crtn;
}

/* 
 * Sign/verify optional "no padding" context attr 
 */
static CSSM_RETURN sigSign(CSSM_CSP_HANDLE cspHand,
	uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
	CSSM_KEY_PTR key,					// private key
	const CSSM_DATA *text,
	CSSM_DATA_PTR sig,
	uint32 digestAlg,					// optional for raw signing
	CSSM_BOOL noPad)					// true --> add PADDING_NONE to context
{
	CSSM_CC_HANDLE	sigHand;
	CSSM_RETURN		crtn;
	
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		algorithm,
		NULL,				// passPhrase
		key,
		&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext", crtn);
		return crtn;
	}
	if(noPad) {
		crtn = AddContextAttribute(sigHand,
			CSSM_ATTRIBUTE_PADDING,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			CSSM_PADDING_NONE);
		if(crtn) {
			return crtn;
		}
	}
	crtn = CSSM_SignData(sigHand,
		text,
		1,
		digestAlg,
		sig);
	if(crtn) {
		printError("CSSM_SignData", crtn);
	}
	CSSM_DeleteContext(sigHand);
	return crtn;
}

static CSSM_RETURN sigVerify(CSSM_CSP_HANDLE cspHand,
	uint32 algorithm,					// CSSM_ALGID_FEE_MD5, etc.
	CSSM_KEY_PTR key,					// public key
	const CSSM_DATA *text,
	const CSSM_DATA *sig,
	uint32 digestAlg,					// optional for raw signing
	CSSM_BOOL noPad)					// true --> add PADDING_NONE to context
{
	CSSM_CC_HANDLE	sigHand;
	CSSM_RETURN		crtn;
	
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		algorithm,
		NULL,				// passPhrase
		key,
		&sigHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext", crtn);
		return crtn;
	}
	if(noPad) {
		crtn = AddContextAttribute(sigHand,
			CSSM_ATTRIBUTE_PADDING,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			CSSM_PADDING_NONE);
		if(crtn) {
			return crtn;
		}
	}
	crtn = CSSM_VerifyData(sigHand,
		text,
		1,
		digestAlg,
		sig);
	if(crtn) {
		printError("CSSM_VerifyData", crtn);
	}
	CSSM_DeleteContext(sigHand);
	return crtn;
}

/*
 * Generate DSA key pair. Algorithm parameters are
 * either specified by caller via inParams, or are generated here
 * and returned to caller in outParams. Exactly one of (inParams,
 * outParams) must be non-NULL.
 */
static CSSM_RETURN genDsaKeyPair(
	CSSM_CSP_HANDLE cspHand,
	const char *keyLabel,
	unsigned keyLabelLen,
	uint32 keySize,					// in bits
	CSSM_KEY_PTR pubKey,			// mallocd by caller
	CSSM_BOOL pubIsRef,				// true - reference key, false - data
	uint32 pubKeyUsage,				// CSSM_KEYUSE_ENCRYPT, etc.
	CSSM_KEYBLOB_FORMAT pubFormat,	// Optional. Some algorithms (currently, FEE)
									//   provide for multiple key blob formats.
									//   Specify 0 or CSSM_KEYBLOB_RAW_FORMAT_NONE
									//   to get the default format. 
	CSSM_KEY_PTR privKey,			// mallocd by caller
	CSSM_BOOL privIsRef,			// true - reference key, false - data
	uint32 privKeyUsage,			// CSSM_KEYUSE_DECRYPT, etc.
	CSSM_KEYBLOB_FORMAT privFormat,	// optional 0 ==> default
	const CSSM_DATA	*inParams,		// optional 
	CSSM_DATA_PTR	outParams)		// optional, we malloc
{
	CSSM_RETURN				crtn;
	CSSM_CC_HANDLE 			ccHand;
	CSSM_DATA				keyLabelData;
	uint32					pubAttr;
	uint32					privAttr;
	CSSM_RETURN 			ocrtn = CSSM_OK;
	
	/* Caller must specify either inParams or outParams, not both */
	if(inParams && outParams) {
		return CSSMERR_CSSM_INVALID_POINTER;
	}
	if(!inParams && !outParams) {
		return CSSMERR_CSSM_INVALID_POINTER;
	}

	keyLabelData.Data        = (uint8 *)keyLabel,
	keyLabelData.Length      = keyLabelLen;
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));
	
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		CSSM_ALGID_DSA,
		keySize,
		NULL,					// Seed
		NULL,					// Salt
		NULL,					// StartDate
		NULL,					// EndDate
		inParams,				// Params, may be NULL
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		return crtn;
	}
	/* cook up attribute bits */
	if(pubIsRef) {
		pubAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		pubAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}
	if(privIsRef) {
		privAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE;
	}
	else {
		privAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}

	if(outParams) {
		/* explicitly generate params and return them to caller */
		outParams->Data = NULL;
		outParams->Length = 0;
		crtn = CSSM_GenerateAlgorithmParams(ccHand, 
			keySize, outParams);
		if(crtn) {
			printError("CSSM_GenerateAlgorithmParams", crtn);
			CSSM_DeleteContext(ccHand);
			return crtn;
		}
	}
	
	/* optional format specifiers */
	if(pubFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
			sizeof(uint32),		
			CAT_Uint32,
			NULL,
			pubFormat);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	if(privFormat != CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		crtn = AddContextAttribute(ccHand,
			CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT,
			sizeof(uint32),
			CAT_Uint32,
			NULL,
			privFormat);
		if(crtn) {
			printError("AddContextAttribute(CSSM_ATTRIBUTE_PRIVATE_KEY_FORMAT)", crtn);
			return crtn;
		}
	}
	crtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubAttr,
		&keyLabelData,
		pubKey,
		privKeyUsage,
		privAttr,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
		ocrtn = crtn;
	}
	if(ccHand != 0) {
		crtn = CSSM_DeleteContext(ccHand);
		if(crtn) {
			printError("CSSM_DeleteContext", crtn);
			ocrtn = CSSM_ERRCODE_INTERNAL_ERROR;
		}
	}
	return ocrtn;
}

/*
 * Given keyFileBase, obtain name of public or private name. Output names
 * mallocd by caller.
 */
#define KEY_FILE_NAME_MAX_LEN	256

static void rtKeyFileName(
	const char 	*keyFileBase,
	CSSM_BOOL 	isPub,
	char		*outFileName)
{
	if(isPub) {
		sprintf(outFileName, "%s_pub.der", keyFileBase);
	}
	else {
		sprintf(outFileName, "%s_priv.der", keyFileBase);
	}
}

/*
 * Given keyFileBase and key type, init a CSSM_KEY.
 */
static int rt_readKey(
	CSSM_CSP_HANDLE		cspHand,
	const char 			*keyFileBase,
	CSSM_BOOL			isPub,
	CSSM_ALGORITHMS		alg,
	CSSM_KEYBLOB_FORMAT	format,	// FORMAT_NONE ==> default
	CSSM_KEY_PTR		key)
{
	char 				fileName[KEY_FILE_NAME_MAX_LEN];
	int 				irtn;
	CSSM_DATA_PTR		keyData = &key->KeyData;
	CSSM_KEYHEADER_PTR	hdr = &key->KeyHeader;
	CSSM_RETURN			crtn;
	CSSM_KEY_SIZE 		keySize;
	unsigned			len;
	
	memset(key, 0, sizeof(CSSM_KEY));
	rtKeyFileName(keyFileBase, isPub, fileName);
	irtn = readFile(fileName, &keyData->Data, &len);
	if(irtn) {
		printf("***error %d reading key file %s\n", irtn, fileName);
		return irtn;
	}
	keyData->Length = len;
	hdr->HeaderVersion = CSSM_KEYHEADER_VERSION;
	hdr->BlobType = CSSM_KEYBLOB_RAW;
	hdr->Format = format;
	hdr->AlgorithmId = alg;
	hdr->KeyClass = isPub ? CSSM_KEYCLASS_PUBLIC_KEY : 
		CSSM_KEYCLASS_PRIVATE_KEY;
	hdr->KeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	hdr->KeyUsage = CSSM_KEYUSE_ANY;
	
	/* ask the CSP for key size */
	crtn = CSSM_QueryKeySizeInBits(cspHand, 0, key, &keySize);
	if(crtn) {
		printError("CSSM_QueryKeySizeInBits", crtn);
		return 1;
	}
	hdr->LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;
	return 0;
}

static int rt_generate(opParams *op)
{
	CSSM_RETURN 	crtn;
	CSSM_KEY		pubKey;
	CSSM_KEY		privKey;
	char			fileName[KEY_FILE_NAME_MAX_LEN];
	int				irtn;
	CSSM_DATA		paramIn = {0, NULL};
	CSSM_DATA		paramOut = {0, NULL};
	CSSM_DATA_PTR	paramInPtr = NULL;
	CSSM_DATA_PTR	paramOutPtr = NULL;
	
	if(op->keyFileName == NULL) {
		printf("***Need a keyFileName to generate key pair.\n");
		return 1;
	}
	memset(&pubKey, 0, sizeof(CSSM_KEY));
	memset(&privKey, 0, sizeof(CSSM_KEY));
	
	if(op->alg == CSSM_ALGID_DSA) {
		/* must specify either inParams or outParams, not both */
		if(op->dsaParamFileIn && op->dsaParamFileOut) {
			printf("***DSA key generation requires one parameter file spec.\n");
			return 1;
		}
		if(!op->dsaParamFileIn && !op->dsaParamFileOut) {
			printf("***DSA key generation requires one parameter file spec.\n");
			return 1;
		}
		if(op->dsaParamFileIn) {
			/* caller-specified params */
			unsigned len;
			irtn = readFile(op->dsaParamFileIn, &paramIn.Data, &len);
			if(irtn) {
				printf("***Error reading DSA params from %s. Aborting.\n",
					op->dsaParamFileIn);
			}
			paramIn.Length = len;
			paramInPtr = &paramIn;
		}
		else {
			/* generate params --> paramOut */
			paramOutPtr = &paramOut;
		}
		crtn = genDsaKeyPair(op->cspHand,
			USAGE_NAME,
			USAGE_NAME_LEN,
			op->keySizeInBits,
			&pubKey,
			CSSM_FALSE,						// not ref
			CSSM_KEYUSE_VERIFY,				// not really important
			op->pubKeyFormat,
			&privKey,
			CSSM_FALSE,						// not ref
			CSSM_KEYUSE_SIGN,
			op->privKeyFormat,
			paramInPtr,
			paramOutPtr);
		if(crtn) {
			return 1;
		}
		if(paramOutPtr) {
			irtn = writeFile(op->dsaParamFileOut, paramOut.Data, paramOut.Length);
			if(irtn) {
				printf("***Error writing DSA params to %s. Aborting.\n",
					op->dsaParamFileOut);
				return 1;
			}
			if(!op->quiet) {
				printf("...wrote %lu bytes to %s\n", paramOut.Length, 
				op->dsaParamFileOut);
			}
			CSSM_FREE(paramOut.Data);
		}
		else {
			/* mallocd by readFile() */
			free(paramIn.Data);
		}
	}
	else {
		/* RSA, ECDSA */
		crtn = cspGenKeyPair(op->cspHand,
			op->alg,
			USAGE_NAME,
			USAGE_NAME_LEN,
			op->keySizeInBits,
			&pubKey,
			CSSM_FALSE,						// not ref
			CSSM_KEYUSE_VERIFY,				// not really important
			op->pubKeyFormat,
			&privKey,
			CSSM_FALSE,						// not ref
			CSSM_KEYUSE_SIGN,
			op->privKeyFormat,
			CSSM_FALSE);					// genSeed, not used here
		if(crtn) {
			return 1;
		}
	}
	
	/* write the blobs */
	rtKeyFileName(op->keyFileName, CSSM_TRUE, fileName);
	irtn = writeFile(fileName, pubKey.KeyData.Data, pubKey.KeyData.Length);
	if(irtn) {
		printf("***Error %d writing to %s\n", irtn, fileName);
		return irtn;
	}
	if(!op->quiet) {
		printf("...wrote %lu bytes to %s\n", pubKey.KeyData.Length, fileName);
	}
	rtKeyFileName(op->keyFileName, CSSM_FALSE, fileName);
	irtn = writeFile(fileName, privKey.KeyData.Data, privKey.KeyData.Length);
	if(irtn) {
		printf("***Error %d writing to %s\n", irtn, fileName);
		return irtn;
	}
	if(!op->quiet) {
		printf("...wrote %lu bytes to %s\n", privKey.KeyData.Length, fileName);
	}
	cspFreeKey(op->cspHand, &pubKey);
	cspFreeKey(op->cspHand, &privKey);
	return 0;
}

/* encrypt using public key */
static int rt_encrypt(opParams *op)
{
	CSSM_KEY 	pubKey;
	int 		irtn;
	CSSM_DATA	ptext;
	CSSM_DATA	ctext;
	CSSM_RETURN	crtn;
	CSSM_BOOL 	isPub;
	CSSM_ENCRYPT_MODE mode = CSSM_ALGMODE_NONE;
	CSSM_KEYBLOB_FORMAT format = op->pubKeyFormat;
    unsigned	len;
	
	if(op->keyFileName == NULL) {
		printf("***Need a keyFileName to encrypt.\n");
		return 1;
	}
	if((op->plainFileName == NULL) || (op->cipherFileName == NULL)) {
		printf("***Need plainFileName and cipherFileName to encrypt.\n");
		return 1;
	}
	if(op->swapKeyClass) {
		isPub = CSSM_FALSE;
		mode = CSSM_ALGMODE_PRIVATE_KEY;
        format = op->privKeyFormat;
	}
	else {
		isPub = CSSM_TRUE;
	}
	irtn = rt_readKey(op->cspHand, op->keyFileName, isPub, op->alg, 
		format, &pubKey);
	if(irtn) {
		return irtn;
	}
	irtn = readFile(op->plainFileName, &ptext.Data, &len);
	if(irtn) {
		printf("***Error reading %s\n", op->plainFileName);
		return irtn;
	}
	ptext.Length = len;
	ctext.Data = NULL;
	ctext.Length = 0;
	
	crtn = cspEncrypt(op->cspHand,
		op->alg,
		mode,
		op->noPad ? CSSM_PADDING_NONE : CSSM_PADDING_PKCS1,
		&pubKey,
		NULL,
		0,			// effectiveKeySize
		0,			// rounds
		NULL,		// initVector
		&ptext,
		&ctext,
		CSSM_FALSE);	// mallocCtext
	if(crtn) {
		printError("cspEncrypt", crtn);
		return 1;
	}
	irtn = writeFile(op->cipherFileName, ctext.Data, ctext.Length);
	if(irtn) {
		printf("***Error writing %s\n", op->cipherFileName);
	}
	else {
		if(!op->quiet) {
			printf("...wrote %lu bytes to %s\n", ctext.Length, op->cipherFileName);
		}
	}

	free(pubKey.KeyData.Data);				// allocd by rt_readKey --> readFile
	free(ptext.Data);						// allocd by readFile
	appFreeCssmData(&ctext, CSSM_FALSE);	// by CSP
	return irtn;
}

/* decrypt using private key */
static int rt_decrypt(opParams *op)
{
	CSSM_KEY 	privKey;
	int 		irtn;
	CSSM_DATA	ptext;
	CSSM_DATA	ctext;
	CSSM_RETURN	crtn;
	CSSM_BOOL 	isPub;
	CSSM_ENCRYPT_MODE mode = CSSM_ALGMODE_NONE;
	CSSM_KEYBLOB_FORMAT format = op->privKeyFormat;
	unsigned	len;
	
	if(op->keyFileName == NULL) {
		printf("***Need a keyFileName to decrypt.\n");
		return 1;
	}
	if((op->plainFileName == NULL) || (op->cipherFileName == NULL)) {
		printf("***Need plainFileName and cipherFileName to decrypt.\n");
		return 1;
	}
	if(op->swapKeyClass) {
		isPub = CSSM_TRUE;
		mode = CSSM_ALGMODE_PUBLIC_KEY;
        format = op->pubKeyFormat;
	}
	else {
		isPub = CSSM_FALSE;
	}
	irtn = rt_readKey(op->cspHand, op->keyFileName, isPub, op->alg, 
		format, &privKey);
	if(irtn) {
		return irtn;
	}
	irtn = readFile(op->cipherFileName, &ctext.Data, &len);
	if(irtn) {
		printf("***Error reading %s\n", op->cipherFileName);
		return irtn;
	}
	ctext.Length = len;
	ptext.Data = NULL;
	ptext.Length = 0;
	
	crtn = cspDecrypt(op->cspHand,
		op->alg,
		mode,
		op->noPad ? CSSM_PADDING_NONE : CSSM_PADDING_PKCS1,
		&privKey,
		NULL,
		0,			// effectiveKeySize
		0,			// rounds
		NULL,		// initVector
		&ctext,
		&ptext,
		CSSM_FALSE);	// mallocCtext
	if(crtn) {
		return 1;
	}
	irtn = writeFile(op->plainFileName, ptext.Data, ptext.Length);
	if(irtn) {
		printf("***Error writing %s\n", op->cipherFileName);
	}
	else {
		if(!op->quiet) {
			printf("...wrote %lu bytes to %s\n", ptext.Length, op->plainFileName);
		}
	}
	free(privKey.KeyData.Data);				// allocd by rt_readKey --> readFile
	free(ctext.Data);						// allocd by readFile
	appFreeCssmData(&ptext, CSSM_FALSE);	// by CSP
	return irtn;
}

static int rt_sign(opParams *op)
{
	CSSM_KEY 	privKey;
	int 		irtn;
	CSSM_DATA	ptext;
	CSSM_DATA	sig;
	CSSM_RETURN	crtn;
	CSSM_ALGORITHMS alg;
	unsigned len;
	
	if(op->keyFileName == NULL) {
		printf("***Need a keyFileName to sign.\n");
		return 1;
	}
	if((op->plainFileName == NULL) || (op->sigFileName == NULL)) {
		printf("***Need plainFileName and sigFileName to sign.\n");
		return 1;
	}
	irtn = rt_readKey(op->cspHand, op->keyFileName, CSSM_FALSE, op->alg, 
		op->privKeyFormat, &privKey);
	if(irtn) {
		return irtn;
	}
	irtn = readFile(op->plainFileName, &ptext.Data, &len);
	if(irtn) {
		printf("***Error reading %s\n", op->plainFileName);
		return irtn;
	}
	ptext.Length = len;
	sig.Data = NULL;
	sig.Length = 0;
	switch(op->alg) {
		case CSSM_ALGID_RSA:
			if(op->rawSign) {
				alg = CSSM_ALGID_RSA;
			}
			else {
				alg = CSSM_ALGID_SHA1WithRSA;
			}
			break;
		case CSSM_ALGID_DSA:
			alg = CSSM_ALGID_SHA1WithDSA;
			break;
		case CSSM_ALGID_ECDSA:
			if(op->rawSign) {
				alg = CSSM_ALGID_ECDSA;
			}
			else {
				alg = CSSM_ALGID_SHA1WithECDSA;
			}
			break;
		default:
			printf("Hey! Try another alg!\n");
			exit(1);
	}
	crtn = sigSign(op->cspHand,
		alg,
		&privKey,
		&ptext,
		&sig,
		op->digestAlg,
		op->noPad);
	if(crtn) {
		printError("cspSign", crtn);
		return 1;
	}
	irtn = writeFile(op->sigFileName, sig.Data, sig.Length);
	if(irtn) {
		printf("***Error writing %s\n", op->sigFileName);
	}
	else if(!op->quiet) {
		printf("...wrote %lu bytes to %s\n", sig.Length, op->sigFileName);
	}
	free(privKey.KeyData.Data);				// allocd by rt_readKey --> readFile
	free(ptext.Data);						// allocd by readFile
	appFreeCssmData(&sig, CSSM_FALSE);		// by CSP
	return irtn;
}

static int rt_verify(opParams *op)
{
	CSSM_KEY 	pubKey;
	int 		irtn;
	CSSM_DATA	ptext;
	CSSM_DATA	sig;
	CSSM_RETURN	crtn;
	CSSM_ALGORITHMS alg;
	unsigned	len;
	
	if(op->keyFileName == NULL) {
		printf("***Need a keyFileName to verify.\n");
		return 1;
	}
	if((op->plainFileName == NULL) || (op->sigFileName == NULL)) {
		printf("***Need plainFileName and sigFileName to verify.\n");
		return 1;
	}
	irtn = rt_readKey(op->cspHand, op->keyFileName, CSSM_TRUE, op->alg, 
		op->pubKeyFormat, &pubKey);
	if(irtn) {
		return irtn;
	}
	irtn = readFile(op->plainFileName, &ptext.Data, &len);
	if(irtn) {
		printf("***Error reading %s\n", op->plainFileName);
		return irtn;
	}
	ptext.Length = len;
	irtn = readFile(op->sigFileName, &sig.Data, (unsigned *)&sig.Length);
	if(irtn) {
		printf("***Error reading %s\n", op->sigFileName);
		return irtn;
	}
	switch(op->alg) {
		case CSSM_ALGID_RSA:
			if(op->rawSign) {
				alg = CSSM_ALGID_RSA;
			}
			else {
				alg = CSSM_ALGID_SHA1WithRSA;
			}
			break;
		case CSSM_ALGID_DSA:
			alg = CSSM_ALGID_SHA1WithDSA;
			break;
		case CSSM_ALGID_ECDSA:
			if(op->rawSign) {
				alg = CSSM_ALGID_ECDSA;
			}
			else {
				alg = CSSM_ALGID_SHA1WithECDSA;
			}
			break;
		default:
			printf("Hey! Try another alg!\n");
			exit(1);
	}
	crtn = sigVerify(op->cspHand,
		alg,
		&pubKey,
		&ptext,
		&sig,
		op->digestAlg,
		op->noPad);
	if(crtn) {
		printError("sigVerify", crtn);
		irtn = 1;
	}
	else if(!op->quiet){
		printf("...signature verifies OK\n");
		irtn = 0;
	}
	free(pubKey.KeyData.Data);				// allocd by rt_readKey --> readFile
	free(ptext.Data);						// allocd by readFile
	free(sig.Data);							// ditto
	return irtn;
}

static int rt_digest(opParams *op)
{
	int 		irtn;
	CSSM_DATA	ptext;
	CSSM_DATA	digest = {0, NULL};
	CSSM_RETURN	crtn;
	unsigned	len;
	
	if((op->plainFileName == NULL) || (op->sigFileName == NULL)) {
		printf("***Need plainFileName and sigFileName to digest.\n");
		return 1;
	}
	irtn = readFile(op->plainFileName, &ptext.Data, &len);
	if(irtn) {
		printf("***Error reading %s\n", op->plainFileName);
		return irtn;
	}
	ptext.Length = len;
	crtn = cspDigest(op->cspHand,
		op->alg,
		CSSM_FALSE,		// mallocDigest - let CSP do it
		&ptext,
		&digest);
	if(crtn) {
		printError("cspDigest", crtn);
		return 1;
	}
	irtn = writeFile(op->sigFileName, digest.Data, digest.Length);
	if(irtn) {
		printf("***Error writing %s\n", op->sigFileName);
	}
	else if(!op->quiet){
		printf("...wrote %lu bytes to %s\n", digest.Length, op->sigFileName);
	}
	free(ptext.Data);						// allocd by readFile
	appFreeCssmData(&digest, CSSM_FALSE);	// by CSP
	return irtn;
}

static int rt_convertPubKey(opParams *op)
{
	CSSM_RETURN crtn;
	int irtn;
	CSSM_KEY pubKeyIn;
	CSSM_KEY pubKeyOut;
	CSSM_KEY refKey;
	char fileName[KEY_FILE_NAME_MAX_LEN];

	if((op->keyFileName == NULL) || (op->outKeyFileName == NULL)) {
		printf("***I need input and output key file names for public key concersion.\n");
		return 1;
	}
	irtn = rt_readKey(op->cspHand, op->keyFileName, CSSM_TRUE, op->alg, 
		op->pubKeyFormat, &pubKeyIn);
	if(irtn) {
		return irtn;
	}
	crtn = cspRawKeyToRef(op->cspHand, &pubKeyIn, &refKey);
	if(crtn) {
		printf("***Error on NULL unwrap of %s\n", op->keyFileName);
		return -1;
	}
	crtn = nullWrapKey(op->cspHand, &refKey, op->outPubKeyFormat, &pubKeyOut);
	if(crtn) {
		printf("***Error on NULL wrap\n");
		return 1;
	}
		
	/* write the blobs */
	rtKeyFileName(op->outKeyFileName, CSSM_TRUE, fileName);
	irtn = writeFile(fileName, pubKeyOut.KeyData.Data, pubKeyOut.KeyData.Length);
	if(irtn) {
		printf("***Error %d writing to %s\n", irtn, fileName);
		return irtn;
	}
	if(!op->quiet) {
		printf("...wrote %lu bytes to %s\n", pubKeyOut.KeyData.Length, fileName);
	}
	cspFreeKey(op->cspHand, &pubKeyOut);
	free(pubKeyIn.KeyData.Data);
	cspFreeKey(op->cspHand, &refKey);
	return 0;
}

/* parse public key format character */
static CSSM_KEYBLOB_FORMAT parsePubKeyFormat(char c, char **argv)
{
	switch(c) {
		case '1':
			return CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
		case 'x':
			return CSSM_KEYBLOB_RAW_FORMAT_X509;
		case 'b':
			return CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
		case 'o':
			return CSSM_KEYBLOB_RAW_FORMAT_OPENSSH;
		case 'O':
			return CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2;
		case 'L':
			/* This is the "parse a private+public key as public only" option */
			return CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;
		default:
			usage(argv);
	}
	/* not reached */
	return -1;
}

int main(int argc, char **argv)
{
	int					arg;
	char				*argp;
	int					rtn;
	opParams			op;
	
	if(argc < 2) {
		usage(argv);
	}
	memset(&op, 0, sizeof(opParams));
	op.keySizeInBits = DEFAULT_KEY_SIZE_BITS;
	op.alg = CSSM_ALGID_RSA;
	op.swapKeyClass = CSSM_FALSE;
	op.rawSign = CSSM_FALSE;
	op.noPad = CSSM_FALSE;
	
	for(arg=2; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'a':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 'r':
						op.alg = CSSM_ALGID_RSA;
						break;
					case 'd':
						op.alg = CSSM_ALGID_DSA;
						break;
					case 'e':
						op.alg = CSSM_ALGID_ECDSA;
						break;
					default:
						usage(argv);
				}
				break;
			case 'z':
				op.keySizeInBits = atoi(&argp[2]);
				break;
			case 'k':
				op.keyFileName = &argp[2];
				break;
			case 'K':
				op.outKeyFileName = &argp[2];
				break;
			case 'p':
				op.plainFileName = &argp[2];
				break;
			case 'c':
				op.cipherFileName = &argp[2];
				break;
			case 's':
				op.sigFileName = &argp[2];
				break;
			case 'w':
				op.swapKeyClass = CSSM_TRUE;
				break;
			case 'r':
				op.rawSign = CSSM_TRUE;
				break;
			case 'P':
				op.noPad = CSSM_TRUE;
				break;
			case 'm':
				op.dsaParamFileIn = &argp[2];
				break;
			case 'M':
				op.dsaParamFileOut = &argp[2];
				break;
			case 'q':
				op.quiet = CSSM_TRUE;
				break;
			case 'b':
				if(argp[1] != '=') {
					usage(argv);
				}
				op.pubKeyFormat = parsePubKeyFormat(argp[2], argv);
				break;
			case 'B':
				if(argp[1] != '=') {
					usage(argv);
				}
				op.outPubKeyFormat = parsePubKeyFormat(argp[2], argv);
				break;
			case 'v':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case '1':
						op.privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
						break;
					case '8':
						op.privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
						break;
					case 's':
						op.privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSH;
						break;
					case 'b':
						op.pubKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
						break;
					#if OPENSSL_ENABLE
					case 'o':
						op.privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;
						break;
					#endif
					default:
						usage(argv);
				}
				break;
			case 'd':
				if(argp[1] != '=') {
					usage(argv);
				}
				switch(argp[2]) {
					case 's':
						op.digestAlg = CSSM_ALGID_SHA1;
						break;
					case '5':
						op.digestAlg = CSSM_ALGID_MD5;
						break;
					default:
						usage(argv);
				}
				break;
			case 'h':
			default:
				usage(argv);
		}
	}
	op.cspHand = cspDlDbStartup(CSSM_TRUE, NULL);
	if(op.cspHand == 0) {
		exit(1);
	}
	
	/* specify blob formats if user didn't */
	if(op.pubKeyFormat == CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		switch(op.alg) {
			case CSSM_ALGID_RSA:
				op.pubKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
				break;
			case CSSM_ALGID_DSA:
			case CSSM_ALGID_ECDSA:
				op.pubKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_X509;
				break;
			default:
				printf("BRRZAP!\n");
				exit(1);
		}
	}
	if(op.privKeyFormat == CSSM_KEYBLOB_RAW_FORMAT_NONE) {
		switch(op.alg) {
			case CSSM_ALGID_RSA:
				op.privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
				break;
			case CSSM_ALGID_DSA:
				op.privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
				break;
			case CSSM_ALGID_ECDSA:
				op.privKeyFormat = CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;
				break;
			default:
				printf("BRRZAP!\n");
				exit(1);
		}
	}
	switch(argv[1][0]) {
		case 'g':
			rtn = rt_generate(&op);
			break;
		case 'e':
			rtn = rt_encrypt(&op);
			break;
		case 'd':
			rtn = rt_decrypt(&op);
			break;
		case 's':
			rtn = rt_sign(&op);
			break;
		case 'v':
			rtn = rt_verify(&op);
			break;
		case 'S':
			op.alg = CSSM_ALGID_SHA1;
			rtn = rt_digest(&op);
			break;
		case 'M':
			op.alg = CSSM_ALGID_MD5;
			rtn = rt_digest(&op);
			break;
		case 'C':
			rtn = rt_convertPubKey(&op);
			break;
		default:
			usage(argv);
			exit(1);		// fool the compiler
	}
	CSSM_ModuleDetach(op.cspHand);
	return rtn;
}
