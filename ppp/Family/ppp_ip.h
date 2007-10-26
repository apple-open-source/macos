/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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


#ifndef __PPP_IP_H__
#define __PPP_IP_H__


int ppp_ip_init(int init_arg);
int ppp_ip_dispose(int term_arg);

errno_t ppp_ip_attach(ifnet_t ifp, protocol_family_t protocol_family);
void ppp_ip_detach(ifnet_t ifp, protocol_family_t protocol_family);

int ppp_ip_af_src_out(ifnet_t ifp, char *pkt);
int ppp_ip_af_src_in(ifnet_t ifp, char *pkt);

int ppp_ip_bootp_server_in(ifnet_t ifp, char *pkt);
int ppp_ip_bootp_client_in(ifnet_t ifp, char *pkt);

#endif
