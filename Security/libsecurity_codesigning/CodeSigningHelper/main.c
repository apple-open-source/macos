/*
 * Copyright (c) 2012 Apple Computer, Inc. All Rights Reserved.
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

/*
 * A simple XPCService that returns the Info.plist for a specific pid,
 * the use-case is is for service that is not running as the user or
 * in a sandbox and can't access the file directly.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <sys/param.h>
#include <xpc/xpc.h>
#include <syslog.h>
#include <assert.h>
#include <libproc.h>
#include <sandbox.h>
#include <syslog.h>

static CFDataRef
CopyDataFromURL(CFURLRef url)
{
	CFReadStreamRef stream = CFReadStreamCreateWithFile(NULL, url);
	if (stream == NULL)
		return NULL;

	if (!CFReadStreamOpen(stream)) {
		CFRelease(stream);
		return NULL;
	}

        CFMutableDataRef data = CFDataCreateMutable(NULL, 0);
        if (data == NULL) {
                CFRelease(stream);
                return NULL;
        }

        UInt8 buf[4096];

        while (1) {
                /* limit to 100k */
                if (CFDataGetLength(data) > 100 * 1024) {
	       		syslog(LOG_ERR, "refusing to handle back Info.plist that is more then 100K");
                        CFRelease(stream);
                        CFRelease(data);
                        return NULL;
                }
                CFIndex readBytes = CFReadStreamRead(stream, buf, sizeof(buf));
                if (readBytes == 0) {
                        break;
                } else if (readBytes <= 0) {
                        CFRelease(data);
                        CFRelease(stream);
                        return NULL;
                }

                assert(readBytes <= sizeof(buf));
                CFDataAppendBytes(data, (void *)buf, readBytes);
        }

        CFReadStreamClose(stream);
        CFRelease(stream);

        return data;
}

static void
fetchData(xpc_connection_t peer, xpc_object_t event)
{
	CFBundleRef bundle = NULL;
        char path[MAXPATHLEN];
        pid_t pid;

        pid = (pid_t)xpc_dictionary_get_int64(event, "pid");
        if (pid <= 0)
                return;

        xpc_object_t reply = xpc_dictionary_create_reply(event);
        if (reply == NULL)
                return;

        if (proc_pidpath(pid, path, sizeof(path)) == 0) {
                xpc_dictionary_set_string(reply, "error", "no process for that pid");
                goto send;
        }
        path[sizeof(path) - 1] = '\0';

        CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const uint8_t *)path, strlen(path), 0);
        if (url == NULL) {
                xpc_dictionary_set_string(reply, "error", "failed to create URL");
                goto send;
        }

        bundle = _CFBundleCreateWithExecutableURLIfMightBeBundle(NULL, url);
        CFRelease(url);
        if (bundle == NULL) {
                xpc_dictionary_set_string(reply, "error", "Failed to create a bundle");
                goto send;
        }

        CFURLRef infoPlistURL = _CFBundleCopyInfoPlistURL(bundle);
        if (infoPlistURL == NULL) {
                xpc_dictionary_set_string(reply, "error", "Info.plist missing");
                goto send;
        }

        CFDataRef data = CopyDataFromURL(infoPlistURL);
        CFRelease(infoPlistURL);
        if (data == NULL) {
                xpc_dictionary_set_string(reply, "error", "can't get content of Info.plist");
                goto send;
        }

        xpc_dictionary_set_data(reply, "infoPlist", CFDataGetBytePtr(data), CFDataGetLength(data));
        CFRelease(data);

	CFURLRef bundleURL = CFBundleCopyBundleURL(bundle);
	if (bundleURL == NULL)
		goto send;

	data = CFURLCreateData(NULL, bundleURL, kCFStringEncodingUTF8, true);
	CFRelease(bundleURL);
	if (data == NULL)
		goto send;
	
        xpc_dictionary_set_data(reply, "bundleURL", CFDataGetBytePtr(data), CFDataGetLength(data));
	CFRelease(data);

send:
	if (bundle)
		CFRelease(bundle);
        xpc_connection_send_message(peer, reply);
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
                fetchData(peer, event);
        else {
                syslog(LOG_ERR, "peer sent invalid command %s", cmd);
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
                syslog(LOG_ERR, "failed to enter sandbox: %s", error);
                exit(EXIT_FAILURE);
        }
        xpc_main(CodeSigningHelper_event_handler);
        return 0;
}

