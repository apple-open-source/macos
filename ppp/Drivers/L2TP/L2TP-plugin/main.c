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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  plugin to add L2TP client support to pppd.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/pfkeyv2.h>
#include <pthread.h>
#include <sys/kern_event.h>
#include <netinet/in_var.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFBundle.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <sys/un.h>


#include "../../../Controller/ppp_msg.h"
#include "../../../Family/ppp_defs.h"
#include "../../../Family/if_ppp.h"
#include "../../../Family/ppp_domain.h"
#include "../L2TP-extension/l2tpk.h"
#include "../../../Helpers/pppd/pppd.h"
#include "../../../Helpers/pppd/fsm.h"
#include "../../../Helpers/pppd/lcp.h"
#include "../../../Helpers/vpnd/RASSchemaDefinitions.h"
#include "../../../Helpers/vpnd/cf_utils.h"
#include "l2tp.h"
#include "../../../Helpers/vpnd/ipsec_utils.h"
#include "vpn_control.h"

#if TARGET_OS_EMBEDDED
#include <CoreTelephony/CTServerConnectionPriv.h>
#endif

/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

#define MODE_CONNECT	"connect"
#define MODE_LISTEN	"listen"
#define MODE_ANSWER	"answer"


#define L2TP_DEFAULT_RECV_TIMEOUT      20 /* seconds */

#define L2TP_MIN_HDR_SIZE 220		/* IPSec + Nat Traversal + L2TP/UDP + PPP */

#define	L2TP_RETRY_CONNECT_CODE			1

#define L2TP_DEFAULT_WAIT_IF_TIMEOUT      20 /* seconds */

#define MAX_CONNECT_RETRIES 10

/* 
	Private IPv4 addresses 
	10.0.0.0 - 10.255.255.255
	172.16.0.0 - 172.31.255.255
	192.168.0.0 - 192.168.255.255
*/

#define IN_PRIVATE_CLASSA_NETNUM	(u_int32_t)0xA000000 /* 10.0.0.0 */
#define IN_PRIVATE_CLASSA(i)		(((u_int32_t)(i) & IN_CLASSA_NET) == IN_PRIVATE_CLASSA_NETNUM)		

#define IN_PRIVATE_CLASSB_NETNUM	(u_int32_t)0xAC100000 /* 172.16.0.0 - 172.31.0.0 */
#define IN_PRIVATE_CLASSB(i)		(((u_int32_t)(i) & 0xFFF00000) == IN_PRIVATE_CLASSB_NETNUM)		
						
#define IN_PRIVATE_CLASSC_NETNUM	(u_int32_t)0xC0A80000 /* 192.168.0.0 */
#define IN_PRIVATE_CLASSC(i)		(((u_int32_t)(i) & 0xFFFF0000) == IN_PRIVATE_CLASSC_NETNUM)								

#undef IN_PRIVATE
#define IN_PRIVATE(i)				(IN_PRIVATE_CLASSA(i) || IN_PRIVATE_CLASSB(i) || IN_PRIVATE_CLASSC(i))


/* -----------------------------------------------------------------------------
 PPP globals
----------------------------------------------------------------------------- */

extern u_int8_t		control_buf[]; 

static int 	ctrlsockfd = -1;		/* control socket (UDP) file descriptor */
static int 	datasockfd = -1;		/* data socket (UDP) file descriptor */
static int 	eventsockfd = -1;		/* event socket to detect interface change */
static CFBundleRef 	bundle = 0;		/* our bundle ref */

/* option variables */
static char 	*opt_mode = MODE_CONNECT;		/* connect mode by default */
static bool 	opt_noload = 0;				/* don't load the kernel extension */
static bool 	opt_noipsec = 0;			/* don't use IPSec */
static int 	opt_udpport = 0;
static int	opt_connect_timeout = L2TP_DEFAULT_CONNECT_TIMEOUT;
static int	opt_connect_retrycount = L2TP_DEFAULT_CONNECT_RETRY_COUNT;
static int	opt_timeout = L2TP_DEFAULT_INITIAL_TIMEOUT;
static int	opt_timeoutcap = L2TP_DEFAULT_TIMEOUT_CAP;
static int	opt_retrycount = L2TP_DEFAULT_RETRY_COUNT;
static int	opt_windowsize = L2TP_DEFAULT_WINDOW_SIZE;
static int	opt_hello_timeout = 0;			/* default - only send for network change event */
static int     	opt_recv_timeout = L2TP_DEFAULT_RECV_TIMEOUT;
static char	opt_ipsecsharedsecret[MAXSECRETLEN] = { 0 };	/* IPSec Shared Secret */
static char     *opt_ipsecsharedsecrettype = "use";	 /* use, key, keychain */
static char	opt_ipseclocalidentifier[MAXNAMELEN] = { 0 };	/* IPSec Local Identifier */
static char     *opt_ipseclocalidentifiertype = "keyid";	 /* keyid, fqdn, user_fqdn, asn1dn, address */
static int	opt_wait_if_timeout = L2TP_DEFAULT_WAIT_IF_TIMEOUT;
static int  scaled_wait_if_timeout = L2TP_DEFAULT_WAIT_IF_TIMEOUT;

struct	l2tp_parameters our_params;
struct 	l2tp_parameters peer_params;

int     interface_media = 0;

int racoon_ctrlsockfd = -1;
/* 
Fast echo request procedure is run when a networking change is detected 
Echos are sent every 5 seconds for 30 seconds, or until a reply is received
If the network is detected dead, the the tunnel is disconnected.
Echos are not sent during normal operation.
*/

static int	hello_timer_running = 0;

static struct sockaddr_in our_address;		/* our side IP address */
static struct sockaddr_in peer_address;		/* the other side IP address */
static int                num_alt_peer_address = 0;
static struct sockaddr_in alt_peer_address[MAX_CONNECT_RETRIES];		/* the other side IP address */
static struct in_addr ip_zeros;
static u_int8_t routeraddress[16] = { 0 };
static u_int8_t interface[17] = { 0 };
static pthread_t resolverthread = 0;
static int 	resolverfds[2] = { -1, -1 };
#if TARGET_OS_EMBEDDED
static pthread_t edgethread = 0;
static int 	edgefds[2] = { -1, -1 };
#endif
static int 	peer_route_set = 0;		/* has a route to the peer been set ? */
//static int	echo_timer_running = 0;
static int	transport_up = 1;
static int	wait_interface_timer_running = 0;

static CFMutableDictionaryRef	ipsec_dict = NULL;

extern int 		kill_link;
extern CFStringRef	serviceidRef;		/* from pppd/sys_MacOSX.c */
extern SCDynamicStoreRef cfgCache;		/* from pppd/sys_MacOSX.c */
 
extern CFPropertyListRef 		userOptions;	/* from pppd/sys_MacOSX.c */
extern CFPropertyListRef 		systemOptions;	/* from pppd/sys_MacOSX.c */

/* option descriptors */
option_t l2tp_options[] = {
    { "l2tpnoload", o_bool, &opt_noload,
      "Don't try to load the L2TP kernel extension", 1 },
    { "l2tpnoipsec", o_bool, &opt_noipsec,
      "Don't use IPSec", 1 },
    { "l2tpipsecsharedsecret", o_string, opt_ipsecsharedsecret,
      "IPSec Shared Secret", 
      OPT_PRIO | OPT_STATIC | OPT_HIDE, NULL, MAXSECRETLEN },
    { "l2tpipsecsharedsecrettype", o_string, &opt_ipsecsharedsecrettype,
      "IPSec Shared Secret Type [use, key, keychain]" },
    { "l2tpipseclocalidentifier", o_string, opt_ipseclocalidentifier,
      "IPSec Local Identifier", 
      OPT_PRIO | OPT_STATIC, NULL, MAXNAMELEN },
    { "l2tpipseclocalidentifiertype", o_string, &opt_ipseclocalidentifiertype,
      "IPSec Local Identifier Type [keyid, fqdn, user_fqdn, asn1dn, address]" },
    { "l2tpudpport", o_int, &opt_udpport,
      "UDP port for connect"},
    { "l2tpmode", o_string, &opt_mode,
      "Configure configuration mode [connect, listen, answer]" },
    { "l2tpretrytimeout", o_int, &opt_timeout,
      "Set control message initial retry timeout (seconds)" },
    { "l2tptimeoutcap", o_int, &opt_timeoutcap,
      "Set control message retry timeout cap (seconds)" },
    { "l2tpretries", o_int, &opt_retrycount,
      "Set control message max retries" },
    { "l2tpconnecttimeout", o_int, &opt_connect_timeout,
      "Set connection control message retry timeout (seconds)" },
    { "l2tpconnectretries", o_int, &opt_connect_retrycount,
      "Set connection control message max retries" },
    { "l2tpwindow", o_int, &opt_windowsize,
      "Set control message window size" },
    { "l2tprecvtimeout", o_int, &opt_recv_timeout,
      "Set plugin receive timeout" },    
    { "l2tphellotimeout", o_int, &opt_hello_timeout,
      "Set timeout for hello messages: zero = send only for network change events" },    
    { "l2tpwaitiftimeout", o_int, &opt_wait_if_timeout,
      "How long do we wait for our transport interface to come back after interface events" },
    { NULL }
};

 
/* -----------------------------------------------------------------------------
    Function Prototypes
----------------------------------------------------------------------------- */

void l2tp_process_extra_options();
void l2tp_check_options();
int  l2tp_pre_start_link_check();
int l2tp_connect(int *errorcode);
void l2tp_disconnect();
void l2tp_close_fds();
void l2tp_cleanup();
int l2tp_establish_ppp(int);
void l2tp_wait_input();
void l2tp_disestablish_ppp(int);

static void l2tp_hello_timeout(void *arg);
static void closeall(void);
static u_long load_kext(char*, int byBundleID);
static void l2tp_link_failure();
static boolean_t l2tp_set_host_gateway(int cmd, struct in_addr host, struct in_addr gateway, char *ifname, int isnet);
static int l2tp_set_peer_route();
static int l2tp_clean_peer_route();
static void l2tp_ip_up(void *arg, uintptr_t p);
static void l2tp_start_wait_interface ();
static void l2tp_stop_wait_interface ();
static void l2tp_wait_interface_timeout (void *arg);
static void l2tp_assert_ipsec();

void l2tp_init_session __P((char *, u_int32_t, struct in_addr *, link_failure_func));

static ppp_session_t  l2tp_session = PPP_SESSION_INITIALIZER();
 
/* -----------------------------------------------------------------------------
plugin entry point, called by pppd
----------------------------------------------------------------------------- */
int start(CFBundleRef ref)
{
 
    bundle = ref;
    CFRetain(bundle);
        
    // hookup our socket handlers
    bzero(the_channel, sizeof(struct channel));
    the_channel->options = l2tp_options;
    the_channel->process_extra_options = l2tp_process_extra_options;
    the_channel->wait_input = l2tp_wait_input;
    the_channel->check_options = l2tp_check_options;
    the_channel->pre_start_link_check = l2tp_pre_start_link_check;
    the_channel->connect = l2tp_connect;
    the_channel->disconnect = l2tp_disconnect;
    the_channel->cleanup = l2tp_cleanup;
    the_channel->close = l2tp_close_fds;
    the_channel->establish_ppp = l2tp_establish_ppp;
    the_channel->disestablish_ppp = l2tp_disestablish_ppp;
    // use the default config functions
    the_channel->send_config = generic_send_config;
    the_channel->recv_config = generic_recv_config;

    add_notifier(&ip_up_notify, l2tp_ip_up, 0);

    return 0;
}

/* ----------------------------------------------------------------------------- 
do consistency checks on the options we were given
----------------------------------------------------------------------------- */
void l2tp_check_options()
{
    if (strcmp(opt_mode, MODE_CONNECT) 
        && strcmp(opt_mode, MODE_LISTEN) 
        && strcmp(opt_mode, MODE_ANSWER)) {
        error("L2TP incorrect mode : '%s'", opt_mode ? opt_mode : "");
        opt_mode = MODE_CONNECT;
    }
    
    if (opt_timeout < 1 || opt_timeout > 8) {
        error("L2TP incorrect timeout - must be between 1 and 8");
        opt_timeout = L2TP_DEFAULT_INITIAL_TIMEOUT;
    }

    if (opt_timeoutcap < 4) {
        error("L2TP incorrect timeout cap - cannot be less than 4");
        opt_timeoutcap = L2TP_DEFAULT_TIMEOUT_CAP;
    }
        
	/*
		reuse pppd redial functionality to retry connection
		retry every 3 seconds for the duration of the extra time
		do not report busy state
	 */  
	if (extraconnecttime) {
		busycode = L2TP_RETRY_CONNECT_CODE;
		redialtimer = 3 ;
		redialcount = extraconnecttime / redialtimer;
		hasbusystate = 0;
	}
		
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
void l2tp_process_extra_options()
{
    if (!strcmp(opt_mode, MODE_ANSWER)) {
        // make sure we get a file descriptor > 2 so that pppd can detach and close 0,1,2
        ctrlsockfd = dup(0);
    }
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void l2tp_start_wait_interface ()
{
    int timeout_scale_factor = 0;

    if (!wait_underlying_interface_up && lcp_echo_interval) {
        wait_underlying_interface_up = 1;
    }
    // the interface timer takes priority over the mapping timer
    ppp_block_public_nat_port_mapping_timer();

    if (wait_interface_timer_running != 0)
        return;

#if !TARGET_OS_EMBEDDED
    // increase the timeout if we're waiting for a wireless interface
    if (IFM_TYPE(interface_media) == IFM_IEEE80211) {
        timeout_scale_factor = 2;
    }
#endif /* !iPhone */
    scaled_wait_if_timeout = (opt_wait_if_timeout << timeout_scale_factor);
    notice("starting wait-interface timer for l2tp: %d secs", scaled_wait_if_timeout);
    TIMEOUT (l2tp_wait_interface_timeout, 0, scaled_wait_if_timeout);
    wait_interface_timer_running = 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void l2tp_stop_wait_interface ()
{
    ppp_variable_echo_start();

    if (wait_interface_timer_running) {
        UNTIMEOUT (l2tp_wait_interface_timeout, 0);
        wait_interface_timer_running = 0;
    }
    ppp_unblock_public_nat_port_mapping_timer();
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void l2tp_wait_interface_timeout (void *arg)
{
    if (wait_interface_timer_running != 0) {
        wait_interface_timer_running = 0;
		log_vpn_interface_address_event(__FUNCTION__, NULL, scaled_wait_if_timeout, interface, &our_address.sin_addr);
		// our transport interface didn't come back, take down the connection
		l2tp_link_failure();
    }
}

/* ----------------------------------------------------------------------------- 
called back everytime we go out of select, and data needs to be read
the hook is called and has a chance to get data out of its file descriptor
in the case of L2TP, we get control data on the socket
or get awaken when connection is closed
----------------------------------------------------------------------------- */
void l2tp_wait_input()
{
    int err, found;
    struct ifaddrs *ifap = NULL;

    if (eventsockfd != -1 && is_ready_fd(eventsockfd)) {
    
		char                 	buf[256] __attribute__ ((aligned(4)));		// Wcast-align fix - force alignment           
        char ev_if[32];
        struct kern_event_msg	*ev_msg;
        struct kev_in_data     	*inetdata;

        if (recv(eventsockfd, &buf, sizeof(buf), 0) != -1) {
            ev_msg = ALIGNED_CAST(struct kern_event_msg *)&buf;
            inetdata = (struct kev_in_data *) &ev_msg->event_data[0];
			log_vpn_interface_address_event(__FUNCTION__, ev_msg, opt_wait_if_timeout, interface, &our_address.sin_addr);
            switch (ev_msg->event_code) {
                case KEV_INET_NEW_ADDR:
                case KEV_INET_CHANGED_ADDR:
                case KEV_INET_ADDR_DELETED:
                    snprintf(ev_if, sizeof(ev_if), "%s%d", inetdata->link_data.if_name, inetdata->link_data.if_unit);
                    // check if changes occured on the interface we are using
                    if (!strncmp(ev_if, (char*)interface, sizeof(interface))) {
                        if (inetdata->link_data.if_family == APPLE_IF_FAM_PPP) {
                            // disconnect immediatly
                            l2tp_link_failure();
                        }
                        else {
							
                            /* check if address still exist */
                            found = 0;
                            if (getifaddrs(&ifap) == 0) {
                                struct ifaddrs *ifa;
                                for (ifa = ifap; ifa && !found ; ifa = ifa->ifa_next) {
                                    found = (ifa->ifa_name  
                                            && ifa->ifa_addr
                                            && !strncmp(ifa->ifa_name, (char*)interface, sizeof(interface))
                                            && ifa->ifa_addr->sa_family == AF_INET
                                            && (ALIGNED_CAST(struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr == our_address.sin_addr.s_addr);
                                }
                                freeifaddrs(ifap);
                            }

							if (found) {

								// no meaningful change, or address came back. Cancel timer if it was on.
								if (transport_up == 0) {

									/* 
										Our transport interface comes back with the same address.
										Stop waiting for interface. 
										A smarter algorithm should be implemented here.
									*/
									transport_up = 1;
									if (phase == PHASE_WAITING)
										new_phase(PHASE_RUNNING);
									l2tp_stop_wait_interface();

									/* since there could be a NAT inbetween, let's flush security association to force renegotiation and 
									  reacquisition of the correct port */
									if (!opt_noipsec) {
										l2tp_assert_ipsec();
									} else {
										/* reassert the tunnel by sending hello request */
										if (l2tp_send_hello(ctrlsockfd, &our_params)) {
											error("L2TP error on control channel sending Hello message after network change\n");
											/* ???? */
										}
									}
								}
							}
							else {
								// quick exit if there has been an unrecoverable change in interface/service
								if (check_vpn_interface_or_service_unrecoverable(cfgCache,
														 __FUNCTION__,
														 ev_msg,
														 (char*)interface)) {
									error("L2TP: the underlying interface/service has changed unrecoverably\n");
									transport_up = 0;
									l2tp_link_failure();
									break;
								}

								if (transport_up == 1) {
									transport_up = 0;
									if (phase == PHASE_RUNNING)
										new_phase(PHASE_WAITING);
									l2tp_start_wait_interface();
								} else {
								        // transport is still down: check if there was a valid address change
								        if (check_vpn_interface_address_change(wait_interface_timer_running /* && !transport_up */,
													       ev_msg,
													       (char*)interface,
														   interface_media,
													       &our_address.sin_addr)) {
									        error("L2TP: the underlying interface %s address changed\n",
										      interface);
										// disconnect immediately
										l2tp_link_failure();
									}
								}
                            }

                        }
                    } else {
		            /* if transport is still down: ignore deletes, and check if this alternative interface has a valid address */
		            if (check_vpn_interface_alternate((!transport_up && wait_interface_timer_running),
							      ev_msg,
							      (char*)interface)) {
			            error("L2TP: an alternative interface %s was detected while the underlying interface %s was down\n",
					  ev_if, interface);
				    // disconnect immediatly
				    l2tp_link_failure();
			    }
                    }
                    break;
            }
        }
    }

    if (ctrlsockfd != -1 && is_ready_fd(ctrlsockfd)) {

       err = l2tp_data_in(ctrlsockfd);
       if (err < 0) {
            // looks like we have been disconnected...
            // it's OK to get a hangup during terminate phase
            if (phase != PHASE_TERMINATE) {
                notice("L2TP hangup");
                status = EXIT_HANGUP;
            }
            remove_fd(ctrlsockfd);
            remove_fd(eventsockfd);
            hungup = 1;
            lcp_lowerdown(0);	/* L2TP link is no longer available */
            link_terminated(0);
            ppp_auxiliary_probe_stop();
            l2tp_clear_nat_port_mapping();
            ppp_session_clear(&l2tp_session);
            session = NULL;
	   }
    }

	ppp_process_nat_port_mapping_events();
	ppp_process_auxiliary_probe_input();
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void *l2tp_resolver_thread(void *arg)
{
    struct hostent 	*host;
    char		result = -1;
	int			count, fd;
	u_int8_t	rd8; 

    if (pthread_detach(pthread_self()) == 0) {
        
        // try to resolve the name
        if ((host = gethostbyname(remoteaddress))) {

			for (count = 0; host->h_addr_list[count]; count++);
		
			rd8 = 0;
			fd = open("/dev/random", O_RDONLY);
			if (fd) {
				read(fd, &rd8, sizeof(rd8));
				close(fd);
			}

			bzero(&peer_address.sin_addr, sizeof(peer_address.sin_addr));
			if ( count )
                memcpy(&peer_address.sin_addr, host->h_addr_list[rd8 % count], sizeof(struct in_addr));    // Wcast-align fix - using memcpy for unknown alignment
            bzero(alt_peer_address, sizeof(alt_peer_address));
            num_alt_peer_address = 0;
            if (count > 1) {
                while (num_alt_peer_address < (count - 1) &&
                       num_alt_peer_address < MAX_CONNECT_RETRIES) {
                    alt_peer_address[num_alt_peer_address].sin_len = sizeof(alt_peer_address[num_alt_peer_address]);
                    alt_peer_address[num_alt_peer_address].sin_family = AF_INET;
                    alt_peer_address[num_alt_peer_address].sin_port = htons(L2TP_UDP_PORT);
                    
                    // Wcast-align fix - using memcpy for unknown alignment
                    memcpy(&alt_peer_address[num_alt_peer_address].sin_addr, host->h_addr_list[(rd8 + num_alt_peer_address + 1)% count], sizeof(struct in_addr));
                    num_alt_peer_address++;
                }
            }
            result = 0;
        }
    }

    write(resolverfds[1], &result, 1);
    return 0;
}

#if TARGET_OS_EMBEDDED
static 
void callbackEDGE(CTServerConnectionRef connection, CFStringRef notification, CFDictionaryRef notificationInfo, void* info) {
	
	/* not used */
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
void *l2tp_edge_thread(void *arg)
{
    char		result = -1;
	int			count;
	CTServerConnectionRef	edgeConnection;
	_CTServerConnectionContext ctxt = { 0, NULL, NULL, NULL, NULL };
	Boolean active = FALSE;
	CTError cterror = { kCTErrorDomainNoError, 0 };

    if (pthread_detach(pthread_self()) == 0) {
       
		edgeConnection = _CTServerConnectionCreate(kCFAllocatorDefault, callbackEDGE, &ctxt);
		if (edgeConnection) {
			_CTServerConnectionSetPacketContextActiveByServiceType(edgeConnection, kCTDataConnectionServiceTypeInternet, TRUE);

			count = PPPD_WWAN_INTERFACE_TIMEOUT;
			cterror = _CTServerConnectionGetPacketContextActive(edgeConnection, 0, &active);
			while (!cterror.error && !active && count--) {
				sleep(1);
				cterror = _CTServerConnectionGetPacketContextActive(edgeConnection, 0, &active);
			}
			CFRelease(edgeConnection);

			if (active) {
				sleep(2); // additionnal 2 seconds for DNS information to be installed
				result = 0;
			}
		}
	}
	
	
    write(edgefds[1], &result, 1);
    return 0;
}
#endif

/* ----------------------------------------------------------------------------- 
get the ipsec string corresponding to the ike error
----------------------------------------------------------------------------- */
char *ipsec_error_to_str(int ike_code)
{
	switch (ike_code) {
		case VPNCTL_NTYPE_INVALID_PAYLOAD_TYPE: return "Invalid payload type";
		case VPNCTL_NTYPE_DOI_NOT_SUPPORTED: return "DOI not supported";
		case VPNCTL_NTYPE_SITUATION_NOT_SUPPORTED: return "Situation not supported";
		case VPNCTL_NTYPE_INVALID_COOKIE: return "Invalid cookie";
		case VPNCTL_NTYPE_INVALID_MAJOR_VERSION: return "Invalid major version";
		case VPNCTL_NTYPE_INVALID_MINOR_VERSION: return "Invalid minor version";
		case VPNCTL_NTYPE_INVALID_EXCHANGE_TYPE: return "Invalid exchange type";
		case VPNCTL_NTYPE_INVALID_FLAGS: return "Invalid flags";
		case VPNCTL_NTYPE_INVALID_MESSAGE_ID: return "Invalid message id";
		case VPNCTL_NTYPE_INVALID_PROTOCOL_ID: return "Invalid protocol id";
		case VPNCTL_NTYPE_INVALID_SPI: return "Invalid SPI";
		case VPNCTL_NTYPE_INVALID_TRANSFORM_ID: return "Invalid transform id";
		case VPNCTL_NTYPE_ATTRIBUTES_NOT_SUPPORTED: return "Attributes not supported";
		case VPNCTL_NTYPE_NO_PROPOSAL_CHOSEN: return "No proposal chosen";
		case VPNCTL_NTYPE_BAD_PROPOSAL_SYNTAX: return "Bad proposal syntax";
		case VPNCTL_NTYPE_PAYLOAD_MALFORMED: return "Payload malformed";
		case VPNCTL_NTYPE_INVALID_KEY_INFORMATION: return "Invalid key information";
		case VPNCTL_NTYPE_INVALID_ID_INFORMATION: return "Invalid id information";
		case VPNCTL_NTYPE_INVALID_CERT_ENCODING: return "Invalid cert encoding";
		case VPNCTL_NTYPE_INVALID_CERTIFICATE: return "Invalid certificate";
		case VPNCTL_NTYPE_BAD_CERT_REQUEST_SYNTAX: return "Bad cert request syntax";
		case VPNCTL_NTYPE_INVALID_CERT_AUTHORITY: return "Invalid cert authority";
		case VPNCTL_NTYPE_INVALID_HASH_INFORMATION: return "Invalid hash information";
		case VPNCTL_NTYPE_AUTHENTICATION_FAILED: return "Authentication Failed";
		case VPNCTL_NTYPE_INVALID_SIGNATURE: return "Invalid signature";
		case VPNCTL_NTYPE_ADDRESS_NOTIFICATION: return "Address notification";
		case VPNCTL_NTYPE_NOTIFY_SA_LIFETIME: return "Notify SA lifetime";
		case VPNCTL_NTYPE_CERTIFICATE_UNAVAILABLE: return "Certificate unavailable";
		case VPNCTL_NTYPE_UNSUPPORTED_EXCHANGE_TYPE: return "Unsupported exchange type";
		case VPNCTL_NTYPE_UNEQUAL_PAYLOAD_LENGTHS: return "Unequal payload lengths";
		case VPNCTL_NTYPE_LOAD_BALANCE: return "Load balance";
		case VPNCTL_NTYPE_INTERNAL_ERROR: return "Internal error";
	}
	return "Unknown error";
}

enum {
    RACOON_BINDING = 1,		// we send a bind command to racoon
    RACOON_TRIGGERED,		// we send a packet to triggerd racoon
    RACOON_STARTED,			// racoon received our request and started IKE
    RACOON_NEGOTIATING,		// the server replied, negotiation in progress 
    RACOON_DONE				// racoon is done
};

/* ----------------------------------------------------------------------------- 
trigger an IKE exchange
----------------------------------------------------------------------------- */
int l2tp_trigger_ipsec(int listenmode,
                       int *ipsec_status)
{			
	int					size=0, state = 0, timeo, err = -1;
	struct sockaddr_un	sun;
    struct sockaddr		from;
	u_int16_t			reliable;
	struct sockaddr_in	redirect_addr;
	u_int8_t					data[256] __attribute__ ((aligned(4))); 		// Wcast-align fix - force alignment 
	struct vpnctl_hdr			*hdr = ALIGNED_CAST(struct vpnctl_hdr *)data;
	struct vpnctl_cmd_bind		*cmd_bind = ALIGNED_CAST(struct vpnctl_cmd_bind *)data;
	struct vpnctl_status_failed *failed_status = ALIGNED_CAST(struct vpnctl_status_failed *)data;
	int                          num_ipsec_triggers = 0;

	/* open and connect to the racoon control socket */
	if (racoon_ctrlsockfd < 0) {
		racoon_ctrlsockfd = socket(PF_LOCAL, SOCK_STREAM, 0);
		if (racoon_ctrlsockfd < 0) {
			error("L2TP: cannot create racoon control socket: %m\n");
			goto fail;
		}

		bzero(&sun, sizeof(sun));
		sun.sun_family = AF_LOCAL;
		strncpy(sun.sun_path, "/var/run/vpncontrol.sock", sizeof(sun.sun_path));

		if (connect(racoon_ctrlsockfd,  (struct sockaddr *)&sun, sizeof(sun)) < 0) {
			error("L2TP: cannot connect racoon control socket: %m\n");
			goto fail;
		}
	}
	
	if ( listenmode ){
		close(racoon_ctrlsockfd);		/* connect to vpncontrol socket to start up racoon only */
		racoon_ctrlsockfd = -1;
		return 0;		
	}

	//sleep(2);
start:	

	// bind racoon control socket to the peer address to receive only pertinent messages
	bzero(cmd_bind, sizeof(struct vpnctl_cmd_bind));
	cmd_bind->hdr.len = htons(sizeof(struct vpnctl_cmd_bind) - sizeof(struct vpnctl_hdr));
	cmd_bind->hdr.msg_type = htons(VPNCTL_CMD_BIND);
	cmd_bind->address = peer_address.sin_addr.s_addr;
	write(racoon_ctrlsockfd, cmd_bind, sizeof(struct vpnctl_cmd_bind));

	notice("IPSec connection started\n");
	state = RACOON_BINDING;

	while (state != RACOON_DONE) {

		switch (state) {
			case RACOON_BINDING:  timeo = 5; break;
			case RACOON_TRIGGERED:  timeo = 1; break;
			case RACOON_STARTED:	timeo = 10; break;
			default:				timeo = 30; break;
		}
		
		from.sa_len = sizeof(from);
		err = l2tp_recv(racoon_ctrlsockfd, data, sizeof(struct vpnctl_hdr), &size, &from, timeo, "from racoon control socket");
		if (err || size == 0) {	// no reply
		        // RACOON_TRIGGERED's timeout is actually 5 x 1 second increments.
		        if (err == -1 &&
			    state == RACOON_TRIGGERED &&
			    num_ipsec_triggers < 5) {
			        // trigger racoon again
			        l2tp_send_hello_trigger(ctrlsockfd, (struct sockaddr *)&peer_address);
				num_ipsec_triggers++;
				continue;
			}
			if (err != -2) // cancel
				notice("IPSec connection failed\n");
			goto fail;
		}
		
		/* read end of packet */
		if (ntohs(hdr->len)) {
			from.sa_len = sizeof(from);
			err = l2tp_recv(racoon_ctrlsockfd, data + sizeof(struct vpnctl_hdr), ntohs(hdr->len), &size, &from, timeo, "from racoon control socket");
			if (err || size == 0) {	// no reply
				if (err != -2) // cancel
					notice("IPSec connection failed\n");
				goto fail;
			}
		}

		if (debug > 1) {
			dbglog("L2TP received racoon message <type 0x%x> <flags 0x%x> <cookie 0x%x> <result %d> <reserved 0x%x> <len %d>", 
				ntohs(hdr->msg_type), ntohs(hdr->flags), ntohl(hdr->cookie), ntohl(hdr->reserved), ntohs(hdr->result), ntohs(hdr->len));
		}

		switch (ntohs(hdr->msg_type)) {
		
			case VPNCTL_STATUS_IKE_FAILED:
											 
				switch (ntohs(failed_status->ike_code)) {

					case VPNCTL_NTYPE_LOAD_BALANCE:		

						redirect_addr = peer_address;
						redirect_addr.sin_addr.s_addr = *ALIGNED_CAST(u_int32_t*)failed_status->data;
						notice("IPSec connection redirected to server '%s'...\n", inet_ntoa(redirect_addr.sin_addr));
				
						err = l2tp_change_peeraddress(ctrlsockfd, (struct sockaddr *)&redirect_addr);
						if (err)
							goto fail;
				
						goto start; // restart the connection to an other server
						break;

					default:	
						notice("IPSec connection failed <IKE Error %d (0x%x) %s>\n", ntohs(failed_status->ike_code), ntohs(failed_status->ike_code), ipsec_error_to_str(ntohs(failed_status->ike_code)));
						err = -1;
						goto fail;
						break;
				}
				break;

			case VPNCTL_STATUS_PH1_START_US:
				dbglog("IPSec phase 1 client started\n");
				state = RACOON_STARTED;
				reliable = 1;
				setsockopt(ctrlsockfd, PPPPROTO_L2TP, L2TP_OPT_RELIABILITY, &reliable, 2);
				break;

			case VPNCTL_STATUS_PH1_START_PEER:
				dbglog("IPSec phase 1 server replied\n");
				state = RACOON_NEGOTIATING;
				break;

			case VPNCTL_STATUS_PH1_ESTABLISHED:
				dbglog("IPSec phase 1 established\n");
				state = RACOON_NEGOTIATING;
				break;

			case VPNCTL_STATUS_PH2_START:
				dbglog("IPSec phase 2 started\n");
				state = RACOON_NEGOTIATING;
				break;

			case VPNCTL_STATUS_PH2_ESTABLISHED:
				state = RACOON_DONE;
				dbglog("IPSec phase 2 established\n");
				notice("IPSec connection established\n");
				break;

			default:
				/* ignore other messages */
				if (state == RACOON_BINDING) {
					
					/* send L2TP packets to trigger IPSec connection */
					reliable = 0;
					setsockopt(ctrlsockfd, PPPPROTO_L2TP, L2TP_OPT_RELIABILITY, &reliable, 2);
					l2tp_send_hello_trigger(ctrlsockfd, (struct sockaddr *)&peer_address);
					num_ipsec_triggers = 1;
					state = RACOON_TRIGGERED;
				}
				break;

		}
	}
	
	err = 0;
	
fail:
    *ipsec_status = state;
	if (err == -1 || err == -2)
        return EXIT_L2TP_NOANSWER;
	else
        return err;
}

/* ----------------------------------------------------------------------------- 
 assert the ipsec transport after an address change or slee-wake
 ----------------------------------------------------------------------------- */
static void l2tp_assert_ipsec()
{
	if (!strcmp(opt_mode, MODE_CONNECT)){
		struct sockaddr_un       sun;
		struct vpnctl_cmd_assert msg;

		/* open and connect to the racoon control socket */
		if (racoon_ctrlsockfd < 0) {
			racoon_ctrlsockfd = socket(PF_LOCAL, SOCK_STREAM, 0);
			if (racoon_ctrlsockfd < 0) {
				error("L2TP: cannot create racoon control socket: %m\n");
				goto remove_sa; // fallback to removal of SAs
			}

			bzero(&sun, sizeof(sun));
			sun.sun_family = AF_LOCAL;
			strncpy(sun.sun_path, "/var/run/vpncontrol.sock", sizeof(sun.sun_path));

			if (connect(racoon_ctrlsockfd,  (struct sockaddr *)&sun, sizeof(sun)) < 0) {
				error("L2TP: cannot connect racoon control socket: %m\n");
				close(racoon_ctrlsockfd);
				racoon_ctrlsockfd = -1;
				goto remove_sa; // fallback to removal of SAs
			}
		}

		bzero(&msg, sizeof(msg));
		msg.hdr.msg_type = htons(VPNCTL_CMD_ASSERT);
		msg.src_address = our_address.sin_addr.s_addr;
		msg.dst_address = peer_address.sin_addr.s_addr;
		msg.hdr.len = htons(sizeof(msg) - sizeof(msg.hdr));;
		write(racoon_ctrlsockfd, &msg, sizeof(msg));
		goto done;
	}

remove_sa:
	IPSecRemoveSecurityAssociations((struct sockaddr *)&our_address, (struct sockaddr *)&peer_address);
done:
	// wait 3 seconds before trying to trigger tunnel
	sleep(3);
	return;
}

int l2tp_pre_start_link_check()
{
    int                      reachable = FALSE;
    SCNetworkReachabilityRef ref;
    SCNetworkConnectionFlags flags;
    
    ref = SCNetworkReachabilityCreateWithName(NULL, remoteaddress);
    if (ref) {        
        if (SCNetworkReachabilityGetFlags(ref, &flags)) {
            if (REACHABLE_NOW || REACHABLE_AUTOMATICALLY_WITHOUT_USER) {
                reachable = TRUE;
            }
        }
        CFRelease(ref);
    }
    
    if (reachable) {
        return 0;
    }
    return -1;
}

static CFStringRef l2tp_copy_str_at_index(CFStringRef key, int index)
{
    CFArrayRef	components;
    CFStringRef foundstr = NULL;

    components = CFStringCreateArrayBySeparatingStrings(NULL, key, CFSTR("/"));
    if (index < CFArrayGetCount(components)) {
        if ((foundstr = CFArrayGetValueAtIndex(components, index))){
            CFRetain(foundstr);
        }
    }
    CFRelease(components);
    return foundstr;
}

static void l2tp_get_router_address(CFStringRef serviceID)
{
    CFStringRef		routerAddress = NULL;
    CFStringRef     ipv4Key = NULL;
    CFMutableArrayRef ipv4Keys = NULL;
    CFDictionaryRef ipv4Dict = NULL;
    CFIndex         n;

    warning("l2tp_get_router_address\n");

    ipv4Keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);

    if (serviceID == NULL || ipv4Keys == NULL) {
        goto done;
    }

    ipv4Key = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
                                                          kSCDynamicStoreDomainSetup,
                                                          serviceID,
                                                          kSCEntNetIPv4);
    if (!ipv4Key) {
        goto done;
    }
    CFArrayAppendValue(ipv4Keys, ipv4Key);
    CFRelease(ipv4Key);

    ipv4Key = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
                                                          kSCDynamicStoreDomainState,
                                                          serviceID,
                                                          kSCEntNetIPv4);
    if (!ipv4Key) {
        goto done;
    }
    CFArrayAppendValue(ipv4Keys, ipv4Key);
    CFRelease(ipv4Key);

    ipv4Dict = SCDynamicStoreCopyMultiple(NULL, ipv4Keys, NULL);
    if (ipv4Dict == NULL) {
        warning("l2tp_get_router_address: could not get router address\n");
        goto done;
    }

    n = CFDictionaryGetCount(ipv4Dict);
    if (n <= 0) {
        warning("l2tp_get_router_address: empty router address dictionary\n");
        goto done;
    } else {
        CFDictionaryRef dict;
        int i;

        // Try to retrieve router info from system config first, and from the current network state next if not found
        n = CFArrayGetCount(ipv4Keys);
        for (i = 0; i < n; i++) {
            ipv4Key = CFArrayGetValueAtIndex(ipv4Keys, i);
            dict = CFDictionaryGetValue(ipv4Dict, ipv4Key);
            if (dict && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
                routerAddress = CFDictionaryGetValue(dict, kSCPropNetIPv4Router);
                if (routerAddress) {
                    CFStringGetCString(routerAddress, (char*)routeraddress, sizeof(routeraddress), kCFStringEncodingUTF8);
                    warning("l2tp_get_router_address %s from dict %d\n", routeraddress, i);
                    goto done;
                }
            }
        }
    }

done:
    if (ipv4Keys) {
        CFRelease(ipv4Keys);
    }
    if (ipv4Dict) {
        CFRelease(ipv4Dict);
    }
}

static void l2tp_get_router_address_for_interface(void)
{
    CFDictionaryRef     dict = NULL;
    CFStringRef         pattern = NULL;
    CFMutableArrayRef   patterns = NULL;
    CFStringRef         *keys = NULL;
    CFDictionaryRef     *values = NULL;
    CFIndex             count = 0;
    CFIndex             i = 0;
    CFStringRef         serviceID = NULL;

    if (interface == NULL || interface[0] == 0) {
        goto done;
    }

    patterns = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    pattern = SCDynamicStoreKeyCreateNetworkServiceEntity(kCFAllocatorDefault,
                                                          kSCDynamicStoreDomainState,
                                                          kSCCompAnyRegex,
                                                          kSCEntNetIPv4);

    if (patterns == NULL || pattern == NULL)
        goto done;
    CFArrayAppendValue(patterns, pattern);

	dict = SCDynamicStoreCopyMultiple(NULL, NULL, patterns);
    if (dict == NULL)
	    goto done;
	
    count = CFDictionaryGetCount(dict);

    keys = calloc(count, sizeof(CFStringRef));
    values = calloc(count, sizeof(CFDictionaryRef));
    if (keys == NULL || values == NULL)
        goto done;
    CFDictionaryGetKeysAndValues(dict, (const void**)keys, (const void**)values);

    for (i=0; i < count; i++) {
        CFDictionaryRef ipv4Dict = NULL;
        CFStringRef     ipv4Key = NULL;
        
        ipv4Key  = keys[i];
        ipv4Dict = values[i];
        
        if (ipv4Key == NULL || ipv4Dict == NULL) {
            continue;
        }
        
        /* Match interface name here */
        CFStringRef ifnameRef = CFDictionaryGetValue(ipv4Dict, kSCPropInterfaceName);
        if (ifnameRef) {
            char ifname[IFNAMSIZ] = { 0 };
            CFStringGetCString(ifnameRef, ifname, sizeof(ifname), kCFStringEncodingASCII);
            if (!strcmp(ifname, interface)) {
                if ((CFStringHasPrefix(ipv4Key, kSCDynamicStoreDomainState)) && (CFStringHasSuffix(ipv4Key, kSCEntNetIPv4))) {
                    // Fetch the serviceID, then the router address
                    serviceID = l2tp_copy_str_at_index(ipv4Key, 3);
                    l2tp_get_router_address(serviceID);
                    break;
                }
            }
        }
    }
    
done:
    if (serviceID) {
        CFRelease(serviceID);
    }
    if (pattern) {
        CFRelease(pattern);
    }
    if (patterns) {
        CFRelease(patterns);
    }
    if (dict) {
        CFRelease(dict);
    }
    if (keys) {
        free(keys);
    }
    if (values) {
        free(values);
    }
}

/* ----------------------------------------------------------------------------- 
get the socket ready to start doing PPP.
That is, open the socket and start the L2TP dialog
----------------------------------------------------------------------------- */
int l2tp_connect(int *errorcode)
{
    char 		dev[32], name[MAXPATHLEN], c; 
    int 		err = 0, rcvlen;  
	socklen_t	optlen;
    CFURLRef		url;
    CFDictionaryRef	dict;
    CFStringRef		string, key;
    struct kev_request	kev_req;
    struct sockaddr_in	from;
    struct sockaddr_in 	any_address;
	u_int32_t baudrate;
	char				*errstr;
	int					host_name_specified;
    int                 ipsec_status = 0;
    int                 num_connect_retries = 0;
	
	*errorcode = 0;

    if (cfgCache == NULL || serviceidRef == NULL) {
        goto fail;
    }

    snprintf(dev, sizeof(dev), "socket[%d:%d]", PF_PPP, PPPPROTO_L2TP);
    strlcpy(ppp_devnam, dev, sizeof(ppp_devnam));

    hungup = 0;
    kill_link = 0;

    routeraddress[0] = 0;
    interface[0] = 0;
	host_name_specified = 0;

    /* unknown src and dst addresses */
    bzero(&any_address, sizeof(any_address));
    any_address.sin_len = sizeof(any_address);
    any_address.sin_family = AF_INET;
    any_address.sin_port = htons(0);
    any_address.sin_addr.s_addr = htonl(INADDR_ANY);

    our_address = any_address;
    peer_address = any_address;

    /* init params */
    bzero(&our_params, sizeof(our_params));
    bzero(&peer_params, sizeof(peer_params));
    
    our_params.tunnel_id = 0;					/* our tunnel ID - will be assigned later */
    our_params.session_id = getpid();			/* our session ID - use pid as unique number */
    our_params.window_size = opt_windowsize;	/* our receive window size */
    our_params.seq_required = 0;				/* sequencing required - not used for now */
    our_params.call_serial_num = 1; 				/* our call serial number - always 1 for now */
    our_params.framing_caps = L2TP_SYNC_FRAMING|L2TP_ASYNC_FRAMING;
    our_params.framing_type = L2TP_SYNC_FRAMING|L2TP_ASYNC_FRAMING;
    our_params.tx_connect_speed = 1000000;
    if (gethostname((char*)our_params.host_name, sizeof(our_params.host_name)) != 0) {
        our_params.host_name[0] = 0;
    }
    our_params.protocol_vers = L2TP_PROTOCOL_VERSION;

    if (ifscope && ifscope[0]) {
        strcpy(interface, ifscope);
        l2tp_get_router_address_for_interface();
    } else {
        key = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
        if (key) {
            dict = SCDynamicStoreCopyValue(cfgCache, key);
        CFRelease(key);
            if (dict) {
                if ((string  = CFDictionaryGetValue(dict, kSCPropNetIPv4Router)))
                    CFStringGetCString(string, (char*)routeraddress, sizeof(routeraddress), kCFStringEncodingUTF8);
                if ((string  = CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface)))
                    CFStringGetCString(string, (char*)interface, sizeof(interface), kCFStringEncodingUTF8);
                CFRelease(dict);
            }
        }
    }

	/* now that we know our interface, adjust the MTU if necessary */
	if (interface[0]) {
		int min_mtu = get_if_mtu((char*)interface) - L2TP_MIN_HDR_SIZE;
		if (lcp_allowoptions[0].mru > min_mtu)	/* defines out mtu */
			lcp_allowoptions[0].mru = min_mtu;

		/* Don't adjust MRU, radar 3974763 */ 
#if 0
		if (lcp_wantoptions[0].mru > min_mtu)	/* defines out mru */
			lcp_wantoptions[0].mru = min_mtu;
		if (lcp_wantoptions[0].neg_mru > min_mtu)	/* defines our mru */
			lcp_wantoptions[0].neg_mru = min_mtu;
#endif
	}

#if !TARGET_OS_EMBEDDED
    interface_media = get_if_media((char*)interface);
#endif /* !iPhone */
	
	/* let's say our underlying transport is up */
	transport_up = 1;
	wait_interface_timer_running = 0;
	wait_underlying_interface_up = 0;
	ppp_session_clear(&l2tp_session);
	session = NULL;

    eventsockfd = socket(PF_SYSTEM, SOCK_RAW, SYSPROTO_EVENT);
    if (eventsockfd != -1) {
        // L2TP can survive without event socket anyway
        kev_req.vendor_code = KEV_VENDOR_APPLE;
        kev_req.kev_class = KEV_NETWORK_CLASS;
        kev_req.kev_subclass = KEV_INET_SUBCLASS;
        ioctl(eventsockfd, SIOCSKEVFILT, &kev_req);
    }

    if (strcmp(opt_mode, MODE_ANSWER)) {		/* not for answer mode */
        /* open the L2TP socket control socket */
        ctrlsockfd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_L2TP);
        if (ctrlsockfd < 0) {
            if (!opt_noload) {
                if ((url = CFBundleCopyBundleURL(bundle))) {
                    name[0] = 0;
                    CFURLGetFileSystemRepresentation(url, 0, (UInt8 *)name, MAXPATHLEN - 1);
                    CFRelease(url);
                    strlcat(name, "/", sizeof(name));
                    if ((url = CFBundleCopyBuiltInPlugInsURL(bundle))) {
                        CFURLGetFileSystemRepresentation(url, 0, (UInt8 *)(name + strlen(name)), 
                                MAXPATHLEN - strlen(name) - strlen(L2TP_NKE) - 1);
                        CFRelease(url);
                        strlcat(name, "/", sizeof(name));
                        strlcat(name, L2TP_NKE, sizeof(name));
#if !TARGET_OS_EMBEDDED
                        if (!load_kext(name, 0))
#else
                        if (!load_kext(L2TP_NKE_ID, 1))
#endif
                            ctrlsockfd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_L2TP);
                    }	
                }
            }
            if (ctrlsockfd < 0) {
                    error("Failed to open L2TP control socket: %m");
                    goto fail;
            }
        }
    } 
    
    l2tp_set_flag(ctrlsockfd, kdebugflag & 1, L2TP_FLAG_DEBUG);
    l2tp_set_flag(ctrlsockfd, 1, L2TP_FLAG_CONTROL);
    l2tp_set_flag(ctrlsockfd, our_params.seq_required, L2TP_FLAG_SEQ_REQ);
    l2tp_set_flag(ctrlsockfd, !opt_noipsec, L2TP_FLAG_IPSEC);
    l2tp_set_delegated_process(ctrlsockfd, getpid());  // must be set before calling l2tp_set_ouraddress

    /* ask the kernel extension to make and assign a new tunnel id to the tunnel */
    optlen = 2;
    getsockopt(ctrlsockfd, PPPPROTO_L2TP, L2TP_OPT_NEW_TUNNEL_ID, &our_params.tunnel_id, &optlen);

    l2tp_set_ourparams(ctrlsockfd,  &our_params);
    l2tp_reset_timers(ctrlsockfd, 0);

    if (kill_link)
        goto fail1;

    if (!strcmp(opt_mode, MODE_CONNECT)) {
        //-------------------------------------------------
        //	connect mode
        //-------------------------------------------------  

        struct sockaddr_in orig_our_address, orig_peer_address;
        
        if (remoteaddress == 0) {
            error("L2TP: No remote address supplied...\n");
            devstatus = EXIT_L2TP_NOSERVER;
            goto fail;
        }

		set_network_signature("VPN.RemoteAddress", remoteaddress, 0, 0);
		
#if TARGET_OS_EMBEDDED
		{
			/* first, bring up EDGE */
			int need_edge = FALSE;
			SCNetworkReachabilityRef ref = NULL;
			SCNetworkConnectionFlags	flags;
			
			ref = SCNetworkReachabilityCreateWithName(NULL, remoteaddress);
			if (ref) {
				
				if (SCNetworkReachabilityGetFlags(ref, &flags)) {
					if ((flags & kSCNetworkReachabilityFlagsReachable) &&
						(flags & kSCNetworkReachabilityFlagsConnectionRequired) &&
						(flags & kSCNetworkReachabilityFlagsIsWWAN)) {
						need_edge = TRUE;
					}
				}
				CFRelease(ref);
			}
			
			if (need_edge) {
				
				if (pipe(edgefds) < 0) {
					error("L2TP: failed to create pipe for starting edge...\n");
					goto fail;
				}
				
				if (pthread_create(&edgethread, NULL, l2tp_edge_thread, NULL)) {
					error("L2TP: failed to create thread for starting edge...\n");
					close(edgefds[0]);
					close(edgefds[1]);
					goto fail;
				}
				
				while (read(edgefds[0], &c, 1) != 1) {
					if (kill_link) {
						pthread_cancel(edgethread);
						break;
					}
				}
				
				close(edgefds[0]);
				close(edgefds[1]);
				
				if (kill_link)
					goto fail1;
				
				if (c) {
					error("L2TP: Cannot start EDGE connection...\n");
					*errorcode = L2TP_RETRY_CONNECT_CODE; /* wait and retry if necessary */
					devstatus = EXIT_L2TP_NOEDGE;
					goto fail;
				}

			}

		}
#endif
        
        /* build the peer address */
        peer_address.sin_len = sizeof(peer_address);
        peer_address.sin_family = AF_INET;
        peer_address.sin_port = htons(L2TP_UDP_PORT);
        if (inet_aton(remoteaddress, &peer_address.sin_addr) == 0) {
			
            if (pipe(resolverfds) < 0) {
                error("L2TP: failed to create pipe for gethostbyname...\n");
                goto fail;
            }
			
            if (pthread_create(&resolverthread, NULL, l2tp_resolver_thread, NULL)) {
                error("L2TP: failed to create thread for gethostbyname...\n");
                close(resolverfds[0]);
                close(resolverfds[1]);
                goto fail;
            }
            
            while (read(resolverfds[0], &c, 1) != 1) {
                if (kill_link) {
                    pthread_cancel(resolverthread);
                    break;
                }
            }
            
            close(resolverfds[0]);
            close(resolverfds[1]);
            
            if (kill_link)
                goto fail1;
            
            if (c) {
                error("L2TP: Host '%s' not found...\n", remoteaddress);
				*errorcode = L2TP_RETRY_CONNECT_CODE; /* wait and retry if necessary */
                devstatus = EXIT_L2TP_NOSERVER;
                goto fail;
            }
			host_name_specified = 1;
        }

        notice("L2TP connecting to server '%s' (%s)...\n", remoteaddress, inet_ntoa(peer_address.sin_addr));

        set_server_peer(peer_address.sin_addr);

        /* get the source address that will be used to reach the peer */
        if (get_src_address((struct sockaddr *)&our_address, (struct sockaddr *)&peer_address, ifscope, NULL)) {
            error("L2TP: cannot get our local address...\n");
			*errorcode = L2TP_RETRY_CONNECT_CODE; /* wait and retry if necessary */
            goto fail;
        }

        /* bind the socket in the kernel with take an ephemeral port */
        /* on return, it ouraddress will contain actual port selected */
        our_address.sin_port = htons(opt_udpport);
        l2tp_set_ouraddress(ctrlsockfd, (struct sockaddr *)&our_address);        
        
        /* set our peer address */
        l2tp_set_peeraddress(ctrlsockfd, (struct sockaddr *)&peer_address);

        /* remember the original source and dest addresses */
        orig_our_address = our_address;
        orig_peer_address = peer_address;

        err = 0;

        /* install IPSec filters for our address and peer address */
        if (!opt_noipsec) {

			CFStringRef				secret_string = NULL;
			CFStringRef				secret_encryption_string = NULL;
			CFStringRef				localidentifier_string = NULL;
			CFStringRef				localidentifiertype_string = NULL;
			CFStringRef				auth_method = NULL;
			CFStringRef				verify_id = NULL;
			CFDataRef				certificate = NULL;
			CFDictionaryRef			useripsec_dict = NULL;
			int useripsec_dict_fromsystem = 0;
			
            struct sockaddr_in addr = orig_peer_address;
            addr.sin_port = htons(0);	// allow port to change

			auth_method = kRASValIPSecAuthenticationMethodSharedSecret;
			
			if (userOptions)
				useripsec_dict = CFDictionaryGetValue(userOptions, kSCEntNetIPSec);
			if (!useripsec_dict && systemOptions) {
				useripsec_dict = CFDictionaryGetValue(systemOptions, kSCEntNetIPSec);
				useripsec_dict_fromsystem = 1;
			}

			if (!useripsec_dict && !opt_ipsecsharedsecret[0]) {
				error("L2TP: no user shared secret found.\n");
				devstatus = EXIT_L2TP_NOSHAREDSECRET;
				goto fail;
			}

			if (useripsec_dict) {
				/* XXX as a simplification, the authentication method is set in the main dictionary
					instead of having an array of proposals with one proposal with the
					requested authentication method
				  */
				auth_method = CFDictionaryGetValue(useripsec_dict, kRASPropIPSecAuthenticationMethod);
				if (!isString(auth_method) || CFEqual(auth_method, kRASValIPSecAuthenticationMethodSharedSecret)) {
					auth_method = kRASValIPSecAuthenticationMethodSharedSecret;
					secret_string = CFDictionaryGetValue(useripsec_dict, kRASPropIPSecSharedSecret);
					if (!isString(secret_string) &&
						!(isData(secret_string) && ((CFDataGetLength((CFDataRef)secret_string) % sizeof(UniChar)) == 0))) {
						error("L2TP: incorrect user shared secret found.\n");
						devstatus = EXIT_L2TP_NOSHAREDSECRET;
						goto fail;
					}
					if (useripsec_dict_fromsystem) {
						secret_encryption_string = CFDictionaryGetValue(useripsec_dict, kRASPropIPSecSharedSecretEncryption);
						if (secret_encryption_string && !isString(secret_encryption_string)) {
							error("L2TP: incorrect secret encyption found.\n");
							goto fail;
						}
					} 

				}
				else if (CFEqual(auth_method, kRASValIPSecAuthenticationMethodCertificate)) {
					certificate = CFDictionaryGetValue(useripsec_dict, kRASPropIPSecLocalCertificate);
					if (!isData(certificate)) {
						devstatus = EXIT_L2TP_NOCERTIFICATE;
						error("L2TP: no user certificate  found.\n");
						goto fail;
					}
				}
				else {
					error("L2TP: incorrect authentication method.\n");
					goto fail;
				}

				localidentifier_string = CFDictionaryGetValue(useripsec_dict, kRASPropIPSecLocalIdentifier);
				if (localidentifier_string && !isString(localidentifier_string)) {
					error("L2TP: incorrect local identifier found.\n");
					goto fail;
				}

					verify_id = CFDictionaryGetValue(useripsec_dict, kRASPropIPSecIdentifierVerification);
					if (verify_id && !isString(verify_id)) {
						error("L2TP: incorrect identifier verification found.\n");
						goto fail;
					}


			}

			if (ipsec_dict) {
				CFRelease(ipsec_dict);
				ipsec_dict = NULL;
			}
			
			ipsec_dict = IPSecCreateL2TPDefaultConfiguration(&our_address, &peer_address, 
				(host_name_specified ? remoteaddress : NULL),
				auth_method, 1, 0, verify_id); 
	
			if (!ipsec_dict) {
				error("L2TP: cannot create L2TP configuration.\n");
				goto fail;
			}
			
			if (secret_string) {
				if (isString(secret_string))
					CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecret, secret_string);
				else {
					CFStringEncoding	encoding;
					CFDataRef			secret_data	= (CFDataRef)secret_string;

#if     __BIG_ENDIAN__
					encoding = (*(CFDataGetBytePtr(secret_data) + 1) == 0x00) ? kCFStringEncodingUTF16LE : kCFStringEncodingUTF16BE;
#else   // __LITTLE_ENDIAN__
					encoding = (*(CFDataGetBytePtr(secret_data)    ) == 0x00) ? kCFStringEncodingUTF16BE : kCFStringEncodingUTF16LE;
#endif
					secret_string = CFStringCreateWithBytes(NULL, (const UInt8 *)CFDataGetBytePtr(secret_data), CFDataGetLength(secret_data), encoding, FALSE);
					CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecret, secret_string);
					CFRelease(secret_string);
				}
				if (secret_encryption_string) {
					CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecretEncryption, secret_encryption_string);
				}
			}
			else if (certificate) {
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecLocalCertificate, certificate);
			}
			else {
				/* set the authentication information */
				secret_string = CFStringCreateWithCString(0, opt_ipsecsharedsecret, kCFStringEncodingUTF8);
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecret, secret_string);
				if (!strcmp(opt_ipsecsharedsecrettype, "key")) 
					CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecretEncryption, kRASValIPSecSharedSecretEncryptionKey);
				else if (!strcmp(opt_ipsecsharedsecrettype, "keychain")) 
					CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecretEncryption, kRASValIPSecSharedSecretEncryptionKeychain);
				CFRelease(secret_string);
			}

			if (localidentifier_string) {
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecLocalIdentifier, localidentifier_string);
			}
			else if (opt_ipseclocalidentifier[0]) {
				/* set the local identifier information */
				localidentifier_string = CFStringCreateWithCString(0, opt_ipseclocalidentifier, kCFStringEncodingUTF8);
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecLocalIdentifier, localidentifier_string);
				CFRelease(localidentifier_string);
				localidentifier_string = NULL;
			}

			if (localidentifiertype_string) {
				CFDictionarySetValue(ipsec_dict, CFSTR("LocalIdentifierType"), localidentifiertype_string);
			}
			else if (opt_ipseclocalidentifiertype) {
				/* set the local identifier type information */
				if (!strcmp(opt_ipseclocalidentifiertype, "keyid")) 
					CFDictionarySetValue(ipsec_dict, CFSTR("LocalIdentifierType"), CFSTR("KeyID"));
				else if (!strcmp(opt_ipseclocalidentifiertype, "fqdn")) 
					CFDictionarySetValue(ipsec_dict, CFSTR("LocalIdentifierType"), CFSTR("FQDN"));
				else if (!strcmp(opt_ipseclocalidentifiertype, "user_fqdn")) 
					CFDictionarySetValue(ipsec_dict, CFSTR("LocalIdentifierType"), CFSTR("UserFQDN"));
				else if (!strcmp(opt_ipseclocalidentifiertype, "asn1dn")) 
					CFDictionarySetValue(ipsec_dict, CFSTR("LocalIdentifierType"), CFSTR("ASN1DN"));
				else if (!strcmp(opt_ipseclocalidentifiertype, "address")) 
					CFDictionarySetValue(ipsec_dict, CFSTR("LocalIdentifierType"), CFSTR("Address"));
			}
			if (debug) {
				// enable IPSec 'VerboseLogging' if ppp's is also enabled
				IPSecConfigureVerboseLogging(ipsec_dict, debug);
			}


			if (IPSecApplyConfiguration(ipsec_dict, &errstr)
				|| IPSecInstallPolicies(ipsec_dict, -1, &errstr)) {
				error("L2TP: cannot configure secure transport (%s).\n", errstr);
				goto fail;
			}

			/* now trigger IKE */
            while ((err = l2tp_trigger_ipsec(0, &ipsec_status)) &&
                   ipsec_status == RACOON_STARTED &&
                   num_connect_retries < num_alt_peer_address) {
                ipsec_status = 0;

                /* get the source address that will be used to reach the peer */
                if ((err = l2tp_change_peeraddress(ctrlsockfd, (struct sockaddr *)&alt_peer_address[num_connect_retries++]))) {
                    error("L2TP: cannot try alternate server...\n");
                    goto fail;
                }

                notice("L2TP connecting to alternate server '%s' (%s)...\n", remoteaddress, inet_ntoa(peer_address.sin_addr));
            }
        }

		if (err == 0) {
			err = l2tp_outgoing_call(ctrlsockfd, (struct sockaddr *)&peer_address, &our_params, &peer_params, opt_recv_timeout);

			/* setup the specific route */
			l2tp_set_peer_route();
		}

    } else {	            

        //-------------------------------------------------
        //	listen or answer mode
        //-------------------------------------------------   

        int 	listenfd = -1;          
        
        if (!strcmp(opt_mode, MODE_LISTEN)) {
            //-------------------------------------------------
            //	listen mode
            //		setup listen socket and listen for calls
            //-------------------------------------------------   
            struct sockaddr_in listen_address;
            
            notice("L2TP listening...\n");
    
            listenfd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_L2TP);
            if (listenfd < 0) {
                    error("Failed to open L2TP listening control socket: %m");
                    goto fail;
            }
    
            l2tp_set_flag(listenfd, kdebugflag & 1, L2TP_FLAG_DEBUG);
            l2tp_set_flag(listenfd, 1, L2TP_FLAG_CONTROL);
			l2tp_set_flag(listenfd, !opt_noipsec, L2TP_FLAG_IPSEC);
            l2tp_set_delegated_process(listenfd, getpid());  // must be set before calling l2tp_set_ouraddress

    
            /* bind the socket in the kernel with L2TP port */
            listen_address.sin_len = sizeof(peer_address);
            listen_address.sin_family = AF_INET;
            listen_address.sin_port = htons(L2TP_UDP_PORT);
            listen_address.sin_addr.s_addr = INADDR_ANY;
            l2tp_set_ouraddress(listenfd, (struct sockaddr *)&listen_address);
    
            our_address = listen_address;
    
            /* add security policies */
            if (!opt_noipsec) {
			
				CFStringRef				secret_string;

				if (ipsec_dict) 
					CFRelease(ipsec_dict);

				ipsec_dict = IPSecCreateL2TPDefaultConfiguration(&our_address, &peer_address, 
					(host_name_specified ? remoteaddress : NULL),
					kRASValIPSecAuthenticationMethodSharedSecret, 0, 0, 0); 

				/* set the authentication information */
				secret_string = CFStringCreateWithCString(0, opt_ipsecsharedsecret, kCFStringEncodingUTF8);
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecret, secret_string);
				if (!strcmp(opt_ipsecsharedsecrettype, "key")) 
					CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecretEncryption, kRASValIPSecSharedSecretEncryptionKey);
				else if (!strcmp(opt_ipsecsharedsecrettype, "keychain")) 
					CFDictionarySetValue(ipsec_dict, kRASPropIPSecSharedSecretEncryption, kRASValIPSecSharedSecretEncryptionKeychain);
				CFRelease(secret_string);

				if (debug) {
					// enable IPSec 'VerboseLogging' if ppp's is also enabled
					IPSecConfigureVerboseLogging(ipsec_dict, debug);
				}

				if (IPSecApplyConfiguration(ipsec_dict, &errstr)
					|| IPSecInstallPolicies(ipsec_dict, -1, &errstr)) {
					error("L2TP: cannot configure secure transport (%s).\n", errstr);
					goto fail;
				}
			}
			err = l2tp_trigger_ipsec(1, &ipsec_status);
            /* wait indefinitely and read the duplicated SCCRQ from the listen socket and ignore for now */
            if ((err = l2tp_recv(listenfd, control_buf, MAX_CNTL_BUFFER_SIZE, &rcvlen, (struct sockaddr*)&from, -1, "SCCRQ"))){
            	if (err == 0) {
                	setsockopt(ctrlsockfd, PPPPROTO_L2TP, L2TP_OPT_ACCEPT, 0, 0);
            	}
			}

            close(listenfd);
        }
        
		//-------------------------------------------------
        //	listen or answer mode
        //		process incoming connection		
        //-------------------------------------------------
        if (err == 0) {

			// log incoming call from l2tp_change_peeraddress() because that's when we know the peer address

           err = l2tp_incoming_call(ctrlsockfd, &our_params, &peer_params, opt_recv_timeout);
        }

		//remoteaddress = inet_ntoa(peer_address.sin_addr);

    }

    //-------------------------------------------------
    //	all modes
    //-------------------------------------------------
    if (err) {
        if (err != -2) {
            if (err != -1)
                devstatus = err;
            goto fail;
        }
        goto fail1;
    }
    
    notice("L2TP connection established.");

    /* start hello timer */
    if (opt_hello_timeout) {
        hello_timer_running = 1;
        TIMEOUT(l2tp_hello_timeout, 0, opt_hello_timeout);
    }

    /* open the data socket */
    datasockfd = socket(PF_PPP, SOCK_DGRAM, PPPPROTO_L2TP);
    if (datasockfd < 0) {
        error("Failed to open L2TP data socket: %m");
        goto fail;
    }

    l2tp_set_flag(datasockfd, 0, L2TP_FLAG_CONTROL);
    l2tp_set_flag(datasockfd, kdebugflag & 1, L2TP_FLAG_DEBUG);
    l2tp_set_flag(datasockfd, peer_params.seq_required, L2TP_FLAG_SEQ_REQ);
	l2tp_set_flag(datasockfd, !opt_noipsec, L2TP_FLAG_IPSEC);

    l2tp_set_ouraddress(datasockfd, (struct sockaddr *)&our_address);
    /* set the peer address of the data socket */
    /* on a data socket, this will find the socket of the corresponding control connection */
    l2tp_set_peeraddress(datasockfd, (struct sockaddr *)&peer_address);

    l2tp_set_ourparams(datasockfd, &our_params);
    l2tp_set_peerparams(datasockfd, &peer_params);
    l2tp_reset_timers(datasockfd, 0);
	
	baudrate = get_if_baudrate((char*)interface);
	l2tp_set_baudrate(datasockfd, baudrate);

    if (!strcmp(opt_mode, MODE_CONNECT)) {
        l2tp_init_session((char *)interface, sizeof(interface), &our_address.sin_addr, ppp_variable_echo_start);
        l2tp_set_nat_port_mapping();
    }

    return datasockfd;
 
fail:   

    status = EXIT_CONNECT_FAILED;
fail1:
    l2tp_close_fds();
    return -1;
}

/* ----------------------------------------------------------------------------- 
run the disconnector
----------------------------------------------------------------------------- */
void l2tp_disconnect()
{
    notice("L2TP disconnecting...\n");
    
    if (hello_timer_running) {
        UNTIMEOUT(l2tp_hello_timeout, 0);
        hello_timer_running = 0;
    }
        
    if (ctrlsockfd != -1) {

        /* send CDN and StopCCN only if this is a local disconnection 
           don't send them if the peer requested the disconnection */
        if (status) {
            /* send CDN message */
            our_params.result_code = L2TP_CALLRESULT_ADMIN;
            our_params.cause_code = 0;
            if (l2tp_send_CDN(ctrlsockfd, &our_params, &peer_params) == 0) {
                /* send StopCCN message */
                our_params.result_code = L2TP_CCNRESULT_GENERAL;
                our_params.cause_code = 0;
                l2tp_send_StopCCN(ctrlsockfd, &our_params);
            }
        }
    }

    l2tp_close_fds();

    ppp_auxiliary_probe_stop();
    l2tp_clear_nat_port_mapping();
    ppp_session_clear(&l2tp_session);
    session = NULL;

    notice("L2TP disconnected\n");
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_set_baudrate(int fd, u_int32_t baudrate)
{
	setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_BAUDRATE, &baudrate, 4);
    return 0;
}

/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int l2tp_set_delegated_process(int fd, int pid)
{
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_SETDELEGATEDPID, &pid, sizeof(pid));
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_set_ouraddress(int fd, struct sockaddr *addr)
{
    socklen_t optlen;

    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_OURADDRESS, addr, sizeof(*addr));
    /* get the address to retrieve the actual port used */
    optlen = sizeof(*addr);
    getsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_OURADDRESS, addr, &optlen);
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_set_peeraddress(int fd, struct sockaddr *addr)
{

    if (setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_PEERADDRESS, addr, sizeof(*addr))) {
        error("L2TP can't set L2TP server address...\n");
        return -1;
    }
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_new_tunnelid(int fd, u_int16_t *tunnelid)
{
    socklen_t optlen = 2;
	
    getsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_NEW_TUNNEL_ID, &tunnelid, &optlen);
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_set_ourparams(int fd, struct l2tp_parameters *our_params)
{
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_TUNNEL_ID, &our_params->tunnel_id, 2);
    /* session id is ignored for control connections */
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_SESSION_ID, &our_params->session_id, 2);
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_WINDOW, &our_params->window_size, 2);
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_set_peerparams(int fd, struct l2tp_parameters *peer_params)
{
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_PEER_TUNNEL_ID, &peer_params->tunnel_id, 2);
    /* session id is ignored for control connections */
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_PEER_SESSION_ID, &peer_params->session_id, 2);
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_PEER_WINDOW, &peer_params->window_size, 2);
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_change_peeraddress(int fd, struct sockaddr *peer)
{
    struct sockaddr_in src;
    int err = 0;
	char *errstr;

    if (peer->sa_len != peer_address.sin_len) {
        error("L2TP received an invalid server address...\n");
        return -1;
    }
    
    if (bcmp(&peer_address, peer, peer->sa_len)) {

        /* reset IPSec filters */
        if (!opt_noipsec) {
            if (!strcmp(opt_mode, MODE_CONNECT)) {
                IPSecRemoveConfiguration(ipsec_dict, &errstr);
                // security associations are base on IP addresses only
                // Wcast-align fix - use memcmp for unaligned compare
                if (memcmp(&((struct sockaddr_in *)(void*)peer)->sin_addr.s_addr, &((struct sockaddr_in *)(void*)&peer_address)->sin_addr.s_addr, sizeof(struct in_addr)))
                    IPSecRemoveSecurityAssociations((struct sockaddr *)&our_address, (struct sockaddr *)&peer_address);
                IPSecRemovePolicies(ipsec_dict, -1, &errstr);
            }
        }
    
        if (get_src_address((struct sockaddr *)&src, peer, ifscope, NULL)) {
            error("L2TP: cannot get our local address...\n");
            return -1;
        }
        
        /* the path to the peer has changed (beacuse it was unknown or because we use a different server) */
        if (src.sin_addr.s_addr != our_address.sin_addr.s_addr) {
            our_address = src;
            /* outgoing call use ephemeral ports, incoming call reuse L2TP_UDP_PORT */
            if (!strcmp(opt_mode, MODE_CONNECT))
                our_address.sin_port = htons(0);
            else 
                our_address.sin_port = htons(L2TP_UDP_PORT);
            l2tp_set_ouraddress(fd, (struct sockaddr *)&our_address);
        }
            
        bcopy(peer, &peer_address, peer->sa_len);        

        err = l2tp_set_peeraddress(fd, peer);

		if (!strncmp(opt_mode, MODE_ANSWER, strlen(opt_mode))
			|| !strncmp(opt_mode, MODE_LISTEN, strlen(opt_mode))) {
		
			remoteaddress = inet_ntoa(peer_address.sin_addr);
			notice("L2TP incoming call in progress from '%s'...", remoteaddress ? remoteaddress : "");
		}

        /* install new IPSec filters */
        if (!opt_noipsec) {
            if (!strcmp(opt_mode, MODE_CONNECT)) {

				CFStringRef				dst_string;
				CFMutableDictionaryRef	policy0;
				CFMutableArrayRef		policy_array;
				CFNumberRef				dst_port_num;
				int						val;
				
				dst_string = CFStringCreateWithCString(0, addr2ascii(AF_INET, &peer_address.sin_addr, sizeof(peer_address.sin_addr), 0), kCFStringEncodingASCII);
				val = ntohs(peer_address.sin_port); /* because there is no uint16 type */
				dst_port_num = CFNumberCreate(0, kCFNumberIntType, &val);

				CFDictionarySetValue(ipsec_dict, kRASPropIPSecRemoteAddress, dst_string);
				
				/* create the policies */
				policy_array = (CFMutableArrayRef)CFDictionaryGetValue(ipsec_dict, kRASPropIPSecPolicies);
				if (CFArrayGetCount(policy_array) > 1)
					CFArrayRemoveValueAtIndex(policy_array, 1);
					
				policy0 = (CFMutableDictionaryRef)CFArrayGetValueAtIndex(policy_array, 0);
				CFDictionarySetValue(policy0, kRASPropIPSecPolicyRemotePort, dst_port_num);
				CFArraySetValueAtIndex(policy_array, 0, policy0);
								
				CFDictionarySetValue(ipsec_dict, kRASPropIPSecPolicies, policy_array);

				CFRelease(dst_string);
				CFRelease(dst_port_num);
				
				if (IPSecApplyConfiguration(ipsec_dict, &errstr)
					|| IPSecInstallPolicies(ipsec_dict, -1, &errstr)) {
					error("L2TP: cannot reconfigure secure transport (%s).\n", errstr);
					return -1;
				}
			}
        }
    }
    
    return err;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_set_peer_route()
{
    SCNetworkReachabilityRef	ref;
    SCNetworkConnectionFlags	flags;
    bool 			is_peer_local;
    struct in_addr		gateway;

    if (peer_address.sin_addr.s_addr == 0)
        return -1;

    /* check if is peer on our local subnet */
    ref = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&peer_address);
    is_peer_local = SCNetworkReachabilityGetFlags(ref, &flags) && (flags & kSCNetworkFlagsIsDirect);
    CFRelease(ref);

    l2tp_set_host_gateway(RTM_DELETE, peer_address.sin_addr, ip_zeros, 0, 0);

    if (is_peer_local 
        || routeraddress[0] == 0
        || inet_aton((char*)routeraddress, &gateway) != 1) {
        
        if (interface[0]) {
            bzero(&gateway, sizeof(gateway));
            /* subnet route */
            l2tp_set_host_gateway(RTM_ADD, peer_address.sin_addr, gateway, (char*)interface, 1);
            peer_route_set = 2;
        }
    }
    else {
        /* host route */
        l2tp_set_host_gateway(RTM_ADD, peer_address.sin_addr, gateway, 0, 0);
        peer_route_set = 1;
    }
    
    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_clean_peer_route()
{

    if (peer_address.sin_addr.s_addr == 0)
        return -1;

    if (peer_route_set) {
	l2tp_set_host_gateway(RTM_DELETE, peer_address.sin_addr, ip_zeros, 0, peer_route_set == 1 ? 0 : 1);
        peer_route_set = 0;
    }

    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
void l2tp_ip_up(void *arg, uintptr_t p)
{

    if (peer_route_set == 2) {
        /* in the link local case, delete the route to the server, 
            in case it conflicts with the one from the ppp interface */
	l2tp_set_host_gateway(RTM_DELETE, peer_address.sin_addr, ip_zeros, 0, 0);
    }
}

/* ----------------------------------------------------------------------------- 
close the socket descriptors
----------------------------------------------------------------------------- */
void l2tp_close_fds()
{
	
    if (hello_timer_running) {
        UNTIMEOUT(l2tp_hello_timeout, 0);
        hello_timer_running = 0;
    }
    if (eventsockfd != -1) {
        close(eventsockfd);
        eventsockfd = -1;
    }
    if (datasockfd != -1) {
        close(datasockfd);
        datasockfd = -1;
    }
    if (ctrlsockfd >= 0) {
        close(ctrlsockfd);
        ctrlsockfd = -1;
    }
    if (racoon_ctrlsockfd >= 0) {
        close(racoon_ctrlsockfd);
        racoon_ctrlsockfd = -1;
    }
    
}

/* ----------------------------------------------------------------------------- 
clean up before quitting
----------------------------------------------------------------------------- */
void l2tp_cleanup()
{
	char *errstr;

    l2tp_close_fds();
    if (!opt_noipsec) {
             
		if (ipsec_dict) {
			IPSecRemoveConfiguration(ipsec_dict, &errstr);
			IPSecRemovePolicies(ipsec_dict, -1, &errstr);
			CFRelease(ipsec_dict);
			ipsec_dict = NULL;
		}
        if (strcmp(opt_mode, MODE_ANSWER)) {
            IPSecRemoveSecurityAssociations((struct sockaddr *)&our_address, (struct sockaddr *)&peer_address);
        }
    }
    l2tp_clean_peer_route();
}

/* ----------------------------------------------------------------------------- 
establish the socket as a ppp link
----------------------------------------------------------------------------- */
int l2tp_establish_ppp(int fd)
{
    int x, new_fd;

    if (ioctl(fd, PPPIOCATTACH, &x) < 0) {
        error("Couldn't attach socket to the link layer: %m");
        return -1;
    }

    new_fd = generic_establish_ppp(fd, interface);
    if (new_fd == -1)
        return -1;

    // add just the control socket
    // the data socket is just for moving data in the kernel
    add_fd(ctrlsockfd);	
    add_fd(eventsockfd);	
    return new_fd;
}

/* ----------------------------------------------------------------------------- 
dis-establish the socket as a ppp link
----------------------------------------------------------------------------- */
void l2tp_disestablish_ppp(int fd)
{
    int 	x;
    
    remove_fd(ctrlsockfd);
    remove_fd(eventsockfd);		

    if (ioctl(fd, PPPIOCDETACH, &x) < 0)
        error("Couldn't detach socket from link layer: %m");
        
    generic_disestablish_ppp(fd);
}

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
    hello timeout
----------------------------------------------------------------------------- */
static void l2tp_hello_timeout(void *arg)
{
    
    if (l2tp_send_hello(ctrlsockfd, &our_params)) {
        error("L2TP error on control channel sending Hello message\n");
        /* ???? */
    }

    TIMEOUT(l2tp_hello_timeout, 0, opt_hello_timeout);
}

/* -----------------------------------------------------------------------------
add/remove a host route
----------------------------------------------------------------------------- */
static boolean_t
l2tp_set_host_gateway(int cmd, struct in_addr host, struct in_addr gateway, char *ifname, int isnet)
{
    int 			len;
    int 			rtm_seq = 0;
    struct {
	struct rt_msghdr	hdr;
	struct sockaddr_in	dst;
	struct sockaddr_in	gway;
	struct sockaddr_in	mask;
	struct sockaddr_dl	link;
    } 				rtmsg;
    int 			sockfd = -1;

    if ((sockfd = socket(PF_ROUTE, SOCK_RAW, PF_ROUTE)) < 0) {
	syslog(LOG_INFO, "host_gateway: open routing socket failed, %s",
	       strerror(errno));
	return (FALSE);
    }

    memset(&rtmsg, 0, sizeof(rtmsg));
    rtmsg.hdr.rtm_type = cmd;
    rtmsg.hdr.rtm_flags = RTF_UP | RTF_STATIC;
    if (isnet)
        rtmsg.hdr.rtm_flags |= RTF_CLONING;
    else 
        rtmsg.hdr.rtm_flags |= RTF_HOST;
    if (gateway.s_addr)
        rtmsg.hdr.rtm_flags |= RTF_GATEWAY;
    rtmsg.hdr.rtm_version = RTM_VERSION;
    rtmsg.hdr.rtm_seq = ++rtm_seq;
    rtmsg.hdr.rtm_addrs = RTA_DST | RTA_NETMASK;
    rtmsg.dst.sin_len = sizeof(rtmsg.dst);
    rtmsg.dst.sin_family = AF_INET;
    rtmsg.dst.sin_addr = host;
    rtmsg.hdr.rtm_addrs |= RTA_GATEWAY;
    rtmsg.gway.sin_len = sizeof(rtmsg.gway);
    rtmsg.gway.sin_family = AF_INET;
    rtmsg.gway.sin_addr = gateway;
    rtmsg.mask.sin_len = sizeof(rtmsg.mask);
    rtmsg.mask.sin_family = AF_INET;
    rtmsg.mask.sin_addr.s_addr = 0xFFFFFFFF;

    len = sizeof(rtmsg);
    if (ifname) {
	rtmsg.link.sdl_len = sizeof(rtmsg.link);
	rtmsg.link.sdl_family = AF_LINK;
	rtmsg.link.sdl_nlen = MIN(strlen(ifname), sizeof(rtmsg.link.sdl_data));
	rtmsg.hdr.rtm_addrs |= RTA_IFP;
	bcopy(ifname, rtmsg.link.sdl_data, rtmsg.link.sdl_nlen);
    }
    else {
	/* no link information */
	len -= sizeof(rtmsg.link);
    }
    rtmsg.hdr.rtm_msglen = len;
    if (write(sockfd, &rtmsg, len) < 0) {
	syslog(LOG_DEBUG, "host_gateway: write routing socket failed, %s",
	       strerror(errno));
	close(sockfd);
	return (FALSE);
    }

    close(sockfd);
    return (TRUE);
}


/* -----------------------------------------------------------------------------
set or clear a flag 
----------------------------------------------------------------------------- */
int l2tp_set_flag(int fd, int set, u_int32_t flag)
{
    int			error;
	socklen_t	optlen;
    u_int32_t	flags;
    
    optlen = 4;
    error = getsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_FLAGS, &flags, &optlen);
    if (error == 0) {
        flags = set ? flags | flag : flags & ~flag;
        error = setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_FLAGS, &flags, 4);
    }
    return error;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void l2tp_reset_timers (int fd, int connect_mode)
{
    u_int16_t		timeout, timeoutcap, retries;

    /* use non adaptative time for initial packet */
    l2tp_set_flag(fd, !connect_mode, L2TP_FLAG_ADAPT_TIMER);

    if (connect_mode) {
        timeout = opt_connect_timeout;
        timeoutcap = opt_connect_timeout;
        retries = opt_connect_retrycount;
    }
    else {
        timeout = opt_timeout;
        timeoutcap = opt_timeoutcap;
        retries = opt_retrycount;
    }

    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_INITIAL_TIMEOUT, &timeout, 2);
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_TIMEOUT_CAP, &timeoutcap, 2);
    setsockopt(fd, PPPPROTO_L2TP, L2TP_OPT_MAX_RETRIES, &retries, 2);
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static void l2tp_link_failure ()
{
    // major change happen on the interface we are using.
    // disconnect L2TP 
    // Enhancement : should check if link is still usable
    notice("L2TP has detected change in the network and lost connection with the server.");
    devstatus = EXIT_L2TP_NETWORKCHANGED;
    status = EXIT_HANGUP;
    remove_fd(ctrlsockfd);
    remove_fd(eventsockfd);
    hungup = 1;
    lcp_lowerdown(0);	/* L2TP link is no longer available */
    link_terminated(0);
    ppp_auxiliary_probe_stop();
    l2tp_clear_nat_port_mapping();
    ppp_session_clear(&l2tp_session);
    session = NULL;
}


/* -----------------------------------------------------------------------------
 ----------------------------------------------------------------------------- */
int
l2tp_ip_probe_init (struct sockaddr_in *probe_addrs,
					int                *probe_fds,
					int                 num)
{
	int scope;
	
	if (!probe_addrs || !probe_fds || num < 3) {
		return -1;
	}
	
	scope = if_nametoindex((char *)interface);
	
	bzero(probe_addrs, (sizeof(*probe_addrs) * num));
	probe_addrs[GOOG_DNS_PROBE].sin_len = sizeof(struct in_addr);
	probe_addrs[GOOG_DNS_PROBE].sin_family = AF_INET;
	probe_addrs[GOOG_DNS_PROBE].sin_port = 0;
	probe_addrs[GOOG_DNS_PROBE].sin_addr.s_addr = GOOG_DNS_PROBE_ADDR_A; // google-public-dns-a.google.com
	if (peer_address.sin_family == AF_INET &&
		peer_address.sin_addr.s_addr) {
		bcopy(&peer_address, &probe_addrs[PEER_ADDR_PROBE], sizeof(probe_addrs[PEER_ADDR_PROBE]));
		if (num_alt_peer_address) {
			bcopy(&alt_peer_address[(arc4random() % num_alt_peer_address)], &probe_addrs[ALT_PEER_ADDR_PROBE], sizeof(probe_addrs[ALT_PEER_ADDR_PROBE]));
		}
	}
	probe_fds[GOOG_DNS_PROBE] = -1;
	probe_fds[PEER_ADDR_PROBE] = -1;
	probe_fds[ALT_PEER_ADDR_PROBE] = -1;
	return 0;
}

static char *l2tp_sd_name = "L2TP";
void
l2tp_init_session (char              *interface_name,
				   u_int32_t          interface_name_siz,
				   struct in_addr    *addr,
				   link_failure_func  func)
{
	ppp_session_clear(&l2tp_session);
	l2tp_session.sd_name = l2tp_sd_name;
	l2tp_session.interface_name = interface_name;
	l2tp_session.interface_name_siz = interface_name_siz;
	l2tp_session.interface_address.s_addr = addr->s_addr;
	l2tp_session.failure_func = func;
	//sess->probe_timer_running = 0;
	l2tp_ip_probe_init(l2tp_session.probe_addrs, l2tp_session.probe_fds, MAX_PROBE_ADDRS);
	l2tp_session.opt_noipsec = opt_noipsec;
	l2tp_session.valid = 1;
	session = &l2tp_session;
}
