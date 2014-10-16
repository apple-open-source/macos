/*
 * kcExport.cpp - export keychain items using SecKeychainItemExport
 */
 
#include <Security/Security.h>
#include <Security/SecImportExport.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utilLib/common.h>
#include <clAppUtils/identPicker.h>

static void usage(char **argv)
{
    printf("Usage: %s infile [option ...]\n", argv[0]);
	printf("Options:\n");
	printf("   -k keychain     Keychain to import into\n");
	printf("   -z passphrase   For PKCS12 and wrapped keys only\n");
	printf("   -w              Private keys are wrapped\n");
	printf("   -d              Display Imported Items\n");
	printf("   -i              Interactive: displays items before possible import\n");
	printf("   -v              Verbose display\n");
	printf("   -l              Loop & Pause for MallocDebug\n");
	printf("   -q              Quiet\n");
	printf("Import type/format options:\n");
	printf("   -t <type>       type = pub|priv|session|cert|agg\n");
	printf("   -f <format>     format = openssl|openssh1|openssh2|bsafe|\n"
	       "                            raw|pkcs7|pkcs8|pkcs12|netscape|pemseq\n");
	printf("Private key options:\n");
	printf("   -a appPath      Add appPath to list of trusted apps in private key's ACL\n");
	printf("   -e              Allow private keys to be extractable in the clear\n");
	printf("   -n              Private keys have *NO* ACL\n");
	printf("   -s              Private keys can sign (only)\n");
	printf("Secure passphrase options:\n");
	printf("   -Z              Get secure passphrase\n");
	printf("   -L title\n");
	printf("   -p prompt\n");
	printf("Verification options:\n");
	printf("   -C numCerts     Verify number of certificates\n");
	printf("   -K numKeys      Verify number of keys\n");
	printf("   -I numIdents    Verify number of identities\n");
	printf("   -F rtnFormat    Returned format = "
					"openssl|openssh1|openssh2|bsafe|raw|pkcs7|pkcs12|netscape|pemseq\n");
	printf("   -T <type>       Returned type = pub|priv|session|cert|agg\n");
	printf("   -m              Set ImportOnlyOneKey bit\n");
	printf("   -M              Set ImportOnlyOneKey bit and expect errSecMultiplePrivKeys\n");
	exit(1);
}

const char *secExtFormatStr(
	SecExternalFormat format)
{
	switch(format) {
		case kSecFormatUnknown:			return "kSecFormatUnknown";
		case kSecFormatOpenSSL:			return "kSecFormatOpenSSL";
		case kSecFormatSSH:				return "kSecFormatSSH";
		case kSecFormatBSAFE:			return "kSecFormatBSAFE";
		case kSecFormatRawKey:			return "kSecFormatRawKey";
		case kSecFormatWrappedPKCS8:	return "kSecFormatWrappedPKCS8";
		case kSecFormatWrappedOpenSSL:  return "kSecFormatWrappedOpenSSL";
		case kSecFormatWrappedSSH:		return "kSecFormatWrappedSSH";
		case kSecFormatWrappedLSH:		return "kSecFormatWrappedLSH";
		case kSecFormatX509Cert:		return "kSecFormatX509Cert";
		case kSecFormatPEMSequence:		return "kSecFormatPEMSequence";
		case kSecFormatPKCS7:			return "kSecFormatPKCS7";
		case kSecFormatPKCS12:			return "kSecFormatPKCS12";
		case kSecFormatNetscapeCertSequence:  return "kSecFormatNetscapeCertSequence";
		case kSecFormatSSHv2:			return "kSecFormatSSHv2";
		default:						return "UNKNOWN FORMAT ENUM";
	}
}

const char *secExtItemTypeStr(
	SecExternalItemType itemType)
{
	switch(itemType) {
		case kSecItemTypeUnknown:		return "kSecItemTypeUnknown";
		case kSecItemTypePrivateKey:	return "kSecItemTypePrivateKey";
		case kSecItemTypePublicKey:		return "kSecItemTypePublicKey";
		case kSecItemTypeSessionKey:	return "kSecItemTypeSessionKey";
		case kSecItemTypeCertificate:   return "kSecItemTypeCertificate";
		case kSecItemTypeAggregate:		return "kSecItemTypeAggregate";
		default:						return "UNKNOWN ITEM TYPE ENUM";
	}
}

static OSStatus processItem(
	SecKeychainRef keychain, 
	SecKeychainItemRef item,
	int *numCerts,				// IN/OUT
	int *numKeys,
	int *numIds,
	bool interactive,			// unimplemented
	bool verbose)
{
	char *kcName = NULL;
	CFTypeID itemType = CFGetTypeID(item);
	if(itemType == SecIdentityGetTypeID()) {
		(*numIds)++;
		if(verbose) {
			/* identities don't have keychains, only their components do */
			SecCertificateRef certRef = NULL;
			OSStatus ortn;
			ortn = SecIdentityCopyCertificate((SecIdentityRef)item, &certRef);
			if(ortn) {
				cssmPerror("SecIdentityCopyCertificate", ortn);
				return ortn;
			}
			kcName = kcItemKcFileName((SecKeychainItemRef)certRef);
			printf(" identity : cert keychain name %s\n", kcName ? kcName : "<none>");
			CFRelease(certRef);
			
			SecKeyRef keyRef = NULL;
			ortn = SecIdentityCopyPrivateKey((SecIdentityRef)item, &keyRef);
			if(ortn) {
				cssmPerror("SecIdentityCopyPrivateKey", ortn);
				return ortn;
			}
			free(kcName);
			kcName = kcItemKcFileName((SecKeychainItemRef)keyRef);
			printf(" identity : key  keychain name %s\n", kcName ? kcName : "<none>");
			CFRelease(keyRef);
		}
	}
	else if(itemType == SecCertificateGetTypeID()) {
		(*numCerts)++;
		if(verbose) {
			kcName = kcItemKcFileName(item);
			printf(" cert     : keychain name %s\n", kcName ? kcName : "<none>");
		}
	}
	else if(itemType == SecKeyGetTypeID()) {
		(*numKeys)++;
		if(verbose) {
			kcName = kcItemKcFileName(item);
			printf(" key      : keychain name %s\n", kcName ? kcName : "<none>");
		}
	}
	/* FIX display attr info, at least names, eventually */
	else {
		printf("***Unknown type returned from SecKeychainItemImport()\n");
		return errSecUnknownFormat;
	}
	if(kcName) {
		free(kcName);
	}
	return noErr;
}

static OSStatus	processItems(
	SecKeychainRef keychain, 
	CFArrayRef outArray,
	int expectNumCerts,				// -1 means don't check
	int expectNumKeys,
	int expectNumIds,
	bool interactive,
	bool displayItems,
	bool verbose)
{
	int numCerts = 0;
	int numKeys = 0;
	int numIds = 0;
	OSStatus ortn;
	
	CFIndex numItems = CFArrayGetCount(outArray);
	for(CFIndex dex=0; dex<numItems; dex++) {
		ortn = processItem(keychain, 
			(SecKeychainItemRef)CFArrayGetValueAtIndex(outArray, dex),
			&numCerts, &numKeys, &numIds, interactive, verbose);
		if(ortn) {
			break;
		}
	}
	if(ortn) {
		return ortn;
	}

	if(displayItems) {
		printf("Certs  found : %d\n", numCerts);
		printf("Keys   found : %d\n", numKeys);
		printf("Idents found : %d\n", numIds);
	}
	if(expectNumCerts >= 0) {
		if(expectNumCerts != numCerts) {
			printf("***Expected %d certs, got %d\n",
				expectNumCerts, numCerts);
			ortn = -1;
		}
	}
	if(expectNumKeys >= 0) {
		if(expectNumKeys != numKeys) {
			printf("***Expected %d keys, got %d\n",
				expectNumKeys, numKeys);
			ortn = -1;
		}
	}
	if(expectNumIds >= 0) {
		if(expectNumIds != numIds) {
			printf("***Expected %d certs, got %d\n",
				expectNumIds, numIds);
			ortn = -1;
		}
	}
	return ortn;
}

/*
 * Parse cmd-line format specifier into an SecExternalFormat.
 * Returns true if it works. 
 */
static bool formatFromString(
	const char *formStr,
	SecExternalFormat *externFormat)
{
	if(!strcmp("openssl", formStr)) {
		*externFormat = kSecFormatOpenSSL;
	}
	else if(!strcmp("openssh1", formStr)) {
		*externFormat = kSecFormatSSH;
	}
	else if(!strcmp("openssh2", formStr)) {
		*externFormat = kSecFormatSSHv2;
	}
	else if(!strcmp("bsafe", formStr)) {
		*externFormat = kSecFormatBSAFE;
	}
	else if(!strcmp("raw", formStr)) {
		*externFormat = kSecFormatRawKey;
	}
	else if(!strcmp("pkcs7", formStr)) {
		*externFormat = kSecFormatPKCS7;
	}
	else if(!strcmp("pkcs8", formStr)) {
		*externFormat = kSecFormatWrappedPKCS8;
	}
	else if(!strcmp("pkcs12", formStr)) {
		*externFormat = kSecFormatPKCS12;
	}
	else if(!strcmp("netscape", formStr)) {
		*externFormat = kSecFormatNetscapeCertSequence;
	}
	else if(!strcmp("pemseq", formStr)) {
		*externFormat = kSecFormatPEMSequence;
	}
	else {
		return false;
	}
	return true;
}
 
 /*
 * Parse cmd-line type specifier into an SecExternalItemType.
 * Returns true if it works. 
 */
static bool itemTypeFromString(
	const char *typeStr,
	SecExternalItemType *itemType)
{
	if(!strcmp("pub", typeStr)) {
		*itemType = kSecItemTypePublicKey;
	}
	else if(!strcmp("priv", typeStr)) {
		*itemType = kSecItemTypePrivateKey;
	}
	else if(!strcmp("session", typeStr)) {
		*itemType = kSecItemTypeSessionKey;
	}
	else if(!strcmp("cert", typeStr)) {
		*itemType = kSecItemTypeCertificate;
	}
	else if(!strcmp("agg", typeStr)) {
		*itemType = kSecItemTypeAggregate;
	}
	else {
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	if(argc < 2) {
		usage(argv);
	}
	
	const char *inFileName = argv[1];
	
	extern int optind;
	extern char *optarg;
	int arg;
	optind = 2;
	int ourRtn = 0;
	SecAccessRef accessRef = NULL;
	OSStatus ortn;
	
	/* optional args */
	const char *keychainName = NULL;
	CFStringRef passphrase = NULL;
	bool interactive = false;
	bool securePassphrase = false;
	SecExternalFormat externFormat = kSecFormatUnknown;
	SecExternalItemType itemType = kSecItemTypeUnknown;
	bool doWrap = false;
	bool allowClearExtract = false;
	int expectNumCerts = -1;		// >=0 means verify per user spec
	int expectNumKeys = -1;
	int expectNumIds = -1;
	bool processOutput = false;
	bool displayItems = false;
	SecExternalFormat expectFormat = kSecFormatUnknown; // otherwise verify 
	SecExternalItemType expectItemType = kSecItemTypeUnknown;   // ditto
	bool quiet = false;
	bool noACL = false;
	char *alertTitle = NULL;
	char *alertPrompt = NULL;
	bool setAllowOnlyOne = false;
	bool expectMultiKeysError = false;
	CFMutableArrayRef trustedAppList = NULL;
	bool loopPause = false;
	bool signOnly = false;
	bool verbose = false;
	
	while ((arg = getopt(argc, argv, "k:iZz:wet:f:C:K:I:F:T:qnL:p:mMa:dlsv")) != -1) {
		switch (arg) {
			case 'k':
				keychainName = optarg;
				break;
			case 'i':
				interactive = true;
				processOutput = true;
				break;
			case 'Z':
				securePassphrase = true;
				break;
			case 'z':
				passphrase = CFStringCreateWithCString(NULL, optarg,
					kCFStringEncodingASCII);
				break;
			case 'w':
				doWrap = true;
				break;
			case 'e':
				allowClearExtract = true;
				break;
			case 't':
				if(!itemTypeFromString(optarg, &itemType)) {
					usage(argv);
				}
				break;
			case 'T':
				if(!itemTypeFromString(optarg, &expectItemType)) {
					usage(argv);
				}
				break;
			case 'f':
				if(!formatFromString(optarg, &externFormat)) {
					usage(argv);
				}
				break;
			case 'F':
				if(!formatFromString(optarg, &expectFormat)) {
					usage(argv);
				}
				break;
			case 'C':
				expectNumCerts = atoi(optarg);
				processOutput = true;
				break;
			case 'K':
				expectNumKeys = atoi(optarg);
				processOutput = true;
				break;
			case 'I':
				expectNumIds = atoi(optarg);
				processOutput = true;
				break;
			case 'd':
				displayItems = true;
				processOutput = true;
				break;
			case 'q':
				quiet = true;
				break;
			case 'n':
				noACL = true;
				break;
			case 'L':
				alertTitle = optarg;
				break;
			case 'p':
				alertPrompt = optarg;
				break;
			case 'm':
				setAllowOnlyOne = true;
				break;
			case 'M':
				setAllowOnlyOne = true;
				expectMultiKeysError = true;
				break;
			case 'a':
			{
				if(trustedAppList == NULL) {
					trustedAppList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				}
				SecTrustedApplicationRef appRef;
				ortn = SecTrustedApplicationCreateFromPath(optarg, &appRef);
				if(ortn) {
					cssmPerror("SecTrustedApplicationCreateFromPath", ortn);
					exit(1);
				}
				CFArrayAppendValue(trustedAppList, appRef);
				break;
			}
			case 's':
				signOnly = true;
				break;
			case 'l':
				loopPause = true;
				break;
			case 'v':
				verbose = true;
				break;
			default:
			case '?':
				usage(argv);
		}
	}
	if(optind != argc) {
		/* getopt does not return '?' */
		usage(argv);
	}
	if(doWrap) {
		switch(externFormat) {
			case kSecFormatOpenSSL:
			case kSecFormatUnknown:		// i.e., use default
				externFormat = kSecFormatWrappedOpenSSL;
				break;
			case kSecFormatSSH:
				externFormat = kSecFormatWrappedSSH;
				break;
			case kSecFormatSSHv2:
				/* there is no wrappedSSHv2 */
				externFormat = kSecFormatWrappedOpenSSL;
				break;
			case kSecFormatWrappedPKCS8:
				/* proceed */
				break;
			default:
				printf("Don't know how to wrap in specified format/type.\n");
				exit(1);
		}
	}
	
	CFArrayRef outArray = NULL;
	CFArrayRef *outArrayP = NULL;
	if(processOutput) {
		outArrayP = &outArray;
	}
	
	SecKeychainRef kcRef = NULL;
	if(keychainName) {
		OSStatus ortn = SecKeychainOpen(keychainName, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainOpen", ortn);
			exit(1);
		}
		/* why is this failing later */
		CSSM_DL_DB_HANDLE dlDbHandle;
		ortn = SecKeychainGetDLDBHandle(kcRef, &dlDbHandle);
		if(ortn) {
			cssmPerror("SecKeychainGetDLDBHandle", ortn);
			exit(1);
		}

	}
	
	unsigned char *inFile = NULL;
	unsigned inFileLen = 0;
	if(readFile(inFileName, &inFile, &inFileLen)) {
		printf("***Error reading input file %s. Aborting.\n", inFileName);
		exit(1);
	}
	CFDataRef inFileRef = CFDataCreate(NULL, inFile, inFileLen);
	CFStringRef fileNameStr = CFStringCreateWithCString(NULL, inFileName,
			kCFStringEncodingASCII);
	
loopTop:
	SecKeyImportExportParameters keyParams;
	SecKeyImportExportParameters *keyParamPtr = NULL;

	if(passphrase || securePassphrase || allowClearExtract || noACL || 
					 setAllowOnlyOne || trustedAppList || signOnly) {
		keyParamPtr = &keyParams;
		memset(&keyParams, 0, sizeof(keyParams));
		if(securePassphrase) {
			/* give this precedence */
			keyParams.flags |= kSecKeySecurePassphrase;
			if(alertTitle) {
				keyParams.alertTitle =
					CFStringCreateWithCString(NULL, alertTitle, kCFStringEncodingASCII);
			}
			if(alertPrompt) {
				keyParams.alertPrompt =
					CFStringCreateWithCString(NULL, alertPrompt, kCFStringEncodingASCII);
			}
		}
		else if(passphrase) {
			keyParams.passphrase = passphrase;
		}
		if(noACL) {
			keyParams.flags |= kSecKeyNoAccessControl;
		}
		if(setAllowOnlyOne) {
			keyParams.flags |= kSecKeyImportOnlyOne;
		}
		keyParams.keyAttributes = ( CSSM_KEYATTR_RETURN_REF | CSSM_KEYATTR_EXTRACTABLE);
		if(!allowClearExtract) {
			keyParams.keyAttributes |= CSSM_KEYATTR_SENSITIVE;
		}
		if(kcRef) {
			keyParams.keyAttributes |= CSSM_KEYATTR_PERMANENT;
		}
		if(signOnly) {
			keyParams.keyUsage = CSSM_KEYUSE_SIGN;
		}
		else {
			keyParams.keyUsage = CSSM_KEYUSE_ANY;
		}
		if(trustedAppList) {
			ortn = SecAccessCreate(CFSTR("Imported Private Key"), trustedAppList, &accessRef);
			if(ortn) {
				cssmPerror("SecAccessCreate", ortn);
				exit(1);
			}
			keyParams.accessRef = accessRef;
		}
		/* TBD other stuff - usage? Other? */
	}
	
	/* GO */
	ortn = SecKeychainItemImport(inFileRef, 
		fileNameStr,
		&externFormat,
		&itemType,
		0,			// flags
		keyParamPtr,
		kcRef,
		outArrayP);
	ourRtn = 0;
	if(ortn) {
		if(expectMultiKeysError && (ortn == errSecMultiplePrivKeys)) {
			if(!quiet) {
				printf("...errSecMultiplePrivKeys error seen as expected\n");
			}
		}
		else {
			cssmPerror("SecKeychainItemImport", ortn);
			ourRtn = -1;
		}
	}
	else if(expectMultiKeysError) {
		printf("***errSecMultiplePrivKeys expected but no error seen\n");
		ourRtn = -1;
	}
	if(ortn == noErr) {
		if(!quiet) {
			printf("...import successful. Returned format %s\n",
				secExtFormatStr(externFormat));
		}
		if(expectFormat != kSecFormatUnknown) {
			if(expectFormat != externFormat) {
				printf("***Expected format %s, got %s\n",
					secExtFormatStr(expectFormat),
					secExtFormatStr(externFormat));
				ourRtn = -1;
			}
		}
		if(expectItemType != kSecItemTypeUnknown) {
			if(expectItemType != itemType) {
				printf("***Expected itemType %s, got %s\n",
					secExtItemTypeStr(expectItemType),
					secExtItemTypeStr(itemType));
				ourRtn = -1;
			}
		}
		if(processOutput) {
			ourRtn |= processItems(kcRef, outArray, expectNumCerts, 
				expectNumKeys, expectNumIds, interactive, displayItems, verbose);
		}
	}
	if(loopPause) {
		fflush(stdin);
		printf("Pausing for MallocDebug; CR to continue: ");
		getchar();
		goto loopTop;
	}
	return ourRtn;
}
