/*
 * p12Reencode - take a p12 PFX, decode and reencode
 */
#include <Security/SecImportExport.h>
#include <Security/Security.h>
#include <stdio.h>
#include <stdlib.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <utilLib/common.h>

static void usage(char **argv)
{
	printf("Usage: %s pfx password keychain1 keychain2 [l=loops] [q(uiet)] "
		"[v(erbose)]\n", argv[0]);
	exit(1);
}


#define WRITE_BLOBS	0
#if		WRITE_BLOBS
static void writeBlobs(CFDataRef pfx1, CFDataRef pfx2)
{
	writeFile("pfx1.der", CFDataGetBytePtr(pfx1), CFDataGetLength(pfx1));
	writeFile("pfx2.der", CFDataGetBytePtr(pfx2), CFDataGetLength(pfx2));
	printf("...wrote %u bytes to pfx1.der, %u bytes to pfx2.der\n",
		CFDataGetLength(pfx1), CFDataGetLength(pfx2));
}
#else
#define writeBlobs(p1, p2)
#endif

#if 0
/* Not possible using import/export API */
/* compare attrs, all of which are optional */
static int compareAttrs(
	CFStringRef 	refFriendlyName,
	CFDataRef 		refLocalKeyId,
	CFStringRef 	testFriendlyName,
	CFDataRef 		testLocalKeyId,
	char			*itemType,
	CSSM_BOOL		quiet)
{
	if(refFriendlyName == NULL) {
		if(testFriendlyName != NULL) {
			printf("****s refFriendlyName NULL, testFriendlyName "
				"non-NULL\n", itemType);
			return testError(quiet);
		}
	}
	else {
		CFComparisonResult res = CFStringCompare(refFriendlyName,
			testFriendlyName, 0);
		if(res != kCFCompareEqualTo) {
			printf("***%s friendlyName Miscompare\n", itemType);
			return testError(quiet);
		}
	}
	
	if(refLocalKeyId == NULL) {
		if(testLocalKeyId != NULL) {
			printf("****s refLocalKeyId NULL, testLocalKeyId "
				"non-NULL\n", itemType);
			return testError(quiet);
		}
	}
	else {
		if(compareCfData(refLocalKeyId, testLocalKeyId)) {
			printf("***%s localKeyId Miscompare\n", itemType);
			return testError(quiet);
		}
	}

	/* release the attrs */
	if(refFriendlyName) {
		CFRelease(refFriendlyName);
	}
	if(refLocalKeyId) {
		CFRelease(refLocalKeyId);
	}
	if(testFriendlyName) {
		CFRelease(testFriendlyName);
	}
	if(testLocalKeyId) {
		CFRelease(testLocalKeyId);
	}
	return 0;
}
#endif

static void setUpKeyParams(
	SecKeyImportExportParameters	&keyParams,
	CFStringRef						pwd)
{
	memset(&keyParams, 0, sizeof(keyParams));
	keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
	keyParams.passphrase = pwd;
}

/*
 * Basic import/export: convert between CFArray of keychain items and a CFDataRef
 */
static OSStatus p12Import(
	CFDataRef		pfx,
	CFStringRef		pwd,
	SecKeychainRef	kcRef,
	CFArrayRef		*outArray)
{
	SecKeyImportExportParameters keyParams;
	setUpKeyParams(keyParams, pwd);
	OSStatus ortn;
	SecExternalFormat format = kSecFormatPKCS12;
	
	ortn = SecKeychainItemImport(pfx, NULL, &format, NULL, 0, &keyParams,
		kcRef, outArray);
	if(ortn) {
		cssmPerror("SecKeychainItemImport", ortn);
	}
	return ortn;
}
	
static OSStatus p12Export(
	CFArrayRef		inArray,
	CFStringRef		pwd,
	CFDataRef		*pfx)
{
	SecKeyImportExportParameters keyParams;
	setUpKeyParams(keyParams, pwd);
	OSStatus ortn;
	
	ortn = SecKeychainItemExport(inArray, kSecFormatPKCS12, 0, &keyParams, pfx);
	if(ortn) {
		cssmPerror("SecKeychainItemExport", ortn);
	}
	return ortn;
}

/*
 * Compare two CFArrayRefs containing various items, subsequent to decode. Returns
 * nonzero if they differ.
 *
 * As of April 9 2004, we do NOT see CRLs so we don't compare them. I think
 * we need a SecCRLRef...
 */
static int compareDecodedArrays(
	CFArrayRef refArray,
	CFArrayRef testArray,
	CSSM_BOOL quiet)
{
	OSStatus ortn;
	int ourRtn = 0;
	
	CFIndex numRefItems = CFArrayGetCount(refArray);
	CFIndex numTestItems = CFArrayGetCount(testArray);
	if(numRefItems != numTestItems) {
		printf("***item count mismatch: ref %ld test %ld\n",
			numRefItems, numTestItems);
		return 1;
	}
	for(CFIndex dex=0; dex<numRefItems; dex++) {
		CFTypeRef refItem = CFArrayGetValueAtIndex(refArray, dex);
		CFTypeRef testItem = CFArrayGetValueAtIndex(testArray, dex);
		CFTypeID theType = CFGetTypeID(refItem);
		if(theType != CFGetTypeID(testItem)) {
			printf("***item type mismatch: ref %ld test %ld\n",
				theType, CFGetTypeID(testItem));
			return 1;
		}
		if(theType == SecCertificateGetTypeID()) {
			/* cert: compare raw data */
			CSSM_DATA refData;
			CSSM_DATA testData;
			ortn = SecCertificateGetData((SecCertificateRef)refItem, &refData);
			if(ortn) {
				cssmPerror("SecCertificateGetData", ortn);
				return ++ourRtn;
			}
			ortn = SecCertificateGetData((SecCertificateRef)testItem, &testData);
			if(ortn) {
				cssmPerror("SecCertificateGetData", ortn);
				return ++ourRtn;
			}
			if(!appCompareCssmData(&refData, &testData)) {
				printf("***Data miscompare on cert %ld\n", dex);
				ourRtn = testError(quiet);
				if(ourRtn) {
					return ourRtn;
				}
			}
		}
		else if(theType == SecKeyGetTypeID()) {
			/* Keys - an inexact science to be sure since we don't attempt
			 * to access the raw key material */
			
			const CSSM_KEY *refKey;
			ortn = SecKeyGetCSSMKey((SecKeyRef)refItem, &refKey);
			if(ortn) {
				cssmPerror("SecKeyGetCSSMKey", ortn);
				return ++ourRtn;
			}
			const CSSM_KEY *testKey;
			ortn = SecKeyGetCSSMKey((SecKeyRef)testItem, &testKey);
			if(ortn) {
				cssmPerror("SecPkcs12GetCssmPrivateKey", ortn);
				return ++ourRtn;
			}
			
			/* compare key sizes and algorithm */
			if(refKey->KeyHeader.LogicalKeySizeInBits != 
			   testKey->KeyHeader.LogicalKeySizeInBits) {
				printf("***Key size miscompare on Key %ld\n", dex);
				ourRtn = testError(quiet);
				if(ourRtn) {
					return ourRtn;
				}
			}
			if(refKey->KeyHeader.AlgorithmId != 
			   testKey->KeyHeader.AlgorithmId) {
				printf("***AlgorithmId miscompare on Key %ld\n", dex);
				ourRtn = testError(quiet);
				if(ourRtn) {
					return ourRtn;
				}
			}
		}
		else {
			/* this program may need work here. e.g. for SecCRLRefs */
			printf("***Unknown type ID (%ld)\n", theType);
			ourRtn++;
		}
	}
	
	return ourRtn;
}

int main(int argc, char **argv)
{	
	unsigned char *pfx;
	unsigned pfxLen;
	SecKeychainRef kcRef1 = nil;		// reference, 1st import destination
	SecKeychainRef kcRef2 = nil;		// subsequent import destination
	
	CSSM_BOOL quiet = CSSM_FALSE;
	unsigned loops = 10;
	bool verbose = false;
	bool doPause = false;
	char *kcName = NULL;
	
	int i;

	if(argc < 5) {
		usage(argv);
	}
	
	if(readFile(argv[1], &pfx, &pfxLen)) {
		printf("***Error reading PFX from %s. Aborting.\n", argv[1]);
		exit(1);
	}
	CFStringRef pwd = CFStringCreateWithCString(NULL, argv[2],
					kCFStringEncodingASCII);
	if(pwd == NULL) {
		printf("Bad password (%s)\n", argv[2]);
		exit(1);
	}
	kcName = argv[3];
	OSStatus ortn = SecKeychainOpen(kcName, &kcRef1);
	if(ortn) {
		cssmPerror("SecKeychainOpen", ortn);
		exit(1);
	}
	kcName = argv[4];
	ortn = SecKeychainOpen(kcName, &kcRef2);
	if(ortn) {
		cssmPerror("SecKeychainOpen", ortn);
		exit(1);
	}
	
	for(i=5; i<argc; i++) {
		char *arg = argv[i];
		switch(arg[0]) {
			case 'l':
				loops = atoi(&arg[2]);
				break;
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'p':
				doPause = true;
				break;
			case 'v':
				verbose = true;
				break;
			default:
				usage(argv);
		}
	}
	
	/* do first decode to get the PFX into "our" form */
	CFArrayRef refArray;
	CFDataRef cfdPfx = CFDataCreate(NULL, pfx, pfxLen);

	if(verbose) {
		printf("   ...initial decode\n");
	}
	ortn = p12Import(cfdPfx, pwd, kcRef1, &refArray);
	if(ortn) {
		printf("Error on initial p12Import; aborting.\n");
		exit(1);
	}
	
	/* reencode. At this point the PFXs will not be identical since
	 * everyone packages these up a little differently. */
	CFDataRef refPfx = NULL;
	if(verbose) {
		printf("   ...first reencode\n");
	}
	ortn = p12Export(refArray, pwd, &refPfx);
	if(ortn) {
		printf("Error on initial p12Export; aborting.\n");
		exit(1);
	}
	CFDataRef pfxToDecode = refPfx;
	CFRetain(pfxToDecode);
	
	for(unsigned loop=0; loop<loops; loop++) {
		if(!quiet) {
			printf("..loop %u\n", loop);
		}
		CFArrayRef testArray;
		if(verbose) {
			printf("   ...decode\n");
		}
		ortn = p12Import(pfxToDecode, pwd, kcRef2, &testArray);
		if(ortn) {
			return ortn;
		}

		/*
		 * Compare that decode to our original
		 */
		if(compareDecodedArrays(refArray, testArray, quiet)) {
			exit(1);
		}
		
		/* now reencode, should get blob with same length but different 
		 * data (because salt is random each time) */
		CFDataRef newPfx = NULL;
		if(verbose) {
			printf("   ...reencode\n");
		}
		ortn = p12Export(testArray, pwd, &newPfx);
		if(ortn) {
			exit(1);
		}
		
		if(CFDataGetLength(refPfx) != CFDataGetLength(newPfx)) {
			printf("***PFX length miscompare after reencode\n");
			writeBlobs(refPfx, newPfx);
			return 1;
		}
		if(!memcmp(CFDataGetBytePtr(refPfx), CFDataGetBytePtr(newPfx),
				CFDataGetLength(refPfx))) {
			printf("***Unexpected PFX data compare after reencode\n");
			writeBlobs(refPfx, newPfx);
			return 1;
		}
		CFRelease(pfxToDecode);
		pfxToDecode = newPfx;
		if(doPause) {
			fpurge(stdin);
			printf("Hit CR to continue: ");
			getchar();
		}
		
		/* delete everything we imported into kcRef2 */
		CFIndex numItems = CFArrayGetCount(testArray);
		for(CFIndex dex=0; dex<numItems; dex++) {
			SecKeychainItemRef itemRef = 
				(SecKeychainItemRef)CFArrayGetValueAtIndex(refArray, dex);
			ortn = SecKeychainItemDelete(itemRef);
			if(ortn) {
				cssmPerror("SecKeychainItemDelete", ortn);
				/* 
				 * keep going, but if we're looping this will result in a dup 
				 * item error on the next import 
				 */
			}
		}
		CFRelease(testArray);
	}
	if(!quiet) {
		printf("...p12Reencode complete\n");
	}
	return ortn;
}	

