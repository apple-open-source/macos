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

extern char *serverid; 	// defined in sys-MacOSX.c
extern char *serviceid; // defined in sys-MacOSX.c


static void acsp_start_plugin(acsp_ext *ext, int mtu);
static void acsp_stop_plugin(acsp_ext *ext);
static void acsp_output(acsp_ext *ext);
static void acsp_timeout(void *arg);

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
acsp_init_plugins(void *arg, int phase)
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
}

//------------------------------------------------------------
// acsp_start_plugin
//------------------------------------------------------------
void
acsp_start(int mtu)
{
    acsp_ext	*ext;
    
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

//------------------------------------------------------------
// acsp_data_input
//------------------------------------------------------------
void 
acsp_data_input(int unit, u_char *packet, int len)
{
    
    acsp_ext 	*ext;
    acsp_packet *pkt = (acsp_packet*)packet;
    
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

    if (pkt->flags & ACSP_FLAG_ACK && ext->timer_state == ACSP_TIMERSTATE_PACKET && pkt->seq == ext->last_seq) {
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
                    *((u_int16_t*)ptr) = PPP_ACSP;
                    ptr += 2;
                    
                    if (ext->out.action == ACSP_ACTION_SEND_WITH_TIMEOUT) {
                        if (ext->timer_state != ACSP_TIMERSTATE_STOPPED)
                            UNTIMEOUT(acsp_timeout, ext);
                        ext->timer_state = ACSP_TIMERSTATE_PACKET;
                        ext->last_seq = ((acsp_packet*)ptr)->seq; 
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


//
// globals
//

// lists of routes and domains read from the config file
// these lists are placed in the context for the accociated
// option type when the context is created and intialized
static acsp_route 	*routes_list;		
static acsp_domain 	*domains_list;	
static struct in_addr	primary_router;		// address of primary router (before PPP connection)

extern CFStringRef 		serviceidRef;
extern SCDynamicStoreRef	cfgCache;
extern int publish_dns_entry(CFStringRef str, CFStringRef property, int clean);
extern int route_interface(int cmd, struct in_addr host, struct in_addr mask, char iftype, char *ifname, int is_host);
extern int route_gateway(int cmd, struct in_addr dest, struct in_addr mask, struct in_addr gateway, int use_gway_flag);

//
// funtion prototypes
//
static void acsp_plugin_ip_up(void *arg, int phase);
static void acsp_plugin_ip_down(void *arg, int phase);
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

static void acsp_plugin_add_routes(acsp_plugin_context* context);
static void acsp_plugin_add_domains(acsp_plugin_context* context);
static void acsp_plugin_remove_routes(acsp_plugin_context* context);

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
    u_char 		sopt[32];
    CFTypeRef		dictRef;
    CFStringRef		string, key;
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
                if (strRef) {
                    if (CFGetTypeID(strRef) == CFStringGetTypeID()) {
                        CFStringGetCString((CFStringRef)strRef, sopt, 32, kCFStringEncodingUTF8);
                    } else if (CFGetTypeID(strRef) == CFDataGetTypeID()) {
                        string = CFStringCreateWithCharacters(NULL, (UniChar *)CFDataGetBytePtr(strRef), 									CFDataGetLength(ref)/sizeof(UniChar));                 
                        if (string) {
                            CFStringGetCString(string, sopt, 32, kCFStringEncodingUTF8);
                            CFRelease(string);
                        }
                    }
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
        if ((prefs = SCPreferencesCreate(0, SCSTR("pppd"), kRASServerPrefsFileName)) != 0) {
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
                    if (pkt->flags & ACSP_FLAG_START == 0) {
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
                            if (pkt->flags & ACSP_FLAG_END) {
                                theContext->state = PLUGIN_STATE_DONE;
                                if (theContext->ip_up)
                                    acsp_plugin_install_config(theContext);
                            }
                        } else
                            break;					// bad packet or error - send no ack
                    }					
                        
                    if (pkt->flags & ACSP_FLAG_REQUIRE_ACK)
                        acsp_plugin_send_ack(theContext, acsp_in, acsp_out);
                    break;
                
                case PLUGIN_SENDSTATE_WAITING_ACK:
                    if (pkt->flags & ACSP_FLAG_ACK)	// if ack - process it - otherwise drop the packet
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
    acsp_domain_data	*domain_data;
    
    
    pkt = (acsp_packet*)(context->buf + 4);	// leave space for address, control, and protocol
    space = MIN(context->buf_size, acsp_in->mtu) - 4;
    
    if (context->state == PLUGIN_STATE_INITIAL) {
        pkt->flags = ACSP_FLAG_START;
        context->next_to_send = context->list;
    } else
        pkt->flags = 0;

    pkt->type = context->type;
    pkt->seq = context->next_seq;
    pkt->flags |= ACSP_FLAG_REQUIRE_ACK;
    pkt->reserved = 0;
    len = ACSP_HDR_SIZE;

    switch (context->type) {   
        case CI_ROUTES:
            route_data = (acsp_route_data*)pkt->data;
            while (context->next_to_send && space >= len + sizeof(acsp_route_data)) {
                route_data->address = ((acsp_route*)context->next_to_send)->address.s_addr;
                route_data->mask = ((acsp_route*)context->next_to_send)->mask.s_addr;
                route_data->flags = ((acsp_route*)context->next_to_send)->flags;
                route_data->reserved = 0;
                len += sizeof(acsp_route_data);
                context->next_to_send = ((acsp_route*)(context->next_to_send))->next;
                route_data++;
            }
            break;
    	case CI_DOMAINS:
            domain_data = (acsp_domain_data*)pkt->data;
            while (context->next_to_send && space >= (len + (slen = strlen(((acsp_domain*)(context->next_to_send))->name)) + 6)) {
                domain_data->server = ((acsp_domain*)(context->next_to_send))->server.s_addr;
                domain_data->len = slen;
                memcpy(domain_data->name, ((acsp_domain*)(context->next_to_send))->name, slen);
                len += (slen + 6);
                context->next_to_send = ((acsp_domain*)(context->next_to_send))->next;
                domain_data = (acsp_domain_data*)(domain_data->name + slen);
            }
            break;
    }
    
    if (context->next_to_send == 0)
        pkt->flags |= ACSP_FLAG_END;
    acsp_out->action = ACSP_ACTION_SEND_WITH_TIMEOUT;
    context->last_pkt_len = pkt->len = len;
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
    outPkt = (acsp_packet*)(context->buf + 4);	// leave space for address, control, and protocol
    outPkt->type = context->type;
    outPkt->seq = inPkt->seq;
    outPkt->flags = ACSP_FLAG_ACK;
    outPkt->len = ACSP_HDR_SIZE;
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
    int			len;
    acsp_route_data	*route_data;
    acsp_domain_data	*domain_data;
    
    len = acsp_in->data_len - ACSP_HDR_SIZE;
    pkt = acsp_in->data;
    
    switch (context->type) {
        case CI_ROUTES:
            if (len % 4 != 0) {
                error("ACSP plugin: received packet with invalid len\n");
                return -1;
            }
            route_data = (acsp_route_data*)pkt->data;
            while (len > 4) {
                if ((route = (acsp_route*)malloc(sizeof(acsp_route))) == 0) {
                    error("ACSP plugin: no memory\n");
                    return -1;
                }
                route->address.s_addr = route_data->address;
                route->mask.s_addr = route_data->mask;
                route->flags = route_data->flags;
                route->installed = 0;
                route->next = (acsp_route*)context->list;
                context->list = route;
                len -= sizeof(acsp_route_data);
                route_data++;
            }
            break;
        
        case CI_DOMAINS:
            domain_data = (acsp_domain_data*)pkt->data;
            while (len > 2) {
                if ((domain = (acsp_domain*)malloc(sizeof(acsp_domain))) == 0) {
                    error("ACSP plugin: no memory\n");
                    return -1;
                }
                if ((domain->name = malloc(domain_data->len + 1)) == 0) {
                    error("ACSP plugin: no memory\n");
                    free(domain);
                    return -1;
                }
                domain->server.s_addr = domain_data->server;
                memcpy(domain->name, domain_data->name, domain_data->len);
                *(domain->name + domain_data->len) = 0;	// zero terminate
                domain->next = (acsp_domain*)context->list;
                context->list = domain;
                len -= (domain_data->len + 6);
                domain_data = (acsp_domain_data*)(domain_data->name + domain_data->len);
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
static void acsp_plugin_ip_up(void *arg, int phase)
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
static void acsp_plugin_ip_down(void *arg, int phase)
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
        acsp_plugin_add_routes(context);
    else
        acsp_plugin_add_domains(context);
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
        acsp_plugin_remove_routes(context);

    context->config_installed = 0;
}

//------------------------------------------------------------
// acsp_plugin_add_routes
//	install routes
//------------------------------------------------------------
static void acsp_plugin_add_routes(acsp_plugin_context* context)
{
    acsp_route	*route = (acsp_route*)context->list;  
    struct in_addr	addr;

    if (route) {	// only if routes are present
        sleep(1);
        addr.s_addr = 0; 
        
        // remove current default route
        if (route_gateway(RTM_DELETE, addr, addr, addr, 1) == 0)
            error("ACSP plugin: error removing default route\n");
        // add new default route on previous primary interface
        if (route_gateway(RTM_ADD, addr, addr, primary_router, 1) == 0) {
            error("ACSP plugin: error adding default route\n");
            return;
        }    
        
        while (route) {
            route->installed = 1;
            if (route->flags & ACSP_ROUTEFLAGS_PRIVATE) {
                if (route_interface(RTM_ADD, route->address, route->mask, IFT_PPP, ifname, 0) == 0) {
                    error("ACSP plugin: error installing private net route\n");
                    route->installed = 0;
                }
            } else if (route->flags & ACSP_ROUTEFLAGS_PUBLIC) {
                if (route_gateway(RTM_ADD, route->address, route->mask, primary_router, 1) == 0) {
                    error("ACSP plugin: error installing public net route\n");
                    route->installed = 0;
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
static void acsp_plugin_remove_routes(acsp_plugin_context* context)
{
    acsp_route	*route = (acsp_route*)context->list;  

    // remove only the public routes
    // the private routes will be removed when the intefaces is removed
    while (route) {
        if (route->flags & ACSP_ROUTEFLAGS_PUBLIC && route->installed) {
            if (route_gateway(RTM_DELETE, route->address, route->mask, primary_router, 0) == 0) {
                error("ACSP plugin: error removing route\n");
            } else
                route->installed = 0;
        }
        route = route->next;
    }
}

//------------------------------------------------------------
// acsp_plugin_add_domains
//	install search domains
//------------------------------------------------------------
static void acsp_plugin_add_domains(acsp_plugin_context* context)
{
    CFStringRef	str;
    acsp_domain	*domain = (acsp_domain*)context->list;    
    int 	err, clean = 1;

    while (domain) {
        if (str = CFStringCreateWithCString(NULL, domain->name, kCFStringEncodingUTF8)) {
            err = publish_dns_entry(str, kSCPropNetDNSSearchDomains, clean);
            CFRelease(str);
            if (err == 0) {
                error("ACSP plugin: error adding domain name\n");
                return;
            }
        } else {
            error("ACSP plugin: error adding domain name - could not create CFString\n");
            return;
        }
        domain = domain->next;
        clean = 0;
    }
}

