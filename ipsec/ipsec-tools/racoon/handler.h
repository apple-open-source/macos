/*	$NetBSD: handler.h,v 1.9 2006/09/09 16:22:09 manu Exp $	*/

/* Id: handler.h,v 1.19 2006/02/25 08:25:12 manubsd Exp */

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

#ifndef _HANDLER_H
#define _HANDLER_H

#include "config.h"
#include "racoon_types.h"

#include <sys/queue.h>
#ifdef HAVE_OPENSSL
#include <openssl/rsa.h>
#endif

#include <sys/time.h>

#include "isakmp_var.h"
#include "oakley.h"
#ifndef HAVE_OPENSSL
#include <Security/SecDH.h>
#endif
#include <sys/socket.h>

#include <schedule.h>

/* About address semantics in each case.
 *			initiator(addr=I)	responder(addr=R)
 *			src	dst		src	dst
 *			(local)	(remote)	(local)	(remote)
 * phase 1 handler	I	R		R	I
 * phase 2 handler	I	R		R	I
 * getspi msg		R	I		I	R
 * acquire msg		I	R
 * ID payload		I	R		I	R
 */
#ifdef ENABLE_HYBRID
struct isakmp_cfg_state;
#endif

#define INVALID_MSGID           0xFFFFFFFF

//=======================================================================
// PHASE 1
//=======================================================================

struct phase1handle {
	isakmp_index index;
    
	int status;			/* status of this SA */
	int side;			/* INITIATOR or RESPONDER */
	int started_by_api;		/* connection started by VPNControl API */
    
	struct sockaddr_storage *remote;	/* remote address to negosiate ph1 */
	struct sockaddr_storage *local;		/* local address to negosiate ph1 */
    /* XXX copy from rmconf due to anonymous configuration.
     * If anonymous will be forbidden, we do delete them. */
    
	struct remoteconf *rmconf;	/* pointer to remote configuration */
    
	struct isakmpsa *approval;	/* pointer to SA(s) approved. */
    /* for example pre-shared key */
    
	u_int8_t version;		/* ISAKMP version */
	u_int8_t etype;			/* Exchange type actually for use */
	u_int8_t flags;			/* Flags */
	u_int32_t msgid;		/* message id */
	
#ifdef ENABLE_NATT
	struct ph1natt_options *natt_options;	/* Selected NAT-T IKE version */
	u_int32_t natt_flags;		/* NAT-T related flags */
#endif
#ifdef ENABLE_FRAG
	int frag;			/* IKE phase 1 fragmentation */
	struct isakmp_frag_item *frag_chain;	/* Received fragments */
#endif
    
	schedule_ref sce;		/* schedule for expire */
	schedule_ref sce_rekey; /* schedule for rekey */
    
	schedule_ref scr;		/* schedule for resend */
	int retry_counter;		/* for resend. */
	vchar_t *sendbuf;		/* buffer for re-sending */
    
#ifndef HAVE_OPENSSL
	SecDHContext dhC;		/* Context for Security Framework Diffie-Hellman calculations */
	size_t publicKeySize;
#endif
	vchar_t *dhpriv;		/* DH; private value */
	vchar_t *dhpub;			/* DH; public value */
	vchar_t *dhpub_p;		/* DH; partner's public value */
	vchar_t *dhgxy;			/* DH; shared secret */
	vchar_t *nonce;			/* nonce value */
	vchar_t *nonce_p;		/* partner's nonce value */
	vchar_t *skeyid;		/* SKEYID */
	vchar_t *skeyid_d;		/* SKEYID_d */
	vchar_t *skeyid_a;		/* SKEYID_a, i.e. integrity protection */
	vchar_t *skeyid_a_p;    /* SKEYID_a_p, i.e. integrity protection */
	vchar_t *skeyid_e;		/* SKEYID_e, i.e. encryption */
    vchar_t *skeyid_e_p;	/* peer's SKEYID_e, i.e. encryption */
	vchar_t *key;			/* cipher key */
    vchar_t *key_p;         /* peer's cipher key */
	vchar_t *hash;			/* HASH minus general header */
	vchar_t *sig;			/* SIG minus general header */
	vchar_t *sig_p;			/* peer's SIG minus general header */
	cert_t *cert;			/* CERT minus general header */
	cert_t *cert_p;			/* peer's CERT minus general header */
	cert_t *crl_p;			/* peer's CRL minus general header */
	cert_t *cr_p;			/* peer's CR not including general */
	vchar_t *id;			/* ID minus gen header */
	vchar_t *id_p;			/* partner's ID minus general header */
    /* i.e. struct ipsecdoi_id_b*. */
	struct isakmp_ivm *ivm;		/* IVs */
    
	vchar_t *sa;			/* whole SA payload to send/to be sent*/
    /* to calculate HASH */
    /* NOT INCLUDING general header. */
    
	vchar_t *sa_ret;		/* SA payload to reply/to be replyed */
    /* NOT INCLUDING general header. */
    /* NOTE: Should be release after use. */
    
	struct isakmp_pl_hash *pl_hash;	/* pointer to hash payload */
    
	time_t created;			/* timestamp for establish */
#ifdef ENABLE_STATS
	struct timeval start;
	struct timeval end;
#endif
    
#ifdef ENABLE_DPD
	int		dpd_support;	/* Does remote supports DPD ? */
	time_t		dpd_lastack;	/* Last ack received */
	u_int16_t	dpd_seq;		/* DPD seq number to receive */
	u_int8_t	dpd_fails;		/* number of failures */
    u_int8_t        peer_sent_ike;
	schedule_ref    dpd_r_u;
#endif
    
#ifdef ENABLE_VPNCONTROL_PORT
	schedule_ref ping_sched;	/* for sending pings to keep FW open */
#endif
	
	u_int32_t msgid2;		/* msgid counter for Phase 2 */
	int ph2cnt;	/* the number which is negotiated by this phase 1 */
#ifdef ENABLE_HYBRID
	struct isakmp_cfg_state *mode_cfg;	/* ISAKMP mode config state */
	u_int8_t pended_xauth_id;			/* saved id for reply from vpn control socket */
	u_int8_t xauth_awaiting_userinput;	/* indicates we are waiting for user input */
    vchar_t *xauth_awaiting_userinput_msg; /* tracks the last packet that triggered XAUTH */
#endif
	int                                     is_rekey:1;
	int                                     is_dying:1;
	ike_session_t                           *parent_session;
	LIST_HEAD(_ph2ofph1_, phase2handle)     bound_ph2tree;
	LIST_ENTRY(phase1handle)                ph1ofsession_chain;
};

#define PHASE2_TYPE_SA          0
#define PHASE2_TYPE_INFO        1
#define PHASE2_TYPE_CFG         2

//=======================================================================
// PHASE 2
//=======================================================================
struct phase2handle {
	struct sockaddr_storage *src;		/* my address of SA. */
	struct sockaddr_storage *dst;		/* peer's address of SA. */
    
    /*
     * copy ip address from ID payloads when ID type is ip address.
     * In other case, they must be null.
     */
	struct sockaddr_storage *src_id;
	struct sockaddr_storage *dst_id;
    
    int phase2_type;        /* what this phase2 struct is for - see defines for PHASE2_TYPE... */
	u_int32_t spid;			/* policy id by kernel */
    
	int status;             /* ipsec sa status */
	u_int8_t side;			/* INITIATOR or RESPONDER */
	u_int8_t version;		/* ISAKMP version */
    
	schedule_ref sce;		/* schedule for expire */
	schedule_ref scr;		/* schedule for resend */
	int retry_counter;		/* for resend. */
	vchar_t *sendbuf;		/* buffer for re-sending */
	vchar_t *msg1;			/* buffer for re-sending */
    /* used for responder's first message */
    
	int retry_checkph1;		/* counter to wait phase 1 finished. */
    /* NOTE: actually it's timer. */
    
	u_int32_t seq;			/* sequence number used by PF_KEY */
    /*
     * NOTE: In responder side, we can't identify each SAs
     * with same destination address for example, when
     * socket based SA is required.  So we set a identifier
     * number to "seq", and sent kernel by pfkey.
     */
	u_int8_t satype;		/* satype in PF_KEY */
    /*
     * saved satype in the original PF_KEY request from
     * the kernel in order to reply a error.
     */
    
	u_int8_t flags;			/* Flags for phase 2 */
	u_int32_t msgid;		/* msgid for phase 2 */
    
	struct sainfo *sainfo;		/* place holder of sainfo */
	struct saprop *proposal;	/* SA(s) proposal. */
	struct saprop *approval;	/* SA(s) approved. */
	struct policyindex * spidx_gen;		/* policy from peer's proposal */
    
#ifndef HAVE_OPENSSL
	SecDHContext dhC;		/* Context for Security Framework Diffie-Hellman calculations */
	size_t publicKeySize;
#endif	
	struct dhgroup *pfsgrp;		/* DH; prime number */
	vchar_t *dhpriv;		/* DH; private value */
	vchar_t *dhpub;			/* DH; public value */
	vchar_t *dhpub_p;		/* DH; partner's public value */
	vchar_t *dhgxy;			/* DH; shared secret */
	vchar_t *id;			/* ID minus gen header */
	vchar_t *id_p;			/* peer's ID minus general header */
	vchar_t *nonce;			/* nonce value in phase 2 */
	vchar_t *nonce_p;		/* partner's nonce value in phase 2 */
    
	vchar_t *sa;			/* whole SA payload to send/to be sent*/
    /* to calculate HASH */
    /* NOT INCLUDING general header. */
    
	vchar_t *sa_ret;		/* SA payload to reply/to be replyed */
    /* NOT INCLUDING general header. */
    /* NOTE: Should be release after use. */
    
	struct isakmp_ivm *ivm;		/* IVs */
    
	int generated_spidx;	/* mark handlers whith generated policy */
    
#ifdef ENABLE_STATS
	struct timeval start;
	struct timeval end;
#endif
	struct phase1handle *ph1;	/* back pointer to isakmp status */
	int                    is_rekey:1;
	int                    is_dying:1;
    	int		       is_defunct:1;
	ike_session_t         *parent_session;
	vchar_t               *ext_nat_id;
	vchar_t               *ext_nat_id_p;
	LIST_ENTRY(phase2handle)    ph2ofsession_chain;	
	LIST_ENTRY(phase2handle)    ph1bind_chain;	/* chain to ph1handle */
};

/*
 * for handling initial contact.
 */
struct contacted {
	struct sockaddr_storage *remote;	/* remote address to negotiate ph1 */
	LIST_ENTRY(contacted) chain;
};

/*
 * for checking if a packet is retransmited.
 */
struct recvdpkt {
	struct sockaddr_storage *remote;	/* the remote address */
	struct sockaddr_storage *local;		/* the local address */
	vchar_t *hash;			/* hash of the received packet */
	vchar_t *sendbuf;		/* buffer for the response */
	int retry_counter;		/* how many times to send */
	time_t time_send;		/* timestamp to send a packet */
	time_t created;			/* timestamp to create a queue */
	time_t retry_interval;
#ifdef ENABLE_FRAG
	u_int32_t frag_flags;            /* IKE phase 1 fragmentation */
#endif

	schedule_ref scr;		/* schedule for resend, may not used */

	LIST_ENTRY(recvdpkt) chain;
};

/* for parsing ISAKMP header. */
struct isakmp_parse_t {
	u_char type;		/* payload type of mine */
	int len;		/* ntohs(ptr->len) */
	struct isakmp_gen *ptr;
};

/*
 * for IV management.
 *
 * - normal case
 * initiator                                     responder
 * -------------------------                     --------------------------
 * initialize iv(A), ive(A).                     initialize iv(A), ive(A).
 * encode by ive(A).
 * save to iv(B).            ---[packet(B)]-->   save to ive(B).
 *                                               decode by iv(A).
 *                                               packet consistency.
 *                                               sync iv(B) with ive(B).
 *                                               check auth, integrity.
 *                                               encode by ive(B).
 * save to ive(C).          <--[packet(C)]---    save to iv(C).
 * decoded by iv(B).
 *      :
 *
 * - In the case that a error is found while cipher processing,
 * initiator                                     responder
 * -------------------------                     --------------------------
 * initialize iv(A), ive(A).                     initialize iv(A), ive(A).
 * encode by ive(A).
 * save to iv(B).            ---[packet(B)]-->   save to ive(B).
 *                                               decode by iv(A).
 *                                               packet consistency.
 *                                               sync iv(B) with ive(B).
 *                                               check auth, integrity.
 *                                               error found.
 *                                               create notify.
 *                                               get ive2(X) from iv(B).
 *                                               encode by ive2(X).
 * get iv2(X) from iv(B).   <--[packet(Y)]---    save to iv2(Y).
 * save to ive2(Y).
 * decoded by iv2(X).
 *      :
 *
 * The reason why the responder synchronizes iv with ive after checking the
 * packet consistency is that it is required to leave the IV for decoding
 * packet.  Because there is a potential of error while checking the packet
 * consistency.  Also the reason why that is before authentication and
 * integirty check is that the IV for informational exchange has to be made
 * by the IV which is after packet decoded and checking the packet consistency.
 * Otherwise IV mismatched happens between the intitiator and the responder.
 */
struct isakmp_ivm {
	vchar_t *iv;	/* for decoding packet */
			/* if phase 1, it's for computing phase2 iv */
	vchar_t *ive;	/* for encoding packet */
};

/* for dumping */
struct ph1dump {
	isakmp_index index;
	int status;
	int side;
	struct sockaddr_storage remote;
	struct sockaddr_storage local;
	u_int8_t version;
	u_int8_t etype;	
	time_t created;
	int ph2cnt;
};

struct sockaddr_storage;
struct policyindex;

extern int                  ike_session_check_recvdpkt (struct sockaddr_storage *, struct sockaddr_storage *, vchar_t *);

extern void                 ike_session_flush_all_phase1_for_session(ike_session_t *, int);
extern void                 ike_session_flush_all_phase1 (int);

extern phase1_handle_t      *ike_session_getph1byindex (ike_session_t *, isakmp_index *);
extern phase1_handle_t      *ike_session_getph1byindex0 (ike_session_t *, isakmp_index *);
extern phase1_handle_t      *ike_session_getph1byaddr (ike_session_t *, struct sockaddr_storage *,
                                                            struct sockaddr_storage *);
extern phase1_handle_t      *ike_session_getph1byaddrwop (ike_session_t *, struct sockaddr_storage *,
                                                            struct sockaddr_storage *);
extern phase1_handle_t      *ike_session_getph1bydstaddrwop (ike_session_t *, struct sockaddr_storage *);
extern int                  ike_session_islast_ph1 (phase1_handle_t *);

extern int                  ike_session_expire_session(ike_session_t *session);
extern int                  ike_session_purgephXbydstaddrwop (struct sockaddr_storage *);
extern void                 ike_session_purgephXbyspid (u_int32_t, int);

extern phase1_handle_t      *ike_session_newph1 (unsigned int);
extern void                 ike_session_delph1 (phase1_handle_t *);

extern phase2_handle_t      *ike_session_getph2byspidx (ike_session_t *, struct policyindex *);
extern phase2_handle_t      *ike_session_getph2byspid (u_int32_t);
extern phase2_handle_t      *ike_session_getph2byseq (u_int32_t);
//extern phase2_handle_t      *ike_session_getph2bysaddr (struct sockaddr_storage *, struct sockaddr_storage *);
extern phase2_handle_t      *ike_session_getph2bymsgid (phase1_handle_t *, u_int32_t);
extern phase2_handle_t      *ike_session_getonlyph2(phase1_handle_t *iph1);
extern phase2_handle_t      *ike_session_getph2byid (struct sockaddr_storage *, struct sockaddr_storage *, u_int32_t);
extern phase2_handle_t      *ike_session_getph2bysaidx (struct sockaddr_storage *, struct sockaddr_storage *, u_int, u_int32_t);
extern phase2_handle_t      *ike_session_getph2bysaidx2(struct sockaddr_storage *src, struct sockaddr_storage *dst, u_int proto_id, u_int32_t spi, u_int32_t *opposite_spi);
extern phase2_handle_t      *ike_session_newph2 (unsigned int, int);
extern void                 ike_session_initph2 (phase2_handle_t *);
extern void                 ike_session_delph2 (phase2_handle_t *);
extern void                 ike_session_flush_all_phase2_for_session(ike_session_t *, int);
extern void                 ike_session_flush_all_phase2 (int);
extern void                 ike_session_deleteallph2 (struct sockaddr_storage *, struct sockaddr_storage *, u_int);
extern void                 ike_session_deleteallph1 (struct sockaddr_storage *, struct sockaddr_storage *);

#ifdef ENABLE_DPD
extern int                  ike_session_ph1_force_dpd (struct sockaddr_storage *);
#endif

//%%%%%%%%%%% don't know where the following will go yet - all these below could change
extern struct contacted     *ike_session_getcontacted (struct sockaddr_storage *);
extern int                  ike_session_inscontacted (struct sockaddr_storage *);
extern void                 ike_session_clear_contacted (void);
extern void                 ike_session_initctdtree (void);

extern time_t               ike_session_get_exp_retx_interval (int num_retries, int fixed_retry_interval);

extern int                  ike_session_add_recvdpkt (struct sockaddr_storage *, struct sockaddr_storage *,
                                                      vchar_t *, vchar_t *, size_t, u_int32_t);
extern void                 ike_session_clear_recvdpkt (void);
extern void                 ike_session_init_recvdpkt (void);

#ifdef ENABLE_HYBRID
//extern int                  ike_session_exclude_cfg_addr (const struct sockaddr_storage *);
#endif

extern void                 sweep_sleepwake (void);

#endif /* _HANDLER_H */
