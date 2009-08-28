/* 
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include <Security/Security.h>
#include <err.h>
#include <stdio.h>
#include <unistd.h>

static CFArrayRef
getTrustedApps(const char *trustedApp)
{
    SecTrustedApplicationRef app;
    void *values[1];
    OSStatus ret;
    
    /*
     * Create ACL, XXX should add acl, not modify ACL.
     */

    ret = SecTrustedApplicationCreateFromPath(trustedApp, &app);
    if (ret)
	errx(1, "SecTrustedApplicationCreateFromPath");
    
    values[0] = app;
    
    return CFArrayCreate(NULL,  (const void **)values,
			 sizeof(values)/sizeof(values[0]), NULL);
}



static SecKeychainItemRef
findKeychainItem(SecKeychainRef keychain, const char *label)
{
    SecKeychainItemRef itemRef;
    OSStatus ret;

    SecKeychainSearchRef search;
    SecKeychainAttribute attributes[1];
    SecKeychainAttributeList list;
    
    attributes[0].tag = kSecLabelItemAttr;
    attributes[0].data = (char *)(uintptr_t)label;
    attributes[0].length = strlen(label);
    
    list.count = 1;
    list.attr = attributes;
    
    /* Should only search the System keychain ! */
    ret = SecKeychainSearchCreateFromAttributes(keychain,
						CSSM_DL_DB_RECORD_PRIVATE_KEY,
						&list,
						&search);
    if (ret)
	errx(1, "failure in SecKeychainSearchCreateFromAttributes");
    
    ret = SecKeychainSearchCopyNext(search, &itemRef);
    CFRelease(search);
    if (ret == errSecItemNotFound)
	errx(1, "didn't find private key for: %s", label);
    else if (ret)
	errx(1, "Failure in SecKeychainSearchCopyNext: %d for %s", 
	     ret, label);

    return itemRef;
}	

static void
testSign(SecKeychainItemRef itemRef)
{
    const CSSM_ACCESS_CREDENTIALS *creds;
    CSSM_CC_HANDLE sigHandle = 0;
    CSSM_CSP_HANDLE cspHandle;
    const CSSM_KEY *cssmKey;
    uint8_t to[4096];
    CSSM_DATA sig, in;
    CSSM_RETURN cret;
    SecKeyRef privKeyRef = (SecKeyRef)itemRef;
    OSStatus ret;
    
    
    cret = SecKeyGetCSSMKey(privKeyRef, &cssmKey);
    if(cret) abort();
    
    cret = SecKeyGetCSPHandle(privKeyRef, &cspHandle);
    if(cret) abort();
    
    ret = SecKeyGetCredentials(privKeyRef, CSSM_ACL_AUTHORIZATION_SIGN,
			       kSecCredentialTypeDefault, &creds);
    if(ret) abort();
    
    ret = CSSM_CSP_CreateSignatureContext(cspHandle, CSSM_ALGID_RSA,
					  creds, cssmKey, &sigHandle);
    if(ret) abort();
    
    in.Data = (uint8 *)"test signature";
    in.Length = strlen((char *)in.Data);
    
    sig.Data = to;
    sig.Length = sizeof(to);
    
    cret = CSSM_SignData(sigHandle, &in, 1, CSSM_ALGID_NONE, &sig);
    if(cret)
	errx(1, "failed to sign");
    
    CSSM_DeleteContext(sigHandle);
    
    printf("signing worked\n");
}

static int
checkOnACLAlreadyAndRemoveAny(SecKeychainItemRef itemRef, 
			      const char *trustedAppName)
{
    SecAccessRef secaccess;
    CFArrayRef aclList;
    OSStatus ret;
    SecACLRef acl;
    int modified = 0, found = 0;
    uint32 i, j;
    
    ret = SecKeychainItemCopyAccess(itemRef, &secaccess);
    if (ret)
	errx(1, "SecKeychainItemCopyAccess");

    ret = SecAccessCopyACLList (secaccess, &aclList);
    if (ret)
	errx(1, "SecAccessCopyACLList");
    
    for (i = 0; i < CFArrayGetCount(aclList) && !found; i++) {
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR prompt;
	CFStringRef description;
	CFArrayRef apps;
	
	acl = (SecACLRef)CFArrayGetValueAtIndex(aclList, i);
	
	/*
	 * XXX SecACLCopySimpleContents failes on "complex"
	 * entries, what is that and should I care ?
	 */
	
	ret = SecACLCopySimpleContents(acl, &apps, &description, &prompt);
	if (ret) {
	    /* warnx("SecACLCopySimpleContents: %d: %d", i, (int)ret); */
	    continue;
	}
	
	CFRelease(description);
	
	if (apps == NULL) {
	    SecACLRemove(acl);
	    modified = 1;
	    printf("removing any access\n");
	} else {
	    for (j = 0; j < CFArrayGetCount(apps) && !found; j++) {
		SecTrustedApplicationRef app;
		CFDataRef data;
		
		app = (SecTrustedApplicationRef)CFArrayGetValueAtIndex(apps, j);
		
		ret = SecTrustedApplicationCopyData(app, &data);
		if (ret)
		    errx(1, "SecTrustedApplicationCopyData");
		
		/* http://lists.apple.com/archives/apple-cdsa/2007/Dec/msg00021.html */
		if (strcmp(trustedAppName, (char *)CFDataGetBytePtr(data)) == 0)
		    found = 1;
		
		CFRelease(data);
	    }
	    CFRelease(apps);
	}
    }

    if (modified) {
	ret = SecKeychainItemSetAccess(itemRef, secaccess);
	if (ret)
	    errx(1, "SecKeychainItemSetAccess: %d (any access)", ret);
    }

    CFRelease(aclList);
    CFRelease(secaccess);

    return found;
}

static void
addTrustedApplication(SecKeychainItemRef itemRef,
		      const char *trustedAppName,
		      const char *identity)
{
    CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR prompt;
    CSSM_ACL_AUTHORIZATION_TAG tags[6];
    CFArrayRef trustedApps;
    SecAccessRef secaccess;
    SecACLRef acl;	
    OSStatus ret;
    
    /*
     * Check if we are already on the acl list
     */

    if (checkOnACLAlreadyAndRemoveAny(itemRef, trustedAppName)) {
	printf("%s already in acl\n", trustedAppName);
	return;
    }
    
    /*
     * Add new acl entry for our trusted app
     */
    
    ret = SecKeychainItemCopyAccess(itemRef, &secaccess);
    if (ret)
	errx(1, "SecKeychainItemCopyAccess");
    
    prompt.version = CSSM_ACL_KEYCHAIN_PROMPT_CURRENT_VERSION;
    prompt.flags = 0;
    
    trustedApps = getTrustedApps(trustedAppName);
    if (trustedApps == NULL)
	errx(1, "getTrustedApps");
    
    ret = SecACLCreateFromSimpleContents(secaccess, trustedApps, 
					 CFSTR("lkdc-acl"), &prompt,
					 &acl);
    if (ret)
	errx(1, "SecACLCreateFromSimpleContents");
    
    tags[0] = CSSM_ACL_AUTHORIZATION_DECRYPT;
    tags[1] = CSSM_ACL_AUTHORIZATION_DERIVE;
    tags[2] = CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR;
    tags[3] = CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED;
    tags[4] = CSSM_ACL_AUTHORIZATION_MAC;
    tags[5] = CSSM_ACL_AUTHORIZATION_SIGN;
    
    ret = SecACLSetAuthorizations(acl, tags, sizeof(tags)/sizeof(tags[0]));
    if (ret)
	errx(1, "SecACLSetAuthorizations");
    
    ret = SecKeychainItemSetAccess(itemRef, secaccess);
    if (ret)
	errx(1, "SecKeychainItemSetAccess: %d", ret);
    
    printf("added %s to acl for %s\n", trustedAppName, identity);

    CFRelease(secaccess);
}

static void
deleteAllACL(SecKeychainItemRef itemRef)
{
    SecAccessRef secaccess;
    CFArrayRef aclList;
    OSStatus ret;
    CFIndex i;
    
    ret = SecKeychainItemCopyAccess(itemRef, &secaccess);
    if (ret)
	errx(1, "deleteAllACL: SecKeychainItemCopyAccess: %d", ret);
    
    ret = SecAccessCopyACLList (secaccess, &aclList);
    if (ret)
	errx(1, "deleteAllACL: SecAccessCopyACLList %d", ret);
    
    for (i = 0; i < CFArrayGetCount(aclList); i++) {
	CSSM_ACL_AUTHORIZATION_TAG tags[30];
	uint32 j, tagcount = sizeof(tags)/sizeof(tags[0]);
	SecACLRef acl;
	
	acl = (SecACLRef)CFArrayGetValueAtIndex(aclList, i);
	
	ret = SecACLGetAuthorizations(acl, tags, &tagcount);
	if (ret)
	    errx(1, "deleteAllACL: SecACLGetAuthorizations: %d", ret);
	
	for (j = 0; j < tagcount; j++) {
	    /* skip owner */
	    if (tags[j] == CSSM_ACL_AUTHORIZATION_CHANGE_ACL)
		break;
	}
	if (j < tagcount)
	    continue;
	
	ret = SecACLRemove(acl);
	if (ret)
	    errx(1, "deleteAllACL: SecACLRemove: %d", ret);
    }
    
    ret = SecKeychainItemSetAccess(itemRef, secaccess);
    if (ret)
	errx(1, "deleteAllACL: SecKeychainItemSetAccess: %d", ret);

    CFRelease(secaccess);
}


static void
listACL(SecKeychainItemRef itemRef)
{
    SecAccessRef secaccess;
    CFArrayRef aclList;
    SecACLRef acl;
    OSStatus ret;
    uint32 i, j;
    
    ret = SecKeychainItemCopyAccess(itemRef, &secaccess);
    if (ret)
	errx(1, "SecKeychainItemCopyAccess");

    ret = SecAccessCopyACLList (secaccess, &aclList);
    if (ret)
	errx(1, "SecAccessCopyACLList");
    
    for (i = 0; i < CFArrayGetCount(aclList); i++) {
	CSSM_ACL_KEYCHAIN_PROMPT_SELECTOR prompt;
	CFStringRef description;
	CFArrayRef apps;
	
	acl = (SecACLRef)CFArrayGetValueAtIndex(aclList, i);
	
	printf("acl: %lu\n", (unsigned long)i);

	/*
	 * XXX SecACLCopySimpleContents failes on "complex"
	 * entries, what is that and should I care ?
	 */
	
	ret = SecACLCopySimpleContents(acl, &apps, &description, &prompt);
	if (ret) {
	    /* warnx("SecACLCopySimpleContents: %d: %d", i, (int)ret); */
	    continue;
	}
	
	if (description) {
	    size_t len;
	    char *str;

	    len = CFStringGetMaximumSizeForEncoding(
		CFStringGetLength(description), kCFStringEncodingUTF8);
	    len += 1;

	    str = malloc(len);
	    if (str == NULL)
		errx(1, "out of memory");

	    CFStringGetCString(description, str, len, kCFStringEncodingUTF8);
	    printf("\tdescription: %s\n",  str);
	    free(str);
	    CFRelease(description);
	}
	
	{
	    CSSM_ACL_AUTHORIZATION_TAG tags[30];
	    uint32 k, tagcount = sizeof(tags)/sizeof(tags[0]);
	    
	    ret = SecACLGetAuthorizations(acl, tags, &tagcount);
	    if (ret)
		errx(1, "listAcl: SecACLGetAuthorizations: %d", ret);
	
	    printf("\ttags: ");
	    for (k = 0; k < tagcount; k++)
		printf("%d ", tags[k]);
	    printf("\n");

	}
	
	if (apps == NULL) {
	    printf("\tany ACL\n");
	} else {
	    for (j = 0; j < CFArrayGetCount(apps); j++) {
		SecTrustedApplicationRef app;
		CFDataRef data;
		
		app = (SecTrustedApplicationRef)CFArrayGetValueAtIndex(apps, j);
		
		ret = SecTrustedApplicationCopyData(app, &data);
		if (ret)
		    errx(1, "SecTrustedApplicationCopyData");
		
		/* http://lists.apple.com/archives/apple-cdsa/2007/Dec/msg00021.html */
		printf("\tapp: %s\n", (char *)CFDataGetBytePtr(data));
		CFRelease(data);
	    }
	    CFRelease(apps);
	}
    }

    CFRelease(aclList);
    CFRelease(secaccess);
}

static void
usage(int exit_code)
{
    printf("%s -s identity -t\t\t\ttest using identitys key\n",
	   getprogname());
    printf("%s -s identity -a application\tadd appliction to acl\n", 
	   getprogname());
    printf("%s -s identity -D\t\t\tdelete all ACL\n", 
	   getprogname());
    printf("%s -s identity -l\t\t\tlist ACL\n", 
	   getprogname());
    exit(exit_code);
}

extern int optind;
extern char *optarg;

int
main(int argc, char **argv)
{
    SecKeychainItemRef itemRef;
    SecKeychainRef keychain = NULL;
    int ch, test_sign = 0, delete_all = 0, do_list = 0;
    const char *trustedAppName = NULL;
    const char *identity = NULL;

    setprogname(argv[0]);

    while ((ch = getopt(argc, argv, "Da:s:tl?h")) != -1) {
	switch(ch) {
	case 'D':
	    delete_all = 1;
	    break;
	case 'a':
	    trustedAppName = optarg;
	    break;
	case 'l':
	    do_list = 1;
	    break;
	case 's':
	    identity = optarg;
	    break;
	case 't':
	    test_sign = 1;
	    break;
	case '?':
	case 'h':
	    usage(0);
	    break;
	}
    }
    
    if (identity == NULL) {
	fprintf(stderr, "no identity given\n");
	usage(1);
    }

    SecKeychainSetUserInteractionAllowed(FALSE);

    itemRef = findKeychainItem(keychain, identity);

    if (test_sign)
	testSign(itemRef);

    if (delete_all)
	deleteAllACL(itemRef);

    if (trustedAppName)
	addTrustedApplication(itemRef, trustedAppName, identity);

    if (do_list)
	listACL(itemRef);

    return 0;
}
