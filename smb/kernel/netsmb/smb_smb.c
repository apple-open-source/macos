/*
 * Copyright (c) 2000-2001 Boris Popov
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

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_gss.h>
#include <smbfs/smbfs_subr.h>
#include <netinet/in.h>
#include <sys/kauth.h>
#include <smbfs/smbfs.h>
#include <netsmb/smb_converter.h>
#include <netsmb/smb_dev.h>
#include <smbclient/ntstatus.h>

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
	SMB_DIALECT_NTLM0_12,		/* NT LM 0.12 */
	SMB_DIALECT_SMB2_002,		/* SMB 2.002 */
	SMB_DIALECT_SMB2_000		/* SMB 2.??? */
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
	{SMB_DIALECT_SMB2_002,	"SMB 2.002"},
	{SMB_DIALECT_SMB2_000,	"SMB 2.???"},
    {-1,			NULL}
};

static struct smb_dialect smb1_dialects[] = {
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
#define MIN_MAXTRANSBUFFER				1024
#define SMB1_MAXTRANSBUFFER				0xffff
#define MAX_LARGEX_READ_CAP_SIZE		128*1024
#define MAX_LARGEX_WRITE_CAP_SIZE		128*1024
#define SNOW_LARGEX_READ_CAP_SIZE		126*1024
#define SNOW_LARGEX_WRITE_CAP_SIZE		126*1024
#define WINDOWS_LARGEX_READ_CAP_SIZE	60*1024
#define WINDOWS_LARGEX_WRITE_CAP_SIZE	60*1024

static 
uint32_t smb_vc_maxread(struct smb_vc *vcp)
{
	uint32_t socksize = vcp->vc_sopt.sv_maxtx;
	uint32_t maxmsgsize = vcp->vc_sopt.sv_maxtx;
	uint32_t hdrsize = SMB_HDRLEN;
	
	hdrsize += SMB_READANDX_HDRLEN; /* we only use ReadAndX */

	/* Make sure we never use a size bigger than the socket can support */
	SMB_TRAN_GETPARAM(vcp, SMBTP_RCVSZ, &socksize);
	maxmsgsize = MIN(maxmsgsize, socksize);
	maxmsgsize -= hdrsize;
	/*
	 * SNIA Specs say up to 64k data bytes, but it is wrong. Windows traffic
	 * uses 60k... no doubt for some good reason.
	 *
	 * The NetBIOS Header supports up 128K for the whole message. Some Samba servers
	 * can only handle reads of 64K minus 1. We want the read and writes to be mutilples of 
	 * each other. Remember we want IO request to not only be a multiple of our 
	 * max buffer size but they must land on a PAGE_SIZE boundry. See smbfs_vfs_getattr
	 * more on this issue.
	 *
	 * NOTE: For NetBIOS-less connections the NetBIOS Header supports 24 bits for
	 * the length field. 
	 */
	if (VC_CAPS(vcp) & SMB_CAP_LARGE_READX) {
		/* Leave the UNIX SERVER check for now, but in the futre we should drop it */
		if (UNIX_SERVER(vcp) && (vcp->vc_saddr->sa_family != AF_NETBIOS)  && 
			(maxmsgsize >= MAX_LARGEX_READ_CAP_SIZE)) {
			/*
			 * Once we do <rdar://problem/8753536> we should change the 
			 * maxmsgsize to be the following:
			 *		maxmsgsize = (maxmsgsize / PAGE_SIZE) * PAGE_SIZE;
			 * For now limit max size 128K.
			 */
			maxmsgsize = MAX_LARGEX_READ_CAP_SIZE;
		} else if (UNIX_SERVER(vcp) && (maxmsgsize > WINDOWS_LARGEX_READ_CAP_SIZE)) {
			maxmsgsize = SNOW_LARGEX_READ_CAP_SIZE; 	
		} else {
			socksize -= hdrsize;
			maxmsgsize = MIN(WINDOWS_LARGEX_READ_CAP_SIZE, socksize);			
		}
	}
	SMB_LOG_IO("%s max = %d sock = %d sv_maxtx = %d\n", vcp->vc_srvname, 
			   maxmsgsize, socksize, vcp->vc_sopt.sv_maxtx);
	return maxmsgsize;
}

/*
 * Figure out the largest write we can do to the server.
 */
static uint32_t 
smb_vc_maxwrite(struct smb_vc *vcp)
{
	uint32_t socksize = vcp->vc_sopt.sv_maxtx;
	uint32_t maxmsgsize = vcp->vc_sopt.sv_maxtx;
	uint32_t hdrsize = SMB_HDRLEN;
	
	hdrsize += SMB_WRITEANDX_HDRLEN;    /* we only use WriteAndX */
	
	/* Make sure we never use a size bigger than the socket can support */
	SMB_TRAN_GETPARAM(vcp, SMBTP_SNDSZ, &socksize);
	maxmsgsize = MIN(maxmsgsize, socksize);
	maxmsgsize -= hdrsize;
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
	 *
	 * NOTE: For NetBIOS-less connections the NetBIOS Header supports 24 bits for
	 * the length field. 
	 */
	if ((vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) && (! UNIX_SERVER(vcp))) {
		SMB_LOG_IO("%s %d - SIGNING ON\n", vcp->vc_srvname, maxmsgsize);
		return maxmsgsize;
	} else  if (VC_CAPS(vcp) & SMB_CAP_LARGE_WRITEX) {
		/* Leave the UNIX SERVER check for now, but in the futre we should drop it */
		if (UNIX_SERVER(vcp) && (vcp->vc_saddr->sa_family != AF_NETBIOS) && 
			(maxmsgsize >= MAX_LARGEX_WRITE_CAP_SIZE)) {
			/*
			 * Once we do <rdar://problem/8753536> we should change the 
			 * maxmsgsize to be the following:
			 *		maxmsgsize = (maxmsgsize / PAGE_SIZE) * PAGE_SIZE;
			 * For now limit max size 128K.
			 */
			maxmsgsize = MAX_LARGEX_WRITE_CAP_SIZE;
		} else if (UNIX_SERVER(vcp) && (maxmsgsize > WINDOWS_LARGEX_WRITE_CAP_SIZE)) {
			maxmsgsize = SNOW_LARGEX_WRITE_CAP_SIZE;
		} else {
			socksize -= hdrsize;
			maxmsgsize = MIN(WINDOWS_LARGEX_WRITE_CAP_SIZE, socksize);			
		}
	}
	SMB_LOG_IO("%s max = %d sock = %d sv_maxtx = %d\n", vcp->vc_srvname, 
			   maxmsgsize, socksize, vcp->vc_sopt.sv_maxtx);
	return maxmsgsize;
}

int
smb_smb_nomux(struct smb_vc *vcp, const char *name, vfs_context_t context)
{
	if (context == vcp->vc_iod->iod_context)
		return 0;
	SMBERROR("wrong function called(%s)\n", name);
	return EINVAL;
}

int 
smb1_smb_negotiate(struct smb_vc *vcp, vfs_context_t user_context,
                   int inReconnect, int onlySMB1, vfs_context_t context)
{
	struct smb_dialect *dp;
	struct smb_sopt *sp = NULL;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc = 0, stime[8], sblen;
	uint16_t dindex, bc;
	int error;
	uint32_t maxqsz;
	uint16_t toklen;
	u_char security_mode;
	uint32_t	original_caps;

	if (smb_smb_nomux(vcp, __FUNCTION__, context) != 0)
		return EINVAL;
	vcp->vc_hflags = SMB_FLAGS_CASELESS;
	/* Leave SMB_FLAGS2_UNICODE "off" - no need to do anything */ 
	vcp->vc_hflags2 |= SMB_FLAGS2_ERR_STATUS;
	sp = &vcp->vc_sopt;
	original_caps = sp->sv_caps;
	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_NEGOTIATE, 0, context, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
    
	/*
	 * The dialects are never in UNICODE, so just put the strings in by hand. 
	 */
    if (onlySMB1 == 1) {
        /* only advertise SMB 1 dialects */
        for(dp = smb1_dialects; dp->d_id != -1; dp++) {
            mb_put_uint8(mbp, SMB_DT_DIALECT);
            mb_put_mem(mbp, dp->d_name, strlen(dp->d_name), MB_MSYSTEM);
            mb_put_uint8(mbp, 0);
        }
    } 
    else {
        /* Advertise SMB 1 and SMB 2/3 dialects */
        for(dp = smb_dialects; dp->d_id != -1; dp++) {
            mb_put_uint8(mbp, SMB_DT_DIALECT);
            mb_put_mem(mbp, dp->d_name, strlen(dp->d_name), MB_MSYSTEM);
            mb_put_uint8(mbp, 0);
        }
    }
	smb_rq_bend(rqp);

	error = smb_rq_simple(rqp);
	if (error)
		goto bad;

    if (rqp->sr_extflags & SMB2_RESPONSE) {
        /* Got a SMB 2/3 response, do SMB 2/3 Negotiate */
        error = smb2_smb_negotiate(vcp, rqp, inReconnect, user_context, context);
        smb_rq_done(rqp);
        return (error);
    }

    /* SMB 1 does not support File IDs */
    vcp->vc_misc_flags &= ~SMBV_HAS_FILEIDS;

    /* Now get the response */
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
	if (sp->sv_maxmux == 0) {
		SMBERROR(" Maximum multiplexer is zero, not supported\n");
		error = ENOTSUP;
		goto bad;
	}

	md_get_uint16le(mdp, &sp->sv_maxvcs);
	md_get_uint32le(mdp, &sp->sv_maxtx);
	/* Was sv_maxraw, we never do raw reads or writes so just ignore */
	md_get_uint32le(mdp, NULL);
	md_get_uint32le(mdp, &sp->sv_skey);
	md_get_uint32le(mdp, &sp->sv_caps);
	md_get_mem(mdp, (caddr_t)stime, 8, MB_MSYSTEM);
	/* Servers time zone no longer needed */
	md_get_uint16le(mdp, NULL);
	md_get_uint8(mdp, &sblen);
	error = md_get_uint16le(mdp, &bc);
	if (error)
		goto bad;
	
	if (vcp->vc_misc_flags & SMBV_CLIENT_SIGNING_REQUIRED) {
		SMB_LOG_AUTH(" SMB Client requires Signing, server supports 0x%x\n", vcp->vc_flags);
        if (vcp->vc_flags & (SMBV_SIGNING_REQUIRED | SMBV_SIGNING)) {
            vcp->vc_flags |= SMBV_SIGNING_REQUIRED;
        } else {
            SMBERROR(" SMB Client requires Signing, server doesn't!\n");
            error = EAUTH;
            goto bad;
        }
    }
	if (vcp->vc_flags & SMBV_SIGNING_REQUIRED)
		vcp->vc_hflags2 |= SMB_FLAGS2_SECURITY_SIGNATURE;
 
    if (!(sp->sv_caps & SMB_CAP_NT_SMBS) || !(sp->sv_caps & SMB_CAP_UNICODE))  {
		SMBERROR("Support for the server %s has been deprecated (PreXP), disconnecting\n", vcp->vc_srvname);
		error = SMB_ENETFSNOPROTOVERSSUPP;
		goto bad;
	}
	/*
	 * Is this a NT4 server, there is a very simple way to tell if it is a NT4 system. NT4 
	 * support SMB_CAP_LARGE_READX, but not SMB_CAP_LARGE_WRITEX. I suppose some other system could do 
	 * the same, but that shouldn't hurt since this is a very limited case we are checking.
	 * If they say they are UNIX then they can't be a NT4 server
	 */
	if (((sp->sv_caps & SMB_CAP_UNIX) != SMB_CAP_UNIX) &&
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
        if (vcp->negotiate_token != NULL) {
            SMB_FREE(vcp->negotiate_token, M_SMBTEMP);
        }
		
		/* 
		 * We don't currently use the GUID, now if no guid isn't that a 
		 * protocol error. For now lets just ignore, but really think this
		 * should be an error
		 */
		(void)md_get_mem(mdp, NULL, SMB_GUIDLEN, MB_MSYSTEM);
		toklen = (toklen >= SMB_GUIDLEN) ? toklen - SMB_GUIDLEN : 0;
		
        if (toklen) {
            SMB_MALLOC(vcp->negotiate_token, uint8_t *, toklen, M_SMBTEMP, M_WAITOK);
        }
        else {
            vcp->negotiate_token = NULL;
        }
		if (vcp->negotiate_token) {
			vcp->negotiate_tokenlen = toklen;
			error = md_get_mem(mdp, (void *)vcp->negotiate_token, vcp->negotiate_tokenlen, MB_MSYSTEM);
			/* If we get an error pretend we have no blob and force NTLMSSP */
			if (error) {
                if (vcp->negotiate_token != NULL) {
                    SMB_FREE(vcp->negotiate_token, M_SMBTEMP);
                }
			}
		}
		/* If no token then say we have no length */
		if (vcp->negotiate_token == NULL) {
			vcp->negotiate_tokenlen = 0;
		}
		error = smb_gss_negotiate(vcp, user_context);
		if (error)
			goto bad;
	}

	if (sp->sv_maxtx < MIN_MAXTRANSBUFFER) {
		error = EBADRPC;
		goto bad;
	}

	vcp->vc_rxmax = smb_vc_maxread(vcp);
	vcp->vc_wxmax = smb_vc_maxwrite(vcp);
	/* Make sure the  socket buffer supports this size */
	SMB_TRAN_GETPARAM(vcp, SMBTP_RCVSZ, &maxqsz);
	vcp->vc_txmax = MIN(sp->sv_maxtx, maxqsz);
	SMB_TRAN_GETPARAM(vcp, SMBTP_SNDSZ, &maxqsz);
	vcp->vc_txmax = MIN(vcp->vc_txmax, maxqsz);
	/*
	 * SMB currently returns this buffer size in the SetupAndX message as a uint16_t
	 * value even though the server can pass us a uint32_t value. Make sure it
	 * fits in a uint16_t field.
	 */
	vcp->vc_txmax = MIN(vcp->vc_txmax, SMB1_MAXTRANSBUFFER);
	SMBSDEBUG("CAPS = %x\n", sp->sv_caps);
	SMBSDEBUG("MAXMUX = %d\n", sp->sv_maxmux);
	SMBSDEBUG("MAXVCS = %d\n", sp->sv_maxvcs);
	SMBSDEBUG("MAXTX = %d\n", sp->sv_maxtx);
	SMBSDEBUG("TXMAX = %d\n", vcp->vc_txmax);
	SMBSDEBUG("MAXWR = %d\n", vcp->vc_wxmax);
	SMBSDEBUG("MAXRD = %d\n", vcp->vc_rxmax);
	

	/* When doing a reconnect we never allow them to change the encode */
	if (inReconnect) {
		if (original_caps != sp->sv_caps)
			SMBWARNING("Reconnecting with different sv_caps %x != %x\n", original_caps, sp->sv_caps);
	}
    vcp->vc_hflags2 |= SMB_FLAGS2_UNICODE;

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
uint32_t 
smb_vc_caps(struct smb_vc *vcp)
{
	uint32_t caps =  SMB_CAP_LARGE_FILES | SMB_CAP_NT_SMBS | SMB_CAP_UNICODE;
	
	if (vcp->vc_hflags2 & SMB_FLAGS2_ERR_STATUS)
		caps |= SMB_CAP_STATUS32;
	
	/* If they support it then we support it. */
	if (VC_CAPS(vcp) & SMB_CAP_LARGE_READX)
		caps |= SMB_CAP_LARGE_READX;
	
	if (VC_CAPS(vcp) & SMB_CAP_LARGE_WRITEX)
		caps |= SMB_CAP_LARGE_WRITEX;
	
	if (VC_CAPS(vcp) & SMB_CAP_UNIX)
		caps |= SMB_CAP_UNIX;
	
	return (caps);
	
}

/*
 * Retreive the OS and Lan Man Strings. The calling routines 
 */
void 
parse_server_os_lanman_strings(struct smb_vc *vcp, void *refptr, uint16_t bc)
{
	struct mdchain *mdp = (struct mdchain *)refptr;
	uint8_t *tmpbuf= NULL;
	size_t oslen = 0, lanmanlen = 0, lanmanoffset = 0;
	int error;
#ifdef SMB_DEBUG
	size_t domainoffset = 0;
#endif // SMB_DEBUG
	
	/*
	 * Make sure we  have a byte count and the byte cound needs to be less that the
	 * amount we negotiated. Also only get this info once, NTLMSSP will cause us to see
	 * this message twice we only need to get it once.
	 */
	if ((bc == 0) || (bc > vcp->vc_txmax) || vcp->NativeOS || vcp->NativeLANManager)
		goto done;
    SMB_MALLOC(tmpbuf, uint8_t *, bc, M_SMBTEMP, M_WAITOK);
	if (!tmpbuf)
		goto done;
	
	error = md_get_mem(mdp, (void *)tmpbuf, bc, MB_MSYSTEM);
	if (error)
		goto done;
	
#ifdef SMB_DEBUG
	smb_hexdump(__FUNCTION__, "BLOB = ", tmpbuf, bc);
#endif // SMB_DEBUG
	
	if (SMB_UNICODE_STRINGS(vcp)) {
		/* Find the end of the OS String */
		lanmanoffset = oslen = smb_utf16_strnsize((const uint16_t *)tmpbuf, bc);
		lanmanoffset += 2;	/* Skip the null bytes */
		if (lanmanoffset < bc) {
			bc -= lanmanoffset;
			/* Find the end of the Lanman String */
			lanmanlen = smb_utf16_strnsize((const uint16_t *)&tmpbuf[lanmanoffset], bc);
#ifdef SMB_DEBUG
			domainoffset = lanmanlen;
			domainoffset += 2;	/* Skip the null bytes */
#endif // SMB_DEBUG
		}
	} else {
		/* Find the end of the OS String */
		lanmanoffset = oslen = strnlen((const char *)tmpbuf, bc);
		lanmanoffset += 1;	/* Skip the null bytes */
		if (lanmanoffset < bc) {
			bc -= lanmanoffset;
			/* Find the end of the Lanman String */
			lanmanlen = strnlen((const char *)&tmpbuf[lanmanoffset], bc);
#ifdef SMB_DEBUG
			domainoffset = lanmanlen;
			domainoffset += 1;	/* Skip the null bytes */
#endif // SMB_DEBUG
		}
	}
	
#ifdef SMB_DEBUG
	if (domainoffset && (domainoffset < bc)) {
		bc -= domainoffset;
	} else {
		bc = 0;
	}
	smb_hexdump(__FUNCTION__, "OS = ", tmpbuf, oslen);
	smb_hexdump(__FUNCTION__, "LANMAN = ", &tmpbuf[lanmanoffset], lanmanlen);
	smb_hexdump(__FUNCTION__, "DOMAIN = ", &tmpbuf[domainoffset+lanmanoffset], bc);
#endif // SMB_DEBUG
	
	vcp->vc_flags &= ~(SMBV_WIN2K_XP | SMBV_DARWIN);
	if (oslen) {
		vcp->NativeOS = smbfs_ntwrkname_tolocal((const char *)tmpbuf, &oslen, 
												SMB_UNICODE_STRINGS(vcp));
		if (vcp->NativeOS) {
			/*
			 * Windows 2000 and Windows XP don't handle zero fill correctly. See
			 * if this is a Windows 2000 or Windows XP by checking the OS name 
			 * string. Windows 2000 has an OS name of "Windows 5.0" and XP has a 
			 * OS name of "Windows 5.1".  Windows 2003 returns a totally different 
			 * OS name.
			 */
			if ((oslen >= strlen(WIN2K_XP_UTF8_NAME)) && 
				(strncasecmp(vcp->NativeOS, WIN2K_XP_UTF8_NAME, 
							 strlen(WIN2K_XP_UTF8_NAME)) == 0)) {
				vcp->vc_flags |= SMBV_WIN2K_XP;
			}
			/* Now see this is a Darwin smbx server */
			if ((oslen >= strlen(DARWIN_UTF8_NAME)) && 
				(strncasecmp(vcp->NativeOS, DARWIN_UTF8_NAME, 
							 strlen(DARWIN_UTF8_NAME)) == 0)) {
				vcp->vc_flags |= SMBV_DARWIN;
			}
		}
	}
	if (lanmanlen) {
		vcp->NativeLANManager= smbfs_ntwrkname_tolocal((const char *)&tmpbuf[lanmanoffset], &lanmanlen, SMB_UNICODE_STRINGS(vcp));
	}
	SMB_LOG_AUTH("NativeOS = %s NativeLANManager = %s server type 0x%x\n", 
			   (vcp->NativeOS) ? vcp->NativeOS : "NULL",
			   (vcp->NativeLANManager) ? vcp->NativeLANManager : "NULL", 
			   (vcp->vc_flags & SMBV_SERVER_MODE_MASK));
	
done:
    if (tmpbuf != NULL) {
        SMB_FREE(tmpbuf, M_SMBTEMP);
    }
}

static char *
upper_casify_string(const char *string)
{
    size_t string_len, i;
    char *ucstrbuf;

    string_len = strlen(string);
    SMB_MALLOC(ucstrbuf, char *, string_len + 1, M_SMBTEMP, M_WAITOK | M_ZERO);
    
    for (i = 0; i < string_len; i++) {
        if ((string[i] >= 'a') && (string[i] <= 'z')) {
            ucstrbuf[i] = string[i] - 32;
        }
        else {
            ucstrbuf[i] = string[i];
        }
    }
    
    return (ucstrbuf);
}

static u_char *
add_name_to_blob(u_char *blobnames, const u_char *name, size_t namelen,
                 int nametype, int uppercase)
{
    struct ntlmv2_namehdr namehdr;
    char *namebuf;
    u_int16_t *uninamebuf;
    size_t uninamelen;

    if (name != NULL) {
        SMB_MALLOC(uninamebuf, u_int16_t *, 2 * namelen, M_SMBTEMP, M_WAITOK | M_ZERO);
        if (uppercase) {
            namebuf = upper_casify_string((char *) name);
            uninamelen = smb_strtouni(uninamebuf, namebuf, namelen,
                                      UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
            if (namebuf) {
                SMB_FREE(namebuf, M_SMBTEMP);
            }
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
        if (uninamebuf) {
            SMB_FREE(uninamebuf, M_SMBTEMP);
        }
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
    blobsize = sizeof(struct ntlmv2_blobhdr) +
               (3 * sizeof (struct ntlmv2_namehdr))
               + 4 + (2 * domainlen) + (2 * srvlen);
    SMB_MALLOC(blob, u_char *, blobsize, M_SMBTEMP, M_WAITOK | M_ZERO);
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
    smb_time_local2NT(&now, &timestamp, 1);
    blobhdr->timestamp = htoleq(timestamp);
    blobhdr->client_nonce = client_nonce;
    blobnames = blob + sizeof (struct ntlmv2_blobhdr);
    blobnames = add_name_to_blob(blobnames, (u_char *)dom, domainlen,
                                 NAMETYPE_DOMAIN_NB, 1);
    
    //	blobnames = add_name_to_blob(blobnames, vcp, (u_char *)vcp->vc_srvname,
    //				     srvlen, NAMETYPE_MACHINE_NB, 1);
    blobnames = add_name_to_blob(blobnames, NULL, 0, NAMETYPE_EOL, 0);
    *bloblen = blobnames - blob;
    return (blob);
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
#define kSTATE_NTLMv2 0
#define kSTATE_NTLMv1 1

int
smb_smb_ssnsetup(struct smb_vc *vcp, int inReconnect, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc;
	const char *pp = NULL;		/* Holds the information used in the case insesitive password field */
	size_t plen = 0, uniplen = 0;
	int error = 0;
	uint32_t caps;
	uint16_t action;
	uint16_t bc;  
	uint16_t maxtx = vcp->vc_txmax;
	uint8_t lm[24] = {0};
	uint8_t ntlm[24] = {0};
	int state = kSTATE_NTLMv2;
	u_int64_t client_nonce;
    u_char v2hash[16];
	u_char *upper_case_userp = NULL;
	u_int16_t string_len;
	char *upper_case_domainp = NULL;
	char *encrypted_password = NULL;
    u_char *v2_blob = NULL;
	size_t v2_bloblen;
	smb_uniptr unipp = NULL, nt_encrypt_password = NULL;
    
	if (smb_smb_nomux(vcp, __FUNCTION__, context) != 0) {
		error = EINVAL;
		goto ssn_exit;
	}

	if ((VC_CAPS(vcp) & SMB_CAP_EXT_SECURITY)) {
		error = smb_gss_ssnsetup(vcp, context);

        if (!(inReconnect) && (vcp->vc_flags & SMBV_SMB2)) {
            /* Not in reconnect, its now safe to start up crediting */
            smb2_rq_credit_start(vcp, 0);
        }

        goto ssn_exit;
	}

	caps = smb_vc_caps(vcp);
    
again:
	vcp->vc_smbuid = SMB_UID_UNKNOWN;

	/*
	 * Domain name must be upper-case, as that's what's used
	 * when computing LMv2 and NTLMv2 responses - and, for NTLMv2,
	 * the domain name in the request has to be upper-cased as well.
	 * (That appears not to be the case for the user name.  Go
	 * figure.)
	 *
	 * Don't need to uppercase domain. It's already uppercase UTF-8.
	 *
	 * NOTE: We use to copy the vc_domain into an allocated buffer and then
	 * copy it into the mbuf, not sure why. Now we just copy vc_domain straight
	 * in to the mbuf.
	 */
	string_len = strlen(vcp->vc_domain) + 1 /* strlen doesn't count null */;
    SMB_MALLOC(upper_case_domainp, char *, string_len, M_SMBTEMP, M_WAITOK);
	memcpy(upper_case_domainp, vcp->vc_domain, string_len);

	if (!(vcp->vc_flags & SMBV_USER_SECURITY)) {
		/*
		 * In the share security mode password will be used
		 * only in the tree authentication
		 */
		pp = "";
		plen = 1;
		uniplen = 0;
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
	} else if (vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) {
        /* 
         * The server wants NTLMv1/v2 without going through Extended Security 
         * Negotiation. We no longer support this if signing is on.
         */
        SMBERROR("SIGNING REQUIRED: Server %s needs to enable extended security negotiation for NTLM authentication, disconnecting\n", vcp->vc_srvname);
		error = SMB_ENETFSNOPROTOVERSSUPP;
        goto ssn_exit;
	}
    else {
        SMBERROR("%s doesn't support extended security, this server will be deprecated in the future!\n",
                 vcp->vc_srvname);
        
        if (state == kSTATE_NTLMv2) {
            /* Do NTLMv2 authentication */
            
            /*
             * Compute the LMv2 and NTLMv2 responses,
             * derived from the challenge, the user name,
             * the domain/workgroup into which we're
             * logging, and the Unicode password.
             */
            
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
            if ((vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) &&
                (vcp->vc_username[0] == '\0')) {
                *upper_case_domainp = '\0';
            }
            
            /*
             * Convert the user name to upper-case, as
             * that's what's used when computing LMv2
             * and NTLMv2 responses.
             */
            upper_case_userp = (u_char *)upper_casify_string(vcp->vc_username);
            
            smb_ntlmv2hash((u_char *)smb_vc_getpass(vcp), upper_case_userp,
                           (u_char *)upper_case_domainp, &v2hash[0]);
            
            if (upper_case_userp) {
                SMB_FREE(upper_case_userp, M_SMBTEMP);
            }
            
            /*
             * Compute the LMv2 response, derived
             * from the v2hash, the server challenge,
             * and the client nonce.
             */
            smb_ntlmv2response(v2hash, vcp->vc_ch,
                               (u_char *)&client_nonce, sizeof(client_nonce),
                               (u_char**)&encrypted_password, &plen);
            pp = encrypted_password;
            
            /*
             * Construct the blob.
             */
            v2_blob = make_ntlmv2_blob(vcp, upper_case_domainp, client_nonce,
                                       &v2_bloblen);
            
            /*
             * Compute the NTLMv2 response, derived
             * from the server challenge, the
             * user name, the domain/workgroup
             * into which we're logging, the
             * blob, and the Unicode password.
             */
            smb_ntlmv2response(v2hash, vcp->vc_ch, v2_blob, v2_bloblen,
                               (u_char**)&nt_encrypt_password, &uniplen);
            if (v2_blob) {
                SMB_FREE(v2_blob, M_SMBTEMP);
            }
            unipp = nt_encrypt_password;
        }
        else {
            /* Do NTLMv1 authentication */
            plen = sizeof(lm);
            pp = (char *)lm;
            uniplen = sizeof(ntlm);
            smb_ntlmresponse((u_char *)smb_vc_getpass(vcp), vcp->vc_ch, (u_char*)ntlm);
            unipp = (smb_uniptr) ntlm;
        }
	}
    
	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX, 0, context, &rqp);
    if (error) {
        goto ssn_exit;
    }
	
	smb_rq_wstart(rqp);
    smb_rq_getrequest(rqp, &mbp);
	/*
	 * We now have a flag telling us to attempt an anonymous connection. All 
	 * this means is have no user name, password or domain.
	 */
	if (vcp->vc_flags & SMBV_ANONYMOUS_ACCESS) /*  anon */
		plen = uniplen = 0;

	mb_put_uint8(mbp, 0xff);
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0);
	mb_put_uint16le(mbp, maxtx);
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
	if (uniplen) {
		mb_put_mem(mbp, (caddr_t)unipp, uniplen, MB_MSYSTEM);
	}
	smb_put_dstring(mbp, SMB_UNICODE_STRINGS(vcp), vcp->vc_username,
                    SMB_MAXUSERNAMELEN + 1, NO_SFM_CONVERSIONS); /* user */
	smb_put_dstring(mbp, SMB_UNICODE_STRINGS(vcp), vcp->vc_domain,
                    SMB_MAXNetBIOSNAMELEN + 1, NO_SFM_CONVERSIONS); /* domain */

	smb_put_dstring(mbp, SMB_UNICODE_STRINGS(vcp), SMBFS_NATIVEOS,
                    sizeof(SMBFS_NATIVEOS), NO_SFM_CONVERSIONS);	/* Native OS */
	smb_put_dstring(mbp, SMB_UNICODE_STRINGS(vcp), SMBFS_LANMAN,
                    sizeof(SMBFS_LANMAN), NO_SFM_CONVERSIONS);	/* LAN Mgr */

	smb_rq_bend(rqp);

    if (nt_encrypt_password) {
		SMB_FREE(nt_encrypt_password, M_SMBTEMP);
		nt_encrypt_password = NULL;
	}
    
    if (upper_case_domainp) {
        SMB_FREE(upper_case_domainp, M_SMBTEMP);
        upper_case_domainp = NULL;
    }
    
	error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);
	if (error) {
		SMBDEBUG("error = %d, rpflags2 = 0x%x, sr_ntstatus = 0x%x\n", error, 
				 rqp->sr_rpflags2, rqp->sr_ntstatus);
	}

	if (error) {
		if (error == EACCES) {
			error = EAUTH;
		}
		if (rqp->sr_ntstatus != STATUS_MORE_PROCESSING_REQUIRED) {
			goto bad;
		}
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
		 * so we need to always deal with the pad byte.
		 */
		if (bc > 0) {
			md_get_uint8(mdp, NULL);	/* Skip Pad Byte */
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
	 * We are in user level security, got log in as guest, but we are not 
     * using guest access. We need to log off and return an error.
	 */
	if ((error == 0) && (vcp->vc_flags & SMBV_USER_SECURITY) && 
		!SMBV_HAS_GUEST_ACCESS(vcp) &&
        !SMBV_HAS_ANONYMOUS_ACCESS(vcp) &&
        (action & SMB_ACT_GUEST)) {
		/* 
		 * Wanted to only login the users as guest if they ask to be login ask guest. Window system will
		 * login any bad user name as guest if guest is turn on. The problem here is with XPHome. XPHome
		 * doesn't care if the user is real or made up, it always logs them is as guest.
		 */
		SMBWARNING("Got guess access, but wanted real access.\n");
		(void)smb_smb_ssnclose(vcp, context);
		error = EAUTH;
	}
    else {
        if ((error) && (vcp->vc_flags & SMBV_USER_SECURITY) && (state == kSTATE_NTLMv2)) {
            if (vcp->vc_flags & SMBV_NO_NTLMV1) {
                SMBERROR("NTLMv1 is not allowed!\n");
                error = SMB_ENETFSNOAUTHMECHSUPP;
            }
            else {
                SMBDEBUG("Trying NTLMv1 \n");
                state = kSTATE_NTLMv1;
                smb_rq_done(rqp);
                goto again;
            }
        }
    }
	
	smb_rq_done(rqp);

ssn_exit:
	
	if ((vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) != SMBV_ENCRYPT_PASSWORD) {
		/* We turned off UNICODE for Clear Text Password turn it back on */
		vcp->vc_hflags2 |= SMB_FLAGS2_UNICODE;
	}
    
    if (error) {
        /* Reset the signature info */
        smb_reset_sig(vcp);
    }

    if (error && (error != EAUTH)) {
        SMBWARNING("SetupAndX failed error = %d\n", error);
    }
    
    return (error);
}

int
smb1_smb_ssnclose(struct smb_vc *vcp, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	if (vcp->vc_smbuid == SMB_UID_UNKNOWN)
		return 0;

	if (smb_smb_nomux(vcp, __FUNCTION__, context) != 0)
		return EINVAL;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_LOGOFF_ANDX, 0, context, &rqp);
	if (error)
		return error;
    smb_rq_getrequest(rqp, &mbp);
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

/*
 * Get the type of file system this share is exporting, we treat all unknown
 * file system types as FAT. This should hurt since this is only used for
 * Dfs failover currently. If we ever expand its use we should rethink name
 * pipes.
 */
static void
smb_get_share_fstype(struct smb_vc *vcp, struct smb_share *share, 
					 struct mdchain *mdp)
{
	char *tmpbuf = NULL;
	char *fsname = NULL;
	uint16_t bc;
	size_t fs_nmlen, fs_offset;
	int error;
	
	/* We always default to saying its a fat file system type */
	share->ss_fstype = SMB_FS_FAT;
	/* Get the remaining number of bytes */
	md_get_uint16le(mdp, &bc);
	SMBDEBUG("tree bc = %d\n", bc);
	if ((bc == 0) || (bc >= vcp->vc_txmax)) {
		/* Nothing here just get out we are done */
		return;
	}
	
	SMB_MALLOC(tmpbuf, char *, bc+1, M_SMBFSDATA, M_WAITOK | M_ZERO);
	if (! tmpbuf) {
		/* Couldn't allocate the buffer just get out */
		return;
	}
	error = md_get_mem(mdp, (void *)tmpbuf, bc, MB_MSYSTEM);
	if (error) {
        if (tmpbuf) {
            SMB_FREE(tmpbuf, M_SMBFSDATA);
        }
		return;
	}
	/*
	 * Skip over the service type:
	 * Service (variable): The type of the shared resource to which the 
	 * TID is connected. The Service field MUST be encoded as a 
	 * null-terminated array of OEM characters, even if the client and 
	 * server have negotiated to use Unicode strings.
	 *
	 * Note we allocated the buffer 1 byte bigger so tmpbuf is always
	 * null terminated
	 */
	SMBDEBUG("Service type = %s\n", tmpbuf);
	/* Get the offset to the file system name, skip the null byte */
	fs_offset = strnlen(tmpbuf, bc) + 1;
	if (fs_offset >= bc) {
        if (tmpbuf) {
            SMB_FREE(tmpbuf, M_SMBFSDATA);
        }
		return;
	}
	/*
	 * Pad(variable): Padding bytes. If Unicode support has been enabled and 
	 * SMB_FLAGS2_UNICODE is set in SMB_Header.Flags2, this field MUST contain 
	 * zero or one null padding byte as needed to ensure that the 
	 * NativeFileSystem string is aligned on a 16-bit boundary.
	 *
	 * We always start on an odd boundry, because of word count. So if fs_offset
	 * is even then we need to pad.
	 */
	if (!(fs_offset & 1) && (SMB_UNICODE_STRINGS(vcp))) {
			fs_offset += 1;
	}
	/*
	 * NativeFileSystem (variable): The name of the file system on the local 
	 * resource to which the returned TID is connected. If SMB_FLAGS2_UNICODE 
	 * is set in the Flags2 field of the SMB Header of the response, this value 
	 * MUST be a null-terminated string of Unicode characters. Otherwise, this 
	 * field MUST be a null-terminated string of OEM characters. For resources 
	 * that are not backed by a file system, such as the IPC$ share used for 
	 * named pipes, this field MUST be set to the empty string.
	 * 
	 * The smbfs_ntwrkname_tolocal routine doesn't expect null terminated 
	 * unicode strings, so figure out how long the string is without the
	 * null bytes.
	 */
	if (SMB_UNICODE_STRINGS(vcp)) {
		fs_nmlen = smb_utf16_strnsize((const uint16_t *)&tmpbuf[fs_offset], 
									  bc - fs_offset);
	} else {
		fs_nmlen = bc - fs_offset;
	}
	SMBDEBUG("fs_offset = %d fs_nmlen = %d\n", (int)fs_offset, (int)fs_nmlen);
	fsname = smbfs_ntwrkname_tolocal(&tmpbuf[fs_offset], &fs_nmlen, SMB_UNICODE_STRINGS(vcp));
	/*
	 * Since we default to FAT the following can be ignored:
	 *		"FAT", "FAT12", "FAT16", "FAT32"
	 */
	if (strncmp(fsname, "CDFS", fs_nmlen) == 0) {
		share->ss_fstype = SMB_FS_CDFS;
	} else if (strncmp(fsname, "UDF", fs_nmlen) == 0) {
		share->ss_fstype = SMB_FS_UDF;
	} else if (strncmp(fsname, "NTFS", fs_nmlen) == 0) {
		share->ss_fstype = SMB_FS_NTFS;
	}
	SMBDEBUG("fsname = %s fs_nmlen = %d ss_fstype = %d\n", fsname, 
			 (int)fs_nmlen, share->ss_fstype);

    if (tmpbuf != NULL) {
        SMB_FREE(tmpbuf, M_SMBFSDATA);
    }
    if (fsname != NULL) {
        SMB_FREE(fsname, M_SMBFSDATA);
    }
	return;
}

#define SMB_ANY_SHARE_NAME		"?????"
#define SMB_DISK_SHARE_NAME		"A:"
#define SMB_PRINTER_SHARE_NAME	SMB_ANY_SHARE_NAME
#define SMB_PIPE_SHARE_NAME		"IPC"
#define SMB_COMM_SHARE_NAME		"COMM"

static int 
smb1_treeconnect_internal(struct smb_vc *vcp, struct smb_share *share,  
						 const char *serverName, size_t serverNameLen, 
						 vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc = 0;
	const char *pp;
	char *pbuf, *encpass;
	int error;
	uint16_t plen;

	share->ss_tid = SMB_TID_UNKNOWN;
	error = smb_rq_alloc(SSTOCP(share), SMB_COM_TREE_CONNECT_ANDX, 0, context, &rqp);
	if (error)
		goto treeconnect_exit;
	if (vcp->vc_flags & SMBV_USER_SECURITY) {
		plen = 1;
		pp = "";
		pbuf = NULL;
		encpass = NULL;
	} else {
        SMB_MALLOC(pbuf, char *, SMB_MAXPASSWORDLEN + 1, M_SMBTEMP, M_WAITOK);
        SMB_MALLOC(encpass, char *, 24, M_SMBTEMP,  M_WAITOK);
		strlcpy(pbuf, smb_share_getpass(share), SMB_MAXPASSWORDLEN+1);
		pbuf[SMB_MAXPASSWORDLEN] = '\0';

		if (vcp->vc_flags & SMBV_ENCRYPT_PASSWORD) {
			plen = 24;
			smb_lmresponse((u_char *)pbuf, vcp->vc_ch, (u_char *)encpass);
			pp = encpass;
		} else {
			plen = (uint16_t)strnlen(pbuf, SMB_MAXPASSWORDLEN + 1) + 1;
			pp = pbuf;
		}
	}
    smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0);
	mb_put_uint16le(mbp, TREE_CONNECT_ANDX_EXTENDED_RESPONSE);
	mb_put_uint16le(mbp, plen);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	error = mb_put_mem(mbp, pp, plen, MB_MSYSTEM);
	if (error) {
		SMBERROR("error %d from mb_put_mem for pp\n", error);
		goto bad;
	}
	smb_put_dmem(mbp, "\\\\", 2, NO_SFM_CONVERSIONS, SMB_UNICODE_STRINGS(vcp), NULL);
	/*
	 * User land code now passes down the server name in the proper format that 
	 * can be used in a tree connection. This is two complicated of an issue to
	 * be handle in the kernel, but we do know the following:
	 *
	 * The server's NetBIOS name will always work, but we can't always get it because
	 * of firewalls. Window cluster system require the name to be a NetBIOS
	 * name or the cluster's fully qualified dns domain name.
	 *
	 * Windows XP will not allow DNS names to be used and in fact requires a
	 * name that must fit in a NetBIOS name. So if we don't have the NetBIOS
	 * name we can send the IPv4 address in presentation form (xxx.xxx.xxx.xxx).
	 *
	 * If we are doing IPv6 then it looks like we can just send the server name
	 * provided by the user. The same goes for Bonjour names. 
	 *
	 * We now always use the name passed in and let the calling routine decide.
	 */
	error = smb_put_dmem(mbp, serverName, serverNameLen, NO_SFM_CONVERSIONS,SMB_UNICODE_STRINGS(vcp),  NULL);
	if (error) {
		SMBERROR("error %d from smb_put_dmem for srvname\n", error);
		goto bad;
	}		
	smb_put_dmem(mbp, "\\", 1, NO_SFM_CONVERSIONS, SMB_UNICODE_STRINGS(vcp), NULL);

	error = smb_put_dstring(mbp, SMB_UNICODE_STRINGS(vcp), share->ss_name, SMB_MAXSHARENAMELEN+1, NO_SFM_CONVERSIONS);
	if (error) {
		SMBERROR("error %d from smb_put_dstring for ss_name\n", error);
		goto bad;
	}
	/* The type name is always ASCII */
	error = mb_put_mem(mbp, SMB_ANY_SHARE_NAME, sizeof(SMB_ANY_SHARE_NAME), MB_MSYSTEM);
	if (error) {
		SMBERROR("error %d from mb_put_mem for share type\n", error);
		goto bad;
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	if (error)
		goto bad;
		
	smb_rq_getreply(rqp, &mdp);
	md_get_uint8(mdp, &wc);
	if ((wc != TREE_CONNECT_NORMAL_WDCNT) && (wc != TREE_CONNECT_EXTENDED_WDCNT)) {
		SMBERROR("Malformed tree connect with a bad word count! wc= %d \n", wc);
		error = EBADRPC;
		goto bad;
	}
	md_get_uint8(mdp, NULL);	/* AndXCommand */
	md_get_uint8(mdp, NULL);	/* AndXReserved */
	md_get_uint16le(mdp, NULL);	/* AndXOffset */
	md_get_uint16le(mdp, &share->optionalSupport);	/* OptionalSupport */
	/* 
	 * If extended response the we need to get the maximal access of the share.
	 */
	if (wc == TREE_CONNECT_EXTENDED_WDCNT) {
		if (md_get_uint32le(mdp, &share->maxAccessRights)) {
			share->maxAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
		} else {
			SMB_LOG_ACCESS("maxAccessRights = 0x%x \n", share->maxAccessRights);
		}

		if (md_get_uint32le(mdp, &share->maxGuestAccessRights)) {
			share->maxGuestAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
		}
	} else {
		share->maxAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
		share->maxGuestAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
	}
	smb_get_share_fstype(vcp, share, mdp);

	share->ss_tid = rqp->sr_rptid;
	lck_mtx_lock(&share->ss_stlock);
	share->ss_flags |= SMBS_CONNECTED;
	lck_mtx_unlock(&share->ss_stlock);
bad:
    if (encpass) {
        SMB_FREE(encpass, M_SMBTEMP);
    }
    
    if (pbuf) {
        SMB_FREE(pbuf, M_SMBTEMP);
    }
    
    smb_rq_done(rqp);
treeconnect_exit:
	return error;
}

/*
 * XP requires the tree connect server name to either contain the NetBIOS
 * name or IPv4 in the dot presentation format. Windows Cluster wants the
 * cluster NetBIOS name or the cluster dns name. So if we resolved using
 * NetBIOS then we always use the NetBIOS name, otherwise we use the name
 * that we use to resolve the address. Now if the treeconnect fails then
 * attempt to use the IPv4 dot presentation name.
 */
int 
smb_smb_treeconnect(struct smb_share *share, vfs_context_t context)
{
	struct smb_vc *vcp = SSTOVC(share);
	int error;
	size_t srvnamelen;
	char *serverName;
	
	/* No IPv4 or IPv6 dot name so use the real server name in the tree connect */
	if (vcp->ipv4v6DotName[0] == 0) {
		serverName = vcp->vc_srvname;
		srvnamelen = strnlen(serverName, SMB_MAX_DNS_SRVNAMELEN+1); 
        if (vcp->vc_flags & SMBV_SMB2) {
            error = smb2_smb_tree_connect(vcp, share, serverName, srvnamelen,
                                          context);
       }
        else {
            error = smb1_treeconnect_internal(vcp, share, serverName, 
                                              srvnamelen, context);
        }
        
		/* 
         * See if we can get the IPv4 or IPv6 presentation format.
         * If so, see if we can Tree Connect with that address instead
         */
        if (error) {
            if (vcp->vc_saddr->sa_family == AF_INET) {
                /* IPv4 */
                struct sockaddr_in *in = (struct sockaddr_in *)vcp->vc_saddr;
                (void)inet_ntop(AF_INET, &in->sin_addr.s_addr, vcp->ipv4v6DotName, sizeof(vcp->ipv4v6DotName));
                SMBWARNING("treeconnect failed using server name %s with error %d\n", serverName, error);
            }
            else if (vcp->vc_saddr->sa_family == AF_INET6) {
                /* IPv6 - returns with no brackets */
                struct sockaddr_in *in = (struct sockaddr_in *)vcp->vc_saddr;
                (void)inet_ntop(AF_INET6, &in->sin_addr.s_addr, vcp->ipv4v6DotName, sizeof(vcp->ipv4v6DotName));
                SMBWARNING("treeconnect failed using server name %s with error %d\n", serverName, error);
            }
		}
    }
    
	/* Use the IPv4 or IPv6 dot name in the tree connect */
	if (vcp->ipv4v6DotName[0] != 0) {
		serverName = vcp->ipv4v6DotName;
		srvnamelen = strnlen(serverName, SMB_MAXNetBIOSNAMELEN+1); 
        if (vcp->vc_flags & SMBV_SMB2) {
            error = smb2_smb_tree_connect(vcp, share, serverName, srvnamelen,
                                          context);
        }
        else {
            error = smb1_treeconnect_internal(vcp, share, serverName, 
                                              srvnamelen, context);
        }
		if (error)
			vcp->ipv4v6DotName[0] = 0;	/* We failed don't use it again */
	}
	return error;
}

int 
smb1_smb_treedisconnect(struct smb_share *share, vfs_context_t context)
{
	struct smb_rq *rqp;
	int error;
    
	if (share->ss_tid == SMB_TID_UNKNOWN) {
		return 0;
    }
    
	error = smb_rq_alloc(SSTOCP(share), SMB_COM_TREE_DISCONNECT, 0, context, &rqp);
	if (error)
		return error;
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	smb_rq_done(rqp);
	share->ss_tid = SMB_TID_UNKNOWN;
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
static __inline int
smb_smb_readx(struct smb_share *share, SMBFID fid, user_ssize_t *len, 
	user_ssize_t *rresid, uint16_t *available, uio_t uio, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc;
	int error;
	uint16_t residhi, residlo, off, doff;
	uint32_t resid;
    uint16_t smb1_fid = (uint16_t) fid;

	error = smb_rq_alloc(SSTOCP(share), SMB_COM_READ_ANDX, 0, context, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* no secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to secondary */
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
	mb_put_uint32le(mbp, (uint32_t)uio_offset(uio));
	*len = MIN(SSTOVC(share)->vc_rxmax, *len);

	mb_put_uint16le(mbp, (uint16_t)*len);	/* MaxCount */
	mb_put_uint16le(mbp, (uint16_t)*len);	/* MinCount (only indicates blocking) */
	mb_put_uint32le(mbp, (uint32_t)((user_size_t)*len >> 16));	/* MaxCountHigh */
	mb_put_uint16le(mbp, (uint16_t)*len);	/* Remaining ("obsolete") */
	mb_put_uint32le(mbp, (uint32_t)(uio_offset(uio) >> 32));
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
		/*
		 * Available (2 bytes): This field is valid when reading from named pipes 
		 * or I/O devices. This field indicates the number of bytes remaining to 
		 * be read after the requested read was completed. If the client reads 
		 * from a disk file, this field MUST be set to -1 (0xFFFF). <62>
		 */
		*available = 0;
		md_get_uint16le(mdp, available);
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

/*
 * The calling routine must hold a reference on the share
 */
static __inline int
smb_writex(struct smb_share *share, SMBFID fid, user_ssize_t *len, 
		   user_ssize_t *rresid, uio_t uio, uint16_t writeMode, vfs_context_t context)
{
	int supportsLargeWrites = (VC_CAPS(SSTOVC(share)) & SMB_CAP_LARGE_FILES) ? TRUE : FALSE;
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	uint8_t wc;
	uint16_t resid;
	uint16_t *dataOffsetPtr;
    uint16_t smb1_fid = (uint16_t) fid;
	
	/* vc_wxmax now holds the max buffer size the server supports for writes */
	*len = MIN(*len, SSTOVC(share)->vc_wxmax);
	
	error = smb_rq_alloc(SSTOCP(share), SMB_COM_WRITE_ANDX, 0, context, &rqp);
	if (error)
		return (error);
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* AndXCommand */
	mb_put_uint8(mbp, 0);		/* Reserved */
	mb_put_uint16le(mbp, 0);	/* AndXOffset */
	/* 
	 * [MS-CIFS]
	 * FID (2 bytes): This field MUST be a valid FID indicating the file to 
	 * which the data SHOULD be written.
	 *
	 * NOTE: Currently the fid always stays in the wire format.
	 */
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
	/* 
	 * [MS-CIFS]
	 * Offset (4 bytes): If WordCount is 0x0C, this field represents a 32-bit 
	 * offset, measured in bytes, of where the write SHOULD start relative to the 
	 * beginning of the file. If WordCount is 0xE, this field represents the 
	 * lower 32 bits of a 64-bit offset.
	 */
	mb_put_uint32le(mbp, (uint32_t)uio_offset(uio));
	/* 
	 * [MS-CIFS]
	 * Timeout (4 bytes): This field represents the amount of time, in milliseconds, 
	 * that a server MUST wait before sending a response. It is used only when 
	 * writing to a named pipe or I/O device and does not apply when writing to 
	 * a disk file. Support for this field is optional. Two values have special 
	 * meaning in this field:
	 * If the Timeout value is -1 (0xFFFFFFFF, "wait forever"), the server MUST 
	 * wait until all bytes of data are written to the device before returning a 
	 * response to the client.
	 * If the Timeout value is -2 (0xFFFFFFFE, "default"), the server MUST wait 
	 * for the default time-out associated with the named pipe or I/O device.
	 *
	 * NOTE: We have always set it to zero and so does Windows Clients.
	 */
	mb_put_uint32le(mbp, 0);	/* Timeout  */
	/*
	 * [MS-CIFS]	 
	 * WriteMode (2 bytes): A 16-bit field containing flags defined as follows:
	 * WritethroughMode 0x0001
	 *		If set the server MUST NOT respond to the client before the data is 
	 *		written to disk (write-through).
	 * ReadBytesAvailable 0x0002
	 *		If set the server SHOULD set the Response.SMB_Parameters.Available 
	 *		field correctly for writes to named pipes or I/O devices.
	 * RAW_MODE 0x0004
	 *		Applicable to named pipes only. If set, the named pipe MUST be written 
	 *		to in raw mode (no translation).
	 * MSG_START 0x0008
	 *		Applicable to named pipes only. If set, this data is the start of a message. 
	 */
	mb_put_uint16le(mbp, writeMode);
	/* 
	 * [MS-CIFS]	 
	 * Remaining (2 bytes): This field is an advisory field telling the server 
	 * approximately how many bytes are to be written to this file before the next 
	 * non-write operation. It SHOULD include the number of bytes to be written 
	 * by this request. The server MAY either ignore this field or use it to 
	 * perform optimizations.
	 */
	mb_put_uint16le(mbp, 0);
	/*
	 * [MS-CIFS]	 	 
	 * Reserved (2 bytes): This field MUST be 0x0000.
	 *
	 * Wrong only zero if the server doesn't support SMB_CAP_LARGE_WRITEX otherwise
	 * contains the DataLengthHigh. If the server doesn't support SMB_CAP_LARGE_WRITEX 
	 * then the upper part of length will zero.
	 */
	mb_put_uint16le(mbp, (uint16_t)((user_size_t)*len >> 16));
	/*
	 * [MS-CIFS]	 	 
	 * DataLength (2 bytes): This field is the number of bytes included in the 
	 * SMB_Data that are to be written to the file.
	 */
	mb_put_uint16le(mbp, (uint16_t)*len);
	/*
	 * [MS-CIFS]	 	 
	 * DataOffset (2 bytes): The offset in bytes from the start of the SMB header 
	 * to the start of the data that is to be written to the file. Specifying this 
	 * offset allows a client to efficiently align the data buffer.
	 */
	dataOffsetPtr = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));
	/*
	 * [MS-CIFS]	 	 
	 * OffsetHigh (4 bytes): This field is optional. If WordCount is 0x0C, this 
	 * field is not included in the request. If WordCount is 0x0E, this field 
	 * represents the upper 32 bits of a 64-bit offset, measured in bytes, of 
	 * where the write SHOULD start relative to the beginning of the file.
	 */
	if (!supportsLargeWrites && ((uint32_t)(uio_offset(uio) >> 32))) {
		error = ENOTSUP;
		goto done;
	}
	mb_put_uint32le(mbp, (uint32_t)(uio_offset(uio) >> 32));
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	do {
		
		/*
		 * [MS-CIFS]	 	 
		 * Pad (1 byte): Padding byte that MUST be ignored.
		 */
		mb_put_uint8(mbp, 0);
		
		*dataOffsetPtr = htoles(mb_fixhdr(mbp));	
		error = mb_put_uio(mbp, uio, (int)*len);
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple_timed(rqp, SMBWRTTIMO);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		/*
		 * [MS-CIFS]	 	 
		 * WordCount (1 byte): This field MUST be 0x06. The length in two-byte 
		 * words of the remaining SMB_Parameters.
		 */
		md_get_uint8(mdp, &wc);
		if (wc != 6) {
			error = EBADRPC;
			break;
		}
		md_get_uint8(mdp, NULL);	/* AndXCommand */
		md_get_uint8(mdp, NULL);	/* AndXReserved */
		md_get_uint16le(mdp, NULL);	/* AndXOffset */
		/*
		 * [MS-CIFS]	 	 
		 * Count (2 bytes): The number of bytes written to the file.
		 */
		md_get_uint16le(mdp, &resid);
		*rresid = resid;
		/*
		 * [MS-CIFS]	 	 
		 * Available (2 bytes): This field is valid when writing to named pipes 
		 * or I/O devices. This field indicates the number of bytes remaining to 
		 * be read after the requested write was completed. If the client wrote 
		 * to a disk file, this field MUST be set to -1 (0xFFFF).
		 */
		md_get_uint16le(mdp, NULL);
		/*
		 * [MS-CIFS]	 	 
		 * Reserved (4 bytes): This field MUST be 0x00000000.
		 *
		 * NOTE: Wrong the first two bytes are the high count field and the
		 * last two bytes should be zero.
		 */
		md_get_uint16le(mdp, &resid);
		*rresid |= resid << 16;
		md_get_uint16le(mdp, NULL);
	} while(0);
	
done:
	smb_rq_done(rqp);
	return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb1_read(struct smb_share *share, SMBFID fid, uio_t uio, 
          vfs_context_t context)
{
	user_ssize_t tsize, len, resid = 0;
	int error = 0;
	uint16_t available;

	tsize = uio_resid(uio);
	while (tsize > 0) {
		len = tsize;
		error = smb_smb_readx(share, fid, &len, &resid, &available, uio, context);
		if (error)
			break;
		tsize -= resid;
		/* Nothing else to read we are done */
		if (!resid) {
			SMB_LOG_IO("Server zero bytes read\n");
			break;
		}
		/*
		 * Available (2 bytes): This field is valid when reading from named pipes 
		 * or I/O devices. This field indicates the number of bytes remaining to 
		 * be read after the requested read was completed. If the client reads 
		 * from a disk file, this field MUST be set to -1 (0xFFFF). <62>
		 */
		if (resid < len) {
			if (available == 0xffff) {
				/* They didn't read all the data, log it and keep trying */
				SMB_LOG_IO("Disk IO: Server returns %lld we request %lld\n", resid, len);
			} else if (available) {
				/* They didn't read all the data, log it and keep trying */
				SMB_LOG_IO("PIPE IO: Server returns %lld we request %lld available = %d\n", 
						   resid, len, available);
			} else {
				/* Nothing left to read, we are done */
				break;
			}

		}
	}
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smb1_write(struct smb_share *share, SMBFID fid, uio_t uio, int ioflag, 
			  vfs_context_t context)
{
	int error = 0;
	user_ssize_t  old_resid, len, tsize, resid = 0;
	off_t  old_offset;
	uint16_t writeMode = (ioflag & IO_SYNC) ? WritethroughMode : 0;
		
	tsize = old_resid = uio_resid(uio);
	old_offset = uio_offset(uio);
	
	while (tsize > 0) {
		len = tsize;
		error  = smb_writex(share, fid, &len, &resid, uio, writeMode, context);
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

/*
 * This call is done on the vc not the share. Really should be an async call
 * if we ever get the request queue to work async.
 */
int
smb1_echo(struct smb_vc *vcp, int timo, uint32_t EchoCount, 
         vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_ECHO, 0, context, &rqp);
	if (error)
		return error;
    smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, 1); /* echo count */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint32le(mbp, EchoCount);
	smb_rq_bend(rqp);
	error = smb_rq_simple_timed(rqp, timo);
	smb_rq_done(rqp);
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int smb_checkdir(struct smb_share *share, struct smbnode *dnp, const char *name,  
					 size_t nmlen, vfs_context_t context)
{
	struct smb_rq *rqp;
	struct mbchain *mbp;
	int error;

	error = smb_rq_alloc(SSTOCP(share), SMB_COM_CHECK_DIRECTORY, 0, context, &rqp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	smbfs_fullpath(mbp, dnp, name, &nmlen, UTF_SFM_CONVERSIONS, 
				   SMB_UNICODE_STRINGS(SSTOVC(share)), '\\');
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	return error;
}
