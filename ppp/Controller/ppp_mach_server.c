/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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


/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/ucred.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <CoreFoundation/CFMachPort.h>
#include <SystemConfiguration/SCPrivate.h>      // for SCLog()
#include <SystemConfiguration/SCValidation.h>
#include "bsm/libbsm.h"

#include "ppp_client.h"
#include "ppp_manager.h"
#include "ppp_utils.h"
#include "pppcontroller.h"
#include "pppcontroller_types.h"
#include "ppp_mach_server.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
forward declarations
----------------------------------------------------------------------------- */

void server_handle_request(CFMachPortRef port, void *msg, CFIndex size, void *info);


/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

static CFMachPortRef		gServer_cfport;

extern struct mig_subsystem	_pppcontroller_subsystem;
extern boolean_t		pppcontroller_server(mach_msg_header_t *, 
						       mach_msg_header_t *);


static uid_t S_uid = -1;
static gid_t S_gid = -1;

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_attach(mach_port_t server,
	    xmlData_t nameRef,		/* raw XML bytes */
	    mach_msg_type_number_t nameLen,
		mach_port_t bootstrap,
		mach_port_t notify,
		mach_port_t *session,
		int * result)
{
	CFStringRef			serviceID = NULL;
	CFMachPortRef		port = NULL;
	CFRunLoopSourceRef  rls = NULL;
	struct client		*client = NULL;
	mach_port_t			oldport;
	kern_return_t		status;
	
	*session = 0;
	/* un-serialize the serviceID */
	if (!_SCUnserializeString(&serviceID, NULL, (void *)nameRef, nameLen)) {
		*result = kSCStatusFailed;
		goto failed;
	}

	if (!isA_CFString(serviceID)) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}

	if ((ppp_findbyserviceID(serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}

    port = CFMachPortCreate(NULL, server_handle_request, NULL, NULL);
    rls = CFMachPortCreateRunLoopSource(NULL, port, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);			     

	client = client_new_mach(port, rls, serviceID, S_uid, S_gid, bootstrap, notify);
	if (client == 0) {
		*result = kSCStatusFailed;
		goto failed;
	}
	
    *session = CFMachPortGetPort(port);

	/* Request a notification when/if the client dies */
	status = mach_port_request_notification(mach_task_self(),
						*session, MACH_NOTIFY_NO_SENDERS, 1,
						*session, MACH_MSG_TYPE_MAKE_SEND_ONCE, &oldport);
	if (status != KERN_SUCCESS) {
		*result = kSCStatusFailed;
		goto failed;
	}

	*result = kSCStatusOK;
	
	my_CFRelease(serviceID);
	my_CFRelease(port);
	my_CFRelease(rls);
    return KERN_SUCCESS;
	
 failed:
	my_CFRelease(serviceID);
	if (port) {
		CFMachPortInvalidate(port);
		my_CFRelease(port);
	}
	if (rls) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
		my_CFRelease(rls);
	}
	if (client) {
		client_dispose(client);
	} else {
		if (bootstrap != MACH_PORT_NULL)
			mach_port_deallocate(mach_task_self(), bootstrap);
		if (notify != MACH_PORT_NULL)
			mach_port_deallocate(mach_task_self(), notify);
	}
    return KERN_SUCCESS;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_getstatus(mach_port_t session,
	    int * phase,
		int * result)
{
	struct client		*client;
	struct ppp			*ppp = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((ppp = ppp_findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}
        
    *phase = ppp->phase;
	
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_copyextendedstatus(mach_port_t session,
		xmlDataOut_t * extstatus, 
		mach_msg_type_number_t * extstatus_len,
		int * result)
{
	struct client		*client;
	struct ppp			*ppp = 0;
	void				*reply = 0;
	u_int16_t			replylen = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((ppp = ppp_findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}
        
	if (ppp_copyextendedstatus(ppp, &reply, &replylen)) {
		*result = kSCStatusFailed;
		goto failed;
	}
	
	*extstatus = reply;
	*extstatus_len = replylen;
	
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
	*extstatus = 0;
	*extstatus_len = 0;
    return (KERN_SUCCESS);
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_copystatistics(mach_port_t session,
		xmlDataOut_t * statistics, 
		mach_msg_type_number_t * statistics_len,
		int * result)
{
	struct client		*client;
	struct ppp			*ppp = 0;
	void				*reply = 0;
	u_int16_t			replylen = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((ppp = ppp_findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}
        
	if (ppp_copystatistics(ppp, &reply, &replylen)) {
		*result = kSCStatusFailed;
		goto failed;
	}
	
	*statistics = reply;
	*statistics_len = replylen;
	
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
	*statistics = 0;
	*statistics_len = 0;
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_copyuseroptions(mach_port_t session,
		xmlDataOut_t * options, 
		mach_msg_type_number_t * options_len,
		int * result)
{
	struct client		*client;
	struct ppp			*ppp = 0;
	void				*reply = 0;
	u_int16_t			replylen = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((ppp = ppp_findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}
        
	if (ppp_getconnectdata(ppp, &reply, &replylen, 0)) {
		*result = kSCStatusFailed;
		goto failed;
	}
	
	*options = reply;
	*options_len = replylen;
	
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
	*options = 0;
	*options_len = 0;
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_start(mach_port_t session,
				xmlData_t dataRef,		/* raw XML bytes */
				mach_msg_type_number_t dataLen,
				int linger,
				int * result)
{
	struct client		*client;
	struct ppp			*ppp = 0;
    CFDictionaryRef		optRef = 0;
	int					err;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((ppp = ppp_findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}
        
	/* un-serialize the user options */
	if (dataLen) {
		if (!_SCUnserialize((CFPropertyListRef *)&optRef, NULL, (void *)dataRef, dataLen)) {
			*result = kSCStatusFailed;
			goto failed;
		}

		if (!isA_CFDictionary(optRef)) {
			*result = kSCStatusInvalidArgument;
			goto failed;
		}
	}
	
    err = ppp_connect(ppp, optRef, 0, client, linger ? 0 : 1, client->uid, client->gid, client->bootstrap_port);
	if (err) {
		*result = kSCStatusFailed;
		goto failed;
	}
	
	my_CFRelease(optRef);
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
	my_CFRelease(optRef);
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_stop(mach_port_t session,
				int		force,
				int		*result)
{
	struct client		*client;
	struct ppp			*ppp = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((ppp = ppp_findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}
        
    ppp_disconnect(ppp, force ? 0 : client, SIGHUP);
	
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_suspend(mach_port_t session,
				int		*result)
{
	struct client		*client;
	struct ppp			*ppp = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((ppp = ppp_findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}
        
    ppp_suspend(ppp);
	
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_resume(mach_port_t session,
				int		*result)
{
	struct client		*client;
	struct ppp			*ppp = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((ppp = ppp_findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}
        
    ppp_resume(ppp);
	
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_notification(mach_port_t session,
			 int enable,
		     int * result)
{
	struct client		*client;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if (enable) {
		client->flags |= CLIENT_FLAG_NOTIFY_STATUS;
	}
	else {
		client->flags &= ~CLIENT_FLAG_NOTIFY_STATUS;
	}
	
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_bootstrap(mach_port_t server,
		mach_port_t *bootstrap,
		int * result,
		audit_token_t *audit_token)
{
    int                 pid;
	struct ppp			*ppp;

	audit_token_to_au32(*audit_token,
			    NULL,			// auidp
			    NULL,			// euid
			    NULL,			// egid
			    NULL,			// ruid
			    NULL,			// rgid
			    &pid,			// pid
			    NULL,			// asid
			    NULL);			// tid

	if ((ppp = ppp_findbypid(pid)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}

	*bootstrap = ppp->bootstrap;
	*result = kSCStatusOK;

    return (KERN_SUCCESS);
	
failed:
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_copyprivoptions(mach_port_t server,
		int options_type,
		xmlDataOut_t * options, 
		mach_msg_type_number_t * options_len,
		int * result,
		audit_token_t *audit_token)
{
    int                 pid;
	struct ppp			*ppp;
	void				*reply = 0;
	u_int16_t			replylen = 0;
	
	audit_token_to_au32(*audit_token,
			    NULL,			// auidp
			    NULL,			// euid
			    NULL,			// egid
			    NULL,			// ruid
			    NULL,			// rgid
			    &pid,			// pid
			    NULL,			// asid
			    NULL);			// tid

	if ((ppp = ppp_findbypid(pid)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}

	switch (options_type) {
	
		/* system options */
		case 0:
			if (ppp_getconnectsystemdata(ppp, &reply, &replylen)) {
				*result = kSCStatusFailed;
				goto failed;
			}
			break;

		/* user options */
		case 1:

			if (ppp_getconnectdata(ppp, &reply, &replylen, 1)) {
				*result = kSCStatusFailed;
				goto failed;
			}
			break;
	}

	*options = reply;
	*options_len = replylen;

	*result = kSCStatusOK;

    return (KERN_SUCCESS);
	
failed:
	*options = 0;
	*options_len = 0;
    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_iscontrolled(mach_port_t server,
				int * result,
				audit_token_t *audit_token)
{
    pid_t                pid = 0;
	struct ppp			*ppp;

	audit_token_to_au32(*audit_token,
			    NULL,			// auidp
			    NULL,			// euid
			    NULL,			// egid
			    NULL,			// ruid
			    NULL,			// rgid
			    &pid,			// pid
			    NULL,			// asid
			    NULL);			// tid

	if ((ppp = ppp_findbypid(pid)) == 0)
		*result = kSCStatusInvalidArgument;
	else 
		*result = kSCStatusOK;

    return (KERN_SUCCESS);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void mach_client_notify (mach_port_t port, CFStringRef serviceID, u_long event, u_long error)
{
	mach_msg_empty_send_t	msg;
	kern_return_t			status;

	/* Post notification as mach message */
	msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
	msg.header.msgh_size = sizeof(msg);
	msg.header.msgh_remote_port = port;
	msg.header.msgh_local_port = MACH_PORT_NULL;
	msg.header.msgh_id = 0;
	status = mach_msg(&msg.header,		/* msg */
			  MACH_SEND_MSG|MACH_SEND_TIMEOUT,	/* options */
			  msg.header.msgh_size,		/* send_size */
			  0,						/* rcv_size */
			  MACH_PORT_NULL,			/* rcv_name */
			  0,						/* timeout */
			  MACH_PORT_NULL);			/* notify */

	if (status == MACH_SEND_TIMEOUT)
		mach_msg_destroy(&msg.header);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static __inline__ void
read_trailer(mach_msg_header_t * request)
{
    mach_msg_format_0_trailer_t	*trailer;
    trailer = (mach_msg_security_trailer_t *)((vm_offset_t)request +
					      round_msg(request->msgh_size));

    if ((trailer->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0) &&
	(trailer->msgh_trailer_size >= MACH_MSG_TRAILER_FORMAT_0_SIZE)) {
	S_uid = trailer->msgh_sender.val[0];
	S_gid = trailer->msgh_sender.val[1];
    }
    else {
	S_uid = -1;
	S_gid = -1;
    }
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static boolean_t
process_notification(mach_msg_header_t * request)
{
	struct client		*client;
	
    mach_no_senders_notification_t * notify;

    notify = (mach_no_senders_notification_t *)request;
    if ((notify->not_header.msgh_id > MACH_NOTIFY_LAST) ||
		(notify->not_header.msgh_id < MACH_NOTIFY_FIRST)) {
		return FALSE;	/* if this is not a notification message */
	}
    switch (notify->not_header.msgh_id) {
		case MACH_NOTIFY_NO_SENDERS:
		case MACH_NOTIFY_DEAD_NAME:

			client = client_findbymachport(notify->not_header.msgh_local_port);
			if (client) {
				client_dispose(client);
			}

			break;
		default :
			break;
    }
    return (TRUE);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void
server_handle_request(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mach_msg_return_t 	r;
    mach_msg_header_t *	request = (mach_msg_header_t *)msg;
    mach_msg_header_t *	reply;
    char		reply_s[128];

    if (process_notification(request) == FALSE) {
		read_trailer(request);
		if (_pppcontroller_subsystem.maxsize > sizeof(reply_s)) {
			syslog(LOG_ERR, "PPPController: %d > %d",
				_pppcontroller_subsystem.maxsize, sizeof(reply_s));
			reply = (mach_msg_header_t *)
			malloc(_pppcontroller_subsystem.maxsize);
		}
		else {
			reply = (mach_msg_header_t *)reply_s;
		}
		if (pppcontroller_server(request, reply) == FALSE) {
			syslog(LOG_INFO, "unknown message ID (%d) received",
			   request->msgh_id);
			mach_msg_destroy(request);
		}
		else {
			int		options;

			options = MACH_SEND_MSG;
			if (MACH_MSGH_BITS_REMOTE(reply->msgh_bits) == MACH_MSG_TYPE_MOVE_SEND) {
				options |= MACH_SEND_TIMEOUT;
			}
			r = mach_msg(reply,
				 options,
				 reply->msgh_size,
				 0,
				 MACH_PORT_NULL,
				 MACH_MSG_TIMEOUT_NONE,
				 MACH_PORT_NULL);
			if (r != MACH_MSG_SUCCESS) {
				syslog(LOG_INFO, "PPPController: mach_msg(send): %s", 
					mach_error_string(r));
				mach_msg_destroy(reply);
			}
		}
		if (reply != (mach_msg_header_t *)reply_s) {
			free(reply);
		}
    }
    return;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
ppp_mach_start_server()
{
    boolean_t		active;
    kern_return_t 	status;
    CFRunLoopSourceRef	rls;

	active = FALSE;
	status = bootstrap_status(bootstrap_port, PPPCONTROLLER_SERVER, &active);

	switch (status) {
		case BOOTSTRAP_SUCCESS:
			if (active) {
				fprintf(stderr, "\"%s\" is currently active.\n", 
					 PPPCONTROLLER_SERVER);
				return -1;
			}
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE:
			break;
		default:
			fprintf(stderr,
			 "bootstrap_status(): %s\n", mach_error_string(status));
			return -1;
    }

    gServer_cfport = CFMachPortCreate(NULL, server_handle_request, NULL, NULL);
    rls = CFMachPortCreateRunLoopSource(NULL, gServer_cfport, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    status = bootstrap_register(bootstrap_port, PPPCONTROLLER_SERVER, 
				CFMachPortGetPort(gServer_cfport));
    if (status != BOOTSTRAP_SUCCESS) {
		mach_error("bootstrap_register", status);
		return -1;
    }

    return 0;
}


