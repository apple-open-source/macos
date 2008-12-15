
/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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

struct EAPOLClient_s {
    CFMachPortRef		notify_cfport;
    CFRunLoopSourceRef		rls;
    mach_port_t			session_port;
    EAPOLClientCallBackRef	callback_func;
    void *			callback_arg;
    if_name_t			if_name;
};

static void
EAPOLClientInvalidate(EAPOLClientRef client)
{
    if (client->notify_cfport != NULL) {
	CFMachPortInvalidate(client->notify_cfport);
	my_CFRelease(&client->notify_cfport);
    }
    if (client->rls != NULL) {
	CFRunLoopRemoveSource(CFRunLoopGetCurrent(),
			      client->rls, kCFRunLoopDefaultMode);
	my_CFRelease(&client->rls);
    }
    if (client->session_port != MACH_PORT_NULL) {
	(void)mach_port_destroy(mach_task_self(), client->session_port);
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
	syslog(LOG_NOTICE, 
	       "EAPOLClientHandleMessage: EAPOLController server died");
	server_died = TRUE;
	EAPOLClientInvalidate(client);
    }
    (*client->callback_func)(client, server_died, client->callback_arg);
    return;
}

EAPOLClientRef
EAPOLClientAttach(const char * interface_name, 
		  EAPOLClientCallBack callback_func, 
		  void * callback_arg, 
		  CFDictionaryRef * control_dict, 
		  int * result_p)
{
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
	fprintf(stderr, "EAPOLClient: eapolcontroller_server_port(): %s", 
		mach_error_string(status));
	result = ENXIO;
	goto failed;
    }

    client = malloc(sizeof(*client));
    bzero(client, sizeof(*client));
    strlcpy(client->if_name, interface_name, sizeof(client->if_name));
    context.info = client;
    client->notify_cfport 
	= CFMachPortCreate(NULL, EAPOLClientHandleMessage, &context, NULL);
    port = CFMachPortGetPort(client->notify_cfport);
    status = mach_port_request_notification(mach_task_self(),
					    port,
					    MACH_NOTIFY_NO_SENDERS,
					    1,
					    port,
					    MACH_MSG_TYPE_MAKE_SEND_ONCE,
					    &port_old);
    if (status != KERN_SUCCESS) {
	fprintf(stderr, "EAPOLClient: mach_port_request_notification(): %s", 
		mach_error_string(status));
	goto failed;
    }
    status = eapolcontroller_client_attach(server, mach_task_self(),
					   client->if_name,
					   port, &client->session_port,
					   &control, &control_len,
					   &bootstrap, &result);
    if (status != KERN_SUCCESS) {
	fprintf(stderr, 
		"EAPOLClient: eapolcontroller_client_attach(%s): %s\n",
		client->if_name, mach_error_string(status));
	result = ENXIO;
	goto failed;
    }
    if (bootstrap != MACH_PORT_NULL) {
	task_set_bootstrap_port(mach_task_self(), bootstrap);
    }
    if (control != NULL) {
	if (xmlUnserialize((CFPropertyListRef *)control_dict, 
			   control, control_len) == FALSE) {
	    result = EINVAL;
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
	EAPOLClientInvalidate(client);
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
    status = eapolcontroller_client_detach(client->session_port, 
					   &result);
    if (status != KERN_SUCCESS) {
	fprintf(stderr, 
		"EAPOLClient: eapolcontroller_client_detach(%s): %s\n",
		client->if_name, mach_error_string(status));
	result = ENXIO;
    }
    EAPOLClientInvalidate(client);
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
	fprintf(stderr, 
		"EAPOLClient: eapolcontroller_client_getconfig(%s): %s\n",
		client->if_name, mach_error_string(status));
	result = ENXIO;
	goto done;
    }
    if (control != NULL) {
	if (xmlUnserialize((CFPropertyListRef *)control_dict, 
			   control, control_len)
	    == FALSE) {
	    result = EINVAL;
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
    xmlDataOut_t		xml_data = NULL;
    CFIndex			xml_data_len = 0;

    if (isA_CFDictionary(status_dict) == NULL) {
	result = EINVAL;
	goto done;
    }
    if (!xmlSerialize(status_dict, &data, 
		      (void **)&xml_data, &xml_data_len)) {
	result = ENOMEM;
	goto done;
    }
    status = eapolcontroller_client_report_status(client->session_port,
						  xml_data, xml_data_len, 
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
