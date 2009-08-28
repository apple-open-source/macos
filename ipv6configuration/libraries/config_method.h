#ifndef _CONFIG_METHOD_H_
#define _CONFIG_METHOD_H_

/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
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

#include "configthreads_common.h"

ip6config_status_t	config_method_start(Service_t * service_p, ip6config_method_t method,
			ip6config_method_data_t * data);
ip6config_status_t	config_method_state_change(Service_t * service_p,
			ip6_addrinfo_list_t * ip6_addrs);
ip6config_status_t	config_method_change(Service_t * service_p, ip6config_method_t method,
			ip6config_method_data_t * data, boolean_t * needs_stop);
ip6config_status_t	config_method_ipv4_primary_change(Service_t * service_p,
			ip6config_method_t method, ip6config_method_data_t * data);
ip6config_status_t	config_method_stop(Service_t * service_p);
ip6config_status_t	config_method_media(Service_t * service_p);
int			ip6config_get_6to4_address_data(SCDynamicStoreRef session,
			ip6config_method_data_t * method_data);
ip6config_method_data_t* ip6config_method_data_from_dict(CFDictionaryRef dict,
			ip6config_method_t * method);
int			ip6config_address_data_from_state(CFDictionaryRef dict,
			ip6_addrinfo_list_t * ip6_addrs);

#endif /* _CONFIG_METHOD_H_ */
