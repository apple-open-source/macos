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
    PPPOE_CMD_SETLOOPBACK,	// set loopback mode
    PPPOE_CMD_GETLOOPBACK	// set loopback mode

};

typedef void (*pppoe_rfc_event_callback)(void *data, u_int32_t event, u_int32_t msg);
typedef int (*pppoe_rfc_input_callback)(void *data, struct mbuf *m);

u_int16_t pppoe_rfc_init();
void pppoe_rfc_dispose();
int pppoe_rfc_attach(u_short unit);
int pppoe_rfc_detach(u_short unit);
u_int16_t pppoe_rfc_new_client(void *host, u_int16_t unit, void **data,
                         pppoe_rfc_input_callback input,
                               pppoe_rfc_event_callback event);
void pppoe_rfc_free_client(void *data);


u_int16_t pppoe_rfc_bind(void *data, u_int8_t *ac_name, u_int8_t *service);
u_int16_t pppoe_rfc_connect(void *data, u_int8_t *ac_name, u_int8_t *service);
u_int16_t pppoe_rfc_disconnect(void *data);
u_int16_t pppoe_rfc_abort(void *data);
u_int16_t pppoe_rfc_listen(void *data);
u_int16_t pppoe_rfc_accept(void *data);
void pppoe_rfc_clone(void *data1, void *data2);
void pppoe_rfc_getpeeraddr(void *data, u_int8_t *address);
u_int16_t pppoe_rfc_command(void *userdata, u_int32_t cmd, void *cmddata);

void pppoe_rfc_timer();

u_int16_t pppoe_rfc_output(void *data, struct mbuf *m);

// callback from dlil layer
void pppoe_rfc_lower_input(u_long dl_tag, struct mbuf *m, u_int8_t *from, u_int16_t typ);


#endif