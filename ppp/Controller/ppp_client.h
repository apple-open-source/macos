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


#ifndef __PPP_CLIENT_H__
#define __PPP_CLIENT_H__

// current version of the pppconfd api
#define CURRENT_VERSION 1


#define MAXDATASIZE 2048

struct msg {
    struct ppp_msg_hdr	hdr;
    unsigned char 	data[MAXDATASIZE];
};

// option char
struct opt_long {
    u_char 	set;
    u_long 	val;
};

#define OPT_STR_LEN 256
// option str
struct opt_str {
    u_char 	set;
    u_char 	str[OPT_STR_LEN];
};


// options structure
struct options {
    struct {
        struct opt_str 		name;
        struct opt_long 	speed;
        struct opt_str 		connectscript;
        struct opt_long 	speaker;
        struct opt_long 	pulse;
        struct opt_long 	dialmode;
    } dev;
    struct {
        struct opt_str 		remoteaddr;
        struct opt_str 		altremoteaddr;
        struct opt_long 	idletimer;
        struct opt_long 	remindertimer;
        struct opt_long 	sessiontimer;
        struct opt_long		terminalmode;
        struct opt_str 		terminalscript;
        struct opt_long 	connectdelay;
        struct opt_long 	redialcount;
        struct opt_long 	redialinterval;
   } comm;
    struct {
        struct opt_long 	pcomp;
        struct opt_long 	accomp;
        struct opt_long 	mru;
        struct opt_long 	mtu;
        struct opt_long 	rcaccm;
        struct opt_long 	txaccm;
        struct opt_long 	echointerval;
        struct opt_long 	echofailure;
    } lcp;
    struct {
        struct opt_long 	hdrcomp;
        struct opt_long 	localaddr;
        struct opt_long 	remoteaddr;
        struct opt_long 	useserverdns;
    } ipcp;
    struct {
        struct opt_long 	proto;
        struct opt_str 		name;
        struct opt_str 		passwd;
    } auth;
    struct {
        struct opt_str 		logfile;
        struct opt_long 	alertenable;
        struct opt_long 	autoconnect;
        struct opt_long 	disclogout;
        struct opt_long 	verboselog;
    } misc;
};

struct client_opts {
    TAILQ_ENTRY(client_opts) next;
    u_long		link;		// link for which options apply
    struct options	opts;		// options to apply
};

struct client {

    TAILQ_ENTRY(client) next;

    CFSocketRef	 	ref;		// socket we talk with

    /* event notification */
    u_char	 	notify; 	// 0 = do not notify, 1 = notification active
    u_long	 	notify_link; 	// link ref we want notification (or 0xFFFFFFFF for all links)
    u_char	 	notify_useservice;	// add service id in the notification
    u_char	 	*notify_service;	// add service id in the notification
    
    /* option management */
    TAILQ_HEAD(, client_opts) 	opts_head;

};



u_long client_init_all ();
struct client *client_new (CFSocketRef ref);
void client_dispose (struct client *client);
struct options *client_newoptset (struct client *client, u_long link);
struct options *client_findoptset (struct client *client, u_long link);
u_long client_notify (u_char *serviceid, u_long link, u_long state, u_long error);
struct client *client_findbysocketref(CFSocketRef ref);


#endif
