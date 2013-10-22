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
#include <net/if.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "../../Family/if_ppplink.h"
#include "../../Family/ppp_defs.h"

#include "vpnd.h"
#include "vpnoptions.h"
#include "vpnplugins.h"
#include "RASSchemaDefinitions.h"
#include "cf_utils.h"

static u_char *empty_str = (u_char*)"";

static int process_interface_prefs(struct vpn_params *params);
static int process_ipv4_prefs(struct vpn_params *params);
static int process_ipv6_prefs(struct vpn_params *params);
static int process_dns_prefs(struct vpn_params *params);
static int process_ppp_prefs(struct vpn_params *params);

//-----------------------------------------------------------------------------
//	process_options
//-----------------------------------------------------------------------------
void ppp_process_options(struct vpn_params *params)
{
    
    params->plugin_path = 0;
    params->serverSubTypeRef = 0;
    params->server_subtype = PPP_TYPE_OTHER;
}

// ----------------------------------------------------------------------------
//	process_prefs
// ----------------------------------------------------------------------------
int ppp_process_prefs(struct vpn_params *params)
{

    u_char 			pathStr[MAXPATHLEN];
    u_int32_t		len;
	int				i;
      
    // clear the argument array
    params->next_arg_index = 0;
    for (i = 0; i < MAXARG; i++)
        params->exec_args[i] = 0;
    
    // add the pppd program name to the exec args
    addparam(params->exec_args, &params->next_arg_index, PPPD_PRGM);	

    // add some general args that are always needed
    addstrparam(params->exec_args, &params->next_arg_index, "serverid", params->server_id);	// server ID
    addparam(params->exec_args, &params->next_arg_index, "nodetach");	// we don't want pppd to detach.
    addparam(params->exec_args, &params->next_arg_index, "proxyarp");  	// we proxy for the client

    // process the dictionaries
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
	len = sizeof(pathStr);
    get_str_option(params->serverRef, kRASEntPPP, kRASPropUserDefinedName, pathStr, sizeof(pathStr), &len, empty_str);
    if (pathStr[0])
        addstrparam(params->exec_args, &params->next_arg_index, "call", (char*)pathStr);

    return 0;

fail:
    if (params->serverSubTypeRef) {
        CFRelease(params->serverSubTypeRef);
        params->serverSubTypeRef = 0;
    }
    return -1;
}

//-----------------------------------------------------------------------------
//	process_interface_prefs
//-----------------------------------------------------------------------------
static int process_interface_prefs(struct vpn_params *params)
{
    CFDictionaryRef	dict;
    int path_len = 0;
	
    //  get type/subtype of server
    dict = CFDictionaryGetValue(params->serverRef, kRASEntInterface);

	// get server subtype and check if supported
	params->serverSubTypeRef = CFDictionaryGetValue(dict, kRASPropInterfaceSubType);
	if (!isString(params->serverSubTypeRef)) {
		vpnlog(LOG_ERR, "Incorrect server subtype found\n");
		params->serverSubTypeRef = NULL;
		return -1;
	}
	path_len = CFStringGetLength(params->serverSubTypeRef) + 5;
	params->plugin_path = malloc(path_len); 
	CFStringGetCString(params->serverSubTypeRef, params->plugin_path, 
						path_len, kCFStringEncodingUTF8);
	strlcat(params->plugin_path, ".ppp", path_len);
	if (!plugin_exists(params->plugin_path)) {
		vpnlog(LOG_ERR, "Unsupported plugin '%s'\n", params->plugin_path);
		params->serverSubTypeRef = NULL;
		free(params->plugin_path);
		params->plugin_path = NULL;
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
		
	CFRetain(params->serverSubTypeRef);

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
    uint32_t 		i, nb, len;
    char		ipcstr[100], ipcstr2[100], ip_addr[100], ip_addr2[100];
    char		*ip, *ip2;

    // Check if the IPv4 dictionary is present
    if ((dict = CFDictionaryGetValue(params->serverRef, kRASEntIPv4)) && isDictionary(dict)) {
    
        // get server side address
		len = sizeof(str);
        get_array_option(params->serverRef, kRASEntIPv4, kRASPropIPv4Addresses, 0, (u_char*)str, sizeof(str), &len, empty_str);
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
            strlcat(str, ":", sizeof(str));
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
                            if ((ip = validate_ip_string(ipcstr, ip_addr, sizeof(ip_addr)))) {
                                if (add_address(ip)) {
                                    vpnlog(LOG_ERR, "Error while processing ip address %s\n", ip);
                                    return -1;
                                }
                            } else
                                vpnlog(LOG_ERR, "Ignoring invalid ip address %s\n", ipcstr);
                        }
                    }
                }
            }
            // ip address ranges
            array  = CFDictionaryGetValue(dict, kRASPropIPv4DestAddressRanges);
            if (isArray(array)) {
                if (CFArrayGetCount(array) % 2)
                    vpnlog(LOG_ERR, "Error - ip address ranges must be in pairs\n");
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
                                        vpnlog(LOG_ERR, "Error while processing ip address range %s\n", ip);
                                        return -1;
                                    }
                                } else
                                    vpnlog(LOG_ERR, "Ignoring invalid ip address range %s\n", ipcstr);
                            }
                        }
                    }
                }
            }
        }
    }
    
    if (!address_avail()) {
        vpnlog(LOG_ERR, "No valid client IP addresses\n");
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
#define AUTH_BITS_PAP		0x1
#define AUTH_BITS_CHAP		0x2
#define AUTH_BITS_MSCHAP1	0x4
#define AUTH_BITS_MSCHAP2	0x8
#define AUTH_BITS_EAP		0x10
    u_int32_t			auth_bits = 0; /* none */

    //
    // some basic admin options 
    //
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPVerboseLogging, &lval, 0);
    if (lval)
        addparam(params->exec_args, &params->next_arg_index, "debug");

	len = sizeof(optStr);
    get_str_option(params->serverRef, kRASEntPPP, kRASPropPPPLogfile, (u_char*)optStr, sizeof(optStr), &len, empty_str);
    if (optStr[0]) {
        // if logfile start with /, it's a full path
        // otherwise it's relative to the logs folder (convention)
        // we also strongly advise to name the file with the link number
        // for example ppplog0
        // the default path is /var/log
        // it's useful to have the debug option with the logfile option
        // with debug option, pppd will log the negociation
        // debug option is different from kernel debug trace

        snprintf(pathStr, sizeof(pathStr), "%s%s", optStr[0] == '/' ? "" : DIR_LOGS, optStr);
        addstrparam(params->exec_args, &params->next_arg_index, "logfile", pathStr);
    }
                    
    get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPDisconnectOnIdle, &lval, 0);
    if (lval) {
        get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPDisconnectOnIdleTimer, &lval, OPT_COMM_IDLETIMER_DEF);
        if (lval) {
            addintparam(params->exec_args, &params->next_arg_index, "idle", lval);
            addparam(params->exec_args, &params->next_arg_index, "noidlesend");
        }
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
                snprintf(optStr, sizeof(optStr), "%d,", lval1);
                strlcat(pathStr, optStr, sizeof(pathStr));
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
		            
        /* enforce the source ip address filtering */
        addintparam(params->exec_args, &params->next_arg_index, "ip-src-address-filter",  NPAFMODE_SRC_IN);

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
        //&& !ppp_getoptval(ppp, opts, PPP_OPT_AUTH_PROTO, &lval, sizeof(lval), &len, service) 
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
                        vpnlog(LOG_ERR, "Invalid compression type specified - ignored\n");
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

	get_int_option(params->serverRef, kRASEntPPP, kRASPropPPPInterceptDHCP, &lval, 1);
	if (lval)
		addparam(params->exec_args, &params->next_arg_index, "intercept-dhcp");

    
    //
    // Authentication
    //
    
    auth_bits = 0;
	dict = CFDictionaryGetValue(params->serverRef, kRASEntPPP);
    if (isDictionary(dict)) {
        array = CFDictionaryGetValue(dict, kRASPropPPPAuthenticatorProtocol);
        if (isArray(array)) {
            count = CFArrayGetCount(array);
			
			for (i = 0; i < count; i++) {
				string = CFArrayGetValueAtIndex(array, i);
				if (isString(string)) {
					if (CFStringCompare(string, kRASValPPPAuthProtocolMSCHAP2, 0) == kCFCompareEqualTo)
						auth_bits |= AUTH_BITS_MSCHAP2;
					else if (CFStringCompare(string, kRASValPPPAuthProtocolMSCHAP1, 0) == kCFCompareEqualTo)
						auth_bits |= AUTH_BITS_MSCHAP1;
					else if (CFStringCompare(string, kRASValPPPAuthProtocolCHAP, 0) == kCFCompareEqualTo)
						auth_bits |= AUTH_BITS_CHAP;
					else if (CFStringCompare(string, kRASValPPPAuthProtocolPAP, 0) == kCFCompareEqualTo)
						auth_bits |= AUTH_BITS_PAP;
					else if (CFStringCompare(string, kRASValPPPAuthProtocolEAP, 0) == kCFCompareEqualTo)
						auth_bits |= AUTH_BITS_EAP;
					else {
						vpnlog(LOG_ERR, "Unknown authentication type specified\n");
						return -1;
					}
				}
            }
        }
	}
	
	if (auth_bits & AUTH_BITS_PAP)
		addparam(params->exec_args, &params->next_arg_index, "require-pap");

	if (auth_bits & AUTH_BITS_CHAP)
		addparam(params->exec_args, &params->next_arg_index, "require-chap");

	if (auth_bits & AUTH_BITS_MSCHAP1)
		addparam(params->exec_args, &params->next_arg_index, "require-mschap");

	if (auth_bits & AUTH_BITS_MSCHAP2) 
		addparam(params->exec_args, &params->next_arg_index, "require-mschap-v2");

	if (auth_bits & AUTH_BITS_EAP) {

		addparam(params->exec_args, &params->next_arg_index, "require-eap");  
		
		// add EAP plugins - must be at least one
		int eapPluginFound = 0;
		
		i = 0;
		do {
			lval = get_array_option(params->serverRef, kRASEntPPP, kRASPropPPPAuthenticatorEAPPlugins, 
									i++, (u_char*)pathStr, sizeof(pathStr), &len, empty_str);
			if (pathStr[0]) {
				strlcat(pathStr, ".ppp", sizeof(pathStr));	// add plugin suffix
				if (!plugin_exists(pathStr)) {
					vpnlog(LOG_ERR, "EAP plugin '%s' not found\n", pathStr);
					return -1;
				}
				addstrparam(params->exec_args, &params->next_arg_index, "eapplugin", pathStr);
				eapPluginFound = 1;
			}
		}
		while (lval);

		if (!eapPluginFound) {
			 /* should check if Radius EAP proxy is enabled */
			//vpnlog(LOG_ERR, "No EAP authentication plugin(s) specified\n");
			//return -1;
		}
    }


    //
    // Plugins
    //
    
    // add authentication plugins
    i = 0;
    do {
		len = sizeof(pathStr);
       lval = get_array_option(params->serverRef, kRASEntPPP, kRASPropPPPAuthenticatorPlugins, 
							   i++, (u_char*)pathStr, sizeof(pathStr), &len, empty_str);
        if (pathStr[0]) {
            strlcat(pathStr, ".ppp", sizeof(pathStr));	// add plugin suffix
            if (!plugin_exists(pathStr)) {
                vpnlog(LOG_ERR, "Authentication plugin '%s' not found\n", pathStr);
                return -1;
            }
            addstrparam(params->exec_args, &params->next_arg_index, "plugin", pathStr);
        }
    }
    while (lval);

    // add access control list plugins
    i = 0;
    do {
 		len = sizeof(pathStr);
       lval = get_array_option(params->serverRef, kRASEntPPP, kRASPropPPPAuthenticatorACLPlugins, 
							   i++, (u_char*)pathStr, sizeof(pathStr), &len, empty_str);
        if (pathStr[0]) {
            strlcat(pathStr, ".ppp", sizeof(pathStr));	// add plugin suffix
            if (!plugin_exists(pathStr)) {
                vpnlog(LOG_ERR, "Access Control plugin '%s' not found\n", pathStr);
                return -1;
            }
            addstrparam(params->exec_args, &params->next_arg_index, "plugin2", pathStr);
        }
    }
    while (lval);
    
    // add any additional plugin we want to load
    i = 0;
    do {
		len = sizeof(pathStr);
        lval = get_array_option(params->serverRef, kRASEntPPP, kRASPropPPPPlugins, 
								i++, (u_char*)pathStr, sizeof(pathStr), &len, empty_str);
        if (pathStr[0]) {
            strlcat(pathStr, ".ppp", sizeof(pathStr));	// add plugin suffix
            if (!plugin_exists(pathStr)) {
                vpnlog(LOG_ERR, "Plugin '%s' not found\n", pathStr);
                return -1;
            }
            addstrparam(params->exec_args, &params->next_arg_index, "plugin", pathStr);
        }
    }
    while (lval);
    
    return 0;
}

//-----------------------------------------------------------------------------
//	check_conflicts
//-----------------------------------------------------------------------------
int ppp_check_conflicts(struct vpn_params *params)
{
    
    CFArrayRef			array;
    CFStringRef			pattern, key;
    CFStringRef			type, subtype;
    CFPropertyListRef		ref;
    int				count, i, ret = 0;
    char			str[OPT_STR_LEN];

    pattern = CFStringCreateWithFormat(0, 0, CFSTR("%@/%@/%@/%s/%@"), kSCDynamicStoreDomainState, 
                    kSCCompNetwork, kRASRemoteAccessServer, ".*", kRASEntInterface);

    if (pattern) {
        if ((array = SCDynamicStoreCopyKeyList(params->storeRef, pattern))) {
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
                            vpnlog(LOG_ERR, "Server for subtype %s already running - vpnd launch failed.\n", str);
                            ret = -1;
                            CFRelease(ref);
                            break;
                        }
                    }
                    if (ref)
                        CFRelease(ref);
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
int ppp_kill_orphans(struct vpn_params* params)
{
    
    CFArrayRef			array;
    CFStringRef			pattern, key;
    CFMutableStringRef		mutable_key;
    CFStringRef			server_id;
    CFNumberRef			pidRef;
    CFPropertyListRef		interface_dict, ppp_dict;
    int				count, i, pid;
    int				ret = 0;
    
    pattern = CFStringCreateWithFormat(0, 0, CFSTR("%@/%@/%@/%s/%@"), kSCDynamicStoreDomainState, 
                    kSCCompNetwork, kSCCompService, ".*", kSCEntNetInterface);
    
    if (pattern) {
        if ((array = SCDynamicStoreCopyKeyList(params->storeRef, pattern))) {
            count = CFArrayGetCount(array);
            // for each pppd - check if server id is the same as ours
            for (i = 0; i < count; i++) {
                key = CFArrayGetValueAtIndex(array, i);
                if (key) {
                    interface_dict = SCDynamicStoreCopyValue(params->storeRef, key);
                    if (isDictionary(interface_dict)) {
                        server_id = CFDictionaryGetValue(interface_dict, CFSTR("ServerID"));
                        if (isString(server_id) && CFStringCompare(server_id, params->serverIDRef, 0) == kCFCompareEqualTo) {
                            // server id matches - get the pid and kill it
                            mutable_key = CFStringCreateMutableCopy(0, 0, key);
                            // modify the key to get the PPP dictionary
                            if (CFStringFindAndReplace(mutable_key, kSCEntNetInterface, kSCEntNetPPP, 
                                    CFRangeMake(0, CFStringGetLength(mutable_key)), kCFCompareBackwards | kCFCompareAnchored)) {
                                ppp_dict = SCDynamicStoreCopyValue(params->storeRef, mutable_key);
                                if (isDictionary(ppp_dict)) {
                                    pidRef = CFDictionaryGetValue(ppp_dict, CFSTR("pid"));
                                    if (isNumber(pidRef))
                                        if (CFNumberGetValue(pidRef, kCFNumberIntType, &pid))
                                            ret = kill(pid, SIGTERM);
                                }
                                if (ppp_dict)
                                    CFRelease(ppp_dict);
                            }
                            if (mutable_key)
                                CFRelease(&mutable_key);
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
