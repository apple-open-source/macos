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
 * $Id: smb_smb.c,v 1.23 2003/09/06 20:27:15 lindak Exp $
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

#ifdef APPLE
#include <sys/mbuf.h>
#include <sys/smb_apple.h>
#endif

#include <sys/iconv.h>

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
	{SMB_DIALECT_LANMAN2_0,	"Samba"},
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
#ifdef APPLE
	int unicode = SMB_UNICODE_STRINGS(vcp);
	void * servercharset = vcp->vc_toserver;
	void * localcharset = vcp->vc_tolocal;
#endif

	if (smb_smb_nomux(vcp, scred, __FUNCTION__) != 0)
		return EINVAL;
#ifdef APPLE
	/* Disable Unicode for SMB_COM_NEGOTIATE requests */ 
	if (unicode) {
		/* Use NULL until ASCII -> UTF-8 works */
		vcp->vc_toserver = NULL;
		vcp->vc_tolocal  = NULL;
	}
#endif
	vcp->vc_hflags = SMB_FLAGS_CASELESS;
#ifndef XXX
	vcp->vc_hflags2 |= SMB_FLAGS2_ERR_STATUS;
#endif
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
			md_get_mem(mdp, stime, 8, MB_MSYSTEM);
			md_get_uint16le(mdp, (u_int16_t*)&sp->sv_tz);
			md_get_uint8(mdp, &sblen);
			error = md_get_uint16le(mdp, &bc);
			if (error)
				break;
#ifdef APPLE
			if (sp->sv_caps & SMB_CAP_UNICODE) {
				/*
				 * They do Unicode.
				 */
				vcp->obj.co_flags |= SMBV_UNICODE;
			}
			if (!(sp->sv_caps & SMB_CAP_STATUS32)) {
				/*
				 * They don't do NT error codes.
				 *
				 * If we send requests with
				 * SMB_FLAGS2_ERR_STATUS set in
				 * Flags2, Windows 98, at least
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
#endif
			if (dp->d_id == SMB_DIALECT_NTLM0_12 &&
			    sp->sv_maxtx < 4096 &&
			    (sp->sv_caps & SMB_CAP_NT_SMBS) == 0) {
				vcp->obj.co_flags |= SMBV_WIN95;
				SMBSDEBUG("Win95 detected\n");
			}
			if (sblen && sblen <= SMB_MAXCHALLENGELEN &&
			    sp->sv_sm & SMB_SM_ENCRYPT) {
				error = md_get_mem(mdp, vcp->vc_ch, sblen,
						   MB_MSYSTEM);
				if (error)
					break;
				vcp->vc_chlen = sblen;
				vcp->obj.co_flags |= SMBV_ENCRYPT;
				break;
			}
			if (!(sp->sv_caps & SMB_CAP_EXT_SECURITY))
				break;
			error = EBADRPC;
			/* Warning: NetApp may omit the GUID */
			vcp->vc_outtoklen =  bc;
			vcp->vc_outtok = malloc(bc, M_SMBTEMP, M_WAITOK);
			error = md_get_mem(mdp, vcp->vc_outtok, bc, MB_MSYSTEM);
			break;
		}
		vcp->vc_hflags2 &= ~(SMB_FLAGS2_EXT_SEC|SMB_FLAGS2_DFS|
				     SMB_FLAGS2_ERR_STATUS|SMB_FLAGS2_UNICODE);
		unicode = 0;
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
					error = md_get_mem(mdp, vcp->vc_ch, swlen, MB_MSYSTEM);
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
bad:
#ifdef APPLE
	/* Restore Unicode conversion state */
	if (unicode) {
		vcp->vc_toserver = servercharset;
		vcp->vc_tolocal  = localcharset;
		vcp->vc_hflags2 |= SMB_FLAGS2_UNICODE;
	}
#endif
	smb_rq_done(rqp);
	return error;
}

int
smb_smb_ssnsetup(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
/*	u_int16_t tw, tw1;*/
	smb_uniptr unipp = NULL, ntencpass = NULL;
	char *pp = NULL, *up;
	char *pbuf = NULL;
	char *encpass = NULL;
	int error, plen = 0, uniplen = 0, ulen;
	int upper = 0;
	u_int32_t caps = 0;
	u_int16_t bl; /* BLOB length */

#ifdef APPLE
	caps |= SMB_CAP_LARGE_FILES;
	if (vcp->obj.co_flags & SMBV_UNICODE)
		caps |= SMB_CAP_UNICODE;
#endif
#ifdef XXX
	caps |= SMB_CAP_NT_SMBS;
#endif
	if (vcp->vc_intok) {
		if (vcp->vc_intoklen > 65536 ||
		    !(vcp->vc_hflags2 & SMB_FLAGS2_EXT_SEC) ||
		    SMB_DIALECT(vcp) < SMB_DIALECT_NTLM0_12)
			return EINVAL;
		vcp->vc_smbuid = 0;
	}
	if (vcp->vc_hflags2 & SMB_FLAGS2_ERR_STATUS)
		caps |= SMB_CAP_STATUS32;
again:

	if (!vcp->vc_intok)
		vcp->vc_smbuid = SMB_UID_UNKNOWN;

	if (smb_smb_nomux(vcp, scred, __FUNCTION__) != 0)
		return EINVAL;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX,
			     scred, &rqp);
	if (error)
		return error;
	if (vcp->vc_intok) {
		caps |= SMB_CAP_EXT_SECURITY;
	} else if (!(vcp->vc_sopt.sv_sm & SMB_SM_USER)) {
		/*
		 * In the share security mode password will be used
		 * only in the tree authentication
		 */
		 pp = "";
		 plen = 1;
		 unipp = &smb_unieol;
		 uniplen = sizeof(smb_unieol);
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
				      smb_vc_getpass(vcp), SMB_MAXPASSWORDLEN);
		} else {
			strncpy(pbuf, smb_vc_getpass(vcp), SMB_MAXPASSWORDLEN);
			pbuf[SMB_MAXPASSWORDLEN] = '\0';
		}
#ifdef APPLE
		if (!SMB_UNICODE_STRINGS(vcp))
#endif /* APPLE */
			iconv_convstr(vcp->vc_toserver, pbuf, pbuf,
				      SMB_MAXPASSWORDLEN);
		if (vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT) {
			uniplen = plen = 24;
			smb_encrypt(pbuf, vcp->vc_ch, encpass);
			ntencpass = malloc(uniplen, M_SMBTEMP, M_WAITOK);
#ifdef APPLE
			if (SMB_UNICODE_STRINGS(vcp)) {
				strncpy(pbuf, smb_vc_getpass(vcp),
					SMB_MAXPASSWORDLEN);
				pbuf[SMB_MAXPASSWORDLEN] = '\0';
			} else
#endif /* APPLE */
				iconv_convstr(vcp->vc_toserver, pbuf,
					      smb_vc_getpass(vcp),
					      SMB_MAXPASSWORDLEN);
			smb_ntencrypt(pbuf, vcp->vc_ch, (u_char*)ntencpass);
			pp = encpass;
			unipp = ntencpass;
		} else {
			plen = strlen(pbuf) + 1;
			pp = pbuf;
			uniplen = plen * 2;
			ntencpass = malloc(uniplen, M_SMBTEMP, M_WAITOK);
			smb_strtouni(ntencpass, smb_vc_getpass(vcp));
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
		} else {
			mb_put_uint16le(mbp, plen);
			mb_put_uint16le(mbp, uniplen);
			mb_put_uint32le(mbp, 0);		/* reserved */
			mb_put_uint32le(mbp, caps);		/* my caps */
			smb_rq_wend(rqp);
			smb_rq_bstart(rqp);
			mb_put_mem(mbp, pp, plen, MB_MSYSTEM); /* password */
			mb_put_mem(mbp, (caddr_t)unipp, uniplen, MB_MSYSTEM);
			smb_put_dstring(mbp, vcp, up, SMB_CS_NONE); /* user */
			smb_put_dstring(mbp, vcp, vcp->vc_domain, SMB_CS_NONE);
		}
#ifdef APPLE
		smb_put_dstring(mbp, vcp, "MacOSX", SMB_CS_NONE);
#else
		smb_put_dstring(mbp, vcp, "FreeBSD", SMB_CS_NONE);
#endif /* APPLE */
		smb_put_dstring(mbp, vcp, "NETSMB", SMB_CS_NONE); /* LAN Mgr */
	}
	smb_rq_bend(rqp);
	if (ntencpass)
		free(ntencpass, M_SMBTEMP);
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
		md_get_uint16le(mdp, NULL);	/* action */
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
	if (encpass)
		free(encpass, M_SMBTEMP);
	if (pbuf)
		free(pbuf, M_SMBTEMP);
	smb_rq_done(rqp);
	if (error && upper == 1 && vcp->vc_sopt.sv_sm & SMB_SM_USER)
		goto again;
	return error;
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
#ifdef APPLE
	int upper = 0;

 again:
        vcp = SSTOVC(ssp);
        
        /* Disable Unicode for SMB_COM_TREE_CONNECT_ANDX requests */
	if (vcp->vc_hflags2 & SMB_FLAGS2_UNICODE) {
                vcp->vc_hflags2 &= ~SMB_FLAGS2_UNICODE;
	} 
	
        /* 
         * Disable the converters during the request. With Unicode off, strings from 
         * Windows servers will likely be encoded with their local system encoding.
         * If we try to convert them and fail then the treeconnect will fail. In this function,
         * we can just use the strings as they're given to us. The converters are enabled
         * on exit.
         */
	if (vcp->vc_toserver) {
		iconv_close(vcp->vc_toserver);
		/* Use NULL until UTF-8 -> ASCII works */
                vcp->vc_toserver = NULL;
        }
        if (vcp->vc_tolocal) {
                iconv_close(vcp->vc_tolocal);
                /* Use NULL until ASCII -> UTF-8 works*/
                vcp->vc_tolocal = NULL;
        }
#endif
	
	ssp->ss_tid = SMB_TID_UNKNOWN;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_TREE_CONNECT_ANDX, scred, &rqp);
	if (error)
		return error;
	caseopt = SMB_CS_NONE;
	if (vcp->vc_sopt.sv_sm & SMB_SM_USER) {
		plen = 1;
		pp = "";
		pbuf = NULL;
		encpass = NULL;
	} else {
		pbuf = malloc(SMB_MAXPASSWORDLEN + 1, M_SMBTEMP, M_WAITOK);
		encpass = malloc(24, M_SMBTEMP, M_WAITOK);
#ifdef APPLE
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
#else
		iconv_convstr(vcp->vc_toupper, pbuf, smb_share_getpass(ssp),
			      SMB_MAXPASSWORDLEN);
#endif /* APPLE */
		iconv_convstr(vcp->vc_toserver, pbuf, pbuf, SMB_MAXPASSWORDLEN);
		if (vcp->vc_sopt.sv_sm & SMB_SM_ENCRYPT) {
			plen = 24;
			smb_encrypt(pbuf, vcp->vc_ch, encpass);
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
	smb_put_dmem(mbp, vcp, "\\\\", 2, caseopt);
	pp = vcp->vc_srvname;
	error = smb_put_dmem(mbp, vcp, pp, strlen(pp), caseopt);
	if (error) {
		SMBERROR("error %d from smb_put_dmem for srvname\n", error);
		goto bad;
	}
	smb_put_dmem(mbp, vcp, "\\", 1, caseopt);
	pp = ssp->ss_name;
	error = smb_put_dstring(mbp, vcp, pp, caseopt);
	if (error) {
		SMBERROR("error %d from smb_put_dstring for ss_name\n", error);
		goto bad;
	}
	pp = smb_share_typename(ssp->ss_type);
	smb_put_dstring(mbp, vcp, pp, caseopt);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	if (error)
		goto bad;
	ssp->ss_tid = rqp->sr_rptid;
	ssp->ss_vcgenid = vcp->vc_genid;
	ssp->ss_flags |= SMBS_CONNECTED;
#ifdef APPLE
	/*
	 * If the server can speak Unicode then switch
	 * our converters to do Unicode <--> UTF-8
	 */
	if (vcp->obj.co_flags & SMBV_UNICODE) {
		void *servercharset = NULL;
		void *localcharset = NULL;

		if (iconv_open("ucs-2", "utf-8", &servercharset) != 0)
			goto bad;
		if (iconv_open("utf-8", "ucs-2", &localcharset) != 0) {
			iconv_close(servercharset);
			goto bad;
		}
		if (vcp->vc_toserver)
			iconv_close(vcp->vc_toserver);
		if (vcp->vc_tolocal)
			iconv_close(vcp->vc_tolocal);
		vcp->vc_toserver = servercharset;
		vcp->vc_tolocal  = localcharset;
		vcp->vc_hflags2 |= SMB_FLAGS2_UNICODE;
	} else if (vcp->vc_toserver == NULL) {
		void *servercharset = NULL;
		void *localcharset = NULL;
		
                /* todo: if we can't determine the server's encoding, we need to try a best-guess here */
		if (iconv_open("cp437", "utf-8", &servercharset) != 0)
			goto bad;
		if (iconv_open("utf-8", "cp437", &localcharset) != 0) {
			iconv_close(servercharset);
			goto bad;
		}
		if (vcp->vc_toserver)
			iconv_close(vcp->vc_toserver);
		if (vcp->vc_tolocal)
			iconv_close(vcp->vc_tolocal);		
		vcp->vc_toserver = servercharset;
		vcp->vc_tolocal  = localcharset;
	}
#endif /* APPLE */
bad:
	if (encpass)
		free(encpass, M_SMBTEMP);
	if (pbuf)
		free(pbuf, M_SMBTEMP);
	smb_rq_done(rqp);
#ifdef APPLE
	if (error && upper == 1)
		goto again;
#endif /* APPLE */
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
	      struct uio *uio, struct smb_cred *scred)
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
	mb_put_uint32le(mbp, (u_int32_t)uio->uio_offset);
	*len = min(SSTOVC(ssp)->vc_rxmax, *len);
	mb_put_uint16le(mbp, (u_int16_t)*len);	/* MaxCount */
	mb_put_uint16le(mbp, (u_int16_t)*len);	/* MinCount (only indicates blocking) */
	mb_put_uint32le(mbp, (unsigned)*len >> 16);	/* MaxCountHigh */
	mb_put_uint16le(mbp, (u_int16_t)*len);	/* Remaining ("obsolete") */
	mb_put_uint32le(mbp, (u_int32_t)((u_int64_t)uio->uio_offset >> 32));
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
	struct uio *uio, struct smb_cred *scred, int timo)
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
	mb_put_uint32le(mbp, (u_int32_t)uio->uio_offset);
	mb_put_uint32le(mbp, 0);	/* MBZ (timeout) */
	mb_put_uint16le(mbp, 0);	/* !write-thru */
	mb_put_uint16le(mbp, 0);
	*len = min(SSTOVC(ssp)->vc_wxmax, *len);
	mb_put_uint16le(mbp, (u_int16_t)((unsigned)*len >> 16));
	mb_put_uint16le(mbp, (u_int16_t)*len);
	mb_put_uint16le(mbp, 64);	/* data offset from header start */
	mb_put_uint32le(mbp, (u_int32_t)((u_int64_t)uio->uio_offset >> 32));
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
	int *len, int *rresid, struct uio *uio, struct smb_cred *scred)
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
	mb_put_uint32le(mbp, (u_int32_t)uio->uio_offset);
	mb_put_uint16le(mbp, (u_int16_t)min(uio->uio_resid, 0xffff));
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
smb_read(struct smb_share *ssp, u_int16_t fid, struct uio *uio,
	struct smb_cred *scred)
{
	int tsize, len, resid;
	int error = 0;

	tsize = uio->uio_resid;
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
	struct uio *uio, struct smb_cred *scred, int timo)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t resid;
	u_int8_t wc;
	int error;

	if (uio->uio_offset + *len > UINT32_MAX &&
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
	mb_put_uint32le(mbp, (u_int32_t)uio->uio_offset);
	mb_put_uint16le(mbp, (u_int16_t)min(uio->uio_resid, 0xffff));
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
smb_write(struct smb_share *ssp, u_int16_t fid, struct uio *uio,
	struct smb_cred *scred, int timo)
{
	int error = 0, len, tsize, resid;
	struct uio olduio;

	tsize = uio->uio_resid;
	olduio = *uio;
	while (tsize > 0) {
		len = tsize;
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
		*uio = olduio;
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
	smbfs_fullpath(mbp, SSTOVC(ssp), dnp, name, nmlen);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	smb_rq_done(rqp);
	return error;
}
