/*
 * idPref.cpp - maniuplate Identity Prefs
 */
 
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <clAppUtils/identPicker.h>
#include <Security/Security.h>
#include <CoreFoundation/CoreFoundation.h>

static void usage(char **argv)
{
	printf("usage: %s set|get [options]\n", argv[0]);
	printf("Options:\n");
	printf("  -p prefName\n");
	printf("  -k keychain\n");
	exit(1);
}

int main(int argc, char **argv)
{
	char *kcName = NULL;
	SecKeychainRef kcRef = NULL;
	char *prefName = NULL;
	bool doSet = false;
	
	if((argc < 2) || (argv[1][0] == 'h')) {
		usage(argv);
	}
	if(!strcmp(argv[1], "get")) {
		doSet = false;
	}
	else if(!strcmp(argv[1], "set")) {
		doSet = true;
	}
	else {
		printf("Bad op argument\n");
		usage(argv);
	}
	
	extern int optind;
	optind = 2;
	extern char *optarg;
	int arg;
	while ((arg = getopt(argc, argv, "p:k:h")) != -1) {
		switch (arg) {
			case 'p':
				prefName = optarg;
				break;
			case 'k':
				kcName = optarg;
				break;
			case 'h':
				usage(argv);
		}
	}
	if(optind != argc) {
		usage(argv);
	}
	if(prefName == NULL) {
		printf("***You must specify a preference name via -p.\n");
		usage(argv);
	}
	CFStringRef prefStr = CFStringCreateWithCString(NULL, prefName, kCFStringEncodingASCII);
	if(prefStr == NULL) {
		printf("***Error converting pref name '%s' to CFString.\n", prefName);
		exit(1);
	}
	
	OSStatus ortn;
	if(kcName) {
		ortn = SecKeychainOpen(kcName, &kcRef);
		if(ortn) {
			cssmPerror("SecKeychainOpen", ortn);
			exit(1);
		}
	}
	
	SecIdentityRef idRef = NULL;
	if(doSet) {
		ortn = sslSimpleIdentPicker(kcRef, &idRef);
		if(ortn) {
			printf("Error picking identity; aborting.\n");
			exit(1);
		}
		ortn = SecIdentitySetPreference(idRef, prefStr, 0);
		if(ortn) {
			cssmPerror("SecIdentitySetPreference", ortn);
			exit(1);
		}
		printf("...Identity preference set for name '%s'.\n", prefName);
	}
	else {
		ortn = SecIdentityCopyPreference(prefStr, 0, NULL, &idRef);
		if(ortn) {
			cssmPerror("SecIdentityCopyPreference", ortn);
		}
		else {
			SecCertificateRef certRef = NULL;
			ortn = SecIdentityCopyCertificate(idRef, &certRef);
			if(ortn) {
				cssmPerror("SecIdentityCopyCertificate", ortn);
				exit(1);
			}
			char *idName = kcItemPrintableName((SecKeychainItemRef)certRef);
			printf("Identity for prefName '%s' found : '%s'\n", 
				prefName, idName);
			free(idName);
			CFRelease(certRef);
		}
	}
	CFRelease(idRef);
	
	return 0;
}

