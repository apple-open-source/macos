/*	$NetBSD: oakley.c,v 1.9.6.2 2007/04/04 13:08:28 vanhu Exp $	*/

/* Id: oakley.c,v 1.32 2006/05/26 12:19:46 manubsd Exp */

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
#include <sys/socket.h>	/* XXX for subjectaltname */
#include <netinet/in.h>	/* XXX for subjectaltname */

#ifdef HAVE_OPENSSL
#include <openssl/pkcs7.h>
#include <openssl/x509.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <TargetConditionals.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "str2val.h"
#include "plog.h"
#include "debug.h"

#include "isakmp_var.h"
#include "isakmp.h"
#ifdef ENABLE_HYBRID
#include "isakmp_xauth.h"
#include "isakmp_cfg.h" 
#endif                
#include "oakley.h"
#include "localconf.h"
#include "policy.h"
#include "handler.h"
#include "ipsec_doi.h"
#include "algorithm.h"
#include "dhgroup.h"
#include "sainfo.h"
#include "proposal.h"
#include "crypto_openssl.h"
#include "crypto_cssm.h"
#if HAVE_OPENDIR
#include "open_dir.h"
#endif
#include "sockmisc.h"
#include "strnames.h"
#include "gcmalloc.h"
#include <CoreFoundation/CoreFoundation.h>
#include "remoteconf.h"
#include "vpn_control.h"
#ifndef HAVE_OPENSSL
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#endif
#include "vpn_control_var.h"
#include "extern.h"

#define OUTBOUND_SA	0
#define INBOUND_SA	1

#define CERT_CHECKID_FROM_PEER 		0
#define CERT_CHECKID_FROM_RMCONFIG	1

#ifdef HAVE_OPENSSL
#define INITDHVAL(a, s, d, t)                                                  \
do {                                                                           \
vchar_t buf;                                                           \
buf.v = str2val((s), 16, &buf.l);                                      \
memset(&a, 0, sizeof(struct dhgroup));                                 \
a.type = (t);                                                          \
a.prime = vdup(&buf);                                                  \
a.gen1 = 2;                                                            \
a.gen2 = 0;                                                            \
racoon_free(buf.v);                                                    \
} while(0);
#else /* HAVE_OPENSSL */
#define INITDHVAL(a, s, d, t)                                                  \
do {                                                                           \
vchar_t buf;                                                           \
buf.v = str2val((s), 16, &buf.l);                                      \
memset(&a, 0, sizeof(struct dhgroup));                                 \
a.desc = (d);															\
a.type = (t);                                                          \
a.prime = vdup(&buf);                                                  \
a.gen1 = 2;                                                            \
a.gen2 = 0;                                                            \
racoon_free(buf.v);                                                    \
} while(0);
#endif /* HAVE_OPENSSL */

struct dhgroup dh_modp768;
struct dhgroup dh_modp1024;
struct dhgroup dh_modp1536;
struct dhgroup dh_modp2048;
struct dhgroup dh_modp3072;
struct dhgroup dh_modp4096;
struct dhgroup dh_modp6144;
struct dhgroup dh_modp8192;


static int oakley_check_dh_pub (vchar_t *, vchar_t **);
static int oakley_compute_keymat_x (phase2_handle_t *, int, int);
static int get_cert_fromlocal (phase1_handle_t *, int);
static int oakley_check_certid (phase1_handle_t *iph1);
static int oakley_check_certid_1 (vchar_t *, int, int, void*, cert_status_t *certStatus);
#ifdef HAVE_OPENSSL
static int check_typeofcertname (int, int);
#endif
static cert_t *save_certbuf (struct isakmp_gen *);
static int oakley_padlen (int, int);

static int base64toCFData (vchar_t *, CFDataRef*);
static cert_t *oakley_appendcert_to_certchain (cert_t *, cert_t *);

int
oakley_get_defaultlifetime()
{
	return OAKLEY_ATTR_SA_LD_SEC_DEFAULT;
}

int
oakley_dhinit()
{
	/* set DH MODP */
	INITDHVAL(dh_modp768, OAKLEY_PRIME_MODP768,
		OAKLEY_ATTR_GRP_DESC_MODP768, OAKLEY_ATTR_GRP_TYPE_MODP);
	INITDHVAL(dh_modp1024, OAKLEY_PRIME_MODP1024,
		OAKLEY_ATTR_GRP_DESC_MODP1024, OAKLEY_ATTR_GRP_TYPE_MODP);
	INITDHVAL(dh_modp1536, OAKLEY_PRIME_MODP1536,
		OAKLEY_ATTR_GRP_DESC_MODP1536, OAKLEY_ATTR_GRP_TYPE_MODP);
	INITDHVAL(dh_modp2048, OAKLEY_PRIME_MODP2048,
		OAKLEY_ATTR_GRP_DESC_MODP2048, OAKLEY_ATTR_GRP_TYPE_MODP);
	INITDHVAL(dh_modp3072, OAKLEY_PRIME_MODP3072,
		OAKLEY_ATTR_GRP_DESC_MODP3072, OAKLEY_ATTR_GRP_TYPE_MODP);
	INITDHVAL(dh_modp4096, OAKLEY_PRIME_MODP4096,
		OAKLEY_ATTR_GRP_DESC_MODP4096, OAKLEY_ATTR_GRP_TYPE_MODP);
	INITDHVAL(dh_modp6144, OAKLEY_PRIME_MODP6144,
		OAKLEY_ATTR_GRP_DESC_MODP6144, OAKLEY_ATTR_GRP_TYPE_MODP);
	INITDHVAL(dh_modp8192, OAKLEY_PRIME_MODP8192,
		OAKLEY_ATTR_GRP_DESC_MODP8192, OAKLEY_ATTR_GRP_TYPE_MODP);

	return 0;
}

void
oakley_dhgrp_free(struct dhgroup *dhgrp)
{
    VPTRINIT(dhgrp->prime);
    VPTRINIT(dhgrp->curve_a);
    VPTRINIT(dhgrp->curve_b);
    VPTRINIT(dhgrp->order);
	racoon_free(dhgrp);
}

/*
 * RFC2409 5
 * The length of the Diffie-Hellman public value MUST be equal to the
 * length of the prime modulus over which the exponentiation was
 * performed, prepending zero bits to the value if necessary.
 */
static int
oakley_check_dh_pub(vchar_t *prime, vchar_t **pub0)
{
	vchar_t *tmp;
	vchar_t *pub = *pub0;

	if (prime->l == pub->l)
		return 0;

	if (prime->l < pub->l) {
		/* what should i do ? */
		plog(ASL_LEVEL_ERR, 
			"invalid public information was generated.\n");
		return -1;
	}

	/* prime->l > pub->l */
	tmp = vmalloc(prime->l);
	if (tmp == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get DH buffer.\n");
		return -1;
	}
	memcpy(tmp->v + prime->l - pub->l, pub->v, pub->l);

	vfree(*pub0);
	*pub0 = tmp;

	return 0;
}

/*
 * compute sharing secret of DH
 * IN:	*dh, *pub, *priv, *pub_p
 * OUT: **gxy
 */
#ifdef HAVE_OPENSSL
int
oakley_dh_compute(const struct dhgroup *dh, vchar_t *pub, vchar_t *priv, vchar_t *pub_p, vchar_t **gxy)
{
#ifdef ENABLE_STATS
	struct timeval start, end;
#endif
	if ((*gxy = vmalloc(dh->prime->l)) == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get DH buffer.\n");
		return -1;
	}

#ifdef ENABLE_STATS
	gettimeofday(&start, NULL);
#endif
	switch (dh->type) {
	case OAKLEY_ATTR_GRP_TYPE_MODP:
		if (eay_dh_compute(dh->prime, dh->gen1, pub, priv, pub_p, gxy) < 0) {
			plog(ASL_LEVEL_ERR, 
				"failed to compute dh value.\n");
			return -1;
		}
		break;
	case OAKLEY_ATTR_GRP_TYPE_ECP:
	case OAKLEY_ATTR_GRP_TYPE_EC2N:
		plog(ASL_LEVEL_ERR, 
			"dh type %d isn't supported.\n", dh->type);
		return -1;
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid dh type %d.\n", dh->type);
		return -1;
	}

#ifdef ENABLE_STATS
	gettimeofday(&end, NULL);
	plog(ASL_LEVEL_NOTICE, "%s(%s%d): %8.6f", __func__,
		s_attr_isakmp_group(dh->type), dh->prime->l << 3,
		timedelta(&start, &end));
#endif

	plog(ASL_LEVEL_DEBUG, "compute DH's shared.\n");

	return 0;
}
#else
int
oakley_dh_compute(const struct dhgroup *dh, vchar_t *pub_p, vchar_t **gxy, SecDHContext *dhC)
{
	
	vchar_t *computed_key = NULL;
	size_t	computed_keylen;
	size_t	maxKeyLen;
	
#ifdef ENABLE_STATS
	struct timeval start, end;
	gettimeofday(&start, NULL);
#endif
	
	plog(ASL_LEVEL_DEBUG, "compute DH result.\n");

	if (pub_p->l != dh->prime->l) {
		plog(ASL_LEVEL_ERR, "remote public key length (%zu) != prime length (%zu)\n",
			 pub_p->l, dh->prime->l);
		goto fail;
	}

	maxKeyLen = SecDHGetMaxKeyLength(*dhC);
	computed_key = vmalloc(maxKeyLen);
	if (computed_key == NULL) {
		plog(ASL_LEVEL_ERR, "memory error.\n");
		goto fail;
	}
	computed_keylen = computed_key->l;
	if (SecDHComputeKey(*dhC, (uint8_t*)pub_p->v, pub_p->l,
						(uint8_t*)computed_key->v, &computed_keylen)) {
		plog(ASL_LEVEL_ERR, "failed to compute dh value.\n");
		goto fail;
	}
	
#ifdef ENABLE_STATS
	gettimeofday(&end, NULL);
	plog(ASL_LEVEL_NOTICE, "%s(%s%d): %8.6f", __func__,
		   s_attr_isakmp_group(dh->type), dh->prime->l << 3,
		   timedelta(&start, &end));
#endif
	
	*gxy = vmalloc(maxKeyLen);
	if (*gxy == NULL) {
		plog(ASL_LEVEL_ERR, "memory error.\n");
		goto fail;
	}
	memcpy((*gxy)->v + (maxKeyLen - computed_keylen), computed_key->v, computed_keylen);
	plog(ASL_LEVEL_DEBUG, "compute DH's shared.\n");
	if (*dhC) {
		SecDHDestroy(*dhC);
		*dhC = NULL;
	}
	vfree(computed_key);
	return 0;
	
fail:
	if (*dhC) {
		SecDHDestroy(*dhC);
		*dhC = NULL;
	}
	vfree(*gxy);
	vfree(computed_key);
	return -1;
}

#endif

/*
 * generate values of DH
 * IN:	*dh
 * OUT: **pub, **priv
 */
#ifdef HAVE_OPENSSL
int
oakley_dh_generate(const struct dhgroup *dh, vchar_t **pub, vchar_t **priv)
{
#ifdef ENABLE_STATS
	struct timeval start, end;
	gettimeofday(&start, NULL);
#endif
	switch (dh->type) {
	case OAKLEY_ATTR_GRP_TYPE_MODP:
		if (eay_dh_generate(dh->prime, dh->gen1, dh->gen2, pub, priv) < 0) {
			plog(ASL_LEVEL_ERR, 
				"failed to compute dh value.\n");
			return -1;
		}
		break;

	case OAKLEY_ATTR_GRP_TYPE_ECP:
	case OAKLEY_ATTR_GRP_TYPE_EC2N:
		plog(ASL_LEVEL_ERR, 
			"dh type %d isn't supported.\n", dh->type);
		return -1;
	default:
		plog(ASL_LEVEL_ERR, 
			"invalid dh type %d.\n", dh->type);
		return -1;
	}

#ifdef ENABLE_STATS
	gettimeofday(&end, NULL);
	plog(ASL_LEVEL_NOTICE, "%s(%s%d): %8.6f", __func__,
		s_attr_isakmp_group(dh->type), dh->prime->l << 3,
		timedelta(&start, &end));
#endif

	if (oakley_check_dh_pub(dh->prime, pub) != 0)
		return -1;

	plog(ASL_LEVEL_DEBUG, "compute DH's private.\n");
	plog(ASL_LEVEL_DEBUG, "compute DH's public.\n");

	return 0;
}
#else
int
oakley_dh_generate(const struct dhgroup *dh, vchar_t **pub, SecDHContext *dhC)
{
	vchar_t *public = NULL;
	size_t maxKeyLen;
	size_t publicKeySize;

#ifdef ENABLE_STATS
	struct timeval start, end;
	gettimeofday(&start, NULL);
#endif
		
	plog(ASL_LEVEL_DEBUG, "generate DH key pair.\n");
	*pub = NULL;
	switch (dh->type) {
		case OAKLEY_ATTR_GRP_TYPE_MODP:
#define SECDH_MODP_GENERATOR 2
			if (SecDHCreate(SECDH_MODP_GENERATOR, (uint8_t*)dh->prime->v, dh->prime->l, 0, NULL, 0, dhC)) {
				plog(ASL_LEVEL_ERR, "failed to create dh context.\n");
				goto fail;
			}
			maxKeyLen = SecDHGetMaxKeyLength(*dhC);
			public = vmalloc(maxKeyLen);
			publicKeySize = public->l;
			if (public == NULL) {
				plog(ASL_LEVEL_ERR, "memory error.\n");
				goto fail;
			}
			if (SecDHGenerateKeypair(*dhC, (uint8_t*)public->v, &publicKeySize)) {
				plog(ASL_LEVEL_ERR, "failed to generate dh key pair.\n");
				goto fail;
			}
			plog(ASL_LEVEL_DEBUG, "got DH key pair.\n");
			
			*pub = vmalloc(maxKeyLen);
			if (*pub == NULL) {
				plog(ASL_LEVEL_ERR, "memory error.\n");
				goto fail;
			}			
			/* copy and fill with leading zeros */
			memcpy((*pub)->v + (maxKeyLen - publicKeySize), public->v, publicKeySize);
			break;
			
		case OAKLEY_ATTR_GRP_TYPE_ECP:
		case OAKLEY_ATTR_GRP_TYPE_EC2N:
			plog(ASL_LEVEL_ERR, 
				 "dh type %d isn't supported.\n", dh->type);
			goto fail;
		default:
			plog(ASL_LEVEL_ERR, 
				 "invalid dh type %d.\n", dh->type);
			goto fail;
	}
	
#ifdef ENABLE_STATS
	gettimeofday(&end, NULL);
	plog(ASL_LEVEL_NOTICE, "%s(%s%d): %8.6f", __func__,
		   s_attr_isakmp_group(dh->type), dh->prime->l << 3,
		   timedelta(&start, &end));
#endif
	
	if (oakley_check_dh_pub(dh->prime, pub) != 0) {
		plog(ASL_LEVEL_DEBUG, "failed DH public key size check.\n");
		goto fail;
	}
	
	//plogdump(ASL_LEVEL_DEBUG, (*pub)->v, (*pub)->l, "compute DH's public.\n");
	
	vfree(public);
	return 0;
	
fail:
	if (*dhC) {
		SecDHDestroy(*dhC);
		*dhC = NULL;
	}
	vfree(*pub);
	vfree(public);
	return -1;
	
}
#endif

/*
 * copy pre-defined dhgroup values.
 */
int
oakley_setdhgroup(int group, struct dhgroup **dhgrp)
{
	struct dhgroup *g;

	*dhgrp = NULL;	/* just make sure, initialize */

	g = alg_oakley_dhdef_group(group);
	if (g == NULL) {
		plog(ASL_LEVEL_ERR, 
			"invalid DH parameter grp=%d.\n", group);
		return -1;
	}

	if (!g->type || !g->prime || !g->gen1) {
		/* unsuported */
		plog(ASL_LEVEL_ERR, 
			"unsupported DH parameters grp=%d.\n", group);
		return -1;
	}

	*dhgrp = racoon_calloc(1, sizeof(struct dhgroup));
	if (*dhgrp == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get DH buffer.\n");
		return 0;
	}

	/* set defined dh vlaues */
	memcpy(*dhgrp, g, sizeof(*g));
	(*dhgrp)->prime = vdup(g->prime);

	return 0;
}

/*
 * PRF
 *
 * NOTE: we do not support prf with different input/output bitwidth,
 * so we do not implement RFC2409 Appendix B (DOORAK-MAC example) in
 * oakley_compute_keymat().  If you add support for such prf function,
 * modify oakley_compute_keymat() accordingly.
 */
vchar_t *
oakley_prf(vchar_t *key, vchar_t *buf, phase1_handle_t *iph1)
{
	vchar_t *res = NULL;
	int type = OAKLEY_ATTR_HASH_ALG_MD5;

	if (iph1->approval == NULL) {
		if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
			/*
			 * it's before negotiating hash algorithm.
			 * We use md5 as default.
			 */
			type = OAKLEY_ATTR_HASH_ALG_MD5;
		}
	} else
    {
        type = iph1->approval->hashtype;
    }
    res = alg_oakley_hmacdef_one(type, key, buf);
	if (res == NULL) {
		plog(ASL_LEVEL_ERR, 
			"invalid hmac algorithm %d.\n", type);
		return NULL;
	}

	return res;
}

/*
 * hash
 */
vchar_t *
oakley_hash(vchar_t *buf, phase1_handle_t *iph1)
{
	vchar_t *res = NULL;
	int type = OAKLEY_ATTR_HASH_ALG_MD5;

	if (iph1->approval == NULL) {
		if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
			/*
			 * it's before negotiating hash algorithm.
			 * We use md5 as default.
			 */
			type = OAKLEY_ATTR_HASH_ALG_MD5;
		}
	} else {
        if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
            type = iph1->approval->hashtype;
        }
    }

	res = alg_oakley_hashdef_one(type, buf);
	if (res == NULL) {
		plog(ASL_LEVEL_ERR, 
			"invalid hash algorithm %d.\n", type);
		return NULL;
	}

	return res;
}

/*
 * compute KEYMAT
 *   see seciton 5.5 Phase 2 - Quick Mode in isakmp-oakley-05.
 */
int
oakley_compute_keymat(phase2_handle_t *iph2, int side)
{
	int error = -1;

	/* compute sharing secret of DH when PFS */
	if (iph2->approval->pfs_group && iph2->dhpub_p) {
#ifdef HAVE_OPENSSL
		if (oakley_dh_compute(iph2->pfsgrp, iph2->dhpub,
							  iph2->dhpriv, iph2->dhpub_p, &iph2->dhgxy) < 0)
#else
		if (oakley_dh_compute(iph2->pfsgrp, iph2->dhpub_p, &iph2->dhgxy, &iph2->dhC) < 0)
#endif
			goto end;
	}

	/* compute keymat */
	if (oakley_compute_keymat_x(iph2, side, INBOUND_SA) < 0
	 || oakley_compute_keymat_x(iph2, side, OUTBOUND_SA) < 0)
		goto end;

	plog(ASL_LEVEL_DEBUG, "KEYMAT computed.\n");

	error = 0;

end:
	return error;
}

/*
 * compute KEYMAT.
 * KEYMAT = prf(SKEYID_d, protocol | SPI | Ni_b | Nr_b).
 * If PFS is desired and KE payloads were exchanged,
 *   KEYMAT = prf(SKEYID_d, g(qm)^xy | protocol | SPI | Ni_b | Nr_b)
 *
 * NOTE: we do not support prf with different input/output bitwidth,
 * so we do not implement RFC2409 Appendix B (DOORAK-MAC example).
 */
static int
oakley_compute_keymat_x(phase2_handle_t *iph2, int side, int sa_dir)
{
	vchar_t *buf = NULL, *res = NULL, *bp;
	char *p;
	int len;
	int error = -1;
	int pfs = 0;
	int dupkeymat;	/* generate K[1-dupkeymat] */
	struct saproto *pr;
	struct satrns *tr;
	int encklen, authklen, l;

	pfs = ((iph2->approval->pfs_group && iph2->dhgxy) ? 1 : 0);
	
	len = pfs ? iph2->dhgxy->l : 0;
	len += (1
		+ sizeof(u_int32_t)	/* XXX SPI size */
		+ iph2->nonce->l
		+ iph2->nonce_p->l);
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get keymat buffer.\n");
		goto end;
	}

	for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
		p = buf->v;

		/* if PFS */
		if (pfs) {
			memcpy(p, iph2->dhgxy->v, iph2->dhgxy->l);
			p += iph2->dhgxy->l;
		}

		p[0] = pr->proto_id;
		p += 1;

		memcpy(p, (sa_dir == INBOUND_SA ? &pr->spi : &pr->spi_p),
			sizeof(pr->spi));
		p += sizeof(pr->spi);

		bp = (side == INITIATOR ? iph2->nonce : iph2->nonce_p);
		memcpy(p, bp->v, bp->l);
		p += bp->l;

		bp = (side == INITIATOR ? iph2->nonce_p : iph2->nonce);
		memcpy(p, bp->v, bp->l);
		p += bp->l;

		/* compute IV */
		//plogdump(ASL_LEVEL_DEBUG, buf->v, buf->l, "KEYMAT compute with\n");

		/* res = K1 */
		res = oakley_prf(iph2->ph1->skeyid_d, buf, iph2->ph1);
		if (res == NULL)
			goto end;

		/* compute key length needed */
		encklen = authklen = 0;
		switch (pr->proto_id) {
		case IPSECDOI_PROTO_IPSEC_ESP:
			for (tr = pr->head; tr; tr = tr->next) {
				l = alg_ipsec_encdef_keylen(tr->trns_id,
				    tr->encklen);
				if (l > encklen)
					encklen = l;

				l = alg_ipsec_hmacdef_hashlen(tr->authtype);
				if (l > authklen)
					authklen = l;
			}
			break;
		case IPSECDOI_PROTO_IPSEC_AH:
			for (tr = pr->head; tr; tr = tr->next) {
				l = alg_ipsec_hmacdef_hashlen(tr->trns_id);
				if (l > authklen)
					authklen = l;
			}
			break;
		default:
			break;
		}
		plog(ASL_LEVEL_DEBUG, "encklen=%d authklen=%d\n",
			encklen, authklen);

		dupkeymat = (encklen + authklen) / 8 / res->l;
		dupkeymat += 2;	/* safety mergin */
		if (dupkeymat < 3)
			dupkeymat = 3;
		//plog(ASL_LEVEL_DEBUG,
		//	"generating %zu bits of key (dupkeymat=%d)\n",
		//	dupkeymat * 8 * res->l, dupkeymat);
		if (0 < --dupkeymat) {
			vchar_t *prev = res;	/* K(n-1) */
			vchar_t *seed = NULL;	/* seed for Kn */
			size_t l;

			/*
			 * generating long key (isakmp-oakley-08 5.5)
			 *   KEYMAT = K1 | K2 | K3 | ...
			 * where
			 *   src = [ g(qm)^xy | ] protocol | SPI | Ni_b | Nr_b
			 *   K1 = prf(SKEYID_d, src)
			 *   K2 = prf(SKEYID_d, K1 | src)
			 *   K3 = prf(SKEYID_d, K2 | src)
			 *   Kn = prf(SKEYID_d, K(n-1) | src)
			 */
			//plog(ASL_LEVEL_DEBUG,
			//	"generating K1...K%d for KEYMAT.\n",
			//	dupkeymat + 1);

			seed = vmalloc(prev->l + buf->l);
			if (seed == NULL) {
				plog(ASL_LEVEL_ERR, 
					"failed to get keymat buffer.\n");
				if (prev && prev != res)
					vfree(prev);
				goto end;
			}

			while (dupkeymat--) {
				vchar_t *this = NULL;	/* Kn */
				int update_prev;

				memcpy(seed->v, prev->v, prev->l);
				memcpy(seed->v + prev->l, buf->v, buf->l);
				this = oakley_prf(iph2->ph1->skeyid_d, seed,
							iph2->ph1);
				if (!this) {
					plog(ASL_LEVEL_ERR, 
						"oakley_prf memory overflow\n");
					if (prev && prev != res)
						vfree(prev);
					vfree(this);
					vfree(seed);
					goto end;
				}

				update_prev = (prev && prev == res) ? 1 : 0;

				l = res->l;
				res = vrealloc(res, l + this->l);

				if (update_prev)
					prev = res;

				if (res == NULL) {
					plog(ASL_LEVEL_ERR, 
						"failed to get keymat buffer.\n");
					if (prev && prev != res)
						vfree(prev);
					vfree(this);
					vfree(seed);
					goto end;
				}
				memcpy(res->v + l, this->v, this->l);

				if (prev && prev != res)
					vfree(prev);
				prev = this;
				this = NULL;
			}

			if (prev && prev != res)
				vfree(prev);
			vfree(seed);
		}

		//plogdump(ASL_LEVEL_DEBUG, res->v, res->l, "");

		if (sa_dir == INBOUND_SA)
			pr->keymat = res;
		else
			pr->keymat_p = res;
		res = NULL;
	}

	error = 0;

end:
	if (error) {
		for (pr = iph2->approval->head; pr != NULL; pr = pr->next) {
			if (pr->keymat) {
				vfree(pr->keymat);
				pr->keymat = NULL;
			}
			if (pr->keymat_p) {
				vfree(pr->keymat_p);
				pr->keymat_p = NULL;
			}
		}
	}

	if (buf != NULL)
		vfree(buf);
	if (res)
		vfree(res);

	return error;
}


/*
 * compute HASH(3) prf(SKEYID_a, 0 | M-ID | Ni_b | Nr_b)
 *   see seciton 5.5 Phase 2 - Quick Mode in isakmp-oakley-05.
 */
vchar_t *
oakley_compute_hash3(phase1_handle_t *iph1, u_int32_t msgid, vchar_t *body)
{
	vchar_t *buf = 0, *res = 0;
	int len;
	int error = -1;

	/* create buffer */
	len = 1 + sizeof(u_int32_t) + body->l;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_NOTICE,
			"failed to get hash buffer\n");
		goto end;
	}

	buf->v[0] = 0;

	memcpy(buf->v + 1, (char *)&msgid, sizeof(msgid));

	memcpy(buf->v + 1 + sizeof(u_int32_t), body->v, body->l);

	/* compute HASH */
	res = oakley_prf(iph1->skeyid_a, buf, iph1);
	if (res == NULL)
		goto end;

	error = 0;

	//plogdump(ASL_LEVEL_DEBUG, res->v, res->l, "HASH computed:\n");

end:
	if (buf != NULL)
		vfree(buf);
	return res;
}

/*
 * compute HASH type of prf(SKEYID_a, M-ID | buffer)
 *	e.g.
 *	for quick mode HASH(1):
 *		prf(SKEYID_a, M-ID | SA | Ni [ | KE ] [ | IDci | IDcr ])
 *	for quick mode HASH(2):
 *		prf(SKEYID_a, M-ID | Ni_b | SA | Nr [ | KE ] [ | IDci | IDcr ])
 *	for Informational exchange:
 *		prf(SKEYID_a, M-ID | N/D)
 */
vchar_t *
oakley_compute_hash1(phase1_handle_t *iph1, u_int32_t msgid, vchar_t *body)
{
	vchar_t *buf = NULL, *res = NULL;
	char *p;
	int len;
	int error = -1;

	/* create buffer */
	len = sizeof(u_int32_t) + body->l;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_NOTICE,
			"failed to get hash buffer\n");
		goto end;
	}

	p = buf->v;

	memcpy(buf->v, (char *)&msgid, sizeof(msgid));
	p += sizeof(u_int32_t);

	memcpy(p, body->v, body->l);

	/* compute HASH */
	res = oakley_prf(iph1->skeyid_a, buf, iph1);
	if (res == NULL)
		goto end;

	error = 0;

	//plogdump(ASL_LEVEL_DEBUG, res->v, res->l, "HASH computed:\n");

end:
	if (buf != NULL)
		vfree(buf);
	return res;
}

/*
 * compute phase1 HASH
 * main/aggressive
 *   I-digest = prf(SKEYID, g^i | g^r | CKY-I | CKY-R | SAi_b | ID_i1_b)
 *   R-digest = prf(SKEYID, g^r | g^i | CKY-R | CKY-I | SAi_b | ID_r1_b)
 * for gssapi, also include all GSS tokens, and call gss_wrap on the result
 */
vchar_t *
oakley_ph1hash_common(phase1_handle_t *iph1, int sw)
{
	vchar_t *buf = NULL, *res = NULL, *bp;
	char *p, *bp2;
	int len, bl;
	int error = -1;

	/* create buffer */
	len = iph1->dhpub->l
		+ iph1->dhpub_p->l
		+ sizeof(cookie_t) * 2
		+ iph1->sa->l
		+ (sw == GENERATE ? iph1->id->l : iph1->id_p->l);

	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get hash buffer\n");
		goto end;
	}

	p = buf->v;

	bp = (sw == GENERATE ? iph1->dhpub : iph1->dhpub_p);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	bp = (sw == GENERATE ? iph1->dhpub_p : iph1->dhpub);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	if (iph1->side == INITIATOR)
		bp2 = (sw == GENERATE ?
		      (char *)&iph1->index.i_ck : (char *)&iph1->index.r_ck);
	else
		bp2 = (sw == GENERATE ?
		      (char *)&iph1->index.r_ck : (char *)&iph1->index.i_ck);
	bl = sizeof(cookie_t);
	memcpy(p, bp2, bl);
	p += bl;

	if (iph1->side == INITIATOR)
		bp2 = (sw == GENERATE ?
		      (char *)&iph1->index.r_ck : (char *)&iph1->index.i_ck);
	else
		bp2 = (sw == GENERATE ?
		      (char *)&iph1->index.i_ck : (char *)&iph1->index.r_ck);
	bl = sizeof(cookie_t);
	memcpy(p, bp2, bl);
	p += bl;

	bp = iph1->sa;
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	bp = (sw == GENERATE ? iph1->id : iph1->id_p);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	/* compute HASH */
	res = oakley_prf(iph1->skeyid, buf, iph1);
	if (res == NULL)
		goto end;

	error = 0;

end:
	if (buf != NULL)
		vfree(buf);
	return res;
}

/*
 * compute HASH_I on base mode.
 * base:psk,rsa
 *   HASH_I = prf(SKEYID, g^xi | CKY-I | CKY-R | SAi_b | IDii_b)
 * base:sig
 *   HASH_I = prf(hash(Ni_b | Nr_b), g^xi | CKY-I | CKY-R | SAi_b | IDii_b)
 */
vchar_t *
oakley_ph1hash_base_i(phase1_handle_t *iph1, int sw)
{
	vchar_t *buf = NULL, *res = NULL, *bp;
	vchar_t *hashkey = NULL;
	vchar_t *hash = NULL;	/* for signature mode */
	char *p;
	int len;
	int error = -1;

	/* sanity check */
	if (iph1->etype != ISAKMP_ETYPE_BASE) {
		plog(ASL_LEVEL_ERR, 
			"invalid etype for this hash function\n");
		return NULL;
	}

	switch (AUTHMETHOD(iph1)) {
	case OAKLEY_ATTR_AUTH_METHOD_PSKEY:
	case OAKLEY_ATTR_AUTH_METHOD_RSAENC:
	case OAKLEY_ATTR_AUTH_METHOD_RSAREV:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_R:
	case FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
#endif
		if (iph1->skeyid == NULL) {
			plog(ASL_LEVEL_ERR, "no SKEYID found.\n");
			return NULL;
		}
		hashkey = iph1->skeyid;
		break;

	case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
#endif
		/* make hash for seed */
		len = iph1->nonce->l + iph1->nonce_p->l;
		buf = vmalloc(len);
		if (buf == NULL) {
			plog(ASL_LEVEL_ERR, 
				"failed to get hash buffer\n");
			goto end;
		}
		p = buf->v;

		bp = (sw == GENERATE ? iph1->nonce_p : iph1->nonce);
		memcpy(p, bp->v, bp->l);
		p += bp->l;

		bp = (sw == GENERATE ? iph1->nonce : iph1->nonce_p);
		memcpy(p, bp->v, bp->l);
		p += bp->l;

		hash = oakley_hash(buf, iph1);
		if (hash == NULL)
			goto end;
		vfree(buf);
		buf = NULL;

		hashkey = hash;
		break;

	default:
		plog(ASL_LEVEL_ERR, 
			"not supported authentication method %d\n",
			iph1->approval->authmethod);
		return NULL;

	}

	len = (sw == GENERATE ? iph1->dhpub->l : iph1->dhpub_p->l)
		+ sizeof(cookie_t) * 2
		+ iph1->sa->l
		+ (sw == GENERATE ? iph1->id->l : iph1->id_p->l);
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get hash buffer\n");
		goto end;
	}
	p = buf->v;

	bp = (sw == GENERATE ? iph1->dhpub : iph1->dhpub_p);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	memcpy(p, &iph1->index.i_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);
	memcpy(p, &iph1->index.r_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);

	memcpy(p, iph1->sa->v, iph1->sa->l);
	p += iph1->sa->l;

	bp = (sw == GENERATE ? iph1->id : iph1->id_p);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	//plogdump(ASL_LEVEL_DEBUG, buf->v, buf->l, "HASH_I with:\n");

	/* compute HASH */
	res = oakley_prf(hashkey, buf, iph1);
	if (res == NULL)
		goto end;

	error = 0;

	//plogdump(ASL_LEVEL_DEBUG, res->v, res->l, "HASH_I computed:\n");

end:
	if (hash != NULL)
		vfree(hash);
	if (buf != NULL)
		vfree(buf);
	return res;
}

/*
 * compute HASH_R on base mode for signature method.
 * base:
 * HASH_R = prf(hash(Ni_b | Nr_b), g^xi | g^xr | CKY-I | CKY-R | SAi_b | IDii_b)
 */
vchar_t *
oakley_ph1hash_base_r(phase1_handle_t *iph1, int sw)
{
	vchar_t *buf = NULL, *res = NULL, *bp;
	vchar_t *hash = NULL;
	char *p;
	int len;
	int error = -1;

	/* sanity check */
	if (iph1->etype != ISAKMP_ETYPE_BASE) {
		plog(ASL_LEVEL_ERR, 
			"invalid etype for this hash function\n");
		return NULL;
	}

	switch(AUTHMETHOD(iph1)) {
	case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
	case FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I:
#endif
		break;
	default:
		plog(ASL_LEVEL_ERR, 
			"not supported authentication method %d\n",
			iph1->approval->authmethod);
		return NULL;
		break;
	}

	/* make hash for seed */
	len = iph1->nonce->l + iph1->nonce_p->l;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get hash buffer\n");
		goto end;
	}
	p = buf->v;

	bp = (sw == GENERATE ? iph1->nonce_p : iph1->nonce);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	bp = (sw == GENERATE ? iph1->nonce : iph1->nonce_p);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	hash = oakley_hash(buf, iph1);
	if (hash == NULL)
		goto end;
	vfree(buf);
	buf = NULL;

	/* make really hash */
	len = (sw == GENERATE ? iph1->dhpub_p->l : iph1->dhpub->l)
		+ (sw == GENERATE ? iph1->dhpub->l : iph1->dhpub_p->l)
		+ sizeof(cookie_t) * 2
		+ iph1->sa->l
		+ (sw == GENERATE ? iph1->id_p->l : iph1->id->l);
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get hash buffer\n");
		goto end;
	}
	p = buf->v;


	bp = (sw == GENERATE ? iph1->dhpub_p : iph1->dhpub);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	bp = (sw == GENERATE ? iph1->dhpub : iph1->dhpub_p);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	memcpy(p, &iph1->index.i_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);
	memcpy(p, &iph1->index.r_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);

	memcpy(p, iph1->sa->v, iph1->sa->l);
	p += iph1->sa->l;

	bp = (sw == GENERATE ? iph1->id_p : iph1->id);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	//plogdump(ASL_LEVEL_DEBUG, buf->v, buf->l, "HASH_R with:\n");

	/* compute HASH */
	res = oakley_prf(hash, buf, iph1);
	if (res == NULL)
		goto end;

	error = 0;

	//plogdump(ASL_LEVEL_DEBUG, res->v, res->l, "HASH_R computed:\n");

end:
	if (buf != NULL)
		vfree(buf);
	if (hash)
		vfree(hash);
	return res;
}

#if HAVE_OPENDIR
static int
oakley_verify_userid(phase1_handle_t *iph1)
{
	cert_t  *p;
	vchar_t *user_id;
	int      user_id_found = 0;
#ifndef HAVE_OPENSSL
	SecCertificateRef certificate;
	CFArrayRef commonNames;
	CFIndex i, l;
	CFStringRef name;
#endif /* HAVE_OPENSSL */

	for (p = iph1->cert_p; p; p = p->chain) {
#ifdef HAVE_OPENSSL
		user_id = eay_get_x509_common_name(&p->cert); //%%%%%%%% fix this
		if (user_id) {
			user_id_found = 1;
			// the following functions will check if user_id == 0
			if (open_dir_authorize_id(user_id, iph1->rmconf->open_dir_auth_group)) {
				vfree(user_id);
				return 0;
			}
			vfree(user_id);
		}
#else /* HAVE_OPENSSL */
		certificate = crypto_cssm_x509cert_CreateSecCertificateRef(&p->cert);
		if (certificate == NULL) {
			plog(ASL_LEVEL_ERR,
				 "ovuid failed to get SecCertificateRef\n");
			continue;
		}

		commonNames = SecCertificateCopyCommonNames(certificate);
		if (commonNames == NULL) {
			plog(ASL_LEVEL_ERR,
				 "ovuid failed to get commonNames\n");
			CFRelease(certificate);
			continue;
		}

		l = CFArrayGetCount(commonNames);
		for (i = 0; i < l; i++) {
			name = CFArrayGetValueAtIndex(commonNames, i);
			user_id = vmalloc(CFStringGetMaximumSizeForEncoding(CFStringGetLength(name),
																kCFStringEncodingUTF8) + 1);
			if (user_id) {
				if (CFStringGetCString(name, user_id->v, user_id->l,
										kCFStringEncodingUTF8)) {
					user_id_found = 1;
					// the following functions will check if user_id == 0
					if (open_dir_authorize_id(user_id, iph1->rmconf->open_dir_auth_group)) {
						vfree(user_id);
						CFRelease(certificate);
						CFRelease(commonNames);
						return 0;
					}
				}
				vfree(user_id);
			}
		}
		CFRelease(certificate);
		CFRelease(commonNames);
#endif /* HAVE_OPENSSL */
	}
	if (user_id_found) {
		plog(ASL_LEVEL_ERR, 
			 "the peer is not authorized for access.\n");
	} else {
		plog(ASL_LEVEL_ERR, 
			 "the peer is not authorized for access - user ID not found.\n");
	}
	return ISAKMP_NTYPE_AUTHENTICATION_FAILED;
}
#endif /* HAVE_OPENDIR */

/*
 * compute each authentication method in phase 1.
 * OUT:
 *	0:	OK
 *	-1:	error
 *	other:	error to be reply with notification.
 *	        the value is notification type.
 */
int
oakley_validate_auth(phase1_handle_t *iph1)
{
	vchar_t *my_hash = NULL;
	int result;
#ifdef ENABLE_STATS
	struct timeval start, end;
#endif
	SecKeyRef publicKeyRef = NULL;

#ifdef ENABLE_STATS
	gettimeofday(&start, NULL);
#endif

	switch (AUTHMETHOD(iph1)) {
	case OAKLEY_ATTR_AUTH_METHOD_PSKEY:
#ifdef ENABLE_HYBRID
	case FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
#endif
		/* validate HASH */
	    {
		char *r_hash;

		if (iph1->id_p == NULL || iph1->pl_hash == NULL) {
			plog(ASL_LEVEL_ERR,
				"few isakmp message received.\n");
			return ISAKMP_NTYPE_PAYLOAD_MALFORMED;
		}
#ifdef ENABLE_HYBRID
		if (AUTHMETHOD(iph1) == FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I &&
		    ((iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) == 0))
		{
			plog(ASL_LEVEL_ERR, "No SIG was passed, "
			    "hybrid auth is enabled, "
			    "but peer is no Xauth compliant\n");
			return ISAKMP_NTYPE_SITUATION_NOT_SUPPORTED;
			break;
		}
#endif
		r_hash = (caddr_t)(iph1->pl_hash + 1);

		//plogdump(ASL_LEVEL_DEBUG, r_hash,
		//	ntohs(iph1->pl_hash->h.len) - sizeof(*iph1->pl_hash), "HASH received:\n");

		if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
			switch (iph1->etype) {
			case ISAKMP_ETYPE_IDENT:
			case ISAKMP_ETYPE_AGG:
				my_hash = oakley_ph1hash_common(iph1, VALIDATE);
				break;
			case ISAKMP_ETYPE_BASE:
				if (iph1->side == INITIATOR)
					my_hash = oakley_ph1hash_common(iph1, VALIDATE);
				else
					my_hash = oakley_ph1hash_base_i(iph1, VALIDATE);
				break;
			default:
				plog(ASL_LEVEL_ERR, 
					 "invalid etype %d\n", iph1->etype);
				return ISAKMP_NTYPE_INVALID_EXCHANGE_TYPE;
			}
		}
		if (my_hash == NULL)
			return ISAKMP_INTERNAL_ERROR;

		result = memcmp(my_hash->v, r_hash, my_hash->l);
		vfree(my_hash);

		if (result) {
			plog(ASL_LEVEL_ERR, "HASH mismatched\n");
			return ISAKMP_NTYPE_INVALID_HASH_INFORMATION;
		}

		plog(ASL_LEVEL_DEBUG, "HASH for PSK validated.\n");
	    }
		break;
	case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
#endif
	    {
		int error = 0;
		int certtype = 0;

		/* validation */
		if (iph1->id_p == NULL) {
			plog(ASL_LEVEL_ERR,
				"no ID payload was passed.\n");
			return ISAKMP_NTYPE_PAYLOAD_MALFORMED;
		}
		if (iph1->sig_p == NULL) {
			plog(ASL_LEVEL_ERR,
				"no SIG payload was passed.\n");
			return ISAKMP_NTYPE_PAYLOAD_MALFORMED;
		}

		plog(ASL_LEVEL_DEBUG, "SIGN passed\n");

		/* get peer's cert */
		switch (iph1->rmconf->getcert_method) {
		case ISAKMP_GETCERT_PAYLOAD:
			if (iph1->cert_p == NULL) {
				plog(ASL_LEVEL_ERR, 
					"no peer's CERT payload found.\n");
				return ISAKMP_INTERNAL_ERROR;
			}
			break;
		default:
			plog(ASL_LEVEL_ERR, 
				"invalid getcert_mothod: %d\n",
				iph1->rmconf->getcert_method);
			return ISAKMP_INTERNAL_ERROR;
		}

		/* compare ID payload and certificate name */
		if (iph1->rmconf->verify_cert &&
		    (error = oakley_check_certid(iph1)) != 0)
			return error;

#if HAVE_OPENDIR
		/* check cert common name against Open Directory authentication group */
		if (iph1->rmconf->cert_verification_option == VERIFICATION_OPTION_OPEN_DIR) {
			if (oakley_verify_userid(iph1)) {
				return ISAKMP_NTYPE_AUTHENTICATION_FAILED;
			}
		}
#endif /* HAVE_OPENDIR */

		/* verify certificate */
		if (iph1->rmconf->verify_cert
		 && iph1->rmconf->getcert_method == ISAKMP_GETCERT_PAYLOAD) {
			certtype = iph1->rmconf->certtype;
#ifdef ENABLE_HYBRID
			switch (AUTHMETHOD(iph1)) {
			case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
				certtype = iph1->cert_p->type;
				break;
			default:
				break;
			}
#endif
			switch (certtype) {
			case ISAKMP_CERT_X509SIGN:
			{
				/* use ID from remote configuration */	
				/* check each ID in list			*/
				struct idspec *id_spec;
				CFStringRef	hostname = NULL;
				char *peers_id;
				struct genlist_entry *gpb = NULL;
				
				if (iph1->rmconf->cert_verification_option == VERIFICATION_OPTION_PEERS_IDENTIFIER) {
					id_spec = genlist_next(iph1->rmconf->idvl_p, &gpb);	/* expect only one id */						
					if (id_spec->idtype == IDTYPE_ADDRESS) {
						switch ((ALIGNED_CAST(struct sockaddr_storage *)(id_spec->id->v))->ss_family) {
							case AF_INET:
								peers_id = inet_ntoa((ALIGNED_CAST(struct sockaddr_in *)(id_spec->id->v))->sin_addr);
								hostname = CFStringCreateWithCString(NULL, peers_id, kCFStringEncodingUTF8);
								break;
#ifdef INET6
							case AF_INET6:
								return ISAKMP_NTYPE_INVALID_ID_INFORMATION;		/* not currently supported for embedded */
								break;
#endif
							default:
								plog(ASL_LEVEL_ERR, 
									"unknown address type for peers identifier.\n");
								return ISAKMP_NTYPE_AUTHENTICATION_FAILED;
								break;
						}						
					} else {
						hostname = CFStringCreateWithBytes(NULL, (u_int8_t *)id_spec->id->v, id_spec->id->l, kCFStringEncodingUTF8, FALSE);
					}
					if (hostname == NULL) {
						plog(ASL_LEVEL_ERR, "missing hostname for peers identifier.\n");
						return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
					}
				}
				error = crypto_cssm_check_x509cert(oakley_get_peer_cert_from_certchain(iph1), iph1->cert_p, hostname, &publicKeyRef);
				if (hostname)
					CFRelease(hostname);
			}
            break;
			
			default:
				plog(ASL_LEVEL_ERR, 
					"no supported certtype %d\n", certtype);
				return ISAKMP_INTERNAL_ERROR;
			}
			if (error != 0) {
				plog(ASL_LEVEL_ERR, 
					"the peer's certificate is not verified.\n");
				return ISAKMP_NTYPE_INVALID_CERT_AUTHORITY;
			}
		}

		plog(ASL_LEVEL_DEBUG, "CERT validated\n");

		if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
			/* compute hash */
			switch (iph1->etype) {
			case ISAKMP_ETYPE_IDENT:
			case ISAKMP_ETYPE_AGG:
				my_hash = oakley_ph1hash_common(iph1, VALIDATE);
				break;
			case ISAKMP_ETYPE_BASE:
				if (iph1->side == INITIATOR)
					my_hash = oakley_ph1hash_base_r(iph1, VALIDATE);
				else
					my_hash = oakley_ph1hash_base_i(iph1, VALIDATE);
				break;
			default:
				plog(ASL_LEVEL_ERR, 
					 "invalid etype %d\n", iph1->etype);
				return ISAKMP_NTYPE_INVALID_EXCHANGE_TYPE;
			}
		}
		if (my_hash == NULL)
			return ISAKMP_INTERNAL_ERROR;


		certtype = iph1->rmconf->certtype;
#ifdef ENABLE_HYBRID
		switch (AUTHMETHOD(iph1)) {
		case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
			certtype = iph1->cert_p->type;
			break;
		default:
			break;
		}
#endif
		/* check signature */
		switch (certtype) {
			case ISAKMP_CERT_X509SIGN:
				if (publicKeyRef == NULL) {
					plog(ASL_LEVEL_ERR, "@@@@@@ publicKeyRef is NULL\n");
				}
				if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
					error = crypto_cssm_verify_x509sign(publicKeyRef, my_hash, iph1->sig_p, FALSE);
				}
				if (error) {
					plog(ASL_LEVEL_ERR, "error verifying signature %s\n", GetSecurityErrorString(error));
				}
					
				CFRelease(publicKeyRef);				
				break;
			default:
				plog(ASL_LEVEL_ERR, 
					"no supported certtype %d\n",
					certtype);
				vfree(my_hash);
				return ISAKMP_INTERNAL_ERROR;
		}

		vfree(my_hash);
		if (error != 0) {
			plog(ASL_LEVEL_ERR, 
				"Invalid SIG.\n");
			return ISAKMP_NTYPE_INVALID_SIGNATURE;
		}
		plog(ASL_LEVEL_DEBUG, "SIG authenticated\n");
	    }
		break;
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
	    {
		if ((iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) == 0) {
			plog(ASL_LEVEL_ERR, "No SIG was passed, "
			    "hybrid auth is enabled, "
			    "but peer is no Xauth compliant\n");
			return ISAKMP_NTYPE_SITUATION_NOT_SUPPORTED;
			break;
		}
		plog(ASL_LEVEL_NOTICE, "No SIG was passed, "
		    "but hybrid auth is enabled\n");

		return 0;
		break;
	    }
#endif
	case OAKLEY_ATTR_AUTH_METHOD_RSAENC:
	case OAKLEY_ATTR_AUTH_METHOD_RSAREV:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_R:
#endif
		if (iph1->id_p == NULL || iph1->pl_hash == NULL) {
			plog(ASL_LEVEL_ERR,
				"few isakmp message received.\n");
			return ISAKMP_NTYPE_PAYLOAD_MALFORMED;
		}
		plog(ASL_LEVEL_ERR,
			"not supported authmethod type %s\n",
			s_oakley_attr_method(iph1->approval->authmethod));
		return ISAKMP_INTERNAL_ERROR;
	default:
		plog(ASL_LEVEL_ERR,
			"invalid authmethod %d why ?\n",
			iph1->approval->authmethod);
		return ISAKMP_INTERNAL_ERROR;
	}
#ifdef ENABLE_STATS
	gettimeofday(&end, NULL);
	plog(ASL_LEVEL_NOTICE, "%s(%s): %8.6f", __func__,
		s_oakley_attr_method(iph1->approval->authmethod),
		timedelta(&start, &end));
#endif

	return 0;
}

int
oakley_find_status_in_certchain (cert_t *certchain, cert_status_t certStatus)
{
	cert_t *p;

	for (p = certchain; p; p = p->chain) {
		if (p->status == certStatus) {
			return 1;
		}
	}
	return 0;
}

static
int
oakley_vpncontrol_notify_ike_failed_if_mycert_invalid (phase1_handle_t *iph1, int notify_initiator)
{
#ifndef HAVE_OPENSSL
	int premature = oakley_find_status_in_certchain(iph1->cert, CERT_STATUS_PREMATURE);
	int expired = oakley_find_status_in_certchain(iph1->cert, CERT_STATUS_EXPIRED);
	if (premature || expired) {
		u_int32_t fail_reason;

		if (premature) {
			fail_reason = VPNCTL_NTYPE_LOCAL_CERT_PREMATURE;
		} else {
			fail_reason = VPNCTL_NTYPE_LOCAL_CERT_EXPIRED;
		}
		vpncontrol_notify_ike_failed(fail_reason, notify_initiator, iph1_get_remote_v4_address(iph1), 0, NULL);
		return -1;
	}
#endif /* HAVE_OPENSSL */
	return 0;
}

/* get my certificate
 * NOTE: include certificate type.
 */
int
oakley_getmycert(phase1_handle_t *iph1)
{
	int	err;
	
	switch (iph1->rmconf->certtype) {
		case ISAKMP_CERT_X509SIGN:
			if (iph1->cert)
				return 0;
			if ( !(err = get_cert_fromlocal(iph1, 1))){
				if (oakley_vpncontrol_notify_ike_failed_if_mycert_invalid(iph1, FROM_LOCAL)) {
					return -1;
				}
			}
			return err;
		default:
			plog(ASL_LEVEL_ERR, 
			     "Unknown certtype #%d\n",
			     iph1->rmconf->certtype);
			return -1;
	}

}

/*
 * get a CERT from local file.
 * IN:
 *	my != 0 my cert.
 *	my == 0 peer's cert.
 */
static int
get_cert_fromlocal(phase1_handle_t *iph1, int my)
{
	vchar_t *cert = NULL;
	cert_t **certpl;
	int error = -1;
	cert_status_t status = CERT_STATUS_OK;

	if (my)
		certpl = &iph1->cert;
	else
		certpl = &iph1->cert_p;
	if (iph1->rmconf->identity_in_keychain == 0) {
		plog(ASL_LEVEL_ERR, "no CERT defined.\n");
		return 0;
	}

	switch (iph1->rmconf->certtype) {
	case ISAKMP_CERT_X509SIGN:
		if (iph1->rmconf->identity_in_keychain) {
			CFDataRef dataRef;
			
			if (iph1->rmconf->keychainCertRef == NULL || base64toCFData(iph1->rmconf->keychainCertRef, &dataRef))
				goto end;
			cert = crypto_cssm_get_x509cert(dataRef, &status);
			plog(ASL_LEVEL_DEBUG, "done with chking cert status %d\n",status);
			CFRelease(dataRef);
			break;
		} // else fall thru
	default:
		plog(ASL_LEVEL_ERR, 
			"not supported certtype %d\n",
			iph1->rmconf->certtype);
		goto end;
	}

	if (!cert) {
		plog(ASL_LEVEL_ERR, 
			"failed to get %s CERT.\n",
			my ? "my" : "peers");
		goto end;
	}

	*certpl = oakley_newcert();
	if (!*certpl) {
		plog(ASL_LEVEL_ERR, 
			"failed to get cert buffer.\n");
		goto end;
	}
	(*certpl)->pl = vmalloc(cert->l + 1);
	if ((*certpl)->pl == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get cert buffer\n");
		oakley_delcert(*certpl);
		*certpl = NULL;
		goto end;
	}
	memcpy((*certpl)->pl->v + 1, cert->v, cert->l);
	(*certpl)->pl->v[0] = iph1->rmconf->certtype;
	(*certpl)->type = iph1->rmconf->certtype;
	(*certpl)->status = status;    
	(*certpl)->cert.v = (*certpl)->pl->v + 1;
	(*certpl)->cert.l = (*certpl)->pl->l - 1;

	plog(ASL_LEVEL_DEBUG, "created CERT payload\n");
		
	error = 0;

end:
	if (cert != NULL)
		vfree(cert);

	return error;
}


/* get signature */
int
oakley_getsign(phase1_handle_t *iph1)
{
	vchar_t *privkey = NULL;
	int error = -1;

	switch (iph1->rmconf->certtype) {
	case ISAKMP_CERT_X509SIGN:
		// cert in keychain - use cssm to sign
		if (iph1->rmconf->identity_in_keychain) {
			CFDataRef dataRef;
			
			if (iph1->rmconf->keychainCertRef == NULL || base64toCFData(iph1->rmconf->keychainCertRef, &dataRef))
				goto end;
			iph1->sig = crypto_cssm_getsign(dataRef, iph1->hash);
			CFRelease(dataRef);
			break;
		} // else fall thru
	default:
		plog(ASL_LEVEL_ERR, 
		     "Unknown certtype #%d\n",
		     iph1->rmconf->certtype);
		goto end;
	}

	if (iph1->sig == NULL) {
		plog(ASL_LEVEL_ERR, "failed to sign.\n");
		goto end;
	}

	//plogdump(ASL_LEVEL_DEBUG, iph1->sig->v, iph1->sig->l, "SIGN computed:\n");

	error = 0;

end:
	if (privkey != NULL)
		vfree(privkey);

	return error;
}

void
oakley_verify_certid(phase1_handle_t *iph1)
{
	if (iph1->rmconf->verify_cert &&
		oakley_check_certid(iph1)){
		plog(ASL_LEVEL_DEBUG, 
			 "Discarding CERT: does not match ID:\n");
		oakley_delcert(iph1->cert_p);
		iph1->cert_p = NULL;
	}
}

static int
oakley_check_certid_in_certchain(cert_t *certchain, int idtype, int idlen, void *id)
{
	cert_t *p;

	for (p = certchain; p; p = p->chain) {
		if (oakley_check_certid_1(&p->cert, idtype, idlen, id, &p->status) == 0) {
			return 0;
		}
	}
	return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
}

cert_t *
oakley_get_peer_cert_from_certchain(phase1_handle_t * iph1)
{
	cert_t               *p;
	struct ipsecdoi_id_b *id_b;
	int                   idlen;
	void                 *peers_id;

	if (!iph1->id_p || !iph1->cert_p) {
		plog(ASL_LEVEL_ERR, "no ID nor CERT found.\n");
		return NULL;
	}
	if (!iph1->cert_p->chain) {
		// no chain: simply return the only cert
		return iph1->cert_p;
	}

	id_b = ALIGNED_CAST(struct ipsecdoi_id_b *)iph1->id_p->v;
	peers_id = id_b + 1;
	idlen = iph1->id_p->l - sizeof(*id_b);
	for (p = iph1->cert_p; p; p = p->chain) {
		if (oakley_check_certid_1(&p->cert, id_b->type, idlen, peers_id, &p->status) == 0) {
			return p;
		}
	}
	return NULL;
}

/*
 * compare certificate name and ID value.
 */
static int
oakley_check_certid(phase1_handle_t *iph1)
{
	struct ipsecdoi_id_b *id_b;
	int idlen;
	u_int8_t doi_type = 255;
	void *peers_id = NULL;

    /* use ID from peer */
    if (iph1->id_p == NULL || iph1->cert_p == NULL) {
        plog(ASL_LEVEL_ERR, "no ID nor CERT found.\n");
        return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
    }
    id_b = ALIGNED_CAST(struct ipsecdoi_id_b *)iph1->id_p->v;
    doi_type = id_b->type;
    peers_id = id_b + 1;
    idlen = iph1->id_p->l - sizeof(*id_b);
    
    return oakley_check_certid_in_certchain(iph1->cert_p, doi_type, idlen, peers_id);

}

static int
oakley_check_certid_1(vchar_t *cert, int idtype, int idlen, void *id, cert_status_t *certStatus)
{

	int len = 0;
	int error = 0;

#ifdef HAVE_OPENSSL
    int type;
	char *altname = NULL;
#endif
    
	switch (idtype) {
	case IPSECDOI_ID_DER_ASN1_DN:
	{
        CFDataRef subject;
        SecCertificateRef certificate;
		UInt8* namePtr = NULL;

		certificate = crypto_cssm_x509cert_CreateSecCertificateRef(cert);
		if (certificate == NULL) {
			plog(ASL_LEVEL_ERR,
				 "failed to get SecCertificateRef\n");
			if (certStatus && !*certStatus) {
				*certStatus = CERT_STATUS_INVALID;
			}
			return ISAKMP_NTYPE_INVALID_CERTIFICATE;
		}        
        subject = crypto_cssm_CopySubjectSequence(certificate);
        if (subject == NULL) {
            plog(ASL_LEVEL_ERR, "failed to get certificate subjectName\n");
            if (certStatus && !*certStatus) {
                *certStatus = CERT_STATUS_INVALID_SUBJNAME;
            }
            error = ISAKMP_NTYPE_INVALID_CERTIFICATE;
        } else {
            len = CFDataGetLength(subject);
            namePtr = (UInt8*)CFDataGetBytePtr(subject);
            if (namePtr) {
                if (idlen != len || memcmp(id, namePtr, idlen)) {
                    plog(ASL_LEVEL_ERR, "ID mismatched with certificate subjectName\n");
                    error =ISAKMP_NTYPE_INVALID_ID_INFORMATION;
                }
            } else {
                plog(ASL_LEVEL_ERR, "no certificate subjectName found\n");
                error = ISAKMP_NTYPE_INVALID_CERTIFICATE;
            }
        }
        if (error) {
            plog(ASL_LEVEL_ERR,
                 "ID mismatched with certificate subjectName\n");
			if (namePtr != NULL) {
				plogdump(ASL_LEVEL_ERR, namePtr, len, "subjectName (type %s):\n",
						 s_ipsecdoi_ident(idtype));
			} else {
				plog(ASL_LEVEL_ERR, "subjectName (type %s):\n", s_ipsecdoi_ident(idtype));
			}
            plogdump(ASL_LEVEL_ERR, id, idlen, "ID:\n");
            if (certStatus && !*certStatus) {
                *certStatus = CERT_STATUS_INVALID_SUBJNAME;
            }
        }
        CFRelease(certificate);
        if (subject != NULL) {
            CFRelease(subject);
        }
        return 0;
    }
    break;

	case IPSECDOI_ID_IPV4_ADDR:			
	case IPSECDOI_ID_IPV6_ADDR:
	{
#ifndef HAVE_OPENSSL
		CFIndex pos, count;
		SecCertificateRef certificate;
		CFArrayRef addresses;
#define ADDRESS_BUF_SIZE    64
        
		certificate = crypto_cssm_x509cert_CreateSecCertificateRef(cert);
		if (certificate == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get SecCertificateRef\n");
			if (certStatus && !*certStatus) {
				*certStatus = CERT_STATUS_INVALID;
			}
			return ISAKMP_NTYPE_INVALID_CERTIFICATE;
		}
		addresses = SecCertificateCopyIPAddresses(certificate);
		if (addresses == NULL) {
			plog(ASL_LEVEL_ERR, "failed to get subjectName\n");
			if (certStatus && !*certStatus) {
				*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
			}
			CFRelease(certificate);
			return ISAKMP_NTYPE_INVALID_CERTIFICATE;
		}
		count = CFArrayGetCount(addresses);		
		for (pos = 0; pos < count; pos++) {
			
			CFStringRef address;
			CFIndex addressLen;
            char *addressBuf;
            union sockaddr_in_4_6 sa = {};
			int result;
			
			address = CFArrayGetValueAtIndex(addresses, pos);			
			addressLen = CFStringGetLength(address);
			if (addressLen == 0)
				continue;
			addressBuf = racoon_malloc(ADDRESS_BUF_SIZE);
			if (addressBuf == NULL) {
				plog(ASL_LEVEL_ERR, "out of memory\n");
                CFRelease(addresses);
                CFRelease(certificate);
				return -1;
			}
			if (CFStringGetCString(address, addressBuf, ADDRESS_BUF_SIZE, kCFStringEncodingUTF8) == TRUE) {
                if (idtype == IPSECDOI_ID_IPV4_ADDR) {
                    result = inet_pton(AF_INET, addressBuf, &(sa.sin.sin_addr));
                } else if (idtype == IPSECDOI_ID_IPV6_ADDR) {
                    result = inet_pton(AF_INET6, addressBuf, &(sa.sin6.sin6_addr));
                } else {
                    result = 0;
                }
				racoon_free(addressBuf);
				if (result == 0)
					continue;	// wrong type or invalid address
                if (idtype == IPSECDOI_ID_IPV4_ADDR && idlen >= sizeof(sa.sin.sin_addr)) {
                    result = memcmp(id, &(sa.sin.sin_addr), sizeof(sa.sin.sin_addr));
                } else if (idtype == IPSECDOI_ID_IPV6_ADDR && idlen >= sizeof(sa.sin6.sin6_addr)) {
                    result = memcmp(id, &(sa.sin.sin_addr), sizeof(sa.sin6.sin6_addr));
                } else {
                    continue;
                }

                if (result == 0) {
					CFRelease(addresses);
					CFRelease(certificate);
					return 0;
				}
			} else
				racoon_free(addressBuf);
		}
		plog(ASL_LEVEL_ERR, "ID mismatched with subjectAltName.\n");
		plog(ASL_LEVEL_ERR, 
			 "subjectAltName (expected type %s):\n", s_ipsecdoi_ident(idtype));
		plogdump(ASL_LEVEL_ERR, id, idlen, "ID:\n");
		CFRelease(addresses);
		CFRelease(certificate);		
		if (certStatus && !*certStatus) {
			*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
		}
		return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
#else			
		/* 
		 * Openssl returns the IPAddress as an ASN1 octet string (binary format)
		 * followed by a trailing NULL.  5 bytes for IPv4 and 17 bytes for IPv6
		 */	
		#define SUBJ_ALT_NAME_IPV4_ADDRESS_LEN  5
		#define SUBJ_ALT_NAME_IPV6_ADDRESS_LEN	17
		
		int pos;
		
		if ((idtype == IPSECDOI_ID_IPV4_ADDR && idlen != sizeof(struct in_addr))
			|| (idtype == IPSECDOI_ID_IPV6_ADDR && idlen != sizeof(struct in6_addr))) {
			plog(ASL_LEVEL_ERR, 
					"invalid address length passed.\n");
			return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
		}

		for (pos = 1; ; pos++) {
			if (eay_get_x509subjectaltname(cert, &altname, &type, pos, &len) !=0) {
				plog(ASL_LEVEL_ERR, 
					"failed to get subjectAltName\n");
				if (certStatus && !*certStatus) {
					*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
				}
				return ISAKMP_NTYPE_INVALID_CERTIFICATE;
			}

			/* it's the end condition of the loop. */
			if (!altname) {
				plog(ASL_LEVEL_ERR, 
					 "invalid subjectAltName\n");
				if (certStatus && !*certStatus) {
					*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
				}
				return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
			}

			if (check_typeofcertname(idtype, type) != 0) {
				/* wrong type - skip this one */
				racoon_free(altname);
				altname = NULL;
				continue;
			}
		
			if (len == SUBJ_ALT_NAME_IPV4_ADDRESS_LEN) { /* IPv4 */
				if (idtype != IPSECDOI_ID_IPV4_ADDR) {
					/* wrong IP address type - skip this one */				
					racoon_free(altname);
					altname = NULL;
					continue;
				}
			}
#ifdef INET6
			else if (len == SUBJ_ALT_NAME_IPV6_ADDRESS_LEN) { /* IPv6 */
				if (idtype != IPSECDOI_ID_IPV6_ADDR) {
					/* wrong IP address type - skip this one */				
					racoon_free(altname);
					altname = NULL;
					continue;
				}
			}
#endif
			else {
				/* invalid IP address length in certificate - bad or bogus certificate */
				plog(ASL_LEVEL_ERR, 
					"invalid IP address in certificate.\n");
				plogdump(ASL_LEVEL_ERR, altname, len, "subjectAltName (expected type %s, got type %s):\n",
						 s_ipsecdoi_ident(idtype),
						 s_ipsecdoi_ident(type));
				racoon_free(altname);
				altname = NULL;
				if (certStatus && !*certStatus) {
					*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
				}
				return ISAKMP_NTYPE_INVALID_CERTIFICATE;
			}
			
			/* compare the addresses */		
			error = memcmp(id, altname, idlen);
			if (error)
				continue;
			racoon_free(altname);
			return 0;
		}		
		/* failed to find a match */
		plog(ASL_LEVEL_ERR, 
			 "ID mismatched with subjectAltName.\n");
		plogdump(ASL_LEVEL_ERR, altname, len, "subjectAltName (expected type %s, got type %s):\n",
				 s_ipsecdoi_ident(idtype),
				 s_ipsecdoi_ident(type));
		plogdump(ASL_LEVEL_ERR, id, idlen, "ID:\n");
		racoon_free(altname);
		if (certStatus && !*certStatus)
			*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
		return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
		
#endif /* HAVE_OPENSSL */
	}

#ifndef HAVE_OPENSSL
	case IPSECDOI_ID_FQDN:
	{
		CFIndex pos, count;
		SecCertificateRef certificate;
		CFArrayRef names;
		CFStringRef name, ID;
		
		certificate = crypto_cssm_x509cert_CreateSecCertificateRef(cert);
		if (certificate == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get SecCertificateRef\n");
			if (certStatus && !*certStatus) {
				*certStatus = CERT_STATUS_INVALID;
			}
			return ISAKMP_NTYPE_INVALID_CERTIFICATE;
		}
		names = SecCertificateCopyDNSNames(certificate);
		if (names == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get subjectName\n");
			if (certStatus && !*certStatus) {
				*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
			}
			CFRelease(certificate);
			return ISAKMP_NTYPE_INVALID_CERTIFICATE;
		}
		count = CFArrayGetCount(names);		
		ID = CFStringCreateWithBytes(kCFAllocatorDefault, id, idlen, kCFStringEncodingUTF8, FALSE);
		if (ID== NULL) {
			plog(ASL_LEVEL_ERR, "memory error\n");
			CFRelease(names);
			CFRelease(certificate);
			return 0;
		}
		for (pos = 0; pos < count; pos++) {
			name = CFArrayGetValueAtIndex(names, pos);
			if (CFStringCompare(name, ID, 0) == kCFCompareEqualTo) {
				CFRelease(ID);
				CFRelease(names);
				CFRelease(certificate);
				return 0;
			}
		}
		plog(ASL_LEVEL_ERR, "ID mismatched with subjectAltName.\n");
		plog(ASL_LEVEL_ERR, 
			 "subjectAltName (expected type %s):\n", s_ipsecdoi_ident(idtype));
		plogdump(ASL_LEVEL_ERR, id, idlen, "ID:\n");
		CFRelease(ID);
		CFRelease(names);
		CFRelease(certificate);		
		if (certStatus && !*certStatus) {
			*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
		}
		return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
	}
		
	case IPSECDOI_ID_USER_FQDN:
	{
		CFIndex pos, count;
		
		SecCertificateRef certificate;
		CFArrayRef names;
		CFStringRef name, ID;
		
		certificate = crypto_cssm_x509cert_CreateSecCertificateRef(cert);
		if (certificate == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get SecCertificateRef\n");
			if (certStatus && !*certStatus) {
				*certStatus = CERT_STATUS_INVALID;
			}
			return ISAKMP_NTYPE_INVALID_CERTIFICATE;
		}
		names = SecCertificateCopyRFC822Names(certificate);
		if (names == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "failed to get subjectName\n");
			if (certStatus && !*certStatus) {
				*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
			}
			CFRelease(certificate);
			return ISAKMP_NTYPE_INVALID_CERTIFICATE;
		}
		count = CFArrayGetCount(names);
		ID = CFStringCreateWithBytes(kCFAllocatorDefault, id, idlen, kCFStringEncodingUTF8, FALSE);
		if (ID == NULL) {
			plog(ASL_LEVEL_ERR, 
				 "memory error\n");
			if (certStatus && !*certStatus) {
				*certStatus = CERT_STATUS_INVALID;
			}
			CFRelease(names);
			CFRelease(certificate);
			return ISAKMP_NTYPE_INVALID_CERTIFICATE;
		}		
		for (pos = 0; pos < count; pos++) {
			name = CFArrayGetValueAtIndex(names, pos);
			if (CFStringCompare(name, ID, 0) == kCFCompareEqualTo) {
				CFRelease(ID);
				CFRelease(names);
				CFRelease(certificate);
				return 0;
			}
		}
		plog(ASL_LEVEL_ERR, "ID mismatched with subjectAltName.\n");
		plog(ASL_LEVEL_ERR, 
			 "subjectAltName (expected type %s):\n", s_ipsecdoi_ident(idtype));
		plogdump(ASL_LEVEL_ERR, id, idlen, "ID:\n");
		CFRelease(ID);
		CFRelease(names);
		CFRelease(certificate);		
		if (certStatus && !*certStatus) {
			*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
		}
		return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
	}
#else	
	case IPSECDOI_ID_FQDN:
	case IPSECDOI_ID_USER_FQDN:
	{
		int pos;

		for (pos = 1; ; pos++) {
			if (eay_get_x509subjectaltname(cert, &altname, &type, pos, &len) != 0) {
				plog(ASL_LEVEL_ERR, 
					"failed to get subjectAltName\n");
				if (certStatus && !*certStatus) {
					*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
				}
				return ISAKMP_NTYPE_INVALID_CERTIFICATE;
			}

			/* it's the end condition of the loop. */
			if (!altname) {
				plog(ASL_LEVEL_ERR, 
					 "invalid subjectAltName\n");
				if (certStatus && !*certStatus) {
					*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
				}
				return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
			}

			if (check_typeofcertname(idtype, type) != 0) {
				/* wrong general type - skip this one */
				racoon_free(altname);
				altname = NULL;
				continue;
			}

			if (idlen != strlen(altname)) {
				/* wrong length - skip this one */
				racoon_free(altname);
				altname = NULL;
				continue;
			}
			error = memcmp(id, altname, idlen);
			if (error)
				continue;
			racoon_free(altname);
			return 0;
		}
		plog(ASL_LEVEL_ERR, "ID mismatched with subjectAltName.\n");
		plog(ASL_LEVEL_ERR, 
			 "subjectAltName (expected type %s, got type %s):\n",
			 s_ipsecdoi_ident(idtype),
			 s_ipsecdoi_ident(type));
		plogdump(ASL_LEVEL_ERR, altname, len, "subjectAltName (expected type %s, got type %s):\n",
				 s_ipsecdoi_ident(idtype),
				 s_ipsecdoi_ident(type));
		plogdump(ASL_LEVEL_ERR, id, idlen, "ID:\n");
		racoon_free(altname);
		if (certStatus && !*certStatus)
			*certStatus = CERT_STATUS_INVALID_SUBJALTNAME;
		return ISAKMP_NTYPE_INVALID_ID_INFORMATION;		
	}
#endif			
	default:
		plog(ASL_LEVEL_ERR, 
			"Impropper ID type passed: %s.\n",
			s_ipsecdoi_ident(idtype));
		return ISAKMP_NTYPE_INVALID_ID_INFORMATION;
	}	
	/*NOTREACHED*/
}
#ifdef HAVE_OPENSSL
static int
check_typeofcertname(int doi, int genid)
{
	switch (doi) {
	case IPSECDOI_ID_IPV4_ADDR:
	case IPSECDOI_ID_IPV4_ADDR_SUBNET:
	case IPSECDOI_ID_IPV6_ADDR:
	case IPSECDOI_ID_IPV6_ADDR_SUBNET:
	case IPSECDOI_ID_IPV4_ADDR_RANGE:
	case IPSECDOI_ID_IPV6_ADDR_RANGE:
		if (genid != GENT_IPADD)
			return -1;
		return 0;
	case IPSECDOI_ID_FQDN:
		if (genid != GENT_DNS)
			return -1;
		return 0;
	case IPSECDOI_ID_USER_FQDN:
		if (genid != GENT_EMAIL)
			return -1;
		return 0;
	case IPSECDOI_ID_DER_ASN1_DN: /* should not be passed to this function*/
	case IPSECDOI_ID_DER_ASN1_GN:
	case IPSECDOI_ID_KEY_ID:
	default:
		return -1;
	}
	/*NOTREACHED*/
}
#endif

/*
 * save certificate including certificate type.
 */
int
oakley_savecert(phase1_handle_t *iph1, struct isakmp_gen *gen)
{
	cert_t **c;
	u_int8_t type;
	type = *(u_int8_t *)(gen + 1) & 0xff;

	switch (type) {
	case ISAKMP_CERT_X509SIGN:
		c = &iph1->cert_p;
		break;
	default:
		plog(ASL_LEVEL_ERR, 
			"Invalid CERT type %d\n", type);
		return -1;
	}

	if (*c) {
		plog(ASL_LEVEL_WARNING, 
			"preexisting CERT payload... chaining.\n");
	}

    cert_t *new;
    new = save_certbuf(gen);
    if (!new) {
        plog(ASL_LEVEL_ERR, 
             "Failed to get CERT buffer.\n");
        return -1;
    }

    switch (new->type) {
    case ISAKMP_CERT_X509SIGN:
        /* Ignore cert if it doesn't match identity
         * XXX If verify cert is disabled, we still just take
         * the first certificate....
         */
        *c = oakley_appendcert_to_certchain(*c, new);
        plog(ASL_LEVEL_DEBUG, "CERT saved:\n");
        break;
    default:
        /* XXX */
        oakley_delcert(new);
        return 0;
    }
	
	return 0;
}

/*
 * save certificate including certificate type.
 */
int
oakley_savecr(phase1_handle_t *iph1, struct isakmp_gen *gen)
{
	cert_t **c;
	u_int8_t type;
	cert_t *new;

	type = *(u_int8_t *)(gen + 1) & 0xff;

	switch (type) {
	case ISAKMP_CERT_X509SIGN:
		if (iph1->cr_p) {
			oakley_delcert(iph1->cr_p);
			iph1->cr_p = NULL;
		}
		c = &iph1->cr_p;
		break;
	default:
		plog(ASL_LEVEL_ERR, 
			"Invalid CR type %d\n", type);
		return -1;
	}

	new = save_certbuf(gen);
	if (!new) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get CR buffer.\n");
		return -1;
	}
	*c = oakley_appendcert_to_certchain(*c, new);
	plog(ASL_LEVEL_DEBUG, "CR saved\n");

	return 0;
}

static cert_t *
save_certbuf(struct isakmp_gen *gen)
{
	cert_t *new;

	if(ntohs(gen->len) <= sizeof(*gen)){
		plog(ASL_LEVEL_ERR, 
			 "Len is too small !!.\n");
		return NULL;
	}

	new = oakley_newcert();
	if (!new) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get CERT buffer.\n");
		return NULL;
	}

	new->pl = vmalloc(ntohs(gen->len) - sizeof(*gen));
	if (new->pl == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to copy CERT from packet.\n");
		oakley_delcert(new);
		new = NULL;
		return NULL;
	}
	memcpy(new->pl->v, gen + 1, new->pl->l);
	new->type = new->pl->v[0] & 0xff;
	new->cert.v = new->pl->v + 1;
	new->cert.l = new->pl->l - 1;

	return new;
}

/*
 * get my CR.
 * NOTE: No Certificate Authority field is included to CR payload at the
 * moment. Becuase any certificate authority are accepted without any check.
 * The section 3.10 in RFC2408 says that this field SHOULD not be included,
 * if there is no specific certificate authority requested.
 */
vchar_t *
oakley_getcr(phase1_handle_t *iph1)
{
	vchar_t *buf;

	buf = vmalloc(1);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get cr buffer\n");
		return NULL;
	}
	if(iph1->rmconf->certtype == ISAKMP_CERT_NONE) {
		buf->v[0] = iph1->rmconf->cacerttype;
		plog(ASL_LEVEL_DEBUG, "create my CR: NONE, using %s instead\n",
		s_isakmp_certtype(iph1->rmconf->cacerttype));
	} else {
		buf->v[0] = iph1->rmconf->certtype;
		plog(ASL_LEVEL_DEBUG, "create my CR: %s\n",
		s_isakmp_certtype(iph1->rmconf->certtype));
	}
	//if (buf->l > 1)
	//	plogdump(ASL_LEVEL_DEBUG, buf->v, buf->l, "");

	return buf;
}

/*
 * check peer's CR.
 */
int
oakley_checkcr(phase1_handle_t *iph1)
{
	if (iph1->cr_p == NULL)
		return 0;

	plog(ASL_LEVEL_DEBUG,
		"peer transmitted CR: %s\n",
		s_isakmp_certtype(iph1->cr_p->type));

	if (iph1->cr_p->type != iph1->rmconf->certtype) {
		plog(ASL_LEVEL_ERR,
			"such a cert type isn't supported: %d\n",
			(char)iph1->cr_p->type);
		return -1;
	}

	return 0;
}

/*
 * check to need CR payload.
 */
int
oakley_needcr(int type)
{
	switch (type) {
	case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
#endif
		return 1;
	default:
		return 0;
	}
	/*NOTREACHED*/
}

vchar_t *
oakley_getpskall(phase1_handle_t *iph1)
{
	vchar_t *secret = NULL;

	if (iph1->rmconf->shared_secret) {
		
		switch (iph1->rmconf->secrettype) {
			case SECRETTYPE_KEY:
				/* in psk file - use KEY from remote configuration to locate it */
				secret = getpsk(iph1->rmconf->shared_secret->v, iph1->rmconf->shared_secret->l-1);
				break;
#if HAVE_KEYCHAIN
			case SECRETTYPE_KEYCHAIN:
				/* in the system keychain */
				secret = getpskfromkeychain(iph1->rmconf->shared_secret->v, iph1->etype, iph1->rmconf->secrettype, NULL);
				break;
			case SECRETTYPE_KEYCHAIN_BY_ID:
				/* in the system keychain - use peer id */
				secret = getpskfromkeychain(iph1->rmconf->shared_secret->v, iph1->etype, iph1->rmconf->secrettype, iph1->id_p);
				break;
#endif // HAVE_KEYCHAIN
			case SECRETTYPE_USE:
				/* in the remote configuration */
			default:
				/* rmconf->shared_secret is a string and contains a NULL character that must be removed */
				secret = vmalloc(iph1->rmconf->shared_secret->l - 1);
				if (secret == NULL) {
					plog(ASL_LEVEL_ERR, "memory error.\n");
					goto end;
				}
				memcpy(secret->v, iph1->rmconf->shared_secret->v, secret->l);
		}
	} else if (iph1->etype != ISAKMP_ETYPE_IDENT) {
		secret = getpskbyname(iph1->id_p);
		if (!secret) {
			if (iph1->rmconf->verify_identifier) {
				plog(ASL_LEVEL_ERR, "couldn't find pskey by peer's ID.\n");
				goto end;
			}
		}
	}
	if (!secret) {
		plog(ASL_LEVEL_NOTICE, "try to get pskey by the peer's address.\n");
		secret = getpskbyaddr(iph1->remote);
		if (!secret) {
			plog(ASL_LEVEL_ERR,
			     "couldn't find the pskey by address %s.\n",
			     saddrwop2str((struct sockaddr *)iph1->remote));
		}
	}

end:
	return secret;
}

/*
 * compute SKEYID
 * see seciton 5. Exchanges in RFC 2409
 * psk: SKEYID = prf(pre-shared-key, Ni_b | Nr_b)
 * sig: SKEYID = prf(Ni_b | Nr_b, g^ir)
 * enc: SKEYID = prf(H(Ni_b | Nr_b), CKY-I | CKY-R)
 */
int
oakley_skeyid(phase1_handle_t *iph1)
{
    vchar_t *key = NULL;
	vchar_t *buf = NULL;
    vchar_t *bp;
	char *p;
	int len;
	int error = -1;
	
	/* SKEYID */
	switch (AUTHMETHOD(iph1)) {
        case OAKLEY_ATTR_AUTH_METHOD_PSKEY:
#ifdef ENABLE_HYBRID
        case FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I:
        case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
#endif
            key = oakley_getpskall(iph1);
            if (key == NULL) {
                plog(ASL_LEVEL_ERR,
                     "couldn't find the pskey for %s.\n",
                     saddrwop2str((struct sockaddr *)iph1->remote));
                goto end;
            }
            plog(ASL_LEVEL_DEBUG, "the psk found.\n");
            /* should be secret PSK */
            plogdump(ASL_LEVEL_DEBUG, key->v, key->l, "psk: ");
            
            len = iph1->nonce->l + iph1->nonce_p->l;
            buf = vmalloc(len);
            if (buf == NULL) {
                plog(ASL_LEVEL_ERR,
                     "failed to get skeyid buffer\n");
                goto end;
            }
            p = buf->v;
            
            bp = (iph1->side == INITIATOR ? iph1->nonce : iph1->nonce_p);
            //plogdump(ASL_LEVEL_DEBUG, bp->v, bp->l, "nonce 1: ");
            memcpy(p, bp->v, bp->l);
            p += bp->l;
            
            bp = (iph1->side == INITIATOR ? iph1->nonce_p : iph1->nonce);
            //plogdump(ASL_LEVEL_DEBUG, bp->v, bp->l, "nonce 2: ");
            memcpy(p, bp->v, bp->l);
            p += bp->l;
            
            iph1->skeyid = oakley_prf(key, buf, iph1);
            
            if (iph1->skeyid == NULL)
                goto end;
            break;
            
        case OAKLEY_ATTR_AUTH_METHOD_RSASIG:
#ifdef ENABLE_HYBRID
        case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
        case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
        case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
        case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
#endif
            len = iph1->nonce->l + iph1->nonce_p->l;
            buf = vmalloc(len);
            if (buf == NULL) {
                plog(ASL_LEVEL_ERR,
                     "failed to get nonce buffer\n");
                goto end;
            }
            p = buf->v;
            
            bp = (iph1->side == INITIATOR ? iph1->nonce : iph1->nonce_p);
            //plogdump(ASL_LEVEL_DEBUG, bp->v, bp->l, "nonce1: ");
            memcpy(p, bp->v, bp->l);
            p += bp->l;
            
            bp = (iph1->side == INITIATOR ? iph1->nonce_p : iph1->nonce);
            //plogdump(ASL_LEVEL_DEBUG, bp->v, bp->l, "nonce2: ");
            memcpy(p, bp->v, bp->l);
            p += bp->l;
            
            iph1->skeyid = oakley_prf(buf, iph1->dhgxy, iph1);
            if (iph1->skeyid == NULL)
                goto end;
            break;
        case OAKLEY_ATTR_AUTH_METHOD_RSAENC:
        case OAKLEY_ATTR_AUTH_METHOD_RSAREV:
#ifdef ENABLE_HYBRID
        case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_I:
        case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_R:
        case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_I:
        case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_R:
#endif
            plog(ASL_LEVEL_WARNING, 
                 "not supported authentication method %s\n",
                 s_oakley_attr_method(iph1->approval->authmethod));
            goto end;
        default:
            plog(ASL_LEVEL_ERR, 
                 "invalid authentication method %d\n",
                 iph1->approval->authmethod);
            goto end;
	}
    
	//plogdump(ASL_LEVEL_DEBUG, iph1->skeyid->v, iph1->skeyid->l, "IKEv1 SKEYID computed:\n");
    
	error = 0;
    
end:
    if (key != NULL)
        vfree(key);
	if (buf != NULL)
		vfree(buf);
	return error;
}

/*
 * compute SKEYID_[dae]
 */
int
oakley_skeyid_dae(phase1_handle_t *iph1)
{
	vchar_t *buf = NULL;
	char *p;
	int len;
	int error = -1;

	if (iph1->skeyid == NULL) {
		plog(ASL_LEVEL_ERR, "no SKEYID found.\n");
		goto end;
	}
	
	/*
	 * see seciton 5. Exchanges in RFC 2409
	 * SKEYID_d = prf(SKEYID, g^ir | CKY-I | CKY-R | 0)
	 * SKEYID_a = prf(SKEYID, SKEYID_d | g^ir | CKY-I | CKY-R | 1)
	 * SKEYID_e = prf(SKEYID, SKEYID_a | g^ir | CKY-I | CKY-R | 2)
	 */
	/* SKEYID D */
	/* SKEYID_d = prf(SKEYID, g^xy | CKY-I | CKY-R | 0) */
	len = iph1->dhgxy->l + sizeof(cookie_t) * 2 + 1;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get skeyid buffer\n");
		goto end;
	}
	p = buf->v;

	memcpy(p, iph1->dhgxy->v, iph1->dhgxy->l);
	p += iph1->dhgxy->l;
	memcpy(p, (caddr_t)&iph1->index.i_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);
	memcpy(p, (caddr_t)&iph1->index.r_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);
	*p = 0;
	iph1->skeyid_d = oakley_prf(iph1->skeyid, buf, iph1);
	if (iph1->skeyid_d == NULL)
		goto end;

	vfree(buf);
	buf = NULL;

	//plogdump(ASL_LEVEL_DEBUG, iph1->skeyid_d->v, iph1->skeyid_d->l, "SKEYID_d computed:\n");

	/* SKEYID A */
	/* SKEYID_a = prf(SKEYID, SKEYID_d | g^xy | CKY-I | CKY-R | 1) */
	len = iph1->skeyid_d->l + iph1->dhgxy->l + sizeof(cookie_t) * 2 + 1;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get skeyid buffer\n");
		goto end;
	}
	p = buf->v;
	memcpy(p, iph1->skeyid_d->v, iph1->skeyid_d->l);
	p += iph1->skeyid_d->l;
	memcpy(p, iph1->dhgxy->v, iph1->dhgxy->l);
	p += iph1->dhgxy->l;
	memcpy(p, (caddr_t)&iph1->index.i_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);
	memcpy(p, (caddr_t)&iph1->index.r_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);
	*p = 1;
	iph1->skeyid_a = oakley_prf(iph1->skeyid, buf, iph1);
	if (iph1->skeyid_a == NULL)
		goto end;

	vfree(buf);
	buf = NULL;

	//plogdump(ASL_LEVEL_DEBUG, iph1->skeyid_a->v, iph1->skeyid_a->l, "SKEYID_a computed:\n");

	/* SKEYID E */
	/* SKEYID_e = prf(SKEYID, SKEYID_a | g^xy | CKY-I | CKY-R | 2) */
	len = iph1->skeyid_a->l + iph1->dhgxy->l + sizeof(cookie_t) * 2 + 1;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get skeyid buffer\n");
		goto end;
	}
	p = buf->v;
	memcpy(p, iph1->skeyid_a->v, iph1->skeyid_a->l);
	p += iph1->skeyid_a->l;
	memcpy(p, iph1->dhgxy->v, iph1->dhgxy->l);
	p += iph1->dhgxy->l;
	memcpy(p, (caddr_t)&iph1->index.i_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);
	memcpy(p, (caddr_t)&iph1->index.r_ck, sizeof(cookie_t));
	p += sizeof(cookie_t);
	*p = 2;
	iph1->skeyid_e = oakley_prf(iph1->skeyid, buf, iph1);
	if (iph1->skeyid_e == NULL)
		goto end;

	vfree(buf);
	buf = NULL;

	//plogdump(ASL_LEVEL_DEBUG, iph1->skeyid_e->v, iph1->skeyid_e->l, "SKEYID_e computed:\n");

	error = 0;

end:
	if (buf != NULL)
		vfree(buf);
	return error;
}

/*
 * compute final encryption key.
 * see Appendix B.
 */
int
oakley_compute_enckey(phase1_handle_t *iph1)
{
	u_int keylen, prflen;
	int error = -1;

	/* RFC2409 p39 */
	keylen = alg_oakley_encdef_keylen(iph1->approval->enctype,
					iph1->approval->encklen);
	if (keylen == -1) {
		plog(ASL_LEVEL_ERR, 
			"invalid encryption algoritym %d, "
			"or invalid key length %d.\n",
			iph1->approval->enctype,
			iph1->approval->encklen);
		goto end;
	}
	iph1->key = vmalloc(keylen >> 3);
	if (iph1->key == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get key buffer\n");
		goto end;
	}

	/* set prf length */
	prflen = alg_oakley_hashdef_hashlen(iph1->approval->hashtype);
	if (prflen == -1) {
		plog(ASL_LEVEL_ERR, 
			"invalid hash type %d.\n", iph1->approval->hashtype);
		goto end;
	}

	/* see isakmp-oakley-08 5.3. */
	if (iph1->key->l <= iph1->skeyid_e->l) {
		/*
		 * if length(Ka) <= length(SKEYID_e)
		 *	Ka = first length(K) bit of SKEYID_e
		 */
		memcpy(iph1->key->v, iph1->skeyid_e->v, iph1->key->l);
	} else {
		vchar_t *buf = NULL, *res = NULL;
		u_char *p, *ep;
		int cplen;
		int subkey;

		/*
		 * otherwise,
		 *	Ka = K1 | K2 | K3
		 * where
		 *	K1 = prf(SKEYID_e, 0)
		 *	K2 = prf(SKEYID_e, K1)
		 *	K3 = prf(SKEYID_e, K2)
		 */
		plog(ASL_LEVEL_DEBUG, 
			"len(SKEYID_e) < len(Ka) (%zu < %zu), "
			"generating long key (Ka = K1 | K2 | ...)\n",
			iph1->skeyid_e->l, iph1->key->l);

		if ((buf = vmalloc(prflen >> 3)) == 0) {
			plog(ASL_LEVEL_ERR, 
				"failed to get key buffer\n");
			goto end;
		}
		p = (u_char *)iph1->key->v;
		ep = p + iph1->key->l;

		subkey = 1;
		while (p < ep) {
			if (p == (u_char *)iph1->key->v) {
				/* just for computing K1 */
				buf->v[0] = 0;
				buf->l = 1;
			}
			res = oakley_prf(iph1->skeyid_e, buf, iph1);
			if (res == NULL) {
				vfree(buf);
				goto end;
			}
			plog(ASL_LEVEL_DEBUG, 
				"compute intermediate encryption key K%d\n",
				subkey);
			//plogdump(ASL_LEVEL_DEBUG, buf->v, buf->l, "");
			//plogdump(ASL_LEVEL_DEBUG, res->v, res->l, "");

			cplen = (res->l < ep - p) ? res->l : ep - p;
			memcpy(p, res->v, cplen);
			p += cplen;

			buf->l = prflen >> 3;	/* to cancel K1 speciality */
			if (res->l != buf->l) {
				plog(ASL_LEVEL_ERR, 
					"internal error: res->l=%zu buf->l=%zu\n",
					res->l, buf->l);
				vfree(res);
				vfree(buf);
				goto end;
			}
			memcpy(buf->v, res->v, res->l);
			vfree(res);
			subkey++;
		}

		vfree(buf);
	}

	/*
	 * don't check any weak key or not.
	 * draft-ietf-ipsec-ike-01.txt Appendix B.
	 * draft-ietf-ipsec-ciph-aes-cbc-00.txt Section 2.3.
	 */

	//plogdump(ASL_LEVEL_DEBUG, iph1->key->v, iph1->key->l, "final encryption key computed:\n");

	error = 0;

end:
	return error;
}

/* allocated new buffer for CERT */
cert_t *
oakley_newcert(void)
{
	cert_t *new;

	new = racoon_calloc(1, sizeof(*new));
	if (new == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get cert's buffer\n");
		return NULL;
	}

	new->pl = NULL;
	new->chain = NULL;

	return new;
}

/* delete buffer for CERT */
void
oakley_delcert_1(cert_t *cert)
{
	if (!cert)
		return;
	if (cert->pl)
		VPTRINIT(cert->pl);
	racoon_free(cert);
}

/* delete buffer for CERT */
void
oakley_delcert(cert_t *cert)
{
	cert_t *p, *to_delete;

	if (!cert)
		return;

	for (p = cert; p;) {
		to_delete = p;
		p = p->chain;
		oakley_delcert_1(to_delete);
	}
}

/* delete buffer for CERT */
static cert_t *
oakley_appendcert_to_certchain(cert_t *certchain, cert_t *new)
{
	cert_t *p;

	if (!certchain)
		return new;

	for (p = certchain; p; p = p->chain) {
		if (!p->chain) {
			p->chain = new;
			return certchain;
		}
	}
	return NULL;
}

/*
 * compute IV and set to ph1handle
 *	IV = hash(g^xi | g^xr)
 * see 4.1 Phase 1 state in draft-ietf-ipsec-ike.
 */
int
oakley_newiv(phase1_handle_t *iph1)
{
	struct isakmp_ivm *newivm = NULL;
	vchar_t *buf = NULL, *bp;
	char *p;
	int len;

	/* create buffer */
	len = iph1->dhpub->l + iph1->dhpub_p->l;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get IV buffer\n");
		return -1;
	}

	p = buf->v;

	bp = (iph1->side == INITIATOR ? iph1->dhpub : iph1->dhpub_p);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	bp = (iph1->side == INITIATOR ? iph1->dhpub_p : iph1->dhpub);
	memcpy(p, bp->v, bp->l);
	p += bp->l;

	/* allocate IVm */
	newivm = racoon_calloc(1, sizeof(struct isakmp_ivm));
	if (newivm == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get IV buffer\n");
		vfree(buf);
		return -1;
	}

	/* compute IV */
	newivm->iv = oakley_hash(buf, iph1);
	if (newivm->iv == NULL) {
		vfree(buf);
		oakley_delivm(newivm);
		return -1;
	}

	/* adjust length of iv */
	newivm->iv->l = alg_oakley_encdef_blocklen(iph1->approval->enctype);
	if (newivm->iv->l == -1) {
		plog(ASL_LEVEL_ERR, 
			"Invalid encryption algorithm %d.\n",
			iph1->approval->enctype);
		vfree(buf);
		oakley_delivm(newivm);
		return -1;
	}

	/* create buffer to save iv */
	if ((newivm->ive = vdup(newivm->iv)) == NULL) {
		plog(ASL_LEVEL_ERR, 
			"vdup (%s)\n", strerror(errno));
		vfree(buf);
		oakley_delivm(newivm);
		return -1;
	}

	vfree(buf);

	//plogdump(ASL_LEVEL_DEBUG, newivm->iv->v, newivm->iv->l, "IV computed:\n");

	if (iph1->ivm != NULL)
		oakley_delivm(iph1->ivm);

	iph1->ivm = newivm;

	return 0;
}

/*
 * compute IV for the payload after phase 1.
 * It's not limited for phase 2.
 * if pahse 1 was encrypted.
 *	IV = hash(last CBC block of Phase 1 | M-ID)
 * if phase 1 was not encrypted.
 *	IV = hash(phase 1 IV | M-ID)
 * see 4.2 Phase 2 state in draft-ietf-ipsec-ike.
 */
struct isakmp_ivm *
oakley_newiv2(phase1_handle_t *iph1, u_int32_t msgid)
{
	struct isakmp_ivm *newivm = NULL;
	vchar_t *buf = NULL;
	char *p;
	int len;
	int error = -1;

	/* create buffer */
	len = iph1->ivm->iv->l + sizeof(msgid_t);
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get IV buffer\n");
		goto end;
	}

	p = buf->v;

	memcpy(p, iph1->ivm->iv->v, iph1->ivm->iv->l);
	p += iph1->ivm->iv->l;

	memcpy(p, &msgid, sizeof(msgid));

	plog(ASL_LEVEL_DEBUG, "Compute IV for Phase 2\n");
	//plogdump(ASL_LEVEL_DEBUG, buf->v, buf->l, "Phase 1 last IV:\n");

	/* allocate IVm */
	newivm = racoon_calloc(1, sizeof(struct isakmp_ivm));
	if (newivm == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get IV buffer\n");
		goto end;
	}

	/* compute IV */
	if ((newivm->iv = oakley_hash(buf, iph1)) == NULL)
		goto end;

	/* adjust length of iv */
	newivm->iv->l = alg_oakley_encdef_blocklen(iph1->approval->enctype);
	if (newivm->iv->l == -1) {
		plog(ASL_LEVEL_ERR, 
			"Invalid encryption algorithm %d.\n",
			iph1->approval->enctype);
		goto end;
	}

	/* create buffer to save new iv */
	if ((newivm->ive = vdup(newivm->iv)) == NULL) {
		plog(ASL_LEVEL_ERR, "vdup (%s)\n", strerror(errno));
		goto end;
	}

	error = 0;

	//plogdump(ASL_LEVEL_DEBUG, newivm->iv->v, newivm->iv->l, "Phase 2 IV computed:\n");

end:
	if (error && newivm != NULL){
		oakley_delivm(newivm);
		newivm=NULL;
	}
	if (buf != NULL)
		vfree(buf);
	return newivm;
}

void
oakley_delivm(struct isakmp_ivm *ivm)
{
	if (ivm == NULL)
		return;

	if (ivm->iv != NULL)
		vfree(ivm->iv);
	if (ivm->ive != NULL)
		vfree(ivm->ive);
	racoon_free(ivm);
	plog(ASL_LEVEL_DEBUG, "IV freed\n");

	return;
}

/*
 * decrypt packet.
 *   save new iv and old iv.
 */
vchar_t *
oakley_do_ikev1_decrypt(phase1_handle_t *iph1, vchar_t *msg, vchar_t *ivdp, vchar_t *ivep)
{
	vchar_t *buf = NULL, *new = NULL;
	char *pl;
	int len;
	u_int8_t padlen;
	int blen;
	int error = -1;

	plog(ASL_LEVEL_DEBUG, "Begin decryption.\n");

	blen = alg_oakley_encdef_blocklen(iph1->approval->enctype);
	if (blen == -1) {
		plog(ASL_LEVEL_ERR, 
			"Invalid encryption algorithm %d.\n",
			iph1->approval->enctype);
		goto end;
	}

	/* save IV for next, but not sync. */
	memset(ivep->v, 0, ivep->l);
	memcpy(ivep->v, (caddr_t)&msg->v[msg->l - blen], blen);

	plogdump(ASL_LEVEL_DEBUG, ivep->v, ivep->l, "IV was saved for next processing:\n");

	pl = msg->v + sizeof(struct isakmp);

	len = msg->l - sizeof(struct isakmp);

	/* create buffer */
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get buffer to decrypt.\n");
		goto end;
	}
	memcpy(buf->v, pl, len);

	/* do decrypt */
	new = alg_oakley_encdef_decrypt(iph1->approval->enctype,
					buf, iph1->key, ivdp);
	if (new == NULL || new->v == NULL || new->l == 0) {
		plog(ASL_LEVEL_ERR, 
			"Decryption %d failed.\n", iph1->approval->enctype);
		goto end;
	}
	//plogdump(ASL_LEVEL_DEBUG, iph1->key->v, iph1->key->l, "with key:\n");

	vfree(buf);
	buf = NULL;
	if (new == NULL)
		goto end;

	plog(ASL_LEVEL_DEBUG, "decrypted payload by IV:\n");

	/* get padding length */
	if (lcconf->pad_excltail)
		padlen = new->v[new->l - 1] + 1;
	else
		padlen = new->v[new->l - 1];
	plog(ASL_LEVEL_DEBUG, "padding len=%u\n", padlen);

	/* trim padding */
	if (lcconf->pad_strict) {
		if (padlen > new->l) {
			plog(ASL_LEVEL_ERR, "invalid padding len=%u, buflen=%zu.\n",
					 padlen, new->l);
			goto end;
		}
		new->l -= padlen;
		plog(ASL_LEVEL_DEBUG, "trimmed padding\n");
	} else {
		plog(ASL_LEVEL_DEBUG, "skip to trim padding.\n");
	}

	/* create new buffer */
	len = sizeof(struct isakmp) + new->l;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"failed to get buffer to decrypt.\n");
		goto end;
	}
	memcpy(buf->v, msg->v, sizeof(struct isakmp));
	memcpy(buf->v + sizeof(struct isakmp), new->v, new->l);
	((struct isakmp *)buf->v)->len = htonl(buf->l);

	plog(ASL_LEVEL_DEBUG, "decrypted.\n");

#ifdef HAVE_PRINT_ISAKMP_C
	isakmp_printpacket(buf, iph1->remote, iph1->local, 1);
#endif

	error = 0;

end:
	if (error && buf != NULL) {
		vfree(buf);
		buf = NULL;
	}
	if (new != NULL)
		vfree(new);

	return buf;
}

/*
 * decrypt packet.
 */
vchar_t *
oakley_do_decrypt(phase1_handle_t *iph1, vchar_t *msg, vchar_t *ivdp, vchar_t *ivep)
{
	if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
		return(oakley_do_ikev1_decrypt(iph1, msg, ivdp, ivep));
	}
	
	plog(ASL_LEVEL_ERR, "Failed to decrypt invalid IKE version");
	return NULL;
}

/*
 * encrypt packet.
 */
vchar_t *
oakley_do_ikev1_encrypt(phase1_handle_t *iph1, vchar_t *msg, vchar_t *ivep, vchar_t *ivp)
{
	vchar_t *buf = 0, *new = 0;
	char *pl;
	int len;
	u_int padlen;
	int blen;
	int error = -1;

	plog(ASL_LEVEL_DEBUG, "Begin encryption.\n");

	/* set cbc block length */
	blen = alg_oakley_encdef_blocklen(iph1->approval->enctype);
	if (blen == -1) {
		plog(ASL_LEVEL_ERR, 
			"Invalid encryption algorithm %d.\n",
			iph1->approval->enctype);
		goto end;
	}

	pl = msg->v + sizeof(struct isakmp);
	len = msg->l - sizeof(struct isakmp);

	/* add padding */
	padlen = oakley_padlen(len, blen);
	plog(ASL_LEVEL_DEBUG, "pad length = %u\n", padlen);

	/* create buffer */
	buf = vmalloc(len + padlen);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get buffer to encrypt.\n");
		goto end;
	}
        if (padlen) {
                int i;
		char *p = &buf->v[len];
		if (lcconf->pad_random) {
			for (i = 0; i < padlen; i++)
				*p++ = eay_random() & 0xff;
		}
        }
        memcpy(buf->v, pl, len);

	/* make pad into tail */
	if (lcconf->pad_excltail)
		buf->v[len + padlen - 1] = padlen - 1;
	else
		buf->v[len + padlen - 1] = padlen;

	plogdump(ASL_LEVEL_DEBUG, buf->v, buf->l, "About to encrypt %d bytes", buf->l);

	/* do encrypt */
	new = alg_oakley_encdef_encrypt(iph1->approval->enctype,
					buf, iph1->key, ivep);
	if (new == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Encryption %d failed.\n", iph1->approval->enctype);
		goto end;
	}
	//plogdump(ASL_LEVEL_DEBUG, iph1->key->v, iph1->key->l, "with key:\n");

	vfree(buf);
	buf = NULL;
	if (new == NULL)
		goto end;

	//plogdump(ASL_LEVEL_DEBUG, ivep->v, ivep->l, "encrypted payload by IV:\n");

	/* save IV for next */
	memset(ivp->v, 0, ivp->l);
	memcpy(ivp->v, (caddr_t)&new->v[new->l - blen], blen);

	//plogdump(ASL_LEVEL_DEBUG, ivp->v, ivp->l, "save IV for next:\n");

	/* create new buffer */
	len = sizeof(struct isakmp) + new->l;
	buf = vmalloc(len);
	if (buf == NULL) {
		plog(ASL_LEVEL_ERR, 
			"Failed to get buffer to encrypt.\n");
		goto end;
	}
	memcpy(buf->v, msg->v, sizeof(struct isakmp));
	memcpy(buf->v + sizeof(struct isakmp), new->v, new->l);
	((struct isakmp *)buf->v)->len = htonl(buf->l);

	error = 0;

	plog(ASL_LEVEL_DEBUG, "Encrypted.\n");

end:
	if (error && buf != NULL) {
		vfree(buf);
		buf = NULL;
	}
	if (new != NULL)
		vfree(new);

	return buf;
}

/*
 * encrypt packet.
 */
vchar_t *
oakley_do_encrypt(phase1_handle_t *iph1, vchar_t *msg, vchar_t *ivep, vchar_t *ivp)
{
	if (iph1->version == ISAKMP_VERSION_NUMBER_IKEV1) {
		return(oakley_do_ikev1_encrypt(iph1, msg, ivep, ivp));		
	}

	plog(ASL_LEVEL_ERR, "Failed to encrypt invalid IKE version");
	return NULL;
}

/* culculate padding length */
static int
oakley_padlen(int len, int base)
{
	int padlen;

	padlen = base - (len % base);

	if (lcconf->pad_randomlen)
		padlen += ((eay_random() % (lcconf->pad_maxsize + 1) + 1) *
		    base);

	return padlen;
}

/* -----------------------------------------------------------------------------
The base-64 encoding packs three 8-bit bytes into four 7-bit ASCII
characters.  If the number of bytes in the original data isn't divisable
by three, "=" characters are used to pad the encoded data.  The complete
set of characters used in base-64 are:
     'A'..'Z' => 00..25
     'a'..'z' => 26..51
     '0'..'9' => 52..61
     '+'      => 62
     '/'      => 63
     '='      => pad

----------------------------------------------------------------------------- */
static const signed char base64_DecodeTable[128] = {
    /* 000 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 010 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 020 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* 030 */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* ' ' */ -1, -1, -1, -1, -1, -1, -1, -1,
    /* '(' */ -1, -1, -1, 62, -1, -1, -1, 63,
    /* '0' */ 52, 53, 54, 55, 56, 57, 58, 59,
    /* '8' */ 60, 61, -1, -1, -1,  0, -1, -1,
    /* '@' */ -1,  0,  1,  2,  3,  4,  5,  6,
    /* 'H' */  7,  8,  9, 10, 11, 12, 13, 14,
    /* 'P' */ 15, 16, 17, 18, 19, 20, 21, 22,
    /* 'X' */ 23, 24, 25, -1, -1, -1, -1, -1,
    /* '`' */ -1, 26, 27, 28, 29, 30, 31, 32,
    /* 'h' */ 33, 34, 35, 36, 37, 38, 39, 40,
    /* 'p' */ 41, 42, 43, 44, 45, 46, 47, 48,
    /* 'x' */ 49, 50, 51, -1, -1, -1, -1, -1
};

static int base64toCFData(vchar_t *textin, CFDataRef *dataRef)
{
    uint8_t 	*tmpbuf;
    uint8_t		c;
    int 		tmpbufpos = 0;
    int 		numeq = 0;
    int 		acc = 0;
    int 		cntr = 0;
    uint8_t		*textcur = (__typeof__(textcur))textin->v;
    int			len = textin->l;
    int 		i;

    tmpbuf = malloc(len);		// len of result will be less than encoded len
    if (tmpbuf == NULL) {
    	yyerror("memory error - could not allocate buffer for certificate reference conversion from base-64.");
    	return -1;
    }
    	
    for (i = 0; i < len; i++) {
        c = *(textcur++);
        if (c == '=')
            numeq++;
        else if (!isspace(c))
            numeq = 0;
        if (base64_DecodeTable[c] < 0)
            continue;
        cntr++;
        acc <<= 6;
        acc += base64_DecodeTable[c];
        if (0 == (cntr & 0x3)) {
            tmpbuf[tmpbufpos++] = (acc >> 16) & 0xff;
            if (numeq < 2)
                tmpbuf[tmpbufpos++] = (acc >> 8) & 0xff;
            if (numeq < 1)
                tmpbuf[tmpbufpos++] = acc & 0xff;
        }
    }
    *dataRef = CFDataCreate(NULL, tmpbuf, tmpbufpos);
    free(tmpbuf);
	if (*dataRef)
		return 0;
	else
		return -1;
  
}

