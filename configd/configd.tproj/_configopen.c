/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "configd.h"
#include "configd_server.h"
#include "session.h"

SCDStatus
_SCDOpen(SCDSessionRef *session, CFStringRef name)
{
	SCDSessionPrivateRef	sessionPrivate;

	SCDLog(LOG_DEBUG, CFSTR("_SCDOpen:"));
	SCDLog(LOG_DEBUG, CFSTR("  name = %@"), name);

	/*
	 * allocate and initialize a new session
	 */
	sessionPrivate = (SCDSessionPrivateRef)_SCDSessionCreatePrivate();
	*session       = (SCDSessionRef)sessionPrivate;

	/*
	 * If necessary, initialize the cache and session data dictionaries
	 */
	if (cacheData == NULL) {
		cacheData          = CFDictionaryCreateMutable(NULL,
							       0,
							       &kCFTypeDictionaryKeyCallBacks,
							       &kCFTypeDictionaryValueCallBacks);
		sessionData        = CFDictionaryCreateMutable(NULL,
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

	return SCD_OK;
}


kern_return_t
_configopen(mach_port_t			server,
	    xmlData_t			nameRef,		/* raw XML bytes */
	    mach_msg_type_number_t	nameLen,
	    mach_port_t			*newServer,
	    int				*scd_status)
{
	kern_return_t 		status;
	serverSessionRef	mySession, newSession;
	CFDataRef		xmlName;	/* name (XML serialized) */
	CFStringRef		name;		/* name (un-serialized) */
	CFStringRef		xmlError;
	mach_port_t             oldNotify;
	CFStringRef		sessionKey;
	CFDictionaryRef		info;
	CFMutableDictionaryRef	newInfo;
	CFMachPortRef		mp;

	SCDLog(LOG_DEBUG, CFSTR("Open new session."));
	SCDLog(LOG_DEBUG, CFSTR("  server = %d"), server);

	/* un-serialize the name */
	xmlName = CFDataCreate(NULL, nameRef, nameLen);
	status = vm_deallocate(mach_task_self(), (vm_address_t)nameRef, nameLen);
	if (status != KERN_SUCCESS) {
		CFRelease(xmlName);
		SCDLog(LOG_DEBUG, CFSTR("vm_deallocate(): %s"), mach_error_string(status));
		/* non-fatal???, proceed */
	}
	name = CFPropertyListCreateFromXMLData(NULL,
					       xmlName,
					       kCFPropertyListImmutable,
					       &xmlError);
	CFRelease(xmlName);
	if (xmlError) {
		SCDLog(LOG_DEBUG, CFSTR("CFPropertyListCreateFromXMLData() name: %s"), xmlError);
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
	}

	mySession = getSession(server);
	if (mySession->session) {
		CFRelease(name);
		SCDLog(LOG_DEBUG, CFSTR("  Sorry, this session is already open."));
		*scd_status = SCD_FAILED;	/* you can't re-open an "open" session */
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

	*scd_status = _SCDOpen(&newSession->session, name);

	/*
	 * Make the server port accessible to the framework routines.
	 */
	((SCDSessionPrivateRef)newSession->session)->server = *newServer;

	/* Request a notification when/if the client dies */
	status = mach_port_request_notification(mach_task_self(),
						*newServer,
						MACH_NOTIFY_NO_SENDERS,
						1,
						*newServer,
						MACH_MSG_TYPE_MAKE_SEND_ONCE,
						&oldNotify);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("mach_port_request_notification(): %s"), mach_error_string(status));
		CFRelease(name);
		cleanupSession(*newServer);
		*newServer = MACH_PORT_NULL;
		*scd_status = SCD_FAILED;
		return KERN_SUCCESS;
       }

#ifdef	DEBUG
	if (oldNotify != MACH_PORT_NULL) {
		SCDLog(LOG_DEBUG, CFSTR("_configopen(): why is oldNotify != MACH_PORT_NULL?"));
	}
#endif	/* DEBUG */

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
