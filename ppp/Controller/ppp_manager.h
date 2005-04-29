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



#ifndef __PPP_MANAGER__
#define __PPP_MANAGER__

#include <net/if.h>

//#define PRINTF(x) 	printf x
#define PRINTF(x)

//#define DEBUG 1

/* ppp service client, used for arbitration */
struct ppp_client {
    TAILQ_ENTRY(ppp_client) next;
    void 	*client;
    int 	autoclose;
};

enum {
    FLAG_SETUP = 0x1,		/* needs to process service setup */
    FLAG_FREE = 0x2,		/* needs to dispose of the ppp structure */
    FLAG_CONNECT = 0x4,		/* needs to connect service */
    FLAG_CONFIGCHANGEDNOW = 0x8,	/* setup has changed, dialondemand needs to rearm with no delay */
    FLAG_CONFIGCHANGEDLATER = 0x10,	/* setup has changed, dialondemand needs to rearm with delay if applicable */
    FLAG_DIALONDEMAND = 0x20,	/* is the connection currently in dial-on-demand mode */
    FLAG_ALERTERRORS = 0x40,	/* error alerts are enabled */
    FLAG_ALERTPASSWORDS = 0x80,	/* passwords alerts are enabled */
   // FLAG_STARTING = 0x100	/* pppd is started, and hasn't yet updated the phase */
	FLAG_FIRSTDIAL = 0x200 /* is it the first autodial attempt after major event */
};

/* this struct contains all the information to control a ppp interface */
struct ppp {

    TAILQ_ENTRY(ppp) next;

    CFStringRef	serviceID;		/* service ID in the cache */
    CFStringRef	subtypeRef;		/* subtype string */
    u_char 	*sid;			/* C version of the servceID */
    
    // suptype/unit will make the reference number
    u_int16_t 	subtype;		/* ppp subtype of link */
    u_int16_t 	unit;			/* ref number in the interfaces managed by this Controller */
    
    // status information frequently used
    u_int32_t 	phase;			/* where the link is at */    
    u_char      ifname[IFNAMSIZ];	/* real ifname */
    CFStringRef device;				/* transport device (en0, en1,...) */
    int			ndrv_socket;		/* ndrv socket to maintain transport device up */
    u_int32_t 	laststatus;		/* last fail status */
    u_int32_t 	lastdevstatus;		/* last device specific fail status */
    CFBundleRef	bundle;			/* PPP device bundle */

    int		controlfd[2];		/* pipe for pppd control */
    int		statusfd[2];		/* pipe for pppd status */

	uid_t   uid;				/* uid of the user who started the connection */
	gid_t   gid;				/* gid of the user who started the connection */
	mach_port_t   bootstrap;	/* bootstrap of the user who started the connection */
	
    u_int32_t	flags;			/* action flags */
    pid_t     	pid;                    /* pid of associated pppd */
    CFDictionaryRef connectopts; 	/* connect options in use */ 
    CFDictionaryRef newconnectopts; 	/* new connect options to use */ 
    uid_t		newconnectuid; 	/* new connect uid */ 
    gid_t		newconnectgid; 	/* new connect gid */ 
    mach_port_t		newconnectbootstrap; 	/* new connect bootstrap */ 

    // list of clients for this service. used to arbitrate connection/disconnection
    TAILQ_HEAD(, ppp_client) 	client_head;
};


u_long 	ppp_init_all();
void ppp_stop_all(CFRunLoopSourceRef stopRls);

void ppp_updatestatus(struct ppp *ppp, int status, int devstatus);
void ppp_updatephase(struct ppp *ppp, int phase);

struct ppp *ppp_findbyserviceID(CFStringRef serviceID);
struct ppp *ppp_findbyref(u_long ref);
struct ppp *ppp_findbysid(u_char *data, int len);
struct ppp *ppp_findbypid(pid_t pid);
u_int32_t ppp_makeref(struct ppp *ppp);

int ppp_connect(struct ppp *ppp, CFDictionaryRef options, u_int8_t dialondemand, void *client, int autoclose, uid_t uid, gid_t gid, mach_port_t bootstrap);
int ppp_disconnect(struct ppp *ppp, void *client, int signal);
int ppp_suspend(struct ppp *ppp);
int ppp_resume(struct ppp *ppp);
int ppp_getstatus (struct ppp *ppp, void **reply, u_int16_t *replylen);
int ppp_copyextendedstatus (struct ppp *ppp, void **reply, u_int16_t *replylen);
int ppp_copystatistics(struct ppp *ppp, void **reply, u_int16_t *replylen);
int ppp_getconnectdata(struct ppp *ppp, void **reply, u_int16_t *replylen, int all);
int ppp_getconnectsystemdata(struct ppp *ppp, void **reply, u_int16_t *replylen);

u_int32_t ppp_translate_error(u_int16_t subtype, u_int32_t native_ppp_error, u_int32_t native_dev_error);

int ppp_clientgone(void *client);


extern CFURLRef 	gBundleURLRef;
extern CFBundleRef 	gBundleRef;
extern CFStringRef 	gCancelRef;
extern CFStringRef 	gInternetConnectRef;
extern CFURLRef 	gIconURLRef;
extern CFStringRef 	gPluginsDir;
extern CFURLRef		gPluginsURLRef;

#endif
