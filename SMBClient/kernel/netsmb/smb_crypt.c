/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2014 Apple Inc. All rights reserved.
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
#include <sys/kauth.h>

#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <smbfs/smbfs.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_dev.h>
#include <netsmb/md4.h>
#include <netsmb/smb_packets_2.h>

#include <smbfs/smbfs_subr.h>
#include <netsmb/smb_converter.h>

#include <crypto/des.h>
#include <corecrypto/cchmac.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/cccmac.h>
#include <corecrypto/ccnistkdf.h>


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
    SMB_MALLOC(ksp, des_key_schedule *, sizeof(des_key_schedule), M_SMBTEMP, M_WAITOK);
	des_set_key((des_cblock*)kk, *ksp);
	des_ecb_encrypt((des_cblock*)data, (des_cblock*)dest, *ksp, 1);
    
    if (ksp) {
        SMB_FREE(ksp, M_SMBTEMP);
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

    SMB_MALLOC(p, u_char *, 14+21, M_SMBTEMP, M_WAITOK);
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
        SMB_FREE(p, M_SMBTEMP);
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
	size_t len;
	
	bzero(ntlmHash, ntlmHash_len);
	len = strnlen((char *)passwd, SMB_MAXPASSWORDLEN + 1);
    
    if (len == 0) {
        /* Empty password, but we still need to allocate a buffer to */
        /* encrypt two NULL bytes so we can compute the NTLM hash correctly. */
        len = 1;
    }
    
    SMB_MALLOC(unicode_passwd, uint16_t *, len * sizeof(uint16_t), M_SMBTEMP, M_WAITOK);
	if (unicode_passwd == NULL)	/* Should never happen, but better safe than sorry */
		return;
    
	len = smb_strtouni(unicode_passwd, (char *)passwd, len, UTF_PRECOMPOSED);
	bzero(&md4, sizeof(md4)); 
	MD4Init(&md4);
	MD4Update(&md4, (uint8_t *)unicode_passwd, (unsigned int)len);
	MD4Final(ntlmHash, &md4);
    
    if (unicode_passwd) {
        SMB_FREE(unicode_passwd, M_SMBTEMP);
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
    size_t uniuserlen, unidestlen;
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
    SMB_MALLOC(uniuser, u_int16_t *, len * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
    uniuserlen = smb_strtouni(uniuser, (char *)user, len,
                              UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
    
    len = strlen((char *)destination);
    SMB_MALLOC(unidest, u_int16_t *, len * sizeof(u_int16_t), M_SMBTEMP, M_WAITOK);
    unidestlen = smb_strtouni(unidest, (char *)destination, len,
                              UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
    
    datalen = uniuserlen + unidestlen;
    SMB_MALLOC(data, u_char *, datalen, M_SMBTEMP, M_WAITOK);
    bcopy(uniuser, data, uniuserlen);
    bcopy(unidest, data + uniuserlen, unidestlen);
    
    if (uniuser) {
        SMB_FREE(uniuser, M_SMBTEMP);
    }
    
    if (unidest) {
        SMB_FREE(unidest, M_SMBTEMP);
    }
    
    HMACT64(v1hash, 16, data, datalen, v2hash);
    
    if (data) {
        SMB_FREE(data, M_SMBTEMP);
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
    SMB_MALLOC(data, u_char *, datalen, M_SMBTEMP, M_WAITOK);
    bcopy(C8, data, 8);
    bcopy(blob, data + 8, bloblen);

    v2resplen = 16 + bloblen;
    SMB_MALLOC(v2resp, u_char *, v2resplen, M_SMBTEMP, M_WAITOK);
    HMACT64(v2hash, 16, data, datalen, v2resp);

    if (data) {
        SMB_FREE(data, M_SMBTEMP);
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
        SMB_FREE(sessionp->session_mackey, M_SMBTEMP);
    }
    
    sessionp->session_mackey = NULL;
    sessionp->session_mackeylen = 0;
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
	
	KASSERT(sessionp->session_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE,
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
		KASSERT(rqp->sr_t2 == NULL ||
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
	KASSERT(mbuf_len(mb) >= SMB_HDRLEN, ("forgot to mbuf_pullup"));
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
    
    SMB_MALLOC(mac, u_char *, di->output_size, M_SMBTEMP, M_WAITOK);
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
        SMB_FREE(mac, M_SMBTEMP);
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
    uint32_t      do_smb3_sign = 0;
    
    if (rqp == NULL) {
        SMBDEBUG("Called with NULL rqp\n");
        return (EINVAL);
    }

    sessionp = rqp->sr_session;
    
    if (sessionp == NULL) {
        SMBERROR("sessionp is NULL\n");
        return  (EINVAL);
    }
    
    /* Is signing required for the command? */
    if ((rqp->sr_command == SMB2_SESSION_SETUP) ||
         (rqp->sr_command == SMB2_NEGOTIATE)) {
        return (0);
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

    SMB_LOG_KTRACE(SMB_DBG_SMB_RQ_SIGN | DBG_FUNC_START,
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
     
    SMB_LOG_KTRACE(SMB_DBG_SMB_RQ_SIGN | DBG_FUNC_END,
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
    size_t length = 0;
    
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
    
    SMB_MALLOC(mac, u_char *, di->output_size, M_SMBTEMP, M_WAITOK);
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
            SMB_FREE(mac, M_SMBTEMP);
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
            SMB_FREE(mac, M_SMBTEMP);
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
                    SMB_FREE(mac, M_SMBTEMP);
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
                SMB_FREE(mac, M_SMBTEMP);
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
                    SMB_FREE(mac, M_SMBTEMP);
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
                SMB_FREE(mac, M_SMBTEMP);
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
                    SMB_FREE(mac, M_SMBTEMP);
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
                SMB_FREE(mac, M_SMBTEMP);
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
        SMB_FREE(mac, M_SMBTEMP);
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
        ((rqp->sr_command == SMB2_SESSION_SETUP) && !(rqp->sr_rspflags & SMB2_FLAGS_SIGNED))) {
        /*
         * Don't verify signature if we don't have a session key from gssd yet.
         * Don't verify signature if a SessionSetup reply that hasn't
         * been signed yet (server only signs the final SessionSetup reply).
         */
		return (0);
    }

    /* Its an anonymous login, signing is not supported */
	if ((sessionp->session_flags & SMBV_ANONYMOUS_ACCESS) == SMBV_ANONYMOUS_ACCESS) {
		return (0);
    }
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_RQ_VERIFY | DBG_FUNC_START,
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
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_RQ_VERIFY | DBG_FUNC_END,
                   err, 0, 0, 0, 0);

    return (err);
}

/*
 * SMB 3 Routines
 */

static void
smb3_get_signature(struct smb_session *sessionp,
				   struct mbchain *mbp,
				   struct mdchain *mdp,
				   size_t packet_len,
				   uint8_t *out_signature,
				   size_t out_signature_len)
{
	mbuf_t mb, starting_mp;
	size_t mb_off, mb_len, n;
	u_char mac[CMAC_BLOCKSIZE], *mb_data;
	const struct ccmode_cbc *ccmode = ccaes_cbc_encrypt_mode();
	int mb_count = 0;
	int error;
	u_char *block = NULL;
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
	
	if (out_signature == NULL) {
		SMBERROR("mbp or out_signature is null\n");
		return;
	}
	
	if ((mbp == NULL) && (mdp == NULL)) {
		SMBERROR("mbp and mdp are null\n");
		return;
	}
	
	if (sessionp->session_smb3_signing_key_len < SMB3_KEY_LEN) {
		SMBERROR("smb3 keylen %u\n", sessionp->session_smb3_signing_key_len);
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
		SMB_MALLOC(block, u_char *, malloc_size, M_SMBTEMP, M_WAITOK);
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
		}
		else {
			/* Its a reply to process */
			mb = mdp->md_cur;
			mb_len = (size_t) mbuf_data(mb) + mbuf_len(mb) - (size_t) mdp->md_pos;
			mb_off = mbuf_len(mb) - mb_len;
		}
		
		//SMBERROR("One chunk %zu \n", malloc_size);
		
		n = mbuf_get_nbytes(malloc_size, block, 0, &mb, &mb_len, &mb_off);
		if (n != malloc_size) {
			SMBERROR("mbuf chain exhausted on first block exp: %zu, got: %lu\n",
					 malloc_size, n);
			goto out;
		}
		
		/* Sign entire packet in one call */
		error = cccmac_one_shot_generate(ccmode,
										 SMB3_KEY_LEN, sessionp->session_smb3_signing_key,
										 malloc_size, block,
										 CMAC_BLOCKSIZE, mac);
		if (error) {
			SMBERROR("cccmac_one_shot_generate failed %d \n", error);
			goto out;
		}
	}
	else {
		/*
		 * Probably a read or write. Have to do multiple calls to the crypto
		 * code to process this packet
		 */
		
		/* Init the cipher */
		cccmac_mode_decl(ccmode, cmac);
		error = cccmac_init(ccmode, cmac, SMB3_KEY_LEN, sessionp->session_smb3_signing_key);
		if (error) {
			SMBERROR("cccmac_init failed %d \n", error);
			goto out;
		}
		
		/* Initial mb setup */
		mb_count = 0;
		
		if (mdp == NULL) {
			/* Signing a request - easy case */
			starting_mp = mbp->mb_top;
		}
		else {
			/* Signing a reply - complicated case */
			starting_mp = mdp->md_cur;
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
			
			/*SMBERROR("mb number <%d> mb_len() <%ld> mb_len <%ld>, packet_len %ld\n",
					 mb_count, mbuf_len(mb), mb_len, packet_len);*/
			
			/* Process this m_buf's data */
			error = cccmac_update(cmac, mb_len, mb_data);
			if (error) {
				SMBERROR("cccmac_update failed %d \n", error);
				break;
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
		//SMBERROR("Calling cccmac_final_generate \n");
		error = cccmac_final_generate(cmac, CMAC_BLOCKSIZE, mac);
		if (error) {
			SMBERROR("cccmac_final_generate failed %d \n", error);
		}
		
		/* Replies only check */
		if ((mdp) && (packet_len > 0)) {
			SMBERROR("Not enough bytes in packet? <%zu>\n", packet_len);
		}
	}
	
	/* Copy first 16 bytes of the HMAC hash into the signature field */
	bcopy(mac, out_signature, SMB2SIGLEN);

out:
	if (block) {
		SMB_FREE(block, M_SMBTEMP);
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
    uint8_t reply_signature[SMB2SIGLEN];
	size_t reply_len;
	mbuf_t mb_temp;
	
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

	/* Calculate reply's signature */
	/*SMBERROR("SMB3 verify signature: mid %llu NextCmd %d\n",
			 rqp->sr_messageid, nextCmdOffset);*/
	smb3_get_signature(sessionp, NULL, mdp, reply_len,
					   reply_signature, SMB2SIGLEN);
	
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
	smb3_get_signature(sessionp, mbp, NULL, request_len,
					   rqp->sr_rqsig, SMB2SIGLEN);
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
    unsigned char           dig[CCAES_BLOCK_SIZE];
    uint64_t                i64;
    uint32_t                msglen, i32;
    uint16_t                i16;
    int                     error;
    unsigned char           *msgp;
    const struct ccmode_ccm *ccmode = ccaes_ccm_encrypt_mode();
    size_t                  nbytes;
    mbuf_t                  mb, m2;
    struct mbchain          *mbp, *tmp_mbp;
    struct smb_rq           *tmp_rqp;

    SMB_LOG_KTRACE(SMB_DBG_SMB_RQ_ENCRYPT | DBG_FUNC_START,
                   0, 0, 0, 0, 0);

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
         * Create the first chain
         * Save current sr_rq.mp_top into "m", set sr_rq.mp_top to NULL,
         * then send "m"
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
         * Save current sr_rq.mp_top into "m", set sr_rq.mp_top to NULL,
         * then send "m"
         */
        mb = mb_detach(mbp);
    }
    
    mb_hdr = NULL;
    
    /* Declare/Init cypher context */
    ccccm_ctx_decl(ccmode->size, ctx);
    ccccm_nonce_decl(ccmode->nonce_size, nonce_ctx);

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
    
    // Zero last 5 bytes per spec
    memset(&nonce[11], 0, 5);
    
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
    i16 = htoles(SMB2_ENCRYPTION_AES128_CCM);
    memcpy(msgp + SMB3_AES_TF_ENCR_ALG_OFF, &i16,
           SMB3_AES_TF_ENCR_ALG_LEN);
    
    /* Session ID */
    i64 = htoleq(rqp->sr_rqsessionid);
    memcpy(msgp + SMB3_AES_TF_SESSID_OFF, &i64,
           SMB3_AES_TF_SESSID_LEN);
    
    // Set data length of mb_hdr
    mbuf_setlen(mb_hdr, SMB3_AES_TF_HDR_LEN);
    
    /* Init the cipher */
    ccccm_init(ccmode, ctx, sessionp->session_smb3_encrypt_key_len, sessionp->session_smb3_encrypt_key);
    
    ccccm_set_iv(ccmode, ctx, nonce_ctx, SMB3_CCM_NONCE_LEN, msgp + SMB3_AES_TF_NONCE_OFF,
                           SMB3_AES_TF_SIG_LEN, SMB3_AES_AUTHDATA_LEN, msglen);

    // Sign authenticated data
    ccccm_cbcmac(ccmode, ctx, nonce_ctx, SMB3_AES_AUTHDATA_LEN, msgp + SMB3_AES_AUTHDATA_OFF);
    
    // Encrypt msg data in place
    for (mb_tmp = mb; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
        nbytes = mbuf_len(mb_tmp);
        
        if (nbytes) {
            ccccm_update(ccmode, ctx, nonce_ctx, nbytes,
                         mbuf_data(mb_tmp), mbuf_data(mb_tmp));
        }
    }
    
    // Set transform header signature
    ccccm_finalize(ccmode, ctx, nonce_ctx, dig);
    memcpy(msgp + SMB3_AES_TF_SIG_OFF, dig, CCAES_BLOCK_SIZE);
    
    // Ideally, should turn off these flags from original mb:
    // (*mb)->m_flags &= ~(M_PKTHDR | M_EOR);
    
    // Return the new mbuf chain
    mb_hdr = mbuf_concatenate(mb_hdr, mb);
    m_fixhdr(mb_hdr);
    mb = mb_hdr;
    
    /* Store it back into the rqp */
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
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_RQ_ENCRYPT | DBG_FUNC_END,
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
    unsigned char           sig[SMB3_AES_TF_SIG_LEN];
    const struct ccmode_ccm *ccmode = ccaes_ccm_decrypt_mode();
    size_t                  nbytes;
    uint32_t                mbuf_cnt = 0;
    char                    *cptr = NULL;
#if TIMING_ON
    struct timespec         start, stop;

    nanotime(&start);
#endif

    SMB_LOG_KTRACE(SMB_DBG_SMB_RQ_DECRYPT | DBG_FUNC_START,
                   0, 0, 0, 0, 0);

    mbuf_payload = NULL;
    mb_hdr = NULL;
    error = 0;
    
    /* Declare/Init cypher context */
    ccccm_ctx_decl(ccmode->size, ctx);
    ccccm_nonce_decl(ccmode->nonce_size, nonce_ctx);
    
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
    
    // Verify the encryption algorithm
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
    
    // Init the cipher
    ccccm_init(ccmode, ctx, sessionp->session_smb3_decrypt_key_len, sessionp->session_smb3_decrypt_key);
    
    ccccm_set_iv(ccmode, ctx, nonce_ctx, SMB3_CCM_NONCE_LEN, msgp + SMB3_AES_TF_NONCE_OFF,
                 SMB3_AES_TF_SIG_LEN, SMB3_AES_AUTHDATA_LEN, msglen);

    /* Calculate Signature of Authenticated Data + Payload */
    ccccm_cbcmac(ccmode, ctx, nonce_ctx, SMB3_AES_AUTHDATA_LEN, msgp + SMB3_AES_AUTHDATA_OFF);

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
            SMB_FREE(sessionp->decrypt_bufferp, M_TEMP);
            sessionp->decrypt_bufferp = NULL;
            sessionp->decrypt_buf_len = 0;
        }

        if (sessionp->decrypt_bufferp == NULL) {
            SMB_MALLOC(sessionp->decrypt_bufferp, char *, msglen,
                       M_TEMP, M_WAITOK);
            if (sessionp->decrypt_bufferp == NULL) {
                SMBERROR("malloc for buffer failed \n");
                error = EAUTH;
                goto out;
            }
           sessionp->decrypt_buf_len = msglen;
        }

        /* copy the mbuf data into local buffer */
        cptr = sessionp->decrypt_bufferp;
        for (mb_tmp = mbuf_payload; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
            nbytes = mbuf_len(mb_tmp);

            if (nbytes) {
                bcopy(mbuf_data(mb_tmp), cptr, nbytes);
                cptr += nbytes;
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
        ccccm_update(ccmode, ctx, nonce_ctx, msglen,
                     sessionp->decrypt_bufferp, sessionp->decrypt_bufferp);

#if TIMING_ON
        nanotime(&stop);
        SMBERROR("After ccccm_update - Reply len %u. elapsed time %ld secs, %ld micro_secs\n",
                 msglen,
                 stop.tv_sec - start.tv_sec,
                 (stop.tv_nsec - start.tv_nsec) / 1000
                 );
#endif

        /* copy the local buffer back into mbuf chain */
        cptr = sessionp->decrypt_bufferp;
        for (mb_tmp = mbuf_payload; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
            nbytes = mbuf_len(mb_tmp);

            if (nbytes) {
                bcopy(cptr, mbuf_data(mb_tmp), nbytes);
                cptr += nbytes;
            }
        }

#if TIMING_ON
        nanotime(&stop);
        SMBERROR("After bcopy back to mbufs - Reply len %u. elapsed time %ld secs, %ld micro_secs\n",
                 msglen,
                 stop.tv_sec - start.tv_sec,
                 (stop.tv_nsec - start.tv_nsec) / 1000
                 );
#endif
    }
    else {
        /* Decrypt msg data in place */
        for (mb_tmp = mbuf_payload; mb_tmp != NULL; mb_tmp = mbuf_next(mb_tmp)) {
            mbuf_cnt += 1;
            nbytes = mbuf_len(mb_tmp);

            if (nbytes) {
                ccccm_update(ccmode, ctx, nonce_ctx, nbytes,
                             mbuf_data(mb_tmp), mbuf_data(mb_tmp));
            }
        }

#if TIMING_ON
        nanotime(&stop);
        SMBERROR("After mbuf ccccm_update - nbytes %zu, Reply len %u. elapsed time %ld secs, %ld micro_secs, mbuf_cnt %d\n",
                 nbytes,
                 msglen,
                 stop.tv_sec - start.tv_sec,
                 (stop.tv_nsec - start.tv_nsec) / 1000,
                 mbuf_cnt);
#endif
    }

    /* Final signature -> sig */
    ccccm_finalize(ccmode, ctx, nonce_ctx, sig);
    
#if TIMING_ON
    nanotime(&stop);
    SMBERROR("After ccccm_finalize - Reply len %u. elapsed time %ld secs, %ld micro_secs\n",
             msglen,
             stop.tv_sec - start.tv_sec,
             (stop.tv_nsec - start.tv_nsec) / 1000
             );
#endif

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
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_RQ_DECRYPT | DBG_FUNC_END,
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
 * int smb3_derive_keys(struct smb_session *sessionp)
 *
 * Derives all necessary keys for SMB 3
 * signing and encryption, which are all
 * stored in the smb_session struct.
 *
 * Keys are derived using KDF in Counter Mode
 * from as specified by sp800-108.
 *
 * Keys generated:
 *
 * session_smb3_signing_key
 * session_smb3_signing_key_len
 *
 */
int smb3_derive_keys(struct smb_session *sessionp)
{
    uint8_t label[16];
    uint8_t context[16];
    int     err;
    
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
    memset(label, 0, 16);
    memset(context, 0, 16);
    
    strncpy((char *)label, "SMB2AESCMAC", 11);
    strncpy((char *)context, "SmbSign", 7);
    
    err = smb_kdf_hmac_sha256(sessionp->session_mackey, sessionp->session_mackeylen,
                              label, 12,  // includes NULL Terminator
                              context, 8, // includes NULL Terminator
                              sessionp->session_smb3_signing_key,
                              SMB3_KEY_LEN);
    if (!err) {
        sessionp->session_smb3_signing_key_len = SMB3_KEY_LEN;
    } else {
        SMBDEBUG("Could not generate smb3 signing key, error: %d\n", err);
    }
    
    // Derive Session.EncryptionKey (session_smb3_encrypt_key)
    memset(label, 0, 16);
    memset(context, 0, 16);
    
    memcpy(label, "SMB2AESCCM", 10);
    memcpy(context, "ServerIn ", 9);
    
    err = smb_kdf_hmac_sha256(sessionp->session_mackey, sessionp->session_mackeylen,
                              label, 11,  // includes NULL Terminator
                              context, 10, // includes NULL Terminator
                              sessionp->session_smb3_encrypt_key,
                              SMB3_KEY_LEN);
    if (!err) {
        sessionp->session_smb3_encrypt_key_len = SMB3_KEY_LEN;
    } else {
        SMBDEBUG("Could not generate smb3 encrypt key, error: %d\n", err);
    }
    
    // Derive Session.DecryptionKey (session_smb3_decrypt_key)
    memset(label, 0, 16);
    memset(context, 0, 16);
    
    memcpy(label, "SMB2AESCCM", 10);
    memcpy(context, "ServerOut", 9);
    
    err = smb_kdf_hmac_sha256(sessionp->session_mackey, sessionp->session_mackeylen,
                              label, 11,  // includes NULL Terminator
                              context, 10, // includes NULL Terminator
                              sessionp->session_smb3_decrypt_key,
                              SMB3_KEY_LEN);
    if (!err) {
        sessionp->session_smb3_decrypt_key_len = SMB3_KEY_LEN;
    } else {
        SMBDEBUG("Could not generate smb3 decrypt key, error: %d\n", err);
    }

out:
    return (err);
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
	SMB_MALLOC(cptr, char *, packet_len, M_SMBTEMP, M_WAITOK);
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
	
	SMB_MALLOC(mac, u_char *, CMAC_BLOCKSIZE, M_SMBTEMP, M_WAITOK);
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
		SMB_FREE(mac, M_SMBTEMP);
	}
	
	if (cptr) {
		SMB_FREE(cptr, M_SMBTEMP);
	}
}
#endif
