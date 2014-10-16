/*
 * sysIdTool.cpp
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <Security/Security.h>
#include <utilLib/common.h>
#include <clAppUtils/identPicker.h>
#include <clAppUtils/printCertName.h>
#include <security_cdsa_utils/cuPrintCert.h>

static void usage(char **argv)
{
	printf("usage: %s command domain [options]\n", argv[0]);
	printf("Commands:\n");
	printf("  s   -- select with picker, set as identity for domain\n");
	printf("  d   -- display identity for domain\n");
	printf("  D   -- delete identity for domain\n");
	printf("Options:\n");
	printf("  -v  -- verbose display of certs\n");
	printf("  -l  -- loop for malloc debug\n");
	printf("  <none for now>\n");
	/* etc. */
	exit(1);
}


static int selectId(CFStringRef domain)
{
	/* open system keychain */
	SecKeychainRef kcRef;
	const char *sysKcPath = kSystemKeychainDir  kSystemKeychainName;
	
	OSStatus ortn = SecKeychainOpen(sysKcPath, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainOpen", ortn);
		exit(1);
	}
	
	/* pick an identity */
	SecIdentityRef idRef = NULL;
	ortn = sslSimpleIdentPicker(kcRef, &idRef);
	CFRelease(kcRef);
	if(ortn) {
		printf("IdentityPicker aborted\n");
		return -1;
	}
	
	ortn = SecIdentitySetSystemIdentity(domain, idRef);
	if(ortn) {
		cssmPerror("SecIdentitySetSystemIdentity", ortn);
	}
	else {
		printf("...system identity set.\n");
	}
	CFRelease(idRef);
	return ortn;
}

static void printCFString(
	const char *label,
	CFStringRef cfString)
{
	char cstr[300];
	if(!CFStringGetCString(cfString, cstr, sizeof(cstr),
			kCFStringEncodingUTF8)) {
		printf("***Error converting %s to UTF8\n", label);
	}
	else {
		printf("%s '%s'\n", label, cstr);
	}
}

static int showId(CFStringRef domain, bool verbose)
{
	SecIdentityRef idRef = NULL;
	CFStringRef actualDomain = NULL;
	OSStatus ortn;
	
	ortn = SecIdentityCopySystemIdentity(domain, &idRef, &actualDomain);
	if(ortn) {
		cssmPerror("SecIdentityCopySystemIdentity", ortn);
		return ortn;
	}
	SecCertificateRef certRef = NULL;
	ortn = SecIdentityCopyCertificate(idRef, &certRef);
	if(ortn) {
		cssmPerror("SecIdentityCopyCertificate", ortn);
		CFRelease(idRef);
		return ortn;
	}
	CSSM_DATA certData;
	ortn = SecCertificateGetData(certRef, &certData);
	if(ortn) {
		cssmPerror("SecCertificateGetData", ortn);
		CFRelease(idRef);
		CFRelease(certRef);
		return ortn;
	}
	
	printCFString("Identity obtained for domain", domain);
	if(verbose) {
		printf("\n ---- System Identity Certificate ----\n");
		printCert(certData.Data, certData.Length, CSSM_FALSE);
		printf(" ---- End of System Identity Certificate ----\n");
	}
	else {
		printCertName(certData.Data, certData.Length, NameIssuer);
	}
	printCFString("Actual domain :", actualDomain);
	CFRelease(idRef);
	CFRelease(certRef);
	CFRelease(actualDomain);
	return 0;
}

int main(int argc, char **argv)
{
	char op;
	char *domain;
	
	if(argc < 3) {
		usage(argv);
	}
	op = argv[1][0];
	domain = argv[2];
	
	bool verbose = false;
	bool loop = false;
	
	//extern char *optarg;
	int arg;
	optind = 3;
	while ((arg = getopt(argc, argv, "hvl")) != -1) {
		switch (arg) {
			case 'v':
				verbose = true;
				break;
			case 'l':
				loop = true;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	
	CFStringRef cfDomain = CFStringCreateWithCString(NULL, domain, kCFStringEncodingASCII);
	int ourRtn = 0;
	do {
		switch(op) {
			case 's':
				ourRtn = selectId(cfDomain);
				break;
			case 'd':
				ourRtn = showId(cfDomain, verbose);
				break;
			case 'D':
				ourRtn = SecIdentitySetSystemIdentity(cfDomain, NULL);
				if(ourRtn) {
					cssmPerror("SecIdentitySetSystemIdentity(NULL)", ourRtn);
				}
				else {
					printf("...system identity assignment deleted.\n");
				}
				break;
			default:
				usage(argv);	
		}
		if(ourRtn) {
			break;
		}
		if(loop) {
			fpurge(stdin);
			printf("q to quit, CR to loop again: ");
			if(getchar() == 'q') {
				break;
			}
		}
	} while(loop);
	return ourRtn;
}
