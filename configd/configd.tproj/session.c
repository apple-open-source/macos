/*
 * Copyright (c) 2000, 2001, 2003-2005, 2007 Apple Inc. All rights reserved.
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

/* information maintained for each active session */
static serverSessionRef	*sessions = NULL;
static int		nSessions = 0;


__private_extern__
serverSessionRef
getSession(mach_port_t server)
{
	int	i;

	if (server == MACH_PORT_NULL) {
		SCLog(TRUE, LOG_NOTICE, CFSTR("Excuse me, why is getSession() being called with an invalid port?"));
		return NULL;
	}

	for (i = 0; i < nSessions; i++) {
		serverSessionRef	thisSession = sessions[i];

		if (thisSession == NULL) {
			/* found an empty slot, skip it */
			continue;
		} else if (thisSession->key == server) {
			return thisSession;	/* we've seen this server before */
		} else if (thisSession->store &&
				      (((SCDynamicStorePrivateRef)thisSession->store)->notifySignalTask == server)) {
			return thisSession;
		}
	}

	/* no sessions available */
	return NULL;
}


__private_extern__
serverSessionRef
addSession(mach_port_t server, CFStringRef (*copyDescription)(const void *info))
{
	CFMachPortContext	context	= { 0, NULL, NULL, NULL, NULL };
	int			n = -1;

	if (nSessions <= 0) {
		/* new session (actually, the first) found */
		sessions = malloc(sizeof(serverSessionRef));
		n = 0;
		nSessions = 1;
	} else {
		int	i;

		for (i = 0; i < nSessions; i++) {
			if (sessions[i] == NULL) {
				/* found an empty slot, use it */
				n = i;
				break;
			}
		}
		/* new session identified */
		if (n < 0) {
			/* no empty slots, add one to the list */
			n = nSessions++;
			sessions = reallocf(sessions, ((nSessions) * sizeof(serverSessionRef)));
		}
	}

	// allocate a new session for this server
	sessions[n] = malloc(sizeof(serverSession));
	bzero(sessions[n], sizeof(serverSession));

	// create server port
	context.info		= sessions[n];
	context.copyDescription	= copyDescription;

	if (server == MACH_PORT_NULL) {
		// SCDynamicStore client ports 
		(void) mach_port_allocate(mach_task_self(),
					  MACH_PORT_RIGHT_RECEIVE,
					  &server);
		(void) mach_port_insert_right(mach_task_self(),
					      server,
					      server,
					      MACH_MSG_TYPE_MAKE_SEND);
	}
	sessions[n]->key		 = server;
	sessions[n]->serverPort		 = CFMachPortCreateWithPort(NULL,
								    server,
								    configdCallback,
								    &context,
								    NULL);
//	sessions[n]->serverRunLoopSource = NULL;
//	sessions[n]->store		 = NULL;
	sessions[n]->callerEUID          = 1;		/* not "root" */

	return sessions[n];
}


__private_extern__
void
removeSession(mach_port_t server)
{
	int			i;
	serverSessionRef	thisSession;
	CFStringRef		sessionKey;

	for (i = 0; i < nSessions; i++) {
		thisSession = sessions[i];

		if (thisSession == NULL) {
			/* found an empty slot, skip it */
			continue;
		} else if (thisSession->key == server) {
			/*
			 * We don't need any remaining information in the
			 * sessionData dictionary, remove it.
			 */
			sessionKey = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d"), server);
			CFDictionaryRemoveValue(sessionData, sessionKey);
			CFRelease(sessionKey);

			/*
			 * Lastly, get rid of the per-session structure.
			 */
			free(thisSession);
			sessions[i] = NULL;

			return;
		}
	}

	return;
}


__private_extern__
void
cleanupSession(mach_port_t server)
{
	int		i;

	for (i = 0; i < nSessions; i++) {
		serverSessionRef	thisSession = sessions[i];

		if ((thisSession != NULL) && (thisSession->key == server)) {
			/*
			 * session entry still exists.
			 */

			if (_configd_trace) {
				SCTrace(TRUE, _configd_trace, CFSTR("cleanup : %5d\n"), server);
			}

			/*
			 * Ensure that any changes made while we held the "lock"
			 * are discarded.
			 */
			if ((storeLocked > 0) &&
			    ((SCDynamicStorePrivateRef)thisSession->store)->locked) {
				/*
				 * swap store and associated data which, after
				 * being closed, will result in the restoration
				 * of the original pre-"locked" data.
				 */
				_swapLockedStoreData();
			}

			/*
			 * Close any open connections including cancelling any outstanding
			 * notification requests and releasing any locks.
			 */
			(void) __SCDynamicStoreClose(&thisSession->store, TRUE);

			/*
			 * Our send right has already been removed. Remove our
			 * receive right.
			 */
			mach_port_mod_refs(mach_task_self(),
					   thisSession->key,
					   MACH_PORT_RIGHT_RECEIVE,
					   -1);
			
			/*
			 * Lastly, remove the session entry.
			 */
			removeSession(server);

			return;
		}
	}
	return;
}


__private_extern__
void
listSessions()
{
	int	i;

	fprintf(stderr, "Current sessions:");
	for (i = 0; i < nSessions; i++) {
		serverSessionRef	thisSession = sessions[i];

		if (thisSession == NULL) {
			continue;
		}

		fprintf(stderr, " %d", thisSession->key);

		if (thisSession->store) {
			task_t	task = ((SCDynamicStorePrivateRef)thisSession->store)->notifySignalTask;

			if (task != TASK_NULL) {
			       fprintf(stderr, "/%d", task);
			}
		}
	}
	fprintf(stderr, "\n");
}

