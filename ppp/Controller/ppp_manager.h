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

//#define PRINTF(x) 	printf x
#define PRINTF(x)


u_long 	ppp_init_all();
struct ppp *ppp_new(CFStringRef serviceID, CFStringRef subtypeRef);
void ppp_updatesetup(struct ppp *ppp, CFDictionaryRef service);
void ppp_updatestate(struct ppp *ppp, CFDictionaryRef service);
void ppp_printlist();
void ppp_postupdatesetup();
int ppp_dispose(struct ppp *ppp);
struct ppp *ppp_findbyname(u_char *name, u_short unit);
struct ppp *ppp_findbyserviceID(CFStringRef serviceID);
struct ppp *ppp_findbyref(u_long ref);
struct ppp *ppp_findbysid(u_char *data, int len);
void ppp_setorder(struct ppp *ppp, u_int16_t order);
u_short ppp_findfreeunit(u_short subfam);
u_int32_t ppp_makeref(struct ppp *ppp);
u_int32_t ppp_makeifref(struct ppp *ppp);
int ppp_logout();
int ppp_login();
int ppp_logswitch();
int ppp_doconnect(struct ppp *ppp, CFDictionaryRef opts, u_int8_t dialondemand, void *client, int autoclose);
int ppp_dodisconnect(struct ppp *ppp, int sig, void *client);
int ppp_dosuspend(struct ppp *ppp);
int ppp_doresume(struct ppp *ppp);
int ppp_clientgone(void *client);

/* ppp service client, used for arbitration */
struct ppp_client {
    TAILQ_ENTRY(ppp_client) next;
    void 	*client;
    int 	autoclose;
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
    
    // name/ifunit make the ifnet interface
    u_char      name[IFNAMSIZ];		/* real ifname */
    u_int16_t 	ifunit;			/* real ifunit number */
    
    // status information frequently used
    u_int32_t 	phase;			/* where the link is at */    
    u_int32_t 	oldphase;		/* where the link was at last lime */    
    u_int32_t 	conntime; 		/* time when connected	*/
    u_int32_t 	disconntime; 		/* time when disconnection is planned, 
                                            as advertised by the server, or as required by the session timer */
    u_int32_t 	laststatus;		/* last fail status */
    u_int32_t 	lastdevstatus;		/* last device specific fail status */
    u_int32_t 	alertenable;		/* alert level flags */
    CFBundleRef	bundle;			/* PPP device bundle */

    int	iFD[2];				/* pipe for pppd params */

    u_int8_t	dialondemand;		/* is pppd curently running in dialondemand mode ? */
    u_int8_t	started_link;		/* pppd has just been started */
    u_int8_t	kill_link;		/* pppd needs to be killed when it appears (sig number) */
    u_int8_t	kill_sent;		/* kill signal has been sent, wait for idle */
    u_int8_t	dosetup;		/* needs to process service setup */
    u_int8_t	needconnect;		/* needs to process service setup */
    u_int8_t	needdispose;		/* needs to dispose of the ppp structure */
    u_int8_t    setupchanged;           /* setup has changed, dialondemand needs to rearm */
    pid_t     	pid;                    /* pid of associated pppd */
    CFDictionaryRef needconnectopts; 	/* connect options to use */ 
    CFDictionaryRef connectopts; 	/* connect options in use */ 

    // list of clients for this service. used to arbitrate connection/disconnection
    TAILQ_HEAD(, ppp_client) 	client_head;
};


extern CFURLRef 	gBundleURLRef;
extern CFBundleRef 	gBundleRef;
extern CFStringRef 	gCancelRef;
extern CFStringRef 	gInternetConnectRef;
extern CFURLRef 	gIconURLRef;
extern CFStringRef 	gPluginsDir;
extern CFURLRef		gPluginsURLRef;

#endif
