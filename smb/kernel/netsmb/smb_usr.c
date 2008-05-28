/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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

#include <sys/smb_apple.h>

#include <sys/smb_iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

static void
smb_usr_vcspec_free(struct smb_vcspec *spec)
{
	if (spec->sap)
		smb_memfree(spec->sap);
	spec->sap = NULL;
	if (spec->lap)
		smb_memfree(spec->lap);
	spec->lap = NULL;
}

static int
smb_usr_vc2spec(struct smbioc_ossn *dp, struct smb_vcspec *spec)
{
	bzero(spec, sizeof(*spec));
	if (!dp->ioc_kern_server)
		return EINVAL;
	if (dp->ioc_localcs[0] == 0) {
		SMBERROR("no local charset ?\n");
		return EINVAL;
	}
	/*
	 * Most likely something went bad in user land, just get out. 
	 */
	if (dp->ioc_svlen < sizeof(*spec->sap)) {
		SMBERROR("server name's length is zero ?\n");
		return EINVAL;
	}
	spec->sap = smb_memdupin(dp->ioc_kern_server, dp->ioc_svlen);
	if (spec->sap == NULL)
		return ENOMEM;
	if (dp->ioc_kern_local) {
		spec->lap = smb_memdupin(dp->ioc_kern_local, dp->ioc_lolen);
		if (spec->lap == NULL) {
			smb_usr_vcspec_free(spec);
			return ENOMEM;
		}
	}

	spec->srvname = dp->ioc_srvname;
	spec->pass = dp->ioc_password;
	spec->domain = dp->ioc_domain;
	spec->username = dp->ioc_user;
	spec->mode = dp->ioc_mode;
	spec->rights = dp->ioc_rights;
	spec->owner = dp->ioc_owner;
	spec->group = dp->ioc_group;
	
	spec->reconnect_wait_time = dp->ioc_reconnect_wait_time;

	spec->localcs = dp->ioc_localcs;
	spec->flags = dp->ioc_opt;
	return 0;
}

static void smb_usr_share2spec(struct smbioc_oshare *dp, struct smb_sharespec *spec)
{
	bzero(spec, sizeof(*spec));
	spec->mode = dp->ioc_mode;
	spec->rights = dp->ioc_rights;
	spec->owner = dp->ioc_owner;
	spec->group = dp->ioc_group;
	spec->name = dp->ioc_share;
	spec->stype = dp->ioc_stype;
	spec->pass = dp->ioc_password;
}


int
smb_usr_negotiate(struct smbioc_negotiate *dp, struct smb_cred *scred, struct smb_vc **vcpp, struct smb_dev *sdp)
{
	struct smb_vc *vcp = NULL;
	struct smb_vcspec vspec;
	int error;

	error = smb_usr_vc2spec(&dp->ioc_ssn, &vspec);
	if (error)
		return error;
	/* Do they want us to try both ports while connecting */
	sdp->sd_flags |= (dp->vc_conn_state & NSMBFL_TRYBOTH);
	error = smb_sm_negotiate(&vspec, scred, &vcp, sdp);
	/* Clear it out we don't want this to happen during reconnect */
	sdp->sd_flags &= ~(dp->vc_conn_state & NSMBFL_TRYBOTH);
	if (error)
		goto out;

	*vcpp = vcp;
		/* Return to the user the server's capablilities */
	dp->vc_caps = vcp->vc_sopt.sv_caps;
	dp->flags = vcp->vc_flags;
	dp->spn_len = 0;
	/* We are sharing the vc return the user name if there is one */
	dp->vc_conn_state = (sdp->sd_flags & NSMBFL_SHAREVC);
	if ((sdp->sd_flags & NSMBFL_SHAREVC) && vcp->vc_username && (dp->ioc_ssn.ioc_user[0] == 0))
		strlcpy(dp->ioc_ssn.ioc_user, vcp->vc_username, SMB_MAXUSERNAMELEN + 1);
		
	if ((vcp->vc_flags & SMBV_KERBEROS_SUPPORT) && vcp->vc_gss.gss_spn && (vcp->vc_gss.gss_spnlen < sizeof(dp->spn))) {
		bcopy(vcp->vc_gss.gss_spn, dp->spn, vcp->vc_gss.gss_spnlen);
		dp->spn_len = vcp->vc_gss.gss_spnlen;	
		smb_get_username_from_kcpn(vcp, dp->ioc_ssn.ioc_kuser);
	}
	
out: 
	smb_usr_vcspec_free(&vspec);
	return error;
}

int
smb_usr_ssnsetup(struct smbioc_ssnsetup *dp, struct smb_cred *scred, struct smb_vc *vcp)
{
	struct smb_vcspec vspec;
	int error;

	error = smb_usr_vc2spec(&dp->ioc_ssn, &vspec);
	if (error)
		return error;

	error = smb_sm_ssnsetup(dp, &vspec, scred, vcp);

	smb_usr_vcspec_free(&vspec);
	return error;
}


int
smb_usr_tcon(struct smbioc_treeconn *dp, struct smb_cred *scred,
	struct smb_vc *vcp, struct smb_share **sspp)
{
	struct smb_vcspec vspec;
	struct smb_sharespec sspec, *sspecp = NULL;
	int error;

	error = smb_usr_vc2spec(&dp->ioc_ssn, &vspec);
	if (error)
		return error;
	
	smb_usr_share2spec(&dp->ioc_sh, &sspec);
	sspecp = &sspec;
	
	error = smb_sm_tcon(&vspec, sspecp, scred, vcp);
	if (error == 0)
		*sspp = vspec.ssp;

	smb_usr_vcspec_free(&vspec);
	return error;
}

/*
 * Connect to the resource specified by smbioc_ossn structure.
 * It may either find an existing connection or try to establish a new one.
 * If no errors occured smb_vc returned locked and referenced.
 */

int
smb_usr_simplerequest(struct smb_share *ssp, struct smbioc_rq *dp, struct smb_cred *scred)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	u_int16_t bc;
	int error;
	vfs_context_t context =  scred->scr_vfsctx;

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
	error = smb_rq_init(rqp, SSTOCP(ssp), dp->ioc_cmd, scred);
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
	error = md_get_user_mem(mdp, dp->ioc_kern_rpbuf, bc, wc, context);
bad:
	dp->ioc_errclass = rqp->sr_errclass;
	dp->ioc_serror = rqp->sr_serror;
	dp->ioc_error = rqp->sr_error;
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
smb_usr_t2request(struct smb_share *ssp, struct smbioc_t2rq *dp, struct smb_cred *scred)
{
	struct smb_t2rq t2, *t2p = &t2;
	struct mdchain *mdp;
	int error, len;
	vfs_context_t context =  scred->scr_vfsctx;

	if (dp->ioc_setupcnt > SMB_MAXSETUPWORDS)
		return EINVAL;
	error = smb_t2_init(t2p, SSTOCP(ssp), dp->ioc_setup, dp->ioc_setupcnt, scred);
	if (error)
		return error;
	len = t2p->t2_setupcount = dp->ioc_setupcnt;
	if (len > 1)
		t2p->t2_setupdata = dp->ioc_setup; 
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
	dp->ioc_errclass = t2p->t2_sr_errclass;
	dp->ioc_serror = t2p->t2_sr_serror;
	dp->ioc_error = t2p->t2_sr_error;
	dp->ioc_rpflags2 = t2p->t2_sr_rpflags2;
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
	if (t2p->t_name)
		smb_strfree(t2p->t_name);
	smb_t2_done(t2p);
	return error;
}
