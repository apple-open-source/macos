/*
 * Copyright (c) 2000-2003 Apple Computer, Inc. All rights reserved.
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
 * Modification History
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include "configd.h"
#include "configd_server.h"
#include "session.h"

__private_extern__
int
__SCDynamicStoreOpen(SCDynamicStoreRef *store, CFStringRef name)
{
	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("__SCDynamicStoreOpen:"));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  name = %@"), name);
	}

	/*
	 * allocate and initialize a new session
	 */
	*store = (SCDynamicStoreRef)__SCDynamicStoreCreatePrivate(NULL, name, NULL, NULL);

	/*
	 * If necessary, initialize the store and session data dictionaries
	 */
	if (storeData == NULL) {
		sessionData        = CFDictionaryCreateMutable(NULL,
							       0,
							       &kCFTypeDictionaryKeyCallBacks,
							       &kCFTypeDictionaryValueCallBacks);
		storeData          = CFDictionaryCreateMutable(NULL,
							       0,
							       &kCFTypeDictionaryKeyCallBacks,
							       &kCFTypeDictionaryValueCallBacks);
		patternData        = CFDictionaryCreateMutable(NULL,
							       0,
							       &kCFTypeDictionaryKeyCallBacks,
							       &kCFTypeDictionaryValueCallBacks);
		changedKeys        = CFSetCreateMutable(NULL,
							0,
							&kCFTypeSetCallBacks);
		deferredRemovals   = CFSetCreateMutable(NULL,
							0,
							&kCFTypeSetCallBacks);
		removedSessionKeys = CFSetCreateMutable(NULL,
							0,
							&kCFTypeSetCallBacks);
	}

	return kSCStatusOK;
}


__private_extern__
kern_return_t
_configopen(mach_port_t			server,
	    xmlData_t			nameRef,		/* raw XML bytes */
	    mach_msg_type_number_t	nameLen,
	    mach_port_t			*newServer,
	    int				*sc_status)
{
	kern_return_t 		status;
	serverSessionRef	mySession, newSession;
	CFStringRef		name;		/* name (un-serialized) */
	mach_port_t		oldNotify;
	CFStringRef		sessionKey;
	CFDictionaryRef		info;
	CFMutableDictionaryRef	newInfo;
	CFMachPortRef		mp;

	if (_configd_verbose) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("Open new session."));
		SCLog(TRUE, LOG_DEBUG, CFSTR("  server = %d"), server);
	}

	/* un-serialize the name */
	if (!_SCUnserializeString(&name, NULL, (void *)nameRef, nameLen)) {
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	if (!isA_CFString(name)) {
		CFRelease(name);
		*sc_status = kSCStatusInvalidArgument;
		return KERN_SUCCESS;
	}

	mySession = getSession(server);
	if (mySession->store) {
		CFRelease(name);
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("  Sorry, this session is already open."));
		*sc_status = kSCStatusFailed;	/* you can't re-open an "open" session */
		return KERN_SUCCESS;
	}

	/* Create the server port for this session */
	mp = CFMachPortCreate(NULL, configdCallback, NULL, NULL);

	/* return the newly allocated port to be used for this session */
	*newServer = CFMachPortGetPort(mp);

	/*
	 * establish the new session
	 */
	newSession = addSession(mp);

	/* Create and add a run loop source for the port */
	newSession->serverRunLoopSource = CFMachPortCreateRunLoopSource(NULL, mp, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(),
			   newSession->serverRunLoopSource,
			   kCFRunLoopDefaultMode);

	/*
	 * save the credentials associated with the caller.
	 */
	newSession->callerEUID = mySession->callerEUID;
	newSession->callerEGID = mySession->callerEGID;

	if (_configd_trace) {
		SCTrace(TRUE, _configd_trace, CFSTR("open    : %5d : %@\n"), *newServer, name);
	}

	*sc_status = __SCDynamicStoreOpen(&newSession->store, name);

	/*
	 * Make the server port accessible to the framework routines.
	 */
	((SCDynamicStorePrivateRef)newSession->store)->server = *newServer;

	/* Request a notification when/if the client dies */
	status = mach_port_request_notification(mach_task_self(),
						*newServer,
						MACH_NOTIFY_NO_SENDERS,
						1,
						*newServer,
						MACH_MSG_TYPE_MAKE_SEND_ONCE,
						&oldNotify);
	if (status != KERN_SUCCESS) {
		SCLog(_configd_verbose, LOG_DEBUG, CFSTR("mach_port_request_notification(): %s"), mach_error_string(status));
		CFRelease(name);
		cleanupSession(*newServer);
		*newServer = MACH_PORT_NULL;
		*sc_status = kSCStatusFailed;
		return KERN_SUCCESS;
	}

	if (oldNotify != MACH_PORT_NULL) {
		SCLog(_configd_verbose, LOG_ERR, CFSTR("_configopen(): why is oldNotify != MACH_PORT_NULL?"));
	}

	/*
	 * Save the name of the calling application / plug-in with the session data.
	 */
	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), *newServer);
	info = CFDictionaryGetValue(sessionData, sessionKey);
	if (info) {
		newInfo = CFDictionaryCreateMutableCopy(NULL, 0, info);
	} else {
		newInfo = CFDictionaryCreateMutable(NULL,
						    0,
						    &kCFTypeDictionaryKeyCallBacks,
						    &kCFTypeDictionaryValueCallBacks);
	}
	CFDictionarySetValue(newInfo, kSCDName, name);
	CFRelease(name);
	CFDictionarySetValue(sessionData, sessionKey, newInfo);
	CFRelease(newInfo);
	CFRelease(sessionKey);

	return KERN_SUCCESS;
}
