/*
 * Copyright (c) 2000, 2013 Apple Computer, Inc. All rights reserved.
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
#include <dns_sd.h>
#include <IOKit/network/IOEthernetController.h>

#if TARGET_OS_EMBEDDED
#include <CoreTelephony/CTServerConnectionPriv.h>
#endif
#include <sys/types.h>

//#define PRINTF(x) 	printf x
#define PRINTF(x)

#define DEBUG 1

#define FAR_FUTURE          (60.0 * 60.0 * 24.0 * 365.0 * 1000.0)

#define FLOW_DIVERT_CONTROL_FD_MAX_COUNT	2


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
    FLAG_DARKWAKE = 0x100,   /* system is in dark wake */
	FLAG_FIRSTDIAL = 0x200, /* is it the first autodial attempt after major event */
	FLAG_ONDEMAND = 0x400, /* is the connection currently in on-demand mode */
	FLAG_USECERTIFICATE = 0x800, /* is the connection using cert authentication ? */
	FLAG_AUTHEN_EXTERNAL = 0x00001000, /* get credentials externally from vpn authen agent */

	/* setup keys */
	FLAG_SETUP_ONTRAFFIC = 0x00010000, /* is DialOnDemand (onTraffic) set ? ONLY FOR PPP */
	FLAG_SETUP_DISCONNECTONLOGOUT = 0x00020000, /* is DisconnectOnLogout set ? */
	FLAG_SETUP_DISCONNECTONSLEEP = 0x00040000, /* is DisconnectOnSleep set ? */
	FLAG_SETUP_PREVENTIDLESLEEP = 0x00080000, /* is PreventIdleSleep set ? */
	FLAG_SETUP_DISCONNECTONFASTUSERSWITCH = 0x00100000, /* is DisconnectOnFastUserSwitch set ? */
	FLAG_SETUP_ONDEMAND = 0x00200000, /* is OnDemand set ? VPN OnDemand scheme */
	FLAG_SETUP_PERSISTCONNECTION = 0x00400000, /* is ConnectionPersistence enabled ? */
	FLAG_SETUP_APP_LAYER = 0x00800000, /* Are there App Rules configured? */

    FLAG_SETUP_NETWORKDETECTION = 0x01000000,
    FLAG_CONNECT_ONDEMAND = 0x02000000,
    FLAG_PROBE_FOUND = 0x04000000,
	FLAG_SETUP_DISCONNECTONWAKE = 0x08000000  /* is DisconnectOnWake enabled? */
};

enum {
    TYPE_PPP = 0x0,			/* PPP TYPE service */
    TYPE_IPSEC = 0x1,		/* IPSEC TYPE service */
};

#if TARGET_OS_EMBEDDED
enum {
	CELLULAR_BRINGUP_SUCCESS_EVENT, 
	CELLULAR_BRINGUP_FATAL_FAILURE_EVENT,
	CELLULAR_BRINGUP_NETWORK_FAILURE_EVENT
};
#endif

#ifndef kSCPropNetVPNOnDemandRuleInterfaceTypeMatch
#define kSCPropNetVPNOnDemandRuleInterfaceTypeMatch         CFSTR("InterfaceTypeMatch")

#define kSCValNetVPNOnDemandRuleInterfaceTypeMatchCellular       CFSTR("Cellular")
#define kSCValNetVPNOnDemandRuleInterfaceTypeMatchEthernet       CFSTR("Ethernet")
#define kSCValNetVPNOnDemandRuleInterfaceTypeMatchWiFi           CFSTR("WiFi")

#endif

#ifndef kSCNetworkConnectionOnDemandPluginPIDs
#define kSCNetworkConnectionOnDemandPluginPIDs		CFSTR("PluginPIDs")
#endif

#ifndef kSCNetworkConnectionOnDemandProbeResults
#define kSCNetworkConnectionOnDemandProbeResults CFSTR("ProbeResults")
#define kSCPropNetVPNOnDemandRuleActionParameters CFSTR("ActionParameters")
#define kSCValNetVPNOnDemandRuleActionEvaluateConnection CFSTR("EvaluateConnection")
#define kSCPropNetVPNOnDemandRuleActionParametersDomainAction CFSTR("DomainAction")
#define kSCPropNetVPNOnDemandRuleActionParametersDomains CFSTR("Domains")
#define kSCPropNetVPNOnDemandRuleActionParametersRequiredDNSServers CFSTR("RequiredDNSServers")
#define kSCPropNetVPNOnDemandRuleActionParametersRequiredURLStringProbe CFSTR("RequiredURLStringProbe")
#define kSCValNetVPNOnDemandRuleActionParametersDomainActionConnectIfNeeded CFSTR("ConnectIfNeeded")
#define kSCValNetVPNOnDemandRuleActionParametersDomainActionNeverConnect CFSTR("NeverConnect")
#endif

enum {
    PLUGIN_UPDATE_DOWNLOAD_COMPLETE    = 0x01,
    PLUGIN_UPDATE_INSTALL_COMPLETE      = 0x02,
    PLUGIN_UPDATE_FINISHED              = 0x03
};

#define MDNS_NAT_MAPPING_MAX	4 // num of mappings per service

typedef struct nat_reflexive_addr {
	u_int32_t          addr;
	u_int16_t          port;
} nat_reflexive_addr_t;

typedef struct mdns_nat_mapping {
	DNSServiceRef         mDNSRef;
	DNSServiceRef         mDNSRef_tmp;
	int                   mDNSRef_fd;
	u_int32_t             interfaceIndex;
	DNSServiceProtocol    protocol;
	uint16_t              privatePort;
	nat_reflexive_addr_t  reflexive;
	int                   up;
} mdns_nat_mapping_t;

typedef struct service_route {
    struct service_route	*next;
    struct in_addr			local_address;
    struct in_addr			local_mask;
    struct in_addr			dest_address;
    struct in_addr			dest_mask;
    struct in_addr			gtwy_address;
    u_int16_t				flags;
    int						installed;
} service_route_t;

/* this struct contains all the information to control a service */

struct ppp_service {
    int			controlfd[2];	/* pipe for process agent control */
    int			statusfd[2];	/* pipe for process agent status */
    pid_t     	pid;            /* pid of associated process */
    CFBundleRef	bundleRef;		/* bundle */
	
    int			ndrv_socket;	/* ndrv socket to maintain transport device up */
    u_int32_t 	phase;			/* where the link is at */    

    u_int32_t 	laststatus;		/* last fail status */
    u_int32_t 	lastdevstatus;	/* last device specific fail status */

    CFDictionaryRef newconnectopts; 	/* new connect options to use */ 
    uid_t		newconnectuid;			/* new connect uid */ 
    gid_t		newconnectgid;			/* new connect gid */ 
    mach_port_t	newconnectbootstrap; 	/* new connect bootstrap */ 
    mach_port_t	newconnectausession; 	/* new connect audit session */
	char		lower_interface[16]; /* underlying interface name */
	u_int32_t	lower_interface_media; /* underlying interface media */
};

struct ipsec_service {
    u_int32_t 	phase;			/* where the link is at */    
    u_int32_t 	laststatus;		/* last fail status */
    u_int32_t    asserted;
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
	u_int16_t		xauth_flags;		/* fields being requested in xauth  */
	char			lower_interface[16]; /* underlying interface name */
	Boolean			lower_interface_cellular;
	struct sockaddr_in	lower_gateway;		/* lower interface gateway */
	CFRunLoopTimerRef interface_timerref ;	/* timer ref */
	CFStringRef		banner;	/* banner ref */

    u_int32_t               lower_interface_media;
    u_int32_t               timeout_lower_interface_change;
	CFRunLoopRef            port_mapping_timerrun;
	CFRunLoopTimerRef       port_mapping_timerref;
	int                     awaiting_peer_resp;
	
	/* async dns query */
	CFMachPortRef			dnsPort;
	struct timeval			dnsQueryStart;
	CFArrayRef				resolvedAddress;	/* CFArray[CFData] */
	int						resolvedAddressError;
	int						next_address; // next address to use in the array
	CFAbsoluteTime			display_reenroll_alert_time;
	/* routes */
	service_route_t         *routes;
};


typedef enum {
    ONDEMAND_PAUSE_STATE_TYPE_OFF = 0,
    ONDEMAND_PAUSE_STATE_TYPE_UNTIL_REBOOT = 1,
    ONDEMAND_PAUSE_STATE_TYPE_UNTIL_NETCHANGE = 2,
} onDemandPauseStateType;


struct service {
 
	/* generic portion of the service */

    TAILQ_ENTRY(service) next;
	
    Boolean initialized; /* TRUE if successfully initialized */

    CFStringRef	serviceID;		/* service ID in the cache */
    CFStringRef	typeRef;		/* type string */
    CFStringRef	subtypeRef;		/* subtype string */
    CFStringRef authSubtypeRef; /* authentication subtype string */
    u_char		*sid;			/* C version of the servceID */
    // type/subtype/unit will make the reference number
    u_int16_t 	type;			/* type of link (PPP or IPSEC or VPN) */
    u_int16_t 	subtype;		/* subtype of link */
    u_int16_t 	unit;			/* ref number in the interfaces managed by this Controller */

    
    // status information frequently used
    u_int32_t	flags;			/* action flags */
    CFStringRef device;			/* transport device (en0, en1,...) */
	uid_t		uid;			/* uid of the user who started the connection */
	gid_t		gid;			/* gid of the user who started the connection */
	mach_port_t   bootstrap;	/* bootstrap of the user who started the connection */
	mach_port_t	au_session;	/* audit session of the user who started the connection */
	char		if_name[16];	/* virtual interface name (e.g ppp0, utun0, ...)  */
	int			if_index;		/* virtual index, as returned by if_nametoindex(ifname)  */

    CFDictionaryRef connectopts;		/* connect options in use */ 
    CFDictionaryRef systemprefs;		/* system prefs */ 
#if TARGET_OS_EMBEDDED
    CFStringRef profileIdentifier;		/* profile Identifier in the prefs */ 
	/* Cellular context */
	CTServerConnectionRef	cellularConnection;
	CFRunLoopTimerRef		cellular_timerref;
#endif
	
    CFUserNotificationRef userNotificationRef;	/* user notification */ 
    CFRunLoopSourceRef userNotificationRLS;		/* user notification rls */ 

#if !TARGET_OS_EMBEDDED
	vproc_transaction_t	vt;		/* opaque handle used to track outstanding transactions, used by instant off */
#endif
	u_int32_t 	connecttime;		/* time when connection occured */
    u_int32_t   establishtime;      /* time when connection established */
	u_int32_t	connectionslepttime;	/* amount of time connection slept for */
	u_int32_t	sleepwaketimeout;	/* disconnect if sleep-wake duration is longer than this */
	mdns_nat_mapping_t nat_mapping[MDNS_NAT_MAPPING_MAX];
	u_int32_t          nat_mapping_cnt;
	u_int32_t          was_running;
	u_int32_t          persist_connect;
	u_int32_t          persist_connect_status;
	u_int32_t          persist_connect_devstatus;
	u_int32_t          ondemand_paused;
	CFStringRef        ondemandAction;
	CFPropertyListRef  ondemandActionParameters;
	CFDictionaryRef	   ondemandDNSTriggeringDicts; /* Dynamic store service DNS dicts to set supplemental match domains when not connected */
	Boolean	           ondemandDNSTriggeringDictsArePublished;
	CFDictionaryRef	   ondemandProbeResults;
	CFDictionaryRef	   ondemandSavedDns;
	CFDictionaryRef    persist_connect_opts;
	CFStringRef        connection_nid;
	CFStringRef        connection_nap;
	Boolean            dnsRedirectDetected;
	CFDictionaryRef    dnsRedirectedAddresses;
	CFDictionaryRef	   routeCache;
#if !TARGET_OS_EMBEDDED
	void              *connection_nap_monitor;
#endif
	CFDictionaryRef    environmentVars;
	SCNetworkReachabilityRef	remote_address_reachability;
	SCNetworkReachabilityFlags	remote_address_reach_flags;
	int							remote_address_reach_ifindex;
    
	CFRunLoopTimerRef	ondemand_pause_timerref;				/* ondemand pause end timer to resume VOD */
	u_int32_t			ondemand_pause_type_on_timer_expire;	/* ondemand pause type to set when timer expires */

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
extern CFStringRef	gBundleDir;
extern CFURLRef 	gIconURLRef;
extern CFStringRef	gIconDir;
extern CFStringRef 	gPluginsDir;
extern CFURLRef		gPluginsURLRef;
extern CFStringRef 	gResourcesDir;

extern SCDynamicStoreRef	gDynamicStore;
extern CFStringRef			gLoggedInUser;
extern uid_t				gLoggedInUserUID;

extern int					gSleeping;
extern time_t				gSleptAt;
extern time_t				gWokeAt;
extern uint64_t				gWakeUpTime;
extern double				gSleepWakeTimeout;
extern double				gTimeScaleSeconds;
extern CFRunLoopSourceRef 	gStopRls;

extern char			*gIPSecAppVersion;

extern int					gSCNCVerbose;
extern int					gSCNCDebug;

extern CFStringRef          gOndemand_key;

#if TARGET_OS_EMBEDDED
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
void application_installed(CFDictionaryRef appInfo);
void application_removed(CFDictionaryRef appInfo);
#if TARGET_OS_EMBEDDED
int start_profile_janitor(struct service *serv);
int bringup_cellular(struct service *serv);
#endif
int check_interface_captive_and_not_ready(SCDynamicStoreRef dynamicStoreRef, char *interface_name);


void user_notification_callback(CFUserNotificationRef userNotification, CFOptionFlags responseFlags);

void scnc_bootstrap_dealloc(struct service *serv);
void scnc_bootstrap_retain(struct service *serv, mach_port_t bootstrap);
void scnc_ausession_dealloc(struct service *serv);
void scnc_ausession_retain(struct service *serv, mach_port_t au_session);

int scnc_stop(struct service *serv, void *client, int signal, int scnc_reason);
int scnc_start(struct service *serv, CFDictionaryRef options, void *client, int autoclose, uid_t uid, gid_t gid, int pid, mach_port_t bootstrap, mach_port_t au_session);
int scnc_getstatus(struct service *serv);
int scnc_copyextendedstatus(struct service *serv, void **reply, u_int16_t *replylen);
int scnc_copystatistics(struct service *serv, void **reply, u_int16_t *replylen);
int scnc_getconnectdata(struct service *serv, void **reply, u_int16_t *replylen, int all);
int scnc_getconnectsystemdata(struct service *serv, void **reply, u_int16_t *replylen);
int scnc_suspend(struct service *serv);
int scnc_resume(struct service *serv);
int scnc_sendmsg(struct service *serv, uint32_t msg_type, CFDataRef cfdata, uid_t uid, gid_t gid, int pid, mach_port_t bootstrap, mach_port_t au_session);
struct service *findbyserviceID(CFStringRef serviceID);
struct service *findbypid(pid_t pid);
struct service *findbysid(u_char *data, int len);
struct service *findbyref(u_int16_t type, u_int32_t ref);
u_int32_t makeref(struct service *serv);

int scnc_disconnectifoverslept(const char *function, struct service *serv, char *if_name);
void nat_port_mapping_set(struct service *serv);
void nat_port_mapping_clear(struct service *serv);
void initVPNConnectionLocation(struct service *serv);
void clearVPNLocation(struct service *serv);
Boolean disconnectIfVPNLocationChanged(struct service *serv);
Boolean didVPNLocationChange (struct service *serv);
void check_network_refresh(void);
void ondemand_set_pause(struct service *serv, uint32_t pauseflag, Boolean update_store);
Boolean set_ondemand_pause_timer(struct service *serv, uint32_t timeout, uint32_t pause_type, uint32_t pause_type_on_expire);
void clear_ondemand_pause_timer(struct service *serv);
void ondemand_clear_pause_all(onDemandPauseStateType type_to_clear);
Boolean ondemand_unpublish_dns_triggering_dicts (struct service *serv);
int ondemand_add_service(struct service *serv, Boolean update_configuration);

#define DISCONNECT_VPN_IFOVERSLEPT(f,s,i) scnc_disconnectifoverslept(f,s,i)

#if TARGET_OS_EMBEDDED

#define SET_VPN_PORTMAPPING(s)

#define CLEAR_VPN_PORTMAPPING(s)

#define TRACK_VPN_LOCATION(s)

#define STOP_TRACKING_VPN_LOCATION(s)

#define DISCONNECT_VPN_IFLOCATIONCHANGED(s) 0

#define DID_VPN_LOCATIONCHANGE(s) 0

#else

#define SET_VPN_PORTMAPPING(s) nat_port_mapping_set(s)

#define CLEAR_VPN_PORTMAPPING(s) nat_port_mapping_clear(s)

#define TRACK_VPN_LOCATION(s) initVPNConnectionLocation(s)

#define STOP_TRACKING_VPN_LOCATION(s) clearVPNLocation(s)

#define DISCONNECT_VPN_IFLOCATIONCHANGED(s) disconnectIfVPNLocationChanged(s)

#define DID_VPN_LOCATIONCHANGE(s) didVPNLocationChange(s)

#endif

#endif
