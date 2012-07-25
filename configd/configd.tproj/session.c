/*
 * Copyright (c) 2000, 2001, 2003-2005, 2007-2011 Apple Inc. All rights reserved.
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

#include <unistd.h>
#include <bsm/libbsm.h>
#include <sandbox.h>


/* information maintained for each active session */
static serverSessionRef	*sessions	= NULL;
static int		nSessions	= 0;	/* # of allocated sessions */
static int		lastSession	= -1;	/* # of last used session */

/* CFMachPortInvalidation runloop */
static CFRunLoopRef	sessionRunLoop	= NULL;

/* temp session */
static serverSessionRef	temp_session	= NULL;


static void
CFMachPortInvalidateSessionCallback(CFMachPortRef port, void *info)
{
	CFRunLoopRef	currentRunLoop	= CFRunLoopGetCurrent();

	// Bear trap
	if (!_SC_CFEqual(currentRunLoop, sessionRunLoop)) {
		_SC_crash("SCDynamicStore CFMachPort invalidation error",
			   CFSTR("CFMachPort invalidated"),
			   CFSTR("An SCDynamicStore CFMachPort has incorrectly been invalidated."));
	}
}


__private_extern__
serverSessionRef
getSession(mach_port_t server)
{
	int	i;

	if (server == MACH_PORT_NULL) {
		SCLog(TRUE, LOG_ERR, CFSTR("Excuse me, why is getSession() being called with an invalid port?"));
		return NULL;
	}

	/* look for matching session (note: slot 0 is the "server" port) */
	for (i = 1; i <= lastSession; i++) {
		serverSessionRef	thisSession = sessions[i];

		if (thisSession == NULL) {
			/* found an empty slot, skip it */
			continue;
		}

		if (thisSession->key == server) {
			/* we've seen this server before */
			return thisSession;
		}

		if ((thisSession->store != NULL) &&
		    (((SCDynamicStorePrivateRef)thisSession->store)->notifySignalTask == server)) {
			/* we've seen this task port before */
			return thisSession;
		}
	}

	/* no sessions available */
	return NULL;
}


__private_extern__
serverSessionRef
tempSession(mach_port_t server, CFStringRef name, audit_token_t auditToken)
{
	static dispatch_once_t		once;
	SCDynamicStorePrivateRef	storePrivate;

	if (sessions[0]->key != server) {
		// if not SCDynamicStore "server" port
		return NULL;
	}

	dispatch_once(&once, ^{
		temp_session = sessions[0];	/* use "server" session */
		(void) __SCDynamicStoreOpen(&temp_session->store, NULL);
	});

	/* save audit token */
	temp_session->auditToken        = auditToken;
	temp_session->callerEUID        = -1;		/* not "root" */
	temp_session->callerRootAccess  = UNKNOWN;
	temp_session->callerWriteAccess = UNKNOWN;

	/* save name */
	storePrivate = (SCDynamicStorePrivateRef)temp_session->store;
	if (storePrivate->name != NULL) CFRelease(storePrivate->name);
	storePrivate->name = CFRetain(name);

	return temp_session;
}


__private_extern__
serverSessionRef
addSession(mach_port_t server, CFStringRef (*copyDescription)(const void *info))
{
	CFMachPortContext	context	= { 0, NULL, NULL, NULL, NULL };
	mach_port_t		mp	= server;
	int			n	= -1;

	/* save current (SCDynamicStore) runloop */
	if (sessionRunLoop == NULL) {
		sessionRunLoop = CFRunLoopGetCurrent();
	}

	if (nSessions <= 0) {
		/* if first session (the "server" port) */
		n = 0;		/* use slot "0" */
		lastSession = 0;	/* last used slot */

		nSessions = 64;
		sessions = malloc(nSessions * sizeof(serverSessionRef));
	} else {
		int	i;

		/* check to see if we already have an open session (note: slot 0 is the "server" port) */
		for (i = 1; i <= lastSession; i++) {
			serverSessionRef	thisSession = sessions[i];

			if (thisSession == NULL) {
				/* found an empty slot */
				if (n < 0) {
					/* keep track of the first [empty] slot */
					n = i;
				}

				/* and keep looking for a matching session */
				continue;
			}

			if (thisSession->key == server) {
				/* we've seen this server before */
				return NULL;
			}

			if ((thisSession->store != NULL) &&
				   (((SCDynamicStorePrivateRef)thisSession->store)->notifySignalTask == server)) {
				/* we've seen this task port before */
				return NULL;
			}
		}

		/* add a new session */
		if (n < 0) {
			/* if no empty slots */
			n = ++lastSession;
			if (lastSession >= nSessions) {
				/* expand the session list */
				nSessions *= 2;
				sessions = reallocf(sessions, (nSessions * sizeof(serverSessionRef)));
			}
		}

		// create mach port for SCDynamicStore client
		mp = MACH_PORT_NULL;
		(void) mach_port_allocate(mach_task_self(),
					  MACH_PORT_RIGHT_RECEIVE,
					  &mp);
	}

	// allocate a new session for this server
	sessions[n] = malloc(sizeof(serverSession));
	bzero(sessions[n], sizeof(serverSession));

	// create server port
	context.info		 = sessions[n];
	context.copyDescription = copyDescription;

	//
	// Note: we create the CFMachPort *before* we insert a send
	//       right present to ensure that CF does not establish
	//       its dead name notification.
	//
	sessions[n]->serverPort = _SC_CFMachPortCreateWithPort("SCDynamicStore/session",
							       mp,
							       configdCallback,
							       &context);

	//
	// Set bear trap (an invalidation callback) to catch other
	// threads stomping on us
	//
	CFMachPortSetInvalidationCallBack(sessions[n]->serverPort,
					  CFMachPortInvalidateSessionCallback);

	if (n > 0) {
		// insert send right that will be moved to the client
		(void) mach_port_insert_right(mach_task_self(),
					      mp,
					      mp,
					      MACH_MSG_TYPE_MAKE_SEND);
	}

	sessions[n]->key		 = mp;
//	sessions[n]->serverRunLoopSource = NULL;
//	sessions[n]->store		 = NULL;
	sessions[n]->callerEUID          = 1;           /* not "root" */
	sessions[n]->callerRootAccess	 = UNKNOWN;
	sessions[n]->callerWriteAccess	 = UNKNOWN;

	return sessions[n];
}


__private_extern__
void
cleanupSession(mach_port_t server)
{
	int		i;

	for (i = 1; i <= lastSession; i++) {
		CFStringRef		sessionKey;
		serverSessionRef	thisSession = sessions[i];

		if (thisSession == NULL) {
			/* found an empty slot, skip it */
			continue;
		}

		if (thisSession->key == server) {
			/*
			 * session entry still exists.
			 */

			if (_configd_trace) {
				SCTrace(TRUE, _configd_trace, CFSTR("cleanup : %5d\n"), server);
			}

			/*
			 * Close any open connections including cancelling any outstanding
			 * notification requests and releasing any locks.
			 */
			__MACH_PORT_DEBUG(TRUE, "*** cleanupSession", server);
			(void) __SCDynamicStoreClose(&thisSession->store);
			__MACH_PORT_DEBUG(TRUE, "*** cleanupSession (after __SCDynamicStoreClose)", server);

			/*
			 * Our send right has already been removed. Remove our receive right.
			 */
			(void) mach_port_mod_refs(mach_task_self(), server, MACH_PORT_RIGHT_RECEIVE, -1);

			/*
			 * We don't need any remaining information in the
			 * sessionData dictionary, remove it.
			 */
			sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), server);
			CFDictionaryRemoveValue(sessionData, sessionKey);
			CFRelease(sessionKey);

			/*
			 * get rid of the per-session structure.
			 */
			free(thisSession);
			sessions[i] = NULL;

			if (i == lastSession) {
				/* we are removing the last session, update last used slot */
				while (--lastSession > 0) {
					if (sessions[lastSession] != NULL) {
						break;
					}
				}
			}

			return;
		}
	}

	SCLog(TRUE, LOG_ERR, CFSTR("MACH_NOTIFY_NO_SENDERS w/no session, port = %d"), server);
	__MACH_PORT_DEBUG(TRUE, "*** cleanupSession w/no session", server);
	return;
}


__private_extern__
void
listSessions(FILE *f)
{
	int	i;

	SCPrint(TRUE, f, CFSTR("Current sessions :\n"));
	for (i = 0; i <= lastSession; i++) {
		serverSessionRef	thisSession = sessions[i];

		if (thisSession == NULL) {
			continue;
		}

		SCPrint(TRUE, f, CFSTR("\t%d : port = 0x%x"), i, thisSession->key);

		if (thisSession->store != NULL) {
			SCDynamicStorePrivateRef	storePrivate	= (SCDynamicStorePrivateRef)thisSession->store;

			if (storePrivate->notifySignalTask != TASK_NULL) {
			       SCPrint(TRUE, f, CFSTR(", task = %d"), storePrivate->notifySignalTask);
			}
		}

		if (sessionData != NULL) {
			CFDictionaryRef	info;
			CFStringRef	key;

			key = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), thisSession->key);
			info = CFDictionaryGetValue(sessionData, key);
			CFRelease(key);
			if (info != NULL) {
				CFStringRef	name;

				name = CFDictionaryGetValue(info, kSCDName);
				if (name != NULL) {
					SCPrint(TRUE, f, CFSTR(", name = %@"), name);
				}
			}
		}

		if (thisSession->serverPort != NULL) {
			SCPrint(TRUE, f, CFSTR("\n\t\t%@"), thisSession->serverPort);
		}

		if (thisSession->serverRunLoopSource != NULL) {
			SCPrint(TRUE, f, CFSTR("\n\t\t%@"), thisSession->serverRunLoopSource);
		}

		SCPrint(TRUE, f, CFSTR("\n"));
	}

	SCPrint(TRUE, f, CFSTR("\n"));
	return;
}


#if	TARGET_OS_IPHONE || (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1070)

#include <Security/Security.h>
#include <Security/SecTask.h>

static CFStringRef
sessionName(serverSessionRef session)
{
	CFDictionaryRef	info;
	CFStringRef	name	= NULL;
	CFStringRef	sessionKey;

	sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), session->key);
	info = CFDictionaryGetValue(sessionData, sessionKey);
	CFRelease(sessionKey);

	if (info != NULL) {
		name = CFDictionaryGetValue(info, kSCDName);
	}

	return (name != NULL) ? name : CFSTR("???");
}


static Boolean
hasEntitlement(serverSessionRef session, CFStringRef entitlement)
{
	Boolean		hasEntitlement	= FALSE;
	SecTaskRef	task;

	/* Create the security task from the audit token. */
	task = SecTaskCreateWithAuditToken(NULL, session->auditToken);
	if (task != NULL) {
		CFErrorRef	error	= NULL;
		CFTypeRef	value;

		/* Get the value for the entitlement. */
		value = SecTaskCopyValueForEntitlement(task, entitlement, &error);
		if (value != NULL) {
			if (isA_CFBoolean(value)) {
				if (CFBooleanGetValue(value)) {
					/* if client DOES have entitlement */
					hasEntitlement = TRUE;
				}
			} else {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("hasEntitlement: entitlement not valid: %@"),
				      sessionName(session));
			}

			CFRelease(value);
		}
		if (error != NULL) {
			SCLog(TRUE,
			      (value == NULL) ? LOG_ERR : LOG_DEBUG,
			      CFSTR("hasEntitlement SecTaskCopyValueForEntitlement() %s, error domain=%@, error code=%lx"),
			      (value == NULL) ? "failed" : "warned",
			      CFErrorGetDomain(error),
			      CFErrorGetCode(error));
			CFRelease(error);
		}

		CFRelease(task);
	} else {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("hasEntitlement SecTaskCreateWithAuditToken() failed: %@"),
		      sessionName(session));
	}

	return hasEntitlement;
}

#endif  // TARGET_OS_IPHONE || (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1070)


__private_extern__
Boolean
hasRootAccess(serverSessionRef session)
{
	if (session->callerRootAccess == UNKNOWN) {
		/*
		 * get the credentials associated with the caller.
		 */
		audit_token_to_au32(session->auditToken,
				    NULL,			// auidp
				    &session->callerEUID,	// euid
				    NULL,			// egid
				    NULL,			// ruid
				    NULL,			// rgid
				    NULL,			// pid
				    NULL,			// asid
				    NULL);			// tid

		session->callerRootAccess = (session->callerEUID == 0) ? YES : NO;
	}

	return (session->callerRootAccess == YES) ? TRUE : FALSE;
}


__private_extern__
Boolean
hasWriteAccess(serverSessionRef session)
{
	if (session->callerWriteAccess == UNKNOWN) {
		/* assume that the client DOES NOT have the entitlement */
		session->callerWriteAccess = NO;

		if (hasRootAccess(session)) {
			// grant write access to eUID==0 processes
			session->callerWriteAccess = YES;
		}
#if	TARGET_OS_IPHONE || (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1070)
		else if (hasEntitlement(session, kSCWriteEntitlementName)) {
			// grant write access to "entitled" processes
			session->callerWriteAccess = YES;
		}
#endif  // TARGET_OS_IPHONE || (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1070)
	}

	return (session->callerWriteAccess == YES) ? TRUE : FALSE;
}


__private_extern__
Boolean
hasPathAccess(serverSessionRef session, const char *path)
{
	pid_t	pid;
	char	realPath[PATH_MAX];

	if (realpath(path, realPath) == NULL) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("hasPathAccess realpath() failed: %s"), strerror(errno));
		return FALSE;
	}

	audit_token_to_au32(session->auditToken,
			    NULL,		// auidp
			    NULL,		// euid
			    NULL,		// egid
			    NULL,		// ruid
			    NULL,		// rgid
			    &pid,		// pid
			    NULL,		// asid
			    NULL);		// tid

	if (sandbox_check(pid, "file-write-data", SANDBOX_FILTER_PATH, realPath) > 0) {
		SCLog(TRUE, LOG_DEBUG, CFSTR("hasPathAccess sandbox access denied: %s"), strerror(errno));
		return FALSE;
	}

	return TRUE;
}
