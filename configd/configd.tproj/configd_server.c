/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
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
 * March 9, 2004		Allan Nathanson <ajn@apple.com>
 * - add DNS configuration server
 *
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <sysexits.h>
#include <unistd.h>
#include <sys/types.h>
#include <servers/bootstrap.h>

#include "configd.h"
#include "configd_server.h"
#include "notify_server.h"
#include "session.h"

/* MiG generated externals and functions */
extern struct mig_subsystem	_config_subsystem;
extern boolean_t		config_server(mach_msg_header_t *, mach_msg_header_t *);

#include "shared_dns_info_types.h"
#include "dnsinfo_server.h"

/* MiG generated externals and functions */
extern struct mig_subsystem	_shared_dns_info_subsystem;
extern boolean_t		shared_dns_info_server(mach_msg_header_t *, mach_msg_header_t *);

/* configd server port (for new session requests) */
static CFMachPortRef		configd_port		= NULL;

/* priviledged bootstrap port (for registering/unregistering w/launchd) */
static mach_port_t		priv_bootstrap_port	= MACH_PORT_NULL;

__private_extern__
boolean_t
config_demux(mach_msg_header_t *request, mach_msg_header_t *reply)
{
	Boolean				processed = FALSE;

	/*
	 * (attempt to) process SCDynamicStore requests.
	 */
	processed = config_server(request, reply);
	if (processed) {
		return TRUE;
	}

	/*
	 * (attempt to) process DNS configuration requests.
	 */
	processed = shared_dns_info_server(request, reply);
	if (processed) {
		return TRUE;
	}

	/*
	 * (attempt to) process (NO MORE SENDERS) notification messages.
	 */
	processed = notify_server(request, reply);
	if (processed) {
		return TRUE;
	}

	/*
	 * unknown message ID, log and return an error.
	 */
	SCLog(TRUE, LOG_ERR, CFSTR("config_demux(): unknown message ID (%d) received"), request->msgh_id);
	reply->msgh_bits        = MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(request->msgh_bits), 0);
	reply->msgh_remote_port = request->msgh_remote_port;
	reply->msgh_size        = sizeof(mig_reply_error_t);	/* Minimal size */
	reply->msgh_local_port  = MACH_PORT_NULL;
	reply->msgh_id          = request->msgh_id + 100;
	((mig_reply_error_t *)reply)->NDR = NDR_record;
	((mig_reply_error_t *)reply)->RetCode = MIG_BAD_ID;

	return FALSE;
}


#define	MACH_MSG_BUFFER_SIZE	128


__private_extern__
void
configdCallback(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
	mig_reply_error_t *	bufRequest	= msg;
	uint32_t		bufReply_q[MACH_MSG_BUFFER_SIZE/sizeof(uint32_t)];
	mig_reply_error_t *	bufReply	= (mig_reply_error_t *)bufReply_q;
	static CFIndex		bufSize		= 0;
	mach_msg_return_t	mr;
	int			options;

	if (bufSize == 0) {
		// get max size for MiG reply buffers
		bufSize = _config_subsystem.maxsize;
		if (_shared_dns_info_subsystem.maxsize > bufSize) {
			bufSize = _shared_dns_info_subsystem.maxsize;
		}

		// check if our on-the-stack reply buffer will be big enough
		if (bufSize > sizeof(bufReply_q)) {
			SCLog(TRUE, LOG_NOTICE,
			      CFSTR("configdCallback(): buffer size should be increased > %d"),
			      _config_subsystem.maxsize);
		}
	}

	if (bufSize > sizeof(bufReply_q)) {
		bufReply = CFAllocatorAllocate(NULL, _config_subsystem.maxsize, 0);
	}

	/* we have a request message */
	(void) config_demux(&bufRequest->Head, &bufReply->Head);

	if (!(bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
		if (bufReply->RetCode == MIG_NO_REPLY) {
			bufReply->Head.msgh_remote_port = MACH_PORT_NULL;
		} else if ((bufReply->RetCode != KERN_SUCCESS) &&
			   (bufRequest->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
			/*
			 * destroy the request - but not the reply port
			 */
			bufRequest->Head.msgh_remote_port = MACH_PORT_NULL;
			mach_msg_destroy(&bufRequest->Head);
		}
	}

	if (bufReply->Head.msgh_remote_port != MACH_PORT_NULL) {
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
		if (MACH_MSGH_BITS_REMOTE(bufReply->Head.msgh_bits) != MACH_MSG_TYPE_MOVE_SEND_ONCE) {
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
				break;
			default :
				/* Includes success case.  */
				goto done;
		}
	}

	if (bufReply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
		mach_msg_destroy(&bufReply->Head);
	}

    done :

	if (bufReply != (mig_reply_error_t *)bufReply_q)
		CFAllocatorDeallocate(NULL, bufReply);
	return;
}


__private_extern__
boolean_t
server_active(mach_port_t *restart_service_port)
{
	char			*service_name;
	kern_return_t 		status;

	service_name = getenv("SCD_SERVER");
	if (!service_name) {
		service_name = SCD_SERVER;
	}

	/* Check "configd" server status */
	status = bootstrap_check_in(bootstrap_port, service_name, restart_service_port);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* if we are being restarted by launchd */
			priv_bootstrap_port = bootstrap_port;
			break;
		case BOOTSTRAP_SERVICE_ACTIVE :
		case BOOTSTRAP_NOT_PRIVILEGED :
			/* if another instance of the server is active (or starting) */
			fprintf(stderr, "'%s' server already active\n",
				service_name);
			return TRUE;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* if the server is not currently registered/active */
			*restart_service_port = MACH_PORT_NULL;
			break;
		default :
			fprintf(stderr,
				"bootstrap_check_in() failed: %s\n",
				bootstrap_strerror(status));
			exit (EX_UNAVAILABLE);
	}

	return FALSE;
}


static CFStringRef
serverMPCopyDescription(const void *info)
{
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("<main DynamicStore MP>"));
}


__private_extern__
void
server_init(mach_port_t		restart_service_port,
	    Boolean		enableRestart)
{
	serverSessionRef	mySession;
	CFRunLoopSourceRef	rls;
	char			*service_name;
	mach_port_t		service_port	= restart_service_port;
	kern_return_t 		status;
	mach_port_t		unpriv_bootstrap_port;

	service_name = getenv("SCD_SERVER");
	if (!service_name) {
		service_name = SCD_SERVER;
	}

	if (service_port == MACH_PORT_NULL) {
		mach_port_t	service_send_port;

		/* Check "configd" server status */
		status = bootstrap_check_in(bootstrap_port, service_name, &service_port);
		switch (status) {
			case BOOTSTRAP_SUCCESS :
				/* if we are being restarted by launchd */
				priv_bootstrap_port = bootstrap_port;
				break;
			case BOOTSTRAP_NOT_PRIVILEGED :
				/* if another instance of the server is starting */
				SCLog(TRUE, LOG_ERR, CFSTR("'%s' server already starting"), service_name);
				exit (EX_UNAVAILABLE);
			case BOOTSTRAP_UNKNOWN_SERVICE :
				/* service not currently registered, "a good thing" (tm) */
				if (enableRestart) {
					status = bootstrap_create_server(bootstrap_port,
									 "/usr/sbin/configd",
									 geteuid(),
									 FALSE,		/* not onDemand == restart now */
									 &priv_bootstrap_port);
					if (status != BOOTSTRAP_SUCCESS) {
						SCLog(TRUE, LOG_ERR,
						      CFSTR("server_init bootstrap_create_server() failed: %s"),
						      bootstrap_strerror(status));
						exit (EX_UNAVAILABLE);
					}
				} else {
					priv_bootstrap_port = bootstrap_port;
				}

				status = bootstrap_create_service(priv_bootstrap_port, service_name, &service_send_port);
				if (status != BOOTSTRAP_SUCCESS) {
					SCLog(TRUE, LOG_ERR,
					      CFSTR("server_init bootstrap_create_service() failed: %s"),
					      bootstrap_strerror(status));
					exit (EX_UNAVAILABLE);
				}

				status = bootstrap_check_in(priv_bootstrap_port, service_name, &service_port);
				if (status != BOOTSTRAP_SUCCESS) {
					SCLog(TRUE, LOG_ERR,
					      CFSTR("server_init bootstrap_check_in() failed: %s"),
					      bootstrap_strerror(status));
					exit (EX_UNAVAILABLE);
				}
				break;
			case BOOTSTRAP_SERVICE_ACTIVE :
				/* if another instance of the server is active */
				SCLog(TRUE, LOG_ERR, CFSTR("'%s' server already active"), service_name);
				exit (EX_UNAVAILABLE);
			default :
			SCLog(TRUE, LOG_ERR,
			      CFSTR("server_init bootstrap_check_in() failed: %s"),
			      bootstrap_strerror(status));
			exit (EX_UNAVAILABLE);
		}

	}

	/* we don't want to pass our priviledged bootstrap port along to any spawned helpers so... */
	status = bootstrap_unprivileged(priv_bootstrap_port, &unpriv_bootstrap_port);
	if (status != BOOTSTRAP_SUCCESS) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("server_init bootstrap_unprivileged() failed: %s"),
		      bootstrap_strerror(status));
		exit (EX_UNAVAILABLE);
	}

	status = task_set_bootstrap_port(mach_task_self(), unpriv_bootstrap_port);
	if (status != BOOTSTRAP_SUCCESS) {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("server_init task_set_bootstrap_port(): %s"),
		      mach_error_string(status));
		exit (EX_UNAVAILABLE);
	}

	/* ... and make sure that the global "bootstrap_port" is also unpriviledged */
	bootstrap_port = unpriv_bootstrap_port;

	/* Create the primary / new connection port and backing session */
	mySession = addSession(service_port, serverMPCopyDescription);
	configd_port = mySession->serverPort;

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

	return;
}


__private_extern__
int
server_shutdown()
{
	char		*service_name;
	mach_port_t	service_port;
	kern_return_t 	status;

	/*
	 * Note: we can't use SCLog() since the signal may be received while the
	 *       logging thread lock is held.
	 */
	if ((priv_bootstrap_port == MACH_PORT_NULL) || (configd_port == NULL)) {
		return EX_OK;
	}

	service_name = getenv("SCD_SERVER");
	if (!service_name) {
		service_name = SCD_SERVER;
	}

	service_port = CFMachPortGetPort(configd_port);
	if (service_port != MACH_PORT_NULL) {
		(void) mach_port_destroy(mach_task_self(), service_port);
	}

	status = bootstrap_register(priv_bootstrap_port, service_name, MACH_PORT_NULL);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			break;
		case MACH_SEND_INVALID_DEST :
		case MIG_SERVER_DIED :
			/* something happened to launchd */
			break;
		default :
			SCLog(TRUE, LOG_ERR,
			      CFSTR("server_shutdown bootstrap_register(): %s"),
			      bootstrap_strerror(status));
			return EX_UNAVAILABLE;
	}

	return EX_OK;
}


__private_extern__
void
server_loop()
{
	CFStringRef	rlMode;
	int		rlStatus;

	while (TRUE) {
		/*
		 * if linked with a DEBUG version of the framework, display some
		 * debugging information
		 */
		__showMachPortStatus();

		/*
		 * process one run loop event
		 */
		rlMode = (storeLocked > 0) ? CFSTR("locked") : kCFRunLoopDefaultMode;
		rlStatus = CFRunLoopRunInMode(rlMode, 1.0e10, TRUE);

		/*
		 * check for, and if necessary, push out change notifications
		 * to other processes.
		 */
		pushNotifications();
	}
}
