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



int link_init();
int link_connect(struct ppp *ppp, u_char afterbusy);
int link_disconnect(struct ppp *ppp);
int link_listen(struct ppp *ppp);
int link_abort(struct ppp *ppp);
int link_event();
int link_getcaps(struct ppp *ppp);
void link_serial_up(struct ppp *ppp);
void link_serial_down(struct ppp *ppp);
int link_setloopback(struct ppp *ppp, u_int32_t mode);
int link_getloopback(struct ppp *ppp, u_int32_t *mode);
int link_setnblinks(struct ppp *ppp, u_int32_t nb);
int link_getnblinks(struct ppp *ppp, u_int32_t *nb);
int link_setconfig (struct ppp *ppp);
int link_attach(struct ppp *ppp);
int link_detach(struct ppp *ppp);
