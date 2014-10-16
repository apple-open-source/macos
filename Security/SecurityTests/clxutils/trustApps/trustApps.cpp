/*
 * trustApps.cpp - set list of trusted apps for specified executable
 */
#include <Security/Security.h>
#include <stdio.h>
#include <stdlib.h>
#include <utilLib/common.h>
#include <clAppUtils/identPicker.h>

static void usage(char **argv)
{
	printf("Usage: %s keychain [-q(uiet)] executable ...\n", argv[0]);
	exit(1);
}

int main(int argc, char **argv)
{
	if(argc < 3) {
		usage(argv);
	}

	const char *keychainName = argv[1];
	int nextArg; 
	bool quiet = false;
	OSStatus ortn;
	
	for(nextArg=2; nextArg<argc; ) {
		char *argp = argv[nextArg];
		if(argp[0] != '-') {
			break;
		}
		switch(argp[2]) {
			case 'q':
				quiet = true;
				break;
			default:
				usage(argv);
		}
	}
	if(nextArg == argc) {
		usage(argv);
	}

	/* create an array of SecTrustedApplications */
	CFMutableArrayRef appList = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for(; nextArg<argc; nextArg++) {
		SecTrustedApplicationRef appRef;
		ortn = SecTrustedApplicationCreateFromPath(argv[nextArg], &appRef);
		if(ortn) {
			cssmPerror("SecTrustedApplicationCreateFromPath", ortn);
			exit(1);
		}
		CFArrayAppendValue(appList, appRef);
	}

	/* Find a signing identity; extract its private key */
	SecKeychainRef kcRef;
	ortn = SecKeychainOpen(keychainName, &kcRef);
	if(ortn) {
		cssmPerror("SecKeychainOpen", ortn);
		exit(1);
	}
	SecIdentityRef identRef;
	ortn = sslSimpleIdentPicker(kcRef, &identRef);
	if(ortn) {
		exit(1);
	}
	
	SecKeyRef keyRef;
	ortn = SecIdentityCopyPrivateKey(identRef, &keyRef);
	if(ortn) {
		cssmPerror("SecIdentityCopyPrivateKey", ortn);
		exit(1);
	}
	
	/*
	 * Get existing ACL list (may be empty)
	 */
	SecAccessRef accessRef;
	CFArrayRef aclList = NULL;
	ortn = SecKeychainItemCopyAccess((SecKeychainItemRef)keyRef, &accessRef);
	if(ortn) {
		cssmPerror("SecIdentityCopyPrivateKey", ortn);
		exit(1);
	}
	ortn = SecAccessCopySelectedACLList(accessRef, CSSM_ACL_AUTHORIZATION_DECRYPT, 
		&aclList);
	if(ortn) {
		cssmPerror("SecAccessCopySelectedACLList", ortn);
		exit(1);
	}
	if((aclList == NULL) || (CFArrayGetCount(aclList) == 0)) {
		printf("No ACL list found. I don't know how to set the trusted app list.\n");
		exit(1);
	}
	
	/* append our app list to each ACL's trusted app list */
	for(int aclDex=0; aclDex<CFArrayGetCount(aclList); aclDex++) {
		
		/* get existing app list */
		SecACLRef aclRef = (SecACLRef)CFArrayGetValueAtIndex(aclList, aclDex);
		CFArrayRef existApps = NULL;
		CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR promptSelector;
		CFStringRef promptDescription;
		
		ortn = SecACLCopySimpleContents(aclRef, &existApps, &promptDescription, 
			&promptSelector);
		if(ortn) {
			cssmPerror("SecACLCopySimpleContents", ortn);
			exit(1);
		}
		
		/* appends its contents to our list */
		if(existApps != NULL) {
			for(int i=0; i<CFArrayGetCount(existApps); i++) {
				CFArrayAppendValue(appList, CFArrayGetValueAtIndex(existApps, i));
			}
		}
		
		/* turn off possible keychain prompt flag */
		promptSelector.flags &= ~CSSM_ACL_KEYCHAIN_PROMPT_REQUIRE_PASSPHRASE;
		
		/* Update */
		ortn = SecACLSetSimpleContents(aclRef, appList, promptDescription, 
			&promptSelector);
		if(ortn) {
			cssmPerror("SecACLCopySimpleContents", ortn);
			exit(1);
		}
		if(existApps != NULL) {
			CFRelease(existApps);
		}
	}
	
	/* presumably we're been operating on "the" ACL list in "the" SecAccess,
	 * not a separate copy... */
	ortn =	SecKeychainItemSetAccess((SecKeychainItemRef)keyRef, accessRef);
	if(ortn) {
		cssmPerror("SecKeychainItemSetAccess", ortn);
		exit(1);
	}

	/* is that it? */
	CFRelease(appList);
	CFRelease(kcRef);
	CFRelease(identRef);
	CFRelease(keyRef);
	CFRelease(accessRef);
	CFRelease(aclList);
	if(!quiet) {
		printf("...success\n");
	}
}
