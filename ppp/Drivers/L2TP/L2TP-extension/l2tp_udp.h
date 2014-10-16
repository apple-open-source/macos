/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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


#ifndef __L2TP_UDP_H__
#define __L2TP_UDP_H__


int l2tp_udp_init();
int l2tp_udp_dispose();
int l2tp_udp_attach(socket_t *so, struct sockaddr *addr, int *thread, int nocksum, int delegated_process);
int l2tp_udp_detach(socket_t so, int thread);
int l2tp_udp_setpeer(socket_t so, struct sockaddr *addr);
int l2tp_udp_output(socket_t so, int thread, mbuf_t m, struct sockaddr* to);
void l2tp_udp_input(socket_t so, void *arg, int waitflag);
void l2tp_udp_clear_INP_INADDR_ANY(socket_t so);

#endif