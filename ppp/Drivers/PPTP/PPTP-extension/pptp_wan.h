/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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


#ifndef __PPTP_WAN_H__
#define __PPTP_WAN_H__

int pptp_wan_init();
int pptp_wan_dispose();
int pptp_wan_attach(void *rfc, struct ppp_link **link);
void pptp_wan_detach(struct ppp_link *link);
int pptp_wan_input(struct ppp_link *link, struct mbuf *m);
void pptp_wan_xmit_full(struct ppp_link *link);
void pptp_wan_xmit_ok(struct ppp_link *link);
void pptp_wan_input_error(struct ppp_link *);


#endif