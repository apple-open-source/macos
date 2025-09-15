/*
 * Copyright (c) 2015, 2016 Apple Inc. All rights reserved.
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

#ifndef CONTROLLER_H
#define CONTROLLER_H

#import "configAgent.h"
#import "dnsAgent.h"
#import "proxyAgent.h"

@interface AgentController : NWNetworkAgentRegistration

// A reduced set of SVCB Service Parameters that are
// supported for DNR
typedef enum
{
	dnr_svcb_key_alpn = 1,
	dnr_svcb_key_port = 3,
	dnr_svcb_key_doh_path = 7,
} dnr_svc_params_key;

typedef bool (^_dnr_access_svc_param_value_block_t)(const void *value, size_t value_size);
typedef bool (^_dnr_access_svc_param_alpn_t)(const char *alpn);

@property (readwrite) dispatch_queue_t	controllerQueue;

- (void)processProxyChanges;
- (void)processDNSChanges;
+ (AgentController *)sharedController;
- (const void *)copyProxyAgentData:(uuid_t)requested_uuid length:(uint64_t *)length;
- (const void *)copyDNSAgentData:(uuid_t)requested_uuid length:(uint64_t *)length;

@end

#endif /* CONTROLLER_H */
