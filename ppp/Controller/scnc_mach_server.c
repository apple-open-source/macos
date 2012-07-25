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
#include <SystemConfiguration/VPNPrivate.h>      
#include <SystemConfiguration/SCValidation.h>
#include <Security/SecItem.h>
#include <Security/SecCertificatePriv.h>
#include <Security/Security.h>
#include <Security/SecTask.h>
#include "bsm/libbsm.h"

#include "scnc_client.h"
#include "scnc_main.h"
#include "scnc_utils.h"
#include "pppcontroller.h"
#include "pppcontroller_types.h"
#include "pppcontrollerServer.h"
#include "scnc_mach_server.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

#ifndef kSCStatusConnectionNoService
#define kSCStatusConnectionNoService 5001
#endif


#define	IPCSENDTIMEOUT	120				// timeout value for sending IPC message to client

/* -----------------------------------------------------------------------------
forward declarations
----------------------------------------------------------------------------- */

void server_handle_request(CFMachPortRef port, void *msg, CFIndex size, void *info);

/* -----------------------------------------------------------------------------
globals
----------------------------------------------------------------------------- */

static CFMachPortRef		gServer_cfport;
static Boolean hasEntitlement(audit_token_t audit_token, CFStringRef entitlement, CFStringRef vpntype);


extern CFRunLoopRef			gControllerRunloop;
extern CFRunLoopSourceRef	gPluginRunloop;
extern CFRunLoopSourceRef	gTerminalrls;


#define kSCVPNConnectionEntitlementName CFSTR("com.apple.private.SCNetworkConnection-proxy-user")

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_attach_proxy(mach_port_t server,
							xmlData_t nameRef,		/* raw XML bytes */
							mach_msg_type_number_t nameLen,
							mach_port_t bootstrap,
							mach_port_t notify,
							mach_port_t au_session,
							int uid,
							int gid,
							int pid,
							mach_port_t *session,
							int * result,
							audit_token_t audit_token)
{
	CFStringRef			serviceID = NULL;
	CFMachPortRef		port = NULL;
	CFRunLoopSourceRef  rls = NULL;
	struct client		*client = NULL;
	mach_port_t			oldport;
	uid_t				audit_euid = -1;
	gid_t				audit_egid = -1;
	pid_t				audit_pid = -1;
	
	*session = MACH_PORT_NULL;
	/* un-serialize the serviceID */
	if (!_SCUnserializeString(&serviceID, NULL, (void *)nameRef, nameLen)) {
		*result = kSCStatusFailed;
		goto failed;
	}

	if (!isA_CFString(serviceID)) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}

	/* only allow "root" callers to change the client uid/gid/pid */
	audit_token_to_au32(audit_token,
						NULL,			// auidp
						&audit_euid,	// euid
						&audit_egid,	// egid
						NULL,			// ruid
						NULL,			// rgid
						&audit_pid,		// pid
						NULL,			// asid
						NULL);			// tid

    if ((audit_euid != 0) &&
        ((uid != audit_euid) || (gid != audit_egid) || (pid != audit_pid))) {
        /*
         * the caller is NOT "root" and is trying to masquerade
         * as some other user/process.
         */
        
        /* does caller has the right entitlement */
        if (!(hasEntitlement(audit_token, kSCVPNConnectionEntitlementName, NULL))){
           *result = kSCStatusAccessError;
            goto failed;
        }
    }
    
	
	//if ((findbyserviceID(serviceID)) == 0) {
	//	*result = kSCStatusInvalidArgument;
	//	goto failed;
	//}

	/* allocate session port */
	(void) mach_port_allocate(mach_task_self(),
							  MACH_PORT_RIGHT_RECEIVE,
							  session);

    /*
     * Note: we create the CFMachPort *before* we insert a send
     *       right present to ensure that CF does not establish
     *       it's dead name notification.
     */
	port = _SC_CFMachPortCreateWithPort("PPPController/PPP", *session, server_handle_request, NULL);

    /* insert send right that will be moved to the client */
	(void) mach_port_insert_right(mach_task_self(),
								  *session,
								  *session,
								  MACH_MSG_TYPE_MAKE_SEND);

	/* Request a notification when/if the client dies */
	(void) mach_port_request_notification(mach_task_self(),
										  *session,
										  MACH_NOTIFY_NO_SENDERS,
										  1,
										  *session,
										  MACH_MSG_TYPE_MAKE_SEND_ONCE,
										  &oldport);

	/* add to runloop */
	rls = CFMachPortCreateRunLoopSource(NULL, port, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);

	if (au_session != MACH_PORT_NULL) {
		if ((audit_session_join(au_session)) == AU_DEFAUDITSID) {
			SCLog(TRUE, LOG_ERR, CFSTR("_pppcontroller_attach audit_session_join fails"));
		}
	}else {
		SCLog(TRUE, LOG_ERR, CFSTR("_pppcontroller_attach au_session == NULL"));
	}

	client = client_new_mach(port, rls, serviceID, uid, gid, pid, bootstrap, notify, au_session);
	if (client == 0) {
		*result = kSCStatusFailed;
		goto failed;
	}

	*result = kSCStatusOK;
	
	my_CFRelease(&serviceID);
	my_CFRelease(&port);
	my_CFRelease(&rls);
    return KERN_SUCCESS;
	
 failed:
	my_CFRelease(&serviceID);
	if (port) {
		CFMachPortInvalidate(port);
		my_CFRelease(&port);
	}
	if (rls) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
		my_CFRelease(&rls);
	}
	if (*session != MACH_PORT_NULL) {
		mach_port_mod_refs(mach_task_self(), *session, MACH_PORT_RIGHT_SEND   , -1);
		mach_port_mod_refs(mach_task_self(), *session, MACH_PORT_RIGHT_RECEIVE, -1);
		*session = MACH_PORT_NULL;
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

__private_extern__
kern_return_t
_pppcontroller_attach(mach_port_t server,
					  xmlData_t nameRef,		/* raw XML bytes */
					  mach_msg_type_number_t nameLen,
					  mach_port_t bootstrap,
					  mach_port_t notify,
					  mach_port_t au_session,
					  mach_port_t *session,
					  int * result,
					  audit_token_t audit_token)
{
	kern_return_t	kr;
	uid_t			euid	= -1;
	gid_t			egid	= -1;
	pid_t			pid		= -1;
	
	audit_token_to_au32(audit_token,
						NULL,			// auidp
						&euid,			// euid
						&egid,			// egid
						NULL,			// ruid
						NULL,			// rgid
						&pid,			// pid
						NULL,			// asid
						NULL);			// tid
	
	kr = _pppcontroller_attach_proxy(server,
									 nameRef,
									 nameLen,
									 bootstrap,
									 notify,
									 au_session,
									 euid,
									 egid,
									 pid,
									 session,
									 result,
									 audit_token);
	return kr;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
__private_extern__
kern_return_t
_pppcontroller_getstatus(mach_port_t session,
	    int * status,
		int * result)
{
	struct client		*client;
	struct service		*serv = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((serv = findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusConnectionNoService;
		goto failed;
	}
        
	*status = scnc_getstatus(serv);
	
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
	struct service		*serv = 0;
	void				*reply = 0;
	u_int16_t			replylen = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((serv = findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusConnectionNoService;
		goto failed;
	}
        
	if (scnc_copyextendedstatus(serv, &reply, &replylen)) {
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
	struct service		*serv = 0;
	void				*reply = 0;
	u_int16_t			replylen = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((serv = findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusConnectionNoService;
		goto failed;
	}
        
	if (scnc_copystatistics(serv, &reply, &replylen)) {
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
	struct service		*serv = 0;
	void				*reply = 0;
	u_int16_t			replylen = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((serv = findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusConnectionNoService;
		goto failed;
	}
        
	if (scnc_getconnectdata(serv, &reply, &replylen, 0)) {
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
	struct service		*serv = 0;
    CFDictionaryRef		optRef = 0;
	int					err;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((serv = findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusConnectionNoService;
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
	
    err = scnc_start(serv, optRef, client, linger ? 0 : 1, client->uid, client->gid, client->pid, client->bootstrap_port);
	if (err) {
		*result = kSCStatusFailed;
		goto failed;
	}
	
	my_CFRelease(&optRef);
	*result = kSCStatusOK;
    return (KERN_SUCCESS);

failed:
	my_CFRelease(&optRef);
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
	struct client		*client, *arb_client;
	struct service		*serv = 0;
	int                  scnc_reason;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((serv = findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusConnectionNoService;
		goto failed;
	}
	arb_client = force ? 0 : client;
	scnc_reason = arb_client? SCNC_STOP_USER_REQ : SCNC_STOP_USER_REQ_NO_CLIENT; 
    scnc_stop(serv, client, SIGHUP, scnc_reason);
	
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
	struct service		*serv = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((serv = findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusConnectionNoService;
		goto failed;
	}
        
    scnc_suspend(serv);
	
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
	struct service		*serv = 0;
	
    client = client_findbymachport(session);
    if (!client ) {
		*result = kSCStatusInvalidArgument;
		goto failed;
    }

	if ((serv = findbyserviceID(client->serviceID)) == 0) {
		*result = kSCStatusConnectionNoService;
		goto failed;
	}
        
    scnc_resume(serv);
	
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
	struct service		*serv;

	audit_token_to_au32(*audit_token,
			    NULL,			// auidp
			    NULL,			// euid
			    NULL,			// egid
			    NULL,			// ruid
			    NULL,			// rgid
			    &pid,			// pid
			    NULL,			// asid
			    NULL);			// tid

	if ((serv = findbypid(pid)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}

	*bootstrap = serv->bootstrap;
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
	struct service		*serv;
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

	if ((serv = findbypid(pid)) == 0) {
		*result = kSCStatusInvalidArgument;
		goto failed;
	}

	switch (options_type) {
	
		/* system options */
		case 0:
			if (scnc_getconnectsystemdata(serv, &reply, &replylen)) {
				*result = kSCStatusFailed;
				goto failed;
			}
			break;

		/* user options */
		case 1:

			if (scnc_getconnectdata(serv, &reply, &replylen, 1)) {
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
	struct service		*serv;

	audit_token_to_au32(*audit_token,
			    NULL,			// auidp
			    NULL,			// euid
			    NULL,			// egid
			    NULL,			// ruid
			    NULL,			// rgid
			    &pid,			// pid
			    NULL,			// asid
			    NULL);			// tid

	if ((serv = findbypid(pid)) == 0)
		*result = kSCStatusInvalidArgument;
	else 
		*result = kSCStatusOK;

    return (KERN_SUCCESS);
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static Boolean
hasEntitlement(audit_token_t audit_token, CFStringRef entitlement, CFStringRef vpntype)
{
	Boolean		hasEntitlement	= FALSE;
	SecTaskRef	task;

	/* Create the security task from the audit token. */
	task = SecTaskCreateWithAuditToken(NULL, audit_token);
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
			} else if (isA_CFArray(value)){
				if (vpntype == NULL){
					/* we don't care about subtype */
					hasEntitlement = TRUE;
				}else {
					if (CFArrayContainsValue(value,
											 CFRangeMake(0, CFArrayGetCount(value)),
											 vpntype)) {
						// if client DOES have entitlement
						hasEntitlement = TRUE;
					}
				}
			} else {
				SCLog(TRUE, LOG_ERR,
				      CFSTR("SCNC Controller: entitlement not valid: %@"),
				      entitlement);
			}

			CFRelease(value);
		} else if (error != NULL) {
			SCLog(TRUE, LOG_ERR,
			      CFSTR("SCNC Controller: SecTaskCopyValueForEntitlement() failed, error=%@: %@"),
			      error,
			      entitlement);
			CFRelease(error);
		}

		CFRelease(task);
	} else {
		SCLog(TRUE, LOG_ERR,
		      CFSTR("SCNC Controller: SecTaskCreateWithAuditToken() failed: %@"),
		      entitlement);
	}

	return hasEntitlement;
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
		case MACH_NOTIFY_NO_SENDERS: {
			mach_port_t	session	= notify->not_header.msgh_local_port;

			client = client_findbymachport(session);
			if (client) {
				client_dispose(client);
			}

			/*
			 * Our send right has already been removed. Remove our
			 * receive right.
			 */
			mach_port_mod_refs(mach_task_self(),
							   session,
							   MACH_PORT_RIGHT_RECEIVE,
							   -1);
			break;
		}
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
    char		reply_s[128] __attribute__ ((aligned (4)));		// Wcast-align fix - force alignment

    if (process_notification(request) == FALSE) {
		if (_pppcontroller_subsystem.maxsize > sizeof(reply_s)) {
			syslog(LOG_ERR, "PPPController: %d > %ld",
				_pppcontroller_subsystem.maxsize, sizeof(reply_s));
			reply = (mach_msg_header_t *)
			malloc(_pppcontroller_subsystem.maxsize);
		}
		else {
			reply = ALIGNED_CAST(mach_msg_header_t *)reply_s;
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
		if (reply != ALIGNED_CAST(mach_msg_header_t *)reply_s) {
			free(reply);
		}
    }
    return;
}

#if !TARGET_OS_EMBEDDED
/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static CFStringRef
serverMPCopyDescription(const void *info)
{
	return CFStringCreateWithFormat(NULL, NULL, CFSTR("PPPController"));
}
#endif

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
#if TARGET_OS_EMBEDDED
/* Radar 6386278 New launchd api is npot on embedded yet */
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
	gControllerRunloop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(gControllerRunloop, rls, kCFRunLoopDefaultMode);
	CFRunLoopAddSource(gControllerRunloop, gTerminalrls, kCFRunLoopDefaultMode);	
    CFRelease(rls);
	
    status = bootstrap_register(bootstrap_port, PPPCONTROLLER_SERVER, 
								CFMachPortGetPort(gServer_cfport));
    if (status != BOOTSTRAP_SUCCESS) {
		mach_error("bootstrap_register", status);
		return -1;
    }
	
    return 0;
}
#else
int
ppp_mach_start_server()
{
    kern_return_t 	status;
    CFRunLoopSourceRef	rls;
	mach_port_t      our_port = MACH_PORT_NULL;
	CFMachPortContext      context  = { 0, (void *)1, NULL, NULL, serverMPCopyDescription };
	
	status = bootstrap_check_in(bootstrap_port, PPPCONTROLLER_SERVER, &our_port);
	if (status != BOOTSTRAP_SUCCESS) {
		SCLog(TRUE, LOG_ERR, CFSTR("PPPController: bootstrap_check_in \"%s\" error = %s"),
			  PPPCONTROLLER_SERVER, bootstrap_strerror(status));
		return -1;
	}
	
	gServer_cfport = _SC_CFMachPortCreateWithPort("PPPController", our_port, server_handle_request, &context);
	if (!gServer_cfport) {
		SCLog(TRUE, LOG_ERR, CFSTR("PPPController: cannot create mach port"));
		return -1;
	}
	
	rls = CFMachPortCreateRunLoopSource(0, gServer_cfport, 0);
	if (!rls) {
		SCLog(TRUE, LOG_ERR, CFSTR("PPPController: cannot create rls"));
		CFRelease(gServer_cfport);
		gServer_cfport = NULL;
		return -1;
	}

	gControllerRunloop = CFRunLoopGetCurrent();
	CFRunLoopAddSource(gControllerRunloop, rls, kCFRunLoopDefaultMode);
	CFRunLoopAddSource(gControllerRunloop, gTerminalrls, kCFRunLoopDefaultMode);	
    CFRelease(rls);
	
	return 0;
	
}
#endif
