/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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


#ifndef __PPP_CLIENT_H__
#define __PPP_CLIENT_H__

// current version of the pppconfd api
#define CURRENT_VERSION 1


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

struct client {

    TAILQ_ENTRY(client) next;

    CFSocketRef	 	ref;		// socket we talk with

    u_int8_t		*msg;		// message in pogress from client
    u_int32_t		msglen;		// current message length
    u_int32_t		msgtotallen;	// total expected len
    struct ppp_msg_hdr	msghdr;		// message header read 
    
    /* 
        event notification
        events can be for event transition of status change
        Event/Status are generated for ALL the services or for a unique service
        Service is the same for both status and events
    */
    u_char	 	notify; 		// 0x0 = do not notify, 
                                                // 0x1 = event notification active, 0x2 = status notification active 
    u_char	 	notify_useserviceid;	// add service id in the notification
    u_char	 	*notify_serviceid;	// add service id in the notification
    u_long	 	notify_link; 		// link ref we want notification (or 0xFFFFFFFF for all links)
    
    /* option management */
    TAILQ_HEAD(, client_opts) 	opts_head;

};



u_long client_init_all ();
struct client *client_new (CFSocketRef ref);
void client_dispose (struct client *client);
CFMutableDictionaryRef client_newoptset (struct client *client, CFStringRef serviceid);
CFMutableDictionaryRef client_findoptset (struct client *client, CFStringRef serviceid);
u_long client_notify (u_char* sid, u_int32_t link, u_long state, u_long error, int notification);
struct client *client_findbysocketref(CFSocketRef ref);
int readn(int ref, void *data, int len);
int writen(int ref, void *data, int len);


#endif
