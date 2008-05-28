/*
 * Copyright (c) 2000-2001 Boris Popov
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

#include <sys/kpi_mbuf.h>
#include <sys/smb_apple.h>
#include <sys/utfconv.h>

#include <sys/smb_iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_gss.h>
#include <fs/smbfs/smbfs_subr.h>
#include <netinet/in.h>
#include <sys/kauth.h>
#include <fs/smbfs/smbfs.h>

struct smb_dialect {
	int		d_id;
	const char *	d_name;
};

#ifdef OLD_DIALECT_SUPPORT
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
#else // OLD_DIALECT_SUPPORT

static struct smb_dialect smb_dialects[] = {
	{SMB_DIALECT_NTLM0_12,	"NT LM 0.12"},
	{-1,			NULL}
};

#endif // OLD_DIALECT_SUPPORT		
	
/* 
 * Really could be 128K - (SMBHDR + SMBREADANDX RESPONSE HDR), but 126K works better with the Finder.
 *
 * Note old Samba's are busted, they set the SMB_CAP_LARGE_READX and SMB_CAP_LARGE_WRITEX, but can't handle anything
 * larger that 64K-1. This was fixed in Samba 3.0.23 and greater, but since Tiger is running 3.0.10 we need to work
 * around this problem yuk! Samba has a way to set the transfer buffer size and our Leopard Samba was this set to 
 * a value larger than 60K. So here is the kludge that I would like to have removed in the future. If they support
 * the SMB_CAP_LARGE_READX and SMB_CAP_LARGE_WRITEX, they say they are UNIX and they have a transfer buffer size 
 * greater than 60K then use the 126K buffer size.
 */
#define MAX_LARGEX_READ_CAP_SIZE	126*1024
#define MAX_LARGEX_WRITE_CAP_SIZE	126*1024
#define WINDOWS_LARGEX_READ_CAP_SIZE	60*1024
#define WINDOWS_LARGEX_WRITE_CAP_SIZE	60*1024

static u_int32_t smb_vc_maxread(const struct smb_vc *vcp)
{
	/*
	 * SNIA Specs say up to 64k data bytes, but it is wrong. Windows traffic
	 * uses 60k... no doubt for some good reason.
	 *
	 * The NetBIOS Header supports up 128K for the whole message. Some Samba servers
	 * can only handle reads of 64K minus 1. We want the read and writes to be mutilples of 
	 * each other. Remember we want IO request to not only be a multiple of our 
	 * max buffer size but they must land on a PAGE_SIZE boundry. See smbfs_vfs_getattr
	 * more on this issue.
	 */
	if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_READX) {
		if (UNIX_SERVER(vcp) && (vcp->vc_sopt.sv_maxtx > WINDOWS_LARGEX_READ_CAP_SIZE))
			return (MAX_LARGEX_READ_CAP_SIZE); 	
		else 
			return (WINDOWS_LARGEX_READ_CAP_SIZE);
	}
	else if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES)
		return (vcp->vc_sopt.sv_maxtx - SMB_HDRLEN - SMB_READANDX_HDRLEN);
	else
		return (vcp->vc_sopt.sv_maxtx - SMB_HDRLEN - SMB_READ_COM_HDRLEN);
}

/*
 * 
 Figure out the largest write we can do to the server.
 */
static u_int32_t smb_vc_maxwrite(const struct smb_vc *vcp)
{
	/*
	 * SNIA Specs say up to 64k data bytes, but it is wrong. Windows traffic
	 * uses 60k... no doubt for some good reason.
	 *
	 * The NetBIOS Header supports up 128K for the whole message. Samba will 
	 * handle up to 127K for writes. We want the read and writes to be mutilples of 
	 * each other. Remember we want IO request to not only be a multiple of our 
	 * max buffer size but they must land on a PAGE_SIZE boundry. See smbfs_vfs_getattr
	 * more on this issue.
	 *
	 *
	 * When doing packet signing Windows server will break the connection if you
	 * use large writes. If we are going against SAMBA this should not be an issue.
	 *
	 * NOTE: Windows XP/2000/2003 support 126K writes, but not reads so for now we use 60K buffers
	 *		 in both cases.
	 */
	if ((vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) && (! UNIX_SERVER(vcp)))
		return (vcp->vc_sopt.sv_maxtx - SMB_HDRLEN - SMB_WRITEANDX_HDRLEN);
	else  if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_WRITEX) {
		if (UNIX_SERVER(vcp) && (vcp->vc_sopt.sv_maxtx > WINDOWS_LARGEX_WRITE_CAP_SIZE))
			return (MAX_LARGEX_WRITE_CAP_SIZE);
		else 
			return (WINDOWS_LARGEX_WRITE_CAP_SIZE);
	}
	else if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES)
		return (vcp->vc_sopt.sv_maxtx - SMB_HDRLEN - SMB_WRITEANDX_HDRLEN);
	else
		return (vcp->vc_sopt.sv_maxtx - SMB_HDRLEN - SMB_WRITE_COM_HDRLEN);
		
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
smb_smb_negotiate(struct smb_vc *vcp, struct smb_cred *scred, struct smb_cred *user_scred, int inReconnect)
{
	struct smb_dialect *dp;
	struct smb_sopt *sp = NULL;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc = 0, stime[8], sblen;
	u_int16_t dindex, bc;
	int error, maxqsz;
	char *servercs;
	u_int16_t toklen;
	u_char		security_mode;
	u_int32_t	original_caps;

	if (smb_smb_nomux(vcp, scred, __FUNCTION__) != 0)
		return EINVAL;
	vcp->vc_hflags = SMB_FLAGS_CASELESS;
	/* Leave SMB_FLAGS2_UNICODE "off" - no need to do anything */ 
	vcp->vc_hflags2 |= SMB_FLAGS2_ERR_STATUS;
	sp = &vcp->vc_sopt;
	original_caps = sp->sv_caps;
	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_NEGOTIATE, scred, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	/*
	 * Currently we only support one dialect; leave this in just in case we 
	 * decide to add the SMB2 dialect. The dialects are never in UNICODE, so
	 * just put the strings in by hand. 
	 */
	for(dp = smb_dialects; dp->d_id != -1; dp++) {
		mb_put_uint8(mbp, SMB_DT_DIALECT);
		mb_put_mem(mbp, dp->d_name, strlen(dp->d_name), MB_MSYSTEM);
		mb_put_uint8(mbp, 0);
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	if (error)
		goto bad;
	smb_rq_getreply(rqp, &mdp);

	error = md_get_uint8(mdp, &wc);
	/* 
	 * If they didn't return an error and word count is wrong then error out.
	 * We always expect a work count of 17 now that we only support NTLM 012 dialect.
	 */
	 if ((error == 0) && (wc != 17)) {
		 error = EBADRPC;
		 goto bad;
	 }
	
	if (error == 0)
		error = md_get_uint16le(mdp, &dindex);
	if (error)
		goto bad;
	/*
	 * The old code support more than one dialect. Since everything equal to or newer than
	 * Windows 95 supports the NTLM 012 dialect we only request that dialect. So
	 * if the server responded without an error then they must support our dialect.
	 */
	dp = smb_dialects + dindex;
	sp->sv_proto = dp->d_id;
	if (dindex)	/* In our case should always be zero now */
		SMBWARNING("Dialect %s (%d, %d)\n", dp->d_name, dindex, wc);

	md_get_uint8(mdp, &security_mode);	/* Ge the server security modes */
	vcp->vc_flags |= (security_mode & SMBV_SECURITY_MODE_MASK);
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
		goto bad;
	if (vcp->vc_flags & SMBV_SIGNING_REQUIRED)
		vcp->vc_hflags2 |= SMB_FLAGS2_SECURITY_SIGNATURE;

	/*
	 * Is this a NT4 server, there is a very simple way to tell if it is a NT4 system. NT4 
	 * support SMB_CAP_LARGE_READX, but not SMB_CAP_LARGE_WRITEX. I suppose some other system could do 
	 * the same, but that shouldn't hurt since this is a very limited case we are checking.
	 */
	if ((sp->sv_caps & SMB_CAP_NT_SMBS) && ((sp->sv_caps & SMB_CAP_LARGE_RDWRX) == SMB_CAP_LARGE_READX))
		vcp->vc_flags |= SMBV_NT4;
	
	/*
	 * They don't do NT error codes.
	 *
	 * If we send requests with SMB_FLAGS2_ERR_STATUS set in Flags2, Windows 98, at least, appears to send 
	 * replies with that bit set even though it sends back  DOS error codes. (They probably just use the 
	 * request header as a template for the reply header, and don't bother clearing that bit.) Therefore, we 
	 * clear that bit in our vc_hflags2 field.
	 */
	if ((sp->sv_caps & SMB_CAP_STATUS32) != SMB_CAP_STATUS32)
		vcp->vc_hflags2 &= ~SMB_FLAGS2_ERR_STATUS;

	/*
	 * If the server doesn't do extended security then we need to turn off the SMB_FLAGS2_EXT_SEC flag. This 
	 * will allow us to continue using this VC without the server breaking the connection. Keeps us from doing 
	 * another round of connections when connecting to a server that doesn't support extended security.
	 */
	if ((sp->sv_caps & SMB_CAP_EXT_SECURITY) != SMB_CAP_EXT_SECURITY) {
		vcp->vc_flags &= ~SMBV_EXT_SEC;
		vcp->vc_hflags2 &= ~SMB_FLAGS2_EXT_SEC;
	}

	/* Windows 95/98/Me Server, could be some other server, but safer treating it like Windows 98 */
	if ((sp->sv_maxtx < 4096) && ((sp->sv_caps & SMB_CAP_NT_SMBS) == 0))
		vcp->vc_flags |= SMBV_WIN98;

	/*
	 * 3 cases here:
	 *
	 * 1) Extended security. Read bc bytes below for security blob.
	 *
	 * 2) No extended security, have challenge data and possibly a domain name (which might be zero
	 * bytes long, meaning "missing"). Copy challenge stuff to vcp->vc_ch (sblen bytes),
	 *
	 * 3) No extended security, no challenge data, just possibly a domain name.
	 */

	/*
	 * Sanity check: make sure the challenge length
	 * isn't bigger than the byte count.
	 */
	if (sblen > bc) {
		error = EBADRPC;
		goto bad;
	}
	toklen = bc;

	if (sblen && (sblen <= SMB_MAXCHALLENGELEN) && (vcp->vc_flags & SMBV_ENCRYPT_PASSWORD)) {
		error = md_get_mem(mdp, (caddr_t)(vcp->vc_ch), sblen, MB_MSYSTEM);
		if (error)
			goto bad;
		vcp->vc_chlen = sblen;
		toklen -= sblen; 
	}
	/* The server does extend security, we are trying to do extend security, see if they do kerberos */
	if ((toklen > 0) && ((vcp->vc_flags & SMBV_EXT_SEC) == SMBV_EXT_SEC)) {
		void *outtok = malloc(toklen, M_SMBTEMP, M_WAITOK);
		
		if (outtok) {
			error = md_get_mem(mdp, outtok, toklen, MB_MSYSTEM);
			if ((!error) && (smb_gss_negotiate(vcp, user_scred, outtok)))
				vcp->vc_flags |= SMBV_KERBEROS_SUPPORT;
			free(outtok, M_SMBTEMP);
		}
	}

	vcp->vc_maxvcs = sp->sv_maxvcs;
	if (vcp->vc_maxvcs == 0)
		vcp->vc_maxvcs = 1;

	if (sp->sv_maxtx <= 0)
		sp->sv_maxtx = 1024;

	sp->sv_maxtx = min(sp->sv_maxtx, 63*1024 + SMB_HDRLEN + 16);
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
	

	/* When doing a reconnect we never allow them to change the encode */
	if (inReconnect) {
		DBG_ASSERT(vcp->vc_toserver);
		DBG_ASSERT(vcp->vc_tolocal);
		if (original_caps != sp->sv_caps)
			SMBWARNING("Reconnecting with different sv_caps %x != %x\n", original_caps, sp->sv_caps);
		if ((sp->sv_caps & SMB_CAP_UNICODE) != (original_caps & SMB_CAP_UNICODE)) {
			SMBERROR("Server changed ecoding on us durring reconnect: abort reconnect\n");
			error = ENOTSUP;
			goto bad;
		}
	} else {
		DBG_ASSERT(vcp->vc_toserver == NULL);
		DBG_ASSERT(vcp->vc_tolocal == NULL);
		/* 
		 * If the server supports Unicode, set up to use Unicode when talking 
		 * to them.  Othewise, use code page 437. 
		 */
		if (sp->sv_caps & SMB_CAP_UNICODE)
			servercs = "ucs-2";
		else 
			servercs = "cp437";
		
		error = iconv_open(servercs, "utf-8", &vcp->vc_toserver);
		if (error != 0)
			goto bad;
		error = iconv_open("utf-8", servercs, &vcp->vc_tolocal);
		if (error != 0) {
			iconv_close(vcp->vc_toserver);
			vcp->vc_toserver = NULL;
			goto bad;
		}
	}
		
	if (sp->sv_caps & SMB_CAP_UNICODE)
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
			      SMB_MAXPASSWORDLEN, NO_SFM_CONVERSIONS);
	} else {
		strlcpy(pbuf, smb_vc_getpass(vcp), SMB_MAXPASSWORDLEN+1);
		pbuf[SMB_MAXPASSWORDLEN] = '\0';
	}
}

static void
get_unicode_password(struct smb_vc *vcp, char *pbuf)
{
	strlcpy(pbuf, smb_vc_getpass(vcp), SMB_MAXPASSWORDLEN+1);
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
			iconv_convstr(vcp->vc_toupper, namebuf, (char *)name,
				      namelen, NO_SFM_CONVERSIONS);
			uninamelen = smb_strtouni(uninamebuf, namebuf, namelen,
			    UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
			free(namebuf, M_SMBTEMP);
		} else {
			uninamelen = smb_strtouni(uninamebuf, (char *)name,
						  namelen,
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
make_ntlmv2_blob(struct smb_vc *vcp, char *dom, u_int64_t client_nonce, size_t *bloblen)
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
	domainlen = strlen(dom);
	srvlen = strlen(vcp->vc_srvname);
	blobsize = sizeof(struct ntlmv2_blobhdr)
	    + 3*sizeof (struct ntlmv2_namehdr) + 4 + 2*domainlen + 2*srvlen;
	blob = malloc(blobsize, M_SMBTEMP, M_WAITOK);
	bzero(blob, blobsize);
	blobhdr = (struct ntlmv2_blobhdr *)blob;
	blobhdr->header = htolel(0x00000101);
	nanotime(&now);
	/*
	 * %%%
	 * I would prefer not to change this yet. Once I am done with reconnects
	 * and the new auth methods, We should relook at this again.
	 * 
	 * Really should not force this to be on a two second interval.
	 */
	smb_time_local2NT(&now, 0, &timestamp, 1);
	blobhdr->timestamp = htoleq(timestamp);
	blobhdr->client_nonce = client_nonce;
	blobnames = blob + sizeof (struct ntlmv2_blobhdr);
	blobnames = add_name_to_blob(blobnames, vcp, (u_char *)dom, domainlen,
				     NAMETYPE_DOMAIN_NB, 1);
//	blobnames = add_name_to_blob(blobnames, vcp, (u_char *)vcp->vc_srvname,
//				     srvlen, NAMETYPE_MACHINE_NB, 1);
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
	iconv_convstr(vcp->vc_toupper, ucstrbuf, string, stringlen, NO_SFM_CONVERSIONS);
	return (ucstrbuf);
}

/*
 * smb_vc_caps:
 *
 * Given a virtual circut, determine our capabilities to send to the server
 * as part of "ssandx" message.
 */
uint32_t smb_vc_caps(struct smb_vc *vcp)
{
	uint32_t caps =  SMB_CAP_LARGE_FILES;
	
	if (vcp->vc_sopt.sv_caps & SMB_CAP_UNICODE)
		caps |= SMB_CAP_UNICODE;
	
	if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)
		caps |= SMB_CAP_NT_SMBS;
	
	if (vcp->vc_hflags2 & SMB_FLAGS2_ERR_STATUS)
		caps |= SMB_CAP_STATUS32;
	
	/* If they support it then we support it. */
	if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_READX)
		caps |= SMB_CAP_LARGE_READX;
	
	if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_WRITEX)
		caps |= SMB_CAP_LARGE_WRITEX;
	
	if (vcp->vc_sopt.sv_caps & SMB_CAP_UNIX)
		caps |= SMB_CAP_UNIX;
	
	return (caps);
	
}

/*
 * smb_check_for_win2k:
 *
 * We need to determine whether the server is win2k or not. This is used in
 * deciding whether we need to zero fill "gaps" in the file so that subsequent
 * reads will return zeros. Later than win2k can be made to handle that.
 * At the end of session setup and X messages the os name from the server is
 * usually returned and if it matches the win2k uc name we will return
 * true, else false.
 */
int
smb_check_for_win2k(void *refptr, int namelen)
{
	static  uint8_t WIN2K_UC_NAME[] = {
		'W', 0, 'i', 0, 'n', 0, 'd', 0, 'o', 0, 'w', 0, 's', 0,
		' ', 0, '5', 0, '.', 0, '0', 0
	};
#define WIN2K_UC_NAME_LEN sizeof(WIN2K_UC_NAME) 
	uint8_t osname[WIN2K_UC_NAME_LEN];
	int error;
	struct mdchain *mdp = (struct mdchain *)refptr;
	
	if (namelen <= 0)
		return (0);
	
	md_get_uint8(mdp, NULL);	/* Skip Padd Byte */
	/*
	 * Now see if the OS name says they are Windows 2000. Windows 2000 has an OS
	 * name of "Windows 5.0" and XP has a OS name of "Windows 5.1".  Windows
	 * 2003 returns a totally different OS name.
	 */
	if (namelen >= WIN2K_UC_NAME_LEN) {
		error = md_get_mem(mdp, (void *)osname, WIN2K_UC_NAME_LEN, MB_MSYSTEM);
		if ((error == 0) &&
			(bcmp(WIN2K_UC_NAME, osname, WIN2K_UC_NAME_LEN) == 0))
		return (1); /* It's a Windows 2000 server */
	}
	
	return (0);
}

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
#define STATE_UCPW		2
#define STATE_DONE		3

int
smb_smb_ssnsetup(struct smb_vc *vcp, struct smb_cred *scred)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	smb_uniptr unipp = NULL, ntencpass = NULL;
	char *pp = NULL, *up, *ucdp;
	char *pbuf = NULL;
	char *encpass = NULL;
	int error = 0, ulen;
	size_t plen = 0, uniplen = 0;
	int state;
	size_t v2_bloblen;
	u_char *v2_blob, *ucup;
	u_int64_t client_nonce;
	u_int32_t caps;
	u_int16_t bl; /* BLOB length */
	u_int16_t stringlen;
	u_int16_t action;
	u_int16_t osnamelen;  

	/* In the future smb_gss_ssnsetup will need to make sure we are doing the min auth (kerberos, NTLMv2, etc) */
	if (SMB_USE_GSS(vcp))
		return (smb_gss_ssnsetup(vcp, scred));
	
	/* Kerberos is required, if we got here we are not doing Kerberos */
	if (vcp->vc_flags & SMBV_MINAUTH_KERBEROS) {
		SMBERROR("Kerberos security is required!\n");
		return EAUTH;
	}
	/* Server requires clear text password, but we are not doing clear text passwords. */
	if (((vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) != SMBV_ENCRYPT_PASSWORD) && (vcp->vc_flags & SMBV_MINAUTH)) {
		SMBERROR("Clear text passwords are not allowed!\n");
		return EAUTH;
	}
	caps = smb_vc_caps(vcp);

	if (vcp->vc_flags & SMBV_ENCRYPT_PASSWORD)
		state = STATE_NTLMV2;	/* try NTLMv2 first */
	else 
		state = STATE_NOUCPW;	/* try plain-text mixed-case first */			

again:

	vcp->vc_smbuid = SMB_UID_UNKNOWN;

	if (smb_smb_nomux(vcp, scred, __FUNCTION__) != 0) {
		error = EINVAL;
		goto ssn_exit;
	}
	/* If we're going to try NTLM, fail if the minimum authentication level is not NTLMv2. */
	if ((vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) && (state != STATE_NTLMV2) && (vcp->vc_flags & SMBV_MINAUTH_NTLMV2)) {
		SMBERROR("NTLMv2 security required!\n");
		error = EAUTH;
		goto ssn_exit;
	}

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX, scred, &rqp);
	if (error)
		goto ssn_exit;
	/*
	 * Domain name must be upper-case, as that's what's used
	 * when computing LMv2 and NTLMv2 responses - and, for NTLMv2,
	 * the domain name in the request has to be upper-cased as well.
	 * (That appears not to be the case for the user name.  Go
	 * figure.)
	 *
	 * don't need to uppercase domain. It's already uppercase UTF-8.
	 */

	stringlen = strlen(vcp->vc_domain)+1 /* strlen doesn't count null */;
	ucdp = malloc(stringlen, M_SMBTEMP, M_WAITOK);
	memcpy(ucdp, vcp->vc_domain, stringlen);

	if (!(vcp->vc_flags & SMBV_USER_SECURITY)) {
		/*
		 * In the share security mode password will be used
		 * only in the tree authentication
		 */
		 pp = "";
		 plen = 1;
	} else {
		pbuf = malloc(SMB_MAXPASSWORDLEN + 1, M_SMBTEMP, M_WAITOK);
		if (vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) {
			if (state == STATE_NTLMV2) {
				u_char v2hash[16];

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
				read_random((void *)&client_nonce, sizeof(client_nonce));
				/*
				 * For anonymous login with packet signing we
				 * need a null domain as well as a null user
				 * and password.
				 */
				if ((vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) && (vcp->vc_username[0] == '\0'))
					*ucdp = '\0';
				/*
				 * Convert the user name to upper-case, as
				 * that's what's used when computing LMv2
				 * and NTLMv2 responses.
				 */
				ucup = (u_char *)uppercasify_string(vcp, vcp->vc_username);

				smb_ntlmv2hash((u_char *)pbuf, ucup, (u_char *)ucdp, &v2hash[0]);
				free(ucup, M_SMBTEMP);
				/*
				 * Compute the LMv2 response, derived
				 * from the v2hash, the server challenge,
				 * and the client nonce.
				 */
				smb_ntlmv2response(v2hash, vcp->vc_ch, (u_char *)&client_nonce, 8, (u_char**)&encpass, &plen);
				pp = encpass;

				/*
				 * Construct the blob.
				 */
				v2_blob = make_ntlmv2_blob(vcp, ucdp, client_nonce, &v2_bloblen);
				/*
				 * Compute the NTLMv2 response, derived
				 * from the server challenge, the
				 * user name, the domain/workgroup
				 * into which we're logging, the
				 * blob, and the Unicode password.
				 */
				smb_ntlmv2response(v2hash, vcp->vc_ch, v2_blob, v2_bloblen, (u_char**)&ntencpass, &uniplen);
				free(v2_blob, M_SMBTEMP);
				unipp = ntencpass;

				/*
				 * If required, make a packet-signing / MAC key.
				 */
				smb_calcv2mackey(vcp, v2hash, (u_char *)ntencpass, uniplen);
			} else {
				plen = 24;
				encpass = malloc(plen, M_SMBTEMP, M_WAITOK);
				if (vcp->vc_flags & SMBV_MINAUTH_NTLM) {
					/* Don't put the LM response on the wire - it's too easy to crack. */
					bzero(encpass, plen);
					/*
					 * In this case we no longer do a LM response so there is no reason to try the
					 * uppercase passwords. The old code could fail three times and lock people out of 
					 * their account. Now we never try more than twice.
					 */
					state = STATE_DONE;
				} else {
					/*
					 * Compute the LM response, derived from the challenge and the ASCII
					 * password.
					 *
					 * We try w/o uppercasing first so Samba mixed case passwords work.
					 * If that fails, we come back and try uppercasing to satisfy OS/2 and Windows for Workgroups.
					 */
					get_ascii_password(vcp, (state == STATE_UCPW), pbuf);
					smb_lmresponse((u_char *)pbuf, vcp->vc_ch, (u_char *)encpass);
					/*
					 * We no longer try uppercase passwords any more. The above comment said we only did this for
					 * OS/2 and Windows for Workgroups. We no longer support those systems. The old code could fail
					 * three times and lock people out of their account. Now we never try more than twice. In the 
					 * future we should clean this code up, but thats for another day when I have more time to test.
					 */
					state = STATE_DONE;
				}
				pp = encpass;

				/*
				 * Compute the NTLM response, derived from
				 * the challenge and the Unicode password.
				 */
				get_unicode_password(vcp, pbuf);
				uniplen = 24;
				ntencpass = malloc(uniplen, M_SMBTEMP, M_WAITOK);
				smb_ntlmresponse((u_char *)pbuf, vcp->vc_ch, (u_char*)ntencpass);
				unipp = ntencpass;
			}
		} else {
			/*
			 * We try w/o uppercasing first so Samba mixed case passwords work.  If that fails, we come back and
			 * try uppercasing to satisfy OS/2 and Windows for Workgroups.
			 */
			get_ascii_password(vcp, (state == STATE_UCPW), pbuf);
			plen = strlen(pbuf) + 1;
			pp = pbuf;
			uniplen = plen * 2;
			ntencpass = malloc(uniplen, M_SMBTEMP, M_WAITOK);
			(void)smb_strtouni(ntencpass, smb_vc_getpass(vcp), 0,  UTF_PRECOMPOSED);
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

	mb_put_uint16le(mbp, plen);
	mb_put_uint16le(mbp, uniplen);
	mb_put_uint32le(mbp, 0);		/* reserved */
	mb_put_uint32le(mbp, caps);		/* my caps */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_mem(mbp, pp, plen, MB_MSYSTEM); /* password */
	if (uniplen)
		mb_put_mem(mbp, (caddr_t)unipp, uniplen, MB_MSYSTEM);
	smb_put_dstring(mbp, vcp, up, NO_SFM_CONVERSIONS); /* user */
	smb_put_dstring(mbp, vcp, ucdp, NO_SFM_CONVERSIONS); /* domain */

	smb_put_dstring(mbp, vcp, SMBFS_NATIVEOS, NO_SFM_CONVERSIONS);	/* Native OS */
	smb_put_dstring(mbp, vcp, SMBFS_LANMAN, NO_SFM_CONVERSIONS);	/* LAN Mgr */

	smb_rq_bend(rqp);
	if (ntencpass) {
		free(ntencpass, M_SMBTEMP);
		ntencpass = NULL;
	}
	free(ucdp, M_SMBTEMP);
	/*
	 * If not kerberos/extendedsecurity and not NTLMv2 we create the
	 * packet-signing / MAC key here.
	 */
	if (state != STATE_NTLMV2)
		smb_calcmackey(vcp);
	error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);
#ifdef SSNDEBUG
	if (error)
		SMBDEBUG("error = %d, rpflags2 = 0x%x, sr_error = 0x%x\n", error, rqp->sr_rpflags2, rqp->sr_error);
#endif // SSNDEBUG

	if (error) {
		if (rqp->sr_errclass == ERRDOS && rqp->sr_serror == ERRnoaccess)
			error = EAUTH;
		if (!(rqp->sr_errclass == ERRDOS && rqp->sr_serror == ERRmoredata))
			goto bad;
	}
	vcp->vc_smbuid = rqp->sr_rpuid;
	smb_rq_getreply(rqp, &mdp);
	do {
		error = md_get_uint8(mdp, &wc);
		if (error)
			break;
		error = EBADRPC;
		if (wc != 3)
			break;
		md_get_uint8(mdp, NULL);	/* secondary cmd */
		md_get_uint8(mdp, NULL);	/* mbz */
		md_get_uint16le(mdp, NULL);	/* andxoffset */
		md_get_uint16le(mdp, &action);	/* action */
		bl = 0;
		md_get_uint16le(mdp, &osnamelen); /* remaining bytes */
		/* server OS, LANMGR, & Domain here */
		/*
		 * If no os name then skip nothing we can do about this case. If we already figured out this
		 * server is a Windows 2000 then no reason to check again. Now see if the OS name says they
		 * are Windows 2000. Windows 2000 has a OS name of "Windows 5.0" and XP has a OS name of
		 * "Windows 5.1".  Windows 2003 returns a totally different OS name.
		 */
		/* If we need to, check if this is a win2k server */
		if ((vcp->vc_flags & SMBV_WIN2K) == 0)
			vcp->vc_flags |= (smb_check_for_win2k(mdp, osnamelen) ? SMBV_WIN2K : 0);
		
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
	/*
	 * We are in user level security, got log in as guest, but we are not using guest access. We need to log off
	 * and return an error.
	 */
	if ((error == 0) && (vcp->vc_flags & SMBV_USER_SECURITY) && 
		((vcp->vc_flags & SMBV_GUEST_ACCESS) != SMBV_GUEST_ACCESS) && (action & SMB_ACT_GUEST)) {
		/*
		 * See radar 4114174.  This kludge helps us avoid a bug with a snap server which would
		 * grant limited Guest access when we try NTLMv2, but works fine with NTLM. The fingerprint 
		 * we are looking for here is DOS error codes and no-Unicode.
		 * Note XP grants Guest access but uses Unicode and NT error codes.
		 */
	    	if ((state == STATE_NTLMV2) &&
			((vcp->vc_sopt.sv_caps & SMB_CAP_UNICODE) != SMB_CAP_UNICODE) && 
			((vcp->vc_sopt.sv_caps & SMB_CAP_STATUS32) != SMB_CAP_STATUS32)) {
			state++;
			smb_rq_done(rqp);
			goto again;			
		}
		/* 
		 * Wanted to only login the users as guest if they ask to be login ask guest. Window system will
		 * login any bad user name as guest if guest is turn on. The problem here is with XPHome. XPHome
		 * doesn't care if the user is real or made up, it always logs them is as guest.
		 */
		SMBWARNING("Got guess access, but wanted real access.\n");
#ifdef GUEST_ACCESS_LOG_OFF
		(void)smb_smb_ssnclose(vcp, scred);
		error = EAUTH;
#endif // GUEST_ACCESS_LOG_OFF
		vcp->vc_flags |= SMBV_GUEST_ACCESS;
	}
	else if (error && (vcp->vc_flags & SMBV_USER_SECURITY) && (state < STATE_UCPW)) {
		/* Trying the next type of authentication */
		SMBDEBUG("Trying the next type of authentication state = %d\n", state);
		state++;
		smb_rq_done(rqp);
		goto again;
	}
	
	smb_rq_done(rqp);

ssn_exit:
	if (error)
		SMBWARNING("SetupAndX failed error = %d\n", error);
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
	int error, plen, srvnamelen;
	int upper = 0;
	char serverstring[SMB_MAXNetBIOSNAMELEN+1];	/* Inlude the null byte */
	struct sockaddr_in *sockaddr_ptr;

	vcp = SSTOVC(ssp);
      
 again:
	sockaddr_ptr = (struct sockaddr_in*)(&vcp->vc_paddr->sa_data[2]);
 
	ssp->ss_tid = SMB_TID_UNKNOWN;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_TREE_CONNECT_ANDX, scred, &rqp);
	if (error)
		goto treeconnect_exit;
	if (vcp->vc_flags & SMBV_USER_SECURITY) {
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
				      SMB_MAXPASSWORDLEN, NO_SFM_CONVERSIONS);
		} else {
			strlcpy(pbuf, smb_share_getpass(ssp), SMB_MAXPASSWORDLEN+1);
			pbuf[SMB_MAXPASSWORDLEN] = '\0';
		}
		if (vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) {
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
	smb_put_dmem(mbp, vcp, "\\\\", 2, NO_SFM_CONVERSIONS, NULL);
	/* 
	 * This is for XP-home, which has problems
	 * with dns names in the server field.
	 * Convert the 4-byte IP to ascii
	 * xxx.xxx.xxx.xxx
	 */
	if (betohs(sockaddr_ptr->sin_port) != NBSS_TCP_PORT_139) {
		pp = serverstring;
		/* 
		 * Note no endian-macros needed for &s_addr. It's a type void ptr in the call arg
		 *
		 * inet_ntop now expects the buffer to be big enough to hold the null byte. It will
		 * also put the null byte into our buffer. The old code had a big enough buffer,
		 * but sent the wrong size in. We now send SMB_MAXNetBIOSNAMELEN + 1, the one is for the
		 * null byte.
		 *
		 * Also not all system care if the server name is too big, so don't error out
		 * just because this fails, just log that we had a problem.
		 *
		 * One more thing, if we are doing AF_INET6 then we can skip sending the server
		 * name and only send to share name. 
		 */
		if (!inet_ntop(AF_INET,&(sockaddr_ptr->sin_addr.s_addr), pp, SMB_MAXNetBIOSNAMELEN+1)) {
			SMBERROR("inet_ntop() unsuccessful server name may be too long\n");
			pp = vcp->vc_srvname;
		}
	} else {
		pp = vcp->vc_srvname;
	}
	srvnamelen = strlen(pp); 
	error = smb_put_dmem(mbp, vcp, pp, srvnamelen, NO_SFM_CONVERSIONS, NULL);
	if (error) {
		SMBERROR("error %d from smb_put_dmem for srvname\n", error);
		goto bad;
	}
	
	smb_put_dmem(mbp, vcp, "\\", 1, NO_SFM_CONVERSIONS, NULL);
	pp = ssp->ss_name;
	error = smb_put_dstring(mbp, vcp, pp, NO_SFM_CONVERSIONS);
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
smb_smb_readx(struct smb_share *ssp, u_int16_t fid, user_ssize_t *len, 
	user_ssize_t *rresid, uio_t uio, struct smb_cred *scred)
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
smb_smb_writex(struct smb_share *ssp, u_int16_t fid, user_ssize_t *len, 
	user_ssize_t *rresid, uio_t uio, struct smb_cred *scred, int timo)
{
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	u_int8_t wc;
	u_int16_t resid;

	/* vc_wxmax now holds the max buffer size the server supports for writes */
	*len = min(*len, vcp->vc_wxmax);

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
smb_smb_read(struct smb_share *ssp, u_int16_t fid, user_ssize_t *len, 
	user_ssize_t *rresid, uio_t uio, struct smb_cred *scred)
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

	*len = rlen = min(SSTOVC(ssp)->vc_rxmax, *len);

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
	user_ssize_t tsize, len, resid = 0;
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
smb_smb_write(struct smb_share *ssp, u_int16_t fid, user_ssize_t *len, 
	user_ssize_t *rresid, uio_t uio, struct smb_cred *scred, int timo)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t resid;
	u_int8_t wc;
	int error;

	/*
	 * Remember that a zero length write will truncate the file if done with the old
	 * write call. So if we get a zero length write let it fall through to the old
	 * write call.   
	 */ 
	if ((*len) && (SSTOVC(ssp)->vc_sopt.sv_caps & (SMB_CAP_LARGE_FILES | SMB_CAP_LARGE_WRITEX)))
		return (smb_smb_writex(ssp, fid, len, rresid, uio, scred, timo));

	if ((uio_offset(uio) + *len) > UINT32_MAX)
		return (EFBIG);

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_WRITE, scred, &rqp);
	if (error)
		return error;

	/* vc_wxmax now holds the max buffer size the server supports for writes */
	resid = min(*len, SSTOVC(ssp)->vc_wxmax);
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
	int error = 0;
	user_ssize_t  old_resid, len, tsize, resid = 0;
	off_t  old_offset;

	tsize = old_resid = uio_resid(uio);
	old_offset = uio_offset(uio);

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
	smbfs_fullpath(mbp, SSTOVC(ssp), dnp, name, &nmlen, UTF_SFM_CONVERSIONS, '\\');
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	smb_rq_done(rqp);
	return error;
}
