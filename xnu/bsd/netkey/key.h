/*	$KAME: key.h,v 1.11 2000/03/25 07:24:12 sumikawa Exp $	*/

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

#ifndef _NETKEY_KEY_H_
#define _NETKEY_KEY_H_
#include <sys/appleapiopts.h>

#ifdef BSD_KERNEL_PRIVATE

#define KEY_SADB_UNLOCKED       0
#define KEY_SADB_LOCKED         1

extern struct key_cb key_cb;
extern lck_mtx_t sadb_mutex_data;
#define sadb_mutex (&sadb_mutex_data)

struct secpolicy;
struct secpolicyindex;
struct ipsecrequest;
struct secasvar;
struct sockaddr;
struct socket;
struct sadb_msg;
struct sadb_x_policy;
struct secasindex;
struct secashead;
struct sadb_key;
struct sadb_lifetime;

extern struct secpolicy *key_allocsp(struct secpolicyindex *, u_int);
extern struct secasvar *key_allocsa_policy(struct secasindex *);
extern struct secpolicy *key_gettunnel(struct sockaddr *,
    struct sockaddr *, struct sockaddr *, struct sockaddr *);
extern struct secasvar *key_alloc_outbound_sav_for_interface(ifnet_t interface, int family,
    struct sockaddr *src,
    struct sockaddr *dst);
extern int key_checkrequest(struct ipsecrequest *isr, struct secasindex *,
    struct secasvar **sav);
struct secasvar *key_allocsa(union sockaddr_in_4_6 *src, union sockaddr_in_4_6 *dst,
    u_int proto, u_int32_t spi, ifnet_t interface);
extern bool key_checksa_present(union sockaddr_in_4_6 *src, union sockaddr_in_4_6 *dst);
extern u_int16_t key_natt_get_translated_port(struct secasvar *);
extern void key_freesp(struct secpolicy *, int);
extern void key_freesav(struct secasvar *, int);
extern struct secpolicy *key_newsp(void);
extern struct secpolicy *key_msg2sp(struct sadb_x_policy *__sized_by(len), size_t len, int *);
extern struct mbuf *key_sp2msg(struct secpolicy *);
extern int key_ismyaddr(struct sockaddr *);
extern int key_spdacquire(struct secpolicy *);
extern void key_timehandler(void);
extern u_int32_t key_random(void);
extern void key_randomfill(void *, size_t);
extern void key_freereg(struct socket *);
extern int key_parse(struct mbuf *, struct socket *);
extern int key_checktunnelsanity(struct secasvar *, u_int, caddr_t, caddr_t);
extern void key_sa_recordxfer(struct secasvar *, size_t);
extern void key_sa_routechange(struct sockaddr *);
extern void key_sa_chgstate(struct secasvar *, u_int8_t);
extern void key_sa_stir_iv(struct secasvar *);
extern void key_delsah(struct secashead *sah);
extern struct secashead *key_newsah2(struct secasindex *saidx, u_int8_t dir);
extern void key_delsav(struct secasvar *sav);
extern struct secpolicy *key_getspbyid(u_int32_t);
extern void key_delsp_for_ipsec_if(ifnet_t ipsec_if);

struct ifnet;
struct ifnet_keepalive_offload_frame;
extern u_int32_t key_fill_offload_frames_for_savs(struct ifnet *,
    struct ifnet_keepalive_offload_frame *__counted_by(frames_count)frames_array, u_int32_t frames_count, size_t);

extern bool key_custom_ipsec_token_is_valid(void *);
extern int key_reserve_custom_ipsec(void **, union sockaddr_in_4_6 *, union sockaddr_in_4_6 *, u_int8_t proto);
extern void key_release_custom_ipsec(void **);

#endif /* BSD_KERNEL_PRIVATE */
#endif /* _NETKEY_KEY_H_ */
