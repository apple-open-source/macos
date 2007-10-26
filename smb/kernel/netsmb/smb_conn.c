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

/*
 * Connection engine.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <sys/kauth.h>

#include <sys/smb_apple.h>

#include <sys/smb_iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>
#include <netsmb/smb_gss.h>

static struct smb_connobj smb_vclist;
static int smb_vcnext = 1;	/* next unique id for VC */

extern struct linker_set sysctl_net_smb;

SYSCTL_DECL(_net_smb);

SYSCTL_NODE(_net, OID_AUTO, smb, CTLFLAG_RW, NULL, "SMB protocol");

MALLOC_DEFINE(M_SMBCONN, "SMB conn", "SMB connection");

static void smb_co_init(struct smb_connobj *cp, int level, char *objname,
	struct proc *p);
static void smb_co_done(struct smb_connobj *cp);

static int  smb_vc_disconnect(struct smb_vc *vcp);
static void smb_vc_free(struct smb_connobj *cp);
static void smb_vc_gone(struct smb_connobj *cp, struct smb_cred *scred);
static smb_co_free_t smb_share_free;
static smb_co_gone_t smb_share_gone;

/* XXX This function replicates dup_sockaddr.  An SPI would be wiser. */
struct sockaddr *
smb_dup_sockaddr(struct sockaddr *sa, int canwait)
{
	struct sockaddr *sa2;

	MALLOC(sa2, struct sockaddr *, sa->sa_len, M_SONAME,
	       canwait ? M_WAITOK : M_NOWAIT);
	if (sa2)
		bcopy(sa, sa2, sa->sa_len);
	return (sa2);
}

int
smb_sm_init(void)
{
	smb_co_init(&smb_vclist, SMBL_SM, "smbsm", current_proc());
	smb_co_unlock(&smb_vclist);
	return (0);
}

int
smb_sm_done(void)
{
	/* XXX: hold the mutex */
	if (smb_vclist.co_usecount > 1) {
		SMBERROR("%d connections still active\n", smb_vclist.co_usecount - 1);
		return (EBUSY);
	}
	/* XXX Q4BP why are we not iterating on smb_vclist here with SMBCO_FOREACH? */
	smb_co_done(&smb_vclist);
	return (0);
}

static void
smb_sm_lockvclist()
{
  	/*
	 * The smb_vclist never goes away so there is no way for smb_co_lock
	 * to fail in this case. 
	 */	
	KASSERT((smb_co_lock(&smb_vclist) == 0), ("smb_sm_lockvclist: lock failed"));
}

static void
smb_sm_unlockvclist()
{
	smb_co_unlock(&smb_vclist);
}

/*
 * On success the vc will have a reference taken and a lock.
 *
 * Only smb_sm_negotiate passes *vcpp as null, all other routines
 * need to pass in a vcp to search on.
 */
static int
smb_sm_lookupint(struct smb_vcspec *vcspec, struct smb_sharespec *shspec, struct smb_cred *scred, struct smb_vc **vcpp)
{
	struct smb_vc *vcp;
	int error;
	int negotiate;
	

	DBG_ASSERT(vcpp);	/* Better have passed us a vcpp */
	negotiate = (*vcpp == NULL) ? TRUE : FALSE;
	vcspec->shspec = shspec;
tryagain:
	smb_sm_lockvclist();
	error = ENOENT;
	SMBCO_FOREACH(vcp, &smb_vclist) {
		
		if (*vcpp && vcp != *vcpp)
			continue;
		itry {
			/* Address and port need to match */
			if (!CONNADDREQ(vcp->vc_paddr, vcspec->sap))
				ithrow(EPERM);
			
			/* Must be the same user */
			if (vcp->vc_uid != vcspec->owner)
				ithrow(EPERM);
			
			/* Private vc session, don't share this session */
			if (negotiate && (vcp->vc_flags & SMBV_PRIVATE_VC))
				ithrow(EPERM);				
				/* Ok we have to lock the vc now, so any error after this must unlock the vc */
			error = smb_vc_lock(vcp);
			/*
			 * This VC is going away, but is currently block on the lock we hold for smb_vclist. We need to unlock
			 * the list and allow the VC to be remove. This still may not be the VC we were looking for so start the
			 * search again.
			 */
			if (error) {
				smb_sm_unlockvclist();
				goto tryagain;
			}			
			/* Called from negotiate make sure the remote user name matches, if they gave us one */
			if (negotiate && (!(vcspec->flags & SMBV_GUEST_ACCESS)) &&  (vcspec->username[0]) && vcp->vc_username) {
				if ((strncmp(vcp->vc_username, vcspec->username, SMB_MAXUSERNAMELEN + 1)) != 0) {
					smb_vc_unlock(vcp);									
					ithrow(EPERM);
				}
			}
			vcspec->ssp = NULL;
			if (shspec) {
				error = smb_vc_lookupshare(vcp, shspec, scred, &vcspec->ssp);
				if (error)
					smb_vc_unlock(vcp);				
				ithrow(error);
			}
			error = 0;
			break;
		} icatch(error) {
			if (*vcpp)
				break;
		} ifinally {
		} iendtry;
		if (error == 0)
			break;
	}
	if (vcp && !error) {
		smb_vc_ref(vcp);
		*vcpp = vcp;
	}
	smb_sm_unlockvclist();
	return error;
}

int
smb_sm_negotiate(struct smb_vcspec *vcspec, struct smb_cred *scred, struct smb_vc **vcpp, struct smb_dev *sdp)
{
	struct smb_vc *vcp;
	int error;

	*vcpp = vcp = NULL;

	error = ENOENT;
	if ((vcspec->flags & SMBV_PRIVATE_VC) != SMBV_PRIVATE_VC)
		error = smb_sm_lookupint(vcspec, NULL, scred, &vcp);
	/* Either we didn't find it or we were asked to create a new one */
	if (error) {
		error = smb_vc_create(vcspec, scred, &vcp);
		if (error == 0) {
			/* Flags used to cancel the connection */
			vcp->connect_flag = &sdp->sd_flags;
			error = smb_vc_negotiate(vcp, scred);
			vcp->connect_flag = NULL;
		}
	} else
		sdp->sd_flags |= NSMBFL_SHAREVC;	/* Mark that we are sharing the vc */

	if (error == 0)
		*vcpp = vcp;
	else if (vcp)
		smb_vc_put(vcp, scred);
	return error;
}


/*
 * Session implementation
 */

static int smb_vc_setup(struct smb_vcspec *vcspec, struct smb_vc *vcp)
{
	char *domain = (vcspec->domain) ? vcspec->domain : "";
	int error = 0;

	/* Reset any user flag changes */
	vcp->vc_flags &= ~SMBV_USER_LAND_MASK; /* Remove any user setable items */
	vcp->vc_flags |= (vcspec->flags & SMBV_USER_LAND_MASK); /* Now add the users setable items */
	/* Reset the password, domain and username */
	if (vcp->vc_pass)
		free(vcp->vc_pass, M_SMBTEMP);
	if (vcp->vc_domain)
		free(vcp->vc_domain, M_SMBTEMP);
	if (vcp->vc_username)
		free(vcp->vc_username, M_SMBTEMP);
	vcp->vc_username = vcp->vc_domain = vcp->vc_pass = NULL;
	
	vcp->vc_pass = smb_strdup(vcspec->pass);
	vcp->vc_domain = smb_strdup(domain);
	vcp->vc_username = smb_strdup(vcspec->username);
	if ((vcp->vc_pass == NULL) || (vcp->vc_domain == NULL) || (vcp->vc_username == NULL))
		error = ENOMEM;
	return error;
}

int
smb_sm_ssnsetup(struct smbioc_ssnsetup *dp, struct smb_vcspec *vcspec, struct smb_cred *scred, struct smb_vc *vcp)
{
	int error;

	error = smb_sm_lookupint(vcspec, NULL, scred, &vcp);
	if (error)
		return (error);

	if ((vcp->vc_flags & SMBV_AUTH_DONE) != SMBV_AUTH_DONE) {
		error = smb_vc_setup(vcspec, vcp);
		if (!error) {
			/* kern_clientpn is only set if we are doing kerberos */
			if (dp->kern_clientpn && dp->clientpn_len) {
				if (vcp->vc_gss.gss_cpn)	/* Free any memory we already have */
					smb_memfree(vcp->vc_gss.gss_cpn);
				vcp->vc_gss.gss_cpn = smb_memdupin(dp->kern_clientpn, dp->clientpn_len);
			}
			/* kern_servicepn are only set if we are doing kerberos */
			if (dp->kern_servicepn && dp->servicepn_len) {
				if (vcp->vc_gss.gss_spn)	/* Free any memory we already have */
					smb_memfree(vcp->vc_gss.gss_spn);
				vcp->vc_gss.gss_spn = smb_memdupin(dp->kern_servicepn, dp->servicepn_len);
			}
			error = smb_vc_ssnsetup(vcp, scred);
		}
		if (error) {
			/* 
			 * Authorization failed, reset any authorization
			 * information. This includes removing guest access,
			 * user name, password and the domain name. We should
			 * not every return these values after authorization
			 * fails.
			 */ 
			vcp->vc_flags &= ~SMBV_GUEST_ACCESS;
			if (vcp->vc_pass)
				free(vcp->vc_pass, M_SMBTEMP);
			if (vcp->vc_domain)
				free(vcp->vc_domain, M_SMBTEMP);
			if (vcp->vc_username)
				free(vcp->vc_username, M_SMBTEMP);
			vcp->vc_username = vcp->vc_domain = vcp->vc_pass = NULL;
		}
	}

	if (error)
		smb_vc_put(vcp, scred);
	else
		vcp->vc_flags |= SMBV_AUTH_DONE;
	return error;
}

int
smb_sm_tcon(struct smb_vcspec *vcspec, struct smb_sharespec *shspec,
	struct smb_cred *scred,	struct smb_vc *vcp)
{
	struct smb_share *ssp = NULL;
	int error;

	error = smb_sm_lookupint(vcspec, shspec, scred, &vcp);
	if (error == 0)
		return error;

	error = smb_sm_lookupint(vcspec, NULL, scred, &vcp);
	if (error || shspec == NULL)
		return (error);

	error = smb_share_create(vcp, shspec, scred, &ssp);
	if (!error) {
		error = smb_smb_treeconnect(ssp, scred);
		if (error)
			smb_share_put(ssp, scred);
		else
			vcspec->ssp = ssp;
	
	}
	if (error)
		smb_vc_put(vcp, scred);
	return error;
}

/*
 * Common code for connection object
 */
static void
smb_co_init(struct smb_connobj *cp, int level, char *objname, struct proc *p)
{
	SLIST_INIT(&cp->co_children);
	lck_mtx_init(&cp->co_interlock, co_lck_group, co_lck_attr);
	cp->co_lock	= lck_mtx_alloc_init(co_lck_group, co_lck_attr);
	cp->co_lock_flags = 0;
	cp->co_lockowner = 0;
	cp->co_lockcount = 0;

	cp->co_level = level;
	cp->co_usecount = 1;
	KASSERT(smb_co_lock(cp) == 0,
		("smb_co_init: lock failed"));
}

static void
smb_co_done(struct smb_connobj *cp)
{
	lck_mtx_destroy(&cp->co_interlock, co_lck_group);
	lck_mtx_free(cp->co_lock, co_lck_group);
	cp->co_lock = 0;
	cp->co_lock_flags = 0;
	cp->co_lockowner = 0;
	cp->co_lockcount = 0;
}

static void
smb_co_gone(struct smb_connobj *cp, struct smb_cred *scred)
{
	struct smb_connobj *parent;

	if (cp->co_gone)
		cp->co_gone(cp, scred);
	parent = cp->co_parent;
	if (parent) {
		if (smb_co_lock(parent)) {
			SMBERROR("unable to lock level %d\n", parent->co_level);
		} else {
			SLIST_REMOVE(&parent->co_children, cp, smb_connobj,
				     co_next);
			smb_co_put(parent, scred);
		}
	}
	if (cp->co_free)
		cp->co_free(cp);
}

void
smb_co_ref(struct smb_connobj *cp)
{
	SMB_CO_LOCK(cp);
	cp->co_usecount++;
	SMB_CO_UNLOCK(cp);
}

void
smb_co_rele(struct smb_connobj *cp, struct smb_cred *scred)
{

	SMB_CO_LOCK(cp);
	if (cp->co_usecount > 1) {
		cp->co_usecount--;
		SMB_CO_UNLOCK(cp);
		return;
	}
	if (cp->co_usecount == 0) {
		SMBERROR("negative co_usecount for level %d\n", cp->co_level);
		SMB_CO_UNLOCK(cp);
		return;
	}
	cp->co_usecount--;
	if (cp->co_flags & SMBO_GONE) {
		SMB_CO_UNLOCK(cp);
		return; /* someone is already draining */
	}
	cp->co_flags |= SMBO_GONE;
	SMB_CO_UNLOCK(cp);

	smb_co_drain(cp);
	smb_co_gone(cp, scred);
}

void
smb_co_put(struct smb_connobj *cp, struct smb_cred *scred)
{

	SMB_CO_LOCK(cp);
	if (cp->co_usecount > 1) {
		cp->co_usecount--;
	} else if (cp->co_usecount == 1) {
		cp->co_usecount--;
		cp->co_flags |= SMBO_GONE;
	} else {
		SMBERROR("negative usecount\n");
	}
	SMB_CO_UNLOCK(cp);
	smb_co_unlock(cp);
	if ((cp->co_flags & SMBO_GONE) == 0)
		return;
	smb_co_drain(cp);
	smb_co_gone(cp, scred);
}


int
smb_co_lock(struct smb_connobj *cp)
{

	if (cp->co_flags & SMBO_GONE)
		return EBUSY;
	if (cp->co_lockowner == current_thread()) {
		cp->co_lockcount++;
	} else  {
		lck_mtx_lock(cp->co_lock);
		/* We got the lock, but  the VC is going away, so unlock it return EBUSY */
		if (cp->co_flags & SMBO_GONE) {
			lck_mtx_unlock(cp->co_lock);
			return EBUSY;
		}
		cp->co_lockowner = current_thread();
		cp->co_lockcount = 1;
	}
	return (0);
}

void
smb_co_unlock(struct smb_connobj *cp)
{		
	if (cp->co_lockowner && (cp->co_lockowner != current_thread())) {
		SMBERROR("not owner of lock");
	} else if (cp->co_lockcount && (--cp->co_lockcount == 0)) {
		cp->co_lockowner = NULL;
		lck_mtx_unlock(cp->co_lock);
		/* the funnel protects the sleep/wakeup */
		if (cp->co_lock_flags & SMBFS_CO_LOCK_WAIT){
			cp->co_lock_flags &= ~SMBFS_CO_LOCK_WAIT;
			wakeup(&cp->co_lock);
		}
	}
}

void
smb_co_drain(struct smb_connobj *cp)
{
	/* the funnel protects the sleep/wakeup */
	while (cp->co_lockcount > 0) {
		cp->co_lock_flags |= SMBFS_CO_LOCK_WAIT;
		msleep(&cp->co_lock, 0, 0, 0, 0);
	}
	smb_co_lock(cp);
}

static void
smb_co_addchild(struct smb_connobj *parent, struct smb_connobj *child)
{
	smb_co_ref(parent);
	SLIST_INSERT_HEAD(&parent->co_children, child, co_next);
	child->co_parent = parent;
}

/*
 * This routine will reset the virtual circuit. When doing a reconnect we need to
 * keep some of the virtual circuit information around. We only reset the information
 * that is required to do the reconnect.
 */
void smb_vc_reset(struct smb_vc *vcp)
{
    	/* 
	 * If these two flags were set keep them for the reconnect. Clear out 
	 * any other flags that may have been set in the original connection. 
	 */
	vcp->vc_hflags2 &= (SMB_FLAGS2_EXT_SEC | SMB_FLAGS2_KNOWS_LONG_NAMES);

	vcp->vc_mid = 0;	/* Should we reset this value? */
	vcp->vc_number = smb_vcnext++;
	if (vcp->vc_toserver)
		iconv_close(vcp->vc_toserver);
	if (vcp->vc_tolocal)
		iconv_close(vcp->vc_tolocal);
	vcp->vc_toserver = NULL;
	vcp->vc_tolocal = NULL;
}

int
smb_vc_create(struct smb_vcspec *vcspec, struct smb_cred *scred, struct smb_vc **vcpp)
{
	struct smb_vc *vcp;
	int error;

	vcp = smb_zmalloc(sizeof(*vcp), M_SMBCONN, M_WAITOK);
	smb_co_init(VCTOCP(vcp), SMBL_VC, "smb_vc", vfs_context_proc(scred->scr_vfsctx));
	vcp->obj.co_free = smb_vc_free;
	vcp->obj.co_gone = smb_vc_gone;
	vcp->vc_number = smb_vcnext++;
	vcp->vc_timo = SMB_DEFRQTIMO;
	vcp->vc_smbuid = SMB_UID_UNKNOWN;
	vcp->vc_tdesc = &smb_tran_nbtcp_desc;
	vcp->vc_seqno = 0;
	vcp->vc_mackey = NULL;
	vcp->vc_mackeylen = 0;
	
	vcp->vc_mode = vcspec->rights & SMBM_MASK;
	vcp->vc_flags &= ~SMBV_USER_LAND_MASK; /* Remove any user setable items */
	vcp->vc_flags |= (vcspec->flags & SMBV_USER_LAND_MASK); /* Now add the users setable items */
	if (vcspec->flags & SMBV_EXT_SEC)
		vcp->vc_hflags2 |= SMB_FLAGS2_EXT_SEC;
	
	vcp->vc_uid = vcspec->owner;
	vcp->vc_grp = vcspec->group;
	/* gets disabled if nego response shows antique server!*/
	vcp->vc_hflags2 |= SMB_FLAGS2_KNOWS_LONG_NAMES;
	vcp->reconnect_wait_time = vcspec->reconnect_wait_time;		/* Amount of time to wait while reconnecting */

	lck_mtx_init(&vcp->vc_stlock, vcst_lck_group, vcst_lck_attr);
	error = 0;
	itry {
		vcp->vc_paddr = smb_dup_sockaddr(vcspec->sap, 1);
		ierror(vcp->vc_paddr == NULL, ENOMEM);

		vcp->vc_laddr = smb_dup_sockaddr(vcspec->lap, 1);
		ierror(vcp->vc_laddr == NULL, ENOMEM);

		ierror((vcp->vc_srvname = smb_strdup(vcspec->srvname)) == NULL, ENOMEM);

		ithrow(iconv_open("tolower", vcspec->localcs, &vcp->vc_tolower));
		ithrow(iconv_open("toupper", vcspec->localcs, &vcp->vc_toupper));
		if (vcspec->servercs[0]) {
			ithrow(iconv_open(vcspec->servercs, vcspec->localcs,
			    &vcp->vc_toserver));
			ithrow(iconv_open(vcspec->localcs, vcspec->servercs,
			    &vcp->vc_tolocal));
		}

		ithrow(smb_iod_create(vcp));
		*vcpp = vcp;
		smb_sm_lockvclist();
		smb_co_addchild(&smb_vclist, VCTOCP(vcp));
		smb_sm_unlockvclist();
	} icatch(error) {
		smb_vc_put(vcp, scred);
	} ifinally {
	} iendtry;
	return error;
}

static void
smb_vc_free(struct smb_connobj *cp)
{
	struct smb_vc *vcp = CPTOVC(cp);

	if (vcp->vc_iod)
		smb_iod_destroy(vcp->vc_iod);
	vcp->vc_iod = NULL;
	SMB_STRFREE(vcp->vc_username);
	SMB_STRFREE(vcp->vc_srvname);
	SMB_STRFREE(vcp->vc_pass);
	SMB_STRFREE(vcp->vc_domain);
	if (vcp->vc_mackey)
		free(vcp->vc_mackey, M_SMBTEMP);
	if (vcp->vc_paddr)
		free(vcp->vc_paddr, M_SONAME);
	if (vcp->vc_laddr)
		free(vcp->vc_laddr, M_SONAME);
	if (vcp->vc_tolower)
		iconv_close(vcp->vc_tolower);
	if (vcp->vc_toupper)
		iconv_close(vcp->vc_toupper);
	if (vcp->vc_tolocal)
		iconv_close(vcp->vc_tolocal);
	if (vcp->vc_toserver)
		iconv_close(vcp->vc_toserver);
	smb_gss_destroy(&vcp->vc_gss);
	smb_co_done(VCTOCP(vcp));
	lck_mtx_destroy(&vcp->vc_stlock, vcst_lck_group);
	free(vcp, M_SMBCONN);
}

/*
 * Called when use count of VC dropped to zero.
 * VC should be locked on enter with LK_DRAIN.
 */
static void
smb_vc_gone(struct smb_connobj *cp, struct smb_cred *scred)
{
	#pragma unused(scred)
	struct smb_vc *vcp = CPTOVC(cp);

	smb_vc_disconnect(vcp);
}

void
smb_vc_ref(struct smb_vc *vcp)
{
	smb_co_ref(VCTOCP(vcp));
}

void
smb_vc_rele(struct smb_vc *vcp, struct smb_cred *scred)
{
	smb_co_rele(VCTOCP(vcp), scred);
}

void
smb_vc_put(struct smb_vc *vcp, struct smb_cred *scred)
{
	smb_co_put(VCTOCP(vcp), scred);
}

int
smb_vc_lock(struct smb_vc *vcp)
{
	return smb_co_lock(VCTOCP(vcp));
}

void
smb_vc_unlock(struct smb_vc *vcp)
{
	smb_co_unlock(VCTOCP(vcp));
}

int
smb_vc_access(struct smb_vc *vcp, vfs_context_t context)
{
	kauth_cred_t cred = vfs_context_ucred(context);
	
	if (vcp->vc_flags & SMBV_GUEST_ACCESS)
		return(0);
	
	if (suser(cred, NULL) == 0 || kauth_cred_getuid(cred) == vcp->vc_uid)
		return (0);
	return (EACCES);
}

static int
smb_vc_cmpshare(struct smb_share *ssp, struct smb_sharespec *dp)
{
	if (strncmp(ssp->ss_name, dp->name, SMB_MAXUSERNAMELEN + 1) != 0)
		return 1;
	if (ssp->ss_uid != dp->owner)
		return 1;

	if (smb_share_access(ssp, dp->scred->scr_vfsctx) != 0)
		return 1;
	return (0);
}

/*
 * Look up the share on the given VC. If we find it then take a reference and a lock 
 * out on the share. The VC should to be locked on entry and will be left locked on exit.
 */
int
smb_vc_lookupshare(struct smb_vc *vcp, struct smb_sharespec *dp,
	struct smb_cred *scred,	struct smb_share **sspp)
{
	struct smb_share *ssp = NULL;
	int error;

	*sspp = NULL;
	dp->scred = scred;
	SMBCO_FOREACH(ssp, VCTOCP(vcp)) {
		error = smb_share_lock(ssp);
		if (error)
			continue;
		if (smb_vc_cmpshare(ssp, dp) == 0)
			break;
		smb_share_unlock(ssp);
	}
	if (ssp) {
		smb_share_ref(ssp);
		*sspp = ssp;
		error = 0;
	} else
		error = ENOENT;
	return error;
}


int
smb_vc_negotiate(struct smb_vc *vcp, struct smb_cred *scred)
{
	return smb_iod_request(vcp->vc_iod,
			       SMBIOD_EV_NEGOTIATE | SMBIOD_EV_SYNC, scred);
}

int
smb_vc_ssnsetup(struct smb_vc *vcp, struct smb_cred *scred)
{
	return smb_iod_request(vcp->vc_iod,
 			       SMBIOD_EV_SSNSETUP | SMBIOD_EV_SYNC, NULL);
}

/*
 * Destroy VC to server, invalidate shares linked with it.
 * Transport should be locked on entry.
 */
int
smb_vc_disconnect(struct smb_vc *vcp)
{
	if (vcp->vc_iod)
		smb_iod_request(vcp->vc_iod, SMBIOD_EV_DISCONNECT | SMBIOD_EV_SYNC, NULL);
	return (0);
}

static char smb_emptypass[] = "";

const char *
smb_vc_getpass(struct smb_vc *vcp)
{
	if (vcp->vc_pass)
		return vcp->vc_pass;
	return smb_emptypass;
}

u_short
smb_vc_nextmid(struct smb_vc *vcp)
{
	u_short r;

	SMB_CO_LOCK(&vcp->obj);
	r = vcp->vc_mid++;
	SMB_CO_UNLOCK(&vcp->obj);
	return r;
}

/*
 * Share implementation
 */
/*
 * Allocate share structure and attach it to the given VC
 * Connection expected to be locked on entry. Share will be returned
 * in locked state.
 */
int
smb_share_create(struct smb_vc *vcp, struct smb_sharespec *shspec,
	struct smb_cred *scred, struct smb_share **sspp)
{
	struct smb_share *ssp;
	int error;

	error = smb_vc_lookupshare(vcp, shspec, scred, &ssp);
	if (!error) {
		smb_share_put(ssp, scred);
		return EEXIST;
	}
	ssp = smb_zmalloc(sizeof(*ssp), M_SMBCONN, M_WAITOK);
	smb_co_init(SSTOCP(ssp), SMBL_SHARE, "smbss", vfs_context_proc(scred->scr_vfsctx));
	ssp->obj.co_free = smb_share_free;
	ssp->obj.co_gone = smb_share_gone;
	lck_mtx_init(&ssp->ss_stlock, ssst_lck_group, ssst_lck_attr);
	ssp->ss_name = smb_strdup(shspec->name);
	ssp->ss_mount = NULL;
	if (shspec->pass && shspec->pass[0])
		ssp->ss_pass = smb_strdup(shspec->pass);
	ssp->ss_type = shspec->stype;
	ssp->ss_tid = SMB_TID_UNKNOWN;
	ssp->ss_uid = shspec->owner;
	ssp->ss_grp = shspec->group;
	ssp->ss_mode = shspec->rights & SMBM_MASK;
	ssp->ss_fsname = NULL;
	smb_co_addchild(VCTOCP(vcp), SSTOCP(ssp));
	*sspp = ssp;
	return (0);
}

static void
smb_share_free(struct smb_connobj *cp)
{
	struct smb_share *ssp = CPTOSS(cp);

	SMB_STRFREE(ssp->ss_name);
	SMB_STRFREE(ssp->ss_pass);
	SMB_STRFREE(ssp->ss_fsname);
	lck_mtx_destroy(&ssp->ss_stlock, ssst_lck_group);
	smb_co_done(SSTOCP(ssp));
	free(ssp, M_SMBCONN);
}

static void
smb_share_gone(struct smb_connobj *cp, struct smb_cred *scred)
{
	struct smb_share *ssp = CPTOSS(cp);
	
	DBG_ASSERT(ssp);
	DBG_ASSERT(SSTOVC(ssp));
	DBG_ASSERT(SSTOVC(ssp)->vc_iod);
	smb_smb_treedisconnect(ssp, scred);
}

void
smb_share_ref(struct smb_share *ssp)
{
	smb_co_ref(SSTOCP(ssp));
}

void
smb_share_rele(struct smb_share *ssp, struct smb_cred *scred)
{
	smb_co_rele(SSTOCP(ssp), scred);
}

void
smb_share_put(struct smb_share *ssp, struct smb_cred *scred)
{
	smb_co_put(SSTOCP(ssp), scred);
}

int
smb_share_lock(struct smb_share *ssp)
{
	return smb_co_lock(SSTOCP(ssp));
}

void
smb_share_unlock(struct smb_share *ssp)
{
	smb_co_unlock(SSTOCP(ssp));
}

int
smb_share_access(struct smb_share *ssp, vfs_context_t context)
{
	kauth_cred_t cred = vfs_context_ucred(context);
	struct smb_vc *vcp = SSTOVC(ssp);

	if (vcp->vc_flags & SMBV_GUEST_ACCESS)
		return(0);
	
	if (suser(cred, NULL) == 0 || kauth_cred_getuid(cred) == ssp->ss_uid)
		return (0);
	return (EACCES);
}

void
smb_share_invalidate(struct smb_share *ssp)
{
	ssp->ss_tid = SMB_TID_UNKNOWN;
}

const char*
smb_share_getpass(struct smb_share *ssp)
{
	struct smb_vc *vcp;

	if (ssp->ss_pass)
		return ssp->ss_pass;
	vcp = SSTOVC(ssp);
	if (vcp->vc_pass)
		return vcp->vc_pass;
	return smb_emptypass;
}

