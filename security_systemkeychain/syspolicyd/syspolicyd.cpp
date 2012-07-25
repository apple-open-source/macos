/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
#include <Security/SecAssessment.h>
#include <getopt.h>
#include <syslog.h>
#include <libgen.h>
#include <xpc/private.h>
#include <security_utilities/cfutilities.h>


static const char serviceName[] = "com.apple.security.syspolicy";


//
// Local functions
//
static void usage();


//
// Operations functions
//
void doAssess(xpc_object_t msg, xpc_object_t reply);
void doUpdate(xpc_object_t msg, xpc_object_t reply);


//
// Your standard server-side xpc main
//
int main (int argc, char * const argv[])
{
	const char *name = serviceName;
	if (const char *s = getenv("SYSPOLICYNAME"))
		name = s;
	
//    extern char *optarg;
    extern int optind;
    int arg;
    while ((arg = getopt(argc, argv, "v")) != -1)
        switch (arg) {
		case 'v':
			break;
		case '?':
			usage();
		}
	if (optind < argc)
		usage();
	
	dispatch_queue_t queue = dispatch_queue_create("server", 0);
	xpc_connection_t service = xpc_connection_create_mach_service(name, queue, XPC_CONNECTION_MACH_SERVICE_LISTENER /* | XPC_CONNECTION_MACH_SERVICE_PRIVILEGED */);
	
	xpc_connection_set_event_handler(service, ^(xpc_object_t cmsg) {
		if (xpc_get_type(cmsg) == XPC_TYPE_CONNECTION) {
			xpc_connection_t connection = xpc_connection_t(cmsg);
			syslog(LOG_DEBUG, "Connection from pid %d", xpc_connection_get_pid(connection));
			xpc_connection_set_event_handler(connection, ^(xpc_object_t msg) {
				if (xpc_get_type(msg) == XPC_TYPE_DICTIONARY) {
					const char *function = xpc_dictionary_get_string(msg, "function");
					syslog(LOG_DEBUG, "pid %d requested %s", xpc_connection_get_pid(connection), function);
					xpc_object_t reply = xpc_dictionary_create_reply(msg);
					try {
						if (function == NULL) {
							xpc_dictionary_set_int64(reply, "error", errSecCSInternalError);
						} else if (!strcmp(function, "assess")) {
							doAssess(msg, reply);
						} else if (!strcmp(function, "update")) {
							doUpdate(msg, reply);
						} else {
							xpc_dictionary_set_int64(reply, "error", errSecCSInternalError);
						}
					} catch (...) {
						xpc_dictionary_set_int64(reply, "error", errSecCSInternalError);
					}
					xpc_connection_send_message(connection, reply);
					xpc_release(reply);
				}
			});
			xpc_connection_resume(connection);
		} else {
			const char *s = xpc_copy_description(cmsg);
			printf("Incoming message - %s\n", s);
			free((char*)s);
		}
	});
	xpc_connection_resume(service);

    dispatch_main();
    return 1;
}


static void usage()
{
	fprintf(stderr, "Usage: spd\n");
	exit(2);
}


void doAssess(xpc_object_t msg, xpc_object_t reply)
{
	const char *path = xpc_dictionary_get_string(msg, "path");
	uint64_t flags = xpc_dictionary_get_int64(msg, "flags");
	
	CFErrorRef errors = NULL;
	size_t contextLength;
	const void *contextData = xpc_dictionary_get_data(msg, "context", &contextLength);
	CFRef<CFDictionaryRef> context = makeCFDictionaryFrom(contextData, contextLength);
	if (CFRef<SecAssessmentRef> assessment = SecAssessmentCreate(CFTempURL(path), flags | kSecAssessmentFlagDirect | kSecAssessmentFlagIgnoreCache, context, &errors))
		if (CFRef<CFDictionaryRef> result = SecAssessmentCopyResult(assessment, kSecAssessmentDefaultFlags, &errors)) {
			CFRef<CFDataRef> resultData = makeCFData(result.get());
			xpc_dictionary_set_data(reply, "result", CFDataGetBytePtr(resultData), CFDataGetLength(resultData));
			return;
		}
	xpc_dictionary_set_int64(reply, "error", CFErrorGetCode(errors));
}


void doUpdate(xpc_object_t msg, xpc_object_t reply)
{
	// target is polymorphic optional; put it together here
	CFRef<CFTypeRef> target;
	size_t length;
	if (const void *reqblob = xpc_dictionary_get_data(msg, "requirement", &length))
		MacOSError::check(SecRequirementCreateWithData(CFTempData(reqblob, length), kSecCSDefaultFlags, (SecRequirementRef *)&target.aref()));
	else if (uint64_t rule = xpc_dictionary_get_uint64(msg, "rule"))
		target.take(makeCFNumber(rule));
	else if (const char *s = xpc_dictionary_get_string(msg, "url"))
		target.take(makeCFURL(s));
	uint64_t flags = xpc_dictionary_get_int64(msg, "flags");
	const void *contextData = xpc_dictionary_get_data(msg, "context", &length);
	CFRef<CFDictionaryRef> context = makeCFDictionaryFrom(contextData, length);

	CFErrorRef errors = NULL;
	if (CFRef<CFDictionaryRef> result = SecAssessmentCopyUpdate(target, flags | kSecAssessmentFlagDirect, context, &errors)) {
		CFRef<CFDataRef> resultData = makeCFData(result.get());
		xpc_dictionary_set_data(reply, "result", CFDataGetBytePtr(resultData), CFDataGetLength(resultData));
	} else {
		xpc_dictionary_set_int64(reply, "error", CFErrorGetCode(errors));
		CFRelease(errors);
	}
}
