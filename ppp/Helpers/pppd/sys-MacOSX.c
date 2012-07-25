/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
 * sys-bsd.c - System-dependent procedures for setting up
 * PPP interfaces on bsd-4.4-ish systems (including 386BSD, NetBSD, etc.)
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * Copyright (c) 1995 The Australian National University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University and The Australian National University.
 * The names of the Universities may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#define RCSID	"$Id: sys-MacOSX.c,v 1.40 2006/02/16 01:47:04 lindak Exp $"

/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <util.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/ucred.h>
#ifdef PPP_FILTER
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_types.h>
#include <mach-o/dyld.h>
#include <dirent.h>
#include <NSSystemDirectories.h>
#include <mach/mach_time.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <CoreFoundation/CFBundle.h>
#include <ppp_defs.h>
#include <ppp_domain.h>
#include <ppp_msg.h>
#include <ppp_privmsg.h>
#include <if_ppp.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>
#include <netinet/if_ether.h>
#include <syslog.h>
#include <sys/un.h>
#include <pthread.h>
#include <notify.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <IOKit/network/IOEthernetController.h>
#include <servers/bootstrap.h>
#include <arpa/inet.h>
#include <bsm/libbsm.h>
#include <ifaddrs.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#include "pppcontroller.h"
#include <ppp/pppcontroller_types.h>

#include "../vpnd/RASSchemaDefinitions.h"


#include "pppd.h"
#include "fsm.h"
#include "ipcp.h"
#include "lcp.h"
#include "eap.h"
#include "../vpnd/RASSchemaDefinitions.h"

/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

#define IP_FORMAT	"%d.%d.%d.%d"
#define IP_CH(ip)	((u_char *)(ip))
#define IP_LIST(ip)	IP_CH(ip)[0],IP_CH(ip)[1],IP_CH(ip)[2],IP_CH(ip)[3]

#define PPP_NKE_PATH 	"/System/Library/Extensions/PPP.kext"
#define PPP_NKE_ID		"com.apple.nke.ppp"

/* We can get an EIO error on an ioctl if the modem has hung up */
#define ok_error(num) ((num)==EIO)

#ifndef N_SYNC_PPP
#define N_SYNC_PPP 14
#endif

/*
 * If PPP_DRV_NAME is not defined, use the default "ppp" as the device name.
 */
#if !defined(PPP_DRV_NAME)
#define PPP_DRV_NAME	"ppp"
#endif /* !defined(PPP_DRV_NAME) */

/* -----------------------------------------------------------------------------
 Forward declarations
----------------------------------------------------------------------------- */

static int get_flags (int fd);
static void set_flags (int fd, int flags);
static int set_kdebugflag(int level);
static int make_ppp_unit(void);
/* Prototypes for procedures local to this file. */
static int get_ether_addr __P((u_int32_t, struct sockaddr_dl *));
static int connect_pfppp();
//static void sys_pidchange(void *arg, int pid);
static void sys_phasechange(void *arg, uintptr_t phase);
static void sys_exitnotify(void *arg, uintptr_t exitcode);
int publish_keyentry(CFStringRef key, CFStringRef entry, CFTypeRef value);
int publish_dictnumentry(CFStringRef dict, CFStringRef entry, int val);
int publish_dictstrentry(CFStringRef dict, CFStringRef entry, char *str, int encoding);
int unpublish_keyentry(CFStringRef key, CFStringRef entry);
int unpublish_dict(CFStringRef dict);
int publish_dns_wins_entry(CFStringRef entity, CFStringRef property1, CFTypeRef ref1, CFTypeRef ref1a, 
						CFStringRef property2, CFTypeRef ref2,
						CFStringRef property3, CFTypeRef ref3, int clean);
int unpublish_dictentry(CFStringRef dict, CFStringRef entry);
static void sys_eventnotify(void *param, uintptr_t code);
static void sys_timeremaining(void *param, uintptr_t info);
static void sys_authpeersuccessnotify(void *param, uintptr_t info);
static int update_stateaddr(u_int32_t o, u_int32_t h, u_int32_t m);
int publish_stateaddr(u_int32_t o, u_int32_t h, u_int32_t m);
int route_interface(int cmd, struct in_addr host, struct in_addr mask, char iftype, char *ifname, int is_host);
int route_gateway(int cmd, struct in_addr dest, struct in_addr mask, struct in_addr gateway, int use_gway_flag);
static void ppp_ip_probe_timeout (void *arg);
static void republish_dict();

/* -----------------------------------------------------------------------------
 Globals
----------------------------------------------------------------------------- */
#ifndef lint
static const char rcsid[] = RCSID;
#endif

static int 		ttydisc = TTYDISC;	/* The default tty discipline */
static int 		pppdisc = PPPDISC;	/* The PPP sync or async discipline */

static int 		initfdflags = -1;	/* Initial file descriptor flags for ppp_fd */
static int 		ppp_fd = -1;		/* fd which is set to PPP discipline */
static int		rtm_seq;

static int 		restore_term;		/* 1 => we've munged the terminal */
static struct termios 	inittermios; 		/* Initial TTY termios */
static struct winsize 	wsinfo;			/* Initial window size info */

static int 		ip_sockfd;		/* socket for doing interface ioctls */

static fd_set 		in_fds;			/* set of fds that wait_input waits for */
static fd_set 		ready_fds;		/* set of fds currently ready (out of select) */
static int 		max_in_fd;		/* highest fd set in in_fds */

static int 		if_is_up;		/* the interface is currently up */
static int		ipv4_plumbed = 0; 	/* is ipv4 plumbed on the interface ? */
static u_int32_t 	ifaddrs[2];		/* local and remote addresses we set */
static u_int32_t 	default_route_gateway;	/* gateway addr for default route */
static u_int32_t 	proxy_arp_addr;		/* remote addr for proxy arp */
SCDynamicStoreRef	cfgCache = 0;		/* configd session */
CFRunLoopSourceRef	rls = 0;		/* runloop source */
CFStringRef		serviceidRef = 0;	/* service id ref */
CFStringRef		serveridRef = 0;	/* server id ref */

extern u_char		inpacket_buf[];		/* borrowed from main.c */

static u_int32_t 	connecttime;		/* time when connection occured */
int			looped;			/* 1 if using loop */
int	 		ppp_sockfd = -1;	/* fd for PF_PPP socket */
char 			*serviceid = NULL; 	/* configuration service ID to publish */
char 			*serverid = NULL; 	/* server ID that spwaned this service */
static CFStringRef	server_peer = NULL;	/* remote peer address for server */
static char 	*network_signature = NULL; 	/* network signature */
bool	 		noload = 0;		/* don't load the kernel extension */
bool                    looplocal = 0;  /* Don't loop local traffic destined to the local address some applications rely on this default behavior */
bool            addifroute = 0;  /* install route for the netmask of the interface */

static struct in_addr		ifroute_address;
static struct in_addr		ifroute_mask;
static int		ifroute_installed = 0;

double	 		timeScaleSeconds;	/* scale factor for machine absolute time to seconds */
double	 		timeScaleMicroSeconds;	/* scale factor for machine absolute time to microseconds */

CFPropertyListRef 		userOptions		= NULL;
CFPropertyListRef 		systemOptions		= NULL;

CFMutableDictionaryRef	publish_dict = NULL;

option_t sys_options[] = {
    { "serviceid", o_string, &serviceid,
      "Service ID to publish"},
    { "serverid", o_string, &serverid,
      "Server ID that spawned this service."},
    { "nopppload", o_bool, &noload,
      "Don't try to load PPP NKE", 1},
    { "looplocal", o_bool, &looplocal,
      "Loop local traffic destined to the local address", 1},
    { "noifroute", o_bool, &addifroute,
      "Don't install route for the interface", 0},
    { "addifroute", o_bool, &addifroute,
      "Install route for the interface", 1},
    { "nolooplocal", o_bool, &looplocal,
      "Don't loop local traffic destined to the local address", 0},
    { NULL }
};

ppp_session_t *session = NULL;
static int ppp_auxiliary_probe_echos_pending = 0;
static int ppp_auxiliary_probe_success = 0;
static int ppp_auxiliary_probe_ip_notify_init = 0;
static int ppp_auxiliary_probe_ip = 0;

static int override_primary = 0;
static int wait_port_mapping_changed = 0;

extern int kill_link;

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void closeall()
{
    int i;

    for (i = getdtablesize() - 1; i >= 0; i--) close(i);
    open("/dev/null", O_RDWR, 0);
    dup(0);
    dup(0);
    return;
}

/* -----------------------------------------------------------------------------
close all file descriptor above 'from'
----------------------------------------------------------------------------- */
void closeallfrom(int from)
{
	int fd;
	struct dirent entry, *entryp;
	
	DIR *dirp = opendir("/dev/fd");
	if (dirp == NULL) {
		/* perhaps fall back on getdtablesize method */ ;
		for (fd = from; fd < getdtablesize(); ++fd) close(fd);
		return;
	}
	
	while (readdir_r(dirp, &entry, &entryp) == 0 && entryp != NULL) {
		fd = atoi(entryp->d_name);
		if (fd >= from && fd != dirp->__dd_fd)
			close(fd);
	}
	closedir(dirp);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long load_kext(char *kext, int byBundleID)
{
    int pid;

    if ((pid = fork()) < 0)
        return 1;

    if (pid == 0) {
        closeall();
        // PPP kernel extension not loaded, try load it...
		if (byBundleID)
			execle("/sbin/kextload", "kextload", "-b", kext, (char *)0, (char *)0);
		else
			execle("/sbin/kextload", "kextload", kext, (char *)0, (char *)0);
        exit(1);
    }

    while (waitpid(pid, 0, 0) < 0) {
        if (errno == EINTR)
            continue;
       return 1;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
preinitialize options, called before sysinit
----------------------------------------------------------------------------- */
void sys_install_options()
{
    add_options(sys_options);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int sys_check_controller()
{
	mach_port_t			server;
	kern_return_t		status;
	int					result;
	audit_token_t		audit_token;
	uid_t               euid;

	status = bootstrap_look_up(bootstrap_port, PPPCONTROLLER_SERVER, &server);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			return 0;
		default :
			return 0;
	}
	
	status = pppcontroller_iscontrolled(server, &result, &audit_token);

	if (status == KERN_SUCCESS) {
		audit_token_to_au32(audit_token,
					NULL,			// auidp
					&euid,			// euid
					NULL,			// egid
					NULL,			// ruid
					NULL,			// rgid
					NULL,			// pid
					NULL,			// asid
					NULL);			// tid

		return ((result == kSCStatusOK) && (euid == 0));
	}

	return 0;
}

/* -------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------- */
CFPropertyListRef Unserialize(void *data, u_int32_t dataLen)
{
    CFDataRef           	xml;
    CFPropertyListRef	ref = 0;

    xml = CFDataCreate(NULL, data, dataLen);
    if (xml) {
        ref = CFPropertyListCreateFromXMLData(NULL,
                xml,  kCFPropertyListImmutable, NULL);
        CFRelease(xml);
    }

    return ref;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void CopyControllerData()
{
	mach_port_t			server;
	kern_return_t		status;
	void				*data			= NULL;
	unsigned int		datalen;
	int				result			= kSCStatusFailed;
	audit_token_t		audit_token;
	uid_t               euid;

	status = bootstrap_look_up(bootstrap_port, PPPCONTROLLER_SERVER, &server);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			return ;
		default :
			return;
	}

	status = pppcontroller_copyprivoptions(server, 0, (xmlDataOut_t *)&data, &datalen, &result, &audit_token);

	if (status != KERN_SUCCESS
		|| result != kSCStatusOK) {
		error("cannot get private system options from controller\n");
		return;
	}
	audit_token_to_au32(audit_token,
				NULL,			// auidp
				&euid,			// euid
				NULL,			// egid
				NULL,			// ruid
				NULL,			// rgid
				NULL,			// pid
				NULL,			// asid
				NULL);			// tid
	if (euid != 0) {
		error("cannot authenticate private system options from controller\n");
		return;
	}

	systemOptions = Unserialize(data, datalen);

	status = pppcontroller_copyprivoptions(server, 1, (xmlDataOut_t *)&data, &datalen, &result, &audit_token);

	if (status != KERN_SUCCESS
		|| result != kSCStatusOK) {
		error("cannot get private user options from controller\n");
		return;
	}
	audit_token_to_au32(audit_token,
				NULL,			// auidp
				&euid,			// euid
				NULL,			// egid
				NULL,			// ruid
				NULL,			// rgid
				NULL,			// pid
				NULL,			// asid
				NULL);			// tid
	if (euid != 0) {
		error("cannot authenticate private user options from controller\n");
		return;
	}

	userOptions = Unserialize(data, datalen);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void CopyServerData()
{
    SCPreferencesRef 		prefs = 0;
    CFPropertyListRef		servers_list;
   
    // open the prefs file
    prefs = SCPreferencesCreate(0, CFSTR("pppd"), kRASServerPrefsFileName);
    if (prefs == NULL) {
        fatal("Cannot open servers plist\n");
		return; 
	}
	
    // get servers list from the plist
    servers_list = SCPreferencesGetValue(prefs, kRASServers);
    if (servers_list == NULL) {
        fatal("No servers found in servers plist\n");
        CFRelease(prefs);
		return; 
    }

    systemOptions = CFDictionaryGetValue(servers_list, serveridRef);
    if (!systemOptions || CFGetTypeID(systemOptions) != CFDictionaryGetTypeID()) {
        fatal("Server ID '%s' not found in servers plist\n", serverid);
		systemOptions = 0;
        CFRelease(prefs);
		return; 
    }
	
	CFRetain(systemOptions);
    CFRelease(prefs);
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
static int setstorevalue(CFStringRef key, CFMutableDictionaryRef dict)
{
	int result = 1;
	
	if (SCDynamicStoreSetValue(cfgCache, key, dict) == 0)
		result = 0;
	
	if (publish_dict){
		CFDictionarySetValue(publish_dict, key, dict);
	}
	return result;
}

/* -----------------------------------------------------------------------------
System-dependent initialization
----------------------------------------------------------------------------- */
void sys_init()
{
    int 		flags;
    mach_timebase_info_data_t   timebaseInfo;

    openlog("pppd", LOG_PID | LOG_NDELAY, LOG_PPP);
    setlogmask(LOG_UPTO(LOG_INFO));
    if (debug)
	setlogmask(LOG_UPTO(LOG_DEBUG));

    // establish pppd as session leader
    // if started via terminal, setsid will fail, which doesn't matter
    // if started via configd, setsid will succeed and will allow reception of SIGHUP
    setsid();
    
    // open a socket to the PF_PPP protocol
    ppp_sockfd = connect_pfppp();
    if (ppp_sockfd < 0)
        fatal("Couldn't open PF_PPP: %m");
                
    flags = fcntl(ppp_sockfd, F_GETFL);
    if (flags == -1
        || fcntl(ppp_sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
        warning("Couldn't set PF_PPP to nonblock: %m");

    // Get an internet socket for doing socket ioctls
    ip_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (ip_sockfd < 0)
	fatal("Couldn't create IP socket: %m(%d)", errno);

    // serviceid is required if we want pppd to publish information into the cache
    if (!serviceid) {
        CFUUIDRef       uuid;
        CFStringRef	strref;
        char 		str[100];
        
        uuid = CFUUIDCreate(NULL);
        strref = CFUUIDCreateString(NULL, uuid);
        CFStringGetCString(strref, str, sizeof(str), kCFStringEncodingUTF8);

        if ((serviceid = strdup(str)) == NULL)
            fatal("Couldn't allocate memory to create temporary service id: %m(%d)", errno);

        CFRelease(strref);
        CFRelease(uuid);
    }        

    serviceidRef = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), serviceid);
    if (!serviceidRef) 
        fatal("Couldn't allocate memory to create service id reference: %m(%d)", errno);
            
	/*	if started as a client by PPPController 
		copy user and system options from the controller */
    if (controlled) {
		CopyControllerData();
	}

    //sys_pidchange(0, getpid());
    cfgCache = SCDynamicStoreCreate(0, CFSTR("pppd"), 0, 0);
    if (cfgCache == 0)
        fatal("SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
    // if we are going to detach, wait to publish pid
    if (nodetach)
        publish_dictnumentry(kSCEntNetPPP, CFSTR("pid"), getpid());

    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPStatus, phase);
    if (serverid) {
		serveridRef = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), serverid);
		if (!serveridRef) 
			fatal("Couldn't allocate memory to create server id reference: %m(%d)", errno);
		/* copy system options from the server plist */
		CopyServerData();
    	publish_dictstrentry(kSCEntNetInterface, CFSTR("ServerID"), serverid, kCFStringEncodingMacRoman);
	}
	
	if (!controlled){
		
		publish_dict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (publish_dict){
			rls = SCDynamicStoreCreateRunLoopSource(kCFAllocatorDefault, cfgCache, 0);
			if ( rls == NULL ){
				notice("SCDynamicStoreCreateRunLoopSource FAILED %s", SCErrorString(SCError()));
			}else {
				CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
			}
			SCDynamicStoreSetDisconnectCallBack(cfgCache, republish_dict);
		}
	}
	
	
    //add_notifier(&pidchange, sys_pidchange, 0);
    add_notifier(&phasechange, sys_phasechange, 0);
    add_notifier(&exitnotify, sys_exitnotify, 0);
    
    // only send event if started when we are started by the PPPController 
    if (statusfd != -1) {

        add_notifier(&ip_up_notify, sys_eventnotify, (void*)PPP_EVT_IPCP_UP);
        add_notifier(&ip_down_notify, sys_eventnotify, (void*)PPP_EVT_IPCP_DOWN);
        add_notifier(&lcp_up_notify, sys_eventnotify, (void*)PPP_EVT_LCP_UP);
        add_notifier(&lcp_down_notify, sys_eventnotify, (void*)PPP_EVT_LCP_DOWN);
        add_notifier(&lcp_lowerup_notify, sys_eventnotify, (void*)PPP_EVT_LOWERLAYER_UP);
        add_notifier(&lcp_lowerdown_notify, sys_eventnotify, (void*)PPP_EVT_LOWERLAYER_DOWN);
        add_notifier(&auth_start_notify, sys_eventnotify, (void*)PPP_EVT_AUTH_STARTED);
        add_notifier(&auth_withpeer_fail_notify, sys_eventnotify, (void*)PPP_EVT_AUTH_FAILED);
        add_notifier(&auth_withpeer_success_notify, sys_eventnotify, (void*)PPP_EVT_AUTH_SUCCEDED);
        add_notifier(&connectscript_started_notify, sys_eventnotify, (void*)PPP_EVT_CONNSCRIPT_STARTED);
        add_notifier(&connectscript_finished_notify, sys_eventnotify, (void*)PPP_EVT_CONNSCRIPT_FINISHED);
        add_notifier(&terminalscript_started_notify, sys_eventnotify, (void*)PPP_EVT_TERMSCRIPT_STARTED);
        add_notifier(&terminalscript_finished_notify, sys_eventnotify, (void*)PPP_EVT_TERMSCRIPT_FINISHED);
        add_notifier(&connect_started_notify, sys_eventnotify, (void*)PPP_EVT_CONN_STARTED);
        add_notifier(&connect_success_notify, sys_eventnotify, (void*)PPP_EVT_CONN_SUCCEDED);
        add_notifier(&connect_fail_notify, sys_eventnotify, (void*)PPP_EVT_CONN_FAILED);
        add_notifier(&disconnect_started_notify, sys_eventnotify, (void*)PPP_EVT_DISC_STARTED);
        add_notifier(&disconnect_done_notify, sys_eventnotify, (void*)PPP_EVT_DISC_FINISHED);
        add_notifier(&stop_notify, sys_eventnotify, (void*)PPP_EVT_STOPPED);
        add_notifier(&cont_notify, sys_eventnotify, (void*)PPP_EVT_CONTINUED);
    }

    add_notifier(&lcp_timeremaining_notify, sys_timeremaining, 0);
    add_notifier(&auth_peer_success_notify, sys_authpeersuccessnotify, 0);

    if (mach_timebase_info(&timebaseInfo) == KERN_SUCCESS) {	// returns scale factor for ns
        timeScaleMicroSeconds = ((double) timebaseInfo.numer / (double) timebaseInfo.denom) / 1000;
        timeScaleSeconds = timeScaleMicroSeconds / 1000000;
    }

    FD_ZERO(&in_fds);
    FD_ZERO(&ready_fds);
    max_in_fd = 0;
}

/* ----------------------------------------------------------------------------- 
 sys_cleanup - restore any system state we modified before exiting:
 mark the interface down, delete default route and/or proxy arp entry.
 This should call die() because it's called from die()
----------------------------------------------------------------------------- */
void sys_cleanup()
{
    struct ifreq ifr;

	if (!controlled && rls) {
		CFRunLoopRemoveSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
		CFRunLoopSourceInvalidate(rls);
		CFRelease(rls);
		rls = NULL;
	}
	
	cifroute();

    if (if_is_up) {
	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(ip_sockfd, SIOCGIFFLAGS, &ifr) >= 0
	    && ((ifr.ifr_flags & IFF_UP) != 0)) {
	    ifr.ifr_flags &= ~IFF_UP;
	    ioctl(ip_sockfd, SIOCSIFFLAGS, &ifr);
	}
    }

    if (ifaddrs[0] != 0)
	cifaddr(0, ifaddrs[0], ifaddrs[1]);

    if (default_route_gateway)
	cifdefaultroute(0, 0, default_route_gateway);
    if (proxy_arp_addr)
	cifproxyarp(0, proxy_arp_addr);
}

/* ----------------------------------------------------------------------------- 
 sys_runloop - called from main loop
----------------------------------------------------------------------------- */
void sys_runloop()
{
	if (!controlled && rls) {
		if (kill_link)
			CFRunLoopStop(CFRunLoopGetCurrent());
		else
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
	}
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
void set_network_signature(char *key1, char *val1, char *key2, char *val2)
{
	int len = 0;
	
	if (network_signature) {
		free(network_signature);
		network_signature = 0;
	}
	
	if (key1) 
		len += strlen(key1) + strlen(val1) + 1; 
	if (key2) {
		if (key1) len++; // add separator ";"
		len += strlen(key2) + strlen(val2) + 1; 
	}
	
	 if (len) {
		network_signature = malloc(len + 1);
		if (!network_signature) {
			warning("no memory to create network signature");
			return;
		}
		network_signature[0] = 0;
        if (key1) {
			strlcat(network_signature, key1, len + 1);
			strlcat(network_signature, "=", len + 1);
			strlcat(network_signature, val1, len + 1);
		}
        if (key2) {
			if (key1) strlcat(network_signature, ";", len + 1);
			strlcat(network_signature, key2, len + 1);
			strlcat(network_signature, "=", len + 1);
			strlcat(network_signature, val2, len + 1);
		}
	 }
}


/* ----------------------------------------------------------------------------- 
 ----------------------------------------------------------------------------- */
void set_server_peer(struct in_addr peer)
{
	if (server_peer) {
		CFRelease(server_peer);
		server_peer = NULL;
	}
	
	server_peer = CFStringCreateWithCString(NULL, inet_ntoa(peer), kCFStringEncodingASCII);
}


/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
void sys_close()
{
    if (ip_sockfd != -1) {
	close(ip_sockfd);
	ip_sockfd = -1;
    }
    if (ppp_sockfd != -1) {
        close(ppp_sockfd);
        ppp_sockfd = -1;
    }
}

/* ----------------------------------------------------------------------------- 
 Functions to read and set the flags value in the device driver
----------------------------------------------------------------------------- */
static int get_flags (int fd)
{    
    int flags;

    if (ioctl(fd, PPPIOCGFLAGS, (caddr_t) &flags) < 0) {
	if ( ok_error (errno) )
	    flags = 0;
	else
	    fatal("ioctl(PPPIOCGFLAGS): %m");
    }

    SYSDEBUG ((LOG_DEBUG, "get flags = %x\n", flags));
    return flags;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
static void set_flags (int fd, int flags)
{    
    SYSDEBUG ((LOG_DEBUG, "set flags = %x\n", flags));

    if (ioctl(fd, PPPIOCSFLAGS, (caddr_t) &flags) < 0) {
	if (! ok_error (errno) )
	    fatal("ioctl(PPPIOCSFLAGS, %x): %m", flags, errno);
    }
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
void sys_notify(u_int32_t message, uintptr_t code1, uintptr_t code2)
{
    struct ppp_msg_hdr	*hdr;
    int 		servlen, totlen;
    u_char 		*p, *msg;

    if (statusfd == -1)
        return;
        
    servlen = strlen(serviceid);
    totlen = sizeof(struct ppp_msg_hdr) + servlen + 8;

    msg = malloc(totlen);
    if (!msg) {
	warning("no memory to send event to PPPController");
        return;
    }
    
    p  = msg;
    bzero(p, totlen);
    hdr = ALIGNED_CAST(struct ppp_msg_hdr *)p;
    hdr->m_type = message;
    hdr->m_len = 8;
    hdr->m_flags |= USE_SERVICEID;
    hdr->m_link = servlen;
    
    p += sizeof(struct ppp_msg_hdr);
    bcopy(serviceid, p, servlen);
    p += servlen;
    bcopy((u_char*)&code1, p, 4);
    p += 4;
    bcopy((u_char*)&code2, p, 4);

    if (write(statusfd, msg, totlen) != totlen) {
	warning("can't talk to PPPController : %m");
    }
    free(msg);
}

/* ----------------------------------------------------------------------------- 
we installed the notifier with the event as the parameter
----------------------------------------------------------------------------- */
void sys_eventnotify(void *param, uintptr_t code)
{
    
    if (param == (void*)PPP_EVT_CONN_FAILED) 
        code = EXIT_CONNECT_FAILED;

    sys_notify(PPPD_EVENT, (uintptr_t)param, code);
}

/* ----------------------------------------------------------------------------- 
send status notification to the controller
----------------------------------------------------------------------------- */
void sys_statusnotify()
{

    sys_notify(PPPD_STATUS, status, devstatus);
}


/* ----------------------------------------------------------------------------- 
check the options that the user specified
----------------------------------------------------------------------------- */
int sys_check_options()
{
#ifndef CDTRCTS
    if (crtscts == 2) {
	warning("DTR/CTS flow control is not supported on this system");
	return 0;
    }
#endif
    return 1;
}

/* ----------------------------------------------------------------------------- 
check if the kernel supports PPP
----------------------------------------------------------------------------- */
int ppp_available()
{
    int 	s;
    extern char *no_ppp_msg;
    
    no_ppp_msg = "\
Mac OS X lacks kernel support for PPP.  \n\
To include PPP support in the kernel, please follow \n\
the steps detailed in the README.MacOSX file.\n";

    // open to socket to the PF_PPP family
    // if that works, the kernel extension is loaded.
    if ((s = socket(PF_PPP, SOCK_RAW, PPPPROTO_CTL)) < 0) {
    
#if !TARGET_OS_EMBEDDED
        if (!noload && !load_kext(PPP_NKE_PATH, 0))
#else
        if (!noload && !load_kext(PPP_NKE_ID, 1))
#endif
            s = socket(PF_PPP, SOCK_RAW, PPPPROTO_CTL);
            
        if (s < 0)
            return 0;
    }
    
    // could be smarter and get the version of the ppp family, 
    // using get option or ioctl

    close(s);

    return 1;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
static int still_ppp(void)
{

    return !hungup && ppp_fd >= 0;
}

/* ----------------------------------------------------------------------------- 
Define the debugging level for the kernel
----------------------------------------------------------------------------- */
static int set_kdebugflag (int requested_level)
{
   if (ifunit < 0)
	return 1;
    if (ioctl(ppp_sockfd, PPPIOCSDEBUG, &requested_level) < 0) {
	if ( ! ok_error (errno) )
	    error("ioctl(PPPIOCSDEBUG): %m");
	return (0);
    }
    SYSDEBUG ((LOG_INFO, "set kernel debugging level to %d",
		requested_level));
    return (1);
}

/* ----------------------------------------------------------------------------- 
make a new ppp unit for ppp_sockfd
----------------------------------------------------------------------------- */
static int make_ppp_unit()
{
    int x;
    char name[32];

    ifunit = req_unit;
    x = ioctl(ppp_sockfd, PPPIOCNEWUNIT, &ifunit);
    if (x < 0 && req_unit >= 0 && errno == EEXIST) {
        warning("Couldn't allocate PPP unit %d as it is already in use");
        ifunit = -1;
        x = ioctl(ppp_sockfd, PPPIOCNEWUNIT, &ifunit);


    }
    if (x < 0)
        error("Couldn't create new ppp unit: %m");
    else {
        slprintf(name, sizeof(name), "%s%d", PPP_DRV_NAME, ifunit);
        publish_dictstrentry(kSCEntNetPPP, kSCPropInterfaceName, name, kCFStringEncodingMacRoman);
    }

    return x;
}

/* ----------------------------------------------------------------------------- 
coen a socket to the PF_PPP protocol
----------------------------------------------------------------------------- */
int connect_pfppp()
{
    int 			fd = -1;
    struct sockaddr_ppp 	pppaddr;
    
    // open a PF_PPP socket
    fd = socket(PF_PPP, SOCK_RAW, PPPPROTO_CTL);
    if (fd < 0) {
        error("Couldn't open PF_PPP: %m");
        return -1;
    }
    // need to connect to the PPP protocol
    pppaddr.ppp_len = sizeof(struct sockaddr_ppp);
    pppaddr.ppp_family = AF_PPP;
    pppaddr.ppp_proto = PPPPROTO_CTL;
    pppaddr.ppp_cookie = 0;
    if (connect(fd, (struct sockaddr *)&pppaddr, sizeof(struct sockaddr_ppp)) < 0) {
        error("Couldn't connect to PF_PPP: %m");
        close(fd);
        return -1;
    }
    return fd;
}


/* ----------------------------------------------------------------------------- 
Turn the serial port into a ppp interface
----------------------------------------------------------------------------- */
int tty_establish_ppp (int tty_fd)
{
    int new_fd;
    
    // First, turn the device into ppp link

    /* Ensure that the tty device is in exclusive mode.  */
    if (ioctl(tty_fd, TIOCEXCL, 0) < 0) {
        if ( ! ok_error ( errno ))
	  ;//warning("Couldn't make tty exclusive: %m");
    }
    
    // Set the current tty to the PPP discpline
    pppdisc = sync_serial ? N_SYNC_PPP: PPPDISC;
    if (ioctl(tty_fd, TIOCSETD, &pppdisc) < 0) {
        if ( ! ok_error (errno) ) {
            error("Couldn't set tty to PPP discipline: %m");
            return -1;
        }
    }

    // Then, then do the generic link work, and get a generic fd back

    new_fd = generic_establish_ppp(tty_fd);
    if (new_fd == -1) {
        // Restore the previous line discipline
        if (ioctl(tty_fd, TIOCSETD, &ttydisc) < 0)
            if ( ! ok_error (errno))
                error("ioctl(TIOCSETD, TTYDISC): %m");
        return -1;
    }

    set_flags(new_fd, get_flags(ppp_fd) & ~(SC_RCV_B7_0 | SC_RCV_B7_1 | SC_RCV_EVNP | SC_RCV_ODDP));

    return new_fd;
}

/* -----------------------------------------------------------------------------
Restore the serial port to normal operation.
This shouldn't call die() because it's called from die()
----------------------------------------------------------------------------- */
void tty_disestablish_ppp(int tty_fd)
{
        
    if (!hungup) {
        
        // Flush the tty output buffer so that the TIOCSETD doesn't hang.
        //if (tcflush(tty_fd, TCIOFLUSH) < 0)
        //    warning("tcflush failed: %m");
        
        // Restore the previous line discipline
        if (ioctl(tty_fd, TIOCSETD, &ttydisc) < 0) {
            if ( ! ok_error (errno))
                error("ioctl(TIOCSETD, TTYDISC): %m");
        }
        
        if (ioctl(tty_fd, TIOCNXCL, 0) < 0) {
            if ( ! ok_error (errno))
                warning("ioctl(TIOCNXCL): %m(%d)", errno);
        }

	// Reset non-blocking mode on fd
	if (initfdflags != -1 && fcntl(tty_fd, F_SETFL, initfdflags) < 0) {
	    if ( ! ok_error (errno))
		warning("Couldn't restore device fd flags: %m");
	}
    }

    initfdflags = -1;

    generic_disestablish_ppp(tty_fd);
}

/* ----------------------------------------------------------------------------- 
generic code to establish ppp interface
----------------------------------------------------------------------------- */
int generic_establish_ppp (int fd)
{
    int flags, s = -1, link = 0;
            
    // Open another instance of the ppp socket and connect the link to it
    if (ioctl(fd, PPPIOCGCHAN, &link) == -1) {
        error("Couldn't get link number: %m");
        goto err;
    }

    dbglog("using link %d", link);
        
    // open a socket to the PPP protocol
    s = connect_pfppp();
    if (s < 0) {
        error("Couldn't reopen PF_PPP: %m");
        goto err;
    }
    
    if (ioctl(s, PPPIOCATTCHAN, &link) < 0) {
        error("Couldn't attach to the ppp link %d: %m", link);
        goto err_close;
    }

    flags = fcntl(s, F_GETFL);
    if (flags == -1 || fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1)
        warning("Couldn't set ppp socket link to nonblock: %m");

    /* set the ppp_fd socket now */
    ppp_fd = s;

    if (!looped)
        ifunit = -1;
    if (!looped && !multilink) {
        // Create a new PPP unit.
        if (make_ppp_unit() < 0)
            goto err_close;
    }

    if (looped) {
        set_flags(ppp_sockfd, get_flags(ppp_sockfd) & ~SC_LOOP_TRAFFIC);
        looped = 0;
    }

    if (!multilink) {
        add_fd(ppp_sockfd);
        if (ioctl(s, PPPIOCCONNECT, &ifunit) < 0) {
            error("Couldn't attach to PPP unit %d: %m", ifunit);
            goto err_close;
        }
    }

    // Enable debug in the driver if requested.
    set_kdebugflag (kdebugflag);

    return ppp_fd;

 err_close:
    close(s);
    
 err:
    return -1;
}

/* -----------------------------------------------------------------------------
Restore the serial port to normal operation.
This shouldn't call die() because it's called from die()
fd is the file descriptor of the device
----------------------------------------------------------------------------- */
void generic_disestablish_ppp(int fd)
{
    int 	x;

    close(ppp_fd);
    ppp_fd = -1;
    if (demand) {
	looped = 1;
        set_flags(ppp_sockfd, get_flags(ppp_sockfd) | SC_LOOP_TRAFFIC);
    }
    else {
        unpublish_dictentry(kSCEntNetPPP, kSCPropInterfaceName);
        if (ifunit >= 0 && ioctl(ppp_sockfd, PPPIOCDETACH, &x) < 0)
            error("Couldn't release PPP unit ppp_sockfd %d: %m", ppp_sockfd);
    }
    if (!multilink)
        remove_fd(ppp_sockfd);
}

/* -----------------------------------------------------------------------------
Check whether the link seems not to be 8-bit clean
----------------------------------------------------------------------------- */
void clean_check()
{
    int x;
    char *s;

    if (!still_ppp())
        return;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) == 0) {
	s = NULL;
	switch (~x & (SC_RCV_B7_0|SC_RCV_B7_1|SC_RCV_EVNP|SC_RCV_ODDP)) {
	case SC_RCV_B7_0:
	    s = "bit 7 set to 1";
	    break;
	case SC_RCV_B7_1:
	    s = "bit 7 set to 0";
	    break;
	case SC_RCV_EVNP:
	    s = "odd parity";
	    break;
	case SC_RCV_ODDP:
	    s = "even parity";
	    break;
	}
	if (s != NULL) {
	    warning("Serial link is not 8-bit clean:");
	    warning("All received characters had %s", s);
	}
    }
}

/* -----------------------------------------------------------------------------
Set up the serial port on `fd' for 8 bits, no parity,
 * at the requested speed, etc.  If `local' is true, set CLOCAL
 * regardless of whether the modem option was specified.
 *
 * For *BSD, we assume that speed_t values numerically equal bits/second
----------------------------------------------------------------------------- */
void set_up_tty(int fd, int local)
{
    struct termios tios;

    // set the file descriptor as the controlling terminal, in order to receive SIGHUP
    if (ioctl(fd, TIOCSCTTY, (char *)NULL) == -1)
        error("set_up_tty, can't set controlling terminal: %m");

    if (tcgetattr(fd, &tios) < 0) {
	error("tcgetattr: %m");
        return;
    }

    if (!restore_term) {
	inittermios = tios;
	ioctl(fd, TIOCGWINSZ, &wsinfo);
    }

    tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CLOCAL);
    if (crtscts > 0 && !local) {
        if (crtscts == 2) {
#ifdef CDTRCTS
            tios.c_cflag |= CDTRCTS;
#endif
	} else
	    tios.c_cflag |= CRTSCTS;
    } else if (crtscts < 0) {
	tios.c_cflag &= ~CRTSCTS;
#ifdef CDTRCTS
	tios.c_cflag &= ~CDTRCTS;
#endif
    }

    tios.c_cflag |= CS8 | CREAD | HUPCL;
    if (local || !modem)
	tios.c_cflag |= CLOCAL;
    tios.c_iflag = IGNBRK | IGNPAR;
    tios.c_oflag = 0;
    tios.c_lflag = 0;
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;

    if (crtscts == -2) {
	tios.c_iflag |= IXON | IXOFF;
	tios.c_cc[VSTOP] = 0x13;	/* DC3 = XOFF = ^S */
	tios.c_cc[VSTART] = 0x11;	/* DC1 = XON  = ^Q */
    }

    if (inspeed) {
	cfsetospeed(&tios, inspeed);
	cfsetispeed(&tios, inspeed);
    } else {
	inspeed = cfgetospeed(&tios);
	/*
	 * We can't proceed if the serial port speed is 0,
	 * since that implies that the serial port is disabled.
	 */
	if (inspeed == 0)
	    fatal("Baud rate for %s is 0; need explicit baud rate", devnam);
    }
	
    baud_rate = inspeed;
    if (tcsetattr(fd, TCSAFLUSH, &tios) < 0)
	fatal("tcsetattr: %m");

    restore_term = 1;
}


/* -----------------------------------------------------------------------------
Set up the serial port
 * If `local' is true, set CLOCAL
 * regardless of whether the modem option was specified.
 * other parameter (speed, start/stop bits, parity) may have been changed by the CCL
----------------------------------------------------------------------------- */
void set_up_tty_local(int fd, int local)
{
    struct termios tios;

    if (tcgetattr(fd, &tios) < 0) {
	error("tcgetattr: %m");
        return;
    }

   tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;

    tios.c_cflag &= ~CLOCAL;
    if (local || !modem)
	tios.c_cflag |= CLOCAL;

    if (tcsetattr(fd, TCSANOW, &tios) < 0)
	fatal("tcsetattr: %m");
}

/* -----------------------------------------------------------------------------
restore the terminal to the saved settings 
----------------------------------------------------------------------------- */
void restore_tty(int fd)
{
    if (restore_term) {
	if (!default_device) {
	    /*
	     * Turn off echoing, because otherwise we can get into
	     * a loop with the tty and the modem echoing to each other.
	     * We presume we are the sole user of this tty device, so
	     * when we close it, it will revert to its defaults anyway.
	     */
	    inittermios.c_lflag &= ~(ECHO | ECHONL);
	}
	if (!hungup) {
            if (tcsetattr(fd, TCSAFLUSH, &inittermios) < 0)
                if (errno != ENXIO)
                    warning("tcsetattr: %m");
            ioctl(fd, TIOCSWINSZ, &wsinfo);
        }
	restore_term = 0;
    }
}

/* -----------------------------------------------------------------------------
control the DTR line on the serial port.
 * This is called from die(), so it shouldn't call die()
----------------------------------------------------------------------------- */
void setdtr(int fd, int on)
{
    int modembits = TIOCM_DTR;

    ioctl(fd, (on? TIOCMBIS: TIOCMBIC), &modembits);
}

/* -----------------------------------------------------------------------------
get a pty master/slave pair and chown the slave side
 * to the uid given.  Assumes slave_name points to >= 12 bytes of space
----------------------------------------------------------------------------- */
int get_pty(int *master_fdp, int *slave_fdp, char *slave_name, int uid)
{
    struct termios tios;

    if (openpty(master_fdp, slave_fdp, slave_name, NULL, NULL) < 0)
	return 0;

    fchown(*slave_fdp, uid, -1);
    fchmod(*slave_fdp, S_IRUSR | S_IWUSR);
    if (tcgetattr(*slave_fdp, &tios) == 0) {
	tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB);
	tios.c_cflag |= CS8 | CREAD;
	tios.c_iflag  = IGNPAR | CLOCAL;
	tios.c_oflag  = 0;
	tios.c_lflag  = 0;
	if (tcsetattr(*slave_fdp, TCSAFLUSH, &tios) < 0)
	    warning("couldn't set attributes on pty: %m");
    } else
	warning("couldn't get attributes on pty: %m");

    return 1;
}

/* -----------------------------------------------------------------------------
create the ppp interface and configure it in loopback mode.
----------------------------------------------------------------------------- */
int open_ppp_loopback()
{
    looped = 1;
    /* allocate ourselves a ppp unit */
    if (make_ppp_unit() < 0)
        die(1);
    set_flags(ppp_sockfd, SC_LOOP_TRAFFIC);
    set_kdebugflag(kdebugflag);
    ppp_fd = -1;
    return ppp_sockfd;
}

/* -----------------------------------------------------------------------------
Output PPP packet
----------------------------------------------------------------------------- */
void output(int unit, u_char *p, int len)
{

    dump_packet("sent", p, len);
    
    // don't write FF03
    len -= 2;
    p += 2;
    
    // link protocol are sent to the link
    // other protocols are send to the bundle
  /* test was changed because compiler strangeness for embedded os */
//    if (write((ntohs(*(u_short*)p) >= 0xC000) ? ppp_fd : ppp_sockfd, p, len) < 0) {
	if (write((p[0] >= 0xC0) ? ppp_fd : ppp_sockfd, p, len) < 0) {
	if (errno != EIO)
	    error("write: %m");
    }
}

/* -----------------------------------------------------------------------------
wait until there is data available, for the length of time specified by *timo
(indefinite if timo is NULL)
----------------------------------------------------------------------------- */
void wait_input(struct timeval *timo)
{
    int n;

    ready_fds = in_fds;
    n = select(max_in_fd + 1, &ready_fds, NULL, NULL, timo);
   if (n < 0 && errno != EINTR)
	fatal("select: %m");

   if (n < 0) {
        FD_ZERO(&ready_fds);
   }
}

/* -----------------------------------------------------------------------------
wait on fd until there is data available, for the delay in milliseconds
return 0 if timeout expires, < 0 if error, otherwise > 0
----------------------------------------------------------------------------- */
int wait_input_fd(int fd, int delay)
{
    fd_set 		ready;
    int 		n;
    struct timeval 	t;

    t.tv_sec = delay / 1000;
    t.tv_usec = delay % 1000;

    FD_ZERO(&ready);
    FD_SET(fd, &ready);

    do {
        n = select(fd + 1, &ready, NULL, &ready, &t);
    } while (n < 0 && errno == EINTR);
    
    if (n > 0)
        if (ioctl(fd, FIONREAD, &n) == -1)
            n = -1;

    return n;
}


/* -----------------------------------------------------------------------------
add an fd to the set that wait_input waits for
----------------------------------------------------------------------------- */
void add_fd(int fd)
{
    FD_SET(fd, &in_fds);
    if (fd > max_in_fd)
	max_in_fd = fd;
}

/* -----------------------------------------------------------------------------
remove an fd from the set that wait_input waits for
----------------------------------------------------------------------------- */
void remove_fd(int fd)
{
    FD_CLR(fd, &in_fds);
}

/* -----------------------------------------------------------------------------
return 1 is fd is set (i.e. select returned with this file descriptor set)
----------------------------------------------------------------------------- */
bool is_ready_fd(int fd)
{
    return (FD_ISSET(fd, &ready_fds) != 0);
}

/* -----------------------------------------------------------------------------
get a PPP packet from the serial device
----------------------------------------------------------------------------- */
int read_packet(u_char *buf)
{
    int len = -1;
    
    // FF03 are not read
    *buf++ = PPP_ALLSTATIONS;
    *buf++ = PPP_UI;

    // read first the socket attached to the link
    if (ppp_fd >= 0) {
        if ((len = read(ppp_fd, buf, PPP_MRU + PPP_HDRLEN - 2)) < 0) {
            if (errno != EWOULDBLOCK && errno != EINTR)
                error("read from socket link: %m");
        }
    }
    
    // then, if nothing, link the socket attached to the bundle
    if (len < 0 && ifunit >= 0) {
        if ((len = read(ppp_sockfd, buf, PPP_MRU + PPP_HDRLEN - 2)) < 0) {
            if (errno != EWOULDBLOCK && errno != EINTR)
                error("read from socket bundle: %m");
        }
    }
    return (len <= 0 ? len : len + 2);
}

/* -----------------------------------------------------------------------------
 read characters from the loopback, form them
 * into frames, and detect when we want to bring the real link up.
 * Return value is 1 if we need to bring up the link, 0 otherwise
----------------------------------------------------------------------------- */
int get_loop_output()
{
    int rv = 0;
    int n;

    while ((n = read_packet(inpacket_buf)) > 0)
        if (loop_frame(inpacket_buf, n))
            rv = 1;
    return rv;
}

/* -----------------------------------------------------------------------------
configure the transmit characteristics of the ppp interface
 ----------------------------------------------------------------------------- */
void tty_send_config(int mtu, u_int32_t asyncmap, int pcomp, int accomp)
{
    u_int x;

    if (!still_ppp())
	return;

    if (ioctl(ppp_fd, PPPIOCSASYNCMAP, (caddr_t) &asyncmap) < 0)
	fatal("ioctl(PPPIOCSASYNCMAP): %m");

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl (PPPIOCGFLAGS): %m");
    x = pcomp? x | SC_COMP_PROT: x &~ SC_COMP_PROT;
    x = accomp? x | SC_COMP_AC: x &~ SC_COMP_AC;
    x = sync_serial ? x | SC_SYNC : x & ~SC_SYNC;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl(PPPIOCSFLAGS): %m");
        
    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPCompressionPField, pcomp);
    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPCompressionACField, accomp);
    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPTransmitACCM, asyncmap);
}

/* -----------------------------------------------------------------------------
configure the transmit characteristics of the ppp interface
function used for synchronous links, asyncmap is irrelevant
 ----------------------------------------------------------------------------- */
void generic_send_config(int mtu, u_int32_t asyncmap, int pcomp, int accomp)
{
    u_int x;

    if (!still_ppp())
	return;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl (PPPIOCGFLAGS): %m");
    x = pcomp? x | SC_COMP_PROT: x &~ SC_COMP_PROT;
    x = accomp? x | SC_COMP_AC: x &~ SC_COMP_AC;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl(PPPIOCSFLAGS): %m");
        
    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPCompressionPField, pcomp);
    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPCompressionACField, accomp);
}

/* -----------------------------------------------------------------------------
set the MTU on the PPP network interface
----------------------------------------------------------------------------- */
void netif_set_mtu(int unit, int mtu)
{
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    ifr.ifr_mtu = mtu;
    if (ioctl(ip_sockfd, SIOCSIFMTU, (caddr_t) &ifr) < 0)
	error("ioctl (SIOCSIFMTU): %m");

    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPMTU, mtu);
}

/* -----------------------------------------------------------------------------
get the MTU on the PPP network interface 
----------------------------------------------------------------------------- */
int netif_get_mtu(int unit)
{
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(ip_sockfd, SIOCGIFMTU, (caddr_t) &ifr) < 0) {
	error("ioctl (SIOCGIFMTU): %m");
        return 0;
    }
    return ifr.ifr_mtu;
}


/* -----------------------------------------------------------------------------
stop traffic on this link
 ----------------------------------------------------------------------------- */
void ppp_hold(int unit)
{
    u_int x;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	warning("ioctl (PPPIOCGFLAGS): %m");
        return;
    }
    x |= SC_HOLD;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	warning("ioctl(PPPIOCSFLAGS): %m");
}

/* -----------------------------------------------------------------------------
resume traffic on this link
 ----------------------------------------------------------------------------- */
void ppp_cont(int unit)
{
    u_int x;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	warning("ioctl (PPPIOCGFLAGS): %m");
        return;
    }
    x &= ~SC_HOLD;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	warning("ioctl(PPPIOCSFLAGS): %m");
}

/* -----------------------------------------------------------------------------
set the extended transmit ACCM for the interface
----------------------------------------------------------------------------- */
void tty_set_xaccm(ext_accm accm)
{
    if (!still_ppp())
	return;
    if (ioctl(ppp_fd, PPPIOCSXASYNCMAP, accm) < 0 && errno != ENOTTY)
	warning("ioctl(set extended ACCM): %m");
}

/* -----------------------------------------------------------------------------
configure the receive-side characteristics of the ppp interface.
----------------------------------------------------------------------------- */
void tty_recv_config(int mru, u_int32_t asyncmap, int pcomp, int accomp)
{
    int x;

    if (!still_ppp())
	return;

    if (ioctl(ppp_fd, PPPIOCSMRU, (caddr_t) &mru) < 0)
	fatal("ioctl(PPPIOCSMRU): %m");
    if (ioctl(ppp_fd, PPPIOCSRASYNCMAP, (caddr_t) &asyncmap) < 0)
	fatal("ioctl(PPPIOCSRASYNCMAP): %m");
    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl (PPPIOCGFLAGS): %m");
    x = !accomp? x | SC_REJ_COMP_AC: x &~ SC_REJ_COMP_AC;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl(PPPIOCSFLAGS): %m");

    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPMRU, mru);
    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPReceiveACCM, asyncmap);
}

/* -----------------------------------------------------------------------------
configure the receive-side characteristics of the ppp interface.
function used for synchronous links, asyncmap is irrelevant
----------------------------------------------------------------------------- */
void generic_recv_config(int mru, u_int32_t asyncmap, int pcomp, int accomp)
{
    int x;

    if (!still_ppp())
	return;

    if (ioctl(ppp_fd, PPPIOCSMRU, (caddr_t) &mru) < 0)
	fatal("ioctl(PPPIOCSMRU): %m");
    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl (PPPIOCGFLAGS): %m");
    x = !accomp? x | SC_REJ_COMP_AC: x &~ SC_REJ_COMP_AC;
    if (ioctl(ppp_fd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	fatal("ioctl(PPPIOCSFLAGS): %m");

    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPLCPMRU, mru);
}

/* -----------------------------------------------------------------------------
ask kernel whether a given compression method
 * is acceptable for use.  Returns 1 if the method and parameters
 * are OK, 0 if the method is known but the parameters are not OK
 * (e.g. code size should be reduced), or -1 if the method is unknown
 ----------------------------------------------------------------------------- */
int ccp_test(int unit, u_char * opt_ptr, int opt_len, int for_transmit)
{
    struct ppp_option_data data;

    data.ptr = opt_ptr;
    data.length = opt_len;
    data.transmit = for_transmit;
    if (ioctl(ppp_sockfd, PPPIOCSCOMPRESS, (caddr_t) &data) >= 0)
	return 1;
    return (errno == ENOBUFS)? 0: -1;
}

/* -----------------------------------------------------------------------------
inform kernel about the current state of CCP
----------------------------------------------------------------------------- */
void ccp_flags_set(int unit, int isopen, int isup)
{
    int x;

    if (!still_ppp())
	return;

    if (ioctl(ppp_sockfd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl (PPPIOCGFLAGS): %m");
	return;
    }

    x = isopen? x | SC_CCP_OPEN: x &~ SC_CCP_OPEN;
    x = isup? x | SC_CCP_UP: x &~ SC_CCP_UP;
    if (ioctl(ppp_sockfd, PPPIOCSFLAGS, (caddr_t) &x) < 0)
	error("ioctl(PPPIOCSFLAGS): %m");
}

/* -----------------------------------------------------------------------------
returns 1 if decompression was disabled as a
 * result of an error detected after decompression of a packet,
 * 0 otherwise.  This is necessary because of patent nonsense
 ----------------------------------------------------------------------------- */
int ccp_fatal_error(int unit)
{
    int x;

    if (ioctl(ppp_fd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl(PPPIOCGFLAGS): %m");
	return 0;
    }
    return x & SC_DC_FERROR;
}

/* -----------------------------------------------------------------------------
return how long the link has been idle
----------------------------------------------------------------------------- */
int get_idle_time(int u, struct ppp_idle *ip)
{
    return ioctl(ppp_sockfd, PPPIOCGIDLE, ip) >= 0;
}

/* -----------------------------------------------------------------------------
return statistics for the link
----------------------------------------------------------------------------- */
int get_ppp_stats(int u, struct pppd_stats *stats)
{
    struct ifpppstatsreq req;

    memset (&req, 0, sizeof (req));
    strlcpy(req.ifr_name, ifname, sizeof(req.ifr_name));
    if (ioctl(ip_sockfd, SIOCGPPPSTATS, &req) < 0) {
	error("Couldn't get PPP statistics: %m");
	return 0;
    }
    stats->bytes_in = req.stats.p.ppp_ibytes;
    stats->bytes_out = req.stats.p.ppp_obytes;
    stats->pkts_in = req.stats.p.ppp_ipackets;
    stats->pkts_out = req.stats.p.ppp_opackets;
    return 1;
}


#ifdef PPP_FILTER
/* -----------------------------------------------------------------------------
transfer the pass and active filters to the kernel
----------------------------------------------------------------------------- */
int set_filters(struct bpf_program *pass, struct bpf_program *active)
{
    int ret = 1;

    if (pass->bf_len > 0) {
	if (ioctl(ppp_fd, PPPIOCSPASS, pass) < 0) {
	    error("Couldn't set pass-filter in kernel: %m");
	    ret = 0;
	}
    }
    if (active->bf_len > 0) {
	if (ioctl(ppp_fd, PPPIOCSACTIVE, active) < 0) {
	    error("Couldn't set active-filter in kernel: %m");
	    ret = 0;
	}
    }
    return ret;
}
#endif

/* -----------------------------------------------------------------------------
config tcp header compression
----------------------------------------------------------------------------- */
int sifvjcomp(int u, int vjcomp, int cidcomp, int maxcid)
{
    u_int x;

    if (ioctl(ppp_sockfd, PPPIOCGFLAGS, (caddr_t) &x) < 0) {
	error("ioctl (PPPIOCGFLAGS): %m");
	return 0;
    }
    x = vjcomp ? x | SC_COMP_TCP: x &~ SC_COMP_TCP;
    x = cidcomp? x & ~SC_NO_TCP_CCID: x | SC_NO_TCP_CCID;
    if (ioctl(ppp_sockfd, PPPIOCSFLAGS, (caddr_t) &x) < 0) {
	error("ioctl(PPPIOCSFLAGS): %m");
	return 0;
    }
    if (vjcomp && ioctl(ppp_sockfd, PPPIOCSMAXCID, (caddr_t) &maxcid) < 0) {
	error("ioctl(PPPIOCSMAXCID): %m");
	return 0;
    }
    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPIPCPCompressionVJ, vjcomp);
    return 1;
}

/* -----------------------------------------------------------------------------
Config the interface up and enable IP packets to pass
----------------------------------------------------------------------------- */
int sifup(int u)
{
    struct ifreq ifr;

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(ip_sockfd, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl (SIOCGIFFLAGS): %m");
	return 0;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(ip_sockfd, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl(SIOCSIFFLAGS): %m");
	return 0;
    }
    if_is_up = 1;
    return 1;
}

/* -----------------------------------------------------------------------------
Set the mode for handling packets for a given NP
----------------------------------------------------------------------------- */
int sifnpmode(int u, int proto, enum NPmode mode)
{
    struct npioctl npi;

    npi.protocol = proto;
    npi.mode = mode;
    if (ioctl(ppp_sockfd, PPPIOCSNPMODE, &npi) < 0) {
	error("ioctl(set NP %d mode to %d): %m", proto, mode);
	return 0;
    }
    return 1;
}

/* -----------------------------------------------------------------------------
Set the mode for filtering protocol addresses
----------------------------------------------------------------------------- */
int sifnpafmode(int u, int proto, enum NPAFmode mode)
{
    struct npafioctl npi;

    npi.protocol = proto;
    npi.mode = mode;
    if (ioctl(ppp_sockfd, PPPIOCSNPAFMODE, &npi) < 0) {
	error("ioctl(set NPAF %d mode to %d): %m", proto, mode);
	return 0;
    }
    return 1;
}

/* -----------------------------------------------------------------------------
Config the interface down
----------------------------------------------------------------------------- */
int sifdown(int u)
{
    struct ifreq ifr;
    int rv;
    struct npioctl npi;

    npi.protocol = PPP_IP;
    if ((ioctl(ppp_sockfd, PPPIOCGNPMODE, (caddr_t) &npi) == 0)	
        && (npi.mode != NPMODE_DROP)) {
        return 0;
    }

    npi.protocol = PPP_IPV6;
    if ((ioctl(ppp_sockfd, PPPIOCGNPMODE, (caddr_t) &npi) == 0)	
        && (npi.mode != NPMODE_DROP)) {
        return 0;
    }

    /* ipv4 and ipv4 are both down. take the interface down now */
    
    rv = 1;

    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(ip_sockfd, SIOCGIFFLAGS, (caddr_t) &ifr) < 0) {
	error("ioctl (SIOCGIFFLAGS): %m");
	rv = 0;
    } else {
	ifr.ifr_flags &= ~IFF_UP;
	if (ioctl(ip_sockfd, SIOCSIFFLAGS, (caddr_t) &ifr) < 0) {
	    error("ioctl(SIOCSIFFLAGS): %m");
	    rv = 0;
	} else
	    if_is_up = 0;
    }
    return rv;
}


/* -----------------------------------------------------------------------------
set the route for the interface.
----------------------------------------------------------------------------- */
int sifroute(int u, u_int32_t o, u_int32_t h, u_int32_t m)
{
	if (addifroute && m != 0xFFFFFFFF) {
		ifroute_address.s_addr = o & m;
		ifroute_mask.s_addr = m;
		ifroute_installed = route_interface(RTM_ADD, ifroute_address, ifroute_mask, IFT_PPP, ifname, 0);
	}

	return 1;
}

/* -----------------------------------------------------------------------------
clear the route for the interface.
----------------------------------------------------------------------------- */
int cifroute()
{
	if (addifroute && ifroute_installed) {
		route_interface(RTM_DELETE, ifroute_address, ifroute_mask, IFT_PPP, ifname, 0);
		ifroute_installed = 0;
	}

	return 1;
}

/* -----------------------------------------------------------------------------
set the sa_family field of a struct sockaddr, if it exists.
----------------------------------------------------------------------------- */
#define SET_SA_FAMILY(addr, family)		\
    BZERO((char *) &(addr), sizeof(addr));	\
    addr.sa_family = (family); 			\
    addr.sa_len = sizeof(addr);

/* -----------------------------------------------------------------------------
Config the interface IP addresses and netmask
----------------------------------------------------------------------------- */
int sifaddr(int u, u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct ifaliasreq ifra __attribute__ ((aligned (4)));		// Wcast-align fix - force alignment
    struct ifreq ifr;

// XXX from sys/sockio.h
#define SIOCPROTOATTACH _IOWR('i', 80, struct ifreq)    /* attach proto to interface */
#define SIOCPROTODETACH _IOWR('i', 81, struct ifreq)    /* detach proto from interface */

    // first plumb ip over ppp
    if (ipv4_plumbed == 0) {
        strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
        if (ioctl(ip_sockfd, SIOCPROTOATTACH, (caddr_t) &ifr) < 0) {
            error("Couldn't plumb IP to the interface: %d %m", errno);
            //return 0;
        }
        ipv4_plumbed = 1;
    }
    
    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    if (m != 0) {
        SET_SA_FAMILY(ifra.ifra_mask, AF_INET);
        (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_mask)->sin_addr.s_addr = m;
    } else
        BZERO(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
        BZERO(&ifr, sizeof(ifr));
        strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
        if (ioctl(ip_sockfd, SIOCDIFADDR, (caddr_t) &ifr) < 0) {
            if (errno != EADDRNOTAVAIL)
	    warning("Couldn't remove interface address: %m");
    }
    if (ioctl(ip_sockfd, SIOCAIFADDR, (caddr_t) &ifra) < 0) {
	if (errno != EEXIST) {
	    error("Couldn't set interface address: %m");
	    return 0;
	}
	warning("Couldn't set interface address: Address %I already exists", o);
    }
    ifaddrs[0] = o;
    ifaddrs[1] = h;
    
    if (looplocal) {
	struct in_addr o1;
        struct in_addr mask;
        
        set_flags(ppp_sockfd, get_flags(ppp_sockfd) | SC_LOOP_LOCAL);
        // add a route for our local address via our interface
        o1.s_addr = o;
        mask.s_addr = 0;
        route_interface(RTM_ADD, o1, mask, IFT_PPP, ifname, 1);
    }

	sifroute(u, o, h, m);
	
    publish_stateaddr(o, h, m);

	return 1;
}

/* -----------------------------------------------------------------------------
 Update the interface IP addresses and netmask
 ----------------------------------------------------------------------------- */
int uifaddr(int u, u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct ifaliasreq ifra __attribute__ ((aligned (4)));	// Wcast-align fix - force alignment
    struct ifreq ifr;
    
    cifroute();
	
	// XXX from sys/sockio.h
#define SIOCPROTOATTACH _IOWR('i', 80, struct ifreq)    /* attach proto to interface */
#define SIOCPROTODETACH _IOWR('i', 81, struct ifreq)    /* detach proto from interface */
	
    // first plumb ip over ppp
    if (ipv4_plumbed == 0) {
		error("Interface should have been plumbed already");
		return -1;
    }
    
    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    if (m != 0) {
		SET_SA_FAMILY(ifra.ifra_mask, AF_INET);
		(ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_mask)->sin_addr.s_addr = m;
    } else
		BZERO(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    BZERO(&ifr, sizeof(ifr));
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(ip_sockfd, SIOCDIFADDR, (caddr_t) &ifr) < 0) {
		if (errno != EADDRNOTAVAIL)
			warning("Couldn't remove interface address: %m");
    }
    if (ioctl(ip_sockfd, SIOCAIFADDR, (caddr_t) &ifra) < 0) {
		if (errno != EEXIST) {
			error("Couldn't set interface address: %m");
			return 0;
		}
		warning("Couldn't set interface address: Address %I already exists", o);
    }
    ifaddrs[0] = o;
    ifaddrs[1] = h;
    
	sifroute(u, o, h, m);
	
    update_stateaddr(o, h, m);
	
	return 1;
}

/* -----------------------------------------------------------------------------
Clear the interface IP addresses, and delete routes
 * through the interface if possible
 ----------------------------------------------------------------------------- */
int cifaddr(int u, u_int32_t o, u_int32_t h)
{
    //struct ifreq ifr;
    struct ifaliasreq ifra __attribute__ ((aligned (4)));		// Wcast-align fix - force alignment

// XXX from sys/sockio.h
#define SIOCPROTOATTACH _IOWR('i', 80, struct ifreq)    /* attach proto to interface */
#define SIOCPROTODETACH _IOWR('i', 81, struct ifreq)    /* detach proto from interface */

	cifroute();

    ifaddrs[0] = 0;
    strlcpy(ifra.ifra_name, ifname, sizeof(ifra.ifra_name));
    SET_SA_FAMILY(ifra.ifra_addr, AF_INET);
    (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_addr)->sin_addr.s_addr = o;
    SET_SA_FAMILY(ifra.ifra_broadaddr, AF_INET);
    (ALIGNED_CAST(struct sockaddr_in *) &ifra.ifra_broadaddr)->sin_addr.s_addr = h;
    BZERO(&ifra.ifra_mask, sizeof(ifra.ifra_mask));
    if (ioctl(ip_sockfd, SIOCDIFADDR, (caddr_t) &ifra) < 0) {
	if (errno != EADDRNOTAVAIL)
	    warning("Couldn't delete interface address: %m");
	return 0;
    }

#if 0
    // unplumb ip from ppp
    strlcpy(ifr.ifr_name, ifname, sizeof (ifr.ifr_name));
    if (ioctl(ip_sockfd, SIOCPROTODETACH, (caddr_t) &ifr) < 0) {
        error("Couldn't unplumb IP from the interface: %m");
	return 0;
    }
#endif

    unpublish_dict(kSCEntNetIPv4);

    return 1;
}

#ifdef INET6
/* -----------------------------------------------------------------------------
Config the interface IPv6 addresses
----------------------------------------------------------------------------- */
int sif6addr (int unit, eui64_t our_eui64, eui64_t his_eui64)
{
    int ifindex, s;
    struct in6_ifreq ifr6;
    struct in6_aliasreq addreq6 __attribute__ ((aligned (4)));		// Wcast-align fix - force alignment

// XXX from sys/sockio.h
#define SIOCPROTOATTACH_IN6 _IOWR('i', 110, struct in6_aliasreq)    /* attach proto to interface */
#define SIOCPROTODETACH_IN6 _IOWR('i', 111, struct in6_ifreq)    /* detach proto from interface */
#define SIOCPROTOATTACH _IOWR('i', 80, struct ifreq)    /* attach proto to interface */
#define SIOCPROTODETACH _IOWR('i', 81, struct ifreq)    /* detach proto from interface */
#define SIOCLL_START _IOWR('i', 130, struct in6_aliasreq)    /* start aquiring linklocal on interface */
#define SIOCLL_STOP _IOWR('i', 131, struct in6_ifreq)    /* deconfigure linklocal from interface */

    s = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s < 0) {
        error("Can't create IPv6 socket: %m");
        return 0;
    }

    /* actually, this part is not kame local - RFC2553 conformant */
    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
	error("sifaddr6: no interface %s", ifname);
	return 0;
    }

    strlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
    if (ioctl(s, SIOCPROTOATTACH_IN6, &ifr6) < 0) {
        error("sif6addr: can't attach IPv6 protocol: %m");
        close(s);
        return 0;
    }

    memset(&addreq6, 0, sizeof(addreq6));
    strlcpy(addreq6.ifra_name, ifname, sizeof(addreq6.ifra_name));

    /* my addr */
    addreq6.ifra_addr.sin6_family = AF_INET6;
    addreq6.ifra_addr.sin6_len = sizeof(struct sockaddr_in6);
    addreq6.ifra_addr.sin6_addr.s6_addr[0] = 0xfe;
    addreq6.ifra_addr.sin6_addr.s6_addr[1] = 0x80;
    memcpy(&addreq6.ifra_addr.sin6_addr.s6_addr[8], &our_eui64,
	sizeof(our_eui64));
    /* KAME ifindex hack */
    *ALIGNED_CAST(u_int16_t *)&addreq6.ifra_addr.sin6_addr.s6_addr[2] = htons(ifindex);
    
    /* his addr */
    addreq6.ifra_dstaddr.sin6_family = AF_INET6;
    addreq6.ifra_dstaddr.sin6_len = sizeof(struct sockaddr_in6);
    addreq6.ifra_dstaddr.sin6_addr.s6_addr[0] = 0xfe;
    addreq6.ifra_dstaddr.sin6_addr.s6_addr[1] = 0x80;
    memcpy(&addreq6.ifra_dstaddr.sin6_addr.s6_addr[8], &his_eui64,
	sizeof(our_eui64));
    /* KAME ifindex hack */
    *ALIGNED_CAST(u_int16_t *)&addreq6.ifra_dstaddr.sin6_addr.s6_addr[2] = htons(ifindex);
    
    /* prefix mask: 128bit */
    addreq6.ifra_prefixmask.sin6_family = AF_INET6;
    addreq6.ifra_prefixmask.sin6_len = sizeof(struct sockaddr_in6);
    memset(&addreq6.ifra_prefixmask.sin6_addr, 0xff,
	sizeof(addreq6.ifra_prefixmask.sin6_addr));

    /* address lifetime (infty) */
    addreq6.ifra_lifetime.ia6t_pltime = ND6_INFINITE_LIFETIME;
    addreq6.ifra_lifetime.ia6t_vltime = ND6_INFINITE_LIFETIME;

    if (ioctl(s, SIOCLL_START, &addreq6) < 0) {
        error("sif6addr: can't set LL address: %m");
        close(s);
        return 0;
    }

    close(s);

    return 1;
}

/* -----------------------------------------------------------------------------
Clear the interface IPv6 addresses
 ----------------------------------------------------------------------------- */
int cif6addr (int unit, eui64_t our_eui64, eui64_t his_eui64)
{
    int s;
    struct ifreq ifr;
    struct in6_ifreq ifr6;

// XXX from sys/sockio.h
#define SIOCPROTOATTACH_IN6 _IOWR('i', 110, struct in6_aliasreq)    /* attach proto to interface */
#define SIOCPROTODETACH_IN6 _IOWR('i', 111, struct in6_ifreq)    /* detach proto from interface */
#define SIOCPROTOATTACH _IOWR('i', 80, struct ifreq)    /* attach proto to interface */
#define SIOCPROTODETACH _IOWR('i', 81, struct ifreq)    /* detach proto from interface */
#define SIOCLL_START _IOWR('i', 130, struct in6_aliasreq)    /* start aquiring linklocal on interface */
#define SIOCLL_STOP _IOWR('i', 131, struct in6_ifreq)    /* deconfigure linklocal from interface */

    s = socket(AF_INET6, SOCK_DGRAM, 0);
    if (s < 0) {
        error("Can't create IPv6 socket: %m");
        return 0;
    }

    /* first try old ioctl */
    strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
    if (ioctl(s, SIOCPROTODETACH, &ifr) < 0) {
        /* then new ioctl */
        strlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
        if (ioctl(s, SIOCLL_STOP, &ifr6) < 0) {
            warning("Can't stop LL address: %m");
        }
        strlcpy(ifr6.ifr_name, ifname, sizeof(ifr6.ifr_name));
        if (ioctl(s, SIOCPROTODETACH_IN6, &ifr6) < 0) {
            warning("Can't detach IPv6 protocol: %m");
        }
        
        close(s);
        return 0;
    }

    close(s);

    return 1;
}

/* -----------------------------------------------------------------------------
Returns an iterator containing the primary (built-in) Ethernet interface. 
The caller is responsible for releasing the iterator after the caller is done with it.
 ----------------------------------------------------------------------------- */
static kern_return_t FindPrimaryEthernetInterfaces(io_iterator_t *matchingServices)
{
    kern_return_t  kernResult; 
    CFMutableDictionaryRef matchingDict;
    CFMutableDictionaryRef propertyMatchDict;
    
    // Ethernet interfaces are instances of class kIOEthernetInterfaceClass. 
    // IOServiceMatching is a convenience function to create a dictionary with the key kIOProviderClassKey and 
    // the specified value.
    matchingDict = IOServiceMatching(kIOEthernetInterfaceClass);
    if (matchingDict) {
    
        // Each IONetworkInterface object has a Boolean property with the key kIOPrimaryInterface. Only the
        // primary (built-in) interface has this property set to TRUE.
        
        // IOServiceGetMatchingServices uses the default matching criteria defined by IOService. This considers
        // only the following properties plus any family-specific matching in this order of precedence 
        // (see IOService::passiveMatch):
        //
        // kIOProviderClassKey (IOServiceMatching)
        // kIONameMatchKey (IOServiceNameMatching)
        // kIOPropertyMatchKey
        // kIOPathMatchKey
        // kIOMatchedServiceCountKey
        // family-specific matching
        // kIOBSDNameKey (IOBSDNameMatching)
        // kIOLocationMatchKey
        
        // The IONetworkingFamily does not define any family-specific matching. This means that in            
        // order to have IOServiceGetMatchingServices consider the kIOPrimaryInterface property, we must
        // add that property to a separate dictionary and then add that to our matching dictionary
        // specifying kIOPropertyMatchKey.
            
        propertyMatchDict = CFDictionaryCreateMutable( kCFAllocatorDefault, 0,
                                                       &kCFTypeDictionaryKeyCallBacks,
                                                       &kCFTypeDictionaryValueCallBacks);
        if (propertyMatchDict) {
        
            // Set the value in the dictionary of the property with the given key, or add the key 
            // to the dictionary if it doesn't exist. This call retains the value object passed in.
            CFDictionarySetValue(propertyMatchDict, CFSTR(kIOPrimaryInterface), kCFBooleanTrue); 
            
            // Now add the dictionary containing the matching value for kIOPrimaryInterface to our main
            // matching dictionary. This call will retain propertyMatchDict, so we can release our reference 
            // on propertyMatchDict after adding it to matchingDict.
            CFDictionarySetValue(matchingDict, CFSTR(kIOPropertyMatchKey), propertyMatchDict);
            CFRelease(propertyMatchDict);
        }
    }
    
    // IOServiceGetMatchingServices retains the returned iterator, so release the iterator when we're done with it.
    // IOServiceGetMatchingServices also consumes a reference on the matching dictionary so we don't need to release
    // the dictionary explicitly.
    kernResult = IOServiceGetMatchingServices(kIOMasterPortDefault, matchingDict, matchingServices);    
        
    return kernResult;
}
    
/* -----------------------------------------------------------------------------
Get the mac address of the primary interface
----------------------------------------------------------------------------- */
static kern_return_t GetPrimaryMACAddress(UInt8 *MACAddress)
{
    io_object_t  intfService;
    io_object_t  controllerService;
    kern_return_t kernResult = KERN_FAILURE;
    io_iterator_t intfIterator;

    kernResult = FindPrimaryEthernetInterfaces(&intfIterator);
    if (kernResult != KERN_SUCCESS)
        return kernResult;

    // Initialize the returned address
    bzero(MACAddress, kIOEthernetAddressSize);
    
    // IOIteratorNext retains the returned object, so release it when we're done with it.
    while (intfService = IOIteratorNext(intfIterator)) {
    
        CFTypeRef MACAddressAsCFData;        

        // IONetworkControllers can't be found directly by the IOServiceGetMatchingServices call, 
        // since they are hardware nubs and do not participate in driver matching. In other words,
        // registerService() is never called on them. So we've found the IONetworkInterface and will 
        // get its parent controller by asking for it specifically.
        
        // IORegistryEntryGetParentEntry retains the returned object, so release it when we're done with it.
        kernResult = IORegistryEntryGetParentEntry( intfService,
                                                    kIOServicePlane,
                                                    &controllerService );

        if (kernResult == KERN_SUCCESS) {
        
            // Retrieve the MAC address property from the I/O Registry in the form of a CFData
            MACAddressAsCFData = IORegistryEntryCreateCFProperty( controllerService,
                                                                  CFSTR(kIOMACAddress),
                                                                  kCFAllocatorDefault, 0);
            if (MACAddressAsCFData) {
                // Get the raw bytes of the MAC address from the CFData
                CFDataGetBytes(MACAddressAsCFData, CFRangeMake(0, kIOEthernetAddressSize), MACAddress);
                CFRelease(MACAddressAsCFData);
            }
                
            // Done with the parent Ethernet controller object so we release it.
            IOObjectRelease(controllerService);
        }
        
        // Done with the Ethernet interface object so we release it.
        IOObjectRelease(intfService);
    }
    
    IOObjectRelease(intfIterator);

    return kernResult;
}

/* -----------------------------------------------------------------------------
Convert 48-bit Ethernet address into 64-bit EUI
 ----------------------------------------------------------------------------- */
int
ether_to_eui64(eui64_t *p_eui64)
{
    static u_int8_t allzero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    static u_int8_t allone[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    kern_return_t err;
    UInt8  addr[kIOEthernetAddressSize];
 
    err = GetPrimaryMACAddress(addr);
    if (err != KERN_SUCCESS) {
        warning("Can't get hardware interface address for en0 (error = %d)\n", err);
        return 0;
    }
    
    /* check for invalid MAC address */
     if (bcmp(addr, allzero, ETHER_ADDR_LEN) == 0)
            return 0;
    if (bcmp(addr, allone, ETHER_ADDR_LEN) == 0)
            return 0;

    /* And convert the EUI-48 into EUI-64, per RFC 2472 [sec 4.1] */
    p_eui64->e8[0] = addr[0] | 0x02;
    p_eui64->e8[1] = addr[1];
    p_eui64->e8[2] = addr[2];
    p_eui64->e8[3] = 0xFF;
    p_eui64->e8[4] = 0xFE;
    p_eui64->e8[5] = addr[3];
    p_eui64->e8[6] = addr[4];
    p_eui64->e8[7] = addr[5];

    return 1;
}

#endif

/* -----------------------------------------------------------------------------
assign a default route through the address given 
----------------------------------------------------------------------------- */
int sifdefaultroute(int u, u_int32_t l, u_int32_t g)
{
    override_primary = 1;
    return publish_dictnumentry(kSCEntNetIPv4, kSCPropNetOverridePrimary, 1);
}

/* -----------------------------------------------------------------------------
delete a default route through the address given
----------------------------------------------------------------------------- */
int cifdefaultroute(int u, u_int32_t l, u_int32_t g)
{
    override_primary = 0;
    return unpublish_dictentry(kSCEntNetIPv4, kSCPropNetOverridePrimary);
}

/* ----------------------------------------------------------------------------
 update ip addresses using configd cache mechanism
 use new state information model
 ----------------------------------------------------------------------------- */
static int update_stateaddr(u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct in_addr             addr;
    CFMutableArrayRef          array;
    CFMutableDictionaryRef     ipv4_dict;
    CFStringRef                        str;
    CFStringRef                        key;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;

    /* update the store now */
    if (key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceidRef, kSCEntNetIPv4)) {
        CFPropertyListRef ref;
        if ((ref = SCDynamicStoreCopyValue(cfgCache, key)) == NULL ||
            CFGetTypeID(ref) != CFDictionaryGetTypeID()) {
            warning("SCDynamicStoreCopyValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));
            if (ref) {
                CFRelease(ref);
            }
            CFRelease(key);
            return 0;
        }
        ipv4_dict = CFDictionaryCreateMutableCopy(NULL, 0, ref);
        CFRelease(ref);
        if (!ipv4_dict || CFGetTypeID(ipv4_dict) != CFDictionaryGetTypeID()) {
            warning("CFDictionaryCreateMutableCopy IP %s failed: %s\n", ifname, SCErrorString(SCError()));
            if (ipv4_dict) {
                CFRelease(ipv4_dict);
            }
            CFRelease(key);
            return 0;
        }
    } else {
        return 0;
    }

    /* set the ip address src and dest arrays */
    if (array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks)) {
        addr.s_addr = o;
        if (str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
            CFArrayAppendValue(array, str);
            CFRelease(str);
            CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array); 
        }
        CFRelease(array);
    }
       
    if (array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks)) {
        addr.s_addr = h;
        if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
            CFArrayAppendValue(array, str);
            CFRelease(str);
            CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4DestAddresses, array);
        }
        CFRelease(array);
    }
       
    /* set the router */
    addr.s_addr = h;
    if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
        CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
        CFRelease(str);
    }
       
    /* add the interface name */
    if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), ifname)) {
        CFDictionarySetValue(ipv4_dict, kSCPropInterfaceName, str);
        CFRelease(str);
    }
       
    /* add the network signature */
    if (network_signature) {
               if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), network_signature)) {
                       CFDictionarySetValue(ipv4_dict, CFSTR("NetworkSignature"), str);
                       CFRelease(str);
               }
       }
       
    /* add the remote server peer address */
    if (server_peer) {
        CFDictionarySetValue(ipv4_dict, CFSTR("ServerAddress"), server_peer);
    }


	if (setstorevalue(key, ipv4_dict) == 0)
        warning("SCDynamicStoreSetValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));

    CFRelease(ipv4_dict);
    CFRelease(key);
    return 1;
}

/* ----------------------------------------------------------------------------
publish ip addresses using configd cache mechanism
use new state information model
----------------------------------------------------------------------------- */
int publish_stateaddr(u_int32_t o, u_int32_t h, u_int32_t m)
{
    struct in_addr		addr;
    CFMutableArrayRef		array;
    CFMutableDictionaryRef	ipv4_dict;
    CFStringRef			str;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;
            
    /* create the IPV4 dictionnary */
    if ((ipv4_dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks)) == 0)
        return 0;

   /* set the ip address src and dest arrays */
    if (array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks)) {
        addr.s_addr = o;
        if (str = CFStringCreateWithFormat(0, 0, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
            CFArrayAppendValue(array, str);
            CFRelease(str);
            CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Addresses, array); 
        }
        CFRelease(array);
    }

    if (array = CFArrayCreateMutable(0, 1, &kCFTypeArrayCallBacks)) {
        addr.s_addr = h;
        if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
            CFArrayAppendValue(array, str);
            CFRelease(str);
            CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4DestAddresses, array);
        }
        CFRelease(array);
    }

    /* set the router */
    addr.s_addr = h;
    if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&addr.s_addr))) {
        CFDictionarySetValue(ipv4_dict, kSCPropNetIPv4Router, str);
        CFRelease(str);
    }

    /* add the interface name */
    if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), ifname)) {
        CFDictionarySetValue(ipv4_dict, kSCPropInterfaceName, str);
        CFRelease(str);
    }

    /* add the network signature */
    if (network_signature) {
		if (str = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), network_signature)) {
			CFDictionarySetValue(ipv4_dict, CFSTR("NetworkSignature"), str);
			CFRelease(str);
		}
	}

    /* add the remote server peer address */
    if (server_peer) {
        CFDictionarySetValue(ipv4_dict, CFSTR("ServerAddress"), server_peer);
    }
	
    /* update the store now */
    if (str = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceidRef, kSCEntNetIPv4)) {
        
		if (setstorevalue(str, ipv4_dict) == 0)
            warning("SCDynamicStoreSetValue IP %s failed: %s\n", ifname, SCErrorString(SCError()));

        CFRelease(str);
    }
    
    CFRelease(ipv4_dict);
    return 1;
}

/* -----------------------------------------------------------------------------
 add a search domain, using configd cache mechanism.
----------------------------------------------------------------------------- */
int publish_dns_wins_entry(CFStringRef entity, CFStringRef property1, CFTypeRef ref1, CFTypeRef ref1a, 
						CFStringRef property2, CFTypeRef ref2,
						CFStringRef property3, CFTypeRef ref3, int clean)
{
    CFMutableArrayRef		mutable_array;
    CFArrayRef			array = NULL;
    CFMutableDictionaryRef	dict = NULL;
    CFStringRef			key = NULL;
    CFPropertyListRef		ref;
    int				ret = 0;

    if (cfgCache == NULL)
        return 0;

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceidRef, entity);
    if (!key) 
        goto end;
        
    if (ref = SCDynamicStoreCopyValue(cfgCache, key)) {
        dict = CFDictionaryCreateMutableCopy(0, 0, ref);
        CFRelease(ref);
    } else
        dict = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
    if (!dict || (CFGetTypeID(dict) != CFDictionaryGetTypeID()))
        goto end;

    if (!clean)
        array = CFDictionaryGetValue(dict, property1);
    if (array && (CFGetTypeID(array) == CFArrayGetTypeID()))
        mutable_array = CFArrayCreateMutableCopy(NULL, CFArrayGetCount(array) + 1, array);
    else
        mutable_array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);

    if (!mutable_array) 
        goto end;

    CFArrayAppendValue(mutable_array, ref1);
	if (ref1a)
		CFArrayAppendValue(mutable_array, ref1a);
    CFDictionarySetValue(dict, property1, mutable_array);
    CFRelease(mutable_array);

    if (property2) {
		if (!clean)
			array = CFDictionaryGetValue(dict, property2);
		if (array && (CFGetTypeID(array) == CFArrayGetTypeID()))
			mutable_array = CFArrayCreateMutableCopy(NULL, CFArrayGetCount(array) + 1, array);
		else
			mutable_array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);

		if (!mutable_array) 
			goto end;

		CFArrayAppendValue(mutable_array, ref2);
		CFDictionarySetValue(dict, property2, mutable_array);
		CFRelease(mutable_array);
    }

    if (property3) {
		if (!clean)
			array = CFDictionaryGetValue(dict, property3);
		if (array && (CFGetTypeID(array) == CFArrayGetTypeID()))
			mutable_array = CFArrayCreateMutableCopy(NULL, CFArrayGetCount(array) + 1, array);
		else
			mutable_array = CFArrayCreateMutable(NULL, 1, &kCFTypeArrayCallBacks);

		if (!mutable_array) 
			goto end;

		CFArrayAppendValue(mutable_array, ref3);
		CFDictionarySetValue(dict, property3, mutable_array);
		CFRelease(mutable_array);
    }
	    
	if (setstorevalue(key,dict))
        ret = 1;
    else
        warning("SCDynamicStoreSetValue DNS/WINS %s failed: %s\n", ifname, SCErrorString(SCError()));

end:
    if (key)  
        CFRelease(key);
    if (dict)  
        CFRelease(dict);
    return ret;
}


/* -----------------------------------------------------------------------------
set dns information
----------------------------------------------------------------------------- */
int sifdns(u_int32_t dns1, u_int32_t dns2)
{    
    CFStringRef		str1 = 0, str2 = 0, strname = 0;
    int				result = 0, clean = 1;
	long			order = 100000;
	CFNumberRef		num = 0;

	num = CFNumberCreate(NULL, kCFNumberLongType, &order);
	if (!num)
		goto done;
	
	strname = CFStringCreateWithCString(NULL, "", kCFStringEncodingUTF8);
	if (!strname)
		goto done;

	/* warn lookupd of upcoming change */
	notify_post("com.apple.system.dns.delay");
	
	if (dns1)
		str1 = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&dns1));
	if (!str1)
		goto done;
		
	if (dns2 && (dns2!=dns1))
		str2 = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&dns2));
		
	result = publish_dns_wins_entry(kSCEntNetDNS, kSCPropNetDNSServerAddresses, str1, str2, kSCPropNetDNSSupplementalMatchDomains, strname, kSCPropNetDNSSupplementalMatchOrders, num, clean);
#ifndef kSCPropNetProxiesSupplementalMatchDomains			
#define kSCPropNetProxiesSupplementalMatchDomains kSCPropNetDNSSupplementalMatchDomains
#define kSCPropNetProxiesSupplementalMatchOrders kSCPropNetDNSSupplementalMatchOrders
#endif
	if (result) publish_dns_wins_entry(kSCEntNetProxies, kSCPropNetProxiesSupplementalMatchDomains, strname, 0, kSCPropNetProxiesSupplementalMatchOrders, num, 0, 0, clean);

done:
	if (num)
		CFRelease(num);
	if (str1)
		CFRelease(str1);
	if (str2)
		CFRelease(str2);
	if (strname)
		CFRelease(strname);

	return result;
}

/* -----------------------------------------------------------------------------
set wins information
----------------------------------------------------------------------------- */
int sifwins(u_int32_t wins1, u_int32_t wins2)
{    
#if !TARGET_OS_EMBEDDED
    CFStringRef		str1 = 0, str2 = 0;
    int				result = 0, clean = 1;

	if (wins1)
		str1 = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&wins1));
	if (!str1)
		goto done;
		
	if (wins2)
		str2 = CFStringCreateWithFormat(NULL, NULL, CFSTR(IP_FORMAT), IP_LIST(&wins2));
		
	result = publish_dns_wins_entry(kSCEntNetSMB, kSCPropNetSMBWINSAddresses, str1, str2, 0, 0, 0, 0, clean);

done:
	if (str1)
		CFRelease(str1);
	if (str2)
		CFRelease(str2);

	return result;
#else
	return 0;
#endif
}

static struct {
    struct rt_msghdr		hdr;
    struct sockaddr_inarp	dst;
    struct sockaddr_dl		hwa;
    char			extra[128];
} arpmsg;

static int arpmsg_valid;

/* -----------------------------------------------------------------------------
Make a proxy ARP entry for the peer
----------------------------------------------------------------------------- */
int sifproxyarp(int unit, u_int32_t hisaddr)
{
    int routes;

    /*
     * Get the hardware address of an interface on the same subnet
     * as our local address.
     */
    memset(&arpmsg, 0, sizeof(arpmsg));
    if (!get_ether_addr(hisaddr, &arpmsg.hwa)) {
	error("Cannot determine ethernet address for proxy ARP");
	return 0;
    }

    if ((routes = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
	error("Couldn't add proxy arp entry: socket: %m");
	return 0;
    }

    arpmsg.hdr.rtm_type = RTM_ADD;
    arpmsg.hdr.rtm_flags = RTF_ANNOUNCE | RTF_HOST | RTF_STATIC;
    arpmsg.hdr.rtm_version = RTM_VERSION;
    arpmsg.hdr.rtm_seq = ++rtm_seq;
    arpmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
    arpmsg.hdr.rtm_inits = RTV_EXPIRE;
    arpmsg.dst.sin_len = sizeof(struct sockaddr_inarp);
    arpmsg.dst.sin_family = AF_INET;
    arpmsg.dst.sin_addr.s_addr = hisaddr;
    arpmsg.dst.sin_other = SIN_PROXY;

    arpmsg.hdr.rtm_msglen = (char *) &arpmsg.hwa - (char *) &arpmsg
	+ arpmsg.hwa.sdl_len;
    if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
	error("Couldn't add proxy arp entry: %m");
	close(routes);
	return 0;
    }

    close(routes);
    arpmsg_valid = 1;
    proxy_arp_addr = hisaddr;
    return 1;
}

/* -----------------------------------------------------------------------------
Delete the proxy ARP entry for the peer
----------------------------------------------------------------------------- */
int cifproxyarp(int unit, u_int32_t hisaddr)
{
    int routes;

    if (!arpmsg_valid)
	return 0;
    arpmsg_valid = 0;

    arpmsg.hdr.rtm_type = RTM_DELETE;
    arpmsg.hdr.rtm_seq = ++rtm_seq;

    if ((routes = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
	error("Couldn't delete proxy arp entry: socket: %m");
	return 0;
    }

    if (write(routes, &arpmsg, arpmsg.hdr.rtm_msglen) < 0) {
	error("Couldn't delete proxy arp entry: %m");
	close(routes);
	return 0;
    }

    close(routes);
    proxy_arp_addr = 0;
    return 1;
}

#define MAX_IFS		32

/* -----------------------------------------------------------------------------
get the hardware address of an interface on the
 * the same subnet as ipaddr.
----------------------------------------------------------------------------- */
static int get_ether_addr(u_int32_t ipaddr, struct sockaddr_dl *hwaddr)
{
    struct ifreq *ifr, *ifend, *ifp;
    u_int32_t ina, mask;
    struct sockaddr_dl *dla;
    struct ifreq ifreq;
    struct ifconf ifc;
    struct ifreq ifs[MAX_IFS];

    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;
    if (ioctl(ip_sockfd, SIOCGIFCONF, &ifc) < 0) {
	error("ioctl(SIOCGIFCONF): %m");
	return 0;
    }

    /*
     * Scan through looking for an interface with an Internet
     * address on the same subnet as `ipaddr'.
     */
    ifend = ALIGNED_CAST(struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ifr = ALIGNED_CAST(struct ifreq *)
	 	((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len)) {
	if (ifr->ifr_addr.sa_family == AF_INET) {
	    ina = (ALIGNED_CAST(struct sockaddr_in *) &ifr->ifr_addr)->sin_addr.s_addr;
	    strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
	    /*
	     * Check that the interface is up, and not point-to-point
	     * or loopback.
	     */
	    if (ioctl(ip_sockfd, SIOCGIFFLAGS, &ifreq) < 0)
		continue;
	    if ((ifreq.ifr_flags &
		 (IFF_UP|IFF_BROADCAST|IFF_POINTOPOINT|IFF_LOOPBACK|IFF_NOARP))
		 != (IFF_UP|IFF_BROADCAST))
		continue;
	    /*
	     * Get its netmask and check that it's on the right subnet.
	     */
	    if (ioctl(ip_sockfd, SIOCGIFNETMASK, &ifreq) < 0)
		continue;
	    mask = (ALIGNED_CAST(struct sockaddr_in *) &ifreq.ifr_addr)->sin_addr.s_addr;
	    if ((ipaddr & mask) != (ina & mask))
		continue;

	    break;
	}
    }

    if (ifr >= ifend)
	return 0;
    info("found interface %s for proxy arp", ifr->ifr_name);

    /*
     * Now scan through again looking for a link-level address
     * for this interface.
     */
    ifp = ifr;
    for (ifr = ifc.ifc_req; ifr < ifend; ) {
	if (strcmp(ifp->ifr_name, ifr->ifr_name) == 0
	    && ifr->ifr_addr.sa_family == AF_LINK) {
	    /*
	     * Found the link-level address - copy it out
	     */
	    dla = ALIGNED_CAST(struct sockaddr_dl *) &ifr->ifr_addr;
	    BCOPY(dla, hwaddr, dla->sdl_len);
	    return 1;
	}
	ifr = ALIGNED_CAST(struct ifreq *) ((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len);
    }

    return 0;
}

/* -----------------------------------------------------------------------------
 * Return user specified netmask, modified by any mask we might determine
 * for address `addr' (in network byte order).
 * Here we scan through the system's list of interfaces, looking for
 * any non-point-to-point interfaces which might appear to be on the same
 * network as `addr'.  If we find any, we OR in their netmask to the
 * user-specified netmask.
----------------------------------------------------------------------------- */
u_int32_t GetMask(u_int32_t addr)
{
    u_int32_t mask, nmask, ina;
    struct ifreq *ifr, *ifend, ifreq;
    struct ifconf ifc;
    struct ifreq ifs[MAX_IFS];

    addr = ntohl(addr);
    if (IN_CLASSA(addr))	/* determine network mask for address class */
	nmask = IN_CLASSA_NET;
    else if (IN_CLASSB(addr))
	nmask = IN_CLASSB_NET;
    else
	nmask = IN_CLASSC_NET;
    /* class D nets are disallowed by bad_ip_adrs */
    mask = netmask | htonl(nmask);

    /*
     * Scan through the system's network interfaces.
     */
    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;
    if (ioctl(ip_sockfd, SIOCGIFCONF, &ifc) < 0) {
	warning("ioctl(SIOCGIFCONF): %m");
	return mask;
    }
    ifend = ALIGNED_CAST(struct ifreq *) (ifc.ifc_buf + ifc.ifc_len);
    for (ifr = ifc.ifc_req; ifr < ifend; ifr = ALIGNED_CAST(struct ifreq *)
	 	((char *)&ifr->ifr_addr + ifr->ifr_addr.sa_len)) {
	/*
	 * Check the interface's internet address.
	 */
	if (ifr->ifr_addr.sa_family != AF_INET)
	    continue;
	ina = (ALIGNED_CAST(struct sockaddr_in *) &ifr->ifr_addr)->sin_addr.s_addr;
	if ((ntohl(ina) & nmask) != (addr & nmask))
	    continue;
	/*
	 * Check that the interface is up, and not point-to-point or loopback.
	 */
	strlcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifreq.ifr_name));
	if (ioctl(ip_sockfd, SIOCGIFFLAGS, &ifreq) < 0)
	    continue;
	if ((ifreq.ifr_flags & (IFF_UP|IFF_POINTOPOINT|IFF_LOOPBACK))
	    != IFF_UP)
	    continue;
	/*
	 * Get its netmask and OR it into our mask.
	 */
	if (ioctl(ip_sockfd, SIOCGIFNETMASK, &ifreq) < 0)
	    continue;
	mask |= (ALIGNED_CAST(struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr.s_addr;
    }

    return mask;
}

/* -----------------------------------------------------------------------------
determine if the system has any route to
 * a given IP address.
 * For demand mode to work properly, we have to ignore routes
 * through our own interface.
 ----------------------------------------------------------------------------- */
int have_route_to(u_int32_t addr)
{
    return -1;
}

/* -----------------------------------------------------------------------------
Use the hostid as part of the random number seed
----------------------------------------------------------------------------- */
int get_host_seed()
{
    return gethostid();
}


#if 0

#define	LOCK_PREFIX	"/var/spool/lock/LCK.."

static char *lock_file;		/* name of lock file created */

/* -----------------------------------------------------------------------------
create a lock file for the named lock device
----------------------------------------------------------------------------- */
int lock(char *dev)
{
    char hdb_lock_buffer[12];
    int fd, pid, n;
    char *p;
    size_t l;

    if ((p = strrchr(dev, '/')) != NULL)
	dev = p + 1;
    l = strlen(LOCK_PREFIX) + strlen(dev) + 1;
    lock_file = malloc(l);
    if (lock_file == NULL)
	novm("lock file name");
    slprintf(lock_file, l, "%s%s", LOCK_PREFIX, dev);

    while ((fd = open(lock_file, O_EXCL | O_CREAT | O_RDWR, 0644)) < 0) {
	if (errno == EEXIST
	    && (fd = open(lock_file, O_RDONLY, 0)) >= 0) {
	    /* Read the lock file to find out who has the device locked */
	    n = read(fd, hdb_lock_buffer, 11);
	    if (n <= 0) {
		error("Can't read pid from lock file %s", lock_file);
		close(fd);
	    } else {
		hdb_lock_buffer[n] = 0;
		pid = atoi(hdb_lock_buffer);
		if (kill(pid, 0) == -1 && errno == ESRCH) {
		    /* pid no longer exists - remove the lock file */
		    if (unlink(lock_file) == 0) {
			close(fd);
			notice("Removed stale lock on %s (pid %d)",
			       dev, pid);
			continue;
		    } else
			warning("Couldn't remove stale lock on %s",
			       dev);
		} else
		    notice("Device %s is locked by pid %d",
			   dev, pid);
	    }
	    close(fd);
	} else
	    error("Can't create lock file %s: %m", lock_file);
	free(lock_file);
	lock_file = NULL;
	return -1;
    }

    slprintf(hdb_lock_buffer, sizeof(hdb_lock_buffer), "%10d\n", getpid());
    write(fd, hdb_lock_buffer, 11);

    close(fd);
    return 0;
}

/* -----------------------------------------------------------------------------
remove our lockfile
----------------------------------------------------------------------------- */
void unlock()
{
    if (lock_file) {
	unlink(lock_file);
	free(lock_file);
	lock_file = NULL;
    }
}
#endif

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int sys_loadplugin(char *arg)
{
    char		path[MAXPATHLEN];
    int 		ret = -1;
    CFBundleRef		bundle;
    CFURLRef		url;
    int 		(*start)(CFBundleRef);


    if (arg[0] == '/') {
        strlcpy(path, arg, sizeof(path));
    }
    else {
        strlcpy(path, "/System/Library/Extensions/", sizeof(path));
        strlcat(path, arg, sizeof(path));
    } 

    url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), TRUE);
    if (url) {        
        bundle =  CFBundleCreate(NULL, url);
        if (bundle) {

            if (CFBundleLoadExecutable(bundle)
                && (start = CFBundleGetFunctionPointerForName(bundle, CFSTR("start")))) {
            
                ret = (*start)(bundle);
            }
                        
            CFRelease(bundle);
        }
        CFRelease(url);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int sys_eaploadplugin(char *arg, eap_ext *eap)
{
    char		path[MAXPATHLEN];
    int 		ret = -1;
    CFBundleRef		bundle;
    CFURLRef		url;
    CFDictionaryRef	dict;

    if (arg[0] == '/') {
        strlcpy(path, arg, sizeof(path));
    }
    else {
        strlcpy(path, "/System/Library/Extensions/", sizeof(path));
        strlcat(path, arg, sizeof(path));
    } 

    url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, strlen(path), TRUE);
    if (url) {        

        dict = CFBundleCopyInfoDictionaryForURL(url);
        if (dict) {
            CFNumberRef	num;
            CFStringRef	str;
            int		i;
       
            bzero(eap, sizeof(eap_ext));
            num = CFDictionaryGetValue(dict, CFSTR("EAPType"));
            if (num && (CFGetTypeID(num) == CFNumberGetTypeID())) {
                CFNumberGetValue(num, kCFNumberSInt32Type, &i);
                eap->type = i;
            }

            str = CFDictionaryGetValue(dict, CFSTR("EAPName"));
            if (str && (CFGetTypeID(str) == CFStringGetTypeID())) {
                eap->name = malloc(CFStringGetLength(str) + 1);
                if (eap->name) 
                    CFStringGetCString((CFStringRef)str, eap->name, CFStringGetLength(str) + 1, kCFStringEncodingUTF8);
            }
            CFRelease(dict);

            bundle =  CFBundleCreate(NULL, url);
            if (bundle) {
    
                if (CFBundleLoadExecutable(bundle)) {
                
                    eap->init = CFBundleGetFunctionPointerForName(bundle, CFSTR("Init"));
                    eap->dispose = CFBundleGetFunctionPointerForName(bundle, CFSTR("Dispose"));
                    eap->process = CFBundleGetFunctionPointerForName(bundle, CFSTR("Process"));
                    eap->free = CFBundleGetFunctionPointerForName(bundle, CFSTR("Free"));
                    eap->attribute = CFBundleGetFunctionPointerForName(bundle, CFSTR("GetAttribute"));
                    eap->interactive_ui = CFBundleGetFunctionPointerForName(bundle, CFSTR("InteractiveUI"));
                    eap->print_packet = CFBundleGetFunctionPointerForName(bundle, CFSTR("PrintPacket"));
                    eap->identity = CFBundleGetFunctionPointerForName(bundle, CFSTR("Identity"));
    
                    // keep a ref to release later
                    eap->plugin = bundle;
    
                    ret = 0;
                }
                
                if (ret)
                    CFRelease(bundle);
            }
        }
        CFRelease(url);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
publish a dictionnary entry in the cache, given a key
----------------------------------------------------------------------------- */
int publish_keyentry(CFStringRef key, CFStringRef entry, CFTypeRef value)
{
    CFMutableDictionaryRef	dict;
    CFPropertyListRef		ref;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;
            
    if (ref = SCDynamicStoreCopyValue(cfgCache, key)) {
        dict = CFDictionaryCreateMutableCopy(0, 0, ref);
        CFRelease(ref);
    }
    else
        dict = CFDictionaryCreateMutable(0, 0, 
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    if (dict == 0)
        return 0;
    
    CFDictionarySetValue(dict,  entry, value);
	if (setstorevalue(key,dict) == 0)
        warning("publish_entry SCDSet() failed: %s\n", SCErrorString(SCError()));
    CFRelease(dict);
    
    return 1;
 }

/* -----------------------------------------------------------------------------
publish a numerical entry in the cache, given a dictionary
----------------------------------------------------------------------------- */
int publish_dictnumentry(CFStringRef dict, CFStringRef entry, int val)
{
    int			ret = ENOMEM;
    CFNumberRef		num;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceidRef, dict);
    if (key) {
        num = CFNumberCreate(NULL, kCFNumberIntType, &val);
        if (num) {
            ret = publish_keyentry(key, entry, num);
            CFRelease(num);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
publish a string entry in the cache, given a dictionary
----------------------------------------------------------------------------- */
int publish_dictstrentry(CFStringRef dict, CFStringRef entry, char *str, int encoding)
{
    
    int			ret = ENOMEM;
    CFStringRef 	ref;
    CFStringRef		key;
    
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceidRef, dict);
    if (key) {
        ref = CFStringCreateWithCString(NULL, str, encoding);
        if (ref) {
            ret = publish_keyentry(key, entry, ref);
            CFRelease(ref);
            ret = 0;
        }
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
 republish Dynamic store
 ----------------------------------------------------------------------------- */
static void
republish_dict(SCDynamicStoreRef store __unused, void *info  __unused)
{
	int i,count;
	const void	**keys = NULL;
	const void	**values = NULL;
	
    dbglog("DynamicStore Server has reconnected, republish keys");
	if (publish_dict == NULL)
		return;
	
	count = CFDictionaryGetCount(publish_dict);
	if ( count ){
		CFRelease(cfgCache);
		cfgCache = SCDynamicStoreCreate(0, CFSTR("pppd"), 0, 0);		/* create new ref with server */
		if (cfgCache == 0){
			fatal("republish_dict SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
			return;
		}
		
		keys = (const void * *)malloc(sizeof(keys) * count);
		values = (const void * *)malloc(sizeof(values) * count);
		
		if ((keys == NULL) || (values == NULL))
			goto done;
		
		CFDictionaryGetKeysAndValues(publish_dict, keys, values);
		dbglog("republish_dict: processing %d keys", count);
		for (i=0 ; i<count; i++){
			if (!SCDynamicStoreSetValue(cfgCache, keys[i], values[i])){
				warning("republish_dict SCDynamicStoreSetValue failed key %s: %s\n", CFStringGetCStringPtr(keys[i], kCFStringEncodingMacRoman), SCErrorString(SCError()));
			}
			
		}
	}
	
done:
	if (keys)
		free(keys);
	if (values)
		free(values);
	
}

/* -----------------------------------------------------------------------------
unpublish a dictionnary entry from the cache, given the dict key
----------------------------------------------------------------------------- */
int unpublish_keyentry(CFStringRef key, CFStringRef entry)
{
    CFMutableDictionaryRef	dict;
    CFPropertyListRef		ref;

    // ppp daemons without services are not published in the cache
    if (cfgCache == NULL)
        return 0;
        
    if (ref = SCDynamicStoreCopyValue(cfgCache, key)) {
        if (dict = CFDictionaryCreateMutableCopy(0, 0, ref)) {
            CFDictionaryRemoveValue(dict, entry);
			if (setstorevalue(key, dict) == 0)
                warning("unpublish_keyentry SCDSet() failed: %s\n", SCErrorString(SCError()));
            CFRelease(dict);
        }
        CFRelease(ref);
    }
    return 0;
}

/* -----------------------------------------------------------------------------
unpublish a complete dictionnary from the cache
----------------------------------------------------------------------------- */
int unpublish_dict(CFStringRef dict)
{
    int			ret = ENOMEM;
    CFStringRef		key;

    if (cfgCache == NULL)
        return 0;
            
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceidRef, dict);
    if (key) {
		if (publish_dict){
			CFDictionaryRemoveValue(publish_dict, key);
		}
        ret = !SCDynamicStoreRemoveValue(cfgCache, key);
        CFRelease(key);
    }
    return ret;
}

/* -----------------------------------------------------------------------------
unpublish a dictionnary entry from the cache, given the dict name
----------------------------------------------------------------------------- */
int unpublish_dictentry(CFStringRef dict, CFStringRef entry)
{
    int			ret = ENOMEM;
    CFStringRef		key;
    
    key = SCDynamicStoreKeyCreateNetworkServiceEntity(0, kSCDynamicStoreDomainState, serviceidRef, dict);
    if (key) {
        ret = unpublish_keyentry(key, entry);
        CFRelease(key);
        ret = 0;
    }
    return ret;
}

/* -----------------------------------------------------------------------------
get mach asbolute time, for timeout purpose independent of date changes
----------------------------------------------------------------------------- */
int getabsolutetime(struct timeval *timenow)
{
    double	now;

    if (timeScaleSeconds == 0) 
        return -1;
    
    now = mach_absolute_time();
    timenow->tv_sec = now * timeScaleSeconds;
    timenow->tv_usec =  (now * timeScaleMicroSeconds) - ((double)timenow->tv_sec * 1000000);
    return 0;
}

/* -----------------------------------------------------------------------------
our new phase hook
----------------------------------------------------------------------------- */
void sys_phasechange(void *arg, uintptr_t p)
{

    publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPStatus, p);

    switch (p) {
    
        case PHASE_ESTABLISH:
            connecttime = mach_absolute_time() * timeScaleSeconds;
            publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPConnectTime, connecttime);
            if (maxconnect)
                publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPDisconnectTime, connecttime + maxconnect);
            break;

        case PHASE_SERIALCONN:
            unpublish_dictentry(kSCEntNetPPP, kSCPropNetPPPRetryConnectTime);
            break;

        case PHASE_WAITONBUSY:
            publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPRetryConnectTime, redialtimer + (mach_absolute_time() * timeScaleSeconds));
            break;

        case PHASE_RUNNING:
            break;
            
        case PHASE_DORMANT:
        case PHASE_HOLDOFF:
        case PHASE_DEAD:
            sys_eventnotify((void*)PPP_EVT_DISCONNECTED, status);
            break;

        case PHASE_DISCONNECT:
            unpublish_dictentry(kSCEntNetPPP, CFSTR("AuthPeerName") /*kSCPropNetPPPAuthPeerName*/);
            unpublish_dictentry(kSCEntNetPPP, kSCPropNetPPPCommRemoteAddress);
            unpublish_dictentry(kSCEntNetPPP, kSCPropNetPPPConnectTime);
            unpublish_dictentry(kSCEntNetPPP, kSCPropNetPPPDisconnectTime);
            break;
    }

    /* send phase notification to the controller */
    if (phase != PHASE_DEAD)
		sys_notify(PPPD_PHASE, phase, 0);
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void sys_authpeersuccessnotify(void *param, uintptr_t info)
{
    struct auth_peer_success_info *peerinfo = (struct auth_peer_success_info *)info;

    publish_dictstrentry(kSCEntNetPPP, CFSTR("AuthPeerName") /*kSCPropNetPPPAuthPeerName*/, peerinfo->name, kCFStringEncodingUTF8);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void sys_timeremaining(void *param, uintptr_t info)
{
    struct lcp_timeremaining_info *timeinfo = (struct lcp_timeremaining_info *)info;
    u_int32_t	val = 0, curtime = mach_absolute_time() * timeScaleSeconds;
    
    if (timeinfo->time == 0xFFFFFFFF) {
        // infinite timer from the server, restore our maxconnect time if any
        if (maxconnect)
            val = connecttime + maxconnect;
    }
    else {
        // valid timer from the server, the disconnect time will be the min between
        // our setup and what says the server.
        if (maxconnect)
            val = MIN(curtime + timeinfo->time, connecttime + maxconnect);
        else
            val = curtime + timeinfo->time;
    }
    
    if (val)
        publish_dictnumentry(kSCEntNetPPP, kSCPropNetPPPDisconnectTime, val);
    else
        unpublish_dictentry(kSCEntNetPPP, kSCPropNetPPPDisconnectTime);

}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void sys_publish_remoteaddress(char *addr)
{
    
    if (addr)
        publish_dictstrentry(kSCEntNetPPP, kSCPropNetPPPCommRemoteAddress, addr, kCFStringEncodingUTF8);
}

/* -----------------------------------------------------------------------------
our pid has changed, reinit things
----------------------------------------------------------------------------- */
void sys_reinit()
{
    
    cfgCache = SCDynamicStoreCreate(0, CFSTR("pppd"), 0, 0);
    if (cfgCache == 0)
        fatal("SCDynamicStoreCreate failed: %s", SCErrorString(SCError()));
    
    publish_dictnumentry(kSCEntNetPPP, CFSTR("pid"), getpid());
}

/* ----------------------------------------------------------------------------- 
we are exiting
----------------------------------------------------------------------------- */
void sys_exitnotify(void *arg, uintptr_t exitcode)
{
   
    // unpublish the various info about the connection
    unpublish_dict(kSCEntNetPPP);
    unpublish_dict(kSCEntNetDNS);
    unpublish_dict(kSCEntNetProxies);
#if !TARGET_OS_EMBEDDED
    unpublish_dict(kSCEntNetSMB);
#endif
    unpublish_dict(kSCEntNetInterface);

    sys_eventnotify((void*)PPP_EVT_DISCONNECTED, exitcode);

    if (cfgCache) {
        CFRelease(cfgCache);
        cfgCache = 0;
    }
	
	if (publish_dict){
		CFRelease(publish_dict);
		publish_dict = NULL;
	}
}

/* -----------------------------------------------------------------------------
add/remove a route via an interface
----------------------------------------------------------------------------- */
int
route_interface(int cmd, struct in_addr host, struct in_addr addr_mask, char iftype, char *ifname, int is_host)
{
    int 			len, iflen;
    int 			rtm_seq = 0;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_dl	iface;
        struct sockaddr_in	mask;
    } rtmsg;
    
	char            host_str[INET_ADDRSTRLEN];
	char            mask_str[INET_ADDRSTRLEN];
    int 			sockfd = -1;

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
		error("route_interface: open routing socket failed, %m. (address %s, mask %s, interface %s, host %d).",
			  addr2ascii(AF_INET, &host, sizeof(host), host_str),
			  addr2ascii(AF_INET, &addr_mask, sizeof(addr_mask), mask_str),
			  ifname,
			  is_host);
		return (0);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
    if (is_host)
        rtmsg.hdr.rtm_flags |= RTF_HOST;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_GATEWAY;
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.dst.sin_addr = host;
    iflen = MIN(strlen(ifname), sizeof(rtmsg.iface.sdl_data));
    rtmsg.iface.sdl_len = sizeof(rtmsg.iface);
    rtmsg.iface.sdl_family = AF_LINK;
    rtmsg.iface.sdl_type = iftype;
    rtmsg.iface.sdl_nlen = iflen;
    strncpy(rtmsg.iface.sdl_data, ifname, iflen);
    
    // if addr_mask non-zero add it
    if (addr_mask.s_addr != 0) {
        rtmsg.hdr.rtm_addrs |= RTA_NETMASK;
        rtmsg.mask.sin_len = sizeof(rtmsg.mask);
        rtmsg.mask.sin_family = AF_INET;
        rtmsg.mask.sin_addr = addr_mask;
    }

    len = sizeof(rtmsg);
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
		syslog((cmd == RTM_DELETE)? LOG_DEBUG : LOG_ERR, "route_interface: write routing socket failed, %s. (address %s, mask %s, interface %s, host %d).",
			   strerror(errno),
			  addr2ascii(AF_INET, &host, sizeof(host), host_str),
			  addr2ascii(AF_INET, &addr_mask, sizeof(addr_mask), mask_str),
			  ifname,
			  is_host);
		close(sockfd);
		return (0);
    }

    close(sockfd);
    return (1);
}

/* -----------------------------------------------------------------------------
    add/remove a route via a gateway
----------------------------------------------------------------------------- */
int
route_gateway(int cmd, struct in_addr dest, struct in_addr mask, struct in_addr gateway, int use_gway_flag)
{
	char            dest_str[INET_ADDRSTRLEN];
	char            mask_str[INET_ADDRSTRLEN];
	char            gateway_str[INET_ADDRSTRLEN];
    int 			len;
    int 			rtm_seq = 0;

    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
        struct sockaddr_in	gway;
        struct sockaddr_in	mask;
    } rtmsg;
   
    int 			sockfd = -1;
    
    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
		syslog(LOG_INFO, "host_gateway: open routing socket failed, %s. (address %s, mask %s, gateway %s, use-gateway %d).",
			   strerror(errno),
			   addr2ascii(AF_INET, &dest, sizeof(dest), dest_str),
			   addr2ascii(AF_INET, &mask, sizeof(mask), mask_str),
			   addr2ascii(AF_INET, &gateway, sizeof(gateway), gateway_str),
			   use_gway_flag);
		return (0);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
    if (use_gway_flag)
        rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK | RTA_GATEWAY;
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.dst.sin_addr = dest;
    rtmsg.gway.sin_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin_family = AF_INET;
    rtmsg.gway.sin_addr = gateway;
    rtmsg.mask.sin_len = sizeof(rtmsg.mask);
    rtmsg.mask.sin_family = AF_INET;
    rtmsg.mask.sin_addr = mask;

    len = sizeof(rtmsg);
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
		syslog((cmd == RTM_DELETE)? LOG_DEBUG : LOG_ERR, "host_gateway: write routing socket failed, %s. (address %s, mask %s, gateway %s, use-gateway %d).",
			   strerror(errno),
			   addr2ascii(AF_INET, &dest, sizeof(dest), dest_str),
			   addr2ascii(AF_INET, &mask, sizeof(mask), mask_str),
			   addr2ascii(AF_INET, &gateway, sizeof(gateway), gateway_str),
			   use_gway_flag);
		close(sockfd);
		return (0);
    }

    close(sockfd);
    return (1);
}

// Here stealing the in_cksum function which computes checksum from original ping.
static int
in_cksum(u_short *addr, int len)
{
	register int nleft = len;
	register u_short *w = addr;
	register int sum = 0;
	u_short answer = 0;
	
	// Our algorithm is simple, using a 32 bit accumulator (sum), we add
	// sequential 16 bit words to it, and at the end, fold back all the
	// carry bits from the top 16 bits into the lower 16 bits.
	
	while (nleft > 1) {
		sum += *w++;
		nleft -= 2;
	}
	
	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w;
		sum += answer;
	}
	
	// Add back carry outs from top 16 bits to low 16 bits
	sum = (sum >> 16) + (sum & 0xffff);  // Add hi 16 to low 16
	sum += (sum >> 16);                  // Add carry
	answer = ~sum;                       // Truncate to 16 bits
	
	return answer;
}

static int
ppp_scoped_ping (int                 s,
				 unsigned int        ifscope,
				 struct sockaddr_in *dst,
				 int                 ntransmit)
{
	int          i;
	int          hold;
	struct icmp *icp;
	u_char      *packet;
	u_char       outpack[IP_MAXPACKET] __attribute__ ((aligned(4)));		// Wcast-align fix - force alignment

	if (s < 0) {
		s = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
		if (s < 0) {
			return -1;
		}
		if (ifscope) {
			if (setsockopt(s, IPPROTO_IP, IP_BOUND_IF, (char *)&ifscope, sizeof(ifscope)) != 0) {
				close(s);
				return -1;
			}
		}
		
		hold = IP_MAXPACKET + 128;
		(void)setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&hold, sizeof(hold));
	}
	
	packet = outpack;
	icp = ALIGNED_CAST(struct icmp *)outpack;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_cksum = 0;
	icp->icmp_seq = htons(ntransmit);
	icp->icmp_id = getpid() & 0xFFFF;
	icp->icmp_cksum = in_cksum((u_short *)icp, ICMP_MINLEN);
	if ((i = sendto(s, (char *)packet, ICMP_MINLEN, 0, (struct sockaddr *)dst, sizeof(*dst))) != ICMP_MINLEN) {
		close(s);
		return -1;
	}
	return s;
}

int
ppp_ip_probe_send (void *arg)
{
	int scope;
	int i = 0;

	dbglog("%s: starting", __FUNCTION__);

	if (!session || !session->valid) {
		return -1;
	}

	scope = if_nametoindex(session->interface_name);

	session->probe_success = 0;

	// first probe to goog-dns
	if ((session->probe_addrs[GOOG_DNS_PROBE].sin_family == AF_INET) &&
		session->probe_addrs[GOOG_DNS_PROBE].sin_addr.s_addr) {
		dbglog("%s: found goog-dns address", __FUNCTION__);		
		if ((session->probe_fds[GOOG_DNS_PROBE] = ppp_scoped_ping(session->probe_fds[GOOG_DNS_PROBE],
																  scope,
																  &session->probe_addrs[GOOG_DNS_PROBE],
																  session->probe_ntransmit)) != -1) {
			add_fd(session->probe_fds[GOOG_DNS_PROBE]);
			dbglog("%s: sent to goog-dns over scope %d", __FUNCTION__, scope);
			i++;
		}
	} else {
		info("%s: no goog-dns address", __FUNCTION__);		
	}
	// next probes to server and alternate server
	if ((session->probe_addrs[PEER_ADDR_PROBE].sin_family == AF_INET) &&
		session->probe_addrs[PEER_ADDR_PROBE].sin_addr.s_addr) {
		dbglog("%s: found peer address", __FUNCTION__);		
		// current vpn server
		if ((session->probe_fds[PEER_ADDR_PROBE] = ppp_scoped_ping(session->probe_fds[PEER_ADDR_PROBE],
																   scope,
																   &session->probe_addrs[PEER_ADDR_PROBE],
																   session->probe_ntransmit)) != -1) {
			add_fd(session->probe_fds[PEER_ADDR_PROBE]);
			dbglog("%s: sent to peer over scope %d", __FUNCTION__, scope);
			i++;
		}
		
		if ((session->probe_addrs[ALT_PEER_ADDR_PROBE].sin_family == AF_INET) &&
			session->probe_addrs[ALT_PEER_ADDR_PROBE].sin_addr.s_addr) {
			dbglog("%s: found alternate peer address", __FUNCTION__);		
			// alternate vpn server
			if ((session->probe_fds[ALT_PEER_ADDR_PROBE] = ppp_scoped_ping(session->probe_fds[ALT_PEER_ADDR_PROBE],
																		   scope,
																		   &session->probe_addrs[ALT_PEER_ADDR_PROBE],
																		   session->probe_ntransmit)) != -1) {
				add_fd(session->probe_fds[ALT_PEER_ADDR_PROBE]);
				info("%s: sent to alternate peer over scope %d", __FUNCTION__, scope);
				i++;
			}
		} else {
			dbglog("%s: no alternate peer address", __FUNCTION__);		
		}
	} else {
		dbglog("%s: no peer address", __FUNCTION__);		
	}
	if (i) {
		dbglog("%s: %d probes sent", __FUNCTION__, i);
		session->probe_tries++;
		if (!session->probe_timer_running) {
			session->probe_timer_running = 1;
			TIMEOUT(ppp_ip_probe_timeout, 0, 3);
		}
		return 0;
	}
	return -1;
}

int
ppp_ip_probe_stop (void *arg)
{
	int i;

	if (!session || !session->valid) {
		return -1;
	}

	if (session->probe_timer_running) {
		session->probe_timer_running = 0;
		UNTIMEOUT(ppp_ip_probe_timeout, 0);
		dbglog("ppp_auxiliary_probe stopped");
	}

	for (i = 0; i < MAX_PROBE_ADDRS; i++) {
		if (session->probe_fds[i] > 0) {
			remove_fd(session->probe_fds[i]);
			close(session->probe_fds[i]);
			session->probe_fds[i] = -1;
		}
	}
	session->probe_tries = 0;
	session->probe_success = 0;
	return 0;
}

static void
ppp_ip_probe_timeout (void *arg)
{
	if (!session || !session->valid) {
		return;
	}
	
	if (session->probe_tries < 15) {
		if (!ppp_ip_probe_send(arg)) {
			return;
		}
	}
	error("ppp_auxiliary_probe timed out");
}

void ppp_session_clear (ppp_session_t *sess)
{
	int i;

	if (sess) {
		bzero(sess, sizeof(*sess));
		for (i = 0; i < MAX_PROBE_ADDRS; i++) {
			sess->probe_fds[i] = -1;
		}
	}
}

int
ppp_variable_echo_is_off (void)
{
	if (!session || !session->valid) {
		return 1;
	}
	return(!wait_underlying_interface_up);
}

void
ppp_variable_echo_start (void)
{
	if (!session || !session->valid) {
		return;
	}
    if (wait_underlying_interface_up || wait_port_mapping_changed) {
        // speed up LCP echos
        if (!lcp_echos_hastened) {
			dbglog("ppp_variable_echo_start");
            lcp_echo_interval_slow = lcp_echo_interval;
            lcp_echo_interval = 1;
            lcp_echo_fails_slow = lcp_echo_fails;
            if (lcp_echo_fails > 10) {
                lcp_echo_fails = 10; // max of 10 secs
            }
            lcp_echos_hastened = 1;
            lcp_echo_restart(0);
        }
    }
}

void
ppp_variable_echo_stop (void)
{
	if (!session || !session->valid) {
		return;
	}
    if (wait_underlying_interface_up || wait_port_mapping_changed) {
        dbglog("received echo-reply, ppp_variable_echo_stop!");
        wait_underlying_interface_up = 0;
        wait_port_mapping_changed = 0;
        if (lcp_echos_hastened) {
            lcp_echo_interval = lcp_echo_interval_slow;
            lcp_echo_fails = lcp_echo_fails_slow;
            lcp_echos_hastened = 0;
        }
    }
}

void ppp_auxiliary_probe_ip_up(void *arg, uintptr_t p)
{
	ppp_auxiliary_probe_ip = 1;
}

void ppp_auxiliary_probe_ip_down(void *arg, uintptr_t p)
{
	ppp_auxiliary_probe_ip = 0;
}

void
ppp_auxiliary_probe_init (void)
{
	ppp_auxiliary_probe_echos_pending = 0;
	ppp_auxiliary_probe_success = 0;
	if (!ppp_auxiliary_probe_ip_notify_init) {
		add_notifier(&ip_up_notify, ppp_auxiliary_probe_ip_up, 0);
		add_notifier(&ip_down_notify, ppp_auxiliary_probe_ip_down, 0);
		ppp_auxiliary_probe_ip_notify_init= 1;
	}
}

void
ppp_auxiliary_probe_stop (void)
{
	ppp_ip_probe_stop(session);
	ppp_auxiliary_probe_echos_pending = 0;
	ppp_auxiliary_probe_success = 0;
}

void
ppp_auxiliary_probe_check (int                    echos_pending,
							probe_disconnect_func  disconnect_func,
							fsm                   *f)
{
	if (ppp_auxiliary_probe_ip &&
		!wait_underlying_interface_up &&
		!wait_port_mapping_changed &&    
		echos_pending > 1){
		// more than 2 echo responses pending: try probing the network
		if (!ppp_auxiliary_probe_echos_pending) {
			// probe hasn't been started
			error("no echo-reply, start ppp_auxiliary_probe!");
			ppp_ip_probe_send(session);
			ppp_auxiliary_probe_echos_pending = 1;
			ppp_auxiliary_probe_success = 0;
		} else if (++ppp_auxiliary_probe_echos_pending > 1 &&
				   ppp_auxiliary_probe_success) {
			// we have more than 3 pending echo responses, while the network
			// probe succeeded... disconnect
			error("no echo-reply, despite successful ppp_auxiliary_probe!");
			// network probe succeeded but no echo replies, disconnect
			if (disconnect_func) {
				disconnect_func(f);
			}
		}
	}	
}

void
ppp_process_auxiliary_probe_input (void)
{
	int i, j, result;

	if (!session || !session->valid) {
		return;
	}

	for (i = 0, j = 0; i < MAX_PROBE_ADDRS; i++) {
		if (session->probe_fds[i] > 0 && is_ready_fd(session->probe_fds[i])) {
			result = 0;
			read(session->probe_fds[i], &result, 1);
			remove_fd(session->probe_fds[i]);
			if (result > 0) {
				// assume success for now. TODO: log and ignore alt-peer success
				session->probe_success++;
				j++;
				dbglog("ppp_auxiliary_probe[%d] response!", i);
			}
			close(session->probe_fds[i]);
			session->probe_fds[i] = -1;
		}
	}
	if (j) {
		if (session->probe_timer_running) {
			session->probe_timer_running = 0;
			UNTIMEOUT(ppp_ip_probe_timeout, 0);
		}
		if (ppp_auxiliary_probe_echos_pending) {
			ppp_auxiliary_probe_success++;
		}
	}
}

#if !TARGET_OS_EMBEDDED
static void
ppp_clear_one_nat_port_mapping (mdns_nat_mapping_t *mapping)
{
	if (mapping) {
		// what about mapping->mDNSRef_fd?
		if (mapping->mDNSRef != NULL) {
			if (mapping->mDNSRef_fd)
				remove_fd(mapping->mDNSRef_fd);
			DNSServiceRefDeallocate(mapping->mDNSRef);
		}
		bzero(mapping, sizeof(*mapping));
	}
}

static int
ppp_ignore_nat_port_mapping_update (DNSServiceRef       sdRef,
									char               *sd_name,
									char               *if_name,
									uint32_t            if_name_siz,
									uint32_t            interfaceIndex,
									uint32_t            publicAddress,
									DNSServiceProtocol  protocol,
									uint16_t            privatePort,
									uint16_t            publicPort)
{
	int i = 0, vpn = 0, found = 0;
	struct ifaddrs *ifap = NULL;
	char interfaceName[32];

	/* check if address still exist */
	if (getifaddrs(&ifap) == 0) {
		struct ifaddrs *ifa;
		for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
			if (ifa->ifa_name  
				&& ifa->ifa_addr
				&& (!strncmp(ifa->ifa_name, "utun", 4) || !strncmp(ifa->ifa_name, "ppp", 3))
				&& ifa->ifa_addr->sa_family == AF_INET
				&& ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == htonl(publicAddress)) {
				notice("%s port-mapping update for %s ignored: related to VPN interface. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n",
					   sd_name,
					   ifa->ifa_name,
					   publicAddress,
					   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
					   privatePort,
					   publicPort);
				vpn = 1;
			}
			if (ifa->ifa_name  
				&& ifa->ifa_addr
				&& !strncmp(ifa->ifa_name, if_name, if_name_siz)
				&& ifa->ifa_addr->sa_family == AF_INET
				&& ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == session->interface_address.s_addr) {
				found = 1;
			}
		}
		freeifaddrs(ifap);
	} else {
		error("%s port-mapping update for %s ignored: failed to get interface list",
			  sd_name,
			  if_name);
		return 1;
	}
	
	if (vpn) {
		return 1;
	}
	bzero(interfaceName, sizeof(interfaceName));
	if_indextoname(interfaceIndex, (char *)interfaceName);
	if (!strncmp(interfaceName, if_name, if_name_siz)) {
		if (strstr(if_name, "ppp") || strstr(if_name, "utun")) {
			// change on PPP interface... we don't care
			notice("%s port-mapping update for %s ignored: underlying interface is PPP/VPN. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n",
				   sd_name,
				   if_name,
				   publicAddress,
				   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
				   privatePort,
				   publicPort);
			return 1;
		} else {
			/* check if address still exist */
			if (found) {
				return 0;
			} else {
				if (!publicAddress || (!publicPort && privatePort)) {
					notice("%s port-mapping update for %s ignored: underlying interface down. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n",
						   sd_name,
						   if_name,
						   publicAddress,
						   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
						   privatePort,
						   publicPort);
					for (i = 0; i < session->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
						if (session->nat_mapping[i].mDNSRef_tmp == sdRef) {
							session->nat_mapping[i].up = 0;
							notice("%s port-mapping for %s flagged down because of no connectivity\n", sd_name, if_name);
						}
					}
					return 1;
				}
				notice("%s port-mapping update for %s ignored: underlying interface's address changed. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n",
					   sd_name,
					   if_name,
					   publicAddress,
					   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
					   privatePort,
					   publicPort);
				return 1;
			}
		}
	} else if (publicAddress && !publicPort && !privatePort && !protocol) {
		// an update on the NAT's public IP
		return 0;
	} else {
		// public interface disappeared?
		if (!publicAddress && !publicPort && !privatePort && !protocol) {
			return 0;
		}
		/* check if address still exist */
		if (session->interface_address.s_addr == htonl(publicAddress) && found) {
			return 0;
		}
		// change due to another interface, ignore for now
		notice("%s port-mapping update for %s ignored: not for interface %s. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n",
			   sd_name,
			   interfaceName,
			   if_name,
			   publicAddress,
			   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
			   privatePort,
			   publicPort);
		return 1;
	}
}
#endif

static void
ppp_public_nat_port_mapping_timeout (void *arg)
{
	if (!session || !session->valid) {
		return;
	}

	if (session->nat_mapping_timer_running) {
		session->nat_mapping_timer_running = 0;
		syslog(LOG_ERR, "NAT's public interface down for more than %d secs... starting faster probe.\n",
			   PUBLIC_NAT_PORT_MAPPING_TIMEOUT);
		// our public interface didn't come back... start faster probe
		if (session->failure_func) {
			wait_port_mapping_changed = 1;
			session->failure_func();
		}
	}
}

void
ppp_start_public_nat_port_mapping_timer (void)
{
	if (!session || !session->valid) {
		return;
	}

	if (session->nat_mapping_timer_blocked) {
		return;
	}

	if (session->nat_mapping_timer_running) {
		return;
	}

	notice("starting wait-port-mapping timer for %s: %d secs", session->sd_name, PUBLIC_NAT_PORT_MAPPING_TIMEOUT);
	TIMEOUT (ppp_public_nat_port_mapping_timeout, 0, PUBLIC_NAT_PORT_MAPPING_TIMEOUT);
	session->nat_mapping_timer_running = 1;
}

void
ppp_stop_public_nat_port_mapping_timer (void)
{
	if (!session || !session->valid) {
		return;
	}

	if (session->nat_mapping_timer_running) {
		UNTIMEOUT (ppp_public_nat_port_mapping_timeout, 0);
		session->nat_mapping_timer_running = 0;
	}
}

void
ppp_block_public_nat_port_mapping_timer (void)
{
	if (!session || !session->valid) {
		return;
	}

	ppp_stop_public_nat_port_mapping_timer();
	session->nat_mapping_timer_blocked = 1;
}

void
ppp_unblock_public_nat_port_mapping_timer (void)
{
	if (!session || !session->valid) {
		return;
	}
	
	session->nat_mapping_timer_blocked = 0;
}

#if !TARGET_OS_EMBEDDED

static void ppp_clear_nat_port_mapping(void);

static void
ppp_set_nat_port_mapping_callback (DNSServiceRef        sdRef,
								   DNSServiceFlags      flags,
								   uint32_t             interfaceIndex,
								   DNSServiceErrorType  errorCode,
								   uint32_t             nPublicAddress,	   /* four byte IPv4 address in network byte order */
								   DNSServiceProtocol   protocol,
								   uint16_t             nPrivatePort,
								   uint16_t             nPublicPort,	   /* may be different than the requested port */
								   uint32_t             ttl,			   /* may be different than the requested ttl */
								   void                *context)
{
	uint32_t publicAddress = ntohl(nPublicAddress);
	uint16_t privatePort = ntohs(nPrivatePort);
	uint16_t publicPort = ntohs(nPublicPort);
	int i;
	char *sd_name = session->sd_name;
	int is_connected = session->valid;
	char *if_name = session->interface_name;
	uint32_t if_name_siz = session->interface_name_siz;

	if (!session || !session->valid) {
		return;
	}

	if (override_primary) {
		info("%s port-mapping update for %s ignored: VPN is the Primary interface. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n",
			 sd_name,
			 if_name,
			 publicAddress,
			 (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
			 privatePort,
			 publicPort);
		ppp_clear_nat_port_mapping();
		return;
	}

	if (errorCode != kDNSServiceErr_NoError && errorCode != kDNSServiceErr_DoubleNAT) {
		error("%s failed to set port-mapping for %s, errorCode: %d\n", sd_name, if_name, errorCode);
		if (errorCode == kDNSServiceErr_NATPortMappingUnsupported || errorCode == kDNSServiceErr_NATPortMappingDisabled) {
			for (i = 0; i < session->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
				if (session->nat_mapping[i].mDNSRef_tmp == sdRef) {
					error("%s port-mapping for %s became invalid. is Connected: %d, Protocol: %s, Private Port: %d, Previous publicAddress: (%x), Previous publicPort: (%d)\n",
						  sd_name,
						  if_name,
						  is_connected,
						  (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
						  privatePort,
						  session->nat_mapping[i].reflexive.addr,
						  session->nat_mapping[i].reflexive.port);
					if (session->nat_mapping[i].up && is_connected && session->failure_func) {
						notice("%s public port-mapping for %s changed... starting faster probe.\n", sd_name, if_name);
						wait_port_mapping_changed = 1;
						session->failure_func();
					} else {
						ppp_clear_nat_port_mapping();
					}
					return;
				}
			}
		} else if (errorCode == kDNSServiceErr_ServiceNotRunning) {
			ppp_clear_nat_port_mapping();
		}
		return;
	}
	
	if (ppp_ignore_nat_port_mapping_update(sdRef, sd_name, if_name, if_name_siz, interfaceIndex, publicAddress, protocol, privatePort, publicPort)) {
		return;
	}
	
	info("%s port-mapping for %s, interfaceIndex: %d, Protocol: %s, Private Port: %d, Public Address: %x, Public Port: %d, TTL: %d%s\n",
		 sd_name,
		 if_name,
		 interfaceIndex,
		 (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
		 privatePort,
		 publicAddress,
		 publicPort,
		 ttl,
		 (errorCode == kDNSServiceErr_DoubleNAT)? " (Double NAT)" : ".");
	
	// generate a disconnect if we were already connected and we just detected a change in publicAddress or publicPort
	for (i = 0; i < session->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
		if (session->nat_mapping[i].mDNSRef_tmp == sdRef) {
			if (session->nat_mapping[i].up && !publicAddress && !publicPort) {
				notice("%s port-mapping for %s indicates public interface down. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n",
					   sd_name,
					   if_name,
					   publicAddress,
					   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
					   privatePort,
					   publicPort);
				// start timer
				ppp_start_public_nat_port_mapping_timer();
				return;
			} else if (session->nat_mapping[i].up) {
				ppp_stop_public_nat_port_mapping_timer();
			}
			
			if (session->interface_address.s_addr == htonl(publicAddress) && privatePort == publicPort) {
				notice("%s port-mapping update for %s indicates no NAT. Public Address: %x, Protocol: %s, Private Port: %d, Public Port: %d\n",
					   sd_name,
					   if_name,
					   publicAddress,
					   (protocol == 0)? "None":((protocol == kDNSServiceProtocol_UDP)? "UDP":"TCP"),
					   privatePort,
					   publicPort);
			}
			
			if (session->nat_mapping[i].interfaceIndex == interfaceIndex &&
				session->nat_mapping[i].protocol == protocol &&
				session->nat_mapping[i].privatePort == privatePort) {
				info("%s port-mapping for %s consistent. is Connected: %d, interface: %d, protocol: %d, privatePort: %d\n",
					 sd_name, if_name, is_connected, interfaceIndex, protocol, privatePort);
			} else {
				// inconsistency (mostly because of mdns api only works for the primary interface): TODO revise
				if (session->nat_mapping[i].interfaceIndex != interfaceIndex) {
					info("%s port-mapping for %s inconsistent. is Connected: %d, Previous interface: %d, Current interface %d\n",
						 sd_name, if_name, is_connected, session->nat_mapping[i].interfaceIndex, interfaceIndex);
					session->nat_mapping[i].interfaceIndex = interfaceIndex;
				}
				if (session->nat_mapping[i].protocol != protocol) {
					info("%s port-mapping for %s inconsistent. is Connected: %d, Previous protocol: %x, Current protocol %x\n",
						 sd_name, if_name, is_connected, session->nat_mapping[i].protocol, protocol);
					session->nat_mapping[i].protocol = protocol;
				}
				if (session->nat_mapping[i].privatePort != privatePort) {
					info("%s port-mapping for %s inconsistent. is Connected: %d, Previous privatePort: %d, Current privatePort %d\n",
						 sd_name, if_name, session->nat_mapping[i].privatePort, privatePort);
					session->nat_mapping[i].privatePort = privatePort;
				}
			}
			// is mapping up?
			if (!session->nat_mapping[i].up) {
				if (session->nat_mapping[i].reflexive.addr != publicAddress) {
					info("%s port-mapping for %s initialized. is Connected: %d, Previous publicAddress: (%d), Current publicAddress %x\n",
						 sd_name, if_name, is_connected, session->nat_mapping[i].reflexive.addr, publicAddress);
					session->nat_mapping[i].reflexive.addr = publicAddress;
				}
				if (session->nat_mapping[i].reflexive.port != publicPort) {
					info("%s port-mapping for %s initialized. is Connected: %d, Previous publicPort: (%d), Current publicPort %d\n",
						 sd_name, if_name, is_connected, session->nat_mapping[i].reflexive.port, publicPort);
					session->nat_mapping[i].reflexive.port = publicPort;
				}
				if (session->nat_mapping[i].reflexive.addr &&
				    ((!privatePort && !session->nat_mapping[i].reflexive.port) || (session->nat_mapping[i].reflexive.port))) {
					// flag up mapping
					session->nat_mapping[i].up = 1;
					info("%s port-mapping for %s fully initialized. Flagging up\n", sd_name, if_name);
				}
				return;
			}
			
			if (session->nat_mapping[i].reflexive.addr != publicAddress) {
				error("%s port-mapping for %s changed. is Connected: %d, Previous publicAddress: (%x), Current publicAddress %x\n",
					  sd_name, if_name, is_connected, session->nat_mapping[i].reflexive.addr, publicAddress);
				if (is_connected) {
					if (!privatePort || publicAddress) {
						syslog(LOG_ERR, "NAT's public address down or changed... starting faster probe.\n");
						if (session->failure_func) {
                            wait_port_mapping_changed = 1;
							session->failure_func();
						}
						return;
					}
					// let function that handles (KEV_INET_NEW_ADDR || KEV_INET_CHANGED_ADDR || KEV_INET_ADDR_DELETED) deal with the connection
					return;
				}
				session->nat_mapping[i].reflexive.addr = publicAddress;
				return;
			}
			if (session->nat_mapping[i].reflexive.port != publicPort) {
				error("%s port-mapping for %s changed. is Connected: %d, Previous publicPort: (%d), Current publicPort %d\n",
					  sd_name, if_name, is_connected, session->nat_mapping[i].reflexive.port, publicPort);
				if (is_connected) {
					if (!privatePort || publicPort) {
						syslog(LOG_ERR, "NAT's public port down or changed... starting faster probe.\n");
						if (session->failure_func) {
							wait_port_mapping_changed = 1;
							session->failure_func();
						}
						return;
					}
					// let function that handles (KEV_INET_NEW_ADDR || KEV_INET_CHANGED_ADDR || KEV_INET_ADDR_DELETED) deal with the connection
					return;
				}
				session->nat_mapping[i].reflexive.port = publicPort;
				return;
			}
			if (errorCode == kDNSServiceErr_DoubleNAT) {
				error("%s port-mapping for %s hasn't changed, however there's a Double NAT.  is Connected: %d\n",
					  sd_name, if_name, is_connected);
			}
			return;
		}
	}
	return;
}

static void
ppp_clear_nat_port_mapping (void)
{
	int i;

	if (!session || !session->valid)
		return;

	info("%s clearing port-mapping for %s\n", session->sd_name, session->interface_name);

	ppp_stop_public_nat_port_mapping_timer();

	for (i = 0; i < session->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
		ppp_clear_one_nat_port_mapping(&session->nat_mapping[i]);
	}
	session->nat_mapping_cnt = 0;
}

static int
ppp_set_nat_port_mapping (mdns_nat_mapping_t *mapping,
						  DNSServiceProtocol  protocol,
						  uint16_t            privatePort,
						  int                 probe_only)
{
	DNSServiceErrorType  err = kDNSServiceErr_NoError;
	uint16_t             publicPort;
	uint32_t             ttl;
	char                *sd_name = session->sd_name;
	char                *if_name = session->interface_name;
	uint32_t             interfaceIndex = if_nametoindex(session->interface_name);

	if (!probe_only) {
		publicPort = 0;
		ttl = 2 * 3600; // 2 hours
	} else {
		publicPort = 0;
		ttl = 0;
		protocol = 0;
		privatePort = 0;
	}

	if (mapping == NULL) {
		error("%s invalid mapping pointer for %s\n", sd_name, if_name);
		return -1;
	}

	if (mapping->mDNSRef == NULL) {
		err = DNSServiceCreateConnection(&mapping->mDNSRef);
		if (err != kDNSServiceErr_NoError || mapping->mDNSRef == NULL) {
			error("%s Error calling DNSServiceCreateConnection for %s, error: %d\n", sd_name, if_name, err);
			ppp_clear_one_nat_port_mapping(mapping);
			return -1;
		}
		mapping->mDNSRef_fd = DNSServiceRefSockFD(mapping->mDNSRef);
		add_fd(mapping->mDNSRef_fd);
	}
	mapping->mDNSRef_tmp = mapping->mDNSRef;
	err = DNSServiceNATPortMappingCreate(&mapping->mDNSRef_tmp, kDNSServiceFlagsShareConnection, interfaceIndex, protocol, htons(privatePort), publicPort, ttl, ppp_set_nat_port_mapping_callback, session);
	if (err != kDNSServiceErr_NoError) {
		error("%s Error calling DNSServiceNATPortMappingCreate for %s, error: %d\n", sd_name, if_name, err);
		ppp_clear_one_nat_port_mapping(mapping);
		return -1;
	}
	mapping->interfaceIndex = interfaceIndex;
	mapping->protocol = protocol;
	mapping->privatePort = privatePort;
	bzero(&mapping->reflexive, sizeof(mapping->reflexive));
	
	info("%s set port-mapping for %s, interface: %d, protocol: %d, privatePort: %d\n", sd_name, if_name, interfaceIndex, protocol, privatePort);
	return 0;
}

static void
l2tp_ipsec_set_nat_port_mapping (void)
{
	if (ppp_set_nat_port_mapping(&session->nat_mapping[0], 0, 0, 1) == 0) {
		session->nat_mapping_cnt++;
	}
#if 0 // <rdar://problem/8001582>
	if (ppp_set_nat_port_mapping(&session->nat_mapping[1], kDNSServiceProtocol_UDP, (u_int16_t)500, 0) == 0) {
		session->nat_mapping_cnt++;
	}
	if (ppp_set_nat_port_mapping(&session->nat_mapping[2], kDNSServiceProtocol_UDP, (u_int16_t)4500, 0) == 0) {
		session->nat_mapping_cnt++;
	}
#endif
}
#endif

void
l2tp_set_nat_port_mapping (void)
{
#if !TARGET_OS_EMBEDDED
	if (!session || !session->valid) {
		return;
	}

	if (override_primary) {
		info("%s port-mapping API for %s ignored: VPN is the Primary interface.\n",
             session->sd_name,
             session->interface_name);
		return;
	}

	// exit early if interface is PPP/VPN
	if (strstr(session->interface_name, "ppp") || strstr(session->interface_name, "utun")) {
		return;
	}

	if (!session->opt_noipsec) {
		// we always tranport l2tp over ipsec
		l2tp_ipsec_set_nat_port_mapping();
	} else {
		if (ppp_set_nat_port_mapping(&session->nat_mapping[0], 0, 0, 1) == 0) {
			session->nat_mapping_cnt++;
		}
#if 0 // <rdar://problem/8001582>
		if (ppp_set_nat_port_mapping(&session->nat_mapping[1], kDNSServiceProtocol_UDP, (u_int16_t)1701, 0) == 0) {
			session->nat_mapping_cnt++;
		}
#endif
	}
#endif // TARGET_OS_EMBEDDED
}

void
l2tp_clear_nat_port_mapping (void)
{
#if !TARGET_OS_EMBEDDED
	ppp_clear_nat_port_mapping();
#endif // TARGET_OS_EMBEDDED
}

void
pptp_set_nat_port_mapping (void)
{
#if !TARGET_OS_EMBEDDED
	if (!session || !session->valid) {
		return;
	}

	if (override_primary) {
		info("%s port-mapping API for %s ignored: VPN is the Primary interface.\n",
             session->sd_name,
             session->interface_name);
		return;
	}
	
	// exit early if interface is PPP/VPN
	if (strstr(session->interface_name, "ppp") || strstr(session->interface_name, "utun")) {
		return;
	}
	
	if (ppp_set_nat_port_mapping(&session->nat_mapping[0], 0, 0, 1) == 0) {
		session->nat_mapping_cnt++;
	}
#if 0 // <rdar://problem/8001582>
	if (ppp_set_nat_port_mapping(&session->nat_mapping[1], kDNSServiceProtocol_TCP, (u_int16_t)1723, 0) == 0) {
		session->nat_mapping_cnt++;
	}
#endif
#endif // TARGET_OS_EMBEDDED
}

void
pptp_clear_nat_port_mapping (void)
{
#if !TARGET_OS_EMBEDDED
	ppp_clear_nat_port_mapping();
#endif // TARGET_OS_EMBEDDED
}

void
ppp_process_nat_port_mapping_events (void)
{
#if !TARGET_OS_EMBEDDED
	int i = 0;
	DNSServiceErrorType ret;

	for (i = 0; session && session->valid && i < session->nat_mapping_cnt && i < MDNS_NAT_MAPPING_MAX; i++) {
		if (session->nat_mapping[i].mDNSRef && is_ready_fd(session->nat_mapping[i].mDNSRef_fd)) {
			ret = DNSServiceProcessResult(session->nat_mapping[i].mDNSRef);
			if (ret != kDNSServiceErr_NoError && ret != kDNSServiceErr_Transient) {
				error("Error calling DNSServiceProcessResult, error: %d\n", ret);
				if (ret == kDNSServiceErr_ServiceNotRunning) {
					ppp_clear_nat_port_mapping();
				}
			}
			// the connection may have been disconnected... hence session may be invalidated after DNSServiceProcessResult.
		}
	}
#endif // TARGET_OS_EMBEDDED
}
