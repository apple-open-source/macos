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

#include <servers/bootstrap.h>
#include <sysexits.h>

#include "configd.h"
#include "configd_server.h"
#include "notify_server.h"
#include "session.h"
#include "notify.h"

/* MiG generated externals and functions */
extern struct rpc_subsystem	_config_subsystem;
extern boolean_t		config_server(mach_msg_header_t *, mach_msg_header_t *);

/* server state information */
CFMachPortRef		configd_port;		/* configd server port (for new session requests) */


boolean_t
config_demux(mach_msg_header_t *request, mach_msg_header_t *reply)
{
	boolean_t			processed = FALSE;

	mach_msg_format_0_trailer_t	*trailer;

	/* Feed the request into the ("MiG" generated) server */
	if (!processed &&
	    (request->msgh_id >= _config_subsystem.start && request->msgh_id < _config_subsystem.end)) {
		serverSessionRef	thisSession;

		thisSession = getSession(request->msgh_local_port);
		if (thisSession) {
			/*
			 * Get the caller's credentials (eUID/eGID) from the message trailer.
			 */
			trailer = (mach_msg_security_trailer_t *)((vm_offset_t)request +
								  round_msg(request->msgh_size));

			if ((trailer->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0) &&
			    (trailer->msgh_trailer_size >= MACH_MSG_TRAILER_FORMAT_0_SIZE)) {
				thisSession->callerEUID = trailer->msgh_sender.val[0];
				thisSession->callerEGID = trailer->msgh_sender.val[1];
				SCDLog(LOG_DEBUG, CFSTR("caller has eUID = %d, eGID = %d"),
				       thisSession->callerEUID,
				       thisSession->callerEGID);
			} else {
				static boolean_t warned = FALSE;

				if (!warned) {
					SCDLog(LOG_WARNING, CFSTR("caller's credentials not available."));
					warned = TRUE;
				}
				thisSession->callerEUID = 0;
				thisSession->callerEGID = 0;
			}
	       }

		/*
		 * Process configd requests.
		 */
		processed = config_server(request, reply);
	}

	if (!processed &&
	    (request->msgh_id >= MACH_NOTIFY_FIRST && request->msgh_id < MACH_NOTIFY_LAST)) {
		processed = notify_server(request, reply);
	}

	if (!processed) {
		SCDLog(LOG_WARNING, CFSTR("unknown message received"));
		exit (EX_OSERR);
	}

	return processed;
}


void
configdCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	mig_reply_error_t	*bufRequest = msg;
	mig_reply_error_t	*bufReply   = CFAllocatorAllocate(NULL, _config_subsystem.maxsize, 0);
	mach_msg_return_t	mr;
	int			options;

	/* we have a request message */
	(void) config_demux(&bufRequest->Head, &bufReply->Head);

	if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) && (bufReply->RetCode != KERN_SUCCESS)) {

		if (bufReply->RetCode == MIG_NO_REPLY) {
			/*
			 * This return code is a little tricky -- it appears that the
			 * demux routine found an error of some sort, but since that
			 * error would not normally get returned either to the local
			 * user or the remote one, we pretend it's ok.
			 */
			CFAllocatorDeallocate(NULL, bufReply);
			return;
		}

		/*
		 * destroy any out-of-line data in the request buffer but don't destroy
		 * the reply port right (since we need that to send an error message).
		 */
		bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
		mach_msg_destroy(&bufRequest->Head);
	}

	if (bufReply->Head.msgh_remote_port == MACH_PORT_NULL) {
		/* no reply port, so destroy the reply */
		if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
			mach_msg_destroy(&bufReply->Head);
		}
		CFAllocatorDeallocate(NULL, bufReply);
		return;
	}

	/*
	 * send reply.
	 *
	 * We don't want to block indefinitely because the client
	 * isn't receiving messages from the reply port.
	 * If we have a send-once right for the reply port, then
	 * this isn't a concern because the send won't block.
	 * If we have a send right, we need to use MACH_SEND_TIMEOUT.
	 * To avoid falling off the kernel's fast RPC path unnecessarily,
	 * we only supply MACH_SEND_TIMEOUT when absolutely necessary.
	 */

	options = MACH_SEND_MSG;
	if (MACH_MSGH_BITS_REMOTE(bufReply->Head.msgh_bits) == MACH_MSG_TYPE_MOVE_SEND_ONCE) {
		options |= MACH_SEND_TIMEOUT;
	}
	mr = mach_msg(&bufReply->Head,		/* msg */
		      options,			/* option */
		      bufReply->Head.msgh_size,	/* send_size */
		      0,			/* rcv_size */
		      MACH_PORT_NULL,		/* rcv_name */
		      MACH_MSG_TIMEOUT_NONE,	/* timeout */
		      MACH_PORT_NULL);		/* notify */


	/* Has a message error occurred? */
	switch (mr) {
		case MACH_SEND_INVALID_DEST:
		case MACH_SEND_TIMED_OUT:
			/* the reply can't be delivered, so destroy it */
			mach_msg_destroy(&bufReply->Head);
			break;

		default :
			/* Includes success case.  */
			break;
	}

	CFAllocatorDeallocate(NULL, bufReply);
}


boolean_t
server_active()
{
	kern_return_t 		status;
	mach_port_t		bootstrap_port;
	boolean_t		active;

	/* Getting bootstrap server port */
	status = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
	if (status != KERN_SUCCESS) {
		fprintf(stderr, "task_get_bootstrap_port(): %s\n",
			mach_error_string(status));
		exit (EX_UNAVAILABLE);
	}

	/* Check "configd" server status */
	status = bootstrap_status(bootstrap_port, SCD_SERVER, &active);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			if (active) {
				fprintf(stderr, "configd: '%s' server already active\n",
					SCD_SERVER);
				return TRUE;
			}
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			break;
		default :
			fprintf(stderr, "bootstrap_status(): %s\n",
				mach_error_string(status));
			exit (EX_UNAVAILABLE);
	}
	return FALSE;
}

void
server_init()
{
	kern_return_t 		status;
	mach_port_t		bootstrap_port;
	boolean_t		active;
	CFRunLoopSourceRef	rls;

	/* Getting bootstrap server port */
	status = task_get_bootstrap_port(mach_task_self(), &bootstrap_port);
	if (status != KERN_SUCCESS) {
		SCDLog(LOG_DEBUG, CFSTR("task_get_bootstrap_port(): %s"), mach_error_string(status));
		exit (EX_UNAVAILABLE);
	}

	/* Check "configd" server status */
	status = bootstrap_status(bootstrap_port, SCD_SERVER, &active);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			if (active) {
				SCDLog(LOG_DEBUG, CFSTR("\"%s\" is currently active, exiting."), SCD_SERVER);
				exit (EX_UNAVAILABLE);
			}
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, "a good thing" (tm) */
			break;
		default :
			fprintf(stderr, "bootstrap_status(): %s\n", mach_error_string(status));
			exit (EX_UNAVAILABLE);
	}

	/* Create the primary / new connection port */
	configd_port = CFMachPortCreate(NULL, configdCallback, NULL, NULL);

	/*
	 * Create and add a run loop source for the port and add this source
	 * to both the default run loop mode and the "locked" mode. These two
	 * modes will be used for normal (unlocked) communication with the
	 * server and when multiple (locked) updates are requested by a single
	 * session.
	 */
	rls = CFMachPortCreateRunLoopSource(NULL, configd_port, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, CFSTR("locked"));
	CFRelease(rls);

	/* Create a session for the primary / new connection port */
	(void) addSession(configd_port);

	SCDLog(LOG_DEBUG, CFSTR("Registering service \"%s\""), SCD_SERVER);
	status = bootstrap_register(bootstrap_port, SCD_SERVER, CFMachPortGetPort(configd_port));
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* service not currently registered, "a good thing" (tm) */
			break;
		case BOOTSTRAP_NOT_PRIVILEGED :
			SCDLog(LOG_ERR, CFSTR("bootstrap_register(): bootstrap not privileged"));
			exit (EX_OSERR);
		case BOOTSTRAP_SERVICE_ACTIVE :
			SCDLog(LOG_ERR, CFSTR("bootstrap_register(): bootstrap service active"));
			exit (EX_OSERR);
		default :
			SCDLog(LOG_ERR, CFSTR("bootstrap_register(): %s"), mach_error_string(status));
			exit (EX_OSERR);
	}

	return;
}


void
server_loop()
{
	CFStringRef	rlMode;
	int		rlStatus;

	while (TRUE) {
		boolean_t	isLocked = SCDOptionGet(NULL, kSCDOptionIsLocked);

		/*
		 * if linked with a DEBUG version of the framework, display some
		 * debugging information
		 */
		_showMachPortStatus();

		/*
		 * process one run loop event
		 */
		rlMode = isLocked ? CFSTR("locked") : kCFRunLoopDefaultMode;
		rlStatus = CFRunLoopRunInMode(rlMode, 1.0e10, TRUE);

		/*
		 * check for, and if necessary, push out change notifications
		 * to other processes.
		 */
		pushNotifications();
	}
}
