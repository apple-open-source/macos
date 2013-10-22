/*	$NetBSD: isakmp_xauth.h,v 1.4 2006/09/09 16:22:09 manu Exp $	*/

/*	$KAME$ */

/*
 * Copyright (C) 2004 Emmanuel Dreyfus 
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

#ifndef _ISAKMP_XAUTH_H
#define _ISAKMP_XAUTH_H

#include "racoon_types.h"

/* ISAKMP mode config attribute types specific to the Xauth vendor ID */
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

/* Values for XAUTH_STATUS */
#define	XAUTH_STATUS_FAIL       0
#define	XAUTH_STATUS_OK         1

/* For phase 1 Xauth status */
struct xauth_state {
	int status; /* authentication status, used only on server side */
	int vendorid;
	int authtype;
	union {
		struct authgeneric {
			char *usr;
			char *pwd;
		} generic;
	} authdata;
};

/* What's been sent */
#define XAUTH_SENT_USERNAME 1
#define XAUTH_SENT_PASSWORD 2
#define XAUTH_SENT_EVERYTHING (XAUTH_SENT_USERNAME | XAUTH_SENT_PASSWORD)

/* For rmconf Xauth data */
struct xauth_rmconf {
	vchar_t *login;	/* xauth login */
	vchar_t *pass;	/* xauth password */
	int state;      /* what's been sent */
};

/* status */
#define XAUTHST_NOTYET	0
#define XAUTHST_REQSENT	1
#define XAUTHST_OK	2

struct xauth_reply_arg {
	isakmp_index index;
	int port;
	int id;
	int res;
};

struct isakmp_data;
void xauth_sendreq (phase1_handle_t *);
int xauth_attr_reply (phase1_handle_t *, struct isakmp_data *, int);
int xauth_login_system (char *, char *);
void xauth_sendstatus (phase1_handle_t *, int, int);
int xauth_check (phase1_handle_t *);
int group_check (phase1_handle_t *, char **, int);
vchar_t *isakmp_xauth_req (phase1_handle_t *, struct isakmp_data *);
vchar_t *isakmp_xauth_set (phase1_handle_t *, struct isakmp_data *);
void xauth_rmstate (struct xauth_state *);
void xauth_reply_stub (void *);
int xauth_reply (phase1_handle_t *, int, int, int);
int xauth_rmconf_used (struct xauth_rmconf **);
void xauth_rmconf_delete (struct xauth_rmconf **);


#endif /* _ISAKMP_XAUTH_H */
