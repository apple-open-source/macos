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
struct ppp *ppp_find(struct msg *msg);
void ppp_setorder(struct ppp *ppp, u_int16_t order);
u_short ppp_findfreeunit(u_short subfam);
u_int32_t ppp_makeref(struct ppp *ppp);
u_int32_t ppp_makeifref(struct ppp *ppp);
int ppp_logout();
int ppp_login();
int ppp_doconnect(struct ppp *ppp, struct options *opts, u_int8_t dialondemand);
int ppp_dodisconnect(struct ppp *ppp, int sig);
int ppp_dosuspend(struct ppp *ppp);
int ppp_doresume(struct ppp *ppp);


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

    u_int8_t	started_link;		/* pppd has just been started */
    u_int8_t	kill_link;		/* pppd needs to be killed when it appears (sig number) */
    u_int8_t	kill_sent;		/* kill signal has been sent, wait for idle */
    u_int8_t	dosetup;		/* needs to process service setup */
    u_int8_t	needconnect;		/* needs to process service setup */
    u_int8_t	needdispose;		/* needs to dispose of the ppp structure */
    struct options *needconnectopts; 	/* connect options to use */ 
};


int ppp_getoptval(struct ppp *ppp, struct options *opts, u_int32_t otype, void *pdata, u_int32_t *plen, CFDictionaryRef dict);

extern CFURLRef 	gBundleURLRef;
extern CFBundleRef 	gBundleRef;
extern CFStringRef 	gCancelRef;
extern CFStringRef 	gInternetConnectRef;
extern CFURLRef 	gIconURLRef;
extern CFStringRef 	gPluginsDir;
extern CFURLRef		gPluginsURLRef;

#endif
