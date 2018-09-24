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

#include <nelog.h>

#define CONSTSTR(str) (const char *)str

#define PPP_CONNECTION_ESTABLISHED_DOMAIN									CONSTSTR("com.apple.Networking.ppp.disconnect")
#define PPP_CONNECTION_NOTESTABLISHED_DOMAIN								CONSTSTR("com.apple.Networking.ppp.connect")

#define PPPOENONVPN															CONSTSTR("pppoe")
#define PPPSERIALNONVPN														CONSTSTR("pppserial")
#define PLAINPPPNONVPN														CONSTSTR("ppp")

#define IPSECLOGASLMSG(format, args...) os_log(ne_log_obj(), format, ##args);

#if TARGET_OS_EMBEDDED
#define SESSIONTRACERSTOP(service)                                      {service->connecttime = 0; service->establishtime = 0;}
#define SESSIONTRACERESTABLISHED(service)                               {if (!service->establishtime)                                           \
                                                                            service->establishtime = mach_absolute_time() * gTimeScaleSeconds;}
#else
#define SESSIONTRACERSTOP(service)                                      sessionTracerStop(service)
#define SESSIONTRACERESTABLISHED(service)                               sessionTracerLogEstablished(service)
#endif

#endif /* _MESSAGETRACER_H */
