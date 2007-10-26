/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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
#include <libkern/crypto/md5.h>

#include <sys/smb_apple.h>
#include <sys/utfconv.h>

#include <sys/smb_iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
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

#define SSNDEBUG 0
#if SSNDEBUG
void
prblob(u_char *b, size_t len)
{
	while (len--)
		SMBDEBUG("%02x", *b++);
	SMBDEBUG("\n");
}
#endif

/*
 * Compute an NTLMv2 hash given the Unicode password (as an ASCII string,
 * not a Unicode string!), the user name, the destination workgroup/domain
 * name, and a challenge.
 */
PRIVSYM int
smb_ntlmv2hash(const u_char *apwd, const u_char *user,
    const u_char *destination, u_char *v2hash)
{
	u_char v1hash[21];
	u_int16_t *uniuser, *unidest;
	size_t uniuserlen, unidestlen;
	int len;
	size_t datalen;
	u_char *data;

	smb_ntlmv1hash(apwd, v1hash);
#if SSNDEBUG
        SMBDEBUG("v1hash = ");
        prblob(v1hash, 21);
#endif

	/*
	 * v2hash = HMACT64(v1hash, 16, concat(upcase(user), upcase(dest))
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
#if SSNDEBUG
        SMBDEBUG("v2hash = ");
        prblob(v2hash, 16);
#endif
	return (0);
}

/*
 * Compute an NTLMv2 response given the Unicode password (as an ASCII string,
 * not a Unicode string!), the user name, the destination workgroup/domain
 * name, a challenge, and the blob.
 */
PRIVSYM int
smb_ntlmv2response(u_char *v2hash, u_char *C8, const u_char *blob,
    size_t bloblen, u_char **RN, size_t *RNlen)
{
	size_t datalen;
	u_char *data;
	size_t v2resplen;
	u_char *v2resp;

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
#if SSNDEBUG
        SMBDEBUG("v2resp = ");
        prblob(v2resp, v2resplen);
#endif
	return (0);
}

/*
 * Calculate NTLMv2 message authentication code (MAC) key for virtual circuit.
 */      
PRIVSYM int      
smb_calcv2mackey(struct smb_vc *vcp, u_char *v2hash, u_char *ntresp,
		 size_t resplen)
{       
	u_char sesskey[16];

	if (!(vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE))
		return (0);
	SMBWARNING("(fyi) packet signing with ntlmv2\n");

	if (vcp->vc_mackey != NULL) {
		free(vcp->vc_mackey, M_SMBTEMP);
		vcp->vc_mackey = NULL;
		vcp->vc_mackeylen = 0;
		vcp->vc_seqno = 0;
	}

	/* session key uses only 1st 16 bytes of ntresp */
	HMACT64(v2hash, 16, ntresp, (size_t) 16, sesskey);

	/*
	 * The partial MAC key is the concatenation of the 16 byte session
	 * key and the NT response.
	 */
	vcp->vc_mackeylen = 16 + resplen;
	vcp->vc_mackey = malloc(vcp->vc_mackeylen, M_SMBTEMP, M_WAITOK);

	bcopy(sesskey, vcp->vc_mackey, 16);
	bcopy(ntresp, vcp->vc_mackey + 16, (int)resplen);
#if SSNDEBUG
	SMBDEBUG("setting vc_mackey=");
	prblob(vcp->vc_mackey, vcp->vc_mackeylen);
#endif

	return(0);
}

/*
 * Calculate message authentication code (MAC) key for virtual circuit.
 */      
PRIVSYM int      
smb_calcmackey(struct smb_vc *vcp)
{       
	const char *pwd;
	u_int16_t *unipwd;
	int len;
	MD4_CTX md4;
	u_char S16[16], S21[21];

	if (!(vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE))
		return (0);
	SMBWARNING("(fyi) packet signing with ntlm(v1)\n");

	if (vcp->vc_mackey != NULL) {
		free(vcp->vc_mackey, M_SMBTEMP);
		vcp->vc_mackey = NULL;
		vcp->vc_mackeylen = 0;
		vcp->vc_seqno = 0;
	}

	/*
	 * The partial MAC key is the concatenation of the 16 byte session
	 * key and the 24 byte challenge response.
	 */
	vcp->vc_mackeylen = 16 + 24;
	vcp->vc_mackey = malloc(vcp->vc_mackeylen, M_SMBTEMP, M_WAITOK);

	/*
	 * Calculate session key:
	 *      MD4(MD4(U(PN)))
	 */
	pwd = smb_vc_getpass(vcp);
	len = strlen(pwd);
	unipwd = malloc((len + 1) * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
	smb_strtouni(unipwd, pwd, 0, UTF_PRECOMPOSED);
	MD4Init(&md4);
	MD4Update(&md4, (u_char *)unipwd, len * sizeof(u_int16_t));
	MD4Final(S16, &md4);
	MD4Init(&md4);
	MD4Update(&md4, S16, 16);
	MD4Final(vcp->vc_mackey, &md4);
	free(unipwd, M_SMBTEMP);
	
	/* 
	 * Calculate response to challenge:
	 *      Ex(concat(MD4(U(pass)), zeros(5)), C8)
	 */
	bzero(S21, 21); 
	bcopy(S16, S21, 16);
	smb_E(S21, vcp->vc_ch, vcp->vc_mackey + 16);
	smb_E(S21 + 7, vcp->vc_ch, vcp->vc_mackey + 24);
	smb_E(S21 + 14, vcp->vc_ch, vcp->vc_mackey + 32);
#if SSNDEBUG
	SMBDEBUG("setting vc_mackey=");
	prblob(vcp->vc_mackey, vcp->vc_mackeylen);
#endif
	return (0);
}

/* 
 * Sign request with MAC.
 */
int
smb_rq_sign(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct mbchain *mbp;
	mbuf_t mb;
	MD5_CTX md5;
	u_char digest[16];
	
	KASSERT(vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE,
	    ("signatures not enabled"));
	
	if (vcp->vc_mackey == NULL) {
		/* Signing is required, but we have no key yet fill in with the magic fake signing value */
		rqp->sr_seqno = vcp->vc_seqno++;
		rqp->sr_rseqno = vcp->vc_seqno++;
		*(u_int32_t *)rqp->sr_rqsig = htolel(rqp->sr_seqno);
		*(u_int32_t *)(rqp->sr_rqsig + 4) = 0;
		smb_rq_getrequest(rqp, &mbp);
		bcopy("BSRSPLY", rqp->sr_rqsig, 8);
		return (0);
	}
	
	/* 
	 * This is a bit of a kludge. If the request is non-TRANSACTION,
	 * or it is the first request of a transaction, give it the next
	 * sequence number, and expect the reply to have the sequence number
	 * following that one. Otherwise, it is a secondary request in
	 * a transaction, and it gets the same sequence numbers as the
	 * primary request.
	 */
	if (rqp->sr_t2 == NULL || 
	    (rqp->sr_t2->t2_flags & SMBT2_SECONDARY) == 0) {
		rqp->sr_seqno = vcp->vc_seqno++;
		rqp->sr_rseqno = vcp->vc_seqno++;
	} else {
		/* 
		 * Sequence numbers are already in the struct because
		 * smb_t2_request_int() uses the same one for all the
		 * requests in the transaction.
		 * (At least we hope so.)
		 */
		KASSERT(rqp->sr_t2 == NULL ||
		    (rqp->sr_t2->t2_flags & SMBT2_SECONDARY) == 0 ||
		    rqp->sr_t2->t2_rq == rqp,
		    ("sec t2 rq not using same smb_rq"));
	}
	
	/* Initialize sec. signature field to sequence number + zeros. */
	*(u_int32_t *)rqp->sr_rqsig = htolel(rqp->sr_seqno);
	*(u_int32_t *)(rqp->sr_rqsig + 4) = 0;
	
	/* 
	 * Compute HMAC-MD5 of packet data, keyed by MAC key.
	 * Store the first 8 bytes in the sec. signature field.
	 */
	smb_rq_getrequest(rqp, &mbp);
	MD5Init(&md5);
	MD5Update(&md5, vcp->vc_mackey, vcp->vc_mackeylen);
	for (mb = mbp->mb_top; mb != NULL; mb = mbuf_next(mb))
		MD5Update(&md5, mbuf_data(mb), mbuf_len(mb));
	MD5Final(digest, &md5);
	bcopy(digest, rqp->sr_rqsig, 8);
	
	return (0);
}

#define SMBSIGLEN (8)
#define SMBSIGOFF (14)
#define SMBPASTSIG (SMBSIGOFF + SMBSIGLEN)

static int
smb_verify(struct smb_rq *rqp, u_int32_t seqno)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct mdchain *mdp;
	mbuf_t mb;
	u_char sigbuf[SMBSIGLEN];
	MD5_CTX md5;
	u_char digest[16];

	/*
	 * Compute HMAC-MD5 of packet data, keyed by MAC key.
	 * We play games to pretend the security signature field
	 * contains their sequence number, to avoid modifying
	 * the packet itself.
	 */
	smb_rq_getreply(rqp, &mdp);
	mb = mdp->md_top;
	KASSERT(mbuf_len(mb) >= SMB_HDRLEN, ("forgot to mbuf_pullup"));
	MD5Init(&md5);
	MD5Update(&md5, vcp->vc_mackey, vcp->vc_mackeylen);
	MD5Update(&md5, mbuf_data(mb), SMBSIGOFF);
	*(u_int32_t *)sigbuf = htolel(seqno);
	*(u_int32_t *)(sigbuf + 4) = 0;
	MD5Update(&md5, sigbuf, SMBSIGLEN);
	MD5Update(&md5, mbuf_data(mb) + SMBPASTSIG,
		  mbuf_len(mb) - SMBPASTSIG);
	for (mb = mbuf_next(mb); mb != NULL; mb = mbuf_next(mb))
		if (mbuf_len(mb))
			MD5Update(&md5, mbuf_data(mb), mbuf_len(mb));
	MD5Final(digest, &md5);
	/*
	 * Finally, verify the signature.
	 */
	return (bcmp(mbuf_data(mdp->md_top) + SMBSIGOFF, digest,
		     SMBSIGLEN));
}

/* 
 * Verify reply signature.
 */
int
smb_rq_verify(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	int32_t fudge;
#define SMBFUDGESIGN 4

	if (!(vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE)) {
		SMBWARNING("signatures not enabled!\n");
		return (0);
	}
	/*
	 * note vc_mackey gets initialized by smb_smb_ssnsetup.
	 */
	if (vcp->vc_mackey == NULL)
		return (0);

	if (smb_verify(rqp, rqp->sr_rseqno) == 0)
		return (0);

	/*
	 * now for diag purposes me check whether the client/server idea
	 * of the sequence # has gotten a bit out of sync.
	 */
	for (fudge = -SMBFUDGESIGN; fudge <= SMBFUDGESIGN; fudge++)
		if (fudge == 0)
			continue;
		else if (smb_verify(rqp, rqp->sr_rseqno + fudge) == 0)
			break;

	if (fudge <= SMBFUDGESIGN) {
		SMBDEBUG("sr_rseqno=%d, but %d would have worked\n",
			 rqp->sr_rseqno, rqp->sr_rseqno + fudge);
	} else
#if !SSNDEBUG
		if (vcp->vc_username[0] != '\0') /* not anon? */
#endif
			SMBDEBUG("sr_rseqno=%d\n", rqp->sr_rseqno);
	/*
	 * Verification fails with anonymous logins, though our vc_mackey
	 * is aok for outbound signing.  XXX
	 */
#if !SSNDEBUG
	if (vcp->vc_username[0] != '\0') /* not anon? */
		return (EAUTH);
#endif
	return (0);
}
