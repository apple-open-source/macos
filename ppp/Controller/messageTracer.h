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

#ifndef _MESSAGETRACER_H
#define _MESSAGETRACER_H

#import	 <asl.h>

#define CONSTSTR(str) (const char *)str

#define L2TPVPN_CONNECTION_ESTABLISHED_DOMAIN                           CONSTSTR("com.apple.Networking.vpn.disconnect.l2tpipsec")
#define PPTPVPN_CONNECTION_ESTABLISHED_DOMAIN                           CONSTSTR("com.apple.Networking.vpn.disconnect.pptp")
#define CISCOVPN_CONNECTION_ESTABLISHED_DOMAIN                          CONSTSTR("com.apple.Networking.vpn.disconnect.ciscoipsec")
#define PPPOEVPN_CONNECTION_ESTABLISHED_DOMAIN                          CONSTSTR("com.apple.Networking.vpn.disconnect.pppoe")
#define PPPSERIALVPN_CONNECTION_ESTABLISHED_DOMAIN                      CONSTSTR("com.apple.Networking.vpn.disconnect.pppserial")
#define PLAINPPPVPN_CONNECTION_ESTABLISHED_DOMAIN                       CONSTSTR("com.apple.Networking.vpn.disconnect.ppp")
#define L2TPVPN_CONNECTION_NOTESTABLISHED_DOMAIN                        CONSTSTR("com.apple.Networking.vpn.connect.l2tpipsec")
#define PPTPVPN_CONNECTION_NOTESTABLISHED_DOMAIN                        CONSTSTR("com.apple.Networking.vpn.connect.pptp")
#define CISCOVPN_CONNECTION_NOTESTABLISHED_DOMAIN                       CONSTSTR("com.apple.Networking.vpn.connect.ciscoipsec")
#define PPPOEVPN_CONNECTION_NOTESTABLISHED_DOMAIN                       CONSTSTR("com.apple.Networking.vpn.connect.pppoe")
#define PPPSERIALVPN_CONNECTION_NOTESTABLISHED_DOMAIN                   CONSTSTR("com.apple.Networking.vpn.connect.pppserial")
#define PLAINPPPVPN_CONNECTION_NOTESTABLISHED_DOMAIN                    CONSTSTR("com.apple.Networking.vpn.connect.ppp")

#define IPSECASLDOMAIN                                                  CONSTSTR("com.apple.Networking.vpn.asl.ipsec")
#define IPSECASLKEY                                                     CONSTSTR("IPSEC")

#if 1 //TARGET_OS_EMBEDDED
#define IPSECLOGASLMSG(format, args...) syslog(LOG_NOTICE, format, ##args);
#else
#define IPSECLOGASLMSG(format, args...) do {						       		\
						aslmsg m = asl_new(ASL_TYPE_MSG);			\
						asl_set(m, ASL_KEY_FACILITY, IPSECASLDOMAIN);		\
						asl_set(m, ASL_KEY_MSG, IPSECASLKEY);			\
						asl_log(NULL, m, ASL_LEVEL_NOTICE, format, ##args);	\
						asl_free(m);						\
					} while(0)
#endif

#if TARGET_OS_EMBEDDED
#define SESSIONTRACERSTOP(service)                                      
#define SESSIONTRACERESTABLISHED(service)                               
#else
#define SESSIONTRACERSTOP(service)                                      sessionTracerStop(service)
#define SESSIONTRACERESTABLISHED(service)                               sessionTracerLogEstablished(service)
#endif

#endif /* _MESSAGETRACER_H */
