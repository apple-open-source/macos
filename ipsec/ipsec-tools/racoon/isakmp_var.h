/* $Id: isakmp_var.h,v 1.9.2.1 2005/05/07 17:26:06 manubsd Exp $ */

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

#ifndef _ISAKMP_VAR_H
#define _ISAKMP_VAR_H

#include "vmbuf.h"
#include "racoon_types.h"
#include <schedule.h>

#define PORT_ISAKMP 500
#define PORT_ISAKMP_NATT 4500

#define DEFAULT_NONCE_SIZE	16

typedef u_char cookie_t[8];
typedef u_char msgid_t[4];

typedef struct { /* i_cookie + r_cookie */
	cookie_t i_ck;
	cookie_t r_ck;
} isakmp_index;

struct isakmp_gen;



struct sockaddr_storage;
struct remoteconf;
struct isakmp_gen;
struct ipsecdoi_pl_id;	/* XXX */
struct isakmp_pl_ke;	/* XXX */
struct isakmp_pl_nonce;	/* XXX */

extern void isakmp_handler (int);
extern int ikev1_ph1begin_i (ike_session_t *session, struct remoteconf *, struct sockaddr_storage *,
	struct sockaddr_storage *, int);
extern int get_sainfo_r (phase2_handle_t *);
extern int get_proposal_r (phase2_handle_t *);

extern vchar_t *isakmp_parsewoh (int, struct isakmp_gen *, int);
extern vchar_t *isakmp_parse (vchar_t *);

extern int isakmp_init (void);
extern void isakmp_cleanup (void);

extern const char *isakmp_pindex (const isakmp_index *, const u_int32_t);
extern int isakmp_open (void);
extern void isakmp_suspend_sockets(void);
extern void isakmp_close (void);
extern void isakmp_close_sockets (void);
extern void isakmp_close_unused (void);
extern int isakmp_send (phase1_handle_t *, vchar_t *);

extern void isakmp_ph1resend_stub (void *);
extern int isakmp_ph1resend (phase1_handle_t *);
extern void isakmp_ph2resend_stub (void *);
extern int isakmp_ph2resend (phase2_handle_t *);

extern void isakmp_ph1expire_stub (void *);
extern void isakmp_ph1expire (phase1_handle_t *);
extern void isakmp_ph1rekeyexpire_stub (void *);
extern void isakmp_ph1rekeyexpire (phase1_handle_t *, int);
extern int  isakmp_ph1rekeyretry (phase1_handle_t *);
extern void isakmp_ph1delete_stub (void *);
extern void isakmp_ph1delete (phase1_handle_t *);
extern void isakmp_ph2expire_stub (void *);
extern void isakmp_ph2expire (phase2_handle_t *);
extern void isakmp_ph2delete_stub (void *);
extern void isakmp_ph2delete (phase2_handle_t *);
extern int ikev1_phase1_established(phase1_handle_t *);

extern int isakmp_post_acquire (phase2_handle_t *);
extern int isakmp_post_getspi (phase2_handle_t *);
extern void isakmp_chkph1there_stub (void *);
extern void isakmp_chkph1there (phase2_handle_t *);

extern caddr_t isakmp_set_attr_v (caddr_t, int, caddr_t, int);
extern caddr_t isakmp_set_attr_l (caddr_t, int, u_int32_t);
extern vchar_t *isakmp_add_attr_v (vchar_t *, int, caddr_t, int);
extern vchar_t *isakmp_add_attr_l (vchar_t *, int, u_int32_t);

extern int isakmp_newcookie (caddr_t, struct sockaddr_storage *, struct sockaddr_storage *);

extern int isakmp_p2ph (vchar_t **, struct isakmp_gen *);

extern u_int32_t isakmp_newmsgid2 (phase1_handle_t *);
extern caddr_t set_isakmp_header1 (vchar_t *, phase1_handle_t *, int);
extern caddr_t set_isakmp_header2 (vchar_t *, phase2_handle_t *, int);
extern caddr_t set_isakmp_payload (caddr_t, vchar_t *, int);

extern struct payload_list *isakmp_plist_append (struct payload_list *plist, 
	vchar_t *payload, int payload_type);
extern vchar_t *isakmp_plist_set_all (struct payload_list **plist,
	phase1_handle_t *iph1);
extern vchar_t *isakmp_plist_append_initial_contact (phase1_handle_t *, struct payload_list *);

#ifdef HAVE_PRINT_ISAKMP_C
extern void isakmp_printpacket (vchar_t *, struct sockaddr_storage *,
	struct sockaddr_storage *, int);
#endif

extern int copy_ph1addresses (phase1_handle_t *,
	struct remoteconf *, struct sockaddr_storage *, struct sockaddr_storage *);
extern void log_ph1established (const phase1_handle_t *);

extern void script_hook (phase1_handle_t *, int); 
extern int script_env_append (char ***, int *, char *, char *);
extern int script_exec (char *, int, char * const *);

void purge_remote (phase1_handle_t *);
void delete_spd (phase2_handle_t *);
#ifdef INET6
u_int32_t setscopeid (struct sockaddr_storage *, struct sockaddr_storage *);
#endif
#endif /* _ISAKMP_VAR_H */
