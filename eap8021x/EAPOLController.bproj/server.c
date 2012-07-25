/*
 * Copyright (c) 2001-2010 Apple Inc. All rights reserved.
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
#include "eapolcontrollerServer.h"
#include "eapolcontroller_types.h"
#include "eapolcontroller_ext.h"
#include "server.h"
#include "myCFUtil.h"

extern boolean_t		eapolcontroller_server(mach_msg_header_t *, 
						       mach_msg_header_t *);

static uid_t S_uid = -1;
static gid_t S_gid = -1;

static CFMachPortRef		server_cfport;

static vm_address_t
my_CFPropertyListCreateVMData(CFPropertyListRef plist,
			      mach_msg_type_number_t * 	ret_data_len)
{
    vm_address_t	data;
    int			data_len;
    kern_return_t	status;
    CFDataRef		xml_data;

    data = 0;
    *ret_data_len = 0;
    xml_data = CFPropertyListCreateXMLData(NULL, plist);
    if (xml_data == NULL) {
	goto done;
    }
    data_len = CFDataGetLength(xml_data);
    status = vm_allocate(mach_task_self(), &data, data_len, TRUE);
    if (status != KERN_SUCCESS) {
	goto done;
    }
    bcopy((char *)CFDataGetBytePtr(xml_data), (char *)data, data_len);
    *ret_data_len = data_len;

 done:
    my_CFRelease(&xml_data);
    return (data);
}

static CFPropertyListRef
my_CFPropertyListCreateWithBytePtrAndLength(const void * data, int data_len)
{
    CFPropertyListRef	plist;
    CFDataRef		xml_data;

    xml_data = CFDataCreateWithBytesNoCopy(NULL, 
					   (const UInt8 *)data, data_len,
					   kCFAllocatorNull);
    if (xml_data == NULL) {
	return (NULL);
    }
    plist = CFPropertyListCreateFromXMLData(NULL,
					    xml_data,
					    kCFPropertyListImmutable,
					    NULL);
    CFRelease(xml_data);
    return (plist);
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
	*status = (xmlDataOut_t)my_CFPropertyListCreateVMData(dict, status_len);
	if (*status == NULL) {
	    syslog(LOG_NOTICE, "EAPOLController: failed to serialize data");
	}
    }
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

#if ! TARGET_OS_EMBEDDED
kern_return_t
eapolcontroller_copy_loginwindow_config(mach_port_t server,
					if_name_t if_name,
					xmlDataOut_t * config,
					mach_msg_type_number_t * config_len,
					int * result)
{
    CFDictionaryRef	dict = NULL;

    *config = NULL;
    *config_len = 0;
    *result = ControllerCopyLoginWindowConfiguration(if_name, &dict);
    if (dict != NULL) {
	*config = (xmlDataOut_t)my_CFPropertyListCreateVMData(dict, config_len);
	if (*config == NULL) {
	    syslog(LOG_NOTICE, "EAPOLController: failed to serialize data");
	}
    }
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_copy_autodetect_info(mach_port_t server,
				     xmlDataOut_t * info,
				     mach_msg_type_number_t * info_len,
				     int * result)
{
    CFDictionaryRef	dict = NULL;

    *info = NULL;
    *info_len = 0;
    *result = ControllerCopyAutoDetectInformation(&dict);
    if (dict != NULL) {
	*info = (xmlDataOut_t)my_CFPropertyListCreateVMData(dict, info_len);
	if (*info == NULL) {
	    syslog(LOG_NOTICE, "EAPOLController: failed to serialize data");
	}
    }
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_did_user_cancel(mach_port_t server,
				if_name_t if_name,
				boolean_t * user_cancelled)
{
    *user_cancelled = ControllerDidUserCancel(if_name);
    return (KERN_SUCCESS);
}

#endif /* ! TARGET_OS_EMBEDDED */

kern_return_t
eapolcontroller_start(mach_port_t server,
		      if_name_t if_name, xmlData_t config, 
		      mach_msg_type_number_t config_len,
		      mach_port_t bootstrap,
		      mach_port_t au_session,
		      int * ret_result)
{
    CFDictionaryRef	dict = NULL;
    int			result = EINVAL;

    if (config == NULL) {
	goto done;
    }
    dict = my_CFPropertyListCreateWithBytePtrAndLength(config, config_len);
    (void)vm_deallocate(mach_task_self(), (vm_address_t)config, config_len);
    if (isA_CFDictionary(dict) == NULL) {
	goto done;
    }
    if (S_uid == -1) {
	result = EPERM;
	goto done;
    }
    result = ControllerStart(if_name, S_uid, S_gid, dict, bootstrap,
			     au_session);

done:
    *ret_result = result;
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

#if ! TARGET_OS_EMBEDDED 
kern_return_t
eapolcontroller_start_system(mach_port_t server,
			     if_name_t if_name, 
			     xmlData_t options,
			     mach_msg_type_number_t options_len,
			     int * ret_result)
{
    CFDictionaryRef	dict = NULL;
    int			result = EINVAL;

    if (options != NULL) {
	dict = my_CFPropertyListCreateWithBytePtrAndLength(options, 
							   options_len);
	(void)vm_deallocate(mach_task_self(), (vm_address_t)options,
			    options_len);
	if (isA_CFDictionary(dict) == NULL) {
	    goto done;
	}
    }
    if (S_uid == -1) {
	result = EPERM;
	goto done;
    }
    result = ControllerStartSystem(if_name, S_uid, S_gid, dict);

 done:
    my_CFRelease(&dict);
    *ret_result = result;
    return (KERN_SUCCESS);
}

kern_return_t 
eapolcontroller_client_user_cancelled(mach_port_t server,
				      int * result)
{
    *result = ControllerClientUserCancelled(server);
    return (KERN_SUCCESS);
};

#endif /* ! TARGET_OS_EMBEDDED */

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
		       int * ret_result)
{
    CFDictionaryRef	dict = NULL;
    int			result = EINVAL;

    if (config == NULL) {
	goto done;
    }
    dict = my_CFPropertyListCreateWithBytePtrAndLength(config, config_len);
    (void)vm_deallocate(mach_task_self(), (vm_address_t)config, config_len);
    if (isA_CFDictionary(dict) == NULL) {
	goto done;
    }
    result = ControllerUpdate(if_name, S_uid, S_gid, dict);

 done:
    my_CFRelease(&dict);
    *ret_result = result;
    return (KERN_SUCCESS);
}

kern_return_t
eapolcontroller_provide_user_input(mach_port_t server,
				   if_name_t if_name, xmlData_t user_input, 
				   mach_msg_type_number_t user_input_len,
				   int * ret_result)
{
    CFDictionaryRef	dict = NULL;
    int			result = EINVAL;
    
    if (user_input == NULL) {
	goto done;
    }
    dict = my_CFPropertyListCreateWithBytePtrAndLength(user_input,
						       user_input_len);
    (void)vm_deallocate(mach_task_self(), (vm_address_t)user_input,
			user_input_len);
    if (isA_CFDictionary(dict) == NULL) {
	goto done;
    }
    result = ControllerProvideUserInput(if_name, S_uid, S_gid, dict);

 done:
    my_CFRelease(&dict);
    *ret_result = result;
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
			      mach_port_t * au_session,
			      int * ret_result)
{
    int			pid;
    CFDictionaryRef	dict = NULL;
    int			result = EINVAL;
    kern_return_t	status;

    *control = NULL;
    *control_len = 0;
    status = pid_for_task(task, &pid);
    if (status != KERN_SUCCESS) {
	(void)mach_port_deallocate(mach_task_self(), notify_port);
	goto done;
    }
    result = ControllerClientAttach(pid, if_name, notify_port, session,
				    &dict, bootstrap, au_session);
    if (dict != NULL) {
	*control = (xmlDataOut_t)my_CFPropertyListCreateVMData(dict, 
							       control_len);
	if (*control == 0) {
	    syslog(LOG_NOTICE, "EAPOLController: failed to serialize data");
	}
    }
    my_CFRelease(&dict);

 done:
    if (task != TASK_NULL) {
	(void)mach_port_deallocate(mach_task_self(), task);
    }
    *ret_result = result;
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
	*control = (xmlDataOut_t)my_CFPropertyListCreateVMData(dict, 
							       control_len);
	if (*control == NULL) {
	    syslog(LOG_NOTICE, "EAPOLController: failed to serialize data");
	}
    }
    my_CFRelease(&dict);
    return (KERN_SUCCESS);
}

kern_return_t 
eapolcontroller_client_report_status(mach_port_t server,
				     xmlData_t status_data,
				     mach_msg_type_number_t status_data_len,
				     int * ret_result)
{
    CFDictionaryRef	dict = NULL;
    int			result = EINVAL;

    if (status_data == NULL) {
	goto done;
    }
    dict = my_CFPropertyListCreateWithBytePtrAndLength(status_data,
						       status_data_len);
    (void)vm_deallocate(mach_task_self(), (vm_address_t)status_data,
			status_data_len);
    if (isA_CFDictionary(dict) == NULL) {
	goto done;
    }
    result = ControllerClientReportStatus(server, dict);

 done:
    my_CFRelease(&dict);
    *ret_result = result;
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
server_active(void)
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
    if (notify->not_header.msgh_id > MACH_NOTIFY_LAST
	|| notify->not_header.msgh_id < MACH_NOTIFY_FIRST) {
	return FALSE;	/* if this is not a notification message */
    }
    return (TRUE);
}

void
server_handle_request(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    mach_msg_return_t 	r;
    mach_msg_header_t *	request = (mach_msg_header_t *)msg;
    mach_msg_header_t *	reply;
#define REPLY_SIZE_AS_UINT64 \
	((eapolcontroller_subsystem.maxsize + sizeof(uint64_t) - 1) / sizeof(uint64_t))
    uint64_t		reply_s[REPLY_SIZE_AS_UINT64];

    if (process_notification(request) == FALSE) {
	read_trailer(request);
        
	/* ALIGN: reply is aligned to at least sizeof(uint64) bytes */
	reply = (mach_msg_header_t *)(void *)reply_s;
        
	if (eapolcontroller_server(request, reply) == FALSE) {
	    syslog(LOG_NOTICE,
		   "EAPOLController: unknown message ID (%d)",
		   request->msgh_id);
	    mach_msg_destroy(request);
	}
	else {
	    int		options;

	    options = MACH_SEND_MSG;
	    if (MACH_MSGH_BITS_REMOTE(reply->msgh_bits)
                != MACH_MSG_TYPE_MOVE_SEND_ONCE) {
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
    }
    return;
}

void
server_register(void)
{
    mach_port_t		server_port;
    kern_return_t 	status;

    status = bootstrap_check_in(bootstrap_port, EAPOLCONTROLLER_SERVER, 
				&server_port);
    if (status != BOOTSTRAP_SUCCESS) {
	syslog(LOG_NOTICE, "EAPOLController: bootstrap_check_in failed, %s",
	       mach_error_string(status));
	return;
    }
    server_cfport = _SC_CFMachPortCreateWithPort(NULL, server_port,
						 server_handle_request, NULL);
    return;
}

void
server_start(void)
{
    CFRunLoopSourceRef	rls;

    if (server_cfport != NULL) {
	rls = CFMachPortCreateRunLoopSource(NULL, server_cfport, 0);
	CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
	CFRelease(rls);
    }
}
