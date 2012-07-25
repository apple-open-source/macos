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


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/route.h>
#include <net/if_types.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <netinet/bootp.h>
#include "pppd.h"
#include "fsm.h"
#include "pathnames.h"
#include "acsp.h"
#include "acscp.h"
#include "acscp_plugin.h"
#include <net/if.h> 		// required for if_ppp.h
#include "../../Family/if_ppp.h"
#include "../vpnd/RASSchemaDefinitions.h"
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCSchemaDefinitions.h>

#define ACSP_TIMEOUT_VALUE	3	/* seconds */

static acsp_ext *ext_list;		// list of acsp_ext structs - one for each option type	
//static struct acsp_plugin *plugin_list;	// list of plugin channels - not currently used

extern bool	acsp_no_routes;
extern bool	acsp_no_domains;

extern bool	acsp_use_dhcp;
extern bool	acsp_intercept_dhcp;


extern char *serverid; 	// defined in sys-MacOSX.c
extern char *serviceid; // defined in sys-MacOSX.c


static void acsp_start_plugin(acsp_ext *ext, int mtu);
static void acsp_stop_plugin(acsp_ext *ext);
static void acsp_output(acsp_ext *ext);
static void acsp_timeout(void *arg);
static void acsp_ipdata_input(int unit, u_char *pkt, int len, u_int32_t ouraddr, u_int32_t hisaddr);
static void acsp_ipdata_up(int unit, u_int32_t ouraddr, u_int32_t hisaddr);
static void acsp_ipdata_down(int unit);
static void acsp_ipdata_timeout(void *arg);
static int acsp_ipdata_print(u_char *, int, void (*) __P((void *, char *, ...)), void *);

// ACSP plugin channel functions - will eventually be called thru acsp_channel
static void acsp_plugin_check_options(void);
static int acsp_plugin_get_type_count(void);
static int acsp_plugin_init_type(acsp_ext *ext, int type_index);


//------------------------------------------------------------
//------------------------------------------------------------
// ACSP Protocol Functions
//------------------------------------------------------------
//------------------------------------------------------------

//------------------------------------------------------------
// acsp_init_plugins
//------------------------------------------------------------
void
acsp_init_plugins(void *arg, uintptr_t phase)
{
    acsp_ext	*ext;
    int		i, option_count;
    
    ext_list = 0;
		
    remove_notifier(&phasechange, acsp_init_plugins, 0);
    
    // load each plugin here and call start - is this the right place ???
    
    // for each plugin
    
    acsp_plugin_check_options();
    
    // call get_type_count
    option_count = acsp_plugin_get_type_count();
    
    // for each option the plugin supports - get the ext struct initialized
    for (i = 0; i < option_count; i++) {
        if ((ext = (acsp_ext*)malloc(sizeof(acsp_ext))) == 0) {
            error("acscp unable allocate plugin structures\n");
            acscp_protent.enabled_flag = 0;
            return;
        }
        
        if (acsp_plugin_init_type(ext, i)) {
            error("error initializing acsp plugin type\n");
            free(ext);
            continue;
        }
        
        ext->next = 0;
        ext->timer_state = ACSP_TIMERSTATE_STOPPED;
        ext->in.size = sizeof(ACSP_Input);
        ext->in.mtu = 0;
        ext->in.log_debug = 0;
        ext->in.log_error = 0;
        ext->out.size = sizeof(ACSP_Output);
        
        // add the ext struct to the list
        ext->next = ext_list;
        ext_list = ext;
    }
	
	// add hook for dhcp/ipdata
	// will use route and domain information from acsp
	ipdata_input_hook = acsp_ipdata_input;
	ipdata_up_hook = acsp_ipdata_up;
	ipdata_down_hook = acsp_ipdata_down;
	ipdata_print_hook = acsp_ipdata_print;

}

//------------------------------------------------------------
// acsp_start_plugin
//------------------------------------------------------------
void
acsp_start(int mtu)
{
    acsp_ext	*ext;
    
	// acscp is negotiated, stop dhcp
	acsp_ipdata_down(0);

    // let the plugins do their thing
    ext = ext_list;    
    while (ext) {
        acsp_start_plugin(ext, mtu);
        ext = ext->next;
    }

}

//------------------------------------------------------------
// acsp_start_plugin
//------------------------------------------------------------
void
acsp_stop(void)
{
    acsp_ext	*ext;
   
    // stop the plugins
    ext = ext_list;
    while (ext) {
        acsp_stop_plugin(ext);
        ext = ext->next;
    }
}

//------------------------------------------------------------
// acsp_start_plugin
//------------------------------------------------------------
static void
acsp_start_plugin(acsp_ext *ext, int mtu)
{
    ext->in.mtu = mtu;
    ext->in.notification = ACSP_NOTIFICATION_START;
    ext->in.data_len = 0;
    ext->in.data = 0;
    ext->out.action = ACSP_ACTION_NONE;
    ext->out.data_len = 0;
    ext->out.data = 0;
    ext->process(ext->context, &ext->in, &ext->out);
    
    acsp_output(ext);
}

//------------------------------------------------------------
// acsp_stop_plugin
//------------------------------------------------------------
static void
acsp_stop_plugin(acsp_ext *ext)
{
    if (ext->timer_state != ACSP_TIMERSTATE_STOPPED) {
        UNTIMEOUT(acsp_timeout, ext);
        ext->timer_state = ACSP_TIMERSTATE_STOPPED;
    }
    ext->in.notification = ACSP_NOTIFICATION_STOP;
    ext->in.data_len = 0;
    ext->in.data = 0;
    ext->out.action = ACSP_ACTION_NONE;
    ext->out.data_len = 0;
    ext->out.data = 0;
    ext->process(ext->context, &ext->in, &ext->out);
}

/* Wcast-align fix - The input and output buffers used by pppd are aligned(4)
 * which makes them still aligned(4) after the 4 byte ppp header is removed.
 * These routines expect the packet buffer to have such alignment.
 */
 
//------------------------------------------------------------
// acsp_data_input
//------------------------------------------------------------
void 
acsp_data_input(int unit, u_char *packet, int len)
{
    
    acsp_ext 	*ext;
    acsp_packet *pkt = ALIGNED_CAST(acsp_packet*)packet;    
    
    if (len < ACSP_HDR_SIZE) {
        error("ACSP packet received was too short\n");
        return;		// discard the packet
    }
    
    // find the option struct and plugin for this packet
    for (ext = ext_list; ext != 0; ext = ext->next)
        if (ext->type == pkt->type)
            break;

    if (ext == 0) {
        error("ACSP packet received with invalid type\n");
        return;
    }

    if (ntohs(pkt->flags) & ACSP_FLAG_ACK && ext->timer_state == ACSP_TIMERSTATE_PACKET && pkt->seq == ext->last_seq) {
            UNTIMEOUT(acsp_timeout, ext);
            ext->timer_state = ACSP_TIMERSTATE_STOPPED;
    }
    ext->in.notification = ACSP_NOTIFICATION_PACKET;
    ext->in.data = pkt;
    ext->in.data_len = len;
    ext->out.data = 0;
    ext->out.data_len = 0;
    ext->out.action = ACSP_ACTION_NONE;
    
    ext->process(ext->context, &ext->in, &ext->out);
    acsp_output(ext);
}

//------------------------------------------------------------
// acsp_output
//------------------------------------------------------------
static void
acsp_output(acsp_ext *ext)
{
    int 	done = 0;
    u_int8_t	*ptr;

    while (!done) {
        done = 1;
        switch (ext->out.action) {
            case ACSP_ACTION_NONE:
                break;
                
            case ACSP_ACTION_SEND:
            case ACSP_ACTION_SEND_WITH_TIMEOUT:
                if (ext->out.data_len > ext->in.mtu || ext->out.data_len < ACSP_HDR_SIZE) {
                    error("ACSP plugin for option # %d trying to send packet of invalid length\n", ext->type);
                    ext->in.notification = ACSP_NOTIFICATION_ERROR;
                    ext->in.data = 0;
                    ext->in.data_len = 0;
                    ext->out.data = 0;
                    ext->out.data_len = 0;
                    ext->out.action = ACSP_ACTION_NONE;
                    done = 0;
                    ext->process(ext->context, &ext->in, &ext->out);
                } else {
                    ptr = ext->out.data;	// add address, control, and proto
                    *ptr++ = 0xff;
                    *ptr++ = 0x03;
                    *(ALIGNED_CAST(u_int16_t*)ptr) = htons(PPP_ACSP);
                    ptr += 2;
                    
                    if (ext->out.action == ACSP_ACTION_SEND_WITH_TIMEOUT) {
                        if (ext->timer_state != ACSP_TIMERSTATE_STOPPED)
                            UNTIMEOUT(acsp_timeout, ext);
                        ext->timer_state = ACSP_TIMERSTATE_PACKET;
                        ext->last_seq = (ALIGNED_CAST(acsp_packet*)ptr)->seq; 
                        TIMEOUT(acsp_timeout, ext, ACSP_TIMEOUT_VALUE);
                    }                    
                    output(0, ext->out.data, ext->out.data_len);
                    if (ext->free)
                        ext->free(ext->context, &ext->out);

                }
                break;
                
            case ACSP_ACTION_INVOKE_UI:
                error("ACSP plugin for option # %d attempted to invoke UI - not supported\n", ext->type);
                ext->in.notification = ACSP_NOTIFICATION_ERROR;
                ext->in.data = 0;
                ext->in.data_len = 0;
                ext->out.data = 0;
                ext->out.data_len = 0;
                ext->out.action = ACSP_ACTION_NONE;
                done = 0;
                ext->process(ext->context, &ext->in, &ext->out);
                break;
                           
            case ACSP_ACTION_SET_TIMEOUT:
                if (ext->out.data_len != 4) {
                    error("ACSP plugin for option # %d attempted timeout action with invalid time value\n", ext->type);
                    ext->in.notification = ACSP_NOTIFICATION_ERROR;
                    ext->in.data = 0;
                    ext->in.data_len = 0;
                    ext->out.data = 0;
                    ext->out.data_len = 0;
                    ext->out.action = ACSP_ACTION_NONE;
                    done = 0;
                    ext->process(ext->context, &ext->in, &ext->out);
                } else {
                    ext->timer_state = ACSP_TIMERSTATE_GENERAL;
                    TIMEOUT(acsp_timeout, ext, *((int*)ext->out.data));
                }
                break;

            case ACSP_ACTION_CANCEL_TIMEOUT:
                if (ext->timer_state != ACSP_TIMERSTATE_STOPPED) {
                    UNTIMEOUT(acsp_timeout, ext);
                    ext->timer_state = ACSP_TIMERSTATE_STOPPED;
                }
                break;
                            
            default:
                error("ACSP plugin for option # %d trying to perform invalid action\n", ext->type);
                ext->in.notification = ACSP_NOTIFICATION_ERROR;
                ext->in.data = 0;
                ext->in.data_len = 0;
                ext->out.data = 0;
                ext->out.data_len = 0;
                ext->out.action = ACSP_ACTION_NONE;
                done = 0;
                ext->process(ext->context, &ext->in, &ext->out);
                break;
        } // switch
    } // while
}


//------------------------------------------------------------
// acsp_timeout
//------------------------------------------------------------
static void
acsp_timeout(void *arg)
{
    acsp_ext	*ext = (acsp_ext*)arg;
    
    ext->timer_state = ACSP_TIMERSTATE_STOPPED;
    ext->in.notification = ACSP_NOTIFICATION_TIMEOUT;
    ext->in.data_len = 0;
    ext->in.data = 0;
    ext->out.action = ACSP_ACTION_NONE;
    ext->out.data_len = 0;
    ext->out.data = 0;
    ext->process(ext->context, &ext->in, &ext->out);
    
    acsp_output(ext);
}    

//------------------------------------------------------------
//------------------------------------------------------------
//  Plugin stuff - to be moved to plugin eventually
//------------------------------------------------------------
//------------------------------------------------------------

#define STR_BUFSIZE			1024
#define ACSP_PLUGIN_MAX_RETRIES		10

//
// plugin state
//
#define	PLUGIN_STATE_NONE		0	/* no state set */
#define PLUGIN_STATE_INITIAL		1	/* setup but not started - ACSP not up */
#define PLUGIN_RCVSTATE_LISTEN		2	/* listening for incomming data */
#define PLUGIN_RCVSTATE_WAITING_PKT	3	/* waiting for more data - partial data recvd */
#define PLUGIN_SENDSTATE_WAITING_ACK	4	/* waiting for ack for last packet sent */
#define PLUGIN_STATE_DONE		5	/* done sending */

//
// modes to indicate sending or receiving
//
#define PLUGIN_MODE_NONE		0
#define PLUGIN_MODE_RCV			1
#define PLUGIN_MODE_SEND		2

#define ACSP_ROUTEFLAGS_PRIVATE		0x0001
#define ACSP_ROUTEFLAGS_PUBLIC		0x0002

//
// route struct to hold 1 route for sending or
// a route received to be installed
//
typedef struct acsp_route {
    struct acsp_route		*next;
    struct in_addr		address;
    struct in_addr		mask;
    struct in_addr		router;
    u_int16_t			flags;
    int				installed;
} acsp_route;

// structure of route data in a packet
typedef struct acsp_route_data {
    u_int32_t		address;
    u_int32_t		mask;
    u_int16_t		flags;
    u_int16_t		reserved;
} acsp_route_data;

//
// domain struct to hold 1 domain name for sending or
// a domain name recieved to be installed
//
typedef struct acsp_domain {
    struct acsp_domain		*next;
    struct in_addr		server;
    char			*name;
} acsp_domain;

// structure of domain data in a packet
#define ACSP_DOMAIN_DATA_HDRSIZE    6
typedef struct acsp_domain_data {
    u_int32_t	server;
    u_int16_t	len;
    char	name[1];
} acsp_domain_data;

//
// plugin context - holds all state for a particular
// option type
//
typedef struct acsp_plugin_context {
    u_int8_t		type;
    int			mode;		// indicates client or server side
    int			state;
    u_int8_t		next_seq;
    int			ip_up;
    // the following is used only by the server side
    int			retry_count;
    void		*next_to_send;
    char		*buf;
    u_int16_t		buf_size;
    u_int16_t		last_pkt_len;
    void		*list;
    int			config_installed;
} acsp_plugin_context;

typedef struct acsp_dhcp_context {
    int				state;
    int				retry_count;
    void			*route;
    char			*domain;
    struct in_addr	netmask;
    int				config_installed;
	int				unit;
    struct in_addr	ouraddr;
    struct in_addr	hisaddr;
} acsp_dhcp_context;

//
// globals
//

// lists of routes and domains read from the config file
// these lists are placed in the context for the accociated
// option type when the context is created and intialized
static acsp_route 	*routes_list = 0;		
static acsp_domain 	*domains_list = 0;	
static struct in_addr	primary_router = { 0 };		// address of primary router (before PPP connection)
static u_int32_t	subnet_mask = 0;

static acsp_dhcp_context *dhcp_context = 0;

extern CFStringRef 		serviceidRef;
extern SCDynamicStoreRef	cfgCache;
extern int publish_dns_wins_entry(CFStringRef entity, CFStringRef property1, CFTypeRef ref1, CFTypeRef ref1a, 
						CFStringRef property2, CFTypeRef ref2,
						CFStringRef property3, CFTypeRef ref3, int clean);
extern int route_interface(int cmd, struct in_addr host, struct in_addr mask, char iftype, char *ifname, int is_host);
extern int route_gateway(int cmd, struct in_addr dest, struct in_addr mask, struct in_addr gateway, int use_gway_flag);

//
// funtion prototypes
//
static void acsp_plugin_ip_up(void *arg, uintptr_t phase);
static void acsp_plugin_ip_down(void *arg, uintptr_t phase);
static void acsp_plugin_install_config(acsp_plugin_context *context);
static void acsp_plugin_remove_config(acsp_plugin_context *context);
static void acsp_plugin_read_routes(CFPropertyListRef serverRef);
static void acsp_plugin_read_domains(CFPropertyListRef serverRef);
static void acsp_plugin_read_domain_from_store(void);
static int acsp_plugin_alloc_context(acsp_ext *ext, u_int8_t type);
static void acsp_plugin_send(acsp_plugin_context* context, ACSP_Input* acsp_in, ACSP_Output* acsp_out);
static void acsp_plugin_resend(acsp_plugin_context* context, ACSP_Input* acsp_in, ACSP_Output* acsp_out);
static void acsp_plugin_send_ack(acsp_plugin_context* context, ACSP_Input* acsp_in, ACSP_Output* acsp_out);
static int acsp_plugin_read(acsp_plugin_context* context, ACSP_Input* acsp_in);
static void acsp_plugin_clear_list(acsp_plugin_context* context);
static void acsp_plugin_dispose __P((void *context));
static void acsp_plugin_process __P((void *context, ACSP_Input *acsp_in, ACSP_Output *acsp_out));
static void acsp_plugin_print_packet __P((void (*printer)(void *, char *, ...), void *arg, u_char code, char *inbuf, int insize));

static void acsp_plugin_add_routes(acsp_route *route);
static void acsp_plugin_add_domains(acsp_domain	*domain);
static void acsp_plugin_remove_routes(acsp_route *route);

//------------------------------------------------------------
// *** Plugin functions called thru Plugin Channel ***
//	Used to init the plugin and init the ext structs
//	for each option type supported
//------------------------------------------------------------

//------------------------------------------------------------
// acsp_plugin_check_options
//	read options from the config file
//------------------------------------------------------------
static void acsp_plugin_check_options(void)
{

    SCPreferencesRef 	prefs;
    CFPropertyListRef	ref; 
    CFStringRef		idRef;
    char			sopt[32];
    CFTypeRef		dictRef;
    CFStringRef		key;
    CFDataRef		strRef;
    CFPropertyListRef	servers_list;

      
    routes_list = 0;
    domains_list = 0;
      
    if (serverid == 0) {	// client side
        if (acsp_no_routes == 0)			// command line disable ?
            acscp_wantoptions[0].neg_routes = 1;	// default setting for client
        if (acsp_no_domains == 0) 			// command line disable ?		
            acscp_wantoptions[0].neg_domains = 1;	// default setting for client
            
        // get the primary interface router address
        sopt[0] = 0;
        key = SCDynamicStoreKeyCreateNetworkGlobalEntity(0, kSCDynamicStoreDomainState, kSCEntNetIPv4);
        if (key) {
            if (dictRef = SCDynamicStoreCopyValue(cfgCache, key)) {
                strRef = CFDictionaryGetValue(dictRef, kSCPropNetIPv4Router);
                if (strRef && (CFGetTypeID(strRef) == CFStringGetTypeID())) {
                    CFStringGetCString((CFStringRef)strRef, sopt, 32, kCFStringEncodingUTF8);
                }
                CFRelease(dictRef);
            }
            CFRelease(key);
        }        
        if (sopt[0])
            inet_aton(sopt, &primary_router);
        else
            primary_router.s_addr = 0;
    } else {
        /* open the prefs file */
        if ((prefs = SCPreferencesCreate(0, CFSTR("pppd"), kRASServerPrefsFileName)) != 0) {
            // get servers list from the plist
            if ((servers_list = SCPreferencesGetValue(prefs, kRASServers)) != 0) {
                if ((idRef = CFStringCreateWithCString(0, serverid , kCFStringEncodingMacRoman)) != 0) {
                    if ((ref = CFDictionaryGetValue(servers_list, idRef)) != 0) {
                        if (acsp_no_routes == 0)
                            acsp_plugin_read_routes(ref);	// get routes option configuration
                        if (acsp_no_domains == 0)
                            acsp_plugin_read_domains(ref);	// get domains option configuration
                    }
                    CFRelease(idRef);
                }
                CFRelease(prefs);
            }
        }
    }
}

//------------------------------------------------------------
// acsp_plugin_read_routes
//	read routes option configuration
//------------------------------------------------------------
static void acsp_plugin_read_routes(CFPropertyListRef serverRef)
{

    CFDictionaryRef		dict;
    CFStringRef			stringRef;
    CFArrayRef			addrArrayRef, maskArrayRef, typeArrayRef;
    int				addrCount, maskCount, typeCount, i;
    char			addrStr[STR_BUFSIZE], maskStr[STR_BUFSIZE];
    acsp_route			*route;
    struct in_addr 		outAddr, outMask;
    u_int32_t			flags;
        
    // routes option parameters                
    dict = CFDictionaryGetValue(serverRef, kRASEntIPv4);
    if (dict && CFGetTypeID(dict) == CFDictionaryGetTypeID()) {
        // get the route address array
        addrArrayRef  = CFDictionaryGetValue(dict, kRASPropIPv4OfferedRouteAddresses);
        if (addrArrayRef && CFGetTypeID(addrArrayRef) == CFArrayGetTypeID()) {
            addrCount = CFArrayGetCount(addrArrayRef);
            // get subnet mask array
            maskArrayRef  = CFDictionaryGetValue(dict, kRASPropIPv4OfferedRouteMasks);
            if (maskArrayRef && CFGetTypeID(maskArrayRef) == CFArrayGetTypeID()) {
                maskCount = CFArrayGetCount(addrArrayRef);
                // get route type array
                typeArrayRef  = CFDictionaryGetValue(dict, kRASPropIPv4OfferedRouteTypes);
                if (typeArrayRef && CFGetTypeID(typeArrayRef) == CFArrayGetTypeID()) {
                    typeCount = CFArrayGetCount(typeArrayRef);

                    if (addrCount != maskCount || addrCount != typeCount) {
                        error("ACSP plugin: while reading prefs - route address, mask, and type counts not equal\n");
                        return;
                    }
                    acscp_allowoptions[0].neg_routes = 1;	// found routes - set negotiate flag

                    // get address and mask for each route
                    for (i = 0; i < addrCount; i++) {
                        stringRef = CFArrayGetValueAtIndex(addrArrayRef, i);
                        addrStr[0] = 0;
                        CFStringGetCString(stringRef, addrStr, STR_BUFSIZE, kCFStringEncodingUTF8);
                        // get mask
                        stringRef = CFArrayGetValueAtIndex(maskArrayRef, i);
                        maskStr[0] = 0;
                        CFStringGetCString(stringRef, maskStr, STR_BUFSIZE, kCFStringEncodingUTF8);
                        // get route type
                        stringRef = CFArrayGetValueAtIndex(typeArrayRef, i);
                        if (CFStringCompare(stringRef, kRASValIPv4OfferedRouteTypesPrivate, 0) == kCFCompareEqualTo)
                            flags = ACSP_ROUTEFLAGS_PRIVATE;
                        else if(CFStringCompare(stringRef, kRASValIPv4OfferedRouteTypesPublic, 0) == kCFCompareEqualTo)
                            flags = ACSP_ROUTEFLAGS_PUBLIC;
                        else {
                            error("ACSP plugin: invalid route type specified\n");
                            acscp_allowoptions[0].neg_routes = 0;
                            break;
                        }
                        // allocate the structure
                        if ((route = (acsp_route*)malloc(sizeof(acsp_route))) == 0) {
                            error("ACSP plugin: no memory\n");
                            acscp_allowoptions[0].neg_routes = 0;
                            break;
                        }         
						bzero(route, sizeof(acsp_route));                   
                        // convert
                        if (inet_aton(addrStr, &outAddr) == 0 || inet_aton(maskStr, &outMask) == 0) {
                            error("ACSP plugin: invalid ip address or mask specified\n");
                            free(route);
                            acscp_allowoptions[0].neg_routes = 0;
                            return;
                        }
                        route->address = outAddr;
                        route->mask = outMask;
                        route->flags = flags;
                        route->installed = 0;
                        
                        route->next = routes_list;
                        routes_list = route;
                    }
                    // check if we really have something to send
                    if (routes_list == 0)
                        acscp_allowoptions[0].neg_routes = 0;
                }
            }
        }
    }
}

//------------------------------------------------------------
// acsp_plugin_read_domains
//	read the domains option configuration
//------------------------------------------------------------
static void acsp_plugin_read_domains(CFPropertyListRef serverRef)
{
    CFDictionaryRef		dict;
    CFStringRef			stringRef;
    CFArrayRef			nameArrayRef, serverArrayRef;
    int				nameCount, serverCount, outlen, i;
    char			str[STR_BUFSIZE];
    acsp_domain			*domain;
    struct in_addr		outAddr;
    
    // domains option parameters                
    dict = CFDictionaryGetValue(serverRef, kRASEntDNS);
    if (dict && CFGetTypeID(dict) == CFDictionaryGetTypeID()) {                
        // get the domain names aray
        nameArrayRef  = CFDictionaryGetValue(dict, kRASPropDNSOfferedSearchDomains);
        if (nameArrayRef && CFGetTypeID(nameArrayRef) == CFArrayGetTypeID()) {
            nameCount = CFArrayGetCount(nameArrayRef);
            // get the search domain server addresses if present
            serverArrayRef  = CFDictionaryGetValue(dict, kRASPropDNSOfferedSearchDomainServers);
            if (serverArrayRef && CFGetTypeID(serverArrayRef) == CFArrayGetTypeID()) {
                serverCount = CFArrayGetCount(serverArrayRef);
                if (serverArrayRef && (serverCount != nameCount)) {
                    error("ACSP plugin: search domain count not equal to search domain server count\n");
                    return;
                }
            } else
                serverCount = 0;
            
            acscp_allowoptions[0].neg_domains = 1;	// enable negotiation of this option
            if (nameCount == 0)
               acsp_plugin_read_domain_from_store();
            else {
                for (i = 0; i < nameCount; i++) {
                    stringRef = CFArrayGetValueAtIndex(nameArrayRef, i);
                    str[0] = 0;
                    CFStringGetCString(stringRef, str, STR_BUFSIZE, kCFStringEncodingUTF8);
                    outlen = strlen(str);
                    if (outlen) {
                        if ((domain = (acsp_domain*)malloc(sizeof(acsp_domain))) == 0) {
                            error("ACSP plugin: no memory\n");
                            acscp_allowoptions[0].neg_domains = 0;
                            break;
                        }
                        if ((domain->name = malloc(outlen + 1)) == 0) {
                            error("ACSP plugin: no memory\n");
                            acscp_allowoptions[0].neg_domains = 0;
                            free(domain);
                            break;
                        }
                        memcpy(domain->name, str, outlen + 1);
                        if (serverCount) {
                            stringRef = CFArrayGetValueAtIndex(serverArrayRef, i);
                            str[0] = 0;
                            CFStringGetCString(stringRef, str, STR_BUFSIZE, kCFStringEncodingUTF8);
                            if (inet_aton(str, &outAddr) == 0) {
                                error("ACSP plugin: invalid ip address specified for DNS server\n");
                                free(domain);
                                acscp_allowoptions[0].neg_domains = 0;
                                return;
                            }
                            domain->server = outAddr;
                        } else
                            domain->server.s_addr = 0;
                        domain->next = domains_list;
                        domains_list = domain;
                    }
                }
            }
            // check if we really have something to send
            if (domains_list == 0)
                acscp_allowoptions[0].neg_domains = 0;
        }
    }
}

//------------------------------------------------------------
// acsp_plugin_get_type_count
//	returns the number of types supported by the plugin
//------------------------------------------------------------
static void acsp_plugin_read_domain_from_store(void)
{
    SCDynamicStoreRef 	storeRef; 
    CFPropertyListRef	ref = 0;
    CFStringRef		key = 0;
    CFStringRef		stringRef;
    char 		str[STR_BUFSIZE];
    int			len;
    acsp_domain		*domain;

    storeRef = SCDynamicStoreCreate(0, CFSTR("pppd"), 0, 0);    
    if (storeRef) {                                            
        key = SCDynamicStoreKeyCreateNetworkGlobalEntity(0, kSCDynamicStoreDomainState, kSCEntNetDNS);
        if (key) {
            ref = SCDynamicStoreCopyValue(storeRef, key);
            if (ref && CFGetTypeID(ref) == CFDictionaryGetTypeID()) {
                stringRef = CFDictionaryGetValue(ref, kSCPropNetDNSDomainName); 
                if (stringRef && CFGetTypeID(stringRef) == CFStringGetTypeID()) {
                    str[0] = 0;
                    CFStringGetCString(stringRef, str, STR_BUFSIZE, kCFStringEncodingUTF8);
                    len = strlen(str);
                    if (len) {
                        if ((domain = (acsp_domain*)malloc(sizeof(acsp_domain))) == 0) {
                            error("ACSP plugin: no memory\n");
                        } else if ((domain->name = malloc(len + 1)) == 0) {
                            error("ACSP plugin: no memory\n");
                            free(domain);
                        } else {
                            memcpy(domain->name, str, len + 1);
                            domain->server.s_addr = 0;
                            domain->next = domains_list;
                            domains_list = domain;
                        }
                    }
                }
            }
            CFRelease(key);
            if (ref)
                CFRelease(ref);
        }
        CFRelease(storeRef);
    }
}            

//------------------------------------------------------------
// acsp_plugin_get_type_count
//	returns the number of types supported by the plugin
//------------------------------------------------------------
static int acsp_plugin_get_type_count(void)
{
    return 2;
}	

//------------------------------------------------------------
// acsp_plugin_init_type
//	init the ext struct for an option type
//	the option being inited is determined by the index
//	the function get_type_count() is used to get the
//	number of option types the plugin supports
//------------------------------------------------------------
static int acsp_plugin_init_type(acsp_ext *ext, int type_index)
{
    if (type_index == 0) {
        ext->type = CI_ROUTES;
        if (acsp_plugin_alloc_context(ext, CI_ROUTES) != 0)
            return -1;
        ((acsp_plugin_context*)(ext->context))->list = routes_list;	// save the list read from prefs into the context
    } else if (type_index == 1) {
        ext->type = CI_DOMAINS;
        if (acsp_plugin_alloc_context(ext, CI_DOMAINS) != 0)
            return -1;
        ((acsp_plugin_context*)(ext->context))->list = domains_list;	// save the list read from prefs into the context
    } else
        return -1;	// bad index
    
    // we want the routes notifier to fire first, then the domains
    if (ext->type == CI_DOMAINS)
        add_notifier_last(&ip_up_notify, acsp_plugin_ip_up, ext->context);
    else 
        add_notifier(&ip_up_notify, acsp_plugin_ip_up, ext->context);
    add_notifier(&ip_down_notify, acsp_plugin_ip_down, ext->context);

    return 0;
}

//------------------------------------------------------------
// acsp_plugin_alloc_context
//	allocate a context for the specified type
//------------------------------------------------------------
static int acsp_plugin_alloc_context(acsp_ext *ext, u_int8_t type)
{
    acsp_plugin_context* context;
    
    if ((context = (acsp_plugin_context*)malloc(sizeof(acsp_plugin_context))) == 0) {
        error("ACSP plugin: no memory\n");
        return -1;
    }
    // allocte buffer using max mtu since nothing has been negotiated yet
    if ((context->buf = malloc(PPP_MTU)) == 0) {
        error("ACSP plugin: no memory\n");
        free(context);
        return -1;
    }
    context->buf_size = PPP_MTU;
    context->type = type;
    context->mode = PLUGIN_MODE_NONE;
    context->state = PLUGIN_STATE_INITIAL;
    context->ip_up = 0;
    context->retry_count = 0;
    context->next_seq = 0;
    context->last_pkt_len = 0;
    context->list = 0;
    context->config_installed = 0;
    ext->context = context;
    
    // setup funtions pointers
    ext->dispose = acsp_plugin_dispose;
    ext->free = 0;
    ext->interactive_ui = 0;
    ext->process = acsp_plugin_process;
    ext->print_packet = acsp_plugin_print_packet;
    
    return 0;
}

//------------------------------------------------------------
// *** Plugin functions called thru the ext struct ***
//	Called for a particular option type
//------------------------------------------------------------

//------------------------------------------------------------
// acsp_plugin_dispose
//	clean up
//------------------------------------------------------------
static void acsp_plugin_dispose(void *context)
{
    acsp_plugin_clear_list((acsp_plugin_context*)context);
}

//------------------------------------------------------------
// acsp_plugin_process
//	handle an event
//------------------------------------------------------------
static void acsp_plugin_process(void *context, ACSP_Input *acsp_in, ACSP_Output *acsp_out)
{
    acsp_packet			*pkt = (acsp_packet*)acsp_in->data;
    acsp_plugin_context*	theContext = (acsp_plugin_context*)context;
    
    switch (acsp_in->notification) {
        case ACSP_NOTIFICATION_NONE:
            break;
            
        case ACSP_NOTIFICATION_DATA_FROM_UI:
            error("ACSP plugin: unexpected notification received\n");
            break;
            
        case ACSP_NOTIFICATION_START:
            // get negotiated values and setup modes in context
            if (theContext->type == CI_ROUTES) {
                if (acscp_gotoptions[0].neg_routes)
                    theContext->mode = PLUGIN_MODE_RCV;
                else if (acscp_hisoptions[0].neg_routes)
                    theContext->mode = PLUGIN_MODE_SEND;
                else
                    theContext->mode = PLUGIN_MODE_NONE;
            } else {
                if (acscp_gotoptions[0].neg_domains)
                    theContext->mode = PLUGIN_MODE_RCV;
                else if (acscp_hisoptions[0].neg_domains)
                    theContext->mode = PLUGIN_MODE_SEND;
                else
                    theContext->mode = PLUGIN_MODE_NONE;        
            }
            // setup the context and send if required
            theContext->next_seq = 0;
            theContext->retry_count = 0;

            if (theContext->mode == PLUGIN_MODE_SEND) {
                acsp_plugin_send(theContext, acsp_in, acsp_out);
                theContext->next_seq++;
                theContext->state = PLUGIN_SENDSTATE_WAITING_ACK;
            }
            else if (theContext->mode == PLUGIN_MODE_RCV)
                theContext->state = PLUGIN_RCVSTATE_LISTEN;
            break;
            
        case ACSP_NOTIFICATION_STOP:
            switch (theContext->state) {
            	case PLUGIN_SENDSTATE_WAITING_ACK:
                    acsp_out->action = ACSP_ACTION_CANCEL_TIMEOUT;
                    // fall thru
                                
                case PLUGIN_STATE_DONE:
                    if (theContext->config_installed) {
                        theContext->config_installed = 0;
                        if (theContext->type == CI_ROUTES)
                            acsp_plugin_remove_config(theContext);
                    }
                    break;
            }
            // if client side - clear the list of received data
            if (theContext->mode == PLUGIN_MODE_RCV)
                acsp_plugin_clear_list(theContext);
            theContext->state = PLUGIN_STATE_INITIAL;
            break;
        
        case ACSP_NOTIFICATION_PACKET:
            switch (theContext->state) {                        
                case PLUGIN_RCVSTATE_LISTEN:
                    if (ntohs(pkt->flags) & ACSP_FLAG_START == 0) {
                        error("ACSP plugin: received first packet with no start flag\n");
                        break;
                    } else
                        theContext->state = PLUGIN_RCVSTATE_WAITING_PKT;
                    // fall thru
                
                case PLUGIN_RCVSTATE_WAITING_PKT:
                    // copy route data
                    if (pkt->seq == theContext->next_seq) {
                        if (acsp_plugin_read(theContext, acsp_in) == 0) {
                            theContext->next_seq++;
                            if (ntohs(pkt->flags) & ACSP_FLAG_END) {
                                theContext->state = PLUGIN_STATE_DONE;
                                if (theContext->ip_up)
                                    acsp_plugin_install_config(theContext);
                            }
                        } else
                            break;					// bad packet or error - send no ack
                    }					
                        
                    if (ntohs(pkt->flags) & ACSP_FLAG_REQUIRE_ACK)
                        acsp_plugin_send_ack(theContext, acsp_in, acsp_out);
                    break;
                
                case PLUGIN_SENDSTATE_WAITING_ACK:
                    if (ntohs(pkt->flags) & ACSP_FLAG_ACK)	// if ack - process it - otherwise drop the packet
                        if (theContext->next_to_send) {
                            acsp_plugin_send(theContext, acsp_in, acsp_out);
                            theContext->next_seq++;
                        }
                        else
                            theContext->state = PLUGIN_STATE_DONE;        
                    break;
            }
            break;
            
        case ACSP_NOTIFICATION_TIMEOUT:
            if (theContext->state == PLUGIN_SENDSTATE_WAITING_ACK) {
                if (theContext->retry_count++ < ACSP_PLUGIN_MAX_RETRIES)
                    acsp_plugin_resend(theContext, acsp_in, acsp_out);
                else {
                    error("ACSP plugin: no acknowlegement from peer\n");
                    theContext->state = PLUGIN_STATE_DONE;
                }
            } else
                error("ACSP plugin: received unexpected timeout\n");
            break;
                
        case ACSP_NOTIFICATION_ERROR:
            error("ACSP plugin: error notificationr received\n");
            if (theContext->state == PLUGIN_SENDSTATE_WAITING_ACK)
                acsp_out->action = ACSP_ACTION_CANCEL_TIMEOUT;
            theContext->state = PLUGIN_STATE_DONE;
            break;
        
        default:
            break;
    }
}

//------------------------------------------------------------
// acsp_plugin_print_packet
//------------------------------------------------------------
static void acsp_plugin_print_packet(void (*printer)(void *, char *, ...), void *arg, u_char code, char *inbuf, int insize)
{
}

//------------------------------------------------------------
// acsp_plugin_send
//	send a data packet
//------------------------------------------------------------
static void acsp_plugin_send(acsp_plugin_context* context, ACSP_Input* acsp_in, ACSP_Output* acsp_out)
{

    acsp_packet 	*pkt;
    u_int16_t		space, len;
    int			slen;
    acsp_route_data	*route_data;
    acsp_domain_data	domain_data;
    u_int8_t *outPtr;
    
    
    pkt = ALIGNED_CAST(acsp_packet*)(context->buf + 4);	// leave space for address, control, and protocol
    space = MIN(context->buf_size, acsp_in->mtu) - 4;
    
    if (context->state == PLUGIN_STATE_INITIAL) {
        pkt->flags = htons(ACSP_FLAG_START);
        context->next_to_send = context->list;
    } else
        pkt->flags = 0;

    pkt->type = context->type;
    pkt->seq = context->next_seq;
    pkt->flags = htons(ntohs(pkt->flags) | ACSP_FLAG_REQUIRE_ACK);
    pkt->reserved = 0;
    len = ACSP_HDR_SIZE;

    switch (context->type) {   
        case CI_ROUTES:
            route_data = ALIGNED_CAST(acsp_route_data*)pkt->data;
            while (context->next_to_send && space >= len + sizeof(acsp_route_data)) {
                route_data->address = ((acsp_route*)context->next_to_send)->address.s_addr;
                route_data->mask = ((acsp_route*)context->next_to_send)->mask.s_addr;
                route_data->flags = htons(((acsp_route*)context->next_to_send)->flags);
                route_data->reserved = htons(0);
                len += sizeof(acsp_route_data);
                context->next_to_send = ((acsp_route*)(context->next_to_send))->next;
                route_data++;
            }
            break;
    	case CI_DOMAINS:
            // WCast-align fix - use memcpy for unaligned access
            outPtr = pkt->data;
            while (context->next_to_send && space >= (len + (slen = strlen(((acsp_domain*)(context->next_to_send))->name)) + 6)) {
                domain_data.server = ((acsp_domain*)(context->next_to_send))->server.s_addr;
                domain_data.len = htons(slen);
                memcpy(outPtr, &domain_data, ACSP_DOMAIN_DATA_HDRSIZE);
                memcpy(outPtr + ACSP_DOMAIN_DATA_HDRSIZE, ((acsp_domain*)(context->next_to_send))->name, slen);
                len += (slen + ACSP_DOMAIN_DATA_HDRSIZE);
                context->next_to_send = ((acsp_domain*)(context->next_to_send))->next;
                outPtr += (ACSP_DOMAIN_DATA_HDRSIZE + slen);
            }
            break;
    }
    
    if (context->next_to_send == 0)
		pkt->flags = htons(ntohs(pkt->flags) | ACSP_FLAG_END);
    acsp_out->action = ACSP_ACTION_SEND_WITH_TIMEOUT;
    context->last_pkt_len = len;
    pkt->len = htons(len);
    acsp_out->data_len = len + 4;
    acsp_out->data = context->buf;
}

//------------------------------------------------------------
// acsp_plugin_resend
//	re-send a data packet
//------------------------------------------------------------
static void acsp_plugin_resend(acsp_plugin_context* context, ACSP_Input* acsp_in, ACSP_Output* acsp_out)
{    
    // resent the packet;
    acsp_out->data = context->buf;
    acsp_out->data_len = context->last_pkt_len + 4;
    acsp_out->action = ACSP_ACTION_SEND_WITH_TIMEOUT;
}

//------------------------------------------------------------
// acsp_plugin_send_ack
//	send an ack
//------------------------------------------------------------
static void acsp_plugin_send_ack(acsp_plugin_context* context, ACSP_Input* acsp_in, ACSP_Output* acsp_out)
{
    acsp_packet 	*inPkt, *outPkt;

    inPkt = (acsp_packet*)acsp_in->data;
    outPkt = ALIGNED_CAST(acsp_packet*)(context->buf + 4);	// leave space for address, control, and protocol
    outPkt->type = context->type;
    outPkt->seq = inPkt->seq;
    outPkt->flags = htons(ACSP_FLAG_ACK);
    outPkt->len = htons(ACSP_HDR_SIZE);
    outPkt->reserved = 0;
        
    acsp_out->action = ACSP_ACTION_SEND;
    acsp_out->data_len = ACSP_HDR_SIZE + 4;
    acsp_out->data = context->buf;    
}

//------------------------------------------------------------
// acsp_plugin_read
//	process an incomming data packet
//------------------------------------------------------------ 
static int acsp_plugin_read(acsp_plugin_context* context, ACSP_Input* acsp_in)
{
    acsp_packet 	*pkt;
    acsp_route		*route;
    acsp_domain		*domain;
    int			len, domain_len;
    acsp_route_data	*route_data;
    acsp_domain_data domain_data;
    u_int8_t        *inPtr;
    
    len = acsp_in->data_len - ACSP_HDR_SIZE;
    pkt = acsp_in->data;
    
    switch (context->type) {
        case CI_ROUTES:
            if (len % 4 != 0) {
                error("ACSP plugin: received packet with invalid len\n");
                return -1;
            }
            route_data = ALIGNED_CAST(acsp_route_data*)pkt->data;
            while (len > 4) {
                if ((route = (acsp_route*)malloc(sizeof(acsp_route))) == 0) {
                    error("ACSP plugin: no memory\n");
                    return -1;
                }
				bzero(route, sizeof(acsp_route));                   
                route->address.s_addr = route_data->address & route_data->mask;
                route->mask.s_addr = route_data->mask;
                route->flags = ntohs(route_data->flags);
                route->installed = 0;
                route->next = (acsp_route*)context->list;
                context->list = route;
                len -= sizeof(acsp_route_data);
                route_data++;
            }
            break;
        
        case CI_DOMAINS:
            // WCast-align fix - this routine changed to use memcpy for unaligned access
            inPtr = pkt->data;
            while (len > 2) {
                memcpy(&domain_data, inPtr, ACSP_DOMAIN_DATA_HDRSIZE);
                if ((domain = (acsp_domain*)malloc(sizeof(acsp_domain))) == 0) {
                    error("ACSP plugin: no memory\n");
                    return -1;
                }
                domain_len = ntohs(domain_data.len);
                if ((domain->name = malloc(domain_len + 1)) == 0) {
                    error("ACSP plugin: no memory\n");
                    free(domain);
                    return -1;
                }
                domain->server.s_addr = domain_data.server;
                memcpy(domain->name, inPtr + ACSP_DOMAIN_DATA_HDRSIZE, domain_len);
                *(domain->name + domain_len) = 0;	// zero terminate
                domain->next = (acsp_domain*)context->list;
                context->list = domain;
                len -= (domain_len + ACSP_DOMAIN_DATA_HDRSIZE);
                inPtr += (ACSP_DOMAIN_DATA_HDRSIZE + domain_len);
            }
            break;
        
        default:
            return -1;
            break;
    }
    return 0;
}

//------------------------------------------------------------
// acsp_plugin_clear_list
//	clear the route or domain list in the context
//------------------------------------------------------------
static void acsp_plugin_clear_list(acsp_plugin_context* context)
{
    void* temp = context->list;
    
    while (temp) {
        if (context->type == CI_ROUTES)
            context->list = ((acsp_route*)temp)->next;
        else {
            free(((acsp_domain*)temp)->name);
            context->list = ((acsp_domain*)temp)->next;
        }
        free(temp);
        temp = context->list;
    }
}

//------------------------------------------------------------
// acsp_ip_up
//	called when ipcp comes up
//------------------------------------------------------------
static void acsp_plugin_ip_up(void *arg, uintptr_t phase)
{
    acsp_plugin_context* context = (acsp_plugin_context*)arg;

    context->ip_up = 1;
    if (context->mode == PLUGIN_MODE_RCV 
            && context->state == PLUGIN_STATE_DONE 
            && context->config_installed == 0)
        acsp_plugin_install_config(context);
}

//------------------------------------------------------------
// acsp_ip_down
//	called when ipcp goes down
//------------------------------------------------------------
static void acsp_plugin_ip_down(void *arg, uintptr_t phase)
{
    acsp_plugin_context* context = (acsp_plugin_context*)arg;
    
    context->ip_up = 0;
    if (context->mode == PLUGIN_MODE_RCV && context->config_installed)
        acsp_plugin_remove_config(context);
    
}

//------------------------------------------------------------
// acsp_install_config
//	called do add the config for the option 
//	specified by the context
//------------------------------------------------------------
static void acsp_plugin_install_config(acsp_plugin_context *context)
{    
    if (context->type == CI_ROUTES)
        acsp_plugin_add_routes(context->list);
    else
        acsp_plugin_add_domains(context->list);
    context->config_installed = 1;
}

//------------------------------------------------------------
// acsp_remove_config
//	called do remove the config for the option 
//	specified by the context
//------------------------------------------------------------
static void acsp_plugin_remove_config(acsp_plugin_context *context)
{
    // we don't remove anything - pppd will cleanup the dynamic store
    if (context->type == CI_ROUTES)
        acsp_plugin_remove_routes(context->list);

    context->config_installed = 0;
}

//------------------------------------------------------------
// acsp_plugin_add_routes
//	install routes
//------------------------------------------------------------
static void acsp_plugin_add_routes(acsp_route *route)
{
	char   route_str[INET_ADDRSTRLEN];
	char   mask_str[INET_ADDRSTRLEN];
	char   gateway_str[INET_ADDRSTRLEN];
    struct in_addr	addr;
	int err;

    if (route) {	// only if routes are present
        sleep(1);
        addr.s_addr = 0; 
        
        // remove current default route
		cifdefaultroute(0, 0, 0);
		cifroute();
#if 0
        if (route_gateway(RTM_DELETE, addr, addr, addr, 1) == 0)
            error("ACSP plugin: error removing default route\n");
        // add new default route on previous primary interface
        if (route_gateway(RTM_ADD, addr, addr, primary_router, 1) == 0) {
            error("ACSP plugin: error adding default route\n");
            return;
        }    
#endif
        
        while (route) {
            route->installed = 1;
            if (route->flags & ACSP_ROUTEFLAGS_PRIVATE) {
				if (route->address.s_addr == 0)
					sifdefaultroute(0, 0, 0);
				else {	
					//if (route->router.s_addr)
					//	err = route_gateway(RTM_ADD, route->address, route->mask, route->router, 1);
					//else 
						err = route_interface(RTM_ADD, route->address, route->mask, IFT_PPP, ifname, 0);
					if (err == 0) {
						error("ACSP plugin: error installing private net route. (%s/%s).",
							  addr2ascii(AF_INET, &route->address, sizeof(route->address), route_str),
							  addr2ascii(AF_INET, &route->mask, sizeof(route->mask), mask_str));
						route->installed = 0;
					}
				}
            } else if (route->flags & ACSP_ROUTEFLAGS_PUBLIC) {
				if (route->address.s_addr == 0)
					cifdefaultroute(0, 0, 0);
				else {
					if (route_gateway(RTM_ADD, route->address, route->mask, primary_router, 1) == 0) {
						error("ACSP plugin: error installing public net route. (%s/%s -> %s).",
							  addr2ascii(AF_INET, &route->address, sizeof(route->address), route_str),
							  addr2ascii(AF_INET, &route->mask, sizeof(route->mask), mask_str),
							  addr2ascii(AF_INET, &primary_router, sizeof(primary_router), gateway_str));
						route->installed = 0;
					}       
				}
            }
            route = route->next;
        }
    }
}

//------------------------------------------------------------
// acsp_plugin_remove_routes
//	remove routes
//------------------------------------------------------------
static void acsp_plugin_remove_routes(acsp_route *route)
{
	char   route_str[INET_ADDRSTRLEN];
	char   mask_str[INET_ADDRSTRLEN];
	char   gateway_str[INET_ADDRSTRLEN];
	
    while (route) {
        if (route->installed) {
			route->installed = 0;
			if (route->flags & ACSP_ROUTEFLAGS_PRIVATE) {
				if (route->address.s_addr == 0)
					cifdefaultroute(0, 0, 0);
				else {	
					if (route_interface(RTM_DELETE, route->address, route->mask, IFT_PPP, ifname, 0) == 0) {
						error("ACSP plugin: error removing private net route. (%s/%s).",
							  addr2ascii(AF_INET, &route->address, sizeof(route->address), route_str),
							  addr2ascii(AF_INET, &route->mask, sizeof(route->mask), mask_str));
						route->installed = 1;
					} 
				}
			} else if (route->flags & ACSP_ROUTEFLAGS_PUBLIC) {
				if (route->address.s_addr == 0) {
					// nothing to do
				}
				else {	
					if (route_gateway(RTM_DELETE, route->address, route->mask, primary_router, 0) == 0) {
						error("ACSP plugin: error removing public net route. (%s/%s -> %s).",
							  addr2ascii(AF_INET, &route->address, sizeof(route->address), route_str),
							  addr2ascii(AF_INET, &route->mask, sizeof(route->mask), mask_str),
							  addr2ascii(AF_INET, &primary_router, sizeof(primary_router), gateway_str));
						route->installed = 1;
					}
				}
			}
		}
        route = route->next;
    }
}

//------------------------------------------------------------
// acsp_plugin_add_domains
//	install search domains
//------------------------------------------------------------
static void acsp_plugin_add_domains(acsp_domain	*domain)
{
    CFStringRef	str;
    int 	err, clean = 1;
	long	order = 100000;
	CFNumberRef num;
	
	num = CFNumberCreate(NULL, kCFNumberLongType, &order);
	if (num == 0) {
		error("ACSP plugin: error adding domain name - could not create CFNumber\n");
		return;
	}
		
    while (domain) {
        if (str = CFStringCreateWithCString(NULL, domain->name, kCFStringEncodingUTF8)) {
			err = publish_dns_wins_entry(kSCEntNetDNS, kSCPropNetDNSSearchDomains, str, 0, kSCPropNetDNSSupplementalMatchDomains, str, kSCPropNetDNSSupplementalMatchOrders, num, clean);
#ifndef kSCPropNetProxiesSupplementalMatchDomains			
#define kSCPropNetProxiesSupplementalMatchDomains kSCPropNetDNSSupplementalMatchDomains
#define kSCPropNetProxiesSupplementalMatchOrders kSCPropNetDNSSupplementalMatchOrders
#endif
			if (err) publish_dns_wins_entry(kSCEntNetProxies, kSCPropNetProxiesSupplementalMatchDomains, str, 0, kSCPropNetProxiesSupplementalMatchOrders, num, 0, 0, clean);
			CFRelease(str);
            if (err == 0) {
                error("ACSP plugin: error adding domain name\n");
				goto end;
            }
        } else {
            error("ACSP plugin: error adding domain name - could not create CFString\n");
			goto end;
        }
        domain = domain->next;
        clean = 0;
    }

end:
	CFRelease(num);
}

#pragma -

/*
 code to act as a DHCP client and server 
*/

struct pseudo_udphdr {
	struct in_addr	src_addr;		/* source address */
	struct in_addr	dst_addr;		/* source address */
	u_int8_t		zero;			/* just zero */
	u_int8_t		proto;			/* destination port */
	u_int16_t		len;			/* packet len */
};

struct dhcp {
    u_char              dp_op;          /* packet opcode type */
    u_char              dp_htype;       /* hardware addr type */
    u_char              dp_hlen;        /* hardware addr length */
    u_char              dp_hops;        /* gateway hops */
    u_int32_t           dp_xid;         /* transaction ID */
    u_int16_t           dp_secs;        /* seconds since boot began */  
    u_int16_t           dp_flags;       /* flags */
    struct in_addr      dp_ciaddr;      /* client IP address */
    struct in_addr      dp_yiaddr;      /* 'your' IP address */
    struct in_addr      dp_siaddr;      /* server IP address */
    struct in_addr      dp_giaddr;      /* gateway IP address */
    u_char              dp_chaddr[16];  /* client hardware address */
    u_char              dp_sname[64];   /* server host name */
    u_char              dp_file[128];   /* boot file name */
    u_char              dp_options[0];  /* variable-length options field */
};

struct dhcp_packet {
    struct ip           ip;
    struct udphdr       udp;
    struct dhcp         dhcp;
};
#define DHCP_COOKIE		0x63825363

#define DHCP_OPTION_END				255 

#define DHCP_OPTION_LEASE_TIME			51
#define DHCP_OPTION_MSG_TYPE			53
#define DHCP_OPTION_HOST_NAME			12
#define DHCP_OPTION_SERVER_ID			54
#define DHCP_OPTION_PARAM_REQUEST_LIST	55
#define DHCP_OPTION_VENDOR_CLASS_ID		60
#define DHCP_OPTION_CLIENT_ID			61

#define DHCP_OPTION_MSG_TYPE_ACK		5
#define DHCP_OPTION_MSG_TYPE_INFORM		8

#define DHCP_OPTION_SUBNET_MASK			1
#define DHCP_OPTION_DNS					6
#define DHCP_OPTION_DOMAIN_NAME			15
#define DHCP_OPTION_WINS				43
#define DHCP_OPTION_NETBIOS				44
#define DHCP_OPTION_STATIC_ROUTE		249

#define DHCP_TIMEOUT_VALUE		3	/* seconds */
#define DHCP_MAX_RETRIES		4

//------------------------------------------------------------
// cksum
//------------------------------------------------------------
static unsigned short
cksum(unsigned char *data, int len)
{
	long sum = 0;
	
	while (len > 1) {
		sum += *(ALIGNED_CAST(unsigned short *)data);
		data += sizeof(unsigned short);
		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}
	if (len)
		sum += (unsigned short)*data;
	while (sum >> 16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

//------------------------------------------------------------
// log_dhcp
//------------------------------------------------------------
static void
log_dhcp(u_char *pkt, int len, char *text)
{
#if 0
	u_char *p;
	int i;
	u_int32_t cookie;
	struct dhcp *dp;
	struct dhcp_packet *packet;
	u_int32_t masklen, addrlen, addr, mask;
	char str[2048];
	char str2[16];
	
	/* log only if debug level is super verbose */ 
	if (debug <= 1)
		return;

	packet = (struct dhcp_packet *)pkt;
	dp = (struct dhcp *)&packet->dhcp;

	dbglog("%s\n", text);
	dbglog(" op = %s\n", dp->dp_op == BOOTREQUEST ? "BOOTREQUEST" : dp->dp_op == BOOTREPLY ? "BOOTREPLY" : "UNKNOWN");
	dbglog(" htype = %d\n", dp->dp_htype);
	dbglog(" hlen = %d\n", dp->dp_hlen);
	dbglog(" hops = %d\n", dp->dp_hops);
	dbglog(" xid = %d\n", dp->dp_xid);
	dbglog(" flags = %d\n", dp->dp_flags);
	dbglog(" client address = %s\n", inet_ntoa(dp->dp_ciaddr));
	dbglog(" your address = %s\n", inet_ntoa(dp->dp_yiaddr));
	dbglog(" server address = %s\n", inet_ntoa(dp->dp_siaddr));
	dbglog(" gateway address = %s\n", inet_ntoa(dp->dp_giaddr));

	dbglog(" hardware address = %X:%X:%X:%X:%X:%X\n", dp->dp_chaddr[0], dp->dp_chaddr[1], dp->dp_chaddr[2], dp->dp_chaddr[3], dp->dp_chaddr[4], dp->dp_chaddr[5]);
	dbglog(" hardware address = %X:%X:%X:%X:%X:%X\n", dp->dp_chaddr[6], dp->dp_chaddr[7], dp->dp_chaddr[8], dp->dp_chaddr[9], dp->dp_chaddr[10], dp->dp_chaddr[11]);
	dbglog(" hardware address = %X:%X:%X:%X\n", dp->dp_chaddr[12], dp->dp_chaddr[13], dp->dp_chaddr[14], dp->dp_chaddr[15]);
	dbglog(" server host name = %s\n", dp->dp_sname);
	dbglog(" boot file name = %s\n", dp->dp_file);
	
	p = dp->dp_options;
	cookie = ntohl(*(u_int32_t *)p);
	if (ntohl(*(u_int32_t *)p) != DHCP_COOKIE) {
		dbglog(" >>> incorrect cookie = %d.%d.%d.%d\n", cookie >> 24, cookie >> 16 & 0xFF, cookie >> 8 & 0xFF,cookie & 0xFF);
		return;
	}
	p+=4;

	if (*p++ != DHCP_OPTION_MSG_TYPE || *p++ != 1 || (*p != DHCP_OPTION_MSG_TYPE_INFORM && *p != DHCP_OPTION_MSG_TYPE_ACK)) {
		dbglog(" >>> incorrect message type = %d\n", *p);
		return;
	}
	dbglog(" dhcp option msg type = %s\n", *p == DHCP_OPTION_MSG_TYPE_INFORM ? "INFORM" : "ACK");
	p++;

	len -= sizeof(struct dhcp_packet) + 7;
	while (*p != DHCP_OPTION_END && len > 0) {
		u_int8_t optcode, optlen;
		
		optcode = *p++;
		// check for pad option
		if (optcode == 0) {
			len--;
			continue;
		}
			
		optlen = *p++;
		len-=2;
		if (len == 0) {
			warning(">>> incorrect message option\n");
			return;
		}
		
		str[0] = 0;
		switch (optcode) {
			case DHCP_OPTION_HOST_NAME:
				memcpy(str, p, optlen);
				str[optlen] = 0;
				dbglog(" dhcp option host name = %s\n", str);
				break;
			case DHCP_OPTION_VENDOR_CLASS_ID:
				memcpy(str, p, optlen);
				str[optlen] = 0;
				dbglog(" dhcp option vendor class id = %s\n", str);
				break;
			case DHCP_OPTION_CLIENT_ID:
				for (i = 0; i < optlen; i++) {
					snprintf(str2, sizeof(str2), "0x%x ", p[i]);
					strlcat(str, str2, sizeof(str));
				}
				dbglog(" dhcp option client id = %s\n", str);
				break;
			case DHCP_OPTION_SERVER_ID:
				for (i = 0; i < optlen; i++) {
					snprintf(str2, sizeof(str2), "0x%x ", p[i]);
					strlcat(str, str2, sizeof(str));
				}
				dbglog(" dhcp option server id = %s\n", str);
				break;
			case DHCP_OPTION_LEASE_TIME:
				dbglog(" dhcp option lease time = %d\n", ntohl(*(u_int32_t*)p));
				break;
			case DHCP_OPTION_SUBNET_MASK:
				dbglog(" dhcp option subnet mask = %d.%d.%d.%d\n", p[0], p[1], p[2], p[3]);
				break;
			case DHCP_OPTION_DOMAIN_NAME:
				memcpy(str, p, optlen);
				str[optlen] = 0;
				dbglog(" dhcp option domain name = %s\n", str);
				break;
			case DHCP_OPTION_STATIC_ROUTE:
				dbglog(" dhcp option parameter static routes = \n");
				i = 0;
				while (i < optlen) {
					masklen = p[i];
					mask = 0xFFFFFFFF << (32 - masklen);
					addrlen = (masklen / 8);
					if (masklen % 8)
						addrlen++;
					addr = ntohl(*(u_int32_t*)(&p[i+1])) & mask;
					router = ntohl(*(u_int32_t*)(&p[i+1+addrlen]));
					i += addrlen + 1 + sizeof(in_addr_t);
					dbglog("    route %d.%d.%d.%d mask %d.%d.%d.%d router %d.%d.%d.%d\n", 
						(addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF, 
						(mask >> 24) & 0xFF, (mask >> 16) & 0xFF, (mask >> 8) & 0xFF, mask & 0xFF,
						(router >> 24) & 0xFF, (router >> 16) & 0xFF, (router >> 8) & 0xFF, router & 0xFF);
				}
/*
				for (i = 0; i < optlen; i++) {
					// UNSAFE! Don't use sprintf.
					sprintf(str+strlen(str), "0x%x ", p[i]);
				}
				dbglog(" dhcp option parameter static routes = %s \n", str);
*/
				break;
			case DHCP_OPTION_PARAM_REQUEST_LIST:
				for (i = 0; i < optlen; i++) {
					snprintf(str2, sizeof(str2), "0x%x ", p[i]);
					strlcat(str, str2, sizeof(str));
				}
				dbglog(" dhcp option parameter request list = %s \n", str);
				break;

			default:
				dbglog(" dhcp option code = %d, len = %d\n", optcode, optlen);
				break;
		}
		
		p+=optlen;
		len-=optlen;
	}
	dbglog(" end of options\n");
#endif
}

//------------------------------------------------------------
// acsp_ipdata_send_packet
//------------------------------------------------------------
static void
acsp_ipdata_send_packet(int unit, u_char *data, int len, u_int32_t srcaddr, u_int16_t srcport, u_int32_t dstaddr, u_int16_t dstport, char *text)
{
	u_char *outp;
	u_int16_t checksum;
static u_int16_t dhcp_ip_id = 1;
	
	// build a udp pseudo header for checksum calculation
	outp = data + PPP_HDRLEN + sizeof(struct ip) - sizeof(struct pseudo_udphdr);
	PUTLONG(srcaddr, outp);					// source address
	PUTLONG(dstaddr, outp);		// destination address
	PUTCHAR(0, outp);						// zero
	PUTCHAR(IPPROTO_UDP, outp);				// protocol
	PUTSHORT(len - PPP_HDRLEN - sizeof(struct ip), outp);	// total length (udp data + udp header)

	// build udp header
	outp = data + PPP_HDRLEN + sizeof(struct ip);
	PUTSHORT(srcport, outp);			// source port
	PUTSHORT(dstport, outp);			// dest port
	PUTSHORT(len - PPP_HDRLEN - sizeof(struct ip), outp);	// total length (udp data + udp header)
	PUTSHORT(0, outp);						// cksum
	checksum = cksum(data + PPP_HDRLEN + sizeof(struct ip) - sizeof(struct pseudo_udphdr), 
						len - PPP_HDRLEN - sizeof(struct ip) + sizeof(struct pseudo_udphdr));
	if (checksum == 0)
		checksum =0xffff;
	outp -= 2;								// back to cksum
	PUTSHORT(0, outp);						// cksum

	// build ip header
	outp = data + PPP_HDRLEN;
	PUTSHORT(0x4500, outp);					// hdr len and service type
	PUTSHORT(len - PPP_HDRLEN, outp);	// total length
	PUTSHORT(dhcp_ip_id++, outp);			// identification
	PUTSHORT(0, outp);						// flags and fragment offset
	PUTCHAR(0x40, outp);					// ttl
	PUTCHAR(IPPROTO_UDP, outp);				// protocol
	PUTSHORT(0, outp);						// cksum
	PUTLONG(srcaddr, outp);					// source address
	PUTLONG(dstaddr, outp);		// destination address
	checksum = cksum(data + PPP_HDRLEN, sizeof(struct ip));
	outp -= 10;								// back to cksum
	PUTSHORT(ntohs(checksum), outp);				// header checksum

	// log the packet
	log_dhcp(data + PPP_HDRLEN, len - PPP_HDRLEN, text);

	// now it's time to send it...
	output(unit, data, len);
}

//------------------------------------------------------------
// acsp_ipdata_input_server
//------------------------------------------------------------
static void
acsp_ipdata_input_server(int unit, u_char *pkt, int len, u_int32_t ouraddr, u_int32_t hisaddr)
{
	struct dhcp_packet *dp;
	struct	in_addr src;
	u_char *p, *outp;
	char str[2048];
	int i, outlen, pad;
	int need_subnet_mask = 0, need_domain_name = 0, need_static_route = 0;
	struct dhcp reply;
	u_int32_t	l;

	dp = ALIGNED_CAST(struct dhcp_packet *)pkt;

	/* basic length sanity check */
	if (len < (sizeof(struct dhcp_packet) + 7)) {  // dhcp packet + cookie + inform
		warning("DHCP packet received with incorrect length\n");
		return;
	}

	log_dhcp(pkt, len, "DHCP packet received");

	src.s_addr = dp->dhcp.dp_ciaddr.s_addr;
	
	p = dp->dhcp.dp_options;
    GETLONG(l, p);
	if (l != DHCP_COOKIE) {
		warning("DHCP packet received with incorrect cookie\n");
		return;
	}

	if (*p++ != DHCP_OPTION_MSG_TYPE || *p++ != 1 || *p++ != DHCP_OPTION_MSG_TYPE_INFORM) {
		warning("DHCP packet received with incorrect message type\n");
		return;
	}

	len -= sizeof(struct dhcp_packet) + 7;
	while (*p != DHCP_OPTION_END && len > 0) {
		u_int8_t optcode, optlen;
		
		optcode = *p++;
		// check for pad option
		if (optcode == 0) {
			len--;
			continue;
		}
			
		optlen = *p++;
		len-=2;
		if (optlen >= len) {
			warning("DHCP packet received with incorrect message option\n");
			return;
		}
		
		str[0] = 0;
		switch (optcode) {
			case DHCP_OPTION_PARAM_REQUEST_LIST:
				for (i = 0; i < optlen; i++) {
					if ((p[i] == DHCP_OPTION_SUBNET_MASK) && subnet_mask)
						need_subnet_mask = 1;
					else if ((p[i] == DHCP_OPTION_DOMAIN_NAME) && domains_list)
						need_domain_name = 1;
					else if ((p[i] == DHCP_OPTION_STATIC_ROUTE) && routes_list)
						need_static_route = 1;
				}
				break;

			default:
				break;
		}
		
		p+=optlen;
		len-=optlen;
	}

	/* build reply dhcp packet */
	if (need_subnet_mask || need_domain_name || need_static_route) {

		outp = outpacket_buf;

		// ppp
		MAKEHEADER(outp, PPP_IP);
		outlen = PPP_HDRLEN;

		// ip
		bzero(outp, sizeof(struct ip));
		outp += sizeof(struct ip);
		outlen += sizeof(struct ip);
		
		// udp	
		bzero(outp, sizeof(struct udphdr));
		outp += sizeof(struct udphdr);
		outlen += sizeof(struct udphdr);
		
		// bootp	
		memcpy(&reply, &dp->dhcp, sizeof(struct dhcp));
		reply.dp_op = BOOTREPLY;
		reply.dp_htype = 1;
		reply.dp_secs = 0;
		memcpy(outp, &reply, sizeof(struct dhcp));
		outlen += sizeof(struct dhcp);		
		outp +=  sizeof(struct dhcp);

		// dhcp options
		PUTLONG(DHCP_COOKIE, outp);	// dhcp cookie
		outlen += 4;

		PUTCHAR(DHCP_OPTION_MSG_TYPE, outp);	// dhcp message type
		PUTCHAR(1, outp);		// dhcp message type len
		PUTCHAR(DHCP_OPTION_MSG_TYPE_ACK, outp);	// dhcp message type ack
		outlen += 3;
		
		PUTCHAR(DHCP_OPTION_SERVER_ID, outp);	// dhcp server id
		PUTCHAR(4, outp);		// dhcp server id len
		l = ntohl(ouraddr);
		PUTLONG(l, outp);	// server id is our source address
		outlen += 6;

		if (need_subnet_mask) {
			PUTCHAR(DHCP_OPTION_SUBNET_MASK, outp);	// dhcp subnet mask
			PUTCHAR(4, outp);		// dhcp subnet mask len
			PUTLONG(subnet_mask, outp);	// server mask
			outlen += 6;
		}
		
		if (need_domain_name) {
			int len;
			acsp_domain 	*list = domains_list;
			// domain are reversed in the list use last one.
			while (list->next) list = list->next;
				
			PUTCHAR(DHCP_OPTION_DOMAIN_NAME, outp);	// dhcp domain name
			len = strlen(list->name);
			if (outlen + len + 2 >= sizeof(outpacket_buf)) {
				warning("Domain name too large for DHCP\n");
				return;
			}
			PUTCHAR(len, outp);						// domain name len
			memcpy(outp, list->name, len);	// the domain
			outp += len;
			outlen += len + 2;
		}
		
		if (need_static_route) {
			acsp_route 	*list = routes_list;
			u_int32_t mask, addr, masklen, addrlen, totlen = 0;
			int opdone = 0;
			
			while (list) {
				if (list->flags & ACSP_ROUTEFLAGS_PRIVATE) {
					if (!opdone) {
						if (outlen + 2 >= sizeof(outpacket_buf)) {
							warning("No space for DHCP routes\n");
							return;
						}
						PUTCHAR(DHCP_OPTION_STATIC_ROUTE, outp);	// dhcp static route
						PUTCHAR(0, outp);	// dhcp static route total len
						opdone = 1;
					}
					mask = ntohl(list->mask.s_addr);					
					addr = ntohl(list->address.s_addr) & mask;					
				
					for ( masklen = 32; masklen && (mask& 1) == 0; mask = mask >> 1, masklen--);
					addrlen = (masklen / 8);
					if (masklen % 8)
						addrlen++;

					if (outlen + addrlen + 1 + 4 >= sizeof(outpacket_buf)) {
						warning("Static routes list too large DHCP\n");
						return;
					}
					PUTCHAR(masklen, outp);		// route mask
					PUTLONG(addr, outp);	// route address
					outp -= 4 - addrlen;	// move pointer back according to addr len.
					l = ntohl(hisaddr);
					PUTLONG(l, outp);	// router address
					totlen += addrlen + 1 + 4;
				}
				list = list->next;
			}
			if (opdone) {
				outp -= totlen + 1;
				PUTCHAR(totlen, outp);	// move back to update option len
				outp += totlen;
				outlen += totlen + 2;
			}
		}
		
		PUTCHAR(DHCP_OPTION_END, outp);		// end of options
		outlen ++;
		
		pad = outlen%4;
		for (i = 0; i < pad && outlen < sizeof(outpacket_buf); i++) {
			PUTCHAR(0, outp);		// byte padding
			outlen ++;
		}
		
		// build ip/udp header and send it...
		acsp_ipdata_send_packet(unit, outpacket_buf, outlen, ntohl(ouraddr), IPPORT_BOOTPS, ntohl(src.s_addr), IPPORT_BOOTPC, "DHCP packet replied");
	}		
}

//------------------------------------------------------------
// acsp_ipdata_input_client
//------------------------------------------------------------
static void
acsp_ipdata_input_client(int unit, u_char *pkt, int len, u_int32_t ouraddr, u_int32_t hisaddr)
{
	struct dhcp_packet *dp;
	struct	in_addr src;
	u_char *p;
	u_int32_t masklen, addrlen, i, mask, l;
	char str[2048];
	acsp_route  *route;
	acsp_domain *domain_list = NULL, *domain;
	char *str_p, *tok, *delim;
	
	dp = ALIGNED_CAST(struct dhcp_packet *)pkt;

	/* basic length sanity check */
	if (len < (sizeof(struct dhcp_packet) + 7)) {  // dhcp packet + cookie + inform
		warning("DHCP packet received with incorrect length\n");
		return;
	}

	log_dhcp(pkt, len, "DHCP packet received");

	if (!dhcp_context) {
		// we didn't start DHCP or already closed it
		return;
	}

	src.s_addr = ntohl(dp->dhcp.dp_ciaddr.s_addr);
	
	p = dp->dhcp.dp_options;
    GETLONG(l, p);
	if (l != DHCP_COOKIE) {
		warning("DHCP packet received with incorrect cookie\n");
		return;
	}

	if (*p++ != DHCP_OPTION_MSG_TYPE || *p++ != 1 || *p++ != DHCP_OPTION_MSG_TYPE_ACK) {
		warning("DHCP packet received with incorrect message type\n");
		return;
	}

	len -= sizeof(struct dhcp_packet) + 7;
	while (*p != DHCP_OPTION_END && len > 0) {
		u_int8_t optcode, optlen;
		
		optcode = *p++;
		// check for pad option
		if (optcode == 0) {
			len--;
			continue;
		}
			
		optlen = *p++;
		len-=2;
		if (optlen >= len) {
			warning("DHCP packet received with incorrect message option\n");
			return;
		}
		
		switch (optcode) {
			case DHCP_OPTION_SUBNET_MASK:
                memcpy(&mask, p, sizeof(u_int32_t));        // Wcast-align fix - memcpy for unaligned access
				if (mask &&
					dhcp_context->ouraddr.s_addr == ouraddr &&
					dhcp_context->netmask.s_addr != mask) {
					dhcp_context->netmask.s_addr = mask;
					if (!uifaddr(unit, dhcp_context->ouraddr.s_addr, dhcp_context->hisaddr.s_addr, dhcp_context->netmask.s_addr)) {
						notice("failed to configure dhcp option 'subnet mask' = %d.%d.%d.%d, our %x, his %x\n", p[0], p[1], p[2], p[3], ntohl(dhcp_context->ouraddr.s_addr), ntohl(dhcp_context->hisaddr.s_addr));
					}
				} else {
					info("ignoring dhcp option 'subnet mask' = %d.%d.%d.%d, current addr %x, current mask %x\n", p[0], p[1], p[2], p[3], ntohl(dhcp_context->ouraddr.s_addr), ntohl(dhcp_context->netmask.s_addr));
				}
				break;
			case DHCP_OPTION_DOMAIN_NAME:

				if (domain_list) {
                    notice("ignoring dhcp option 'domain name', option already processed.\n");
                    break;
				}

				memcpy(str, p, optlen);
				str[optlen] = 0;
				str_p = str;
				// check if domain is tokenized by a variety of delimiters
				GET_SPLITDNS_DELIM(str, delim);
				tok = strsep(&str_p, delim);
				do {
					if (!tok || *tok != '\0') {
						// tok may be NULL the first time through here.
						if((domain = (__typeof__(domain))malloc(sizeof(*domain))) == NULL) {
							error("failed to allocate domain from DHCP packet\n");
							break;
						}
						bzero(domain, sizeof(*domain));
						domain->next = domain_list;
						domain_list = domain;
						if (!tok) {
							domain->name = str;
							break;
						} else {
							domain->name = tok;
						}
					}
					tok = strsep(&str_p, delim);
				} while (tok != NULL);
				break;
			case DHCP_OPTION_STATIC_ROUTE:
				i = 0;
				while (i < optlen) {
					if ((route = (acsp_route*)malloc(sizeof(acsp_route))) == 0) {
						error("DHCP: no memory\n");
						return;
					}
					
					bzero(route, sizeof(acsp_route));                   
					masklen = p[i];
					route->mask.s_addr = htonl(0xFFFFFFFF << (32 - masklen));
					addrlen = (masklen / 8);
					if (masklen % 8)
						addrlen++;
                    
                    memcpy(&route->address.s_addr, &p[i+1], sizeof(route->address.s_addr));         // Wcast-align fix - memcpy for unaligned access
					route->address.s_addr &= route->mask.s_addr;
					memcpy(&route->router.s_addr, &p[i+1+addrlen], sizeof(route->router.s_addr));   
					route->flags = ACSP_ROUTEFLAGS_PRIVATE;
					route->installed = 0;
					route->next = (acsp_route*)dhcp_context->route;
					dhcp_context->route = route;
					i += addrlen + 1 + sizeof(in_addr_t);
				}
                acsp_plugin_add_routes(dhcp_context->route);
				break;

			default:
				break;
		}
		
		p+=optlen;
		len-=optlen;
	}
	
    if (domain_list) {
       acsp_plugin_add_domains(domain_list);
        while (domain_list) {
            domain = domain_list;
            domain_list = domain_list->next;
            free(domain);
        }
    }
    
	/* dhcp is done */
	UNTIMEOUT(acsp_ipdata_timeout, dhcp_context);
	dhcp_context->state = PLUGIN_STATE_DONE;
}

//------------------------------------------------------------
// acsp_ipdata_start_dhcp_client
//------------------------------------------------------------
static void
acsp_ipdata_start_dhcp_client(int unit, u_int32_t ouraddr, u_int32_t hisaddr)
{
	u_char *outp;
	int i, outlen, pad;
static u_int16_t dhcp_ip_client_xid = 1;
	u_int32_t l, clientid = 1; // ??	 
	
	outp = outpacket_buf;

	// ppp
	MAKEHEADER(outp, PPP_IP);
	outlen = PPP_HDRLEN;

	// ip
	bzero(outp, sizeof(struct ip));
	outp += sizeof(struct ip);
	outlen += sizeof(struct ip);
	
	// udp	
	bzero(outp, sizeof(struct udphdr));
	outp += sizeof(struct udphdr);
	outlen += sizeof(struct udphdr);
	
	// bootp	
	bzero(outp, sizeof(struct dhcp));
	PUTCHAR(BOOTREQUEST, outp);		// dp_op
	PUTCHAR(8, outp);				// dp_htype
	PUTCHAR(6, outp);				// dp_hlen
	PUTCHAR(0, outp);				// dp_hops
	PUTLONG(dhcp_ip_client_xid++, outp);		// dp_xid
	PUTSHORT(0, outp);				// dp_secs
	PUTSHORT(0, outp);				// dp_flags
	l = ntohl(ouraddr);
	PUTLONG(l, outp);				// dp_ciaddr
	PUTLONG(0, outp);				// dp_yiaddr
	PUTLONG(0, outp);				// dp_siaddr
	PUTLONG(0, outp);				// dp_giaddr
	PUTLONG(clientid, outp);		// dp_chaddr[0..3]
	PUTLONG(0, outp);				// dp_chaddr[4..7]
	PUTLONG(0, outp);				// dp_chaddr[8..11]
	PUTLONG(1, outp);				// dp_chaddr[12..15]
	outp += 64;						// dp_sname
	outp += 128;					// dp_file
	outlen += sizeof(struct dhcp);
	
	// dhcp options
	PUTLONG(DHCP_COOKIE, outp);	// dhcp cookie
	outlen += 4;

	PUTCHAR(DHCP_OPTION_MSG_TYPE, outp);	// dhcp message type
	PUTCHAR(1, outp);		// dhcp message type len
	PUTCHAR(DHCP_OPTION_MSG_TYPE_INFORM, outp);	// dhcp message type inform
	outlen += 3;
	
	PUTCHAR(DHCP_OPTION_CLIENT_ID, outp);	// dhcp client id
	PUTCHAR(7, outp);		// dhcp client id len
	PUTCHAR(8, outp);		// htype
	PUTLONG(clientid, outp);	// client id
	PUTSHORT(0, outp);		// client id end
	outlen += 9;

	PUTCHAR(DHCP_OPTION_PARAM_REQUEST_LIST, outp);	// dhcp param request list
	PUTCHAR(6, outp);		// dhcp param request list len
	PUTCHAR(DHCP_OPTION_DNS, outp);
	PUTCHAR(DHCP_OPTION_NETBIOS, outp);
	PUTCHAR(DHCP_OPTION_WINS, outp);
	PUTCHAR(DHCP_OPTION_SUBNET_MASK, outp);
	PUTCHAR(DHCP_OPTION_STATIC_ROUTE, outp);
	PUTCHAR(DHCP_OPTION_DOMAIN_NAME, outp);
	outlen += 8;
	
	PUTCHAR(DHCP_OPTION_END, outp);		// end of options
	outlen ++;
	
	pad = outlen%4;
	for (i = 0; i < pad && outlen < sizeof(outpacket_buf); i++) {
		PUTCHAR(0, outp);		// byte padding
		outlen ++;
	}
	
	// build ip/udp header and send it...
	acsp_ipdata_send_packet(unit, outpacket_buf, outlen, ntohl(ouraddr), IPPORT_BOOTPC, INADDR_BROADCAST, IPPORT_BOOTPS, "DHCP packet inform");

	dhcp_context->state = PLUGIN_SENDSTATE_WAITING_ACK;
	TIMEOUT(acsp_ipdata_timeout, dhcp_context, DHCP_TIMEOUT_VALUE);
}

//------------------------------------------------------------
// acsp_ipdata_input
//------------------------------------------------------------
static void
acsp_ipdata_input(int unit, u_char *pkt, int len, u_int32_t ouraddr, u_int32_t hisaddr)
{
	struct dhcp_packet *dp;

	dp = ALIGNED_CAST(struct dhcp_packet *)pkt;

	/* check if we received a DHCP broadcast from a client */
	if (acsp_intercept_dhcp
		&& ntohl(dp->ip.ip_dst.s_addr) == INADDR_BROADCAST
		&& ntohs(dp->udp.uh_sport) == IPPORT_BOOTPC
		&& ntohs(dp->udp.uh_dport) == IPPORT_BOOTPS) {
		acsp_ipdata_input_server(unit, pkt, len, ouraddr, hisaddr);
		return;
	}

	/* check if we received a DHCP reply from a server */
	if (acsp_use_dhcp 
		&& dp->ip.ip_dst.s_addr == ouraddr
		&& ntohs(dp->udp.uh_sport) == IPPORT_BOOTPS
		&& ntohs(dp->udp.uh_dport) == IPPORT_BOOTPC) {
		acsp_ipdata_input_client(unit, pkt, len, ouraddr, hisaddr);
		return;
	}
}

//------------------------------------------------------------
// acsp_ipdatre_timeout
//------------------------------------------------------------
static void
acsp_ipdata_timeout(void *arg)
{
	acsp_dhcp_context *context = (acsp_dhcp_context*)arg;    

	if (context->state == PLUGIN_SENDSTATE_WAITING_ACK) {
		if (context->retry_count++ < DHCP_MAX_RETRIES)
			acsp_ipdata_start_dhcp_client(context->unit, context->ouraddr.s_addr, context->hisaddr.s_addr);
		else {
			dbglog("No DHCP server replied\n");
			context->state = PLUGIN_STATE_DONE;
		}
	}
}    

//------------------------------------------------------------
// acsp_ipdata_up
//------------------------------------------------------------
static void
acsp_ipdata_up(int unit, u_int32_t ouraddr, u_int32_t hisaddr)
{

	/* check if dhcp is enabled and acsp not running */
	if (acsp_use_dhcp && (acscp_protent.state(unit) != OPENED)) {
		/* 
			allocate dhcp routes context 
			we don't need to keep a context for the domain
		*/
		if ((dhcp_context = (acsp_dhcp_context*)malloc(sizeof(acsp_dhcp_context))) == 0) {
			error("ACSP plugin: no memory to allocate DHCP routes context\n");
			return;
		}
		bzero(dhcp_context, sizeof(acsp_dhcp_context));
		dhcp_context->unit = unit;
		dhcp_context->ouraddr.s_addr = ouraddr;
		dhcp_context->hisaddr.s_addr = hisaddr;
		dhcp_context->state = PLUGIN_STATE_INITIAL;
		
		/* start dhcp client */
		acsp_ipdata_start_dhcp_client(unit, ouraddr, hisaddr);
	}
}

//------------------------------------------------------------
// acsp_ipdata_down
//------------------------------------------------------------
static void
acsp_ipdata_down(int unit)
{
	if (acsp_use_dhcp) {
		/* cleanup route */
		if (dhcp_context) {
			acsp_plugin_remove_routes(dhcp_context->route);
			UNTIMEOUT(acsp_ipdata_timeout, dhcp_context);
			free(dhcp_context);
			dhcp_context = NULL;
		}

	}
}

//------------------------------------------------------------
// acsp_ipdata_print
//------------------------------------------------------------
static int
acsp_ipdata_print(pkt, plen, printer, arg)
    u_char *pkt;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
	u_char *p;
	int i, isbootp = 0, len = plen;
	struct dhcp *dp;
	struct dhcp_packet *packet;
	u_int32_t masklen, addrlen, addr, router, mask, l;
	char str[2048];
	u_int32_t cookie;
	char str2[16];

	packet = ALIGNED_CAST(struct dhcp_packet *)pkt;

	/* check if we received a DHCP broadcast from a client */
	isbootp = 
		(ntohs(packet->udp.uh_sport) == IPPORT_BOOTPC || ntohs(packet->udp.uh_sport) == IPPORT_BOOTPS)
		&& (ntohs(packet->udp.uh_dport) == IPPORT_BOOTPS || ntohs(packet->udp.uh_dport) == IPPORT_BOOTPC);

	if (!isbootp)
		return 0;
	
	dp = (struct dhcp *)&packet->dhcp;

	printer(arg, " <src addr %s>",  inet_ntoa(packet->ip.ip_src));
	printer(arg, " <dst addr %s>",  inet_ntoa(packet->ip.ip_dst));

	if (dp->dp_op != BOOTREPLY && dp->dp_op != BOOTREQUEST) {
		printer(arg, " <bootp code invalid!>");
		return 0;
	}
		
	printer(arg, " <BOOTP %s>", dp->dp_op == BOOTREQUEST ? "Request" : "Reply");

	/* if superverbose debug, perform additional decoding onm the packet */
	if (debug > 1) {
		printer(arg, " <htype %d>", dp->dp_htype);
		printer(arg, " <hlen %d>", dp->dp_hlen);
		printer(arg, " <hops %d>", dp->dp_hops);
		printer(arg, " <xid %d>", dp->dp_xid);
		printer(arg, " <flags %d>", dp->dp_flags);
		printer(arg, " <client address %s>", inet_ntoa(dp->dp_ciaddr));
		printer(arg, " <your address %s>", inet_ntoa(dp->dp_yiaddr));
		printer(arg, " <server address %s>", inet_ntoa(dp->dp_siaddr));
		printer(arg, " <gateway address %s>", inet_ntoa(dp->dp_giaddr));

		p = &dp->dp_chaddr[0];
		snprintf(str, sizeof(str), "%02x", p[0]);
		for (i = 1; i < 16; i++) {
			snprintf(str2, sizeof(str2), ":%02x", p[i]);
			strlcat(str, str2, sizeof(str));
		}
		printer(arg, " <hardware address %s>",  str);
		printer(arg, " <server host name \"%s\">", dp->dp_sname);
		printer(arg, " <boot file name \"%s\">", dp->dp_file);
	}
	
	p = dp->dp_options;
    GETLONG(l, p);
	cookie = l;
	if (cookie != DHCP_COOKIE) {
		printer(arg, " <cookie invalid!>");
		return 0;
	}
	if (debug > 1)
		printer(arg, " <cookie 0x%x>", DHCP_COOKIE);

	if (*p++ != DHCP_OPTION_MSG_TYPE || *p++ != 1 || (*p != DHCP_OPTION_MSG_TYPE_INFORM && *p != DHCP_OPTION_MSG_TYPE_ACK)) {
		printer(arg, " <type invalid!>");
		return 0;
	}
	
	printer(arg, " <type %s>",  *p == DHCP_OPTION_MSG_TYPE_INFORM ? "INFORM" : "ACK");
	p++;

	len = plen - sizeof(struct dhcp_packet) - 7;
	while (*p != DHCP_OPTION_END && len > 0) {
		u_int8_t optcode, optlen;
		
		optcode = *p++;
		// check for pad option
		if (optcode == 0) {
			len--;
			continue;
		}
			
		optlen = *p++;
		len-=2;
		if (len == 0) {
			printer(arg, " <option %d zero len!>", optcode);
			return 0;
		}
		
		str[0] = 0;
		switch (optcode) {
			case DHCP_OPTION_HOST_NAME:
				memcpy(str, p, optlen);
				str[optlen] = 0;
				printer(arg, " <host name \"%s\">",  str);
				break;
			case DHCP_OPTION_VENDOR_CLASS_ID:
				memcpy(str, p, optlen);
				str[optlen] = 0;
				printer(arg, " <vendor class id \"%s\">",  str);
				break;
			case DHCP_OPTION_CLIENT_ID:
				snprintf(str, sizeof(str), "0x");
				for (i = 0; i < optlen; i++) {
					snprintf(str2, sizeof(str2), "%02x", p[i]);
					strlcat(str, str2, sizeof(str));
				}
				printer(arg, " <client id %s>",  str);
				break;
			case DHCP_OPTION_SERVER_ID:
				snprintf(str, sizeof(str), "0x");
				for (i = 0; i < optlen; i++) {
					snprintf(str2, sizeof(str2), "%02x", p[i]);
					strlcat(str, str2, sizeof(str));
				}
				printer(arg, " <server id %s>",  str);
				break;
			case DHCP_OPTION_LEASE_TIME:
                GETLONG(l, p);                  // Wcast-align fix - unaligned access
				printer(arg, " <lease time %d sec>",  l);
                p -= 4;
				break;
			case DHCP_OPTION_SUBNET_MASK:
				printer(arg, " <subnet mask %d.%d.%d.%d>",  p[0], p[1], p[2], p[3]);
				break;
			case DHCP_OPTION_DOMAIN_NAME:
				memcpy(str, p, optlen);
				str[optlen] = 0;
				printer(arg, " <domain name \"%s\">",  str);
				break;
			case DHCP_OPTION_STATIC_ROUTE:
				printer(arg, " <static routes");
				i = 0;
				while (i < optlen) {
					masklen = p[i];
					mask = 0xFFFFFFFF << (32 - masklen);
					addrlen = (masklen / 8);
					if (masklen % 8)
						addrlen++;
                    // Wcast-align fix - memcpy for unaligned access
                    memcpy(&l, &p[i+1], sizeof(l));        
					addr = ntohl(l) & mask;
                    memcpy(&l, &p[i+1+addrlen], sizeof(l));
					router = ntohl(l);
					i += addrlen + 1 + sizeof(in_addr_t);
					printer(arg, " %d.%d.%d.%d/%d.%d.%d.%d/%d.%d.%d.%d", 
						(addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr  >> 8) & 0xFF, addr & 0xFF, 
						(mask >> 24) & 0xFF, (mask >> 16) & 0xFF, (mask >> 8) & 0xFF, mask & 0xFF,
						(router >> 24) & 0xFF, (router >> 16) & 0xFF, (router >> 8) & 0xFF, router & 0xFF);
				}
				printer(arg, ">");
				break;
			case DHCP_OPTION_PARAM_REQUEST_LIST:
				for (i = 0; i < optlen; i++) {
					snprintf(str2, sizeof(str2), " 0x%x", p[i]);
					strlcat(str, str2, sizeof(str));
				}
				printer(arg, " <parameters =%s>", str);
				break;

			default:
				printer(arg, " <option %d>", optcode);
				break;
		}
		
		p+=optlen;
		len-=optlen;
	}
	
	/* if debug is superverbose, dump raw packet as well */
	if (debug > 1)
		return 0;
		
	return plen;
}

int
acsp_printpkt(p, plen, printer, arg)
u_char *p;
int plen;
void (*printer) __P((void *, char *, ...));
void *arg;
{
	int               len;
	u_int16_t         flags;
	int               slen;
    u_char           *pstart = p;
	acsp_packet      *pkt = ALIGNED_CAST(acsp_packet *)p;
    acsp_route_data	 *route_data;
	uint16_t          route_flags;
    u_int8_t          *ptr;
    acsp_domain_data  domain_data_aligned;  
	int               domain_name_len;
	char              addr_str[INET_ADDRSTRLEN];
	char              mask_str[INET_ADDRSTRLEN];
	char              domain_name[255 + 1]; // plus null-termination

	if(pkt && plen >= ACSP_HDR_SIZE) {
		len = ntohs(pkt->len);
		if (len < ACSP_HDR_SIZE)
			len = 0;
		else
			len -= ACSP_HDR_SIZE;

		flags = ntohs(pkt->flags);

	    if (pkt->type == CI_ROUTES) {
            route_data = ALIGNED_CAST(__typeof__(route_data))pkt->data;
			ACSP_PRINTPKT_PAYLOAD("CI_ROUTES");
            while (len >= sizeof(*route_data)) {
				route_flags = ntohs(route_data->flags);
                printer(arg, "\n    <route: address %s, mask %s, flags:%s%s>",
						addr2ascii(AF_INET, &route_data->address, sizeof(route_data->address), addr_str),
						addr2ascii(AF_INET, &route_data->mask, sizeof(route_data->mask), mask_str),
						((route_flags & ACSP_ROUTEFLAGS_PRIVATE) != 0)? " PRIVATE" : "",
						((route_flags & ACSP_ROUTEFLAGS_PUBLIC) != 0)? " PUBLIC" : "");
                len -= sizeof(*route_data);
                route_data++;
            }
			p = (__typeof__(p))route_data;
		} else if (pkt->type == CI_DOMAINS) {
            ptr = pkt->data;
			ACSP_PRINTPKT_PAYLOAD("CI_DOMAINS");
            while (len >= sizeof(acsp_domain_data)) {
                memcpy(&domain_data_aligned, ptr, sizeof(acsp_domain_data));      // Wcast-align fix - memcpy for unaligned move
                slen = ntohs(domain_data_aligned.len);
				domain_name_len = MIN(slen, sizeof(domain_name));
				if (slen) {
					memcpy(domain_name, ((acsp_domain_data *)(void*)ptr)->name, domain_name_len);
				}
				domain_name[domain_name_len] = 0;
				if (domain_data_aligned.server) {
					printer(arg, "\n    <domain: name %s, server %s>",
							domain_name,
							addr2ascii(AF_INET, &domain_data_aligned.server, sizeof(domain_data_aligned.server), addr_str));
				} else {
					printer(arg, "\n    <domain: name %s>",
							domain_name);
				}
                len -= (ACSP_DOMAIN_DATA_HDRSIZE + slen);
                ptr += (ACSP_DOMAIN_DATA_HDRSIZE + slen);
            }
			p = (__typeof__(p))ptr;
		} else {
			ACSP_PRINTPKT_PAYLOAD(NULL);
			p = pkt->data;
		}
		return (p - pstart);
	}
	return 0;
}

