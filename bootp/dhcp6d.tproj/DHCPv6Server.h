/*
 * Copyright (c) 2018 Apple Inc. All rights reserved.
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
 * DHCPv6Server.h
 * - stateless DHCPv6 server
 */
/* 
 * Modification History
 *
 * August 28, 2018		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _S_DHCPV6SERVER_H
#define _S_DHCPV6SERVER_H

#include <stdint.h>
#include "DHCPv6.h"

typedef struct DHCPv6Server * DHCPv6ServerRef;

void
DHCPv6ServerSetVerbose(bool verbose);

void
DHCPv6ServerSetPorts(uint16_t client_port, uint16_t server_port);

DHCPv6ServerRef
DHCPv6ServerCreate(const char * config_file);

void
DHCPv6ServerUpdateConfiguration(DHCPv6ServerRef server);

#endif /* _S_DHCPV6SERVER_H */
