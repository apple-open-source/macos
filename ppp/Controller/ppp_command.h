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

#ifndef __PPP_COMMAND_H__
#define __PPP_COMMAND_H__

u_long ppp_version (u_short id, struct msg *msg);
u_long ppp_status (u_short id, struct msg *msg);

u_long ppp_connect (u_short id, struct msg *msg);
u_long ppp_disconnect (u_short id, struct msg *msg);
u_long ppp_listen (u_short id, struct msg *msg);

u_long ppp_enable_event (u_short id, struct msg *msg);
u_long ppp_disable_event (u_short id, struct msg *msg);

u_long ppp_getnblinks (u_short id, struct msg *msg);
u_long ppp_getlinkbyindex (u_short id, struct msg *msg);

u_long ppp_apply(u_short id, struct msg *msg);

u_long ppp_openfd(u_short id, struct msg *msg);
u_long ppp_closefd(u_short id, struct msg *msg);
u_long ppp_writefd(u_short id, struct msg *msg);
u_long ppp_readfd(u_short id, struct msg *msg);
void notifyreaders (u_long link, struct msg *msg);
u_long findreader (u_long link);
u_long ppp_closeclientfd(u_long link);


#endif