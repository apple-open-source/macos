/*
 * Copyright (c) 2000-2001, Boris Popov
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: smb_crypt.c,v 1.13.108.1 2005/07/20 05:27:00 lindak Exp $
 */
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/md5.h>

#include <sys/smb_apple.h>
#include <sys/utfconv.h>

#include <sys/smb_iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>
#include <netsmb/md4.h>


#include <crypto/des.h>

static u_char N8[] = {0x4b, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25};


static void
smb_E(const u_char *key, u_char *data, u_char *dest)
{
	des_key_schedule *ksp;
	u_char kk[8];

	kk[0] = key[0] & 0xfe;
	kk[1] = key[0] << 7 | (key[1] >> 1 & 0xfe);
	kk[2] = key[1] << 6 | (key[2] >> 2 & 0xfe);
	kk[3] = key[2] << 5 | (key[3] >> 3 & 0xfe);
	kk[4] = key[3] << 4 | (key[4] >> 4 & 0xfe);
	kk[5] = key[4] << 3 | (key[5] >> 5 & 0xfe);
	kk[6] = key[5] << 2 | (key[6] >> 6 & 0xfe);
	kk[7] = key[6] << 1;
	ksp = malloc(sizeof(des_key_schedule), M_SMBTEMP, M_WAITOK);
	des_set_key((des_cblock*)kk, *ksp);
	des_ecb_encrypt((des_cblock*)data, (des_cblock*)dest, *ksp, 1);
	free(ksp, M_SMBTEMP);
}

/*
 * Compute an LM response given the ASCII password and a challenge.
 */
PRIVSYM int
smb_lmresponse(const u_char *apwd, u_char *C8, u_char *RN)
{
	u_char *p, *P14, *S21;

	p = malloc(14 + 21, M_SMBTEMP, M_WAITOK);
	bzero(p, 14 + 21);
	P14 = p;
	S21 = p + 14;
	bcopy(apwd, P14, min(14, strlen((char *)apwd)));
	/*
	 * S21 = concat(Ex(P14, N8), zeros(5));
	 */
	smb_E(P14, N8, S21);
	smb_E(P14 + 7, N8, S21 + 8);

	smb_E(S21, C8, RN);
	smb_E(S21 + 7, C8, RN + 8);
	smb_E(S21 + 14, C8, RN + 16);
	free(p, M_SMBTEMP);
	return 0;
}

/*
 * Compute the NTLMv1 hash, which is used to compute both NTLMv1 and
 * NTLMv2 responses.
 */
static void
smb_ntlmv1hash(const u_char *apwd, u_char *v1hash)
{
	u_int16_t *unipwd;
	MD4_CTX *ctxp;
	size_t alen, unilen;

	alen = strlen((char *)apwd);
	unipwd = malloc(alen * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
	/*
	 * v1hash = concat(MD4(U(apwd)), zeros(5));
	 */
	unilen = smb_strtouni(unipwd, (char *)apwd, alen,
	    UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
	ctxp = malloc(sizeof(MD4_CTX), M_SMBTEMP, M_WAITOK);
	MD4Init(ctxp);
	MD4Update(ctxp, (u_char*)unipwd, unilen);
	free(unipwd, M_SMBTEMP);
	bzero(v1hash, 21);
	MD4Final(v1hash, ctxp);
	free(ctxp, M_SMBTEMP);
}

/*
 * Compute an NTLM response given the Unicode password (as an ASCII string,
 * not a Unicode string!) and a challenge.
 */
PRIVSYM int
smb_ntlmresponse(const u_char *apwd, u_char *C8, u_char *RN)
{
	u_char S21[21];

	smb_ntlmv1hash(apwd, S21);

	smb_E(S21, C8, RN);
	smb_E(S21 + 7, C8, RN + 8);
	smb_E(S21 + 14, C8, RN + 16);
	return 0;
}

static void
HMACT64(const u_char *key, size_t key_len, const u_char *data,
    size_t data_len, u_char *digest)
{
	MD5_CTX context;
	u_char k_ipad[64];	/* inner padding - key XORd with ipad */
	u_char k_opad[64];	/* outer padding - key XORd with opad */
	int i;

	/* if key is longer than 64 bytes use only the first 64 bytes */
	if (key_len > 64)
		key_len = 64;

	/*
	 * The HMAC-MD5 (and HMACT64) transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, data))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and data is the data being protected.
	 */

	/* start out by storing key in pads */
	bzero(k_ipad, sizeof k_ipad);
	bzero(k_opad, sizeof k_opad);
	bcopy(key, k_ipad, key_len);
	bcopy(key, k_opad, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/*
	 * perform inner MD5
	 */
	MD5Init(&context);			/* init context for 1st pass */
	MD5Update(&context, k_ipad, 64);	/* start with inner pad */
	MD5Update(&context, data, data_len);	/* then data of datagram */
	MD5Final(digest, &context);		/* finish up 1st pass */

	/*
	 * perform outer MD5
	 */
	MD5Init(&context);			/* init context for 2nd pass */
	MD5Update(&context, k_opad, 64);	/* start with outer pad */
	MD5Update(&context, digest, 16);	/* then results of 1st hash */
	MD5Final(digest, &context);		/* finish up 2nd pass */
}

/*
 * Compute an NTLMv2 response given the Unicode password (as an ASCII string,
 * not a Unicode string!), the user name, the destination workgroup/domain
 * name, a challenge, and the blob.
 */
PRIVSYM int
smb_ntlmv2response(const u_char *apwd, const u_char *user,
    const u_char *destination, u_char *C8, const u_char *blob,
    size_t bloblen, u_char **RN, size_t *RNlen)
{
	u_char v1hash[21];
	u_int16_t *uniuser, *unidest;
	size_t uniuserlen, unidestlen;
	u_char v2hash[16];
	int len;
	size_t datalen;
	u_char *data;
	size_t v2resplen;
	u_char *v2resp;

	smb_ntlmv1hash(apwd, v1hash);

	/*
	 * v2hash = HMACT64(v1hash, 16, concat(upcase(user), upcase(destination))
	 * We assume that user and destination are supplied to us as
	 * upper-case UTF-8.
	 */
	len = strlen((char *)user);
	uniuser = malloc(len * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
	uniuserlen = smb_strtouni(uniuser, (char *)user, len,
	    UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
	len = strlen((char *)destination);
	unidest = malloc(len * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
	unidestlen = smb_strtouni(unidest, (char *)destination, len,
	    UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
	datalen = uniuserlen + unidestlen;
	data = malloc(datalen, M_SMBTEMP, M_WAITOK);
	bcopy(uniuser, data, uniuserlen);
	bcopy(unidest, data + uniuserlen, unidestlen);
	free(uniuser, M_SMBTEMP);
	free(unidest, M_SMBTEMP);
	HMACT64(v1hash, 16, data, datalen, v2hash);
	free(data, M_SMBTEMP);

	datalen = 8 + bloblen;
	data = malloc(datalen, M_SMBTEMP, M_WAITOK);
	bcopy(C8, data, 8);
	bcopy(blob, data + 8, bloblen);
	v2resplen = 16 + bloblen;
	v2resp = malloc(v2resplen, M_SMBTEMP, M_WAITOK);
	HMACT64(v2hash, 16, data, datalen, v2resp);
	free(data, M_SMBTEMP);
	bcopy(blob, v2resp + 16, bloblen);
	*RN = v2resp;
	*RNlen = v2resplen;
	return 0;
}
