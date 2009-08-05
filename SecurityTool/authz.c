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
 *  authz.c
 */

#include <getopt.h>
#include <stdio.h>
#include <Security/AuthorizationPriv.h>

#include "authz.h"
#include "security.h"

// AEWP?

AuthorizationRef
read_auth_ref_from_stdin()
{
	AuthorizationRef auth_ref = NULL;
	AuthorizationExternalForm extform;
	size_t bytes_read;
	
	while (kAuthorizationExternalFormLength != (bytes_read = read(STDIN_FILENO, &extform, kAuthorizationExternalFormLength)))
	{
		if ((bytes_read == -1) && ((errno != EAGAIN) || (errno != EINTR)))
			break;
	}
	if (bytes_read != kAuthorizationExternalFormLength)
		fprintf(stderr, "ERR: Failed to read authref\n");
	else
		if (AuthorizationCreateFromExternalForm(&extform, &auth_ref))
			fprintf(stderr, "ERR: Failed to internalize authref\n");

	close(0);

	return auth_ref;
}

int
write_auth_ref_to_stdout(AuthorizationRef auth_ref)
{
	AuthorizationExternalForm extform;
	size_t bytes_written;
	
	if (AuthorizationMakeExternalForm(auth_ref, &extform))
		return -1;
		
	while (kAuthorizationExternalFormLength != (bytes_written = write(STDOUT_FILENO, &extform, kAuthorizationExternalFormLength)))
	{
		if ((bytes_written == -1) && ((errno != EAGAIN) || (errno != EINTR)))
			break;
	}
	
	if (bytes_written == kAuthorizationExternalFormLength)
		return 0;

	return -1;
}

void
write_dict_to_stdout(CFDictionaryRef dict)
{
	if (!dict)
		return;

	CFDataRef right_definition_xml = CFPropertyListCreateXMLData(NULL, dict);

	if (!right_definition_xml)
		return;

	write(STDOUT_FILENO, CFDataGetBytePtr(right_definition_xml), CFDataGetLength(right_definition_xml));
	CFRelease(right_definition_xml);
}

CFDictionaryRef
read_dict_from_stdin()
{
	int bytes_read = 0;
	uint8_t buffer[4096];
	CFMutableDataRef data = CFDataCreateMutable(NULL, sizeof(buffer));

	if (!data)
		return NULL;

	while (bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer)))
	{
		if ((bytes_read == -1) && ((errno != EAGAIN) || (errno != EINTR)))
			break;
		else
			CFDataAppendBytes(data, buffer, bytes_read);
	}

	CFDictionaryRef right_dict = (CFDictionaryRef)CFPropertyListCreateFromXMLData(NULL, data, kCFPropertyListImmutable, NULL);
	if (data)
		CFRelease(data);

	if (!right_dict)
		return NULL;
	if (CFGetTypeID(right_dict) != CFDictionaryGetTypeID())
	{
		CFRelease(right_dict);
		return NULL;
	}
	return right_dict;
}

int
authorizationdb(int argc, char * const * argv)
{
	AuthorizationRef auth_ref = NULL;
	int ch;
    while ((ch = getopt(argc, argv, "i")) != -1)
	{
		switch  (ch)
		{
		case 'i':
			auth_ref = read_auth_ref_from_stdin();
			break;
		case '?':
		default:
			return 2;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return 2; // required right parameter(s)
	
	OSStatus status;
	
	if (argc > 1)
	{
		if (!auth_ref && AuthorizationCreate(NULL, NULL, 0, &auth_ref))
			return -1;

		if (!strcmp("read", argv[0]))
		{
			CFDictionaryRef right_definition;
			status = AuthorizationRightGet(argv[1], &right_definition);
			if (!status)
			{
				write_dict_to_stdout(right_definition);
				CFRelease(right_definition);
			}
		}
		else if (!strcmp("write", argv[0]))
		{
			if (argc == 2)
			{
				CFDictionaryRef right_definition = read_dict_from_stdin();
				if (!right_definition)
					return -1;
				status = AuthorizationRightSet(auth_ref, argv[1], right_definition, NULL, NULL, NULL);
				CFRelease(right_definition);
			}
			else if (argc == 3)
			{
				// argv[2] is shortcut string
				CFStringRef shortcut_definition = CFStringCreateWithCStringNoCopy(NULL, argv[2], kCFStringEncodingUTF8, kCFAllocatorNull);
				if (!shortcut_definition)
					return -1;
				status = AuthorizationRightSet(auth_ref, argv[1], shortcut_definition, NULL, NULL, NULL);
				CFRelease(shortcut_definition);
			}
			else
				return 2; // take one optional argument - no more

		}
		else if (!strcmp("remove", argv[0]))
		{
			status = AuthorizationRightRemove(auth_ref, argv[1]);
		}
		else
			return 2;
	}
	else
		return 2;

	if (auth_ref)
		AuthorizationFree(auth_ref, 0);

	if (!do_quiet)
		fprintf(stderr, "%s (%d)\n", status ? "NO" : "YES", (int)status);
	
	return (status ? -1 : 0);
}

int
authorize(int argc, char * const *argv)
{
	int ch;
	OSStatus status;

	Boolean user_interaction_allowed = FALSE, extend_rights = TRUE; 
	Boolean partial_rights = FALSE, destroy_rights = FALSE;
	Boolean pre_authorize = FALSE, internalize = FALSE, externalize = FALSE;
	Boolean wait = FALSE, explicit_credentials = FALSE; 
	Boolean isolate_explicit_credentials = FALSE, least_privileged = FALSE;
	char *login = NULL;

    while ((ch = getopt(argc, argv, "ucC:EpdPieqwxl")) != -1)
	{
		switch  (ch)
		{
		case 'u':
			user_interaction_allowed = TRUE;
			break;
		case 'c':
			explicit_credentials = TRUE;
			break;
		case 'C':
			explicit_credentials = TRUE;
			login = optarg;
			break;
		case 'e':
			externalize = TRUE;
			break;
		case 'E':
			extend_rights = FALSE;
			break;
		case 'p':
			partial_rights = TRUE;
			break;
		case 'd':
			destroy_rights = TRUE;
			break;
		case 'P':
			pre_authorize = TRUE;
			break;
		case 'i':
			internalize = TRUE;
			break;
		case 'w':
			wait = TRUE;
			externalize = TRUE;
			break;
		case 'x':
			isolate_explicit_credentials = TRUE;
			break;
		case 'l':
			least_privileged = TRUE;
			break;
		case '?':
		default:
			return 2; /* @@@ Return 2 triggers usage message. */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return 2; // required right parameter(s)

// set up AuthorizationFlags
	AuthorizationFlags flags = kAuthorizationFlagDefaults |
		(user_interaction_allowed ? kAuthorizationFlagInteractionAllowed : 0) |
		(extend_rights ? kAuthorizationFlagExtendRights : 0) |
		(partial_rights ? kAuthorizationFlagPartialRights : 0) | 
		(pre_authorize ? kAuthorizationFlagPreAuthorize : 0) |
		(least_privileged ? kAuthorizationFlagLeastPrivileged : 0);

// set up AuthorizationRightSet
	AuthorizationItem rights[argc];
	memset(rights, '\0', argc * sizeof(AuthorizationItem));
	AuthorizationItemSet rightset = { argc, rights };
	while (argc > 0)
		rights[--argc].name = *argv++;

	AuthorizationRef auth_ref = NULL;

// internalize AuthorizationRef
	if (internalize)
	{
		auth_ref = read_auth_ref_from_stdin();
		if (!auth_ref)
			return 1;
	}

	if (!auth_ref && AuthorizationCreate(NULL, NULL, 
				(least_privileged ? kAuthorizationFlagLeastPrivileged : 0), 
				&auth_ref))
		return -1;

// prepare environment if needed
	AuthorizationEnvironment *envp = NULL;
	if (explicit_credentials) {
		if (!login)
			login = getlogin();
		char *pass = getpass("Password:");
		if (!(login && pass))
			return 1;
		static AuthorizationItem authenv[] = {
			{ kAuthorizationEnvironmentUsername },
			{ kAuthorizationEnvironmentPassword },
			{ kAuthorizationEnvironmentShared }			// optional (see below)
		};
		static AuthorizationEnvironment env = { 0, authenv };
		authenv[0].valueLength = strlen(login);
		authenv[0].value = login;
		authenv[1].valueLength = strlen(pass);
		authenv[1].value = pass;
		if (isolate_explicit_credentials)
			env.count = 2;		// do not send kAuthorizationEnvironmentShared
		else
			env.count = 3;		// do send kAuthorizationEnvironmentShared
		envp = &env;
	}

// perform Authorization
	AuthorizationRights *granted_rights = NULL;
	status = AuthorizationCopyRights(auth_ref, &rightset, envp, flags, &granted_rights);

// externalize AuthorizationRef
	if (externalize)
		write_auth_ref_to_stdout(auth_ref);
			
	if (!do_quiet)
		fprintf(stderr, "%s (%d) ", status ? "NO" : "YES", (int)status);

	if (!do_quiet && !status && granted_rights)
	{
		uint32_t index;
		fprintf(stderr, "{ %d: ", (int)granted_rights->count);
		for (index = 0; index < granted_rights->count; index++)
		{
			fprintf(stderr, "\"%s\"%s %c ", granted_rights->items[index].name, 
				(kAuthorizationFlagCanNotPreAuthorize & granted_rights->items[index].flags) ? " (cannot-preauthorize)" : "", 
				(index+1 != granted_rights->count) ? ',' : '}');
		}
		AuthorizationFreeItemSet(granted_rights);
	}

	if (!do_quiet)
		fprintf(stderr, "\n");

// wait for client to pick up AuthorizationRef
	if (externalize && wait)
		while (-1 != write(STDOUT_FILENO, NULL, 0))
			usleep(100);
		
// drop AuthorizationRef
	if (auth_ref)
		AuthorizationFree(auth_ref, destroy_rights ? kAuthorizationFlagDestroyRights : 0);
	
	return (status ? -1 : 0);
}


int
execute_with_privileges(int argc, char * const *argv)
{
	AuthorizationRef auth_ref = NULL;
	int ch;
    while ((ch = getopt(argc, argv, "i")) != -1)
	{
		switch  (ch)
		{
		case 'i':
			auth_ref = read_auth_ref_from_stdin();
			break;
		case '?':
		default:
			return 2;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0)
		return 2; // required tool parameter(s)
	
	OSStatus status;
	
	if (!auth_ref && AuthorizationCreate(NULL, NULL, 0, &auth_ref))
		return -1;

	FILE *communications_pipe = NULL;

	status = AuthorizationExecuteWithPrivileges(auth_ref,argv[0], 0, (argc > 1) ? &argv[1] : NULL, &communications_pipe);

	if (!do_quiet)
		fprintf(stderr, "%s (%d) ", status ? "NO" : "YES", (int)status);
	
	if (!status)
	{
		int bytes_read = 0;
		uint8_t buffer[4096];
		
		while (bytes_read = read(STDIN_FILENO, &buffer, sizeof(buffer)))
		{
			if ((bytes_read == -1) && ((errno != EAGAIN) || (errno != EINTR)))
				break;
			else
				while ((-1 == write(fileno(communications_pipe), buffer, bytes_read)) &&
						((errno == EAGAIN) || (errno == EINTR)))
							usleep(100);
		}
	}
	
	return (status ? -1 : 0);	
}

