/* $Id: vpn_control.h,v 1.10 2004/12/30 13:45:49 manubsd Exp $ */

/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _VPN_CONTROL_H
#define _VPN_CONTROL_H

#include "algorithm_types.h"
#include <net/if.h>

#define VPNCONTROLSOCK_PATH ADMINPORTDIR "/vpncontrol.sock"

#define FROM_LOCAL	0
#define FROM_REMOTE 1


extern char *vpncontrolsock_path;
extern uid_t vpncontrolsock_owner;
extern gid_t vpncontrolsock_group;
extern mode_t vpncontrolsock_mode;


/*
 * message types
 */
#define VPNCTL_CMD_BIND					0x0001
#define VPNCTL_CMD_UNBIND				0x0002
#define VPNCTL_CMD_REDIRECT				0x0003
#define VPNCTL_CMD_PING					0x0004
#define VPNCTL_CMD_CONNECT				0x0011
#define VPNCTL_CMD_DISCONNECT			0x0012
#define VPNCTL_CMD_START_PH2			0x0013
#define VPNCTL_CMD_XAUTH_INFO			0x0014
#define VPNCTL_CMD_START_DPD            0x0015
#define VPNCTL_STATUS_IKE_FAILED		0x8001
#define VPNCTL_STATUS_PH1_START_US		0x8011
#define VPNCTL_STATUS_PH1_START_PEER	0x8012
#define VPNCTL_STATUS_PH1_ESTABLISHED	0x8013
#define VPNCTL_STATUS_PH2_START			0x8021
#define VPNCTL_STATUS_PH2_ESTABLISHED	0x8022
#define VPNCTL_STATUS_NEED_AUTHINFO		0x8101
#define VPNCTL_STATUS_NEED_REAUTHINFO	0x8102

/*
 * Flags
 */
#define VPNCTL_FLAG_MODECFG_USED		0x0001

/*
 * XAUTH Attribute Types
 */
#ifndef __IPSEC_BUILD__
#define	XAUTH_TYPE                16520
#define	XAUTH_USER_NAME           16521
#define	XAUTH_USER_PASSWORD       16522
#define	XAUTH_PASSCODE            16523
#define	XAUTH_MESSAGE             16524
#define	XAUTH_CHALLENGE           16525
#define	XAUTH_DOMAIN              16526
#define	XAUTH_STATUS              16527
#define	XAUTH_NEXT_PIN            16528
#define	XAUTH_ANSWER              16529


/* Types for XAUTH_TYPE */
#define	XAUTH_TYPE_GENERIC 	0
#define	XAUTH_TYPE_CHAP    	1
#define	XAUTH_TYPE_OTP     	2
#define	XAUTH_TYPE_SKEY    	3


/* Mode cfg Attribute types */
#define INTERNAL_IP4_ADDRESS        1
#define INTERNAL_IP4_NETMASK        2
#define INTERNAL_IP4_DNS            3
#define INTERNAL_IP4_NBNS           4
#define INTERNAL_ADDRESS_EXPIRY     5
#define INTERNAL_IP4_DHCP           6
#define APPLICATION_VERSION         7
#define INTERNAL_IP6_ADDRESS        8
#define INTERNAL_IP6_NETMASK        9
#define INTERNAL_IP6_DNS           10
#define INTERNAL_IP6_NBNS          11
#define INTERNAL_IP6_DHCP          12
#define INTERNAL_IP4_SUBNET        13
#define SUPPORTED_ATTRIBUTES       14
#define INTERNAL_IP6_SUBNET        15


/* 3.3 Data Attributes
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 !A!       Attribute Type        !    AF=0  Attribute Length     !
 !F!                             !    AF=1  Attribute Value      !
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 .                   AF=0  Attribute Value                       .
 .                   AF=1  Not Transmitted                       .
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
struct isakmp_data {
	u_int16_t type;		/* defined by DOI-spec, and Attribute Format */
	u_int16_t lorv;		/* if f equal 1, Attribute Length */
	/* if f equal 0, Attribute Value */
	/* if f equal 1, Attribute Value */
};
#endif

/* commands and status for vpn control. */
/* network byte order. */

/* Packet header */
struct vpnctl_hdr {
	u_int16_t			msg_type;
	u_int16_t			flags;		
	u_int32_t			cookie;
	u_int32_t			reserved;
	u_int16_t			result;	
	u_int16_t			len;			/* payload length */	
};

/* Packet formats for commands */

/* bind to receive status for specified address */
struct vpnctl_cmd_bind {
	struct vpnctl_hdr		hdr;
	u_int32_t				address;	/* 0xFFFFFFFF = all */
	u_int16_t				vers_len;	/* if zero - no version provided */
	/* name/version string of length vers_len */
};

/* unbind to stop receiving status for specified address */
struct vpnctl_cmd_unbind {
	struct vpnctl_hdr		hdr;
	u_int32_t				address;	/* 0xFFFFFFFF = all */
};


/* connect to specified address */
struct vpnctl_cmd_connect{
	struct vpnctl_hdr		hdr;
	u_int32_t				address;
};

struct vpnctl_sa_selector {
	u_int32_t		src_tunnel_address;
	u_int32_t		src_tunnel_mask;
	u_int32_t		dst_tunnel_address;
	u_int32_t		dst_tunnel_mask;
	u_int16_t		src_tunnel_port;
	u_int16_t		dst_tunnel_port;
	u_int16_t		ul_protocol;
	u_int16_t		reserved;
};

struct vpnctl_algo {
	u_int16_t	algo_class;
	u_int16_t	algo;
	u_int16_t	key_len;	/* for enc algorithms only */
	u_int16_t	reserved;
};

/* start phase 2 */
struct vpnctl_cmd_start_ph2 {
	struct vpnctl_hdr		hdr;
	u_int32_t				address;
	u_int32_t				lifetime;  /* seconds */
	u_int16_t				pfs_group;	/* defined in algorithm_types.h */
	u_int16_t				selector_count;
	u_int16_t				algo_count;
	u_int16_t				reserved;
	/* array of struct vpnctl_sa_selector */
	/* array of struct vpnctl_algo */
};

/* set xauth info */
struct vpnctl_cmd_xauth_info {	
	struct vpnctl_hdr		hdr;
	u_int32_t				address;
	/* packed array of variable sized struct isakmp_data */
};

/* redirect client to specified address */
struct vpnctl_cmd_redirect {	
 	struct vpnctl_hdr		hdr;
 	u_int32_t				address;
	u_int32_t				redirect_address;
	u_int16_t				force;
};

/* start dpd */
struct vpnctl_cmd_start_dpd {
	struct vpnctl_hdr		hdr;
	u_int32_t               address;
};

/*
 * IKE Notify codes - mirrors codes in isakmp.h
 */
#define VPNCTL_NTYPE_INVALID_PAYLOAD_TYPE		1
#define VPNCTL_NTYPE_DOI_NOT_SUPPORTED			2
#define VPNCTL_NTYPE_SITUATION_NOT_SUPPORTED	3
#define VPNCTL_NTYPE_INVALID_COOKIE				4
#define VPNCTL_NTYPE_INVALID_MAJOR_VERSION		5
#define VPNCTL_NTYPE_INVALID_MINOR_VERSION		6
#define VPNCTL_NTYPE_INVALID_EXCHANGE_TYPE		7
#define VPNCTL_NTYPE_INVALID_FLAGS				8
#define VPNCTL_NTYPE_INVALID_MESSAGE_ID			9
#define VPNCTL_NTYPE_INVALID_PROTOCOL_ID		10
#define VPNCTL_NTYPE_INVALID_SPI				11
#define VPNCTL_NTYPE_INVALID_TRANSFORM_ID		12
#define VPNCTL_NTYPE_ATTRIBUTES_NOT_SUPPORTED	13
#define VPNCTL_NTYPE_NO_PROPOSAL_CHOSEN			14
#define VPNCTL_NTYPE_BAD_PROPOSAL_SYNTAX		15
#define VPNCTL_NTYPE_PAYLOAD_MALFORMED			16
#define VPNCTL_NTYPE_INVALID_KEY_INFORMATION	17
#define VPNCTL_NTYPE_INVALID_ID_INFORMATION		18
#define VPNCTL_NTYPE_INVALID_CERT_ENCODING		19
#define VPNCTL_NTYPE_INVALID_CERTIFICATE		20
#define VPNCTL_NTYPE_BAD_CERT_REQUEST_SYNTAX	21
#define VPNCTL_NTYPE_INVALID_CERT_AUTHORITY		22
#define VPNCTL_NTYPE_INVALID_HASH_INFORMATION	23
#define VPNCTL_NTYPE_AUTHENTICATION_FAILED		24
#define VPNCTL_NTYPE_INVALID_SIGNATURE			25
#define VPNCTL_NTYPE_ADDRESS_NOTIFICATION		26
#define VPNCTL_NTYPE_NOTIFY_SA_LIFETIME			27
#define VPNCTL_NTYPE_CERTIFICATE_UNAVAILABLE	28
#define VPNCTL_NTYPE_UNSUPPORTED_EXCHANGE_TYPE	29
#define VPNCTL_NTYPE_UNEQUAL_PAYLOAD_LENGTHS	30
#define VPNCTL_NTYPE_LOAD_BALANCE				40501
#define VPNCTL_NTYPE_PEER_DEAD					50001	/* detected by DPD */
#define VPNCTL_NTYPE_PH1_DELETE					50002	/* received a delete payload leaving no PH1 SA for the remote address */
#define VPNCTL_NTYPE_IDLE_TIMEOUT				50003	/* idle timeout */
#define VPNCTL_NTYPE_INTERNAL_ERROR				-1


/* packet format for phase change status */
struct vpnctl_status_phase_change {
	struct vpnctl_hdr			hdr;
	u_int32_t					address;
	/* The following is included when VPNCTL_FLAG_MODECFG_USED flag set */
	// struct vpnctl_modecfg_params	mode_cfg;
	
};


/* packet format for auth needed status */
struct vpnctl_status_need_authinfo {
	struct vpnctl_hdr			hdr;
	u_int32_t					address;
	/* packed array of variable sized struct isakmp_data */
};


struct split_address {
	u_int32_t	splitaddr;
	u_int32_t	splitmask;
};

struct vpnctl_modecfg_params {	
	u_int32_t					outer_local_addr;
	u_int16_t					outer_remote_port;
	u_int16_t					outer_local_port;
	u_int8_t					ifname[IFNAMSIZ];
	/*
	 *	ifname for outer_local_addr (not null terminated)
	 *	followed by packed array of attributes (struct isakmp_data)
	 */
};


/* Packet formats for failed status */
struct vpnctl_status_failed {
	struct vpnctl_hdr			hdr;
	u_int32_t					address;
	u_int16_t					ike_code;
	u_int16_t					from;
	u_int8_t					data[0];
};


#endif /* _VPN_CONTROL_H */
