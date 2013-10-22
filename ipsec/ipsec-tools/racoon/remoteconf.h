/*	$NetBSD: remoteconf.h,v 1.7 2006/10/03 08:01:56 vanhu Exp $	*/

/* Id: remoteconf.h,v 1.26 2006/05/06 15:52:44 manubsd Exp */

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

#ifndef _REMOTECONF_H
#define _REMOTECONF_H

/* remote configuration */

#include <sys/queue.h>
#include "genlist.h"
#ifdef ENABLE_HYBRID
#include "isakmp_var.h"
#include "isakmp_xauth.h"
#endif
#include <CoreFoundation/CFData.h>
#include "algorithm.h"



struct proposalspec {
	time_t lifetime;		/* for isakmp/ipsec */
	int lifebyte;			/* for isakmp/ipsec */
	struct secprotospec *spspec;	/* the head is always current spec. */
	struct proposalspec *next;	/* the tail is the most prefered. */
	struct proposalspec *prev;
};

struct secprotospec {
	int prop_no;
	int trns_no;
	int strength;		/* for isakmp/ipsec */
	int encklen;		/* for isakmp/ipsec */
	time_t lifetime;	/* for isakmp */
	int lifebyte;		/* for isakmp */
	int proto_id;		/* for ipsec (isakmp?) */
	int ipsec_level;	/* for ipsec */
	int encmode;		/* for ipsec */
	int vendorid;		/* for isakmp */
	char *gssid;
	struct sockaddr_storage *remote;
	int algclass[MAXALGCLASS];

	struct secprotospec *next;	/* the tail is the most prefiered. */
	struct secprotospec *prev;
	struct proposalspec *back;
};


struct etypes {
	int type;
	struct etypes *next;
};

enum {
    DPD_ALGO_DEFAULT = 0,
    DPD_ALGO_INBOUND_DETECT,
    DPD_ALGO_BLACKHOLE_DETECT,
    DPD_ALGO_MAX,
};


struct remoteconf {
	struct sockaddr_storage *remote;	/* remote IP address */
    int remote_prefix;                  /* allows subnet for remote address */
					/* if family is AF_UNSPEC, that is
					 * for anonymous configuration. */

	struct etypes *etypes;		/* exchange type list. the head
					 * is a type to be sent first. */
	int doitype;			/* doi type */
	int sittype;			/* situation type */

	int idvtype;			/* my identifier type */
	vchar_t *idv;			/* my identifier */
	vchar_t *key;			/* my pre-shared key */
	struct genlist *idvl_p;         /* peer's identifiers list */

	int	identity_in_keychain;	/* cert and private key is in the keychain */
	vchar_t *keychainCertRef;	/* peristant keychain ref for cert */
	int secrettype;			/* type of secret [use, key, keychain] */
	vchar_t *shared_secret;	/* shared secret */
	vchar_t *open_dir_auth_group;	/* group to be used to authorize user */

	int certtype;			/* certificate type if need */
	int getcert_method;		/* the way to get peer's certificate */
	int cacerttype;			/* CA type is needed */
	int send_cert;			/* send to CERT or not */
	int send_cr;			/* send to CR or not */
	int verify_cert;		/* verify a CERT strictly */
	int cert_verification;	/* openssl or security framework */
	int cert_verification_option;	/* nothing, peers identifier, or open_dir */
	int verify_identifier;		/* vefify the peer's identifier */
	int nonce_size;			/* the number of bytes of nonce */
	int passive;			/* never initiate */
	int ike_frag;			/* IKE fragmentation */
	int esp_frag;			/* ESP fragmentation */
	int mode_cfg;			/* Gets config through mode config */
	int support_proxy;		/* support mip6/proxy */
#define GENERATE_POLICY_NONE   0
#define GENERATE_POLICY_REQUIRE        1
#define GENERATE_POLICY_UNIQUE 2
	int gen_policy;			/* generate policy if no policy found */
	int ini_contact;		/* initial contact */
	int pcheck_level;		/* level of propocl checking */
	int nat_traversal;		/* NAT-Traversal */
	int natt_multiple_user; /* special handling of multiple users behind a nat - for VPN server */
	int natt_keepalive;		/* do we need to send natt keep alive */
	int dh_group;			/* use it when only aggressive mode */
	struct dhgroup *dhgrp;		/* use it when only aggressive mode */
					/* above two can't be defined by user*/

	int retry_counter;		/* times to retry. */
	int retry_interval;		/* interval each retry. */
				/* above 2 values are copied from localconf. */

	int dpd;				/* Negociate DPD support ? */
	int dpd_retry;			/* in seconds */
	int dpd_interval;		/* in seconds */
	int dpd_maxfails;
    int dpd_algo;
    int idle_timeout;       /* in seconds */
    int idle_timeout_dir;   /* direction to check */
	
	int ph1id; /* ph1id to be matched with sainfo sections */

	int weak_phase1_check;		/* act on unencrypted deletions ? */

	struct isakmpsa *proposal;	/* proposal list */
	struct remoteconf *inherited_from;	/* the original rmconf 
						   from which this one 
						   was inherited */
	struct proposalspec *prhead;

#ifdef ENABLE_HYBRID
	struct xauth_rmconf *xauth;
#endif
    int initiate_ph1rekey;
    int in_list;            // in the linked list
    int refcount;           // ref count - in use
    int ike_version;

	// IKEV2 configs
    struct etypes *eap_types;
    CFDictionaryRef eap_options;
    CFDictionaryRef ikev2_cfg_request;

	TAILQ_ENTRY(remoteconf) chain;	/* next remote conf */
};

struct dhgroup;

/* ISAKMP SA specification */
struct isakmpsa {
	int version;
	int prop_no;
	int trns_no;
	time_t lifetime;
	time_t lifetimegap;
	size_t lifebyte;
	int enctype;
	int encklen;
	int authmethod;
	int hashtype;
	int vendorid;
	int dh_group;				/* don't use it if aggressive mode */
	struct dhgroup *dhgrp;		/* don't use it if aggressive mode */
	int             prf;
	int             prfklen;

	struct isakmpsa *next;		/* next transform */
	struct remoteconf *rmconf;	/* backpointer to remoteconf */
};

struct idspec {
	int idtype;                     /* identifier type */
	vchar_t *id;                    /* identifier */
};

typedef struct remoteconf *(rmconf_func_t) (struct remoteconf *rmconf, void *data);

extern struct remoteconf *getrmconf (struct sockaddr_storage *);
extern struct remoteconf *getrmconf_strict
	(struct sockaddr_storage *remote, int allow_anon);

extern int no_remote_configs (int);
extern struct remoteconf *copyrmconf (struct sockaddr_storage *);
extern struct remoteconf *create_rmconf (void);
extern void retain_rmconf(struct remoteconf *);
extern void release_rmconf(struct remoteconf *);
extern struct remoteconf *duprmconf (struct remoteconf *);
extern void delrmconf (struct remoteconf *);
extern void delisakmpsa (struct isakmpsa *);
extern void deletypes (struct etypes *);
extern struct etypes * dupetypes (struct etypes *);
extern void insrmconf (struct remoteconf *);
extern void remrmconf (struct remoteconf *);
extern void flushrmconf (void);
extern void initrmconf (void);
extern struct etypes *check_etypeok
	(struct remoteconf *, u_int8_t);
extern struct remoteconf *foreachrmconf (rmconf_func_t rmconf_func,
					     void *data);

extern struct isakmpsa *newisakmpsa (void);
extern struct isakmpsa *dupisakmpsa (struct isakmpsa *);

extern void insisakmpsa (struct isakmpsa *, struct remoteconf *);

extern void dumprmconf (void);

extern struct idspec *newidspec (void);

#endif /* _REMOTECONF_H */
