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



#ifndef __PPTP_RFC_H__
#define __PPTP_RFC_H__

#define PPTP_MTU	1500

enum {
    PPTP_EVT_XMIT_OK = 1,
    PPTP_EVT_XMIT_FULL,
    PPTP_EVT_INPUTERROR
};

enum {
    PPTP_CMD_VOID = 1,	 	// command codes to define
    PPTP_CMD_SETFLAGS,		// set flags
    PPTP_CMD_SETPEERADDR,	// set peer IP address
    PPTP_CMD_SETCALLID,		// set call id
    PPTP_CMD_SETPEERCALLID,	// set peer call id
    PPTP_CMD_SETWINDOW,		// set our receive window
    PPTP_CMD_SETPEERWINDOW,	// set peer receive window
    PPTP_CMD_SETPEERPPD,	// set packet processing delay	
    PPTP_CMD_SETMAXTIMEOUT,	// set send maximum timeout	
    PPTP_CMD_SETOURADDR,	// set our IP address	
    PPTP_CMD_SETBAUDRATE,	// set tunnel baud rate
    PPTP_CMD_GETBAUDRATE	// get tunnel baud rate
};

typedef int (*pptp_rfc_input_callback)(void *data, mbuf_t m);
typedef void (*pptp_rfc_event_callback)(void *data, u_int32_t evt, u_int32_t msg);

u_int16_t pptp_rfc_init();
u_int16_t pptp_rfc_dispose();
u_int16_t pptp_rfc_new_client(void *host, void **data,
                         pptp_rfc_input_callback input, 
                         pptp_rfc_event_callback event);
void pptp_rfc_free_client(void *data);


u_int16_t pptp_rfc_command(void *userdata, u_int32_t cmd, void *cmddata);

void pptp_rfc_fasttimer();
void pptp_rfc_slowtimer();

u_int16_t pptp_rfc_output(void *data, mbuf_t m);

// callback from dlil layer
int pptp_rfc_lower_input(mbuf_t m, u_int32_t from);


#endif