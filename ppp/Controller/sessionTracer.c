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

const char *sessionString			   = "Controller";

static const char *
sessionGetConnectionDomain (struct service *serv)
{
    switch (serv->type) {
        case TYPE_PPP:
            switch (serv->subtype) {
                case PPP_TYPE_L2TP:
                    return L2TPVPN_CONNECTION_ESTABLISHED_DOMAIN;
                case PPP_TYPE_PPTP:
                    return PPTPVPN_CONNECTION_ESTABLISHED_DOMAIN;
                case PPP_TYPE_PPPoE:
                    return PPPOEVPN_CONNECTION_ESTABLISHED_DOMAIN;
                case PPP_TYPE_SERIAL:
                    return PPPSERIALVPN_CONNECTION_ESTABLISHED_DOMAIN;
                default:
                    return PLAINPPPVPN_CONNECTION_ESTABLISHED_DOMAIN;
            }
            break;
        case TYPE_IPSEC:
            return CISCOVPN_CONNECTION_ESTABLISHED_DOMAIN;
        default:
            return PLAINPPPVPN_CONNECTION_ESTABLISHED_DOMAIN;
    }
}

static const char *
sessionGetConnectionLessDomain (struct service *serv)
{
    switch (serv->type) {
        case TYPE_PPP:
            switch (serv->subtype) {
                case PPP_TYPE_L2TP:
                    return L2TPVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
                case PPP_TYPE_PPTP:
                    return PPTPVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
                case PPP_TYPE_PPPoE:
                    return PPPOEVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
                case PPP_TYPE_SERIAL:
                    return PPPSERIALVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
                default:
                    return PLAINPPPVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
            }
            break;
        case TYPE_IPSEC:
            return CISCOVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
        default:
            return PLAINPPPVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
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
        case TYPE_IPSEC:
            return (!(serv->u.ipsec.laststatus == IPSEC_NO_ERROR ||
                      serv->u.ipsec.laststatus == IPSEC_IDLETIMEOUT_ERROR));
        default:
            return 0;
    }
}

static const char *
ppp_error_to_string (u_int32_t native_ppp_error)
{
    switch (native_ppp_error) {
        case EXIT_FATAL_ERROR:
            return CONSTSTR("Fatal Error");
        case EXIT_OPTION_ERROR:
            return CONSTSTR("Option Error");
        case EXIT_NOT_ROOT:
            return CONSTSTR("Not Root");
        case EXIT_NO_KERNEL_SUPPORT:
            return CONSTSTR("No Kernel Support");
        case EXIT_USER_REQUEST:
            return CONSTSTR("User requested");
        case EXIT_LOCK_FAILED:
            return CONSTSTR("Lock Failed");
        case EXIT_OPEN_FAILED:
            return CONSTSTR("Open Failed");
        case EXIT_CONNECT_FAILED:
            return CONSTSTR("Connect Failed");
        case EXIT_PTYCMD_FAILED:
            return CONSTSTR("Pty command Failed");
        case EXIT_NEGOTIATION_FAILED:
            return CONSTSTR("Negotiation Failed");
        case EXIT_PEER_AUTH_FAILED:
            return CONSTSTR("Peer Authentication Failed");
        case EXIT_IDLE_TIMEOUT:
            return CONSTSTR("Idle Timeout");
        case EXIT_CONNECT_TIME:
            return CONSTSTR("Session Timeout");
        case EXIT_CALLBACK:
            return CONSTSTR("Callback");
        case EXIT_PEER_DEAD:
            return CONSTSTR("Peer Dead");
        case EXIT_HANGUP:
            return CONSTSTR("Disconnect by Device");
        case EXIT_LOOPBACK:
            return CONSTSTR("Loopback Error");
        case EXIT_INIT_FAILED:
            return CONSTSTR("Init Failed");
        case EXIT_AUTH_TOPEER_FAILED:
            return CONSTSTR("Authentication to Peer Failed");
        case EXIT_TERMINAL_FAILED:
            return CONSTSTR("Terminal Failed");
        case EXIT_DEVICE_ERROR:
            return CONSTSTR("Device Error");
        case EXIT_PEER_NOT_AUTHORIZED:
            return CONSTSTR("Peer Not Authorized");
        case EXIT_CNID_AUTH_FAILED:
            return CONSTSTR("CNID Authentication Failed");
        case EXIT_PEER_UNREACHABLE:
            return CONSTSTR("Peer Unreachable");
    }

    return CONSTSTR(NULL);
}

static const char *
ppp_dev_error_to_string (u_int16_t subtype, u_int32_t native_dev_error)
{
    // override with a more specific error
    if (native_dev_error) {
        switch (subtype) {
            case PPP_TYPE_L2TP:
                switch (native_dev_error) {
                    case EXIT_L2TP_NOSERVER:
                        return CONSTSTR("No Server");
                    case EXIT_L2TP_NOANSWER:
                        return CONSTSTR("No Answer");
                    case EXIT_L2TP_PROTOCOLERROR:
                        return CONSTSTR("Protocol Error");
                    case EXIT_L2TP_NETWORKCHANGED:
                        return CONSTSTR("Network Changed");
                    case EXIT_L2TP_NOSHAREDSECRET:
                        return CONSTSTR("Shared Secret");
                    case EXIT_L2TP_NOCERTIFICATE:
                        return CONSTSTR("No Certificate");
                }
                break;

            case PPP_TYPE_PPTP:
                switch (native_dev_error) {
                    case EXIT_PPTP_NOSERVER:
                        return CONSTSTR("No Server");
                    case EXIT_PPTP_NOANSWER:
                        return CONSTSTR("No Answer");
                    case EXIT_PPTP_PROTOCOLERROR:
                        return CONSTSTR("Protocol Error");
                    case EXIT_PPTP_NETWORKCHANGED:
                        return CONSTSTR("Network Changed");
                }
                break;

            case PPP_TYPE_SERIAL:
                switch (native_dev_error) {
                    case EXIT_PPPSERIAL_NOCARRIER:
                        return CONSTSTR("No Carrier");
                    case EXIT_PPPSERIAL_NONUMBER:
                        return CONSTSTR("No Number");
                    case EXIT_PPPSERIAL_BADSCRIPT:
                        return CONSTSTR("Bad Script");
                    case EXIT_PPPSERIAL_BUSY:
                        return CONSTSTR("Busy");
                    case EXIT_PPPSERIAL_NODIALTONE:
                        return CONSTSTR("No Dial Tone");
                    case EXIT_PPPSERIAL_ERROR:
                        return CONSTSTR("Modem Error");
                    case EXIT_PPPSERIAL_NOANSWER:
                        return CONSTSTR("No Answer");
                    case EXIT_PPPSERIAL_HANGUP:
                        return CONSTSTR("Hang-up");
                }
                break;
                
            case PPP_TYPE_PPPoE:
                switch (native_dev_error) {
                    case EXIT_PPPoE_NOSERVER:
                        return CONSTSTR("No Server");
                    case EXIT_PPPoE_NOSERVICE:
                        return CONSTSTR("No Service");
                    case EXIT_PPPoE_NOAC:
                        return CONSTSTR("No AC");
                    case EXIT_PPPoE_NOACSERVICE:
                        return CONSTSTR("No AC Service");
                    case EXIT_PPPoE_CONNREFUSED:
                        return CONSTSTR("Connection Refused");
                }
                break;
        }
    }
    
    return CONSTSTR(NULL);
}

static const char *
ipsec_error_to_string (int status)
{
    switch (status) {
        case IPSEC_GENERIC_ERROR:
            return CONSTSTR("Generic Error");
        case IPSEC_NOSERVERADDRESS_ERROR:
            return CONSTSTR("No Server Address");
        case IPSEC_NOSHAREDSECRET_ERROR:
            return CONSTSTR("No Shared Secret");
        case IPSEC_NOCERTIFICATE_ERROR:
            return CONSTSTR("No Certificate");
        case IPSEC_RESOLVEADDRESS_ERROR:
            return CONSTSTR("Resolve Address Error");
        case IPSEC_NOLOCALNETWORK_ERROR:
            return CONSTSTR("No Local Network");
        case IPSEC_CONFIGURATION_ERROR:
            return CONSTSTR("Configuration Error");
        case IPSEC_RACOONCONTROL_ERROR:
            return CONSTSTR("Racoon Control Error");
        case IPSEC_CONNECTION_ERROR:
            return CONSTSTR("Connection Error");
        case IPSEC_NEGOTIATION_ERROR:
            return CONSTSTR("Negotiation Error");
        case IPSEC_SHAREDSECRET_ERROR:
            return CONSTSTR("Shared Secret Error");
        case IPSEC_SERVER_CERTIFICATE_ERROR:
            return CONSTSTR("Server Certificate Error");
        case IPSEC_CLIENT_CERTIFICATE_ERROR:
            return CONSTSTR("Client Certificate Error");
        case IPSEC_XAUTH_ERROR:
            return CONSTSTR("Xauth Error");
        case IPSEC_NETWORKCHANGE_ERROR:
            return CONSTSTR("Network Change");
        case IPSEC_PEERDISCONNECT_ERROR:
            return CONSTSTR("Peer Disconnect");
        case IPSEC_PEERDEADETECTION_ERROR:
            return CONSTSTR("Peer Dead");
        case IPSEC_EDGE_ACTIVATION_ERROR:
            return CONSTSTR("Edge Activation Error");
        case IPSEC_IDLETIMEOUT_ERROR:
            return CONSTSTR("Idle Timeout");
		case IPSEC_CLIENT_CERTIFICATE_PREMATURE:
			return CONSTSTR("Client Certificate premature");
		case IPSEC_CLIENT_CERTIFICATE_EXPIRED:
			return CONSTSTR("Client Certificate expired");
		case IPSEC_SERVER_CERTIFICATE_PREMATURE:
			return CONSTSTR("Server Certificate premature");
		case IPSEC_SERVER_CERTIFICATE_EXPIRED:
			return CONSTSTR("Server Certificate expired");
		case IPSEC_SERVER_CERTIFICATE_INVALID_ID:
			return CONSTSTR("Server Certificate identity incorrect");
    }

    return CONSTSTR(NULL);
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
            } else {
				*tmp_buf = 0;
                //snprintf(tmp_buf, sizeof(tmp_buf), "");
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
        case TYPE_IPSEC:
        {
            const char *ipsec_err = ipsec_error_to_string(serv->u.ipsec.laststatus);
            if (!ipsec_err && !serv->u.ipsec.laststatus) {
                // nothing worthwile to report
                return -1;
            }
            if (ipsec_err) {
                snprintf(reason_buf, reason_bufsize, "%s", ipsec_err);
            } else {
                snprintf(reason_buf, reason_bufsize, "Error %d", serv->u.ipsec.laststatus);
            }
            break;
        }
        default:
            snprintf(reason_buf, reason_bufsize, "Unknown Service Type %d", serv->type);
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
        case TYPE_IPSEC:
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
        case TYPE_IPSEC:
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
        case TYPE_IPSEC:
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

static void
sessionTracerLogStop (const char *domain, int caused_by_failure, const char *reason, u_int32_t established, u_int32_t duration)
{
	aslmsg      m;
	char        buf[128];

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, "com.apple.message.domain", domain);
	asl_set(m, ASL_KEY_FACILITY, domain);
	asl_set(m, ASL_KEY_MSG, sessionString);
	if (caused_by_failure) {
		asl_set(m, "com.apple.message.result", CONSTSTR("failure"));	// failure
	} else {
		asl_set(m, "com.apple.message.result", CONSTSTR("success"));	// success
	}
	if (reason) {
		asl_set(m, "com.apple.message.signature", reason);
	} else {
		// reason was NULL; make sure success/failure have different signature
		if (caused_by_failure) {
			asl_set(m, "com.apple.message.signature", CONSTSTR("Internal/Server-side error"));
		} else {
			asl_set(m, "com.apple.message.signature", CONSTSTR("User/System initiated the disconnect"));
		}
	}
	if (established) {
		snprintf(buf, sizeof(buf), "%d", duration);
		asl_set(m, "com.apple.message.value", buf);	// stuff the up time into value
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "SCNCController: Disconnecting. (Connection was up for, %s seconds).", buf);
	} else {
		snprintf(buf, sizeof(buf), "%d", duration);
		asl_set(m, "com.apple.message.value2", buf);	/// stuff the negoing time into value2
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "SCNCController: Disconnecting. (Connection tried to negotiate for, %s seconds).", buf);
	}
	asl_free(m);
}

void
sessionTracerStop (struct service *serv)
{
	if (serv && (serv->connecttime || serv->establishtime)) {
        int established = sessionCheckIfEstablished(serv);
		u_int32_t duration = sessionGetConnectionDuration(serv);
        char reason_buf[512];        

        sessionTracerLogStop((established)? sessionGetConnectionDomain(serv) : sessionGetConnectionLessDomain(serv),
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
        const char *domain = sessionGetConnectionLessDomain(serv);

        if (sessionCheckIfEstablished(serv)) {
            // already established no need to log (mostly here due to sleep-wake)
            return;
        }

        sessionIsEstablished(serv);

        m = asl_new(ASL_TYPE_MSG);
        asl_set(m, "com.apple.message.domain", domain);
        asl_set(m, ASL_KEY_FACILITY, domain);
        asl_set(m, ASL_KEY_MSG, sessionString);
        asl_set(m, "com.apple.message.result", "success");	// success
        asl_set(m, "com.apple.message.signature", "success");
        asl_log(NULL, m, ASL_LEVEL_NOTICE, "SCNCController: Connected.");
        asl_free(m);
    }
}
