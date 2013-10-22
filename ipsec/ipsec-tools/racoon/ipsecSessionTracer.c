/*
 * Copyright (c) 2008 Apple Computer, Inc. All rights reserved.
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
#include "ike_session.h"
#include "ipsecMessageTracer.h"
#include "misc.h"
#include "nattraversal.h"

#define     TRUE	1
#define		FALSE	0
const char *ipsecSessionInvalidEventString = "Invalid Event";
const char *ipsecSessionString			   = "IPSEC";

/* tells us the event's description */
const char * const ipsecSessionEventStrings[IPSECSESSIONEVENTCODE_MAX] = {	CONSTSTR("NONE") /* index place holder */,
																			CONSTSTR("IKE Packet: transmit success"),
																			CONSTSTR("IKE Packet: transmit failed"),
																			CONSTSTR("IKE Packet: receive success"),
																			CONSTSTR("IKE Packet: receive failed"),
																			CONSTSTR("IKEv1 Phase 1 Initiator: success"),
																			CONSTSTR("IKEv1 Phase 1 Initiator: failed"),
																			CONSTSTR("IKEv1 Phase 1 Initiator: dropped"),
																			CONSTSTR("IKEv1 Phase 1 Responder: success"),
																			CONSTSTR("IKEv1 Phase 1 Responder: failed"),
																			CONSTSTR("IKEv1 Phase 1 Responder: drop"),
																			CONSTSTR("IKEv1 Phase 1: maximum retransmits"),
																			CONSTSTR("IKEv1 Phase 1 AUTH: success"),
																			CONSTSTR("IKEv1 Phase 1 AUTH: failed"),
																			CONSTSTR("IKEv1 Dead-Peer-Detection: request transmitted"),
																			CONSTSTR("IKEv1 Dead-Peer-Detection: response received"),
																			CONSTSTR("IKEv1 Dead-Peer-Detection: request retransmitted"),
																			CONSTSTR("IKEv1 Dead-Peer-Detection: request received"),
																			CONSTSTR("IKEv1 Dead-Peer-Detection: response transmitted"),
																			CONSTSTR("IKEv1 Dead-Peer-Detection: response retransmitted"),
																			CONSTSTR("IKEv1 Dead-Peer-Detection: maximum retransmits"),
																			CONSTSTR("IKEv1 Config: retransmited"),
																			CONSTSTR("IKEv1 Mode-Config: success"),
																			CONSTSTR("IKEv1 Mode-Config: failed"),
																			CONSTSTR("IKEv1 Mode-Config: dropped"),
																			CONSTSTR("IKEv1 XAUTH: success"),
																			CONSTSTR("IKEv1 XAUTH: failed"),
																			CONSTSTR("IKEv1 XAUTH: dropped"),
																			CONSTSTR("IKEv1 Phase 2 Initiator: success"),
																			CONSTSTR("IKEv1 Phase 2 Initiator: failed"),
																			CONSTSTR("IKEv1 Phase 2 Initiator: dropped"),
																			CONSTSTR("IKEv1 Phase 2 Responder: success"),
																			CONSTSTR("IKEv1 Phase 2 Responder: fail"),
																			CONSTSTR("IKEv1 Phase 2 Responder: drop"),
																			CONSTSTR("IKEv1 Phase 2: maximum retransmits"),
																			CONSTSTR("IKEv1 Phase 2 AUTH: success"),
																			CONSTSTR("IKEv1 Phase 2 AUTH: failed"),
																			CONSTSTR("IKEv1 Information-Notice: transmit success"),
																			CONSTSTR("IKEv1 Information-Notice: transmit failed"),
																			CONSTSTR("IKEv1 Information-Notice: receive success"),
																			CONSTSTR("IKEv1 Information-Notice: receive failed"),
																		};

/* tells us if we can ignore the failure_reason passed into the event tracer */
const int ipsecSessionEventIgnoreReason[IPSECSESSIONEVENTCODE_MAX] = {TRUE/* index place holder */,
																			TRUE,
																			TRUE,
																			TRUE,
																			TRUE,
																			TRUE,
																			FALSE,
																			TRUE,
																			TRUE,
																			FALSE,
																			TRUE,
																			FALSE,
																			TRUE,
																			FALSE,
																			TRUE,
																			TRUE,
																			TRUE,
																			TRUE,
																			TRUE,
																			TRUE,
																			FALSE,
																			TRUE,
																			TRUE,
																			FALSE,
																			FALSE,
																			TRUE,
																			FALSE,
																			FALSE,
																			TRUE,
																			FALSE,
																			TRUE,
																			TRUE,
																			FALSE,
																			TRUE,
																			FALSE,
																			TRUE,
																			FALSE,
																			TRUE,
																			TRUE,
																			TRUE,
																			TRUE,
																			};


const char *
ipsecSessionEventCodeToString (ipsecSessionEventCode_t eventCode)
{
	if (eventCode <= IPSECSESSIONEVENTCODE_NONE || eventCode >= IPSECSESSIONEVENTCODE_MAX)
		return ipsecSessionInvalidEventString;
	return(ipsecSessionEventStrings[eventCode]);
}

const char *
ipsecSessionGetConnectionDomain (ike_session_t *session)
{
	if (session) {
		if (session->is_cisco_ipsec) {
            if (session->established) {
                return CISCOIPSECVPN_CONNECTION_ESTABLISHED_DOMAIN;
            } else {
                return CISCOIPSECVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
            }
		} else if (session->is_l2tpvpn_ipsec) {
            if (session->established) {
                return L2TPIPSECVPN_CONNECTION_ESTABLISHED_DOMAIN;
            } else {
                return L2TPIPSECVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
            }
		} else if (session->is_btmm_ipsec) {
            if (session->established) {
                return BTMMIPSEC_CONNECTION_ESTABLISHED_DOMAIN;
            } else {
                return BTMMIPSEC_CONNECTION_NOTESTABLISHED_DOMAIN;
            }
		} else {
            if (session->established) {
                return PLAINIPSEC_CONNECTION_ESTABLISHED_DOMAIN;
            } else {
                return PLAINIPSEC_CONNECTION_NOTESTABLISHED_DOMAIN;
            }
        }
	}
	return PLAINIPSECDOMAIN;
}

const char *
ipsecSessionGetConnectionLessDomain (ike_session_t *session)
{
	if (session) {
		if (session->is_cisco_ipsec) {
            return CISCOIPSECVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
		} else if (session->is_l2tpvpn_ipsec) {
            return L2TPIPSECVPN_CONNECTION_NOTESTABLISHED_DOMAIN;
		} else if (session->is_btmm_ipsec) {
            return BTMMIPSEC_CONNECTION_NOTESTABLISHED_DOMAIN;
		} else {
            return PLAINIPSEC_CONNECTION_NOTESTABLISHED_DOMAIN;
        }
	}
	return PLAINIPSECDOMAIN;
}

const char *
ipsecSessionGetPhaseDomain (ike_session_t *session)
{
	if (session) {
		if (session->is_cisco_ipsec) {
			return CISCOIPSECVPN_PHASE_DOMAIN;
		} else if (session->is_l2tpvpn_ipsec) {
			return L2TPIPSECVPN_PHASE_DOMAIN;
		} else if (session->is_btmm_ipsec) {
			return BTMMIPSEC_PHASE_DOMAIN;
		}
	}
	return PLAINIPSEC_PHASE_DOMAIN;
}

static
void
ipsecSessionLogEvent (ike_session_t *session, const char *event_msg)
{
	aslmsg m;

	if (!event_msg) {
		return;
	}

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, ASL_KEY_FACILITY, ipsecSessionGetPhaseDomain(session));
	asl_set(m, ASL_KEY_MSG, ipsecSessionString);
	asl_log(NULL, m, ASL_LEVEL_NOTICE, "%s", event_msg);
	asl_free(m);
}

void
ipsecSessionTracerStart (ike_session_t *session)
{
	if (session == NULL) {
		return;
	}
	bzero(&session->stats, sizeof(session->stats));
	bzero(&session->stop_timestamp, sizeof(session->stop_timestamp));
	bzero(&session->estab_timestamp, sizeof(session->estab_timestamp));
	gettimeofday(&session->start_timestamp, NULL);
	ipsecSessionLogEvent(session, CONSTSTR("Connecting."));
}

void
ipsecSessionTracerEvent (ike_session_t *session, ipsecSessionEventCode_t eventCode, const char *event, const char *failure_reason)
{
	char buf[1024];

	if (session == NULL) {
		//ipsecSessionLogEvent(session, CONSTSTR("tracer failed. (Invalid session)."));
		return;
	}
	if (eventCode <= IPSECSESSIONEVENTCODE_NONE || eventCode >= IPSECSESSIONEVENTCODE_MAX) {
		ipsecSessionLogEvent(session, CONSTSTR("tracer failed. (Invalid event code)."));
		return;
	}
	if (event == NULL) {
		ipsecSessionLogEvent(session, CONSTSTR("tracer failed. (Invalid event)."));
		return;
	}

	if (failure_reason) {
		if (!session->term_reason &&
            !ipsecSessionEventIgnoreReason[eventCode]) {
			session->term_reason = (char*)failure_reason;
		}
	}

	session->stats.counters[eventCode]++;
	buf[0] = (char)0;
	snprintf(buf, sizeof(buf), "%s. (%s).", ipsecSessionEventCodeToString(eventCode), event);
	ipsecSessionLogEvent(session, CONSTSTR(buf));
}

static void
ipsecSessionTracerLogFailureRate (ike_session_t *session, const char *signature, double failure_rate)
{
	aslmsg		m;
	char		buf[128];
	const char *domain = ipsecSessionGetPhaseDomain(session);

	if (!signature || failure_rate <= 0.001) {
		return;
	}

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, "com.apple.message.domain", domain);
	asl_set(m, ASL_KEY_FACILITY, domain);
	asl_set(m, ASL_KEY_MSG, ipsecSessionString);
    asl_set(m, "com.apple.message.result", "noop");
	asl_set(m, "com.apple.message.signature", signature);
	snprintf(buf, sizeof(buf), "%.3f", failure_rate);
	asl_set(m, "com.apple.message.value", buf);	// stuff the up time into value
	asl_log(NULL, m, ASL_LEVEL_NOTICE, "%s. (Failure-Rate = %s).", signature, buf);
	asl_free(m);
}

static void
ipsecSessionTracerLogStop (ike_session_t *session, int caused_by_failure, const char *reason)
{
	aslmsg      m;
	char        nat_buf[128];
	char        buf[128];
	const char *domain = (session->established)? ipsecSessionGetConnectionDomain(session) : ipsecSessionGetConnectionLessDomain(session);

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, "com.apple.message.domain", domain);
	asl_set(m, ASL_KEY_FACILITY, domain);
	asl_set(m, ASL_KEY_MSG, ipsecSessionString);
    if (caused_by_failure ||
        (reason && reason != ike_session_stopped_by_flush && reason != ike_session_stopped_by_vpn_disconnect)) {
        asl_set(m, "com.apple.message.result", CONSTSTR("failure"));	// failure
    } else {
        asl_set(m, "com.apple.message.result", CONSTSTR("success"));	// success
    }
	if (reason) {
        if (session->natt_flags & NAT_DETECTED_ME) {
            snprintf(nat_buf, sizeof(nat_buf), "%s. NAT detected by Me", reason);
            asl_set(m, "com.apple.message.signature", nat_buf);
        } else if (session->natt_flags & NAT_DETECTED_PEER) {
            snprintf(nat_buf, sizeof(nat_buf), "%s. NAT detected by Peer", reason);
            asl_set(m, "com.apple.message.signature", nat_buf);
        } else {
            asl_set(m, "com.apple.message.signature", reason);
        }
	} else {
		// reason was NULL; make sure success/failure have different signature
		if (caused_by_failure) {
			asl_set(m, "com.apple.message.signature", CONSTSTR("Internal/Server-side error"));
		} else {
			asl_set(m, "com.apple.message.signature", CONSTSTR("User/System initiated the disconnect"));
		}
	}
	if (session->established) {
		snprintf(buf, sizeof(buf), "%8.6f", timedelta(&session->estab_timestamp, &session->stop_timestamp));
		asl_set(m, "com.apple.message.value", buf);	// stuff the up time into value
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "Disconnecting. (Connection was up for, %s seconds).", buf);
	} else {
		snprintf(buf, sizeof(buf), "%8.6f", timedelta(&session->start_timestamp, &session->stop_timestamp));
		asl_set(m, "com.apple.message.value2", buf);	/// stuff the negoing time into value2
		asl_log(NULL, m, ASL_LEVEL_NOTICE, "Disconnecting. (Connection tried to negotiate for, %s seconds).", buf);
	}
	asl_free(m);
}

void
ipsecSessionTracerStop (ike_session_t *session, int caused_by_failure, const char *reason)
{
	if (session == NULL) {
		return;
	}

	gettimeofday(&session->stop_timestamp, NULL);

	ipsecSessionTracerLogStop(session, caused_by_failure, reason);

	// go thru counters logging failure-rate events
	if (session->stats.counters[IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL]) {
		ipsecSessionTracerLogFailureRate(session,
										 CONSTSTR("IKE Packets Transmit Failure-Rate Statistic"),
										 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKE_PACKET_TX_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKE_PACKET_TX_SUCC]));
	}
	if (session->stats.counters[IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL]) {
		ipsecSessionTracerLogFailureRate(session,
										 CONSTSTR("IKE Packets Receive Failure-Rate Statistic"),
										 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKE_PACKET_RX_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKE_PACKET_RX_SUCC]));
	}
	//if (session->version == IKE_VERSION_1) {
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_MAX_RETRANSMIT] ||
			session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_INIT_FAIL] ||
			session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_RESP_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Phase 1 Failure-Rate Statistic"),
											 get_percentage((double)(session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_MAX_RETRANSMIT] +
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_INIT_FAIL] + 
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_RESP_FAIL]),
                                                            (double)(session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_INIT_SUCC] +
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_RESP_SUCC])));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_INIT_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Phase 1 Initiator Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_INIT_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_INIT_SUCC]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_RESP_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Phase 1 Responder Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_RESP_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_RESP_SUCC]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Phase 1 Authentication Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH1_AUTH_SUCC]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_MAX_RETRANSMIT]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Dead-Peer-Detection Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_MAX_RETRANSMIT],
                                                            (double)(session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_MAX_RETRANSMIT] +
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_INIT_REQ])));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_INIT_RETRANSMIT] ||
			session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_RESP_RETRANSMIT]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Dead-Peer-Detect Retransmit-Rate Statistic"),
											 get_percentage((double)(session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_INIT_RETRANSMIT] +
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_RESP_RETRANSMIT]),
                                                            (double)(session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_INIT_REQ] +
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_DPD_RESP_REQ])));
		}		
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_MODECFG_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE MODE-Config Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_MODECFG_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_MODECFG_SUCC]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_XAUTH_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE XAUTH Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_XAUTH_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_XAUTH_SUCC]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_MAX_RETRANSMIT] ||
			session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_FAIL] ||
			session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_RESP_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Phase 2 Failure-Rate Statistic"),
											 get_percentage((double)(session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_MAX_RETRANSMIT] +
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_FAIL] + 
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_RESP_FAIL]),
                                                            (double)(session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_SUCC] +
                                                                     session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_RESP_FAIL])));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Phase 2 Initiator Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_SUCC]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_RESP_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Phase 2 Responder Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_RESP_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_RESP_SUCC]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_AUTH_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Phase 2 Authentication Failure-Rate Statistics"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_AUTH_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_PH2_AUTH_SUCC]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_INFO_NOTICE_TX_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Information-Notice Transmit Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_INFO_NOTICE_TX_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_INFO_NOTICE_TX_FAIL]));
		}
		if (session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_INFO_NOTICE_RX_FAIL]) {
			ipsecSessionTracerLogFailureRate(session,
											 CONSTSTR("IKE Information-Notice Receive Failure-Rate Statistic"),
											 get_percentage((double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_INFO_NOTICE_RX_FAIL], (double)session->stats.counters[IPSECSESSIONEVENTCODE_IKEV1_INFO_NOTICE_RX_SUCC]));
		}
	//}
}

void
ipsecSessionTracerLogEstablished (ike_session_t *session)
{
	aslmsg      m;
	const char *domain = ipsecSessionGetConnectionLessDomain(session);

	m = asl_new(ASL_TYPE_MSG);
	asl_set(m, "com.apple.message.domain", domain);
	asl_set(m, ASL_KEY_FACILITY, domain);
	asl_set(m, ASL_KEY_MSG, ipsecSessionString);
    asl_set(m, "com.apple.message.result", "success");	// success
    asl_set(m, "com.apple.message.signature", "success");
    asl_log(NULL, m, ASL_LEVEL_NOTICE, "Connected.");
	asl_free(m);
}
