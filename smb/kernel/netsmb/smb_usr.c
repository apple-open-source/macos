/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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
#include <sys/smb_apple.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_converter.h>

int 
smb_usr_negotiate(struct smbioc_negotiate *vspec, vfs_context_t context, 
					  struct smb_dev *sdp, int searchOnly)
{
	struct smb_vc *vcp;
	int error;
	uint32_t usersMaxBufferLen = vspec->ioc_negotiate_token_len;

	/* Convert any pointers over to using user_addr_t */
	if (! vfs_context_is64bit (context)) {
		vspec->ioc_kern_saddr = CAST_USER_ADDR_T(vspec->ioc_saddr);
		vspec->ioc_kern_laddr = CAST_USER_ADDR_T(vspec->ioc_laddr);
	}
	/* Now do the real work */
	error = smb_sm_negotiate(vspec, context, &sdp->sd_vc, sdp, searchOnly);
	if (error) {
		/* We always return the error in the structure, we never fail the iocl from here */ 
		vspec->ioc_errno = error;
		return 0;
	}

	vcp = sdp->sd_vc;
	/* Return to the user the server's capablilities */
	vspec->ioc_ret_caps = VC_CAPS(vcp);
	/* Return to the user the vc flags */
	vspec->ioc_ret_vc_flags = vcp->vc_flags;
	
	/* If we got a server provide init token copy that back to the caller. */
	vspec->ioc_negotiate_token_len = vcp->negotiate_tokenlen;
	if ((vcp->negotiate_token) && (usersMaxBufferLen >= vcp->negotiate_tokenlen)) {
		user_addr_t uaddr = vspec->ioc_negotiate_token;
		error = copyout(vcp->negotiate_token, uaddr, (size_t)vcp->negotiate_tokenlen);
		vspec->ioc_errno = error;
	}	
	/* We are sharing the vc return the user name if there is one */
	if (vspec->ioc_extra_flags & SMB_SHARING_VC) {
		if (vcp->vc_username) {
			strlcpy(vspec->ioc_user, vcp->vc_username, sizeof(vspec->ioc_user));
		}
		/* Return the size of the client and server principal name */
		vspec->ioc_max_client_size = vcp->vc_gss.gss_cpn_len;
		vspec->ioc_max_target_size = vcp->vc_gss.gss_spn_len;
	}
		
	return error;
}

/*
 * Connect to the resource specified by smbioc_ossn structure.
 * It may either find an existing connection or try to establish a new one.
 * If no errors occured smb_vc returned locked and referenced.
 *
 * Called from user land so we always have a reference on the share.
 */
int
smb_usr_simplerequest(struct smb_share *share, struct smbioc_rq *dp, 
					  vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int32_t response_size;
	int error;

	switch (dp->ioc_cmd) {
	    case SMB_COM_CLOSE_AND_TREE_DISC:
	    case SMB_COM_TREE_CONNECT:
	    case SMB_COM_TREE_DISCONNECT:
	    case SMB_COM_NEGOTIATE:
	    case SMB_COM_SESSION_SETUP_ANDX:
	    case SMB_COM_LOGOFF_ANDX:
	    case SMB_COM_TREE_CONNECT_ANDX:
		return EPERM;
	}
	/* Take the 32 bit world pointers and convert them to user_addr_t. */
	if (! vfs_context_is64bit (context)) {
		dp->ioc_kern_twords = CAST_USER_ADDR_T(dp->ioc_twords);
		dp->ioc_kern_tbytes = CAST_USER_ADDR_T(dp->ioc_tbytes);
		dp->ioc_kern_rpbuf = CAST_USER_ADDR_T(dp->ioc_rpbuf);
	}
	
	error = smb_rq_init(rqp, SSTOCP(share), dp->ioc_cmd, dp->ioc_flags2, context);
	if (error)
		return error;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	error = mb_put_user_mem(mbp, dp->ioc_kern_twords, dp->ioc_twc * 2, 0, context);
	if (error)
		goto bad;
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	error = mb_put_user_mem(mbp, dp->ioc_kern_tbytes, dp->ioc_tbc, 0, context);
	if (error)
		goto bad;
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	if (error)
		goto bad;
	mdp = &rqp->sr_rp;
	/* 
	 * Amount of data left in the response buffer. 
	 * Should include size of word count field + any word count data + size of 
	 * byte count field + any byte count data 
	 */
	response_size = (int32_t)md_get_size(mdp);
	/* Make sure we have room in the users buffer */
	if (response_size > dp->ioc_rpbufsz) {
		error = EBADRPC;
		goto bad;
	}
	/* Copy the response into the users buffer */
	error = md_get_user_mem(mdp, dp->ioc_kern_rpbuf, response_size, 0, context);
	if (error)
		goto bad;

	dp->ioc_rpbufsz = response_size;
	
bad:
	/*
	 * Returning an error to the IOCTL code means no other information in the
	 * structure will be updated. The nsmb_dev_ioctl routine should only return 
	 * unexpected internal errors. We now always return the nt status code and
	 * the errno in the ioc structure. Any internal smb error is store in the errno
	 * field or if we get a nt status code then it holds the mapped error number.
	 */
	dp->ioc_ntstatus = rqp->sr_ntstatus;	
	dp->ioc_errno = error; 
	dp->ioc_flags = rqp->sr_rpflags;
	dp->ioc_flags2 = rqp->sr_rpflags2;
	smb_rq_done(rqp);
	return 0;

}

static int 
smb_cpdatain(struct mbchain *mbp, user_addr_t data, int len, vfs_context_t context)
{
	int error;

	if (len == 0)
		return 0;
	error = mb_init(mbp);
	if (! error)	
		error = mb_put_user_mem(mbp, data, len, 0, context);
	return error;
}

/*
* Called from user land so we always have a reference on the share.
*/
int
smb_usr_t2request(struct smb_share *share, struct smbioc_t2rq *dp, vfs_context_t context)
{
	struct smb_t2rq t2, *t2p = &t2;
	struct mdchain *mdp;
	int error;
	uint16_t len;

	if (dp->ioc_setupcnt > SMB_MAXSETUPWORDS)
		return EINVAL;
	
	error = smb_t2_init(t2p, SSTOCP(share), dp->ioc_setup, dp->ioc_setupcnt, context);
	if (error) {
		return error;
	}
	
	len = t2p->t2_setupcount = dp->ioc_setupcnt;
	if (len > 1)
		t2p->t2_setupdata = dp->ioc_setup; 

	/* Take the 32 bit world pointers and convert them to user_addr_t. */
	if (! vfs_context_is64bit (context)) {
		dp->ioc_kern_name = CAST_USER_ADDR_T(dp->ioc_name);
		dp->ioc_kern_tparam = CAST_USER_ADDR_T(dp->ioc_tparam);
		dp->ioc_kern_tdata = CAST_USER_ADDR_T(dp->ioc_tdata);
		dp->ioc_kern_rparam = CAST_USER_ADDR_T(dp->ioc_rparam);
		dp->ioc_kern_rdata = CAST_USER_ADDR_T(dp->ioc_rdata);
	}
	
	/* ioc_name_len includes the null byte, ioc_kern_name is a c-style string */
	if (dp->ioc_kern_name && dp->ioc_name_len) {
		t2p->t_name = smb_memdupin(dp->ioc_kern_name, dp->ioc_name_len);
		if (t2p->t_name == NULL) {
			error = ENOMEM;
			goto bad;
		}
	}
	t2p->t2_maxscount = 0;
	t2p->t2_maxpcount = dp->ioc_rparamcnt;
	t2p->t2_maxdcount = dp->ioc_rdatacnt;
	
	error = smb_cpdatain(&t2p->t2_tparam, dp->ioc_kern_tparam, dp->ioc_tparamcnt, context);
	if (! error)
		error = smb_cpdatain(&t2p->t2_tdata, dp->ioc_kern_tdata, dp->ioc_tdatacnt, context);
	if (error) {
		goto bad;
	}
	
	error = smb_t2_request(t2p);
	/*
	 * We now convert all non SMB_FLAGS2_ERR_STATUS errors into ntstatus 
	 * error codes, we only deal with ntstatus error here.
	 */
	dp->ioc_ntstatus = t2p->t2_ntstatus;		
	dp->ioc_flags2 = t2p->t2_sr_rpflags2;
	if (error) {
		goto bad;
	}
	mdp = &t2p->t2_rparam;
	if (mdp->md_top) {
		len = m_fixhdr(mdp->md_top);
		if (len > dp->ioc_rparamcnt) {
			error = EMSGSIZE;
			goto bad;
		}
		dp->ioc_rparamcnt = len;
		error = md_get_user_mem(mdp, dp->ioc_kern_rparam, len, 0, context);
		if (error) {
			goto bad;
		}
	} else
		dp->ioc_rparamcnt = 0;
	mdp = &t2p->t2_rdata;
	if (mdp->md_top) {
		len = m_fixhdr(mdp->md_top);
		if (len > dp->ioc_rdatacnt) {
			error = EMSGSIZE;
			goto bad;
		}
		dp->ioc_rdatacnt = len;
		error = md_get_user_mem(mdp, dp->ioc_kern_rdata, len, 0, context);
	} else
		dp->ioc_rdatacnt = 0;
bad:
	SMB_FREE(t2p->t_name, M_SMBSTR);
	smb_t2_done(t2p);
	return error;
}

/*
 * Converrts UTF8 string to a network style STRING. The network STRING returned
 * may be ASCII or UTF16, depending on what was negotiated with the server. The
 * lower level routines will handle any byte swapping issue and will set the 
 * precomosed flag. The only flag support currently is UTF_SFM_CONVERSIONS. 
 */
int smb_usr_convert_path_to_network(struct smb_vc *vc, struct smbioc_path_convert * dp)
{
	size_t ntwrk_len = (size_t)dp->ioc_dest_len;
	char *network = NULL;
	char *utf8str = NULL;
	int error;

	utf8str = smb_memdupin(dp->ioc_kern_src, dp->ioc_src_len);
	if (utf8str) {
        SMB_MALLOC(network, char *, ntwrk_len, M_SMBSTR, M_WAITOK | M_ZERO);
    }
		
	if ((utf8str == NULL) || (network == NULL)) {
		error = ENOMEM;
		goto done;
	}
	error = smb_convert_path_to_network(utf8str, dp->ioc_src_len, network, &ntwrk_len, 
										'\\', (int)dp->ioc_flags, SMB_UNICODE_STRINGS(vc));
	if (error) {
		SMBERROR("converter failed : %d\n", error);
		SMBDEBUG("utf8str = %s src len = %d dest len = %d\n", utf8str,
				 (int)dp->ioc_src_len, (int)dp->ioc_dest_len);
		goto done;
	}
	
	error = copyout(network, dp->ioc_kern_dest, ntwrk_len);
	if (error) {
		SMBERROR("copyout failed : %d\n", error);
		smb_hexdump(__FUNCTION__, "dest buffer: ", (u_char *)network, ntwrk_len);
		goto done;
	}
	
	dp->ioc_dest_len = (uint32_t)ntwrk_len;
	
done:	
	if (utf8str)
		SMB_FREE(utf8str, M_SMBSTR);
	if (network)
		SMB_FREE(network, M_SMBSTR);
	return error;
}

/*
 * Converts a network style STRING to a UTF8 string. The network STRING can be
 * formatted as an ASCII or UTF16 string. If the network STRING is in UTF16 format 
 * then this routine expects it to be decomosed. The only flag support currently 
 * is UTF_SFM_CONVERSIONS.
 */
int smb_usr_convert_network_to_path(struct smb_vc *vc, struct smbioc_path_convert * dp)
{
	size_t utf8str_len = (size_t)dp->ioc_dest_len;
	char *network = NULL;
	char *utf8str = NULL;
	int error;
	
	network = smb_memdupin(dp->ioc_kern_src, dp->ioc_src_len);
	if (network) {
        SMB_MALLOC(utf8str, char *, utf8str_len, M_SMBSTR, M_WAITOK | M_ZERO);
	}
	
	if ((utf8str == NULL) || (network == NULL)) {
		error = ENOMEM;
		goto done;
	}
	
	error = smb_convert_network_to_path(network, dp->ioc_src_len, utf8str, 
										&utf8str_len, '\\', (int)dp->ioc_flags, 
										SMB_UNICODE_STRINGS(vc));
	if (error) {
		SMBERROR("converter failed : %d\n", error);
		smb_hexdump(__FUNCTION__, "source buffer: ", (u_char *)network, dp->ioc_src_len);
		goto done;
	}
	
	error = copyout(utf8str, dp->ioc_kern_dest, utf8str_len);
	if (error) {
		SMBERROR("copyout failed : %d\n", error);
		SMBDEBUG("utf8str = %s src len = %d dest len = %d\n", utf8str,
				 (int)dp->ioc_src_len, (int)dp->ioc_dest_len);
		goto done;
	}
	dp->ioc_dest_len = (uint32_t)utf8str_len;
	
done:
	if (utf8str)
		SMB_FREE(utf8str, M_SMBSTR);
	if (network)
		SMB_FREE(network, M_SMBSTR);
	return error;
}

/*
 * They are setting the network user identity that was obtain by lsa. Currently
 * we only support adding the sid in the future we may want the account name
 * and domian name.
 */
int smb_usr_set_network_identity(struct smb_vc *vcp, struct smbioc_ntwrk_identity *ntwrkID)
{
	if (vcp->vc_flags & SMBV_NETWORK_SID) {
		/* Currently we only allow it to be set once */
		return EEXIST;
	}
	
	if (ntwrkID->ioc_ntsid_len != sizeof(ntsid_t)) {
		/* Needs to be the correct size */
		return EINVAL;
	}
	memcpy(&vcp->vc_ntwrk_sid, &ntwrkID->ioc_ntsid, (size_t)ntwrkID->ioc_ntsid_len);
	vcp->vc_flags |= SMBV_NETWORK_SID;
	return 0;
}

/*
 * Called from user land so we always have a reference on the share.
 */
int 
smb_usr_fsctl(struct smb_share *share, struct smbioc_fsctl *fsctl, vfs_context_t context)
{
	struct smb_ntrq * ntp = NULL;
	int error = 0;

	if (fsctl->ioc_tdatacnt > INT_MAX) {
		error = EINVAL;
		goto done;
	}

	fsctl->ioc_errno = 0;
	fsctl->ioc_ntstatus = 0;

	/* Take the 32 bit world pointers and convert them to user_addr_t. */
	if (! vfs_context_is64bit (context)) {
		fsctl->ioc_kern_tdata = CAST_USER_ADDR_T(fsctl->ioc_tdata);
		fsctl->ioc_kern_rdata = CAST_USER_ADDR_T(fsctl->ioc_rdata);
	}

	error = smb_nt_alloc(SSTOCP(share), NT_TRANSACT_IOCTL, context, &ntp);
	if (error) {
		goto done;
	}

	/* The NT_TRANSACT_IOCTL setup structure is:
	 *		uint32_t	FunctionCode
	 *		uint16_t	FID
	 *		uint8_t		IsFctl
	 *		uint8_t		IsFlags
	 */
	mb_init(&ntp->nt_tsetup);
	mb_put_uint32le(&ntp->nt_tsetup, fsctl->ioc_fsctl);
	mb_put_uint16le(&ntp->nt_tsetup, fsctl->ioc_fh);
	mb_put_uint8(&ntp->nt_tsetup, 1);
	mb_put_uint8(&ntp->nt_tsetup, 0);

	/* The fsctl arguments go in the transmit data. */
	if (fsctl->ioc_tdatacnt) {
		mb_init(&ntp->nt_tdata);
		error = mb_put_user_mem(&ntp->nt_tdata, fsctl->ioc_kern_tdata,
							fsctl->ioc_tdatacnt, 0, context);
		if (error) {
			goto done;
		}
	}

	/* We don't expect any returned params from NT_TRANSACT_IOCTL. */
	ntp->nt_maxpcount = 0;
	ntp->nt_maxdcount = fsctl->ioc_rdatacnt;

	fsctl->ioc_errno = smb_nt_request(ntp);
	fsctl->ioc_ntstatus = ntp->nt_status;

	if (fsctl->ioc_errno) {
		error = 0;
		goto done;
	}

	/* The data buffer wasn't big enough. Caller will have to retry. */
	if (ntp->nt_flags & SMBT2_MOREDATA) {
		fsctl->ioc_errno = ENOSPC;
		error = 0;
		goto done;
	}

	if (ntp->nt_rdata.md_top) {
		size_t datalen = m_fixhdr(ntp->nt_rdata.md_top);
		if (datalen > fsctl->ioc_rdatacnt) {
			SMBERROR("datalen(%u) > ioc_rdatacnt(%u)",
					(unsigned)datalen, (unsigned)fsctl->ioc_rdatacnt);
			error = EMSGSIZE;
			goto done;
		}

		fsctl->ioc_rdatacnt = (uint32_t)datalen;
		error = md_get_user_mem(&ntp->nt_rdata, fsctl->ioc_kern_rdata,
								(uint32_t)datalen, 0, context);
		if (error) {
			SMBERROR("md_get_user_mem failed with %d", error);
			goto done;
		}
	} else {
		fsctl->ioc_rdatacnt = 0;
	}

done:
	if (ntp) {
		smb_nt_done(ntp);
	}

	return error;
}

