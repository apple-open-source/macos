/*
 * Copyright (c) 2004, 2005, 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef _S_DNSINFO_SERVER_H
#define _S_DNSINFO_SERVER_H

#include <sys/cdefs.h>
#include <mach/mach.h>
#include <CoreFoundation/CoreFoundation.h>

#include "shared_dns_info_types.h"

__BEGIN_DECLS

__private_extern__
kern_return_t	_shared_dns_infoGet	(mach_port_t		server,
					 dnsDataOut_t		*dataRef,
					 mach_msg_type_number_t	*dataLen);

__private_extern__
kern_return_t	_shared_dns_infoSet	(mach_port_t		server,
					 dnsData_t		dataRef,
					 mach_msg_type_number_t	dataLen,
					 audit_token_t		audit_token);

__private_extern__
kern_return_t	_shared_nwi_stateGet	(mach_port_t		server,
					 dnsDataOut_t		*dataRef,
					 mach_msg_type_number_t	*dataLen);

__private_extern__
kern_return_t	_shared_nwi_stateSet	(mach_port_t		server,
					 dnsData_t		dataRef,
					 mach_msg_type_number_t	dataLen,
					 audit_token_t		audit_token);

__END_DECLS

#endif	/* !_S_DNSINFO_SERVER_H */
