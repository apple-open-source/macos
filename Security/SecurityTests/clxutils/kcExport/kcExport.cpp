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

static void usage(char **argv)
{
    printf("Usage: %s keychain [option ...]\n", argv[0]);
	printf("Options:\n");
	printf("   -t <type>       itemType = certs|allKeys|pubKeys|privKeys|identities|all\n");
	printf("                   ...default itemType=all\n");
	printf("   -f <format>     format = openssl|openssh1|openssh2|bsafe|pkcs7|pkcs8|pkcs12|pemseq\n"
	       "                            ...default itemType is pemseq for aggregate, openssl\n"
		   "                            for single \n");
	printf("   -p              PEM encode\n");
	printf("   -w              Private keys are wrapped\n");
	printf("   -o outFileName  (default is stdout)\n");
	printf("   -z passphrase   (for PKCS12 and wrapped keys only)\n");
	printf("   -Z              Use secure passphrase\n");
	printf("   -q              Quiet\n");
	printf("   -h              help\n");
	exit(1);
}

typedef enum {
	IS_Certs,
	IS_AllKeys,
	IS_PubKeys,
	IS_PrivKeys,
	IS_Identities,
	IS_All
} ItemSpec;

/* 
 * Add all itmes of specified class from a keychain to an array.
 * Item class are things like kSecCertificateItemClass, and
 * CSSM_DL_DB_RECORD_PRIVATE_KEY. Identities are searched separately.
 */
static OSStatus addKcItems(
	SecKeychainRef kcRef,
	SecItemClass itemClass,		// kSecCertificateItemClass
	CFMutableArrayRef outArray)
{
	OSStatus ortn;
	SecKeychainSearchRef srchRef;
	
	ortn = SecKeychainSearchCreateFromAttributes(kcRef,
		itemClass,
		NULL,		// no attrs
		&srchRef);
	if(ortn) {
		cssmPerror("SecKeychainSearchCreateFromAttributes", ortn);
		return ortn;
	}
	for(;;) {
		SecKeychainItemRef itemRef;
		ortn = SecKeychainSearchCopyNext(srchRef, &itemRef);
		if(ortn) {
			if(ortn == errSecItemNotFound) {
				/* normal search end */
				ortn = noErr;
			}
			else {
				cssmPerror("SecKeychainSearchCopyNext", ortn);
			}
			break;
		}
		CFArrayAppendValue(outArray, itemRef);
		CFRelease(itemRef);		// array owns it
	}
	CFRelease(srchRef);
	return ortn;
}

/* 
 * Add all SecIdentityRefs from a keychain into an array.
 */
static OSStatus addIdentities(
	SecKeychainRef kcRef,
	CFMutableArrayRef outArray)
{
	/* Search for all identities */
	SecIdentitySearchRef srchRef;
	OSStatus ortn = SecIdentitySearchCreate(kcRef, 
		0,				// keyUsage - any
		&srchRef);
	if(ortn) {
		cssmPerror("SecIdentitySearchCreate", ortn);
		return ortn;
	}
	
	do {
		SecIdentityRef identity;
		ortn = SecIdentitySearchCopyNext(srchRef, &identity);
		if(ortn) {
			if(ortn == errSecItemNotFound) {
				/* normal search end */
				ortn = noErr;
			}
			else {
				cssmPerror("SecIdentitySearchCopyNext", ortn);
			}
			break;
		}
		CFArrayAppendValue(outArray, identity);
		
		/* the array has the retain count we need */
		CFRelease(identity);
	} while(ortn == noErr);
	CFRelease(srchRef);
	return ortn;
}

int main(int argc, char **argv)
{
	if(argc < 4) {
		usage(argv);
	}
	
	const char *kcName = argv[1];
	SecKeychainRef kcRef = NULL;
	OSStatus ortn = SecKeychainOpen(kcName, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainOpen", ortn);
		exit(1);
	}
	
	/* user specified options */
	ItemSpec itemSpec = IS_All;		// default
	SecExternalFormat exportForm = kSecFormatUnknown;
	bool pemEncode = false;
	const char *outFile = NULL;
	CFStringRef passphrase = NULL;
	bool securePassphrase = false;
	bool quiet = false;
	bool wrapPrivKeys = false;
	
	extern int optind;
	extern char *optarg;
	int arg;
	optind = 2;
	
	while ((arg = getopt(argc, argv, "t:f:po:z:Zhqw")) != -1) {
		switch (arg) {
			case 't':
				if(!strcmp("certs", optarg)) {
					itemSpec = IS_Certs;
				}
				else if(!strcmp("allKeys", optarg)) {
					itemSpec = IS_AllKeys;
				}
				else if(!strcmp("pubKeys", optarg)) {
					itemSpec = IS_PubKeys;
				}
				else if(!strcmp("privKeys", optarg)) {
					itemSpec = IS_PrivKeys;
				}
				else if(!strcmp("identities", optarg)) {
					itemSpec = IS_Identities;
				}
				else if(!strcmp("all", optarg)) {
					itemSpec = IS_All;
				}
				else {
					usage(argv);
				}
				break;
			case 'f':
				if(!strcmp("openssl", optarg)) {
					exportForm = kSecFormatOpenSSL;
				}
				else if(!strcmp("openssh1", optarg)) {
					exportForm = kSecFormatSSH;
				}
				else if(!strcmp("openssh2", optarg)) {
					exportForm = kSecFormatSSHv2;
				}
				else if(!strcmp("bsafe", optarg)) {
					exportForm = kSecFormatBSAFE;
				}
				else if(!strcmp("pkcs7", optarg)) {
					exportForm = kSecFormatPKCS7;
				}
				else if(!strcmp("pkcs8", optarg)) {
					exportForm = kSecFormatWrappedPKCS8;
				}
				else if(!strcmp("pkcs12", optarg)) {
					exportForm = kSecFormatPKCS12;
				}
				else if(!strcmp("pemseq", optarg)) {
					exportForm = kSecFormatPEMSequence;
				}
				else {
					usage(argv);
				}
				break;
			case 'p':
				pemEncode = true;
				break;
			case 'o':
				outFile = optarg;
				break;
			case 'z':
				passphrase = CFStringCreateWithCString(NULL, optarg,
					kCFStringEncodingASCII);
				break;
			case 'Z':
				securePassphrase = true;
				break;
			case 'w':
				wrapPrivKeys = true;
				break;
			case 'q':
				quiet = true;
				break;
			case '?':
			case 'h':
				default:
					usage(argv);
		}
		
	}
	if(optind != argc) {
		/* getopt does not return '?' */
		usage(argv);
	}
	if(wrapPrivKeys) {
		switch(exportForm) {
			case kSecFormatOpenSSL:
			case kSecFormatUnknown:		// i.e., use default
				exportForm = kSecFormatWrappedOpenSSL;
				break;
			case kSecFormatSSH:
				exportForm = kSecFormatWrappedSSH;
				break;
			case kSecFormatSSHv2:
				/* there is no wrappedSSHv2 */
				exportForm = kSecFormatWrappedOpenSSL;
				break;
			case kSecFormatWrappedPKCS8:
				/* proceed */
				break;
			default:
				printf("Don't know how to wrap in specified format/type.\n");
				exit(1);
		}
	}
	
	/* gather items */
	CFMutableArrayRef exportItems = CFArrayCreateMutable(NULL, 0, 
		&kCFTypeArrayCallBacks);
	switch(itemSpec) {
		case IS_Certs:
			ortn = addKcItems(kcRef, kSecCertificateItemClass, exportItems);
			if(ortn) {
				exit(1);
			}
			break;
			
		case IS_PrivKeys:
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PRIVATE_KEY, exportItems);
			if(ortn) {
				exit(1);
			}
			break;
			
		case IS_PubKeys:
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PUBLIC_KEY, exportItems);
			if(ortn) {
				exit(1);
			}
			break;
			
		case IS_AllKeys:
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PRIVATE_KEY, exportItems);
			if(ortn) {
				exit(1);
			}
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PUBLIC_KEY, exportItems);
			if(ortn) {
				exit(1);
			}
			break;
			
		case IS_All:
			/* No public keys here - PKCS12 doesn't support them */
			ortn = addKcItems(kcRef, kSecCertificateItemClass, exportItems);
			if(ortn) {
				exit(1);
			}
			ortn = addKcItems(kcRef, CSSM_DL_DB_RECORD_PRIVATE_KEY, exportItems);
			if(ortn) {
				exit(1);
			}
			break;
			
		case IS_Identities:
			ortn = addIdentities(kcRef, exportItems);
			if(ortn) {
				exit(1);
			}
			break;
		default:
			printf("Huh? Bogus itemSpec!\n");
			exit(1);
	}
	
	CFIndex numItems = CFArrayGetCount(exportItems);
	
	if(exportForm == kSecFormatUnknown) {
		/* Use default export format per set of items */
		if(numItems > 1) {
			exportForm = kSecFormatPEMSequence;
		}
		else {
			exportForm = kSecFormatOpenSSL;
		}
	}
	uint32 expFlags = 0;		// SecItemImportExportFlags
	if(pemEncode) {
		expFlags |= kSecItemPemArmour;
	}
	
	/* optional key related arguments */
	SecKeyImportExportParameters keyParams;
	SecKeyImportExportParameters *keyParamPtr = NULL;
	if((passphrase != NULL) || securePassphrase) {
		memset(&keyParams, 0, sizeof(keyParams));
		keyParams.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
		if(securePassphrase) {
			/* give this precedence */
			keyParams.flags |= kSecKeySecurePassphrase;
		}
		else {
			keyParams.passphrase = passphrase;		// may be NULL
		}
		keyParamPtr = &keyParams;
	}
	
	/* GO */
	CFDataRef outData = NULL;
	ortn = SecKeychainItemExport(exportItems, exportForm, expFlags, keyParamPtr,
		&outData);
	if(ortn) {
		cssmPerror("SecKeychainItemExport", ortn);
		exit(1);
	}
		
	unsigned len = CFDataGetLength(outData);
	if(outFile) {
		int rtn = writeFile(outFile, CFDataGetBytePtr(outData), len);
		if(rtn == 0) {
			if(!quiet) {
				printf("...%u bytes written to %s\n", len, outFile);
			}
		}
		else {
			printf("***Error writing to %s\n", outFile);
		}
	}
	else {
		int irtn = write(STDOUT_FILENO, CFDataGetBytePtr(outData), len);
		if(irtn != (int)len) {
			perror("write");
		}
	}
	if(!quiet) {
		fprintf(stderr, "\n%u items exported.\n", (unsigned)numItems);
	}
	if(exportItems) {
		/* FIXME this in conjunction with the release of the KC crashes */
		CFRelease(exportItems);
	}
	if(passphrase) {
		CFRelease(passphrase);
	}
	if(outData) {
		CFRelease(outData);
	}
	if(kcRef) {
		CFRelease(kcRef);
	}
	return 0;
}
