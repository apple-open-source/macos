/*
 * Copyright (c) 2000-2001 Boris Popov
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
 * $Id: smb_smb.c,v 1.35.100.5.18.1 2005/10/01 23:12:22 lindak Exp $
 */
/*
 * various SMB requests. Most of the routines merely packs data into mbufs.
 */
#include <stdint.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/random.h>

#include <sys/mbuf.h>
#include <sys/smb_apple.h>
#include <sys/utfconv.h>

#include <sys/smb_iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_tran.h>
#include <fs/smbfs/smbfs_subr.h>

struct smb_dialect {
	int		d_id;
	const char *	d_name;
};

static struct smb_dialect smb_dialects[] = {
	{SMB_DIALECT_CORE,	"PC NETWORK PROGRAM 1.0"},
	{SMB_DIALECT_COREPLUS,	"MICROSOFT NETWORKS 1.03"},
	{SMB_DIALECT_LANMAN1_0,	"MICROSOFT NETWORKS 3.0"},
	{SMB_DIALECT_LANMAN1_0,	"LANMAN1.0"},
	{SMB_DIALECT_LANMAN2_0,	"LM1.2X002"},
	{SMB_DIALECT_LANMAN2_1,	"LANMAN2.1"},
	{SMB_DIALECT_NTLM0_12,	"NT LANMAN 1.0"},
	{SMB_DIALECT_NTLM0_12,	"NT LM 0.12"},
	{-1,			NULL}
};

#define	SMB_DIALECT_MAX	(sizeof(smb_dialects) / sizeof(struct smb_dialect) - 2)

u_int32_t
smb_vc_maxread(struct smb_vc *vcp)
{
	/*
	 * Specs say up to 64k data bytes, but Windows traffic
	 * uses 60k... no doubt for some good reason.
	 */
	if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_READX)
		return (60*1024);
	else
		return (vcp->vc_sopt.sv_maxtx);
}

u_int32_t
smb_vc_maxwrite(struct smb_vc *vcp)
{
	/*
	 * Specs say up to 64k data bytes, but Windows traffic
	 * uses 60k... probably for some good reason.
	 */
	if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_WRITEX)
		return (60*1024);
	else
		return (vcp->vc_sopt.sv_maxtx);
}

static int
smb_smb_nomux(struct smb_vc *vcp, struct smb_cred *scred, const char *name)
{
	if (scred == &vcp->vc_iod->iod_scred)
		return 0;
	SMBERROR("wrong function called(%s)\n", name);
	return EINVAL;
}

int
smb_smb_negotiate(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_dialect *dp;
	struct smb_sopt *sp = NULL;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc, stime[8], sblen;
	u_int16_t dindex, tw, tw1, swlen, bc;
	int error, maxqsz;
	int unicode = 0;
	char *servercs;
	void *servercshandle = NULL;
	void *localcshandle = NULL;
	u_int16_t toklen;

	if (smb_smb_nomux(vcp, scred, __FUNCTION__) != 0)
		return EINVAL;
	vcp->vc_hflags = SMB_FLAGS_CASELESS;
	/* Leave SMB_FLAGS2_UNICODE "off" - no need to do anything */ 
	vcp->vc_hflags2 |= SMB_FLAGS2_ERR_STATUS;
	vcp->obj.co_flags &= ~(SMBV_ENCRYPT);
	sp = &vcp->vc_sopt;
	bzero(sp, sizeof(struct smb_sopt));
	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_NEGOTIATE, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	for(dp = smb_dialects; dp->d_id != -1; dp++) {
		mb_put_uint8(mbp, SMB_DT_DIALECT);
		smb_put_dstring(mbp, vcp, dp->d_name, SMB_CS_NONE);
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	if (error)
		goto bad;
	smb_rq_getreply(rqp, &mdp);
	do {
		error = md_get_uint8(mdp, &wc);
		if (error)
			break;
		error = md_get_uint16le(mdp, &dindex);
		if (error)
			break;
		error = EBADRPC;
		if (dindex > 7) {
			SMBERROR("Don't know how to talk with server %s (%d)\n", "xxx", dindex);
			break;
		}
		dp = smb_dialects + dindex;
		sp->sv_proto = dp->d_id;
		SMBSDEBUG("Dialect %s (%d, %d)\n", dp->d_name, dindex, wc);
		if (dp->d_id >= SMB_DIALECT_NTLM0_12) {
			if (wc != 17)
				break;
			md_get_uint8(mdp, &sp->sv_sm);
			md_get_uint16le(mdp, &sp->sv_maxmux);
			md_get_uint16le(mdp, &sp->sv_maxvcs);
			md_get_uint32le(mdp, &sp->sv_maxtx);
			md_get_uint32le(mdp, &sp->sv_maxraw);
			md_get_uint32le(mdp, &sp->sv_skey);
			md_get_uint32le(mdp, &sp->sv_caps);
			md_get_mem(mdp, (caddr_t)stime, 8, MB_MSYSTEM);
			md_get_uint16le(mdp, (u_int16_t*)&sp->sv_tz);
			md_get_uint8(mdp, &sblen);
			error = md_get_uint16le(mdp, &bc);
			if (error)
				break;
			if (sp->sv_sm & SMB_SM_SIGS_REQUIRE)
				SMBERROR("server configuration requires packet signing, which we dont support\n");
			if (sp->sv_caps & SMB_CAP_UNICODE) {
				/*
				 * They do Unicode.
				 */
				vcp->obj.co_flags |= SMBV_UNICODE;
				unicode = 1;
			}
			if (!(sp->sv_caps & SMB_CAP_STATUS32)) {
				/*
				 * They don't do NT error codes.
				 *
				 * If we send requests with
				 * SMB_FLAGS2_ERR_STATUS set in
				 * Flags2, Windows 98, at least,
				 * appears to send replies with that
				 * bit set even though it sends back
				 * DOS error codes.  (They probably
				 * just use the request header as
				 * a template for the reply header,
				 * and don't bother clearing that bit.)
				 *
				 * Therefore, we clear that bit in
				 * our vc_hflags2 field.
				 */
				vcp->vc_hflags2 &= ~SMB_FLAGS2_ERR_STATUS;
			}
			if (dp->d_id == SMB_DIALECT_NTLM0_12 &&
			    sp->sv_maxtx < 4096 &&
			    (sp->sv_caps & SMB_CAP_NT_SMBS) == 0) {
				vcp->obj.co_flags |= SMBV_WIN95;
				SMBSDEBUG("Win95 detected\n");
			}

			/*
			 * 3 cases here:
			 *
			 * 1) Extended security.
			 * Read bc bytes below for security blob.
			 * Note that we DON'T put the Caps flag in outtok.
			 * outtoklen = bc
			 *
			 * 2) No extended security, have challenge data and
			 * possibly a domain name (which might be zero
			 * bytes long, meaning "missing").
			 * Copy challenge stuff to vcp->vc_ch (sblen bytes),
			 * then copy Cap flags and domain name (bc-sblen
			 * bytes) to outtok.
			 * outtoklen = bc-sblen+4, where the 4 is for the
			 * Caps flag.
			 *
			 * 3) No extended security, no challenge data, just
			 * possibly a domain name.
			 * Copy Capsflags and domain name (bc) to outtok.
			 * outtoklen = bc+4, where 4 is for the Caps flag
			 */

			/*
			 * Sanity check: make sure the challenge length
			 * isn't bigger than the byte count.
			 */
			if (sblen > bc) {
				error = EBADRPC;
				break;
			}
			toklen = bc;

			if (sblen && sblen <= SMB_MAXCHALLENGELEN && 
			    sp->sv_sm & SMB_SM_ENCRYPT) {
				error = md_get_mem(mdp, (caddr_t)(vcp->vc_ch), sblen,
						   MB_MSYSTEM);
				if (error)
					break;
				vcp->vc_chlen = sblen;
				vcp->obj.co_flags |= SMBV_ENCRYPT;
				toklen -= sblen; 

			}	

			/* For servers that don't support unicode
			* there are 2 things we could do:
			* 1) Pass the server Caps flags up to the 
			* user level so the logic up there will
			* know whether the domain name is unicode
			* (this is what I did).
			* 2) Try to convert the non-unicode string
			* to unicode. This doubles the length of
			* the outtok buffer and would be guessing that
			* the string was single-byte ascii, and that 
			* might be wrong. Why ask for trouble? */

			/* Warning: NetApp may omit the GUID */
			
			if (!(sp->sv_caps & SMB_CAP_EXT_SECURITY)) {
				/*
				 * No extended security.
				 * Stick domain name, if present,
				 * and caps in outtok.
				 */
				toklen = toklen + 4; /* space for Caps flags */
				vcp->vc_outtoklen =  toklen;
				vcp->vc_outtok = malloc(toklen, M_SMBTEMP,
							M_WAITOK);
				/* first store server capability bits */
				*(u_int32_t *)(vcp->vc_outtok) = sp->sv_caps;

				/*
				 * Then store the domain name if present;
				 * be sure to subtract 4 from the length
				 * for the Caps flag.
				 */
				if (toklen > 4) {
					error = md_get_mem(mdp,
					    vcp->vc_outtok+4, toklen-4,
					    MB_MSYSTEM);
				}
			} else {
				/*
				 * Extended security.
				 * Stick the rest of the buffer in outtok.
				 */
				vcp->vc_outtoklen =  toklen;
				vcp->vc_outtok = malloc(toklen, M_SMBTEMP,
							M_WAITOK);
				error = md_get_mem(mdp, vcp->vc_outtok, toklen,
						   MB_MSYSTEM);
			}
			break;
		}
		vcp->vc_hflags2 &= ~(SMB_FLAGS2_EXT_SEC|SMB_FLAGS2_DFS|
				     SMB_FLAGS2_ERR_STATUS|SMB_FLAGS2_UNICODE);
		if (dp->d_id > SMB_DIALECT_CORE) {
			md_get_uint16le(mdp, &tw);
			sp->sv_sm = tw;
			md_get_uint16le(mdp, &tw);
			sp->sv_maxtx = tw;
			md_get_uint16le(mdp, &sp->sv_maxmux);
			md_get_uint16le(mdp, &sp->sv_maxvcs);
			md_get_uint16le(mdp, &tw);	/* rawmode */
			md_get_uint32le(mdp, &sp->sv_skey);
			if (wc == 13) {		/* >= LANMAN1 */
				md_get_uint16(mdp, &tw);	/* time */
				md_get_uint16(mdp, &tw1);	/* date */
				md_get_uint16le(mdp, (u_int16_t*)&sp->sv_tz);
				md_get_uint16le(mdp, &swlen);
				if (swlen > SMB_MAXCHALLENGELEN)
					break;
				md_get_uint16(mdp, NULL);	/* mbz */
				if (md_get_uint16le(mdp, &bc) != 0)
					break;
				if (bc < swlen)
					break;
				if (swlen && (sp->sv_sm & SMB_SM_ENCRYPT)) {
					error = md_get_mem(mdp, (caddr_t)(vcp->vc_ch), swlen, MB_MSYSTEM);
					if (error)
						break;
					vcp->vc_chlen = swlen;
					vcp->obj.co_flags |= SMBV_ENCRYPT;
				}
			}
		} else {	/* an old CORE protocol */
			vcp->vc_hflags2 &= ~SMB_FLAGS2_KNOWS_LONG_NAMES;
			sp->sv_maxmux = 1;
		}
		error = 0;
	} while (0);
	if (error == 0) {
		vcp->vc_maxvcs = sp->sv_maxvcs;
		if (vcp->vc_maxvcs <= 1) {
			if (vcp->vc_maxvcs == 0)
				vcp->vc_maxvcs = 1;
		}
		if (sp->sv_maxtx <= 0 || sp->sv_maxtx > 0xffff)
			sp->sv_maxtx = 1024;
		else
			sp->sv_maxtx = min(sp->sv_maxtx,
					   63*1024 + SMB_HDRLEN + 16);
		SMB_TRAN_GETPARAM(vcp, SMBTP_RCVSZ, &maxqsz);
		vcp->vc_rxmax = min(smb_vc_maxread(vcp), maxqsz - 1024);
		SMB_TRAN_GETPARAM(vcp, SMBTP_SNDSZ, &maxqsz);
		vcp->vc_wxmax = min(smb_vc_maxwrite(vcp), maxqsz - 1024);
		vcp->vc_txmax = min(sp->sv_maxtx, maxqsz);
		SMBSDEBUG("TZ = %d\n", sp->sv_tz);
		SMBSDEBUG("CAPS = %x\n", sp->sv_caps);
		SMBSDEBUG("MAXMUX = %d\n", sp->sv_maxmux);
		SMBSDEBUG("MAXVCS = %d\n", sp->sv_maxvcs);
		SMBSDEBUG("MAXRAW = %d\n", sp->sv_maxraw);
		SMBSDEBUG("MAXTX = %d\n", sp->sv_maxtx);
	}

	/*
	 * If the server supports Unicode, set up to use Unicode
	 * when talking to them.  Othewise, use code page 437.
	 */
	if (unicode)
		servercs = "ucs-2";
	else {
		/*
		 * todo: if we can't determine the server's encoding, we
		 * need to try a best-guess here.
		 */
		servercs = "cp437";
	}
	error = iconv_open(servercs, "utf-8", &servercshandle);
	if (error != 0)
		goto bad;
	error = iconv_open("utf-8", servercs, &localcshandle);
	if (error != 0) {
		iconv_close(servercshandle);
		goto bad;
	}
	if (vcp->vc_toserver)
		iconv_close(vcp->vc_toserver);
	if (vcp->vc_tolocal)
		iconv_close(vcp->vc_tolocal);
	vcp->vc_toserver = servercshandle;
	vcp->vc_tolocal  = localcshandle;
	if (unicode)
		vcp->vc_hflags2 |= SMB_FLAGS2_UNICODE;
bad:
	smb_rq_done(rqp);
	return error;
}

static void
get_ascii_password(struct smb_vc *vcp, int upper, char *pbuf)
{
	if (upper) {
		iconv_convstr(vcp->vc_toupper, pbuf, smb_vc_getpass(vcp),
			      SMB_MAXPASSWORDLEN);
	} else {
		strncpy(pbuf, smb_vc_getpass(vcp), SMB_MAXPASSWORDLEN);
		pbuf[SMB_MAXPASSWORDLEN] = '\0';
	}
}

static void
get_unicode_password(struct smb_vc *vcp, char *pbuf)
{
	strncpy(pbuf, smb_vc_getpass(vcp), SMB_MAXPASSWORDLEN);
	pbuf[SMB_MAXPASSWORDLEN] = '\0';
}

static u_char *
add_name_to_blob(u_char *blobnames, struct smb_vc *vcp, const u_char *name,
		 size_t namelen, int nametype, int uppercase)
{
	struct ntlmv2_namehdr namehdr;
	char *namebuf;
	u_int16_t *uninamebuf;
	size_t uninamelen;

	if (name != NULL) {
		uninamebuf = malloc(2 * namelen, M_SMBTEMP, M_WAITOK);
		if (uppercase) {
			namebuf = malloc(namelen + 1, M_SMBTEMP, M_WAITOK);
			iconv_convstr(vcp->vc_toupper, namebuf, (char *)name, namelen);
			uninamelen = smb_strtouni(uninamebuf, namebuf, namelen,
			    UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
			free(namebuf, M_SMBTEMP);
		} else {
			uninamelen = smb_strtouni(uninamebuf, (char *)name, namelen,
			    UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
		}
	} else {
		uninamelen = 0;
		uninamebuf = NULL;
	}
	namehdr.type = htoles(nametype);
	namehdr.len = htoles(uninamelen);
	bcopy(&namehdr, blobnames, sizeof namehdr);
	blobnames += sizeof namehdr;
	if (uninamebuf != NULL) {
		bcopy(uninamebuf, blobnames, uninamelen);
		blobnames += uninamelen;
		free(uninamebuf, M_SMBTEMP);
	}
	return blobnames;
}

static u_char *
make_ntlmv2_blob(struct smb_vc *vcp, u_int64_t client_nonce, size_t *bloblen)
{
	u_char *blob;
	size_t blobsize;
	size_t domainlen, srvlen;
	struct ntlmv2_blobhdr *blobhdr;
	struct timespec now;
	u_int64_t timestamp;
	u_char *blobnames;

	/*
	 * XXX - the information at
	 *
	 * http://davenport.sourceforge.net/ntlm.html#theNtlmv2Response
	 *
	 * says that the "target information" comes from the Type 2 message,
	 * but, as we're not doing NTLMSSP, we don't have that.
	 *
	 * Should we use the names from the NegProt response?  Can we trust
	 * the NegProt response?  (I've seen captures where the primary
	 * domain name has an extra byte in front of it.)
	 *
	 * For now, we don't trust it - we use vcp->vc_domain and
	 * vcp->vc_srvname, instead.  We upper-case them and convert
	 * them to Unicode, as that's what's supposed to be in the blob.
	 */
	domainlen = strlen(vcp->vc_domain);
	srvlen = strlen(vcp->vc_srvname);
	blobsize = sizeof(struct ntlmv2_blobhdr)
	    + 3*sizeof (struct ntlmv2_namehdr) + 4 + 2*domainlen + 2*srvlen;
	blob = malloc(blobsize, M_SMBTEMP, M_WAITOK);
	bzero(blob, blobsize);
	blobhdr = (struct ntlmv2_blobhdr *)blob;
	blobhdr->header = htolel(0x00000101);
	nanotime(&now);
	smb_time_local2NT(&now, 0, &timestamp);
	blobhdr->timestamp = htoleq(timestamp);
	blobhdr->client_nonce = client_nonce;
	blobnames = blob + sizeof (struct ntlmv2_blobhdr);
	blobnames = add_name_to_blob(blobnames, vcp, (u_char *)(vcp->vc_domain), domainlen,
				     NAMETYPE_DOMAIN_NB, 1);
	blobnames = add_name_to_blob(blobnames, vcp, (u_char *)(vcp->vc_srvname), srvlen,
				     NAMETYPE_MACHINE_NB, 1);
	blobnames = add_name_to_blob(blobnames, vcp, NULL, 0, NAMETYPE_EOL, 0);
	*bloblen = blobnames - blob;
	return (blob);
}

static char *
uppercasify_string(struct smb_vc *vcp, const char *string)
{
	size_t stringlen;
	char *ucstrbuf;

	stringlen = strlen(string);
	ucstrbuf = malloc(stringlen + 1, M_SMBTEMP, M_WAITOK);
	iconv_convstr(vcp->vc_toupper, ucstrbuf, string, stringlen);
	return (ucstrbuf);
}

/*
 * See radar 4134676.  This define helps us avoid how a certain old server
 * grants limited Guest access when we try NTLMv2, but works fine with NTLM.
 * The fingerprint we are looking for here is DOS error codes and no-Unicode.
 * Note XP grants Guest access but uses Unicode and NT error codes.
 */
#define smb_antique(rqp) (!((rqp)->sr_rpflags2 & SMB_FLAGS2_ERR_STATUS) && \
			  !((rqp)->sr_rpflags2 & SMB_FLAGS2_UNICODE))

/*
 * When not doing Kerberos, we can try, in order:
 *
 *	NTLMv2
 *	NTLM with the ASCII password not upper-cased
 *	NTLM with the ASCII password upper-cased
 *
 * if the server supports encrypted passwords, or
 *
 *	plain-text with the ASCII password not upper-cased
 *	plain-text with the ASCII password upper-cased
 *
 * if it doesn't.
 */
#define STATE_NTLMV2	0
#define STATE_NOUCPW	1
#define STATE_UCPW	2

int
smb_smb_ssnsetup(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
/*	u_int16_t tw, tw1;*/
	int minauth;
	smb_uniptr unipp = NULL, ntencpass = NULL;
	char *pp = NULL, *up, *ucup, *ucdp;
	char *pbuf = NULL;
	char *encpass = NULL;
	int error = 0, ulen;
	size_t plen = 0, uniplen = 0;
	int state;
	size_t ntlmv2_bloblen;
	u_char *ntlmv2_blob;
	u_int64_t client_nonce;
	u_int32_t caps = 0;
	u_int16_t bl; /* BLOB length */
	u_int16_t stringlen;
	u_short	saveflags2 = vcp->vc_hflags2;
	void *	savetoserver = vcp->vc_toserver;
	u_int16_t action;
	int declinedguest = 0;

	caps |= SMB_CAP_LARGE_FILES;

	if (vcp->obj.co_flags & SMBV_UNICODE) 
		caps |= SMB_CAP_UNICODE;
	/* No unicode unless server supports and encryption on */
	if (!((vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT) && (vcp->obj.co_flags & SMBV_UNICODE))) {
		vcp->vc_hflags2 &= 0xffff - SMB_FLAGS2_UNICODE;
		vcp->vc_toserver = 0;
	}

        if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)
                caps |= SMB_CAP_NT_SMBS; /* we do if they do */
	minauth = vcp->obj.co_flags & SMBV_MINAUTH;
	if (vcp->vc_intok) {
		if (vcp->vc_intoklen > 65536 ||
		    !(vcp->vc_hflags2 & SMB_FLAGS2_EXT_SEC) ||
		    SMB_DIALECT(vcp) < SMB_DIALECT_NTLM0_12) {
			error = EINVAL;
			goto ssn_exit;
		}
		vcp->vc_smbuid = 0;
	}
	if (vcp->vc_hflags2 & SMB_FLAGS2_ERR_STATUS)
		caps |= SMB_CAP_STATUS32;
	if (vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT)
		state = STATE_NTLMV2;	/* try NTLMv2 first */
	else
		state = STATE_NOUCPW;	/* try plain-text mixed-case first */
again:

	if (!vcp->vc_intok)
		vcp->vc_smbuid = SMB_UID_UNKNOWN;

	if (smb_smb_nomux(vcp, scred, __FUNCTION__) != 0) {
		error = EINVAL;
		goto ssn_exit;
	}

	if (!vcp->vc_intok) {
		/*
		 * We're not doing extended security, which, for
		 * now, means we're not doing Kerberos.
		 * Fail if the minimum authentication level is
		 * Kerberos.
		 */
		if (minauth >= SMBV_MINAUTH_KERBEROS) {
			error = EAUTH;
			goto ssn_exit;
		}
		if (vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT) {
			/*
			 * Encrypted passwords.
			 * If we're going to try NTLM, fail if the minimum
			 * authentication level is NTLMv2 or better.
			 */
			if (state > STATE_NTLMV2) {
				if (minauth >= SMBV_MINAUTH_NTLMV2) {
					error =  EAUTH;
					goto ssn_exit;
				}
			}
		} else {
			/*
			 * Plain-text passwords.
			 * Fail if the minimum authentication level is
			 * LM or better.
			 */
			if (minauth >= SMBV_MINAUTH_LM) {
				error =  EAUTH;
				goto ssn_exit;
			}
		}
	}

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX,
			     scred, &rqp);
	if (error)
		goto ssn_exit;
	/*
	 * Domain name must be upper-case, as that's what's used
	 * when computing LMv2 and NTLMv2 responses - and, for NTLMv2,
	 * the domain name in the request has to be upper-cased as well.
	 * (That appears not to be the case for the user name.  Go
	 * figure.)
	 *
	 * don't need to uppercase domain string. It's already uppercase UTF-8. */

	stringlen = strlen(vcp->vc_domain)+1 /* strlen doesn't count null */;
	ucdp = malloc(stringlen, M_SMBTEMP, M_WAITOK);
	memcpy(ucdp,vcp->vc_domain,stringlen);

	if (vcp->vc_intok) {
		caps |= SMB_CAP_EXT_SECURITY;
	} else if (!(vcp->vc_sopt.sv_sm & SMB_SM_USER)) {
		/*
		 * In the share security mode password will be used
		 * only in the tree authentication
		 */
		 pp = "";
		 plen = 1;
	} else {
		pbuf = malloc(SMB_MAXPASSWORDLEN + 1, M_SMBTEMP, M_WAITOK);
		if (vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT) {
			if (state == STATE_NTLMV2) {
				/*
				 * Compute the LMv2 and NTLMv2 responses,
				 * derived from the challenge, the user name,
				 * the domain/workgroup into which we're
				 * logging, and the Unicode password.
				 */
				get_unicode_password(vcp, pbuf);

				/*
				 * Construct the client nonce by getting
				 * a bunch of random data.
				 */
				read_random((void *) &client_nonce,
					    sizeof(client_nonce));

				/*
				 * Convert the user name to upper-case, as
				 * that's what's used when computing LMv2
				 * and NTLMv2 responses.
				 */
				ucup = uppercasify_string(vcp, vcp->vc_username);

				/*
				 * Compute the LMv2 response, derived
				 * from the server challenge, the
				 * user name, the domain/workgroup
				 * into which we're logging, the
				 * client nonce, and the Unicode
				 * password.
				 */
				smb_ntlmv2response((u_char *)pbuf, (u_char *)ucup, (u_char *)ucdp, (u_char *)vcp->vc_ch,
						  (u_char *)&client_nonce, 8,
						  (u_char**)&encpass, &plen);
				pp = encpass;

				/*
				 * Construct the blob.
				 */
				ntlmv2_blob = make_ntlmv2_blob(vcp,
							       client_nonce,
							       &ntlmv2_bloblen);

				/*
				 * Compute the NTLMv2 response, derived
				 * from the server challenge, the
				 * user name, the domain/workgroup
				 * into which we're logging, the
				 * blob, and the Unicode password.
				 */
				smb_ntlmv2response((u_char *)pbuf, (u_char *)ucup, (u_char *)ucdp, 
						vcp->vc_ch, ntlmv2_blob, ntlmv2_bloblen,
						(u_char**)&ntencpass, &uniplen);
				free(ucup, M_SMBTEMP);
				free(ntlmv2_blob, M_SMBTEMP);
				unipp = ntencpass;
			} else {
				plen = 24;
				encpass = malloc(plen, M_SMBTEMP, M_WAITOK);
				if (minauth >= SMBV_MINAUTH_NTLM) {
					/*
					 * Don't put the LM response on
					 * the wire - it's too easy to
					 * crack.
					 */
					bzero(encpass, plen);
				} else {
					/*
					 * Compute the LM response, derived
					 * from the challenge and the ASCII
					 * password.
					 *
					 * We try w/o uppercasing first so
					 * Samba mixed case passwords work.
					 * If that fails, we come back and
					 * try uppercasing to satisfy OS/2
					 * and Windows for Workgroups.
					 */
					get_ascii_password(vcp,
							   (state == STATE_UCPW),
							   pbuf);
					smb_lmresponse((u_char *)pbuf, vcp->vc_ch,
						       (u_char *)encpass);
				}
				pp = encpass;

				/*
				 * Compute the NTLM response, derived from
				 * the challenge and the Unicode password.
				 */
				get_unicode_password(vcp, pbuf);
				uniplen = 24;
				ntencpass = malloc(uniplen, M_SMBTEMP,
						   M_WAITOK);
				smb_ntlmresponse((u_char *)pbuf, vcp->vc_ch,
						(u_char*)ntencpass);
				unipp = ntencpass;
			}
		} else {
			/*
			 * We try w/o uppercasing first so Samba mixed case
			 * passwords work.  If that fails, we come back and
			 * try uppercasing to satisfy OS/2 and Windows for
			 * Workgroups.
			 */
			get_ascii_password(vcp, (state == STATE_UCPW), pbuf);
			plen = strlen(pbuf) + 1;
			pp = pbuf;
			uniplen = plen * 2;
			ntencpass = malloc(uniplen, M_SMBTEMP, M_WAITOK);
			(void)smb_strtouni(ntencpass, smb_vc_getpass(vcp), 0,
					   UTF_PRECOMPOSED);
			plen--;
			/*
			 * The uniplen is zeroed because Samba cannot deal
			 * with this 2nd cleartext password.  This Samba
			 * "bug" is actually a workaround for problems in
			 * Microsoft clients.
			 */
			uniplen = 0/*-= 2*/;
			unipp = ntencpass;
		}
	}
	smb_rq_wstart(rqp);
	mbp = &rqp->sr_rq;
	up = vcp->vc_username;
	/*
	 * If userid is null we are attempting anonymous browse login
	 * so passwords must be zero length.
	 */
	if (*up == '\0')
		plen = uniplen = 0;
	ulen = strlen(up) + 1;
	mb_put_uint8(mbp, 0xff);
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0);
	mb_put_uint16le(mbp, vcp->vc_sopt.sv_maxtx);
	mb_put_uint16le(mbp, vcp->vc_sopt.sv_maxmux);
	mb_put_uint16le(mbp, vcp->vc_number);
	mb_put_uint32le(mbp, vcp->vc_sopt.sv_skey);
	if (SMB_DIALECT(vcp) < SMB_DIALECT_NTLM0_12) {
		mb_put_uint16le(mbp, plen);
		mb_put_uint32le(mbp, 0);
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		mb_put_mem(mbp, pp, plen, MB_MSYSTEM);
		smb_put_dstring(mbp, vcp, up, SMB_CS_NONE);
	} else {
		if (vcp->vc_intok) {
			mb_put_uint16le(mbp, vcp->vc_intoklen);
			mb_put_uint32le(mbp, 0);		/* reserved */
			mb_put_uint32le(mbp, caps);		/* my caps */
			smb_rq_wend(rqp);
			smb_rq_bstart(rqp);
			mb_put_mem(mbp, vcp->vc_intok, vcp->vc_intoklen,
				   MB_MSYSTEM);	/* security blob */
		} else { /* no extended security */
			mb_put_uint16le(mbp, plen);
			mb_put_uint16le(mbp, uniplen);
			mb_put_uint32le(mbp, 0);		/* reserved */
			mb_put_uint32le(mbp, caps);		/* my caps */
			smb_rq_wend(rqp);
			smb_rq_bstart(rqp);
			mb_put_mem(mbp, pp, plen, MB_MSYSTEM); /* password */
			if (uniplen)
				mb_put_mem(mbp, (caddr_t)unipp, uniplen, MB_MSYSTEM);
			smb_put_dstring(mbp, vcp, up, SMB_CS_NONE); /* user */
			smb_put_dstring(mbp, vcp, ucdp, SMB_CS_NONE); /* domain */
		}
		smb_put_dstring(mbp, vcp, "MacOSX", SMB_CS_NONE);
		smb_put_dstring(mbp, vcp, "NETSMB", SMB_CS_NONE); /* LAN Mgr */
	}
	smb_rq_bend(rqp);
	if (ntencpass) {
		free(ntencpass, M_SMBTEMP);
		ntencpass = NULL;
	}
	free(ucdp, M_SMBTEMP);
	error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);
	SMBSDEBUG("%d\n", error);
	if (error) {
		if (rqp->sr_errclass == ERRDOS && rqp->sr_serror == ERRnoaccess)
			error = EAUTH;
		if (!(rqp->sr_errclass == ERRDOS &&
		      rqp->sr_serror == ERRmoredata))
			goto bad;
	}
	vcp->vc_smbuid = rqp->sr_rpuid;
	smb_rq_getreply(rqp, &mdp);
	do {
		error = md_get_uint8(mdp, &wc);
		if (error)
			break;
		error = EBADRPC;
		if (vcp->vc_intok) {
			if (wc != 4)
				break;
		} else if (wc != 3)
			break;
		md_get_uint8(mdp, NULL);	/* secondary cmd */
		md_get_uint8(mdp, NULL);	/* mbz */
		md_get_uint16le(mdp, NULL);	/* andxoffset */
		md_get_uint16le(mdp, &action);	/* action */
		if (vcp->vc_intok)
			md_get_uint16le(mdp, &bl);	/* ext security */
		md_get_uint16le(mdp, NULL); /* byte count */
		if (vcp->vc_intok) {
			vcp->vc_outtoklen =  bl;
			vcp->vc_outtok = malloc(bl, M_SMBTEMP, M_WAITOK);
			error = md_get_mem(mdp, vcp->vc_outtok, bl, MB_MSYSTEM);
			if (error)
				break;
		}
		/* server OS, LANMGR, & Domain here*/
		error = 0;
	} while (0);
bad:
	if (encpass) {
		free(encpass, M_SMBTEMP);
		encpass = NULL;
	}
	if (pbuf) {
		free(pbuf, M_SMBTEMP);
		pbuf = NULL;
	}
	if (vcp->vc_sopt.sv_sm & SMB_SM_USER && !vcp->vc_intok &&
	    (error || (*up != '\0' && action & SMB_ACT_GUEST &&
		       state == STATE_NTLMV2 && smb_antique(rqp)))) {
		/*
		 * We're doing user-level authentication (so we are actually
		 * sending authentication stuff over the wire), and we're
		 * not doing extended security, and the stuff we tried
		 * failed (or we we're trying to login a real user but
		 * got granted guest access instead.)
		 */
		if (!error)
			declinedguest = 1;
		/*
		 * Should we try the next type of authentication?
		 */
		if (state < STATE_UCPW) {
			/*
			 * Yes, we still have more to try.
			 */
			state++;
			smb_rq_done(rqp);
			goto again;
		}
	}
	smb_rq_done(rqp);

ssn_exit:
	if (error && declinedguest)
		SMBERROR("we declined ntlmv2 guest access. errno will be %d\n",
			 error);
	/* Restore things we changed and return */
	vcp->vc_hflags2 = saveflags2;
	vcp->vc_toserver = savetoserver;
	return (error);
}

int
smb_smb_ssnclose(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	if (vcp->vc_smbuid == SMB_UID_UNKNOWN)
		return 0;

	if (smb_smb_nomux(vcp, scred, __FUNCTION__) != 0)
		return EINVAL;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_LOGOFF_ANDX, scred, &rqp);
	if (error)
		return error;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	smb_rq_done(rqp);
	return error;
}

static char smb_any_share[] = "?????";

static char *
smb_share_typename(int stype)
{
	char *pp;

	switch (stype) {
	    case SMB_ST_DISK:
		pp = "A:";
		break;
	    case SMB_ST_PRINTER:
		pp = smb_any_share;		/* can't use LPT: here... */
		break;
	    case SMB_ST_PIPE:
		pp = "IPC";
		break;
	    case SMB_ST_COMM:
		pp = "COMM";
		break;
	    case SMB_ST_ANY:
	    default:
		pp = smb_any_share;
		break;
	}
	return pp;
}

int
smb_smb_treeconnect(struct smb_share *ssp, struct smb_cred *scred)
{
	struct smb_vc *vcp;
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	char *pp, *pbuf, *encpass;
	int error, plen, caseopt;
	int upper = 0;
	u_int16_t save_hflags2;
	void *save_toserver;

        vcp = SSTOVC(ssp);
       
	save_hflags2 = vcp->vc_hflags2;
	save_toserver = vcp->vc_toserver;
 again:
	/* Generic server name when NBNS query fails */
        if (!strcmp(vcp->vc_srvname,"*SMBSERVER")) {
		/* Unicode causes problems in this case */
		vcp->vc_hflags2 &= ~SMB_FLAGS2_UNICODE;
		vcp->vc_toserver = 0;
	}
 
	ssp->ss_tid = SMB_TID_UNKNOWN;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_TREE_CONNECT_ANDX, scred, &rqp);
	if (error)
		goto treeconnect_exit;
	caseopt = SMB_CS_NONE;
	if (vcp->vc_sopt.sv_sm & SMB_SM_USER) {
		plen = 1;
		pp = "";
		pbuf = NULL;
		encpass = NULL;
	} else {
		pbuf = malloc(SMB_MAXPASSWORDLEN + 1, M_SMBTEMP, M_WAITOK);
		encpass = malloc(24, M_SMBTEMP, M_WAITOK);
		/*
		 * We try w/o uppercasing first so Samba mixed case
		 * passwords work.  If that fails we come back and try
		 * uppercasing to satisfy OS/2 and Windows for Workgroups.
		 */
		if (upper++) {
			iconv_convstr(vcp->vc_toupper, pbuf,
				      smb_share_getpass(ssp),
				      SMB_MAXPASSWORDLEN);
		} else {
			strncpy(pbuf, smb_share_getpass(ssp),
				SMB_MAXPASSWORDLEN);
			pbuf[SMB_MAXPASSWORDLEN] = '\0';
		}
		if (vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT) {
			plen = 24;
			smb_lmresponse((u_char *)pbuf, vcp->vc_ch, (u_char *)encpass);
			pp = encpass;
		} else {
			plen = strlen(pbuf) + 1;
			pp = pbuf;
		}
	}
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0);
	mb_put_uint16le(mbp, 0);		/* Flags */
	mb_put_uint16le(mbp, plen);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	error = mb_put_mem(mbp, pp, plen, MB_MSYSTEM);
	if (error) {
		SMBERROR("error %d from mb_put_mem for pp\n", error);
		goto bad;
	}
	smb_put_dmem(mbp, vcp, "\\\\", 2, caseopt, NULL);
	pp = vcp->vc_srvname;
	error = smb_put_dmem(mbp, vcp, pp, strlen(pp), caseopt, NULL);
	if (error) {
		SMBERROR("error %d from smb_put_dmem for srvname\n", error);
		goto bad;
	}
	smb_put_dmem(mbp, vcp, "\\", 1, caseopt, NULL);
	pp = ssp->ss_name;
	error = smb_put_dstring(mbp, vcp, pp, caseopt);
	if (error) {
		SMBERROR("error %d from smb_put_dstring for ss_name\n", error);
		goto bad;
	}
	/* The type name is always ASCII */
	pp = smb_share_typename(ssp->ss_type); 
	error = mb_put_mem(mbp, pp, strlen(pp) + 1, MB_MSYSTEM);
	if (error) {
		SMBERROR("error %d from mb_put_mem for ss_type\n", error);
		goto bad;
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	if (error)
		goto bad;
	ssp->ss_tid = rqp->sr_rptid;
	ssp->ss_vcgenid = vcp->vc_genid;
	ssp->ss_flags |= SMBS_CONNECTED;
bad:
	if (encpass)
		free(encpass, M_SMBTEMP);
	if (pbuf)
		free(pbuf, M_SMBTEMP);
	smb_rq_done(rqp);
	if (error && upper == 1)
		goto again;
treeconnect_exit:
	vcp->vc_hflags2 = save_hflags2;
	vcp->vc_toserver = save_toserver;
	return error;
}

int
smb_smb_treedisconnect(struct smb_share *ssp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	if (ssp->ss_tid == SMB_TID_UNKNOWN)
		return 0;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_TREE_DISCONNECT, scred, &rqp);
	if (error)
		return error;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	smb_rq_done(rqp);
	ssp->ss_tid = SMB_TID_UNKNOWN;
	return error;
}

static __inline int
smb_smb_readx(struct smb_share *ssp, u_int16_t fid, int *len, int *rresid,
	      uio_t uio, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	int error;
	u_int16_t residhi, residlo, off, doff;
	u_int32_t resid;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_READ_ANDX, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* no secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to secondary */
	mb_put_mem(mbp, (caddr_t)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint32le(mbp, (u_int32_t)uio_offset(uio));
	*len = min(SSTOVC(ssp)->vc_rxmax, *len);
	mb_put_uint16le(mbp, (u_int16_t)*len);	/* MaxCount */
	mb_put_uint16le(mbp, (u_int16_t)*len);	/* MinCount (only indicates blocking) */
	mb_put_uint32le(mbp, (unsigned)*len >> 16);	/* MaxCountHigh */
	mb_put_uint16le(mbp, (u_int16_t)*len);	/* Remaining ("obsolete") */
	mb_put_uint32le(mbp, (u_int32_t)(uio_offset(uio) >> 32));
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	do {
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		off = SMB_HDRLEN;
		md_get_uint8(mdp, &wc);
		off++;
		if (wc != 12) {
			error = EBADRPC;
			break;
		}
		md_get_uint8(mdp, NULL);
		off++;
		md_get_uint8(mdp, NULL);
		off++;
		md_get_uint16le(mdp, NULL);
		off += 2;
		md_get_uint16le(mdp, NULL);
		off += 2;
		md_get_uint16le(mdp, NULL);	/* data compaction mode */
		off += 2;
		md_get_uint16le(mdp, NULL);
		off += 2;
		md_get_uint16le(mdp, &residlo);
		off += 2;
		md_get_uint16le(mdp, &doff);	/* data offset */
		off += 2;
		md_get_uint16le(mdp, &residhi);
		off += 2;
		resid = (residhi << 16) | residlo;
		md_get_mem(mdp, NULL, 4 * 2, MB_MSYSTEM);
		off += 4*2;
		md_get_uint16le(mdp, NULL);	/* ByteCount */
		off += 2;
		if (doff > off)	/* pad byte(s)? */
			md_get_mem(mdp, NULL, doff - off, MB_MSYSTEM);
		if (resid == 0) {
			*rresid = resid;
			break;
		}
		error = md_get_uio(mdp, uio, resid);
		if (error)
			break;
		*rresid = resid;
	} while(0);
	smb_rq_done(rqp);
	return (error);
}

static __inline int
smb_smb_writex(struct smb_share *ssp, u_int16_t fid, int *len, int *rresid,
	uio_t uio, struct smb_cred *scred, int timo)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	u_int8_t wc;
	u_int16_t resid;

	if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_WRITEX) {
		*len = min(*len, 0x1ffff);
	} else {
		*len = min(*len, min(0xffff,
				     SSTOVC(ssp)->vc_txmax - SMB_HDRLEN - 34));
		/* (data starts 18 bytes further in than SMB_COM_WRITE) */
	}
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_WRITE_ANDX, scred, &rqp);
	if (error)
		return (error);
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* no secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to secondary */
	mb_put_mem(mbp, (caddr_t)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint32le(mbp, (u_int32_t)uio_offset(uio));
	mb_put_uint32le(mbp, 0);	/* MBZ (timeout) */
	mb_put_uint16le(mbp, 0);	/* !write-thru */
	mb_put_uint16le(mbp, 0);
	*len = min(SSTOVC(ssp)->vc_wxmax, *len);
	mb_put_uint16le(mbp, (u_int16_t)((unsigned)*len >> 16));
	mb_put_uint16le(mbp, (u_int16_t)*len);
	mb_put_uint16le(mbp, 64);	/* data offset from header start */
	mb_put_uint32le(mbp, (u_int32_t)(uio_offset(uio) >> 32));
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	do {
		mb_put_uint8(mbp, 0xee);	/* mimic xp pad byte! */
		error = mb_put_uio(mbp, uio, *len);
		if (error)
			break;
		smb_rq_bend(rqp);
		error = timo ? smb_rq_simple_timed(rqp, timo)
			     : smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		md_get_uint8(mdp, &wc);
		if (wc != 6) {
			error = EBADRPC;
			break;
		}
		md_get_uint8(mdp, NULL);
		md_get_uint8(mdp, NULL);
		md_get_uint16le(mdp, NULL);
		md_get_uint16le(mdp, &resid); /* actually is # written */
		*rresid = resid;
		/*
		 * if LARGE_WRITEX then there's one more bit of # written
		 */
		if ((SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_WRITEX)) {
			md_get_uint16le(mdp, NULL);
			md_get_uint16le(mdp, &resid);
			*rresid |= (int)(resid & 1) << 16;
		}
	} while(0);

	smb_rq_done(rqp);
	return (error);
}

static __inline int
smb_smb_read(struct smb_share *ssp, u_int16_t fid,
	int *len, int *rresid, uio_t uio, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t resid, bc;
	u_int8_t wc;
	int error, rlen;

	if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES ||
	    SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_READX)
		return (smb_smb_readx(ssp, fid, len, rresid, uio, scred));

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_READ, scred, &rqp);
	if (error)
		return error;

	*len = rlen = min(*len, min(0xffff, SSTOVC(ssp)->vc_txmax - SMB_HDRLEN - 16));

	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint16le(mbp, (u_int16_t)rlen);
	mb_put_uint32le(mbp, (u_int32_t)uio_offset(uio));
	mb_put_uint16le(mbp, (u_int16_t)min(uio_resid(uio), 0xffff));
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	do {
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		md_get_uint8(mdp, &wc);
		if (wc != 5) {
			error = EBADRPC;
			break;
		}
		md_get_uint16le(mdp, &resid);
		md_get_mem(mdp, NULL, 4 * 2, MB_MSYSTEM);
		md_get_uint16le(mdp, &bc);
		md_get_uint8(mdp, NULL);		/* ignore buffer type */
		md_get_uint16le(mdp, &resid);
		if (resid == 0) {
			*rresid = resid;
			break;
		}
		error = md_get_uio(mdp, uio, resid);
		if (error)
			break;
		*rresid = resid;
	} while(0);
	smb_rq_done(rqp);
	return error;
}

int
smb_read(struct smb_share *ssp, u_int16_t fid, uio_t uio,
	struct smb_cred *scred)
{
	int tsize, len, resid = 0;
	int error = 0;

	tsize = uio_resid(uio);
	while (tsize > 0) {
		len = tsize;
		error = smb_smb_read(ssp, fid, &len, &resid, uio, scred);
		if (error)
			break;
		tsize -= resid;
		if (resid < len)
			break;
	}
	return error;
}


static __inline int
smb_smb_write(struct smb_share *ssp, u_int16_t fid, int *len, int *rresid,
	uio_t uio, struct smb_cred *scred, int timo)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t resid;
	u_int8_t wc;
	int error;

	if (uio_offset(uio) + *len > UINT32_MAX &&
	    !(SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES))
		return (EFBIG);
	if (*len && (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES ||
		     SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_WRITEX))
		return (smb_smb_writex(ssp, fid, len, rresid,
				       uio, scred, timo));

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_WRITE, scred, &rqp);
	if (error)
		return error;

	resid = min(*len, min(0xffff, SSTOVC(ssp)->vc_txmax - SMB_HDRLEN - 16));
	*len = resid;

	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint16le(mbp, resid);
	mb_put_uint32le(mbp, (u_int32_t)uio_offset(uio));
	mb_put_uint16le(mbp, (u_int16_t)min(uio_resid(uio), 0xffff));
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_DATA);
	mb_put_uint16le(mbp, resid);
	do {
		error = mb_put_uio(mbp, uio, resid);
		if (error)
			break;
		smb_rq_bend(rqp);
		error = timo ? smb_rq_simple_timed(rqp, timo)
			     : smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		md_get_uint8(mdp, &wc);
		if (wc != 1) {
			error = EBADRPC;
			break;
		}
		md_get_uint16le(mdp, &resid);
		*rresid = resid;
	} while(0);
	smb_rq_done(rqp);
	return error;
}

int
smb_write(struct smb_share *ssp, u_int16_t fid, uio_t uio,
	struct smb_cred *scred, int timo)
{
	int error = 0, len, tsize, resid = 0;
	user_ssize_t  old_resid;
	off_t  old_offset;

	tsize = old_resid = uio_resid(uio);
	old_offset = uio_offset(uio);

	while (tsize > 0) {
		len = tsize;
		// LP64todo - resid, tsize should be 64-bit values
		error = smb_smb_write(ssp, fid, &len, &resid, uio, scred, timo);
		timo = 0; /* only first write is special */
		if (error)
			break;
		if (resid < len) {
			error = EIO;
			break;
		}
		tsize -= resid;
	}
	if (error) {
		/*
		 * Errors can happen on the copyin, the rpc, etc.  So they
		 * imply resid is unreliable.  The only safe thing is
		 * to pretend zero bytes made it.  We needn't restore the
		 * iovs because callers don't depend on them in error
		 * paths - uio_resid and uio_offset are what matter.
		 */
		uio_setresid(uio, old_resid);
		uio_setoffset(uio, old_offset);
	}
	return error;
}

static u_int32_t	smbechoes = 0;
int
smb_smb_echo(struct smb_vc *vcp, struct smb_cred *scred, int timo)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_ECHO, scred, &rqp);
	if (error)
		return error;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, 1); /* echo count */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smbechoes++;
	mb_put_uint32le(mbp, smbechoes);
	smb_rq_bend(rqp);
	error = smb_rq_simple_timed(rqp, timo);
	SMBSDEBUG("%d\n", error);
	smb_rq_done(rqp);
	return error;
}

int
smb_smb_checkdir(struct smb_share *ssp, struct smbnode *dnp, char *name, int nmlen, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_CHECK_DIRECTORY, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	smbfs_fullpath(mbp, SSTOVC(ssp), dnp, name, &nmlen, '\\');
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	smb_rq_done(rqp);
	return error;
}
