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

#include "queue_data.h"

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
   //struct opt_str 		file;
    struct {
        struct opt_str 		name;
        struct opt_long 	speed;
        struct opt_str 		connectscript;
        struct opt_long 	speaker;
        struct opt_long 	pulse;
        struct opt_long 	dialmode;
        //struct opt_str 		connectprgm;
    } dev;
    struct {
        struct opt_str 		remoteaddr;
        struct opt_str 		altremoteaddr;
        struct opt_long 	idletimer;
        struct opt_long 	sessiontimer;
        struct opt_long		terminalmode;
        struct opt_str 		terminalscript;
        //struct opt_str 		terminalprgm;
        struct opt_long 	connectdelay;
        struct opt_str 		listenfilter;
        struct opt_long 	loopback;
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
        struct opt_long 	echo;
    } lcp;
    struct {
        struct opt_long 	hdrcomp;
        struct opt_long 	localaddr;
        struct opt_long 	remoteaddr;
        struct opt_long 	useserverdns;
        struct opt_long 	serverdns1;
        struct opt_long 	serverdns2;
    } ipcp;
    struct {
        struct opt_long 	proto;
        struct opt_str 		name;
        struct opt_str 		passwd;
    } auth;
    struct {
        struct opt_str 		logfile;
        struct opt_long 	remindertimer;
        struct opt_long 	alertenable;
        struct opt_long 	autolisten;
        struct opt_long 	autoconnect;
        struct opt_long 	disclogout;
        struct opt_long 	connlogout;
        struct opt_long 	verboselog;
    } misc;
};

struct client_opts {
    TAILQ_ENTRY(client_opts) next;
    u_long		link;		// link for which options apply
    struct options	opts;		// options to apply
};

struct client {
    // u_char 		set;		// is this record in use ?
    CFSocketRef	 	ref;		// socket we talk with, -1 = client free

    /* event notification */
    u_char	 	notify; 	// 0 = do not notify, 1 = notification active
    u_long	 	notify_link; 	// link ref we want notification (or 0xFFFFFFFF for all links)

    /* option management */
    TAILQ_HEAD(, client_opts) 	opts_head;

    /* for clients extensions*/
    u_short		readfd_len;	// not null if read pending for ttybuf
    u_long		readfd_cookie;	// pending cookie
    u_long		readfd_link;	// corresponding link
    struct queue_data 	readfd_queue;  	// unread data

};


#define MAX_CLIENT 	16		// Fix Me : should be more dynamic

extern struct client 		*clients[MAX_CLIENT];


u_long client_init_all ();
u_long client_new (CFSocketRef ref);
u_long client_dispose (u_short id);
u_long client_notify (u_long link, u_long state, u_long error);
struct options *client_newoptset (u_short id, u_long link);
struct options *client_findoptset (u_short id, u_long link);

#endif
