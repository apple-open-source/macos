/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
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
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/sysctl.h>

#include "passwd.h"

#ifdef INFO_OPEN_DIRECTORY

#include <CoreFoundation/CoreFoundation.h>
#include <OpenDirectory/OpenDirectory.h>
#include <OpenDirectory/OpenDirectoryPriv.h>

extern char* progname;
int master_mode;

static int
cfprintf(FILE* file, const char* format, ...) {
		char* cstr;
		int result = 0;
        va_list args;
        va_start(args, format);
        CFStringRef formatStr = CFStringCreateWithCStringNoCopy(NULL, format, kCFStringEncodingUTF8, kCFAllocatorNull);
		if (formatStr) {
			CFStringRef str = CFStringCreateWithFormatAndArguments(NULL, NULL, formatStr, args);
			if (str) {
				size_t size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(str), kCFStringEncodingUTF8) + 1;
				va_end(args);
				cstr = malloc(size);
				if (cstr && CFStringGetCString(str, cstr, size, kCFStringEncodingUTF8)) {
					result = fprintf(file, "%s", cstr);
					free(cstr);
				}
				CFRelease(str);
			}
			CFRelease(formatStr);
		}
		return result;
}

static void
show_error(CFErrorRef error) {
	if (error) {
		CFStringRef desc = CFErrorCopyDescription(error);
		if (desc) {
			cfprintf(stderr, "%s: %@", progname, desc);
			CFRelease(desc);
		}
		desc = CFErrorCopyFailureReason(error);
		if (desc) cfprintf(stderr, "  %@", desc);
		
		desc = CFErrorCopyRecoverySuggestion(error);
		if (desc) cfprintf(stderr, "  %@", desc);
		
		fprintf(stderr, "\n");
	}
}

int
od_passwd(char* uname, char* locn, char* aname)
{
	int change_pass_on_self;
	CFErrorRef error = NULL;
	CFStringRef username = NULL;
	CFStringRef location = NULL;
	CFStringRef authname = NULL;
	ODNodeRef node = NULL;
	ODRecordRef rec = NULL;
	CFStringRef oldpass = NULL;
	CFStringRef newpass = NULL;
	
	if (uname == NULL)
		return -1;

	/*
	 * If no explicit authorization name was specified (via -u)
	 * then default to the target user.
	 */
	if (!aname) {
		aname = strdup(uname);
	}

	master_mode = (getuid() == 0);
	change_pass_on_self = (strcmp(aname, uname) == 0);

	if (locn) {
		location = CFStringCreateWithCString(NULL, locn, kCFStringEncodingUTF8);
	}

	if (aname) {
		authname = CFStringCreateWithCString(NULL, aname, kCFStringEncodingUTF8);
	}

	if (uname) {
		username = CFStringCreateWithCString(NULL, uname, kCFStringEncodingUTF8);
		if (!username) return -1;
	}

	/*
	 * Copy the record from the specified node, or perform a search.
	 */
	if (location) {
		node = ODNodeCreateWithName(NULL, kODSessionDefault, location, &error);
	} else {
		node = ODNodeCreateWithNodeType(NULL, kODSessionDefault, kODNodeTypeAuthentication, &error);
	}

	if (node) {
		rec = ODNodeCopyRecord(node, kODRecordTypeUsers, username, NULL, &error );
		CFRelease(node);
	}

	if (!rec) {
		if (error) {
			show_error(error);
		} else {
			fprintf(stderr, "%s: Unknown user name '%s'.\n", progname, uname);
		}
		return -1;
	}

	/*
	 * Get the actual location.
	 */
	CFArrayRef values = NULL;
	values = ODRecordCopyValues(rec, kODAttributeTypeMetaNodeLocation, &error);
	location = (values && CFArrayGetCount(values) > 0) ? CFArrayGetValueAtIndex(values, 0) : location;
	
	printf("Changing password for %s.\n", uname);

	/*
	 * Prompt for password if not super-user, or if changing a remote node.
	 */
	int needs_auth = (!master_mode || CFStringCompareWithOptions(location, CFSTR("/Local/"), CFRangeMake(0, 7), 0) != kCFCompareEqualTo);

	if (needs_auth) {
		char prompt[BUFSIZ];
		if (change_pass_on_self) {
			strlcpy(prompt, "Old password:", sizeof(prompt));
		} else {
			snprintf(prompt, sizeof(prompt), "Password for %s:", aname);
		}
		char *p = getpass( prompt );
		if (p) {
			oldpass = CFStringCreateWithCString(NULL, p, kCFStringEncodingUTF8);
			memset(p, 0, strlen(p));
		}
	}

	for (;;) {
		char *p = getpass("New password:");
		if (p && strlen(p) > 0) {
			newpass = CFStringCreateWithCString(NULL, p, kCFStringEncodingUTF8);
			memset(p, 0, strlen(p));
		} else {
			printf("Password unchanged.\n");
			exit(0);
		}
		
		p = getpass("Retype new password:");
		if (p) {
			CFStringRef verify = CFStringCreateWithCString(NULL, p, kCFStringEncodingUTF8);
			if (!verify || !CFEqual(newpass, verify)) {
				if (verify) CFRelease(verify);
				printf("Mismatch; try again, EOF to quit.\n");
			} else {
				CFRelease(verify);
				break;
			}
		}
	}

	ODRecordChangePassword(rec, oldpass, newpass, &error);

	if (error) {
		show_error(error);
		exit(1);
	}

	if (oldpass) CFRelease(oldpass);
	if (newpass) CFRelease(newpass);

#if 0
	if ( status != eDSNoErr ) {
		switch( status )
		{
			case eDSAuthPasswordTooShort:
				errMsgStr = "The new password is too short.";
				break;
			
			case eDSAuthPasswordTooLong:
				errMsgStr = "The new password is too long.";
				break;
				
			case eDSAuthPasswordNeedsLetter:
				errMsgStr = "The new password must contain a letter.";
				break;
				
			case eDSAuthPasswordNeedsDigit:
				errMsgStr = "The new password must contain a number.";
				break;
				
			default:
				errMsgStr = "Sorry";
		}
		fprintf(stderr, "%s\n", errMsgStr);
		exit(1);
#endif
	return 0;
}

#endif /* INFO_OPEN_DIRECTORY */
