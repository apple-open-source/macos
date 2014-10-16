/*
 * keyDate.cpp - test handling of KeyHeader.{StartDate,EndDate}
 */
#include <Security/Security.h>
#include <security_cdsa_utilities/cssmdates.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "cspwrap.h"
#include "common.h"
#include <CoreFoundation/CoreFoundation.h>

/*
 * Enumerate algs our own way to allow iteration.
 */
typedef unsigned privAlg;
enum {
	ALG_ASC = 1,
	ALG_DES,
	ALG_AES,
	ALG_BFISH,
	ALG_RSA,
};

#define SYM_FIRST		ALG_ASC
#define SYM_LAST		ALG_BFISH
#define ASYM_FIRST		ALG_RSA
#define ASYM_LAST		ALG_RSA		

#define KD_DB_NAME		"keyDate.db"
#define KD_KEY_LABEL	"keyStoreKey"

static CSSM_DATA keyLabelData = {12, (uint8 *)KD_KEY_LABEL};

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("   Options:\n");
	printf("   s(ymmetric only)\n");
	printf("   a(symmetric only)\n");
	printf("   t (key store only)\n");
	printf("   D (CSPDL; default is bare CSP)\n");
	printf("   q(uiet)\n");
	printf("   h(elp)\n");
	exit(1);
}

#pragma mark -
#pragma mark --- Utilities ---

/*
 * Set a CSSM_DATE to "today plus delta days". Delta can be positive
 * or negative.
 */
static void setDate(
	CSSM_DATE &cdate,
	int deltaDays)
{
	CFAbsoluteTime cfTime = CFAbsoluteTimeGetCurrent();
	float fdelta = 60.0 * 60.0 * 24.0 * deltaDays;
	cfTime += fdelta;
	CFDateRef cfDate = CFDateCreate(NULL, cfTime);
	CssmUniformDate cud(cfDate);
	CFRelease(cfDate);
	cdate = cud;
}

/*
 * Compare two CSSM_DATEs. Returns nonzero on error. 
 */
static int compareDates(
	const CSSM_DATE *refDate,		// what we tried to set, or NULL
	const CSSM_DATE *keyDate,
	const char *op,
	CSSM_BOOL quiet)
{
	if(refDate == NULL) {
		/* make sure key date is empty */
		bool isZero = true;
		unsigned char *cp = (unsigned char *)keyDate;
		for(unsigned i=0; i<sizeof(CSSM_DATE); i++) {
			if(*cp++ != 0) {
				isZero = false;
				break;
			}
		}
		if(!isZero) {
			printf("%s: refDate NULL, non-empty keyDate\n", op);
			return testError(quiet);
		}
		else {
			return 0;
		}
	}
	if(memcmp(refDate, keyDate, sizeof(CSSM_DATE))) {
		printf("%s: refDate/keyDate MISCOMPARE\n", op);
		return testError(quiet);
	}
	else {
		return 0;
	}
}

#pragma mark -
#pragma mark -- Key generation ---

/*
 * symmetric key generator with startDate/endDate
 */
static int genSymKey(
	CSSM_CSP_HANDLE 	cspHand,
	CSSM_KEY_PTR		symKey,
	uint32 				alg,
	const char			*keyAlgStr,
	uint32 				keySizeInBits,
	CSSM_KEYATTR_FLAGS	keyAttr,
	CSSM_KEYUSE			keyUsage,
	CSSM_BOOL			quiet,
	bool 				setStartDate,
	int					startDeltaDays,
	bool				setEndDate,
	int					endDeltaDays,
	CSSM_DL_DB_HANDLE	*dlDbHand = NULL)		// optional
{
	CSSM_RETURN			crtn;
	CSSM_CC_HANDLE 		ccHand;
	CSSM_DATE			startDate;
	CSSM_DATE			endDate;
	
	if(setStartDate) {
		setDate(startDate, startDeltaDays);
	}
	if(setEndDate) {
		setDate(endDate, endDeltaDays);
	}
	
	memset(symKey, 0, sizeof(CSSM_KEY));
	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		alg,
		keySizeInBits,	// keySizeInBits
		NULL,			// Seed
		NULL,			// Salt
		setStartDate ? &startDate : NULL,
		setEndDate ? &endDate : NULL,
		NULL,			// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		return testError(quiet);
	}
	if(dlDbHand) {
		/* add in DL/DB to context */
		crtn = cspAddDlDbToContext(ccHand, dlDbHand->DLHandle, 
			dlDbHand->DBHandle);
		if(crtn) {
			return testError(quiet);
		}
	}
	crtn = CSSM_GenerateKey(ccHand,
		keyUsage,
		keyAttr,
		&keyLabelData,
		NULL,			// ACL
		symKey);
	if(crtn) {
		printError("CSSM_GenerateKey", crtn);
		return testError(quiet);
	}
	CSSM_DeleteContext(ccHand);

	CSSM_KEYHEADER &hdr = symKey->KeyHeader;
	CSSM_DATE *cdp = NULL;
	if(setStartDate) {
		cdp = &startDate;
	}
	if(compareDates(cdp, &hdr.StartDate, keyAlgStr, quiet)) {
		return 1;
	}
	if(setEndDate) {
		cdp = &endDate;
	}
	else {
		cdp = NULL;
	}
	if(compareDates(cdp, &hdr.EndDate, keyAlgStr, quiet)) {
		return 1;
	}
	return 0;
}

/*
 * Common, flexible, error-tolerant key pair generator.
 */
static int genKeyPair(
	CSSM_CSP_HANDLE 	cspHand,
	uint32 				algorithm,
	const char			*keyAlgStr,
	uint32 				keySizeInBits,
	CSSM_KEY_PTR 		pubKey,			
	CSSM_KEYATTR_FLAGS 	pubKeyAttr,
	CSSM_KEYUSE 		pubKeyUsage,	
	CSSM_KEY_PTR 		privKey,		
	CSSM_KEYATTR_FLAGS 	privKeyAttr,
	CSSM_KEYUSE 		privKeyUsage,	
	CSSM_BOOL 			quiet,
	bool 				setStartDate,
	int					startDeltaDays,
	bool				setEndDate,
	int					endDeltaDays,
	CSSM_DL_DB_HANDLE	*dlDbHand = NULL)		// optional
{
	CSSM_RETURN			crtn;
	CSSM_CC_HANDLE 		ccHand;
	CSSM_DATE			startDate;
	CSSM_DATE			endDate;
	
	if(setStartDate) {
		setDate(startDate, startDeltaDays);
	}
	if(setEndDate) {
		setDate(endDate, endDeltaDays);
	}
	
	memset(pubKey, 0, sizeof(CSSM_KEY));
	memset(privKey, 0, sizeof(CSSM_KEY));

	crtn = CSSM_CSP_CreateKeyGenContext(cspHand,
		algorithm,
		keySizeInBits,
		NULL,					// Seed
		NULL,					// Salt
		setStartDate ? &startDate : NULL,
		setEndDate ? &endDate : NULL,
		NULL,					// Params
		&ccHand);
	if(crtn) {
		printError("CSSM_CSP_CreateKeyGenContext", crtn);
		return testError(quiet);
	}
	
	if(dlDbHand) {
		/* add in DL/DB to context */
		crtn = cspAddDlDbToContext(ccHand, dlDbHand->DLHandle, 
			dlDbHand->DBHandle);
		if(crtn) {
			return testError(quiet);
		}
	}
	
	crtn = CSSM_GenerateKeyPair(ccHand,
		pubKeyUsage,
		pubKeyAttr,
		&keyLabelData,
		pubKey,
		privKeyUsage,
		privKeyAttr,
		&keyLabelData,			// same labels
		NULL,					// CredAndAclEntry
		privKey);
	if(crtn) {
		printError("CSSM_GenerateKeyPair", crtn);
		return testError(quiet);
	}
	CSSM_DeleteContext(ccHand);
	CSSM_KEYHEADER &pubHdr  = pubKey->KeyHeader;
	CSSM_KEYHEADER &privHdr = privKey->KeyHeader;
	CSSM_DATE *cdp = NULL;
	if(setStartDate) {
		cdp = &startDate;
	}
	if(compareDates(cdp, &pubHdr.StartDate, keyAlgStr, quiet)) {
		return 1;
	}
	if(compareDates(cdp, &privHdr.StartDate, keyAlgStr, quiet)) {
		return 1;
	}
	if(setEndDate) {
		cdp = &endDate;
	}
	else {
		cdp = NULL;
	}
	if(compareDates(cdp, &pubHdr.EndDate, keyAlgStr, quiet)) {
		return 1;
	}
	if(compareDates(cdp, &privHdr.EndDate, keyAlgStr, quiet)) {
		return 1;
	}
	return 0;
}

/* map one of our private privAlgs (ALG_DES, etc.) to associated CSSM info. */
void privAlgToCssm(
	privAlg 		palg,
	CSSM_ALGORITHMS	*keyAlg,
	CSSM_ALGORITHMS *signAlg,	// CSSM_ALGID_NONE means incapable 
								//		(e.g., DES)
	CSSM_ALGORITHMS	*encrAlg,	// CSSM_ALGID_NONE means incapable
	CSSM_ENCRYPT_MODE *encrMode,
	CSSM_PADDING	*encrPad,
	uint32			*keySizeInBits,
	const char		**keyAlgStr)
{
	*signAlg = *encrAlg = CSSM_ALGID_NONE;	// default
	*encrMode = CSSM_ALGMODE_NONE;
	*encrPad = CSSM_PADDING_NONE;
	switch(palg) {
		case ALG_ASC:
			*encrAlg = *keyAlg = CSSM_ALGID_ASC;
			*keySizeInBits = CSP_ASC_KEY_SIZE_DEFAULT;
			*keyAlgStr = "ASC";
			break;
		case ALG_DES:
			*encrAlg = *keyAlg = CSSM_ALGID_DES;
			*keySizeInBits = CSP_DES_KEY_SIZE_DEFAULT;
			*keyAlgStr = "DES";
			*encrMode = CSSM_ALGMODE_CBCPadIV8;
			*encrPad = CSSM_PADDING_PKCS7;
			break;
		case ALG_AES:
			*encrAlg = *keyAlg = CSSM_ALGID_AES;
			*keySizeInBits = CSP_AES_KEY_SIZE_DEFAULT;
			*keyAlgStr = "AES";
			*encrMode = CSSM_ALGMODE_CBCPadIV8;
			*encrPad = CSSM_PADDING_PKCS7;
			break;
		case ALG_BFISH:
			*encrAlg = *keyAlg = CSSM_ALGID_BLOWFISH;
			*keySizeInBits = CSP_BFISH_KEY_SIZE_DEFAULT;
			*keyAlgStr = "Blowfish";
			*encrMode = CSSM_ALGMODE_CBCPadIV8;
			*encrPad = CSSM_PADDING_PKCS7;
			break;
		case ALG_RSA:
			*keyAlg = CSSM_ALGID_RSA;
			*encrAlg = CSSM_ALGID_RSA;
			*signAlg = CSSM_ALGID_SHA1WithRSA;
			*keySizeInBits = 512;
			*keyAlgStr = "RSA";
			*encrPad = CSSM_PADDING_PKCS1;
			break;
		default:
			printf("***BRRZAP! privAlgToCssm needs work\n");
			exit(1);
	}
	return;
}

#pragma mark -
#pragma mark --- basic ops to detect INVALID_KEY_{START,END}_DATE ---
 
#define PTEXT_SIZE	64
#define IV_SIZE		16

static int doEncrypt(
	CSSM_CSP_HANDLE	cspHand,
	const char *algStr,
	CSSM_KEY_PTR key,			// session, public
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	CSSM_PADDING encrPad,
	CSSM_RETURN expRtn,			// expected result
	CSSM_BOOL quiet)
{
	uint8 ptextData[PTEXT_SIZE];
	CSSM_DATA ptext = {PTEXT_SIZE, ptextData};
	uint8 someIvData[IV_SIZE];
	CSSM_DATA someIv = {IV_SIZE, someIvData};
	 
	simpleGenData(&ptext, PTEXT_SIZE, PTEXT_SIZE);
	simpleGenData(&someIv, IV_SIZE, IV_SIZE);
	
	CSSM_CC_HANDLE cryptHand = 0;
	CSSM_RETURN crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	
	if(key->KeyHeader.KeyClass == CSSM_KEYCLASS_SESSION_KEY) {
		crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			encrAlg,
			encrMode,
			NULL,			// access cred
			key,
			&someIv,
			encrPad,	
			NULL,			// Params
			&cryptHand);
		if(crtn) {
			printError("CSSM_CSP_CreateSymmetricContext", crtn);
			return testError(quiet);
		}
	}
	else if(key->KeyHeader.KeyClass == CSSM_KEYCLASS_PUBLIC_KEY) {
		crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
			encrAlg,
			&creds,			// access
			key,
			encrPad,
			&cryptHand);
		if(crtn) {
			printError("CSSM_CSP_CreateAsymmetricContext", crtn);
			return testError(quiet);
		}
	}
	else {
		printf("***BRRZAP! Only encrypt with session and public keys\n");
		exit(1);
	}

	CSSM_DATA ctext = {0, NULL};
	CSSM_DATA remData = {0, NULL};
	CSSM_SIZE bEncr;
	int irtn = 0;
	
	crtn = CSSM_EncryptData(cryptHand,
		&ptext,
		1,
		&ctext,
		1,
		&bEncr,
		&remData);
	if(crtn != expRtn) {
		if(expRtn == CSSM_OK) {
			printError("CSSM_EncryptData", crtn);
			printf("Unexpected error encrypting with %s\n", algStr);
		}
		else {
			printf("***Encrypt with %s: expected %s, got %s.\n",
				algStr, cssmErrToStr(expRtn),
				cssmErrToStr(crtn));
		}
		irtn = testError(quiet);
	}
	appFreeCssmData(&ctext, CSSM_FALSE);
	appFreeCssmData(&remData, CSSM_FALSE);
	CSSM_DeleteContext(cryptHand);
	return irtn;
}

/*
 * Decrypt bad cipher text. If the key is bad the CSP won't even get 
 * to the ciphertext. Bad ciphertext can result in a number of errors,
 * in some cases it can even result in complete success, which we handle
 * OK if the key is supposed to be good.
 */
 
typedef enum {
	DR_BadStartDate,		// must be CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE
	DR_BadEndDate,			// must be CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE
	DR_BadData				// CSSMERR_CSP_INVALID_DATA. etc.
} DecrResult;

#define CTEXT_SIZE  (PTEXT_SIZE )

static int doDecrypt(
	CSSM_CSP_HANDLE	cspHand,
	const char *algStr,
	CSSM_KEY_PTR key,			// session, private
	CSSM_ALGORITHMS encrAlg,
	CSSM_ENCRYPT_MODE encrMode,
	CSSM_PADDING encrPad,
	DecrResult expResult,
	CSSM_BOOL quiet)
{
	uint8 ctextData[CTEXT_SIZE];
	CSSM_DATA ctext = {CTEXT_SIZE, ctextData};
	uint8 someIvData[IV_SIZE];
	CSSM_DATA someIv = {IV_SIZE, someIvData};
	 
	 /*
	  * I have not found a way to guarantee decrypt failure here, no matter
	  * what ctext and IV I specify. We can't just do an encrypt and 
	  * munge because we might be testing a bad (expired) key. 
	  * We might have to redesign, first generating a good key, then an
	  * expired key from it...? Until then this test is loose about
	  * handling "key is good" detection.
	  */
	memset(ctextData, 0, CTEXT_SIZE);	// guaranteed bad padding
	memset(someIvData, 0, IV_SIZE);
	
	CSSM_CC_HANDLE cryptHand = 0;
	CSSM_RETURN crtn;
	CSSM_ACCESS_CREDENTIALS	creds;
	
	memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
	
	if(key->KeyHeader.KeyClass == CSSM_KEYCLASS_SESSION_KEY) {
		crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
			encrAlg,
			encrMode,
			NULL,			// access cred
			key,
			&someIv,
			encrPad,	
			NULL,			// Params
			&cryptHand);
		if(crtn) {
			printError("CSSM_CSP_CreateSymmetricContext", crtn);
			return testError(quiet);
		}
	}
	else if(key->KeyHeader.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY) {
		crtn = CSSM_CSP_CreateAsymmetricContext(cspHand,
			encrAlg,
			&creds,			// access
			key,
			encrPad,
			&cryptHand);
		if(crtn) {
			printError("CSSM_CSP_CreateAsymmetricContext", crtn);
			return testError(quiet);
		}
	}
	else {
		printf("***BRRZAP! Only decrypt with session and private"
			" keys\n");
		exit(1);
	}

	CSSM_DATA ptext = {0, NULL};
	CSSM_DATA remData = {0, NULL};
	CSSM_SIZE bDecr;
	int irtn = 0;
	
	crtn = CSSM_DecryptData(cryptHand,
		&ctext,
		1,
		&ptext,
		1,
		&bDecr,
		&remData);
	switch(expResult) {
		case DR_BadStartDate:
			if(crtn != CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE) {
				printf("***Decrypt with %s: expected INVALID_KEY_START_DATE, "
					"got %s.\n", algStr, cssmErrToStr(crtn));
				irtn = testError(quiet);
			}
			break;
		case DR_BadEndDate:
			if(crtn != CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE) {
				printf("***Decrypt with %s: expected INVALID_KEY_END_DATE, "
					"got %s.\n", algStr, cssmErrToStr(crtn));
				irtn = testError(quiet);
			}
			break;
		case DR_BadData:
			switch(crtn) {
				case CSSM_OK:						// good data, seen sometimes
				case CSSMERR_CSP_INVALID_DATA:		// common case
				case CSSMERR_CSP_INTERNAL_ERROR:	// default case in CSP's
													//   throwRsaDsa() :-(
					break;
				default:
					printf("***Decrypt with %s: expected INVALID_DATA or OK, "
						"got %s.\n",
						algStr, cssmErrToStr(crtn));
					irtn = testError(quiet);
					break;
			}
			break;
	}
	appFreeCssmData(&ptext, CSSM_FALSE);
	appFreeCssmData(&remData, CSSM_FALSE);
	CSSM_DeleteContext(cryptHand);
	return irtn;
}
	
static int doSign(
	CSSM_CSP_HANDLE	cspHand,
	const char *algStr,
	CSSM_KEY_PTR key,			// private
	CSSM_ALGORITHMS sigAlg,
	CSSM_RETURN expRtn,			// expected result
	CSSM_BOOL quiet)
{
	uint8 ptextData[PTEXT_SIZE];
	CSSM_DATA ptext = {PTEXT_SIZE, ptextData};
	CSSM_DATA sig = {0, NULL};
	
	simpleGenData(&ptext, PTEXT_SIZE, PTEXT_SIZE);
	
	CSSM_CC_HANDLE cryptHand = 0;
	CSSM_RETURN crtn;
	
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		sigAlg,
		NULL,				// passPhrase
		key,
		&cryptHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (1)", crtn);
		return testError(quiet);
	}
	int irtn = 0;
	crtn = CSSM_SignData(cryptHand,
		&ptext,
		1,
		CSSM_ALGID_NONE,
		&sig);
	if(crtn != expRtn) {
		if(expRtn == CSSM_OK) {
			printError("CSSM_SignData", crtn);
			printf("Unexpected error signing with %s\n", algStr);
		}
		else {
			printf("***Sign with %s: expected %s, got %s.\n",
				algStr, cssmErrToStr(expRtn),
				cssmErrToStr(crtn));
		}
		irtn = testError(quiet);
	}
	appFreeCssmData(&sig, CSSM_FALSE);
	CSSM_DeleteContext(cryptHand);
	return irtn;
}

/*
 * Verify bad signature. If the key is bad the CSP won't even get 
 * to the sig verify. Otherwise expect KD_VERIFY_FAIL_ERR.
 */
#define KD_VERIFY_FAIL_ERR		CSSMERR_CSP_VERIFY_FAILED

static int doVerify(
	CSSM_CSP_HANDLE	cspHand,
	const char *algStr,
	CSSM_KEY_PTR key,			// private
	CSSM_ALGORITHMS sigAlg,
	CSSM_RETURN expRtn,			// expected result
	CSSM_BOOL quiet)
{
	uint8 ptextData[PTEXT_SIZE];
	CSSM_DATA ptext = {PTEXT_SIZE, ptextData};
	uint8 sigData[PTEXT_SIZE];
	CSSM_DATA sig = {PTEXT_SIZE, sigData};
	
	simpleGenData(&ptext, PTEXT_SIZE, PTEXT_SIZE);
	memset(sigData, 0, PTEXT_SIZE);
	
	CSSM_CC_HANDLE cryptHand = 0;
	CSSM_RETURN crtn;
	
	crtn = CSSM_CSP_CreateSignatureContext(cspHand,
		sigAlg,
		NULL,				// passPhrase
		key,
		&cryptHand);
	if(crtn) {
		printError("CSSM_CSP_CreateSignatureContext (2)", crtn);
		return testError(quiet);
	}
	int irtn = 0;
	crtn = CSSM_VerifyData(cryptHand,
		&ptext,
		1,
		CSSM_ALGID_NONE,
		&sig);
	if(crtn != expRtn) {
		if(expRtn == CSSM_OK) {
			printError("CSSM_VerifyData", crtn);
			printf("Unexpected error verifying with %s\n", algStr);
		}
		else {
			printf("***Verify with %s: expected %s, got %s.\n",
				algStr, cssmErrToStr(expRtn),
				cssmErrToStr(crtn));
		}
		irtn = testError(quiet);
	}
	CSSM_DeleteContext(cryptHand);
	return irtn;
}


#pragma mark -
#pragma mark -- test suites ---

int doSymTests(
	CSSM_CSP_HANDLE cspHand, 
	privAlg palg,
	CSSM_BOOL refKeys,
	CSSM_BOOL quiet)
{
	CSSM_ALGORITHMS		keyAlg;
	CSSM_ALGORITHMS 	signAlg;
	CSSM_ALGORITHMS		encrAlg;
	CSSM_ENCRYPT_MODE	encrMode;
	CSSM_PADDING		encrPad;
	uint32				keySizeInBits;
	const char			*keyAlgStr;

	privAlgToCssm(palg, &keyAlg, &signAlg, &encrAlg, &encrMode, 
		&encrPad, &keySizeInBits, &keyAlgStr);

	CSSM_KEY symKey;
	int irtn;
	CSSM_KEYATTR_FLAGS keyAttr;
	if(refKeys) {
		keyAttr = CSSM_KEYATTR_RETURN_REF;
	}
	else {
		keyAttr = CSSM_KEYATTR_RETURN_DATA | CSSM_KEYATTR_EXTRACTABLE;
	}

	if(!quiet) {
		printf("...testing %s with %s keys\n", keyAlgStr,
			refKeys ? "Ref" : "Raw");
		printf("   ...verifying empty Dates\n");
	}
	irtn = genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, keySizeInBits,
		keyAttr, CSSM_KEYUSE_ANY, quiet,
		CSSM_FALSE, 0,		// no StartDate
		CSSM_FALSE, 0);		// no EndDate
	if(irtn) {
		return irtn;
	}
	irtn = doEncrypt(cspHand, keyAlgStr, &symKey, encrAlg, encrMode,
		encrPad, CSSM_OK, quiet);
	if(irtn) {
		printf("***Failure on encrypting with empty Key Dates\n");
		return irtn;
	}
	irtn = doDecrypt(cspHand, keyAlgStr, &symKey, encrAlg, encrMode,
		encrPad, DR_BadData, quiet);
	if(irtn) {
		printf("***Failure on decrypting with empty Key Dates\n");
		return irtn;
	}
	cspFreeKey(cspHand, &symKey);
	
	if(!quiet) {
		printf("   ...verifying Good Dates\n");
	}
	irtn = genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, keySizeInBits,
		keyAttr, CSSM_KEYUSE_ANY, quiet,
		CSSM_TRUE, 0,		// StartDate = today
		CSSM_TRUE, 1);		// EndDate = tomorrow
	if(irtn) {
		return irtn;
	}
	irtn = doEncrypt(cspHand, keyAlgStr, &symKey, encrAlg, encrMode,
		encrPad, CSSM_OK, quiet);
	if(irtn) {
		printf("***Failure on encrypting with good Key Dates\n");
		return irtn;
	}
	irtn = doDecrypt(cspHand, keyAlgStr, &symKey, encrAlg, encrMode,
		encrPad, DR_BadData, quiet);
	if(irtn) {
		printf("***Failure on decrypting with good Key Dates\n");
		return irtn;
	}
	cspFreeKey(cspHand, &symKey);
	
	if(!quiet) {
		printf("   ...verifying Bad StartDate\n");
	}
	irtn = genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, keySizeInBits,
		keyAttr, CSSM_KEYUSE_ANY, quiet,
		CSSM_TRUE, 1,		// StartDate = tomorrow
		CSSM_TRUE, 1);		// EndDate = tomorrow
	if(irtn) {
		return irtn;
	}
	irtn = doEncrypt(cspHand, keyAlgStr, &symKey, encrAlg, encrMode,
		encrPad, CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE, quiet);
	if(irtn) {
		printf("***Failure on encrypting with bad StartDate\n");
		return irtn;
	}
	irtn = doDecrypt(cspHand, keyAlgStr, &symKey, encrAlg, encrMode,
		encrPad, DR_BadStartDate, quiet);
	if(irtn) {
		printf("***Failure on decrypting with bad StartDate\n");
		return irtn;
	}
	cspFreeKey(cspHand, &symKey);

	if(!quiet) {
		printf("   ...verifying Bad EndDate\n");
	}
	irtn = genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, keySizeInBits,
		keyAttr, CSSM_KEYUSE_ANY, quiet,
		CSSM_TRUE, 0,		// StartDate = today
		CSSM_TRUE, -1);		// EndDate = yesterday
	if(irtn) {
		return irtn;
	}
	irtn = doEncrypt(cspHand, keyAlgStr, &symKey, encrAlg, encrMode,
		encrPad, CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE, quiet);
	if(irtn) {
		printf("***Failure on encrypting with bad StartDate\n");
		return irtn;
	}
	irtn = doDecrypt(cspHand, keyAlgStr, &symKey, encrAlg, encrMode,
		encrPad, DR_BadEndDate, quiet);
	if(irtn) {
		printf("***Failure on decrypting with bad EndDate\n");
		return irtn;
	}
	cspFreeKey(cspHand, &symKey);

	return 0;
}

int doAsymTests(
	CSSM_CSP_HANDLE cspHand, 
	privAlg palg,
	CSSM_BOOL refKeys,
	CSSM_BOOL quiet)
{
	CSSM_ALGORITHMS		keyAlg;
	CSSM_ALGORITHMS 	sigAlg;
	CSSM_ALGORITHMS		encrAlg;
	CSSM_ENCRYPT_MODE	encrMode;
	CSSM_PADDING		encrPad;
	uint32				keySizeInBits;
	const char			*keyAlgStr;

	privAlgToCssm(palg, &keyAlg, &sigAlg, &encrAlg, &encrMode, 
		&encrPad, &keySizeInBits, &keyAlgStr);

	CSSM_KEY pubKey;
	CSSM_KEY privKey;
	int irtn;
	CSSM_KEYATTR_FLAGS pubKeyAttr  = CSSM_KEYATTR_EXTRACTABLE;
	CSSM_KEYATTR_FLAGS privKeyAttr = CSSM_KEYATTR_EXTRACTABLE;
	if(refKeys) {
		pubKeyAttr  |= CSSM_KEYATTR_RETURN_REF;
		privKeyAttr |= CSSM_KEYATTR_RETURN_REF;
	}
	else {
		pubKeyAttr  |= CSSM_KEYATTR_RETURN_DATA;
		privKeyAttr |= CSSM_KEYATTR_RETURN_DATA;
	}

	if(!quiet) {
		printf("...testing %s with %s keys\n", keyAlgStr,
			refKeys ? "Ref" : "Raw");
		printf("   ...verifying empty Dates\n");
	}
	irtn = genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits,
		&pubKey,  pubKeyAttr, CSSM_KEYUSE_ANY,
		&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
		quiet,
		CSSM_FALSE, 0,		// no StartDate
		CSSM_FALSE, 0);		// no EndDate
	if(irtn) {
		return irtn;
	}
	irtn = doEncrypt(cspHand, keyAlgStr, &pubKey, encrAlg, encrMode,
		encrPad, CSSM_OK, quiet);
	if(irtn) {
		printf("***Failure on encrypting with empty Key Dates\n");
		return irtn;
	}
	irtn = doDecrypt(cspHand, keyAlgStr, &privKey, encrAlg, encrMode,
		encrPad, DR_BadData, quiet);
	if(irtn) {
		printf("***Failure on decrypting with empty Key Dates\n");
		return irtn;
	}
	irtn = doSign(cspHand, keyAlgStr, &privKey, sigAlg,
		CSSM_OK, quiet);
	if(irtn) {
		printf("***Failure on signing with empty Key Dates\n");
		return irtn;
	}
	irtn = doVerify(cspHand, keyAlgStr, &pubKey, sigAlg,
		KD_VERIFY_FAIL_ERR, quiet);
	if(irtn) {
		printf("***Failure on verifying with empty Key Dates\n");
		return irtn;
	}
	cspFreeKey(cspHand, &pubKey);
	cspFreeKey(cspHand, &privKey);
	
	if(!quiet) {
		printf("   ...verifying Good Dates\n");
	}
	irtn = genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits,
		&pubKey,  pubKeyAttr, CSSM_KEYUSE_ANY,
		&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
		quiet,
		CSSM_TRUE, 0,		// StartDate = today
		CSSM_TRUE, 1);		// EndDate = tomorrow
	if(irtn) {
		return irtn;
	}
	irtn = doEncrypt(cspHand, keyAlgStr, &pubKey, encrAlg, encrMode,
		encrPad, CSSM_OK, quiet);
	if(irtn) {
		printf("***Failure on encrypting with good Key Dates\n");
		return irtn;
	}
	irtn = doDecrypt(cspHand, keyAlgStr, &privKey, encrAlg, encrMode,
		encrPad, DR_BadData, quiet);
	if(irtn) {
		printf("***Failure on decrypting with Good Key Dates\n");
		return irtn;
	}
	irtn = doSign(cspHand, keyAlgStr, &privKey, sigAlg,
		CSSM_OK, quiet);
	if(irtn) {
		printf("***Failure on signing with Good Key Dates\n");
		return irtn;
	}
	irtn = doVerify(cspHand, keyAlgStr, &pubKey, sigAlg,
		KD_VERIFY_FAIL_ERR, quiet);
	if(irtn) {
		printf("***Failure on verifying with Good Key Dates\n");
		return irtn;
	}
	cspFreeKey(cspHand, &pubKey);
	cspFreeKey(cspHand, &privKey);
	
	if(!quiet) {
		printf("   ...verifying Bad StartDate\n");
	}
	irtn = genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits,
		&pubKey,  pubKeyAttr, CSSM_KEYUSE_ANY,
		&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
		quiet,
		CSSM_TRUE, 1,		// StartDate = tomorrow
		CSSM_TRUE, 1);		// EndDate = tomorrow
	if(irtn) {
		return irtn;
	}
	irtn = doEncrypt(cspHand, keyAlgStr, &pubKey, encrAlg, encrMode,
		encrPad, CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE, quiet);
	if(irtn) {
		printf("***Failure on encrypting with bad StartDate\n");
		return irtn;
	}
	irtn = doDecrypt(cspHand, keyAlgStr, &privKey, encrAlg, encrMode,
		encrPad, DR_BadStartDate, quiet);
	if(irtn) {
		printf("***Failure on decrypting with bad StartDate\n");
		return irtn;
	}
	irtn = doSign(cspHand, keyAlgStr, &privKey, sigAlg,
		CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE, quiet);
	if(irtn) {
		printf("***Failure on signing with bad StartDate\n");
		return irtn;
	}
	irtn = doVerify(cspHand, keyAlgStr, &pubKey, sigAlg,
		CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE, quiet);
	if(irtn) {
		printf("***Failure on verifying with bad StartDate\n");
		return irtn;
	}
	cspFreeKey(cspHand, &pubKey);
	cspFreeKey(cspHand, &privKey);

	if(!quiet) {
		printf("   ...verifying Bad EndDate\n");
	}
	irtn = genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits,
		&pubKey,  pubKeyAttr, CSSM_KEYUSE_ANY,
		&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
		quiet,
		CSSM_TRUE, 0,		// StartDate = today
		CSSM_TRUE, -1);		// EndDate = yesterday
	if(irtn) {
		return irtn;
	}
	irtn = doEncrypt(cspHand, keyAlgStr, &pubKey, encrAlg, encrMode,
		encrPad, CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE, quiet);
	if(irtn) {
		printf("***Failure on encrypting with bad EndDate\n");
		return irtn;
	}
	irtn = doDecrypt(cspHand, keyAlgStr, &privKey, encrAlg, encrMode,
		encrPad, DR_BadEndDate, quiet);
	if(irtn) {
		printf("***Failure on decrypting with bad EndDate\n");
		return irtn;
	}
	irtn = doSign(cspHand, keyAlgStr, &privKey, sigAlg,
		CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE, quiet);
	if(irtn) {
		printf("***Failure on signing with bad EndDate\n");
		return irtn;
	}
	irtn = doVerify(cspHand, keyAlgStr, &pubKey, sigAlg,
		CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE, quiet);
	if(irtn) {
		printf("***Failure on verifying with bad EndDate\n");
		return irtn;
	}
	cspFreeKey(cspHand, &pubKey);
	cspFreeKey(cspHand, &privKey);

	return 0;
}

/* 
 * fetch stored key from DB, ensure it has same start/end date 
 */
static int fetchStoredKey(
	CSSM_DL_DB_HANDLE	dlDbHand,
	CT_KeyType 			lookupType,
	CSSM_KEY_PTR		compareKey,
	const char 			*op,
	CSSM_BOOL			quiet,
	CSSM_KEY_PTR		*lookupKey)		// RETURNED
{
	CSSM_KEY_PTR lookup = cspLookUpKeyByLabel(dlDbHand.DLHandle,
		dlDbHand.DBHandle,
		&keyLabelData,
		lookupType);
	if(lookup == NULL) {
		printf("%s: Error looking up key in DB\n", op);
		return testError(quiet);
	}
	if(compareDates(&compareKey->KeyHeader.StartDate,
		&lookup->KeyHeader.StartDate,
		op, quiet)) {
			return 1;
	}
	*lookupKey = lookup;
	return 0;
}

int doStoreTests(
	CSSM_CSP_HANDLE cspHand, 		// must be CSPDL
	CSSM_DL_DB_HANDLE dlDbHand,
	privAlg palg,
	CSSM_BOOL isAsym,
	CSSM_BOOL quiet)
{	
	CSSM_ALGORITHMS		keyAlg;
	CSSM_ALGORITHMS 	signAlg;
	CSSM_ALGORITHMS		encrAlg;
	CSSM_ENCRYPT_MODE	encrMode;
	CSSM_PADDING		encrPad;
	uint32				keySizeInBits;
	const char			*keyAlgStr;

	privAlgToCssm(palg, &keyAlg, &signAlg, &encrAlg, &encrMode, 
		&encrPad, &keySizeInBits, &keyAlgStr);

	CSSM_KEY symKey;
	CSSM_KEY privKey;
	CSSM_KEY pubKey;
	int irtn;
	CSSM_KEY_PTR lookupKey = NULL;		// obtained from DB
	CSSM_KEY_PTR compareKey;			// &symKey or &pubKey
	CT_KeyType lookupType;
	CSSM_KEYATTR_FLAGS pubKeyAttr  = 
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE |
		CSSM_KEYATTR_PERMANENT;
	CSSM_KEYATTR_FLAGS privKeyAttr = 
		CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT;
		
	if(!quiet) {
		printf("...testing %s key storage\n", keyAlgStr);
		printf("   ...verifying empty Dates\n");
	}
	if(isAsym) {
		lookupType = CKT_Public;
		compareKey = &pubKey;
		irtn = genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits,
			&pubKey,  pubKeyAttr, CSSM_KEYUSE_ANY,
			&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
			quiet,
			CSSM_FALSE, 0,		// no StartDate
			CSSM_FALSE, 0,		// no EndDate
			&dlDbHand);
	}
	else {
		lookupType = CKT_Session;
		compareKey = &symKey;
		irtn = genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, 
			keySizeInBits,
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
			CSSM_KEYUSE_ANY, quiet,
			CSSM_FALSE, 0,		// no StartDate
			CSSM_FALSE, 0,		// no EndDate
			&dlDbHand);
	}
	if(irtn) {
		return irtn;
	}
	
	/* 
	 * fetch stored key from DB, ensure it has same start/end date 
	 */
	if(fetchStoredKey(dlDbHand, lookupType,
			compareKey, "Store key with empty Dates", quiet, 
			&lookupKey)) {
		return 1;
	}
	
	/* quickie test, use it for encrypt */
	irtn = doEncrypt(cspHand, keyAlgStr, lookupKey, encrAlg, encrMode,
		encrPad, CSSM_OK, quiet);
	if(irtn) {
		printf("***Failure on encrypt, lookup with empty Key Dates\n");
		return irtn;
	}
	
	/* free and delete everything */
	if(isAsym) {
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &pubKey);
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &privKey);
	}
	else {
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &symKey);
	}
	cspFreeKey(cspHand, lookupKey);
	
	/*********************/
	
	if(!quiet) {
		printf("   ...verifying Good Dates\n");
	}
	if(isAsym) {
		lookupType = CKT_Public;
		compareKey = &pubKey;
		irtn = genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits,
			&pubKey,  pubKeyAttr, CSSM_KEYUSE_ANY,
			&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
			quiet,
			CSSM_TRUE, 0,		// StartDate = today
			CSSM_TRUE, 1,		// EndDate = tomorrow
			&dlDbHand);
	}
	else {
		lookupType = CKT_Session;
		compareKey = &symKey;
		irtn = genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, 
			keySizeInBits,
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
			CSSM_KEYUSE_ANY, quiet,
			CSSM_TRUE, 0,		// StartDate = today
			CSSM_TRUE, 1,		// EndDate = tomorrow
			&dlDbHand);
	}
	if(irtn) {
		return irtn;
	}
	
	/* 
	 * fetch stored key from DB, ensure it has same start/end date 
	 */
	if(fetchStoredKey(dlDbHand, lookupType,
			compareKey, "Store key with Good Dates", quiet, 
			&lookupKey)) {
		return 1;
	}
	
	/* quickie test, use it for encrypt */
	irtn = doEncrypt(cspHand, keyAlgStr, lookupKey, encrAlg, encrMode,
		encrPad, CSSM_OK, quiet);
	if(irtn) {
		printf("***Failure on encrypt, lookup with Good Key Dates\n");
		return irtn;
	}
	
	/* free and delete everything */
	if(isAsym) {
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &pubKey);
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &privKey);
	}
	else {
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &symKey);
	}
	cspFreeKey(cspHand, lookupKey);

	/*********************/

	if(!quiet) {
		printf("   ...verifying Bad StartDate\n");
	}
	if(isAsym) {
		lookupType = CKT_Public;
		compareKey = &pubKey;
		irtn = genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits,
			&pubKey,  pubKeyAttr, CSSM_KEYUSE_ANY,
			&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
			quiet,
			CSSM_TRUE, 1,		// StartDate = tomorrow
			CSSM_TRUE, 1,		// EndDate = tomorrow
			&dlDbHand);
	}
	else {
		lookupType = CKT_Session;
		compareKey = &symKey;
		irtn = genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, 
			keySizeInBits,
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
			CSSM_KEYUSE_ANY, quiet,
			CSSM_TRUE, 1,		// StartDate = tomorrow
			CSSM_TRUE, 1,		// EndDate = tomorrow
			&dlDbHand);
	}
	if(irtn) {
		return irtn;
	}
	
	/* 
	 * fetch stored key from DB, ensure it has same start/end date 
	 */
	if(fetchStoredKey(dlDbHand, lookupType,
			compareKey, "Store key with Bad StartDate", quiet, 
			&lookupKey)) {
		return 1;
	}
	
	/* quickie test, use it for encrypt */
	irtn = doEncrypt(cspHand, keyAlgStr, lookupKey, encrAlg, encrMode,
		encrPad, CSSMERR_CSP_APPLE_INVALID_KEY_START_DATE, quiet);
	if(irtn) {
		printf("***Failure on encrypt, lookup with Bad Start Dates\n");
		return irtn;
	}
	
	/* free and delete everything */
	if(isAsym) {
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &pubKey);
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &privKey);
	}
	else {
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &symKey);
	}
	cspFreeKey(cspHand, lookupKey);

	/*********************/
	
	if(!quiet) {
		printf("   ...verifying Bad EndDate\n");
	}
	if(isAsym) {
		lookupType = CKT_Public;
		compareKey = &pubKey;
		irtn = genKeyPair(cspHand, keyAlg, keyAlgStr, keySizeInBits,
			&pubKey,  pubKeyAttr, CSSM_KEYUSE_ANY,
			&privKey, privKeyAttr, CSSM_KEYUSE_ANY,
			quiet,
			CSSM_TRUE, 0,		// StartDate = today
			CSSM_TRUE, -1,		// EndDate = yesterday
			&dlDbHand);
	}
	else {
		lookupType = CKT_Session;
		compareKey = &symKey;
		irtn = genSymKey(cspHand, &symKey, keyAlg, keyAlgStr, 
			keySizeInBits,
			CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_PERMANENT,
			CSSM_KEYUSE_ANY, quiet,
			CSSM_TRUE, 0,		// StartDate = today
			CSSM_TRUE, -1,		// EndDate = yesterday
			&dlDbHand);
	}
	if(irtn) {
		return irtn;
	}
	
	/* 
	 * fetch stored key from DB, ensure it has same start/end date 
	 */
	if(fetchStoredKey(dlDbHand, lookupType,
			compareKey, "Store key with Bad EndDate", quiet, 
			&lookupKey)) {
		return 1;
	}
	
	/* quickie test, use it for encrypt */
	irtn = doEncrypt(cspHand, keyAlgStr, lookupKey, encrAlg, encrMode,
		encrPad, CSSMERR_CSP_APPLE_INVALID_KEY_END_DATE, quiet);
	if(irtn) {
		printf("***Failure on encrypt, lookup with Bad End Dates\n");
		return irtn;
	}
	
	/* free and delete everything */
	if(isAsym) {
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &pubKey);
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &privKey);
	}
	else {
		cspDeleteKey(cspHand, dlDbHand.DLHandle, dlDbHand.DBHandle,
			&keyLabelData, &symKey);
	}
	cspFreeKey(cspHand, lookupKey);

	return 0;
}


int main(int argc, char **argv)
{
	CSSM_CSP_HANDLE cspHand;
	int irtn;
	CSSM_DL_DB_HANDLE dlDbHand = {0, 0};
	char dbName[100];		/* KD_DB_NAME_pid */
	
	/* user-spec'd variables */
	CSSM_BOOL quiet = CSSM_FALSE;
	CSSM_BOOL doSym = CSSM_TRUE;
	CSSM_BOOL doAsym = CSSM_TRUE;
	CSSM_BOOL doKeyStore = CSSM_TRUE;
	CSSM_BOOL bareCsp = CSSM_TRUE;
	
	int arg;
	for(arg=1; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 's':
				doAsym = doKeyStore = CSSM_FALSE;
				break;
			case 'a':
				doSym = CSSM_FALSE;
				break;
			case 'D':
				bareCsp = CSSM_FALSE;
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'h':
			default:
				usage(argv);
		}
	}

	sprintf(dbName, "%s_%d", KD_DB_NAME, (int)getpid());

	testStartBanner("keyDate", argc, argv);
	cspHand = cspDlDbStartup(bareCsp, NULL);
	if(cspHand == 0) {
		exit(1);
	}
	if(!bareCsp) {
		dlDbHand.DLHandle = dlStartup();
		if(dlDbHand.DLHandle == 0) {
			exit(1);
		}
		CSSM_RETURN crtn = dbCreateOpen(dlDbHand.DLHandle,
			dbName, CSSM_TRUE, CSSM_TRUE, dbName,
			&dlDbHand.DBHandle);
		if(crtn) {
			printf("Error creating %s. Aborting.\n", dbName);
			exit(1);
		}
	}
	privAlg	palg;
	if(doSym) {
		for(palg=SYM_FIRST; palg<=SYM_LAST; palg++) {
			/* once with ref keys */
			irtn = doSymTests(cspHand, palg, CSSM_TRUE, quiet);
			if(irtn) {
				goto abort;
			}
			if(bareCsp) {
				/* and once with raw keys for bare CSP only */
				irtn = doSymTests(cspHand, palg, CSSM_FALSE, quiet);
				if(irtn) {
					goto abort;
				}
			}
			else {
				/* test store/retrieve */
				irtn = doStoreTests(cspHand, dlDbHand,
					palg, CSSM_FALSE, quiet);
				if(irtn) {
					goto abort;
				}
			}
		}
	}
	if(doAsym) {
		for(palg=ASYM_FIRST; palg<=ASYM_LAST; palg++) {
			/* once with ref keys */
			irtn = doAsymTests(cspHand, palg, CSSM_TRUE, quiet);
			if(irtn) {
				goto abort;
			}
			if(bareCsp) {
				/* and once with raw keys for bare CSP only */
				irtn = doAsymTests(cspHand, palg, CSSM_TRUE, quiet);
				if(irtn) {
					goto abort;
				}
			}
			else if(doKeyStore) {
				/* test store/retrieve */
				irtn = doStoreTests(cspHand, dlDbHand, 
					palg, CSSM_TRUE, quiet);
				if(irtn) {
					goto abort;
				}
			}
		}
	}
abort:
	if(irtn == 0) {
		/* be nice: if we ran OK delete the cruft DB we created */
		unlink(dbName);
	}
	return irtn;
}
