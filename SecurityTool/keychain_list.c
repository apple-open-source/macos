/*
 * Copyright (c) 2003-2009 Apple Inc. All Rights Reserved.
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
 * keychain_list.c
 */

#include "keychain_list.h"

#include "keychain_utilities.h"
#include "security.h"

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <CoreFoundation/CFArray.h>
#include <Security/SecKeychain.h>

// SecKeychainCopyLogin
#include <Security/SecKeychainPriv.h>


typedef enum
{
	kList,
	kAdd,
	kRemove,
	kSet,
} list_operation;

static void
display_name(const void *value, void *context)
{
	SecKeychainRef keychain = (SecKeychainRef)value;
	UInt32 pathLength = MAXPATHLEN;
	char pathName[MAXPATHLEN + 1];
	OSStatus result = SecKeychainGetPath(keychain, &pathLength, pathName);
	if (result)
		sec_error("SecKeychainGetPath %p: %s", keychain, sec_errstr(result));
	else
		fprintf(stdout, "    \"%*s\"\n", (int)pathLength, pathName);
}


static void
display_list(const char *desc, CFTypeRef keychainOrArray)
{
	if (desc && strlen(desc))
		fprintf(stdout, "%s\n", desc);

	if (!keychainOrArray)
	{
		fprintf(stdout, " <NULL>\n");
	}
	else if (CFGetTypeID(keychainOrArray) == SecKeychainGetTypeID())
	{
		display_name(keychainOrArray, NULL);
	}
	else
	{
		CFArrayRef array = (CFArrayRef)keychainOrArray;
		CFRange range = { 0, CFArrayGetCount(array) };
		CFArrayApplyFunction(array, range, display_name, NULL);
	}
}

static int
parse_domain(const char *name, SecPreferencesDomain *domain)
{
	size_t len = strlen(name);

	if (!strncmp("user", name, len))
		*domain = kSecPreferencesDomainUser;
	else if (!strncmp("system", name, len))
		*domain = kSecPreferencesDomainSystem;
	else if (!strncmp("common", name, len))
		*domain = kSecPreferencesDomainCommon;
	else if (!strncmp("dynamic", name, len))
		*domain = kSecPreferencesDomainDynamic;
	else
	{
		sec_error("Invalid domain: %s", name);
		return 2;
	}

	return 0;
}

const char *
domain2str(SecPreferencesDomain domain)
{
	switch (domain)
	{
	case kSecPreferencesDomainUser:
		return "user";
	case kSecPreferencesDomainSystem:
		return "system";
	case kSecPreferencesDomainCommon:
		return "common";
	case kSecPreferencesDomainDynamic:
		return "dynamic";
	default:
		return "unknown";
	}
}

int
keychain_list(int argc, char * const *argv)
{
	CFTypeRef keychainOrArray = NULL;
	CFArrayRef searchList = NULL;
	list_operation operation = kList;
	SecPreferencesDomain domain = kSecPreferencesDomainUser;
	Boolean use_domain = false;
	int ch, result = 0;
	OSStatus status;

	while ((ch = getopt(argc, argv, "d:hs")) != -1)
	{
		switch  (ch)
		{
		case 'd':
			result = parse_domain(optarg, &domain);
			if (result)
				return result;
			use_domain = true;
			break;
		case 's':
			operation = kSet;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	switch (operation)
	{
	case kAdd:
		result = 1;
		break;
	case kRemove:
		result = 1;
		break;
	case kList:
		if (argc > 0)
			result = 2; // Show usage
		else
		{
			if (use_domain)
			{
				status = SecKeychainCopyDomainSearchList(domain, &searchList);
				if (status)
				{
					sec_error("SecKeychainCopyDomainSearchList %s: %s", domain2str(domain), sec_errstr(status));
					result = 1;
				}
				else
				{
#if 0
					fprintf(stdout, "%s search list: ", domain2str(domain));
#endif
					display_list("", searchList);
				}
			}
			else
			{
				status = SecKeychainCopySearchList(&searchList);
				if (status)
				{
					sec_perror("SecKeychainCopySearchList", status);
					result = 1;
				}
				else
				{
#if 0
					display_list("search list:", searchList);
#else
					display_list("", searchList);
#endif
				}
			}
		}
		break;
	case kSet:
		keychainOrArray = keychain_create_array(argc, argv);
		if (argc == 0)
			searchList = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
		else if (argc == 1)
			searchList = CFArrayCreate(NULL, &keychainOrArray, 1, &kCFTypeArrayCallBacks);
		else
			searchList = (CFArrayRef)CFRetain(keychainOrArray);

		if (use_domain)
		{
			status = SecKeychainSetDomainSearchList(domain, searchList);
			if (status)
			{
				sec_error("SecKeychainSetDomainSearchList %s: %s", domain2str(domain), sec_errstr(status));
				result = 1;
			}
		}
		else
		{
			status = SecKeychainSetSearchList(searchList);
			if (status)
			{
				sec_perror("SecKeychainSetSearchList", status);
				result = 1;
			}
		}
		break;
	}

	if (keychainOrArray)
		CFRelease(keychainOrArray);
	if (searchList)
		CFRelease(searchList);

	return result;
}

int
keychain_default(int argc, char * const *argv)
{
	SecPreferencesDomain domain = kSecPreferencesDomainUser;
	SecKeychainRef keychain = NULL;
	Boolean use_domain = false;
	Boolean do_set = false;
	int ch, result = 0;
	OSStatus status;

	while ((ch = getopt(argc, argv, "d:hs")) != -1)
	{
		switch  (ch)
		{
		case 'd':
			result = parse_domain(optarg, &domain);
			if (result)
				return result;
			use_domain = true;
			break;
		case 's':
			do_set = true;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (do_set)
	{
		if (argc == 1)
			keychain = (SecKeychainRef)keychain_create_array(argc, argv);
		else if (argc > 0)
			return 2;

		if (use_domain)
		{
			status = SecKeychainSetDomainDefault(domain, keychain);
			if (status)
			{
				sec_error("SecKeychainSetDomainDefault %s: %s", domain2str(domain), sec_errstr(status));
				result = 1;
			}
		}
		else
		{
			status = SecKeychainSetDefault(keychain);
			if (status)
			{
				sec_perror("SecKeychainSetDefault", status);
				result = 1;
			}
		}
	}
	else
	{
		if (argc > 0)
			return 2;

		if (use_domain)
		{
			status = SecKeychainCopyDomainDefault(domain, &keychain);
			if (status)
			{
				sec_error("SecKeychainCopyDomainDefault %s: %s", domain2str(domain), sec_errstr(status));
				result = 1;
			}
			else
			{
#if 0
				fprintf(stdout, "default %s keychain: ", domain2str(domain));
#endif
				display_list("", keychain);
			}
		}
		else
		{
			status = SecKeychainCopyDefault(&keychain);
			if (status)
			{
				sec_perror("SecKeychainCopyDefault", status);
				result = 1;
			}
			else
			{
#if 0
				display_list("default keychain: ", keychain);
#else
				display_list("", keychain);
#endif
			}
		}
	}

	if (keychain)
		CFRelease(keychain);

	return result;
}

int
keychain_login(int argc, char * const *argv)
{
	SecPreferencesDomain domain = kSecPreferencesDomainUser;
	SecKeychainRef keychain = NULL;
	Boolean use_domain = false;
	Boolean do_set = false;
	int ch, result = 0;
	OSStatus status;

	while ((ch = getopt(argc, argv, "d:hs")) != -1)
	{
		switch  (ch)
		{
		case 'd':
			result = parse_domain(optarg, &domain);
			if (result)
				return result;
			use_domain = true;
			break;
		case 's':
			do_set = true;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (do_set)
	{
		if (argc == 1)
			keychain = (SecKeychainRef)keychain_create_array(argc, argv);
		else if (argc > 0)
			return 2;

#if 0
		if (use_domain)
		{
			status = SecKeychainSetDomainLogin(domain, keychain);
			if (status)
			{
				sec_error("SecKeychainSetDomainLogin %s: %s", domain2str(domain), sec_errstr(status));
				result = 1;
			}
		}
		else
		{
			status = SecKeychainSetLogin(keychain);
			if (status)
			{
				sec_perror("SecKeychainSetLogin", status);
				result = 1;
			}
		}
#else
		result = 1;
#endif
	}
	else
	{
		if (argc > 0)
			return 2;

		if (use_domain)
		{
#if 0
			status = SecKeychainCopyDomainLogin(domain, &keychain);
			if (status)
			{
				sec_error("SecKeychainCopyDomainLogin %s: %s", domain2str(domain), sec_errstr(status));
				result = 1;
			}
			else
			{
#if 0
				fprintf(stdout, "login %s keychain: ", domain2str(domain));
#endif
				display_list("", keychain);
			}
#else
			result = 1;
#endif
		}
		else
		{
			status = SecKeychainCopyLogin(&keychain);
			if (status)
			{
				sec_perror("SecKeychainCopyLogin", status);
				result = 1;
			}
			else
			{
#if 0
				display_list("login keychain: ", keychain);
#else
				display_list("", keychain);
#endif
			}
		}
	}

	if (keychain)
		CFRelease(keychain);

	return result;
}
