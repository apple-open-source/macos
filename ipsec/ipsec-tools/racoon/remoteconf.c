/*	$NetBSD: remoteconf.c,v 1.9.4.1 2007/08/01 11:52:22 vanhu Exp $	*/

/* Id: remoteconf.c,v 1.38 2006/05/06 15:52:44 manubsd Exp */

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

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>

#ifndef HAVE_NETINET6_IPSEC
#include <netinet/ipsec.h>
#else
#include <netinet6/ipsec.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "genlist.h"
#include "debug.h"

#include "isakmp_var.h"
#ifdef ENABLE_HYBRID
#include "isakmp_xauth.h"
#endif
#include "isakmp.h"
#include "ipsec_doi.h"
#include "oakley.h"
#include "remoteconf.h"
#include "localconf.h"
#include "grabmyaddr.h"
#include "policy.h"
#include "proposal.h"
#include "vendorid.h"
#include "gcmalloc.h"
#include "strnames.h"
#include "algorithm.h"
#include "nattraversal.h"
#include "isakmp_frag.h"
#include "genlist.h"

static TAILQ_HEAD(_rmtree, remoteconf) rmtree;


/*%%%*/
/*
 * search remote configuration.
 * don't use port number to search if its value is either IPSEC_PORT_ANY.
 * If matching anonymous entry, then new entry is copied from anonymous entry.
 * If no anonymous entry found, then return NULL.
 * OUT:	NULL:	NG
 *	Other:	remote configuration entry.
 */
struct remoteconf *
getrmconf_strict(remote, allow_anon)
	struct sockaddr_storage *remote;
	int allow_anon;
{
	struct remoteconf *p;
	struct remoteconf *p_withport_besteffort = NULL;
	struct remoteconf *p_with_prefix = NULL;
	struct remoteconf *p_with_prefix_besteffort = NULL;
    int                last_prefix = 0;
	struct remoteconf *anon = NULL;
    
	int withport;
	char buf[NI_MAXHOST + NI_MAXSERV + 10];
	char addr[NI_MAXHOST], port[NI_MAXSERV];

	withport = 0;

	/* 
	 * We never have ports set in our remote configurations, but when
	 * NAT-T is enabled, the kernel can have policies with ports and
	 * send us an acquire message for a destination that has a port set.
	 * If we do this port check here, we have to fallback to a best-effort result (without the port).
	 *
	 * In an ideal world, we would be able to have remote conf with
	 * port, and the port could be a wildcard. That test could be used.
	 */
	switch (remote->ss_family) {
	case AF_INET:
		if (((struct sockaddr_in *)remote)->sin_port != IPSEC_PORT_ANY)
			withport = 1;
		break;
#ifdef INET6
	case AF_INET6:
		if (((struct sockaddr_in6 *)remote)->sin6_port != IPSEC_PORT_ANY)
			withport = 1;
		break;
#endif
	case AF_UNSPEC:
		break;

	default:
		plog(ASL_LEVEL_ERR, 
			"invalid ip address family: %d\n", remote->ss_family);
		return NULL;
	}

	if (remote->ss_family == AF_UNSPEC)
		snprintf (buf, sizeof(buf), "%s", "anonymous");
	else {
		GETNAMEINFO((struct sockaddr *)remote, addr, port);
		snprintf(buf, sizeof(buf), "%s%s%s%s", addr,
			withport ? "[" : "",
			withport ? port : "",
			withport ? "]" : "");
	}

	TAILQ_FOREACH(p, &rmtree, chain) {
		if (remote->ss_family == AF_UNSPEC
		     && remote->ss_family == p->remote->ss_family) {
            plog(ASL_LEVEL_DEBUG, "configuration found for %s.\n", buf);
			return p;
        }
        if (p->remote_prefix == 0) {
            if ((!withport && cmpsaddrwop(remote, p->remote) == 0)
                || (withport && cmpsaddrstrict(remote, p->remote) == 0)) {
                    plog(ASL_LEVEL_DEBUG, "configuration found for %s.\n", buf);
                    return p;
                } else if (withport && cmpsaddrwop(remote, p->remote) == 0) {
                    // for withport: save the pointer for the best-effort search
                    p_withport_besteffort = p;
                }
        } else {
            if ((!withport && cmpsaddrwop_withprefix(remote, p->remote, p->remote_prefix) == 0)
                || (withport && cmpsaddrstrict_withprefix(remote, p->remote, p->remote_prefix) == 0)) {
                if (p->remote_prefix >= last_prefix) {
                    p_with_prefix = p;
                    last_prefix = p->remote_prefix;
                }
            } else if (withport && cmpsaddrwop_withprefix(remote, p->remote, p->remote_prefix) == 0) {
                if (p->remote_prefix >= last_prefix) {
                    p_with_prefix_besteffort = p;
                    last_prefix = p->remote_prefix;
                }
            }
        }

		/* save the pointer to the anonymous configuration */
		if (p->remote->ss_family == AF_UNSPEC)
			anon = p;
	}

	if (p_withport_besteffort) {
		plog(ASL_LEVEL_DEBUG, "configuration found for %s.\n", buf);
		return p_withport_besteffort;
	}
    if (p_with_prefix) {
        plog(ASL_LEVEL_DEBUG, "configuration found for %s.\n", buf);
        return p_with_prefix;
    }
    if (p_with_prefix_besteffort) {
        plog(ASL_LEVEL_DEBUG, "configuration found for %s.\n", buf);
        return p_with_prefix_besteffort;
    }
	if (allow_anon && anon != NULL) {
		plog(ASL_LEVEL_DEBUG, 
			"anonymous configuration selected for %s.\n", buf);
		return anon;
	}

	plog(ASL_LEVEL_DEBUG, 
		"no remote configuration found.\n");

	return NULL;
}

int
no_remote_configs(ignore_anonymous)
	int ignore_anonymous;
{
	
	struct remoteconf *p;
#if !TARGET_OS_EMBEDDED
	static const char default_idv[] = "macuser@localhost";
	static const int default_idv_len = sizeof(default_idv) - 1;
#endif

	TAILQ_FOREACH(p, &rmtree, chain) {
		if (ignore_anonymous) {
			if (p->remote->ss_family == AF_UNSPEC)	/* anonymous */
				continue;
		}
#if !TARGET_OS_EMBEDDED
		// ignore the default btmm ipv6 config thats always present in racoon.conf
		if (p->remote->ss_family == AF_INET6 &&
			p->idvtype == IDTYPE_USERFQDN &&
			p->idv != NULL &&
			p->idv->l == default_idv_len &&
			strncmp(p->idv->v, default_idv, p->idv->l) == 0) {
			continue;
		}
#endif
		return 0;
	}
	return 1;
}

struct remoteconf *
getrmconf(remote)
	struct sockaddr_storage *remote;
{
	return getrmconf_strict(remote, 1);
}

struct remoteconf *
create_rmconf()
{
	struct remoteconf *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL)
		return NULL;

    new->refcount = 1;
    new->in_list = 0;
	new->proposal = NULL;

	/* set default */
	new->doitype = IPSEC_DOI;
	new->sittype = IPSECDOI_SIT_IDENTITY_ONLY;
    new->ike_version = ISAKMP_VERSION_NUMBER_IKEV1;
	new->idvtype = IDTYPE_UNDEFINED;
	new->idvl_p = genlist_init();
	new->nonce_size = DEFAULT_NONCE_SIZE;
	new->passive = FALSE;
	new->ike_frag = ISAKMP_FRAG_ON;
	new->esp_frag = IP_MAXPACKET;
	new->ini_contact = TRUE;
	new->mode_cfg = FALSE;
	new->pcheck_level = PROP_CHECK_STRICT;
	new->verify_identifier = FALSE;
	new->verify_cert = TRUE;
	new->getcert_method = ISAKMP_GETCERT_PAYLOAD;
	new->cacerttype = ISAKMP_CERT_X509SIGN;
	new->certtype = ISAKMP_CERT_NONE;
	new->send_cert = TRUE;
	new->send_cr = TRUE;
	new->support_proxy = FALSE;
	new->gen_policy = FALSE;
	new->retry_counter = lcconf->retry_counter;
	new->retry_interval = lcconf->retry_interval;
	new->nat_traversal = NATT_ON;
	new->natt_multiple_user = FALSE;
	new->natt_keepalive = TRUE;
	new->idv = NULL;
	new->key = NULL;

	new->dpd = TRUE; /* Enable DPD support by default */
	new->dpd_interval = 0; /* Disable DPD checks by default */
	new->dpd_retry = 5;
	new->dpd_maxfails = 5;
    new->dpd_algo = DPD_ALGO_INBOUND_DETECT;
    new->idle_timeout = 0;

	new->weak_phase1_check = 0;

#ifdef ENABLE_HYBRID
	new->xauth = NULL;
#endif
	new->initiate_ph1rekey = TRUE;
	return new;
}

struct remoteconf *
copyrmconf(struct sockaddr_storage *remote)
{
	struct remoteconf *new, *old;

	old = getrmconf_strict (remote, 0);
	if (old == NULL) {
		plog (ASL_LEVEL_ERR, 
		      "Remote configuration for '%s' not found!\n",
		      saddr2str((struct sockaddr *)remote));
		return NULL;
	}

	new = duprmconf (old);

	return new;
}

void *
dupidvl(void *entry, void *arg)
{
	struct idspec *id;
	struct idspec *old = (struct idspec *) entry;
	id = newidspec();
	if (!id) return (void *) -1;

	if (set_identifier(&id->id, old->idtype, old->id) != 0) {
		racoon_free(id);
		return (void *) -1;
	}

	id->idtype = old->idtype;

	genlist_append(arg, id);
	return NULL;
}

struct remoteconf *
duprmconf (struct remoteconf *rmconf)
{
    struct remoteconf *new;

    new = racoon_calloc(1, sizeof(*new));
    if (new == NULL)
        return NULL;
    memcpy (new, rmconf, sizeof (*new));
    // FIXME: We should duplicate remote, proposal, etc.
    // This is now handled in the cfparse.y
    // new->proposal = ...;

    // zero-out pointers
    new->remote = NULL;
    new->keychainCertRef = NULL;	/* peristant keychain ref for cert */
    new->shared_secret = NULL;	/* shared secret */
    new->open_dir_auth_group = NULL;	/* group to be used to authorize user */
    new->proposal = NULL;
    new->in_list = 0;
    new->refcount = 1;
    new->idv = NULL;
    new->key = NULL;
#ifdef ENABLE_HYBRID
    new->xauth = NULL;
#endif
    
	/* duplicate dynamic structures */
	if (new->etypes)
		new->etypes=dupetypes(new->etypes);
	new->idvl_p = genlist_init();
	genlist_foreach(rmconf->idvl_p, dupidvl, new->idvl_p);

	return new;
}

static void
idspec_free(void *data)
{
	vfree (((struct idspec *)data)->id);
	free (data);
}

static void
proposalspec_free(struct proposalspec *head)
{

	struct proposalspec* next_propsp = head;
		
	while (next_propsp) {
		struct proposalspec* curr_propsp;
		struct secprotospec* next_protosp;
		
		curr_propsp = next_propsp;
		next_propsp = next_propsp->next;
		next_protosp = curr_propsp->spspec;
		while (next_protosp) {
			struct secprotospec* curr_protosp;
			
			curr_protosp = next_protosp;
			next_protosp = next_protosp->next;
			
			if (curr_protosp->gssid)
				free(curr_protosp->gssid);
			if (curr_protosp->remote)
				free(curr_protosp->remote);
			racoon_free(curr_protosp);
		}
		racoon_free(curr_propsp);
	}
}

void
delrmconf(struct remoteconf *rmconf)
{
	if (rmconf->remote)
		racoon_free(rmconf->remote);
#ifdef ENABLE_HYBRID
	if (rmconf->xauth)
		xauth_rmconf_delete(&rmconf->xauth);
#endif
	if (rmconf->etypes) {
		deletypes(rmconf->etypes);
		rmconf->etypes=NULL;
	}
	if (rmconf->idv)
		vfree(rmconf->idv);
	if (rmconf->idvl_p)
		genlist_free(rmconf->idvl_p, idspec_free);
	if (rmconf->dhgrp)
		oakley_dhgrp_free(rmconf->dhgrp);
	if (rmconf->proposal)
		delisakmpsa(rmconf->proposal);
	if (rmconf->prhead)
		proposalspec_free(rmconf->prhead);
	if (rmconf->shared_secret)
		vfree(rmconf->shared_secret);
	if (rmconf->keychainCertRef)
		vfree(rmconf->keychainCertRef);
	if (rmconf->open_dir_auth_group)
		vfree(rmconf->open_dir_auth_group);
    
    if (rmconf->eap_options)
        CFRelease(rmconf->eap_options);
    if (rmconf->eap_types)
        deletypes(rmconf->eap_types);
    if (rmconf->ikev2_cfg_request)
        CFRelease(rmconf->ikev2_cfg_request);

	racoon_free(rmconf);
}

void
delisakmpsa(struct isakmpsa *sa)
{
	if (sa->dhgrp)
		oakley_dhgrp_free(sa->dhgrp);
	if (sa->next)
		delisakmpsa(sa->next);
	racoon_free(sa);
}

struct etypes *
dupetypes(struct etypes *orig)
{
	struct etypes *new;

	if (!orig) 
		return NULL;

	new = racoon_malloc(sizeof(struct etypes));
	if (new == NULL) 
		return NULL;

	new->type = orig->type;
	new->next = NULL;

	if (orig->next)
		new->next=dupetypes(orig->next);

	return new;
}

void
deletypes(struct etypes *e)
{
	if (e->next)
		deletypes(e->next);
	racoon_free(e);
}

/*
 * insert into head of list.
 */
void
insrmconf(struct remoteconf *new)
{
	TAILQ_INSERT_HEAD(&rmtree, new, chain);
    new->in_list = 1;
}

void
remrmconf(struct remoteconf *rmconf)
{
	if (rmconf->in_list)
        TAILQ_REMOVE(&rmtree, rmconf, chain);
    rmconf->in_list = 0;
}

void
retain_rmconf(struct remoteconf *rmconf)
{
    (rmconf->refcount)++;
}

void
release_rmconf(struct remoteconf *rmconf)
{
    if (--(rmconf->refcount) <= 0) {
        remrmconf(rmconf);
        delrmconf(rmconf);
    }
}

void
flushrmconf()
{
	struct remoteconf *p, *next;

	for (p = TAILQ_FIRST(&rmtree); p; p = next) {
		next = TAILQ_NEXT(p, chain);
		remrmconf(p);
        if (--(p->refcount) <= 0)
            delrmconf(p);
	}
}

void
initrmconf()
{
	TAILQ_INIT(&rmtree);
}

/* check exchange type to be acceptable */
struct etypes *
check_etypeok(struct remoteconf *rmconf, u_int8_t etype)
{
	struct etypes *e;

	for (e = rmconf->etypes; e != NULL; e = e->next) {
		if (e->type == etype)
			break;
	}

	return e;
}

/*%%%*/
struct isakmpsa *
newisakmpsa()
{
	struct isakmpsa *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL)
		return NULL;

	/*
	 * Just for sanity, make sure this is initialized.  This is
	 * filled in for real when the ISAKMP proposal is configured.
	 */
	new->vendorid = VENDORID_UNKNOWN;

	new->next = NULL;
	new->rmconf = NULL;

	return new;
}

/*
 * insert into tail of list.
 */
void
insisakmpsa(struct isakmpsa *new, struct remoteconf *rmconf)
{
	struct isakmpsa *p;

	new->rmconf = rmconf;

	if (rmconf->proposal == NULL) {
		rmconf->proposal = new;
		return;
	}

	for (p = rmconf->proposal; p->next != NULL; p = p->next)
		;
	p->next = new;

	return;
}

struct remoteconf *
foreachrmconf(rmconf_func_t rmconf_func, void *data)
{
	struct remoteconf *p, *ret = NULL;

	TAILQ_FOREACH_REVERSE(p, &rmtree, _rmtree, chain) {
		ret = (*rmconf_func)(p, data);
		if (ret)
			break;
	}

	return ret;
}

static void *
dump_peers_identifiers (void *entry, void *arg)
{
	struct idspec *id = (struct idspec*) entry;
	char buf[1024], *pbuf;
	pbuf = buf;
	pbuf += snprintf (pbuf, sizeof(buf) - (pbuf - buf), "\tpeers_identifier %s",
			 s_idtype (id->idtype));
	if (id->id)
		pbuf += snprintf (pbuf, sizeof(buf) - (pbuf - buf), " \"%s\"", id->id->v);
	plog(ASL_LEVEL_INFO, "%s;\n", buf);
	return NULL;
}

static struct remoteconf *
dump_rmconf_single (struct remoteconf *p, void *data)
{
	struct etypes *etype = p->etypes;
	struct isakmpsa *prop = p->proposal;
	char buf[1024], *pbuf;

	pbuf = buf;
    if (p->remote_prefix)
        pbuf += snprintf(pbuf, sizeof(buf) - (pbuf - buf), "remote %s", 
                         saddr2str_with_prefix((struct sockaddr *)p->remote, p->remote_prefix));
    else
        pbuf += snprintf(pbuf, sizeof(buf) - (pbuf - buf), "remote %s", saddr2str((struct sockaddr *)p->remote));
	if (p->inherited_from)
		pbuf += snprintf(pbuf, sizeof(buf) - (pbuf - buf), " inherit %s",
				saddr2str((struct sockaddr *)p->inherited_from->remote));
	plog(ASL_LEVEL_INFO, "%s {\n", buf);
	pbuf = buf;
	pbuf += snprintf(pbuf, sizeof(buf) - (pbuf - buf), "\texchange_type ");
	while (etype) {
		pbuf += snprintf (pbuf, sizeof(buf) - (pbuf - buf), "%s%s", s_etype(etype->type),
				 etype->next != NULL ? ", " : ";\n");
		etype = etype->next;
	}
	plog(ASL_LEVEL_INFO, "%s", buf);
	plog(ASL_LEVEL_INFO, "\tdoi %s;\n", s_doi(p->doitype));
	pbuf = buf;
	pbuf += snprintf(pbuf, sizeof(buf) - (pbuf - buf), "\tmy_identifier %s", s_idtype (p->idvtype));
	if (p->idvtype == IDTYPE_ASN1DN) {
		plog(ASL_LEVEL_INFO, "%s;\n", buf);
		switch (p->getcert_method) {
		  case 0:
		  	break;
		  case ISAKMP_GETCERT_PAYLOAD:
			plog(ASL_LEVEL_INFO, "\t/* peers certificate from payload */\n");
			break;
		  default:
			plog(ASL_LEVEL_INFO, "\tpeers_certfile *UNKNOWN* (%d)\n", p->getcert_method);
		}
	}
	else {
		if (p->idv)
			pbuf += snprintf (pbuf, sizeof(buf) - (pbuf - buf), " \"%s\"", p->idv->v);
		plog(ASL_LEVEL_INFO, "%s;\n", buf);
		genlist_foreach(p->idvl_p, &dump_peers_identifiers, NULL);
	}

	plog(ASL_LEVEL_INFO, "\tsend_cert %s;\n",
		s_switch (p->send_cert));
	plog(ASL_LEVEL_INFO, "\tsend_cr %s;\n",
		s_switch (p->send_cr));
	plog(ASL_LEVEL_INFO, "\tverify_cert %s;\n",
		s_switch (p->verify_cert));
	plog(ASL_LEVEL_INFO, "\tverify_identifier %s;\n",
		s_switch (p->verify_identifier));
	plog(ASL_LEVEL_INFO, "\tnat_traversal %s;\n",
		p->nat_traversal == NATT_FORCE ?
			"force" : s_switch (p->nat_traversal));
	plog(ASL_LEVEL_INFO, "\tnatt_multiple_user %s;\n",
		s_switch (p->natt_multiple_user));
	plog(ASL_LEVEL_INFO, "\tnonce_size %d;\n",
		p->nonce_size);
	plog(ASL_LEVEL_INFO, "\tpassive %s;\n",
		s_switch (p->passive));
	plog(ASL_LEVEL_INFO, "\tike_frag %s;\n",
		p->ike_frag == ISAKMP_FRAG_FORCE ?
			"force" : s_switch (p->ike_frag));
	plog(ASL_LEVEL_INFO, "\tesp_frag %d;\n", p->esp_frag);
	plog(ASL_LEVEL_INFO, "\tinitial_contact %s;\n",
		s_switch (p->ini_contact));
	plog(ASL_LEVEL_INFO, "\tgenerate_policy %s;\n",
		s_switch (p->gen_policy));
	plog(ASL_LEVEL_INFO, "\tsupport_proxy %s;\n",
		s_switch (p->support_proxy));

	while (prop) {
		plog(ASL_LEVEL_INFO, "\n");
		plog(ASL_LEVEL_INFO, 
			"\t/* prop_no=%d, trns_no=%d, rmconf=%s */\n",
			prop->prop_no, prop->trns_no,
			saddr2str((struct sockaddr *)prop->rmconf->remote));
		plog(ASL_LEVEL_INFO, "\tproposal {\n");
		plog(ASL_LEVEL_INFO, "\t\tlifetime time %lu sec;\n",
			(long)prop->lifetime);
		plog(ASL_LEVEL_INFO, "\t\tlifetime bytes %zd;\n",
			prop->lifebyte);
		plog(ASL_LEVEL_INFO, "\t\tdh_group %s;\n",
			alg_oakley_dhdef_name(prop->dh_group));
		plog(ASL_LEVEL_INFO, "\t\tencryption_algorithm %s;\n", 
			alg_oakley_encdef_name(prop->enctype));
		plog(ASL_LEVEL_INFO, "\t\thash_algorithm %s;\n",
			alg_oakley_hashdef_name(prop->hashtype));
		plog(ASL_LEVEL_INFO, "\t\tprf_algorithm %s;\n",
			 alg_oakley_hashdef_name(prop->prf));
		plog(ASL_LEVEL_INFO, "\t\tauthentication_method %s;\n",
			alg_oakley_authdef_name(prop->authmethod));
		plog(ASL_LEVEL_INFO, "\t}\n");
		prop = prop->next;
	}
	plog(ASL_LEVEL_INFO, "}\n");
	plog(ASL_LEVEL_INFO, "\n");

	return NULL;
}

void
dumprmconf()
{
	foreachrmconf (dump_rmconf_single, NULL);
}

struct idspec *
newidspec()
{
	struct idspec *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL)
		return NULL;
	new->idtype = IDTYPE_ADDRESS;

	return new;
}


struct isakmpsa *
dupisakmpsa(struct isakmpsa *sa)
{
	struct isakmpsa *res = NULL;

	if (sa == NULL)
		return NULL;

	res = newisakmpsa();
	if(res == NULL)
		return NULL;

	*res = *sa;
	res->next=NULL;

	if (sa->dhgrp != NULL)
		oakley_setdhgroup(sa->dh_group, &(res->dhgrp));

	return res;

}

