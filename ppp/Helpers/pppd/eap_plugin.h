/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * eap_plugin.h - Extensible Authentication Protocol Plugin API.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: eap_plugin.h,v 1.4 2003/08/14 00:00:29 callie Exp $
 */

/* ----------------------------------------------------------------------
 IMPORTANT: EAP Plugin API is not stable.
            The API will change in the upcoming releases
---------------------------------------------------------------------- */

#ifndef __EAP_PLUGIN_INCLUDE__

#ifdef __cplusplus
extern "C" {
#endif

/* Code + ID + length */
#define EAP_HEADERLEN		4

/*
 * EAP codes.
 */
 
/* support for request types 1..4 is mandatory */
#define EAP_TYPE_IDENTITY	1	/* request for identity */
#define EAP_TYPE_NOTIFICATION	2	/* notification message */
#define EAP_TYPE_NAK		3	/* nak (response only) */
#define EAP_TYPE_MD5CHALLENGE	4	/* password MD5 coded */

#define EAP_TYPE_OTP		5	/* One Time Password (OTP) */
#define EAP_TYPE_TOKEN		6	/* Generic Token Card */

#define EAP_TYPE_RSA		9	/* RSA Public Key Authentication */
#define EAP_TYPE_DSS		10	/* DSS Unilateral */
#define EAP_TYPE_KEA		11	/* KEA */
#define EAP_TYPE_KEA_VALIDATE	12	/* KEA-VALIDATE */
#define EAP_TYPE_TLS		13	/* EAP-TLS */
#define EAP_TYPE_AXENT		14	/* Defender Token (AXENT) */
#define EAP_TYPE_RSA_SECURID	15	/* RSA Security SecurID EAP */
#define EAP_TYPE_ARCOT		16	/* Arcot Systems EAP */
#define EAP_TYPE_CISCO		17	/* EAP-Cisco Wireless */
#define EAP_TYPE_NOKIA		18	/* Nokia IP smart card authentication */
#define EAP_TYPE_SRP_SHA1_1	19	/* SRP-SHA1 Part 1 */
#define EAP_TYPE_SRP_SHA1_2	20	/* SRP-SHA1 Part 2 */
#define EAP_TYPE_TTLS		21	/* EAP-TTLS */
#define EAP_TYPE_RAS		22	/* Remote Access Service */
#define EAP_TYPE_UMTS		23	/* UMTS Authentication and Key Argreement */
#define EAP_TYPE_3COM		24	/* EAP-3Com Wireless */
#define EAP_TYPE_PEAP		25	/* PEAP */
#define EAP_TYPE_MS		26	/* MS-EAP-Authentication */
#define EAP_TYPE_MAKE		27	/* Mutual Authentication w/Key Exchange (MAKE) */
#define EAP_TYPE_CRYPTO		28	/* CRYPTOCard */
#define EAP_TYPE_MSCHAP_V2	29	/* EAP-MSCHAP-V2 */
#define EAP_TYPE_DYNAM_ID	30	/* DynamID */
#define EAP_TYPE_ROB		31	/* Rob EAP */
#define EAP_TYPE_SECUR_ID	32	/* SecurID EAP */
#define EAP_TYPE_MS_TLV		33	/* MS-Authentication-TLV  */
#define EAP_TYPE_SENTRINET	34	/* SentriNET */
#define EAP_TYPE_ACTIONTEC	35	/* EAP-Actiontec Wireless */
#define EAP_TYPE_COGENT		36	/* Cogent Systems Biometrics Authentication EAP */

  
#define EAP_REQUEST		1
#define EAP_RESPONSE		2
#define EAP_SUCCESS		3
#define EAP_FAILURE    		4


struct EAP_Packet
{
    u_int8_t    code;       	// packet type : 1 = Request, 2 = Response, 3 = Success, 4 = Failure
    u_int8_t    id;         	// packet id
    u_int16_t   len;  		// packet len (network order)
    u_int8_t    data[1];    	// packet data
};


#define EAP_NOTIFICATION_NONE 		0
#define EAP_NOTIFICATION_START		1
#define EAP_NOTIFICATION_RESTART	2
#define EAP_NOTIFICATION_SUCCESS	3
#define EAP_NOTIFICATION_PACKET		4
#define EAP_NOTIFICATION_DATA_FROM_UI	5
#define EAP_NOTIFICATION_TIMEOUT	6

typedef struct EAP_Input {
    u_int16_t 	size; 		// size of the structure (for future extension)
    u_int8_t 	mode;		// 0 for client, 1 for server
    u_int8_t 	initial_id;	// initial EAP ID
    u_int16_t	mtu;		// mtu wll determine the maximum packet size to send
    u_int16_t	notification;	// notification the EAP engine sends to the module
    u_int16_t	data_len;	// len of the data
    void	*data;		// data to be consumed depending on the notification
    char 	*identity;	// authenticatee identity
    char 	*username;	// authenticatee user name
    char 	*password;	// authenticatee password
    void 	(*log_debug) __P((char *, ...));	/* log a debug message */
    void 	(*log_error) __P((char *, ...));	/* log an error message */
} EAP_Input;

#define EAP_ACTION_NONE			0
#define EAP_ACTION_SEND			1
#define EAP_ACTION_INVOKE_UI		2
#define EAP_ACTION_ACCESS_GRANTED	3
#define EAP_ACTION_ACCESS_DENIED	4
#define EAP_ACTION_SEND_WITH_TIMEOUT	5
#define EAP_ACTION_SEND_AND_DONE	6
#define EAP_ACTION_CANCEL		7


typedef struct EAP_Output {
    u_int16_t 	size; 		// size of the structure (for future extension)
    u_int16_t	action;		// action the EAP engine needs to perform
    u_int16_t	data_len;	// len of the data
    void	*data;		// data to be consumed depending on the action
    char 	*username;	// authenticatee user name (useful in server mode)
} EAP_Output;

enum {
    EAP_NO_ERROR = 0,
    EAP_ERROR_GENERIC,
    EAP_ERROR_INVALID_PACKET
};

/* attribute information returned upon successful authentication */

#define EAP_ATTRIBUTE_NONE		0
#define EAP_ATTRIBUTE_MPPE_SEND_KEY	1
#define EAP_ATTRIBUTE_MPPE_RECV_KEY	2

typedef struct EAP_Attribute {
    u_int16_t	type;		// type of the attribute
    u_int16_t	data_len;	// len of the data
    void	*data;		// data to be consumed depending on the type
    /* data follow according to the size */
} EAP_Attribute;


#ifdef __cplusplus
}
#endif

#define __EAP_PLUGIN_INCLUDE__
#endif /* __EAP_PLUGIN_INCLUDE__ */
