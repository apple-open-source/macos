/*
 * Copyright (c) 2004, 2005, 2007, 2009 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * March 9, 2004		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include "dnsinfo_private.h"
#include "shared_dns_info_types.h"


__private_extern__
const char *
_dns_configuration_notify_key()
{
	return "com.apple.system.SystemConfiguration.dns_configuration";
}


__private_extern__
mach_port_t
_dns_configuration_server_port()
{
	mach_port_t	server		= MACH_PORT_NULL;
	char		*server_name;
	kern_return_t	status;

	server_name = getenv("DNS_SERVER");
	if (!server_name) {
		server_name = DNS_SERVER;
	}

	status = bootstrap_look_up(bootstrap_port, server_name, &server);
	switch (status) {
		case BOOTSTRAP_SUCCESS :
			/* service currently registered, "a good thing" (tm) */
			break;
		case BOOTSTRAP_UNKNOWN_SERVICE :
			/* service not currently registered, try again later */
			return MACH_PORT_NULL;
		default :
			fprintf(stderr,
				"could not lookup DNS configuration info service: %s\n",
				bootstrap_strerror(status));
			return MACH_PORT_NULL;
	}

	return server;
}

