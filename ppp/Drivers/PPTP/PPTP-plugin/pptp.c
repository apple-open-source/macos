/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
 *  pptp.c
 *  ppp
 *
 *  Created by Christophe Allie on Thu May 23 2002.
 *  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
 *
 */

/* -----------------------------------------------------------------------------
 *
 *  Theory of operation :
 *
 *  PPTP protocol speficic function for the pppd plugin.
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
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <pwd.h>
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <net/dlil.h>
#include <net/if.h>

#include "../../../Helpers/pppd/pppd.h"
#include "pptp.h"

extern int 		kill_link;

/* -----------------------------------------------------------------------------
 Definitions
----------------------------------------------------------------------------- */

void pptp_received_echo_reply(u_int32_t identifier, u_int8_t result, u_int8_t error);

char *control_msgs[] = {
    "", 					/* 0 */
    "Start Control Connection Request",		/* 1 */
    "Start Control Connection Reply",		/* 2 */
    "Stop Control Connection Request",		/* 3 */
    "Stop Control Connection Reply",		/* 4 */
    "Echo Request",				/* 5 */
    "Echo Reply",				/* 6 */
    "Outgoing Call Request",			/* 7 */
    "Outgoing Call Reply",			/* 8 */
    "Incoming Call Request",			/* 9 */
    "Incoming Call Reply",			/* 10 */
    "Incoming Call Connected",			/* 11 */
    "Call Clear Request",			/* 12 */
    "Call Disconnect Notify",			/* 14 */
    "Wan Error Notify",				/* 14 */
    "Set Link Info"				/* 15 */
};

/* -----------------------------------------------------------------------------
send a PPTP control request or reply
----------------------------------------------------------------------------- */
int pptp_send(int fd, u_int16_t msg, void *req, u_int16_t reqlen, char *text)
{
    u_char              buf[256]; /* buffer large enough to send pptp control packets */
    struct pptp_header	*hdr = (struct pptp_header *)buf;
    int                 n, sent;

    if ((sizeof(*hdr) + reqlen) > sizeof(buf)) {
	// should not happen
	error("PPTP length error when sending %s : %m\n", text);
	return -1;
    }

    bzero(hdr, sizeof(*hdr));
    hdr->len = htons(sizeof(*hdr) + reqlen);
    hdr->pptp_msgtype = htons(PPTP_CONTROL_MSG);
    hdr->magic_cookie = htonl(PPTP_MAGIC_COOKIE);
    hdr->ctrl_msgtype = htons(msg);

    bcopy(req, buf + sizeof(*hdr), reqlen);

    sent = 0;
    while ((n = write(fd, buf + sent, ntohs(hdr->len) - sent)) != (ntohs(hdr->len) - sent)) {
	if (n == -1 && errno != EINTR) {
            error("PPTP error when sending %s : %m\n", text);
            return -1;
        }
        if (kill_link) {
            error("PPTP kill_link when sending\n");
            return -2;
        }
        if (n > 0)
            sent += n;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int
readn(int ref, void *data, int len)
{
    int 	n, left = len;
    void 	*p = data;
    
    while (left > 0) {
        if ((n = read(ref, p, left)) < 0) {
            if (errno == EWOULDBLOCK) 
                return (len - left);
            if (errno != EINTR) {
                error("PPTP error when reading socket : %m\n");
                return -1;
            }
            n = 0;
        }
        else if (n == 0) {
            error("PPTP error when reading socket : EOF\n");
            return -1; /* EOF */
        }
            
        left -= n;
        p += n;
    }
    return (len - left);
}        

/* -----------------------------------------------------------------------------
receives a PPTP control reply
----------------------------------------------------------------------------- */
int pptp_recv(int fd, u_int16_t msg, void *rep, u_int16_t replen, char *text)
{
    struct pptp_header	hdr;
    int			err;
    
    if ((err = read(fd, &hdr, sizeof(hdr))) != sizeof(hdr)) {
        if (kill_link)
            return -2;
        if (err == -1) 
            error("PPTP error when reading header for %s : %m\n", text);
        return -1;
    }
    if (ntohs(hdr.ctrl_msgtype) != msg) {
        error("PPTP didn't get %s (got message : %d)\n", text, hdr.ctrl_msgtype);
        return -3;
    }
    if ((err = read(fd, rep, replen)) != replen) {
        if (kill_link)
            return -2;
        if (err == -1) 
            error("PPTP error when reading %s : %m\n", text);
        return -1;
    }
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pptp_outgoing_call(int fd, 
    u_int16_t ourcallid, u_int16_t ourwindow, u_int16_t ourppd,
    u_int16_t *peercallid, u_int16_t *peerwindow, u_int16_t *peerppd)
{
    struct pptp_start_control_request	ctl_req;
    struct pptp_start_control_reply	ctl_reply;
    struct pptp_outgoing_call_request	out_req;
    struct pptp_outgoing_call_reply	out_reply;
    struct pptp_set_link_info		link_info;
    int 				err;
        
    /* send the start request */
    bzero(&ctl_req, sizeof(ctl_req));
    ctl_req.proto_vers = htons(PPTP_VERSION);
    ctl_req.framing_caps = htonl(PPTP_ASYNC_FRAMING);
    ctl_req.bearer_caps = htonl(PPTP_ANALOG_ACCESS);
    if (err = pptp_send(fd, PPTP_START_CONTROL_CONNECTION_REQUEST, &ctl_req, sizeof(ctl_req), "start_control_connection_request")) {
        if (err == -2)
            return -2;
        return -1;
    }

    /* read the start reply */
    if (err = pptp_recv(fd, PPTP_START_CONTROL_CONNECTION_REPLY, 
            &ctl_reply, sizeof(ctl_reply), "start_control_connection_reply")) {
        if (err == -2)
            return -2;
        if (err == -3)
            return EXIT_PPTP_PROTOCOLERROR;
        return -1;
    }
    if (ctl_reply.result_code != PPTP_RESULT_SUCCESS && ctl_reply.result_code != 0 /* radar 4395192 */) {
        error("PPTP start_connection_control request failed, got result = %d, error = %d\n", ctl_reply.result_code, ctl_reply.error_code);
        return EXIT_PPTP_PROTOCOLERROR;
    }

    /* send the outgoing call request */
    bzero(&out_req, sizeof(out_req));
    out_req.call_id = htons(ourcallid);
    out_req.min_bps = htonl(0x12c);	// ???
    out_req.max_bps = htonl(0x5f5e100);	// ???
    out_req.bearer_type = htonl(PPTP_ANALOG_ACCESS + PPTP_DIGITAL_ACCESS);
    out_req.framing_type = htonl(PPTP_ASYNC_FRAMING + PPTP_SYNC_FRAMING);
    out_req.recv_window = htons(ourwindow);
    out_req.processing_delay = htons(ourppd);
    if (err = pptp_send(fd, PPTP_OUTGOING_CALL_REQUEST, &out_req, sizeof(out_req), "outgoing_call_request")) {
        if (err == -2)
            return -2;
        return -1;
    }

    /* read the out reply */
    if (err = pptp_recv(fd, PPTP_OUTGOING_CALL_REPLY, 
            &out_reply, sizeof(out_reply), "outgoing_call_reply")) {
        if (err == -2)
            return -2;
        if (err == -3)
            return EXIT_PPTP_PROTOCOLERROR;
        return -1;
    }
    if (out_reply.result_code != PPTP_OUTGOING_CALL_RESULT_CONNECTED) {
        error("PPTP outgoing_call request failed, got result = %d, error = %d\n", out_reply.result_code, out_reply.error_code);
        return EXIT_PPTP_PROTOCOLERROR;
    }

    /* call succedeed ! */
    *peercallid = ntohs(out_reply.call_id);
    *peerwindow = ntohs(out_reply.recv_window);
    *peerppd = ntohs(out_reply.processing_delay);
    
    /* send set_link_info */
    bzero(&link_info, sizeof(link_info));
    link_info.peer_call_id = htons(*peercallid);
    link_info.send_accm = htonl(0xFFFFFFFF);
    link_info.recv_accm = htonl(0xFFFFFFFF);
    if (err = pptp_send(fd, PPTP_SET_LINK_INFO, &link_info, sizeof(link_info), "set_link_info_request")) {
        if (err == -2)
            return -2;
        return -1;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pptp_incoming_call(int fd,
    u_int16_t ourcallid, u_int16_t ourwindow, u_int16_t ourppd,
    u_int16_t *peercallid, u_int16_t *peerwindow, u_int16_t *peerppd)
{
    struct pptp_start_control_request	ctl_req;
    struct pptp_start_control_reply	ctl_reply;
    struct pptp_outgoing_call_request	out_req;
    struct pptp_outgoing_call_reply	out_reply;
        
    /* read the start request */
    if (pptp_recv(fd, PPTP_START_CONTROL_CONNECTION_REQUEST, 
            &ctl_req, sizeof(ctl_req), "start_control_connection_request")) {
        return -1;
    }
    
    /* send the start control reply */
    bzero(&ctl_reply, sizeof(ctl_reply));
    ctl_reply.proto_vers = htons(PPTP_VERSION);
    ctl_reply.result_code = PPTP_RESULT_SUCCESS;
    ctl_reply.framing_caps = htonl(PPTP_ASYNC_FRAMING | PPTP_SYNC_FRAMING);
    ctl_reply.bearer_caps = htonl(PPTP_ANALOG_ACCESS | PPTP_DIGITAL_ACCESS);
    ctl_reply.max_channels = htons(1);
    ctl_reply.firmware_rev = htons(1);
    gethostname(ctl_reply.hostname, 64);
    strlcpy(ctl_reply.vendor, PPTP_VENDOR, sizeof(ctl_reply.vendor));
    if (pptp_send(fd, PPTP_START_CONTROL_CONNECTION_REPLY, &ctl_reply, sizeof(ctl_reply), "start_control_connection_reply"))
        return -1;

    /* read the out request */
    if (pptp_recv(fd, PPTP_OUTGOING_CALL_REQUEST, 
            &out_req, sizeof(out_req), "outgoing_call_request")) {
        return -1;
    }

    *peercallid = ntohs(out_req.call_id);
    *peerwindow = ntohs(out_req.recv_window);
    *peerppd = ntohs(out_req.processing_delay);

    /* send the outgoing call reply */
    bzero(&out_reply, sizeof(out_reply));
    out_reply.call_id = htons(ourcallid);
    out_reply.peer_call_id = htons(*peercallid);
    out_reply.result_code = PPTP_OUTGOING_CALL_RESULT_CONNECTED;
	// Already in network byte order
    out_reply.connect_speed = out_req.max_bps; // ???
    out_reply.recv_window = htons(ourwindow);
    out_reply.processing_delay = htons(ourppd);
    if (pptp_send(fd, PPTP_OUTGOING_CALL_REPLY, &out_reply, sizeof(out_reply), "outgoing_call_reply")) {
        return -1;
    }
        
    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
int pptp_echo(int fd, u_int32_t identifier)
{
    struct pptp_echo_request 		echo_req;
    int 				err;
        
    /* send the echo request */
    echo_req.identifier = htonl(identifier);
    if (err = pptp_send(fd, PPTP_ECHO_REQUEST, &echo_req, sizeof(echo_req), "echo_request")) {
        if (err == -2)
            return -2;
        return -1;
    }

    return 0;
}

/* ----------------------------------------------------------------------------- 
----------------------------------------------------------------------------- */
int pptp_data_in(int fd)
{
    struct pptp_header		header;
    struct pptp_echo_request 	echo_req;
    struct pptp_echo_reply 	echo_reply;
    struct pptp_set_link_info 	info_req;
    int				err;
    
    if ((err = readn(fd, &header, sizeof(header))) != sizeof(header)) {
        error("PPTP error when reading header : read %d, expected %d bytes\n", err, sizeof(header));
        return -1;
    }
        
    switch (ntohs(header.ctrl_msgtype)) {
        case PPTP_ECHO_REQUEST:
            // read the identifier
            if ((err = readn(fd, &echo_req, sizeof(echo_req))) != sizeof(echo_req)) {
                error("PPTP error when reading echo request : read %d, expected %d bytes\n", err, sizeof(echo_req));
                return -1;
            }
            bzero(&echo_reply, sizeof(echo_reply));
			// Already in network byte order
            echo_reply.identifier = echo_req.identifier;
            echo_reply.result_code = PPTP_RESULT_SUCCESS;
            if (pptp_send(fd, PPTP_ECHO_REPLY, &echo_reply, sizeof(echo_reply), "echo_reply")) {
                return -1;
            }
            break;
            
        case PPTP_ECHO_REPLY:
            // read the identifier
            if ((err = readn(fd, &echo_reply, sizeof(echo_reply))) != sizeof(echo_reply)) {
                error("PPTP error when reading echo echo_reply : read %d, expected %d bytes\n", err, sizeof(echo_reply));
                return -1;
            }
            pptp_received_echo_reply(ntohl(echo_reply.identifier), echo_reply.result_code, echo_reply.error_code);
            break;
            
        case PPTP_SET_LINK_INFO:
            // ignore
            if ((err = readn(fd, &info_req, sizeof(info_req))) != sizeof(info_req)) {
                error("PPTP error when reading set_info_link request : read %d, expected %d bytes\n", err, sizeof(info_req));
                return -1;
            }                
            break;
            
        case PPTP_START_CONTROL_CONNECTION_REQUEST:
        case PPTP_START_CONTROL_CONNECTION_REPLY:
        case PPTP_STOP_CONTROL_CONNECTION_REQUEST:
        case PPTP_STOP_CONTROL_CONNECTION_REPLY:
        case PPTP_OUTGOING_CALL_REQUEST:
        case PPTP_OUTGOING_CALL_REPLY:
        case PPTP_INCOMING_CALL_REQUEST:
        case PPTP_INCOMING_CALL_REPLY:
        case PPTP_INCOMING_CALL_CONNECTED:
        case PPTP_CALL_CLEAR_REQUEST:
        case PPTP_CALL_DISCONNECT_NOTIFY:
        case PPTP_WAN_ERROR_NOTIFY:
            dbglog("PPTP received %s message\n", control_msgs[ntohs(header.ctrl_msgtype)]);
            break;

        default:
	    if (header.ctrl_msgtype)
	        error("PPTP received unexpected message type = %d\n", ntohs(header.ctrl_msgtype));
            //return -1; // do we disconnect ???
    }
    
    return 0;
}

