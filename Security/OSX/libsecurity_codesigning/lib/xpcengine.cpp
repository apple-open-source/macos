/*
 * Copyright (c) 2011-2016 Apple Inc. All Rights Reserved.
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
#include "xpcengine.h"
#include <xpc/connection.h>
#include <syslog.h>
#include <CoreFoundation/CoreFoundation.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/logging.h>
#include <security_utilities/cfmunge.h>

#include <map>

namespace Security {
namespace CodeSigning {
	
	
static void doProgress(xpc_object_t msg);


static const char serviceName[] = "com.apple.security.syspolicy";


static dispatch_once_t dispatchInit;		// one-time init marker
static xpc_connection_t service;			// connection to spd
static dispatch_queue_t queue;				// dispatch queue for service
	
static map<uint64_t, SecAssessmentFeedback> *feedbackBlocks;
	
static void init()
{
	dispatch_once(&dispatchInit, ^void(void) {
		feedbackBlocks = new map<uint64_t, SecAssessmentFeedback>;
		const char *name = serviceName;
		if (const char *env = getenv("SYSPOLICYNAME"))
			name = env;
		queue = dispatch_queue_create("spd-client", DISPATCH_QUEUE_SERIAL);
		service = xpc_connection_create_mach_service(name, queue, XPC_CONNECTION_MACH_SERVICE_PRIVILEGED);
		xpc_connection_set_event_handler(service, ^(xpc_object_t msg) {
			if (xpc_get_type(msg) == XPC_TYPE_DICTIONARY) {
				const char *function = xpc_dictionary_get_string(msg, "function");
				if (strcmp(function, "progress") == 0) {
					try {
						doProgress(msg);
					} catch (...) {
						Syslog::error("Discarding progress handler exception");
					}
				}
			}
		});
		xpc_connection_resume(service);
	});
}


//
// Your standard XPC client-side machinery
//
class Message {
public:
	xpc_object_t obj;
	
	Message(const char *function)
	{
		init();
		obj = xpc_dictionary_create(NULL, NULL, 0);
		xpc_dictionary_set_string(obj, "function", function);
	}
	~Message()
	{
		if (obj)
			xpc_release(obj);
	}
	operator xpc_object_t () { return obj; }

	void send()
	{
		xpc_object_t reply = xpc_connection_send_message_with_reply_sync(service, obj);
		xpc_release(obj);
		obj = NULL;
		xpc_type_t type = xpc_get_type(reply);
		if (type == XPC_TYPE_DICTIONARY) {
			obj = reply;
			if (int64_t error = xpc_dictionary_get_int64(obj, "error"))
				MacOSError::throwMe((int)error);
		} else if (type == XPC_TYPE_ERROR) {
			const char *s = xpc_copy_description(reply);
			secerror("code signing internal problem: unexpected error from xpc: %s", s);
			free((char*)s);
            MacOSError::throwMe(errSecCSInternalError);
		} else {
			const char *s = xpc_copy_description(reply);
			secerror("code signing internal problem: unexpected type of return object: %s", s);
			free((char*)s);
		}
	}
};



static void copyCFDictionary(const void *key, const void *value, void *ctx)
{
	CFMutableDictionaryRef target = CFMutableDictionaryRef(ctx);
	if (CFGetTypeID(value) == CFURLGetTypeID()) {
		CFRef<CFStringRef> path = CFURLCopyFileSystemPath(CFURLRef(value), kCFURLPOSIXPathStyle);
		CFDictionaryAddValue(target, key, path);
	} else if (!CFEqual(key, kSecAssessmentContextKeyFeedback)) {
		CFDictionaryAddValue(target, key, value);
	}
}
	
	
static bool precheckAccess(CFURLRef path, CFDictionaryRef context)
{
	CFTypeRef type = CFDictionaryGetValue(context, kSecAssessmentContextKeyOperation);
	if (type == NULL || CFEqual(type, kSecAssessmentOperationTypeExecute)) {
		CFRef<SecStaticCodeRef> code;
		OSStatus rc = SecStaticCodeCreateWithPath(path, kSecCSDefaultFlags, &code.aref());
        if (rc == errSecCSBadBundleFormat)  // work around <rdar://problem/26075034>
            return false;
		CFRef<CFURLRef> exec;
		MacOSError::check(SecCodeCopyPath(code, kSecCSDefaultFlags, &exec.aref()));
		UnixError::check(::access(cfString(exec).c_str(), R_OK));
	} else {
		UnixError::check(access(cfString(path).c_str(), R_OK));
	}
    return true;
}
	

void xpcEngineAssess(CFURLRef path, SecAssessmentFlags flags, CFDictionaryRef context, CFMutableDictionaryRef result)
{
	precheckAccess(path, context);
	Message msg("assess");
	xpc_dictionary_set_string(msg, "path", cfString(path).c_str());
	xpc_dictionary_set_uint64(msg, "flags", flags);
	CFRef<CFMutableDictionaryRef> ctx = makeCFMutableDictionary();
	if (context) {
		CFDictionaryApplyFunction(context, copyCFDictionary, ctx);
	}

	SecAssessmentFeedback feedback = (SecAssessmentFeedback)CFDictionaryGetValue(context, kSecAssessmentContextKeyFeedback);
	
	/* Map the feedback block to a random number for tracking, because we don't want
	 * to send over a pointer. */
	uint64_t __block feedbackId = 0;
	if (feedback) {
		dispatch_sync(queue, ^{
			bool added = false;
			while (!added) {
				/* Simple sequence number would probably be sufficient,
				 * but making the id unpredictable is also cheap enough here. */
				arc4random_buf(&feedbackId, sizeof(uint64_t));
				if ((*feedbackBlocks)[feedbackId] == NULL /* extremely certain */) {
					(*feedbackBlocks)[feedbackId] = feedback;
					added = true;
				}
			}
		});
		CFDictionaryAddValue(ctx, kSecAssessmentContextKeyFeedback, CFTempNumber(feedbackId));
	}
	
	CFRef<CFDataRef> contextData = makeCFData(CFDictionaryRef(ctx));
	xpc_dictionary_set_data(msg, "context", CFDataGetBytePtr(contextData), CFDataGetLength(contextData));
	
	msg.send();
	
	/* Done, feedback block won't be called anymore,
	 * so remove the feedback mapping from the global map. */
	if (feedback) {
		dispatch_sync(queue, ^{
			feedbackBlocks->erase(feedbackId);
		});
	}
	
	if (int64_t error = xpc_dictionary_get_int64(msg, "error"))
		MacOSError::throwMe((int)error);

	size_t resultLength;
	const void *resultData = xpc_dictionary_get_data(msg, "result", &resultLength);
	CFRef<CFDictionaryRef> resultDict = makeCFDictionaryFrom(resultData, resultLength);
	CFDictionaryApplyFunction(resultDict, copyCFDictionary, result);
	CFDictionaryAddValue(result, CFSTR("assessment:remote"), kCFBooleanTrue);
}
	
static void doProgress(xpc_object_t msg)
{
	uint64_t current = xpc_dictionary_get_uint64(msg, "current");
	uint64_t total = xpc_dictionary_get_uint64(msg, "total");
	uint64_t ref = xpc_dictionary_get_uint64(msg, "ref");
	const char *token = xpc_dictionary_get_string(msg, "token");
	
	SecAssessmentFeedback feedback = NULL;

	// doProgress is called on the queue, so no dispatch_sync here.
	try {
		feedback = feedbackBlocks->at(ref);
	} catch (std::out_of_range) {
		// Indicates that syspolicyd gave us something it shouldn't have.
		Syslog::error("no feedback block registered with ID %lld", ref);
		MacOSError::throwMe(errSecCSInternalError);
	}
	
	CFTemp<CFDictionaryRef> info("{current=%d,total=%d}", current, total);
	Boolean proceed = feedback(kSecAssessmentFeedbackProgress, info);
	if (!proceed) {
		xpc_connection_t connection = xpc_dictionary_get_remote_connection(msg);
		xpc_object_t cancelRequest = xpc_dictionary_create(NULL, NULL, 0);
		xpc_dictionary_set_string(cancelRequest, "function", "cancel");
		xpc_dictionary_set_string(cancelRequest, "token", token);
		xpc_connection_send_message(connection, cancelRequest);
		xpc_release(cancelRequest);
	}
}


CFDictionaryRef xpcEngineUpdate(CFTypeRef target, SecAssessmentFlags flags, CFDictionaryRef context)
{
	Message msg("update");
	// target can be NULL, a CFURLRef, a SecRequirementRef, or a CFNumberRef
	if (target) {
		if (CFGetTypeID(target) == CFNumberGetTypeID())
			xpc_dictionary_set_uint64(msg, "rule", cfNumber<int64_t>(CFNumberRef(target)));
		else if (CFGetTypeID(target) == CFURLGetTypeID()) {
			bool good = precheckAccess(CFURLRef(target), context);
            if (!good)      // work around <rdar://problem/26075034>
                return makeCFDictionary(0); // pretend this worked
			xpc_dictionary_set_string(msg, "url", cfString(CFURLRef(target)).c_str());
		} else if (CFGetTypeID(target) == SecRequirementGetTypeID()) {
			CFRef<CFDataRef> data;
			MacOSError::check(SecRequirementCopyData(SecRequirementRef(target), kSecCSDefaultFlags, &data.aref()));
			xpc_dictionary_set_data(msg, "requirement", CFDataGetBytePtr(data), CFDataGetLength(data));
		} else
			MacOSError::throwMe(errSecCSInvalidObjectRef);
	}
	xpc_dictionary_set_uint64(msg, "flags", flags);
	CFRef<CFMutableDictionaryRef> ctx = makeCFMutableDictionary();
	if (context)
		CFDictionaryApplyFunction(context, copyCFDictionary, ctx);
	AuthorizationRef localAuthorization = NULL;
	if (CFDictionaryGetValue(ctx, kSecAssessmentUpdateKeyAuthorization) == NULL) {	// no caller-provided authorization
		MacOSError::check(AuthorizationCreate(NULL, NULL, kAuthorizationFlagDefaults, &localAuthorization));
		AuthorizationExternalForm extForm;
		MacOSError::check(AuthorizationMakeExternalForm(localAuthorization, &extForm));
		CFDictionaryAddValue(ctx, kSecAssessmentUpdateKeyAuthorization, CFTempData(&extForm, sizeof(extForm)));
	}
	CFRef<CFDataRef> contextData = makeCFData(CFDictionaryRef(ctx));
	xpc_dictionary_set_data(msg, "context", CFDataGetBytePtr(contextData), CFDataGetLength(contextData));
	
	msg.send();

	if (localAuthorization) {
		AuthorizationFree(localAuthorization, kAuthorizationFlagDefaults);
	}
	
	if (int64_t error = xpc_dictionary_get_int64(msg, "error")) {
		MacOSError::throwMe((int)error);
	}
	
	size_t resultLength;
	const void *resultData = xpc_dictionary_get_data(msg, "result", &resultLength);
	return makeCFDictionaryFrom(resultData, resultLength);
}


bool xpcEngineControl(const char *control)
{
	Message msg("control");
	xpc_dictionary_set_string(msg, "control", control);
	msg.send();
	return true;
}


void xpcEngineRecord(CFDictionaryRef info)
{
	Message msg("record");
	CFRef<CFDataRef> infoData = makeCFData(CFDictionaryRef(info));
	xpc_dictionary_set_data(msg, "info", CFDataGetBytePtr(infoData), CFDataGetLength(infoData));

	msg.send();
}

void xpcEngineCheckDevID(CFBooleanRef* result)
{
    Message msg("check-dev-id");

    msg.send();

    if (int64_t error = xpc_dictionary_get_int64(msg, "error")) {
        MacOSError::throwMe((int)error);
    }

    *result = xpc_dictionary_get_bool(msg,"result") ? kCFBooleanTrue : kCFBooleanFalse;
}

void xpcEngineCheckNotarized(CFBooleanRef* result)
{
	Message msg("check-notarized");

	msg.send();

	if (int64_t error = xpc_dictionary_get_int64(msg, "error")) {
		MacOSError::throwMe((int)error);
	}

	*result = xpc_dictionary_get_bool(msg,"result") ? kCFBooleanTrue : kCFBooleanFalse;
}

void xpcEngineTicketRegister(CFDataRef ticketData)
{
	Message msg("ticket-register");
	xpc_dictionary_set_data(msg, "ticketData", CFDataGetBytePtr(ticketData), CFDataGetLength(ticketData));

	msg.send();

	if (int64_t error = xpc_dictionary_get_int64(msg, "error")) {
		MacOSError::throwMe((int)error);
	}
}

void xpcEngineTicketLookup(CFDataRef hashData, SecCSDigestAlgorithm hashType, SecAssessmentTicketFlags flags, double *date)
{
	Message msg("ticket-lookup");
	xpc_dictionary_set_data(msg, "hashData", CFDataGetBytePtr(hashData), CFDataGetLength(hashData));
	xpc_dictionary_set_uint64(msg, "hashType", hashType);
	xpc_dictionary_set_uint64(msg, "flags", flags);

	msg.send();

	if (int64_t error = xpc_dictionary_get_int64(msg, "error")) {
		MacOSError::throwMe((int)error);
	}

	double local_date = xpc_dictionary_get_double(msg, "date");
	if (date && !isnan(local_date)) {
		*date = local_date;
	}
}

void xpcEngineLegacyCheck(CFDataRef hashData, SecCSDigestAlgorithm hashType, CFStringRef teamID)
{
	Message msg("legacy-check");
	xpc_dictionary_set_data(msg, "hashData", CFDataGetBytePtr(hashData), CFDataGetLength(hashData));
	xpc_dictionary_set_uint64(msg, "hashType", hashType);

	// There may not be a team id, so just leave it off if there isn't since xpc_dictionary_set_string
	// will return a NULL if the value isn't provided.
	if (teamID) {
		xpc_dictionary_set_string(msg, "teamID", CFStringGetCStringPtr(teamID, kCFStringEncodingUTF8));
	}

	msg.send();

	if (int64_t error = xpc_dictionary_get_int64(msg, "error")) {
		MacOSError::throwMe((int)error);
	}
}

void xpcEngineEnable(void)
{
	Message msg("enable");
	
	msg.send();
	
	if (int64_t error = xpc_dictionary_get_int64(msg, "error")) {
		MacOSError::throwMe((int)error);
	}
}

void xpcEngineDisable(void)
{
	Message msg("disable");
	
	msg.send();
	
	if (int64_t error = xpc_dictionary_get_int64(msg, "error")) {
		MacOSError::throwMe((int)error);
	}
}

} // end namespace CodeSigning
} // end namespace Security
