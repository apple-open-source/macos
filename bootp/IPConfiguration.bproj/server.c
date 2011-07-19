/*
 * Copyright (c) 2000-2010 Apple Inc. All rights reserved.
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
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <syslog.h>
#include <CoreFoundation/CFMachPort.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFPropertyList.h>
#include <SystemConfiguration/SCPrivate.h>

#include "dprintf.h"
#include "ipconfigServer.h"
#include "ipconfigd.h"
#include "ipconfig_ext.h"
#include "globals.h"

static uid_t S_uid = -1;
static gid_t S_gid = -1;

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

static ipconfig_status_t
method_info_from_xml_data(xmlData_t xml_data,
			  mach_msg_type_number_t xml_data_len,
			  ipconfig_method_t * method_p,
			  ipconfig_method_data_t * * method_data_p)
{
    CFPropertyListRef	plist = NULL;
    ipconfig_status_t	status;

    if (xml_data != NULL) {
	plist = my_CFPropertyListCreateWithBytePtrAndLength(xml_data,
							    xml_data_len);
	(void)vm_deallocate(mach_task_self(), (vm_address_t)xml_data,
			    xml_data_len);
    }
    status = ipconfig_method_info_from_plist(plist, method_p, method_data_p);
    if (plist != NULL) {
	CFRelease(plist);
    }
    return (status);
}

static __inline__ void
read_trailer(mig_reply_error_t * request)
{
    mach_msg_format_0_trailer_t	*trailer;
    trailer = (mach_msg_security_trailer_t *)((vm_offset_t)request +
					      round_msg(request->Head.msgh_size));

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
_ipconfig_if_addr(mach_port_t p, if_name_t name, 
		  u_int32_t * addr, ipconfig_status_t * status)
{
    dprintf(("Getting interface address\n"));
    *status = get_if_addr(name, addr);
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_if_count(mach_port_t p, int * count)
{
    dprintf(("Getting interface count\n"));
    *count = get_if_count();
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_get_option(mach_port_t p, if_name_t name, int option_code,
		     inline_data_t option_data,
		     mach_msg_type_number_t * option_dataCnt,
		     ipconfig_status_t * status)

{
    *status = get_if_option(name, option_code, option_data, option_dataCnt);
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_get_packet(mach_port_t p, if_name_t name,
		     inline_data_t packet_data,
		     mach_msg_type_number_t * packet_dataCnt,
		     ipconfig_status_t * status)
{
    *status = get_if_packet(name, packet_data, packet_dataCnt);
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_get_v6_packet(mach_port_t p, if_name_t name,
		     inline_data_t packet_data,
		     mach_msg_type_number_t * packet_dataCnt,
		     ipconfig_status_t * status)
{
    *status = get_if_v6_packet(name, packet_data, packet_dataCnt);
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_set(mach_port_t p, if_name_t name,
	      xmlData_t xml_data,
	      mach_msg_type_number_t xml_data_len,
	      ipconfig_status_t * ret_status)
{
    ipconfig_method_t		method;
    ipconfig_method_data_t *	method_data;
    ipconfig_status_t		status;

    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    status = method_info_from_xml_data(xml_data, xml_data_len,
				       &method, &method_data);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = set_if(name, method, method_data);
    if (method_data != NULL) {
	free(method_data);
    }
 done:
    *ret_status = status;
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_set_verbose(mach_port_t p, int verbose,
		      ipconfig_status_t * status)
{
    if (S_uid != 0) {
	*status = ipconfig_status_permission_denied_e;
    }
    else {
	*status = set_verbose(verbose);
    }
    return (KERN_SUCCESS);
}

#ifdef IPCONFIG_TEST_NO_ENTRY
kern_return_t
_ipconfig_set_something(mach_port_t p, int verbose,
			ipconfig_status_t * status)
{
    return (KERN_SUCCESS);
}
#endif /* IPCONFIG_TEST_NO_ENTRY */

kern_return_t
_ipconfig_add_service(mach_port_t p, 
		      if_name_t name,
		      xmlData_t xml_data,
		      mach_msg_type_number_t xml_data_len,
		      inline_data_t service_id,
		      mach_msg_type_number_t * service_id_len,
		      ipconfig_status_t * ret_status)
{
    ipconfig_method_t		method;
    ipconfig_method_data_t *	method_data;
    ipconfig_status_t		status;

    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    status = method_info_from_xml_data(xml_data, xml_data_len,
				       &method, &method_data);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = add_service(name, method, method_data, service_id, service_id_len);
    if (method_data != NULL) {
	free(method_data);
    }
 done:
    *ret_status = status;
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_set_service(mach_port_t p, 
		      if_name_t name,
		      xmlData_t xml_data,
		      mach_msg_type_number_t xml_data_len,
		      inline_data_t service_id,
		      mach_msg_type_number_t * service_id_len,
		      ipconfig_status_t * ret_status)
{
    ipconfig_method_t		method;
    ipconfig_method_data_t *	method_data;
    ipconfig_status_t		status;

    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    status = method_info_from_xml_data(xml_data, xml_data_len,
				       &method, &method_data);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = set_service(name, method, method_data, 
			 service_id, service_id_len);
    if (method_data != NULL) {
	free(method_data);
    }
 done:
    *ret_status = status;
    return (KERN_SUCCESS);
}

kern_return_t 
_ipconfig_remove_service_with_id(mach_port_t server,
				 inline_data_t service_id,
				 mach_msg_type_number_t service_id_len,
				 ipconfig_status_t * ret_status)
{
    ipconfig_status_t	status;

    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    status = remove_service_with_id(service_id, service_id_len);

 done:
    *ret_status = status;
    return (KERN_SUCCESS);
}

kern_return_t 
_ipconfig_find_service(mach_port_t server,
		       if_name_t name,
		       boolean_t exact,
		       xmlData_t xml_data,
		       mach_msg_type_number_t xml_data_len,
		       inline_data_t service_id,
		       mach_msg_type_number_t *service_id_len,
		       ipconfig_status_t * ret_status)
{
    ipconfig_method_t		method;
    ipconfig_method_data_t *	method_data;
    ipconfig_status_t		status;

    status = method_info_from_xml_data(xml_data, xml_data_len,
				       &method, &method_data);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = find_service(name, exact, method, method_data,
			  service_id, service_id_len);
    if (method_data != NULL) {
	free(method_data);
    }

 done:
    *ret_status = status;
    return (KERN_SUCCESS);
}

kern_return_t 
_ipconfig_remove_service(mach_port_t server,
			 if_name_t name,
			 xmlData_t xml_data,
			 mach_msg_type_number_t xml_data_len,
			 ipconfig_status_t * ret_status)
{
    ipconfig_method_t		method;
    ipconfig_method_data_t *	method_data;
    ipconfig_status_t		status;

    if (S_uid != 0) {
	status = ipconfig_status_permission_denied_e;
	goto done;
    }
    status = method_info_from_xml_data(xml_data, xml_data_len,
				       &method, &method_data);
    if (status != ipconfig_status_success_e) {
	goto done;
    }
    status = remove_service(name, method, method_data);
    if (method_data != NULL) {
	free(method_data);
    }

 done:
    *ret_status = status;
    return (KERN_SUCCESS);
}

boolean_t
server_active()
{
    mach_port_t		server;
    kern_return_t	status;

    status = ipconfig_server_port(&server);
    if (status == BOOTSTRAP_SUCCESS) {
	return (TRUE);
    }
    return (FALSE);
}

static void
S_ipconfig_server(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    char 		reply_buf[2048 + 256];
    mach_msg_options_t 	options = 0;
    mig_reply_error_t * request = (mig_reply_error_t *)msg;
    mig_reply_error_t *	reply;
    mach_msg_return_t 	r = MACH_MSG_SUCCESS;

    read_trailer(request);
    if (_ipconfig_subsystem.maxsize > sizeof(reply_buf)) {
	syslog(LOG_NOTICE, "IPConfiguration server: %d > %ld",
	       _ipconfig_subsystem.maxsize, sizeof(reply_buf));
	reply = (mig_reply_error_t *)
	    malloc(_ipconfig_subsystem.maxsize);
    }
    else {
	reply = (mig_reply_error_t *)reply_buf;
    }
    if (ipconfig_server(&request->Head, &reply->Head) == FALSE) {
	my_log(LOG_INFO, "IPConfiguration: unknown message ID (%d) received",
	       request->Head.msgh_id);
    }

    /* Copied from Libc/mach/mach_msg.c:mach_msg_server_once(): Start */
    if (!(reply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
	if (reply->RetCode == MIG_NO_REPLY)
	    reply->Head.msgh_remote_port = MACH_PORT_NULL;
	else if ((reply->RetCode != KERN_SUCCESS) &&
		 (request->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)) {
	    /* destroy the request - but not the reply port */
	    request->Head.msgh_remote_port = MACH_PORT_NULL;
	    mach_msg_destroy(&request->Head);
	}
    }
    /*
     *	We don't want to block indefinitely because the client
     *	isn't receiving messages from the reply port.
     *	If we have a send-once right for the reply port, then
     *	this isn't a concern because the send won't block.
     *	If we have a send right, we need to use MACH_SEND_TIMEOUT.
     *	To avoid falling off the kernel's fast RPC path unnecessarily,
     *	we only supply MACH_SEND_TIMEOUT when absolutely necessary.
     */
    if (reply->Head.msgh_remote_port != MACH_PORT_NULL) {
	r = mach_msg(&reply->Head,
		     (MACH_MSGH_BITS_REMOTE(reply->Head.msgh_bits) ==
		      MACH_MSG_TYPE_MOVE_SEND_ONCE) ?
		     MACH_SEND_MSG|options :
		     MACH_SEND_MSG|MACH_SEND_TIMEOUT|options,
		     reply->Head.msgh_size, 0, MACH_PORT_NULL,
		     MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if ((r != MACH_SEND_INVALID_DEST) &&
	    (r != MACH_SEND_TIMED_OUT))
	    goto done_once;
	r = MACH_MSG_SUCCESS;
    }
    if (reply->Head.msgh_bits & MACH_MSGH_BITS_COMPLEX)
	mach_msg_destroy(&reply->Head);
 done_once:
    /* Copied from Libc/mach/mach_msg.c:mach_msg_server_once(): End */

    if (reply != (mig_reply_error_t *)reply_buf) {
	free(reply);
    }

    if (r != MACH_MSG_SUCCESS) {
	my_log(LOG_INFO, "IPConfiguration msg_send: %s", mach_error_string(r));
    }
    return;
}

void
server_init()
{
    CFRunLoopSourceRef	rls;
    CFMachPortRef	ipconfigd_port;
    mach_port_t		server_port;
    kern_return_t 	status;

    status = bootstrap_check_in(bootstrap_port, IPCONFIG_SERVER, 
				&server_port);
    if (status != BOOTSTRAP_SUCCESS) {
	my_log(LOG_NOTICE,
	       "IPConfiguration: bootstrap_check_in failed, %s",
	       mach_error_string(status));
	return;
    }
    ipconfigd_port = _SC_CFMachPortCreateWithPort(NULL, server_port,
						  S_ipconfig_server,
						  NULL);
    rls = CFMachPortCreateRunLoopSource(NULL, ipconfigd_port, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    CFRelease(ipconfigd_port);
    return;
}
