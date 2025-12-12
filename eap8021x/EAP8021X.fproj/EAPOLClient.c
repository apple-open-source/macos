/*
 * Copyright (c) 2002-2021, 2025 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <bsm/audit.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/boolean.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <CoreFoundation/CFMachPort.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "eapolcontroller.h"
#include "eapolcontroller_types.h"
#include "eapolcontroller_ext.h"
#include "myCFUtil.h"
#include "EAPOLClient.h"
#include "EAPLog.h"

struct EAPOLClient_s {
    mach_port_t 		notify_port; // Mach port for notifications
    dispatch_source_t		notify_source; // Dispatch source for notify_port
    Boolean  			notify_source_suspended; // suspension status of notify_source
    mach_port_t			session_port;
    EAPOLClientCallBackRef	callback_func;
    void *			callback_arg;
    if_name_t			if_name;
};

static void
EAPOLClientInvalidate(EAPOLClientRef client, boolean_t remove_send_right)
{
    if (client->notify_source != NULL) {
	dispatch_source_cancel(client->notify_source);
	if (client->notify_source_suspended == FALSE) {
	    /* dispatch source can be released only in unsuspended state */
	    dispatch_release(client->notify_source);
	}
	client->notify_source = NULL;
    }
    if (client->notify_port != MACH_PORT_NULL) {
	mach_port_mod_refs(mach_task_self(), client->notify_port,
			  MACH_PORT_RIGHT_RECEIVE, -1);
	if (remove_send_right) {
	    mach_port_deallocate(mach_task_self(), client->notify_port);
	}
	client->notify_port = MACH_PORT_NULL;
    }
    if (client->session_port != MACH_PORT_NULL) {
	(void)mach_port_deallocate(mach_task_self(), client->session_port);
	client->session_port = MACH_PORT_NULL;
    }
    return;
}

Boolean
EAPOLClientEstablishSession(const char * interface_name)
{
    mach_port_t			au_session;
    mach_port_t			bootstrap;
    mach_port_t			server;
    Boolean			session_established = FALSE;
    kern_return_t		status;
    if_name_t			if_name;

    status = eapolcontroller_server_port(&server);
    if (status != BOOTSTRAP_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, "eapolcontroller_server_port(): %s",
		  mach_error_string(status));
	goto failed;
    }
    bzero(if_name, sizeof(if_name));
    strlcpy(if_name, interface_name, sizeof(if_name));
    status = eapolcontroller_client_get_session(server,
						if_name,
						&bootstrap, &au_session);
    if (status != KERN_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE,
		  "eapolcontroller_client_get_session(%s): %s",
		  interface_name, mach_error_string(status));
	goto failed;
    }
    if (bootstrap != MACH_PORT_NULL && au_session != MACH_PORT_NULL) {
	task_set_bootstrap_port(mach_task_self(), bootstrap);
	if (audit_session_join(au_session) == AU_DEFAUDITSID) {
	    EAPLOG_FL(LOG_NOTICE,
		      "audit_session_join returned AU_DEFAULTSID");
	    goto failed;
	}
	session_established = TRUE;
    }

 failed:
    return (session_established);
}

/* handler for mach messages received on the notify port */
static void
EAPOLClientHandleMachMessage(EAPOLClientRef client)
{
    /* the union data type to resolve MACH_RCV_TOO_LARGE error */
    union {
        mach_msg_header_t 		header;
        mach_no_senders_notification_t 	no_senders;
        uint8_t 			buffer[1024];
    } msg;
    kern_return_t		kr;
    Boolean			server_died = FALSE;

    /* receive the message from the port */
    kr = mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT,
		  0, sizeof(msg), client->notify_port,
		  0, MACH_PORT_NULL);

    if (kr != KERN_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, "mach_msg receive failed: %s",
		  mach_error_string(kr));
	return;
    }
    if (msg.header.msgh_id == MACH_NOTIFY_NO_SENDERS) {
	EAPLOG_FL(LOG_NOTICE, "EAPOLController server died");
	server_died = TRUE;
    }
    (*client->callback_func)(client, server_died, client->callback_arg);
}

/* function that creates a dispatch source for MACH_RECV events */
static dispatch_source_t
EAPOLClientDispatchSourceCreate(EAPOLClientRef client, dispatch_queue_t queue)
{
    dispatch_source_t	source;
    kern_return_t	status;

    /* allocate receive port */
    status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
			       &client->notify_port);
    if (status != KERN_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, "mach_port_allocate failed: %s",
		  mach_error_string(status));
	return NULL;
    }

    /* insert send right for the port */
    status = mach_port_insert_right(mach_task_self(), client->notify_port,
				   client->notify_port, MACH_MSG_TYPE_MAKE_SEND);
    if (status != KERN_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, "mach_port_insert_right failed: %s",
		  mach_error_string(status));
	mach_port_mod_refs(mach_task_self(), client->notify_port,
			  MACH_PORT_RIGHT_RECEIVE, -1);
	client->notify_port = MACH_PORT_NULL;
	return NULL;
    }

    /* create dispatch source with DISPATCH_SOURCE_TYPE_MACH_RECV */
    source = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV,
				    client->notify_port, 0,
				    queue);
    if (source == NULL) {
	EAPLOG_FL(LOG_NOTICE, "dispatch_source_create failed");
	mach_port_mod_refs(mach_task_self(), client->notify_port,
			  MACH_PORT_RIGHT_RECEIVE, -1);
	mach_port_deallocate(mach_task_self(), client->notify_port);
	client->notify_port = MACH_PORT_NULL;
	return NULL;
    }

    dispatch_set_context(source, (void *)client);

    /* dispatch sources are created in a suspended state */
    client->notify_source_suspended = TRUE;

    /* event handler - called on arrival of mach messages */
    dispatch_source_set_event_handler(source, ^{
	EAPOLClientRef client = (EAPOLClientRef)dispatch_get_context(source);
	EAPOLClientHandleMachMessage(client);
    });
    return source;
}

void
EAPOLClientActivate(EAPOLClientRef client)
{
    if (client != NULL && client->notify_source != NULL) {
	/* activate the dispatch source to start receiving messages */
	dispatch_activate(client->notify_source);
	client->notify_source_suspended = FALSE;
    }
}

void
EAPOLClientCancel(EAPOLClientRef client)
{
    if (client != NULL && client->notify_source != NULL) {
	/* cancel the dispatch source to stop receiving messages */
	dispatch_source_cancel(client->notify_source);
    }
}

EAPOLClientRef
EAPOLClientAttach(const char * interface_name,
		  EAPOLClientCallBack callback_func,
		  void * callback_arg,
		  dispatch_queue_t queue,
		  CFDictionaryRef * control_dict, 
		  int * result_p)
{
    EAPOLClientRef		client = NULL;
    xmlDataOut_t		control = NULL;
    unsigned int		control_len = 0;
    mach_port_t			port_old;
    boolean_t			remove_send_right = TRUE;
    int				result = 0;
    mach_port_t			server;
    kern_return_t		status;

    *result_p = 0;
    *control_dict = NULL;
    if (callback_func == NULL) {
	result = EINVAL;
	goto failed;
    }
    status = eapolcontroller_server_port(&server);
    if (status != BOOTSTRAP_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, "eapolcontroller_server_port(): %s", 
		  mach_error_string(status));
	result = ENXIO;
	goto failed;
    }

    client = malloc(sizeof(*client));
    bzero(client, sizeof(*client));
    strlcpy(client->if_name, interface_name, sizeof(client->if_name));
    client->notify_source = EAPOLClientDispatchSourceCreate(client, queue);
    if (client->notify_source == NULL) {
	EAPLOG_FL(LOG_ERR, "EAPOLClientDispatchSourceCreate failed");
	result = errno;
	goto failed;
    }
    status = mach_port_request_notification(mach_task_self(),
					    client->notify_port,
					    MACH_NOTIFY_NO_SENDERS,
					    1,
					    client->notify_port,
					    MACH_MSG_TYPE_MAKE_SEND_ONCE,
					    &port_old);
    if (status != KERN_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, "mach_port_request_notification(): %s", 
		  mach_error_string(status));
	result = ENXIO;
	goto failed;
    }
    remove_send_right = FALSE;
    status = eapolcontroller_client_attach(server,
					   client->if_name,
					   client->notify_port, &client->session_port,
					   &control, &control_len, &result);
    if (status != KERN_SUCCESS) {
	if (status == MACH_SEND_INVALID_DEST) {
	    /* we didn't move the send right to the server */
	    remove_send_right = TRUE;
	}
	EAPLOG_FL(LOG_NOTICE, 
		  "eapolcontroller_client_attach(%s): %s",
		  client->if_name, mach_error_string(status));
	result = ENXIO;
	goto failed;
    }
    if (control != NULL) {
	*control_dict
	    = my_CFPropertyListCreateWithBytePtrAndLength(control,
							  control_len);
	(void)vm_deallocate(mach_task_self(), (vm_address_t)control,
			    control_len);
	if (*control_dict == NULL) {
	    result = ENOMEM;
	    goto failed;
	}
    }
    if (result != 0) {
	goto failed;
    }
    client->callback_func = callback_func;
    client->callback_arg = callback_arg;
    return (client);

 failed:
    if (client != NULL) {
	EAPOLClientInvalidate(client, remove_send_right);
	free(client);
    }
    my_CFRelease(control_dict);
    *result_p = result;
    return (NULL);
}

int
EAPOLClientDetach(EAPOLClientRef * client_p)
{
    EAPOLClientRef 		client;
    int				result = 0;
    kern_return_t		status;

    if (client_p == NULL) {
	return (0);
    }
    client = *client_p;
    if (client == NULL) {
	return (0);
    }
    if (client->session_port != MACH_PORT_NULL) {
	status = eapolcontroller_client_detach(client->session_port, 
					       &result);
	if (status != KERN_SUCCESS) {
	    EAPLOG_FL(LOG_NOTICE, 
		      "eapolcontroller_client_detach(%s): %s",
		      client->if_name, mach_error_string(status));
	    result = ENXIO;
	}
    }
    EAPOLClientInvalidate(client, FALSE);
    free(client);
    *client_p = NULL;
    return (result);
}

int
EAPOLClientGetConfig(EAPOLClientRef client, CFDictionaryRef * control_dict)
{
    xmlDataOut_t		control = NULL;
    unsigned int		control_len = 0;
    int				result = 0;
    kern_return_t		status;

    *control_dict = NULL;
    status = eapolcontroller_client_getconfig(client->session_port, 
					      &control, &control_len,
					      &result);
    if (status != KERN_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, 
		  "eapolcontroller_client_getconfig(%s): %s",
		  client->if_name, mach_error_string(status));
	result = ENXIO;
	goto done;
    }
    if (control != NULL) {
	*control_dict 
	    = my_CFPropertyListCreateWithBytePtrAndLength(control, control_len);
	(void)vm_deallocate(mach_task_self(), (vm_address_t)control,
			    control_len);
	if (*control_dict == NULL) {
	    result = ENOMEM;
	    goto done;
	}
    }
 done:
    if (result != 0) {
	my_CFRelease(control_dict);
    }
    return (result);
}

int
EAPOLClientReportStatus(EAPOLClientRef client, CFDictionaryRef status_dict)
{
    CFDataRef			data = NULL;
    int				result = 0;
    kern_return_t		status;

    if (isA_CFDictionary(status_dict) == NULL) {
	result = EINVAL;
	goto done;
    }
    data = CFPropertyListCreateData(NULL, status_dict,
				    kCFPropertyListBinaryFormat_v1_0,
				    0, NULL);

    if (data == NULL) {
	result = ENOMEM;
	goto done;
    }
    status = eapolcontroller_client_report_status(client->session_port,
						  (xmlDataOut_t)
						  CFDataGetBytePtr(data),
						  (int)CFDataGetLength(data),
						  &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_client_report_status failed", status);
	result = ENXIO;
	goto done;
    }
 done:
    my_CFRelease(&data);
    return (result);
}

int
EAPOLClientForceRenew(EAPOLClientRef client)
{
    int 			result = 0;
    kern_return_t		status;

    status = eapolcontroller_client_force_renew(client->session_port,
						&result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_client_force_renew failed", status);
	result = ENXIO;
    }
    return (result);
}

#if ! TARGET_OS_IPHONE

int
EAPOLClientUserCancelled(EAPOLClientRef client)
{
    int 			result = 0;
    kern_return_t		status;

    status = eapolcontroller_client_user_cancelled(client->session_port,
						   &result);
    if (status != KERN_SUCCESS) {
	mach_error("eapolcontroller_client_user_cancelled failed", status);
	result = ENXIO;
    }
    return (result);
}

#endif /* ! TARGET_OS_IPHONE */
