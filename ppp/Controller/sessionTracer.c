/*
 * Copyright (c) 2009 Apple Computer, Inc. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#import	 <asl.h>
#include <sys/types.h>
#include <mach/mach_time.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include <CoreFoundation/CFBundle.h>

#include "PPPControllerPriv.h"
#include "../Family/ppp_domain.h"
#include "../Helpers/pppd/pppd.h"
#include "../Family/if_ppplink.h"
#include "sessionTracer.h"
#include "ppp_manager.h"
#include "ppp_option.h"
#include "../Drivers/PPTP/PPTP-plugin/pptp.h"
#include "../Drivers/L2TP/L2TP-plugin/l2tp.h"
#include "../Drivers/PPPoE/PPPoE-extension/PPPoE.h"
#include "scnc_utils.h"

const char *sessionString				= "Controller";
const char *nullString					= "NULL";

static const char *
sessionGetConnectionDomain ()
{
	return PPP_CONNECTION_ESTABLISHED_DOMAIN;
}

static const char *
sessionGetConnectionLessDomain ()
{
	return PPP_CONNECTION_NOTESTABLISHED_DOMAIN;
}

static const char *
sessionGetType (struct service *serv)
{
	switch (serv->type) {
		case TYPE_PPP:
			switch (serv->subtype) {
				case PPP_TYPE_L2TP:
					return nullString;
				case PPP_TYPE_PPTP:
					return nullString;
				case PPP_TYPE_PPPoE:
					return PPPOENONVPN;
				case PPP_TYPE_SERIAL:
					return PPPSERIALNONVPN;
				default:
					return PLAINPPPNONVPN;
			}
			break;
		case TYPE_IPSEC:
			return nullString;
		default:
			return PLAINPPPNONVPN;
	}
}

static int
sessionCheckStatusForFailure (struct service *serv)
{
    switch (serv->type) {
        case TYPE_PPP:
            return (!(serv->u.ppp.laststatus == EXIT_OK ||
                      serv->u.ppp.laststatus == EXIT_USER_REQUEST ||
                      serv->u.ppp.laststatus == EXIT_IDLE_TIMEOUT ||
                      serv->u.ppp.laststatus == EXIT_CONNECT_TIME /* ||
                      serv->u.ppp.laststatus == EXIT_TRAFFIC_LIMIT */));
        default:
            return 0;
    }
}

static int
sessionGetReasonString (struct service *serv,
                        char           *reason_buf,
                        int             reason_bufsize)
{
    char tmp_buf[256];

    if (!reason_buf)
        return -1;

    reason_buf[0] = (char)0;
    switch (serv->type) {
        case TYPE_PPP:
        {
            const char *ppp_err = ppp_error_to_string(serv->u.ppp.laststatus);
            const char *dev_err = ppp_dev_error_to_string(serv->subtype, serv->u.ppp.lastdevstatus);
            if (!ppp_err && !serv->u.ppp.laststatus && !dev_err && !serv->u.ppp.lastdevstatus) {
                // nothing worthwile to report
                return -1;
            }
            tmp_buf[0] = (char)0;
            if (ppp_err) {
                snprintf(tmp_buf, sizeof(tmp_buf), "%s", ppp_err);                
            } else if (serv->u.ppp.laststatus) {
                snprintf(tmp_buf, sizeof(tmp_buf), "Error %d", serv->u.ppp.laststatus);
            }
            if (dev_err) {
                snprintf(reason_buf, reason_bufsize, "%s : %s", tmp_buf, dev_err);                
            } else if (serv->u.ppp.lastdevstatus) {
                snprintf(reason_buf, reason_bufsize, "%s : Device Error %d", tmp_buf, serv->u.ppp.lastdevstatus);
            } else {
                snprintf(reason_buf, reason_bufsize, "%s", tmp_buf);
            }
            break;
        }
		default:
            snprintf(reason_buf, reason_bufsize, "Unknown/Unsupported Service Type %d", serv->type);
            break;
    }
    return 0;
}

static int
sessionCheckIfEstablished (struct service *serv)
{
    switch (serv->type) {
        case TYPE_PPP:
            return (serv->establishtime != 0);
        default:
            return 0;
    }
}

static void
sessionIsEstablished (struct service *serv)
{
    switch (serv->type) {
        case TYPE_PPP:
            serv->establishtime = mach_absolute_time() * gTimeScaleSeconds;
            return;
	}
}

static u_int32_t
sessionGetConnectionDuration (struct service *serv)
{
    u_int32_t now = mach_absolute_time() * gTimeScaleSeconds;

    switch (serv->type) {
        case TYPE_PPP:
            if (serv->establishtime) {
                return ((now > serv->establishtime)? now - serv->establishtime : 0);
            }
            if (serv->connecttime) {
                return ((now > serv->connecttime)? now - serv->connecttime : 0);
            }
            return 0;
		default:
            return 0;
    }
}

static void
sessionTracerLogPPPInfo(aslmsg m, struct service *serv)
{
	PPPSession_t pppSess;
	
	MT_pppGetTracerOptions(serv, &pppSess);
	
	switch (serv->subtype) {
		case PPP_TYPE_PPPoE:
			asl_set(m, "com.apple.message.manualipv4", pppSess.manualIPv4);
			asl_set(m, "com.apple.message.manualipv6", pppSess.manualIPv6);
		break;
			
		default:
			asl_set(m, "com.apple.message.cclscript", pppSess.modem);
			asl_set(m, "com.apple.message.authprompt", pppSess.authPrompt);
			asl_set(m, "com.apple.message.hardwareinfo", pppSess.hardwareInfo);
			asl_set(m, "com.apple.message.redialenabled", pppSess.redialEnabled);
			asl_set(m, "com.apple.message.vjcompression", pppSess.vjCompression);
			asl_set(m, "com.apple.message.terminalwindow", pppSess.useTerminal);
		break;
	}

	asl_set(m, "com.apple.message.dialondemand", pppSess.dialOnDemand);
	asl_set(m, "com.apple.message.idlereminder", pppSess.idleReminder);
	asl_set(m, "com.apple.message.disconnectonlogout", pppSess.disconnectOnLogout);
	asl_set(m, "com.apple.message.disconnectonuserswitch", pppSess.disconnectOnUserSwitch);
	asl_set(m, "com.apple.message.echoenabled", pppSess.echoEnabled);
	asl_set(m, "com.apple.message.verboselogging", pppSess.verboseLogging);
	asl_set(m, "com.apple.message.manualdns", pppSess.manualDNS);
	asl_set(m, "com.apple.message.proxiesenabled", pppSess.proxiesEnabled);
	asl_set(m, "com.apple.message.winsenabled", pppSess.winsEnabled);
}

#if 0
static
void
sessionLogEvent (const char *domain, const char *event_msg)
{
	aslmsg m;

	if (!domain || !event_msg) {
		return;
	}

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, ASL_KEY_FACILITY, domain);
	asl_set(m, ASL_KEY_MSG, sessionString);
	asl_log(NULL, m, ASL_LEVEL_NOTICE, "SCNCController: %s", event_msg);
	asl_free(m);
}
#endif

#define ONE_MINUTE		60
#define FIVE_MINUTES	5	* ONE_MINUTE
#define ONE_HOUR		60	* ONE_MINUTE
#define FOUR_HOURS		4	* ONE_HOUR
#define EIGHT_HOURS		8	* ONE_HOUR
#define ONE_DAY			24	* ONE_HOUR

static char *
sessionTracerBucketizeTime (u_int32_t duration)
{
	if (duration <= FIVE_MINUTES) {
		return "< 5 mins";
	} else if (duration <= ONE_HOUR) {
		return "< 1 hour";
	} else if (duration <= FOUR_HOURS) {
		return "< 4 hours";
	} else if (duration <= EIGHT_HOURS) {
		return "< 8 hours";
	} else if (duration <= ONE_DAY) {
		return "< 1 day";
	} else {
		return "> 1 day";
	}
}

static void
sessionTracerLogStop (const char *domain, const char *type, int caused_by_failure, const char *reason, u_int32_t established, u_int32_t duration)
{
	aslmsg      m;
	char *      buf;
	char		data[256] = {0};

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, "com.apple.message.domain", domain);
	asl_set(m, "com.apple.message.type", type);
	asl_set(m, ASL_KEY_FACILITY, domain);
	asl_set(m, ASL_KEY_MSG, sessionString);
	if (caused_by_failure) {
		asl_set(m, "com.apple.message.result", CONSTSTR("failure"));	// failure
	} else {
		asl_set(m, "com.apple.message.result", CONSTSTR("success"));	// success
	}
	if (reason) {
		asl_set(m, "com.apple.message.reason", reason);
	} else {
		// reason was NULL; make sure success/failure have different signature
		if (caused_by_failure) {
			asl_set(m, "com.apple.message.reason", CONSTSTR("Internal/Server-side error"));
		} else {
			asl_set(m, "com.apple.message.reason", CONSTSTR("User/System initiated the disconnect"));
		}
	}
	
	buf = sessionTracerBucketizeTime(duration);
	
	if (established) {
		asl_set(m, "com.apple.message.connectiontime", buf);	// stuff the up time into value
		snprintf(data, sizeof(data), "SCNCController: Disconnecting.");
		asl_set(m, "com.apple.message.controller", data);
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
	} else {
		asl_set(m, "com.apple.message.negotiatingtime", buf);	/// stuff the negoing time into value2
		snprintf(data, sizeof(data), "SCNCController: Disconnecting.");
		asl_set(m, "com.apple.message.controller", data);
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
	}
	
	asl_free(m);
}

void
sessionTracerStop (struct service *serv)
{
	if (serv && (serv->connecttime || serv->establishtime)) {
		const char *type = sessionGetType(serv);
		
		if (!strcmp(type, nullString)) {
			/* We do not trace VPN */
			return;
		}
		
        int established = sessionCheckIfEstablished(serv);
		u_int32_t duration = sessionGetConnectionDuration(serv);
        char reason_buf[512];

        sessionTracerLogStop((established)? sessionGetConnectionDomain() : sessionGetConnectionLessDomain(),
							 type,
                             sessionCheckStatusForFailure(serv),
                             (sessionGetReasonString(serv, reason_buf, sizeof(reason_buf)) == 0)? reason_buf : NULL,
                             established,
                             duration);
        // may be log failure stats?
        // cleanup the messagetracer state info
        serv->establishtime = 0;
        serv->connecttime = 0;
    }
}

void
sessionTracerLogEstablished (struct service *serv)
{
	if (serv) {
		aslmsg      m;
		char data[256] = {0};
		const char *type = sessionGetType(serv);
		
		if (!strcmp(type, nullString)) {
			/* We do not trace VPN */
			return;
		}
		
		const char *domain	= sessionGetConnectionLessDomain();
		
		if (sessionCheckIfEstablished(serv)) {
			// already established no need to log (mostly here due to sleep-wake)
			return;
		}
		
		sessionIsEstablished(serv);
		
		m = asl_new(ASL_TYPE_MSG);
		asl_set(m, "com.apple.message.domain", domain);
		asl_set(m, "com.apple.message.type", type);
		asl_set(m, "com.apple.message.result", CONSTSTR("success"));
		asl_set(m, "com.apple.message.reason", CONSTSTR("established"));
		asl_set(m, ASL_KEY_FACILITY, domain);
		asl_set(m, ASL_KEY_MSG, sessionString);
		
		sessionTracerLogPPPInfo(m, serv);
		
		snprintf(data, sizeof(data), "SCNCController: Connected.");
		asl_set(m, "com.apple.message.controller", data);
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "");
		asl_free(m);
	}
}
