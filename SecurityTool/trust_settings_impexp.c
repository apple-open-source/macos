/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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
 * trust_cert_add.c
 */

#include "trust_settings_impexp.h"
#include "security.h"
#include <Security/Security.h>
#include <Security/SecTrustSettings.h>
#include <errno.h>
#include <unistd.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_cdsa_utils/cuFileIo.h>

extern int trust_settings_export(int argc, char * const *argv)
{
	extern char *optarg;
	extern int optind;
	OSStatus ortn;
	int arg;
	CFDataRef settings = NULL;
	SecTrustSettingsDomain domain = kSecTrustSettingsDomainUser;
	int rtn;
	char *settingsFile = NULL;
	unsigned len;

	if(argc < 2) {
		return 2; /* @@@ Return 2 triggers usage message. */
	}
	
	optind = 1;
	while ((arg = getopt(argc, argv, "dsh")) != -1) {
		switch (arg) {
			case 'd':
				domain = kSecTrustSettingsDomainAdmin;
				break;
			case 's':
				domain = kSecTrustSettingsDomainSystem;
				break;
			default:
				return 2;
		}
	}
	if(optind != (argc - 1)) {
		/* no args left for settings file */
		return 2;
	}
	settingsFile = argv[optind];

	ortn = SecTrustSettingsCreateExternalRepresentation(domain, &settings);
	if(ortn) {
		cssmPerror("SecTrustSettingsCreateExternalRepresentation", ortn);
		return 1;
	}
	len = CFDataGetLength(settings);
	rtn = writeFile(settingsFile, CFDataGetBytePtr(settings), len);
	if(rtn) {
		fprintf(stderr, "Error (%d) writing %s.\n", rtn, settingsFile);
	}
	else if(!do_quiet) {
		fprintf(stdout, "...Trust Settings exported successfully.\n");
	}
	CFRelease(settings);
	return rtn;
}

extern int trust_settings_import(int argc, char * const *argv)
{
	extern char *optarg;
	extern int optind;
	OSStatus ortn;
	int arg;
	char *settingsFile = NULL;
	unsigned char *settingsData = NULL;
	unsigned settingsLen = 0;
	CFDataRef settings = NULL;
	SecTrustSettingsDomain domain = kSecTrustSettingsDomainUser;
	int rtn;

	if(argc < 2) {
		return 2; /* @@@ Return 2 triggers usage message. */
	}
	
	optind = 1;
	while ((arg = getopt(argc, argv, "dh")) != -1) {
		switch (arg) {
			case 'd':
				domain = kSecTrustSettingsDomainAdmin;
				break;
			default:
				return 2;
		}
	}
	if(optind != (argc - 1)) {
		/* no args left for settings file */
		return 2;
	}
	settingsFile = argv[optind];
	rtn = readFile(settingsFile, &settingsData, &settingsLen);
	if(rtn) {
		fprintf(stderr, "Error (%d) reading %s.\n", rtn, settingsFile);
		return 1;
	}
	settings = CFDataCreate(NULL, (const UInt8 *)settingsData, settingsLen);
	free(settingsData);
	ortn = SecTrustSettingsImportExternalRepresentation(domain, settings);
	CFRelease(settings);
	if(ortn) {
		cssmPerror("SecTrustSettingsImportExternalRepresentation", ortn);
		rtn = 1;
	}
	else if(!do_quiet) {
		fprintf(stdout, "...Trust Settings imported successfully.\n");
		rtn = 0;
	}
	return rtn;
}

