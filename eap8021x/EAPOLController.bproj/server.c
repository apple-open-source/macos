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

/* 
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <sys/errno.h>
#include <signal.h>
#include <mach/mach.h>
#include <mach/message.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <syslog.h>
#include <SystemConfiguration/SCPrivate.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCDPlugin.h>
#include <CoreFoundation/CFMachPort.h>
#include "controller.h"
#include "eapolcontroller.h"
#include "eapolcontroller_types.h"
#include "eapolcontroller_ext.h"
#include "server.h"
#include "myCFUtil.h"

extern struct mig_subsystem	eapolcontroller_subsystem;
extern boolean_t		eapolcontroller_server(mach_msg_header_t *, 
						       mach_msg_header_t *);

static uid_t S_uid = -1;
static gid_t S_gid = -1;

static CFMachPortRef		server_cfport;

static Boolean
xmlSerialize(CFPropertyListRef		obj,
	     CFDataRef			*xml,
	     void			**dataRef,
	     CFIndex			*dataLen)
{
    CFDataRef	myXml;

    if (!obj) {
	/* if no object to serialize */
	return FALSE;
    }

    if (!xml && !(dataRef && dataLen)) {
	/* if not keeping track of allocated space */
	return FALSE;
    }

    myXml = CFPropertyListCreateXMLData(NULL, obj);
    if (!myXml) {
	if (xml)	*xml     = NULL;
	if (dataRef)	*dataRef = NULL;
	if (dataLen)	*dataLen = 0;
	return FALSE;
    }

    if (xml) {
	*xml = myXml;
	if (dataRef) {
	    *dataRef = (void *)CFDataGetBytePtr(myXml);
	}
	if (dataLen) {
	    *dataLen = CFDataGetLength(myXml);
	}
    } else {
	kern_return_t	status;

	*dataLen = CFDataGetLength(myXml);
	status = vm_allocate(mach_task_self(), (void *)dataRef, *dataLen, TRUE);
	if (status != KERN_SUCCESS) {
	    CFRelease(myXml);
	    *dataRef = NULL;
	    *dataLen = 0;
	    return FALSE;
	}

	bcopy((char *)CFDataGetBytePtr(myXml), *dataRef, *dataLen);
	CFRelease(myXml);
    }

    return TRUE;
}


static Boolean
xmlUnserialize(CFPropertyListRef	*obj,
	       void			*dataRef,
	       CFIndex			dataLen)
{
    kern_return_t		status;
    CFDataRef		xml;
    CFStringRef		xmlError;

    if (!obj) {
	return FALSE;
    }

    xml = CFDataCreate(NULL, (void *)dataRef, dataLen);
    status = vm_deallocate(mach_task_self(), (vm_address_t)dataRef, dataLen);
    if (status != KERN_SUCCESS) {
	/* non-fatal???, proceed */
    }
    *obj = CFPropertyListCreateFromXMLData(NULL,
					   xml,
					   kCFPropertyListImmutable,
					   &xmlError);
    CFRelease(xml);

    if (!obj) {
	if (xmlError) {
	    CFRelease(xmlError);
	}
	return FALSE;
    }
    return TRUE;
}

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

kern_return_t
eapolcontroller_get_state(mach_port_t server,
			  if_name_t if_name,
			  int * state,
			  int * result)
{
    *result = ControllerGetState(if_name, state);
    return (KERN_SUCCESS);
}


kern_return_t
eapolcontroller_copy_status(mach_port_t server,
			    if_name_t if_name, 
			    xmlDataOut_t * status, 
			    mach_msg_type_number_t * status_len,
			    int * state,
			    int * result)
{
    CFDictionaryRef	dict = NULL;

    *status = NULL;
    *status_len = 0;
    *result = ControllerCopyStateAndStatus(if_name, state, &dict);
    if (dict != NULL) {
	if (!xmlSerialize(dict, NULL, 
			  (void **)status, (CFIndex *)status_len)) {
	    syslog(LOG_NOTICE, "EAPOLController: failed to serialize data");
	    *status = NULL;
	    *status_len = 0;
	}
    }
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_start(mach_port_t server,
		      if_name_t if_name, xmlData_t config, 
		      mach_msg_type_number_t config_len,
		      mach_port_t bootstrap,
		      int * result)
{
    CFDictionaryRef	dict = NULL;

    *result = 0;
    if (config == NULL
	|| xmlUnserialize((CFPropertyListRef *)&dict, 
			  (void *)config, config_len) == FALSE
	|| isA_CFDictionary(dict) == NULL) {
	*result = EINVAL;
	goto failed;
    }
    if (S_uid == -1) {
	*result = EPERM;
	goto failed;
    }
    *result = ControllerStart(if_name, S_uid, S_gid, dict, bootstrap);
 failed:
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_stop(mach_port_t server,
		     if_name_t if_name, int * result)
{
    *result = ControllerStop(if_name, S_uid, S_gid);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_update(mach_port_t server,
		       if_name_t if_name, xmlData_t config, 
		       mach_msg_type_number_t config_len,
		       int * result)
{
    CFDictionaryRef	dict = NULL;

    *result = 0;
    if (config == NULL
	|| xmlUnserialize((CFPropertyListRef *)&dict, 
			  (void *)config, config_len) == FALSE
	|| isA_CFDictionary(dict) == NULL) {
	*result = EINVAL;
	goto failed;
    }
    *result = ControllerUpdate(if_name, S_uid, S_gid,
			       dict);

 failed:
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_retry(mach_port_t server,
		      if_name_t if_name, int * result)
{
    *result = ControllerRetry(if_name, S_uid, S_gid);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_set_logging(mach_port_t server, if_name_t if_name, 
			    int32_t level, int * result)
{
    *result = ControllerSetLogLevel(if_name, S_uid, S_gid, level);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_client_attach(mach_port_t server, task_t task,
			      if_name_t if_name, 
			      mach_port_t notify_port, 
			      mach_port_t * session,
			      xmlDataOut_t * control, 
			      mach_msg_type_number_t * control_len,
			      mach_port_t * bootstrap,
			      int * result)
{
    int			pid;
    CFDictionaryRef	dict = NULL;
    kern_return_t	status;

    *control = NULL;
    *control_len = 0;
    status = pid_for_task(task, &pid);
    if (status != KERN_SUCCESS) {
	(void)mach_port_destroy(mach_task_self(), notify_port);
	*result = EINVAL;
	goto failed;
    }
    *result = ControllerClientAttach(pid, if_name, notify_port, session,
				     &dict, bootstrap);
    if (dict != NULL) {
	if (!xmlSerialize(dict, NULL, 
			  (void **)control, (CFIndex *)control_len)) {
	    syslog(LOG_NOTICE, "EAPOLController: failed to serialize data");
	    *control = NULL;
	    *control_len = 0;
	}
    }
    my_CFRelease(&dict);
 failed:
    if (task != TASK_NULL) {
	(void)mach_port_destroy(mach_task_self(), task);
    }
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_client_detach(mach_port_t server, int * result)
{
    *result = ControllerClientDetach(server);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_client_getconfig(mach_port_t server,
				 xmlDataOut_t * control, 
				 mach_msg_type_number_t * control_len,
				 int * result)
{
    CFDictionaryRef	dict = NULL;

    *control = NULL;
    *control_len = 0;

    *result = ControllerClientGetConfig(server, &dict);
    if (dict != NULL) {
	if (!xmlSerialize(dict, NULL, 
			  (void **)control, (CFIndex *)control_len)) {
	    syslog(LOG_NOTICE, "EAPOLController: failed to serialize data");
	    *control = NULL;
	    *control_len = 0;
	}
    }
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

kern_return_t 
eapolcontroller_client_report_status(mach_port_t server,
				     xmlData_t status_data,
				     mach_msg_type_number_t status_data_len,
				     int * result)
{
    CFDictionaryRef	status_dict = NULL;

    *result = 0;
    if (status_data == NULL
	|| xmlUnserialize((CFPropertyListRef *)&status_dict, 
			  (void *)status_data, status_data_len) == FALSE
	|| isA_CFDictionary(status_dict) == NULL) {
	*result = EINVAL;
	goto failed;
    }
    *result = ControllerClientReportStatus(server, status_dict);
 failed:
    my_CFRelease(&status_dict);
    return (KERN_SUCCESS);
};

kern_return_t 
eapolcontroller_client_force_renew(mach_port_t server,
				   int * result)
{
    *result = ControllerClientForceRenew(server);
    return (KERN_SUCCESS);
};

boolean_t
server_active()
{
    mach_port_t		server;
    kern_return_t	result;

    result = eapolcontroller_server_port(&server);
    if (result == BOOTSTRAP_SUCCESS) {
	return (TRUE);
    }
    return (FALSE);
}

static boolean_t
process_notification(mach_msg_header_t * request)
{
    mach_no_senders_notification_t * notify;

    notify = (mach_no_senders_notification_t *)request;
    if ((notify->not_header.msgh_id > MACH_NOTIFY_LAST) ||
	(notify->not_header.msgh_id < MACH_NOTIFY_FIRST)) {
	return FALSE;	/* if this is not a notification message */
    }
    switch (notify->not_header.msgh_id) {
    case MACH_NOTIFY_NO_SENDERS:
    case MACH_NOTIFY_DEAD_NAME:
	ControllerClientPortDead(notify->not_header.msgh_local_port);
	break;
    default :
	break;
    }
    return (TRUE);
}

void
server_handle_request(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mach_msg_return_t 	r;
    mach_msg_header_t *	request = (mach_msg_header_t *)msg;
    mach_msg_header_t *	reply;
    char		reply_s[128];

    if (process_notification(request) == FALSE) {
	read_trailer(request);
	if (eapolcontroller_subsystem.maxsize > sizeof(reply_s)) {
	    syslog(LOG_NOTICE, "EAPOLController: %d > %d",
		   eapolcontroller_subsystem.maxsize, sizeof(reply_s));
	    reply = (mach_msg_header_t *)
		malloc(eapolcontroller_subsystem.maxsize);
	}
	else {
	    reply = (mach_msg_header_t *)reply_s;
	}
	if (eapolcontroller_server(request, reply) == FALSE) {
	    syslog(LOG_NOTICE, "unknown message ID (%d) received",
		   request->msgh_id);
	    mach_msg_destroy(request);
	}
	else {
	    int		options;

	    options = MACH_SEND_MSG;
	    if (MACH_MSGH_BITS_REMOTE(reply->msgh_bits) 
		== MACH_MSG_TYPE_MOVE_SEND) {
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
		syslog(LOG_NOTICE, "EAPOLController: mach_msg(send): %s", 
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

void
server_init()
{
    kern_return_t 	status;
    CFRunLoopSourceRef	rls;

    server_cfport = CFMachPortCreate(NULL, server_handle_request, NULL, NULL);
    rls = CFMachPortCreateRunLoopSource(NULL, server_cfport, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    status = bootstrap_register(bootstrap_port, EAPOLCONTROLLER_SERVER, 
				CFMachPortGetPort(server_cfport));
    if (status != BOOTSTRAP_SUCCESS) {
	mach_error("bootstrap_register", status);
    }
    return;
}

