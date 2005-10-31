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
 * $Id: smb_conn.c,v 1.27.166.2 2005/07/20 05:27:00 lindak Exp $
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
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>

static struct smb_connobj smb_vclist;
static int smb_vcnext = 1;	/* next unique id for VC */

extern struct linker_set sysctl_net_smb;

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_smb);
#endif

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
		return EBUSY;
	}
	/* XXX Q4BP why are we not iterating on smb_vclist here with SMBCO_FOREACH? */
	smb_co_done(&smb_vclist);
	return (0);
}

static int
smb_sm_lockvclist(struct proc *p)
{
	return smb_co_lock(&smb_vclist);
}

static void
smb_sm_unlockvclist(struct proc *p)
{
	smb_co_unlock(&smb_vclist);
}

static int
smb_sm_lookupint(struct smb_vcspec *vcspec, struct smb_sharespec *shspec,
	struct smb_cred *scred,	struct smb_vc **vcpp, int internal)
{
	struct proc *p = vfs_context_proc(scred->scr_vfsctx);
	struct smb_vc *vcp;
	int exact = 1;
	int error;

	vcspec->shspec = shspec;
	error = ENOENT;
	SMBCO_FOREACH(vcp, &smb_vclist) {

		if (*vcpp && vcp != *vcpp)
			continue;
		error = smb_vc_lock(vcp, p);
		if (error)
			continue;
		itry {
			if (!CONNADDREQ(vcp->vc_paddr, vcspec->sap))
				ithrow(EPERM);
			if (!internal &&
			    (vcp->obj.co_flags & SMBV_PRIVATE ||
			     strcmp(vcp->vc_username, vcspec->username) != 0))
				ithrow(EPERM);
			if (vcspec->owner != SMBM_ANY_OWNER) {
				if (vcp->vc_uid != vcspec->owner)
					ithrow(EPERM);
			} else
				exact = 0;
			if (vcspec->group != SMBM_ANY_GROUP) {
				if (vcp->vc_grp != vcspec->group)
					ithrow(EPERM);
			} else
				exact = 0;

			if (vcspec->mode & SMBM_EXACT) {
				if (!exact ||
				    (vcspec->mode & SMBM_MASK) != vcp->vc_mode)
					ithrow(EPERM);
			}
			ithrow(smb_vc_access(vcp, scred, vcspec->mode));
			vcspec->ssp = NULL;
			if (shspec)
				ithrow(smb_vc_lookupshare(vcp, shspec, scred,
							  &vcspec->ssp));
			error = 0;
			break;
		} icatch(error) {
			smb_vc_unlock(vcp, p);
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
	return error;
}


int
smb_sm_negotiate(struct smb_vcspec *vcspec, struct smb_sharespec *shspec,
	struct smb_cred *scred,	struct smb_vc **vcpp)
{
	struct proc *p = vfs_context_proc(scred->scr_vfsctx);
	struct smb_vc *vcp;
	int error;

	*vcpp = vcp = NULL;

	error = smb_sm_lockvclist(p);
	if (error)
		return error;
	error = smb_sm_lookupint(vcspec, shspec, scred, vcpp, 0);
	if (error == 0 || (vcspec->flags & SMBV_CREATE) == 0) {
		smb_sm_unlockvclist(p);
		return error;
	}
	error = smb_sm_lookupint(vcspec, NULL, scred, &vcp, 0);
	if (!error) {
		error = smb_vc_setup(vcspec, vcp);
	} else {
		error = smb_vc_create(vcspec, scred, &vcp);
		if (error)
			goto out;
		vcp->vc_intok = vcspec->tok;
		vcp->vc_intoklen = vcspec->toklen;

		error = smb_vc_negotiate(vcp, scred);

		vcp->vc_intok = NULL;
		vcp->vc_intoklen = 0;
	}
out:
	smb_sm_unlockvclist(p);
	if (error == 0)
		*vcpp = vcp;
	else if (vcp)
		smb_vc_put(vcp, scred);
	return error;
}

int
smb_sm_ssnsetup(struct smb_vcspec *vcspec, struct smb_sharespec *shspec,
	struct smb_cred *scred,	struct smb_vc *vcp)
{
	struct proc *p = vfs_context_proc(scred->scr_vfsctx);
	int error;

	error = smb_sm_lockvclist(p);
	if (error)
		return error;
	error = smb_sm_lookupint(vcspec, shspec, scred, &vcp, 1);
	if (error == 0) {
		smb_sm_unlockvclist(p);
		return error;
	}
	error = smb_sm_lookupint(vcspec, NULL, scred, &vcp, 1);
	smb_sm_unlockvclist(p);
	if (error)
		return (error);
	if (!vcp->vc_outtok) {
		error = smb_vc_setup(vcspec, vcp);
		if (!error) {
			vcp->vc_intok = vcspec->tok;
			vcp->vc_intoklen = vcspec->toklen;

			error = smb_vc_ssnsetup(vcp, scred);

			vcp->vc_intok = NULL;
			vcp->vc_intoklen = 0;
		}
		if (error)
			smb_vc_put(vcp, scred);
	}
	return error;
}

int
smb_sm_tcon(struct smb_vcspec *vcspec, struct smb_sharespec *shspec,
	struct smb_cred *scred,	struct smb_vc *vcp)
{
	struct proc *p = vfs_context_proc(scred->scr_vfsctx);
	struct smb_share *ssp = NULL;
	int error;

	error = smb_sm_lockvclist(p);
	if (error)
		return error;
	error = smb_sm_lookupint(vcspec, shspec, scred, &vcp, 1);
	if (error == 0) {
		smb_sm_unlockvclist(p);
		return error;
	}
	error = smb_sm_lookupint(vcspec, NULL, scred, &vcp, 1);
	smb_sm_unlockvclist(p);
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
	smb_sl_init(&cp->co_interlock, co_lck_group, co_lck_attr);
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
	smb_sl_destroy(&cp->co_interlock, co_lck_group);
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
		return EINVAL;
	if (cp->co_lockowner == current_thread()) {
		cp->co_lockcount++;
	} else  {
		lck_mtx_lock(cp->co_lock);
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
 * Session implementation
 */

int
smb_vc_setup(struct smb_vcspec *vcspec, struct smb_vc *vcp)
{
	char *domain = vcspec->domain;
	int error;

	vcp->vc_mode = vcspec->rights & SMBM_MASK;
	vcp->obj.co_flags |=
	    vcspec->flags & (SMBV_PRIVATE | SMBV_SINGLESHARE | SMBV_MINAUTH);
	if (vcspec->flags & SMBV_EXT_SEC)
		vcp->vc_hflags2 |= SMB_FLAGS2_EXT_SEC;
	error = 0;
	itry {
		ierror((vcp->vc_pass = smb_strdup(vcspec->pass)) == NULL, ENOMEM);
		if (!domain || !domain[0])
			domain = "NODOMAIN";
		ierror((vcp->vc_domain = smb_strdup(domain)) == NULL, ENOMEM);

		ierror((vcp->vc_username = smb_strdup(vcspec->username)) == NULL, ENOMEM);
	} icatch(error) {
	} ifinally {
	} iendtry;
	return error;
}

int
smb_vc_create(struct smb_vcspec *vcspec,
	struct smb_cred *scred, struct smb_vc **vcpp)
{
	struct smb_vc *vcp;
	struct proc *p = vfs_context_proc(scred->scr_vfsctx);
	struct ucred *cred = vfs_context_ucred(scred->scr_vfsctx);
	uid_t uid = vcspec->owner;
	gid_t gid = vcspec->group;
	uid_t realuid = kauth_cred_getuid(cred);
	int error, isroot;

	isroot = suser(cred, NULL) == 0;
	/*
	 * Only superuser can create VCs with different uid and gid
	 */
	if (uid != SMBM_ANY_OWNER && uid != realuid && !isroot)
		return EPERM;
	if (gid != SMBM_ANY_GROUP && !groupmember(gid, cred) && !isroot)
		return EPERM;

	vcp = smb_zmalloc(sizeof(*vcp), M_SMBCONN, M_WAITOK);
	smb_co_init(VCTOCP(vcp), SMBL_VC, "smb_vc", p);
	vcp->obj.co_free = smb_vc_free;
	vcp->obj.co_gone = smb_vc_gone;
	vcp->vc_number = smb_vcnext++;
	vcp->vc_timo = SMB_DEFRQTIMO;
	vcp->vc_smbuid = SMB_UID_UNKNOWN;
	vcp->vc_tdesc = &smb_tran_nbtcp_desc;
	if (uid == SMBM_ANY_OWNER)
		uid = realuid;
	if (gid == SMBM_ANY_GROUP)
		gid = cred->cr_groups[0];
	vcp->vc_uid = uid;
	vcp->vc_grp = gid;
	/* gets disabled if nego response shows antique server!*/
	vcp->vc_hflags2 |= SMB_FLAGS2_KNOWS_LONG_NAMES;

	smb_sl_init(&vcp->vc_stlock, vcst_lck_group, vcst_lck_attr);
	error = 0;
	itry {
		ithrow(smb_vc_setup(vcspec, vcp));

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
		smb_co_addchild(&smb_vclist, VCTOCP(vcp));
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
	SMB_STRFREE(vcp->vc_username);
	SMB_STRFREE(vcp->vc_srvname);
	SMB_STRFREE(vcp->vc_pass);
	SMB_STRFREE(vcp->vc_domain);
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
	smb_co_done(VCTOCP(vcp));
	smb_sl_destroy(&vcp->vc_stlock, vcst_lck_group);
	if (vcp->vc_intok)
		free(vcp->vc_intok, M_SMBTEMP);
	if (vcp->vc_outtok)
		free(vcp->vc_outtok, M_SMBTEMP);
	if (vcp->vc_negtok)
		free(vcp->vc_negtok, M_SMBTEMP);
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
smb_vc_lock(struct smb_vc *vcp, struct proc *p)
{
	return smb_co_lock(VCTOCP(vcp));
}

void
smb_vc_unlock(struct smb_vc *vcp, struct proc *p)
{
	smb_co_unlock(VCTOCP(vcp));
}

int
smb_vc_access(struct smb_vc *vcp, struct smb_cred *scred, mode_t mode)
{
	struct ucred *cred = vfs_context_ucred(scred->scr_vfsctx);

	if (suser(cred, NULL) == 0 || kauth_cred_getuid(cred) == vcp->vc_uid)
		return (0);
	return (EACCES);
}

static int
smb_vc_cmpshare(struct smb_share *ssp, struct smb_sharespec *dp)
{
	int exact = 1;

	if (strcmp(ssp->ss_name, dp->name) != 0)
		return 1;
	if (dp->owner != SMBM_ANY_OWNER) {
		if (ssp->ss_uid != dp->owner)
			return 1;
	} else
		exact = 0;
	if (dp->group != SMBM_ANY_GROUP) {
		if (ssp->ss_grp != dp->group)
			return 1;
	} else
		exact = 0;

	if (dp->mode & SMBM_EXACT) {
		if (!exact)
			return 1;
		return (dp->mode & SMBM_MASK) == ssp->ss_mode ? 0 : 1;
	}
	if (smb_share_access(ssp, dp->scred, dp->mode) != 0)
		return 1;
	return (0);
}

/*
 * Lookup share in the given VC. Share referenced and locked on return.
 * VC expected to be locked on entry and will be left locked on exit.
 */
int
smb_vc_lookupshare(struct smb_vc *vcp, struct smb_sharespec *dp,
	struct smb_cred *scred,	struct smb_share **sspp)
{
	struct proc *p = vfs_context_proc(scred->scr_vfsctx);
	struct smb_share *ssp = NULL;
	int error;

	*sspp = NULL;
	dp->scred = scred;
	SMBCO_FOREACH(ssp, VCTOCP(vcp)) {

		error = smb_share_lock(ssp, p);
		if (error)
			continue;
		if (smb_vc_cmpshare(ssp, dp) == 0)
			break;
		smb_share_unlock(ssp, p);
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
			       SMBIOD_EV_NEGOTIATE | SMBIOD_EV_SYNC, NULL);
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
	struct proc *p = vfs_context_proc(scred->scr_vfsctx);
	struct ucred *cred = vfs_context_ucred(scred->scr_vfsctx);
	uid_t realuid = kauth_cred_getuid(cred);
	uid_t uid = shspec->owner;
	gid_t gid = shspec->group;
	int error, isroot;

	isroot = suser(cred, NULL) == 0;
	/*
	 * Only superuser can create shares with different uid and gid
	 */
	if (uid != SMBM_ANY_OWNER && uid != realuid && !isroot)
		return EPERM;
	if (gid != SMBM_ANY_GROUP && !groupmember(gid, cred) && !isroot)
		return EPERM;
	error = smb_vc_lookupshare(vcp, shspec, scred, &ssp);
	if (!error) {
		smb_share_put(ssp, scred);
		return EEXIST;
	}
	if (uid == SMBM_ANY_OWNER)
		uid = realuid;
	if (gid == SMBM_ANY_GROUP)
		gid = cred->cr_groups[0];
	ssp = smb_zmalloc(sizeof(*ssp), M_SMBCONN, M_WAITOK);
	smb_co_init(SSTOCP(ssp), SMBL_SHARE, "smbss", p);
	ssp->obj.co_free = smb_share_free;
	ssp->obj.co_gone = smb_share_gone;
	smb_sl_init(&ssp->ss_stlock, ssst_lck_group, ssst_lck_attr);
	ssp->ss_name = smb_strdup(shspec->name);
	ssp->ss_mount = NULL;
	if (shspec->pass && shspec->pass[0])
		ssp->ss_pass = smb_strdup(shspec->pass);
	ssp->ss_type = shspec->stype;
	ssp->ss_tid = SMB_TID_UNKNOWN;
	ssp->ss_uid = uid;
	ssp->ss_grp = gid;
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
	smb_sl_destroy(&ssp->ss_stlock, ssst_lck_group);
	smb_co_done(SSTOCP(ssp));
	free(ssp, M_SMBCONN);
}

static void
smb_share_gone(struct smb_connobj *cp, struct smb_cred *scred)
{
	struct smb_share *ssp = CPTOSS(cp);

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
smb_share_lock(struct smb_share *ssp, struct proc *p)
{
	return smb_co_lock(SSTOCP(ssp));
}

void
smb_share_unlock(struct smb_share *ssp, struct proc *p)
{
	smb_co_unlock(SSTOCP(ssp));
}

int
smb_share_access(struct smb_share *ssp, struct smb_cred *scred, mode_t mode)
{
	struct ucred *cred = vfs_context_ucred(scred->scr_vfsctx);

	if (suser(cred, NULL) == 0 || kauth_cred_getuid(cred) == ssp->ss_uid)
		return (0);
	return (EACCES);
}

void
smb_share_invalidate(struct smb_share *ssp)
{
	ssp->ss_tid = SMB_TID_UNKNOWN;
}

int
smb_share_valid(struct smb_share *ssp)
{
	return ssp->ss_tid != SMB_TID_UNKNOWN &&
	    ssp->ss_vcgenid == SSTOVC(ssp)->vc_genid;
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

int
smb_share_count(void)
{
	struct smb_vc *vcp;
	struct smb_share *ssp;
	int nshares;

	nshares = 0;
	SMBCO_FOREACH(vcp, &smb_vclist) {

		SMBCO_FOREACH(ssp, VCTOCP(vcp))

			nshares++;
	}
	return nshares;
}
