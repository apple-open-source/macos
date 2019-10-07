/*
 * Copyright (c) 2000, 2018 Apple Inc. All rights reserved.
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



#ifndef __PPPOE_RFC_H__
#define __PPPOE_RFC_H__

#define PPPOE_MTU	1492

enum {
    PPPOE_STATE_DISCONNECTED = 0,
    PPPOE_STATE_LOOKING,
    PPPOE_STATE_CONNECTING,
    PPPOE_STATE_CONNECTED,
    PPPOE_STATE_LISTENING,
    PPPOE_STATE_RINGING
};

enum {
    PPPOE_EVT_DISCONNECTED = 1,
    PPPOE_EVT_RINGING,
    PPPOE_EVT_CONNECTED,
    PPPOE_EVT_DATAPRESENT,
    PPPOE_EVT_DATAWRITTEN,
};

enum {
    PPPOE_CMD_VOID = 1,	 	// command codes to define
    PPPOE_CMD_SETFLAGS,		// set flags
    PPPOE_CMD_GETFLAGS,		// get flags
    PPPOE_CMD_SETUNIT,		// set ethernet unit number
    PPPOE_CMD_GETUNIT,		// get ethernet unit number
    PPPOE_CMD_SETCONNECTTIMER,	// set connect timer
    PPPOE_CMD_GETCONNECTTIMER,	// get connect timer
    PPPOE_CMD_SETRINGTIMER,	// set ring timer
    PPPOE_CMD_GETRINGTIMER,	// get ring timer
    PPPOE_CMD_SETPEERADDR,	// set peer ethernet address
    PPPOE_CMD_GETPEERADDR,	// get peer ethernet address
    PPPOE_CMD_SETRETRYTIMER, 	// set ring timer
    PPPOE_CMD_GETRETRYTIMER 	// get ring timer
};

typedef void (*pppoe_rfc_event_callback)(void *data, u_int32_t event, u_int32_t msg);
typedef int (*pppoe_rfc_input_callback)(void *data, mbuf_t m);

u_int16_t pppoe_rfc_init(void);
u_int16_t pppoe_rfc_dispose(void);
int pppoe_rfc_attach(u_short unit, ifnet_t *ifpp);
int pppoe_rfc_detach(u_short unit);
u_int16_t pppoe_rfc_new_client(void *host, void **data,
                         pppoe_rfc_input_callback input,
                               pppoe_rfc_event_callback event);
void pppoe_rfc_free_client(void *data);


u_int16_t pppoe_rfc_bind(void *data, u_int8_t *ac_name, u_int8_t *service);
u_int16_t pppoe_rfc_connect(void *data, u_int8_t *ac_name, u_int8_t *service);
u_int16_t pppoe_rfc_disconnect(void *data);
u_int16_t pppoe_rfc_abort(void *data, int evt_enable);
u_int16_t pppoe_rfc_listen(void *data);
u_int16_t pppoe_rfc_accept(void *data);
void pppoe_rfc_clone(void *data1, void *data2);
u_int16_t pppoe_rfc_command(void *userdata, u_int32_t cmd, void *cmddata);

void pppoe_rfc_timer(void);

u_int16_t pppoe_rfc_output(void *data, mbuf_t m);

// callback from dlil layer
void pppoe_rfc_lower_input(ifnet_t ifp, mbuf_t m, u_int8_t *from, u_int16_t typ);
void pppoe_rfc_lower_detaching(ifnet_t ifp);


#endif
