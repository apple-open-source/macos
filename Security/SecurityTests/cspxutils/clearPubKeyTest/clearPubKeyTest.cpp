/*
 * clearPubKeyTest.cpp
 *
 * Test CSSM_KEYATTR_PUBLIC_KEY_ENCRYPT. This cannot be run on a handsoff environment; 
 * it forces Keychain unlock dialogs. 
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/Security.h>
#include "cspwrap.h"
#include "common.h"

#define KEYCHAIN_NAME	"/tmp/clearPubKey.keychain"
#define KEYCHAIN_PWD	"pwd"
#define KEY_ALG			CSSM_ALGID_RSA
#define KEYSIZE			1024
#define ENCRALG			CSSM_ALGID_RSA

static void usage(char **argv)
{
	printf("usage: %s -v(erbose)\n", argv[0]);
	exit(1);
}

static void printNoDialog()
{
	printf("*** If you get a keychain unlock dialog here the test is failing ***\n");
}

static void printExpectDialog()
{
	printf("*** You MUST get a keychain unlock dialog here (password = '%s') ***\n",
		KEYCHAIN_PWD);
}

static bool didGetDialog()
{
	fpurge(stdin);
	printf("Enter 'y' if you just got a keychain unlock dialog: ");
	if(getchar() == 'y') {
		return 1;
	}
	printf("***Well, you really should have. Test failed.\n");
	return 0;
}

static void verboseDisp(bool verbose, const char *str)
{
	if(verbose) {
		printf("...%s\n", str);
	}
}

/* generate key pair, optionally storing the public key in encrypted form */
static int genKeyPair(
	bool pubKeyIsEncrypted,
	SecKeychainRef kcRef,
	SecKeyRef *pubKeyRef,
	SecKeyRef *privKeyRef)
{
	/* gather keygen args */
	CSSM_ALGORITHMS keyAlg = KEY_ALG;
	uint32 keySizeInBits = KEYSIZE;
	CSSM_KEYUSE pubKeyUsage = CSSM_KEYUSE_ANY;
	uint32 pubKeyAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE | CSSM_KEYATTR_PERMANENT;
	if(pubKeyIsEncrypted) {
		pubKeyAttr |= CSSM_KEYATTR_PUBLIC_KEY_ENCRYPT;
	}
	CSSM_KEYUSE privKeyUsage = CSSM_KEYUSE_ANY;
	uint32 privKeyAttr = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE |
						 CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_SENSITIVE;
	
	OSStatus ortn = SecKeyCreatePair(kcRef, keyAlg, keySizeInBits, 0,
		pubKeyUsage, pubKeyAttr,
		privKeyUsage, privKeyAttr,
		NULL,		// default initial access for now 
		pubKeyRef, privKeyRef);
	if(ortn) {
		cssmPerror("SecKeyCreatePair", ortn);
		return 1;
	}
	return 0;
}

/* encrypt something with a public key */
static int pubKeyEncrypt(
	SecKeyRef pubKeyRef)
{
	const CSSM_KEY *cssmKey;
	OSStatus ortn;
	CSSM_CSP_HANDLE cspHand;
	
	ortn = SecKeyGetCSSMKey(pubKeyRef, &cssmKey);
	if(ortn) {
		cssmPerror("SecKeyGetCSSMKey", ortn);
		return -1;
	}
	ortn = SecKeyGetCSPHandle(pubKeyRef, &cspHand);
	if(ortn) {
		cssmPerror("SecKeyGetCSPHandle", ortn);
		return -1;
	}
	
	char *ptext = "something to encrypt";
	CSSM_DATA ptextData = {strlen(ptext), (uint8 *)ptext};
	CSSM_DATA ctextData = { 0, NULL };
	CSSM_RETURN crtn;
	
	crtn = cspEncrypt(cspHand, CSSM_ALGID_RSA,
		0, CSSM_PADDING_PKCS1,	// mode/pad
		cssmKey, NULL,
		0, 0,	// effect/rounds
		NULL,	// IV
		&ptextData, &ctextData, CSSM_FALSE);
	if(crtn) {
		return -1;
	}
	/* slighyly hazardous, allocated by CSPDL's allocator */
	free(ctextData.Data);
	return 0;
}

int main(int argc, char **argv)
{
	bool verbose = false;
	
	int arg;
	while ((arg = getopt(argc, argv, "vh")) != -1) {
		switch (arg) {
			case 'v':
				verbose = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	printNoDialog();
	
	/* initial setup */
	verboseDisp(verbose, "deleting keychain");
	unlink(KEYCHAIN_NAME);
	
	verboseDisp(verbose, "creating keychain");
	SecKeychainRef kcRef = NULL;
	OSStatus ortn = SecKeychainCreate(KEYCHAIN_NAME, 
		strlen(KEYCHAIN_PWD), KEYCHAIN_PWD,
		false, NULL, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainCreate", ortn);
		exit(1);
	}

	/* 
	 * 1. Generate key pair with cleartext public key.
	 *    Ensure we can use the public key when keychain is locked with no
	 *    user interaction.  
	 */
	 
	/* generate key pair, cleartext public key */
	verboseDisp(verbose, "creating key pair, cleartext public key");
	SecKeyRef pubKeyRef = NULL;
	SecKeyRef privKeyRef = NULL;
	if(genKeyPair(false, kcRef, &pubKeyRef, &privKeyRef)) {
		exit(1);
	}
	
	/* Use generated cleartext public key with locked keychain */
	verboseDisp(verbose, "locking keychain, exporting public key");
	SecKeychainLock(kcRef);
	CFDataRef exportData = NULL;
	ortn = SecKeychainItemExport(pubKeyRef, kSecFormatOpenSSL, 0, NULL, &exportData);
	if(ortn) {
		cssmPerror("SecKeychainCreate", ortn);
		exit(1);
	}
	CFRelease(exportData);
	
	verboseDisp(verbose, "locking keychain, encrypting with public key");
	SecKeychainLock(kcRef);
	if(pubKeyEncrypt(pubKeyRef)) {
		exit(1);
	}

	/* reset */
	verboseDisp(verbose, "deleting keys");
	ortn = SecKeychainItemDelete((SecKeychainItemRef)pubKeyRef);
	if(ortn) {
		cssmPerror("SecKeychainItemDelete", ortn);
		exit(1);
	}
	ortn = SecKeychainItemDelete((SecKeychainItemRef)privKeyRef);
	if(ortn) {
		cssmPerror("SecKeychainItemDelete", ortn);
		exit(1);
	}
	CFRelease(pubKeyRef);
	CFRelease(privKeyRef);

	/* 
	 * 2. Generate key pair with encrypted public key.
	 *    Ensure that user interaction is required when we use the public key 
	 *    when keychain is locked.
	 */

	verboseDisp(verbose, "programmatically unlocking keychain");
	ortn = SecKeychainUnlock(kcRef, strlen(KEYCHAIN_PWD), KEYCHAIN_PWD, TRUE);
	if(ortn) {
		cssmPerror("SecKeychainItemDelete", ortn);
		exit(1);
	}
	
	/* generate key pair, encrypted public key */
	verboseDisp(verbose, "creating key pair, encrypted public key");
	if(genKeyPair(true, kcRef, &pubKeyRef, &privKeyRef)) {
		exit(1);
	}

	/* Use generated encrypted public key with locked keychain */
	verboseDisp(verbose, "locking keychain, exporting public key");
	SecKeychainLock(kcRef);
	printExpectDialog();
	ortn = SecKeychainItemExport(pubKeyRef, kSecFormatOpenSSL, 0, NULL, &exportData);
	if(ortn) {
		cssmPerror("SecKeychainCreate", ortn);
		exit(1);
	}
	/* we'll use that exported blob later to test import */
	if(!didGetDialog()) {
		exit(1);
	}
	
	verboseDisp(verbose, "locking keychain, encrypting with public key");
	SecKeychainLock(kcRef);
	printExpectDialog();
	if(pubKeyEncrypt(pubKeyRef)) {
		exit(1);
	}
	if(!didGetDialog()) {
		exit(1);
	}

	/* reset */
	printNoDialog();
	verboseDisp(verbose, "locking keychain");
	SecKeychainLock(kcRef);
	verboseDisp(verbose, "deleting keys");
	ortn = SecKeychainItemDelete((SecKeychainItemRef)pubKeyRef);
	if(ortn) {
		cssmPerror("SecKeychainItemDelete", ortn);
		exit(1);
	}
	ortn = SecKeychainItemDelete((SecKeychainItemRef)privKeyRef);
	if(ortn) {
		cssmPerror("SecKeychainItemDelete", ortn);
		exit(1);
	}
	CFRelease(pubKeyRef);
	CFRelease(privKeyRef);

	/* 
	 * 3. Import public key, storing in cleartext. Ensure that the import
	 *    doesn't require unlock, and ensure we can use the public key 
	 *    when keychain is locked with no user interaction.  
	 */

	printNoDialog();
	verboseDisp(verbose, "locking keychain");
	SecKeychainLock(kcRef);

	/* import public key - default is in the clear */
	verboseDisp(verbose, "importing public key, store in the clear (default)");
	CFArrayRef outArray = NULL;
	SecExternalFormat format = kSecFormatOpenSSL;
	SecExternalItemType type = kSecItemTypePublicKey;
	ortn = SecKeychainItemImport(exportData, 
		NULL, &format, &type,
		0, NULL,
		kcRef, &outArray);
	if(ortn) {
		cssmPerror("SecKeychainItemImport", ortn);
		exit(1);
	}
	CFRelease(exportData);
	if(CFArrayGetCount(outArray) != 1) {
		printf("***Unexpected outArray size (%ld) after import\n",
			(long)CFArrayGetCount(outArray));
		exit(1);
	}
	pubKeyRef = (SecKeyRef)CFArrayGetValueAtIndex(outArray, 0);
	if(CFGetTypeID(pubKeyRef) != SecKeyGetTypeID()) {
		printf("***Unexpected item type after import\n");
		exit(1);
	}
	
	/* Use imported cleartext public key with locked keychain */
	verboseDisp(verbose, "locking keychain, exporting public key");
	SecKeychainLock(kcRef);
	exportData = NULL;
	ortn = SecKeychainItemExport(pubKeyRef, kSecFormatOpenSSL, 0, NULL, &exportData);
	if(ortn) {
		cssmPerror("SecKeychainItemExport", ortn);
		exit(1);
	}
	/* we'll use exportData again */
	
	verboseDisp(verbose, "locking keychain, encrypting with public key");
	SecKeychainLock(kcRef);
	if(pubKeyEncrypt(pubKeyRef)) {
		exit(1);
	}

	/* reset */
	verboseDisp(verbose, "deleting key");
	ortn = SecKeychainItemDelete((SecKeychainItemRef)pubKeyRef);
	if(ortn) {
		cssmPerror("SecKeychainItemDelete", ortn);
		exit(1);
	}
	CFRelease(pubKeyRef);
	
	/* 
	 * Import public key, storing in encrypted form.
	 * Ensure that user interaction is required when we use the public key 
	 * when keychain is locked.
	 */

	/* import public key, encrypted in the keychain */
	SecKeyImportExportParameters impExpParams;
	memset(&impExpParams, 0, sizeof(impExpParams));
	impExpParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
	impExpParams.keyAttributes = CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE | 
								 CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_PUBLIC_KEY_ENCRYPT;
	verboseDisp(verbose, "importing public key, store encrypted");
	printExpectDialog();
	outArray = NULL;
	format = kSecFormatOpenSSL;
	type = kSecItemTypePublicKey;
	ortn = SecKeychainItemImport(exportData, 
		NULL, &format, &type,
		0, &impExpParams,
		kcRef, &outArray);
	if(ortn) {
		cssmPerror("SecKeychainItemImport", ortn);
		exit(1);
	}
	if(!didGetDialog()) {
		exit(1);
	}
	CFRelease(exportData);
	if(CFArrayGetCount(outArray) != 1) {
		printf("***Unexpected outArray size (%ld) after import\n",
			(long)CFArrayGetCount(outArray));
		exit(1);
	}
	pubKeyRef = (SecKeyRef)CFArrayGetValueAtIndex(outArray, 0);
	if(CFGetTypeID(pubKeyRef) != SecKeyGetTypeID()) {
		printf("***Unexpected item type after import\n");
		exit(1);
	}
					
	/* Use imported encrypted public key with locked keychain */
	verboseDisp(verbose, "locking keychain, exporting public key");
	SecKeychainLock(kcRef);
	printExpectDialog();
	ortn = SecKeychainItemExport(pubKeyRef, kSecFormatOpenSSL, 0, NULL, &exportData);
	if(ortn) {
		cssmPerror("SecKeychainItemExport", ortn);
		exit(1);
	}
	if(!didGetDialog()) {
		exit(1);
	}
	CFRelease(exportData);
	
	verboseDisp(verbose, "locking keychain, encrypting with public key");
	SecKeychainLock(kcRef);
	printExpectDialog();
	if(pubKeyEncrypt(pubKeyRef)) {
		exit(1);
	}
	if(!didGetDialog()) {
		exit(1);
	}

	SecKeychainDelete(kcRef);
	printf("...test succeeded.\n");
	return 0;
}
