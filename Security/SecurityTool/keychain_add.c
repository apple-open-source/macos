/*
 * Copyright (c) 2003-2009,2011-2014 Apple Inc. All Rights Reserved.
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
 *
 * keychain_add.c
 */

#include "keychain_add.h"
#include "keychain_find.h"
#include "readline_cssm.h"
#include "security_tool.h"
#include "access_utils.h"
#include "keychain_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <AssertMacros.h>
#include <libkern/OSByteOrder.h>
#include <Security/SecAccess.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecTrustedApplication.h>

// SecTrustedApplicationCreateApplicationGroup
#include <Security/SecTrustedApplicationPriv.h>


static int
do_update_generic_password(const char *keychainName,
	 FourCharCode itemCreator,
	 FourCharCode itemType,
	 const char *kind,
	 const char *value,
	 const char *comment,
	 const char *label,
	 const char *serviceName,
	 const char *accountName,
	 const void *passwordData,
	 SecAccessRef access)
{
	OSStatus status;
	SecKeychainRef keychainRef = NULL;
	SecKeychainItemRef itemRef = NULL;

	if (keychainName) {
		keychainRef = keychain_open(keychainName);
	}
	itemRef = find_first_generic_password(keychainRef,itemCreator,itemType,kind,value,comment,label,serviceName,accountName);
	if (keychainRef) {
		CFRelease(keychainRef);
	}
	if (!itemRef) {
		return errSecItemNotFound;
	}

	// build list of attributes
	SecKeychainAttribute attrs[8]; // maximum number of attributes
	SecKeychainAttributeList attrList = { 0, attrs };

	if ((UInt32)itemCreator != 0) {
		attrs[attrList.count].tag = kSecCreatorItemAttr;
		attrs[attrList.count].length = sizeof(FourCharCode);
		attrs[attrList.count].data = (FourCharCode *)&itemCreator;
		attrList.count++;
	}
	if ((UInt32)itemType != 0) {
		attrs[attrList.count].tag = kSecTypeItemAttr;
		attrs[attrList.count].length = sizeof(FourCharCode);
		attrs[attrList.count].data = (FourCharCode *)&itemType;
		attrList.count++;
	}
	if (kind != NULL) {
		attrs[attrList.count].tag = kSecDescriptionItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(kind);
		attrs[attrList.count].data = (void *)kind;
		attrList.count++;
	}
	if (value != NULL) {
		attrs[attrList.count].tag = kSecGenericItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(value);
		attrs[attrList.count].data = (void *)value;
		attrList.count++;
	}
	if (comment != NULL) {
		attrs[attrList.count].tag = kSecCommentItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(comment);
		attrs[attrList.count].data = (void *)comment;
		attrList.count++;
	}
	if (label != NULL) {
		attrs[attrList.count].tag = kSecLabelItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(label);
		attrs[attrList.count].data = (void *)label;
		attrList.count++;
	}
	if (serviceName != NULL) {
		attrs[attrList.count].tag = kSecServiceItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(serviceName);
		attrs[attrList.count].data = (void *)serviceName;
		attrList.count++;
	}
	if (accountName != NULL) {
		attrs[attrList.count].tag = kSecAccountItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(accountName);
		attrs[attrList.count].data = (void *)accountName;
		attrList.count++;
	}

	// modify attributes and password data, if provided
	status = SecKeychainItemModifyContent(itemRef, &attrList, (passwordData) ? ((UInt32) strlen(passwordData)) : 0, passwordData);
	if (status) {
		sec_error("SecKeychainItemModifyContent: %s", sec_errstr(status));
	}

	// modify access, if provided
	if (!status && access) {
		SecAccessRef curAccess = NULL;
		// for historical reasons, we have to modify the item's existing access reference
		// (setting the item's access to a freshly created SecAccessRef instance doesn't behave as expected)
		status = SecKeychainItemCopyAccess(itemRef, &curAccess);
		if (status) {
			sec_error("SecKeychainItemCopyAccess: %s", sec_errstr(status));
		} else {
			int result = merge_access(curAccess, access); // make changes to the existing access reference
			if (result == noErr) {
				status = SecKeychainItemSetAccess(itemRef, curAccess); // will prompt
				if (status) {
					sec_error("SecKeychainItemSetAccess: %s", sec_errstr(status));
				}
			}
		}
		if (curAccess) {
			CFRelease(curAccess);
		}
	}

	CFRelease(itemRef);

	return status;
}

static int
do_add_generic_password(const char *keychainName,
	FourCharCode itemCreator,
	FourCharCode itemType,
	const char *kind,
	const char *value,
	const char *comment,
	const char *label,
	const char *serviceName,
	const char *accountName,
	const void *passwordData,
	SecAccessRef access,
	Boolean update)
{
	SecKeychainRef keychain = NULL;
	OSStatus result = 0;
    SecKeychainItemRef itemRef = NULL;

	// if update flag is specified, try to find and update an existing item
	if (update) {
		result = do_update_generic_password(keychainName,itemCreator,itemType,kind,value,comment,label,serviceName,accountName,passwordData,access);
		if (result == noErr)
			return result;
	}

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto cleanup;
		}
	}

	// set up attribute vector for new item (each attribute consists of {tag, length, pointer})
	SecKeychainAttribute attrs[] = {
		{ kSecLabelItemAttr, label ? (UInt32) strlen(label) : 0, (char *)label },
		{ kSecDescriptionItemAttr, kind ? (UInt32) strlen(kind) : 0, (char *)kind },
		{ kSecGenericItemAttr, value ? (UInt32) strlen(value) : 0, (char *)value },
		{ kSecCommentItemAttr, comment ? (UInt32) strlen(comment) : 0, (char *)comment },
		{ kSecServiceItemAttr, serviceName ? (UInt32) strlen(serviceName) : 0, (char *)serviceName },
		{ kSecAccountItemAttr, accountName ? (UInt32) strlen(accountName) : 0, (char *)accountName },
		{ (SecKeychainAttrType)0, sizeof(FourCharCode), NULL }, /* placeholder */
		{ (SecKeychainAttrType)0, sizeof(FourCharCode), NULL }  /* placeholder */
	};
	SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };
	attributes.count -= 2;

	if (itemCreator != 0)
	{
		attrs[attributes.count].tag = kSecCreatorItemAttr;
		attrs[attributes.count].data = (FourCharCode *)&itemCreator;
		attributes.count++;
	}
	if (itemType != 0)
	{
		attrs[attributes.count].tag = kSecTypeItemAttr;
		attrs[attributes.count].data = (FourCharCode *)&itemType;
		attributes.count++;
	}

	result = SecKeychainItemCreateFromContent(kSecGenericPasswordItemClass,
		&attributes,
		passwordData ? (UInt32) strlen(passwordData) : 0,
		passwordData,
		keychain,
		access,
		&itemRef);

	if (result)
	{
		sec_error("SecKeychainItemCreateFromContent (%s): %s", keychainName ? keychainName : "<default>", sec_errstr(result));
	}

cleanup:
	if (itemRef)
		CFRelease(itemRef);
	if (keychain)
		CFRelease(keychain);

	return result;
}

static int
do_update_internet_password(const char *keychainName,
	 FourCharCode itemCreator,
	 FourCharCode itemType,
	 const char *kind,
	 const char *comment,
	 const char *label,
	 const char *serverName,
	 const char *securityDomain,
	 const char *accountName,
	 const char *path,
	 UInt16 port,
	 SecProtocolType protocol,
	 SecAuthenticationType authenticationType,
	 const void *passwordData,
	 SecAccessRef access)
{
	OSStatus status;
	SecKeychainRef keychainRef = NULL;
	SecKeychainItemRef itemRef = NULL;

	if (keychainName) {
		keychainRef = keychain_open(keychainName);
	}
	itemRef = find_first_internet_password(keychainRef,itemCreator,itemType,kind,comment,label,serverName,
										   securityDomain,accountName,path,port,protocol,authenticationType);
	if (keychainRef) {
		CFRelease(keychainRef);
	}
	if (!itemRef) {
		return errSecItemNotFound;
	}

	// build list of attributes
	SecKeychainAttribute attrs[12]; // maximum number of attributes
	SecKeychainAttributeList attrList = { 0, attrs };

	if ((UInt32)itemCreator != 0) {
		attrs[attrList.count].tag = kSecCreatorItemAttr;
		attrs[attrList.count].length = sizeof(FourCharCode);
		attrs[attrList.count].data = (FourCharCode *)&itemCreator;
		attrList.count++;
	}
	if ((UInt32)itemType != 0) {
		attrs[attrList.count].tag = kSecTypeItemAttr;
		attrs[attrList.count].length = sizeof(FourCharCode);
		attrs[attrList.count].data = (FourCharCode *)&itemType;
		attrList.count++;
	}
	if (kind != NULL) {
		attrs[attrList.count].tag = kSecDescriptionItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(kind);
		attrs[attrList.count].data = (void *)kind;
		attrList.count++;
	}
	if (comment != NULL) {
		attrs[attrList.count].tag = kSecCommentItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(comment);
		attrs[attrList.count].data = (void *)comment;
		attrList.count++;
	}
	if (label != NULL) {
		attrs[attrList.count].tag = kSecLabelItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(label);
		attrs[attrList.count].data = (void *)label;
		attrList.count++;
	}
	if (serverName != NULL) {
		attrs[attrList.count].tag = kSecServerItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(serverName);
		attrs[attrList.count].data = (void *)serverName;
		attrList.count++;
	}
	if (securityDomain != NULL) {
		attrs[attrList.count].tag = kSecSecurityDomainItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(securityDomain);
		attrs[attrList.count].data = (void *)securityDomain;
		attrList.count++;
	}
	if (accountName != NULL) {
		attrs[attrList.count].tag = kSecAccountItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(accountName);
		attrs[attrList.count].data = (void *)accountName;
		attrList.count++;
	}
	if (path != NULL) {
		attrs[attrList.count].tag = kSecPathItemAttr;
		attrs[attrList.count].length = (UInt32) strlen(path);
		attrs[attrList.count].data = (void *)path;
		attrList.count++;
	}
	if (port != 0) {
		attrs[attrList.count].tag = kSecPortItemAttr;
		attrs[attrList.count].length = sizeof(UInt16);
		attrs[attrList.count].data = (UInt16 *)&port;
		attrList.count++;
	}
	if ((UInt32)protocol != 0) {
		attrs[attrList.count].tag = kSecProtocolItemAttr;
		attrs[attrList.count].length = sizeof(SecProtocolType);
		attrs[attrList.count].data = (SecProtocolType *)&protocol;
		attrList.count++;
	}
	if ((UInt32)authenticationType != 0) {
		attrs[attrList.count].tag = kSecAuthenticationTypeItemAttr;
		attrs[attrList.count].length = sizeof(SecAuthenticationType);
		attrs[attrList.count].data = (SecAuthenticationType *)&authenticationType;
		attrList.count++;
	}

	// modify attributes and password data, if provided
	status = SecKeychainItemModifyContent(itemRef, &attrList, (passwordData) ? (UInt32) strlen(passwordData) : 0, passwordData);
	if (status) {
		sec_error("SecKeychainItemModifyContent: %s", sec_errstr(status));
	}

	// modify access, if provided
	if (!status && access) {
		status = modify_access(itemRef, access);
	}

	CFRelease(itemRef);

	return status;
}

static int
do_add_internet_password(const char *keychainName,
	FourCharCode itemCreator,
	FourCharCode itemType,
	const char *kind,
	const char *comment,
	const char *label,
	const char *serverName,
	const char *securityDomain,
	const char *accountName,
	const char *path,
	UInt16 port,
	SecProtocolType protocol,
	SecAuthenticationType authenticationType,
	const void *passwordData,
	SecAccessRef access,
	Boolean update)
{
	SecKeychainRef keychain = NULL;
    SecKeychainItemRef itemRef = NULL;
	OSStatus result = 0;

	// if update flag is specified, try to find and update an existing item
	if (update) {
		result = do_update_internet_password(keychainName,itemCreator,itemType,kind,comment,label,serverName,
											 securityDomain,accountName,path,port,protocol,authenticationType,
											 passwordData,access);
		if (result == noErr)
			return result;
	}

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto cleanup;
		}
	}

	// set up attribute vector for new item (each attribute consists of {tag, length, pointer})
	SecKeychainAttribute attrs[] = {
		{ kSecLabelItemAttr, label ? (UInt32) strlen(label) : 0, (char *)label },
		{ kSecDescriptionItemAttr, kind ? (UInt32) strlen(kind) : 0, (char *)kind },
		{ kSecCommentItemAttr, comment ? (UInt32) strlen(comment) : 0, (char *)comment },
		{ kSecServerItemAttr, serverName ? (UInt32) strlen(serverName) : 0, (char *)serverName },
		{ kSecSecurityDomainItemAttr, securityDomain ? (UInt32) strlen(securityDomain) : 0, (char *)securityDomain },
		{ kSecAccountItemAttr, accountName ? (UInt32) strlen(accountName) : 0, (char *)accountName },
		{ kSecPathItemAttr, path ? (UInt32) strlen(path) : 0, (char *)path },
		{ kSecPortItemAttr, sizeof(UInt16), (UInt16 *)&port },
		{ kSecProtocolItemAttr, sizeof(SecProtocolType), (SecProtocolType *)&protocol },
		{ kSecAuthenticationTypeItemAttr, sizeof(SecAuthenticationType), (SecAuthenticationType *)&authenticationType },
		{ (SecKeychainAttrType)0, sizeof(FourCharCode), NULL }, /* placeholder */
		{ (SecKeychainAttrType)0, sizeof(FourCharCode), NULL }  /* placeholder */
	};
	SecKeychainAttributeList attributes = { sizeof(attrs) / sizeof(attrs[0]), attrs };
	attributes.count -= 2;

	if (itemCreator != 0)
	{
		attrs[attributes.count].tag = kSecCreatorItemAttr;
		attrs[attributes.count].data = (FourCharCode *)&itemCreator;
		attributes.count++;
	}
	if (itemType != 0)
	{
		attrs[attributes.count].tag = kSecTypeItemAttr;
		attrs[attributes.count].data = (FourCharCode *)&itemType;
		attributes.count++;
	}

	result = SecKeychainItemCreateFromContent(kSecInternetPasswordItemClass,
		&attributes,
		passwordData ? (UInt32) strlen(passwordData) : 0,
		passwordData,
		keychain,
		access,
		&itemRef);

	if (result)
	{
		sec_error("SecKeychainAddInternetPassword %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
	}

cleanup:
	if (itemRef)
		CFRelease(itemRef);
	if (keychain)
		CFRelease(keychain);

	return result;
}

static int
do_add_certificates(const char *keychainName, int argc, char * const *argv)
{
	SecKeychainRef keychain = NULL;
	int ix, result = 0;

	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto cleanup;
		}
	}

	for (ix = 0; ix < argc; ++ix)
	{
		CSSM_DATA certData = {};
		OSStatus status;
		SecCertificateRef certificate = NULL;

		if (read_file(argv[ix], &certData))
		{
			result = 1;
			continue;
		}

		status = SecCertificateCreateFromData(&certData, CSSM_CERT_X_509v3, CSSM_CERT_ENCODING_UNKNOWN, &certificate);
		if (status)
		{
			sec_perror("SecCertificateCreateFromData", status);
			result = 1;
		}
		else
		{
			status = SecCertificateAddToKeychain(certificate, keychain);
			if (status)
			{
                if (status == errSecDuplicateItem)
                {
                    if (keychainName)
                        sec_error("%s: already in %s", argv[ix], keychainName);
                    else
                        sec_error("%s: already in default keychain", argv[ix]);
                }
                else
                {
                    sec_perror("SecCertificateAddToKeychain", status);
                }
				result = 1;
			}
		}

		if (certData.Data)
			free(certData.Data);
		if (certificate)
			CFRelease(certificate);
	}

cleanup:
	if (keychain)
		CFRelease(keychain);

	return result;
}

static bool promptForPasswordData(char **returnedPasswordData) {
    // Caller must zero memory and free returned value.
    // Returns true if we have data; false means the user cancelled
    if (!returnedPasswordData)
        return false;

    bool result = false;
    char *password = NULL;
    int tries;

    for (tries = 3; tries-- > 0;) {
        bool passwordsMatch = false;
        char *firstpass = NULL;

        password = getpass("password data for new item: ");
        if (!password)
            break;

        firstpass = malloc(strlen(password) + 1);
        strcpy(firstpass, password);
        password = getpass("retype password for new item: ");
        passwordsMatch = password && (strcmp(password, firstpass) == 0);
        memset(firstpass, 0, strlen(firstpass));
        free(firstpass);
        if (!password)
            break;

        if (passwordsMatch) {
            result = true;
            break;
        }

        fprintf(stderr, "passwords don't match\n");
        memset(password, 0, strlen(password));
    }

    if (result) {
        *returnedPasswordData = password;
    } else {
        free(password);
    }
    return result;
}

int
keychain_add_generic_password(int argc, char * const *argv)
{
	char *serviceName = NULL, *passwordData  = NULL, *accountName = NULL;
	char *kind = NULL, *label = NULL, *value = NULL, *comment = NULL;
	FourCharCode itemCreator = 0, itemType = 0;
	int ch, result = 0;
	const char *keychainName = NULL;
	Boolean access_specified = FALSE;
	Boolean always_allow = FALSE;
	Boolean update_item = FALSE;
	SecAccessRef access = NULL;
	CFMutableArrayRef trusted_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	OSStatus status;
    bool mustFreePasswordData = false; // if we got it via user prompting

  /*
   *  "    -a  Specify account name (required)\n"
   *  "    -c  Specify item creator (optional four-character code)\n"
   *  "    -C  Specify item type (optional four-character code)\n"
   *  "    -D  Specify kind (default is \"application password\")\n"
   *  "    -G  Specify generic attribute (optional)\n"
   *  "    -j  Specify comment string (optional)\n"
   *  "    -l  Specify label (if omitted, service name is used as default label)\n"
   *  "    -s  Specify service name (required)\n"
   *  "    -p  Specify password to be added (legacy option, equivalent to -w)\n"
   *  "    -w  Specify password to be added\n"
   *  "    -A  Allow any application to access this item without warning (insecure, not recommended!)\n"
   *  "    -T  Specify an application which may access this item (multiple -T options are allowed)\n"
   *  "    -U  Update item if it already exists (if omitted, the item cannot already exist)\n"
   */

	while ((ch = getopt(argc, argv, ":a:c:C:D:G:j:l:s:p:w:UAT:")) != -1)
	{
		switch  (ch)
		{
        case 'a':
            accountName = optarg;
			break;
		case 'c':
			result = parse_fourcharcode(optarg, &itemCreator);
			if (result) goto cleanup;
			break;
		case 'C':
			result = parse_fourcharcode(optarg, &itemType);
			if (result) goto cleanup;
			break;
        case 'D':
            kind = optarg;
			break;
		case 'G':
			value = optarg;
			break;
		case 'j':
			comment = optarg;
			break;
		case 'l':
			label = optarg;
			break;
		case 's':
			serviceName = optarg;
			break;
        case 'p':
        case 'w':
            passwordData = optarg;
            break;
		case 'U':
			update_item = TRUE;
			break;
		case 'A':
			always_allow = TRUE;
			access_specified = TRUE;
			break;
		case 'T':
		{
			if (optarg[0])
			{
				SecTrustedApplicationRef app = NULL;
				/* check whether the argument specifies an application group */
				const char *groupPrefix = "group://";
				size_t prefixLen = strlen(groupPrefix);
				if (strlen(optarg) > prefixLen && !memcmp(optarg, groupPrefix, prefixLen)) {
					const char *groupName = &optarg[prefixLen];
					if ((status = SecTrustedApplicationCreateApplicationGroup(groupName, NULL, &app)) != noErr) {
						sec_error("SecTrustedApplicationCreateApplicationGroup %s: %s", optarg, sec_errstr(status));
					}
				} else {
					if ((status = SecTrustedApplicationCreateFromPath(optarg, &app)) != noErr) {
						sec_error("SecTrustedApplicationCreateFromPath %s: %s", optarg, sec_errstr(status));
					}
				}

				if (status) {
					result = 1;
					goto cleanup;
				}

				CFArrayAppendValue(trusted_list, app);
				CFRelease(app);
			}
			access_specified = TRUE;
			break;
		}
        case '?':
        case ':':
            // They supplied the -p or -w but with no data, so prompt
            // This differs from the case where no -p or -w was given, where we set the data to empty
            if (optopt == 'p' || optopt == 'w') {
                if (promptForPasswordData(&passwordData)) {
                    mustFreePasswordData = true;
                    break;
                } else {
                    result = 1;
                    goto cleanup; /* @@@ Do not trigger usage message, but indicate failure. */
                }
            }
            result = 2;
            goto cleanup; /* @@@ Return 2 triggers usage message. */
		default:
			result = 2;
			goto cleanup; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (!accountName || !serviceName)
	{
		result = 2;
		goto cleanup;
	}
	else if (argc > 0)
	{
		keychainName = argv[0];
		if (argc > 1 || *keychainName == '\0')
		{
			result = 2;
			goto cleanup;
		}
	}

	if (access_specified)
	{
		const char *accessName = (label) ? label : (serviceName) ? serviceName : (accountName) ? accountName : "";
		if ((result = create_access(accessName, always_allow, trusted_list, &access)) != 0)
			goto cleanup;
	}

	result = do_add_generic_password(keychainName,
									 itemCreator,
									 itemType,
									 kind,
									 value,
									 comment,
									 (label) ? label : serviceName,
									 serviceName,
									 accountName,
									 passwordData,
									 access,
									 update_item);

cleanup:
    if (mustFreePasswordData)
        free(passwordData);
	if (trusted_list)
		CFRelease(trusted_list);
	if (access)
		CFRelease(access);

	return result;
}

int
keychain_add_internet_password(int argc, char * const *argv)
{
	char *serverName = NULL, *securityDomain = NULL, *accountName = NULL, *path = NULL, *passwordData = NULL;
	char *kind = NULL, *comment = NULL, *label = NULL;
	FourCharCode itemCreator = 0, itemType = 0;
	UInt16 port = 0;
	SecProtocolType protocol = 0;
	SecAuthenticationType authenticationType = OSSwapHostToBigInt32('dflt');
	int ch, result = 0;
	const char *keychainName = NULL;
	Boolean access_specified = FALSE;
	Boolean always_allow = FALSE;
	Boolean update_item = FALSE;
	SecAccessRef access = NULL;
	CFMutableArrayRef trusted_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	OSStatus status;
    bool mustFreePasswordData = false; // if we got it via user prompting

  /*
   *  "    -a  Specify account name (required)\n"
   *  "    -c  Specify item creator (optional four-character code)\n"
   *  "    -C  Specify item type (optional four-character code)\n"
   *  "    -d  Specify security domain string (optional)\n"
   *  "    -D  Specify kind (default is \"Internet password\")\n"
   *  "    -j  Specify comment string (optional)\n"
   *  "    -l  Specify label (if omitted, server name is used as default label)\n"
   *  "    -p  Specify path string (optional)\n"
   *  "    -P  Specify port number (optional)\n"
   *  "    -r  Specify protocol (optional four-character SecProtocolType, e.g. \"http\", \"ftp \")\n"
   *  "    -s  Specify server name (required)\n"
   *  "    -t  Specify authentication type (as a four-character SecAuthenticationType, default is \"dflt\")\n"
   *  "    -w  Specify password to be added\n"
   *  "    -A  Allow any application to access this item without warning (insecure, not recommended!)\n"
   *  "    -T  Specify an application which may access this item (multiple -T options are allowed)\n"
   *  "    -U  Update item if it already exists (if omitted, the item cannot already exist)\n"
   */

	while ((ch = getopt(argc, argv, ":a:c:C:d:D:j:l:p:P:r:s:t:w:UAT:h")) != -1)
	{
		switch  (ch)
		{
		case 'a':
			accountName = optarg;
			break;
		case 'c':
			result = parse_fourcharcode(optarg, &itemCreator);
			if (result) goto cleanup;
			break;
		case 'C':
			result = parse_fourcharcode(optarg, &itemType);
			if (result) goto cleanup;
			break;
		case 'd':
			securityDomain = optarg;
			break;
        case 'D':
            kind = optarg;
			break;
		case 'j':
			comment = optarg;
			break;
		case 'l':
			label = optarg;
			break;
		case 'p':
			path = optarg;
			break;
		case 'P':
			port = atoi(optarg);
			break;
		case 'r':
			result = parse_fourcharcode(optarg, &protocol);
			if (result) goto cleanup;
			break;
		case 's':
			serverName = optarg;
			break;
		case 't':
			result = parse_fourcharcode(optarg, &authenticationType);
			if (result) goto cleanup;
			/* auth type attribute is special */
			authenticationType = OSSwapHostToBigInt32(authenticationType);
			break;
		case 'w':
			passwordData = optarg;
			break;
		case 'U':
			update_item = TRUE;
			break;
		case 'A':
			always_allow = TRUE;
			access_specified = TRUE;
			break;
		case 'T':
		{
			if (optarg[0])
			{
				SecTrustedApplicationRef app = NULL;
				/* check whether the argument specifies an application group */
				const char *groupPrefix = "group://";
				size_t prefixLen = strlen(groupPrefix);
				if (strlen(optarg) > prefixLen && !memcmp(optarg, groupPrefix, prefixLen)) {
					const char *groupName = &optarg[prefixLen];
					if ((status = SecTrustedApplicationCreateApplicationGroup(groupName, NULL, &app)) != noErr) {
						sec_error("SecTrustedApplicationCreateApplicationGroup %s: %s", optarg, sec_errstr(status));
					}
				} else {
					if ((status = SecTrustedApplicationCreateFromPath(optarg, &app)) != noErr) {
						sec_error("SecTrustedApplicationCreateFromPath %s: %s", optarg, sec_errstr(status));
					}
				}

				if (status) {
					result = 1;
					goto cleanup;
				}

				CFArrayAppendValue(trusted_list, app);
				CFRelease(app);
			}
			access_specified = TRUE;
			break;
		}
		case '?':
        case ':':
            // They supplied the -p or -w but with no data, so prompt
            // This differs from the case where no -p or -w was given, where we set the data to empty
            if (optopt == 'p' || optopt == 'w') {
                if (promptForPasswordData(&passwordData)) {
                    mustFreePasswordData = true;
                    break;
                } else {
                    result = 1;
                    goto cleanup; /* @@@ Do not trigger usage message, but indicate failure. */
                }
            }
            result = 2;
            goto cleanup; /* @@@ Return 2 triggers usage message. */

		default:
			result = 2;
			goto cleanup; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (!accountName || !serverName)
	{
		result = 2;
		goto cleanup;
	}
	else if (argc > 0)
	{
		keychainName = argv[0];
		if (argc > 1 || *keychainName == '\0')
		{
			result = 2;
			goto cleanup;
		}
	}

	if (access_specified)
	{
		const char *accessName = (label) ? label : (serverName) ? serverName : (accountName) ? accountName : "";
		if ((result = create_access(accessName, always_allow, trusted_list, &access)) != 0)
			goto cleanup;
	}

	result = do_add_internet_password(keychainName,
									  itemCreator,
									  itemType,
									  kind,
									  comment,
									  (label) ? label : serverName,
									  serverName,
									  securityDomain,
									  accountName,
									  path,
									  port,
									  protocol,
									  authenticationType,
									  passwordData,
									  access,
									  update_item);

cleanup:
    if (mustFreePasswordData)
        free(passwordData);
	if (trusted_list)
		CFRelease(trusted_list);
	if (access)
		CFRelease(access);

	return result;
}

int
keychain_add_certificates(int argc, char * const *argv)
{
	int ch, result = 0;
	const char *keychainName = NULL;
	while ((ch = getopt(argc, argv, "hk:")) != -1)
	{
		switch  (ch)
		{
        case 'k':
            keychainName = optarg;
			if (*keychainName == '\0')
				return 2;
            break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return 2;

	result = do_add_certificates(keychainName, argc, argv);

	return result;
}
