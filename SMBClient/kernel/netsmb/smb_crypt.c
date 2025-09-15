/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2023 Apple Inc. All rights reserved.
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
#define CC_CHANGEFUNCTION_28544056_cccmac_init

#define COMPRESSION_PERFORMANCE 0

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
#include <compression/compression_kext.h>
#include <sys/kauth.h>

#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <smbfs/smbfs.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_dev.h>
#include <netsmb/md4.h>
#include <netsmb/smb_packets_2.h>

#include <smbfs/smbfs_subr.h>
#include <netsmb/smb_converter.h>
#include <smbclient/smbclient_internal.h>

#include <crypto/des.h>
#include <corecrypto/cchmac.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/cccmac.h>
#include <corecrypto/ccnistkdf.h>
#include <libkern/crypto/sha2.h>


#define SMBSIGLEN (8)
#define SMBSIGOFF (14)
#define SMBPASTSIG (SMBSIGOFF + SMBSIGLEN)
#define SMBFUDGESIGN 4

/* SMB 2/3 Signing defines */
#define SMB2SIGLEN (16)
#define SMB2SIGOFF (48)

#ifdef SMB_DEBUG
/* Need to build with SMB_DEBUG if you what to turn this on */
#define SSNDEBUG 0
#endif // SMB_DEBUG

/* 
 * Set USE_MALLOC to 1 if you want to malloc a buffer to use when the 
 * request/reply is smaller than HYBRID_MALLOC_SIZE. Then do the cryto 
 * functions on the buffer instead of in the mbufs
 */
#define USE_MALLOC 0
#define HYBRID_MALLOC_SIZE 1024

static int smb3_verify(struct smb_rq *rqp, struct mdchain *mdp,
                       uint32_t nextCmdOffset, uint8_t *signature);
static void smb3_sign(struct smb_rq *rqp);

static u_char N8[] = {0x4b, 0x47, 0x53, 0x21, 0x40, 0x23, 0x24, 0x25};

int smb2_compress_data(uint8_t *data_startp, uint32_t data_len,
                       uint8_t *compress_startp, uint32_t compress_len,
                       uint16_t algorithm, int compress_flag,
                       size_t *actual_len);


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
    SMB_MALLOC_TYPE(ksp, des_key_schedule, Z_WAITOK);
	des_set_key((des_cblock*)kk, *ksp);
	des_ecb_encrypt((des_cblock*)data, (des_cblock*)dest, *ksp, 1);
    
    if (ksp) {
        SMB_FREE_TYPE(des_key_schedule, ksp);
    }
}

/*
 * mbuf_get_nbytes
 *
 * Utility routine to fetch 'nBytes' bytes from an mbuf chain
 * into buffer 'buf' at offset 'offset'.  Returns number of
 * bytes copied into buffer, always 'nBytes' unless mbuf chain
 * was exhausted.
 */
static size_t
mbuf_get_nbytes(size_t nBytes, unsigned char *buf, size_t offset, mbuf_t *mb, size_t *mb_len, size_t *mb_off)
{
    size_t remain, cnt, need, off;
    
    remain = nBytes;
    off = offset;
    cnt = 0;
    
    if (!nBytes) {
        SMBERROR("Called with nBytes == 0\n");
        return 0;
    }
    
    if (*mb == NULL) {
     SMBERROR("Called with NULL mb\n");
        return 0;
    }
    
    while (remain) {
        if (!(*mb_len)) {
            /* Advance to next mbuf */
            *mb = mbuf_next(*mb);
            if (*mb == NULL) {
                break;
            }
            *mb_len = mbuf_len(*mb);
            *mb_off = 0;
        }
        
        need = remain;
        if (need >= *mb_len) {
            need = *mb_len;
        }
        
        memcpy(buf + off, (uint8_t *)(mbuf_data(*mb)) + *mb_off, need);
        
        remain -= need;
        cnt += need;
        off += need;
        *mb_off += need;
        *mb_len -= need;
    }
    
    return (cnt);
}

/*
 * Compute an LM response given the ASCII password and a challenge.
 */
int smb_lmresponse(const u_char *apwd, u_char *C8, u_char *RN)
{
	u_char *p, *P14, *S21;
    size_t p_allocsize = 14 + 21;
    SMB_MALLOC_DATA(p, p_allocsize, Z_WAITOK);
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
    
    if (p) {
        SMB_FREE_DATA(p, p_allocsize);
    }
    
	return 24; /* return the len */
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
	uint16_t *unicode_passwd = NULL;
	MD4_CTX md4;
	size_t len, unicode_passwd_allocsize;
	
	bzero(ntlmHash, ntlmHash_len);
	len = strnlen((char *)passwd, SMB_MAXPASSWORDLEN + 1);
    
    if (len == 0) {
        /* Empty password, but we still need to allocate a buffer to */
        /* encrypt two NULL bytes so we can compute the NTLM hash correctly. */
        len = 1;
    }
    
    unicode_passwd_allocsize = len * sizeof(uint16_t);
    SMB_MALLOC_DATA(unicode_passwd, unicode_passwd_allocsize, Z_WAITOK);
	if (unicode_passwd == NULL)	/* Should never happen, but better safe than sorry */
		return;
    
	len = smb_strtouni(unicode_passwd, (char *)passwd, len, UTF_PRECOMPOSED);
	bzero(&md4, sizeof(md4)); 
	MD4Init(&md4);
	MD4Update(&md4, (uint8_t *)unicode_passwd, (unsigned int)len);
	MD4Final(ntlmHash, &md4);
    
    if (unicode_passwd) {
        SMB_FREE_DATA(unicode_passwd, unicode_passwd_allocsize);
    }
    
#ifdef SSNDEBUG
	smb_hexdump(__FUNCTION__, "ntlmHash = ", ntlmHash, 16);
#endif // SSNDEBUG
}

/*
 * Compute an NTLM response given the Unicode password (as an ASCII string,
 * not a Unicode string!) and a challenge.
 */
void smb_ntlmresponse(const u_char *apwd, u_char *C8, u_char *RN)
{
	u_char S21[SMB_NTLM_LEN];

	smb_ntlmhash(apwd, S21, sizeof(S21));

	smb_E(S21, C8, RN);
	smb_E(S21 + 7, C8, RN + 8);
	smb_E(S21 + 14, C8, RN + 16);
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
    MD5Update(&context, data, (unsigned int) data_len);	/* then data of datagram */
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
 * Compute an NTLMv2 hash given the Unicode password (as an ASCII string,
 * not a Unicode string!), the user name, the destination workgroup/domain
 * name, and a challenge.
 */
int
smb_ntlmv2hash(const u_char *apwd, const u_char *user,
               const u_char *destination, u_char *v2hash)
{
    u_char v1hash[SMB_NTLM_LEN];
    u_int16_t *uniuser = NULL, *unidest = NULL;
    size_t uniuserlen, unidestlen, uniuser_allocsize, unidest_allocsize;
    size_t len;
    size_t datalen;
    u_char *data;

    smb_ntlmhash(apwd, v1hash, sizeof(v1hash));
    
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
    uniuser_allocsize = len * sizeof(u_int16_t);
    SMB_MALLOC_DATA(uniuser, uniuser_allocsize, Z_WAITOK);
    uniuserlen = smb_strtouni(uniuser, (char *)user, len,
                              UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
    
    len = strlen((char *)destination);
    unidest_allocsize = len * sizeof(u_int16_t);
    SMB_MALLOC_DATA(unidest, unidest_allocsize, Z_WAITOK);
    unidestlen = smb_strtouni(unidest, (char *)destination, len,
                              UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
    
    datalen = uniuserlen + unidestlen;
    SMB_MALLOC_DATA(data, datalen, Z_WAITOK);
    bcopy(uniuser, data, uniuserlen);
    bcopy(unidest, data + uniuserlen, unidestlen);
    
    if (uniuser) {
        SMB_FREE_DATA(uniuser, uniuser_allocsize);
    }
    
    if (unidest) {
        SMB_FREE_DATA(unidest, unidest_allocsize);
    }
    
    HMACT64(v1hash, 16, data, datalen, v2hash);
    
    if (data) {
        SMB_FREE_DATA(data, datalen);
    }
    
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
int
smb_ntlmv2response(u_char *v2hash, u_char *C8, const u_char *blob,
                   size_t bloblen, u_char **RN, size_t *RNlen)
{
    size_t datalen;
    u_char *data;
    size_t v2resplen;
    u_char *v2resp;

    datalen = 8 + bloblen;
    SMB_MALLOC_DATA(data, datalen, Z_WAITOK);
    bcopy(C8, data, 8);
    bcopy(blob, data + 8, bloblen);

    v2resplen = 16 + bloblen;
    SMB_MALLOC_DATA(v2resp, v2resplen, Z_WAITOK);
    HMACT64(v2hash, 16, data, datalen, v2resp);

    if (data) {
        SMB_FREE_DATA(data, datalen);
    }
    
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
 * Initialize the signing data, free the key and
 * set everything else to zero.
 */
void smb_reset_sig(struct smb_session *sessionp)
{
    if (sessionp->session_mackey != NULL) {
        // session_mackeylen gets changed, and full_session_mackeylen holds the malloc size for both session_mackey and full_session_mackey
        SMB_FREE_DATA(sessionp->session_mackey, sessionp->full_session_mackeylen);
    }
    
    sessionp->session_mackeylen = 0;
    
    if (sessionp->full_session_mackey != NULL) {
        SMB_FREE_DATA(sessionp->full_session_mackey, sessionp->full_session_mackeylen);
    }
    
    sessionp->full_session_mackeylen = 0;

    sessionp->session_smb3_signing_key_len = 0;
    sessionp->session_seqno = 0;
}

/* 
 * Sign request with MAC.
 */
int
smb_rq_sign(struct smb_rq *rqp)
{
	struct smb_session *sessionp = rqp->sr_session;
	struct mbchain *mbp;
	mbuf_t mb;
	MD5_CTX md5;
	u_char digest[16];
	
	SMB_ASSERT(sessionp->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE,
	    ("signatures not enabled"));
	
	/*
	 * Durring the authentication process we send a magic fake signing string, 
	 * because signing really doesn't start until the authentication process is
	 * complete. The sequence number counter starts once we send our authentication
	 * message and must be reset if authentication fails.
	 */
	if ((sessionp->session_mackey == NULL) || (rqp->sr_cmd == SMB_COM_SESSION_SETUP_ANDX)) {
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
		rqp->sr_seqno = sessionp->session_seqno++;
		rqp->sr_rseqno = sessionp->session_seqno++;
	} else {
		/* 
		 * Sequence numbers are already in the struct because
		 * smb_t2_request_int() uses the same one for all the
		 * requests in the transaction.
		 * (At least we hope so.)
		 */
		SMB_ASSERT(rqp->sr_t2 == NULL ||
		    (rqp->sr_t2->t2_flags & SMBT2_SECONDARY) == 0 ||
		    rqp->sr_t2->t2_rq == rqp,
		    ("sec t2 rq not using same smb_rq"));
	}
	
	/* Initialize sec. signature field to sequence number + zeros. */
	if (rqp->sr_rqsig) {
		*(uint32_t *)rqp->sr_rqsig = htolel(rqp->sr_seqno);
		*(uint32_t *)(rqp->sr_rqsig + 4) = 0;		
	}
	
	/* 
	 * Compute HMAC-MD5 of packet data, keyed by MAC key.
	 * Store the first 8 bytes in the sec. signature field.
	 */
	smb_rq_getrequest(rqp, &mbp);
	MD5Init(&md5);
	MD5Update(&md5, sessionp->session_mackey, sessionp->session_mackeylen);
	for (mb = mbp->mb_top; mb != NULL; mb = mbuf_next(mb))
		MD5Update(&md5, mbuf_data(mb), (unsigned int)mbuf_len(mb));
	MD5Final(digest, &md5);
	if (rqp->sr_rqsig)
		bcopy(digest, rqp->sr_rqsig, 8);
	
	return (0);
}

static int
smb_verify(struct smb_rq *rqp, uint32_t seqno)
{
	struct smb_session *sessionp = rqp->sr_session;
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
	SMB_ASSERT(mbuf_len(mb) >= SMB_HDRLEN, ("forgot to mbuf_pullup"));
	MD5Init(&md5);
	MD5Update(&md5, sessionp->session_mackey, sessionp->session_mackeylen);
	MD5Update(&md5, mbuf_data(mb), SMBSIGOFF);
	*(uint32_t *)sigbuf = htolel(seqno);
	*(uint32_t *)(sigbuf + 4) = 0;
	MD5Update(&md5, sigbuf, SMBSIGLEN);
	MD5Update(&md5, (uint8_t *)mbuf_data(mb) + SMBPASTSIG, 
			  (unsigned int)(mbuf_len(mb) - SMBPASTSIG));
	for (mb = mbuf_next(mb); mb != NULL; mb = mbuf_next(mb))
		if (mbuf_len(mb))
			MD5Update(&md5, mbuf_data(mb), (unsigned int)mbuf_len(mb));
	MD5Final(digest, &md5);
	/*
	 * Finally, verify the signature.
	 */
	return (cc_cmp_safe(SMBSIGLEN, (uint8_t *)mbuf_data(mdp->md_top) + SMBSIGOFF, digest));
}

/* 
 * Verify reply signature.
 */
int
smb_rq_verify(struct smb_rq *rqp)
{
	struct smb_session *sessionp = rqp->sr_session;
	int32_t fudge;

	if (!(sessionp->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE)) {
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
	if ((sessionp->session_mackey == NULL) || (rqp->sr_cmd == SMB_COM_SESSION_SETUP_ANDX))
		return (0);

	/* Its an anonymous logins, signing is not supported */
	if ((sessionp->session_flags & SMBV_ANONYMOUS_ACCESS) == SMBV_ANONYMOUS_ACCESS)
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

/*
 * SMB 2/3 Sign a single request with HMAC-SHA256
 */
static void smb2_sign(struct smb_rq *rqp)
{
    struct smb_session *sessionp = rqp->sr_session;
    struct mbchain *mbp;
    mbuf_t mb;
    const struct ccdigest_info *di = ccsha256_di();
    u_char *mac;
    size_t mac_allocsize;
    
    if (rqp->sr_rqsig == NULL) {
        SMBDEBUG("sr_rqsig was never allocated.\n");
        return;
    }
    
    if (di == NULL) {
        SMBERROR("ccsha256_di returned NULL digest_info\n");
        return;
    }
    
    /* make sure ccdigest_info size is reasonable (sha256 output len is 32 bytes) */
    if (di->output_size > 64) {
        SMBERROR("Unreasonable output size %lu\n", di->output_size);
        return;
    }
    
    mac_allocsize = di->output_size;
    SMB_MALLOC_DATA(mac, mac_allocsize, Z_WAITOK);
    if (mac == NULL) {
        SMBERROR("Out of memory\n");
        return;
    }
    
    bzero(mac, di->output_size);
    
    /* Initialize 16-byte security signature field to all zeros. */
    bzero(rqp->sr_rqsig, SMB2SIGLEN);
    
    /* Set flag to indicate this PDU is signed */
    *rqp->sr_flagsp |= htolel(SMB2_FLAGS_SIGNED);
    
    smb_rq_getrequest(rqp, &mbp);
    cchmac_di_decl(di, hc);
    cchmac_init(di, hc, sessionp->session_mackeylen, sessionp->session_mackey);
    
    for (mb = mbp->mb_top; mb != NULL; mb = mbuf_next(mb))
        cchmac_update(di, hc, mbuf_len(mb), mbuf_data(mb));
    cchmac_final(di, hc, mac);
    
    // Copy first 16 bytes of the HMAC hash into the signature field
    bcopy(mac, rqp->sr_rqsig, SMB2SIGLEN);
    
    if (mac) {
        SMB_FREE_DATA(mac, mac_allocsize);
    }
}

/*
 * SMB 2/3 Sign request or compound chain with HMAC-SHA256
 */
int
smb2_rq_sign(struct smb_rq *rqp)
{
	struct smb_session *sessionp;
    struct smb_rq *this_rqp;
    uint32_t do_smb3_sign = 0;
    
    if (rqp == NULL) {
        SMBDEBUG("Called with NULL rqp\n");
        return (EINVAL);
    }

    sessionp = rqp->sr_session;
    
    if (sessionp == NULL) {
        SMBERROR("sessionp is NULL\n");
        return  (EINVAL);
    }

    /* 
     * If we are supposed to sign, then fail if we do not have a
     * session key.
     */
    if (sessionp->session_mackey == NULL) {
        SMBERROR("No session key for signing.\n");
        return (EINVAL);
    }
    
    /* Check for SMB 3 signing */
	if (SMBV_SMB3_OR_LATER(sessionp)) {
        do_smb3_sign = 1;
    }

    SMB_LOG_KTRACE(SMB_DBG_RQ_SIGN | DBG_FUNC_START,
                   do_smb3_sign, 0, 0, 0, 0);

    this_rqp = rqp;
    while (this_rqp != NULL) {
        if (do_smb3_sign) {
            smb3_sign(this_rqp);
        } else {
            smb2_sign(this_rqp);
        }
        this_rqp = this_rqp->sr_next_rqp;
    }
     
    SMB_LOG_KTRACE(SMB_DBG_RQ_SIGN | DBG_FUNC_END,
                   0, 0, 0, 0, 0);
    return (0);
}

/*
 * SMB 2/3 Verify a reply with HMAC-SHA256
 */
static int smb2_verify(struct smb_rq *rqp, struct mdchain *mdp, uint32_t nextCmdOffset, uint8_t *signature)
{
    struct smb_session *sessionp = rqp->sr_session;
    mbuf_t mb, mb_temp;
    size_t mb_off, remaining, mb_len, sign_len, mb_total_len;
    u_char zero_buf[SMB2SIGLEN];
    int result;
    const struct ccdigest_info *di = ccsha256_di();
    u_char *mac;
    size_t length = 0, mac_allocsize = 0;
    
    if (sessionp == NULL) {
        SMBERROR("sessionp is NULL\n");
        return  (EINVAL);
    }
    
    if (di == NULL) {
        SMBERROR("NULL digest from sha256_di\n");
        return (EINVAL);
    }
    
    /* make sure ccdigest_info size is reasonable (sha256 output len is 32 bytes) */
    if (di->output_size > 64) {
        SMBERROR("Unreasonable output size %lu\n", di->output_size);
        return (EINVAL);
    }
    
    mac_allocsize = di->output_size;
    SMB_MALLOC_DATA(mac, mac_allocsize, Z_WAITOK);
    if (mac == NULL) {
        SMBERROR("Out of memory\n");
        return (ENOMEM);
    }
    
    mb = mdp->md_cur;
    mb_len = (size_t)mbuf_data(mb) + mbuf_len(mb) - (size_t)mdp->md_pos;
    mb_total_len = mbuf_len(mb);
    mb_off = mbuf_len(mb) - mb_len;
    
    /* sanity check */
    if (mb_off > mb_total_len) {
        SMBDEBUG("mb_off: %lu past end of mbuf, mbuf_len: %lu\n", mb_off, mb_total_len);
        if (mac) {
            SMB_FREE_DATA(mac, mac_allocsize);
        }
        return (EBADRPC);
    }
    
    remaining = nextCmdOffset;
    if (!remaining) {
        /*
         * We don't know the length of the reply because it's
         * not a compound reply, or the end of a compound reply.
         * So calculate total length of this reply.
         */
        remaining = mb_len; /* length in first mbuf */
        mb_temp = mbuf_next(mb);
        while (mb_temp) {
            remaining += mbuf_len(mb_temp);
            mb_temp = mbuf_next(mb_temp);
        }
    }

    /* sanity check */
    if (remaining < SMB2_HDRLEN) {
        /* should never happen, but we have to be very careful */
        SMBDEBUG("reply length: %lu too short\n", remaining);
        if (mac) {
            SMB_FREE_DATA(mac, mac_allocsize);
        }
        return (EBADRPC);
    }
    
    bzero(zero_buf, SMB2SIGLEN);
    bzero(mac, di->output_size);
    cchmac_di_decl(di, hc);
    cchmac_init(di, hc, sessionp->session_mackeylen, sessionp->session_mackey);
    
    /*
     * Sign the first SMB2SIGOFF (48) bytes of the reply (up to the signature 
     * field).
     */
    length = SMB2SIGOFF;
    while (length) {
        if (!mb_len) {
            mb = mbuf_next(mb);
            if (!mb) {
                SMBDEBUG("mbuf_next didn't return an mbuf\n");
                if (mac) {
                    SMB_FREE_DATA(mac, mac_allocsize);
                }
                return EBADRPC;
            }
            mb_len = mbuf_len(mb);
            mb_total_len = mbuf_len(mb);
            mb_off = 0;
        }
        
        /* sanity check */
        if (mb_off > mb_total_len) {
            SMBDEBUG("mb_off: %lu past end of mbuf, mbuf_len: %lu\n", mb_off, mb_total_len);
            if (mac) {
                SMB_FREE_DATA(mac, mac_allocsize);
            }
            return (EBADRPC);
        }
        
        /* Calculate length to sign for this pass */
        sign_len = length;
        if (sign_len > mb_len) {
            sign_len = mb_len;
        }
        
        /* Sign it */
        cchmac_update(di, hc, sign_len, (uint8_t *)mbuf_data(mb) + mb_off);
        
        mb_off += sign_len;
        mb_len -= sign_len;
        remaining -= sign_len;
        length -= sign_len;
    }
    
    /* Sign SMB2SIGLEN (16) zeros for the signature */
    cchmac_update(di, hc, SMB2SIGLEN, zero_buf);
    
    /* Skip over the SMB2SIGLEN (16) bytes of the received signature */
    length = SMB2SIGLEN;
    while (length) {
        if (!mb_len) {
            mb = mbuf_next(mb);
            if (!mb) {
                SMBDEBUG("mbuf_next didn't return an mbuf\n");
                if (mac) {
                    SMB_FREE_DATA(mac, mac_allocsize);
                }
                return EBADRPC;
            }
            mb_len = mbuf_len(mb);
            mb_total_len = mbuf_len(mb);
            mb_off = 0;
        }
        
        /* sanity check */
        if (mb_off > mb_total_len) {
            SMBDEBUG("mb_off: %lu past end of mbuf, mbuf_len: %lu\n", mb_off, mb_total_len);
            if (mac) {
                SMB_FREE_DATA(mac, mac_allocsize);
            }
            return (EBADRPC);
        }
        
        /* Calculate length to sign for this pass */
        sign_len = length;
        if (sign_len > mb_len) {
            sign_len = mb_len;
        }
        
        mb_off += sign_len;
        mb_len -= sign_len;
        remaining -= sign_len;
        length -= sign_len;
    }

    /* Sign remainder of this reply */
    while (remaining) {
        if (!mb_len) {
            mb = mbuf_next(mb);
            if (!mb) {
                SMBDEBUG("mbuf_next didn't return an mbuf\n");
                if (mac) {
                    SMB_FREE_DATA(mac, mac_allocsize);
                }
                return EBADRPC;
            }
            mb_len = mbuf_len(mb);
            mb_total_len = mbuf_len(mb);
            mb_off = 0;
        }
        
        /* sanity check */
        if (mb_off > mb_total_len) {
            SMBDEBUG("mb_off: %lu past end of mbuf, mbuf_len: %lu\n", mb_off, mb_total_len);
            if (mac) {
                SMB_FREE_DATA(mac, mac_allocsize);
            }
            return (EBADRPC);
        }

        /* Calculate length to sign for this pass */
        sign_len = remaining;
        if (sign_len > mb_len) {
            sign_len = mb_len;
        }
        
        /* Sign it */
        cchmac_update(di, hc, sign_len, (uint8_t *)mbuf_data(mb) + mb_off);

        mb_off += sign_len;
        mb_len -= sign_len;
        remaining -= sign_len;
    }
    
    cchmac_final(di, hc, mac);
    
	/*
	 * Finally, verify the signature.
	 */
    result = cc_cmp_safe(SMB2SIGLEN, signature, mac);
    if (mac) {
        SMB_FREE_DATA(mac, mac_allocsize);
    }
	return (result);
}

/*
 * SMB 2/3 Verify reply signature with HMAC-SHA256
 */
int
smb2_rq_verify(struct smb_rq *rqp, struct mdchain *mdp, uint8_t *signature)
{
    struct smb_session *sessionp;
    uint32_t nextCmdOffset;
    int err;
    
    if (rqp == NULL) {
        SMBDEBUG("Called with NULL rqp\n");
        return (EINVAL);
    }
    
    sessionp = rqp->sr_session;
    
    if (sessionp == NULL) {
        SMBERROR("NULL sessionp\n");
        return  (0);
    }
    nextCmdOffset = rqp->sr_rspnextcmd;
    
    if (!(sessionp->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) &&
        !(rqp->sr_flags & SMBR_SIGNED)) {
		SMBWARNING("signatures not enabled!\n");
		return (0);
	}
    
    if ((sessionp->session_mackey == NULL) ||
        (rqp->sr_command == SMB2_OPLOCK_BREAK) ||
        (rqp->sr_command == SMB2_NEGOTIATE) ||
        (rqp->sr_command == SMB2_SESSION_SETUP)) {
        /*
         * Don't verify signature if we don't have a session key from gssd yet.
         * Don't verify negotiate messages - are never signed
         * Don't verify signed SessionSetup replies as they will be checked
         * later after deriving the SMB3 keys.
         */
		return (0);
    }

    /* Its an anonymous login, signing is not supported */
	if ((sessionp->session_flags & SMBV_ANONYMOUS_ACCESS) == SMBV_ANONYMOUS_ACCESS) {
		return (0);
    }
    
    SMB_LOG_KTRACE(SMB_DBG_RQ_VERIFY | DBG_FUNC_START,
                   0, 0, 0, 0, 0);

    if (SMBV_SMB3_OR_LATER(sessionp)) {
        err = smb3_verify(rqp, mdp, nextCmdOffset, signature);
    } else {
        err = smb2_verify(rqp, mdp, nextCmdOffset, signature);
    }
    
    if (err) {
        SMBDEBUG("Could not verify signature for sr_command %x, msgid: %llu\n",
                 rqp->sr_command, rqp->sr_messageid);
        err = EAUTH;
    }
    
    SMB_LOG_KTRACE(SMB_DBG_RQ_VERIFY | DBG_FUNC_END,
                   err, 0, 0, 0, 0);

    return (err);
}

/*
 * SMB 3 Routines
 */

#define SMB2_GMAC_IV_IS_RESPONSE 0x1
#define SMB2_GMAC_IV_IS_CANCEL   0x2

typedef struct {
    uint64_t message_id;
    uint32_t flags;
} gmac_iv;

static int
smb3_init_gmac_iv(uint64_t message_id,
                  uint16_t command,
                  bool is_response,
                  gmac_iv  *iv)
{
    if (iv == NULL) {
        SMBERROR("iv is NULL");
        return EINVAL;
    }
    iv->message_id = message_id;
    
    if (is_response) {
        iv->flags |= SMB2_GMAC_IV_IS_RESPONSE;
    }
    if (command == SMB2_CANCEL) {
        iv->flags |= SMB2_GMAC_IV_IS_CANCEL;
    }
    return 0;
}

static int
smb3_get_signature_one_shot(uint16_t algorithm,
                            uint8_t *key,
                            size_t size,
                            u_char *block,
                            u_char *mac,
                            uint64_t message_id,
                            uint16_t command,
                            bool is_response)
{
    
    int error;

    switch (algorithm) {
        case SMB2_SIGNING_AES_CMAC:
        {
            const struct ccmode_cbc *ccmode = ccaes_cbc_encrypt_mode();
            error = cccmac_one_shot_generate(ccmode,
                                             SMB3_KEY_LEN, key,
                                             size, block,
                                             CMAC_BLOCKSIZE, mac);
            break;
        }
        case SMB2_SIGNING_AES_GMAC:
        {
            const struct ccmode_gcm *gcmode = ccaes_gcm_encrypt_mode();
            gmac_iv iv = {0};
            error = smb3_init_gmac_iv(message_id, command, is_response, &iv);
            if (error) {
                return EAUTH;
            }
            error = ccgcm_one_shot(gcmode,
                                   SMB3_KEY_LEN, key,
                                   CCGCM_IV_NBYTES, (void*)&iv,
                                   size, block,
                                   0, NULL, NULL, // not used for signing
                                   CCGCM_BLOCK_NBYTES, mac);
            break;
        }
        default:
        {
            SMBERROR("Signing algorithm (%u) not supported", algorithm);
            error = EINVAL;
            break;
        }
    }
    return error;
}

static int
smb3_get_signature_blocks(struct smb_rq *rqp,
                          uint8_t *key,
                          struct mbchain *mbp,
                          struct mdchain *mdp,
                          u_char *mac,
                          size_t packet_len)
{
    const struct ccmode_cbc *ccmode = ccaes_cbc_encrypt_mode();
    const struct ccmode_gcm *gcmode = ccaes_gcm_encrypt_mode();
    mbuf_t mb, starting_mp;
    size_t mb_len;
    u_char *mb_data;
    int mb_count = 0;
    int error;
    bool is_response;
    uint16_t algorithm = rqp->sr_session->session_smb3_signing_algorithm;
    cccmac_mode_decl(ccmode, cmac);
    ccgcm_ctx_decl(ccgcm_context_size(gcmode), context);

    if (mdp == NULL) {
        /* Signing a request - easy case */
        starting_mp = mbp->mb_top;
        is_response = false;
    }
    else {
        /* Signing a reply - complicated case */
        starting_mp = mdp->md_cur;
        is_response = true;
    }

    /* Initial mb setup */
    mb_count = 0;

    /* Init the cipher */
    switch (algorithm) {
        case SMB2_SIGNING_AES_CMAC:
            error = cccmac_init(ccmode, cmac, SMB3_KEY_LEN, key);
            if (error) {
                SMBERROR("cccmac_init failed %d \n", error);
                goto out;
            }
            break;
        case SMB2_SIGNING_AES_GMAC:
        {
            gmac_iv iv = {0};
            error = smb3_init_gmac_iv(rqp->sr_messageid,
                                      rqp->sr_command,
                                      is_response,
                                      &iv);
            if (error) {
                return EAUTH;
            }

            error = ccgcm_init_with_iv(gcmode, context, SMB3_KEY_LEN, key, (void*)&iv);
            if (error) {
                SMBERROR("ccgcm_init failed %d \n", error);
                goto out;
            }
            break;
        }
        default:
            SMBERROR("Signing algorithm (%u) not supported", algorithm);
            error = EINVAL;
            goto out;
    }

    /*
     * Loop through the mbufs and process each one
     * Requests - loop until we get to end of mbuf chain
     * Replies - loop until packet_len is 0
     */
    for (mb = starting_mp; mb != NULL; mb = mbuf_next(mb)) {
        if ((mdp) &&
            (mb == starting_mp)) {
            /*
             * For replies and for the first mbuf:
             * 1) len is how ever many bytes are left in the current mbuf
             * that have not been processed yet
             * 2) mbuf data starts at mdp->md_pos
             */
            mb_len = (size_t) mbuf_data(mb) + mbuf_len(mb) - (size_t) mdp->md_pos;
            //SMBERROR("reply start len %ld \n", mb_len);
            mb_data = mdp->md_pos;
        }
        else {
            /*
             * Either its is a request or past the first mbuf for a reply
             */
            mb_len = mbuf_len(mb);
            mb_data = mbuf_data(mb);
        }

        /*
         * Only replies use packet_len. See if the reply ends before the end
         * of this mbuf. If so, then only process up to the end of the reply.
         */
        if ((mdp) && (packet_len > 0)) {
            if (packet_len < mb_len) {
                mb_len = packet_len;
            }
        }

        /* Process this m_buf's data */
        switch (algorithm) {
            case SMB2_SIGNING_AES_CMAC:
                error = cccmac_update(cmac, mb_len, mb_data);
                if (error) {
                    SMBERROR("cccmac_update failed %d \n", error);
                    goto out;
                }
                break;
            case SMB2_SIGNING_AES_GMAC:
                error = ccgcm_aad(gcmode, context, mb_len, mb_data);
                if (error) {
                    SMBERROR("ccgcm_aad failed %d \n", error);
                    goto out;
                }
                break;
            default:
                // Should never happen
                SMBERROR("Signing algorithm (%u) not supported", algorithm);
                error = EINVAL;
                goto out;
        }


        /* For replies, decrement how much of the reply we have left */
        if ((mdp) && (packet_len > 0)) {
            packet_len -= mb_len;

            if (packet_len == 0) {
                /* Done with reply, go to cccmac_final */
                break;
            }
        }

        mb_count += 1;
    }

    /* Finalize the signature */
    switch (algorithm) {
        case SMB2_SIGNING_AES_CMAC:
            error = cccmac_final_generate(cmac, CMAC_BLOCKSIZE, mac);
            if (error) {
                SMBERROR("cccmac_final_generate failed %d \n", error);
            }
            break;
        case SMB2_SIGNING_AES_GMAC:
            error = ccgcm_finalize(gcmode, context, CCGCM_BLOCK_NBYTES, mac);
            if (error) {
                SMBERROR("ccgcm_finalize failed %d \n", error);
            }
            break;
        default:
            // Should never happen
            SMBERROR("Signing algorithm (%u) not supported", algorithm);
            break;
    }


    /* Replies only check */
    if ((mdp) && (packet_len > 0)) {
        SMBERROR("Not enough bytes in packet? <%zu>\n", packet_len);
    }

    return 0;
out:
    return error;
}

static void
smb3_get_signature(struct smb_rq *rqp,
                   uint8_t *key, uint32_t keylen,
				   struct mbchain *mbp,
				   struct mdchain *mdp,
				   size_t packet_len,
				   uint8_t *out_signature,
				   size_t out_signature_len)
{
	mbuf_t mb;
	size_t mb_off, mb_len, n;
	u_char mac[CMAC_BLOCKSIZE];
	int error;
	u_char *block = NULL;
    bool is_response;
#if USE_MALLOC
	size_t malloc_size = HYBRID_MALLOC_SIZE;
#else
	size_t malloc_size = 0;
#endif
	
	/*
	 * This function is used for both Requests and Replies
	 * Refer to smb3_verify() for more details on how complicated compound
	 * replies are to process
	 */

    if (rqp == NULL) {
        SMBERROR("rqp is null\n");
        return;
    }

	if (out_signature == NULL) {
		SMBERROR("out_signature is null\n");
		return;
	}
	
	if ((mbp == NULL) && (mdp == NULL)) {
		SMBERROR("mbp and mdp are null\n");
		return;
	}
	
	if (keylen < SMB3_KEY_LEN) {
		SMBERROR("smb3 keylen %u\n", keylen);
		return;
	}
	
	if (out_signature_len < SMB2SIGLEN) {
		SMBERROR("out_signature_len too small %zu\n", out_signature_len);
		return;
	}
	
	/* Init signature field to all zeros */
	bzero(out_signature, out_signature_len);
	
	/* Can we do the packet in a single block? */
	if (malloc_size >= packet_len) {
		/* Yep */
		malloc_size = packet_len;
		
		/* Malloc the block */
        SMB_MALLOC_DATA(block, malloc_size, Z_WAITOK);
		if (block == NULL) {
			SMBERROR("Out of memory for block\n");
			goto out;
		}
		
		/* Initial mb setup */
		if (mdp == NULL) {
			/* Its a request to process */
			mb = mbp->mb_top;
			mb_len = mbuf_len(mb);
			mb_off = 0;
            is_response = false;
		}
		else {
			/* Its a reply to process */
			mb = mdp->md_cur;
			mb_len = (size_t) mbuf_data(mb) + mbuf_len(mb) - (size_t) mdp->md_pos;
			mb_off = mbuf_len(mb) - mb_len;
            is_response = true;
		}

		n = mbuf_get_nbytes(malloc_size, block, 0, &mb, &mb_len, &mb_off);
		if (n != malloc_size) {
			SMBERROR("mbuf chain exhausted on first block exp: %zu, got: %lu\n",
					 malloc_size, n);
			goto out;
		}
		
		/* Sign entire packet in one call */
        error = smb3_get_signature_one_shot(rqp->sr_session->session_smb3_signing_algorithm,
                                            key, malloc_size,
                                            block, mac,
                                            rqp->sr_messageid,
                                            rqp->sr_command,
                                            is_response);
        if (error) {
            SMBERROR("smb3_get_signature_one_shot failed %d \n", error);
            goto out;
        }
	}
	else {
		/*
		 * Probably a read or write. Have to do multiple calls to the crypto
		 * code to process this packet
		 */

        error = smb3_get_signature_blocks(rqp, key, mbp, mdp, mac,
                                          packet_len);
        if (error) {
            SMBERROR("smb3_get_signature_blocks failed %d \n", error);
            goto out;
        }


	}

	/* Copy first 16 bytes of the signature into the signature field */
	bcopy(mac, out_signature, SMB2SIGLEN);

out:
	if (block) {
        SMB_FREE_DATA(block, malloc_size);
	}
}


/*
 * SMB 3 Verify a reply with AES-CMAC-128
 */
static int
smb3_verify(struct smb_rq *rqp, struct mdchain *mdp,
            uint32_t nextCmdOffset, uint8_t *signature)
{
    struct smb_session *sessionp = rqp->sr_session;
    mbuf_t mb;
    size_t sig_offset, mb_len;
    int result;
    uint8_t *sig_ptr = NULL;
    size_t sig_bytes_remaining;
    uint8_t reply_signature[SMB2SIGLEN] = {0};
	size_t reply_len;
	mbuf_t mb_temp;
    uint8_t *key = NULL;
    uint32_t keylen = 0;

    if (sessionp == NULL) {
        SMBERROR("sessionp is NULL\n");
        return  (EINVAL);
    }
	
    if (sessionp->session_smb3_signing_key_len < SMB3_KEY_LEN) {
        SMBERROR("SMB3 signing key len too small: %u\n", sessionp->session_smb3_signing_key_len);
        return (EAUTH);
    }
    
    /*
     * Handling replies is more difficult because the reply could be a
     * compound reply. In that case, its one mbuf chain with multiple replies.
     * As you finish each reply, you have to remember where you are in the
     * current mbuf.
     *
     * Example 1: ReplyOne is 184 bytes, ReplyTwo is 96 bytes and ReplyThree is
     * 128 bytes. mbuf[0] is 88 bytes and mbuf[1] is 320 bytes. ReplyOne starts
     * in mbuf[0] and continues into mbuf[1]. ReplyTwo starts in mbuf[1] at
     * offset 96. ReplyThree starts in mbuf[1] at offset 192
     *
     * Example 2: ReplyOne is 80 bytes.  ReplyTwo is 80 bytes and ReplyThree is
     * 80 bytes. mbuf[0] is 88 bytes and mbuf[1] is 152 bytes. ReplyOne starts 
     * and finishes in mbuf[0]. ReplyTwo starts in mbuf[0] at offset 80 and 
     * continues into mbuf[1]. ReplyThree starts in mbuf[1] at offset 72
     *
     * mb = mdp->md_cur - current mbuf that we are processing
     * mbuf_data(mb) - address of the start of data in current mbuf
     * mdp->md_pos - next byte to process in mdp->md_cur
     * mbuf_len(mb) - len of current mbuf
     * (size_t) mbuf_data(mb) + mbuf_len(mb) - (size_t) mdp->md_pos - len of
     *          data left in the mdp->md_cur that has not yet been processed
     */
    
    /* Find the reply's signature and set it to all zeros */
    sig_offset = SMB2SIGOFF;
    
    /* 
     * Get the len of unprocessed bytes left in the first mbuf. We might have
     * used up some of the bytes in this mbuf for the previous reply
     */
    mb = mdp->md_cur;
    mb_len = (size_t) mbuf_data(mb) + mbuf_len(mb) - (size_t) mdp->md_pos;
   // SMBERROR("first mb_len %ld \n", mb_len);
    
    /* Search for mbuf where the signature starts in */
    for (mb = mdp->md_cur; mb != NULL; mb = mbuf_next(mb)) {
        if (mb != mdp->md_cur) {
            /* After the first mbuf, mb_len is now the same as mbuf_len() */
            mb_len = mbuf_len(mb);
        }
        
        if (mb_len > sig_offset) {
            /* Signature starts in this mbuf */
            break;
        }
        
        /* Not in this mbuf, on to the next */
        sig_offset -= mb_len;
    }

    if (mb == NULL) {
        /* Should never happen */
        SMBERROR("Signature not found\n");
        return (EBADRPC);
    }
    
    /* 
     * Zero out the signature even if it crosses multiple mbufs 
     *
     * Note mb_len is set in the above code
     */
    sig_bytes_remaining = SMB2SIGLEN;
    while (sig_bytes_remaining > 0) {
        if (mb != mdp->md_cur) {
            sig_ptr = mbuf_data(mb);
        }
        else {
            /*
             * If in first mbuf, then have to offset for any bytes that were
             * processed already
             */
            sig_ptr = mdp->md_pos;
        }

        sig_ptr += sig_offset;

        /* 
         * Find out how many bytes can we zero in this mbuf.
         * Already guaranteed that mb_len > sig_offset else wouldnt be here
         */
        mb_len -= sig_offset;
        
        /*SMBERROR("mb_len %zu sig_bytes_remaining %zu \n",
                 mb_len, sig_bytes_remaining);*/
        
        if (sig_bytes_remaining < mb_len) {
            mb_len = sig_bytes_remaining;
        }
        
        bzero(sig_ptr, mb_len);
        sig_bytes_remaining -= mb_len;
        
        /* mb_off is only used in the first mbuf */
        if (sig_offset > 0) {
            sig_offset = 0;
        }
        
        mb = mbuf_next(mb);
        if (mb == NULL) {
            break;
        }
        else {
            /* 
             * Since moving to next mbuf, none of its bytes have been used
             * so entire len is available
             */
            mb_len = mbuf_len(mb);
        }
    }

    if (sig_bytes_remaining > 0) {
        /* Should never happen */
        SMBERROR("Not enough bytes for signature\n");
        return (EBADRPC);
    }
	
	/* Determine the total length to sign */
	if (nextCmdOffset != 0) {
		reply_len = nextCmdOffset;
	}
	else {
		/*
		 * For the first mbuf:
		 * 1) len is how ever many bytes are left in the current mbuf
		 * that have not been processed yet
		 * 2) mbuf data starts at mdp->md_pos
		 */
		mb = mdp->md_cur;
		mb_len = (size_t) mbuf_data(mb) + mbuf_len(mb) - (size_t) mdp->md_pos;
		
		/*
		 * We don't know the length of the reply because it's
		 * not a compound reply or the end of a compound reply.
		 * So calculate total length of this reply.
		 */
		reply_len = mb_len;						/* length in first mbuf */
		mb_temp = mbuf_next(mb);
		while (mb_temp) {
			reply_len += mbuf_len(mb_temp);
			mb_temp = mbuf_next(mb_temp);
		}
	}

    /*
     * Calculate reply's signature
     *
     * For alternate channels, all messages prior to the last Session Setup
     * reply with status of SUCCESS are signed with the main channel signing
     * key. The last Session Setup and onward are signed with the alt channel
     * signing key.
     */
	/*SMBERROR("SMB3 verify signature: mid %llu NextCmd %d\n",
			 rqp->sr_messageid, nextCmdOffset);*/
    if ((rqp->sr_iod) && (rqp->sr_iod->iod_flags & SMBIOD_USE_CHANNEL_KEYS)) {
        key    = rqp->sr_iod->iod_smb3_signing_key;
        keylen = rqp->sr_iod->iod_smb3_signing_key_len;
    } else {
        key    = sessionp->session_smb3_signing_key;
        keylen = sessionp->session_smb3_signing_key_len;
    }
	smb3_get_signature(rqp ,key, keylen,
                       NULL, mdp, reply_len, reply_signature, SMB2SIGLEN);
	
    /*
     * Finally, verify the signature.
     */
    result = cc_cmp_safe(SMB2SIGLEN, signature, reply_signature);
    if (result) {
        SMBDEBUG("SMB3 signature mismatch: mid %llu\n", rqp->sr_messageid);
        
        SMBDEBUG("SigCalc: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 reply_signature[0], reply_signature[1], reply_signature[2], reply_signature[3],
                 reply_signature[4], reply_signature[5], reply_signature[6], reply_signature[7],
                 reply_signature[8], reply_signature[9], reply_signature[10], reply_signature[11],
                 reply_signature[12], reply_signature[13], reply_signature[14], reply_signature[15]);
        
        SMBDEBUG("SigExpected: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 signature[0], signature[1], signature[2],signature[3],
                 signature[4], signature[5], signature[6], signature[7],
                 signature[8], signature[9], signature[10], signature[11],
                 signature[12], signature[13], signature[14], signature[15]);
    }

	return (result);
}

int
smb3_verify_session_setup(struct smb_session *sessionp, struct smbiod *iod,
                          uint8_t *sess_setup_reply, size_t sess_setup_len)
{
    char reply_signature[SMB2SIGLEN] = {0};
    char signature[SMB2SIGLEN] = {0};
    uint8_t *key = NULL;
    uint32_t keylen = 0;
    u_char mac[CMAC_BLOCKSIZE];
    int error;
    
    /*
     * This function is used for to just verify the signature of the last
     * session setup reply that is signed due to SMB v3.1.1 pre auth integrity
     * check or SMB 3.x.x multichannel.
     */
    if ((sessionp == NULL) || (iod == NULL)) {
        SMBERROR("sessionp or iod are null? \n");
        return(EAUTH);
    }

    if ((sess_setup_reply == NULL) || (sess_setup_len == 0)) {
        SMBERROR("sess_setup_reply is null or sess_setup_len is 0 \n");
        return(EAUTH);
    }
    
    if (sess_setup_len < (SMB2SIGOFF + SMB2SIGLEN + 1)) {
        SMBERROR("sess_setup_reply is too small %zu \n", sess_setup_len);
        return(EAUTH);
    }
    
    if (iod->iod_flags & SMBIOD_USE_CHANNEL_KEYS) {
        key = iod->iod_smb3_signing_key;
        keylen = iod->iod_smb3_signing_key_len;
    }
    else {
        key = sessionp->session_smb3_signing_key;
        keylen = sessionp->session_smb3_signing_key_len;
    }

    if (key == NULL) {
        SMBERROR("SMB3 key is NULL \n");
        return(EAUTH);
    }

    if (keylen < SMB3_KEY_LEN) {
        SMBERROR("SMB3 keylen too small %u\n", keylen);
        return(EAUTH);
    }
    
    /* Save the reply's signature */
    bcopy(sess_setup_reply + SMB2SIGOFF, signature, SMB2SIGLEN);
    
    /* Zero out the reply's signature */
    bzero(sess_setup_reply + SMB2SIGOFF, SMB2SIGLEN);
    
    /* Calculate signature in one call */

    error = smb3_get_signature_one_shot(sessionp->session_smb3_signing_algorithm,
                                        key, sess_setup_len, sess_setup_reply, mac,
                                        iod->iod_sess_setup_message_id,
                                        SMB2_SESSION_SETUP,
                                        true);

    if (error) {
        SMBERROR("smb3_get_signature_one_shot failed %d \n", error);
        return(EAUTH);
    }
    
    /* Copy first 16 bytes of the HMAC hash into the reply_signature */
    bcopy(mac, reply_signature, SMB2SIGLEN);

    /*
     * Finally, verify the signature.
     */
    error = cc_cmp_safe(SMB2SIGLEN, signature, reply_signature);
    if (error) {
        SMBDEBUG("SMB3 signature mismatch on last session setup reply \n");
        
        SMBDEBUG("SigCalc: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 reply_signature[0], reply_signature[1], reply_signature[2], reply_signature[3],
                 reply_signature[4], reply_signature[5], reply_signature[6], reply_signature[7],
                 reply_signature[8], reply_signature[9], reply_signature[10], reply_signature[11],
                 reply_signature[12], reply_signature[13], reply_signature[14], reply_signature[15]);
        
        SMBDEBUG("SigExpected: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 signature[0], signature[1], signature[2],signature[3],
                 signature[4], signature[5], signature[6], signature[7],
                 signature[8], signature[9], signature[10], signature[11],
                 signature[12], signature[13], signature[14], signature[15]);
        
        error = EAUTH;
    }

    return (error);
}

/*
 * SMB 3 Sign a single request with AES-CMAC-128
 */
static void
smb3_sign(struct smb_rq *rqp)
{
	struct smb_session *sessionp = rqp->sr_session;
	struct mbchain *mbp;
	size_t request_len;
	mbuf_t mb;

    if (rqp->sr_rqsig == NULL) {
		SMBDEBUG("sr_rqsig was never allocated.\n");
		return;
	}
	
	if (sessionp->session_smb3_signing_key_len < SMB3_KEY_LEN) {
		SMBERROR("smb3 keylen %u\n", sessionp->session_smb3_signing_key_len);
		return;
	}
	
	/* Initialize 16-byte security signature field to all zeros. */
	bzero(rqp->sr_rqsig, SMB2SIGLEN);
	
	/* Set flag to indicate this PDU is signed */
	*rqp->sr_flagsp |= htolel(SMB2_FLAGS_SIGNED);
	
	smb_rq_getrequest(rqp, &mbp);
	
	/* Determine the total length to sign */
	request_len = 0;
	for (mb = mbp->mb_top; mb != NULL; mb = mbuf_next(mb)) {
		request_len += mbuf_len(mb);
	}

	/* Sign the request */
	//SMBERROR("Send: mid %lld cmd %hu \n", rqp->sr_messageid, rqp->sr_command);
    uint8_t *key = NULL;
    uint32_t keylen = 0;
    // For alternate channel, all messages prior to "session-setup-response with status SUCCESS"
    // are signed by the main-channel keys, while the "session-setup-response with status SUCCESS"
    // message and onward are signed with the alternate-channel key
    if ((rqp->sr_iod) && (rqp->sr_iod->iod_flags & SMBIOD_USE_CHANNEL_KEYS)) {
        key    = rqp->sr_iod->iod_smb3_signing_key;
        keylen = rqp->sr_iod->iod_smb3_signing_key_len;
    } else {
        key    = sessionp->session_smb3_signing_key;
        keylen = sessionp->session_smb3_signing_key_len;
    }
	smb3_get_signature(rqp, key, keylen,
                       mbp, NULL, request_len, rqp->sr_rqsig, SMB2SIGLEN);
}


/*
 * Encrypts an SMB msg or msg chain given in 'mb'.
 * Note: On any error the mbuf chain is freed.
 */
int smb3_rq_encrypt(struct smb_rq *rqp)
{
    mbuf_t                  mb_hdr, mb_tmp;
    struct smb_session      *sessionp = rqp->sr_session;
    size_t                  len;
    unsigned char           nonce[16];
    unsigned char           sig[SMB3_AES_TF_SIG_LEN];
    uint64_t                i64;
    uint32_t                msglen, i32;
    uint16_t                i16;
    int                     error;
    unsigned char           *msgp;
    const struct ccmode_ccm *ccmode = ccaes_ccm_encrypt_mode();
    const struct ccmode_gcm *gcmode = ccaes_gcm_encrypt_mode();
    size_t                  nbytes;
    mbuf_t                  mb, m2;
    struct mbchain          *mbp, *tmp_mbp;
    struct smb_rq           *tmp_rqp;

    SMB_LOG_KTRACE(SMB_DBG_RQ_ENCRYPT | DBG_FUNC_START,
                   0, 0, 0, 0, 0);

    switch (sessionp->session_smb3_encrypt_ciper) {
        case SMB2_ENCRYPTION_AES_128_GCM:
        case SMB2_ENCRYPTION_AES_256_GCM:
            break;

        case SMB2_ENCRYPTION_AES_128_CCM:
        case SMB2_ENCRYPTION_AES_256_CCM:
            break;

        default:
            SMBERROR("Unknown cipher %d defaulting to SMB2_ENCRYPTION_AES_128_CCM \n",
                     sessionp->session_smb3_encrypt_ciper);
            sessionp->session_smb3_encrypt_ciper = SMB2_ENCRYPTION_AES_128_CCM;
            break;
    }

    smb_rq_getrequest(rqp, &mbp);

    if (rqp->sr_flags & SMBR_COMPOUND_RQ) {
        /*
         * Compound request to send. The first rqp has its sr_next_rq set to
         * point to the next request to send and so on. The last request will
         * have sr_next_rq set to NULL. The next_command fields should already
         * be filled in with correct offsets. Have to copy all the requests
         * into a single mbuf chain before sending it.
         *
         * ONLY the first rqp in the chain will have its sr_ fields updated.
         */
        DBG_ASSERT(rqp->sr_next_rqp != NULL);
        
        /*
         * Copy all the requests to just one mbuf chain
         */
        mb = mb_detach(mbp);
        
        /* Concatenate the other requests into the mbuf chain */
        tmp_rqp = rqp->sr_next_rqp;
        while (tmp_rqp != NULL) {
            /* copy next request into new mbuf m2 */
            smb_rq_getrequest(tmp_rqp, &tmp_mbp);
            m2 = mb_detach(tmp_mbp);
            
            /* concatenate m2 to m */
            mb = mbuf_concatenate(mb, m2);
            
            tmp_rqp = tmp_rqp->sr_next_rqp;
        }
        
        /* fix up the mbuf packet header */
        m_fixhdr(mb);
    }
    else {
        /*
         * Not a compound request
         * Just get the mbuf chain from the request
         */
        if (rqp->sr_flags & SMBR_COMPRESSED) {
            mb = NULL;

            if (rqp->sr_command == SMB2_WRITE) {
                /* Get compressed mbuf chain */
                mbp = &rqp->sr_rq_compressed;
                mb = mb_detach(mbp);
                
                if (mb == NULL) {
                    /* Sanity checks */
                    SMBERROR("Compressed write missing mbuf? \n");
                }
            }
            else {
                /* Sanity check */
                SMBERROR("Found compressed req <%d> that is not a write? \n",
                         rqp->sr_command);
            }
            
            if (mb == NULL) {
                /* Something went wrong, just fall back to normal mbp */
                smb_rq_getrequest(rqp, &mbp);
                mb = mb_detach(mbp);
            }
        }
        else {
            /* Not compressed request */
            mb = mb_detach(mbp);
        }
    }
    
    mb_hdr = NULL;

    /* Declare ciphers */
    ccccm_ctx_decl(ccmode->size, ctx);
    ccccm_nonce_decl(ccmode->nonce_size, nonce_ctx);

    ccgcm_ctx_decl(gcmode->size, gcm_ctx);

    if (!sessionp->session_smb3_encrypt_key_len) {
        /* Cannot encrypt without a key */
        SMBDEBUG("smb3 encr, no key\n");
        error = EINVAL;
        goto out;
    }

    /* Need an mbuf for the Transform header */
    error = mbuf_gethdr(MBUF_WAITOK, MBUF_TYPE_DATA, &mb_hdr);
    if (error) {
        SMBERROR("No mbuf for transform hdr, error: %d\n", error);
        error = ENOBUFS;
        goto out;
    }
    
    /* Build the Transform header */
    msgp = mbuf_data(mb_hdr);
    memset(msgp, 0, SMB3_AES_TF_HDR_LEN);
    
    /* Set protocol field (0xFD 'S' 'M' 'B') */
    memcpy(msgp + SMB3_AES_TF_PROTO_OFF, SMB3_AES_TF_PROTO_STR,
           SMB3_AES_TF_PROTO_LEN);
    
    /* Update session nonce */
    SMBC_ST_LOCK(sessionp);
    sessionp->session_smb3_nonce_low++;
    if (!sessionp->session_smb3_nonce_low) {
        sessionp->session_smb3_nonce_low++;
        sessionp->session_smb3_nonce_high++;
    }
    SMBC_ST_UNLOCK(sessionp);
    
    /* Setup nonce field */
    memset(nonce, 0, 16);
    memcpy(nonce, &sessionp->session_smb3_nonce_high, 8);
    memcpy(&nonce[8], &sessionp->session_smb3_nonce_low, 8);
    
    switch (sessionp->session_smb3_encrypt_ciper) {
        case SMB2_ENCRYPTION_AES_128_GCM:
        case SMB2_ENCRYPTION_AES_256_GCM:
            // Zero last 4 bytes per spec
            memset(&nonce[12], 0, 4);
           break;

        case SMB2_ENCRYPTION_AES_128_CCM:
        case SMB2_ENCRYPTION_AES_256_CCM:
            // Zero last 5 bytes per spec
            memset(&nonce[11], 0, 5);
            break;
    }

    memcpy(msgp + SMB3_AES_TF_NONCE_OFF, nonce, SMB3_AES_TF_NONCE_LEN);
    
    // Get length of original message
    len = mbuf_get_chain_len(mb);
    
    if (len > 0xffffffff) {
        /* The transform header field "original_message_size" holds the
         * total length of the unencrypted msg(s) going out, and the field
         * is a uint32_t (only 4 bytes).  So we can only encrypt a 4 GB
         * message (chained or not chained).
         */
        SMBERROR("mb msglen too big for transform: %lu\n", len);
        error = EAUTH;
        goto out;
    }
    
    // Set message length (okay to cast, we already checked above */
    msglen = (uint32_t)len;
    i32 = htolel(msglen);
    memcpy(msgp + SMB3_AES_TF_MSGLEN_OFF, &i32,
           SMB3_AES_TF_MSGLEN_LEN);
    
    /* Set Encryption Algorithm */
    if (sessionp->session_flags & SMBV_SMB311) {
        /* SMB 3.1.1 */
        i16 = htoles(SMB311_ENCRYPTED_FLAG);
        memcpy(msgp + SMB3_AES_TF_ENCR_ALG_OFF, &i16,
               SMB3_AES_TF_ENCR_ALG_LEN);
    }
    else {
        /* SMB 3.0 and 3.0.2 */
        i16 = htoles(SMB2_ENCRYPTION_AES128_CCM);
        memcpy(msgp + SMB3_AES_TF_ENCR_ALG_OFF, &i16,
               SMB3_AES_TF_ENCR_ALG_LEN);
    }

    /* Session ID */
    i64 = htoleq(rqp->sr_rqsessionid);
    memcpy(msgp + SMB3_AES_TF_SESSID_OFF, &i64,
           SMB3_AES_TF_SESSID_LEN);
    
    // Set data length of mb_hdr
    mbuf_setlen(mb_hdr, SMB3_AES_TF_HDR_LEN);
    
    /*
     * At this point the transform header in mb_hdr is completely filled out
     *
     * SMB Transform Header looks like [MS-SMB2] 2.2.41
     *      ProtocolId (4 bytes) - always 0x424D53FD
     *      Signature (16 bytes) - Signature of data starting from the
     *          unencrypted Transform Header Nonce and including encrypted data
     *      Nonce (16 bytes) -
     *          For AES-128-CCM and AES-256-CCM the nonce is AES-CCM-Nonce (11 bytes) + 5 bytes of 0
     *          For AES-128-GCM and AES-256-GCM the nonce is AES-GCM-Nonce (12 bytes) + 4 bytes of 0
     *      OriginalMessageSize (4 bytes) - len of the unencrypted data
     *      Reserved (2 bytes) - always zero
     *      Flags/EncryptionAlgorithm (2 bytes) - always 0x0001
     *      SessionId (8 bytes) - set to SMB SessionID
     *      EncryptedData (var length)
     */

    /* Init the cipher */
    switch (sessionp->session_smb3_encrypt_ciper) {
        case SMB2_ENCRYPTION_AES_128_GCM:
        case SMB2_ENCRYPTION_AES_256_GCM:
            /* Init the cipher with key */
            ccgcm_init(gcmode, gcm_ctx,
                       sessionp->session_smb3_encrypt_key_len,
                       sessionp->session_smb3_encrypt_key);

            /* Set Initialization Values */
            ccgcm_set_iv(gcmode, gcm_ctx,
                         SMB3_GCM_NONCE_LEN,
                         msgp + SMB3_AES_TF_NONCE_OFF);

            /*
             * Calculate signature for the following fields inside the SMB
             * Transform Header (adds up to be 32 bytes):
             * Nonce, OriginalMessageSize, Reserved, Flags/EncryptionAlgorithm,
             * SessionID
             */
            ccgcm_aad(gcmode, gcm_ctx, SMB3_AES_AUTHDATA_LEN,
                      msgp + SMB3_AES_AUTHDATA_OFF);

            /* Encrypt msg data in place */
            for (mb_tmp = mb; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
                nbytes = mbuf_len(mb_tmp);

                if (nbytes) {
                    ccgcm_update(gcmode, gcm_ctx, nbytes,
                                 mbuf_data(mb_tmp), mbuf_data(mb_tmp));
                }
            }

            /* Copy final signature to the transform header Signature field */
            ccgcm_finalize(gcmode, gcm_ctx, CCAES_KEY_SIZE_128, sig);
            memcpy(msgp + SMB3_AES_TF_SIG_OFF, sig, CCAES_KEY_SIZE_128);
          break;

        case SMB2_ENCRYPTION_AES_128_CCM:
        case SMB2_ENCRYPTION_AES_256_CCM:
            /* Init the cipher with key */
            ccccm_init(ccmode, ctx,
                       sessionp->session_smb3_encrypt_key_len,
                       sessionp->session_smb3_encrypt_key);

            /*
             * 1. *nonce is the nonce inside the SMB Transform Header. This
             *    API seems to generate and fill in the nonce for us.
             *    SMB3_CCM_NONCE_LEN is defined to be 11 bytes.
             * 2. mac_size is the size of the signature field inside the SMB
             *    Transform Header which is 16 bytes
             * 3. auth_len is the amount of data to sign in SMB Transform Header
             *    which is 32 bytes
             * 4. data_len is the amount of data to sign in Original message
             */

            /* Set Initialization Values */
            ccccm_set_iv(ccmode, ctx, nonce_ctx, SMB3_CCM_NONCE_LEN,
                         /* *nonce */ msgp + SMB3_AES_TF_NONCE_OFF,
                         /* mac_size */ SMB3_AES_TF_SIG_LEN,
                         /* auth_len */ SMB3_AES_AUTHDATA_LEN,
                         /* data_len */ msglen);

            /*
             * Calculate signature for the following fields inside the SMB
             * Transform Header (adds up to be 32 bytes):
             * Nonce, OriginalMessageSize, Reserved, Flags/EncryptionAlgorithm,
             * SessionID
             */
            ccccm_cbcmac(ccmode, ctx, nonce_ctx,
                         SMB3_AES_AUTHDATA_LEN, msgp + SMB3_AES_AUTHDATA_OFF);

            /* Encrypt msg data in place */
            for (mb_tmp = mb; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
                nbytes = mbuf_len(mb_tmp);

                if (nbytes) {
                    ccccm_update(ccmode, ctx, nonce_ctx, nbytes,
                                 mbuf_data(mb_tmp), mbuf_data(mb_tmp));
                }
            }

            /*
             * Get final signature and then copy it to the transform header
             * Signature field
             */
            ccccm_finalize(ccmode, ctx, nonce_ctx, sig);
            memcpy(msgp + SMB3_AES_TF_SIG_OFF, sig, CCAES_KEY_SIZE_128);

            break;
    }

    // Ideally, should turn off these flags from original mb:
    // (*mb)->m_flags &= ~(M_PKTHDR | M_EOR);
    
    // Return the new mbuf chain
    mb_hdr = mbuf_concatenate(mb_hdr, mb);
    m_fixhdr(mb_hdr);
    mb = mb_hdr;
    
    /*
     * Store it back into the rqp's mbchain_t
     * If it was a compressed request, then mbp should still be pointing at
     * the compressed mbchain_t
     */
    mb_initm(mbp, mb);
    
out:
    if (error) {
        mbuf_freem(mb);
        
        if (mb_hdr != NULL) {
            mbuf_freem(mb_hdr);
        }
    }
    
    ccccm_ctx_clear(ccmode->size, ctx);
    ccccm_nonce_clear(ccmode->size, nonce_ctx);
    
    ccgcm_ctx_clear(gcmode->size, gcm_ctx);

    SMB_LOG_KTRACE(SMB_DBG_RQ_ENCRYPT | DBG_FUNC_END,
                   error, 0, 0, 0, 0);

    return (error);
}

/*
 * Decrypts an SMB msg or msg chain given in 'mb'.
 * Note: On any error the mbuf chain is freed.
 */
int smb3_msg_decrypt(struct smb_session *sessionp, mbuf_t *mb)
{
    SMB3_AES_TF_HEADER      *tf_hdr;
    mbuf_t                  mb_hdr, mb_tmp, mbuf_payload;
    uint16_t                i16;
    uint64_t                i64;
    uint32_t                msglen = 0;
    int                     error;
    unsigned char           *msgp;
    unsigned char           sig[SMB3_AES_TF_SIG_LEN] = {0};
    const struct ccmode_ccm *ccmode = ccaes_ccm_decrypt_mode();
    const struct ccmode_gcm *gcmode = ccaes_gcm_decrypt_mode();
    size_t                  nbytes, ncopy_bytes;
    uint32_t                mbuf_cnt = 0;
#if TIMING_ON
    struct timespec         start, stop;

    nanotime(&start);
#endif

    SMB_LOG_KTRACE(SMB_DBG_RQ_DECRYPT | DBG_FUNC_START,
                   0, 0, 0, 0, 0);

    switch (sessionp->session_smb3_encrypt_ciper) {
        case SMB2_ENCRYPTION_AES_128_GCM:
        case SMB2_ENCRYPTION_AES_256_GCM:
            break;

        case SMB2_ENCRYPTION_AES_128_CCM:
        case SMB2_ENCRYPTION_AES_256_CCM:
            break;

        default:
            SMBERROR("Unknown cipher %d defaulting to SMB2_ENCRYPTION_AES_128_CCM \n",
                     sessionp->session_smb3_encrypt_ciper);
            sessionp->session_smb3_encrypt_ciper = SMB2_ENCRYPTION_AES_128_CCM;
            break;
    }

    mbuf_payload = NULL;
    mb_hdr = NULL;
    error = 0;
    
    /* Declare ciphers */
    ccccm_ctx_decl(ccmode->size, ctx);
    ccccm_nonce_decl(ccmode->nonce_size, nonce_ctx);
    
    ccgcm_ctx_decl(gcmode->size, gcm_ctx);

    if (!sessionp->session_smb3_decrypt_key_len) {
        /* Cannot decrypt without a key */
        SMBDEBUG("smb3 decr, no key\n");
        error = EAUTH;
        goto out;
    }
    
    mb_hdr = *mb;
    
    // Split TF header from payload
    if (mbuf_split(mb_hdr, SMB3_AES_TF_HDR_LEN, MBUF_WAITOK, &mbuf_payload)) {
        /* Split failed, avoid freeing mb twice during cleanup */
        mb_hdr = NULL;
        error = EBADRPC;
        goto out;
    }
    
    // Pullup Transform header (52 bytes)
    if (mbuf_pullup(&mb_hdr, SMB3_AES_TF_HDR_LEN)) {
        error = EBADRPC;
        goto out;
    }
    
    msgp = mbuf_data(mb_hdr);
    tf_hdr = (SMB3_AES_TF_HEADER *)msgp;
    
    // Verify the protocol signature
    if (bcmp(msgp, SMB3_AES_TF_PROTO_STR, SMB3_AES_TF_PROTO_LEN) != 0) {
        SMBDEBUG("TF HDR protocol incorrect: %02x %02x %02x %02x\n",
                 msgp[0], msgp[1], msgp[2], msgp[3]);
        error = EBADRPC;
        goto out;
    }
    
    /*
     * [MS-SMB2] 2.2.41
     * For SMB 3.1.1, this field is Flags and set to Encrypted (0x0001)
     * For SMB 3.0.x, this field is EncryptionAlgorithm and set to
     * SMB2_ENCRYPTION_AES128-CCM (0x0001)
     * So, in both cases, we just need to check that its set to 0x0001
     *
     * Verify the encryption algorithm
     */
    i16 = letohs(tf_hdr->encrypt_algorithm);
    if (i16 != SMB2_ENCRYPTION_AES128_CCM) {
        SMBDEBUG("Unsupported ENCR alg: %u\n", (uint32_t)i16);
        error = EAUTH;
        goto out;
    }
    
    // Verify Session Id
    i64 = letohq(tf_hdr->sess_id);
    if (i64 != sessionp->session_session_id) {
        SMBDEBUG("TF sess_id mismatch: expect: %llu got: %llu\n",
                 sessionp->session_session_id, i64);
        error = EAUTH;
        goto out;
    }
    
    // Need msglen from tf header for cypher init
    msglen = letohl(tf_hdr->orig_msg_size);
    if (msglen > kDefaultMaxIOSize) {
        SMBERROR("msglen<%d> > kDefaultMaxIOSize <%d>", msglen, kDefaultMaxIOSize);
        error = EIO;
        goto out;
    }
    
    // Init the cipher
    switch (sessionp->session_smb3_encrypt_ciper) {
        case SMB2_ENCRYPTION_AES_128_GCM:
        case SMB2_ENCRYPTION_AES_256_GCM:
            /* Init the cipher with key */
            ccgcm_init(gcmode, gcm_ctx,
                       sessionp->session_smb3_decrypt_key_len,
                       sessionp->session_smb3_decrypt_key);
            
            /* Set Initialization Values */
            ccgcm_set_iv(gcmode, gcm_ctx,
                         SMB3_GCM_NONCE_LEN,
                         msgp + SMB3_AES_TF_NONCE_OFF);
            
            /* Calculate Signature of Authenticated Data + Payload */
            ccgcm_aad(gcmode, gcm_ctx, SMB3_AES_AUTHDATA_LEN,
                      msgp + SMB3_AES_AUTHDATA_OFF);
           break;
            
        case SMB2_ENCRYPTION_AES_128_CCM:
        case SMB2_ENCRYPTION_AES_256_CCM:
            /* Init the cipher with key */
            ccccm_init(ccmode, ctx,
                       sessionp->session_smb3_decrypt_key_len,
                       sessionp->session_smb3_decrypt_key);
            
            /* Set Initialization Values */
           ccccm_set_iv(ccmode, ctx, nonce_ctx, SMB3_CCM_NONCE_LEN, msgp + SMB3_AES_TF_NONCE_OFF,
                         SMB3_AES_TF_SIG_LEN, SMB3_AES_AUTHDATA_LEN, msglen);

            /* Calculate Signature of Authenticated Data + Payload */
            ccccm_cbcmac(ccmode, ctx, nonce_ctx, SMB3_AES_AUTHDATA_LEN,
                         msgp + SMB3_AES_AUTHDATA_OFF);
           break;
    }

    if (msglen > (64 * 1024)) {
        /*
         * <51533800>
         * Is incoming message big enough to use a malloc'd buffer?
         *
         * For incoming encrypted data, the NICs below us tend to give us non
         * 8 byte aligned mbufs. When we try to decrypt in place in each mbuf,
         * that non alignment causes performance to be dreadful. To avoid this
         * we end up doing the decrypt in a malloc'd buffer.
         *
         * Note: we can use a single malloc'd buffer because the iod thread
         * is the only thread decrypting incoming traffic. It has to decrypt
         * it before we are able to find the request rqp it belongs to.
         */
        if ((sessionp->decrypt_bufferp != NULL) &&
            (msglen > sessionp->decrypt_buf_len)) {
            SMB_FREE_DATA(sessionp->decrypt_bufferp, sessionp->decrypt_buf_len);
            sessionp->decrypt_buf_len = 0;
        }

        if (sessionp->decrypt_bufferp == NULL) {
            SMB_MALLOC_DATA(sessionp->decrypt_bufferp, msglen, Z_WAITOK);
            if (sessionp->decrypt_bufferp == NULL) {
                SMBERROR("malloc for buffer failed \n");
                error = EAUTH;
                goto out;
            }
           sessionp->decrypt_buf_len = msglen;
        }

        /* copy the mbuf data into local buffer */
        ncopy_bytes = 0;
        for (mb_tmp = mbuf_payload; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
            nbytes = mbuf_len(mb_tmp);

            if (nbytes) {
                if ((ncopy_bytes + nbytes) <= sessionp->decrypt_buf_len) {
                    bcopy(mbuf_data(mb_tmp), &sessionp->decrypt_bufferp[ncopy_bytes], nbytes);
                    ncopy_bytes += nbytes;
                }
                else {
                    error = EAUTH;
                    goto out;
                }
            }
        }

#if TIMING_ON
        nanotime(&stop);
        SMBERROR("After bcopy to buffer - Reply len %u. elapsed time %ld secs, %ld micro_secs\n",
                 msglen,
                 stop.tv_sec - start.tv_sec,
                 (stop.tv_nsec - start.tv_nsec) / 1000
                 );
#endif

        /* decrypt the local buffer */
        switch (sessionp->session_smb3_encrypt_ciper) {
            case SMB2_ENCRYPTION_AES_128_GCM:
            case SMB2_ENCRYPTION_AES_256_GCM:
                ccgcm_update(gcmode, gcm_ctx, msglen,
                             sessionp->decrypt_bufferp, sessionp->decrypt_bufferp);
               break;
                
            case SMB2_ENCRYPTION_AES_128_CCM:
            case SMB2_ENCRYPTION_AES_256_CCM:
                /* decrypt the local buffer */
                ccccm_update(ccmode, ctx, nonce_ctx, msglen,
                             sessionp->decrypt_bufferp, sessionp->decrypt_bufferp);
               break;
        }

        /* copy the local buffer back into mbuf chain */
        ncopy_bytes = 0;
        for (mb_tmp = mbuf_payload; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
            nbytes = mbuf_len(mb_tmp);

            if (nbytes) {
                if ((ncopy_bytes + nbytes) <= sessionp->decrypt_buf_len) {
                    bcopy(&sessionp->decrypt_bufferp[ncopy_bytes], mbuf_data(mb_tmp), nbytes);
                    ncopy_bytes += nbytes;
                }
                else {
                    error = EAUTH;
                    goto out;
                }
            }
        }
    }
    else {
        /* Decrypt msg data in place */
        for (mb_tmp = mbuf_payload; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
            mbuf_cnt += 1;
            nbytes = mbuf_len(mb_tmp);

            if (nbytes) {
                switch (sessionp->session_smb3_encrypt_ciper) {
                    case SMB2_ENCRYPTION_AES_128_GCM:
                    case SMB2_ENCRYPTION_AES_256_GCM:
                        ccgcm_update(gcmode, gcm_ctx, nbytes,
                                     mbuf_data(mb_tmp), mbuf_data(mb_tmp));
                       break;
                        
                    case SMB2_ENCRYPTION_AES_128_CCM:
                    case SMB2_ENCRYPTION_AES_256_CCM:
                        /* decrypt the local buffer */
                        ccccm_update(ccmode, ctx, nonce_ctx, nbytes,
                                     mbuf_data(mb_tmp), mbuf_data(mb_tmp));
                       break;
                }
            }
        }
    }

    /* Final signature -> sig */
    switch (sessionp->session_smb3_encrypt_ciper) {
        case SMB2_ENCRYPTION_AES_128_GCM:
        case SMB2_ENCRYPTION_AES_256_GCM:
            ccgcm_finalize(gcmode, gcm_ctx, CCAES_KEY_SIZE_128, sig);
           break;
            
        case SMB2_ENCRYPTION_AES_128_CCM:
        case SMB2_ENCRYPTION_AES_256_CCM:
            ccccm_finalize(ccmode, ctx, nonce_ctx, sig);
           break;
    }

    // Check signature
    if (cc_cmp_safe(SMB3_AES_TF_SIG_LEN, sig, tf_hdr->signature)) {
        SMBDEBUG("Transform signature mismatch\n");
        
        SMBDEBUG("TF Sig: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 tf_hdr->signature[0], tf_hdr->signature[1], tf_hdr->signature[2], tf_hdr->signature[3],
                 tf_hdr->signature[4], tf_hdr->signature[5], tf_hdr->signature[6], tf_hdr->signature[7],
                 tf_hdr->signature[8], tf_hdr->signature[9], tf_hdr->signature[10], tf_hdr->signature[11],
                 tf_hdr->signature[12], tf_hdr->signature[13], tf_hdr->signature[14], tf_hdr->signature[15]);
        
        SMBDEBUG("CalcSig: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                 sig[0], sig[1], sig[2],sig[3], sig[4], sig[5], sig[6], sig[7],
                 sig[8], sig[9], sig[10],sig[11], sig[12], sig[13], sig[14], sig[15]);
        error = EAUTH;
        goto out;
    }
    
    /* And we're done, return plain text */
    *mb = mbuf_payload;
    
out:
    if (error) {
        if (mbuf_payload) {
            mbuf_freem(mbuf_payload);
        }
    }
    
    if (mb_hdr != NULL) {
        mbuf_freem(mb_hdr);
    }
    
    ccccm_ctx_clear(ccmode->size, ctx);
    ccccm_nonce_clear(ccmode->size, nonce_ctx);
    
    ccgcm_ctx_clear(gcmode->size, gcm_ctx);

    SMB_LOG_KTRACE(SMB_DBG_RQ_DECRYPT | DBG_FUNC_END,
                   error, 0, 0, 0, 0);

    return (error);
}

static void smb3_init_nonce(struct smb_session *sessionp)
{
    MD5_CTX md5;
    struct timespec tnow;
    uint64_t nsec;
    unsigned char digest[16];
    
    /*
     * We will whip up a nonce by generating a simple
     * HMAC-MD5 digest of system time and the session_id,
     * keyed by the session key. Nothing fancy.
     */
    
    MD5Init(&md5);
    
    nanouptime(&tnow);
    nsec = tnow.tv_sec;

    MD5Update(&md5, sessionp->session_mackey, sessionp->session_mackeylen);
    MD5Update(&md5, (uint8_t *)&nsec, sizeof(uint64_t));
    MD5Update(&md5, &sessionp->session_session_id, sizeof(uint64_t));
    MD5Final(digest, &md5);
    
    memcpy(&sessionp->session_smb3_nonce_high, digest, 8);
    memcpy(&sessionp->session_smb3_nonce_low, &digest[8], 8);
}

static int
smb_kdf_hmac_sha256(uint8_t *input_key, uint32_t input_keylen,
                    uint8_t *label, uint32_t label_len,
                    uint8_t *context, uint32_t context_len,
                    uint8_t *output_key, uint32_t output_key_len)
{
    int err;
    const struct ccdigest_info *di = ccsha256_di();
    
    err = ccnistkdf_ctr_hmac(di,
                             input_keylen, input_key,
                             label_len, label,
                             context_len, context,
                             output_key_len, output_key);
    return (err);
}

/*
 * int smb3_derive_keys(struct smbiod *iod)
 *
 * Derives all necessary keys for SMB 3 signing and encryption, which are all
 * stored in the smb_session struct.
 *
 * Keys are derived using KDF in Counter Mode from as specified by sp800-108.
 *
 * Keys generated:
 *      session_smb3_signing_key
 *      session_smb3_encrypt_key
 *      session_smb3_decrypt_key
 *
 */
int smb3_derive_keys(struct smbiod *iod)
{
    uint8_t label[16];
    uint8_t context[16];
    int err;
    struct smb_session *sessionp = NULL;
    uint32_t key_len = SMB3_KEY_LEN;

    if (iod == NULL) {
        SMBERROR("iod is null? \n");
        return(EINVAL);
    }
    sessionp = iod->iod_session;

    sessionp->session_smb3_signing_key_len = 0;
    sessionp->session_smb3_encrypt_key_len = 0;
    sessionp->session_smb3_decrypt_key_len = 0;
    
    // Setup session nonce
    smb3_init_nonce(sessionp);
    
    // Check Session.SessionKey
    if (sessionp->session_mackey == NULL) {
        SMBDEBUG("Keys not generated, missing session key\n");
        err = EINVAL;
        goto out;
    }

    if (sessionp->session_mackeylen < SMB3_KEY_LEN) {
        SMBDEBUG("Warning: Session.SessionKey too small, len: %u\n",
                 sessionp->session_mackeylen);
    }
    
    // Derive Session.SigningKey (session_smb3_signing_key)
    memset(label, 0, sizeof(label));

    if (sessionp->session_flags & SMBV_SMB311) {
        /*
         * For SMB 3.1.1, the label changed names and the context is now
         * the pre auth integrity hash
         */
        strncpy((char *)label, "SMBSigningKey", sizeof(label));

        err = smb_kdf_hmac_sha256(sessionp->session_mackey, sessionp->session_mackeylen,
                                  label, 14,  // includes NULL Terminator
                                  &iod->iod_pre_auth_int_hash[0], SHA512_DIGEST_LENGTH,
                                  sessionp->session_smb3_signing_key,
                                  SMB3_KEY_LEN);
    }
    else {
        memset(context, 0, sizeof(context));

        strncpy((char *)label, "SMB2AESCMAC", sizeof(label));
        strncpy((char *)context, "SmbSign", sizeof(context));

        err = smb_kdf_hmac_sha256(sessionp->session_mackey, sessionp->session_mackeylen,
                                  label, 12,  // includes NULL Terminator
                                  context, 8, // includes NULL Terminator
                                  sessionp->session_smb3_signing_key,
                                  SMB3_KEY_LEN);
    }

    if (!err) {
        sessionp->session_smb3_signing_key_len = SMB3_KEY_LEN;
    } else {
        SMBDEBUG("Could not generate smb3 signing key, error: %d\n", err);
    }
    
    // Derive Session.EncryptionKey (session_smb3_encrypt_key)
    memset(label, 0, sizeof(label));

    if (sessionp->session_flags & SMBV_SMB311) {
        /*
         * For SMB 3.1.1, the label changed names and the context is now
         * the pre auth integrity hash
         */
        strncpy((char *)label, "SMBC2SCipherKey", sizeof(label));
        
        if ((sessionp->session_smb3_encrypt_ciper == SMB2_ENCRYPTION_AES_256_CCM) ||
            (sessionp->session_smb3_encrypt_ciper == SMB2_ENCRYPTION_AES_256_GCM)) {
            /* [MS-SMB2] 3.2.5.3.1 Use the Session.FullSessionKey */
            key_len = SMB3_256BIT_KEY_LEN;
            err = smb_kdf_hmac_sha256(sessionp->full_session_mackey,
                                      sessionp->full_session_mackeylen,
                                      label, 16,  // includes NULL Terminator
                                      iod->iod_pre_auth_int_hash, SHA512_DIGEST_LENGTH,
                                      sessionp->session_smb3_encrypt_key,
                                      key_len);
       }
        else {
            key_len = SMB3_KEY_LEN;
            err = smb_kdf_hmac_sha256(sessionp->session_mackey,
                                      sessionp->session_mackeylen,
                                      label, 16,  // includes NULL Terminator
                                      iod->iod_pre_auth_int_hash, SHA512_DIGEST_LENGTH,
                                      sessionp->session_smb3_encrypt_key,
                                      key_len);
        }
    }
    else {
        memset(context, 0, sizeof(context));

        strncpy((char *)label, "SMB2AESCCM", sizeof(label));
        strncpy((char *)context, "ServerIn ", sizeof(context));

        key_len = SMB3_KEY_LEN;
        err = smb_kdf_hmac_sha256(sessionp->session_mackey, sessionp->session_mackeylen,
                                  label, 11,  // includes NULL Terminator
                                  context, 10, // includes NULL Terminator
                                  sessionp->session_smb3_encrypt_key,
                                  key_len);
    }

    if (!err) {
        sessionp->session_smb3_encrypt_key_len = key_len;
    } else {
        SMBDEBUG("Could not generate smb3 encrypt key, error: %d\n", err);
    }
    
    // Derive Session.DecryptionKey (session_smb3_decrypt_key)
    memset(label, 0, sizeof(label));

    if (sessionp->session_flags & SMBV_SMB311) {
        /*
         * For SMB 3.1.1, the label changed names and the context is now
         * the pre auth integrity hash
         */
        strncpy((char *)label, "SMBS2CCipherKey", sizeof(label));

        if ((sessionp->session_smb3_encrypt_ciper == SMB2_ENCRYPTION_AES_256_CCM) ||
            (sessionp->session_smb3_encrypt_ciper == SMB2_ENCRYPTION_AES_256_GCM)) {
            /* [MS-SMB2] 3.2.5.3.1 Use the Session.FullSessionKey */
            key_len = SMB3_256BIT_KEY_LEN;
            err = smb_kdf_hmac_sha256(sessionp->full_session_mackey,
                                      sessionp->full_session_mackeylen,
                                      label, 16,  // includes NULL Terminator
                                      iod->iod_pre_auth_int_hash, SHA512_DIGEST_LENGTH,
                                      sessionp->session_smb3_decrypt_key,
                                      key_len);
        }
        else {
            key_len = SMB3_KEY_LEN;
            err = smb_kdf_hmac_sha256(sessionp->session_mackey,
                                      sessionp->session_mackeylen,
                                      label, 16,  // includes NULL Terminator
                                      iod->iod_pre_auth_int_hash, SHA512_DIGEST_LENGTH,
                                      sessionp->session_smb3_decrypt_key,
                                      key_len);
        }
    }
    else {
        memset(context, 0, sizeof(context));

        strncpy((char *)label, "SMB2AESCCM", sizeof(label));
        strncpy((char *)context, "ServerOut", sizeof(context));

        key_len = SMB3_KEY_LEN;
        
        err = smb_kdf_hmac_sha256(sessionp->session_mackey, sessionp->session_mackeylen,
                                  label, 11,  // includes NULL Terminator
                                  context, 10, // includes NULL Terminator
                                  sessionp->session_smb3_decrypt_key,
                                  key_len);
    }

    if (!err) {
        sessionp->session_smb3_decrypt_key_len = key_len;
    } else {
        SMBDEBUG("Could not generate smb3 decrypt key, error: %d\n", err);
    }

out:
    return (err);
}

/*
 * int smb3_derive_channel_keys(struct smbiod *iod)
 *
 * Derives all necessary keys for SMB 3 alternate channel (Session Binding),
 * which are all stored in the iod struct.
 *
 * For a session binding, we derive a new signing key for this channel, but
 * copy the encryption/decryption keys from the main channel.
 *
 * Keys are derived using KDF in Counter Mode from as specified by sp800-108.
 *
 * Keys generated:
 *      iod_smb3_signing_key
 *
 */
int smb3_derive_channel_keys(struct smbiod *iod)
{
    uint8_t label[16];
    uint8_t context[16];
    int err;
    struct smb_session *sessionp = NULL;

    if (iod == NULL) {
        SMBERROR("iod is null? \n");
        return(EINVAL);
    }
    sessionp = iod->iod_session;

    // Check iod.SessionKey
    if (iod->iod_mackey == NULL) {
        SMBDEBUG("Keys not generated, missing iod key\n");
        return(EINVAL);
    }

    if (iod->iod_mackeylen < SMB3_KEY_LEN) {
        SMBDEBUG("Warning: iod.SessionKey too small, len: %u\n",
                 iod->iod_mackeylen);
    }

    iod->iod_smb3_signing_key_len = 0;
    
    // Derive iod.SigningKey (iod_smb3_signing_key)
    memset(label, 0, sizeof(label));

    if (sessionp->session_flags & SMBV_SMB311) {
        /*
         * For SMB 3.1.1, the label changed names and the context is now
         * the pre auth integrity hash
         */
        strncpy((char *)label, "SMBSigningKey", sizeof(label));

        err = smb_kdf_hmac_sha256(iod->iod_mackey, iod->iod_mackeylen,
                                  label, 14,  // includes NULL Terminator
                                  &iod->iod_pre_auth_int_hash[0], SHA512_DIGEST_LENGTH,
                                  iod->iod_smb3_signing_key,
                                  SMB3_KEY_LEN);
    }
    else {
        memset(context, 0, sizeof(context));

        strncpy((char *)label, "SMB2AESCMAC", sizeof(label));
        strncpy((char *)context, "SmbSign", sizeof(context));

        err = smb_kdf_hmac_sha256(iod->iod_mackey, iod->iod_mackeylen,
                                  label, 12,  // includes NULL Terminator
                                  context, 8, // includes NULL Terminator
                                  iod->iod_smb3_signing_key,
                                  SMB3_KEY_LEN);
    }

    if (!err) {
        iod->iod_smb3_signing_key_len = SMB3_KEY_LEN;
    } else {
        SMBERROR("Could not generate smb3 signing key, error: %d\n", err);
    }
    
    return (err);
}

int
smb311_pre_auth_integrity_hash_init(struct smbiod *iod,
                                    uint16_t command, mbuf_t m0)
{
    SHA512_CTX pre_auth_integrity_ctx;
    uint8_t init_zero[64] = {0};
    mbuf_t m = m0;

    if (iod == NULL) {
        return(EINVAL);
    }

    bzero(&pre_auth_integrity_ctx, sizeof(pre_auth_integrity_ctx));

    /* Initialize the context */
    SHA512_Init(&pre_auth_integrity_ctx);

    if (command == SMB2_NEGOTIATE) {
        /*
         * This does the N = H(N1 || NegRequest), where N1 is 64 bytes of zeros
         * m0 is the Negotiate request
         * Start with 64 Zero bytes
         */
        SHA512_Update(&pre_auth_integrity_ctx, init_zero, 64);
    }
    else {
        /*
         * This does the S = H(N || SessSetupRequest), where N is Neg hash
         * m0 is the Session Setup request
         * Start with negotiate hash
         */
        SHA512_Update(&pre_auth_integrity_ctx,
                      iod->iod_pre_auth_int_hash_neg,
                      SHA512_DIGEST_LENGTH);
    }

    /* Or in the Request data */
    while (m) {
        if (mbuf_len(m) > 0) {
            /* Update the hash */
            SHA512_Update(&pre_auth_integrity_ctx, mbuf_data(m), mbuf_len(m));
        }
        m = mbuf_next(m);
    }

    /* Output the hash */
    SHA512_Final(iod->iod_pre_auth_int_hash, &pre_auth_integrity_ctx);

    return(0);
}

int
smb311_pre_auth_integrity_hash_update(struct smbiod *iod, mbuf_t m0)
{
    SHA512_CTX pre_auth_integrity_ctx;
    mbuf_t m = m0;

    if (iod == NULL) {
        return(EINVAL);
    }

    bzero(&pre_auth_integrity_ctx, sizeof(pre_auth_integrity_ctx));

    /* Initialize the context */
    SHA512_Init(&pre_auth_integrity_ctx);

    /* Set the previous hash value */
    SHA512_Update(&pre_auth_integrity_ctx,
                  iod->iod_pre_auth_int_hash,
                  SHA512_DIGEST_LENGTH);

    while (m) {
        if (mbuf_len(m) > 0) {
            /* Update the hash */
            SHA512_Update(&pre_auth_integrity_ctx, mbuf_data(m), mbuf_len(m));
        }
        m = mbuf_next(m);
    }

    /* Output the hash */
    SHA512_Final(iod->iod_pre_auth_int_hash, &pre_auth_integrity_ctx);

    return(0);
}

int
smb311_pre_auth_integrity_hash_print(struct smbiod *iod)
{
    if (iod == NULL) {
        return(EINVAL);
    }

#ifdef SMB_DEBUG
    SMBERROR("0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x \n",
             iod->iod_pre_auth_int_hash[0],
             iod->iod_pre_auth_int_hash[1],
             iod->iod_pre_auth_int_hash[2],
             iod->iod_pre_auth_int_hash[3],
             iod->iod_pre_auth_int_hash[4],
             iod->iod_pre_auth_int_hash[5],
             iod->iod_pre_auth_int_hash[6],
             iod->iod_pre_auth_int_hash[7],
             iod->iod_pre_auth_int_hash[8],
             iod->iod_pre_auth_int_hash[9],
             iod->iod_pre_auth_int_hash[10],
             iod->iod_pre_auth_int_hash[11],
             iod->iod_pre_auth_int_hash[12],
             iod->iod_pre_auth_int_hash[13],
             iod->iod_pre_auth_int_hash[14],
             iod->iod_pre_auth_int_hash[15]
             );
#endif

    return(0);
}

#if 0
void
smb_test_crypt_performance(struct smb_session *sessionp,
						   size_t orig_packet_len,
						   size_t orig_mb_len)
{
	char *cptr = NULL, *tptr;
	const struct ccmode_cbc *ccmode = ccaes_cbc_encrypt_mode();
	size_t packet_len = orig_packet_len;	/* buffer to encrypt */
	size_t mb_len = orig_mb_len;			/* number of bytes to encrypt at a time */
	u_char *mac;
	struct timespec	start, stop;
	int error;
	
	/* Malloc buffer to encrypt*/
    SMB_MALLOC_DATA(cptr, orig_packet_len, Z_WAITOK);
	if (cptr == NULL) {
		SMBERROR("Out of memory\n");
		return;
	}
	
	/* Init the cipher */
	cccmac_mode_decl(ccmode, cmac);
	error = cccmac_init(ccmode, cmac, 16, sessionp->session_smb3_signing_key);
	if (error) {
		SMBERROR("cccmac_init failed %d \n", error);
		return;
	}
	
    SMB_MALLOC_DATA(mac, CMAC_BLOCKSIZE, Z_WAITOK);
	if (mac == NULL) {
		SMBERROR("Out of memory\n");
		return;
	}
	
	tptr = cptr;
	
	nanotime(&start);
	
	while (packet_len > 0) {
		/*
		 * Only replies use packet_len. See if the reply ends before the end
		 * of this mbuf. If so, then only process up to the end of the reply.
		 */
		if (packet_len > 0) {
			if (packet_len < mb_len) {
				mb_len = packet_len;
			}
		}

		//SMBERROR("packet_len %ld, mb_len %ld\n", packet_len, mb_len);

		/* Process this m_buf's data */
		error = cccmac_update(cmac, mb_len, tptr);
		if (error) {
			SMBERROR("cccmac_update failed %d \n", error);
			break;
		}
		
		/* decrement how much of the reply we have left */
		if (packet_len > 0) {
			packet_len -= mb_len;
			tptr += mb_len;
		}
	}
	
	//SMBERROR("Calling cccmac_final_generate \n");
	error = cccmac_final_generate(cmac, CMAC_BLOCKSIZE, mac);
	if (error) {
		SMBERROR("cccmac_final_generate failed %d \n", error);
	}
	
	nanotime(&stop);
	
	SMBERROR("Block size %ld done in %ld bytes. elapsed time %ld secs, %ld micro_secs\n",
			 orig_packet_len, orig_mb_len,
			 stop.tv_sec - start.tv_sec,
			 (stop.tv_nsec - start.tv_nsec) / 1000
			 );
	
	/* Replies only check */
	if (packet_len != 0) {
		SMBERROR("Not enough bytes in packet? <%zu>\n", packet_len);
	}
	
	if (mac) {
        SMB_FREE_DATA(mac, CMAC_BLOCKSIZE);
	}
	
	if (cptr) {
        SMB_FREE_DATA(cptr, orig_packet_len);
	}
}
#endif

int smb2_compress_data(uint8_t *data_startp, uint32_t data_len,
                       uint8_t *compress_startp, uint32_t compress_len,
                       uint16_t algorithm, int compress_flag,
                       size_t *actual_len)
{
    int error = 0;
    size_t scratch_size = 0;
    char *scratch_bufferp = NULL;
    size_t actual_size = 0;
    compression_algorithm compress_algorithm = 0;
#if COMPRESSION_PERFORMANCE
    struct timespec start, stop;
    nanotime(&start);
#endif

    switch(algorithm) {
        case SMB2_COMPRESSION_LZNT1:
            compress_algorithm = COMPRESSION_SMB_LZNT1;
            break;
    
        case SMB2_COMPRESSION_LZ77:
            compress_algorithm = COMPRESSION_SMB_LZ77;
            break;

        case SMB2_COMPRESSION_LZ77_HUFFMAN:
            compress_algorithm = COMPRESSION_SMB_LZ77H;
            break;

        default:
            SMBERROR("Unknown algorithm %d \n", algorithm);
            error = EINVAL;
            goto bad;
    }
    
    if (compress_flag == COMPRESSION_STREAM_ENCODE) {
        scratch_size = compression_encode_scratch_buffer_size(compress_algorithm);
    }
    else {
        scratch_size = compression_decode_scratch_buffer_size(compress_algorithm);
    }

    /* Malloc scratch buffer if needed */
    if (scratch_size > 0) {
        SMB_MALLOC_DATA(scratch_bufferp, scratch_size, Z_WAITOK);
        if (scratch_bufferp == NULL) {
            error = ENOMEM;
            goto bad;
        }
    }

    SMB_LOG_COMPRESS("compress_algorithm %u scratch buffer size %zu compress_len %d data_len %d\n",
                     compress_algorithm, scratch_size, compress_len, data_len);

#if COMPRESSION_PERFORMANCE
    nanotime(&stop);
    
    SMBERROR("%s start. algorithm %d data_len %u compress_len %d elapsed %ld:%ld \n",
             (compress_flag == COMPRESSION_STREAM_ENCODE) ? "Encode" : "Decode",
             algorithm, data_len, compress_len,
             stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif

    if (compress_flag == COMPRESSION_STREAM_ENCODE) {
        actual_size = compression_encode_buffer(compress_startp, compress_len,
                                                data_startp, data_len,
                                                scratch_bufferp, compress_algorithm);
    }
    else {
        actual_size = compression_decode_buffer(data_startp, data_len,
                                                compress_startp, compress_len,
                                                scratch_bufferp, compress_algorithm);
    }
#if COMPRESSION_PERFORMANCE
    nanotime(&stop);
    
    SMBERROR("%s done. actual_size %zu elapsed %ld:%ld \n",
             (compress_flag == COMPRESSION_STREAM_ENCODE) ? "Encode" : "Decode",
             actual_size,
             stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif

    if (actual_size == 0) {
        if (compress_flag == COMPRESSION_STREAM_ENCODE) {
            /*
             * We can get an (actual_size == 0) when the compressed data is
             * going to end up larger than the original data. This is not
             * really an error to log but it does mean that we should just
             * send the uncompressed write data instead
             */
            SMB_LOG_COMPRESS("Compression failed \n");
        }
        else {
            /* Being unable to decompress read data is an error */
            SMBERROR("Decompression failed \n");
            
            error = EINVAL;
            goto bad;
        }
    }
    else {
        SMB_LOG_COMPRESS("actual_size %zu (0x%zx) \n", actual_size, actual_size);
    }

    /* Compression worked, return actual compression size */
    *actual_len = actual_size;
    
    error = 0;

bad:
    if (scratch_bufferp != NULL) {
        SMB_FREE_DATA(scratch_bufferp, scratch_size);
    }
    
    return(error);
}

int
smb2_rq_compress_chunk(struct smb_session *sessionp, uint16_t algorithm,
                       uint8_t *write_bufferp, uint32_t write_buffer_len,
                       uint32_t *forward_data_repetitions, char *forward_data_char,
                       uint32_t *backward_data_repetitions, char *backward_data_char,
                       uint8_t *compress_startp, uint32_t *data_len,
                       size_t *actual_len)
{
    int error = 0;
    uint32_t i = 0;
    uint32_t backward_data_stop = 0;
    uint8_t *data_startp = NULL;
    uint32_t compress_len = 0, RemainingChunkSize = write_buffer_len;
#if COMPRESSION_PERFORMANCE
    struct timespec start, stop;
    nanotime(&start);
#endif

    *data_len = 0;
    *actual_len = 0;
    *forward_data_repetitions = 1;
    *backward_data_repetitions = 1;

    /*
     * Scan for forward data pattern
     */
    *forward_data_char = write_bufferp[0];
    *forward_data_repetitions = 1;
    for (i = 1; i < write_buffer_len; i++) {
        if (write_bufferp[i] == *forward_data_char) {
            *forward_data_repetitions += 1;
        }
        else {
            break;
        }
    }
    
    if (*forward_data_repetitions > 1) {
        sessionp->write_cnt_fwd_pattern += 1;

        /* Update uncompressed data length */
        RemainingChunkSize -= *forward_data_repetitions;
        SMB_LOG_COMPRESS("Forward Pattern: forward_data_repetitions %d (0x%x) RemainingChunkSize %d \n",
                         *forward_data_repetitions,
                         *forward_data_repetitions,
                         RemainingChunkSize);

#if COMPRESSION_PERFORMANCE
        nanotime(&stop);
    
        SMBERROR("Forward. forward_data_repetitions %d RemainingChunkSize %u elapsed %ld:%ld\n",
                 forward_data_repetitions, RemainingChunkSize,
                 stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif
    }

    /*
     * Any uncompressed data left to check for backwards data pattern?
     */
    if (RemainingChunkSize && (write_buffer_len >= 2)) {
        /*
         * Scan for backwards data pattern
         */
        if (*forward_data_repetitions > 1) {
            /* adjust for write buffer offset starting at 0 */
            backward_data_stop = *forward_data_repetitions - 1;
        }
        else {
            /*
             * Can skip write_bufferp[0] since forward scan would have
             * covered that case
             */
            backward_data_stop = 1;
        }
        
        *backward_data_char = write_bufferp[write_buffer_len - 1];
        *backward_data_repetitions = 1;
        for (i = write_buffer_len - 2; i > backward_data_stop; i--) {
            if (write_bufferp[i] == *backward_data_char) {
                *backward_data_repetitions += 1;
            }
            else {
                break;
            }
        }
        
        if (*backward_data_repetitions > 1) {
            sessionp->write_cnt_bwd_pattern += 1;

            /* Update uncompressed data length */
            RemainingChunkSize -= *backward_data_repetitions;
            SMB_LOG_COMPRESS("Backward Pattern: backward_data_repetitions %d (0x%x) RemainingChunkSize %d \n",
                             *backward_data_repetitions,
                             *backward_data_repetitions,
                             RemainingChunkSize);

#if COMPRESSION_PERFORMANCE
            nanotime(&stop);
    
            SMBERROR("Backward. backward_data_repetitions %d RemainingChunkSize %u elapsed %ld:%ld \n",
                     backward_data_repetitions, RemainingChunkSize,
                     stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif
        }
    }

    /*
     * Any uncompressed data left to check for algorithm compression?
     * Start is write_bufferp[forward_data_repetitions - 1]
     * Length is write_buffer_len - backward_data_repetitions
     */
    if (RemainingChunkSize) {
        data_startp = write_bufferp;
        *data_len = write_buffer_len;
        
        /* Any forward data repetitions to skip over? */
        if (*forward_data_repetitions > 1) {
            data_startp += *forward_data_repetitions;
            *data_len -= *forward_data_repetitions;
        }
        
        /* Any backwards data repetitions to skip over? */
        if (*backward_data_repetitions > 1) {
            *data_len -= *backward_data_repetitions;
        }
        
        if (RemainingChunkSize != *data_len) {
            /* Sanity check */
            SMB_LOG_COMPRESS("RemainingChunkSize %u != data_len %u??? \n",
                             RemainingChunkSize, *data_len);
        }
        
        /*
         * Do algorithmic compression here
         * Use preallocated buffer in compress_startp that is same size of
         * original write data with assumption that compressed data will be
         * smaller.
         */
        compress_len = *data_len;
        
        error = smb2_compress_data(data_startp, *data_len,
                                   compress_startp, compress_len,
                                   algorithm, COMPRESSION_STREAM_ENCODE,
                                   actual_len);
        if (error) {
            SMBERROR("smb2_compress_decompress_data failed %d \n", error);
        }
        else {
            /* Update uncompressed data length */
            RemainingChunkSize -= compress_len;

            if (*actual_len == 0) {
                /*
                 * Failed to compress data, so just copy uncompressed data
                 * into compress_startp
                 */
                memcpy(compress_startp, data_startp, compress_len);
                
                SMB_LOG_COMPRESS("None: compress_len %d (0x%x) RemainingChunkSize %d\n",
                                 compress_len, compress_len,
                                 RemainingChunkSize);
                
            }
            else {
                SMB_LOG_COMPRESS("Algorithm %d: compress_len %d (0x%x) RemainingChunkSize %d\n",
                                 algorithm, compress_len, compress_len,
                                 RemainingChunkSize);
            }
            
        }
        
#if COMPRESSION_PERFORMANCE
        nanotime(&stop);
    
        SMBERROR("Algorithm %d. compress_len %d RemainingChunkSize %u elapsed %ld:%ld \n",
                 algorithm, compress_len, RemainingChunkSize,
                 stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif
    }

    return (error);
}


/*
 * SMB 3 Compress a Write request
 */
int
smb2_rq_compress_write(struct smb_rq *rqp)
{
    struct smb_session *sessionp = NULL;
    uint32_t chained_compress = 1, RemainingUncompressedDataSize = 0;
    uint32_t forward_data_repetitions = 0, backward_data_repetitions = 0;
    char forward_data_char = 0, backward_data_char = 0;
    mbuf_t m;
    struct mbchain *original_mbp = NULL, *compressed_mbp = NULL;
    struct mdchain temp_mbdata;
    int error = 0;
    struct smb2_hdr_and_write {
        struct smb2_header smb2_hdr;
        uint16_t structure_size;
        uint16_t data_offset;
        uint32_t length;
        uint64_t offset;
        uint64_t file_id[2];
        uint32_t channel;
        uint32_t remaining_bytes;
        uint16_t write_channel_info_offset;
        uint16_t write_channel_info_length;
        uint32_t flags;
    } __attribute((packed)) hdr_write;
    uint8_t *bufferp = NULL, *write_bufferp = NULL, *data_startp = NULL, *compress_startp = NULL;
    uint32_t originalCompressedSegmentSize = 0, write_buffer_len = 0;
    uint32_t buffer_len = 0, data_len = 0, compress_len = 0;
    size_t actual_len = 0;
    uint16_t algorithm = 0;
    uint32_t chunk_len = 0, one_chunk_compressed = 0;
#if COMPRESSION_PERFORMANCE
    struct timespec start, stop;
    nanotime(&start);
#endif
    
    if (rqp == NULL) {
        SMBERROR("Called with NULL rqp\n");
        return(EINVAL);
    }

    sessionp = rqp->sr_session;
    
    if (sessionp == NULL) {
        SMBERROR("sessionp is NULL\n");
        return(EINVAL);
    }

    /* If not SMB 3, then just return */
    if (!SMBV_SMB3_OR_LATER(sessionp)) {
        /* Should have been checked before calling this function */
        SMB_LOG_COMPRESS("Not SMB 3 or later \n");
        return(0);
    }

    /* Is compression supported by server? */
    if (sessionp->server_compression_algorithms_map == 0) {
        /* Should have been checked before calling this function */
        SMB_LOG_COMPRESS("Server does not support compression \n");
        return(0);
    }

    if (rqp->sr_flags & SMBR_COMPOUND_RQ) {
        /*
         * Compound writes are not compressed since its probably a small
         * write.
         */
        return(0);
    }
    
    if (rqp->sr_command != SMB2_WRITE) {
        SMB_LOG_COMPRESS("Trying to compress non write request <%d>? \n",
                         rqp->sr_command);
        return(0);
    }

    if (rqp->sr_extflags & SMB2_NO_COMPRESS_WRITE) {
        /* This write not allowed to be compressed */
        return(0);
    }

    if (sessionp->server_compression_algorithms_map & SMB2_COMPRESSION_LZ77_HUFFMAN_ENABLED) {
        sessionp->write_cnt_LZ77Huff += 1;
        algorithm = SMB2_COMPRESSION_LZ77_HUFFMAN;
    }
    else {
        if (sessionp->server_compression_algorithms_map & SMB2_COMPRESSION_LZ77_ENABLED) {
            sessionp->write_cnt_LZ77 += 1;
            algorithm = SMB2_COMPRESSION_LZ77;
        }
        else {
            if (sessionp->server_compression_algorithms_map & SMB2_COMPRESSION_LZNT1_ENABLED) {
                sessionp->write_cnt_LZNT1 += 1;
                algorithm = SMB2_COMPRESSION_LZNT1;
            }
            else {
                SMB_LOG_COMPRESS("Unknown server compression 0x%x \n",
                                 sessionp->server_compression_algorithms_map);
                return(0);
            }
        }
    }

    /* Is chained compression supported by server? */
    if (sessionp->session_misc_flags & SMBV_COMPRESSION_CHAINING_OFF) {
        //SMB_LOG_COMPRESS("Chained compression disabled \n");
        chained_compress = 0;
    }

    SMB_LOG_KTRACE(SMB_DBG_WRITE_COMPRESS | DBG_FUNC_START,
                   sessionp->server_compression_algorithms_map,
                   chained_compress,
                   0, 0, 0);

    /*
     * Will need a new mbp to build the compressed request to send out
     * Note that it is attached to rqp->sr_rq_compressed and SMBR_COMPRESSED
     * is set in rqp->sr_flags.
     */
    compressed_mbp = &rqp->sr_rq_compressed;
    if (compressed_mbp == NULL) {
        SMBERROR("sr_rq_compressed is NULL? \n");
        return(EINVAL);
    }
    
    error = mb_init(compressed_mbp);
    if (error) {
        return error;
    }
    
    /* Need a temp_mbdata to parse some data out of original request */
    smb_rq_getrequest(rqp, &original_mbp);
    m = original_mbp->mb_top;
    md_initm(&temp_mbdata, m);    /* DO NOT FREE temp_mbdata! */

    
    /*
     * If its NOT a write, then compress everything.
     * If its a write, then skip first 70 bytes and then compress
     * the rest. At this time we only compress write requests.
     */


    /*
     * Get SMB2 hdr and Write request from original request
     * Windows 11 Client behavior
     * 1. If chained compression, then first compression payload is the
     *    the non compressed SMB2 hdr and write request
     * 2. If non chained compression, then the Offset is set to skip
     *    the non compressed SMB2 hdr and write request thus the compression
     *    is only done to the actual write payload.
     */
    error = md_get_mem(&temp_mbdata, (caddr_t) &hdr_write, sizeof(hdr_write),
                       MB_MSYSTEM);
    if (error) {
        SMBERROR("md_get_mem for header and write failed %d \n", error);
        goto bad;
    }

    if (hdr_write.length == 0) {
        /* Paranoid check */
        SMBERROR("Write length is zero? \n");
        error = EINVAL;
        goto bad;
    }
    
    /*
     * Do we have enough write data to compress?
     * Minimum of two for the data pattern checks later on in this code
     */
    write_buffer_len = hdr_write.length;
    if (write_buffer_len > kDefaultMaxIOSize) {
        /*
         * We should only create IO up to kDefaultMaxIOSize
         */
        SMBERROR("write_buffer_len<%d> > kDefaultMaxIOSize<%d>\n", write_buffer_len, kDefaultMaxIOSize);
        error = E2BIG;
        goto bad;
    }

    if ((write_buffer_len < 2) ||
        (write_buffer_len < sessionp->compression_io_threshold)) {
        /*SMB_LOG_COMPRESS("Write too small to compress %u < %u \n",
                         write_buffer_len, sessionp->compression_io_threshold);*/
        error = 0;
        goto bad;
    }

    SMB_LOG_COMPRESS("Algorithm: %s, %s, MessageID %llu, Write length %d, offset %llu \n",
                     algorithm == SMB2_COMPRESSION_LZNT1 ? "LZNT1" :
                     (algorithm == SMB2_COMPRESSION_LZ77 ? "LZ77" : "LZ77Huffman"),
                     (chained_compress == 1) ? "Chained" : "Nonchained",
                     hdr_write.smb2_hdr.message_id,
                     hdr_write.length, hdr_write.offset);

    if (chained_compress) {
        /* For chained compression, update original segment size */
        originalCompressedSegmentSize += sizeof(hdr_write);
        SMB_LOG_COMPRESS("Chained Hdr/Write length: originalCompressedSegmentSize %d (0x%x)\n",
                         originalCompressedSegmentSize,
                         originalCompressedSegmentSize);
    }

    /*
     * Do just one malloc of (2 x write_buffer_len) to reduce
     * doing a bunch of smaller mallocs. 2x because need one buffer to hold
     * compressed data and another to hold the decompressed data.
     */
    buffer_len = write_buffer_len * 2;
    SMB_MALLOC_DATA(bufferp, buffer_len, Z_WAITOK);
    if (bufferp == NULL) {
        error = ENOMEM;
        goto bad;
    }

    compress_startp = bufferp;
    //compress_len = write_buffer_len;  /* Should get set later */
    
    write_bufferp = bufferp + write_buffer_len;
    //data_len = write_buffer_len;      /* Should get set later */

    /* Copy Write data from original request into local buffer */
    error = md_get_mem(&temp_mbdata, (caddr_t) write_bufferp, write_buffer_len,
                       MB_MSYSTEM);
    if (error) {
        SMBERROR("md_get_mem for write data failed %d \n", error);
        goto bad;
    }

    /* Update original segment size */
    originalCompressedSegmentSize += write_buffer_len;
    SMB_LOG_COMPRESS("Add write len: originalCompressedSegmentSize %d \n",
                     originalCompressedSegmentSize);

    /* Update uncompressed data length */
    RemainingUncompressedDataSize = originalCompressedSegmentSize;
    SMB_LOG_COMPRESS("RemainingUncompressedDataSize %d \n",
                     RemainingUncompressedDataSize);

#if COMPRESSION_PERFORMANCE
    nanotime(&stop);
    
    SMBERROR("Setup done. RemainingUncompressedDataSize %u elapsed %ld:%ld \n",
             RemainingUncompressedDataSize,
             stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif

    if (chained_compress) {
        /*
         * Build Compression Transform Header Chained
         */
        mb_put_mem(compressed_mbp, SMB2_SIGNATURE_COMPRESSION, SMB2_SIGLEN_COMPRESSION,
                   MB_MSYSTEM);                                             /* Protocol ID */
        mb_put_uint32le(compressed_mbp, originalCompressedSegmentSize);     /* Original Compressed Segment Size*/
        
        /*
         * Add uncompressed SMB2 header and write request
         */
        mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_NONE);             /* CompressionAlgorithm */
        mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_FLAG_CHAINED);     /* Flags */
        mb_put_uint32le(compressed_mbp, sizeof(hdr_write));                 /* CompressedDataLength */
        /* Since not LZNT1, LZ77 or LZ77+Huffman, can skip OrignalPayloadSize */

        /* Put SMB2 hdr and Write request into compression payload */
        mb_put_mem(compressed_mbp, (caddr_t)&hdr_write, sizeof(hdr_write), MB_MSYSTEM);

        RemainingUncompressedDataSize -= sizeof(hdr_write); /* non compressed part */
        SMB_LOG_COMPRESS("None: hdr size %lu RemainingUncompressedDataSize %d \n",
                         sizeof(hdr_write),
                         RemainingUncompressedDataSize);

        /*
         * Process the rest of the uncompressed data in MAX_CHUNK_LEN chunks
         * checking for forward/algorithm/backward
         */
        while (RemainingUncompressedDataSize > 0) {
            SMB_LOG_COMPRESS("Processing RemainingUncompressedDataSize %d compression_chunk_len %d\n",
                             RemainingUncompressedDataSize, sessionp->compression_chunk_len);

            chunk_len = RemainingUncompressedDataSize;
            if (chunk_len > (uint32_t) sessionp->compression_chunk_len) {
                chunk_len = (uint32_t) sessionp->compression_chunk_len;
            }
            
            /* Process this write chunk */
            error = smb2_rq_compress_chunk(sessionp, algorithm,
                                           write_bufferp, chunk_len,
                                           &forward_data_repetitions, &forward_data_char,
                                           &backward_data_repetitions, &backward_data_char,
                                           compress_startp, &data_len,
                                           &actual_len);
            if (error) {
                SMBERROR("smb2_rq_compress_chunk failed %d \n", error);
                goto bad;
            }

            /* Did we save any space at all in this chunk? */
            if (one_chunk_compressed == 0) {
                if ((actual_len != 0) ||
                    (forward_data_repetitions > 1) ||
                    (backward_data_repetitions > 1)) {
                    /* Save that one chunk did save some space */
                    one_chunk_compressed = 1;
                }
            }
            
            /*
             * Check to see if forward data pattern was found
             */
            if (forward_data_repetitions > 1) {
                mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_PATTERN_V1);   /* CompressionAlgorithm */
                mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_FLAG_NONE);    /* Flags */
                mb_put_uint32le(compressed_mbp, 8);                             /* CompressedDataLength */
                /* Since not LZNT1, LZ77 or LZ77+Huffman, can skip OrignalPayloadSize */

                /* Pattern_V1 payload */
                mb_put_uint8(compressed_mbp, forward_data_char);                /* Pattern */
                mb_put_uint8(compressed_mbp, 0);                                /* Reserved1 */
                mb_put_uint16le(compressed_mbp, 0);                             /* Reserved2 */
                mb_put_uint32le(compressed_mbp, forward_data_repetitions);      /* Repetitions */
            }

            /*
             * Check for algorithmic compressed data to add
             * actual_len is the compressed data length
             */
            if (actual_len != 0) {
                /* Add compressed data */
                mb_put_uint16le(compressed_mbp, algorithm);                     /* CompressionAlgorithm */
                mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_FLAG_NONE);    /* Flags */
                /*
                 * Note that compress_len includes the OriginalPayloadSize
                 * thus need to add in it's size
                 */
                mb_put_uint32le(compressed_mbp, (uint32_t) actual_len + 4);     /* CompressedDataLength */
                mb_put_uint32le(compressed_mbp, data_len);                      /* OriginalDataLength */

                /* Put compressed data into compression payload */
                mb_put_mem(compressed_mbp, (caddr_t)compress_startp, actual_len, MB_MSYSTEM);
            }
            else {
                if (data_len != 0) {
                    /* Data failed to compress so add uncompressed data */
                    mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_NONE);             /* CompressionAlgorithm */
                    mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_FLAG_NONE);        /* Flags */
                    mb_put_uint32le(compressed_mbp, data_len);                          /* CompressedDataLength */
                    /* Since not LZNT1, LZ77 or LZ77+Huffman, can skip OrignalPayloadSize */

                    /* Put uncompressed data into compression payload */
                    mb_put_mem(compressed_mbp, (caddr_t)compress_startp, data_len, MB_MSYSTEM);
                }
            }
            
            /*
             * Check to see if backward data pattern was found
             */
            if (backward_data_repetitions > 1) {
                mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_PATTERN_V1);   /* CompressionAlgorithm */
                mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_FLAG_NONE);    /* Flags */
                mb_put_uint32le(compressed_mbp, 8);                             /* CompressedDataLength */
                /* Since not LZNT1, LZ77 or LZ77+Huffman, can skip OrignalPayloadSize */

                /* Pattern_V1 payload */
                mb_put_uint8(compressed_mbp, backward_data_char);               /* Pattern */
                mb_put_uint8(compressed_mbp, 0);                                /* Reserved1 */
                mb_put_uint16le(compressed_mbp, 0);                             /* Reserved2 */
                mb_put_uint32le(compressed_mbp, backward_data_repetitions);     /* Repetitions */
            }

            /* Update values and loop around again */
            write_bufferp += chunk_len;
            RemainingUncompressedDataSize -= chunk_len;
        }
        
        /*
         * If none of the chunks saved any space, then just send the original
         * write request instead
         */
        if (one_chunk_compressed == 0) {
            /* Mark that this write failed to compress */
            rqp->sr_extflags |= SMB2_FAILED_COMPRESS_WRITE;
            
            /* free the compressed mbp we have been building */
            mb_done(compressed_mbp);

            /*
             * Leave WITHOUT setting the SMBR_COMPRESSED flags so that
             * original write request will be sent instead
             */
            error = 0;
            goto bad;
        }

        /* Mark the request as compressed */
        rqp->sr_flags |= SMBR_COMPRESSED;
        
        error = 0;
    }
    else {
        /* For non chained compression, just compress everything */
        data_startp = write_bufferp;
        data_len = write_buffer_len;

        /*
         * Do algorithmic compression here
         * Use preallocated buffer in compress_startp that is same size of
         * original write data with assumption that compressed data will be
         * smaller.
         */
        compress_len = data_len;

        error = smb2_compress_data(data_startp, data_len,
                                   compress_startp, compress_len,
                                   algorithm, COMPRESSION_STREAM_ENCODE,
                                   &actual_len);
        if (error) {
            SMBERROR("smb2_compress_decompress_data failed %d \n", error);
            goto bad;
        }
        
#if COMPRESSION_PERFORMANCE
        nanotime(&stop);
    
        SMBERROR("Algorithm %d. compress_len %u elapsed %ld:%ld \n",
                 algorithm, compress_len,
                 stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif

        /*
         * This should be very rare since we are compressing all the write
         * data, but we will have this check just to be paranoid.
         *
         * If the compressed data would be larger than original data,
         * then (error == 0) and (actual_len == 0). In this case, just
         * send the original write request instead.
         */
        if (actual_len == 0) {
            /* Mark that this write failed to compress */
            rqp->sr_extflags |= SMB2_FAILED_COMPRESS_WRITE;
            
            /* free the compressed mbp we have been building */
            mb_done(compressed_mbp);

            /*
             * Leave WITHOUT setting the SMBR_COMPRESSED flags so that
             * original write request will be sent instead
             */
            error = 0;
            goto bad;
        }

        /* Update uncompressed data length */
        RemainingUncompressedDataSize -= data_len;

        /*
         * Build Compression Transform Header Non Chained
         */
        mb_put_mem(compressed_mbp, SMB2_SIGNATURE_COMPRESSION, SMB2_SIGLEN_COMPRESSION,
                   MB_MSYSTEM);                                             /* Protocol ID */
        mb_put_uint32le(compressed_mbp, originalCompressedSegmentSize);     /* Original Compressed Segment Size*/
        mb_put_uint16le(compressed_mbp, algorithm);                         /* CompressionAlgorithm */
        mb_put_uint16le(compressed_mbp, SMB2_COMPRESSION_FLAG_NONE);        /* Flags */
        mb_put_uint32le(compressed_mbp, sizeof(hdr_write));                 /* Offset */

        /* Put SMB2 hdr and Write request into compression payload */
        mb_put_mem(compressed_mbp, (caddr_t)&hdr_write, sizeof(hdr_write), MB_MSYSTEM);

        /* Put compressed data into compression payload */
        mb_put_mem(compressed_mbp, (caddr_t)compress_startp, actual_len, MB_MSYSTEM);
        
        /* Mark the request as compressed */
        rqp->sr_flags |= SMBR_COMPRESSED;
        
        error = 0;
    }
 
#if COMPRESSION_PERFORMANCE
    nanotime(&stop);
    
    SMBERROR("Done. elapsed %ld:%ld \n",
             stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif

    /* Paranoid check */
    if (RemainingUncompressedDataSize > 0) {
        SMBERROR("Why is there still uncompressed data %d? \n",
                 RemainingUncompressedDataSize);
    }

    sessionp->write_compress_cnt += 1;
    
bad:
    SMB_LOG_KTRACE(SMB_DBG_WRITE_COMPRESS | DBG_FUNC_END,
                   error, 0, 0, 0, 0);
    
    if (bufferp != NULL) {
        SMB_FREE_DATA(bufferp, buffer_len);
        bufferp = NULL;
    }

    return (error);
}

/*
 * SMB 3 Decompress a Read reply
 */
int
smb2_rq_decompress_read(struct smb_session *sessionp, mbuf_t *mpp)
{
    uint32_t chained_compress = 1, CurrentDecompressedDataSize = 0;
    uint32_t pattern_repetitions = 0;
    char pattern_char = 0;
    struct mdchain compressed_mdp = {0};
    struct mbchain decompressed_mbp = {0};
    int error = 0, decompress_called = 0;
    uint32_t originalCompressedSegmentSize = 0, offset = 0, read_smb_hdr = 0;
    uint8_t *bufferp = NULL, *data_startp = NULL, *compress_startp = NULL;
    uint32_t buffer_len = 0, data_len = 0, compress_len = 0;
    uint32_t protocol_id = 0, original_payload_size = 0;
    uint16_t algorithm, flags;
    size_t actual_len = 0, remaining_size = 0;
    struct smb2_header smb2_hdr = {0}, *smb2_hdrp = NULL;
    
#if COMPRESSION_PERFORMANCE
    struct timespec start, stop;
    nanotime(&start);
#endif

    if ((mpp == NULL) || (sessionp == NULL))  {
        SMBERROR("Called with NULL mpp or sessionp \n");
        return(EINVAL);
    }

    /* If not SMB 3, then just return */
    if (!SMBV_SMB3_OR_LATER(sessionp)) {
        /* Should have been checked before calling this function */
        SMB_LOG_COMPRESS("Not SMB 3 or later \n");
        return(0);
    }

    /* Is compression supported by server? */
    if (sessionp->server_compression_algorithms_map == 0) {
        /* Should have been checked before calling this function */
        SMB_LOG_COMPRESS("Server does not support compression \n");
        return(0);
    }

    /* Is chained compression supported by server? */
    if (sessionp->session_misc_flags & SMBV_COMPRESSION_CHAINING_OFF) {
        //SMB_LOG_COMPRESS("Chained compression disabled \n");
        chained_compress = 0;
    }

    SMB_LOG_KTRACE(SMB_DBG_READ_DECOMPRESS | DBG_FUNC_START,
                   sessionp->server_compression_algorithms_map,
                   chained_compress,
                   0, 0, 0);

    /* Will need a new mbp to build the decompressed reply */
    error = mb_init(&decompressed_mbp); /* DO NOT FREE decompressed_mbp! */
    if (error) {
        SMBERROR("mb_init failed %d \n", error);
        return error;
    }

    /* Get compressed mdp to parse */
    md_initm(&compressed_mdp, *mpp);    /* DO NOT FREE compressed_mdp! */
    
    /*
     * Parse Compression Transform Header Chained/Unchained common part
     */

    /* Get Protocol ID */
    error = md_get_uint32le(&compressed_mdp, &protocol_id);
    if (error) {
        goto bad;
    }

    /* Get Original */
    error = md_get_uint32le(&compressed_mdp, &originalCompressedSegmentSize);
    if (error) {
        goto bad;
    }

    /* Sanity check originalCompressedSegmentSize */
    if (originalCompressedSegmentSize > kDefaultMaxIOSize) {
        SMBERROR("originalCompressedSegmentSize too big %d \n",
                 originalCompressedSegmentSize);
        error = E2BIG;
        return error;
    }

    SMB_LOG_COMPRESS("originalCompressedSegmentSize: %d (0x%x) \n",
                     originalCompressedSegmentSize, originalCompressedSegmentSize);

    /*
     * Do just one malloc of (2 x originalCompressedSegmentSize) to reduce
     * doing a bunch of smaller mallocs. 2x because need one buffer to hold
     * compressed data and another to hold the decompressed data.
     */
    buffer_len = originalCompressedSegmentSize * 2;
    SMB_MALLOC_DATA(bufferp, buffer_len, Z_WAITOK);
    if (bufferp == NULL) {
        error = ENOMEM;
        goto bad;
    }

    compress_startp = bufferp;
    //compress_len = originalCompressedSegmentSize; /* Should get set later */
    
    data_startp = bufferp + originalCompressedSegmentSize;
    //data_len = originalCompressedSegmentSize;     /* Should get set later */

    while (CurrentDecompressedDataSize < originalCompressedSegmentSize) {
        /*
         * Get compression chained payload header
         */
        
        /* Get CompressionAlgorithm */
        error = md_get_uint16le(&compressed_mdp, &algorithm);
        if (error) {
            goto bad;
        }

        /* Get Flags */
        error = md_get_uint16le(&compressed_mdp, &flags);
        if (error) {
            goto bad;
        }
        SMB_LOG_COMPRESS("Flags: %s (0x%x) \n",
                         (flags & SMB2_COMPRESSION_FLAG_CHAINED) ? "Chained" : "Nonchained",
                         flags);

        if (chained_compress) {
            /* Chained compression - Get CompressedDataLength */
            error = md_get_uint32le(&compressed_mdp, &compress_len);
            if (error) {
                goto bad;
            }
            remaining_size = md_get_size(&compressed_mdp);
            if (compress_len > remaining_size) {
                SMBERROR("compress_len<%u> larger than remaining_size<%zu>", compress_len, remaining_size);
                error = E2BIG;
                goto bad;
            }
            SMB_LOG_COMPRESS("compress_len: %d (0x%x) \n", compress_len, compress_len);
        }
        else {
            /* Nonchained compression - Get Offset */
            error = md_get_uint32le(&compressed_mdp, &offset);
            if (error) {
                goto bad;
            }
            
            /* Sanity check the offset */
            if (offset > (originalCompressedSegmentSize - CurrentDecompressedDataSize)) {
                SMBERROR("offset %d > remaining to decompress len %d? \n",
                         offset, (originalCompressedSegmentSize - CurrentDecompressedDataSize));
                error = EINVAL;
                goto bad;
            }
            compress_len = offset;

#if COMPRESSION_PERFORMANCE
            nanotime(&stop);
    
            SMBERROR("Setup done. compress_len %u elapsed %ld:%ld \n",
                     compress_len,
                     stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif

            SMB_LOG_COMPRESS("offset: %d (0x%x) \n", offset, offset);

            /*
             * Even non chained reads can have an offset of 0 meaning the
             * entire read reply is compressed
             */
            if (compress_len > 0) {
                if ((smbfs_loglevel & SMB_COMPRESSION_LOG_LEVEL) &&
                    (compress_len > sizeof(smb2_hdr))) {
                    /* Get SMB header so we can log the MessageID for debugging */
                    error = md_get_mem(&compressed_mdp, (caddr_t) &smb2_hdr, sizeof(smb2_hdr),
                                       MB_MSYSTEM);
                    if (error) {
                        goto bad;
                    }

                    SMB_LOG_COMPRESS("Non chained MessageID %llu \n",
                                     smb2_hdr.message_id);
                    
                    /* Read the SMB header, so dont do it again */
                    read_smb_hdr = 1;

                    /* Copy SMB header to decompressed mbp */
                    mb_put_mem(&decompressed_mbp, (caddr_t) &smb2_hdr, sizeof(smb2_hdr),
                               MB_MSYSTEM);
                    
                    /* Copy rest of non compressed data directly to decompressed mdp */
                    error = md_get_mem_put_mem(&compressed_mdp, &decompressed_mbp,
                                               compress_len - sizeof(smb2_hdr),
                                               MB_MSYSTEM);
                }
                else {
                    /* Copy entire non compressed data directly to decompressed mdp */
                    error = md_get_mem_put_mem(&compressed_mdp, &decompressed_mbp,
                                               compress_len, MB_MSYSTEM);
                }

                if (error) {
                    goto bad;
                }
            }
        }
        
        switch(algorithm) {
            case SMB2_COMPRESSION_NONE:
                /* Sanity check the length */
                if (compress_len > (originalCompressedSegmentSize - CurrentDecompressedDataSize)) {
                    SMBERROR("Noncompressed compress_len %d > remaining to decompress len %d? \n",
                             compress_len, (originalCompressedSegmentSize - CurrentDecompressedDataSize));
                    error = EINVAL;
                    goto bad;
                }

                if ((smbfs_loglevel & SMB_COMPRESSION_LOG_LEVEL) &&
                    (compress_len > sizeof(smb2_hdr)) &&
                    (read_smb_hdr == 0)) {
                    /* Get SMB header so we can log the MessageID for debugging */
                    error = md_get_mem(&compressed_mdp, (caddr_t) &smb2_hdr, sizeof(smb2_hdr),
                                       MB_MSYSTEM);
                    if (error) {
                        goto bad;
                    }

                    SMB_LOG_COMPRESS("Algorithm: None, MessageID %llu \n",
                                     smb2_hdr.message_id);

                    /*
                     * For chained compression, usually the first compressed
                     * None is the smb header and read request.
                     *
                     * Read the SMB header, so dont do it again
                     */
                    read_smb_hdr = 1;

                    /* Copy SMB header to decompressed mbp */
                    mb_put_mem(&decompressed_mbp, (caddr_t) &smb2_hdr, sizeof(smb2_hdr),
                               MB_MSYSTEM);
                    
                    /* Copy rest of non compressed data directly to decompressed mdp */
                    error = md_get_mem_put_mem(&compressed_mdp, &decompressed_mbp,
                                               compress_len - sizeof(smb2_hdr),
                                               MB_MSYSTEM);
                }
                else {
                    /* Copy entire non compressed data directly to decompressed mdp */
                    error = md_get_mem_put_mem(&compressed_mdp, &decompressed_mbp,
                                               compress_len, MB_MSYSTEM);
                }
                
                if (error) {
                    goto bad;
                }

                /* Update decompressed length */
                CurrentDecompressedDataSize += compress_len;
                SMB_LOG_COMPRESS("CurrentDecompressedDataSize: %d \n", CurrentDecompressedDataSize);

#if COMPRESSION_PERFORMANCE
                nanotime(&stop);
    
                SMBERROR("None. CurrentDecompressedDataSize %u elapsed %ld:%ld \n",
                         CurrentDecompressedDataSize,
                         stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif
                break;
                
            case SMB2_COMPRESSION_PATTERN_V1:
                SMB_LOG_COMPRESS("Algorithm: PatternV1 \n");

                if (decompress_called == 0) {
                    /* Must be forward pattern */
                    sessionp->read_cnt_fwd_pattern += 1;

                    /*
                     * Set flag indicating some algorithm decompress was done
                     * to distiguish foward versus backward pattern if they occur
                     */
                    decompress_called = 1;
                }
                else {
                    /*
                     * Must be backward pattern since called after some
                     * algorithmic decompression has occured
                     */
                    sessionp->read_cnt_bwd_pattern += 1;
                }

                /* Get Pattern */
                error = md_get_uint8(&compressed_mdp, (uint8_t*) &pattern_char);
                if (error) {
                    goto bad;
                }

                /* Get Reserved1 and discard */
                error = md_get_uint8(&compressed_mdp, NULL);
                if (error) {
                    goto bad;
                }

                /* Get Reserved2 and discard */
                error = md_get_uint16le(&compressed_mdp, NULL);
                if (error) {
                    goto bad;
                }

                /* Get Repetitions */
                error = md_get_uint32le(&compressed_mdp, &pattern_repetitions);
                if (error) {
                    goto bad;
                }
                SMB_LOG_COMPRESS("pattern_repetitions: %d \n", pattern_repetitions);

                /* Sanity check the length */
                if (pattern_repetitions > (originalCompressedSegmentSize - CurrentDecompressedDataSize)) {
                    SMBERROR("PatternV1 repetitions %d > remaining to decompress len %d? \n",
                             pattern_repetitions, (originalCompressedSegmentSize - CurrentDecompressedDataSize));
                    error = EINVAL;
                    goto bad;
                }

                /* Use preallocated buffer in compress_startp to hold pattern */
                compress_len = pattern_repetitions;

                /* Create buffer of repeating char */
                memset(compress_startp, pattern_char, compress_len);
                
                /* Copy buffer to decompressed mbp */
                mb_put_mem(&decompressed_mbp, (caddr_t) compress_startp, compress_len,
                           MB_MSYSTEM);

                /* Update decompressed length */
                CurrentDecompressedDataSize += compress_len;
                SMB_LOG_COMPRESS("CurrentDecompressedDataSize: %d \n", CurrentDecompressedDataSize);

#if COMPRESSION_PERFORMANCE
                nanotime(&stop);
    
                SMBERROR("Pattern. pattern_repetitions %u elapsed %ld:%ld \n",
                         pattern_repetitions,
                         stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif
                break;

            case SMB2_COMPRESSION_LZNT1:
            case SMB2_COMPRESSION_LZ77:
            case SMB2_COMPRESSION_LZ77_HUFFMAN:
                if (algorithm == SMB2_COMPRESSION_LZ77_HUFFMAN) {
                    sessionp->read_cnt_LZ77Huff += 1;
                    SMB_LOG_COMPRESS("Algorithm: LZ77Huffman \n");
                }
                else {
                    if (algorithm == SMB2_COMPRESSION_LZ77) {
                        SMB_LOG_COMPRESS("Algorithm: LZ77 \n");
                        sessionp->read_cnt_LZ77 += 1;
                    }
                    else {
                        SMB_LOG_COMPRESS("Algorithm: LZNT1 \n");
                        sessionp->read_cnt_LZNT1 += 1;
                    }
                }

                /*
                 * Set flag indicating some algorithm decompress was done
                 * to distiguish foward versus backward pattern if they occur
                 */
                decompress_called = 1;
                
                if (chained_compress) {
                    /*
                     * Handle Chained Compression
                     * Get OriginalPayloadSize
                     */
                    error = md_get_uint32le(&compressed_mdp, &original_payload_size);
                    if (error) {
                        goto bad;
                    }
                    SMB_LOG_COMPRESS("original_payload_size: %d \n", original_payload_size);

#if 0
                    /*
                     * Oddly, Windows server will send a compress length that
                     * is bigger than the decompressed length which will cause
                     * this check to fail. Why they dont just send the non
                     * compressed data?
                     *
                     * Sanity check the compress length
                     */
                    if (compress_len > (originalCompressedSegmentSize - CurrentDecompressedDataSize)) {
                        SMBERROR("Algorithm %d compress_len %d > remaining to decompress len %d? \n",
                                 algorithm, compress_len,
                                 (originalCompressedSegmentSize - CurrentDecompressedDataSize));
                        error = EINVAL;
                        goto bad;
                    }
#endif

                    /* Sanity check the original payload size */
                    if (original_payload_size > (originalCompressedSegmentSize - CurrentDecompressedDataSize)) {
                        SMBERROR("Algorithm %d original payload size %d > remaining to decompress len %d? \n",
                                 algorithm, original_payload_size,
                                 (originalCompressedSegmentSize - CurrentDecompressedDataSize));
                        error = EINVAL;
                        goto bad;
                    }

                    /*
                     * Note that compress_len includes the OriginalPayloadSize
                     * thus need to subtract it's size
                     */
                    if (compress_len <= 4) {
                        SMBERROR("compress_len too small %d? \n", compress_len);
                        error = EINVAL;
                        goto bad;
                    }
                    compress_len -= 4;
                }
                else {
                    /*
                     * Handle Non Chained Compression
                     * compressed data length = (replyLen - offset)
                     */
                    compress_len = (uint32_t) mbuf_get_chain_len(*mpp);
                    
                    /* Sanity check the length */
                    if (offset > compress_len) {
                        SMBERROR("Algorithm %d offset %d > compress_len %d? \n",
                                 algorithm, offset, compress_len);
                        error = EINVAL;
                        goto bad;
                    }

                    /* Subtract out the uncompressed part */
                    compress_len -= offset;
                    
                    /* Sanity check the length */
                    if (16 > compress_len) {
                        SMBERROR("Algorithm %d 16 > compress_len %d? \n",
                                 algorithm, compress_len);
                        error = EINVAL;
                        goto bad;
                    }

                    /* Subtract already parsed transform hdr unchained bytes */
                    compress_len -= 16;
                    
                    original_payload_size = originalCompressedSegmentSize;
                }

                /* Copy compressed data to preallocated buffer in compress_startp */
                error = md_get_mem(&compressed_mdp, (caddr_t) compress_startp, compress_len,
                                   MB_MSYSTEM);
                if (error) {
                    goto bad;
                }
                
                /* Use preallocated buffer in data_startp to hold decompressed data */
                data_len = original_payload_size;

                /*
                 * Do algorithmic decompression here
                 */
                error = smb2_compress_data(data_startp, data_len,
                                           compress_startp, compress_len,
                                           algorithm, COMPRESSION_STREAM_DECODE,
                                           &actual_len);
                if (error) {
                    SMBERROR("smb2_compress_decompress_data failed %d \n", error);
                    goto bad;
                }
                
                if (actual_len != original_payload_size) {
                    SMBERROR("actual_len <%zu> != original_payload_size <%d> \n",
                             actual_len, original_payload_size);
                    error = EINVAL;
                    goto bad;
                }
                
                if ((smbfs_loglevel & SMB_COMPRESSION_LOG_LEVEL) &&
                    (actual_len > sizeof(smb2_hdr)) &&
                    (read_smb_hdr == 0)) {
                    /* Get SMB header so we can log the MessageID for debugging */
                    smb2_hdrp = (struct smb2_header*) data_startp;
                    
                    SMB_LOG_COMPRESS("Algorithm: %d, MessageID %llu \n",
                                     algorithm, smb2_hdrp->message_id);

                    /* Read the SMB header, so dont do it again */
                    read_smb_hdr = 1;
                }

                /*
                 * At this point,
                 * data_len = original_payload_size = actual_len
                 */
                                    
                /* Copy decompressed data to decompressed mbp */
                mb_put_mem(&decompressed_mbp, (caddr_t) data_startp, data_len,
                           MB_MSYSTEM);

                /* Update decompressed length */
                CurrentDecompressedDataSize += data_len;
                SMB_LOG_COMPRESS("CurrentDecompressedDataSize: %d \n", CurrentDecompressedDataSize);

#if COMPRESSION_PERFORMANCE
                nanotime(&stop);
    
                SMBERROR("Algorithm %d. CurrentDecompressedDataSize %u elapsed %ld:%ld \n",
                         algorithm, CurrentDecompressedDataSize,
                         stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif
                break;

            default:
                SMBERROR("Unknown algorithm %d \n", algorithm);
                error = EINVAL;
                goto bad;
        }
    }
    
    if (CurrentDecompressedDataSize != originalCompressedSegmentSize) {
        SMBERROR("Decompressed size %d != Original segment size %d \n",
                 CurrentDecompressedDataSize, originalCompressedSegmentSize);
        error = EINVAL;
        goto bad;
    }
        
    /* Remove the current reply mbufs and replace with decompressed mbufs */
    mbuf_freem(*mpp);
    *mpp = decompressed_mbp.mb_top;

    sessionp->read_compress_cnt += 1;
    
    error = 0;
        
#if COMPRESSION_PERFORMANCE
    nanotime(&stop);
    
    SMBERROR("Done. elapsed %ld:%ld \n",
             stop.tv_sec - start.tv_sec, (stop.tv_nsec - start.tv_nsec) / 1000);
#endif

bad:
    if (bufferp != NULL) {
        SMB_FREE_DATA(bufferp, buffer_len);
        bufferp = NULL;
    }

    SMB_LOG_KTRACE(SMB_DBG_READ_DECOMPRESS | DBG_FUNC_END,
                   error, 0, 0, 0, 0);
    
    return (error);
}
