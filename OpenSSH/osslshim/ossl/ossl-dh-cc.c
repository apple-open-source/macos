/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#include "ossl-config.h"

#include <stdio.h>
#include <stdlib.h>

#include "rfc2459_asn1.h"

#include "ossl-dh.h"
#include "ossl-common.h"

#ifdef HAVE_COMMONCRYPTO_COMMONDH_H


/* XXX <rdar://problem/10771188> is blocking this code from working */
#ifdef PR_10771188_FIXED
#include <CommonCrypto/CommonDH.h>
#include <CommonCrypto/CommonBigNum.h>

/*
 * XXX we need the following structs from CommonDHPriv.h
 * given the missing bits in the CommonDH SPI.  See the
 * other XXX's below for the details.
 */
typedef struct DHParms_ {
	CCBigNumRef	p;
	CCBigNumRef	g;
	uint32_t	l;
} DHParmSet;

typedef struct DH_ {
	DHParmSet	parms;
	CCBigNumRef	pub_key;
	CCBigNumRef	priv_key;
} *CCDH;

/*
 *
 */
static /* CCHRef */ CCDH
loadkeys(DH *dh)
{
	void *p = NULL, *g = NULL, *priv_key = NULL, *pub_key = NULL;
	int plen, glen, priv_keylen, pub_keylen;
	/* CCHRef */ CCDH ccdh = NULL;
	CCDHParameters dhparams = NULL;

	plen = BN_num_bytes(dh->p);
	glen = BN_num_bytes(dh->g);

	p = malloc(plen);
	g = malloc(glen);
	if ((NULL == p) || (NULL == g)) {
		goto err;
	}

	if (!BN_bn2bin(dh->p, (unsigned char *)p)) {
		goto err;
	}
	if (!BN_bn2bin(dh->g, (unsigned char *)g)) {
		goto err;
	}

	dhparams = CCDHCreateParametersFromData(p, plen, g, glen);
	if (NULL == dhparams) {
		goto err;
	}

	ccdh = (CCDH)CCDHCreate(dhparams);
	if (NULL == ccdh) {
		goto err;
	}

	/*
	 * XXX doesn't properly initialize priv_key and pub_key.
	 * See <rdar://problem/10771223> for more information.
	 * The following is the workaround for now.
	 */
	ccdh->pub_key = NULL;
	ccdh->priv_key = NULL;

	/* XXX there is no SPI call to use a provided private key
	 * See <rdar://problem/10771283> for more info.
	 */
	if (NULL != dh->priv_key) {
		CCStatus bnstatus;

		/* we have a private key provided */
		priv_keylen = BN_num_bytes(dh->priv_key);
		priv_key = malloc(priv_keylen);
		if (NULL == priv_key) {
			goto err;
		}
		if (!BN_bn2bin(dh->priv_key, (unsigned char *)priv_key)) {
			goto err;
		}
		if ((ccdh->priv_key = CCBigNumFromData(&bnstatus,
		    priv_key, priv_keylen)) == NULL) {
			goto err;
		}
	}

	/* XXX there is no SPI call to use a provided public key
	 * See <rdar://problem/10771283> for more info.
	 */
	if (NULL != dh->pub_key) {
		CCStatus bnstatus;

		/* we have a private key provided */
		pub_keylen = BN_num_bytes(dh->pub_key);
		pub_key = malloc(pub_keylen);
		if (NULL == pub_key) {
			goto err;
		}
		if (!BN_bn2bin(dh->pub_key, (unsigned char *)pub_key)) {
			goto err;
		}
		if ((ccdh->pub_key = CCBigNumFromData(&bnstatus,
		    pub_key, pub_keylen)) == NULL) {
			goto err;
		}
	}

err:
	if (p) {
		memset(p, 0, plen);
		free(p);
	}
	if (g) {
		memset(g, 0, glen);
		free(g);
	}
	if (priv_key) {
		memset(g, 0, priv_keylen);
		free(priv_key);
	}
	if (pub_key) {
		memset(g, 0, pub_keylen);
		free(pub_key);
	}
	if (dhparams) {
		CCDHParametersRelease(dhparams);
	}

	return (ccdh);
}


static int
cc_dh_generate_key(DH *dh)
{
	/* CCDHRef */
	CCDH ccdh = NULL;
	size_t pub_keylen;
	void *pub_key;
	int rv = 0;

	if ((NULL == dh->p) || (NULL == dh->g)) {
		return (0);
	}

	if ((ccdh = loadkeys(dh)) == NULL) {
		goto err;
	}

	/* XXX header doc for CCDHGenerateKey is wrong.
	 * output is bin data not BigNUm.
	 * *outputLength = CCBigNumToData(&bnRetval, dh->pub_key, output);
	 * See <rdar://problem/10771260>
	 */
	if (CCDHGenerateKey(ccdh, pub_key, &pub_keylen) != 0) {
		goto err;
	}

	if ((dh->pub_key = BN_bin2bn(pub_key, (int)pub_keylen, dh->pub_key))
	    == NULL) {
		goto err;
	}

	rv = 1;

err:
	if (ccdh) {
		/*
		 * XXX CCDHRelease() doesn't free pub/priv key.
		 * See <rdar://problem/10771265> for more info.
		 */
		if (ccdh->priv_key) {
			CCBigNumFree(ccdh->priv_key);
		}
		if (ccdh->pub_key) {
			CCBigNumFree(ccdh->pub_key);
		}

		CCDHRelease((CCDHRef)ccdh);
	}

	return (rv);
}


static int
cc_dh_compute_key(unsigned char *shared, const BIGNUM *peer, DH *dh)
{
	/* CCDHRef */
	CCDH ccdh = NULL;
	void *peer_key = NULL;
	int rv = -1, peer_keylen;


	if ((NULL == dh->priv_key) || (NULL == dh->pub_key)) {
		return (-1);
	}

	if ((ccdh = loadkeys(dh)) == NULL) {
		goto err;
	}

	peer_keylen = BN_num_bytes(peer);
	if ((peer_key = malloc(peer_keylen)) == NULL) {
		goto err;
	}
	if (!BN_bn2bin(peer, (unsigned char *)peer_key)) {
		goto err;
	}

	if ((rv = CCDHComputeKey(shared, peer_key, (size_t)peer_keylen,
	    (CCDHRef)ccdh)) == -1) {
		goto err;
	}

err:
	if (ccdh) {
		/* XXX CCDHRelease() doesn't free pub/priv key */
		if (ccdh->priv_key) {
			CCBigNumFree(ccdh->priv_key);
		}
		if (ccdh->pub_key) {
			CCBigNumFree(ccdh->pub_key);
		}

		CCDHRelease((CCDHRef)ccdh);
	}

	return (rv);
}


#else /* ! PR_10771188_FIXED */

/*
 * Given that CC's implementation of DH doesn't work we use libtommath.
 * Note that this requires the libtommath lib to be linked in.
 */
#include <tommath.h>

static void
BN2mpz(mp_int *s, const BIGNUM *bn)
{
	size_t len;
	void *p;

	len = BN_num_bytes(bn);
	p = malloc(len);
	BN_bn2bin(bn, p);
	mp_read_unsigned_bin(s, p, len);
	/* s = (mp_int *)CCBigNumFromData(&status, p, len); */
	free(p);
}


static BIGNUM *
mpz2BN(mp_int *s)
{
	size_t size;
	BIGNUM *bn;
	void *p;

	size = mp_unsigned_bin_size(s);
	p = malloc(size);
	if ((p == NULL) && (size != 0)) {
		return (NULL);
	}
	mp_to_unsigned_bin(s, p);
	/* (void)CCBigNumToData(&status, (CCBigNumRef)s, p); */

	bn = BN_bin2bn(p, size, NULL);
	free(p);
	return (bn);
}


/*
 *
 */

#define DH_NUM_TRIES    10

static int
cc_dh_generate_key(DH *dh)
{
	mp_int pub, priv_key, g, p;
	int have_private_key = (dh->priv_key != NULL);
	int codes, times = 0;
	int res;

	if ((dh->p == NULL) || (dh->g == NULL)) {
		return (0);
	}

	mp_init_multi(&pub, &priv_key, &g, &p, NULL);

	while (times++ < DH_NUM_TRIES) {
		if (!have_private_key) {
			size_t bits = BN_num_bits(dh->p);

			if (dh->priv_key) {
				BN_free(dh->priv_key);
			}

			dh->priv_key = BN_new();
			if (dh->priv_key == NULL) {
				return (0);
			}
			if (!BN_rand(dh->priv_key, bits /* - 1 */, 0, 0)) {
				BN_clear_free(dh->priv_key);
				dh->priv_key = NULL;
				return (0);
			}
		}
		if (dh->pub_key) {
			BN_free(dh->pub_key);
		}

		/* mp_init_multi(&pub, &priv_key, &g, &p, NULL); */

		BN2mpz(&g, dh->g);
		BN2mpz(&priv_key, dh->priv_key);
		BN2mpz(&p, dh->p);

		res = mp_exptmod(&g, &priv_key, &p, &pub);

		mp_clear_multi(&priv_key, &g, &p, NULL);
		if (res != 0) {
			continue;
		}

		dh->pub_key = mpz2BN(&pub);
		mp_clear(&pub);
		if (dh->pub_key == NULL) {
			return (0);
		}

		if (DH_check_pubkey(dh, dh->pub_key, &codes) && (codes == 0)) {
			break;
		}
		if (have_private_key) {
			return (0);
		}
	}

	if (times >= DH_NUM_TRIES) {
		if (!have_private_key && dh->priv_key) {
			BN_free(dh->priv_key);
			dh->priv_key = NULL;
		}
		if (dh->pub_key) {
			BN_free(dh->pub_key);
			dh->pub_key = NULL;
		}
		return (0);
	}

	return (1);
}


#define mp_isneg(a)    (((a)->sign) ? 1 : 0)

static int
cc_dh_compute_key(unsigned char *shared, const BIGNUM *pub, DH *dh)
{
	mp_int s, priv_key, p, peer_pub;
	int ret;

	if ((dh->pub_key == NULL) || (dh->g == NULL) || (dh->priv_key == NULL)) {
		return (-1);
	}

	mp_init_multi(&s, &priv_key, &p, &peer_pub, NULL);

	BN2mpz(&p, dh->p);
	BN2mpz(&peer_pub, pub);

	/* check if peers pubkey is reasonable */
	if (mp_isneg(&peer_pub) ||
	    (mp_cmp(&peer_pub, &p) >= 0) ||
	    (mp_cmp_d(&peer_pub, 1) <= 0)) {
		ret = -1;
		goto out;
	}

	BN2mpz(&priv_key, dh->priv_key);

	ret = mp_exptmod(&peer_pub, &priv_key, &p, &s);
	if (ret != 0) {
		ret = -1;
		goto out;
	}

	ret = mp_unsigned_bin_size(&s);
	mp_to_unsigned_bin(&s, shared);

out:
	mp_clear_multi(&s, &priv_key, &p, &peer_pub, NULL);

	return (ret);
}


#endif /* ! PR_10771188_FIXED */

static int
cc_dh_generate_params(DH *dh, int a, int b, BN_GENCB *callback)
{
	/* groups should already be known, we don't care about this */
	return (0);
}


static int
cc_dh_init(DH *dh)
{
	/* set flags */
	return (1);
}


static int
cc_dh_finish(DH *dh)
{
	/* free resources */
	return (1);
}


/*
 *
 */

const DH_METHOD _ossl_dh_cc_method =
{
	.name			= "CommonCrypto DH",
	.generate_key		= cc_dh_generate_key,
	.compute_key		= cc_dh_compute_key,
	.bn_mod_exp		= NULL,
	.init			= cc_dh_init,
	.finish			= cc_dh_finish,
	.flags			=	    0,
	.app_data		= NULL,
	.generate_params	= cc_dh_generate_params
};

#endif /* HAVE_COMMONCRYPTO_COMMONDH_H */

/**
 * DH implementation using cdsa.
 *
 * @return the DH_METHOD for the DH implementation using CommonCrypto.
 */
const DH_METHOD *
DH_cc_method(void)
{
#ifdef HAVE_COMMONCRYPTO_COMMONDH_H
	return (&_ossl_dh_cc_method);

#else
	return (NULL);
#endif  /* HAVE_COMMONCRYPTO_COMMONDH_H */
}
