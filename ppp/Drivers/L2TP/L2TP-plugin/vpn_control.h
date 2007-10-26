/* $Id: vpn_control.h,v 1.10 2004/12/30 13:45:49 manubsd Exp $ */

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
#define VPNCTL_STATUS_IKE_FAILED		0x8001
#define VPNCTL_STATUS_PH1_START_US		0x8011
#define VPNCTL_STATUS_PH1_START_PEER	0x8012
#define VPNCTL_STATUS_PH1_ESTABLISHED	0x8013
#define VPNCTL_STATUS_PH2_START			0x8021
#define VPNCTL_STATUS_PH2_ESTABLISHED	0x8022

/*
 * Status codes
 *
 * VPNCTL_STATUS_PH1_START_US: 1st IKE packet sent by us
 * VPNCTL_STATUS_PH1_START_PEER: 1st IKE packet received from peer
 * VPNCTL_STATUS_PH1_ESTABLISED: phase 1 established
 * VPNCTL_STATUS_PH2_BEGIN: phase 2 start
 * VPNCTL_STATUS_PH2_ESTABLISED: phase 2 established
 * VPNCTL_STATUS_IKE_FAILED: notification codes defined in isakmp.h
 */

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
};

/* unbind to stop receiving status for specified address */
struct vpnctl_cmd_unbind {
	struct vpnctl_hdr		hdr;
	u_int32_t				address;	/* 0xFFFFFFFF = all */
};

/* redirect client to specified address */
struct vpnctl_cmd_redirect {
	struct vpnctl_hdr		hdr;
	u_int32_t				address;
	u_int32_t				redirect_address;
	u_int16_t				force;
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
#define VPNCTL_NTYPE_INTERNAL_ERROR				-1


/* packet format for phase change status */
struct vpnctl_status_phase_change {
	struct vpnctl_hdr			hdr;
	u_int32_t					address;
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
