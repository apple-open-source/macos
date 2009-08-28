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

#include "vpnd.h"
#include "vpnoptions.h"
#include "cf_utils.h"
#include "vpnplugins.h"
#include "pppoptions.h"
#include "ipsecoptions.h"
#include "ipsec_utils.h"
#include "RASSchemaDefinitions.h"

static char*	default_log_path = "/var/log/ppp/" DAEMON_NAME ".log";


static void usage(FILE *fp, const char *argv0);
static int process_server_prefs(struct vpn_params *params);
static int process_interface_prefs(struct vpn_params *params);


//-----------------------------------------------------------------------------
//	process_options
//-----------------------------------------------------------------------------
int process_options(struct vpn_params *params, int argc, char *argv[])
{
    
    char* 		argv0 = argv[0];
    char* 		args = "dhnxi:";
    char		c;
        
    /* initialize generic portion */
	params->max_sessions = 0;
    params->debug = 0;
	params->log_verbose = 0;
    params->daemonize = 1;
    params->serverIDRef = 0;
    params->serverRef = 0;
    params->server_id = 0;
    params->server_type = -1;
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
                break;

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
                    
	/* init ppp portion */
	ppp_process_options(params);
	
	/* init ipsec portion */
	ipsec_process_options(params);
	
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
    prefs = SCPreferencesCreate(0, CFSTR("vpnd"), kRASServerPrefsFileName);
    if (prefs == NULL) {
        CFStringGetCString(kRASServerPrefsFileName, pathStr, MAXPATHLEN, kCFStringEncodingMacRoman);
        vpnlog(LOG_ERR, "Unable to read vpnd prefs file '%s'\n", pathStr);
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
    SCPreferencesRef 		prefs = 0;
    CFPropertyListRef		servers_list;
    
    char 			text[512] = "";
  
    // open the prefs file
    prefs = SCPreferencesCreate(0, CFSTR("vpnd"), kRASServerPrefsFileName);
    if (prefs == NULL) {
        CFStringGetCString(kRASServerPrefsFileName, pathStr, MAXPATHLEN, kCFStringEncodingMacRoman);
        snprintf(text, sizeof(text), "Unable to read vpnd prefs file '%s'\n", pathStr);
        goto fail;
    }
    // get servers list from the plist
    servers_list = SCPreferencesGetValue(prefs, kRASServers);
    if (servers_list == NULL) {
        snprintf(text, sizeof(text), "Could not get servers dictionary\n");
        goto fail;
    }
    // retrieve the information for the given Server ID
    params->serverIDRef = CFStringCreateWithCString(0, params->server_id, kCFStringEncodingMacRoman);
    if (params->serverIDRef == NULL) {
        snprintf(text, sizeof(text), "Could not create CFString for server ID\n");
        goto fail;
    }
    params->serverRef = CFDictionaryGetValue(servers_list, params->serverIDRef);
    if (params->serverRef == NULL || isDictionary(params->serverRef) == 0) {
        snprintf(text, sizeof(text), "Server ID '%.64s' invalid\n", params->server_id);
        params->serverRef = 0;
        goto fail;
    }
    CFRetain(params->serverRef);
    CFRelease(prefs);
    prefs = 0;    
    
    // process the dictionaries
    if (process_server_prefs(params))
        goto fail;
    if (process_interface_prefs(params))
        goto fail;
	
	switch (params->server_type) {
		case SERVER_TYPE_PPP:
			if (ppp_process_prefs(params)) {
				snprintf(text, sizeof(text), "Error while reading PPP preferences\n");
				goto fail;
			}
			break;
		case SERVER_TYPE_IPSEC:
			if (ipsec_process_prefs(params)) {
				snprintf(text, sizeof(text), "Error while reading IPSec preferences\n");
				goto fail;
			}
			break;
	}

    return 0;

fail:
    vpnlog(LOG_ERR, text[0] ? text : "Error while reading preferences\n");
    if (params->serverIDRef) {
        CFRelease(params->serverIDRef);
        params->serverIDRef = 0;
    }
    if (params->serverRef) {
        CFRelease(params->serverRef);
        params->serverRef = 0;
    }
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
    char            str[MAXPATHLEN];   
	int				err ;
	struct hostent	*hostent;
	
    get_int_option(params->serverRef, kRASEntServer, kRASPropServerMaximumSessions, &lval, 0);
    if (lval)
        params->max_sessions = lval;
	len = sizeof(str);
    get_str_option(params->serverRef, kRASEntServer, kRASPropServerLogfile, str, &len, default_log_path);
    if (str[0])
        memcpy(params->log_path, str, len + 1);

    get_int_option(params->serverRef, kRASEntServer, kRASPropServerVerboseLogging, &lval, 0);
    if (lval)
        params->log_verbose = lval;

	// Load balancing parameters
	get_int_option(params->serverRef, kRASEntServer, kRASPropServerLoadBalancingEnabled, &lval, 0);
	if (lval) {
		params->lb_enable = 1;
		
		// will determine the interface from the cluster address
		//len = sizeof(str);
		//get_str_option(params->serverRef, kRASEntServer, kRASPropServerLoadBalancingInterface, str, &len, "en1");
		//strncpy(params->lb_interface, str, sizeof(params->lb_interface));

		// is priority really useful ?
		//get_int_option(params->serverRef, kRASEntServer, kRASPropServerLoadBalancingPriority, &lval, 5);
		//if (lval < 1) lval = 1;
		//else if (lval > LB_MAX_PRIORITY) lval = LB_MAX_PRIORITY;
		//params->lb_priority = lval;
		
		get_int_option(params->serverRef, kRASEntServer, kRASPropServerLoadBalancingPort, &lval, LB_DEFAULT_PORT);
		params->lb_port = htons(lval);
		len = sizeof(str);
		get_str_option(params->serverRef, kRASEntServer, kRASPropServerLoadBalancingAddress, str, &len, "");
		// ask the system to look up the given name.
		hostent = getipnodebyname (str, AF_INET, 0, &err);
		if (!hostent) {
			vpnlog(LOG_ERR, "Incorrect Load Balancing address found '%s'\n", str);
			params->lb_enable = 0;
			
		}
		else {
			struct sockaddr_in src, dst;
			
			params->lb_cluster_address = *(struct in_addr *)hostent->h_addr_list[0];
			freehostent(hostent);
			
			bzero(&dst, sizeof(dst));
			dst.sin_family = PF_INET;
			dst.sin_len = sizeof(dst);
			dst.sin_addr = params->lb_cluster_address;
		
			// look for the interface and primary address of the cluster address			
			if (get_route_interface((struct sockaddr *)&src, (struct sockaddr *)&dst, params->lb_interface)) {
			
				vpnlog(LOG_ERR, "Cannot get load balancing redirect address and interface (errno = %d)\n", errno);
				params->lb_enable = 0;
			}

			params->lb_redirect_address = src.sin_addr;

			
		}
	}
	
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
    if (!isDictionary(dict)) {
		vpnlog(LOG_ERR, "No Interface dictionary found\n");
		return -1;
	}
		
	str  = CFDictionaryGetValue(dict, kRASPropInterfaceType);		
	if (!isString(str)) {
		vpnlog(LOG_ERR, "No Interface type found\n");
		return -1;
	}
		
	if (CFStringCompare(str, kRASValInterfaceTypePPP, 0) == kCFCompareEqualTo)
		params->server_type = SERVER_TYPE_PPP;
	else if (CFStringCompare(str, kRASValInterfaceTypeIPSec, 0) == kCFCompareEqualTo)
		params->server_type = SERVER_TYPE_IPSEC;
	else {
		vpnlog(LOG_ERR, "Incorrect server type found\n");
		return -1;
	}
	
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
			switch (params->server_type) {
				case SERVER_TYPE_PPP:
					CFDictionarySetValue(dict, kRASPropInterfaceType,  kRASValInterfaceTypePPP);
					CFDictionarySetValue(dict, kRASPropInterfaceSubType,  params->serverSubTypeRef);
					break;
				case SERVER_TYPE_IPSEC:
					CFDictionarySetValue(dict, kRASPropInterfaceType,  kRASValInterfaceTypeIPSec);
					break;
			}
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
//	close_dynamic_store
//-----------------------------------------------------------------------------
void close_dynamic_store(struct vpn_params* params)
{
    if (params->storeRef) {
		CFRelease(params->storeRef);
		params->storeRef = NULL;
	}
		
}

//-----------------------------------------------------------------------------
//	check_conflicts
//-----------------------------------------------------------------------------
int check_conflicts(struct vpn_params *params)
{
    
    if (params->storeRef == 0)
        return 0;

	switch (params->server_type) {
		case SERVER_TYPE_PPP:
			return ppp_check_conflicts(params);

		case SERVER_TYPE_IPSEC:
			return 0;
	}

    return 0;
}

//-----------------------------------------------------------------------------
//	kill_orphans
//-----------------------------------------------------------------------------
int kill_orphans(struct vpn_params* params)
{
    
    if (params->storeRef == 0)
        return 0;

	switch (params->server_type) {
		case SERVER_TYPE_PPP:
			return ppp_kill_orphans(params);

		case SERVER_TYPE_IPSEC:
			return 0;
	}

    return 0;
}

//-----------------------------------------------------------------------------
//	add_builtin_plugin for non plugin based connection
//-----------------------------------------------------------------------------
int add_builtin_plugin(struct vpn_params* params, void *channel)
{
    
	switch (params->server_type) {
		case SERVER_TYPE_PPP:
			/* ppp connection are plugin based */
			return -1;

		case SERVER_TYPE_IPSEC:
			return ipsec_add_builtin_plugin(params, channel);
	}

    return -1;
}

// ----------------------------------------------------------------------------
//	usage
// ----------------------------------------------------------------------------
static void usage(FILE *fp, const char *argv0)
{
	static const char* 	szpUsage =
		"Usage:\t%s [-dhnx] [-i serverID]\n"
		"	-h		this message\n"
		"	-x		does not move to background\n"
		"	-d		enable debug mode\n"
		"	-n		same as -d but terminates after validation\n"
		"	-i		server ID for this server (ex: com.apple.ppp.l2tp)\n"
		;
    fprintf (fp, szpUsage, argv0);
}

//-----------------------------------------------------------------------------
//	plugin_exists - checks to see if the given plugin exists.
//-----------------------------------------------------------------------------
int plugin_exists(const char *inPath)
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
char* validate_ip_string(const char *inIPString, char *outIPString, size_t outSize)
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
    outIP = (char*)inet_ntop(AF_INET, hesp->h_addr_list[0], outIPString, outSize);
    freehostent (hesp);
    return outIP;
}

// ----------------------------------------------------------------------------
//	addparam
// ----------------------------------------------------------------------------

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
    char	str[32];
    
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

