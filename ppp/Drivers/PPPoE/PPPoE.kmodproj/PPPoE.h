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


#ifndef __PPPOE_H__
#define __PPPOE_H__

#include <sys/ioccom.h>
#include <net/ethernet.h>

#define PPPOE_ETHERTYPE_CTRL 	0x8863
#define PPPOE_ETHERTYPE_DATA 	0x8864

#define PF_PPPOE 		247		/* TEMP - move to socket.h */
#define AF_PPPOE 		PF_PPPOE	/* */

#define PPPOEPROTO_PPPOE	1		/* */

#define PPPOE_NAME		"PPPoE"		/* */

#define PPPOE_AC_NAME_LEN	64
#define PPPOE_SERVICE_LEN	64

struct sockaddr_pppoe
{
    unsigned char	pppoe_len;				/* sizeof(struct sockaddr_pppoe) */
    unsigned char	pppoe_family;				/* AF_PPPOE */
    unsigned char 	pppoe_ac_name[PPPOE_AC_NAME_LEN];	/* Access Concentrator name */
    unsigned char 	pppoe_service[PPPOE_SERVICE_LEN];	/* Service name */
    unsigned char 	pppoe_mac_addr[ETHER_ADDR_LEN];		/* Ethernet mac address */
};


#define IOC_PPPOE_SETLOOPBACK	_IOW('P', 1, u_int32_t)

#endif
