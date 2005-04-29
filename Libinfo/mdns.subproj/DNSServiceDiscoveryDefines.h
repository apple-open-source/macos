/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#ifndef __DNS_SERVICE_DISCOVERY_DEFINES_H
#define __DNS_SERVICE_DISCOVERY_DEFINES_H

#include <mach/mach_types.h>

#define DNS_SERVICE_DISCOVERY_SERVER "com.apple.mDNSResponder"

typedef char    DNSCString[1024];
typedef char    sockaddr_t[128];

typedef const char * record_data_t;
typedef struct { char bytes[4]; } IPPort;

#endif	/* __DNS_SERVICE_DISCOVERY_DEFINES_H */

