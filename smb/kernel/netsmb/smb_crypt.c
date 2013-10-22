/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2012 Apple Inc. All rights reserved.
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
#include <netsmb/smb_2.h>
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


#define SMBSIGLEN (8)
#define SMBSIGOFF (14)
#define SMBPASTSIG (SMBSIGOFF + SMBSIGLEN)
#define SMBFUDGESIGN 4

/* SMB2 Signing defines */
#define SMB2SIGLEN (16)
#define SMB2SIGOFF (48)

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
    SMB_MALLOC(ksp, des_key_schedule *, sizeof(des_key_schedule), M_SMBTEMP, M_WAITOK);
	des_set_key((des_cblock*)kk, *ksp);
	des_ecb_encrypt((des_cblock*)data, (des_cblock*)dest, *ksp, 1);
	SMB_FREE(ksp, M_SMBTEMP);
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
	SMB_FREE(p, M_SMBTEMP);
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
	SMB_FREE(unicode_passwd, M_SMBTEMP);
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

/*
 * Initialize the signing data, free the key and
 * set everything else to zero.
 */
void smb_reset_sig(struct smb_vc *vcp)
{
	if (vcp->vc_mackey != NULL)
		SMB_FREE(vcp->vc_mackey, M_SMBTEMP);
	vcp->vc_mackey = NULL;
	vcp->vc_mackeylen = 0;
	vcp->vc_seqno = 0;
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
		*(uint32_t *)rqp->sr_rqsig = htolel(rqp->sr_seqno);
		*(uint32_t *)(rqp->sr_rqsig + 4) = 0;		
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
smb_verify(struct smb_rq *rqp, uint32_t seqno)
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
	return (bcmp((uint8_t *)mbuf_data(mdp->md_top) + SMBSIGOFF, digest, SMBSIGLEN));
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

/*
 * SMB2 Sign a single request with HMCA-SHA256
 */
static void smb2_sign(struct smb_rq *rqp)
{
    struct smb_vc *vcp = rqp->sr_vc;
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
    cchmac_init(di, hc, vcp->vc_mackeylen, vcp->vc_mackey);
    
    for (mb = mbp->mb_top; mb != NULL; mb = mbuf_next(mb))
        cchmac_update(di, hc, mbuf_len(mb), mbuf_data(mb));
    cchmac_final(di, hc, mac);
    
    // Copy first 16 bytes of the HMAC hash into the signature field
    bcopy(mac, rqp->sr_rqsig, SMB2SIGLEN);
    
    SMB_FREE(mac, M_SMBTEMP);
}

/*
 * SMB2 Sign request or compound chain with HMAC-SHA256
 */
int
smb2_rq_sign(struct smb_rq *rqp)
{
	struct smb_vc *vcp;
    struct smb_rq *this_rqp;
    
    if (rqp == NULL) {
        SMBDEBUG("Called with NULL rqp\n");
        return (EINVAL);
    }

    vcp = rqp->sr_vc;
    
    if (vcp == NULL) {
        SMBERROR("vcp is NULL\n");
        return  (EINVAL);
    }
    
    /* Is signing required for the command? */
    if ((rqp->sr_command == SMB2_SESSION_SETUP) || (rqp->sr_command == SMB2_OPLOCK_BREAK)) {
        return (0);
    }

    /* Do we have a session key? */
    if (vcp->vc_mackey == NULL) {
        SMBDEBUG("No session key for signing.\n");
        return (0);
    }

    this_rqp = rqp;
    while (this_rqp != NULL) {
        smb2_sign(this_rqp);
        this_rqp = this_rqp->sr_next_rqp;
    }
     
    return (0);
}

/*
 * SMB2 Verify a reply with HMCA-SHA256
 */
static int smb2_verify(struct smb_rq *rqp, struct mdchain *mdp, uint32_t nextCmdOffset, uint8_t *signature)
{
    struct smb_vc *vcp = rqp->sr_vc;
    mbuf_t mb, mb_temp;
    size_t mb_off, remaining, mb_len, sign_len, mb_total_len;
    u_char zero_buf[SMB2SIGLEN];
    int result;
    const struct ccdigest_info *di = ccsha256_di();
    u_char *mac;
    
    if (vcp == NULL) {
        SMBERROR("vcp is NULL\n");
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
    
    /* sanity checks */
    if (mb_len < SMB2_HDRLEN) {
        SMBDEBUG("mbuf not pulled up for SMB2 header, mbuf_len: %lu\n", mbuf_len(mb));
        SMB_FREE(mac, M_SMBTEMP);
        return (EBADRPC);
    }
    if (mb_off > mb_total_len) {
        SMBDEBUG("mb_off: %lu past end of mbuf, mbuf_len: %lu\n", mb_off, mb_total_len);
        SMB_FREE(mac, M_SMBTEMP);
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
        SMB_FREE(mac, M_SMBTEMP);
        return (EBADRPC);
    }
    
    bzero(zero_buf, SMB2SIGLEN);
    bzero(mac, di->output_size);
    cchmac_di_decl(di, hc);
    cchmac_init(di, hc, vcp->vc_mackeylen, vcp->vc_mackey);
    
    /* sanity check */
    if (mb_len < SMB2SIGOFF) {
        /* mb_len would go negative when decremented below */
        SMBDEBUG("mb_len exhausted: mb_len: %lu SMB2SIGOFF: %u\n", mb_len, (uint32_t)SMB2SIGOFF);
        SMB_FREE(mac, M_SMBTEMP);
        return (EBADRPC);
    }
    
    /* Sign the first 48 bytes of the reply (up to the signature field) */
    cchmac_update(di, hc, SMB2SIGOFF, (uint8_t *)mbuf_data(mb) + mb_off);
    mb_off += SMB2SIGOFF;
    mb_len -= SMB2SIGOFF;
    remaining -= SMB2SIGOFF;
    
    /* sanity check */
    if (mb_off > mb_total_len) {
        // mb_offset would go past the end of current mbuf, when incremented below */
        SMBDEBUG("mb_off past end, mb_off: %lu mbub_len: %lu\n", mb_off, mb_total_len);
        SMB_FREE(mac, M_SMBTEMP);
        return (EBADRPC);
    }
    /* sanity check */
    if (mb_len < SMB2SIGLEN) {
        /* mb_len would go negative when decremented below */
        SMBDEBUG("mb_len exhausted: mb_len: %lu SMB2SIGLEN: %u\n", mb_len, (uint32_t)SMB2SIGLEN);
        SMB_FREE(mac, M_SMBTEMP);
        return (EBADRPC);
    }
    
    // Sign 16 zeros
    cchmac_update(di, hc, SMB2SIGLEN, zero_buf);
    mb_off += SMB2SIGLEN;
    mb_len -= SMB2SIGLEN;
    remaining -= SMB2SIGLEN;
    
    /* Sign remainder of this reply */
    while (remaining) {
        if (!mb_len) {
            mb = mbuf_next(mb);
            if (!mb) {
                SMBDEBUG("mbuf_next didn't return an mbuf\n");
                SMB_FREE(mac, M_SMBTEMP);
                return EBADRPC;
            }
            mb_len = mbuf_len(mb);
            mb_off = 0;
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
    result = bcmp(signature, mac, SMB2SIGLEN);
    SMB_FREE(mac, M_SMBTEMP);
	return (result);
}

/*
 * SMB2 Verify reply signature with HMAC-SHA256
 */
int
smb2_rq_verify(struct smb_rq *rqp, struct mdchain *mdp, uint8_t *signature)
{
    struct smb_vc *vcp;
    uint32_t nextCmdOffset;
    int err;
    
    if (rqp == NULL) {
        SMBDEBUG("Called with NULL rqp\n");
        return (EINVAL);
    }
    
    vcp = rqp->sr_vc;
    
    if (vcp == NULL) {
        SMBERROR("NULL vcp\n");
        return  (0);
    }
    nextCmdOffset = rqp->sr_rspnextcmd;
    
    if (!(vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE)) {
		SMBWARNING("signatures not enabled!\n");
		return (0);
	}
    
    if ((vcp->vc_mackey == NULL) ||
        ( (rqp->sr_command == SMB2_SESSION_SETUP) && !(rqp->sr_rspflags & SMB2_FLAGS_SIGNED))) {
        /*
         * Don't verify signature if we don't have a session key from gssd yet.
         * Don't verify signature if a SessionSetup reply that hasn't
         * been signed yet (server only signs the final SessionSetup reply).
         */
		return (0);
    }
    
    /* Its an anonymous login, signing is not supported */
	if ((vcp->vc_flags & SMBV_ANONYMOUS_ACCESS) == SMBV_ANONYMOUS_ACCESS) {
		return (0);
    }
    
    err = smb2_verify(rqp, mdp, nextCmdOffset, signature);
    
    if (err) {
        SMBDEBUG("Could not verify signature for sr_command %x, msgid: %llu\n", rqp->sr_command, rqp->sr_messageid);
        err = EAUTH;
    }
    
    return (err);
}
