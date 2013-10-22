/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
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

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  plugin to add L2TP client support to pppd.
 *
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
  Includes
----------------------------------------------------------------------------- */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#define APPLE 1
#include "../L2TP-extension/l2tpk.h"
#include "l2tp.h"
#include "../../../Helpers/pppd/pppd.h"

/* -----------------------------------------------------------------------------
 L2TP defines
----------------------------------------------------------------------------- */

#define	MAX_RANDOM_VECTOR_SIZE 		128  	/* ??? just a guess - look into this */
#define	MAX_AVP_VALUE_SIZE		256	/* ??? just a guess */
#define	MAX_CAUSE_MSG_SIZE 		256

/* define l2tp AVP types */
#define L2TP_AVP_MSG_TYPE		0	/* Message Type AVP Attribute Type (All Messages) */
#define L2TP_AVP_RESULT_CODE		1	/* Result Code AVP Attribute Type (CDN, StopCCN) */
#define L2TP_AVP_PROTO_VERS		2	/* Protocol Version AVP Attribute Type (SCCRP, SCCRQ) */
#define L2TP_AVP_FRAMING_CAPS		3	/* Framing Capabilities AVP Attribute Type (SCCRP, SCCRQ) */
#define L2TP_AVP_BEARER_CAPS		4	/* Bearer Capabilities AVP Attribute Type (SCCRP, SCCRQ) */
#define L2TP_AVP_TIE_BREAKER		5	/* Tie Breaker AVP Attribute Type (SCCRQ) */
#define L2TP_AVP_FIRMWARE_REV		6	/* Firmware Revision AVP Attribute Type (SCCRP, SCCRQ) */
#define L2TP_AVP_HOST_NAME		7	/* Host Name AVP Attribute Type (SCCRP, SCCRQ) */
#define L2TP_AVP_VENDOR_NAME		8	/* Vendor Name AVP Attribute Type (SCCRP, SCCRQ) */
#define L2TP_AVP_TUNNEL_ID		9	/* Assigned Tunnel ID AVP Attribute Type (SCCRP, SCCRQ, StopCCN) */
#define L2TP_AVP_WINDOW_SIZE		10	/* Reveive Window Size AVP Attribute Type (SCCRP, SCCRQ) */
#define L2TP_AVP_CHALLENGE		11	/* Challenge AVP Attribute Type (SCCRP, SCCRQ) */
#define L2TP_AVP_CAUSE_CODE		12	/* Cause Code AVP Attribute Type (CDN) */
#define L2TP_AVP_CHALLENGE_RESP		13	/* Challenge Response AVP Attribute Type (SCCCN, SCCRP) */
#define L2TP_AVP_SESSION_ID		14	/* Assigned Session ID AVP Attribute Type (CDN, ICRP, ICRQ, OCRP, OCRQ) */
#define L2TP_AVP_CALL_SERIAL_NUM	15	/* Call Serial Number AVP Attribute Type (ICRQ, OCRQ) */
#define L2TP_AVP_MIN_BPS		16	/* Minimum BPS (OCRQ) */
#define L2TP_AVP_MAX_BPS		17	/* Maximum BPS (OCRQ) */
#define L2TP_AVP_BEARER_TYPE		18	/* Bearer Type (ICRQ, OCRQ) */
#define L2TP_AVP_FRAMING_TYPE		19	/* Framing Type (ICCN, OCCN, OCRQ) */
#define L2TP_AVP_CALLED_NUM		21	/* Called Number (ICRQ, OCRQ) */
#define L2TP_AVP_CALLING_NUM		22	/* Calling Number (ICRQ) */
#define L2TP_AVP_SUB_ADDRESS		23	/* Sub-Address (ICRQ, OCRQ) */
#define L2TP_AVP_TX_CONNECT_SPEED	24	/* TX Connect Speed (ICCN, OCCN) */
#define L2TP_AVP_PHYS_CHANNEL_ID	25	/* Physical Channel ID (ICRQ, OCRP) */
#define L2TP_AVP_INIT_RECVD_CONFREQ	26	/* Initial Receieved LCP CONFREQ (ICCN) */
#define L2TP_AVP_LAST_SENT_CONFREQ	27	/* Last Sent LCP CONFREQ (ICCN) */
#define L2TP_AVP_LAST_RECVD_CONFREQ	28	/* Last Received LCP CONFREQ (ICCN) */
#define L2TP_AVP_PROXY_AUTH_TYPE	29	/* Proxy Authen Type (ICCN) */
#define L2TP_AVP_PROXY_AUTH_NAME	30	/* Proxy Authen Name (ICCN) */
#define L2TP_AVP_PROXY_AUTH_CHALLENGE	31	/* Proxy Authen Challenge (ICCN) */
#define L2TP_AVP_PROXY_AUTH_ID		32	/* Proxy Authen ID (ICCN) */
#define L2TP_AVP_PROXY_AUTH_RESP	33	/* Proxy Authen Response (ICCN) */
#define L2TP_AVP_CALL_ERRORS		34	/* Call Errors (WEN) */
#define L2TP_AVP_ACCM			35	/* ACCM (SLI) */
#define L2TP_AVP_RAND_VECT		36	/* Random Vector AVP Attribute Type (All Messages) */
#define L2TP_AVP_PRIVATE_GROUP_ID	37	/* Private Group ID (ICCN) */
#define L2TP_AVP_RX_CONNECT_SPEED	38	/* RX Connect Speed (ICCN, OCCN) */
#define L2TP_AVP_SEQ_REQUIRED		39	/* Sequencing Required (ICCN, OCCN) */

#define L2TP_LAST_AVP_TYPE		L2TP_AVP_SEQ_REQUIRED
 

/*
 * The following is used to test each AVP as its processed for
 * size and its appropriateness for a received control message.
 */

/* 
 * required-AVP bitmap bits - used to determine if all required AVPs
 * have been received for a particular message.  Only AVPs that are
 * required by one or more message types are included
 */
#define L2TP_AVP_MSG_TYPE_BIT			(0x00000001 << 0)
#define L2TP_AVP_RESULT_CODE_BIT		(0x00000001 << 1)
#define L2TP_AVP_PROTO_VERS_BIT			(0x00000001 << 2)
#define L2TP_AVP_FRAMING_CAPS_BIT		(0x00000001 << 3)
#define L2TP_AVP_HOST_NAME_BIT			(0x00000001 << 4)
#define L2TP_AVP_TUNNEL_ID_BIT			(0x00000001 << 5)
#define L2TP_AVP_WINDOW_SIZE_BIT		(0x00000001 << 6)
#define L2TP_AVP_SESSION_ID_BIT			(0x00000001 << 7)
#define L2TP_AVP_CALL_SERIAL_NUM_BIT		(0x00000001 << 8)
#define L2TP_AVP_MIN_BPS_BIT			(0x00000001 << 9)
#define L2TP_AVP_MAX_BPS_BIT			(0x00000001 << 10)
#define L2TP_AVP_BEARER_TYPE_BIT		(0x00000001 << 11)
#define L2TP_AVP_FRAMING_TYPE_BIT		(0x00000001 << 12)
#define L2TP_AVP_CALLED_NUM_BIT			(0x00000001 << 13)
#define L2TP_AVP_TX_CONNECT_SPEED_BIT		(0x00000001 << 14)
#define L2TP_AVP_CALL_ERRORS_BIT		(0x00000001 << 15)
#define L2TP_AVP_ACCM_BIT			(0x00000001 << 16)

/* bitmaps for indicating which AVPs are required for message types */
#define L2TP_SCCRQ_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_PROTO_VERS_BIT | L2TP_AVP_HOST_NAME_BIT | \
						L2TP_AVP_FRAMING_CAPS_BIT | L2TP_AVP_TUNNEL_ID_BIT) 
#define L2TP_SCCRP_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_PROTO_VERS_BIT | L2TP_AVP_HOST_NAME_BIT | \
						L2TP_AVP_FRAMING_CAPS_BIT | L2TP_AVP_TUNNEL_ID_BIT) 
#define L2TP_SCCCN_BITMAP	(L2TP_AVP_MSG_TYPE_BIT)
#define L2TP_StopCCN_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_TUNNEL_ID_BIT | L2TP_AVP_RESULT_CODE_BIT)
#define L2TP_HELLO_BITMAP	(L2TP_AVP_MSG_TYPE_BIT)
#define L2TP_OCRQ_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_SESSION_ID_BIT | L2TP_AVP_CALL_SERIAL_NUM_BIT | \
                                            L2TP_AVP_MIN_BPS_BIT | L2TP_AVP_MAX_BPS_BIT | L2TP_AVP_BEARER_TYPE_BIT | \
                                            L2TP_AVP_FRAMING_TYPE_BIT | L2TP_AVP_CALLED_NUM_BIT)
#define L2TP_OCRP_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_SESSION_ID_BIT)
#define L2TP_OCCN_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_TX_CONNECT_SPEED_BIT | L2TP_AVP_FRAMING_TYPE_BIT)
#define L2TP_ICRQ_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_SESSION_ID_BIT | L2TP_AVP_CALL_SERIAL_NUM_BIT)
#define L2TP_ICRP_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_SESSION_ID_BIT)
#define L2TP_ICCN_BITMAP	(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_TX_CONNECT_SPEED_BIT | L2TP_AVP_FRAMING_TYPE_BIT)
#define L2TP_CDN_BITMAP		(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_RESULT_CODE_BIT | L2TP_AVP_SESSION_ID_BIT)
#define L2TP_WEN_BITMAP		(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_CALL_ERRORS_BIT)
#define L2TP_SLI_BITMAP		(L2TP_AVP_MSG_TYPE_BIT | L2TP_AVP_ACCM_BIT)


#define AVP_SIZE_VARIABLE 	0xff

struct avp_attributes {
	u_int16_t	size;
	u_int32_t	maskbit;
};

/* array for checking AVP sizes and validity for message type */
struct avp_attributes avp_attr[L2TP_LAST_AVP_TYPE + 1] = {
		{	2,	L2TP_AVP_MSG_TYPE_BIT },			/* 0 Message Type */
		{	AVP_SIZE_VARIABLE,	L2TP_AVP_RESULT_CODE_BIT },	/* 1 Result Code */
		{	2,	L2TP_AVP_PROTO_VERS_BIT },			/* 2 Protocol Version */
		{	4,	L2TP_AVP_FRAMING_CAPS_BIT },			/* 3 Framing Capabilities */
		{	4,	0 },						/* 4 Bearer Capabilities */
		{	8,	0 },						/* 5 Tie Breaker */
		{	2,	0 },						/* 6 Firmware Rev */
		{	AVP_SIZE_VARIABLE,	L2TP_AVP_HOST_NAME_BIT },	/* 7 Host Name */
		{	AVP_SIZE_VARIABLE,	0 },				/* 8 Vendor name */
		{	2,	L2TP_AVP_TUNNEL_ID_BIT },			/* 9 Tunnel ID */
		{	2,	L2TP_AVP_WINDOW_SIZE_BIT },			/* 10 Window Size */
		{	AVP_SIZE_VARIABLE,	0 },				/* 11 Challenge */
		{	AVP_SIZE_VARIABLE,	0 },				/* 12 Cause Code */
		{	AVP_SIZE_VARIABLE,	0 },				/* 13 Challenge Response */
		{	2, L2TP_AVP_SESSION_ID_BIT },				/* 14 Session ID */
		{	4, L2TP_AVP_CALL_SERIAL_NUM_BIT },			/* 15 Call Serial Number */
		{	4, L2TP_AVP_MIN_BPS_BIT },				/* 16 Minimum BPS */
		{	4, L2TP_AVP_MAX_BPS_BIT },				/* 17 Maximum BPS */
		{	4, L2TP_AVP_BEARER_TYPE_BIT },				/* 18 Bearer Type */
		{	4, L2TP_AVP_FRAMING_TYPE_BIT },				/* 19 Framing Type */
		{	0,	0 },						/* 20 unused */
		{	AVP_SIZE_VARIABLE, L2TP_AVP_CALLED_NUM_BIT },		/* 21 Called Number */
		{	AVP_SIZE_VARIABLE,	0 },				/* 22 Calling Number */
		{	AVP_SIZE_VARIABLE,	0 },				/* 23 Sub-Address */
		{	4, L2TP_AVP_TX_CONNECT_SPEED_BIT },			/* 24 TX Connect Speed */
		{	4,	0 },						/* 25 Physical Channel ID */
		{	AVP_SIZE_VARIABLE,	0 },				/* 26 Initial Received CONFREQ */
		{	AVP_SIZE_VARIABLE,	0 },				/* 27 Last Sent CONFREQ */
		{	AVP_SIZE_VARIABLE,	0 },				/* 28 Last Rcvd CONFREQ */
		{	2,	0 },						/* 29 Proxy Authen Type */
		{	AVP_SIZE_VARIABLE,	0 },				/* 30 Proxy Auth Name */
		{	AVP_SIZE_VARIABLE,	0 },				/* 31 Proxy Authen Challenge */
		{	2,	0 },						/* 32 Proxy Authen ID */
		{	AVP_SIZE_VARIABLE,	0 },				/* 33 Proxy Authen Response */
		{	26, L2TP_AVP_CALL_ERRORS_BIT },				/* 34 Call Errors */
		{	10, L2TP_AVP_ACCM_BIT },				/* 35 ACCM */
		{	AVP_SIZE_VARIABLE,	0 },				/* 36 Random Vector */
		{	AVP_SIZE_VARIABLE,	0 },				/* 37 Private Group ID */
		{	4,	0 },						/* 38 RX Connect Speed */
		{	0,	0 }						/* 39 Sequencing Required */
};
 

struct l2tp_avp_hdr {
    /* Attribute-Value Pair header */
    u_int16_t	flags_len;
    u_int16_t	vendor_id;
    u_int16_t	type;
    u_int8_t	value;
};

 
/* -----------------------------------------------------------------------------
    Function Prototypes
----------------------------------------------------------------------------- */

static int process_pkt_data(u_int8_t* buf, size_t len, u_int16_t* type, struct l2tp_parameters* params, u_int16_t expected_type);	
static int unhide_avp(u_int8_t*, u_int16_t*, u_int8_t*, size_t);
static size_t prepare_SCCRQ(u_int8_t*, size_t, struct l2tp_parameters*);
static size_t prepare_SCCRP(u_int8_t*, size_t, struct l2tp_parameters*);
static size_t prepare_SCCRX(u_int8_t*, size_t, u_int16_t, struct l2tp_parameters*);
static size_t prepare_SCCCN(u_int8_t*, size_t);
static int prepare_StopCCN(u_int8_t*, size_t, struct l2tp_parameters*);
static int prepare_Hello(u_int8_t*, size_t);
static int prepare_ICRQ(u_int8_t*, size_t, struct l2tp_parameters*);
static int prepare_ICRP(u_int8_t*, size_t, struct l2tp_parameters*);
static size_t prepare_ICCN(u_int8_t*, size_t, struct l2tp_parameters*);
static size_t prepare_CDN(u_int8_t*, size_t, struct l2tp_parameters*);
#ifdef UNUSED
static size_t prepare_WEN(u_int8_t*, size_t, struct l2tp_parameters*);
#endif
static int make_avp_hdr(u_int8_t**, size_t*, u_int16_t, size_t, u_int16_t);
static int make_avp_short(u_int8_t**, size_t*, u_int16_t, u_int16_t, u_int16_t);
static int make_avp_long(u_int8_t**, size_t*, u_int16_t, u_int32_t, u_int16_t);
static char *msg_type_str(u_int16_t msg_type);
static int l2tp_send(int ctrlsockfd, u_int8_t* buf, int len, u_int16_t session_id, struct sockaddr *to, char *text);

#define PROCESS_PACKET(a,b,c,d,e) \
    if (process_pkt_data(a, b, c, d, e)) \
            return EXIT_L2TP_PROTOCOLERROR;         

#define SEND_PACKET(a,b,c,d,e,f) \
    if ((result = l2tp_send(a, b, c, d, e, f))) \
        return result;

#define RECV_PACKET(a,b,c,d,e,f,g) \
    if ((result = l2tp_recv(a, b, c, d, e, f, g))) \
        return result;


/* -----------------------------------------------------------------------------
    Globals variables
----------------------------------------------------------------------------- */

extern int 		kill_link;

u_int8_t		control_buf[MAX_CNTL_BUFFER_SIZE] __attribute__ ((aligned(4)));
struct l2tp_header	*control_hdr = ALIGNED_CAST(struct l2tp_header *)control_buf;


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_outgoing_call(int fd, struct sockaddr *peer_address, 
                        struct l2tp_parameters *our_params, struct l2tp_parameters *peer_params,
                        int recv_timeout)
{
    int			size;
    int			result;
    u_int16_t		msg_type;
    struct sockaddr	from;

    /* ------------- send SCCRQ  -------------*/	 
	size = prepare_SCCRQ(control_buf, MAX_CNTL_BUFFER_SIZE, our_params);
	SEND_PACKET(fd, control_buf, size, 0, peer_address, "SCCRQ");

    /* ------------- read SCCRP  -------------*/	 
    from.sa_len = sizeof(from);
    result = l2tp_recv(fd, control_buf, MAX_CNTL_BUFFER_SIZE, &size, &from, recv_timeout, "SCCRP");
    if (result == -2) // cancel
		return result;
	if (result == -1 || size == 0) { // no reply
        notice("L2TP cannot connect to the server\n");
        return EXIT_L2TP_NOANSWER;
	}
    
   /* the server can reply from an other port, lock our connection to the new received peer address */
    l2tp_change_peeraddress(fd, &from);

    PROCESS_PACKET(control_buf, size, &msg_type, peer_params, L2TP_SCCRP);
    
    if (peer_params->tunnel_id == 0) { 	/* check peer tunnel ID */
            error("L2TP received invalid Tunnel ID from peer\n");
            return EXIT_L2TP_PROTOCOLERROR;
    }
    
    /* now that we made contact, set adaptative time and timer values */
    l2tp_reset_timers(fd, 0);
    
    /* set peer tunnel id and peer window */
    if (peer_params->window_size == 0)
        peer_params->window_size = 4;		/* assume 4 if absent as per rfc2661 */
    l2tp_set_peerparams(fd, peer_params);
    
    /* ------------- send SCCCN  -------------*/	 
    size = prepare_SCCCN(control_buf, MAX_CNTL_BUFFER_SIZE);
    SEND_PACKET(fd, control_buf, size, 0, 0, "SCCCN");
    
    /* ------------- send ICRQ  -------------*/	 
    size = prepare_ICRQ(control_buf, MAX_CNTL_BUFFER_SIZE, our_params);
    SEND_PACKET(fd, control_buf, size, 0, 0, "ICRQ");
        
    /* ------------- read ICRP  -------------*/	 
    from.sa_len = sizeof(from);
    RECV_PACKET(fd, control_buf, MAX_CNTL_BUFFER_SIZE, &size, &from, recv_timeout, "ICRP");
    
    PROCESS_PACKET(control_buf, size, &msg_type, peer_params, L2TP_ICRP);
    
    if (ntohs(control_hdr->session_id) != our_params->session_id) {
            error("L2TP message from peer addressed to invalid session ID (our ID : %d, target ID : %d)\n", 
                our_params->session_id, ntohs(control_hdr->session_id));
            return EXIT_L2TP_PROTOCOLERROR;
    }
    if (peer_params->session_id == 0) {
            error("L2TP received invalid Session ID from peer\n");
            return EXIT_L2TP_PROTOCOLERROR;
    } 
        
    /* ------------- send ICCN  -------------*/	 
    size = prepare_ICCN(control_buf, MAX_CNTL_BUFFER_SIZE, our_params);
    SEND_PACKET(fd, control_buf, size, peer_params->session_id, 0, "ICCN");
        
    /* call succedeed ! */
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_incoming_call(int fd, struct l2tp_parameters *our_params, struct l2tp_parameters *peer_params, int recv_timeout)
{
    int			size, result;
    u_int16_t		msg_type;
    struct sockaddr	from;

    /* ------------- read SCCRQ  -------------*/	 
    from.sa_len = sizeof(from);
    RECV_PACKET(fd, control_buf, MAX_CNTL_BUFFER_SIZE, &size, &from, 0, "SCCRQ");
    
    /* lock the control connection to the specific address the server responded from */
    l2tp_change_peeraddress(fd, &from);
    
    PROCESS_PACKET(control_buf, size, &msg_type, peer_params, L2TP_SCCRQ);
        
    /* setup our tunnel ID in the kernel */
//    l2tp_new_tunnelid(fd, &our_params->tunnel_id);
    
    if (peer_params->tunnel_id == 0) { 	/* check peer tunnel ID */
            error("L2TP received invalid Tunnel ID from peer\n");
            return -1;
    }
    
    if (peer_params->window_size == 0)
        peer_params->window_size = 4;		/* assume 4 if absent as per rfc2661 */
    l2tp_set_peerparams(fd, peer_params);

    /* ------------- send SCCRP  -------------*/	 
    size = prepare_SCCRP(control_buf, MAX_CNTL_BUFFER_SIZE, our_params);
    SEND_PACKET(fd, control_buf, size, 0, 0, "SCCRP");
        
    /* ------------- read SCCCN  -------------*/	 
    from.sa_len = sizeof(from);
    RECV_PACKET(fd, control_buf, MAX_CNTL_BUFFER_SIZE, &size, &from, recv_timeout, "SCCCN");

    PROCESS_PACKET(control_buf, size, &msg_type, peer_params, L2TP_SCCCN);
        
     /* ------------- read ICRQ  -------------*/	 
    from.sa_len = sizeof(from);
    RECV_PACKET(fd, control_buf, MAX_CNTL_BUFFER_SIZE, &size, &from, recv_timeout, "ICRQ");

    PROCESS_PACKET(control_buf, size, &msg_type, peer_params, L2TP_ICRQ);

    if ((peer_params->session_id) == 0) {
            error("L2TP received invalid Session ID from peer\n");
            return -1;
    } 

    /* ------------- send ICRP  -------------*/	 
    size = prepare_ICRP(control_buf, MAX_CNTL_BUFFER_SIZE, our_params);
    SEND_PACKET(fd, control_buf, size, peer_params->session_id, 0, "ICRP");

    /* ------------- read ICCN  -------------*/	 
    RECV_PACKET(fd, control_buf, MAX_CNTL_BUFFER_SIZE, &size, &from, recv_timeout, "ICCN");

    PROCESS_PACKET(control_buf, size, &msg_type, peer_params, L2TP_ICCN);

    if (ntohs(control_hdr->session_id) != our_params->session_id) {
            error("L2TP message from peer addressed to invalid session ID (our ID : %d, target ID : %d)\n", 
                our_params->session_id, ntohs(control_hdr->session_id));
            return -1;
    }

    /* connected ! */

    return 0;
}

/* -----------------------------------------------------------------------------
        Send a Hello
----------------------------------------------------------------------------- */
int l2tp_send_hello(int fd, struct l2tp_parameters *our_params)	
{		
    int 	size;

    size = prepare_Hello(control_buf, MAX_CNTL_BUFFER_SIZE);
    return l2tp_send(fd, control_buf, size, 0, 0, "Hello");
}

/* -----------------------------------------------------------------------------
 Send a bunch of Hellos to trigger ipsec
 ----------------------------------------------------------------------------- */
int l2tp_send_hello_trigger(int fd, struct sockaddr *peer_address)	
{
    int    hello_count, size, i;
    size_t len = sizeof(int);

    if (sysctlbyname("net.key.blockacq_count", &hello_count, &len, 0, 0)) {
        hello_count = 10;
        error("Failed to probe blockacq count: using %d", hello_count);
    }

    size = prepare_Hello(control_buf, MAX_CNTL_BUFFER_SIZE);
    for (i = 0; i <= hello_count; i++) {
        if (l2tp_send(fd, control_buf, size, 0, peer_address, "Hello")) {
            error("Failed to send L2TP hello trigger. tried %d, max %d", i, hello_count);
            return -1;
        }
    }

    return 0;
}

/* -----------------------------------------------------------------------------
        Send a SCCRQ
----------------------------------------------------------------------------- */
int l2tp_send_SCCRQ(int fd, struct sockaddr *peer_address, 
                        struct l2tp_parameters *our_params)	
{		
    int 	size;

	size = prepare_SCCRQ(control_buf, MAX_CNTL_BUFFER_SIZE, our_params);
    return l2tp_send(fd, control_buf, size, 0, peer_address, "SCCRQ");
}

/* -----------------------------------------------------------------------------
	Send a CDN
----------------------------------------------------------------------------- */
int l2tp_send_CDN(int fd, struct l2tp_parameters *our_params, struct l2tp_parameters *peer_params) 
{   
    int	size;

    size = prepare_CDN(control_buf, MAX_CNTL_BUFFER_SIZE, our_params);
    return l2tp_send(fd, control_buf, size, peer_params->session_id, 0, "CDN"); 
}

/* -----------------------------------------------------------------------------
	Send a StopCCN
----------------------------------------------------------------------------- */
int l2tp_send_StopCCN(int fd, struct l2tp_parameters *our_params) 
{
    int 	size;

    size = prepare_StopCCN(control_buf, MAX_CNTL_BUFFER_SIZE, our_params);
    return l2tp_send(fd, control_buf, size, 0, 0, "StopCCN");
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int l2tp_data_in(int fd)
{
    int 			size, result;
    u_int16_t			msg_type;
    struct sockaddr	 	from;
    struct l2tp_parameters	peer_params;
    
    bzero(&peer_params, sizeof(peer_params));
    
    from.sa_len = sizeof(from);
    RECV_PACKET(fd, control_buf, MAX_CNTL_BUFFER_SIZE, &size, (struct sockaddr*)&from, 0, "data");

    if (size == 0) {
            // No data, reliable layer has disconnected
            return -1;  
    }
        
    /* process message */
    PROCESS_PACKET(control_buf, size, &msg_type, &peer_params, 0);
            
    /* act on the message */
    switch (msg_type) {
            case L2TP_SCCRQ:
            case L2TP_SCCRP:
            case L2TP_SCCCN:
            case L2TP_ICRQ:
            case L2TP_ICRP:
            case L2TP_ICCN:			
            case L2TP_OCRQ:
            case L2TP_OCRP:
            case L2TP_OCCN:
                    error("L2TP received unexpected control message\n");
                    //return -1;
                    break;

            case L2TP_HELLO:
                    /* yeah - hello - nothing to do */
                    break;

            case L2TP_StopCCN:
                return -1;
                break;
                
            case L2TP_CDN:
                return -1;
                break;
                
            case L2TP_WEN:
            case L2TP_SLI:

            default:
                    /* should never get here - program error */
                    /* ??? log error */
                    //return -1;
                    break;
    }


    return 0;
}


/* -----------------------------------------------------------------------------
	Process AVPs from a received control packet
----------------------------------------------------------------------------- */
int process_pkt_data(u_int8_t* buf, size_t len, u_int16_t* type, struct l2tp_parameters* params, u_int16_t expected_type)	
{							
    u_int16_t	msg_type = 0;
    u_int16_t	avp_flags;
    u_int16_t	avp_vendor;
    u_int16_t	avp_len;
    u_int16_t	avp_type;
    u_int16_t	value_len;
    u_int16_t	attr_size;
    u_int8_t*	value_buf = NULL;
    int		first_avp = 1;
    int		mandatory_msg = 0;
    int		random_vector_len = 0;
    u_int8_t	random_vector[MAX_RANDOM_VECTOR_SIZE];
    u_int8_t	unhide_buf[MAX_AVP_VALUE_SIZE];
    u_int32_t	avp_bitmap = 0;			/* for checking if all mandatory AVPs are present */
    struct      l2tp_avp_hdr avp_hdr;
    
    buf += L2TP_CNTL_HDR_SIZE;
    len -= L2TP_CNTL_HDR_SIZE;
    
    /*
        * process AVPs
        */
    while (len > 0) {                
            /* get AVP flags and len */
            memcpy(&avp_hdr, buf, sizeof(struct l2tp_avp_hdr));      // Wcast-align fix - copy header to aligned struct
            
            avp_flags = ntohs(avp_hdr.flags_len);
            avp_len = avp_flags & L2TP_AVP_LEN_MASK;
            avp_type = ntohs(avp_hdr.type);
                            
            /* check the avp length */
            if (avp_len < L2TP_AVP_HDR_SIZE || avp_len > len) {
                    error("L2TP received AVP with bad length... AVP type = %d\n", avp_type);
                    return -1;
            }
                                            
            avp_vendor = ntohs(avp_hdr.vendor_id);

            /* setup ptr and len for value - inc input buf ptr to next AVP */
            if (value_buf != NULL)
                free(value_buf);
            value_buf = malloc(avp_len);                    // Wcast-align fix - copy avp to aligned buffer
            memcpy(value_buf, buf + L2TP_AVP_HDR_SIZE, avp_len);
            value_len = avp_len - L2TP_AVP_HDR_SIZE;
            buf += avp_len; 			
            len -= avp_len;				
            
            /* check that reserved flags and Vendor ID are zero */
            if ( (avp_flags & L2TP_AVP_FLAGS_RESERVED) != 0 || avp_vendor != 0) {
                    if (avp_flags & L2TP_AVP_FLAGS_M || first_avp) {
                            error("L2TP received invalid madatory AVP... AVP type = %d\n", avp_type);
                            return -1;
                    } else {
                            continue;
                    }
            }
        
            /* if first AVP - must be Message Type and cannot be hidden */
            if (first_avp) {
                    if (avp_type != L2TP_AVP_MSG_TYPE || avp_flags & L2TP_AVP_FLAGS_H ||
                                    value_len != sizeof(u_int16_t)) {
                            error("L2TP invalid Message Type AVP... AVP type = %d\n", avp_type);
                            return -1;
                    }
                    msg_type = ntohs(*(ALIGNED_CAST(u_int16_t *)value_buf));
                    if (avp_flags & L2TP_AVP_FLAGS_M)
                            mandatory_msg = 1;
                    first_avp = 0;
                    avp_bitmap |= avp_attr[L2TP_AVP_MSG_TYPE].maskbit;
                    continue;
            }
            

            /*
                * if AVP is hidden - unhide it
                */
            if (avp_flags & L2TP_AVP_FLAGS_H)
                    if (unhide_avp(value_buf, &value_len, unhide_buf, MAX_AVP_VALUE_SIZE) < 0) {
                            error("L2TP error while unhiding a hidden AVP... AVP type = %d\n", avp_type);
                            return -1;		
                    }
            
            /* if known avp type - check avp size and mark it in bitmap */
			if (avp_type <= L2TP_LAST_AVP_TYPE) {
				attr_size = avp_attr[avp_type].size;
				if (attr_size != AVP_SIZE_VARIABLE && value_len != attr_size) {
						error("L2TP AVP with invalid len... AVP type = %d\n", avp_type);
						return -1;
				}
				avp_bitmap |= avp_attr[avp_type].maskbit;
			}
				
            switch (avp_type) {
                    case L2TP_AVP_PROTO_VERS:
                            params->protocol_vers = ntohs(*(ALIGNED_CAST(u_int16_t*)value_buf));
                            if (params->protocol_vers != L2TP_PROTOCOL_VERSION) {
                                    error("L2TP received message for invalid or unknown Protocol Version\n");
                                    return -1;
                            }
                            break;
                            
                    case L2TP_AVP_FRAMING_CAPS:
                            /*params->framing_caps = ntohl(*((u_int32_t*)value_buf));*/
                            break;
                            
                    case L2TP_AVP_BEARER_CAPS:
                            /* params->bearer_caps = ntohl(*((u_int32_t*)value_buf));*/
                            break;
                            
                    case L2TP_AVP_TIE_BREAKER:
                            /*params->tie_breaker[0] = ntohl(*(((u_int32_t*)value_buf)++));*/
                            /*params->tie_breaker[1] = ntohl(*((u_int32_t*)value_buf));*/
                            break;
                            
                    case L2TP_AVP_FIRMWARE_REV:
                            params->firmware_rev = ntohs(*(ALIGNED_CAST(u_int16_t*)value_buf));
                            break;
                            
                    case L2TP_AVP_HOST_NAME:
                            if (value_len >= sizeof(params->host_name))
                                    value_len = sizeof(params->host_name) - 1;
                            bcopy(value_buf, params->host_name, value_len);
                            params->host_name[value_len] = 0;
                            break;
                            
                    case L2TP_AVP_VENDOR_NAME:
                            if (value_len >= sizeof(params->vendor_name))
                                    value_len = sizeof(params->vendor_name) - 1;
                            bcopy(value_buf, params->vendor_name, value_len);
                            params->vendor_name[value_len] = 0;
                            break;
    
                    case L2TP_AVP_TUNNEL_ID:
                            if ((params->tunnel_id = ntohs(*(ALIGNED_CAST(u_int16_t*)value_buf))) == 0) {
                                    error("L2TP received invalid Assigned Tunnel ID\n");
                                    return -1;
                            }
                            break;
                            
                    case L2TP_AVP_WINDOW_SIZE:
                            if ((params->window_size = ntohs(*(ALIGNED_CAST(u_int16_t*)value_buf))) == 0) {
                                    params->window_size = 4;
                            }
                            break;
                            
                    case L2TP_AVP_SESSION_ID:
                            if ((params->session_id = ntohs(*(ALIGNED_CAST(u_int16_t*)value_buf))) == 0) {
                                    error("L2TP received invalid Assigned Session ID\n");
                                    return -1;
                            }
                            break;		
            
                    case L2TP_AVP_CHALLENGE:
                            /* we don't handle this right now */
                            error("L2TP received Auth Challenge AVP - not supported\n");
                            break;
                            
                    case L2TP_AVP_CHALLENGE_RESP:
                            /* we don't handle this right now */
                            error("L2TP received Auth Challenge Response AVP - not supported\n");
                            break;

                    case L2TP_AVP_CALL_SERIAL_NUM:
                            params->call_serial_num = ntohl(*(ALIGNED_CAST(u_int32_t*)value_buf));
                            break;		
                                                            
                    case L2TP_AVP_BEARER_TYPE:
                            /*params->bearer_type = ntohl(*((u_int32_t*)value_buf));*/
                            break;	
                                                            
                    case L2TP_AVP_CALLED_NUM:
    #if 0
                            if (value_len > MAX_CALLED_NUM_SIZE)
                                    value_len = MAX_CALLED_NUM_SIZE;
                            bcopy(value_buf, params->called_num, value_len);
                            params->called_num[value_len] = 0;
    #endif
                            break;
    
                    case L2TP_AVP_CALLING_NUM:
    #if 0
                            if (value_len > MAX_CALLING_NUM_SIZE)
                                    value_len = MAX_CALLING_NUM_SIZE;
                            bcopy(value_buf, params->calling_num, value_len);
                            params->calling_num[value_len] = 0;
    #endif
                            break;
    
                    case L2TP_AVP_SUB_ADDRESS:
    #if 0
                            if (value_len > MAX_SUB_ADDRESS_SIZE)
                                    value_len = MAX_SUB_ADDRESS_SIZE;
                            bcopy(value_buf, params->sub_address, value_len);
                            params->sub_address[value_len] = 0;
    #endif
                            break;
                                                    
                    case L2TP_AVP_PHYS_CHANNEL_ID:
                            /*params->phys_channel_id = ntohl(*((u_unt32_t*)value_buf));*/
                            break;	
                                            
                    case L2TP_AVP_FRAMING_TYPE:
                            /*params->framing_type = ntohl(*((u_unt32_t*)value_buf));*/
                            break;	
                                                            
                    case L2TP_AVP_TX_CONNECT_SPEED:
                            /*params->tx_connect_speed = ntohl(*((u_unt32_t*)value_buf));*/
                            break;	
                            
                    case L2TP_AVP_RX_CONNECT_SPEED:
                            /*params->tx_connect_speed = ntohl(*((u_unt32_t*)value_buf));*/
                            break;	
                                                    
                    case L2TP_AVP_PRIVATE_GROUP_ID:
    #if 0
                            if (value_len > MAX_PRIV_GROUP_ID_SIZE)
                                    value_len = MAX_PRIV_GROUP_ID_SIZE;
                            bcopy(value_buf, params->priv_group_id, value_len);
                            params->priv_group_id[value_len] = 0;
    #endif
                            break;
                            
                    case L2TP_AVP_SEQ_REQUIRED:
                            params->seq_required = 1;
                            break;
            
                    case L2TP_AVP_INIT_RECVD_CONFREQ:
                    case L2TP_AVP_LAST_SENT_CONFREQ:
                    case L2TP_AVP_LAST_RECVD_CONFREQ:
                            /* ignore */
                            break;
                            
                    case L2TP_AVP_PROXY_AUTH_TYPE:
                    case L2TP_AVP_PROXY_AUTH_NAME:
                    case L2TP_AVP_PROXY_AUTH_CHALLENGE:
                    case L2TP_AVP_PROXY_AUTH_ID:
                    case L2TP_AVP_PROXY_AUTH_RESP:
                            /* ignore for now */
                            break;

                    case L2TP_AVP_ACCM:
                            /* ignore */
                            break;

                    case L2TP_AVP_RAND_VECT:
                            if (value_len <= MAX_RANDOM_VECTOR_SIZE) {
                                    random_vector_len = value_len;
                                    bcopy(value_buf, random_vector, value_len);
                            } else {
                                    error("L2TP received larger than supported Random Vector\n");
                                    return -1;
                            }
                            break;
                            
                    case L2TP_AVP_RESULT_CODE:
                            if (value_len < sizeof(u_int16_t)) {
                                    error("L2TP received Result Code AVP with invalid length\n");
                                    return -1;
                            }
                            params->result_code = ntohs(*(ALIGNED_CAST(u_int16_t*)value_buf));
                            if (value_len >= sizeof(u_int32_t)) {
                                    params->error_code = ntohs(*(ALIGNED_CAST(u_int16_t*)value_buf + 1));
                                    if (value_len -= sizeof(u_int32_t)) {
                                            if (value_len >= sizeof(params->error_message))
                                                    value_len = sizeof(params->error_message) - 1;
                                            bcopy(value_buf + sizeof(u_int32_t), params->error_message, value_len);
                                    }
                                    params->error_message[value_len] = 0;
                            }
                            break;	

                    case L2TP_AVP_CALL_ERRORS:
                            #define AVP_CALL_ERRORS_SIZE        26
                            #define AVP_CALL_ERRORS_COUNT       6
                            
                            if (value_len != AVP_CALL_ERRORS_SIZE) {
                                    error("L2TP received Call Errors AVP with invalid length\n");
                                    return -1;
                            }
                            {
                                u_int32_t aligned_values[AVP_CALL_ERRORS_COUNT];
                                /* copy data to aligned buf leaving off the reserved field */
                                memcpy(aligned_values, value_buf + sizeof(u_int16_t), AVP_CALL_ERRORS_COUNT * sizeof(u_int32_t));
                                
                                params->crc_errors = ntohl(aligned_values[0]);
                                params->framing_errors = ntohl(aligned_values[1]);
                                params->hardware_overruns = ntohl(aligned_values[2]);
                                params->buffer_overruns = ntohl(aligned_values[3]);
                                params->timeout_errors = ntohl(aligned_values[4]);
                                params->alignment_errors = ntohl(aligned_values[5]);
                            }
                            break;


                    case L2TP_AVP_CAUSE_CODE:
                            if (value_len < sizeof(u_int16_t) + sizeof(u_int8_t)) {
                                    error("L2TP received Cause Code AVP with invalid length\n");
                                    return -1;
                            }
                            params->cause_code = ntohs(*(ALIGNED_CAST(u_int16_t*)value_buf));
                            params->cause_message = *(value_buf + sizeof(u_int16_t));
                            if (value_len -= (sizeof(u_int16_t) + sizeof(u_int8_t))) {
                                    if (value_len >= sizeof(params->advisory_message))
                                            value_len = sizeof(params->advisory_message) - 1;
                                    bcopy(value_buf + sizeof(u_int16_t) + sizeof(u_int8_t), params->advisory_message, value_len);
                            }
                            params->advisory_message[value_len] = 0;
                            break;



                    default:
                            /* ??? log error - unknown AVP type */
                            if (avp_flags & L2TP_AVP_FLAGS_M) {
                                    error("L2TP received unknown mandatory AVP... AVP type = %d\n", avp_type);
                                    return -1;
                            } else {
                                    /* log unknown AVP */
                            }
                            break;
            }
    }
    if (value_buf != NULL)
        free(value_buf);

                            
    /*
        * Message-type specific processing
        * Check if all required AVPs have been received
        */
    switch (msg_type) {
            case L2TP_HELLO:
                if ((avp_bitmap & L2TP_HELLO_BITMAP) != L2TP_HELLO_BITMAP) {
                    error("L2TP received Hello control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_SCCRQ:			
                if ((avp_bitmap & L2TP_SCCRQ_BITMAP) != L2TP_SCCRQ_BITMAP) {
                    error("L2TP received SCCRQ control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_SCCRP:
                if ((avp_bitmap & L2TP_SCCRP_BITMAP) != L2TP_SCCRP_BITMAP) {
                    error("L2TP received SCCRP control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_SCCCN:
                if ((avp_bitmap & L2TP_SCCCN_BITMAP) != L2TP_SCCCN_BITMAP) {
                    error("L2TP received SCCN control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_StopCCN:			
                if ((avp_bitmap & L2TP_StopCCN_BITMAP) != L2TP_StopCCN_BITMAP) {
                    error("L2TP received StopCCN control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_ICRQ:
                if ((avp_bitmap & L2TP_ICRQ_BITMAP) != L2TP_ICRQ_BITMAP) {
                    error("L2TP received ICRQ control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_ICRP:				
                if ((avp_bitmap & L2TP_ICRP_BITMAP) != L2TP_ICRP_BITMAP) {
                    error("L2TP received ICRP control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_ICCN:
                if ((avp_bitmap & L2TP_ICCN_BITMAP) != L2TP_ICCN_BITMAP) {
                    error("L2TP received ICCN control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_CDN:		
                if ((avp_bitmap & L2TP_CDN_BITMAP) != L2TP_CDN_BITMAP) {
                    error("L2TP received CDN control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_WEN:
                if ((avp_bitmap & L2TP_WEN_BITMAP) != L2TP_WEN_BITMAP) {
                    error("L2TP received WEN control message with missing mandatory parameters\n");
                    return -1;
                }
                break;
            case L2TP_SLI:
                if ((avp_bitmap & L2TP_SLI_BITMAP) != L2TP_SLI_BITMAP) {
                    error("L2TP received SLI control message with missing mandatory parameters\n");
                    return -1;
                }
                break;

            case L2TP_OCRQ:
            case L2TP_OCRP:
            case L2TP_OCCN:
                    error("L2TP reveived unsupported message type\n");
                    return -1;
                    break;
            
            default:
                    if (mandatory_msg) {
                            error("L2TP received unknown mandatory message type\n");
                            return -1;
                    }
                    /* ??? log error */
                    msg_type = 0;
                    break;
            
    }

    *type = msg_type;

    if (msg_type != L2TP_HELLO)
        dbglog("L2TP received %s\n", msg_type_str(msg_type));       
    if (expected_type && msg_type != expected_type)  {
        error("L2TP received invalid message (expected %s, received %s)", msg_type_str(expected_type), msg_type_str(msg_type));
        return -1;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
char *msg_type_str(u_int16_t msg_type)
{
   static char text[255];

    switch (msg_type) {
        case ZLB_ACK: 		return "ZLB_ACK";	/* shouldn't happen */
        case L2TP_SCCRQ: 	return "SCCRQ";
        case L2TP_SCCRP: 	return "SCCRP";
        case L2TP_SCCCN: 	return "SCCCN";
        case L2TP_StopCCN:      return "StopCCN";
        case L2TP_HELLO: 	return "Hello";
        case L2TP_OCRQ: 	return "OCRQ";
        case L2TP_OCCN: 	return "OCCN";
        case L2TP_ICRQ: 	return "ICRQ";
        case L2TP_ICRP: 	return "ICRP";
        case L2TP_ICCN: 	return "ICCN";
        case L2TP_CDN: 		return "CDN";
        case L2TP_WEN: 		return "WEN";
        case L2TP_SLI: 		return "SLI";
   }
   snprintf(text, sizeof(text), "unknown message (type = 0x%x)", msg_type);
    return (text);
}

/* -----------------------------------------------------------------------------
	unhide_avp
----------------------------------------------------------------------------- */
int unhide_avp(u_int8_t* value_buf, u_int16_t* value_len, u_int8_t* unhide_buf, size_t buf_len)
{
    /* not implemented yet */
    return -1;
}

/* -----------------------------------------------------------------------------
	Prepare SCCRQ AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
size_t prepare_SCCRQ(u_int8_t* buf, size_t len, struct l2tp_parameters* params)
{	
    size_t	used;
    
    if ( (used = prepare_SCCRX(buf, len, L2TP_SCCRQ, params)) == 0)
            return 0;
    
#if 0	
    buf += used;
    len -= used;
            
    /* add additional AVPs here - if required */

#endif
    
    return used;
}

/* -----------------------------------------------------------------------------
	Prepare SCCRP AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
size_t prepare_SCCRP(u_int8_t* buf, size_t len, struct l2tp_parameters* params)
{
    size_t	used;
    
    if ( (used = prepare_SCCRX(buf, len, L2TP_SCCRP, params)) == 0)
            return 0;
            
#if 0	
    buf += used;
    len -= used;
            
    /* add additional AVPs here - if required */

#endif
    
    return used;
}
		
/* -----------------------------------------------------------------------------
	Prepare common AVPs for sending SCCRQ and SCCRP
		returns space used
----------------------------------------------------------------------------- */
size_t prepare_SCCRX(u_int8_t* buf, size_t len, u_int16_t type, struct l2tp_parameters* params)
{
    size_t	size;
    size_t	free_space = len;
    
    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;
    
    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, type, L2TP_AVP_FLAGS_M))
            return 0;

    /* Protocol Version */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_PROTO_VERS, params->protocol_vers, L2TP_AVP_FLAGS_M))
            return 0;

    /* Framing Capabililites */
    if (make_avp_long(&buf, &free_space, L2TP_AVP_FRAMING_CAPS, params->framing_caps, L2TP_AVP_FLAGS_M))
            return 0;

    /* Host name */
    size = strlen((char*)params->host_name) + 1;
    if (make_avp_hdr(&buf, &free_space, L2TP_AVP_HOST_NAME, size, L2TP_AVP_FLAGS_M))
            return 0;
    bcopy(params->host_name, buf, size);
    buf += size;

    /* Assigned Tunnel ID */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_TUNNEL_ID, params->tunnel_id, L2TP_AVP_FLAGS_M))
            return 0;
    
    /* Receive Window Size */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_WINDOW_SIZE, params->window_size, L2TP_AVP_FLAGS_M))
            return 0;
    
    return len - free_space;
}

/* -----------------------------------------------------------------------------
	Prepare SCCN AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
size_t prepare_SCCCN(u_int8_t* buf, size_t len)
{
    size_t	free_space = len;
    
    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;

    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, L2TP_SCCCN, L2TP_AVP_FLAGS_M))
            return 0;

    return len - free_space;
}

/* -----------------------------------------------------------------------------
	Prepare StopCCN AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
int prepare_StopCCN(u_int8_t* buf, size_t len, struct l2tp_parameters* params)
{	
    size_t	free_space = len;
    u_int16_t	avp_size;
    u_int16_t	str_size = 0;

    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;

    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, L2TP_StopCCN, L2TP_AVP_FLAGS_M))
            return 0;

    /* Assigned Tunnel ID */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_TUNNEL_ID, params->tunnel_id, L2TP_AVP_FLAGS_M))
            return 0;
    
    /* Result Code */
    
    avp_size = L2TP_AVP_HDR_SIZE + sizeof(u_int16_t);
    if (params->error_code != 0)
            avp_size += (sizeof(u_int16_t) + (str_size = strlen((char*)params->error_message)));
    if (make_avp_hdr(&buf, &free_space, L2TP_AVP_RESULT_CODE, avp_size, L2TP_AVP_FLAGS_M))
            return 0;
    memcpy(buf, &params->result_code, sizeof(u_int16_t));		// Wcast-align fix - memcpy for unaligned access
	buf += sizeof(u_int16_t);
    if (params->error_code)
    {
            memcpy(buf, &params->error_code, sizeof(u_int16_t));	// Wcast-align fix - memcpy for unaligned access
			buf += sizeof(u_int16_t);
            memcpy(buf, params->error_message, str_size);			// Wcast-align fix - memcpy for unaligned access
            buf += str_size;
    }
    
    return len - free_space;	
}

/* -----------------------------------------------------------------------------
	Prepare Hello AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
int prepare_Hello(u_int8_t* buf, size_t len)
{	
    size_t	free_space = len;
    
    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;

    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, L2TP_HELLO, L2TP_AVP_FLAGS_M))
            return 0;
    
    return len - free_space;
}

/* -----------------------------------------------------------------------------
	Prepare ICRQ AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
int prepare_ICRQ(u_int8_t* buf, size_t len, struct l2tp_parameters* params)
{	
    size_t	free_space = len;
    
    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;

    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, L2TP_ICRQ, L2TP_AVP_FLAGS_M))
            return 0;
    
    /* Assigned Session ID */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_SESSION_ID, params->session_id, L2TP_AVP_FLAGS_M))
            return 0;
            
    /* Call Serial Number */
    if (make_avp_long(&buf, &free_space, L2TP_AVP_CALL_SERIAL_NUM, params->call_serial_num, L2TP_AVP_FLAGS_M))
            return 0;
    
    return len - free_space;	
}

/* -----------------------------------------------------------------------------
	Prepare ICRP AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
int prepare_ICRP(u_int8_t* buf, size_t len, struct l2tp_parameters* params)
{	
    size_t	free_space = len;
    
    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;

    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, L2TP_ICRP, L2TP_AVP_FLAGS_M))
            return 0;
    
    /* Assigned Session ID */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_SESSION_ID, params->session_id, L2TP_AVP_FLAGS_M))
            return 0;
            
    return len - free_space;	
}

/* -----------------------------------------------------------------------------
	Prepare ICCN AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
size_t prepare_ICCN(u_int8_t* buf, size_t len, struct l2tp_parameters* params)
{
    size_t	free_space = len;
    
    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;

    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, L2TP_ICCN, L2TP_AVP_FLAGS_M))
            return 0;
    
    /* TX Connect Speed */
    if (make_avp_long(&buf, &free_space, L2TP_AVP_TX_CONNECT_SPEED, params->tx_connect_speed, L2TP_AVP_FLAGS_M))
            return 0;
    
    /* Framing Type */
    if (make_avp_long(&buf, &free_space, L2TP_AVP_FRAMING_TYPE, params->framing_type, L2TP_AVP_FLAGS_M))
            return 0;
    
    return len - free_space;
}

/* -----------------------------------------------------------------------------
	Prepare CDN AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
size_t prepare_CDN(u_int8_t* buf, size_t len, struct l2tp_parameters* params)
{
    size_t	free_space = len;
    u_int16_t	avp_size;
    u_int16_t	str_size = 0;
    
    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;

    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, L2TP_CDN, L2TP_AVP_FLAGS_M))
            return 0;
    
    /* Assigned Session ID */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_SESSION_ID, params->session_id, L2TP_AVP_FLAGS_M))
            return 0;
    
    /* Result Code */
    avp_size = L2TP_AVP_HDR_SIZE + sizeof(u_int16_t);
    if (params->error_code != 0)
            avp_size += (sizeof(u_int16_t) + (str_size = strlen((char*)params->error_message)));
    if (make_avp_hdr(&buf, &free_space, L2TP_AVP_RESULT_CODE, avp_size, L2TP_AVP_FLAGS_M))
            return 0;
    memcpy(buf, &params->result_code, sizeof(u_int16_t));	// Wcast-align fix - memcpy for unaligned access
	buf += sizeof(u_int16_t);
    if (params->error_code)
    {
            memcpy(buf, &params->error_code, sizeof(u_int16_t));	// Wcast-align fix - memcpy for unaligned access
			buf += sizeof(u_int16_t);
            memcpy(buf, params->error_message, str_size);			// Wcast-align fix - memcpy for unaligned access
            buf += str_size;
    }
    
    /* Cause Code - optional */
    //avp_size = L2TP_AVP_HDR_SIZE + sizeof(u_int16_t);
    //if (params->cause_message != 0)
    //	avp_size += (sizeof(u_int8_t) + (str_size = strlen(params->advisory_message)));
    //if (make_avp_hdr(&buf, &free_space, L2TP_AVP_CAUSE_CODE, avp_size, L2TP_AVP_FLAGS_M))
    //	return 0;
    //if (params->cause_message)
    //{
    //	*(((u_int8_t*)buf)++) = params->cause_message;
    //	bcopy(params->error_message, buf, str_size);
    //	buf += str_size;
    //}
    
    return len - free_space;
}

#ifdef UNUSED
/* -----------------------------------------------------------------------------
	Prepare WEN AVPs for sending
		returns space used
----------------------------------------------------------------------------- */
size_t prepare_WEN(u_int8_t* buf, size_t len, struct l2tp_parameters* params)
{
    size_t		free_space = len;
    
    buf += L2TP_CNTL_HDR_SIZE;
    free_space -= L2TP_CNTL_HDR_SIZE;

    /* Message Type */
    if (make_avp_short(&buf, &free_space, L2TP_AVP_MSG_TYPE, L2TP_WEN, L2TP_AVP_FLAGS_M)) 
            return 0;
    
    /* Call Errors */
    if (make_avp_hdr(&buf, &free_space, L2TP_AVP_CALL_ERRORS, 26, L2TP_AVP_FLAGS_M))
            return 0;
    
    ((u_int16_t*)buf)++;	/* skip reserved field */
    *(((u_int32_t*)buf)++) = htonl(params->crc_errors);
    *(((u_int32_t*)buf)++) = htonl(params->framing_errors);
    *(((u_int32_t*)buf)++) = htonl(params->hardware_overruns);
    *(((u_int32_t*)buf)++) = htonl(params->buffer_overruns);
    *(((u_int32_t*)buf)++) = htonl(params->timeout_errors);
    *(((u_int32_t*)buf)++) = htonl(params->alignment_errors);

    return len - free_space;
}
#endif

/* -----------------------------------------------------------------------------
	Make an AVP header in the specified buffer
		updates the buf to point to value field
		decrements len with size of header and value
----------------------------------------------------------------------------- */
int make_avp_hdr(u_int8_t** buf, size_t* len, u_int16_t type, size_t value_size, u_int16_t flags)
{
    u_int16_t val;
    
    if (*len < L2TP_AVP_HDR_SIZE + value_size)
            return -1;
    
    *len -= (L2TP_AVP_HDR_SIZE + value_size);
    val = htons((L2TP_AVP_HDR_SIZE + value_size) | flags);
    memcpy(*buf, &val, sizeof(val));        // Wcast-align fix - memcpy for unaligned move
	*buf += sizeof(u_int16_t);
    val = 0;
    memcpy(*buf, &val, sizeof(val));		// Wcast-align fix - memcpy for unaligned move
	*buf += sizeof(u_int16_t);
    val = htons(type);
    memcpy(*buf, &val, sizeof(val));		// Wcast-align fix - memcpy for unaligned move
	*buf += sizeof(u_int16_t);
    
    return 0;
}

/* -----------------------------------------------------------------------------
	Make an AVP for a 2 byte value
		updates the buf to point to the next AVP location
		decrements len
----------------------------------------------------------------------------- */
int make_avp_short(u_int8_t** buf, size_t* len, u_int16_t type, u_int16_t value, u_int16_t flags)
{
    u_int16_t val;
    
    if (*len < L2TP_AVP_HDR_SIZE + sizeof(u_int16_t))
            return -1;

    *len -= (L2TP_AVP_HDR_SIZE + sizeof(u_int16_t));
    val = htons((L2TP_AVP_HDR_SIZE + sizeof(u_int16_t)) | flags);
    memcpy(*buf, &val, sizeof(val));        // Wcast-align fix - memcpy for unaligned move
    *buf += sizeof(u_int16_t);
    val = 0;
    memcpy(*buf, &val, sizeof(val));		// Wcast-align fix - memcpy for unaligned move
    *buf += sizeof(u_int16_t);
    val = htons(type);
    memcpy(*buf, &val, sizeof(val));		// Wcast-align fix - memcpy for unaligned move
    *buf += sizeof(u_int16_t);
    val = htons(value);
    memcpy(*buf, &val, sizeof(val));		// Wcast-align fix - memcpy for unaligned move
	*buf += sizeof(u_int16_t);
    
    return 0;
}

/* -----------------------------------------------------------------------------
	Make an AVP for a 4 byte value
		updates the buf to point to the next AVP location
		decrements len
----------------------------------------------------------------------------- */
int make_avp_long(u_int8_t** buf, size_t* len, u_int16_t type, u_int32_t value, u_int16_t flags)
{
    u_int16_t short_val;
    u_int32_t long_val;
    
    if (*len < L2TP_AVP_HDR_SIZE + sizeof(u_int32_t))
            return -1;
    
    *len -= (L2TP_AVP_HDR_SIZE + sizeof(u_int32_t));
    short_val = htons((L2TP_AVP_HDR_SIZE + sizeof(u_int32_t)) | flags);
    memcpy(*buf, &short_val, sizeof(short_val));	// Wcast-align fix - memcpy for unaligned move
	*buf += sizeof(u_int16_t);
    short_val = 0;
    memcpy(*buf, &short_val, sizeof(short_val));	// Wcast-align fix - memcpy for unaligned move
	*buf += sizeof(u_int16_t);
    short_val = htons(type);
    memcpy(*buf, &short_val, sizeof(short_val));	// Wcast-align fix - memcpy for unaligned move
   	*buf += sizeof(u_int16_t);
    long_val = htonl(value);
    memcpy(*buf, &long_val, sizeof(long_val));		// Wcast-align fix - memcpy for unaligned move
	*buf += sizeof(u_int32_t);
    
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_send(int fd, u_int8_t* buf, int len, u_int16_t session_id, struct sockaddr *to, char *text)
{
    size_t 		result;
    struct sockaddr	addr;
    u_int16_t   tmp;
    
    if (len <= 0) {
        error("L2TP incorrect size when trying to send %s\n", text);
        return 0;
    }

    /* specify the session id to send to. All other fields are taken care of in the extension */
    tmp = htons(session_id);
    memcpy(buf + (3 * sizeof(u_int16_t)), &tmp, sizeof(u_int16_t));     // Wcast-align fix - memcpy for unaligned access

    /* null address structure */
    bzero(&addr, sizeof(addr));
    addr.sa_len = sizeof(addr);
    
    while ((result = sendto(fd, buf, len, 0, to ? to : &addr, to ? to->sa_len : addr.sa_len)) == -1) {
        if (kill_link)
            return -2;
        if (errno != EINTR) {
            error("L2TP error sending %s (%m)", text);
            return -1;
        }
    }
    
    if (strcmp(text, "Hello"))
        dbglog("L2TP sent %s\n", text);

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int l2tp_recv(int fd, u_int8_t* buf, int len, int *outlen, struct sockaddr *from, int timeout, char *text)
{	
    socklen_t		addrlen = from->sa_len;
    ssize_t 		result;
    struct timeval	tv;
    fd_set		rset;
    int			maxfd;
    
    if (timeout) {
        for (;;) {
            FD_ZERO(&rset);
            FD_SET(fd, &rset);
            maxfd = fd + 1;
            tv.tv_sec = timeout;
            tv.tv_usec = 0;
            if ((result = select(maxfd, &rset, 0, 0, timeout != -1 ? &tv : 0)) == 0) {
				if (debug > 1)
					dbglog("L2TP timeout receiving %s\n", text);
                return -1;
			}
            if (kill_link)
                return -2;
            if (result > 0)
                break;
            if (errno != EINTR) 
                goto fail;
        }
    }
    
    while ((result = recvfrom(fd, buf, len, MSG_DONTWAIT, from, &addrlen)) < 0) { 
    	if (kill_link)
            return -2;
        if (errno != EINTR)
            goto fail;
    }
    
    *outlen = result;
    return 0;
    
fail:
    error("L2TP receive error trying to read %s (%m)\n", text);
    return -1;
}
