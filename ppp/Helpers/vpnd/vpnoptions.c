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
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <paths.h>
#include <sys/queue.h>
		
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "../../Family/if_ppplink.h"

#include "vpnd.h"
#include "vpnoptions.h"
#include "vpnplugins.h"
#include "RASSchemaDefinitions.h"

extern void set_terminate(void);
static char*	default_log_path = "/var/log/ppp/" DAEMON_NAME ".log";


static void usage(FILE *fp, const char *argv0);
static int plugin_exists(const char *inPath);
static char* validate_ip_string(const char *inIPString, char *outIPString, size_t outSize);
static int process_server_prefs(struct vpn_params *params);
static int process_interface_prefs(struct vpn_params *params);
static int process_ipv4_prefs(struct vpn_params *params);
static int process_ipv6_prefs(struct vpn_params *params);
static int process_dns_prefs(struct vpn_params *params);
static int process_ppp_prefs(struct vpn_params *params);


//-----------------------------------------------------------------------------
//	process_options
//-----------------------------------------------------------------------------
int process_options(struct vpn_params *params, int argc, char *argv[])
{
    
    char* 		argv0 = argv[0];
    char* 		args = "dhnxi:";
    char		c;
        
    params->max_sessions = 0;
    params->debug = 0;
    params->daemonize = 1;
    params->plugin_path = 0;
    params->serverSubTypeRef = 0;
    params->serverIDRef = 0;
    params->serverRef = 0;
    params->server_id = 0;
    params->server_subtype = PPP_TYPE_OTHER;
    params->storeRef = 0;
    params->next_arg_index = 0;
    params->log_path[0] = 0;
    
    // Process command line arguments, if any
    while ((opterr == 1) && (c = getopt(argc, argv, args)) != EOF) {
        switch (c) {
            case 'h':
                usage (stdout, argv0);
                exit (0);
                
            case 'n':
                set_terminate();
                /* FALLTHRU */

            case 'd':
                params->debug = 1;
                /* FALLTHRU */

            case 'x':
                params->daemonize = 0;
                break ;
                
            case 'i':
                params->server_id = optarg;
                break ;
                    
            default:
                usage(stderr, argv0);
                return -1;
        }
    }
                    
    return 0;
}

// ----------------------------------------------------------------------------
//	get_active_server
// ----------------------------------------------------------------------------
CFArrayRef get_active_servers(struct vpn_params *params)
{
    SCPreferencesRef 		prefs = 0;
    CFPropertyListRef		active_servers;
    CFArrayRef                  arrayCopy = 0;
    char 			pathStr[MAXPATHLEN];

    // open the prefs file
    prefs = SCPreferencesCreate(0, SCSTR("vpnd"), kRASServerPrefsFileName);
    if (prefs == NULL) {
        CFStringGetCString(kRASServerPrefsFileName, pathStr, MAXPATHLEN, kCFStringEncodingMacRoman);
        vpnlog(LOG_ERR, "VPND: Unable to read vpnd prefs file '%s'\n", pathStr);
        return 0;
    }
    // get active servers list from the plist
    active_servers = SCPreferencesGetValue(prefs, kRASActiveServers);
    if (active_servers && isArray(active_servers))
        if (CFArrayGetCount(active_servers) > 0)
            arrayCopy = CFArrayCreateCopy(0, active_servers);
    CFRelease(prefs);
    return arrayCopy;
}

// ----------------------------------------------------------------------------
//	process_prefs
// ----------------------------------------------------------------------------
int process_prefs(struct vpn_params *params)
{

    char 			pathStr[MAXPATHLEN];
    u_int32_t			len;
    SCPreferencesRef 		prefs = 0;
    CFPropertyListRef		servers_list;
    int				i;
    
    char 			text[512] = "";
  
    // clear the argument array
    params->next_arg_index = 0;
    for (i = 0; i < MAXARG; i++)
        params->exec_args[i] = 0;
    
    // init the address lists
    init_address_lists();	
    
    // open the prefs file
    prefs = SCPreferencesCreate(0, SCSTR("vpnd"), kRASServerPrefsFileName);
    if (prefs == NULL) {
        CFStringGetCString(kRASServerPrefsFileName, pathStr, MAXPATHLEN, kCFStringEncodingMacRoman);
        sprintf(text, "VPND: Unable to read vpnd prefs file '%s'\n", pathStr);
        goto fail;
    }
    // get servers list from the plist
    servers_list = SCPreferencesGetValue(prefs, kRASServers);
    if (servers_list == NULL) {
        sprintf(text, "VPND: could not get servers dictionary\n");
        goto fail;
    }
    // retrieve the information for the given Server ID
    params->serverIDRef = CFStringCreateWithCString(0, params->server_id, kCFStringEncodingMacRoman);
    if (params->serverIDRef == NULL) {
        sprintf(text, "VPND: could not create CFString for server ID\n");
        goto fail;
    }
    params->serverRef = CFDictionaryGetValue(servers_list, params->serverIDRef);
    if (params->serverRef == NULL || isDictionary(params->serverRef) == 0) {
        sprintf(text, "VPND: Server ID '%s' invalid\n", params->server_id);
        goto fail;
    }
    CFRetain(params->serverRef);
    CFRelease(prefs);
    prefs = 0;
    
    // open a connection to the dynamic store 
    open_dynamic_store(params); 
    
    // add the pppd program name to the exec args
    addparam(params->exec_args, &params->next_arg_index, PPPD_PRGM);	

    // add some general args that are always needed
    addstrparam(params->exec_args, &params->next_arg_index, "serverid", params->server_id);	// server ID
    addparam(params->exec_args, &params->next_arg_index, "nodetach");	// we don't want pppd to detach.
    addparam(params->exec_args, &params->next_arg_index, "proxyarp");  	// we proxy for the client

    // process the dictionaries
    if (process_server_prefs(params))
        goto fail;
    if (process_interface_prefs(params))
        goto fail;
    if (process_ipv4_prefs(params))
        goto fail;
    if (process_ipv6_prefs(params))
        goto fail;
    if (process_dns_prefs(params))
        goto fail;
    if (process_ppp_prefs(params))
        goto fail;

    // always try to use options defined in /etc/ppp/peers/[service provider] 
    // they can override what have been specified by the prefs file
    // be careful to the conflicts on options
    get_str_option(params->serverRef, kRASEntPPP, kRASPropUserDefinedName, pathStr, &len, "");
    if (pathStr[0])
        addstrparam(params->exec_args, &params->next_arg_index, "call", pathStr);

#if 0
    syslog(LOG_INFO, "/usr/sbin/pppd ");
    for (i = 1; i < MAXARG; i++) {
        if (cmdarg[i]) {
            syslog(LOG_INFO, "%d :  %s\n", i, cmdarg[i]);
           //printf("%s ", cmdarg[i]);
        }
    }
    syslog(LOG_INFO, "\n");
#endif

    return 0;

fail:

    vpnlog(LOG_ERR, text[0] ? text : "VPND: error while reading preferences\n");
    if (prefs)
        CFRelease(prefs);
    return -1;
}


//-----------------------------------------------------------------------------
//	process_server_prefs
//-----------------------------------------------------------------------------
static int process_server_prefs(struct vpn_params *params)
{
    u_int32_t		lval, len;
    char            	str[MAXPATHLEN] ;
       
    get_int_option(params->serverRef, kRASEntServer, kRASPropServerMaximumSessions, &lval, 0);
    if (lval)
        params->max_sessions = lval;
    get_str_option(params->serverRef, kRASEntServer, kRASPropServerLogfile, str, &len, default_log_path);
    if (str[0])
        memcpy(params->log_path, str, len + 1);

    get_int_option(params->serverRef, kRASEntServer, kRASPropServerVerboseLogging, &lval, 0);
    if (lval)
        params->debug = lval;
    
    return 0;
}

//-----------------------------------------------------------------------------
//	process_interface_prefs
//-----------------------------------------------------------------------------
static int process_interface_prefs(struct vpn_params *params)
{
    CFStringRef 	str = 0;
    CFDictionaryRef	dict;
       
    //  get type/subtype of server
    dict = CFDictionaryGetValue(params->serverRef, kRASEntInterface);
    if (isDictionary(dict)) {
        // server type MUST be PPP
        str  = CFDictionaryGetValue(dict, kRASPropInterfaceType);
        if (!isString(str)
            || CFStringCompare(str, kRASValInterfaceTypePPP, 0) != kCFCompareEqualTo) {
            vpnlog(LOG_ERR, "VPND: incorrect server type found\n");
            return -1;
        }
        // get server subtype and check if supported
        params->serverSubTypeRef = CFDictionaryGetValue(dict, kRASPropInterfaceSubType);
        if (!isString(params->serverSubTypeRef)) {
            vpnlog(LOG_ERR, "VPND: incorrect server subtype found\n");
            return -1;
        }
        CFRetain(params->serverSubTypeRef);
        params->plugin_path = malloc(CFStringGetLength(params->serverSubTypeRef) + 5); 
        CFStringGetCString(params->serverSubTypeRef, params->plugin_path, 
                            CFStringGetLength(params->serverSubTypeRef) + 5, kCFStringEncodingUTF8);
        strcat(params->plugin_path, ".ppp");
        if (!plugin_exists(params->plugin_path)) {
            vpnlog(LOG_ERR, "VPND: unsupported plugin '%s'\n", params->plugin_path);
            return -1;
        }
        
        // add the vpn protocol plugin parameter to the exec args
        addstrparam(params->exec_args, &params->next_arg_index, "plugin", params->plugin_path);

        if (CFStringCompare(params->serverSubTypeRef, kRASValInterfaceSubTypePPTP, 0) == kCFCompareEqualTo)
            params->server_subtype = PPP_TYPE_PPTP;
        else if (CFStringCompare(params->serverSubTypeRef, kRASValInterfaceSubTypeL2TP, 0) == kCFCompareEqualTo)
            params->server_subtype = PPP_TYPE_L2TP;
        else if (CFStringCompare(params->serverSubTypeRef, kRASValInterfaceSubTypePPPoE, 0) == kCFCompareEqualTo)
            params->server_subtype = PPP_TYPE_PPPoE;
        else if (CFStringCompare(params->serverSubTypeRef, kRASValInterfaceSubTypePPPSerial, 0) == kCFCompareEqualTo)
            params->server_subtype = PPP_TYPE_SERIAL;
        else 
            params->server_subtype = PPP_TYPE_OTHER;
            
    } else
        return -1;
    return 0;
}

//-----------------------------------------------------------------------------
//	process_ipv4_prefs
//-----------------------------------------------------------------------------
static int process_ipv4_prefs(struct vpn_params *params)
{
    CFArrayRef		array = 0;
    CFStringRef		ipstr = 0, ipstr2 = 0;
    CFDictionaryRef	dict;
    char 		str[MAXPATHLEN];
    int 		i, nb, len;
    char		ipcstr[100], ipcstr2[100], ip_addr[100], ip_addr2[100];
    char		*ip, *ip2;

    // Check if the IPv4 dictionary is present
    if ((dict = CFDictionaryGetValue(params->serverRef, kRASEntIPv4)) && isDictionary(dict)) {
    
        // get server side address
        get_array_option(params->serverRef, kRASEntIPv4, kRASPropIPv4Addresses, 0, str, &len, "");
        if (str[0] == 0) {
            // get the address of the default interface
            ipstr = CopyDefaultIPAddress();
            if (ipstr) {
                CFStringGetCString(ipstr, str, sizeof(str), kCFStringEncodingMacRoman);
                CFRelease(ipstr);
                ipstr = 0;
            }
        }
        if (str[0]) {
            strcat(str, ":");
            addparam(params->exec_args, &params->next_arg_index, str);
        }
        
        // build client address list
        if (isDictionary(dict)) {
            /* individual ip addresses */
            array  = CFDictionaryGetValue(dict, kRASPropIPv4DestAddresses);
            if (isArray(array)) {
            
                nb = CFArrayGetCount(array);
                for (i = 0; i < nb; i++) {
                    ipstr = CFArrayGetValueAtIndex(array, i);
                    if (isString(ipstr)) {
                        if (CFStringGetCString(ipstr, ipcstr, sizeof(ipcstr), kCFStringEncodingMacRoman)) {
                            if (ip = validate_ip_string(ipcstr, ip_addr, sizeof(ip_addr))) {
                                if (add_address(ip)) {
                                    vpnlog(LOG_ERR, "VPND: error while processing ip address %s\n", ip);
                                    return -1;
                                }
                            } else
                                vpnlog(LOG_ERR, "VPND: Ignoring invalid ip address %s\n", ipcstr);
                        }
                    }
                }
            }
            // ip address ranges
            array  = CFDictionaryGetValue(dict, kRASPropIPv4DestAddressRanges);
            if (isArray(array)) {
                if (CFArrayGetCount(array) % 2)
                    vpnlog(LOG_ERR, "VPND: error - ip address ranges must be in pairs\n");
                else {
                    nb = CFArrayGetCount(array);
                    for (i = 0; i < nb; i += 2) {
                        ipstr = CFArrayGetValueAtIndex(array, i);
                        ipstr2 = CFArrayGetValueAtIndex(array, i+1);
                        if (isString(ipstr) && isString(ipstr2)) {
                            if (CFStringGetCString(ipstr, ipcstr, sizeof(ipcstr), kCFStringEncodingMacRoman) &&
                                CFStringGetCString(ipstr2, ipcstr2, sizeof(ipcstr2), kCFStringEncodingMacRoman)) {                            
                                ip = validate_ip_string(ipcstr, ip_addr, sizeof(ip_addr));
                                ip2 = validate_ip_string(ipcstr2, ip_addr2, sizeof(ip_addr2));
                                if (ip && ip2) {
                                    if (add_address_range(ip, ip2)) {
                                        vpnlog(LOG_ERR, "VPND: error while processing ip address range %s\n", ip);
                                        return -1;
                                    }
                                } else
                                    vpnlog(LOG_ERR, "VPND: Ignoring invalid ip address range %s\n", ipcstr);
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (!address_avail()) {
        vpnlog(LOG_ERR, "VPND: No valid client IP addresses\n");
        return -1;
    }

    return 0;
    
}

// ----------------------------------------------------------------------------
//	process_ipv6_prefs
// ----------------------------------------------------------------------------
static int process_ipv6_prefs(struct vpn_params *params)
{
    CFDictionaryRef	dict;

    // Check if the IPv6 dictionary is present
    if ((dict = CFDictionaryGetValue(params->serverRef, kRASEntIPv6)) && isDictionary(dict)) {
        addparam(params->exec_args, &params->next_arg_index, "+ipv6");
        addparam(params->exec_args, &params->next_arg_index, "ipv6cp-use-persistent");
    }
    return 0;
}

// ----------------------------------------------------------------------------
//	process_dns_prefs
// ----------------------------------------------------------------------------
static int process_dns_prefs(struct vpn_params *params)
{
    CFPropertyListRef	ref = 0;
    CFDictionaryRef	dict;	
    CFStringRef		key = 0;
    CFArrayRef		array;
    CFStringRef		addr; 

    char 		str[OPT_STR_LEN];
    int			count, i;

    // Check if the IPv4 dictionary is present
    if (isDictionary(CFDictionaryGetValue(params->serverRef, kRASEntIPv4))) {
        // get the DNS address array from the plist for from the dynamic store
        dict = CFDictionaryGetValue(params->serverRef, kSCEntNetDNS);
        if (isDictionary(dict)) {
            array = CFDictionaryGetValue(dict, kRASPropDNSOfferedServerAddresses);
            if (isArray(array)) {
                count = CFArrayGetCount(array);
                if (count == 0) {		// array is present but empty - get addresses from dynamic store
                    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(0, kSCDynamicStoreDomainState, kSCEntNetDNS);
                    if (key) {
                        ref = SCDynamicStoreCopyValue(params->storeRef, key);
                        if (isDictionary(ref)) {
                            array = CFDictionaryGetValue(ref, kSCPropNetDNSServerAddresses);
                            if (isArray(array))
                                count = CFArrayGetCount(array);
                        }
                        CFRelease(key);
                    }
                }
                                    
                for (i = 0; i < count && i < 2; i++) {
                    addr = CFArrayGetValueAtIndex(array, i);
                    if (isString(addr)) {
                        str[0] = 0;
                        CFStringGetCString(addr, str, OPT_STR_LEN, kCFStringEncodingUTF8);
                        if (str[0])
                            addstrparam(params->exec_args, &params->next_arg_index, "ms-dns", str);
                    }
                }
                // free items from the store if required
                if (ref)
                    CFRelease(ref);
            }
        }
    }

    return 0;
}

// ----------------------------------------------------------------------------
//	process_ppp_prefs
// ----------------------------------------------------------------------------
static int process_ppp_prefs(struct vpn_params *params)
{
    char 		pathStr[MAXPATHLEN], optStr[OPT_STR_LEN];
    u_int32_t		len, lval, lval1, i;
    CFDictionaryRef	dict;
    CFArrayRef		array;
    CFIndex		count;
    CFStringRef		string;
    int			noCCP;

    //
    // some basic admin options 
    //
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPVerboseLogging, &lval, 0);
    if (lval)
        addparam(params->exec_args, &params->next_arg_index, "debug");

    get_str_option(params->serverRef, kRASEntPPP, kRASPropPPPLogfile, optStr, &len, "");
    if (optStr[0]) {
        // if logfile start with /, it's a full path
        // otherwise it's relative to the logs folder (convention)
        // we also strongly advise to name the file with the link number
        // for example ppplog0
        // the default path is /var/log
        // it's useful to have the debug option with the logfile option
        // with debug option, pppd will log the negociation
        // debug option is different from kernel debug trace

        sprintf(pathStr, "%s%s", optStr[0] == '/' ? "" : DIR_LOGS, optStr);
        addstrparam(params->exec_args, &params->next_arg_index, "logfile", pathStr);
    }
                    
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPDisconnectOnIdle, &lval, 0);
    if (lval) {
        get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPDisconnectOnIdleTimer, &lval, OPT_COMM_IDLETIMER_DEF);
        if (lval)
            addintparam(params->exec_args, &params->next_arg_index, "idle", lval);
    }

    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPUseSessionTimer, &lval, 0);
    if (lval) {
        get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPSessionTimer, &lval, OPT_COMM_IDLETIMER_DEF);
        if (lval)
            addintparam(params->exec_args, &params->next_arg_index, "maxconnect", lval);
    }
    
    //
    // LCP options
    //
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPEchoEnabled, &lval, 0);
    if (lval) {
        get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPEchoInterval, &lval, OPT_LCP_ECHOINTERVAL_DEF);
        if (lval)
            addintparam(params->exec_args, &params->next_arg_index, "lcp-echo-interval", lval);
        get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPEchoFailure, &lval, OPT_LCP_ECHOINTERVAL_DEF);
        if (lval)
            addintparam(params->exec_args, &params->next_arg_index, "lcp-echo-failure", lval);
    }
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPCompressionACField, &lval, OPT_LCP_PCOMP_DEF);
    if (lval == 0) 
        addparam(params->exec_args, &params->next_arg_index, "noaccomp");
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPCompressionPField, &lval, OPT_LCP_ACCOMP_DEF);
    if (lval == 0) 
        addparam(params->exec_args, &params->next_arg_index, "nopcomp");

    switch (params->server_subtype) {
        case PPP_TYPE_PPPoE:
            lval = OPT_LCP_MRU_PPPoE_DEF;
            break;
        case PPP_TYPE_PPTP:
            lval = OPT_LCP_MRU_PPTP_DEF;
            break;
        case PPP_TYPE_L2TP:
            lval = OPT_LCP_MRU_L2TP_DEF;
            break;
        default:
            lval = OPT_LCP_MRU_DEF;
    }
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPMRU, &lval, lval);
    if (lval) 
        addintparam(params->exec_args, &params->next_arg_index, "mru", lval);

    switch (params->server_subtype) {
        case PPP_TYPE_PPPoE:
            lval = OPT_LCP_MTU_PPPoE_DEF;
            break;
        case PPP_TYPE_PPTP:
            lval = OPT_LCP_MTU_PPTP_DEF;
            break;
        case PPP_TYPE_L2TP:
            lval = OPT_LCP_MTU_L2TP_DEF;
            break;
        default:
            lval = OPT_LCP_MTU_DEF;
    }
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPMTU, &lval, lval);
    if (lval) 
        addintparam(params->exec_args, &params->next_arg_index, "mtu", lval);

    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPReceiveACCM, &lval, OPT_LCP_RCACCM_DEF);
    if (lval) 
        addintparam(params->exec_args, &params->next_arg_index, "asyncmap", lval);
    else 
        addparam(params->exec_args, &params->next_arg_index, "receive-all");

    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPLCPReceiveACCM, &lval, OPT_LCP_RCACCM_DEF);
    if (lval) {
        pathStr[0] = 0;
        for (lval1 = 0; lval1 < 32; lval1++) {
            if ((lval >> lval1) & 1) {
                sprintf(optStr, "%d,", lval1);
                strcat(pathStr, optStr);
            }
        }
        pathStr[strlen(pathStr)-1] = 0; // remove last ','
        addstrparam(params->exec_args, &params->next_arg_index, "escape", pathStr);
    }

    //
    // IPCP options
    //
    
    // Check if the IPv4 dictionary is present
    if (!(dict = CFDictionaryGetValue(params->serverRef, kRASEntIPv4))
        || !isDictionary(dict)) {
        addparam(params->exec_args, &params->next_arg_index, "noip");
    }
    else {
        /* XXX */
        /* enforce the source address */
        addintparam(params->exec_args, &params->next_arg_index, "ip-src-address-filter", 1);

        get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPIPCPCompressionVJ, &lval, OPT_IPCP_HDRCOMP_DEF);
        if (lval == 0) 
            addparam(params->exec_args, &params->next_arg_index, "novj");
    }
    
    //
    // CCP options
    //
    noCCP = 1;
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPCCPEnabled, &lval, 0);
    if (lval) {        
        // Fix me : to enforce use of MS-CHAP, refuse any alteration of default auth proto 
        // a dialer specifying PAP or CHAP will works without CCP/MPPE
        // even if CCP is enabled in the configuration.
        // Will be revisited when addition compression modules and
        // authentication modules will be added 
        //&& !ppp_getoptval(ppp, opts, PPP_OPT_AUTH_PROTO, &lval, &len, service) 
        //&& (lval == OPT_AUTH_PROTO_DEF)) {
        if ((dict = CFDictionaryGetValue(params->serverRef, kRASEntPPP)) != 0) {
            array = CFDictionaryGetValue(dict, kRASPropPPPCCPProtocols);
            if (isArray(array)) {
                
                int	mppe_found = 0;
                count = CFArrayGetCount(array);
                for (i = 0; i < count; i++) {
                    string = CFArrayGetValueAtIndex(array, i);
                    if (CFStringCompare(string, kRASValPPPCCPProtocolsMPPE, 0) == kCFCompareEqualTo) {
                        get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPMPPEKeySize128, &lval, 0);
                        if (lval) {        
                            mppe_found = 1;
                            addparam(params->exec_args, &params->next_arg_index, "mppe-128");
                        }
                        get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPMPPEKeySize40, &lval, 0);
                        if (lval) {        
                            mppe_found = 1;
                            addparam(params->exec_args, &params->next_arg_index, "mppe-40");
                        }
                    } else
                        vpnlog(LOG_ERR, "VPND: Invalid compression type specified - ignored\n");
                }
                if (mppe_found) {
                    noCCP = 0;
                    addparam(params->exec_args, &params->next_arg_index, "mppe-stateless");
                }
            }
        }
    }
    if (noCCP)	// no compression protocol
        addparam(params->exec_args, &params->next_arg_index, "noccp");

    
    //
    // ACSP
    //
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPACSPEnabled, &lval, 0);
    if (lval == 0)
        addparam(params->exec_args, &params->next_arg_index, "noacsp");

    
    //
    // Authentication
    //
    
    dict = CFDictionaryGetValue(params->serverRef, kRASEntPPP);
    if (isDictionary(dict)) {
        array = CFDictionaryGetValue(dict, kRASPropPPPAuthenticatorProtocol);
        if (isArray(array)) {
            if ((count = CFArrayGetCount(array)) > 1) {
                vpnlog(LOG_ERR, "%d authentication methods specified - only the first will be used\n", count);
            }
            string = CFArrayGetValueAtIndex(array, 0);
            if (isString(string)) {
                if (CFStringCompare(string, kRASValPPPAuthProtocolMSCHAP2, 0) == kCFCompareEqualTo)
                    addparam(params->exec_args, &params->next_arg_index, "require-mschap-v2");
                else if (CFStringCompare(string, kRASValPPPAuthProtocolMSCHAP1, 0) == kCFCompareEqualTo)
                    addparam(params->exec_args, &params->next_arg_index, "require-mschap");
                else if (CFStringCompare(string, kRASValPPPAuthProtocolCHAP, 0) == kCFCompareEqualTo)
                    addparam(params->exec_args, &params->next_arg_index, "require-chap");
                else if (CFStringCompare(string, kRASValPPPAuthProtocolPAP, 0) == kCFCompareEqualTo)
                    addparam(params->exec_args, &params->next_arg_index, "require-pap");
                else if (CFStringCompare(string, kRASValPPPAuthProtocolEAP, 0) == kCFCompareEqualTo) {
                    addparam(params->exec_args, &params->next_arg_index, "require-eap");  
                    
                    // add EAP plugins - must be at least one
                    int eapPluginFound = 0;
                    
                    i = 0;
                    do {
                        lval = get_array_option(params->serverRef, kRASEntPPP, kRASPropPPPAuthenticatorEAPPlugins, i++, pathStr, &len, "");
                        if (pathStr[0]) {
                            strcat(pathStr, ".ppp");	// add plugin suffix
                            if (!plugin_exists(pathStr)) {
                                vpnlog(LOG_ERR, "VPND: EAP plugin '%s' not found\n", pathStr);
                                return -1;
                            }
                            addstrparam(params->exec_args, &params->next_arg_index, "eapplugin", pathStr);
                            eapPluginFound = 1;
                        }
                    }
                    while (lval);

                    if (!eapPluginFound) {
                        vpnlog(LOG_ERR, "VPND: No EAP authentication plugin(s) specified\n");
                        return -1;
                    }
                } else {
                    vpnlog(LOG_ERR, "VPND: Unknown authentication type specified\n");
                    return -1;
                }
            }
        }
    }
                    
    //
    // Plugins
    //
    
    // add authentication plugins
    i = 0;
    do {
        lval = get_array_option(params->serverRef, kRASEntPPP, kRASPropPPPAuthenticatorPlugins, i++, pathStr, &len, "");
        if (pathStr[0]) {
            strcat(pathStr, ".ppp");	// add plugin suffix
            if (!plugin_exists(pathStr)) {
                vpnlog(LOG_ERR, "VPND: Authentication plugin '%s' not found\n", pathStr);
                return -1;
            }
            addstrparam(params->exec_args, &params->next_arg_index, "plugin", pathStr);
        }
    }
    while (lval);

    // add access control list plugins
    i = 0;
    do {
        lval = get_array_option(params->serverRef, kRASEntPPP, kRASPropPPPAuthenticatorACLPlugins, i++, pathStr, &len, "");
        if (pathStr[0]) {
            strcat(pathStr, ".ppp");	// add plugin suffix
            if (!plugin_exists(pathStr)) {
                vpnlog(LOG_ERR, "VPND: Access Control plugin '%s' not found\n", pathStr);
                return -1;
            }
            addstrparam(params->exec_args, &params->next_arg_index, "plugin", pathStr);
        }
    }
    while (lval);
    
    // add any additional plugin we want to load
    i = 0;
    do {
        lval = get_array_option(params->serverRef, kRASEntPPP, kRASPropPPPPlugins, i++, pathStr, &len, "");
        if (pathStr[0]) {
            strcat(pathStr, ".ppp");	// add plugin suffix
            if (!plugin_exists(pathStr)) {
                vpnlog(LOG_ERR, "VPND: Plugin '%s' not found\n", pathStr);
                return -1;
            }
            addstrparam(params->exec_args, &params->next_arg_index, "plugin", pathStr);
        }
    }
    while (lval);
    
    return 0;
}

//-----------------------------------------------------------------------------
//	publish_state
//-----------------------------------------------------------------------------
int publish_state(struct vpn_params* params)
{
    CFMutableDictionaryRef	dict;
    CFStringRef			key;
    int 			val;
    CFNumberRef			num;
    
    if (params->storeRef == 0)
        return 0;

    /* Interface information */
    key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@/%s"), 
                kSCDynamicStoreDomainState,
                kSCCompNetwork, kRASRemoteAccessServer, params->serverIDRef, "Interface");
    if (key) {
        dict = CFDictionaryCreateMutable(0, 0, 
                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (dict) {
            CFDictionarySetValue(dict, kRASPropInterfaceType,  kRASValInterfaceTypePPP);
            CFDictionarySetValue(dict, kRASPropInterfaceSubType,  params->serverSubTypeRef);
            SCDynamicStoreAddTemporaryValue(params->storeRef, key, dict);           
            CFRelease(dict);
        }
        CFRelease(key);
    }
    
    /* Server information */
    key = SCDynamicStoreKeyCreate(0, CFSTR("%@/%@/%@/%@/%s"), 
                kSCDynamicStoreDomainState,
                kSCCompNetwork, kRASRemoteAccessServer, params->serverIDRef, "Server");
    if (key) {
        dict = CFDictionaryCreateMutable(0, 0, 
                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (dict) {
            val = getpid();
            num = CFNumberCreate(0, kCFNumberIntType, &val);
            if (num) {
                CFDictionarySetValue(dict, CFSTR("pid"),  num);
                SCDynamicStoreAddTemporaryValue(params->storeRef, key, dict); 
                CFRelease(num);
            }
            CFRelease(dict);
        }
        CFRelease(key);
    }
    
    return 1;
}

//-----------------------------------------------------------------------------
//	open_dynamic_store
//-----------------------------------------------------------------------------
void open_dynamic_store(struct vpn_params* params)
{
    params->storeRef = SCDynamicStoreCreate(0, CFSTR("vpnd"), 0, 0);
}

//-----------------------------------------------------------------------------
//	check_conflicts
//-----------------------------------------------------------------------------
int check_conflicts(struct vpn_params *params)
{
    
    CFArrayRef			array;
    CFStringRef			pattern, key;
    CFStringRef			type, subtype;
    CFPropertyListRef		ref;
    int				count, i, ret = 0;
    char			str[OPT_STR_LEN];

    if (params->storeRef == 0)
        return 0;

    pattern = CFStringCreateWithFormat(0, 0, CFSTR("%@/%@/%@/%s/%@"), kSCDynamicStoreDomainState, 
                    kSCCompNetwork, kRASRemoteAccessServer, ".*", kRASEntInterface);

    if (pattern) {
        if (array = SCDynamicStoreCopyKeyList(params->storeRef, pattern)) {
            count = CFArrayGetCount(array);
            for (i = 0; i < count; i++) {
                key = CFArrayGetValueAtIndex(array, i);
                if (key) {
                    ref = SCDynamicStoreCopyValue(params->storeRef, key);
                    if (isDictionary(ref)) {
                        type = CFDictionaryGetValue(ref, kRASPropInterfaceType);
                        subtype = CFDictionaryGetValue(ref, kRASPropInterfaceSubType);
                        if (isString(type) && isString(subtype) &&
                            (CFStringCompare(type, kRASValInterfaceTypePPP, 0) == kCFCompareEqualTo) &&
                            (CFStringCompare(subtype, params->serverSubTypeRef, 0) == kCFCompareEqualTo)) {
                            CFStringGetCString(subtype, str, OPT_STR_LEN, kCFStringEncodingMacRoman);
                            vpnlog(LOG_ERR, "VPND: Server for subtype %s already running - vpnd launch failed.\n", str);
                            ret = -1;
                            CFRelease(ref);
                            break;
                        }
                        CFRelease(ref);
                    }
                }
            }
            CFRelease(array);
        }
        CFRelease(pattern);
    }
    return ret;
}

//-----------------------------------------------------------------------------
//	kill_orphans
//-----------------------------------------------------------------------------
int kill_orphans(struct vpn_params* params)
{
    
    CFArrayRef			array;
    CFStringRef			pattern, key;
    CFMutableStringRef		mutable_key;
    CFStringRef			server_id;
    CFNumberRef			pidRef;
    CFPropertyListRef		interface_dict, ppp_dict;
    int				count, i, pid;
    int				ret = 0;
    

    if (params->storeRef == 0)
        return 0;


    pattern = CFStringCreateWithFormat(0, 0, CFSTR("%@/%@/%@/%s/%@"), kSCDynamicStoreDomainState, 
                    kSCCompNetwork, kSCCompService, ".*", kSCEntNetInterface);
    
    if (pattern) {
        if (array = SCDynamicStoreCopyKeyList(params->storeRef, pattern)) {
            count = CFArrayGetCount(array);
            // for each pppd - check if server id is the same as ours
            for (i = 0; i < count; i++) {
                key = CFArrayGetValueAtIndex(array, i);
                if (key) {
                    interface_dict = SCDynamicStoreCopyValue(params->storeRef, key);
                    if (isDictionary(interface_dict)) {
                        server_id = CFDictionaryGetValue(interface_dict, SCSTR("ServerID"));
                        if (isString(server_id) && CFStringCompare(server_id, params->serverIDRef, 0) == kCFCompareEqualTo) {
                            // server id matches - get the pid and kill it
                            mutable_key = CFStringCreateMutableCopy(0, 0, key);
                            // modify the key to get the PPP dictionary
                            if (CFStringFindAndReplace(mutable_key, kSCEntNetInterface, kSCEntNetPPP, 
                                    CFRangeMake(0, CFStringGetLength(mutable_key)), kCFCompareBackwards | kCFCompareAnchored)) {
                                ppp_dict = SCDynamicStoreCopyValue(params->storeRef, mutable_key);
                                CFRelease(mutable_key);
                                if (isDictionary(ppp_dict)) {
                                    pidRef = CFDictionaryGetValue(ppp_dict, SCSTR("pid"));
                                    if (isNumber(pidRef))
                                        if (CFNumberGetValue(pidRef, kCFNumberIntType, &pid))
                                            ret = kill(pid, SIGTERM);
                                }
                                if (ppp_dict)
                                    CFRelease(ppp_dict);
                            }
                        }
                    }
                    if (interface_dict)
                        CFRelease(interface_dict);
                }
            }
            CFRelease(array);
        }
        CFRelease(pattern);
    }
    return ret;
}

// ----------------------------------------------------------------------------
//	usage
// ----------------------------------------------------------------------------
static void usage(FILE *fp, const char *argv0)
{
	static const char* 	szpUsage =
		"Usage:\t%s [-dhnx] -i serverID\n"
		"	-h		this message\n"
		"	-x		does not move to background\n"
		"	-d		enable debug mode\n"
		"	-n		same as -d but terminates after validation\n"
		"	-i		server ID for this server (ex: com.apple.ppp.l2tp)\n"
		;
    fprintf (fp, szpUsage, argv0);
}

// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
Boolean isDictionary (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFDictionaryGetTypeID());
}

Boolean isArray (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFArrayGetTypeID());
}

Boolean isString (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFStringGetTypeID());
}

Boolean isNumber (CFTypeRef obj)
{
    return (obj && CFGetTypeID(obj) == CFNumberGetTypeID());
}

//-----------------------------------------------------------------------------
//	plugin_exists - checks to see if the given plugin exists.
//-----------------------------------------------------------------------------
static int plugin_exists(const char *inPath)
{
    struct stat	sbTemp, *sbpTemp = &sbTemp;
    char path[MAXPATHLEN];

    path[0] = 0;
    if (inPath[0] != '/') 
        strcpy(path, PLUGINS_DIR);
    strcat(path, inPath);

    if (stat(path, sbpTemp))
            return 0;
    //if (!S_ISREG (sbpTemp->st_mode)) {
     //   errno = ENOENT; 
     //   return-1;
   // }
    return 1;
}

//-----------------------------------------------------------------------------
//	validate_ipstring
//-----------------------------------------------------------------------------
static char* validate_ip_string(const char *inIPString, char *outIPString, size_t outSize)
{

    int		nErr ;
    char*	outIP;

    if (!inIPString)
            return 0;
    if (!*inIPString)
            return 0;
    if (outSize < 16)
            return 0;

    // First, ask the system to look up the given name.
    struct hostent	*hesp = getipnodebyname (inIPString, AF_INET, 0, &nErr);
    if (hesp == NULL)
        return 0;
    // Convert the returned info to dotted decimal string.
    outIP = inet_ntop(AF_INET, hesp->h_addr_list[0], outIPString, outSize);
    freehostent (hesp);
    return outIP;
}

// ----------------------------------------------------------------------------
//	get_array_option
// ----------------------------------------------------------------------------
int get_array_option(CFPropertyListRef options, CFStringRef entity, CFStringRef property, CFIndex index,
            u_char *opt, u_int32_t *outlen, u_char *defaultval)
{
    CFDictionaryRef	dict;
    CFArrayRef		array;
    CFIndex		count;
    CFStringRef		string;

    dict = CFDictionaryGetValue(options, entity);
    if (isDictionary(dict)) {
        
        array = CFDictionaryGetValue(dict, property);
        if (isArray(array)
            && (count = CFArrayGetCount(array)) > index) {
            string = CFArrayGetValueAtIndex(array, index);
            if (isString(string)) {
                opt[0] = 0;
                CFStringGetCString(string, opt, OPT_STR_LEN, kCFStringEncodingMacRoman);
                *outlen = strlen(opt);
            }
            return (count > (index + 1));
        }
    }
    
    strcpy(opt, defaultval);
    *outlen = strlen(opt);
    return 0;
}

// ----------------------------------------------------------------------------
//	get_str_option
// ----------------------------------------------------------------------------
void get_str_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property, 
                        u_char *opt, u_int32_t *outlen, u_char *defaultval)
{
    CFDictionaryRef	dict;
    CFStringRef		ref;
    
    dict = CFDictionaryGetValue(options, entity);
    if (isDictionary(dict)) {
        opt[0] = 0;
        ref  = CFDictionaryGetValue(dict, property);
        if (isString(ref)) {
            CFStringGetCString(ref, opt, OPT_STR_LEN, kCFStringEncodingUTF8);
            *outlen = strlen(opt);
            return;
        }
    }

    strcpy(opt, defaultval);
    *outlen = strlen(opt);
}

// ----------------------------------------------------------------------------
//	get_cfstr_option
// ----------------------------------------------------------------------------
CFStringRef get_cfstr_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property)
{
    CFDictionaryRef	dict;
    CFStringRef		ref;
    
    dict = CFDictionaryGetValue(options, entity);
    if (isDictionary(dict)) {
        ref  = CFDictionaryGetValue(dict, property);
        if (isString(ref))
            return ref;
    }

    return NULL;
}

// ----------------------------------------------------------------------------
//	get_int_option
// ----------------------------------------------------------------------------
void get_int_option (CFPropertyListRef options, CFStringRef entity, CFStringRef property,
        u_int32_t *opt, u_int32_t defaultval)
{
    CFDictionaryRef	dict;
    CFNumberRef		ref;

    dict = CFDictionaryGetValue(options, entity);
    if (isDictionary(dict)) {
        ref  = CFDictionaryGetValue(dict, property);
        if (isNumber(ref)) {
            CFNumberGetValue(ref, kCFNumberSInt32Type, opt);
            return;
        }
    }

    *opt = defaultval;
}

// ----------------------------------------------------------------------------
//	addparam
// ----------------------------------------------------------------------------
#define OPT_STR_LEN 256

void addparam(char **arg, u_int32_t *argi, char *param)
{
    int len = strlen(param);

    if (len && (arg[*argi] = malloc(len + 1))) {
        strcpy(arg[*argi], param);
        (*argi)++;
    }
}

// ----------------------------------------------------------------------------
//	addintparam
// ----------------------------------------------------------------------------
void addintparam(char **arg, u_int32_t *argi, char *param, u_int32_t val)
{
    u_char	str[32];
    
    addparam(arg, argi, param);
    sprintf(str, "%d", val);
    addparam(arg, argi, str);
}

// ----------------------------------------------------------------------------
//	addstrparam
// ----------------------------------------------------------------------------
void addstrparam(char **arg, u_int32_t *argi, char *param, char *val)
{
    
    addparam(arg, argi, param);
    addparam(arg, argi, val);
}

