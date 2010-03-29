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

#ifndef _IPSECMESSAGETRACER_H
#define _IPSECMESSAGETRACER_H

#define CONSTSTR(str) (const char *)str

#define L2TPIPSECVPN_CONNECTION_ESTABLISHED_DOMAIN								CONSTSTR("com.apple.Networking.ipsec.disconnect.l2tpipsec")
#define CISCOIPSECVPN_CONNECTION_ESTABLISHED_DOMAIN								CONSTSTR("com.apple.Networking.ipsec.disconnect.ciscoipsec")
#define BTMMIPSEC_CONNECTION_ESTABLISHED_DOMAIN									CONSTSTR("com.apple.Networking.ipsec.disconnect.btmm")
#define PLAINIPSEC_CONNECTION_ESTABLISHED_DOMAIN                                CONSTSTR("com.apple.Networking.ipsec.disconnect.plain")
#define L2TPIPSECVPN_CONNECTION_NOTESTABLISHED_DOMAIN                           CONSTSTR("com.apple.Networking.ipsec.connect.l2tpipsec")
#define CISCOIPSECVPN_CONNECTION_NOTESTABLISHED_DOMAIN                          CONSTSTR("com.apple.Networking.ipsec.connect.ciscoipsec")
#define BTMMIPSEC_CONNECTION_NOTESTABLISHED_DOMAIN                              CONSTSTR("com.apple.Networking.ipsec.connect.btmm")
#define PLAINIPSEC_CONNECTION_NOTESTABLISHED_DOMAIN                             CONSTSTR("com.apple.Networking.ipsec.connect.plain")
#define L2TPIPSECVPN_PHASE_DOMAIN                                               CONSTSTR("com.apple.Networking.ipsec.phasestats.l2tpipsec")
#define CISCOIPSECVPN_PHASE_DOMAIN                                              CONSTSTR("com.apple.Networking.ipsec.phasestats.ciscoipsec")
#define BTMMIPSEC_PHASE_DOMAIN                                                  CONSTSTR("com.apple.Networking.ipsec.phasestats.btmm")
#define PLAINIPSEC_PHASE_DOMAIN                                                 CONSTSTR("com.apple.Networking.ipsec.phasestats.plain")
#define PLAINIPSECDOMAIN                                                        CONSTSTR("com.apple.Networking.ipsec.main")

#if TARGET_OS_EMBEDDED

#define IPSECCONFIGTRACEREVENT(config, eventCode, message, failure_reason)		

#define IPSECPOLICYTRACEREVENT(policy, eventCode, message, failure_reason)		

#define IPSECSESSIONTRACERSTART(session)										
#define IPSECSESSIONTRACEREVENT(session, eventCode, message, failure_reason)	
#define IPSECSESSIONTRACERSTOP(session, is_failure, reason)						
#define IPSECSESSIONTRACERESTABLISHED(session)                                  

#else

#define IPSECCONFIGTRACEREVENT(config, eventCode, message, failure_reason)		ipsecConfigTracerEvent(config, eventCode, message, failure_reason)

#define IPSECPOLICYTRACEREVENT(policy, eventCode, message, failure_reason)		ipsecPolicyTracerEvent(policy, eventCode, message, failure_reason)

#define IPSECSESSIONTRACERSTART(session)										ipsecSessionTracerStart(session)
#define IPSECSESSIONTRACEREVENT(session, eventCode, message, failure_reason)	ipsecSessionTracerEvent(session, eventCode, message, failure_reason)
#define IPSECSESSIONTRACERSTOP(session, is_failure, reason)						ipsecSessionTracerStop(session, is_failure, reason)
#define IPSECSESSIONTRACERESTABLISHED(session)                                  ipsecSessionTracerLogEstablished(session)

#endif

static inline double get_percentage (double numerator, double denominator)
{
    if (numerator >= denominator || denominator == 0) {
        return((double)100);
    }
    return((numerator/denominator)*100);
}

#endif /* _IPSECMESSAGETRACER_H */
