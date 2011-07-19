/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef __L2TP_H__
#define __L2TP_H__

#include <netinet/in.h>
#include "../L2TP-extension/l2tpk.h"

#define L2TP_NKE	"L2TP.kext"
#define L2TP_NKE_ID	"com.apple.nke.l2tp"

/* L2TP exit codes */
#define EXIT_L2TP_NOSERVER  		1
#define EXIT_L2TP_NOANSWER  		2
#define EXIT_L2TP_PROTOCOLERROR 	5
#define EXIT_L2TP_NETWORKCHANGED 	6
#define EXIT_L2TP_NOSHAREDSECRET 	7
#define EXIT_L2TP_NOCERTIFICATE 	8
#if TARGET_OS_EMBEDDED
#define EXIT_L2TP_NOEDGE			9
#endif

/* AVP flags */
#define L2TP_AVP_FLAGS_M		0x8000
#define L2TP_AVP_FLAGS_H		0x4000
#define L2TP_AVP_FLAGS_RESERVED		0x3C00
#define L2TP_AVP_LEN_MASK		0x03FF

/* misc masks */
#define L2TP_AVP_CAPS_MASK		0x00000003
#define L2TP_CNTL_MSG_HDR_SIZE 12

#define L2TP_AVP_HDR_SIZE 		6		/* size of L2TP AVP header */

#define MAX_HOST_NAME_SIZE		64
#define MAX_VENDOR_NAME_SIZE		64
#define MAX_CALLED_NUM_SIZE		128
#define MAX_CALLING_NUM_SIZE		128
#define MAX_SUB_ADDR_SIZE		128
#define MAX_ERROR_MSG_SIZE		256
#define MAX_ADVISORY_MSG_SIZE		256


/* generic results codes */
#define L2TP_RESULT_SUCCESS	1
#define L2TP_RESULT_ERROR	2

/*outgoing call result codes */
#define L2TP_OUTGOING_CALL_RESULT_CONNECTED	1
#define L2TP_OUTGOING_CALL_RESULT_ERROR		2
#define L2TP_OUTGOING_CALL_RESULT_NOCARRIER	3
#define L2TP_OUTGOING_CALL_RESULT_BUSY		4
#define L2TP_OUTGOING_CALL_RESULT_NODIALTONE	5
#define L2TP_OUTGOING_CALL_RESULT_TIMEOUT	6
#define L2TP_OUTGOING_CALL_RESULT_DONOTACCEPT	7

/* define l2tp control messages */

/* Control Connection Management */
#define ZLB_ACK			0	/* used internally to indicate ZLB ack */

#define L2TP_SCCRQ		1	/* Start-Control-Connection-Request */
#define L2TP_SCCRP		2	/* Start-Control-Connection-Reply */
#define L2TP_SCCCN		3	/* Start-Control-Connection-Connected */
#define L2TP_StopCCN		4	/* Stop-Control-Connection-Notification */
#define L2TP_HELLO		6	/* Hello */

/* Call Management */
#define L2TP_OCRQ		7	/* Outgoing-Call-Request */
#define L2TP_OCRP		8	/* Outgoing-Call-Reply */
#define L2TP_OCCN		9	/* Outgoing-Call-Connected */
#define L2TP_ICRQ		10	/* Incoming-Call-Request */
#define L2TP_ICRP		11	/* Incoming-Call-Reply */
#define L2TP_ICCN		12	/* Incoming-Call-Connected */
#define L2TP_CDN		14	/* Call-Disconnect-Notify */

/* Error Reporting */
#define L2TP_WEN		15	/* WAN-Error-Notify */

/* PPP Session Control */
#define L2TP_SLI		16	/* Set-Link-Info */


#define L2TP_PROTOCOL_VERSION	0x0100	/* L2TP version number */
#define L2TP_VENDOR_ID 		63  	/* from RFC1700 */

/* define framing capabilities */
#define L2TP_SYNC_FRAMING	1
#define L2TP_ASYNC_FRAMING	2

/* define bearer capabilities */
#define L2TP_DIGITAL_ACCESS	1
#define L2TP_ANALOG_ACCESS	2

/* result codes for CDN message */
#define L2TP_CALLRESULT_CARRIERLOSS	1
#define L2TP_CALLRESULT_ERRORCODE	2
#define L2TP_CALLRESULT_ADMIN		3
#define L2TP_CALLRESULT_TEMPRESOURCE	4
#define L2TP_CALLRESULT_PERMRESOURCE	5
#define L2TP_CALLRESULT_INVALIDDEST	6
#define L2TP_CALLRESULT_NOCARRIER	7
#define L2TP_CALLRESULT_BUSY		8
#define L2TP_CALLRESULT_NODIALTONE	9
#define L2TP_CALLRESULT_TIMEOUT	10
#define L2TP_CALLRESULT_BADFRAMING	11

/* result codes for StopCCN */
#define L2TP_CCNRESULT_GENERAL		1
#define L2TP_CCNRESULT_GENERALERROR	2
#define L2TP_CCNRESULT_ALREADYEXISTS	3
#define L2TP_CCNRESULT_NOTAUTHORIZED	4
#define L2TP_CCNRESULT_UNSUPPORTEDVERS	5
#define L2TP_CCNRESULT_SHUTDOWN		6
#define L2TP_CCNRESULT_FSMERROR		7

#define MAX_CNTL_BUFFER_SIZE 		1500	/* should be far more than needed */

struct l2tp_parameters {
	/* parameters used for control and call connection establishment */
	u_int16_t	tunnel_id;
	u_int16_t	protocol_vers;
	u_int16_t	firmware_rev;
	u_int16_t	window_size;
	u_int16_t	session_id;
	u_int16_t	seq_required;
	/*u_int16_t	proxy_authen_type; */
	/*u_int16_t	proxy_authen_id; */
	u_int32_t	framing_type;
	u_int32_t	tx_connect_speed;
	u_int32_t	rx_connect_speed;
	u_int32_t	call_serial_num;
	u_int32_t	bearer_type;
	u_int32_t	phys_channel_id;
	u_int32_t	framing_caps;
	u_int32_t	bearer_caps;
	u_int32_t	tie_breaker[2];
	u_int8_t	host_name[MAX_HOST_NAME_SIZE];
	u_int8_t	vendor_name[MAX_VENDOR_NAME_SIZE];
	u_int8_t	called_number[MAX_CALLED_NUM_SIZE];
	u_int8_t	calling_number[MAX_CALLING_NUM_SIZE];
	u_int8_t	sub_address[MAX_SUB_ADDR_SIZE];
	/*u_int8_t	challenge[MAX_CHALLENGE_SIZE]; */
	/*u_int8_t	challenge_resp[MAX_CHALLENGE_RESP_SIZE]; */
	/*u_int8_t	priv_group_id[MAX_PRIV_GROUP_ID_SIZE]; */
	/*u_int8_t	init_rcvd_confreq[MAX_CONFREQ_SIZE]; */
	/*u_int8_t	last_sent_confreq[MAX_CONFREQ_SIZE]; */
	/*u_int8_t	last_rcvd_confreq[MAX_CONFREQ_SIZE]; */
	/*u_int8_t	proxy_authen_name[MAX_PROXY_AUTHEN_NAME]; */
	/*u_int8_t	proxy_authen_challenge[MAX_PROXY_AUTHEN_CHALLENGE]; */
	/*u_int8_t	proxy_authen_resp[MAX_PROXY_AUTHEN_RES]; */
	/* Result Code */
	u_int16_t	result_code;
	u_int16_t	error_code;
	u_int8_t	error_message[MAX_ERROR_MSG_SIZE];
	/* Cause Code */
	u_int16_t	cause_code;
	u_int8_t	cause_message;
	u_int8_t	advisory_message[MAX_ADVISORY_MSG_SIZE];
	/* SLI */
	u_int32_t	send_accm;
	u_int32_t	recv_accm;
	/* WEN */
	u_int32_t	crc_errors;
	u_int32_t	framing_errors;
	u_int32_t	hardware_overruns;
	u_int32_t	buffer_overruns;
	u_int32_t	timeout_errors;
	u_int32_t	alignment_errors;
};


/*-----------------------------
 * function prototypes
-----------------------------*/
int l2tp_outgoing_call(int fd, struct sockaddr *peer_address, struct l2tp_parameters *our_params, struct l2tp_parameters *peer_params, int recv_timeout);
int l2tp_incoming_call(int fd, struct l2tp_parameters *our_params, struct l2tp_parameters *peer_params, int recv_timeout);
int l2tp_data_in(int fd);
int l2tp_send_hello(int fd, struct l2tp_parameters *our_params);
int l2tp_send_hello_trigger(int fd, struct sockaddr *peer_address);
int l2tp_send_SCCRQ(int fd, struct sockaddr *peer_address, struct l2tp_parameters *our_params);
int l2tp_send_CDN(int fd, struct l2tp_parameters *our_params, struct l2tp_parameters *peer_params);
int l2tp_send_StopCCN(int fd, struct l2tp_parameters *our_params);
void l2tp_reset_timers(int fd, int connect_mode);
int l2tp_set_flag(int fd, int set, u_int32_t flag);
int l2tp_set_baudrate(int fd, u_int32_t baudrate);
int l2tp_recv(int fd, u_int8_t* buf, int len, int *outlen, struct sockaddr *from, int timeout, char *text);

int l2tp_set_ouraddress(int fd, struct sockaddr *addr);
int l2tp_set_peeraddress(int fd, struct sockaddr *addr);
int l2tp_new_tunnelid(int fd, u_int16_t *tunnelid);
int l2tp_set_ourparams(int fd, struct l2tp_parameters *our_params);
int l2tp_set_peerparams(int fd, struct l2tp_parameters *peer_params);
int l2tp_change_peeraddress(int fd, struct sockaddr *peer);

#endif
