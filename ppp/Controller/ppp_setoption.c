/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#include "ppp_client.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "ppp_command.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
CFMutableDictionaryRef prepare_entity (CFMutableDictionaryRef opts, CFStringRef entity, CFStringRef property)
{
    CFMutableDictionaryRef 	dict;
    
    dict = (CFMutableDictionaryRef)CFDictionaryGetValue(opts, entity);
    // make sure we get a valid dictionary here
    if (dict && (CFGetTypeID(dict) != CFDictionaryGetTypeID()))
        return 0;	
        
    if (dict == 0) {
    	dict = CFDictionaryCreateMutable(NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (dict == 0)
            return 0;
        CFDictionaryAddValue((CFMutableDictionaryRef)opts, entity, dict);
        CFRelease(dict);
    }
    CFDictionaryRemoveValue(dict, property);
    
    return dict;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long set_long_opt (CFMutableDictionaryRef opts, CFStringRef entity, CFStringRef property, u_long opt, u_long mini, u_long maxi, u_long limit)
{
    CFMutableDictionaryRef 	dict;
    CFNumberRef 		num;
    
    if (opt < mini) {
        if (limit) opt = mini;
        else return EINVAL;
    }
    else if (opt > maxi) {
        if (limit) opt = maxi;
        else return EINVAL;
    }

    dict = prepare_entity(opts, entity, property);
    if (dict == 0)
        return ENOMEM;
    
    num = CFNumberCreate(NULL, kCFNumberSInt32Type, &opt);
    if (num) {
        CFDictionaryAddValue(dict, property, num);
        CFRelease(num); 
    } 

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long set_str_opt (CFMutableDictionaryRef opts, CFStringRef entity, CFStringRef property, char *opt, int len, CFStringRef optref)
{
    CFMutableDictionaryRef 	dict;
    CFStringRef 		str;
    
    dict = prepare_entity(opts, entity, property);
    if (dict == 0)
        return ENOMEM;

    if (optref)
        CFDictionaryAddValue(dict, property, optref);
    else {
        opt[len] = 0;
        str = CFStringCreateWithCString(NULL, opt, kCFStringEncodingUTF8);
        if (str) {
            CFDictionaryAddValue(dict, property, str);
            CFRelease(str);
        }
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long set_array_opt (CFMutableDictionaryRef opts, CFStringRef entity, CFStringRef property, CFStringRef optref1, CFStringRef optref2)
{
    CFMutableDictionaryRef 	dict;
    CFMutableArrayRef 		array;
    
    dict = prepare_entity(opts, entity, property);
    if (dict == 0)
        return ENOMEM;
    
    array = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    if (array == 0)
        return ENOMEM;
    
    if (optref1) 
        CFArrayAppendValue(array, optref1);
    if (optref2) 
        CFArrayAppendValue(array, optref2);
        
    CFDictionaryAddValue(dict, property, array);
    CFRelease(array);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
void remove_opt (CFMutableDictionaryRef opts, CFStringRef entity, CFStringRef property)
{
    CFMutableDictionaryRef	dict;
    
    dict = (CFMutableDictionaryRef)CFDictionaryGetValue(opts, entity);
    // make sure we get a valid dictionary here
    if (dict&& (CFGetTypeID(dict) == CFDictionaryGetTypeID()))
        CFDictionaryRemoveValue(dict, property);
}


/* -----------------------------------------------------------------------------
id must be a valid client 
----------------------------------------------------------------------------- */
u_long ppp_setoption (struct client *client, struct msg *msg, void **reply)
{
    struct ppp_opt 	*opt = (struct ppp_opt *)&msg->data[MSG_DATAOFF(msg)];
    u_int32_t		optint = *(u_int32_t *)(&opt->o_data[0]);
    u_char		*optstr = &opt->o_data[0];
    CFMutableDictionaryRef	opts;
    u_long		err = 0, len = msg->hdr.m_len - sizeof(struct ppp_opt_hdr), speed;
    struct ppp 		*ppp = ppp_find(msg);
    CFStringRef		string1, string2;
    
    if (!ppp) {
        msg->hdr.m_result = ENODEV;
        msg->hdr.m_len = 0;
        return 0;
    }

    // not connected, set the client options that will be used.
    opts = client_findoptset(client, ppp->serviceID);
    if (!opts) {
        // first option used by client, create private set
        opts = client_newoptset(client, ppp->serviceID);
        if (!opts) {
            msg->hdr.m_result = ENOMEM;
            msg->hdr.m_len = 0;
            return 0;
        }
    }

    switch (opt->o_type) {

        // COMM options
        case PPP_OPT_DEV_NAME:
            err = set_str_opt(opts, kSCEntNetInterface, kSCPropNetInterfaceDeviceName, optstr, len, 0);
            break;
        case PPP_OPT_DEV_SPEED:
            // add flexibility and adapt the speed to the immediatly higher speed
            speed = optint;
            if (speed <= 1200) speed = 1200;
            else if ((speed > 1200) && (speed <= 2400)) speed = 2400;
            else if ((speed > 2400) && (speed <= 9600)) speed = 9600;
            else if ((speed > 9600) && (speed <= 19200)) speed = 19200;
            else if ((speed > 19200) && (speed <= 38400)) speed = 38400;
            else if ((speed > 38400) && (speed <= 57600)) speed = 57600;
            else if ((speed > 38400) && (speed <= 57600)) speed = 57600;
            else if ((speed > 57600) && (speed <= 0xFFFFFFFF)) speed = 115200;
            err = set_long_opt(opts, kSCEntNetModem, kSCPropNetModemSpeed, speed, 0, 0xFFFFFFFF, 0);
            break;
        case PPP_OPT_DEV_CONNECTSCRIPT:
            err = set_str_opt(opts, kSCEntNetModem, kSCPropNetModemConnectionScript, optstr, len, 0);
            break;
        case PPP_OPT_DEV_DIALMODE:
            string1 = kSCValNetModemDialModeWaitForDialTone;
            switch (optint) {
                case PPP_DEV_IGNOREDIALTONE:
                    string1 = kSCValNetModemDialModeIgnoreDialTone;
                    break;
                case PPP_DEV_MANUALDIAL:
                    string1 = kSCValNetModemDialModeManual;
                    break;
            }
            if (string1)
                set_str_opt(opts, kSCEntNetModem, kSCPropNetModemDialMode, 0, 0, string1);
            break;
        case PPP_OPT_COMM_TERMINALMODE:
            switch (optint) {
                case PPP_COMM_TERM_NONE:
                    remove_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommDisplayTerminalWindow);
                    remove_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommUseTerminalScript);
                    break;
                case PPP_COMM_TERM_SCRIPT:
                    err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommUseTerminalScript, 1, 0, 1, 1)
                    	|| set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommDisplayTerminalWindow, 0, 0, 1, 1);
                    break;
                case PPP_COMM_TERM_WINDOW:
                    err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommUseTerminalScript, 0, 0, 1, 1)
                    	|| set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommDisplayTerminalWindow, 1, 0, 1, 1);
                    break;
            }
	    break;
        case PPP_OPT_COMM_TERMINALSCRIPT:
            err = set_str_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommTerminalScript, optstr, len, 0);
            break;
        case PPP_OPT_COMM_REMOTEADDR:
            err = set_str_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommRemoteAddress, optstr, len, 0);
            break;
        case PPP_OPT_COMM_IDLETIMER:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPDisconnectOnIdleTimer, optint, 0, 0xFFFFFFFF, 1)
                || set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPDisconnectOnIdle, optint, 0, 1, 1);
            break;
        case PPP_OPT_COMM_SESSIONTIMER:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPSessionTimer, optint, 0, 0xFFFFFFFF, 1)
            	|| set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPUseSessionTimer, optint, 0, 1, 1);
            break;
        case PPP_OPT_COMM_CONNECTDELAY:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPCommConnectDelay, optint, 0, 0xFFFFFFFF, 1);
            break;

            // LCP options
        case PPP_OPT_LCP_HDRCOMP:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPLCPCompressionPField, optint & PPP_LCP_HDRCOMP_PROTO, 0, 1, 1)
                || set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPLCPCompressionACField, optint & PPP_LCP_HDRCOMP_ADDR, 0, 1, 1);
            break;
        case PPP_OPT_LCP_MRU:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPLCPMRU, optint, 0, 0xFFFFFFFF, 1);
            break;
        case PPP_OPT_LCP_MTU:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPLCPMTU, optint, 0, 0xFFFFFFFF, 1);
            break;
        case PPP_OPT_LCP_RCACCM:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPLCPReceiveACCM, optint, 0, 0xFFFFFFFF, 1);
            break;
        case PPP_OPT_LCP_TXACCM:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPLCPTransmitACCM, optint, 0, 0xFFFFFFFF, 1);
            break;
        case PPP_OPT_LCP_ECHO:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPLCPEchoInterval, ((struct ppp_opt_echo *)opt->o_data)->interval, 0, 0xFFFFFFFF, 1)
            	|| set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPLCPEchoFailure, ((struct ppp_opt_echo *)opt->o_data)->failure, 0, 0xFFFFFFFF, 1);
            break;

            // SEC options
        case PPP_OPT_AUTH_PROTO:
            string1 = string2 = 0;
            switch (optint) {
                case PPP_AUTH_NONE:
                    string1 = CFSTR("None");//kSCValNetPPPAuthProtocolNone;
                    break;
                case PPP_AUTH_PAP:
                    string1 = kSCValNetPPPAuthProtocolPAP;
                    break;
                case PPP_AUTH_CHAP:
                    string2 = kSCValNetPPPAuthProtocolCHAP;
                    break;
                case PPP_AUTH_PAPCHAP:
                    string1 = kSCValNetPPPAuthProtocolPAP;
                    string2 = kSCValNetPPPAuthProtocolCHAP;
                    break;
                default:
                    err = EINVAL;
            }
            if (string1 || string2)
                err = set_array_opt(opts, kSCEntNetPPP, kSCPropNetPPPAuthProtocol, string1, string2);
            break;
        case PPP_OPT_AUTH_NAME:
           err = set_str_opt(opts, kSCEntNetPPP, kSCPropNetPPPAuthName, optstr, len, 0);
            break;
        case PPP_OPT_AUTH_PASSWD:
            err = set_str_opt(opts, kSCEntNetPPP, kSCPropNetPPPAuthPassword, optstr, len, 0);
            break;

            // IPCP options
        case PPP_OPT_IPCP_HDRCOMP:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPIPCPCompressionVJ, optint, 0, 1, 1);
            break;
        case PPP_OPT_IPCP_REMOTEADDR:
            string1 = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d.%d.%d.%d"), 
                optint >> 24, (optint >> 16) & 0xFF, (optint >> 8) & 0xFF, optint & 0xFF);
            if (string1) {
                err = set_array_opt(opts, kSCEntNetIPv4, kSCPropNetIPv4DestAddresses, string1, 0);
                CFRelease(string1);
            }
            break;
        case PPP_OPT_IPCP_LOCALADDR:
            string1 = CFStringCreateWithFormat(NULL, NULL, CFSTR("%d.%d.%d.%d"), 
                optint >> 24, (optint >> 16) & 0xFF, (optint >> 8) & 0xFF, optint & 0xFF);
            if (string1) {
                err = set_array_opt(opts, kSCEntNetIPv4, kSCPropNetIPv4Addresses, string1, 0);
                CFRelease(string1);
            }
            break;
            // MISC options
        case PPP_OPT_LOGFILE:
            err = EOPNOTSUPP;
            //err = set_str_opt(&opts->misc.logfile, optstr, len);
            break;
        case PPP_OPT_COMM_REMINDERTIMER:
            err = set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPIdleReminderTimer, optint, 0, 0xFFFFFFFF, 1)
                || set_long_opt(opts, kSCEntNetPPP, kSCPropNetPPPIdleReminder, optint, 0, 1, 1);
            break;
        case PPP_OPT_ALERTENABLE:
            err = set_long_opt(opts, kSCEntNetPPP, CFSTR("AlertEnable"), optint, 0, 0xFFFFFFFF, 1);
            break;
        default:
            err = EOPNOTSUPP;
    };
    
    msg->hdr.m_result = err;
    msg->hdr.m_len = 0;
    return 0;
}
