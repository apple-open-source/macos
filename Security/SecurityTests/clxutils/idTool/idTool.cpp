/*
 * Examine and test a keychain's identity
 */
#include <Security/Security.h>
#include <stdlib.h>
#include <stdio.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <clAppUtils/clutils.h>

typedef enum {
	KC_Nop,
	KC_GetInfo,
	KC_LockKC,
	KC_UnlockKC,
	KC_SignVfy,
	KC_KeyCertInfo
} KcOp;

static void usage(char **argv)
{
	printf("Usage: %s keychain|- cmd [options]\n", argv[0]);
	printf("Command:\n");
	printf("  i  get KC info\n");
	printf("  k  get key and cert info\n");
	printf("  l  lock\n");
	printf("  u  unlock\n");
	printf("  s  sign and verify\n");
	printf("Options:\n");
	printf("  p=passphrase\n");
	printf("Specifying '-' for keychain means NULL, default\n");
	exit(1);
}

static void showError(
	OSStatus ortn,
	const char *msg)
{
	const char *errStr = NULL;
	switch(ortn) {
		case errSecItemNotFound:
			errStr = "errSecItemNotFound"; break;
		case errSecNoSuchKeychain:
			errStr = "errSecNoSuchKeychain"; break;
		case errSecNotAvailable:
			errStr = "errSecNotAvailable"; break;
		/* more? */
		default:
			if(ortn < (CSSM_BASE_ERROR + 
					(CSSM_ERRORCODE_MODULE_EXTENT * 8))) {
				/* assume CSSM error */
				errStr = cssmErrToStr(ortn);
			}
			break;

	}
	if(errStr) {
		printf("***Error on %s: %s\n", msg, errStr);
	}
	else {
		printf("***Error on %s: %d(d)\n", msg, (int)ortn);
	}
}

static void printDataAsHex(
	const CSSM_DATA *d,
	unsigned maxToPrint = 0)		// optional, 0 means print it all
{
	unsigned i;
	bool more = false;
	uint32 len = d->Length;
	uint8 *cp = d->Data;
	
	if((maxToPrint != 0) && (len > maxToPrint)) {
		len = maxToPrint;
		more = true;
	}	
	for(i=0; i<len; i++) {
		printf("%02X ", ((unsigned char *)cp)[i]);
	}
	if(more) {
		printf("...\n");
	}
	else {
		printf("\n");
	}
}

static void printKeyHeader(
	const CSSM_KEYHEADER &hdr)
{
	printf("   Algorithm       : ");
	switch(hdr.AlgorithmId) {
		case CSSM_ALGID_RSA:
			printf("RSA\n");
			break;
		case CSSM_ALGID_DSA:
			printf("DSA\n");
			break;
		case CSSM_ALGID_FEE:
			printf("FEE\n");
			break;
		case CSSM_ALGID_DH:
			printf("Diffie-Hellman\n");
			break;
		default:
			printf("Unknown(%u(d), 0x%x)\n", (unsigned)hdr.AlgorithmId, 
				(unsigned)hdr.AlgorithmId);
	}
	printf("   Key Size        : %u bits\n", 
		(unsigned)hdr.LogicalKeySizeInBits);
	printf("   Key Use         : ");
	CSSM_KEYUSE usage = hdr.KeyUsage;
	if(usage & CSSM_KEYUSE_ANY) {
		printf("CSSM_KEYUSE_ANY ");
	}
	if(usage & CSSM_KEYUSE_ENCRYPT) {
		printf("CSSM_KEYUSE_ENCRYPT ");
	}
	if(usage & CSSM_KEYUSE_DECRYPT) {
		printf("CSSM_KEYUSE_DECRYPT ");
	}
	if(usage & CSSM_KEYUSE_SIGN) {
		printf("CSSM_KEYUSE_SIGN ");
	}
	if(usage & CSSM_KEYUSE_VERIFY) {
		printf("CSSM_KEYUSE_VERIFY ");
	}
	if(usage & CSSM_KEYUSE_SIGN_RECOVER) {
		printf("CSSM_KEYUSE_SIGN_RECOVER ");
	}
	if(usage & CSSM_KEYUSE_VERIFY_RECOVER) {
		printf("CSSM_KEYUSE_VERIFY_RECOVER ");
	}
	if(usage & CSSM_KEYUSE_WRAP) {
		printf("CSSM_KEYUSE_WRAP ");
	}
	if(usage & CSSM_KEYUSE_UNWRAP) {
		printf("CSSM_KEYUSE_UNWRAP ");
	}
	if(usage & CSSM_KEYUSE_DERIVE) {
		printf("CSSM_KEYUSE_DERIVE ");
	}
	printf("\n");

}

static OSStatus getIdentity(
	SecKeychainRef kcRef,
	CSSM_KEYUSE keyUse,
	SecIdentityRef &idRef)
{
	SecIdentitySearchRef srchRef = nil;
	OSStatus ortn = SecIdentitySearchCreate(kcRef, keyUse, &srchRef);
	if(ortn) {
		showError(ortn, "SecIdentitySearchCreate");
		return ortn;
	}
	ortn = SecIdentitySearchCopyNext(srchRef, &idRef);
	if(ortn) {
		showError(ortn, "SecIdentitySearchCopyNext");
		return ortn;
	}
	if(CFGetTypeID(idRef) != SecIdentityGetTypeID()) {
		printf("SecIdentitySearchCopyNext CFTypeID failure!\n");
		return paramErr;
	}
	return noErr;
}

static OSStatus getKeyCertInfo(
	SecCertificateRef certRef,
	SecKeyRef keyRef,
	CSSM_KEY_PTR cssmKey,
	CSSM_CSP_HANDLE cspHand)
{
	/* display the private key */
	if(cssmKey == NULL) {
		printf("   ***malformed CSSM_KEY\n");
	}
	else {
		printf("Private Key        :\n");
		printKeyHeader(cssmKey->KeyHeader);
		printf("   Key Blob        : ");
		printDataAsHex(&cssmKey->KeyData, 8);
	}

	/* and the cert */
	CSSM_DATA certData;
	OSStatus ortn = SecCertificateGetData(certRef, &certData);
	if(ortn) {
		showError(ortn, "SecCertificateGetData");
		return ortn;
	}
	printf("\nCertificate        :\n");
	printCert((unsigned char *)certData.Data, (unsigned)certData.Length,
		CSSM_TRUE);
	return noErr;
}

#define SIG_ALG		CSSM_ALGID_SHA1WithRSA

static OSStatus signVfy(
	SecCertificateRef certRef,
	SecKeyRef keyRef,
	CSSM_KEY_PTR cssmKey,
	CSSM_CSP_HANDLE cspHand)
{
	uint8 someData[] = {0,1,2,3,4,5,6,7,8};
	CSSM_DATA ptext = {sizeof(someData), someData};
	CSSM_DATA sig = {0, NULL};
	CSSM_RETURN crtn;
	
	/* sign with CSPDL */
	crtn = cspSign(cspHand, SIG_ALG, cssmKey, &ptext, &sig);
	if(crtn) {
		printf("Error signing with private key\n");
		return crtn;
	}
	
	/* attach to CL */
	CSSM_CL_HANDLE clHand = clStartup();
	if(clHand == 0) {
		printf("***Error attaching to CL\n");
		return ioErr;
	}
	
	/* get the public key from the cert */
	CSSM_DATA certData;
	OSStatus ortn = SecCertificateGetData(certRef, &certData);
	if(ortn) {
		showError(ortn, "SecCertificateGetData");
		return ortn;
	}
	CSSM_KEY_PTR pubKey = NULL;
	crtn = CSSM_CL_CertGetKeyInfo(clHand, &certData, &pubKey);
	if(crtn) {
		printError("CSSM_CL_CertGetKeyInfo", crtn);
		return crtn;
	}
	
	/* attach to raw CSP */
	CSSM_CSP_HANDLE rawCspHand = cspStartup();
	if(rawCspHand == 0) {
		printf("***Error attaching to raw CSP\n");
		return ioErr;
	}
	
	/* verify with raw CSP and raw public key */
	crtn = cspSigVerify(rawCspHand, SIG_ALG, pubKey, &ptext, 
		&sig, CSSM_OK);
	if(crtn) {
		printf("Error verifying with public key\n");
		return crtn;
	}
	
	/* free everything */
	CSSM_ModuleDetach(rawCspHand);
	CSSM_ModuleDetach(clHand);
	printf("...sign with private key, vfy with cert OK\n");
	return noErr;
}

/* get cert and private key (in Sec and CSSM form) from identity */
static OSStatus getKeyCert(
	SecIdentityRef 		idRef,
	SecCertificateRef	&certRef,		// RETURNED
	SecKeyRef			&keyRef,		// private key, RETURNED
	CSSM_KEY_PTR		&cssmKey)		// private key, RETURNED
{
	OSStatus ortn = SecIdentityCopyCertificate(idRef, &certRef);
	if(ortn) {
		showError(ortn, "SecIdentityCopyCertificate");
		return ortn;
	}
	ortn = SecIdentityCopyPrivateKey(idRef, &keyRef);
	if(ortn) {
		showError(ortn, "SecIdentityCopyPrivateKey");
		return ortn;
	}
	ortn = SecKeyGetCSSMKey(keyRef, (const CSSM_KEY **)&cssmKey);
	if(ortn) {
		showError(ortn, "SecKeyGetCSSMKey");
	}
	return ortn;	
}

int main(int argc, char **argv)
{
	SecKeychainRef 		kcRef = nil;
	OSStatus 			ortn;
	int					arg;
	char				*argp;
	
	/* user-spec'd variables */
	KcOp				op = KC_Nop;
	char 				*pwd = NULL;
	char				*kcName;
	
	if(argc < 3) {
		usage(argv);
	}
	kcName = argv[1];
	if(!strcmp("-", kcName)) {
		/* null - no open */
		kcName = NULL;
	}
	switch(argv[2][0]) {
		case 'i':
			op = KC_GetInfo; break;
		case 'l':
			op = KC_LockKC; break;
		case 'u':
			op = KC_UnlockKC; break;
		case 's':
			op = KC_SignVfy; break;
		case 'k':
			op = KC_KeyCertInfo; break;
		default:
			usage(argv);
	}
	for(arg=3; arg<argc; arg++) {
		argp = argv[arg];
		switch(argp[0]) {
			case 'p':
				pwd = &argp[2];
				break;
			default:
				usage(argv);
		}
	}
	
	if(kcName != NULL) {
		ortn = SecKeychainOpen(kcName, &kcRef);
		if(ortn) {
			showError(ortn, "SecKeychainOpen");
			printf("Cannot open keychain at %s. Aborting.\n", kcName);
			exit(1);
		}
	}
	
	/* handle trivial commands right now */
	switch(op) {
		case KC_LockKC:
			ortn = SecKeychainLock(kcRef);
			if(ortn) {
				showError(ortn, "SecKeychainLock");
				exit(1);				
			}
			printf("...keychain %s locked.\n", argv[1]);
			exit(0);
			
		case KC_UnlockKC:
			if(pwd == NULL) {
				printf("***Warning: unlocking with no password\n");
			}
			ortn = SecKeychainUnlock(kcRef,
				pwd ? strlen(pwd) : 0,
				pwd,
				pwd ? true : false);		// usePassword
			if(ortn) {
				showError(ortn, "SecKeychainUnlock");
				exit(1);	
			}
			printf("...keychain %s unlocked.\n", argv[1]);
			exit(0);

		case KC_GetInfo:
		{
			SecKeychainStatus kcStat;
			ortn = SecKeychainGetStatus(kcRef, &kcStat);
			if(ortn) {
				showError(ortn, "SecKeychainGetStatus");
				exit(1);				
			}
			printf("...SecKeychainStatus = %u ( ", (unsigned)kcStat);
			if(kcStat & kSecUnlockStateStatus) {
				printf("UnlockState ");
			}
			if(kcStat & kSecReadPermStatus) {
				printf("RdPerm ");
			}
			if(kcStat & kSecWritePermStatus) {
				printf("WrPerm ");
			}
			printf(")\n");
			exit(0);
		}
		
		default:
			/* more processing below */
			break;
	}

	/* remaining cmds need an identity */
	SecIdentityRef idRef;
	ortn = getIdentity(kcRef, CSSM_KEYUSE_SIGN, idRef);
	if(ortn) {
		printf("***No identity found in keychain %s. Aborting.\n", kcName);
		exit(1);
	}
	
	/* and a CSP */
	CSSM_CSP_HANDLE cspHand;
	ortn = SecKeychainGetCSPHandle(kcRef, &cspHand);
	if(ortn) {
		showError(ortn, "SecKeychainGetCSPHandle");
		exit(1);
	}

	/* and the cert and keys */
	SecCertificateRef certRef = nil;
	SecKeyRef keyRef = nil;
	CSSM_KEY_PTR privKey = NULL;
	ortn = getKeyCert(idRef, certRef, keyRef, privKey);
	if(ortn) {
		printf("***Incomplete identity\n");
		exit(1);
	}
	
	switch(op) {
		case KC_KeyCertInfo:
			ortn = getKeyCertInfo(certRef, keyRef, privKey, cspHand);
			break;
		case KC_SignVfy:
			ortn = signVfy(certRef, keyRef, privKey, cspHand);
			break;
		default:
			printf("BRRRZAP!\n");
			exit(1);
	}
	CFRelease(idRef);
	if(kcRef) {
		CFRelease(kcRef);
	}
	return (int)ortn;
}
