/* $Id: pfkey.c,v 1.31.2.10 2005/10/03 14:52:19 manubsd Exp $ */

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
#include "racoon_types.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef ENABLE_NATT
#include <netinet/udp.h>
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/route.h>
#include <net/pfkeyv2.h>

#include <netinet/in.h>
#ifndef HAVE_NETINET6_IPSEC
#include <netinet/ipsec.h>
#else
#include <netinet6/ipsec.h>
#endif

#include "libpfkey.h"

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "debug.h"
#include "fsm.h"
#include "ike_session.h"

#include "schedule.h"
#include "localconf.h"
#include "remoteconf.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "isakmp_inf.h"
#include "ipsec_doi.h"
#include "oakley.h"
#include "pfkey.h"
#include "handler.h"
#include "policy.h"
#include "algorithm.h"
#include "sainfo.h"
#include "proposal.h"
#include "strnames.h"
#include "gcmalloc.h"
#include "nattraversal.h"
#include "crypto_openssl.h"
#include "grabmyaddr.h"
#include "vpn_control.h"
#include "vpn_control_var.h"
#include "ike_session.h"
#include "ipsecSessionTracer.h"
#include "ipsecMessageTracer.h"
#include "power_mgmt.h"
#include "session.h"

#if defined(SADB_X_EALG_RIJNDAELCBC) && !defined(SADB_X_EALG_AESCBC)
#define SADB_X_EALG_AESCBC  SADB_X_EALG_RIJNDAELCBC
#endif

/* prototype */
static u_int ipsecdoi2pfkey_aalg (u_int);
static u_int ipsecdoi2pfkey_ealg (u_int);
static u_int ipsecdoi2pfkey_calg (u_int);
static u_int ipsecdoi2pfkey_alg (u_int, u_int);
static u_int keylen_aalg (u_int);
static u_int keylen_ealg (u_int, int);

static int pk_recvgetspi (caddr_t *);
static int pk_recvupdate (caddr_t *);
static int pk_recvadd (caddr_t *);
static int pk_recvdelete (caddr_t *);
static int pk_recvacquire (caddr_t *);
static int pk_recvexpire (caddr_t *);
static int pk_recvflush (caddr_t *);
static int getsadbpolicy (caddr_t *, int *, int, phase2_handle_t *);
static int pk_recvspdupdate (caddr_t *);
static int pk_recvspdadd (caddr_t *);
static int pk_recvspddelete (caddr_t *);
static int pk_recvspdexpire (caddr_t *);
static int pk_recvspdget (caddr_t *);
static int pk_recvspddump (caddr_t *);
static int pk_recvspdflush (caddr_t *);
static int pk_recvgetsastat (caddr_t *);
static struct sadb_msg *pk_recv (int, ssize_t *);

static int (*pkrecvf[]) (caddr_t *) = {
NULL,
pk_recvgetspi,
pk_recvupdate,
pk_recvadd,
pk_recvdelete,
NULL,	/* SADB_GET */
pk_recvacquire,
NULL,	/* SABD_REGISTER */
pk_recvexpire,
pk_recvflush,
NULL,	/* SADB_DUMP */
NULL,	/* SADB_X_PROMISC */
NULL,	/* SADB_X_PCHANGE */
pk_recvspdupdate,
pk_recvspdadd,
pk_recvspddelete,
pk_recvspdget,
NULL,	/* SADB_X_SPDACQUIRE */
pk_recvspddump,
pk_recvspdflush,
NULL,	/* SADB_X_SPDSETIDX */
pk_recvspdexpire,
NULL,	/* SADB_X_SPDDELETE2 */
pk_recvgetsastat, /* SADB_GETSASTAT */
NULL,	/* SADB_X_NAT_T_NEW_MAPPING */
NULL, /* SADB_X_MIGRATE */
#if (SADB_MAX > 25)
#error "SADB extra message?"
#endif
};

static int addnewsp (caddr_t *);

/* cope with old kame headers - ugly */
#ifndef SADB_X_AALG_MD5
#define SADB_X_AALG_MD5		SADB_AALG_MD5	
#endif
#ifndef SADB_X_AALG_SHA
#define SADB_X_AALG_SHA		SADB_AALG_SHA
#endif
#ifndef SADB_X_AALG_NULL
#define SADB_X_AALG_NULL	SADB_AALG_NULL
#endif

#ifndef SADB_X_EALG_BLOWFISHCBC
#define SADB_X_EALG_BLOWFISHCBC	SADB_EALG_BLOWFISHCBC
#endif
#ifndef SADB_X_EALG_CAST128CBC
#define SADB_X_EALG_CAST128CBC	SADB_EALG_CAST128CBC
#endif
#ifndef SADB_X_EALG_RC5CBC
#ifdef SADB_EALG_RC5CBC
#define SADB_X_EALG_RC5CBC	SADB_EALG_RC5CBC
#endif
#endif

int
pfkey_process(msg)
	struct sadb_msg *msg;
{
	caddr_t mhp[SADB_EXT_MAX + 1];
	int error = -1;
	
    // Special debug use only - creates large logs
	// plogdump(ASL_LEVEL_DEBUG, msg, msg->sadb_msg_len << 3, "get pfkey %s message\n",
	//		 s_pfkey_type(msg->sadb_msg_type));

	/* validity check */
    /* check pfkey message. */
	if (pfkey_align(msg, mhp)) {
		plog(ASL_LEVEL_ERR,
             "libipsec failed pfkey align (%s)\n",
             ipsec_strerror());
		goto end;
	}
	if (pfkey_check(mhp)) {
		plog(ASL_LEVEL_ERR,
             "libipsec failed pfkey check (%s)\n",
             ipsec_strerror());
		goto end;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];             // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
    
	if (msg->sadb_msg_errno) {
		int pri;

		/* when SPD is empty, treat the state as no error. */
		if (msg->sadb_msg_type == SADB_X_SPDDUMP &&
		    msg->sadb_msg_errno == ENOENT)
			pri = ASL_LEVEL_DEBUG;
		else
			pri = ASL_LEVEL_ERR;

		plog(pri, 
			"pfkey %s failed: %s\n",
			s_pfkey_type(msg->sadb_msg_type),
			strerror(msg->sadb_msg_errno));
		goto end;
	}
    
	/* safety check */
	if (msg->sadb_msg_type >= ARRAYLEN(pkrecvf)) {
		plog(ASL_LEVEL_ERR, 
			"unknown PF_KEY message type=%u\n",
			msg->sadb_msg_type);
		goto end;
	}

	if (pkrecvf[msg->sadb_msg_type] == NULL) {
		plog(ASL_LEVEL_INFO, 
			"unsupported PF_KEY message %s\n",
			s_pfkey_type(msg->sadb_msg_type));
		goto end;
	}

	if ((pkrecvf[msg->sadb_msg_type])(mhp) < 0)
		goto end;

	error = 0;
end:
	if (msg)
		racoon_free(msg);
	return(error);
}

/*
 * PF_KEY packet handler
 *	0: success
 *	-1: fail
 */

//%%%%%%%%%%%%%%%%%% need to handle errors encountered here - this no longer returns a result
void
pfkey_handler(void *unused)
{
	struct sadb_msg *msg;
	ssize_t len;

	if (slept_at || woke_at) {
		plog(ASL_LEVEL_DEBUG, 
			 "ignoring pfkey port until power-mgmt event is handled.\n");
		return;
	}
	
	/* receive pfkey message. */
	len = 0;
	msg = (struct sadb_msg *)pk_recv(lcconf->sock_pfkey, &len);

	if (msg == NULL) {
		if (len < 0) {
			plog(ASL_LEVEL_ERR, 
				 "failed to recv from pfkey (%s)\n",
				 strerror(errno));
			return;			
		} else {
			/* short message - msg not ready */
			plog(ASL_LEVEL_DEBUG, "recv short message from pfkey\n");
			return;
		}
	}
	pfkey_process(msg);
}

void
pfkey_post_handler()
{
	struct saved_msg_elem *elem;
	struct saved_msg_elem *elem_tmp = NULL;

	if (slept_at || woke_at) {
		plog(ASL_LEVEL_DEBUG, 
			 "ignoring (saved) pfkey messages until power-mgmt event is handled.\n");
		return;
	}

	TAILQ_FOREACH_SAFE(elem, &lcconf->saved_msg_queue, chain, elem_tmp) {
		pfkey_process((struct sadb_msg *)elem->msg);
		TAILQ_REMOVE(&lcconf->saved_msg_queue, elem, chain);
		racoon_free(elem);

	}
}

int
pfkey_save_msg(msg)
	struct sadb_msg *msg;
{
	struct saved_msg_elem *elem;
	
	elem = (struct saved_msg_elem *)racoon_calloc(sizeof(struct saved_msg_elem), 1);
	if (elem == NULL)
		return -1;
	elem->msg = msg;
	TAILQ_INSERT_TAIL(&lcconf->saved_msg_queue, elem, chain);
	return 0;
}

/*
 * dump SADB
 */
vchar_t *
pfkey_dump_sadb(satype)
	int satype;
{
	int s = -1;
	vchar_t *buf = NULL;
	pid_t pid = getpid();
	struct sadb_msg *msg = NULL;
	size_t bl, ml;
	ssize_t len;

	if ((s = pfkey_open()) < 0) {
		plog(ASL_LEVEL_ERR, 
			"libipsec failed pfkey open: %s\n",
			ipsec_strerror());
		return NULL;
	}

	plog(ASL_LEVEL_DEBUG, "call pfkey_send_dump\n");
	if (pfkey_send_dump(s, satype) < 0) {
		plog(ASL_LEVEL_ERR, 
			"libipsec failed dump: %s\n", ipsec_strerror());
		goto fail;
	}

	while (1) {
		if (msg)
			racoon_free(msg);
		msg = pk_recv(s, &len);
		if (msg == NULL) {
			if (len < 0)
				goto done;
			else
				continue;
		}

		/*
		 * for multi-processor system this had to be added because the messages can
		 * be interleaved - they won't all be dump messages
		 */
		if (msg->sadb_msg_type != SADB_DUMP) {	/* save for later processing */
			pfkey_save_msg(msg);
			msg = NULL;
			continue;
		}

		// ignore dump messages that aren't racoon's
		if (msg->sadb_msg_pid != pid)
			continue;

		ml = msg->sadb_msg_len << 3;
		bl = buf ? buf->l : 0;
		buf = vrealloc(buf, bl + ml);
		if (buf == NULL) {
			plog(ASL_LEVEL_ERR, 
				"failed to reallocate buffer to dump.\n");
			goto fail;
		}
		memcpy(buf->v + bl, msg, ml);

		if (msg->sadb_msg_seq == 0)
			break;
	}
	goto done;

fail:
	if (buf)
		vfree(buf);
	buf = NULL;
done:
	if (msg)
		racoon_free(msg);
	if (s >= 0)
		pfkey_close_sock(s);
	return buf;
}


/*
 * These are the SATYPEs that we manage.  We register to get
 * PF_KEY messages related to these SATYPEs, and we also use
 * this list to determine which SATYPEs to delete SAs for when
 * we receive an INITIAL-CONTACT.
 */
const struct pfkey_satype pfkey_satypes[] = {
	{ SADB_SATYPE_AH,	"AH" },
	{ SADB_SATYPE_ESP,	"ESP" },
	{ SADB_X_SATYPE_IPCOMP,	"IPCOMP" },
};
const int pfkey_nsatypes =
    sizeof(pfkey_satypes) / sizeof(pfkey_satypes[0]);

/*
 * PF_KEY initialization
 */
int
pfkey_init(void)
{
	int i, reg_fail, sock;

	if ((lcconf->sock_pfkey = pfkey_open()) < 0) {
		plog(ASL_LEVEL_ERR, 
			"libipsec failed pfkey open (%s)\n", ipsec_strerror());
		return -1;
	}

	for (i = 0, reg_fail = 0; i < pfkey_nsatypes; i++) {
		plog(ASL_LEVEL_DEBUG, 
		    "call pfkey_send_register for %s\n",
		    pfkey_satypes[i].ps_name);
		if (pfkey_send_register(lcconf->sock_pfkey,
					pfkey_satypes[i].ps_satype) < 0 ||
		    pfkey_recv_register(lcconf->sock_pfkey) < 0) {
			plog(ASL_LEVEL_WARNING, 
			    "failed to register %s (%s)\n",
			    pfkey_satypes[i].ps_name,
			    ipsec_strerror());
			reg_fail++;
		}
	}

	if (reg_fail == pfkey_nsatypes) {
		plog(ASL_LEVEL_ERR, 
			"failed to regist any protocol.\n");
        close(lcconf->sock_pfkey);
		return -1;
	}
    initsp();
    
    lcconf->pfkey_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, lcconf->sock_pfkey, 0, dispatch_get_main_queue());
    if (lcconf->pfkey_source == NULL) {
        plog(ASL_LEVEL_ERR, "could not create pfkey socket source.");
        return -1;
    }
    dispatch_source_set_event_handler_f(lcconf->pfkey_source, pfkey_handler);
    sock = lcconf->sock_pfkey;
    dispatch_source_set_cancel_handler(lcconf->pfkey_source, 
                                       ^{ 
                                           pfkey_close_sock(sock);
                                       });
    dispatch_resume(lcconf->pfkey_source);

	if (pfkey_send_spddump(lcconf->sock_pfkey) < 0) {
		plog(ASL_LEVEL_ERR, 
			"libipsec sending spddump failed: %s\n",
			ipsec_strerror());
		pfkey_close();
		return -1;
	}
#if 0
	if (pfkey_promisc_toggle(1) < 0) {
		pfkey_close();
		return -1;
	}
#endif
    
	return 0;
}

void
pfkey_close(void) 
{
    dispatch_source_cancel(lcconf->pfkey_source);
    lcconf->pfkey_source = NULL;
}

/* %%% for conversion */
/* IPSECDOI_ATTR_AUTH -> SADB_AALG */
static u_int
ipsecdoi2pfkey_aalg(hashtype)
	u_int hashtype;
{
	switch (hashtype) {
        case IPSECDOI_ATTR_AUTH_HMAC_MD5:
        case IPSECDOI_ATTR_AUTH_HMAC_MD5_96:
            return SADB_AALG_MD5HMAC;
        case IPSECDOI_ATTR_AUTH_HMAC_SHA1:
        case IPSECDOI_ATTR_AUTH_HMAC_SHA1_96:
            return SADB_AALG_SHA1HMAC;
        case IPSECDOI_ATTR_AUTH_HMAC_SHA2_256:
#if (defined SADB_X_AALG_SHA2_256) && !defined(SADB_X_AALG_SHA2_256HMAC)
            return SADB_X_AALG_SHA2_256;
#else
            return SADB_X_AALG_SHA2_256HMAC;
#endif
        case IPSECDOI_ATTR_AUTH_HMAC_SHA2_384:
#if (defined SADB_X_AALG_SHA2_384) && !defined(SADB_X_AALG_SHA2_384HMAC)
            return SADB_X_AALG_SHA2_384;
#else
            return SADB_X_AALG_SHA2_384HMAC;
#endif
        case IPSECDOI_ATTR_AUTH_HMAC_SHA2_512:
#if (defined SADB_X_AALG_SHA2_512) && !defined(SADB_X_AALG_SHA2_512HMAC)
            return SADB_X_AALG_SHA2_512;
#else
            return SADB_X_AALG_SHA2_512HMAC;
#endif
        case IPSECDOI_ATTR_AUTH_KPDK:		/* need special care */
            return SADB_AALG_NONE;
            
            /* not supported */
        case IPSECDOI_ATTR_AUTH_DES_MAC:
            plog(ASL_LEVEL_ERR,
                 "Not supported hash type: %u\n", hashtype);
            return ~0;
            
        case 0: /* reserved */
        default:
            return SADB_AALG_NONE;
            
            plog(ASL_LEVEL_ERR, 
                 "Invalid hash type: %u\n", hashtype);
            return ~0;
	}
	/*NOTREACHED*/
}

/* IPSECDOI_ESP -> SADB_EALG */
static u_int
ipsecdoi2pfkey_ealg(t_id)
	u_int t_id;
{
	switch (t_id) {
	case IPSECDOI_ESP_DES_IV64:		/* sa_flags |= SADB_X_EXT_OLD */
		return SADB_EALG_DESCBC;
	case IPSECDOI_ESP_DES:
		return SADB_EALG_DESCBC;
	case IPSECDOI_ESP_3DES:
		return SADB_EALG_3DESCBC;
#ifdef SADB_X_EALG_RC5CBC
	case IPSECDOI_ESP_RC5:
		return SADB_X_EALG_RC5CBC;
#endif
	case IPSECDOI_ESP_CAST:
		return SADB_X_EALG_CAST128CBC;
	case IPSECDOI_ESP_BLOWFISH:
		return SADB_X_EALG_BLOWFISHCBC;
	case IPSECDOI_ESP_DES_IV32:	/* flags |= (SADB_X_EXT_OLD|
							SADB_X_EXT_IV4B)*/
		return SADB_EALG_DESCBC;
	case IPSECDOI_ESP_NULL:
		return SADB_EALG_NULL;
#ifdef SADB_X_EALG_AESCBC
	case IPSECDOI_ESP_AES:
		return SADB_X_EALG_AESCBC;
#endif
#ifdef SADB_X_EALG_TWOFISHCBC
	case IPSECDOI_ESP_TWOFISH:
		return SADB_X_EALG_TWOFISHCBC;
#endif

	/* not supported */
	case IPSECDOI_ESP_3IDEA:
	case IPSECDOI_ESP_IDEA:
	case IPSECDOI_ESP_RC4:
		plog(ASL_LEVEL_ERR, 
			"Not supported transform: %u\n", t_id);
		return ~0;

	case 0: /* reserved */
	default:
		plog(ASL_LEVEL_ERR, 
			"Invalid transform id: %u\n", t_id);
		return ~0;
	}
	/*NOTREACHED*/
}

/* IPCOMP -> SADB_CALG */
static u_int
ipsecdoi2pfkey_calg(t_id)
	u_int t_id;
{
	switch (t_id) {
	case IPSECDOI_IPCOMP_OUI:
		return SADB_X_CALG_OUI;
	case IPSECDOI_IPCOMP_DEFLATE:
		return SADB_X_CALG_DEFLATE;
	case IPSECDOI_IPCOMP_LZS:
		return SADB_X_CALG_LZS;

	case 0: /* reserved */
	default:
		plog(ASL_LEVEL_ERR, 
			"Invalid transform id: %u\n", t_id);
		return ~0;
	}
	/*NOTREACHED*/
}

/* IPSECDOI_PROTO -> SADB_SATYPE */
u_int
ipsecdoi2pfkey_proto(proto)
	u_int proto;
{
	switch (proto) {
	case IPSECDOI_PROTO_IPSEC_AH:
		return SADB_SATYPE_AH;
	case IPSECDOI_PROTO_IPSEC_ESP:
		return SADB_SATYPE_ESP;
	case IPSECDOI_PROTO_IPCOMP:
		return SADB_X_SATYPE_IPCOMP;

	default:
		plog(ASL_LEVEL_ERR, 
			"Invalid ipsec_doi proto: %u\n", proto);
		return ~0;
	}
	/*NOTREACHED*/
}

static u_int
ipsecdoi2pfkey_alg(algclass, type)
	u_int algclass, type;
{
	switch (algclass) {
	case IPSECDOI_ATTR_AUTH:
		return ipsecdoi2pfkey_aalg(type);
	case IPSECDOI_PROTO_IPSEC_ESP:
		return ipsecdoi2pfkey_ealg(type);
	case IPSECDOI_PROTO_IPCOMP:
		return ipsecdoi2pfkey_calg(type);
	default:
		plog(ASL_LEVEL_ERR, 
			"Invalid ipsec_doi algclass: %u\n", algclass);
		return ~0;
	}
	/*NOTREACHED*/
}

/* SADB_SATYPE -> IPSECDOI_PROTO */
u_int
pfkey2ipsecdoi_proto(satype)
	u_int satype;
{
	switch (satype) {
	case SADB_SATYPE_AH:
		return IPSECDOI_PROTO_IPSEC_AH;
	case SADB_SATYPE_ESP:
		return IPSECDOI_PROTO_IPSEC_ESP;
	case SADB_X_SATYPE_IPCOMP:
		return IPSECDOI_PROTO_IPCOMP;

	default:
		plog(ASL_LEVEL_ERR, 
			"Invalid pfkey proto: %u\n", satype);
		return ~0;
	}
	/*NOTREACHED*/
}

/* IPSECDOI_ATTR_ENC_MODE -> IPSEC_MODE */
u_int
ipsecdoi2pfkey_mode(mode)
	u_int mode;
{
	switch (mode) {
	case IPSECDOI_ATTR_ENC_MODE_TUNNEL:
#ifdef ENABLE_NATT
	case IPSECDOI_ATTR_ENC_MODE_UDPTUNNEL_RFC:
	case IPSECDOI_ATTR_ENC_MODE_UDPTUNNEL_DRAFT:
#endif
		return IPSEC_MODE_TUNNEL;
	case IPSECDOI_ATTR_ENC_MODE_TRNS:
#ifdef ENABLE_NATT
	case IPSECDOI_ATTR_ENC_MODE_UDPTRNS_RFC:
	case IPSECDOI_ATTR_ENC_MODE_UDPTRNS_DRAFT:
#endif
		return IPSEC_MODE_TRANSPORT;
	default:
		plog(ASL_LEVEL_ERR, "Invalid mode type: %u\n", mode);
		return ~0;
	}
	/*NOTREACHED*/
}

/* IPSECDOI_ATTR_ENC_MODE -> IPSEC_MODE */
u_int
pfkey2ipsecdoi_mode(mode)
	u_int mode;
{
	switch (mode) {
	case IPSEC_MODE_TUNNEL:
		return IPSECDOI_ATTR_ENC_MODE_TUNNEL;
	case IPSEC_MODE_TRANSPORT:
		return IPSECDOI_ATTR_ENC_MODE_TRNS;
	case IPSEC_MODE_ANY:
		return IPSECDOI_ATTR_ENC_MODE_ANY;
	default:
		plog(ASL_LEVEL_ERR, "Invalid mode type: %u\n", mode);
		return ~0;
	}
	/*NOTREACHED*/
}

/* default key length for encryption algorithm */
static u_int
keylen_aalg(hashtype)
	u_int hashtype;
{
	int res;

	if (hashtype == 0)
		return SADB_AALG_NONE;

	res = alg_ipsec_hmacdef_hashlen(hashtype);
	if (res == -1) {
		plog(ASL_LEVEL_ERR, 
			"invalid hmac algorithm %u.\n", hashtype);
		return ~0;
	}
	return res;
}

/* default key length for encryption algorithm */
static u_int
keylen_ealg(enctype, encklen)
	u_int enctype;
	int encklen;
{
	int res;

	res = alg_ipsec_encdef_keylen(enctype, encklen);
	if (res == -1) {
		plog(ASL_LEVEL_ERR, 
			"invalid encryption algorithm %u.\n", enctype);
		return ~0;
	}
	return res;
}

int
pfkey_convertfromipsecdoi(iph2, proto_id, t_id, hashtype,
		e_type, e_keylen, a_type, a_keylen, flags)
    phase2_handle_t *iph2;
	u_int proto_id;
	u_int t_id;
	u_int hashtype;
	u_int *e_type;
	u_int *e_keylen;
	u_int *a_type;
	u_int *a_keylen;
	u_int *flags;
{
	*flags = 0;
	switch (proto_id) {
	case IPSECDOI_PROTO_IPSEC_ESP:
		if ((*e_type = ipsecdoi2pfkey_ealg(t_id)) == ~0)
			goto bad;
		if ((*e_keylen = keylen_ealg(t_id, *e_keylen)) == ~0)
			goto bad;
		*e_keylen >>= 3;

		if ((*a_type = ipsecdoi2pfkey_aalg(hashtype)) == ~0)
			goto bad;
		if ((*a_keylen = keylen_aalg(hashtype)) == ~0)
			goto bad;
		*a_keylen >>= 3;
			
		if (*e_type == SADB_EALG_NONE) {
			plog(ASL_LEVEL_ERR, "no ESP algorithm.\n");
			goto bad;
		}
		break;

	case IPSECDOI_PROTO_IPSEC_AH:
		if ((*a_type = ipsecdoi2pfkey_aalg(hashtype)) == ~0)
			goto bad;
		if ((*a_keylen = keylen_aalg(hashtype)) == ~0)
			goto bad;
		*a_keylen >>= 3;

		if (t_id == IPSECDOI_ATTR_AUTH_HMAC_MD5 
		 && hashtype == IPSECDOI_ATTR_AUTH_KPDK) {
			/* AH_MD5 + Auth(KPDK) = RFC1826 keyed-MD5 */
			*a_type = SADB_X_AALG_MD5;
			*flags |= SADB_X_EXT_OLD;
		}
		*e_type = SADB_EALG_NONE;
		*e_keylen = 0;
		if (*a_type == SADB_AALG_NONE) {
			plog(ASL_LEVEL_ERR, "no AH algorithm.\n");
			goto bad;
		}
		break;

	case IPSECDOI_PROTO_IPCOMP:
		if ((*e_type = ipsecdoi2pfkey_calg(t_id)) == ~0)
			goto bad;
		*e_keylen = 0;

		*flags = SADB_X_EXT_RAWCPI;

		*a_type = SADB_AALG_NONE;
		*a_keylen = 0;
		if (*e_type == SADB_X_CALG_NONE) {
			plog(ASL_LEVEL_ERR, "no IPCOMP algorithm.\n");
			goto bad;
		}
		break;

	default:
		plog(ASL_LEVEL_ERR, "unknown IPsec protocol.\n");
		goto bad;
	}

	return 0;

    bad:
	errno = EINVAL;
	return -1;
}

/* called from scheduler */
void
pfkey_timeover_stub(p)
	void *p;
{

	pfkey_timeover((phase2_handle_t *)p);
}

void
pfkey_timeover(iph2)
	phase2_handle_t *iph2;
{
	plog(ASL_LEVEL_ERR, 
		"%s give up to get IPsec-SA due to time up to wait.\n",
		saddrwop2str((struct sockaddr *)iph2->dst));
	SCHED_KILL(iph2->sce);

	/* If initiator side, send error to kernel by SADB_ACQUIRE. */
	if (iph2->side == INITIATOR)
		pk_sendeacquire(iph2);

	ike_session_unlink_phase2(iph2);

	return;
}

/*%%%*/
/* send getspi message per ipsec protocol per remote address */
/*
 * the local address and remote address in ph1handle are dealed
 * with destination address and source address respectively.
 * Because SPI is decided by responder.
 */
int
pk_sendgetspi(iph2)
	phase2_handle_t *iph2;
{
	struct sockaddr_storage *src = NULL, *dst = NULL;
	u_int satype, mode;
	struct saprop *pp;
	struct saproto *pr;
	u_int32_t minspi, maxspi;
	int proxy = 0;

	if (iph2->side == INITIATOR) {
		pp = iph2->proposal;
		proxy = iph2->ph1->rmconf->support_proxy;
	} else {
		pp = iph2->approval;
		if (iph2->sainfo && iph2->sainfo->id_i)
			proxy = 1;
	}

	/* for mobile IPv6 */
	if (proxy && iph2->src_id && iph2->dst_id &&
	    ipsecdoi_transportmode(pp)) {
		src = iph2->src_id;
		dst = iph2->dst_id;
	} else {
		src = iph2->src;
		dst = iph2->dst;
	}

	for (pr = pp->head; pr != NULL; pr = pr->next) {

		/* validity check */
		satype = ipsecdoi2pfkey_proto(pr->proto_id);
		if (satype == ~0) {
			plog(ASL_LEVEL_ERR, 
				"invalid proto_id %d\n", pr->proto_id);
			return -1;
		}
		/* this works around a bug in Linux kernel where it allocates 4 byte
		   spi's for IPCOMP */
		else if (satype == SADB_X_SATYPE_IPCOMP) {
			minspi = 0x100;
			maxspi = 0xffff;
		}
		else {
			minspi = 0;
			maxspi = 0;
		}
		mode = ipsecdoi2pfkey_mode(pr->encmode);
		if (mode == ~0) {
			plog(ASL_LEVEL_ERR, 
				"invalid encmode %d\n", pr->encmode);
			return -1;
		}

		plog(ASL_LEVEL_DEBUG, "call pfkey_send_getspi\n");
		if (pfkey_send_getspi(
				lcconf->sock_pfkey,
				satype,
				mode,
				dst,			/* src of SA */
				src,			/* dst of SA */
				minspi, maxspi,
				pr->reqid_in, 0, 0, iph2->seq, 0) < 0) {
			plog(ASL_LEVEL_ERR, 
				"ipseclib failed send getspi (%s)\n",
				ipsec_strerror());
			return -1;
		}

		plog(ASL_LEVEL_DEBUG, 
			"pfkey GETSPI sent: %s\n",
			sadbsecas2str(dst, src, satype, 0, mode));
	}

	return 0;
}

/*
 * receive GETSPI from kernel.
 */
static int
pk_recvgetspi(mhp) 
	caddr_t *mhp;
{
	struct sadb_msg *msg;
	struct sadb_sa *sa;
	phase2_handle_t *iph2;
	struct sockaddr_storage *dst;
	int proto_id;
	int allspiok, notfound;
	struct saprop *pp;
	struct saproto *pr;

	/* validity check */
	if (mhp[SADB_EXT_SA] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Inappropriate sadb getspi message passed.\n");
		return -1;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];                     // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	sa = ALIGNED_CAST(struct sadb_sa *)mhp[SADB_EXT_SA];
	dst = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]); /* note SA dir */   

	/* the message has to be processed or not ? */
	if (msg->sadb_msg_pid != getpid()) {
		plog(ASL_LEVEL_DEBUG, 
			"%s message is not interesting "
			"because pid %d is not mine.\n",
			s_pfkey_type(msg->sadb_msg_type),
			msg->sadb_msg_pid);
		return -1;
	}

	iph2 = ike_session_getph2byseq(msg->sadb_msg_seq);
	if (iph2 == NULL) {
		plog(ASL_LEVEL_DEBUG, 
			"Seq %d of %s message not interesting.\n",
			msg->sadb_msg_seq,
			s_pfkey_type(msg->sadb_msg_type));
		return -1;
	}

	if (iph2->is_dying) {
		plog(ASL_LEVEL_ERR,
			 "Status mismatch Phase 2 dying (db:%d)\n",
			 iph2->status);
		return -1;
	}
    
	switch (iph2->version) {
        case ISAKMP_VERSION_NUMBER_IKEV1:
            if (iph2->status != IKEV1_STATE_QUICK_I_GETSPISENT &&
                iph2->status != IKEV1_STATE_QUICK_R_GETSPISENT) {
                plog(ASL_LEVEL_ERR, "Status mismatch (db:%d)\n", iph2->status);
                return -1;
            }
            // check the underlying iph2->ph1
            if (!iph2->ph1) {
                if (!ike_session_update_ph2_ph1bind(iph2)) {
                    plog(ASL_LEVEL_ERR, 
                         "Can't proceed with getspi for  %s. no suitable ISAKMP-SA found \n",
                         saddrwop2str((struct sockaddr *)iph2->dst));
                    ike_session_unlink_phase2(iph2);
                    return -1;
                }
            }
            break;
        default:
            plog(ASL_LEVEL_ERR, "Internal error: invalid IKE major version %d\n", iph2->version);
            return -1;
    }

	/* set SPI, and check to get all spi whether or not */
	allspiok = 1;
	notfound = 1;
	proto_id = pfkey2ipsecdoi_proto(msg->sadb_msg_satype);
	pp = iph2->side == INITIATOR ? iph2->proposal : iph2->approval;

	for (pr = pp->head; pr != NULL; pr = pr->next) {
		if (pr->proto_id == proto_id && pr->spi == 0) {
			pr->spi = sa->sadb_sa_spi;
			notfound = 0;
			plog(ASL_LEVEL_DEBUG, 
				"pfkey GETSPI succeeded: %s\n",
				sadbsecas2str(iph2->dst, iph2->src,
				    msg->sadb_msg_satype,
				    sa->sadb_sa_spi,
				    ipsecdoi2pfkey_mode(pr->encmode)));
		}
		if (pr->spi == 0)
			allspiok = 0;	/* not get all spi */
	}

	if (notfound) {
		plog(ASL_LEVEL_ERR, 
			"Get spi for unknown address %s\n",
			saddrwop2str((struct sockaddr *)iph2->dst));
        ike_session_unlink_phase2(iph2);
		return -1;
	}

	if (allspiok) {
        switch (iph2->version) {
            case ISAKMP_VERSION_NUMBER_IKEV1:
                if (isakmp_post_getspi(iph2) < 0) {
                    plog(ASL_LEVEL_ERR, "IKEv1 post getspi failed.\n");
                    ike_session_unlink_phase2(iph2);
                    iph2 = NULL;
                    return -1;
                }
                break;
        }
	}       
	return 0;
}

/*
 * set inbound SA
 */
int
pk_sendupdate(iph2)
	phase2_handle_t *iph2;
{
	struct saproto *pr;
	struct sockaddr_storage *src = NULL, *dst = NULL;
	u_int e_type, e_keylen, a_type, a_keylen, flags;
	u_int satype, mode;
	u_int64_t lifebyte = 0;
	u_int wsize = 4;  /* XXX static size of window */ 
	int proxy = 0;
	struct ph2natt natt;
    int authtype;

	/* sanity check */
	if (iph2->approval == NULL) {
		plog(ASL_LEVEL_ERR, 
			"No approved SAs found.\n");
	}

	if (iph2->side == INITIATOR)
		proxy = iph2->ph1->rmconf->support_proxy;
	else if (iph2->sainfo && iph2->sainfo->id_i)
		proxy = 1;

	/* for mobile IPv6 */
	if (proxy && iph2->src_id && iph2->dst_id &&
	    ipsecdoi_transportmode(iph2->approval)) {
		src = iph2->src_id;
		dst = iph2->dst_id;
	} else {
		src = iph2->src;
		dst = iph2->dst;
	}

	for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
		/* validity check */
		satype = ipsecdoi2pfkey_proto(pr->proto_id);
		if (satype == ~0) {
			plog(ASL_LEVEL_ERR, 
				"Invalid proto_id %d\n", pr->proto_id);
			return -1;
		}
		else if (satype == SADB_X_SATYPE_IPCOMP) {
			/* IPCOMP has no replay window */
			wsize = 0;
		}
#ifdef ENABLE_SAMODE_UNSPECIFIED
		mode = IPSEC_MODE_ANY;
#else
		mode = ipsecdoi2pfkey_mode(pr->encmode);
		if (mode == ~0) {
			plog(ASL_LEVEL_ERR, 
				"Invalid encmode %d\n", pr->encmode);
			return -1;
		}
#endif

		/* set algorithm type and key length */
		e_keylen = pr->head->encklen;
        authtype = pr->head->authtype;
        a_keylen = 0;
		if (pfkey_convertfromipsecdoi(
                iph2,
				pr->proto_id,
				pr->head->trns_id,
                authtype,
				&e_type, &e_keylen,
				&a_type, &a_keylen, &flags) < 0)
			return -1;

#if 0
		lifebyte = iph2->approval->lifebyte * 1024,
#else
		lifebyte = 0;
#endif

#ifdef ENABLE_NATT
		//plog(ASL_LEVEL_DEBUG, "call pfkey_send_update\n");
        plog(ASL_LEVEL_DEBUG, "call pfkey_send_update: e_type %d, e_klen %d, a_type %d, a_klen %d\n",
             e_type, e_keylen, a_type, a_keylen);
		if (pr->udp_encap) {
			memset (&natt, 0, sizeof (natt));
			natt.sport = extract_port (iph2->ph1->remote);
			flags |= SADB_X_EXT_NATT;
			if (iph2->ph1->rmconf->natt_multiple_user == TRUE &&
				mode == IPSEC_MODE_TRANSPORT &&
				src->ss_family == AF_INET) {
				flags |= SADB_X_EXT_NATT_MULTIPLEUSERS;
				if (iph2->ph1->natt_flags & NAT_DETECTED_PEER) {
					// is mutually exclusive with SADB_X_EXT_NATT_KEEPALIVE
					flags |= SADB_X_EXT_NATT_DETECTED_PEER;
				}
			} else if (iph2->ph1->natt_flags & NAT_DETECTED_ME) {
				if (iph2->ph1->rmconf->natt_keepalive == TRUE)
					flags |= SADB_X_EXT_NATT_KEEPALIVE;
			} else {
				if (iph2->ph1->natt_flags & NAT_DETECTED_PEER) {
					// is mutually exclusive with SADB_X_EXT_NATT_KEEPALIVE
					flags |= SADB_X_EXT_NATT_DETECTED_PEER;
				}
			}
		} else {
			memset (&natt, 0, sizeof (natt));
		}

		if (pfkey_send_update(
				lcconf->sock_pfkey,
				satype,
				mode,
				dst,
				src,
				pr->spi,
				pr->reqid_in,
				wsize,	
				pr->keymat->v,
				e_type, e_keylen, a_type, a_keylen, flags,
				0, lifebyte, iph2->approval->lifetime, 0,
				iph2->seq, natt.sport, 0) < 0) {
			plog(ASL_LEVEL_ERR, 
				"libipsec failed send update (%s)\n",
				ipsec_strerror());
			return -1;
		}
#else
		plog(ASL_LEVEL_DEBUG, "call pfkey_send_update\n");
		if (pfkey_send_update(
				lcconf->sock_pfkey,
				satype,
				mode,
				dst,
				src,
				pr->spi,
				pr->reqid_in,
				wsize,	
				pr->keymat->v,
				e_type, e_keylen, a_type, a_keylen, flags,
				0, lifebyte, iph2->approval->lifetime, 0,
				iph2->seq, 0, 0) < 0) {
			plog(ASL_LEVEL_ERR, 
				"libipsec failed send update (%s)\n",
				ipsec_strerror());
			return -1;
		}
#endif /* ENABLE_NATT */


	}

	return 0;
}

static int
pk_recvupdate(mhp)
	caddr_t *mhp;
{
	struct sadb_msg *msg;
	struct sadb_sa *sa;
	struct sockaddr_storage *src, *dst;
	phase2_handle_t *iph2;
	u_int proto_id, encmode, sa_mode;
	int incomplete = 0;
	struct saproto *pr;

	/* ignore this message because of local test mode. */
	if (f_local)
		return 0;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_SA] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb update message passed.\n");
		return -1;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];                 // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	src = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]);
	dst = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_DST]);
	sa = ALIGNED_CAST(struct sadb_sa *)mhp[SADB_EXT_SA];

	sa_mode = mhp[SADB_X_EXT_SA2] == NULL
		? IPSEC_MODE_ANY
		: (ALIGNED_CAST(struct sadb_x_sa2 *)mhp[SADB_X_EXT_SA2])->sadb_x_sa2_mode;

	/* the message has to be processed or not ? */
	if (msg->sadb_msg_pid != getpid()) {
		plog(ASL_LEVEL_DEBUG, 
			"%s message is not interesting "
			"because pid %d is not mine.\n",
			s_pfkey_type(msg->sadb_msg_type),
			msg->sadb_msg_pid);
		return -1;
	}

	iph2 = ike_session_getph2byseq(msg->sadb_msg_seq);
	if (iph2 == NULL) {
		plog(ASL_LEVEL_DEBUG, 
			"Seq %d of %s message not interesting.\n",
			msg->sadb_msg_seq,
			s_pfkey_type(msg->sadb_msg_type));
		return -1;
	}

	if (iph2->is_dying) {
		plog(ASL_LEVEL_ERR,
			 "Status mismatch Phase 2 dying (db:%d)\n",
			 iph2->status);
		return -1;
	}
	if (iph2->status != IKEV1_STATE_QUICK_I_ADDSA &&
        iph2->status != IKEV1_STATE_QUICK_R_ADDSA) {
		plog(ASL_LEVEL_ERR,
			"Status mismatch (db:%d)\n",
			iph2->status);
		return -1;
	}

	/* check to complete all keys ? */
	for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
		proto_id = pfkey2ipsecdoi_proto(msg->sadb_msg_satype);
		if (proto_id == ~0) {
			plog(ASL_LEVEL_ERR, 
				"invalid proto_id %d\n", msg->sadb_msg_satype);
			return -1;
		}
		encmode = pfkey2ipsecdoi_mode(sa_mode);
		if (encmode == ~0) {
			plog(ASL_LEVEL_ERR, 
				"invalid encmode %d\n", sa_mode);
			return -1;
		}

		if (pr->proto_id == proto_id
		 && pr->spi == sa->sadb_sa_spi) {
			pr->ok = 1;
			plog(ASL_LEVEL_DEBUG, 
				"pfkey UPDATE succeeded: %s\n",
				sadbsecas2str(iph2->dst, iph2->src,
				    msg->sadb_msg_satype,
				    sa->sadb_sa_spi,
				    sa_mode));

			plog(ASL_LEVEL_INFO, 
				"IPsec-SA established: %s\n",
				sadbsecas2str(iph2->dst, iph2->src,
					msg->sadb_msg_satype, sa->sadb_sa_spi,
					sa_mode));
		}

		if (pr->ok == 0)
			incomplete = 1;
	}

	if (incomplete)
		return 0;

	/* turn off the timer for calling pfkey_timeover() */
	SCHED_KILL(iph2->sce);
	
	/* update status */
	fsm_set_state(&iph2->status, IKEV1_STATE_PHASE2_ESTABLISHED);

	if (iph2->side == INITIATOR) {
		IPSECSESSIONTRACEREVENT(iph2->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_PH2_INIT_SUCC,
								CONSTSTR("Initiator, Quick-Mode"),
								CONSTSTR(NULL));
	} else {
		IPSECSESSIONTRACEREVENT(iph2->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_PH2_RESP_SUCC,
								CONSTSTR("Responder, Quick-Mode"),
								CONSTSTR(NULL));
	}

	ike_session_ph2_established(iph2);

	IPSECLOGASLMSG("IPSec Phase 2 established (Initiated by %s).\n",
				   (iph2->side == INITIATOR)? "me" : "peer");
	
#ifdef ENABLE_STATS
	gettimeofday(&iph2->end, NULL);
	plog(ASL_LEVEL_NOTICE, "%s(%s): %8.6f",
		"Phase 2", "quick", timedelta(&iph2->start, &iph2->end));
#endif

	/* count up */
	if (iph2->ph1)
		iph2->ph1->ph2cnt++;

	/* turn off schedule */
	if (iph2->scr)
		SCHED_KILL(iph2->scr);

	/*
	 * since we are going to reuse the phase2 handler, we need to
	 * remain it and refresh all the references between ph1 and ph2 to use.
	 */
	ike_session_unbindph12(iph2);   //%%%%% fix this

	iph2->sce = sched_new(iph2->approval->lifetime,
	    isakmp_ph2expire_stub, iph2);

	plog(ASL_LEVEL_DEBUG, "===\n");
	return 0;
}

/*
 * set outbound SA
 */
int
pk_sendadd(iph2)
	phase2_handle_t *iph2;
{
	struct saproto *pr;
	struct sockaddr_storage *src = NULL, *dst = NULL;
	u_int e_type, e_keylen, a_type, a_keylen, flags;
	u_int satype, mode;
	u_int64_t lifebyte = 0;
	u_int wsize = 4; /* XXX static size of window */ 
	int proxy = 0;
	struct ph2natt natt;
    int authtype;

	/* sanity check */
	if (iph2->approval == NULL) {
		plog(ASL_LEVEL_ERR, 
			"no approvaled SAs found.\n");
	}

	if (iph2->side == INITIATOR)
		proxy = iph2->ph1->rmconf->support_proxy;
	else if (iph2->sainfo && iph2->sainfo->id_i)
		proxy = 1;

	/* for mobile IPv6 */
	if (proxy && iph2->src_id && iph2->dst_id &&
	    ipsecdoi_transportmode(iph2->approval)) {
		src = iph2->src_id;
		dst = iph2->dst_id;
	} else {
		src = iph2->src;
		dst = iph2->dst;
	}

	for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
		/* validity check */
		satype = ipsecdoi2pfkey_proto(pr->proto_id);
		if (satype == ~0) {
			plog(ASL_LEVEL_ERR, 
				"invalid proto_id %d\n", pr->proto_id);
			return -1;
		}
		else if (satype == SADB_X_SATYPE_IPCOMP) {
			/* no replay window for IPCOMP */
			wsize = 0;
		}
#ifdef ENABLE_SAMODE_UNSPECIFIED
		mode = IPSEC_MODE_ANY;
#else
		mode = ipsecdoi2pfkey_mode(pr->encmode);
		if (mode == ~0) {
			plog(ASL_LEVEL_ERR, 
				"invalid encmode %d\n", pr->encmode);
			return -1;
		}
#endif

		/* set algorithm type and key length */
		e_keylen = pr->head->encklen;
        authtype = pr->head->authtype;
        a_keylen = 0;
		if (pfkey_convertfromipsecdoi(
                iph2,
				pr->proto_id,
				pr->head->trns_id,
                authtype,
				&e_type, &e_keylen,
				&a_type, &a_keylen, &flags) < 0)
			return -1;

#if 0
		lifebyte = iph2->approval->lifebyte * 1024,
#else
		lifebyte = 0;
#endif

#ifdef ENABLE_NATT
		//plog(ASL_LEVEL_DEBUG, "call pfkey_send_add\n");
        plog(ASL_LEVEL_DEBUG, "call pfkey_send_add: e_type %d, e_klen %d, a_type %d, a_klen %d\n",
             e_type, e_keylen, a_type, a_keylen);

		if (pr->udp_encap) {
			memset (&natt, 0, sizeof (natt));
			natt.dport = extract_port (iph2->ph1->remote);
			flags |= SADB_X_EXT_NATT;
			if (iph2->ph1->rmconf->natt_multiple_user == TRUE &&
				mode == IPSEC_MODE_TRANSPORT &&
				src->ss_family == AF_INET) {
				flags |= SADB_X_EXT_NATT_MULTIPLEUSERS;
				if (iph2->ph1->natt_flags & NAT_DETECTED_PEER) {
					// is mutually exclusive with SADB_X_EXT_NATT_KEEPALIVE
					flags |= SADB_X_EXT_NATT_DETECTED_PEER;
				}
			} else if (iph2->ph1->natt_flags & NAT_DETECTED_ME) {
				if (iph2->ph1->rmconf->natt_keepalive == TRUE)
					flags |= SADB_X_EXT_NATT_KEEPALIVE;
			} else {
				if (iph2->ph1->natt_flags & NAT_DETECTED_PEER) {
					// is mutually exclusive with SADB_X_EXT_NATT_KEEPALIVE
					flags |= SADB_X_EXT_NATT_DETECTED_PEER;
				}
			}		
		} else {
			memset (&natt, 0, sizeof (natt));

			/* Remove port information, that SA doesn't use it */
			//set_port(src, 0);
			//set_port(dst, 0);
		}

		if (pfkey_send_add(
				lcconf->sock_pfkey,
				satype,
				mode,
				src,
				dst,
				pr->spi_p,
				pr->reqid_out,
				wsize,	
				pr->keymat_p->v,
				e_type, e_keylen, a_type, a_keylen, flags,
				0, lifebyte, iph2->approval->lifetime, 0,
				iph2->seq,natt.dport, 0) < 0) {
			plog(ASL_LEVEL_ERR, 
				"libipsec failed send add (%s)\n",
				ipsec_strerror());
			return -1;
		}
#else
		plog(ASL_LEVEL_DEBUG, "call pfkey_send_add\n");

		/* Remove port information, it is not used without NAT-T */
		//set_port(src, 0);
		//set_port(dst, 0);

		if (pfkey_send_add(
				lcconf->sock_pfkey,
				satype,
				mode,
				src,
				dst,
				pr->spi_p,
				pr->reqid_out,
				wsize,
				pr->keymat_p->v,
				e_type, e_keylen, a_type, a_keylen, flags,
				0, lifebyte, iph2->approval->lifetime, 0,
				iph2->seq, 0, 0) < 0) {
			plog(ASL_LEVEL_ERR, 
				"libipsec failed send add (%s)\n",
				ipsec_strerror());
			return -1;
		}
#endif /* ENABLE_NATT */
	}

	return 0;
}

static int
pk_recvadd(mhp)
	caddr_t *mhp;
{
	struct sadb_msg *msg;
	struct sadb_sa *sa;
	struct sockaddr_storage *src, *dst;
	phase2_handle_t *iph2;
	u_int sa_mode;

	/* ignore this message because of local test mode. */
	if (f_local)
		return 0;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_SA] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb add message passed.\n");
		return -1;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];                     // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	src = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]);
	dst = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_DST]);
	sa = ALIGNED_CAST(struct sadb_sa *)mhp[SADB_EXT_SA];

	sa_mode = mhp[SADB_X_EXT_SA2] == NULL
		? IPSEC_MODE_ANY
		: (ALIGNED_CAST(struct sadb_x_sa2 *)mhp[SADB_X_EXT_SA2])->sadb_x_sa2_mode;

	/* the message has to be processed or not ? */
	if (msg->sadb_msg_pid != getpid()) {
		plog(ASL_LEVEL_DEBUG, 
			"%s message is not interesting "
			"because pid %d is not mine.\n",
			s_pfkey_type(msg->sadb_msg_type),
			msg->sadb_msg_pid);
		return -1;
	}

	iph2 = ike_session_getph2byseq(msg->sadb_msg_seq);
	if (iph2 == NULL) {
		plog(ASL_LEVEL_DEBUG, 
			"seq %d of %s message not interesting.\n",
			msg->sadb_msg_seq,
			s_pfkey_type(msg->sadb_msg_type));
		return -1;
	}
	/*
	 * NOTE don't update any status of phase2 handle
	 * because they must be updated by SADB_UPDATE message
	 */

	plog(ASL_LEVEL_INFO, 
		"IPsec-SA established: %s\n",
		sadbsecas2str(iph2->src, iph2->dst,
			msg->sadb_msg_satype, sa->sadb_sa_spi, sa_mode));
			
	ike_session_cleanup_other_established_ph2s(iph2->parent_session, iph2);
	
#ifdef ENABLE_VPNCONTROL_PORT
		{
			u_int32_t address;
			
			if (iph2->dst->ss_family == AF_INET)
				address = ((struct sockaddr_in *)iph2->dst)->sin_addr.s_addr;
			else
				address = 0;
			vpncontrol_notify_phase_change(0, FROM_LOCAL, NULL, iph2);
		}	
#endif

	plog(ASL_LEVEL_DEBUG, "===\n");
	return 0;
}

static int
pk_recvexpire(mhp)
	caddr_t *mhp;
{
	struct sadb_msg *msg;
	struct sadb_sa *sa;
	struct sockaddr_storage *src, *dst;
	phase2_handle_t *iph2;
	u_int proto_id, sa_mode;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_SA] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL
	 || (mhp[SADB_EXT_LIFETIME_HARD] != NULL
	  && mhp[SADB_EXT_LIFETIME_SOFT] != NULL)) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb expire message passed.\n");
		return -1;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];                 // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	sa = ALIGNED_CAST(struct sadb_sa *)mhp[SADB_EXT_SA];
	src = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]);
	dst = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_DST]);

	sa_mode = mhp[SADB_X_EXT_SA2] == NULL
		? IPSEC_MODE_ANY
		: (ALIGNED_CAST(struct sadb_x_sa2 *)mhp[SADB_X_EXT_SA2])->sadb_x_sa2_mode;

	proto_id = pfkey2ipsecdoi_proto(msg->sadb_msg_satype);
	if (proto_id == ~0) {
		plog(ASL_LEVEL_ERR, 
			"invalid proto_id %d\n", msg->sadb_msg_satype);
		return -1;
	}

	plog(ASL_LEVEL_INFO, 
		"IPsec-SA expired: %s\n",
		sadbsecas2str(src, dst,
			msg->sadb_msg_satype, sa->sadb_sa_spi, sa_mode));

	iph2 = ike_session_getph2bysaidx(src, dst, proto_id, sa->sadb_sa_spi);
	if (iph2 == NULL) {
		/*
		 * Ignore it because two expire messages are come up.
		 * phase2 handler has been deleted already when 2nd message
		 * is received.
		 */
		plog(ASL_LEVEL_DEBUG, 
			"no such a SA found: %s\n",
			sadbsecas2str(src, dst,
			    msg->sadb_msg_satype, sa->sadb_sa_spi,
			    sa_mode));
		return 0;
	}
	if (iph2->is_dying || !FSM_STATE_IS_ESTABLISHED(iph2->status)) {
		/*
		 * If the status is not equal to PHASE2ST_ESTABLISHED,
		 * racoon ignores this expire message.  There are two reason.
		 * One is that the phase 2 probably starts because there is
		 * a potential that racoon receives the acquire message
		 * without receiving a expire message.  Another is that racoon
		 * may receive the multiple expire messages from the kernel.
		 */
		plog(ASL_LEVEL_WARNING,
             "The expire message is received but the handler %s (status = 0x%x).\n",
             iph2->is_dying ? "is dying" : "has not been established", iph2->status);
		return 0;
	}

	/* turn off the timer for calling isakmp_ph2expire() */ 
	SCHED_KILL(iph2->sce);
	
	fsm_set_state(&iph2->status, IKEV1_STATE_PHASE2_EXPIRED);
	
	/* INITIATOR, begin phase 2 exchange only if there's no other established ph2. */
	/* allocate buffer for status management of pfkey message */
	if (iph2->side == INITIATOR &&
		!ike_session_has_other_established_ph2(iph2->parent_session, iph2) &&
		!ike_session_drop_rekey(iph2->parent_session, IKE_SESSION_REKEY_TYPE_PH2)) {

		ike_session_initph2(iph2);

		/* start isakmp initiation by using ident exchange */
		if (isakmp_post_acquire(iph2) < 0) {
			plog(ASL_LEVEL_ERR,
				"failed to begin ipsec sa "
				"re-negotiation.\n");
			ike_session_unlink_phase2(iph2);
			return -1;
		}

		return 0;
		/*NOTREACHED*/
	}


	/* If not received SADB_EXPIRE, INITIATOR delete ph2handle. */
	/* RESPONDER always delete ph2handle, keep silent.  RESPONDER doesn't
	 * manage IPsec SA, so delete the list */
	ike_session_unlink_phase2(iph2);

	return 0;
}

static int
pk_recvacquire(mhp)
	caddr_t *mhp;
{
	struct sadb_msg *msg;
	struct sadb_x_policy *xpl;
	struct secpolicy *sp_out = NULL, *sp_in = NULL;
	phase2_handle_t *iph2;
	struct sockaddr_storage *src, *dst;
    ike_session_t *session = NULL;
    struct remoteconf *rmconf;

	/* ignore this message because of local test mode. */
	if (f_local)
		return 0;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL
	 || mhp[SADB_X_EXT_POLICY] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb acquire message passed.\n");
		return -1;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];                         // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	xpl = ALIGNED_CAST(struct sadb_x_policy *)mhp[SADB_X_EXT_POLICY];
	src = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]);
	dst = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_DST]);

	/* ignore if type is not IPSEC_POLICY_IPSEC */
	if (xpl->sadb_x_policy_type != IPSEC_POLICY_IPSEC) {
		plog(ASL_LEVEL_DEBUG, 
			"ignore ACQUIRE message. type is not IPsec.\n");
		return 0;
	}

	/* ignore it if src is multicast address */
    {
	struct sockaddr_storage *sa = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_DST]);

	if ((sa->ss_family == AF_INET
	  && IN_MULTICAST(ntohl(((struct sockaddr_in *)sa)->sin_addr.s_addr)))
#ifdef INET6
	 || (sa->ss_family == AF_INET6
	  && IN6_IS_ADDR_MULTICAST(&((struct sockaddr_in6 *)sa)->sin6_addr))
#endif
	) {
		plog(ASL_LEVEL_DEBUG, 
			"ignore due to multicast address: %s.\n",
			saddrwop2str((struct sockaddr *)sa));
		return 0;
	}
    }
   	
    	/* ignore, if we do not listen on source address */
	{
		/* reasons behind:
		 * - if we'll contact peer from address we do not listen -
		 *   we will be unable to complete negotiation;
		 * - if we'll negotiate using address we're listening -
		 *   remote peer will send packets to address different
		 *   than one in the policy, so kernel will drop them;
		 * => therefore this acquire is not for us! --Aidas
		 */
                                                                    // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
		struct sockaddr_storage *sa = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]);    
		struct myaddrs *p;
		int do_listen = 0;
        char * str;
		for (p = lcconf->myaddrs; p; p = p->next) {
            str = saddr2str((struct sockaddr *)p->addr);
            plog(ASL_LEVEL_DEBUG, 
                 "checking listen addrs: %s", str);
            
			if (!cmpsaddrwop(p->addr, sa)) {
				do_listen = 1;
				break;
			}
		}

		if (!do_listen) {
			plog(ASL_LEVEL_DEBUG, 
				"ignore because do not listen on source address : %s.\n",
				saddrwop2str((struct sockaddr *)sa));
			return 0;
		}
	}

	/*
	 * If there is a phase 2 handler against the policy identifier in
	 * the acquire message, and if
	 *    1. its state is less than PHASE2ST_ESTABLISHED, then racoon
	 *       should ignore such a acquire message because the phase 2
	 *       is just negotiating.
	 *    2. its state is equal to PHASE2ST_ESTABLISHED, then racoon
	 *       has to process such a acquire message because racoon may
	 *       have lost the expire message.
	 */
	iph2 = ike_session_getph2byid(src, dst, xpl->sadb_x_policy_id);
	if (iph2 != NULL) {
        session = iph2->parent_session;
		if (!FSM_STATE_IS_ESTABLISHED(iph2->status)) {
			plog(ASL_LEVEL_DEBUG,
				"ignore the acquire because ph2 found\n");
			return -1;
		}
		if (FSM_STATE_IS_EXPIRED(iph2->status))
			iph2 = NULL;
		/*FALLTHROUGH*/
	}

	/* search for proper policyindex */
	sp_out = getspbyspid(xpl->sadb_x_policy_id);
	if (sp_out == NULL) {
		plog(ASL_LEVEL_ERR, "no policy found: id:%d.\n",
			xpl->sadb_x_policy_id);
		return -1;
	}
	plog(ASL_LEVEL_DEBUG, 
		"suitable outbound SP found: %s.\n", spidx2str(&sp_out->spidx));

	/* get inbound policy */
    {
        struct policyindex spidx;

        spidx.dir = IPSEC_DIR_INBOUND;
        memcpy(&spidx.src, &sp_out->spidx.dst, sizeof(spidx.src));
        memcpy(&spidx.dst, &sp_out->spidx.src, sizeof(spidx.dst));
        spidx.prefs = sp_out->spidx.prefd;
        spidx.prefd = sp_out->spidx.prefs;
        spidx.ul_proto = sp_out->spidx.ul_proto;

        sp_in = getsp(&spidx);
        if (sp_in) {
            plog(ASL_LEVEL_DEBUG,
                "Suitable inbound SP found: %s.\n",
                spidx2str(&sp_in->spidx));
        } else {
            plog(ASL_LEVEL_NOTICE,
                "No in-bound policy found: %s\n",
                spidx2str(&spidx));
        }
    }
    
	/* allocate a phase 2 */
    rmconf = getrmconf(dst);
	if (rmconf == NULL) {
		plog(ASL_LEVEL_ERR, "No configuration found for %s.\n",
             saddrwop2str((struct sockaddr *)dst));
		return -1;
	}

	iph2 = ike_session_newph2(rmconf->ike_version, PHASE2_TYPE_SA);
	if (iph2 == NULL) {
		plog(ASL_LEVEL_ERR,
			"Failed to allocate Phase 2 entry.\n");
		return -1;
	}
	plog(ASL_LEVEL_DEBUG, "Got new Phase 2 version %d\n", iph2->version);
    iph2->version = rmconf->ike_version;
	iph2->side = INITIATOR;
	iph2->spid = xpl->sadb_x_policy_id;

	iph2->satype = msg->sadb_msg_satype;
	iph2->seq = msg->sadb_msg_seq;
	/* set end addresses of SA */
                                                // Wcast_align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	iph2->src = dupsaddr(ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]));
	if (iph2->src == NULL) {
		ike_session_delph2(iph2);
		return -1;
	}
    iph2->dst = dupsaddr(ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_DST]));
	if (iph2->dst == NULL) {
        ike_session_delph2(iph2);
		return -1;
    }
    
	if (iph2->version == ISAKMP_VERSION_NUMBER_IKEV1) {
		fsm_set_state(&iph2->status, IKEV1_STATE_QUICK_I_START);
	}

	plog(ASL_LEVEL_DEBUG,
		"new acquire %s\n", spidx2str(&sp_out->spidx));

	/* get sainfo */
    {
        vchar_t *idsrc, *iddst;

        idsrc = ipsecdoi_sockaddr2id(&sp_out->spidx.src,
                    sp_out->spidx.prefs, sp_out->spidx.ul_proto);
        if (idsrc == NULL) {
            plog(ASL_LEVEL_ERR, 
                "failed to get ID for %s\n",
                spidx2str(&sp_out->spidx));
            ike_session_delph2(iph2);
            return -1;
        }
        iddst = ipsecdoi_sockaddr2id(&sp_out->spidx.dst,
                    sp_out->spidx.prefd, sp_out->spidx.ul_proto);
        if (iddst == NULL) {
            plog(ASL_LEVEL_ERR, 
                "failed to get ID for %s\n",
                spidx2str(&sp_out->spidx));
            vfree(idsrc);
            ike_session_delph2(iph2);
            return -1;
        }
        iph2->sainfo = getsainfo(idsrc, iddst, NULL, 0);
        vfree(idsrc);
        vfree(iddst);
        if (iph2->sainfo == NULL) {
            plog(ASL_LEVEL_ERR,
                "failed to get sainfo.\n");
            ike_session_delph2(iph2);
            return -1;
            /* XXX should use the algorithm list from register message */
        }
    }
    retain_sainfo(iph2->sainfo);

	if (set_proposal_from_policy(iph2, sp_out, sp_in) < 0) {
		plog(ASL_LEVEL_ERR,
			 "failed to create saprop.\n");
			ike_session_delph2(iph2);
		return -1;
	}

    if (session == NULL)
        session = ike_session_get_session(iph2->src, iph2->dst, 1, NULL);
    if (session == NULL)
        fatal_error(-1);

    if (ike_session_link_phase2(session, iph2))
        fatal_error(-1);    //????? fix ???

	/* start isakmp initiation by using ident exchange */
	/* XXX should be looped if there are multiple phase 2 handler. */
	if (isakmp_post_acquire(iph2) < 0) {
		plog(ASL_LEVEL_ERR,
			"failed to begin ipsec sa negotiation.\n");
		goto err;
	}
	
#if !TARGET_OS_EMBEDDED
	if ( lcconf->vt == NULL){
		if (!(lcconf->vt = vproc_transaction_begin(NULL)))
			plog(ASL_LEVEL_ERR, 
			 	"vproc_transaction_begin returns NULL.\n");
	}
#endif				

	
	return 0;

err:
    ike_session_unlink_phase2(iph2);
	return -1;
}

static int
pk_recvdelete(mhp)
	caddr_t *mhp;
{
	struct sadb_msg *msg;
	struct sadb_sa *sa;
	struct sockaddr_storage *src, *dst;
	phase2_handle_t *iph2 = NULL;
	u_int proto_id;

	/* ignore this message because of local test mode. */
	if (f_local)
		return 0;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb delete message passed.\n");
		return -1;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];                 // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	sa = ALIGNED_CAST(struct sadb_sa *)mhp[SADB_EXT_SA];
	src = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_SRC]);
	dst = ALIGNED_CAST(struct sockaddr_storage *)PFKEY_ADDR_SADDR(mhp[SADB_EXT_ADDRESS_DST]);

	/* the message has to be processed or not ? */
	if (msg->sadb_msg_pid == getpid()) {
		plog(ASL_LEVEL_DEBUG, 
			"%s message is not interesting "
			"because the message was originated by me.\n",
			s_pfkey_type(msg->sadb_msg_type));
		return -1;
	}

	proto_id = pfkey2ipsecdoi_proto(msg->sadb_msg_satype);
	if (proto_id == ~0) {
		plog(ASL_LEVEL_ERR, 
			"invalid proto_id %d\n", msg->sadb_msg_satype);
		return -1;
	}

    plog(ASL_LEVEL_DEBUG, "SADB delete message: proto-id %d\n", proto_id);
    plog(ASL_LEVEL_DEBUG, "src: %s\n", saddr2str((struct sockaddr *)src));
    plog(ASL_LEVEL_DEBUG, "dst: %s\n", saddr2str((struct sockaddr *)dst));
    
    if (!sa) {
        ike_session_deleteallph2(src, dst, proto_id);
        ike_session_deleteallph1(src, dst);
        return 0;
    }

	iph2 = ike_session_getph2bysaidx(src, dst, proto_id, sa->sadb_sa_spi);
	if (iph2 == NULL) {
		/* ignore */
		plog(ASL_LEVEL_ERR, 
			"no iph2 found: %s\n",
			sadbsecas2str(src, dst, msg->sadb_msg_satype,
				sa->sadb_sa_spi, IPSEC_MODE_ANY));
		return 0;
	}

	plog(ASL_LEVEL_ERR, 
		"pfkey DELETE received: %s\n",
		sadbsecas2str(iph2->src, iph2->dst,
			msg->sadb_msg_satype, sa->sadb_sa_spi, IPSEC_MODE_ANY));

	/* send delete information */
    
    /* TODO: Look into handling this properly. Currently, if we get here, we can end up sending delete messages to the server for their own SAs, which is rejected. */
	/*if (FSM_STATE_IS_ESTABLISHED(iph2->status))
		isakmp_info_send_d2(iph2);

	ike_session_cleanup_ph1s_by_ph2(iph2);
	ike_session_unlink_phase2(iph2);*/

	return 0;
}

static int
pk_recvflush(mhp)
	caddr_t *mhp;
{
	/* sanity check */
	if (mhp[0] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb flush message passed.\n");
		return -1;
	}

	ike_session_flush_all_phase2(false);
	ike_session_flush_all_phase1(false);

	return 0;
}

static int
getsadbpolicy(policy0, policylen0, type, iph2)
	caddr_t *policy0;
	int *policylen0, type;
	phase2_handle_t *iph2;
{
	struct policyindex *spidx = iph2->spidx_gen;
	struct sadb_x_policy *xpl;
	struct sadb_x_ipsecrequest *xisr;
	struct saproto *pr;
	caddr_t policy, p;
	int policylen;
	int xisrlen;
	u_int satype, mode;

	/* get policy buffer size */
	policylen = sizeof(struct sadb_x_policy);
	if (type != SADB_X_SPDDELETE) {
		for (pr = iph2->approval->head; pr; pr = pr->next) {
			xisrlen = sizeof(*xisr);
			if (pr->encmode == IPSECDOI_ATTR_ENC_MODE_TUNNEL) {
				xisrlen += (sysdep_sa_len((struct sockaddr *)iph2->src)
				          + sysdep_sa_len((struct sockaddr *)iph2->dst));
			}

			policylen += PFKEY_ALIGN8(xisrlen);
		}
	}

	/* make policy structure */
	policy = racoon_malloc(policylen);
	if (!policy) {
		plog(ASL_LEVEL_ERR, 
			"buffer allocation failed.\n");
		return -1;
	}

	xpl = ALIGNED_CAST(struct sadb_x_policy *)policy;
	xpl->sadb_x_policy_len = PFKEY_UNIT64(policylen);
	xpl->sadb_x_policy_exttype = SADB_X_EXT_POLICY;
	xpl->sadb_x_policy_type = IPSEC_POLICY_IPSEC;
	xpl->sadb_x_policy_dir = spidx->dir;
	xpl->sadb_x_policy_id = 0;
#ifdef HAVE_PFKEY_POLICY_PRIORITY
	xpl->sadb_x_policy_priority = PRIORITY_DEFAULT;
#endif

	/* no need to append policy information any more if type is SPDDELETE */
	if (type == SADB_X_SPDDELETE)
		goto end;

	xisr = (struct sadb_x_ipsecrequest *)(xpl + 1);

	for (pr = iph2->approval->head; pr; pr = pr->next) {

		satype = doi2ipproto(pr->proto_id);
		if (satype == ~0) {
			plog(ASL_LEVEL_ERR, 
				"invalid proto_id %d\n", pr->proto_id);
			goto err;
		}
		mode = ipsecdoi2pfkey_mode(pr->encmode);
		if (mode == ~0) {
			plog(ASL_LEVEL_ERR, 
				"invalid encmode %d\n", pr->encmode);
			goto err;
		}

		/* 
		 * the policy level cannot be unique because the policy
		 * is defined later than SA, so req_id cannot be bound to SA.
		 */
		xisr->sadb_x_ipsecrequest_proto = satype;
		xisr->sadb_x_ipsecrequest_mode = mode;
		xisr->sadb_x_ipsecrequest_level = IPSEC_LEVEL_REQUIRE;
		xisr->sadb_x_ipsecrequest_reqid = 0;
		p = (caddr_t)(xisr + 1);

		xisrlen = sizeof(*xisr);

		if (pr->encmode == IPSECDOI_ATTR_ENC_MODE_TUNNEL) {
			int src_len, dst_len;

			src_len = sysdep_sa_len((struct sockaddr *)iph2->src);
			dst_len = sysdep_sa_len((struct sockaddr *)iph2->dst);
			xisrlen += src_len + dst_len;

			memcpy(p, iph2->src, src_len);
			p += src_len;

			memcpy(p, iph2->dst, dst_len);
			p += dst_len;
		}

		xisr->sadb_x_ipsecrequest_len = PFKEY_ALIGN8(xisrlen);
	}

end:
	*policy0 = policy;
	*policylen0 = policylen;

	return 0;

err:
	if (policy)
		racoon_free(policy);

	return -1;
}

int
pk_sendspdupdate2(iph2)
	phase2_handle_t *iph2;
{
	struct policyindex *spidx = iph2->spidx_gen;
	caddr_t policy = NULL;
	int policylen = 0;
	u_int64_t ltime, vtime;

	ltime = iph2->approval->lifetime;
	vtime = 0;

	if (getsadbpolicy(&policy, &policylen, SADB_X_SPDUPDATE, iph2)) {
		plog(ASL_LEVEL_ERR, 
			"getting sadb policy failed.\n");
		return -1;
	}

	if (pfkey_send_spdupdate2(
			lcconf->sock_pfkey,
			&spidx->src,
			spidx->prefs,
			&spidx->dst,
			spidx->prefd,
			spidx->ul_proto,
			ltime, vtime,
			policy, policylen, 0) < 0) {
		plog(ASL_LEVEL_ERR, 
			"libipsec failed send spdupdate2 (%s)\n",
			ipsec_strerror());
		goto end;
	}
	plog(ASL_LEVEL_DEBUG, "call pfkey_send_spdupdate2\n");

end:
	if (policy)
		racoon_free(policy);

	return 0;
}

static int
pk_recvspdupdate(mhp)
	caddr_t *mhp;
{
	struct sadb_address *saddr, *daddr;
	struct sadb_x_policy *xpl;
	struct policyindex spidx;
	struct secpolicy *sp;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL
	 || mhp[SADB_X_EXT_POLICY] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spdupdate message passed.\n");
		return -1;
	}
	saddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];        // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	daddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	xpl = ALIGNED_CAST(struct sadb_x_policy *)mhp[SADB_X_EXT_POLICY];

#ifdef HAVE_PFKEY_POLICY_PRIORITY
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			xpl->sadb_x_policy_priority,
			&spidx);
#else
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			&spidx);
#endif

	sp = getsp(&spidx);
	if (sp == NULL) {
		plog(ASL_LEVEL_ERR, 
			"such policy does not already exist: \"%s\"\n",
			spidx2str(&spidx));
	} else {
		remsp(sp);
		delsp(sp);
	}

	if (addnewsp(mhp) < 0)
		return -1;

	return 0;
}

/*
 * this function has to be used by responder side.
 */
int
pk_sendspdadd2(iph2)
	phase2_handle_t *iph2;
{
	struct policyindex *spidx = iph2->spidx_gen;
	caddr_t policy = NULL;
	int policylen = 0;
	u_int64_t ltime, vtime;

	ltime = iph2->approval->lifetime;
	vtime = 0;

	if (getsadbpolicy(&policy, &policylen, SADB_X_SPDADD, iph2)) {
		plog(ASL_LEVEL_ERR, 
			"getting sadb policy failed.\n");
		return -1;
	}

	if (pfkey_send_spdadd2(
			lcconf->sock_pfkey,
			&spidx->src,
			spidx->prefs,
			&spidx->dst,
			spidx->prefd,
			spidx->ul_proto,
			ltime, vtime,
			policy, policylen, 0) < 0) {
		plog(ASL_LEVEL_ERR, 
			"libipsec failed send spdadd2 (%s)\n",
			ipsec_strerror());
		goto end;
	}
	plog(ASL_LEVEL_DEBUG, "call pfkey_send_spdadd2\n");

end:
	if (policy)
		racoon_free(policy);

	return 0;
}

static int
pk_recvspdadd(mhp)
	caddr_t *mhp;
{
	struct sadb_address *saddr, *daddr;
	struct sadb_x_policy *xpl;
	struct policyindex spidx;
	struct secpolicy *sp;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL
	 || mhp[SADB_X_EXT_POLICY] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spdadd message passed.\n");
		return -1;
	}
	saddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];    // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	daddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	xpl = ALIGNED_CAST(struct sadb_x_policy *)mhp[SADB_X_EXT_POLICY];

#ifdef HAVE_PFKEY_POLICY_PRIORITY
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			xpl->sadb_x_policy_priority,
			&spidx);
#else
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			&spidx);
#endif

	sp = getsp(&spidx);
	if (sp != NULL) {
		plog(ASL_LEVEL_ERR, 
			"such policy already exists. "
			"anyway replace it: %s\n",
			spidx2str(&spidx));
		remsp(sp);
		delsp(sp);
	}

	if (addnewsp(mhp) < 0)
		return -1;

	return 0;
}

/*
 * this function has to be used by responder side.
 */
int
pk_sendspddelete(iph2)
	phase2_handle_t *iph2;
{
	struct policyindex *spidx = iph2->spidx_gen;
	caddr_t policy = NULL;
	int policylen;

	if (getsadbpolicy(&policy, &policylen, SADB_X_SPDDELETE, iph2)) {
		plog(ASL_LEVEL_ERR, 
			"getting sadb policy failed.\n");
		return -1;
	}

	if (pfkey_send_spddelete(
			lcconf->sock_pfkey,
			&spidx->src,
			spidx->prefs,
			&spidx->dst,
			spidx->prefd,
			spidx->ul_proto,
			policy, policylen, 0) < 0) {
		plog(ASL_LEVEL_ERR, 
			"libipsec failed send spddelete (%s)\n",
			ipsec_strerror());
		goto end;
	}
	plog(ASL_LEVEL_DEBUG, "call pfkey_send_spddelete\n");

end:
	if (policy)
		racoon_free(policy);

	return 0;
}

static int
pk_recvspddelete(mhp)
	caddr_t *mhp;
{
	struct sadb_address *saddr, *daddr;
	struct sadb_x_policy *xpl;
	struct policyindex spidx;
	struct secpolicy *sp;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL
	 || mhp[SADB_X_EXT_POLICY] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spddelete message passed.\n");
		return -1;
	}
	saddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];    // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	daddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	xpl = ALIGNED_CAST(struct sadb_x_policy *)mhp[SADB_X_EXT_POLICY];

#ifdef HAVE_PFKEY_POLICY_PRIORITY
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			xpl->sadb_x_policy_priority,
			&spidx);
#else
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			&spidx);
#endif

	sp = getsp(&spidx);
	if (sp == NULL) {
		plog(ASL_LEVEL_ERR, 
			"no policy found: %s\n",
			spidx2str(&spidx));
		return -1;
	}

    ike_session_purgephXbyspid(xpl->sadb_x_policy_id, true);

	remsp(sp);
	delsp(sp);

	return 0;
}

static int
pk_recvspdexpire(mhp)
	caddr_t *mhp;
{
	struct sadb_address *saddr, *daddr;
	struct sadb_x_policy *xpl;
	struct policyindex spidx;
	struct secpolicy *sp;

	/* sanity check */
	if (mhp[0] == NULL
	 || mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL
	 || mhp[SADB_X_EXT_POLICY] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spdexpire message passed.\n");
		return -1;
	}
	saddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];    // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	daddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	xpl = ALIGNED_CAST(struct sadb_x_policy *)mhp[SADB_X_EXT_POLICY];

#ifdef HAVE_PFKEY_POLICY_PRIORITY
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			xpl->sadb_x_policy_priority,
			&spidx);
#else
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			&spidx);
#endif

	sp = getsp(&spidx);
	if (sp == NULL) {
		plog(ASL_LEVEL_ERR, 
			"no policy found: %s\n",
			spidx2str(&spidx));
		return -1;
	}

    ike_session_purgephXbyspid(xpl->sadb_x_policy_id, false);

	remsp(sp);
	delsp(sp);

	return 0;
}

static int
pk_recvspdget(mhp)
	caddr_t *mhp;
{
	/* sanity check */
	if (mhp[0] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spdget message passed.\n");
		return -1;
	}

	return 0;
}

static int
pk_recvspddump(mhp)
	caddr_t *mhp;
{
	struct sadb_msg *msg;
	struct sadb_address *saddr, *daddr;
	struct sadb_x_policy *xpl;
	struct policyindex spidx;
	struct secpolicy *sp;

	/* sanity check */
	if (mhp[0] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spddump message passed.\n");
		return -1;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];         // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer

	saddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];
	daddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	xpl = ALIGNED_CAST(struct sadb_x_policy *)mhp[SADB_X_EXT_POLICY];

	if (saddr == NULL || daddr == NULL || xpl == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spddump message passed.\n");
		return -1;
	}

#ifdef HAVE_PFKEY_POLICY_PRIORITY
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			xpl->sadb_x_policy_priority,
			&spidx);
#else
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			&spidx);
#endif

	sp = getsp(&spidx);
	if (sp != NULL) {
		plog(ASL_LEVEL_ERR, 
			"such policy already exists. "
			"anyway replace it: %s\n",
			spidx2str(&spidx));
		remsp(sp);
		delsp(sp);
	}

	if (addnewsp(mhp) < 0)
		return -1;

	return 0;
}

static int
pk_recvspdflush(mhp)
	caddr_t *mhp;
{
	/* sanity check */
	if (mhp[0] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spdflush message passed.\n");
		return -1;
	}

    ike_session_flush_all_phase2(false);
    ike_session_flush_all_phase1(false);
	flushsp();

	return 0;
}

/*
 * send error against acquire message to kenrel.
 */
int
pk_sendeacquire(iph2)
	phase2_handle_t *iph2;
{
	struct sadb_msg *newmsg;
	int len;

	len = sizeof(struct sadb_msg);
	newmsg = racoon_calloc(1, len);
	if (newmsg == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get buffer to send acquire.\n");
		return -1;
	}

	memset(newmsg, 0, len);
	newmsg->sadb_msg_version = PF_KEY_V2;
	newmsg->sadb_msg_type = SADB_ACQUIRE;
	newmsg->sadb_msg_errno = ENOENT;	/* XXX */
	newmsg->sadb_msg_satype = iph2->satype;
	newmsg->sadb_msg_len = PFKEY_UNIT64(len);
	newmsg->sadb_msg_reserved = 0;
	newmsg->sadb_msg_seq = iph2->seq;
	newmsg->sadb_msg_pid = (u_int32_t)getpid();

	/* send message */
	len = pfkey_send(lcconf->sock_pfkey, newmsg, len);

	racoon_free(newmsg);

	return 0;
}

int
pk_sendget_inbound_sastats(ike_session_t *session)
{
    u_int32_t max_stats;
    u_int32_t seq;

    if (!session) {
        plog(ASL_LEVEL_DEBUG, "invalid args in %s \n", __FUNCTION__);
        return -1;
    }

    session->traffic_monitor.num_in_curr_req = 0;
    bzero(session->traffic_monitor.in_curr_req, sizeof(session->traffic_monitor.in_curr_req));
    max_stats = (sizeof(session->traffic_monitor.in_curr_req) / sizeof(session->traffic_monitor.in_curr_req[0]));

    // get list of SAs
    if ((session->traffic_monitor.num_in_curr_req = ike_session_get_sas_for_stats(session,
                                                                                  IPSEC_DIR_INBOUND,
                                                                                  &seq,
                                                                                  session->traffic_monitor.in_curr_req,
                                                                                  max_stats))) {
        u_int64_t session_ids[] = {(u_int64_t)session, 0};

        //plog(ASL_LEVEL_DEBUG, "about to call %s\n", __FUNCTION__);

        if (pfkey_send_getsastats(lcconf->sock_pfkey,
                                  seq,
                                  session_ids,
                                  1,
                                  IPSEC_DIR_INBOUND,
                                  session->traffic_monitor.in_curr_req,
                                  session->traffic_monitor.num_in_curr_req) < 0) {
            return -1;
        }
        //plog(ASL_LEVEL_DEBUG, "%s successful\n", __FUNCTION__);

        return session->traffic_monitor.num_in_curr_req;
    }
    return 0;
}

int
pk_sendget_outbound_sastats(ike_session_t *session)
{
    u_int32_t max_stats;
    u_int32_t seq;
    
    if (!session) {
        plog(ASL_LEVEL_DEBUG, "invalid args in %s \n", __FUNCTION__);
        return -1;
    }
    
    session->traffic_monitor.num_out_curr_req = 0;
    bzero(session->traffic_monitor.out_curr_req, sizeof(session->traffic_monitor.out_curr_req));
    max_stats = (sizeof(session->traffic_monitor.out_curr_req) / sizeof(session->traffic_monitor.out_curr_req[0]));

    // get list of SAs
    if ((session->traffic_monitor.num_out_curr_req = ike_session_get_sas_for_stats(session,
                                                                                   IPSEC_DIR_OUTBOUND,
                                                                                   &seq,
                                                                                   session->traffic_monitor.out_curr_req,
                                                                                   max_stats))) {
        u_int64_t session_ids[] = {(u_int64_t)session, 0};
        
        //plog(ASL_LEVEL_DEBUG, "about to call %s\n", __FUNCTION__);
        
        if (pfkey_send_getsastats(lcconf->sock_pfkey,
                                  seq,
                                  session_ids,
                                  1,
                                  IPSEC_DIR_OUTBOUND,
                                  session->traffic_monitor.out_curr_req,
                                  session->traffic_monitor.num_out_curr_req) < 0) {
            return -1;
        }
        //plog(ASL_LEVEL_DEBUG, "%s successful\n", __FUNCTION__);
        
        return session->traffic_monitor.num_out_curr_req;
    }
    return 0;
}

/*
 * receive GETSPDSTAT from kernel.
 */
static int
pk_recvgetsastat(mhp) 
caddr_t *mhp;
{
	struct sadb_msg        *msg;
    struct sadb_session_id *session_id;
    struct sadb_sastat     *stat_resp;
	ike_session_t          *session;

	/* validity check */
	if (mhp[0] == NULL ||
        mhp[SADB_EXT_SESSION_ID] == NULL ||
        mhp[SADB_EXT_SASTAT] == NULL) {
		plog(ASL_LEVEL_ERR, 
             "inappropriate sadb getsastat response.\n");
		return -1;
	}
	msg = ALIGNED_CAST(struct sadb_msg *)mhp[0];                         // Wcast-align fix (void*) - mhp contains pointers to structs in an aligned buffer
    session_id = ALIGNED_CAST(struct sadb_session_id *)mhp[SADB_EXT_SESSION_ID];
	stat_resp = ALIGNED_CAST(struct sadb_sastat *)mhp[SADB_EXT_SASTAT];

	/* the message has to be processed or not ? */
	if (msg->sadb_msg_pid != getpid()) {
		plog(ASL_LEVEL_DEBUG, 
             "%s message is not interesting "
             "because pid %d is not mine.\n",
             s_pfkey_type(msg->sadb_msg_type),
             msg->sadb_msg_pid);
		return -1;
	}
    if (!session_id->sadb_session_id_v[0]) {
		plog(ASL_LEVEL_DEBUG, 
             "%s message is bad "
             "because session-id[0] is invalid.\n",
             s_pfkey_type(msg->sadb_msg_type));
        return -1;
    }
    session = ALIGNED_CAST(__typeof__(session))session_id->sadb_session_id_v[0];

    if (!stat_resp->sadb_sastat_list_len) {
		plog(ASL_LEVEL_DEBUG, 
             "%s message is bad "
             "because it has no sastats.\n",
             s_pfkey_type(msg->sadb_msg_type));
        return -1;
    }

    ike_session_update_traffic_idle_status(session,
                                           stat_resp->sadb_sastat_dir,
                                           (struct sastat *)(stat_resp + 1),
                                           stat_resp->sadb_sastat_list_len);
	return 0;
}

/*
 * check if the algorithm is supported or not.
 * OUT	 0: ok
 *	-1: ng
 */
int
pk_checkalg(class, calg, keylen)
	int class, calg, keylen;
{
	int sup, error;
	u_int alg;
	struct sadb_alg alg0;

	switch (algclass2doi(class)) {
	case IPSECDOI_PROTO_IPSEC_ESP:
		sup = SADB_EXT_SUPPORTED_ENCRYPT;
		break;
	case IPSECDOI_ATTR_AUTH:
		sup = SADB_EXT_SUPPORTED_AUTH;
		break;
	case IPSECDOI_PROTO_IPCOMP:
		return 0;
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid algorithm class.\n");
		return -1;
	}
	alg = ipsecdoi2pfkey_alg(algclass2doi(class), algtype2doi(class, calg));
	if (alg == ~0)
		return -1;

	if (keylen == 0) {
		if (ipsec_get_keylen(sup, alg, &alg0)) {
			plog(ASL_LEVEL_ERR, 
				"%s.\n", ipsec_strerror());
			return -1;
		}
		keylen = alg0.sadb_alg_minbits;
	}

	error = ipsec_check_keylen(sup, alg, keylen);
	if (error)
		plog(ASL_LEVEL_ERR, 
			"%s.\n", ipsec_strerror());

	return error;
}

/*
 * differences with pfkey_recv() in libipsec/pfkey.c:
 * - never performs busy wait loop.
 * - returns NULL and set *lenp to negative on fatal failures
 * - returns NULL and set *lenp to non-negative on non-fatal failures
 * - returns non-NULL on success
 */
static struct sadb_msg *
pk_recv(so, lenp)
	int so;
	ssize_t *lenp;
{
	struct sadb_msg *newmsg;
	int reallen = 0; 
	socklen_t optlen = sizeof(reallen);
	
	if (getsockopt(so, SOL_SOCKET, SO_NREAD, &reallen, &optlen) < 0)
		return NULL;	/*fatal*/
	
	if (reallen == 0)
		return NULL;

	if ((newmsg = racoon_calloc(1, reallen)) == NULL)
		return NULL;

	while ((*lenp = recv(so, (caddr_t)newmsg, reallen, 0)) < 0) {
		if (errno == EINTR)
			continue;
		plog(ASL_LEVEL_ERR, "failed to recv pfkey message: %s\n", strerror(errno));
		break;
	}
	if (*lenp < 0) {
		racoon_free(newmsg);
		return NULL;	/*fatal*/
	} else if (*lenp != reallen || *lenp < sizeof(struct sadb_msg)) {
		racoon_free(newmsg);
		return NULL;
	}

	return newmsg;
}

/* see handler.h */
u_int32_t
pk_getseq()
{
	return eay_random();
}

static int
addnewsp(mhp)
	caddr_t *mhp;
{
	struct secpolicy *new;
	struct sadb_address *saddr, *daddr;
	struct sadb_x_policy *xpl;

	/* sanity check */
	if (mhp[SADB_EXT_ADDRESS_SRC] == NULL
	 || mhp[SADB_EXT_ADDRESS_DST] == NULL
	 || mhp[SADB_X_EXT_POLICY] == NULL) {
		plog(ASL_LEVEL_ERR, 
			"inappropriate sadb spd management message passed.\n");
		return -1;
	}

	saddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_SRC];    // Wcast-align fix (void*) - mhp contains pointers to aligned structs in malloc'd msg buffer
	daddr = ALIGNED_CAST(struct sadb_address *)mhp[SADB_EXT_ADDRESS_DST];
	xpl = ALIGNED_CAST(struct sadb_x_policy *)mhp[SADB_X_EXT_POLICY];

	new = newsp();
	if (new == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to allocate buffer\n");
		return -1;
	}

	new->spidx.dir = xpl->sadb_x_policy_dir;
	new->id = xpl->sadb_x_policy_id;
	new->policy = xpl->sadb_x_policy_type;
	new->req = NULL;

	/* check policy */
	switch (xpl->sadb_x_policy_type) {
	case IPSEC_POLICY_DISCARD:
	case IPSEC_POLICY_GENERATE:
	case IPSEC_POLICY_NONE:
	case IPSEC_POLICY_ENTRUST:
	case IPSEC_POLICY_BYPASS:
		break;

	case IPSEC_POLICY_IPSEC:
	    {
		int tlen;
		struct sadb_x_ipsecrequest *xisr;
		struct ipsecrequest **p_isr = &new->req;

		/* validity check */
		if (PFKEY_EXTLEN(xpl) < sizeof(*xpl)) {
			plog(ASL_LEVEL_ERR, 
				"invalid msg length.\n");
			return -1;
		}

		tlen = PFKEY_EXTLEN(xpl) - sizeof(*xpl);
		xisr = (struct sadb_x_ipsecrequest *)(xpl + 1);

		while (tlen > 0) {

			/* length check */
			if (xisr->sadb_x_ipsecrequest_len < sizeof(*xisr)) {
				plog(ASL_LEVEL_ERR, 
					"invalid msg length.\n");
				return -1;
			}

			/* allocate request buffer */
			*p_isr = newipsecreq();
			if (*p_isr == NULL) {
				plog(ASL_LEVEL_ERR, 
					"failed to get new ipsecreq.\n");
				return -1;
			}

			/* set values */
			(*p_isr)->next = NULL;

			switch (xisr->sadb_x_ipsecrequest_proto) {
			case IPPROTO_ESP:
			case IPPROTO_AH:
			case IPPROTO_IPCOMP:
				break;
			default:
				plog(ASL_LEVEL_ERR, 
					"invalid proto type: %u\n",
					xisr->sadb_x_ipsecrequest_proto);
				return -1;
			}
			(*p_isr)->saidx.proto = xisr->sadb_x_ipsecrequest_proto;

			switch (xisr->sadb_x_ipsecrequest_mode) {
			case IPSEC_MODE_TRANSPORT:
			case IPSEC_MODE_TUNNEL:
				break;
			case IPSEC_MODE_ANY:
			default:
				plog(ASL_LEVEL_ERR, 
					"invalid mode: %u\n",
					xisr->sadb_x_ipsecrequest_mode);
				return -1;
			}
			(*p_isr)->saidx.mode = xisr->sadb_x_ipsecrequest_mode;

			switch (xisr->sadb_x_ipsecrequest_level) {
			case IPSEC_LEVEL_DEFAULT:
			case IPSEC_LEVEL_USE:
			case IPSEC_LEVEL_REQUIRE:
				break;
			case IPSEC_LEVEL_UNIQUE:
				(*p_isr)->saidx.reqid =
					xisr->sadb_x_ipsecrequest_reqid;
				break;

			default:
				plog(ASL_LEVEL_ERR, 
					"invalid level: %u\n",
					xisr->sadb_x_ipsecrequest_level);
				return -1;
			}
			(*p_isr)->level = xisr->sadb_x_ipsecrequest_level;

			/* set IP addresses if there */
			if (xisr->sadb_x_ipsecrequest_len > sizeof(*xisr)) {
				struct sockaddr *paddr;

				paddr = (struct sockaddr *)(xisr + 1);
				bcopy(paddr, &(*p_isr)->saidx.src,
					sysdep_sa_len(paddr));

				paddr = (struct sockaddr *)((caddr_t)paddr
							+ sysdep_sa_len(paddr));
				bcopy(paddr, &(*p_isr)->saidx.dst,
					sysdep_sa_len(paddr));
			}

			(*p_isr)->sp = new;

			/* initialization for the next. */
			p_isr = &(*p_isr)->next;
			tlen -= xisr->sadb_x_ipsecrequest_len;

			/* validity check */
			if (tlen < 0) {
				plog(ASL_LEVEL_ERR, 
					"becoming tlen < 0\n");
			}

			xisr = ALIGNED_CAST(struct sadb_x_ipsecrequest *)((caddr_t)xisr
			                 + xisr->sadb_x_ipsecrequest_len);
		}
	    }
		break;
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid policy type.\n");
		return -1;
	}

#ifdef HAVE_PFKEY_POLICY_PRIORITY
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			xpl->sadb_x_policy_priority,
			&new->spidx);
#else
	KEY_SETSECSPIDX(xpl->sadb_x_policy_dir,
			saddr + 1,
			daddr + 1,
			saddr->sadb_address_prefixlen,
			daddr->sadb_address_prefixlen,
			saddr->sadb_address_proto,
			&new->spidx);
#endif

	inssp(new);

	return 0;
}

/* proto/mode/src->dst spi */
const char *
sadbsecas2str(src, dst, proto, spi, mode)
	struct sockaddr_storage *src, *dst;
	int proto;
	u_int32_t spi;
	int mode;
{
	static char buf[256];
	u_int doi_proto, doi_mode = 0;
	char *p;
	int blen, i;

	doi_proto = pfkey2ipsecdoi_proto(proto);
	if (doi_proto == ~0)
		return NULL;
	if (mode) {
		doi_mode = pfkey2ipsecdoi_mode(mode);
		if (doi_mode == ~0)
			return NULL;
	}

	blen = sizeof(buf) - 1;
	p = buf;

	i = snprintf(p, blen, "%s%s%s ",
		s_ipsecdoi_proto(doi_proto),
		mode ? "/" : "",
		mode ? s_ipsecdoi_encmode(doi_mode) : "");
	if (i < 0 || i >= blen)
		return NULL;
	p += i;
	blen -= i;

	i = snprintf(p, blen, "%s->", saddr2str((struct sockaddr *)src));
	if (i < 0 || i >= blen)
		return NULL;
	p += i;
	blen -= i;

	i = snprintf(p, blen, "%s ", saddr2str((struct sockaddr *)dst));
	if (i < 0 || i >= blen)
		return NULL;
	p += i;
	blen -= i;

	if (spi) {
		snprintf(p, blen, "spi=%lu(0x%lx)", (unsigned long)ntohl(spi),
		    (unsigned long)ntohl(spi));
	}

	return buf;
}
