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

#import <stdio.h>
#import <unistd.h>
#import <stdlib.h>
#import <mach/mach.h>
#import <mach/mach_error.h>
#import <servers/bootstrap.h>
#define CFRUNLOOP_NEW_API
#import <CoreFoundation/CFMachPort.h>

#import "dprintf.h"
#import "threadcompat.h"
#import "machcompat.h"
#import "../bootplib/ipconfig.h"
#import "ipconfigd.h"
#import "ipconfig_ext.h"
#import "ts_log.h"

//static char request_buf[1024];
static char reply_buf[1024];

static uid_t S_uid = -1;
static gid_t S_gid = -1;

#ifdef MOSX
static __inline__ void
read_trailer(msg_header_t * request)
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
	/* XXX
	 * Change 0 to -1 in the following two lines when CF is fixed.
	 */
	S_uid = 0;
	S_gid = 0;
    }
}
#else MOSX
static __inline__ void
read_trailer(msg_header_t * request)
{
    return;
}
#endif MOSX

kern_return_t
_ipconfig_config_if(port_t p, if_name_t name)
{
    dprintf(("config called with %s\n", name));
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_config_all(port_t p)
{
    dprintf(("config all called\n"));
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_wait_if(port_t p, if_name_t name)
{
    dprintf(("Waiting for %s to complete\n", name));
    if (S_uid == 0 && wait_if(name) == TRUE)
	return (KERN_SUCCESS);
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_wait_all(port_t p)
{

    dprintf(("Waiting for all interfaces to complete\n"));
    if (S_uid == 0) {
	wait_all();
	return (KERN_SUCCESS);
    }
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_if_name(port_t p, int intface, if_name_t name)
{

    dprintf(("Getting interface name\n"));
    if (get_if_name(intface, name) == TRUE)
	return (KERN_SUCCESS);
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_if_addr(port_t p, if_name_t name, u_int32_t * addr)
{
    dprintf(("Getting interface address\n"));
    if (get_if_addr(name, addr) == TRUE)
	return (KERN_SUCCESS);
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_if_count(port_t p, int * count)
{
    dprintf(("Getting interface count\n"));
    *count = get_if_count();
    return (KERN_SUCCESS);
}

kern_return_t
_ipconfig_get_option(port_t p, if_name_t name, int option_code,
		     inline_data_t option_data,
		     unsigned int * option_dataCnt)
{
    if (get_if_option(name, option_code, option_data, option_dataCnt) 
	== TRUE) {
	return (KERN_SUCCESS);
    }
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_get_packet(port_t p, if_name_t name,
		     inline_data_t packet_data,
		     unsigned int * packet_dataCnt)
{
    if (get_if_packet(name, packet_data, packet_dataCnt) == TRUE) {
	return (KERN_SUCCESS);
    }
    return (KERN_FAILURE);
}

kern_return_t
_ipconfig_set(port_t p, if_name_t name,
	      ipconfig_method_t method,
	      inline_data_t method_data,
	      unsigned int method_data_len,
	      ipconfig_status_t * status)
{
    if (S_uid != 0) {
	*status = ipconfig_status_permission_denied_e;
    }
    else {
	*status = set_if(name, method, method_data, method_data_len, NULL);
    }
    return (KERN_SUCCESS);
}

#if 0
void 
server_loop(void * arg)
{
    msg_header_t *	request = (msg_header_t *)request_buf;
    msg_header_t *	reply = (msg_header_t *)reply_buf;

    while (1) {
	msg_return_t r;
#ifdef MOSX
	request->msgh_size = sizeof(request_buf);
	request->msgh_local_port = service_port;
	r = msg_receive(request, 
			MSG_OPTION_NONE 
			| MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_SENDER)
			| MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0), 
			0);
#else MOSX
	request->msg_size = sizeof(request_buf);
	request->msg_local_port = service_port;
	r = msg_receive(request, MSG_OPTION_NONE, 0);
#endif
	if (r == RCV_SUCCESS) {
	    extern boolean_t ipconfig_server(msg_header_t *, msg_header_t *);

	    read_trailer(request);
	    if (ipconfig_server(request, reply)) {
		r = msg_send(reply, MSG_OPTION_NONE, 0);
		if (r != SEND_SUCCESS)
		    ts_log(LOG_INFO, "msg_send: %s", mach_error_string(r));
	    }
	}
	else {
#ifdef MOSX	    
	    ts_log(LOG_INFO, "msg_receive: %s (0x%x)", 
		   mach_error_string(r), r);
	    if (r == MACH_RCV_INVALID_NAME) {
		break; /* out of while */
	    }
#else MOSX
	    ts_log(LOG_INFO, "msg_receive: %s (%d)", mach_error_string(r), r);
	    if (r == RCV_INVALID_PORT) {
		break; /* out of while */
	    }
#endif MOSX
	}
    }
}

#endif 0

boolean_t
server_active()
{
    boolean_t 		active = FALSE;
    port_t		server;
    kern_return_t	status;

    status = ipconfig_server_port(&server, &active);
    if (active != FALSE)
	return (TRUE);
    return (FALSE);
}

static CFMachPortRef	ipconfigd_port;

static void
S_ipconfig_server(CFMachPortRef port, void *msg, CFIndex size, void *info)
{
    msg_header_t *	request = (msg_header_t *)msg;
    msg_header_t *	reply = (msg_header_t *)reply_buf;
    msg_return_t 	r;
    extern boolean_t 	ipconfig_server(msg_header_t *, msg_header_t *);

    read_trailer(request);
    if (ipconfig_server(request, reply)) {
	r = msg_send(reply, MSG_OPTION_NONE, 0);
	if (r != SEND_SUCCESS)
	    ts_log(LOG_INFO, "msg_send: %s", mach_error_string(r));
    }
    return;
}

void
server_init()
{
    CFRunLoopSourceRef	rls;
    boolean_t		active;
    kern_return_t 	status;

    active = FALSE;
    status = bootstrap_status(bootstrap_port, IPCONFIG_SERVER, &active);

    switch (status) {
      case BOOTSTRAP_SUCCESS :
	  if (active) {
	      fprintf(stderr, "\"%s\" is currently active, exiting.\n", 
		     IPCONFIG_SERVER);
	      exit(1);
	  }
	  break;
      case BOOTSTRAP_UNKNOWN_SERVICE :
	  break;
      default :
	  fprintf(stderr,
		 "bootstrap_status(): %s\n", mach_error_string(status));
	  exit(1);
    }

    ipconfigd_port = CFMachPortCreate(NULL, S_ipconfig_server, NULL, NULL);
    rls = CFMachPortCreateRunLoopSource(NULL, ipconfigd_port, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);

    status = bootstrap_register(bootstrap_port, IPCONFIG_SERVER, 
				CFMachPortGetPort(ipconfigd_port));
    if (status != BOOTSTRAP_SUCCESS) {
	mach_error("bootstrap_register", status);
	exit(1);
    }
}

