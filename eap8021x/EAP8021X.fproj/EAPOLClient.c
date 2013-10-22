/*
 * Copyright (c) 2002-2013 Apple Inc. All rights reserved.
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
#include <CoreFoundation/CFRunLoop.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>
#include "eapolcontroller.h"
#include "eapolcontroller_types.h"
#include "eapolcontroller_ext.h"
#include "myCFUtil.h"
#include "EAPOLClient.h"
#include "EAPLog.h"

struct EAPOLClient_s {
    CFMachPortRef		notify_cfport;
    CFRunLoopSourceRef		rls;
    mach_port_t			session_port;
    EAPOLClientCallBackRef	callback_func;
    void *			callback_arg;
    if_name_t			if_name;
};

static void
EAPOLClientInvalidate(EAPOLClientRef client, boolean_t remove_send_right)
{
    if (client->notify_cfport != NULL) {
	mach_port_t	port;

	port = CFMachPortGetPort(client->notify_cfport);
	mach_port_mod_refs(mach_task_self(), port,
			   MACH_PORT_RIGHT_RECEIVE, -1);
	if (remove_send_right) {
	    mach_port_mod_refs(mach_task_self(), port, 
			       MACH_PORT_RIGHT_SEND, -1);
	}
	CFMachPortInvalidate(client->notify_cfport);
	my_CFRelease(&client->notify_cfport);
    }
    if (client->rls != NULL) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
			      client->rls, kCFRunLoopDefaultMode);
	my_CFRelease(&client->rls);
    }
    if (client->session_port != MACH_PORT_NULL) {
	(void)mach_port_deallocate(mach_task_self(), client->session_port);
	client->session_port = MACH_PORT_NULL;
    }
    return;
}

static void
EAPOLClientHandleMessage(CFMachPortRef port, void * msg, 
			 CFIndex size, void * info)
{
    EAPOLClientRef		client = (EAPOLClientRef)info;
    mach_msg_empty_rcv_t *	buf = msg;
    mach_msg_id_t		msgid = buf->header.msgh_id;
    Boolean			server_died = FALSE;

    if (msgid == MACH_NOTIFY_NO_SENDERS) {
	EAPLOG_FL(LOG_NOTICE, "EAPOLController server died");
	server_died = TRUE;
	EAPOLClientInvalidate(client, FALSE);
    }
    (*client->callback_func)(client, server_died, client->callback_arg);
    return;
}

static CFMachPortRef
_EAPOLClientCFMachPortCreate(CFMachPortCallBack callout, CFMachPortContext * context)
{
    CFMachPortRef	cf_port;
    mach_port_t 	port = MACH_PORT_NULL;
    kern_return_t 	status;

    status = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (status != KERN_SUCCESS) {
	goto failed;
    }
    status = mach_port_insert_right(mach_task_self(), port, port, 
				    MACH_MSG_TYPE_MAKE_SEND);
    if (status != KERN_SUCCESS) {
	goto failed;
    }
    cf_port = CFMachPortCreateWithPort(NULL, port, callout, context, NULL);
    if (cf_port != NULL) {
	return (cf_port);
    }
 failed:
    if (port != MACH_PORT_NULL) {
	mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
	mach_port_deallocate(mach_task_self(), port);
    }
    return NULL;

}

EAPOLClientRef
EAPOLClientAttach(const char * interface_name, 
		  EAPOLClientCallBack callback_func, 
		  void * callback_arg, 
		  CFDictionaryRef * control_dict, 
		  int * result_p)
{
    mach_port_t			au_session;
    mach_port_t			bootstrap;
    EAPOLClientRef		client = NULL;
    xmlDataOut_t		control = NULL;
    unsigned int		control_len = 0;
    CFMachPortContext		context = {0, NULL, NULL, NULL, NULL};
    mach_port_t			port;
    mach_port_t			port_old;
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
    context.info = client;
    client->notify_cfport 
	= _EAPOLClientCFMachPortCreate(EAPOLClientHandleMessage, &context);
    if (client->notify_cfport == NULL) {
	EAPLOG_FL(LOG_NOTICE, "_EAPOLClientCFMachPortCreate failed");
	result = errno;
	goto failed;
    }
    port = CFMachPortGetPort(client->notify_cfport);
    status = mach_port_request_notification(mach_task_self(),
					    port,
					    MACH_NOTIFY_NO_SENDERS,
					    1,
					    port,
					    MACH_MSG_TYPE_MAKE_SEND_ONCE,
					    &port_old);
    if (status != KERN_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, "mach_port_request_notification(): %s", 
		  mach_error_string(status));
	goto failed;
    }
    status = eapolcontroller_client_attach(server, mach_task_self(),
					   client->if_name,
					   port, &client->session_port,
					   &control, &control_len,
					   &bootstrap, &au_session, &result);
    if (status != KERN_SUCCESS) {
	EAPLOG_FL(LOG_NOTICE, 
		  "eapolcontroller_client_attach(%s): %s",
		  client->if_name, mach_error_string(status));
	result = ENXIO;
	goto failed;
    }
    if (bootstrap != MACH_PORT_NULL) {
	task_set_bootstrap_port(mach_task_self(), bootstrap);
    }
    if (au_session != MACH_PORT_NULL) {
	if (audit_session_join(au_session) == AU_DEFAUDITSID) {
	    EAPLOG_FL(LOG_NOTICE,
		      "audit_session_join returned AU_DEFAULTSID");
	}
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
    client->rls = CFMachPortCreateRunLoopSource(NULL, client->notify_cfport, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(),
		       client->rls, kCFRunLoopDefaultMode);
    return (client);

 failed:
    if (client != NULL) {
	EAPOLClientInvalidate(client, TRUE);
    }
    my_CFRelease(control_dict);
    if (client != NULL) {
	free(client);
    }
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
    EAPOLClientInvalidate(client, TRUE);
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
    data = CFPropertyListCreateXMLData(NULL, status_dict);
    if (data == NULL) {
	result = ENOMEM;
	goto done;
    }
    status = eapolcontroller_client_report_status(client->session_port,
						  (xmlDataOut_t)
						  CFDataGetBytePtr(data),
						  CFDataGetLength(data),
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

#if ! TARGET_OS_EMBEDDED

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

#endif /* ! TARGET_OS_EMBEDDED */
