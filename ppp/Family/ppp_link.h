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


#ifndef _PPP_LINK_H_
#define _PPP_LINK_H_


int ppp_link_init();
int ppp_link_dispose();
int ppp_link_control(struct ppp_link *link, u_int32_t cmd, void *data);
int ppp_link_attachclient(u_short index, void *host, struct ppp_link **link);
int ppp_link_detachclient(struct ppp_link *link, void *host);
int ppp_link_send(struct ppp_link *link, struct mbuf *m);


#endif /* _PPP_LINK_H_ */
