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

/* information maintained for each active session */
static serverSessionRef	*sessions = NULL;
static int		nSessions = 0;


serverSessionRef
getSession(mach_port_t server)
{
	int	i;

	if (server == MACH_PORT_NULL) {
		SCDLog(LOG_NOTICE, CFSTR("Excuse me, why is getSession() being called with an invalid port?"));
		return NULL;
	}

	if (nSessions > 0) {
		for (i=0; i<nSessions; i++) {
			serverSessionRef	thisSession = sessions[i];

			if (thisSession == NULL) {
				/* found an empty slot, skip it */
				continue;
			} else if (thisSession->key == server) {
				return thisSession;	/* we've seen this server before */
			} else if ((thisSession->session != NULL) &&
				   (((SCDSessionPrivateRef)thisSession->session)->notifySignalTask == server)) {
					return thisSession;
			}
		}
	}

	/* no sessions available */
	return NULL;
}


serverSessionRef
addSession(CFMachPortRef server)
{
	int	i;
	int	n = -1;

	if (nSessions <= 0) {
		/* new session (actually, the first) found */
		sessions = malloc(sizeof(serverSessionRef));
		n = 0;
		nSessions = 1;
	} else {
		for (i=0; i<nSessions; i++) {
			if (sessions[i] == NULL) {
				/* found an empty slot, use it */
				n = i;
			}
		}
		/* new session identified */
		if (n < 0) {
			/* no empty slots, add one to the list */
			n = nSessions++;
			sessions = realloc(sessions, ((nSessions) * sizeof(serverSessionRef)));
		}
	}

	SCDLog(LOG_DEBUG, CFSTR("Allocating new session for port %d"), CFMachPortGetPort(server));
	sessions[n] = malloc(sizeof(serverSession));
	sessions[n]->key                 = CFMachPortGetPort(server);
	sessions[n]->serverPort          = server;
	sessions[n]->serverRunLoopSource = NULL;
	sessions[n]->session             = NULL;
	sessions[n]->callerEUID          = 1;		/* not "root" */
	sessions[n]->callerEGID          = 1;		/* not "wheel" */

	return sessions[n];
}


void
removeSession(mach_port_t server)
{
	int			i;
	serverSessionRef	thisSession;
	CFStringRef		sessionKey;

	for (i=0; i<nSessions; i++) {
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


void
cleanupSession(mach_port_t server)
{
	int		i;

	for (i=0; i<nSessions; i++) {
		serverSessionRef	thisSession = sessions[i];

		if ((thisSession != NULL) && (thisSession->key == server)) {
			/*
			 * session entry still exists.
			 */

			/*
			 * Ensure that any changes made while we held the "lock"
			 * are discarded.
			 */
			if (SCDOptionGet(NULL, kSCDOptionIsLocked) &&
			    SCDOptionGet(thisSession->session, kSCDOptionIsLocked)) {
				/*
				 * swap cache and associated data which, after
				 * being closed, will result in the restoration
				 * of the original pre-"locked" data.
				 */
				_swapLockedCacheData();
			}

			/*
			 * Close any open connections including cancelling any outstanding
			 * notification requests and releasing any locks.
			 */
			(void) _SCDClose(&thisSession->session);

			/*
			 * Lastly, remove the session entry.
			 */
			removeSession(server);

			return;
		}
	}
	return;
}


void
listSessions()
{
	int	i;

	fprintf(stderr, "Current sessions:");
	for (i=0; i<nSessions; i++) {
		serverSessionRef	thisSession = sessions[i];

		if (thisSession == NULL) {
			continue;
		}

		fprintf(stderr, " %d", thisSession->key);

		if (thisSession->session != NULL) {
			task_t	task = ((SCDSessionPrivateRef)thisSession->session)->notifySignalTask;

			if (task != TASK_NULL) {
			       fprintf(stderr, "/%d", task);
			}
		}
	}
	fprintf(stderr, "\n");
}

