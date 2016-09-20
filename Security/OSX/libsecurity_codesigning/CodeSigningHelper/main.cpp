/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
#include <Security/CodeSigning.h>
#include <Security/SecCodePriv.h>
#include <xpc/xpc.h>
#include <sandbox.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/cfmunge.h>
#include <security_utilities/logging.h>
#include "codedirectory.h"



static void
request(xpc_connection_t peer, xpc_object_t event)
{
	OSStatus rc;
	
	pid_t pid = (pid_t)xpc_dictionary_get_int64(event, "pid");
	if (pid <= 0)
		return;
	
	xpc_object_t reply = xpc_dictionary_create_reply(event);
	if (reply == NULL)
		return;
	
	CFTemp<CFDictionaryRef> attributes("{%O=%d}", kSecGuestAttributePid, pid);
	CFRef<SecCodeRef> code;
	if ((rc = SecCodeCopyGuestWithAttributes(NULL, attributes, kSecCSDefaultFlags, &code.aref())) == noErr) {
		
		// path to base of client code
		CFRef<CFURLRef> codePath;
		if ((rc = SecCodeCopyPath(code, kSecCSDefaultFlags, &codePath.aref())) == noErr) {
			CFRef<CFDataRef> data = CFURLCreateData(NULL, codePath, kCFStringEncodingUTF8, true);
			xpc_dictionary_set_data(reply, "bundleURL", CFDataGetBytePtr(data), CFDataGetLength(data));
		}
		
		// if the caller wants the Info.plist, get it and verify the hash passed by the caller
		size_t iphLength;
		if (const void *iphash = xpc_dictionary_get_data(event, "infohash", &iphLength)) {
			if (CFRef<CFDataRef> data = SecCodeCopyComponent(code, Security::CodeSigning::cdInfoSlot, CFTempData(iphash, iphLength))) {
				xpc_dictionary_set_data(reply, "infoPlist", CFDataGetBytePtr(data), CFDataGetLength(data));
			}
		}
	}
	xpc_connection_send_message(peer, reply);
	xpc_release(reply);
}


static void CodeSigningHelper_peer_event_handler(xpc_connection_t peer, xpc_object_t event)
{
	xpc_type_t type = xpc_get_type(event);
	if (type == XPC_TYPE_ERROR)
		return;
	
	assert(type == XPC_TYPE_DICTIONARY);
	
	const char *cmd = xpc_dictionary_get_string(event, "command");
	if (cmd == NULL) {
		xpc_connection_cancel(peer);
	} else if (strcmp(cmd, "fetchData") == 0)
		request(peer, event);
	else {
		Syslog::error("peer sent invalid command %s", cmd);
		xpc_connection_cancel(peer);
	}
}


static void CodeSigningHelper_event_handler(xpc_connection_t peer)
{
	xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
		CodeSigningHelper_peer_event_handler(peer, event);
	});
	xpc_connection_resume(peer);
}

int main(int argc, const char *argv[])
{
	char *error = NULL;
	if (sandbox_init("com.apple.CodeSigningHelper", SANDBOX_NAMED, &error)) {
		Syslog::error("failed to enter sandbox: %s", error);
		exit(EXIT_FAILURE);
	}
	xpc_main(CodeSigningHelper_event_handler);
	return 0;
}
