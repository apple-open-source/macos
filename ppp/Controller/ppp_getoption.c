/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
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

#include "ppp_msg.h"
#include "ppp_privmsg.h"

#include "../Family/ppp_domain.h"
#include "ppp_client.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_command.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

#ifndef MIN
#define MIN(a, b)	((a) < (b)? (a): (b))
#endif
#ifndef MAX
#define MAX(a, b)	((a) > (b)? (a): (b))
#endif

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long get_ipaddr (struct ppp *ppp, struct opt_long *option, 
        CFStringRef entity, CFStringRef property, 
        u_int32_t *opt, u_char lookinsetup, u_int32_t defaultval)
{

    // first search in the state
    if (!getAddressFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
        entity, property, opt)) {
        return 0;
    }

    // then, search in the option set
    if (option && option->set) {
        PRINTF(("    ---> option is set = %ld  \n", option->val));
        *opt = option->val;
        return 0;
    }
    
    // at last, search in the setup 
    if (lookinsetup
        && !getAddressFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
        entity, property, opt)) {
        return 0;
    }

    *opt = defaultval;
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long get_long (struct ppp *ppp, struct opt_long *option, 
        CFStringRef entity, CFStringRef property, 
        u_int32_t *opt, CFDictionaryRef setupdict, u_char lookinsetup, u_int32_t defaultval)
{

    // first search in the state
    if (!getNumberFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
        entity, property, opt)) {
        return 0;
    }

    // then, search in the option set
    if (option && option->set) {
        PRINTF(("    ---> option is set = %ld  \n", option->val));
        *opt = option->val;
        return 0;
    }
    
    // at last, search in the setup, only in lookinsetup flag is set
    if (lookinsetup && 
        ((setupdict && !getNumber(setupdict, property, opt))
        || (!setupdict && !getNumberFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
        entity, property, opt)))) {
        return 0;
    }

    *opt = defaultval;
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int get_str (struct ppp *ppp, struct opt_str *option, 
        CFStringRef entity, CFStringRef property,
        u_char *opt, u_int32_t *outlen, CFDictionaryRef setupdict, u_char lookinsetup, u_char *defaultval)
{
    unsigned short 	len;
    
    // first search in the state
    if (!getStringFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
        entity, property, opt, OPT_STR_LEN)) {
        *outlen = strlen(opt);
        return 0;
    }

    // then, search in the option set
    if (option && option->set) {
        PRINTF(("    ---> option is set = %s  \n", option->str));
        len = MIN(OPT_STR_LEN, strlen(option->str) + 1);
        strncpy(opt, option->str, len);
        opt[len - 1] = 0;
        *outlen = len;
        return 0;
    }
    
    // at last, search in the setup, only in lookinsetup flag is set
    if (lookinsetup && 
        ((setupdict && !getString(setupdict, property, opt, OPT_STR_LEN))
        || (!setupdict && !getStringFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
        entity, property, opt, OPT_STR_LEN)))) {
        *outlen = strlen(opt);
        return 0;
    }

    strcpy(opt, defaultval);
    *outlen = strlen(opt);
    return 1;
}

#if 0
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int get_passwd (struct ppp *ppp, struct opt_str *option, 
        CFStringRef entity, CFStringRef property,
        u_char *opt, u_int32_t *outlen, CFDictionaryRef setupdict)
{
    unsigned short 		len;
    
    *opt = 0;
    *outlen = 0;

    if (ppp->phase != PPP_RUNNING && ppp->phase != PPP_ONHOLD) {
        // not running, search in our set, then in the setup
        if (option && option->set) {
            PRINTF(("    ---> Client %d, option is set = %s  \n", id, option->str));
            len = MIN(OPT_STR_LEN, strlen(option->str) + 1);
            strncpy(opt, option->str, len);
            opt[len - 1] = 0;
            *outlen = len;
        }
        else if (!getStringFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
            entity, property, opt, OPT_STR_LEN)) {
            *outlen = strlen(opt);
        }
        else 
            return 1;
    }
    else {
        // not idle, search in the state
        // Fix Me : during connection/disconnection phases, options may be incomplete
        if (!getStringFromEntity(kSCDynamicStoreDomainState, ppp->serviceID, 
            entity, property, opt, OPT_STR_LEN)) {
            *outlen = strlen(opt);
        }
        else 
            return 1;
    }

    return 0;
}
#endif
/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getoption (struct client *client, struct msg *msg)
{
    struct ppp_opt 		*opt = (struct ppp_opt *)&msg->data[MSG_DATAOFF(msg)];
    struct options		*opts;
    struct ppp 			*ppp = ppp_find(msg);
            
    if (!ppp) {
        msg->hdr.m_len = 0;
        msg->hdr.m_result = ENODEV;
        return 0;
    }

    // not connected, get the client options that will be used.
    opts = client_findoptset(client, ppp_makeref(ppp));
       
    if (ppp_getoptval(ppp, opts, opt->o_type, &opt->o_data[0], &msg->hdr.m_len, 0)) {
        msg->hdr.m_len = 0;
        msg->hdr.m_result = EOPNOTSUPP;
        return 0;
    }
    
    msg->hdr.m_result = 0;
    msg->hdr.m_len += sizeof(struct ppp_opt_hdr);
    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int ppp_getoptval(struct ppp *ppp, struct options *opts, u_int32_t otype, void *pdata, u_int32_t *plen, CFDictionaryRef pppdict)
{
    u_int32_t 			lval, lval1, lval2;
    u_int32_t			*lopt = (u_int32_t *)pdata;
    u_char 			*popt = (u_char *)pdata;
    struct options		emptyopts;
    char 			str[OPT_STR_LEN], str2[OPT_STR_LEN];

    if (!opts) {
        bzero(&emptyopts, sizeof(emptyopts));
        opts = &emptyopts; // just use a fake empty structure
    }

    *plen = 4; // init the len for long options
    *lopt = 0; // init to zero
   
    switch (otype) {

        // DEVICE options
        case PPP_OPT_DEV_NAME:
            get_str(ppp, &opts->dev.name, kSCEntNetInterface, kSCPropNetInterfaceDeviceName, popt, plen, 0, 1, 
                (ppp->subtype == PPP_TYPE_SERIAL) ? OPT_DEV_NAME_DEF : 
                ((ppp->subtype == PPP_TYPE_PPPoE) ? OPT_DEV_NAME_PPPoE_DEF : ""));
            break;
        case PPP_OPT_DEV_SPEED:
            *lopt = 0; 
            switch (ppp->subtype) {
                case PPP_TYPE_SERIAL:
                    get_long(ppp, &opts->dev.speed, kSCEntNetModem, kSCPropNetModemSpeed, lopt, 0, 1, OPT_DEV_SPEED_DEF);
                    break;
                case PPP_TYPE_PPPoE:
                case PPP_TYPE_PPTP:
                case PPP_TYPE_L2TP:
                    break;
            }
            break;
        case PPP_OPT_DEV_CONNECTSCRIPT:
            get_str(ppp, &opts->dev.connectscript, kSCEntNetModem, kSCPropNetModemConnectionScript, popt, plen, 0, 1, 
                (ppp->subtype == PPP_TYPE_SERIAL) ? OPT_DEV_CONNECTSCRIPT_DEF : "");
            break;
 
        case PPP_OPT_DEV_DIALMODE:
            *plen = 4;
            *lopt = PPP_DEV_WAITFORDIALTONE;
            // search in the option set
            if (opts->dev.dialmode.set) {
                *lopt = opts->dev.dialmode.val;
            }
            // then, search in the setup
            else if (!getStringFromEntity(kSCDynamicStoreDomainSetup, ppp->serviceID, 
                kSCEntNetModem, kSCPropNetModemDialMode, str, OPT_STR_LEN)) {

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
            }
            break;

        // COMM options
       case PPP_OPT_DEV_CONNECTSPEED:
            *lopt = 0; 
            switch (ppp->subtype) {
                case PPP_TYPE_SERIAL:
                    get_long(ppp, 0, kSCEntNetModem, kSCPropNetModemConnectSpeed, lopt, 0, 0, 0);
                    break;
                case PPP_TYPE_PPPoE:
                case PPP_TYPE_PPTP:
                case PPP_TYPE_L2TP:
                    break;
            }
            break;
        case PPP_OPT_COMM_TERMINALMODE:
            // this information is not found in the state...
            if (opts->comm.terminalmode.set)
                *lopt = opts->comm.terminalmode.val;
            else {            
                *lopt = OPT_COMM_TERMINALMODE_DEF; 
                if (!get_long(ppp, 0, kSCEntNetPPP, kSCPropNetPPPCommDisplayTerminalWindow, &lval, pppdict, 1, 0) && lval) 
                     *lopt = PPP_COMM_TERM_WINDOW;
                else if (!get_long(ppp, 0, kSCEntNetPPP, kSCPropNetPPPCommUseTerminalScript, &lval, pppdict, 1, 0) && lval) 
                    *lopt = PPP_COMM_TERM_SCRIPT;
            }
	    break;	    
        case PPP_OPT_COMM_TERMINALSCRIPT:
            get_str(ppp, &opts->comm.terminalscript, kSCEntNetPPP, kSCPropNetPPPCommTerminalScript, popt, plen, pppdict, 1, "");
            break;
        case PPP_OPT_COMM_IDLETIMER:
            get_long(ppp, 0, kSCEntNetPPP, kSCPropNetPPPDisconnectOnIdle, &lval, pppdict, 1, 0);
            get_long(ppp, &opts->comm.idletimer, kSCEntNetPPP, kSCPropNetPPPDisconnectOnIdleTimer, lopt, pppdict, lval, lval ? OPT_COMM_IDLETIMER_DEF : 0);
            break;
        case PPP_OPT_COMM_SESSIONTIMER:
            get_long(ppp, 0, kSCEntNetPPP, kSCPropNetPPPUseSessionTimer, &lval, pppdict, 1, 0);
            get_long(ppp, &opts->comm.sessiontimer, kSCEntNetPPP, kSCPropNetPPPSessionTimer, lopt, pppdict, lval, lval ? OPT_COMM_SESSIONTIMER_DEF : 0);
            break;
        case PPP_OPT_COMM_REMINDERTIMER:
            get_long(ppp, 0, kSCEntNetPPP, kSCPropNetPPPIdleReminder, &lval, pppdict, 1, 0);
            get_long(ppp, &opts->comm.remindertimer, kSCEntNetPPP, kSCPropNetPPPIdleReminderTimer, lopt, pppdict, lval, lval ? OPT_COMM_REMINDERTIMER_DEF : 0);
            break;
        case PPP_OPT_COMM_REMOTEADDR:
            get_str(ppp, &opts->comm.remoteaddr, kSCEntNetPPP, kSCPropNetPPPCommRemoteAddress, popt, plen, pppdict, 1, "");
            break;
        case PPP_OPT_COMM_CONNECTDELAY:
            get_long(ppp, &opts->comm.connectdelay, kSCEntNetPPP, kSCPropNetPPPCommConnectDelay, lopt, pppdict, 1, OPT_COMM_CONNECTDELAY_DEF);
            break;
            
        // LCP options
        case PPP_OPT_LCP_HDRCOMP:
            get_long(ppp, &opts->lcp.pcomp, kSCEntNetPPP, kSCPropNetPPPLCPCompressionPField, &lval, pppdict, 1, OPT_LCP_PCOMP_DEF);
            get_long(ppp, &opts->lcp.accomp, kSCEntNetPPP, kSCPropNetPPPLCPCompressionACField, &lval1, pppdict, 1, OPT_LCP_ACCOMP_DEF);
            *lopt = lval + (lval1 << 1); 
            break;
        case PPP_OPT_LCP_MRU:
            switch (ppp->subtype) {
                case PPP_TYPE_PPPoE:
                    lval = OPT_LCP_MRU_PPPoE_DEF;
                    break;
                case PPP_TYPE_PPTP:
                    lval = OPT_LCP_MRU_PPTP_DEF;
                    break;
                default:
                    lval = OPT_LCP_MRU_DEF;
            }
            get_long(ppp, &opts->lcp.mru, kSCEntNetPPP, kSCPropNetPPPLCPMRU, lopt, pppdict, 1, lval);
            break;
        case PPP_OPT_LCP_MTU:
            switch (ppp->subtype) {
                case PPP_TYPE_PPPoE:
                    lval = OPT_LCP_MTU_PPPoE_DEF;
                    break;
                case PPP_TYPE_PPTP:
                    lval = OPT_LCP_MTU_PPTP_DEF;
                    break;
                default:
                    lval = OPT_LCP_MTU_DEF;
            }
            get_long(ppp, &opts->lcp.mtu, kSCEntNetPPP, kSCPropNetPPPLCPMTU, lopt, pppdict, 1, lval);
            break;
        case PPP_OPT_LCP_RCACCM:
            get_long(ppp, &opts->lcp.rcaccm, kSCEntNetPPP, kSCPropNetPPPLCPReceiveACCM, lopt, pppdict, 1, OPT_LCP_RCACCM_DEF);
            break;
        case PPP_OPT_LCP_TXACCM:
            get_long(ppp, &opts->lcp.txaccm, kSCEntNetPPP, kSCPropNetPPPLCPTransmitACCM, lopt, pppdict, 1,  OPT_LCP_TXACCM_DEF);
            break;
        case PPP_OPT_LCP_ECHO:
            get_long(ppp, 0, kSCEntNetPPP, kSCPropNetPPPLCPEchoEnabled, &lval, pppdict, 1, 0);
            get_long(ppp, &opts->lcp.echointerval, kSCEntNetPPP, kSCPropNetPPPLCPEchoInterval, &lval1, pppdict, lval,  lval ? OPT_LCP_ECHOINTERVAL_DEF : 0);
            get_long(ppp, &opts->lcp.echofailure, kSCEntNetPPP, kSCPropNetPPPLCPEchoFailure, &lval2, pppdict, lval, lval ? OPT_LCP_ECHOFAILURE_DEF : 0);
            *lopt = (lval1 << 16) + lval2; 
            break;

        // AUTH options
        case PPP_OPT_AUTH_PROTO:
            *lopt = OPT_AUTH_PROTO_DEF;
            if (opts->auth.proto.set)
                *lopt = opts->auth.proto.val;
            break;
        case PPP_OPT_AUTH_NAME:
             get_str(ppp, &opts->auth.name, kSCEntNetPPP, kSCPropNetPPPAuthName, popt, plen, pppdict, 1, "");
            break;
        case PPP_OPT_AUTH_PASSWD:
//             get_passwd(ppp, &opts->auth.passwd, kSCEntNetPPP, kSCPropNetPPPAuthPassword, popt, plen, pppdict);
            get_str(ppp, &opts->auth.passwd, kSCEntNetPPP, kSCPropNetPPPAuthPassword, popt, plen, pppdict, 1, "");
            // don't return the actual pasword.
            // instead, return len = 1 if password is known, ken = 0 if password is unknown
            if (*plen) {
                popt[0] = '*';
                *plen = 1;
            }
            break;

            // IPCP options
        case PPP_OPT_IPCP_HDRCOMP:
            get_long(ppp, &opts->ipcp.hdrcomp, kSCEntNetPPP, kSCPropNetPPPIPCPCompressionVJ, lopt, pppdict, 1, OPT_IPCP_HDRCOMP_DEF);
            break;
        case PPP_OPT_IPCP_LOCALADDR:
             get_ipaddr(ppp, &opts->ipcp.localaddr, kSCEntNetIPv4, kSCPropNetIPv4Addresses, lopt, 1, 0);
           break;
        case PPP_OPT_IPCP_REMOTEADDR:
            get_ipaddr(ppp, &opts->ipcp.remoteaddr, kSCEntNetIPv4, kSCPropNetIPv4DestAddresses, lopt, 1, 0);
            break;

           // MISC options
        case PPP_OPT_LOGFILE:
             get_str(ppp, &opts->misc.logfile, kSCEntNetPPP, kSCPropNetPPPLogfile, popt, plen, pppdict, 1, "");
             if (popt[0] != '/') {
                lval = strlen(DIR_LOGS);
                strncpy(popt + lval, popt, *plen);
                strncpy(popt, DIR_LOGS, lval);
                *plen += lval;
             }
            break;
        case PPP_OPT_ALERTENABLE:
            get_long(ppp, &opts->misc.alertenable, kSCEntNetPPP, CFSTR("AlertEnable"), lopt, pppdict, 1, 0xFFFFFFFF);
            break;
        case PPP_OPT_AUTOCONNECT:
            get_long(ppp, &opts->misc.autoconnect, kSCEntNetPPP, kSCPropNetPPPDialOnDemand, lopt, pppdict, 1, 0);
            break;
        case PPP_OPT_SERVICEID:
            popt[0] = 0;
            CFStringGetCString(ppp->serviceID, popt, 256, kCFStringEncodingUTF8);
            *plen = strlen(popt);
            break;
        case PPP_OPT_IFNAME:
            if (ppp->ifunit != 0xFFFF) {
                sprintf(popt, "%s%d", ppp->name, ppp->ifunit);
                *plen = strlen(popt);
            }
            break;

        default:
            *plen = 0;
            return EOPNOTSUPP;
    };

    return 0;
}
