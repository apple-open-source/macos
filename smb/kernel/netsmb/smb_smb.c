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
#include <netsmb/smb_converter.h>

struct smb_dialect {
	int				d_id;
	const char *	d_name;
};

/*
 * We no long support older  dialects, but leaving this
 * information here for prosperity.
 * 
 * SMB dialects that we have to deal with.
 *
 * The following are known servers that do not support NT LM 0.12 dialect:
 * Windows for Workgroups and OS/2
 *
 * The following are known servers that do support NT LM 0.12 dialect:
 * Windows 95, Windows 98, Windows NT (include 3.51), Windows 2000, Windows XP, 
 * Windows 2003, NetApp, EMC, Snap,  and SAMBA.
 */
 
enum smb_dialects { 
	SMB_DIALECT_NONE,
	SMB_DIALECT_CORE,			/* PC NETWORK PROGRAM 1.0, PCLAN1.0 */
	SMB_DIALECT_COREPLUS,		/* MICROSOFT NETWORKS 1.03 */
	SMB_DIALECT_LANMAN1_0,		/* MICROSOFT NETWORKS 3.0, LANMAN1.0 */
	SMB_DIALECT_LANMAN2_0,		/* LM1.2X002, DOS LM1.2X002, Samba */
	SMB_DIALECT_LANMAN2_1,		/* DOS LANMAN2.1, LANMAN2.1 */
	SMB_DIALECT_NTLM0_12		/* NT LM 0.12 */
};

/* 
 * MAX_DIALECT_STRING should alway be the largest dialect string length
 * that we support. Currently we only support "NT LM 0.12" so 12 should
 * be fine.
 */
#define MAX_DIALECT_STRING		12

static struct smb_dialect smb_dialects[] = {
/*
 * The following are no longer supported by this
 * client, but have been left here for historical 
 * reasons.
 *
	{SMB_DIALECT_CORE,	"PC NETWORK PROGRAM 1.0"},
	{SMB_DIALECT_COREPLUS,	"MICROSOFT NETWORKS 1.03"},
	{SMB_DIALECT_LANMAN1_0,	"MICROSOFT NETWORKS 3.0"},
	{SMB_DIALECT_LANMAN1_0,	"LANMAN1.0"},
	{SMB_DIALECT_LANMAN2_0,	"LM1.2X002"},
	{SMB_DIALECT_LANMAN2_1,	"LANMAN2.1"},
	{SMB_DIALECT_NTLM0_12,	"NT LANMAN 1.0"},
 */
	{SMB_DIALECT_NTLM0_12,	"NT LM 0.12"},
	{-1,			NULL}
};

	
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
 * Figure out the largest write we can do to the server.
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
smb_smb_nomux(struct smb_vc *vcp, vfs_context_t context, const char *name)
{
	if (context == vcp->vc_iod->iod_context)
		return 0;
	SMBERROR("wrong function called(%s)\n", name);
	return EINVAL;
}

int smb_smb_negotiate(struct smb_vc *vcp, vfs_context_t context, 
		vfs_context_t user_context, int inReconnect)
{
	struct smb_dialect *dp;
	struct smb_sopt *sp = NULL;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc = 0, stime[8], sblen;
	u_int16_t dindex, bc;
	int error;
	u_int32_t maxqsz;
	u_int16_t toklen;
	u_char security_mode;
	u_int32_t	original_caps;

	if (smb_smb_nomux(vcp, context, __FUNCTION__) != 0)
		return EINVAL;
	vcp->vc_hflags = SMB_FLAGS_CASELESS;
	/* Leave SMB_FLAGS2_UNICODE "off" - no need to do anything */ 
	vcp->vc_hflags2 |= SMB_FLAGS2_ERR_STATUS;
	sp = &vcp->vc_sopt;
	original_caps = sp->sv_caps;
	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_NEGOTIATE, context, &rqp);
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
	 *
	 * Seems some servers return a negative one.
	 */
	if (dindex != 0) {
		/* In our case should always be zero! */
		SMBERROR("Bad dialect (%d, %d)\n", dindex, wc);
		error = ENOTSUP;
		goto bad;			
	}

	md_get_uint8(mdp, &security_mode);	/* Ge the server security modes */
	vcp->vc_flags |= (security_mode & SMBV_SECURITY_MODE_MASK);
	md_get_uint16le(mdp, &sp->sv_maxmux);
	md_get_uint16le(mdp, &sp->sv_maxvcs);
	md_get_uint32le(mdp, &sp->sv_maxtx);
	/* Was sv_maxraw, we never do raw reads or writes so just ignore */
	md_get_uint32le(mdp, NULL);
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
	 * If they say they are UNIX then they can't be a NT4 server
	 */
	if (((sp->sv_caps & SMB_CAP_UNIX) != SMB_CAP_UNIX) &&
		(sp->sv_caps & SMB_CAP_NT_SMBS) && 
		((sp->sv_caps & SMB_CAP_LARGE_RDWRX) == SMB_CAP_LARGE_READX))
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

	/* If the server doesn't do extended security then turn off the SMB_FLAGS2_EXT_SEC flag. */
	if ((sp->sv_caps & SMB_CAP_EXT_SECURITY) != SMB_CAP_EXT_SECURITY)
		vcp->vc_hflags2 &= ~SMB_FLAGS2_EXT_SEC;

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
	/* The server does extend security, find out what mech type they support. */
	if (vcp->vc_hflags2 & SMB_FLAGS2_EXT_SEC) {
		void *outtok = (toklen) ? malloc(toklen, M_SMBTEMP, M_WAITOK) : NULL;
		
		if (outtok) {
			error = md_get_mem(mdp, outtok, toklen, MB_MSYSTEM);
			/* If we get an error pretend we have no blob and force NTLMSSP */
			if (error) {
				free(outtok, M_SMBTEMP);
				outtok = NULL;
			}
		}
		/* If no token then say we have no length */
		if (outtok == NULL)
			toklen = 0;
		error = smb_gss_negotiate(vcp, user_context, outtok, toklen);
		if (outtok)
			free(outtok, M_SMBTEMP);
		if (error)
			goto bad;
	}

	vcp->vc_maxvcs = sp->sv_maxvcs;
	if (vcp->vc_maxvcs == 0)
		vcp->vc_maxvcs = 1;

	if (sp->sv_maxtx <= 0)
		sp->sv_maxtx = 1024;

	sp->sv_maxtx = MIN(sp->sv_maxtx, 63*1024 + SMB_HDRLEN + 16);
	SMB_TRAN_GETPARAM(vcp, SMBTP_RCVSZ, &maxqsz);
	vcp->vc_rxmax = MIN(smb_vc_maxread(vcp), maxqsz - 1024);
	SMB_TRAN_GETPARAM(vcp, SMBTP_SNDSZ, &maxqsz);
	vcp->vc_wxmax = MIN(smb_vc_maxwrite(vcp), maxqsz - 1024);
	vcp->vc_txmax = MIN(sp->sv_maxtx, maxqsz);
	SMBSDEBUG("TZ = %d\n", sp->sv_tz);
	SMBSDEBUG("CAPS = %x\n", sp->sv_caps);
	SMBSDEBUG("MAXMUX = %d\n", sp->sv_maxmux);
	SMBSDEBUG("MAXVCS = %d\n", sp->sv_maxvcs);
	SMBSDEBUG("MAXTX = %d\n", sp->sv_maxtx);
	

	/* When doing a reconnect we never allow them to change the encode */
	if (inReconnect) {
		if (original_caps != sp->sv_caps)
			SMBWARNING("Reconnecting with different sv_caps %x != %x\n", original_caps, sp->sv_caps);
		if ((sp->sv_caps & SMB_CAP_UNICODE) != (original_caps & SMB_CAP_UNICODE)) {
			SMBERROR("Server changed encoding on us during reconnect: abort reconnect\n");
			error = ENOTSUP;
			goto bad;
		}
	}		
	if (sp->sv_caps & SMB_CAP_UNICODE)
		vcp->vc_hflags2 |= SMB_FLAGS2_UNICODE;
	else
		vcp->vc_hflags2 &= ~SMB_FLAGS2_UNICODE;
bad:
	smb_rq_done(rqp);
	return error;
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
 * usually returned and if it matches the win2k uc name then set the flag.
 */
static void
smb_check_for_win2k(struct smb_vc *vcp, void *osname, int namelen)
{
	static  uint8_t WIN2K_XP_UC_NAME[] = {
		'W', 0, 'i', 0, 'n', 0, 'd', 0, 'o', 0, 'w', 0, 's', 0,
		' ', 0, '5', 0, '.', 0
	};
#define WIN2K_UC_NAME_LEN sizeof(WIN2K_XP_UC_NAME) 
	
	vcp->vc_flags &= ~SMBV_WIN2K_XP;
	/*
	 * Now see if the OS name says they are Windows 2000. Windows 2000 has an OS
	 * name of "Windows 5.0" and XP has a OS name of "Windows 5.1".  Windows
	 * 2003 returns a totally different OS name.
	 */
	if ((namelen >= (int)WIN2K_UC_NAME_LEN) && (bcmp(WIN2K_XP_UC_NAME, osname, WIN2K_UC_NAME_LEN) == 0)) {
			vcp->vc_flags |= SMBV_WIN2K_XP; /* It's a Windows 2000 or XP server */
	}
	return;
}


/*
 * Retreive the OS and Lan Man Strings. The calling routines 
 */
void parse_server_os_lanman_strings(struct smb_vc *vcp, void *refptr, uint16_t bc)
{
	struct mdchain *mdp = (struct mdchain *)refptr;
	uint8_t *tmpbuf= NULL;
	size_t oslen = 0, lanmanlen = 0, lanmanoffset = 0, domainoffset = 0;
	int error;
	
	/*
	 * Make sure we  have a byte count and the byte cound needs to be less that the
	 * amount we negotiated. Also only get this info once, NTLMSSP will cause us to see
	 * this message twice we only need to get it once.
	 */
	if ((bc == 0) || (bc > vcp->vc_txmax) || vcp->NativeOS || vcp->NativeLANManager)
		goto done; 
	tmpbuf = malloc(bc, M_SMBTEMP, M_NOWAIT);
	if (!tmpbuf)
		goto done;
	
	error = md_get_mem(mdp, (void *)tmpbuf, bc, MB_MSYSTEM);
	if (error)
		goto done;
	
#ifdef SMB_DEBUG
	smb_hexdump(__FUNCTION__, "BLOB = ", tmpbuf, bc);
#endif // SMB_DEBUG
	
	/* 
	 * Since both Window 2000 and XP support UNICODE, only do this check if the 
	 * server is doing UNICODE
	 */
	if (vcp->vc_hflags2 & SMB_FLAGS2_UNICODE)
		smb_check_for_win2k(vcp, tmpbuf, bc);
	
	if (vcp->vc_hflags2 & SMB_FLAGS2_UNICODE) {
		/* Find the end of the OS String */
		lanmanoffset = oslen = smb_utf16_strnlen((const uint16_t *)tmpbuf, bc);
		if (lanmanoffset != bc)
			lanmanoffset += 2;	/* Add the null bytes */
		bc -= lanmanoffset;
		/* Find the end of the Lanman String */
		domainoffset = lanmanlen = smb_utf16_strnlen((const uint16_t *)&tmpbuf[lanmanoffset], bc);
		if (lanmanoffset != bc)
			domainoffset += 2;	/* Add the null bytes */
		bc -= domainoffset;
	} else {
		/* Find the end of the OS String */
		lanmanoffset = oslen = strnlen((const char *)tmpbuf, bc);
		if (lanmanoffset != bc)
			lanmanoffset += 1;	/* Add the null bytes */
		bc -= lanmanoffset;
		/* Find the end of the Lanman String */
		domainoffset = lanmanlen = strnlen((const char *)&tmpbuf[lanmanoffset], bc);
		if (lanmanoffset != bc)
			domainoffset += 1;	/* Add the null bytes */
		bc -= domainoffset;
	}
	
#ifdef SMB_DEBUG
	smb_hexdump(__FUNCTION__, "OS = ", tmpbuf, oslen);
	smb_hexdump(__FUNCTION__, "LANMAN = ", &tmpbuf[lanmanoffset], lanmanlen);
	smb_hexdump(__FUNCTION__, "DOMAIN = ", &tmpbuf[domainoffset+lanmanoffset], bc);
#endif // SMB_DEBUG
	
	if (oslen) {
		vcp->NativeOS = smbfs_ntwrkname_tolocal(vcp, (const char *)tmpbuf, &oslen);
		if (vcp->NativeOS)
			vcp->NativeOS[oslen] = 0;
	}
	if (lanmanlen) {
		vcp->NativeLANManager= smbfs_ntwrkname_tolocal(vcp, (const char *)&tmpbuf[lanmanoffset], &lanmanlen);
		if (vcp->NativeLANManager)
			vcp->NativeLANManager[lanmanlen] = 0;
	}
	
	SMBDEBUG("NativeOS = %s NativeLANManager = %s\n", 
			 (vcp->NativeOS) ? vcp->NativeOS : "NULL",
			 (vcp->NativeLANManager) ? vcp->NativeLANManager : "NULL");
	
done:
	if (tmpbuf)
		free(tmpbuf, M_SMBTEMP);
}

/*
 * If the server supports extended security then we let smb_gss_ssnsetup handle
 * that work. For the older systems that don't support extended security, either
 * samba in share level, or some system pre-Windows2000.
 * 
 * So we support the following:
 *
 *	NTLMv2
 *	NTLM with the ASCII password
 *
 * if the server supports encrypted passwords, or
 * plain-text with the ASCII password
 *
 */
int smb_smb_ssnsetup(struct smb_vc *vcp, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	char *tmpbuf = NULL;	/* An allocated buffer that needs to be freed */
	void *unipp = NULL;		/* An allocated buffer that holds the information used in the case sesitive password field */
	const char *pp = NULL;		/* Holds the information used in the case insesitive password field */
	size_t plen = 0, uniplen = 0;
	int error = 0;
	u_int32_t caps;
	u_int16_t action;
	u_int16_t bc;  

	if (smb_smb_nomux(vcp, context, __FUNCTION__) != 0) {
		error = EINVAL;
		goto ssn_exit;
	}

	if (vcp->vc_sopt.sv_caps & SMB_CAP_EXT_SECURITY) {		
		error = smb_gss_ssnsetup(vcp, context);
		goto ssn_exit;		
	}

	caps = smb_vc_caps(vcp);
	
	vcp->vc_smbuid = SMB_UID_UNKNOWN;

	/* If we're going to try NTLM, fail if the minimum authentication level is not NTLMv2. */
	if ((vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) && (vcp->vc_flags & SMBV_MINAUTH_NTLMV2)) {
		SMBERROR("NTLMv2 security required!\n");
		error = EAUTH;
		goto ssn_exit;
	}

	/*
	 * Domain name must be upper-case, as that's what's used
	 * when computing LMv2 and NTLMv2 responses - and, for NTLMv2,
	 * the domain name in the request has to be upper-cased as well.
	 * (That appears not to be the case for the user name.  Go
	 * figure.)
	 *
	 * don't need to uppercase domain. It's already uppercase UTF-8.
	 *
	 * NOTE: We use to copy the vc_domain into an allocated buffer and then
	 * copy it into the mbuf, not sure why. Now we just copy vc_domain straight
	 * in to the mbuf.
	 */

	if (!(vcp->vc_flags & SMBV_USER_SECURITY)) {
		/*
		 * In the share security mode password will be used
		 * only in the tree authentication
		 */
		pp = "";
		plen = 1;
		uniplen = 0;
		unipp = NULL;
		tmpbuf = NULL;	/* Nothing to free here */
	} else if ((vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) != SMBV_ENCRYPT_PASSWORD) {
		/* 
		 * Clear text passwords. The smb library already check the preferences to 
		 * make sure its enabled. 
		 *
		 * We no longer uppercase the clear text password. May cause problems for 
		 * older servers, but in the worst case the user can type it in uppercase.
		 *
		 * When doing clear text password the old code never sent the case insesitive 
		 * password, so neither will we.
		 *
		 * We need to turn off UNICODE for clear text passwords.
		 */
		vcp->vc_hflags2 &= ~SMB_FLAGS2_UNICODE;

		pp = smb_vc_getpass(vcp);
		plen = strnlen(pp, SMB_MAXPASSWORDLEN + 1);
		uniplen = 0;
		unipp = NULL;
		tmpbuf = NULL;	/* Nothing to free here */
	} else {
		plen = 24;
		tmpbuf = malloc(plen, M_SMBTEMP, M_WAITOK);
		if (vcp->vc_flags & SMBV_MINAUTH_NTLM) {
			/* Don't put the LM response on the wire - it's too easy to crack. */
			bzero(tmpbuf, plen);
		} else {
			/*
			 * Compute the LM response, derived from the challenge and the ASCII
			 * password.
			 */
			smb_lmresponse((u_char *)smb_vc_getpass(vcp), vcp->vc_ch, (u_char *)tmpbuf);
		}
		pp = tmpbuf;	/* Need to free this when we are done */

		/*
		 * Compute the NTLM response, derived from
		 * the challenge and the password.
		 */
		uniplen = 24;
		unipp = malloc(uniplen, M_SMBTEMP, M_WAITOK);
		if (unipp) {
			smb_ntlmresponse((u_char *)smb_vc_getpass(vcp), vcp->vc_ch, (u_char*)unipp);
			smb_calcmackey(vcp, unipp, uniplen);			
		}
	}
	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX, context, &rqp);
	if (error)
		goto ssn_exit;
	
	smb_rq_wstart(rqp);
	mbp = &rqp->sr_rq;
	/*
	 * We now have a flag telling us to attempt an anonymous connection. All 
	 * this means is  have no user name, password or domain.
	 */
	if (vcp->vc_flags & SMBV_ANONYMOUS_ACCESS) /*  anon */
		plen = uniplen = 0;

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
	smb_put_dstring(mbp, vcp, vcp->vc_username, SMB_MAXUSERNAMELEN + 1, NO_SFM_CONVERSIONS); /* user */
	smb_put_dstring(mbp, vcp, vcp->vc_domain, SMB_MAXNetBIOSNAMELEN + 1, NO_SFM_CONVERSIONS); /* domain */

	smb_put_dstring(mbp, vcp, SMBFS_NATIVEOS, sizeof(SMBFS_NATIVEOS), NO_SFM_CONVERSIONS);	/* Native OS */
	smb_put_dstring(mbp, vcp, SMBFS_LANMAN, sizeof(SMBFS_LANMAN), NO_SFM_CONVERSIONS);	/* LAN Mgr */

	smb_rq_bend(rqp);
	error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);
	if (error) {
		SMBDEBUG("error = %d, rpflags2 = 0x%x, sr_error = 0x%x\n", error, 
				 rqp->sr_rpflags2, rqp->sr_error);
	}

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
		md_get_uint16le(mdp, &bc); /* remaining bytes */
		/* server OS, LANMGR, & Domain here */

		/* 
		 * If we are doing UNICODE then byte count is always on an odd boundry
		 * so we need to always deal with the padd byte.
		 */
		if (bc > 0) {
			md_get_uint8(mdp, NULL);	/* Skip Padd Byte */
			bc -= 1;
		}
		/*
		 * Now see if we can get the NativeOS and NativeLANManager strings. We 
		 * use these strings to tell if the server is a Win2k or XP system, 
		 * also Shared Computers wants this info.
		 */
		parse_server_os_lanman_strings(vcp, mdp, bc);
		error = 0;
	} while (0);
bad:
	/*
	 * We are in user level security, got log in as guest, but we are not using guest access. We need to log off
	 * and return an error.
	 */
	if ((error == 0) && (vcp->vc_flags & SMBV_USER_SECURITY) && 
		((vcp->vc_flags & SMBV_GUEST_ACCESS) != SMBV_GUEST_ACCESS) && (action & SMB_ACT_GUEST)) {
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
	
	smb_rq_done(rqp);

ssn_exit:
	if (unipp) {
		free(unipp, M_SMBTEMP);
		unipp = NULL;		
	}
	if (tmpbuf) {
		free(tmpbuf, M_SMBTEMP);
		tmpbuf = NULL;
	}
	
	if (((vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) != SMBV_ENCRYPT_PASSWORD) &&
		(UNIX_CAPS(vcp) & SMB_CAP_UNICODE)) {
		/* We turned off UNICODE for Clear Text Password turn it back on */
		vcp->vc_hflags2 |= SMB_FLAGS2_UNICODE;
	}
	if (error)	/* Reset the signature info */
		smb_reset_sig(vcp);

	if (error && (error != EAUTH))
		SMBWARNING("SetupAndX failed error = %d\n", error);
	return (error);
}

int
smb_smb_ssnclose(struct smb_vc *vcp, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	if (vcp->vc_smbuid == SMB_UID_UNKNOWN)
		return 0;

	if (smb_smb_nomux(vcp, context, __FUNCTION__) != 0)
		return EINVAL;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_LOGOFF_ANDX, context, &rqp);
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

#define SMB_ANY_SHARE_NAME		"?????"
#define SMB_DISK_SHARE_NAME		"A:"
#define SMB_PRINTER_SHARE_NAME	SMB_ANY_SHARE_NAME
#define SMB_PIPE_SHARE_NAME		"IPC"
#define SMB_COMM_SHARE_NAME		"COMM"

static const char * smb_share_typename(int stype, size_t *sharenamelen)
{
	const char *pp;

	switch (stype) {
		case SMB_ST_DISK:
			pp = SMB_DISK_SHARE_NAME;
			*sharenamelen = sizeof(SMB_DISK_SHARE_NAME);	
			break;
	    case SMB_ST_PRINTER:
			pp = SMB_PRINTER_SHARE_NAME;	/* can't use LPT: here... */
			*sharenamelen = sizeof(SMB_PRINTER_SHARE_NAME);	
			break;
	    case SMB_ST_PIPE:
			pp = SMB_PIPE_SHARE_NAME;
			*sharenamelen = sizeof(SMB_PIPE_SHARE_NAME);	
			break;
	    case SMB_ST_COMM:
			pp = SMB_COMM_SHARE_NAME;
			*sharenamelen = sizeof(SMB_COMM_SHARE_NAME);	
			break;
	    case SMB_ST_ANY:
	    default:
			pp = SMB_ANY_SHARE_NAME;
			*sharenamelen = sizeof(SMB_ANY_SHARE_NAME);	
			break;
	}
	return pp;
}

int
smb_smb_treeconnect(struct smb_share *ssp, vfs_context_t context)
{
	struct smb_vc *vcp;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	const char *pp;
	char *pbuf, *encpass;
	int error;
	u_int16_t plen;
	size_t sharenamelen = 0;
	struct sockaddr_in *sockaddr_ptr;

	vcp = SSTOVC(ssp);
	sockaddr_ptr = (struct sockaddr_in*)(&vcp->vc_saddr->sa_data[2]);
 
	ssp->ss_tid = SMB_TID_UNKNOWN;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_TREE_CONNECT_ANDX, context, &rqp);
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
		strlcpy(pbuf, smb_share_getpass(ssp), SMB_MAXPASSWORDLEN+1);
		pbuf[SMB_MAXPASSWORDLEN] = '\0';

		if (vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) {
			plen = 24;
			smb_lmresponse((u_char *)pbuf, vcp->vc_ch, (u_char *)encpass);
			pp = encpass;
		} else {
			plen = (u_int16_t)strnlen(pbuf, SMB_MAXPASSWORDLEN + 1) + 1;
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
	 * User land code now passes down the server name in the proper format that 
	 * can be used in a tree connection. This is two complicated of an issue to
	 * be handle in the kernel, but we do know the following:
	 *
	 * The server's NetBIOS name will always work, but we can't always get it because
	 * of firewalls. Window cluster system require the name to be a NetBIOS
	 * name.
	 *
	 * Windows XP will not allow DNS names to be used and in fact requires a
	 * name that must fit in a NetBIOS name. So if we don't have the NetBIOS
	 * name we can send the IPv4 address in presentation form (xxx.xxx.xxx.xxx).
	 *
	 * If we are doing IPv6 then it looks like we can skip sending the server 
	 * name and only send to share name. Really need to test this theory out, but
	 * since we don't support IPv6 currently this isn't an issue. Note we must 
	 * send the server name when doing IPv4.
	 *
	 * Now what should we do about Bonjour names? We could send the IPv4 address 
	 * in presentation form, we could send the Bonjour name, or could we just
	 * skip the name completely? For now we send the IPv4 address in presentation
	 * form just to be safe.
	 */
	if (sockaddr_ptr->sin_family == AF_INET) {
		size_t srvnamelen = strnlen(vcp->vc_srvname, SMB_MAX_DNS_SRVNAMELEN+1); 
		error = smb_put_dmem(mbp, vcp, vcp->vc_srvname, srvnamelen, NO_SFM_CONVERSIONS, NULL);
		if (error) {
			SMBERROR("error %d from smb_put_dmem for srvname\n", error);
			goto bad;
		}		
		smb_put_dmem(mbp, vcp, "\\", 1, NO_SFM_CONVERSIONS, NULL);
	} else {
		/* XXX When we add IPv6 support we will need to test this out */
		smb_put_dmem(mbp, vcp, "\\", 1, NO_SFM_CONVERSIONS, NULL);
	}	
	error = smb_put_dstring(mbp, vcp, ssp->ss_name, SMB_MAXSHARENAMELEN+1, NO_SFM_CONVERSIONS);
	if (error) {
		SMBERROR("error %d from smb_put_dstring for ss_name\n", error);
		goto bad;
	}
	/* The type name is always ASCII */
	pp = smb_share_typename(ssp->ss_type, &sharenamelen);
	/* See smb_share_typename to find out why this strlen is ok */
	error = mb_put_mem(mbp, pp, sharenamelen, MB_MSYSTEM);
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
	lck_mtx_lock(&ssp->ss_stlock);
	ssp->ss_flags |= SMBS_CONNECTED;
	lck_mtx_unlock(&ssp->ss_stlock);
bad:
	if (encpass)
		free(encpass, M_SMBTEMP);
	if (pbuf)
		free(pbuf, M_SMBTEMP);
	smb_rq_done(rqp);
treeconnect_exit:
	return error;
}

int smb_smb_treedisconnect(struct smb_share *ssp, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	if (ssp->ss_tid == SMB_TID_UNKNOWN)
		return 0;
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_TREE_DISCONNECT, context, &rqp);
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
	user_ssize_t *rresid, uio_t uio, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	int error;
	u_int16_t residhi, residlo, off, doff;
	u_int32_t resid;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_READ_ANDX, context, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* no secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to secondary */
	mb_put_mem(mbp, (caddr_t)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint32le(mbp, (u_int32_t)uio_offset(uio));
	*len = MIN(SSTOVC(ssp)->vc_rxmax, *len);

	mb_put_uint16le(mbp, (u_int16_t)*len);	/* MaxCount */
	mb_put_uint16le(mbp, (u_int16_t)*len);	/* MinCount (only indicates blocking) */
	mb_put_uint32le(mbp, (u_int32_t)((user_size_t)*len >> 16));	/* MaxCountHigh */
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
	user_ssize_t *rresid, uio_t uio, vfs_context_t context, int timo)
{
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	u_int8_t wc;
	u_int16_t resid;

	/* vc_wxmax now holds the max buffer size the server supports for writes */
	*len = MIN(*len, vcp->vc_wxmax);

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_WRITE_ANDX, context, &rqp);
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
	mb_put_uint16le(mbp, (u_int16_t)((user_size_t)*len >> 16));
	mb_put_uint16le(mbp, (u_int16_t)*len);
	mb_put_uint16le(mbp, 64);	/* data offset from header start */
	mb_put_uint32le(mbp, (u_int32_t)(uio_offset(uio) >> 32));
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	do {
		mb_put_uint8(mbp, 0xee);	/* mimic xp pad byte! */
		error = mb_put_uio(mbp, uio, (int)*len);
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
	user_ssize_t *rresid, uio_t uio, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t resid, bc;
	u_int8_t wc;
	int error;
	u_int16_t rlen;

	if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES ||
	    SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_READX)
		return (smb_smb_readx(ssp, fid, len, rresid, uio, context));

	/* Someday we need to drop this call, since even Win98 supports SMB_CAP_LARGE_READX */
	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_READ, context, &rqp);
	if (error)
		return error;

	*len = MIN(SSTOVC(ssp)->vc_rxmax, *len);
	rlen = (u_int16_t)*len;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint16le(mbp, (u_int16_t)rlen);
	mb_put_uint32le(mbp, (u_int32_t)uio_offset(uio));
	mb_put_uint16le(mbp, (u_int16_t)MIN(uio_resid(uio), 0xffff));
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

int smb_read(struct smb_share *ssp, u_int16_t fid, uio_t uio, 
			 vfs_context_t context)
{
	user_ssize_t tsize, len, resid = 0;
	int error = 0;

	tsize = uio_resid(uio);
	while (tsize > 0) {
		len = tsize;
		error = smb_smb_read(ssp, fid, &len, &resid, uio, context);
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
	user_ssize_t *rresid, uio_t uio, vfs_context_t context, int timo)
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
		return (smb_smb_writex(ssp, fid, len, rresid, uio, context, timo));

	if ((uio_offset(uio) + *len) > UINT32_MAX)
		return (EFBIG);

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_WRITE, context, &rqp);
	if (error)
		return error;

	/* vc_wxmax now holds the max buffer size the server supports for writes */
	resid = MIN(*len, SSTOVC(ssp)->vc_wxmax);
	*len = resid;

	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, sizeof(fid), MB_MSYSTEM);
	mb_put_uint16le(mbp, resid);
	mb_put_uint32le(mbp, (u_int32_t)uio_offset(uio));
	mb_put_uint16le(mbp, (u_int16_t)MIN(uio_resid(uio), 0xffff));
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
		  vfs_context_t context, int timo)
{
	int error = 0;
	user_ssize_t  old_resid, len, tsize, resid = 0;
	off_t  old_offset;

	tsize = old_resid = uio_resid(uio);
	old_offset = uio_offset(uio);

	while (tsize > 0) {
		len = tsize;
		error = smb_smb_write(ssp, fid, &len, &resid, uio, context, timo);
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
smb_smb_echo(struct smb_vc *vcp, vfs_context_t context, int timo)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_ECHO, context, &rqp);
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

int smb_smb_checkdir(struct smb_share *ssp, struct smbnode *dnp, const char *name,  
					 size_t nmlen, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(ssp), SMB_COM_CHECK_DIRECTORY, context, &rqp);
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
