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

/*
 * February 2000 - created.
 */

/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */
#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>

#include "ppp_msg.h"
#include "ppp_privmsg.h"

#include "../Family/ppp_domain.h"
#include "scnc_main.h"
#include "scnc_client.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "scnc_utils.h"
#include "scnc_main.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

#ifndef MIN
#define MIN(a, b)	((a) < (b)? (a): (b))
#endif
#ifndef MAX
#define MAX(a, b)	((a) > (b)? (a): (b))
#endif

static u_char *empty_str = (u_char *)"";

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long get_addr_option (struct service *serv, CFStringRef entity, CFStringRef property, 
        CFDictionaryRef options, CFDictionaryRef setup, u_int32_t *opt, u_int32_t defaultval)
{
    CFDictionaryRef	dict;
    CFArrayRef		array;

    // first search in the state
    if (serv->u.ppp.phase != PPP_IDLE
        && getAddressFromEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, 
                entity, property, opt)) {
        return 1;
    }
    // then, search in the option set
    if (options
        && (dict = CFDictionaryGetValue(options, entity))
        && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
        
        if ((array = CFDictionaryGetValue(dict, property))
            && (CFGetTypeID(array) == CFArrayGetTypeID())
            && CFArrayGetCount(array)) {
            *opt = CFStringAddrToLong(CFArrayGetValueAtIndex(array, 0));
            return 2;
        }
    }
    
    // at last, search in the setup 
    if (getAddressFromEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, 
        entity, property, opt)) {
        return 3;
    }

    *opt = defaultval;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long get_int_option (struct service *serv, CFStringRef entity, CFStringRef property,
        CFDictionaryRef options, CFDictionaryRef setup, u_int32_t *opt, u_int32_t defaultval)
{
    CFDictionaryRef	dict;

    // first search in the state
    if (serv->u.ppp.phase != PPP_IDLE
        && getNumberFromEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, 
                entity, property, opt)) {
        return 1;
    }

    // then, search in the option set
    if (options
        && (dict = CFDictionaryGetValue(options, entity))
        && (CFGetTypeID(dict) == CFDictionaryGetTypeID())
        && getNumber(dict, property, opt)) {
        return 2;
    }

    // at last, search in the setup
    if ((setup 
        && (dict = CFDictionaryGetValue(setup, entity))
        && (CFGetTypeID(dict) == CFDictionaryGetTypeID())
        && getNumber(dict, property, opt))
        || (!setup && getNumberFromEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, 
        entity, property, opt))) {
        return 3;
    }

    *opt = defaultval;
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int get_str_option (struct service *serv, CFStringRef entity, CFStringRef property,
        CFDictionaryRef options, CFDictionaryRef setup, u_char *opt, u_int32_t optsiz, u_int32_t *outlen, u_char *defaultval)
{
    CFDictionaryRef	dict;
    
    // first search in the state
    if (serv->u.ppp.phase != PPP_IDLE
    	&& getStringFromEntity(gDynamicStore, kSCDynamicStoreDomainState, serv->serviceID, 
                entity, property, opt, optsiz)) {
        *outlen = strlen((char*)opt);
        return 1;
    }

    // then, search in the option set
    if (options
        && (dict = CFDictionaryGetValue(options, entity))
    	&& (CFGetTypeID(dict) == CFDictionaryGetTypeID())
        && getString(dict, property, opt, optsiz)) {
        *outlen = strlen((char*)opt);
        return 2;
    }
    // at last, search in the setup, only in lookinsetup flag is set
    if ((setup 
        && (dict = CFDictionaryGetValue(setup, entity))
        && (CFGetTypeID(dict) == CFDictionaryGetTypeID()) 
        && getString(dict, property, opt, optsiz))
        || (!setup && getStringFromEntity(gDynamicStore, kSCDynamicStoreDomainSetup, serv->serviceID, 
        entity, property, opt, optsiz))) {
        *outlen = strlen((char*)opt);
        return 3;
    }

    strlcpy((char*)opt, (char*)defaultval, optsiz);
    *outlen = strlen((char*)opt);
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFTypeRef get_cf_option (CFStringRef entity, CFStringRef property, CFTypeID type, 
        CFDictionaryRef options, CFDictionaryRef setup, CFTypeRef defaultval)
{
    CFDictionaryRef	dict;
    CFTypeRef		ref;
    
    // first, search in the option set
    if (options
        && (dict = CFDictionaryGetValue(options, entity))
    	&& (CFGetTypeID(dict) == CFDictionaryGetTypeID())
        && (ref = CFDictionaryGetValue(dict, property))
    	&& (CFGetTypeID(ref) == type)) {
        return ref;
    }
    
    // then, search in the setup
    if (setup 
        && (dict = CFDictionaryGetValue(setup, entity))
        && (CFGetTypeID(dict) == CFDictionaryGetTypeID()) 
        && (ref = CFDictionaryGetValue(dict, property))
    	&& (CFGetTypeID(ref) == type)) {
        return ref;
    }

    return defaultval;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_getoptval(struct service *serv, CFDictionaryRef opts, CFDictionaryRef setup, u_int32_t otype, void *pdata, u_int32_t pdatasiz, u_int32_t *plen)
{
    u_int32_t 			lval, lval1, lval2;
    u_int32_t			*lopt = (u_int32_t *)pdata;
    u_char 			*popt = (u_char *)pdata;
    char 			str[OPT_STR_LEN], str2[OPT_STR_LEN];

    *plen = 4; // init the len for long options
    *lopt = 0; // init to zero
   
    switch (otype) {

        // DEVICE options
        case PPP_OPT_DEV_NAME:
            if (serv->subtype == PPP_TYPE_SERIAL) {
                if (setup) {
                    CFDictionaryRef         dict;

                    if ((dict = CFDictionaryGetValue(setup, kSCEntNetInterface))
                        && (CFGetTypeID(dict) == CFDictionaryGetTypeID())) {
                        SCNetworkInterfaceRef   interface;

                        if ((interface = _SCNetworkInterfaceCreateWithEntity(NULL, dict, NULL))) {
                            CFStringRef path;
                                
                            path = _SCNetworkInterfaceCopySlashDevPath(interface);
                            CFRelease(interface);
                            if (path) {

                                CFStringGetCString(path, (char *)popt, OPT_STR_LEN, kCFStringEncodingUTF8);
                                CFRelease(path);
                                *plen = strlen((const char *)popt);
                                break;
                            }
                        }
                    }
                }
            }
            get_str_option(serv, kSCEntNetInterface, kSCPropNetInterfaceDeviceName, opts, setup, popt, pdatasiz, plen,
                           (serv->subtype == PPP_TYPE_SERIAL) ? (u_char*)OPT_DEV_NAME_DEF :
                           ((serv->subtype == PPP_TYPE_PPPoE) ? (u_char*)OPT_DEV_NAME_PPPoE_DEF : empty_str));
            break;
        case PPP_OPT_DEV_SPEED:
            *lopt = 0; 
            switch (serv->subtype) {
                case PPP_TYPE_SERIAL:
                    get_int_option(serv, kSCEntNetModem, kSCPropNetModemSpeed, opts, setup, lopt, OPT_DEV_SPEED_DEF);
                    break;
                case PPP_TYPE_PPPoE:
                case PPP_TYPE_PPTP:
                case PPP_TYPE_L2TP:
                    break;
            }
            break;
        case PPP_OPT_DEV_CONNECTSCRIPT:
            get_str_option(serv, kSCEntNetModem, kSCPropNetModemConnectionScript, opts, setup, popt, pdatasiz, plen, 
                (serv->subtype == PPP_TYPE_SERIAL) ? (u_char*)OPT_DEV_CONNECTSCRIPT_DEF : empty_str);
            break;
 
        case PPP_OPT_DEV_DIALMODE:
            *plen = 4;
            *lopt = PPP_DEV_WAITFORDIALTONE;
            str[0] = 0;
            lval = sizeof(str);
            get_str_option(serv, kSCEntNetModem, kSCPropNetModemDialMode, opts, setup, (u_char *)str, sizeof(str), &lval, empty_str);
            str2[0] = 0;
            CFStringGetCString(kSCValNetModemDialModeIgnoreDialTone, str2, sizeof(str2), kCFStringEncodingUTF8);
            if (!strcmp(str, str2))
                *lopt = PPP_DEV_IGNOREDIALTONE;
            else {
                str2[0] = 0;
                CFStringGetCString(kSCValNetModemDialModeManual, str2, sizeof(str2), kCFStringEncodingUTF8);
                if (!strcmp(str, str2))
                    *lopt = PPP_DEV_MANUALDIAL;
            }
            break;

        // COMM options
       case PPP_OPT_DEV_CONNECTSPEED:
            switch (serv->subtype) {
                case PPP_TYPE_SERIAL:
                    get_int_option(serv, kSCEntNetModem, kSCPropNetModemConnectSpeed, opts, setup, lopt, 0);
                    break;
                case PPP_TYPE_PPPoE:
                case PPP_TYPE_PPTP:
                case PPP_TYPE_L2TP:
                    break;
            }
            break;
        case PPP_OPT_COMM_TERMINALMODE:
            *lopt = OPT_COMM_TERMINALMODE_DEF; 
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPCommDisplayTerminalWindow, opts, setup, &lval, 0);
            if (lval) 
                *lopt = PPP_COMM_TERM_WINDOW;
            else {
                get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPCommUseTerminalScript, opts, setup, &lval, 0);
                if (lval) 
                    *lopt = PPP_COMM_TERM_SCRIPT;
            }
	    break;	    
        case PPP_OPT_COMM_TERMINALSCRIPT:
            get_str_option(serv, kSCEntNetPPP, kSCPropNetPPPCommTerminalScript, opts, setup, popt, pdatasiz, plen, empty_str);
            break;
        case PPP_OPT_COMM_IDLETIMER:
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPDisconnectOnIdle, opts, setup, &lval, 0);
            if (lval)
                get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPDisconnectOnIdleTimer, opts, setup, lopt, OPT_COMM_IDLETIMER_DEF);
            break;
        case PPP_OPT_COMM_SESSIONTIMER:
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPUseSessionTimer, opts, setup, &lval, 0);
            if (lval)
                get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPSessionTimer, opts, setup, lopt, OPT_COMM_SESSIONTIMER_DEF);
            break;
        case PPP_OPT_COMM_REMINDERTIMER:
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPIdleReminder,opts, setup,  &lval, 0);
            if (lval)
                get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPIdleReminderTimer, opts, setup, lopt, OPT_COMM_REMINDERTIMER_DEF);
            break;
        case PPP_OPT_COMM_REMOTEADDR:
            get_str_option(serv, kSCEntNetPPP, kSCPropNetPPPCommRemoteAddress, opts, setup, popt, pdatasiz, plen, empty_str);
            break;
        case PPP_OPT_COMM_CONNECTDELAY:
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPCommConnectDelay, opts, setup, lopt, OPT_COMM_CONNECTDELAY_DEF);
            break;
            
        // LCP options
        case PPP_OPT_LCP_HDRCOMP:
			get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPCompressionPField, opts, setup, &lval, serv->subtype == PPP_TYPE_PPPoE ? 0 : OPT_LCP_PCOMP_DEF);
            if (serv->subtype == PPP_TYPE_PPPoE)
				lval1 = 0; // not applicable
			else
				get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPCompressionACField, opts, setup, &lval1, OPT_LCP_ACCOMP_DEF);
            *lopt = lval + (lval1 << 1); 
            break;
        case PPP_OPT_LCP_MRU:
            switch (serv->subtype) {
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
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPMRU, opts, setup, lopt, lval);
            break;
        case PPP_OPT_LCP_MTU:
            switch (serv->subtype) {
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
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPMTU, opts, setup, lopt, lval);
            break;
        case PPP_OPT_LCP_RCACCM:
            if (serv->subtype == PPP_TYPE_PPPoE) {
				// not applicable
				*plen = 0;
				return 0;
			}
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPReceiveACCM, opts, setup, lopt, OPT_LCP_RCACCM_DEF);
            break;
        case PPP_OPT_LCP_TXACCM:
            if (serv->subtype == PPP_TYPE_PPPoE) {
				// not applicable
				*plen = 0;
				return 0;
			}
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPTransmitACCM, opts, setup, lopt, OPT_LCP_TXACCM_DEF);
            break;
        case PPP_OPT_LCP_ECHO:
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPEchoEnabled, opts, setup, &lval, 0);
            if (lval) {
                get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPEchoInterval, opts, setup, &lval1, OPT_LCP_ECHOINTERVAL_DEF);
                get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPLCPEchoFailure, opts, setup, &lval2, OPT_LCP_ECHOFAILURE_DEF);
                *lopt = (lval1 << 16) + lval2; 
            }
            break;

        // AUTH options
        case PPP_OPT_AUTH_PROTO:
            *lopt = OPT_AUTH_PROTO_DEF;
            // XXX To be fixed
            break;
        case PPP_OPT_AUTH_NAME:
             get_str_option(serv, kSCEntNetPPP, kSCPropNetPPPAuthName, opts, setup, popt, pdatasiz, plen, empty_str);
            break;
        case PPP_OPT_AUTH_PASSWD:
            get_str_option(serv, kSCEntNetPPP, kSCPropNetPPPAuthPassword, opts, setup, popt, pdatasiz, plen, empty_str);
            // don't return the actual pasword.
            // instead, return len = 1 if password is known, len = 0 if password is unknown
            if (*plen) {
                popt[0] = '*';
                *plen = 1;
            }
            break;

            // IPCP options
        case PPP_OPT_IPCP_HDRCOMP:
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPIPCPCompressionVJ, opts, setup, lopt, serv->subtype == PPP_TYPE_PPPoE ? 0 : OPT_IPCP_HDRCOMP_DEF);
            break;
        case PPP_OPT_IPCP_LOCALADDR:
             get_addr_option(serv, kSCEntNetIPv4, kSCPropNetIPv4Addresses, opts, setup, lopt, 0);
           break;
        case PPP_OPT_IPCP_REMOTEADDR:
            get_addr_option(serv, kSCEntNetIPv4, kSCPropNetIPv4DestAddresses, opts, setup, lopt, 0);
            break;

           // MISC options
        case PPP_OPT_LOGFILE:
            // Note: this option is not taken from the user options
             get_str_option(serv, kSCEntNetPPP, kSCPropNetPPPLogfile, 0 /* opts */, setup, popt, pdatasiz, plen, empty_str);
             if (popt[0] && popt[0] != '/') {
                lval = strlen(DIR_LOGS);
                strncpy((char*)(popt + lval), (char*)popt, *plen);
                strncpy((char*)popt, DIR_LOGS, lval);
                *plen += lval;
             }
            break;
        case PPP_OPT_ALERTENABLE:
            get_int_option(serv, kSCEntNetPPP, CFSTR("AlertEnable"), opts, setup, lopt, 0xFFFFFFFF);
            break;
        case PPP_OPT_DIALONDEMAND:
        case PPP_OPT_AUTOCONNECT_DEPRECATED:
            get_int_option(serv, kSCEntNetPPP, kSCPropNetPPPDialOnDemand, opts, setup, lopt, 0);
            break;
        case PPP_OPT_SERVICEID:
            popt[0] = 0;
            CFStringGetCString(serv->serviceID, (char*)popt, 256, kCFStringEncodingUTF8);
            *plen = strlen((char*)popt);
            break;
        case PPP_OPT_IFNAME:
            strncpy((char*)popt, (char*)serv->if_name, sizeof(serv->if_name));
            *plen = strlen((char*)popt);
            break;

        default:
            *plen = 0;
            return 0; // not found
    };

    return 1; // OK
}
