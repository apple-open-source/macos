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
 * eap.h - Extensible Authentication Protocol definitions.
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
 * $Id: eap.h,v 1.5 2003/08/14 00:00:29 callie Exp $
 */

#ifndef __EAP_INCLUDE__

#include "eap_plugin.h"


/*
 *  Challenge lengths (for challenges we send) and other limits.
 */
#define MAX_EAP_RESPONSE_LENGTH	1024	/* Max len for the EAP data part */
#define MAX_NAME_LENGTH		256

/*
 * Extension structure for eap types.
 */

#define EAP_EXT_CLIENT		0x1	// support client mode  
#define EAP_EXT_SERVER 		0x2	// support server mode

typedef struct eap_ext {
    struct eap_ext 	*next;		// next extensiopn structure
    u_int8_t 		type;		// eap type
    char 		*name;		// extension name
    u_int32_t 		flags;		// support flags
    void		*plugin;	// used to keep ref of the plugin
    int (*init) __P((EAP_Input *eap_in, void **context));
    //int (*reinit) __P((void *context));
    int (*dispose) __P((void *context));
    int (*process) __P((void *context, EAP_Input *eap_in, EAP_Output *eap_out));
    int (*free) __P((void *context, EAP_Output *eap_out));
    int (*attribute) __P((void *context, EAP_Attribute *eap_attr));
    int (*interactive_ui) __P((void *data_in, int data_in_len, void **data_out, int *data_out_len));
    void (*print_packet) __P((void (*printer)(void *, char *, ...), void *arg, u_char code, char *inbuf, int insize));

} eap_ext;

/*
 * Each interface is described by a eap structure.
 */

typedef struct eap_state {
    int unit;			/* Interface unit number */
 
    int clientstate;		/* Client state */
    int serverstate;		/* Server state */

    char *our_identity;		/* Our identity name */
    char *username;		/* the user name (only for client mode) */
    char *password;		/* the password (only for client mode) */
    char peer_identity[MAX_NAME_LENGTH];	/* peer name discovered with identity request */

    u_char req_id;		/* ID of last challenge */
    u_char req_type;		/* last request type  */
    int req_interval;		/* Time until we challenge peer again */
    int timeouttime;		/* Timeout time in seconds */
    int max_transmits;		/* Maximum # of challenge transmissions */
    int req_transmits;		/* Number of transmissions of challenge */

    eap_ext *client_ext;	/* client eap extension */
    void *client_ext_ctx;	/* client eap extension context */
    EAP_Input *client_ext_input;	/* client eap extension input structure */
    EAP_Output *client_ext_output;	/* client eap extension output structure */
    int client_ext_ui_fds[2];	/* files descriptors for UI thread */
    void *client_ext_ui_data;	/* UI data */
    int client_ext_ui_data_len;	/* UI data len */
    pthread_t client_ui_thread; /* UI thread */
    
    eap_ext *server_ext;	/* server eap extension */
    void *server_ext_ctx;	/* server eap extension context */
    EAP_Input *server_ext_input;	/* server eap extension input structure */
    EAP_Output *server_ext_output;	/* server eap extension output structure */

} eap_state;


int EapExtAdd(eap_ext *newext);

/*
 * Client (peer) states.
 */
#define EAPCS_INITIAL		0	/* Lower layer down, not opened */
#define EAPCS_CLOSED		1	/* Lower layer up, not opened */
#define EAPCS_PENDING		2	/* Auth us to peer when lower up */
#define EAPCS_LISTEN		3	/* Listening for a challenge */
#define EAPCS_OPEN		4	/* We've received Success */

/*
 * Server (authenticator) states.
 */
#define EAPSS_INITIAL		0	/* Lower layer down, not opened */
#define EAPSS_CLOSED		1	/* Lower layer up, not opened */
#define EAPSS_PENDING		2	/* Auth peer when lower up */
#define EAPSS_INITIAL_CHAL	3	/* We've sent the first challenge */
#define EAPSS_OPEN		4	/* We've sent a Success msg */
#define EAPSS_RECHALLENGE	5	/* We've sent another challenge */
#define EAPSS_BADAUTH		6	/* We've sent a Failure msg */

/*
 * Timeouts.
 */
#define EAP_DEFTIMEOUT		3	/* Timeout time in seconds */
#define EAP_DEFTRANSMITS	10	/* max # times to send challenge */

extern eap_state eap[];

void EapAuthWithPeer __P((int, char *));
void EapAuthPeer __P((int, char *));
void EapGenChallenge __P((eap_state *));

int EapGetClientSecret(void *cookie, u_char *our_name, u_char *peer_name, u_char *secret, int *secretlen);
int EapGetServerSecret(void *cookie, u_char *our_name, u_char *peer_name, u_char *secret, int *secretlen);

int EAPAllowedAddr(int unit, u_int32_t addr);

int reqeap(char **);

extern struct protent eap_protent;

#define __EAP_INCLUDE__
#endif /* __EAP_INCLUDE__ */
