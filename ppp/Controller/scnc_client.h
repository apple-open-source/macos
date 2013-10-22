/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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


#ifndef __SCNC_CLIENT_H__
#define __SCNC_CLIENT_H__

#include "ppp_msg.h"

#include <sys/queue.h>

#define MAXDATASIZE 2048

struct msg {
    struct ppp_msg_hdr	hdr;
    unsigned char 	data[MAXDATASIZE];
};


struct client_opts {
    TAILQ_ENTRY(client_opts)	next;
    CFStringRef			serviceid;	// service for which options apply
    CFMutableDictionaryRef	opts;		// options to apply
};

#define CLIENT_FLAG_PRIVILEDGED			0x1		// client can send priviledged commands
#define CLIENT_FLAG_IPC_READY			0x2		// client is IPC ready VPN configuration app
#define CLIENT_FLAG_NOTIFY_EVENT		0x4		// client wants notifications for events
#define CLIENT_FLAG_NOTIFY_STATUS		0x8		// client wants notifications for status
#define CLIENT_FLAG_IS_SOCKET			0x10	// client uses socket API (instead of Mach)
#define CLIENT_FLAG_SWAP_BYTES			0x20	// client requires bytes swapping (not in network order)

struct client {

    TAILQ_ENTRY(client) next;

	/* socket API */
    CFSocketRef	 	socketRef;		// socket we talk with
	
	/* Mach API */
    CFMachPortRef   sessionPortRef;	// session mach port ref
    mach_port_t		notify_port;	// session mach port ref
    CFRunLoopSourceRef   sessionRls;	// session mach port ref
    CFStringRef		serviceID;		// service used by the client
	mach_port_t		bootstrap_port;		// bootstrap port use by client
	mach_port_name_t    au_session;		// audit session port
	
	uid_t			uid;			// user uid at the end of the control api
	uid_t			gid;			// user gid at the end of the control api
	pid_t			pid;			// pid of user app
	
    u_int8_t		*msg;			// message in pogress from client
    u_int32_t		msglen;			// current message length
    u_int32_t		msgtotallen;	// total expected len
    struct ppp_msg_hdr	msghdr;		// message header read 

	u_int32_t		flags;			//flags for this structure

    /* 
        event notification
        events can be for event transition of status change
        Event/Status are generated for ALL the services or for a unique service
        Service is the same for both status and events
    */
    u_char	 	*notify_serviceid;	// add service id in the notification
    u_int32_t	 notify_link; 		// link ref we want notification (or 0xFFFFFFFF for all links)
    
    Boolean     has_machport_priv;  // sandbox mach port privelege
    /* option management */
    TAILQ_HEAD(, client_opts) 	opts_head;

};



u_long client_init_all ();
struct client *client_new_socket (CFSocketRef ref, int priviledged, uid_t uid, gid_t gid);
struct client *client_new_mach (CFMachPortRef port, CFRunLoopSourceRef rls, CFStringRef serviceID, uid_t uid, gid_t gid, pid_t pid, mach_port_t bootstrap, mach_port_t notify_port, mach_port_t au_session, Boolean has_machport_priv);
void client_dispose (struct client *client);
CFMutableDictionaryRef client_newoptset (struct client *client, CFStringRef serviceid);
CFMutableDictionaryRef client_findoptset (struct client *client, CFStringRef serviceid);
u_long client_notify (CFStringRef serviceID, u_char* sid, u_int32_t link, u_long state, u_long error, int notification, SCNetworkConnectionStatus status);


struct client *client_findbysocketref(CFSocketRef ref);
struct client *client_findbymachport(mach_port_t port);


#endif
