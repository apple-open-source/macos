/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2008 Apple Inc. All rights reserved.
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

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_dev.h>
#include <netsmb/md4.h>

#include <fs/smbfs/smbfs_subr.h>
#include <netsmb/smb_converter.h>

#include <crypto/des.h>


#define SMBSIGLEN (8)
#define SMBSIGOFF (14)
#define SMBPASTSIG (SMBSIGOFF + SMBSIGLEN)
#define SMBFUDGESIGN 4

#ifdef SMB_DEBUG
/* Need to build with SMB_DEBUG if you what to turn this on */
#define SSNDEBUG 0
#endif // SMB_DEBUG

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
int smb_lmresponse(const u_char *apwd, u_char *C8, u_char *RN)
{
	u_char *p, *P14, *S21;

	p = malloc(14 + 21, M_SMBTEMP, M_WAITOK);
	bzero(p, 14 + 21);
	P14 = p;
	S21 = p + 14;
	bcopy(apwd, P14, MIN(14, strnlen((char *)apwd, SMB_MAXPASSWORDLEN + 1)));
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
 * smb_ntlmhash
 *
 * Compute the NTLM hash of the given password, which is used in the calculation of 
 * the NTLM Response. Used for both the NTLMv2 and LMv2 hashes.
 *
 */
static void smb_ntlmhash(const uint8_t *passwd, uint8_t *ntlmHash, size_t ntlmHash_len)
{
	u_int16_t *unicode_passwd = NULL;
	MD4_CTX md4;
	size_t len;
	
	bzero(ntlmHash, ntlmHash_len);
	len = strnlen((char *)passwd, SMB_MAXPASSWORDLEN + 1);
	unicode_passwd = malloc(len * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
	if (unicode_passwd == NULL)	/* Should never happen, but better safe than sorry */
		return;
	len = smb_strtouni(unicode_passwd, (char *)passwd, len,  UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
	bzero(&md4, sizeof(md4)); 
	MD4Init(&md4);
	MD4Update(&md4, (uint8_t *)unicode_passwd, (unsigned int)len);
	MD4Final(ntlmHash, &md4);
	free(unicode_passwd, M_SMBTEMP);
#ifdef SSNDEBUG
	smb_hexdump(__FUNCTION__, "ntlmHash = ", ntlmHash, 16);
#endif // SSNDEBUG
}

/*
 * Compute an NTLM response given the Unicode password (as an ASCII string,
 * not a Unicode string!) and a challenge.
 */
int smb_ntlmresponse(const u_char *apwd, u_char *C8, u_char *RN)
{
	u_char S21[SMB_NTLM_LEN];

	smb_ntlmhash(apwd, S21, sizeof(S21));

	smb_E(S21, C8, RN);
	smb_E(S21 + 7, C8, RN + 8);
	smb_E(S21 + 14, C8, RN + 16);
	return 0;
}

static void HMACT64(const u_char *key, size_t key_len, const u_char *data, 
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
	MD5Update(&context, data, (unsigned int)data_len);	/* then data of datagram */
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
 * smb_ntlmv2hash
 *
 * Compute the NTLMv2 hash given the user's password, name and the domain.
 *
 *	1. The NTLM password hash is obtained, this is the MD4 digest of the Unicode mixed-case password.
 *	2. The Unicode uppercase username is concatenated with the Unicode authentication
 *		domain name. Note that this calculation always uses the Unicode representation, 
 *		even if OEM encoding has been negotiated; also note that the username is converted 
 *		to uppercase, while the authentication domain is case-sensitive. The HMAC-MD5 message authentication code 
 *		algorithm (described in RFC 2104) is applied to this value using the 16-byte NTLM hash 
 *		as the key. This results in a 16-byte value - the NTLMv2 hash.
 *
 * NOTE:
 *		Need to figure out if domain should be the user selected domain or what was 
 *		obtain from the domain/server name specified in the Target Name field of
 *		the Type 3 message. The spec says to use Target Name, but from my test that 
 *		is not correct?
 *
 *
 */
void smb_ntlmv2hash(uint8_t *ntlmv2Hash, const void * domain, const void * user,
					const void * password)
{
	uint8_t ntlmHash[SMB_NTLM_LEN];
	u_int16_t *UserDomainUTF16 = NULL;
	char *UserDomain = NULL;
	size_t len;
	
	DBG_ASSERT(domain);
	DBG_ASSERT(user);
	DBG_ASSERT(password);
	/* Get the NTLM Hash */
	smb_ntlmhash(password, ntlmHash, sizeof(ntlmHash));
	
	len = strnlen(user, SMB_MAXUSERNAMELEN + 1) + strnlen(domain, SMB_MAXNetBIOSNAMELEN + 1);
	UserDomain = malloc(len + 1, M_SMBTEMP, M_WAITOK);
	if (UserDomain == NULL)	/* Should never happen, but better safe than sorry */
		goto done;
	strlcpy(UserDomain, user, len + 1);
	strncat(UserDomain, domain, SMB_MAXNetBIOSNAMELEN + 1);
	SMBDEBUG("UserDomain = %s\n", UserDomain);
	
	UserDomainUTF16 = malloc(len * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
	if (UserDomainUTF16 == NULL)	/* Should never happen, but better safe than sorry */
		goto done;
	len = smb_strtouni(UserDomainUTF16, UserDomain, len, UTF_PRECOMPOSED | UTF_NO_NULL_TERM);
	
	HMACT64(ntlmHash, SMB_NTLMV2_LEN, (const u_char *)UserDomainUTF16, len, ntlmv2Hash);
#ifdef SSNDEBUG
	smb_hexdump(__FUNCTION__, "ntlmv2Hash = ", ntlmv2Hash, SMB_NTLMV2_LEN);
#endif // SSNDEBUG
done:
	if (UserDomain)
		free(UserDomain, M_SMBTEMP);
	if (UserDomainUTF16)
		free(UserDomainUTF16, M_SMBTEMP);
}

/*
 * smb_lmv2_response
 *
 * Compute the LMv2 response, derived from the NTLMv2 hash, the server challenge,
 * and the client nonce.
 */
void *smb_lmv2_response(void *ntlmv2Hash, u_int64_t server_nonce, u_int64_t client_nonce, size_t *lmv2_len)
{
	void *lmv2;
	u_int64_t nonce[2];
	
	nonce[0] = server_nonce;
	nonce[1] = client_nonce;
	*lmv2_len = SMB_NTLMV2_LEN + sizeof(nonce);
	lmv2 = malloc(*lmv2_len, M_SMBTEMP, M_WAITOK);
	if (lmv2 == NULL)
		return NULL;
	HMACT64(ntlmv2Hash, SMB_NTLMV2_LEN, (const u_char *)&nonce, sizeof(nonce), lmv2);
	bcopy(&client_nonce, (u_int8_t *)lmv2 + SMB_NTLMV2_LEN, sizeof(client_nonce));
#ifdef SSNDEBUG
	smb_hexdump(__FUNCTION__, "lmv2 = ", lmv2, (int32_t)*lmv2_len);
#endif // SSNDEBUG
	*lmv2_len = SMB_LMV2_LEN;	/* We only send the first 24 bytes */ 
	return lmv2;
}

/*
 * smb_ntlmv2_response
 *
 * Compute the LMv2 response, derived from the NTLMv2 hash, the server challenge,
 * and the client nonce.
 */
void smb_ntlmv2_response(void *ntlmv2Hash, void *ntlmv2, size_t ntlmv2_len, u_int64_t server_nonce)
{
	uint8_t *blob = ntlmv2;
	size_t blob_len = ntlmv2_len;
	
	blob += 8;
	blob_len -= 8;
	bcopy((char *)&server_nonce, blob, sizeof(server_nonce));
	HMACT64(ntlmv2Hash, SMB_NTLMV2_LEN, blob, blob_len, ntlmv2);
}

/*
 * make_ntlmv2_blob
 */
uint8_t *make_ntlmv2_blob(u_int64_t client_nonce, void *target_info, u_int16_t target_len, size_t *blob_len)
{
	uint8_t *blob = NULL;
	struct ntlmv2_blobhdr *blobhdr;
	struct timespec now;
	u_int64_t timestamp;
	u_char *target_offset;
	
	/* We always make the buffer big enough to hold the HMAC */
	*blob_len = SMB_NTLMV2_LEN + sizeof(struct ntlmv2_blobhdr) + target_len;
	blob = malloc(*blob_len, M_SMBTEMP, M_WAITOK);
	if (blob == NULL) {
		*blob_len = 0;
		return NULL;
	}
	
	bzero(blob, *blob_len);
	/* Skip pass the HMAC */
	blobhdr = (struct ntlmv2_blobhdr *)(blob + SMB_NTLMV2_LEN);
	blobhdr->header = htolel(0x00000101);
	nanotime(&now);
	smb_time_local2NT(&now, 0, &timestamp, 0);
	blobhdr->timestamp = htoleq(timestamp);
	blobhdr->client_nonce = client_nonce;
	
	target_offset = (uint8_t *)&blobhdr->unknown1 + sizeof(blobhdr->unknown1);
	if (target_info)
		memcpy(target_offset, (void *)target_info, target_len);

#ifdef SSNDEBUG
	smb_hexdump(__FUNCTION__, "ntlmv2 blob = ", blob, (int32_t)*blob_len);
#endif // SSNDEBUG
	return (blob);
	
}

/*
 * Initialize the signing data, free the key and
 * set everything else to zero.
 */
void smb_reset_sig(struct smb_vc *vcp)
{
	if (vcp->vc_mackey != NULL)
		free(vcp->vc_mackey, M_SMBTEMP);
	vcp->vc_mackey = NULL;
	vcp->vc_mackeylen = 0;
	vcp->vc_seqno = 0;
}

/*
 * Calculate NTLMv2 message authentication code (MAC) key for virtual circuit.
 *
 * The NTLMv2 User Session Key
 * 
 * Used when the NTLMv2 response is sent. Calculation of this key is very 
 * similar to the LMv2 User Session Key:
 * 
 * 1. The NTLMv2 hash is obtained (as calculated previously).
 * 2. The NTLMv2 "blob" is obtained (as used in the NTLMv2 response).
 * 3. The challenge from the Type 2 message is concatenated with the blob. The 
 *	  HMAC-MD5 message authentication code algorithm is applied to this value 
 *    using the NTLMv2 hash as the key, resulting in a 16-byte output value.
 * 4. The HMAC-MD5 algorithm is applied to this value, again using the NTLMv2 
 *	  hash as the key. The resulting 16-byte value is the NTLMv2 User Session 
 *    Key.
 *
 * The NTLMv2 User Session Key is quite similar cryptographically to the LMv2 
 * User Session Key. It can be stated as the HMAC-MD5 digest of the first 16 
 * bytes of the NTLMv2 response (using the NTLMv2 hash as the key).
 */
void smb_calcv2mackey(struct smb_vc *vcp, void *ntlmv2Hash, void *ntlmv2, 
					  void *resp, size_t resplen)
{
	if (!(vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE))
		return;
	
	SMBDEBUG("(fyi) packet signing with ntlmv2\n");

	smb_reset_sig(vcp);
	
	vcp->vc_mackeylen = (u_int32_t)(16 + resplen);
	vcp->vc_mackey = malloc(vcp->vc_mackeylen, M_SMBTEMP, M_WAITOK);
	/* Should never happen, but better safe than sorry */
	if (vcp->vc_mackey == NULL) {
		SMBDEBUG("malloc of vc_mackey failed?\n");
		return;
	}
	/* session key uses only 1st 16 bytes of ntlmv2 blob */
	HMACT64(ntlmv2Hash, 16, ntlmv2, (size_t)16, vcp->vc_mackey);
	
	/* If we have a response concatenation it on to user session key */	
	if (resp)
		bcopy(resp, vcp->vc_mackey + 16, (int)resplen);
#if SSNDEBUG
	smb_hexdump(__FUNCTION__, "setting vc_mackey = ", vcp->vc_mackey, vcp->vc_mackeylen);
#endif
}

/*
 * Calculate NTLM message authentication code (MAC) key for virtual circuit.
 *
 * The NTLM User Session Key
 * 
 * This variant is used when the client sends the NTLM response. 
 * The calculation of the key is fairly straightforward:
 * 
 *	1. The NTLM hash is obtained (the MD4 digest of the Unicode mixed-case password, calculated previously).
 *	2. The MD4 message-digest algorithm is applied to the NTLM hash, resulting in a 16-byte value. This is 
 *		the NTLM User Session Key.
 *
 * The NTLM User Session Key is much improved over the LM User Session Key. The password space is larger 
 * (it is case-sensitive, rather than converting the password to uppercase); additionally, all password 
 * characters have input in the key generation. However, it is still only changed when the user changes 
 * his or her password; this makes offline attacks much easier.
 */      
void smb_calcmackey(struct smb_vc *vcp,  void *resp, size_t resplen)
{       
	MD4_CTX md4;
	u_char S16[16];

	if (!(vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE))
		return;
	
	SMBDEBUG("(fyi) packet signing with ntlm(v1)\n");

	smb_reset_sig(vcp);

	vcp->vc_mackeylen = (u_int32_t)(16 + resplen);
	vcp->vc_mackey = malloc(vcp->vc_mackeylen, M_SMBTEMP, M_WAITOK);
		/* Should never happen, but better safe than sorry */
	if (vcp->vc_mackey == NULL) {
		SMBDEBUG("malloc of vc_mackey failed?\n");
		return;
	}
	/* Calculate session key: MD4(MD4(U(PN))) */
	smb_ntlmhash((const uint8_t *)smb_vc_getpass(vcp), (uint8_t *)S16, sizeof(S16));
	MD4Init(&md4);
	MD4Update(&md4, S16, 16);
	MD4Final(vcp->vc_mackey, &md4);
	/* If we have a response concatenation it on to user session key */	
	if (resp)
		bcopy(resp, vcp->vc_mackey+16, resplen);
	
#if SSNDEBUG
	smb_hexdump(__FUNCTION__, "setting vc_mackey = ", vcp->vc_mackey, vcp->vc_mackeylen);
#endif
	return;
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
	
	/*
	 * Durring the authentication process we send a magic fake signing string, 
	 * because signing really doesn't start until the authentication process is
	 * complete. The sequence number counter starts once we send our authentication
	 * message and must be reset if authentication fails.
	 */
	if ((vcp->vc_mackey == NULL) || (rqp->sr_cmd == SMB_COM_SESSION_SETUP_ANDX)) {
		/* The NTLMSSP code will reset these once it has a mac key */
		rqp->sr_seqno = vcp->vc_seqno++;
		rqp->sr_rseqno = vcp->vc_seqno++;
		/* Should never be null, but just to be safe */ 
		if (rqp->sr_rqsig)
			bcopy("BSRSPLY ", rqp->sr_rqsig, 8);
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
	if (rqp->sr_rqsig) {
		*(u_int32_t *)rqp->sr_rqsig = htolel(rqp->sr_seqno);
		*(u_int32_t *)(rqp->sr_rqsig + 4) = 0;		
	}
	
	/* 
	 * Compute HMAC-MD5 of packet data, keyed by MAC key.
	 * Store the first 8 bytes in the sec. signature field.
	 */
	smb_rq_getrequest(rqp, &mbp);
	MD5Init(&md5);
	MD5Update(&md5, vcp->vc_mackey, vcp->vc_mackeylen);
	for (mb = mbp->mb_top; mb != NULL; mb = mbuf_next(mb))
		MD5Update(&md5, mbuf_data(mb), (unsigned int)mbuf_len(mb));
	MD5Final(digest, &md5);
	if (rqp->sr_rqsig)
		bcopy(digest, rqp->sr_rqsig, 8);
	
	return (0);
}

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
	MD5Update(&md5, (u_int8_t *)mbuf_data(mb) + SMBPASTSIG, 
			  (unsigned int)(mbuf_len(mb) - SMBPASTSIG));
	for (mb = mbuf_next(mb); mb != NULL; mb = mbuf_next(mb))
		if (mbuf_len(mb))
			MD5Update(&md5, mbuf_data(mb), (unsigned int)mbuf_len(mb));
	MD5Final(digest, &md5);
	/*
	 * Finally, verify the signature.
	 */
	return (bcmp((u_int8_t *)mbuf_data(mdp->md_top) + SMBSIGOFF, digest, SMBSIGLEN));
}

/* 
 * Verify reply signature.
 */
int
smb_rq_verify(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	int32_t fudge;

	if (!(vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE)) {
		SMBWARNING("signatures not enabled!\n");
		return (0);
	}
	
	/*
	 * Durring the authentication process we send a dummy signature, because 
	 * signing really doesn't start until the authentication process is 
	 * complete. The last authentication message return by the server can be 
	 * signed, but only if the authentication succeed. If an error is return 
	 * then the signing process will fail and if we tested for signing the wrong 
	 * error would be return. So durring the authentication process we no longer
	 * verify the signature. From my testing it looks like Windows and Samba 
	 * clients do the same thing.
	 */
	if ((vcp->vc_mackey == NULL) || (rqp->sr_cmd == SMB_COM_SESSION_SETUP_ANDX))
		return (0);

	/* Its an anonymous logins, signing is not supported */
	if ((vcp->vc_flags & SMBV_ANONYMOUS_ACCESS) == SMBV_ANONYMOUS_ACCESS)
		return (0);
		
	if (smb_verify(rqp, rqp->sr_rseqno) == 0)
		return (0);

	/*
	 * Now for diag purposes we check whether the client/server idea
	 * of the sequence # has gotten a bit out of sync. This only gets
	 * excute if debugging has been turned on.
	 */
	if (smbfs_loglevel) {
		for (fudge = -SMBFUDGESIGN; fudge <= SMBFUDGESIGN; fudge++)
			if (fudge == 0)
				continue;
			else if (smb_verify(rqp, rqp->sr_rseqno + fudge) == 0)
				break;
		
		if (fudge <= SMBFUDGESIGN)
			SMBERROR("sr_rseqno=%d, but %d would have worked\n", 
						rqp->sr_rseqno, rqp->sr_rseqno + fudge);
		else
			SMBERROR("sr_rseqno=%d\n", rqp->sr_rseqno);
	}
	
	return (EAUTH);
}
