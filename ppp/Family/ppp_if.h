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

#ifndef _PPP_IF_H_
#define _PPP_IF_H_


int ppp_if_init();
int ppp_if_dispose();
int ppp_if_attach(u_short *unit);
int ppp_if_attachclient(u_short unit, void *host, struct ifnet **ifp);
void ppp_if_detachclient(struct ifnet *ifp, void *host);

int ppp_if_input(struct ifnet *ifp, struct mbuf *m, u_int16_t proto, u_int16_t hdrlen);
int ppp_if_control(struct ifnet *ifp, u_long cmd, void *data);
int ppp_if_attachlink(struct ppp_link *link, int unit);
int ppp_if_detachlink(struct ppp_link *link);
int ppp_if_send(struct ifnet *ifp, struct mbuf *m);
void ppp_if_linkerror(struct ppp_link *link);



#define APPLE_PPP_NAME	"ppp"



#endif /* _PPP_IF_H_ */
