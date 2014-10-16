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
#include <notify.h>
#include <xpc/private.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/logging.h>


static const char serviceName[] = "com.apple.security.syspolicy";
static const char rearmActivityName[] = "com.apple.security.syspolicy.rearm";

static const CFTimeInterval rearmPeriod = 30*24*60*60;	// 30 days


//
// Local functions
//
static void usage();


//
// Operations functions
//
void doAssess(xpc_object_t msg, xpc_object_t reply);
void doUpdate(xpc_object_t msg, xpc_object_t reply);
void doRecord(xpc_object_t msg, xpc_object_t reply, xpc_connection_t connection);
void doCancel(xpc_object_t msg);
void doRearmCheck();

void sendProgress(xpc_connection_t connection, uint64_t ref, std::string token, CFDictionaryRef info);


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
	

	xpc_activity_register(rearmActivityName, XPC_ACTIVITY_CHECK_IN, ^(xpc_activity_t activity) {
		if (xpc_activity_get_state(activity) == XPC_ACTIVITY_STATE_RUN) {
			doRearmCheck();
		}
	});

	dispatch_queue_t queue = dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0);	// concurrent work queue
	xpc_connection_t service = xpc_connection_create_mach_service(name, queue, XPC_CONNECTION_MACH_SERVICE_LISTENER /* | XPC_CONNECTION_MACH_SERVICE_PRIVILEGED */);
	
	xpc_connection_set_event_handler(service, ^(xpc_object_t cmsg) {
		if (xpc_get_type(cmsg) == XPC_TYPE_CONNECTION) {
			xpc_connection_t connection = xpc_connection_t(cmsg);
			syslog(LOG_DEBUG, "Connection from pid %d", xpc_connection_get_pid(connection));
			xpc_connection_set_event_handler(connection, ^(xpc_object_t msg) {
				if (xpc_get_type(msg) == XPC_TYPE_DICTIONARY) {
					xpc_retain(msg);
					dispatch_async(queue, ^{	// concurrent from here
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
							} else if (!strcmp(function, "record")) {
								doRecord(msg, reply, connection);
							} else if (!strcmp(function, "cancel")) {
								doCancel(msg);
							} else {
								xpc_dictionary_set_int64(reply, "error", errSecCSInternalError);
							}
						} catch (...) {
							xpc_dictionary_set_int64(reply, "error", errSecCSInternalError);
						}
						xpc_release(msg);
						if (reply) {
							xpc_connection_send_message(connection, reply);
							xpc_release(reply);
						}
					});
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


//
// A rudimentary cancellation mailbox facility.
// A relayed progress reply requesting cancellation is recorded here, and picked up
// on the NEXT progress call from SecAssessment. This avoids synchronous delays
// within one progress call.
//
std::set<std::string> cancellationMailbox;
Mutex cancellationLock;


void doAssess(xpc_object_t msg, xpc_object_t reply)
{
	const char *path = xpc_dictionary_get_string(msg, "path");
	uint64_t flags = xpc_dictionary_get_int64(msg, "flags");
	
	CFErrorRef errors = NULL;
	size_t contextLength;
	const void *contextData = xpc_dictionary_get_data(msg, "context", &contextLength);
	CFRef<CFDictionaryRef> context = makeCFDictionaryFrom(contextData, contextLength);
	CFTypeRef cfprogress = CFDictionaryGetValue(context, kSecAssessmentContextKeyFeedback);
	CFNumberRef progress = NULL;		// incoming progress reference
	__block string progressToken;		// (__block to work around limitations of the block runtime. We don't write this from the block)
	if (cfprogress && CFGetTypeID(cfprogress) == CFNumberGetTypeID())
		progress = CFNumberRef(cfprogress);
	if (progress) {
		// Assign a progress-reporting token to connect cancellation requests.
		// We use this to match incoming cancellation requests with ongoing progress activity.
		CFRef<CFUUIDRef> uuid = CFUUIDCreate(NULL);
		CFRef<CFStringRef> uuids = CFUUIDCreateString(NULL, uuid);
		progressToken = cfString(uuids);
		
		// turn client-side progress reference into a block that processes callbacks locally
		uint64_t progressRef = cfNumber<uint64_t>(progress);	// this is the remote caller's handle for our transaction
		CFRef<CFMutableDictionaryRef> ctx = makeCFMutableDictionary(context.get());
		CFDictionarySetValue(ctx, kSecAssessmentContextKeyFeedback, ^Boolean(CFStringRef type, CFDictionaryRef info) {
			// check for pending cancellation
			{
				StLock<Mutex> _(cancellationLock);
				if (cancellationMailbox.find(progressToken) != cancellationMailbox.end()) {
					return false;
				}
			}
			
			// no cancellation yet; forward progress report
			xpc_connection_t connection = xpc_dictionary_get_remote_connection(msg);
			sendProgress(connection, progressRef, progressToken, info);
			return true;
		});
		context = ctx.get();
	}
	
	// here is where we do the actual work
	if (CFRef<SecAssessmentRef> assessment = SecAssessmentCreate(CFTempURL(path), flags | kSecAssessmentFlagDirect | kSecAssessmentFlagIgnoreCache, context, &errors))
		if (CFRef<CFDictionaryRef> result = SecAssessmentCopyResult(assessment, kSecAssessmentDefaultFlags, &errors)) {
			CFRef<CFDataRef> resultData = makeCFData(result.get());
			xpc_dictionary_set_data(reply, "result", CFDataGetBytePtr(resultData), CFDataGetLength(resultData));
		}
	if (errors)
		xpc_dictionary_set_int64(reply, "error", CFErrorGetCode(errors));

	// clean up cancellation mailbox (best effort)
	if (!progressToken.empty()) {
		StLock<Mutex> _(cancellationLock);
		cancellationMailbox.erase(progressToken);
	}
}


//
// Send a progress-reporting xpc message to the client.
// This is "against the flow" of the request.
//
void sendProgress(xpc_connection_t connection, uint64_t ref, std::string token, CFDictionaryRef info)
{
	CFDictionary state(info, 0);
	unsigned current = cfNumber<unsigned>(state.get<CFNumberRef>("current"));
	unsigned total = cfNumber<unsigned>(state.get<CFNumberRef>("total"));
	xpc_object_t progress = xpc_dictionary_create(NULL, NULL, 0);
	xpc_dictionary_set_string(progress, "function", "progress");
	xpc_dictionary_set_uint64(progress, "current", current);
	xpc_dictionary_set_uint64(progress, "total", total);
	xpc_dictionary_set_uint64(progress, "ref", ref);
	xpc_dictionary_set_string(progress, "token", token.c_str());
	xpc_connection_send_message(connection, progress);
}


//
// Process a cancellation request from the client.
// We simply set a "mailbox" flag for the token that we sent the client and that he sent us back here.
void doCancel(xpc_object_t msg)
{
	if (const char* token = xpc_dictionary_get_string(msg, "token")) {
		// enter into pending cancellation mailbox
		StLock<Mutex> _(cancellationLock);
		cancellationMailbox.insert(token);
	}
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


void doRecord(xpc_object_t msg, xpc_object_t reply, xpc_connection_t connection)
{
	// check caller for required entitlement
	xpc_object_t entitlement = xpc_connection_copy_entitlement_value(connection, "com.apple.private.assessment.recording");
	if (entitlement == NULL || entitlement == XPC_BOOL_FALSE) {
		xpc_dictionary_set_int64(reply, "error", errSecAuthFailed);
		return;
	}

	// make local control call and relay result
	size_t length;
	const void *infoData = xpc_dictionary_get_data(msg, "info", &length);
	CFRef<CFDictionaryRef> info = makeCFDictionaryFrom(infoData, length);

	CFErrorRef errors = NULL;
	if (!SecAssessmentControl(CFSTR("ui-record-reject-local"), (void *)info.get(), &errors)) {
		xpc_dictionary_set_int64(reply, "error", CFErrorGetCode(errors));
		CFRelease(errors);
	}
}


void doRearmCheck()
{
	// check global preference
	CFRef<CFBooleanRef> rearmPref = (CFBooleanRef)CFPreferencesCopyValue(CFSTR("GKAutoRearm"), CFSTR("com.apple.security"), kCFPreferencesAnyUser, kCFPreferencesCurrentHost);
	if (rearmPref == kCFBooleanFalse)
		return;

	CFBooleanRef status;
	CFTimeInterval delta;
	if (SecAssessmentControl(CFSTR("ui-status"), &status, NULL) && status == kCFBooleanFalse)
		if (SecAssessmentControl(CFSTR("rearm-status"), &delta, NULL) && delta > rearmPeriod) {
			SecAssessmentControl(CFSTR("ui-enable"), NULL, NULL);		// enable assessments
			SecAssessmentControl(CFSTR("ui-enable-devid"), NULL, NULL);	// allow Developer ID
			notify_post("com.apple.security.assessment.rearm");
		}
}
