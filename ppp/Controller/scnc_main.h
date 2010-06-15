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



#ifndef __SCNC_MAIN__
#define __SCNC_MAIN__

#include <net/if.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <vproc.h>
#include "../Drivers/L2TP/L2TP-plugin/vpn_control.h"
#include <CoreFoundation/CFUserNotification.h>

#ifdef TARGET_EMBEDDED_OS
#include <CoreTelephony/CTServerConnectionPriv.h>
#endif

//#define PRINTF(x) 	printf x
#define PRINTF(x)

#define DEBUG 1

// pretend L2TP/IPSec is a Cisco IPSec connection
#define FAKE_L2TP_IPSEC 1

/* service client, used for arbitration */
struct service_client {
    TAILQ_ENTRY(service_client) next;
    void 	*client;
    int 	autoclose;
};

enum {
    FLAG_SETUP = 0x1,		/* needs to process service setup */
    FLAG_FREE = 0x2,		/* needs to dispose of the ppp structure */
    FLAG_CONNECT = 0x4,		/* needs to connect service */
    FLAG_CONFIGCHANGEDNOW = 0x8,	/* setup has changed, dialondemand needs to rearm with no delay */
    FLAG_CONFIGCHANGEDLATER = 0x10,	/* setup has changed, dialondemand needs to rearm with delay if applicable */
    FLAG_ONTRAFFIC = 0x20,	/* is the connection currently in dial-on-traffic mode */
    FLAG_ALERTERRORS = 0x40,	/* error alerts are enabled */
    FLAG_ALERTPASSWORDS = 0x80,	/* passwords alerts are enabled */
   // FLAG_STARTING = 0x100	/* pppd is started, and hasn't yet updated the phase */
	FLAG_FIRSTDIAL = 0x200, /* is it the first autodial attempt after major event */
	FLAG_ONDEMAND = 0x400, /* is the connection currently in on-demand mode */
	FLAG_USECERTIFICATE = 0x800, /* is the connection using cert authentication ? */

	/* setup keys */
	FLAG_SETUP_ONTRAFFIC = 0x00010000, /* is DialOnDemand (onTraffic) set ? ONLY FOR PPP */
	FLAG_SETUP_DISCONNECTONLOGOUT = 0x00020000, /* is DisconnectOnLogout set ? */
	FLAG_SETUP_DISCONNECTONSLEEP = 0x00040000, /* is DisconnectOnSleep set ? */
	FLAG_SETUP_PREVENTIDLESLEEP = 0x00080000, /* is PreventIdleSleep set ? */
	FLAG_SETUP_DISCONNECTONFASTUSERSWITCH = 0x00100000, /* is DisconnectOnFastUserSwitch set ? */
	FLAG_SETUP_ONDEMAND = 0x00200000 /* is OnDemand set ? VPN OnDemand scheme */
};

enum {
    TYPE_PPP = 0x0,			/* PPP TYPE service */
    TYPE_IPSEC = 0x1		/* IPSEC TYPE service */
};

/* this struct contains all the information to control a service */

struct ppp_service {
    CFBundleRef	bundle;			/* PPP device bundle */

    int		controlfd[2];		/* pipe for pppd control */
    int		statusfd[2];		/* pipe for pppd status */

    int			ndrv_socket;		/* ndrv socket to maintain transport device up */
    u_int32_t 	phase;			/* where the link is at */    
    u_char      ifname[IFNAMSIZ];	/* real ifname */

    u_int32_t 	laststatus;		/* last fail status */
    u_int32_t 	lastdevstatus;	/* last device specific fail status */

    CFDictionaryRef newconnectopts; 	/* new connect options to use */ 
    uid_t		newconnectuid;			/* new connect uid */ 
    gid_t		newconnectgid;			/* new connect gid */ 
    mach_port_t	newconnectbootstrap; 	/* new connect bootstrap */ 
};

struct ipsec_service {
    u_int32_t 	phase;			/* where the link is at */    
    u_int32_t 	laststatus;		/* last fail status */
    CFMutableDictionaryRef config;		/* ipsec config dict */ 
	struct sockaddr_in our_address;		/* our side IP address */
	struct sockaddr_in peer_address;	/* the other side IP address */
	CFRunLoopTimerRef timerref ;	/* timer ref */
	
	/* racoon communication */
	int			controlfd;		/* racoon control socket */
	CFSocketRef	controlref;		/* racoon control socket ref */
	int			eventfd;		/* kernel event socket */
	CFSocketRef	eventref;		/* kernel event socket ref */
    u_int8_t		*msg;			// message in pogress from client
    u_int32_t		msglen;			// current message length
    u_int32_t		msgtotallen;	// total expected len
    struct vpnctl_hdr	msghdr;		// message header read 
	int				config_applied; // has racoon config been applied ?
	int				policies_installed; // were ipsec policies installed ?
	/* dynamically installed mode config policies */
	int				modecfg_installed; 
    CFMutableDictionaryRef modecfg_policies;		/* mode config policies */ 
	int				modecfg_defaultroute; /* is default route intalled for that service ? */
	int				modecfg_peer_route_set; 
	int				modecfg_routes_installed;
	u_int32_t		inner_local_addr;
	u_int32_t		inner_local_mask;
	int				kernctl_sock;		/* kernel control socket to the virtual interface */
	struct in_addr	ping_addr;			/* ping address to trigger phase 2 */ 
	int				ping_count;			/* numer of ping phase 2 left */ 
	char			if_name[16];		/* virtual interface name (e.g ipsec0)  */
	u_int16_t		xauth_flags;		/* fields being requested in xauth  */
	char			lower_interface[16]; /* underlying interface name */
	struct in_addr	lower_gateway;		/* lower interface gateway */
	CFRunLoopTimerRef interface_timerref ;	/* timer ref */
	CFStringRef		banner;	/* banner ref */

#ifdef TARGET_EMBEDDED_OS
	/* Edge context */
	CFMachPortRef			edgePort;
	CFRunLoopSourceRef		edgeRLS;
	CTServerConnectionRef	edgeConnection;
	CFRunLoopTimerRef		edge_timerref;
#else
    u_int32_t               lower_interface_media;
#endif
    u_int32_t               timeout_lower_interface_change;
	
	/* async dns query */
	CFMachPortRef			dnsPort;
	struct timeval			dnsQueryStart;
	CFArrayRef				resolvedAddress;	/* CFArray[CFData] */
	int						resolvedAddressError;
	int						next_address; // next address to use in the array
	CFAbsoluteTime			display_reenroll_alert_time;
};

struct service {
 
	/* generic portion of the service */

    TAILQ_ENTRY(service) next;

    CFStringRef	serviceID;		/* service ID in the cache */
    CFStringRef	typeRef;		/* type string */
    CFStringRef	subtypeRef;		/* subtype string */
    u_char		*sid;			/* C version of the servceID */
    // type/subtype/unit will make the reference number
    u_int16_t 	type;			/* type of link (PPP or IPSEC) */
    u_int16_t 	subtype;		/* subtype of link */
    u_int16_t 	unit;			/* ref number in the interfaces managed by this Controller */

    
    // status information frequently used
    u_int32_t	flags;			/* action flags */
    CFStringRef device;			/* transport device (en0, en1,...) */
	uid_t		uid;			/* uid of the user who started the connection */
	gid_t		gid;			/* gid of the user who started the connection */
	mach_port_t   bootstrap;	/* bootstrap of the user who started the connection */
    pid_t     	pid;            /* pid of associated process */
	
    CFDictionaryRef connectopts;		/* connect options in use */ 
    CFDictionaryRef systemprefs;		/* system prefs */ 
#ifdef TARGET_EMBEDDED_OS
    CFStringRef profileIdentifier;		/* profile Identifier in the prefs */ 
#endif
	
    CFUserNotificationRef userNotificationRef;		/* user notification */ 
    CFRunLoopSourceRef userNotificationRLS;		/* user notification rls */ 

#ifndef TARGET_EMBEDDED_OS
	vproc_transaction_t	vt;		/* opaque handle used to track outstanding transactions, used by instant off */
#endif
	u_int32_t 	connecttime;		/* time when connection occured */
    u_int32_t   establishtime;      /* time when connection established */

    // list of clients for this service. used to arbitrate connection/disconnection
    TAILQ_HEAD(, service_client) 	client_head;

	/* specific portion of the service */
	union {
		struct ppp_service ppp;
		struct ipsec_service ipsec;
	} u;
	
};


#ifndef kSCValNetInterfaceTypeIPSec
#define kSCValNetInterfaceTypeIPSec CFSTR("IPSec")
#endif


extern CFURLRef 	gBundleURLRef;
extern CFBundleRef 	gBundleRef;
extern CFURLRef 	gIconURLRef;
extern CFStringRef 	gPluginsDir;
extern CFURLRef		gPluginsURLRef;

extern SCDynamicStoreRef	gDynamicStore;
extern CFStringRef			gLoggedInUser;
extern uid_t				gLoggedInUserUID;

extern int					gSleeping;
extern uint64_t				gWakeUpTime;
extern double				gTimeScaleSeconds;
extern CFRunLoopSourceRef 	gStopRls;

extern char			*gIPSecAppVersion;

extern int					gSCNCVerbose;
extern int					gSCNCDebug;

#ifdef TARGET_EMBEDDED_OS
extern int					gNattKeepAliveInterval;
#endif

int client_gone(void *client);

int allow_sleep();
int allow_stop();
int allow_dispose(struct service *serv);
void service_started(struct service *serv);
void service_ended(struct service *serv);
void phase_changed(struct service *serv, int phase);
void disable_ondemand(struct service *serv);


void user_notification_callback(CFUserNotificationRef userNotification, CFOptionFlags responseFlags);

int scnc_stop(struct service *serv, void *client, int signal);
int scnc_start(struct service *serv, CFDictionaryRef options, void *client, int autoclose, uid_t uid, gid_t gid, mach_port_t bootstrap);
int scnc_getstatus(struct service *serv);
int scnc_copyextendedstatus(struct service *serv, void **reply, u_int16_t *replylen);
int scnc_copystatistics(struct service *serv, void **reply, u_int16_t *replylen);
int scnc_getconnectdata(struct service *serv, void **reply, u_int16_t *replylen, int all);
int scnc_getconnectsystemdata(struct service *serv, void **reply, u_int16_t *replylen);
int scnc_suspend(struct service *serv);
int scnc_resume(struct service *serv);

struct service *findbyserviceID(CFStringRef serviceID);
struct service *findbypid(pid_t pid);
struct service *findbysid(u_char *data, int len);
struct service *findbyref(u_int16_t type, u_int32_t ref);
u_int32_t makeref(struct service *serv);


#endif
