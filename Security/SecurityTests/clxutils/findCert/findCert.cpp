/* 
 * Find all certs in keychain search list matching specified email address.
 */
#include <Security/Security.h>
#include <Security/SecKeychainItemPriv.h>
#include <unistd.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include "asnUtils.h"

static void usage(char **argv)
{
	printf("Usage: %s [emailaddrs] [option...]\n", argv[0]);
	printf("Options:\n");
	printf("   -p       -- print cert contents\n");
	printf("   -a       -- show all certs, no email match\n");
	printf("   -A       -- add to default keychain\n");
	exit(1);
}

int main(int argc, char **argv)
{
	if(argc < 2) {
		usage(argv);
	}
	
	bool print_cert = false;
	bool allCerts = false;
	const char *emailAddress = NULL;
	bool addToKC = false;
	
	extern int optind;
	optind = 1;

	if(argv[1][0] != '-') {
		/* normal case, email address specified */
		emailAddress = argv[1];
		optind++;
	}
	
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "aphA")) != -1) {
		switch (arg) {
			case 'p':
				print_cert = true;
				break;
			case 'a':
				allCerts = true;
				break;
			case 'A':
				addToKC = true;
				break;
			case 'h':
			default:
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	if(!allCerts && (emailAddress == NULL)) {
		printf("***You must specify either an email address or the -a option.\n");
		exit(1);
	}
	
	OSStatus					ortn;
	SecKeychainSearchRef		srch;
	SecKeychainAttributeList	attrList;
	SecKeychainAttribute		attr;
	unsigned					numCerts = 0;
	
	if(emailAddress) {
		attr.tag = kSecAlias;			// i.e., email address
		attr.length = strlen(emailAddress);
		attr.data = (void *)emailAddress;
		attrList.count = 1;
		attrList.attr = &attr;
	}
	else {
		attrList.count = 0;
		attrList.attr = NULL;
	}
	ortn = SecKeychainSearchCreateFromAttributes(NULL,	// default search list
		kSecCertificateItemClass,
		&attrList,
		&srch);
	if(ortn) {
		cssmPerror("SecKeychainSearchCreateFromAttributes", ortn);
		exit(1);
	}
	
	do {
		SecCertificateRef certRef = NULL;
		CSSM_DATA certData;
		
		ortn = SecKeychainSearchCopyNext(srch, (SecKeychainItemRef *)&certRef);
		if(ortn) {
			break;
		}
		ortn = SecCertificateGetData(certRef, &certData);
		if(ortn) {
			cssmPerror("SecCertificateGetData", ortn);
			continue;
		}
		
		printf("=== Cert %u ===\n", numCerts);
		printCertName(certData.Data, certData.Length, NameBoth);
		if(print_cert) {
			printCert(certData.Data, certData.Length, CSSM_FALSE);
		}
		if(addToKC) {
			/* 
			 * Can't call SecCertificateAddToKeychain directly since this 
			 * cert already has a keychain. 
			 */
			SecCertificateRef newCertRef = NULL;
			ortn = SecCertificateCreateFromData(&certData,	
				CSSM_CERT_X_509v3,	CSSM_CERT_ENCODING_DER, 
				&newCertRef);
			if(ortn) {
				cssmPerror("SecCertificateCreateFromData", ortn);
				printf("***Error adding this cert to default keychain.\n");
			}
			else {
				ortn = SecCertificateAddToKeychain(newCertRef, NULL);
				if(ortn) {
					cssmPerror("SecCertificateAddToKeychain", ortn);
					printf("***Error adding this cert to default keychain.\n");
				}
				else {
					printf("...cert added to default keychain.\n");
				}
				CFRelease(newCertRef);
			}
		}
		CFRelease(certRef);
		numCerts++;
	} while(ortn == noErr);
	printf("...%u certs found matching email address \'%s\'\n", numCerts, 
		emailAddress ? emailAddress : "<any>");
	CFRelease(srch);
	return 0;
}

