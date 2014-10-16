/*
 * extendAttrTest.cpp
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/SecKeychainItemExtendedAttributes.h>
#include <Security/Security.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>

#define DEFAULT_KC_NAME	"extendAttr.keychain"

static void usage(char **argv)
{
	printf("usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -k keychain     -- default is %s\n", DEFAULT_KC_NAME);
	printf("  -n              -- don't delete attributes or keychain\n");
	printf("  -q              -- quiet\n");
	exit(1);
}

/* RSA keys, both in OpenSSL format */
#define PUB_KEY			"rsakey_pub.der"
#define PRIV_KEY		"rsakey_priv.der"
#define CERT_FILE		"amazon_v3.100.cer"
#define PWD_SERVICE		"some service"
#define PWD_ACCOUNT		"some account"
#define PWD_PWD			"some password"

/* set up unique extended attributes for each tested item */
typedef struct {
	CFStringRef		attr1Name;
	const char		*attr1Value;
	CFStringRef		attr2Name;
	const char		*attr2Value;
} ItemAttrs;

static const ItemAttrs pubKeyAttrs = {
	CFSTR("one pub key Attribute"),
	"some pub key value",
	CFSTR("another pub key Attribute"),
	"another pub key value"
};

static const ItemAttrs privKeyAttrs = {
	CFSTR("one priv key Attribute"),
	"some priv key value",
	CFSTR("another priv key Attribute"),
	"another priv key value"
};

static const ItemAttrs certAttrs = {
	CFSTR("one cert Attribute"),
	"some cert value",
	CFSTR("another cert Attribute"),
	"another cert value"
};

static const ItemAttrs pwdAttrs = {
	CFSTR("one pwd Attribute"),
	"some pwd value",
	CFSTR("another pwd Attribute"),
	"another pwd value"
};

#define CFRELEASE(cf)	if(cf) { CFRelease(cf); }

/* import file as key into specified keychain */
static int doImportKey(
	const char *fileName,
	SecExternalFormat format,
	SecExternalItemType itemType,
	SecKeychainRef kcRef,
	SecKeyRef *keyRef)			// RETURNED 
{
	unsigned char *item = NULL;
	unsigned itemLen = 0;
	
	if(readFile(fileName, &item, &itemLen)) {
		printf("***Error reading %s. \n", fileName);
	}
	CFDataRef cfd = CFDataCreate(NULL, (const UInt8 *)item, itemLen);
	free(item);
	SecKeyImportExportParameters params;
	memset(&params, 0, sizeof(params));
	params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
	params.keyUsage = CSSM_KEYUSE_ANY;
	params.keyAttributes = CSSM_KEYATTR_PERMANENT;
	if(itemType == kSecItemTypePrivateKey) {
		params.keyAttributes |= CSSM_KEYATTR_SENSITIVE;
	}
	CFArrayRef outArray = NULL;
	OSStatus ortn;
	ortn = SecKeychainItemImport(cfd, NULL, &format, &itemType, 0, &params, kcRef, &outArray);
	if(ortn) {
		cssmPerror("SecKeychainItemImport", ortn);
	}
	CFRelease(cfd);
	if(ortn) {
		return -1;
	}
	if((outArray == NULL) || (CFArrayGetCount(outArray) == 0)) {
		printf("SecKeychainItemImport succeeded, but no returned items\n");
		return -1;
	}
	*keyRef = (SecKeyRef)CFArrayGetValueAtIndex(outArray, 0);
	if(CFGetTypeID(*keyRef) != SecKeyGetTypeID()) {
		printf("***Unknown type returned after import\n");
		return -1;
	}
	CFRetain(*keyRef);
	CFRelease(outArray);
	return 0;
}

/* import file as cert into specified keychain */
static int doImportCert(
	const char *fileName,
	SecKeychainRef kcRef,
	SecCertificateRef *certRef)			// RETURNED 
{
	unsigned char *item = NULL;
	unsigned itemLen = 0;
	
	if(readFile(fileName, &item, &itemLen)) {
		printf("***Error reading %s. \n", fileName);
		return -1;
	}
	CSSM_DATA certData = {itemLen, (uint8 *)item};
	OSStatus ortn = SecCertificateCreateFromData(&certData, 
			CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_DER, certRef);
	if(ortn) {
		cssmPerror("SecCertificateCreateFromData", ortn);
		return -1;
	}
	ortn = SecCertificateAddToKeychain(*certRef, kcRef);
	if(ortn) {
		cssmPerror("SecCertificateAddToKeychain", ortn);
		return -1;
	}
	return 0;
}

/* 
 * Verify specified attr does not exist
 * set it 
 * make sure we get it back 
 */
int testOneAttr(
	SecKeychainItemRef itemRef,
	CFStringRef attrName,
	CFDataRef attrVal,
	bool quiet)
{
	OSStatus ortn;
	CFDataRef fetchedVal = NULL;
	int ourRtn = 0;
	
	if(!quiet) {
		printf("   ...verifying attribute doesn't exist\n");
	}
	ortn = SecKeychainItemCopyExtendedAttribute(itemRef, attrName, &fetchedVal);
	if(ortn != errSecNoSuchAttr) {
		printf("***First SecKeychainItemCopyExtendedAttribute returned %d, expected %d\n",
			(int)ortn, (int)errSecNoSuchAttr);
		ourRtn = -1;
		goto errOut;
	}
	if(!quiet) {
		printf("   ...setting attribute\n");
	}
	ortn = SecKeychainItemSetExtendedAttribute(itemRef, attrName, attrVal);
	if(ortn) {
		cssmPerror("SecKeychainItemSetExtendedAttribute", ortn);
		ourRtn = -1;
		goto errOut;
	}
	if(!quiet) {
		printf("   ...verify attribute\n");
	}
	ortn = SecKeychainItemCopyExtendedAttribute(itemRef, attrName, &fetchedVal);
	if(ortn) {
		cssmPerror("SecKeychainItemCopyExtendedAttribute", ortn);
		ourRtn = -1;
		goto errOut;
	}
	if(!CFEqual(fetchedVal, attrVal)) {
		printf("***Mismatch in set and fetched attribute\n");
		ourRtn = -1;
	}
errOut:
	CFRELEASE(fetchedVal);
	return ourRtn;
}

/*
 * Set two distinct extended attributes;
 * Ensure that each comes back via SecKeychainItemCopyExtendedAttribute();
 * Ensure that both come back via SecKeychainItemCopyAllExtendedAttributes();
 */
int doTest(SecKeychainItemRef itemRef,
	const ItemAttrs &itemAttrs,
	bool quiet)
{
	CFDataRef attrVal1 = CFDataCreate(NULL, 
		(const UInt8 *)itemAttrs.attr1Value, strlen(itemAttrs.attr1Value));
	if(testOneAttr(itemRef, itemAttrs.attr1Name, attrVal1, quiet)) {
		return -1;
	}
	CFDataRef attrVal2 = CFDataCreate(NULL, 
		(const UInt8 *)itemAttrs.attr2Value, strlen(itemAttrs.attr2Value));
	if(testOneAttr(itemRef, itemAttrs.attr2Name, attrVal2, quiet)) {
		return -1;
	}
	
	if(!quiet) {
		printf("   ...verify both attributes via CopyAllExtendedAttributes()\n");
	}
	/* make sure they both come back in SecKeychainItemCopyAllExtendedAttributes */
	CFArrayRef attrNames = NULL;
	CFArrayRef attrValues = NULL;
	OSStatus ortn = SecKeychainItemCopyAllExtendedAttributes(itemRef, &attrNames, &attrValues);
	if(ortn) {
		cssmPerror("SecKeychainItemCopyAllExtendedAttributes", ortn);
		return -1;
	}
	CFIndex numNames = CFArrayGetCount(attrNames);
	CFIndex numValues = CFArrayGetCount(attrValues);
	if((numNames != 2) || (numValues != 2)) {
		printf("***Bad array count after SecKeychainItemCopyAllExtendedAttributes\n");
		printf("   numNames %ld   numValues %ld; expected 2 for both\n",
			(long)numNames, (long)numValues);
		return -1;
	}
	bool found1 = false;
	bool found2 = false;
	for(CFIndex dex=0; dex<numNames; dex++) {
		CFStringRef attrName = (CFStringRef)CFArrayGetValueAtIndex(attrNames, dex);
		CFDataRef valToCompare = NULL;
		if(CFEqual(attrName, itemAttrs.attr1Name)) {
			found1 = true;
			valToCompare = attrVal1;
		}
		else if(CFEqual(attrName, itemAttrs.attr2Name)) {
			found2 = true;
			valToCompare = attrVal2;
		}
		else {
			printf("***Found unknown attribute name\n");
			return -1;
		}
		CFDataRef foundVal = (CFDataRef)CFArrayGetValueAtIndex(attrValues, dex);
		if(!CFEqual(foundVal, valToCompare)) {
			printf("***Attribute Value miscompare\n");
			return -1;
		}
	}
	CFRelease(attrNames);
	CFRelease(attrValues);
	CFRelease(attrVal1);
	CFRelease(attrVal2);
	
	if(!found1 || !found2) {
		printf("***wrote two attribute; found1 %s, found2 %s\n",
			found1 ? "true" : "false", found2 ? "true" : "false");
		return 1;
	}
	
	return 0;
}

/* delete two attrs, verify that none are left */
static int doDeleteTest(
	SecKeychainItemRef itemRef,
	const ItemAttrs &itemAttrs,
	bool quiet)
{
	if(!quiet) {
		printf("   ...deleting both attributes, verifying none are left\n");
	}
	
	OSStatus ortn = SecKeychainItemSetExtendedAttribute(itemRef, itemAttrs.attr1Name, NULL);
	if(ortn) {
		cssmPerror("SecKeychainItemSetExtendedAttribute (NULL)", ortn);
		return -1;
	}
	ortn = SecKeychainItemSetExtendedAttribute(itemRef, itemAttrs.attr2Name, NULL);
	if(ortn) {
		cssmPerror("SecKeychainItemSetExtendedAttribute (NULL)", ortn);
		return -1;
	}
	CFArrayRef attrNames = NULL;
	CFArrayRef attrValues = NULL;
	ortn = SecKeychainItemCopyAllExtendedAttributes(itemRef, &attrNames, &attrValues);
	if(ortn != errSecNoSuchAttr) {
		printf("***Last SecKeychainItemCopyExtendedAttribute returned %d, expected %d\n",
			(int)ortn, (int)errSecNoSuchAttr);
		return -1;
	}
	return 0;
}

/*
 * Verify that SecKeychainItemDelete() also deletes extended attributes.
 *
 * Assuming empty keychain:
 * Import a cert;
 * Set two extended attributes, make sure they're there;
 * Delete the cert;
 * Import the cert again;
 * Verify that the new item has *no* extended attributes;
 */
static int doDeleteItemTest(
	SecKeychainRef kcRef,
	bool quiet)
{
	SecCertificateRef certRef = NULL;
	
	if(doImportCert(CERT_FILE, kcRef, &certRef)) {
		return 1;
	}
	if(!quiet) {
		printf("...testing cert\n");
	}
	if(doTest((SecKeychainItemRef)certRef, certAttrs, quiet)) {
		return -1;
	}
	
	/* doTest() verified that there are two extended attrs */
	if(!quiet) {
		printf("...deleting cert\n");
	}
	OSStatus ortn = SecKeychainItemDelete((SecKeychainItemRef)certRef);
	if(ortn) {
		cssmPerror("SecKeychainItemDelete", ortn);
		return -1;
	}
	CFRelease(certRef);

	if(!quiet) {
		printf("...reimporting cert, verifying it has no extended attributes\n");
	}
	if(doImportCert(CERT_FILE, kcRef, &certRef)) {
		return 1;
	}
	CFArrayRef attrNames = NULL;
	ortn = SecKeychainItemCopyAllExtendedAttributes((SecKeychainItemRef)certRef, &attrNames, 
		NULL);
	if(ortn != errSecNoSuchAttr) {
		printf("***Deleted cert, re-imported it, and the new cert has extended attributes!\n");
		return -1;
	}
	CFRelease(certRef);
	return 0;
}

int main(int argc, char **argv)
{
	const char *kcName = DEFAULT_KC_NAME;
	extern char *optarg;
	int arg;
	bool quiet = false;
	bool noDelete = false;
	
	while ((arg = getopt(argc, argv, "k:qnh")) != -1) {
		switch (arg) {
			case 'k':
				kcName = optarg;
				break;
			case 'n':
				noDelete = true;
				break;
			case 'q':
				quiet = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	testStartBanner("extendAttrTest", argc, argv);
	
	SecKeychainRef kcRef = NULL;
	OSStatus ortn;
	
	if(!quiet) {
		printf("Deleting possible existing keychain and creating %s...\n", kcName);
		
	}
	/* delete possible existing keychain, then create it */
	if (SecKeychainOpen(kcName, &kcRef) == noErr)
	{
		SecKeychainDelete(kcRef);
		CFRelease(kcRef);
	}

	kcRef = NULL;
	ortn = SecKeychainCreate(kcName, 
		strlen(DEFAULT_KC_NAME), DEFAULT_KC_NAME,
		false, NULL, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainCreate", ortn);
		exit(1);
	}
		
	/* import keys */
	SecKeyRef pubKey = NULL;
	SecKeyRef privKey = NULL;
	if(!quiet) {
		printf("Importing %s to keychain...\n", PUB_KEY);
	}
	if(doImportKey(PUB_KEY, kSecFormatOpenSSL, kSecItemTypePublicKey, kcRef, &pubKey)) {
		exit(1);
	}
	if(!quiet) {
		printf("Importing %s to keychain...\n", PRIV_KEY);
	}
	if(doImportKey(PRIV_KEY, kSecFormatOpenSSL, kSecItemTypePrivateKey, kcRef, &privKey)) {
		exit(1);
	}
	
	if(!quiet) {
		printf("...testing public key\n");
	}
	if(doTest((SecKeychainItemRef)pubKey, pubKeyAttrs, quiet)) {
		return -1;
	}
	if(!quiet) {
		printf("...testing private key\n");
	}
	if(doTest((SecKeychainItemRef)privKey, privKeyAttrs, quiet)) {
		return -1;
	}
	
	/* 
	 * Those keys and their extended attrs are still in the keychain. Test a cert. 
	 */
	SecCertificateRef certRef = NULL;
	if(doImportCert(CERT_FILE, kcRef, &certRef)) {
		exit(1);
	}
	if(!quiet) {
		printf("...testing cert\n");
	}
	if(doTest((SecKeychainItemRef)certRef, certAttrs, quiet)) {
		return -1;
	}
	
	/* leaving everything in place, test a generic password. */
	SecKeychainItemRef pwdRef = NULL;
	ortn = SecKeychainAddGenericPassword(kcRef,
		strlen(PWD_SERVICE), PWD_SERVICE,
		strlen(PWD_ACCOUNT), PWD_ACCOUNT,
		strlen(PWD_PWD), PWD_PWD,
		&pwdRef);
	if(ortn) {
		cssmPerror("SecKeychainAddGenericPassword", ortn);
		exit(1);
	}
	if(!quiet) {
		printf("...testing generic password\n");
	}
	if(doTest(pwdRef, pwdAttrs, quiet)) {
		return -1;
	}

	if(noDelete) {
		goto done;
	}
	
	/* delete extended attrs; make sure they really get deleted */
	if(!quiet) {
		printf("...removing extended attributes from public key\n");
	}
	if(doDeleteTest((SecKeychainItemRef)pubKey, pubKeyAttrs, quiet)) {
		exit(1);
	}
	if(!quiet) {
		printf("...removing extended attributes from private key\n");
	}
	if(doDeleteTest((SecKeychainItemRef)privKey, privKeyAttrs, quiet)) {
		exit(1);
	}
	if(!quiet) {
		printf("...removing extended attributes from certificate\n");
	}
	if(doDeleteTest((SecKeychainItemRef)certRef, certAttrs, quiet)) {
		exit(1);
	}
	if(!quiet) {
		printf("...removing extended attributes from generic password\n");
	}
	if(doDeleteTest(pwdRef, pwdAttrs, quiet)) {
		exit(1);
	}

	CFRelease(pubKey);
	CFRelease(privKey);
	CFRelease(pwdRef);
	
	/* Verify that SecKeychainItemDelete() also deletes extended attributes */
	ortn = SecKeychainItemDelete((SecKeychainItemRef)certRef);
	if(ortn) {
		cssmPerror("SecKeychainItemDelete", ortn);
		exit(1);
	}
	CFRelease(certRef);
	if(doDeleteItemTest(kcRef, quiet)) {
		exit(1);
	}
	
	SecKeychainDelete(kcRef);
	CFRelease(kcRef);
done:
	if(!quiet) {
		printf("...Success\n");
	}
	return 0;
}
