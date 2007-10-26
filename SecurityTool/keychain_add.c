/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All Rights Reserved.
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
#include "readline.h"
#include "security.h"
#include "keychain_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <Security/SecCertificate.h>
#include <libkern/OSByteOrder.h>



static int
do_addgenericpassword(const char *keychainName, const char *serviceName, const char *accountName, const void *passwordData)
 {
	SecKeychainRef keychain = NULL;
	OSStatus result;
    SecKeychainItemRef initemRef = NULL;
  
	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto loser;
		}
	}

    result = SecKeychainAddGenericPassword(keychain, serviceName ? strlen(serviceName) : 0, serviceName, accountName ? strlen(accountName) : 0, accountName,passwordData ? strlen(passwordData) : 0,passwordData, &initemRef);
	if (result)
	{
		sec_error("SecKeychainAddGenericPassword %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
	}

loser:
	if (keychain)
		CFRelease(keychain);

	return result;
}

static int
do_addinternetpassword(const char *keychainName, const char *serverName, const char *securityDomain, const char *accountName, const char *path, UInt16 port, SecProtocolType protocol,SecAuthenticationType authenticationType, const void *passwordData)
 {
	SecKeychainRef keychain = NULL;
	OSStatus result;
    SecKeychainItemRef initemRef = NULL;
  
	if (keychainName)
	{
		keychain = keychain_open(keychainName);
		if (!keychain)
		{
			result = 1;
			goto loser;
		}
	}

result = SecKeychainAddInternetPassword(keychain, serverName ? strlen(serverName) : 0, serverName, securityDomain ? strlen(securityDomain) : 0, securityDomain, accountName ? strlen(accountName) : 0, accountName, path ? strlen(path) : 0, path, port, protocol, authenticationType,passwordData ? strlen(passwordData) : 0,passwordData, &initemRef);
	if (result)
	{
		sec_error("SecKeychainAddInternetPassword %s: %s", keychainName ? keychainName : "<NULL>", sec_errstr(result));
	}

loser:
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
			goto loser;
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

loser:
	if (keychain)
		CFRelease(keychain);

	return result;
}

int
keychain_add_generic_password(int argc, char * const *argv)
{
	char *serviceName = NULL,*passwordData  = NULL,*accountName = NULL;
    int ch, result = 0;
	const char *keychainName = NULL;
  /*
      "	   -s Use servicename\n"
      "    -a Use accountname\n"
      "    -p Use passwordData  \n"
      */
	while ((ch = getopt(argc, argv, "s:a:p:")) != -1)
	{
		switch  (ch)
		{
		case 's':
			serviceName = optarg;
			break;
        case 'a':
            accountName = optarg;
			break;
        case 'p':
            passwordData = optarg;
            break;
        case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
	{
		keychainName = argv[0];
		if (*keychainName == '\0')
		{
			result = 2;
			goto loser;
		}
	}
	else if (argc != 0)
		return 2;

	result = do_addgenericpassword(keychainName, serviceName, accountName, passwordData);

loser:
	return result;
}

int keychain_add_internet_password(int argc, char * const *argv)
{
	char *serverName = NULL, *securityDomain = NULL, *accountName = NULL, *path = NULL, *passwordData  = NULL;
    UInt16 port = 0;
    SecProtocolType protocol;
    SecAuthenticationType authenticationType;
	int ch, result = 0;
	const char *keychainName = NULL;
  /*
           -s Use servername\n"
	  "    -e Use securitydomain\n"
      "    -a Use accountname\n"
      "    -p Use path\n"
      "    -o Use port \n"
      "    -r Use protocol \n"
      "    -c Use SecAuthenticationType  \n"
      "    -w Use passwordData  \n"
      */
	while ((ch = getopt(argc, argv, "s:d:a:p:P:r:t:w:h")) != -1)
	{
		switch  (ch)
		{
		case 's':
			serverName = optarg;
			break;
        case 'd':
            securityDomain = optarg;
			break;
        case 'a':
            accountName = optarg;
            break;
        case 'p':
            path = optarg;
            break;
        case 'P':
            port = atoi(optarg);
            break;
        case 'r':
			protocol = OSSwapHostToBigInt32(*(u_int32_t*) optarg);
            break;
        case 't':
		   authenticationType = OSSwapHostToBigInt32(*(u_int32_t*) optarg);
            break;
        case 'w':
            passwordData = optarg;
            break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 1)
	{
		keychainName = argv[0];
		if (*keychainName == '\0')
		{
			result = 2;
			goto loser;
		}
	}
	else if (argc != 0)
		return 2;

	result = do_addinternetpassword(keychainName, serverName, securityDomain, accountName, path, port, protocol,authenticationType, passwordData);

loser:
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
