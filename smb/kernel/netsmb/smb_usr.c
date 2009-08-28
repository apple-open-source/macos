/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2009 Apple Inc. All rights reserved.
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

int smb_usr_negotiate(struct smbioc_negotiate *vspec, vfs_context_t context, 
					  struct smb_dev *sdp)
{
	struct smb_vc *vcp;
	int error;

	/* Convert any pointers over to using user_addr_t */
	if (! vfs_context_is64bit (context)) {
		vspec->ioc_kern_saddr = CAST_USER_ADDR_T(vspec->ioc_saddr);
		vspec->ioc_kern_laddr = CAST_USER_ADDR_T(vspec->ioc_laddr);
	}
	/* Now do the real work */
	error = smb_sm_negotiate(vspec, context, &sdp->sd_vc, sdp);
	if (error)
		return error;

	vcp = sdp->sd_vc;
	/* Return to the user the server's capablilities */
	vspec->ioc_ret_caps = vcp->vc_sopt.sv_caps;
	/* Return to the user the vc flags */
	vspec->ioc_ret_vc_flags = vcp->vc_flags;
	
	/* We are sharing the vc return the user name if there is one */
	if (vspec->ioc_extra_flags & SMB_SHARING_VC) {
		if (vcp->vc_username)
			strlcpy(vspec->ioc_user, vcp->vc_username, sizeof(vspec->ioc_user));
		/* See if we have a kerberos username to return */
		smb_get_username_from_kcpn(vcp, vspec->ioc_ssn.ioc_kuser, 
								   sizeof(vspec->ioc_ssn.ioc_kuser));
	}
		
	if ((vcp->vc_flags & SMBV_MECHTYPE_KRB5) && vcp->vc_gss.gss_spn && 
		(vcp->vc_gss.gss_spnlen < sizeof(vspec->ioc_ssn.ioc_kspn_hint))) {
		bcopy(vcp->vc_gss.gss_spn, vspec->ioc_ssn.ioc_kspn_hint, vcp->vc_gss.gss_spnlen);
	}
	
	return error;
}

/*
 * Connect to the resource specified by smbioc_ossn structure.
 * It may either find an existing connection or try to establish a new one.
 * If no errors occured smb_vc returned locked and referenced.
 */

int
smb_usr_simplerequest(struct smb_share *ssp, struct smbioc_rq *dp, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	u_int16_t bc;
	int error;

	switch (dp->ioc_cmd) {
	    case SMB_COM_TRANSACTION2:
	    case SMB_COM_TRANSACTION2_SECONDARY:
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
	
	error = smb_rq_init(rqp, SSTOCP(ssp), dp->ioc_cmd, context);
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
	md_get_uint8(mdp, &wc);
	dp->ioc_rwc = wc;
	wc *= 2;
	if (wc > dp->ioc_rpbufsz) {
		error = EBADRPC;
		goto bad;
	}
	error = md_get_user_mem(mdp, dp->ioc_kern_rpbuf, wc, 0, context);
	if (error)
		goto bad;
	md_get_uint16le(mdp, &bc);
	if ((wc + bc) > dp->ioc_rpbufsz) {
		error = EBADRPC;
		goto bad;
	}
	dp->ioc_rbc = bc;
	error = md_get_user_mem(mdp, dp->ioc_kern_rpbuf+wc, bc, 0, context);

bad:
	if (rqp->sr_rpflags2 & SMB_FLAGS2_ERR_STATUS) {
		dp->ioc_nt_error = rqp->sr_error;		
	} else {
		dp->ioc_dos_error.errclass = rqp->sr_errclass;
		dp->ioc_dos_error.error = rqp->sr_serror;
	}
	dp->ioc_srflags2 = rqp->sr_rpflags2;
	smb_rq_done(rqp);
	return error;

}

static int smb_cpdatain(struct mbchain *mbp, user_addr_t data, int len, vfs_context_t context)
{
	int error;

	if (len == 0)
		return 0;
	error = mb_init(mbp);
	if (! error)	
		error = mb_put_user_mem(mbp, data, len, 0, context);
	return error;
}

int
smb_usr_t2request(struct smb_share *ssp, struct smbioc_t2rq *dp, vfs_context_t context)
{
	struct smb_t2rq t2, *t2p = &t2;
	struct mdchain *mdp;
	int error;
	u_int16_t len;

	if (dp->ioc_setupcnt > SMB_MAXSETUPWORDS)
		return EINVAL;
	
	error = smb_t2_init(t2p, SSTOCP(ssp), dp->ioc_setup, dp->ioc_setupcnt, context);
	if (error)
		return error;
	
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
	if (error)
		goto bad;

	error = smb_t2_request(t2p);
	if (t2p->t2_sr_rpflags2 & SMB_FLAGS2_ERR_STATUS) {
		dp->ioc_nt_error = t2p->t2_sr_error;		
	} else {
		dp->ioc_dos_error.errclass = t2p->t2_sr_errclass;
		dp->ioc_dos_error.error = t2p->t2_sr_error;
	}
	dp->ioc_srflags2 = t2p->t2_sr_rpflags2;
	if (error)
		goto bad;
	mdp = &t2p->t2_rparam;
	if (mdp->md_top) {
		len = m_fixhdr(mdp->md_top);
		if (len > dp->ioc_rparamcnt) {
			error = EMSGSIZE;
			goto bad;
		}
		dp->ioc_rparamcnt = len;
		error = md_get_user_mem(mdp, dp->ioc_kern_rparam, len, 0, context);
		if (error)
			goto bad;
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
	SMB_STRFREE(t2p->t_name);
	smb_t2_done(t2p);
	return error;
}
