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
 * Connection engine.
 */

#include <sys/sysctl.h>			/* can't avoid that */

#include <sys/smb_apple.h>
#include <sys/kauth.h>

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

static void smb_co_put(struct smb_connobj *cp, vfs_context_t context);

/*
 * The smb_co_lock, smb_co_unlock, smb_co_ref, smb_co_rel and smb_co_put deal
 * with the vclist, vc and shares. So the vclist owns the vc which owns the share.
 * Currently the share owns nothing even though it does have some relationship
 * with mount structure.
 */
static int smb_co_lock(struct smb_connobj *cp)
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

static void smb_co_unlock(struct smb_connobj *cp)
{		
	if (cp->co_lockowner && (cp->co_lockowner != current_thread())) {
		SMBERROR("not owner of lock");
	} else if (cp->co_lockcount && (--cp->co_lockcount == 0)) {
		cp->co_lockowner = NULL;
		lck_mtx_unlock(cp->co_lock);
		lck_mtx_lock(&(cp)->co_interlock);
		if (cp->co_lock_flags & SMBFS_CO_LOCK_WAIT){
			cp->co_lock_flags &= ~SMBFS_CO_LOCK_WAIT;
			lck_mtx_unlock(&(cp)->co_interlock);
			wakeup(&cp->co_lock);
		} else 
			lck_mtx_unlock(&(cp)->co_interlock);
	}
}

/*
 * Common code for connection object
 */
static void
smb_co_init(struct smb_connobj *cp, int level, const char *objname, struct proc *p)
{
#pragma unused (objname, p)
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

static void smb_co_done(struct smb_connobj *cp)
{
	lck_mtx_destroy(&cp->co_interlock, co_lck_group);
	lck_mtx_free(cp->co_lock, co_lck_group);
	cp->co_lock = 0;
	cp->co_lock_flags = 0;
	cp->co_lockowner = 0;
	cp->co_lockcount = 0;
}

static void smb_co_gone(struct smb_connobj *cp, vfs_context_t context)
{
	struct smb_connobj *parent;
	
	/* Drain any locks that are still held */
	lck_mtx_lock(&(cp)->co_interlock);
	while (cp->co_lockcount > 0) {
		cp->co_lock_flags |= SMBFS_CO_LOCK_WAIT;
		msleep(&cp->co_lock, &(cp)->co_interlock, 0, 0, 0);
	}
	lck_mtx_unlock(&(cp)->co_interlock);
	/* 
	 * The old code would take a smb_co_lock here. Since SMBO_GONE is set
	 * the smb_co_lock did nothing. So I removed that code.
	 */
	
	if (cp->co_gone)
		cp->co_gone(cp, context);
	parent = cp->co_parent;
	if (parent) {
		if (smb_co_lock(parent)) {
			SMBERROR("unable to lock level %d\n", parent->co_level);
		} else {
			SLIST_REMOVE(&parent->co_children, cp, smb_connobj,
						 co_next);
			smb_co_put(parent, context);
		}
	}
	if (cp->co_free)
		cp->co_free(cp);
}

static void smb_co_put(struct smb_connobj *cp, vfs_context_t context)
{
	
	lck_mtx_lock(&(cp)->co_interlock);
	if (cp->co_usecount > 1) {
		cp->co_usecount--;
	} else if (cp->co_usecount == 1) {
		cp->co_usecount--;
		cp->co_flags |= SMBO_GONE;
	} else {
		SMBERROR("negative usecount\n");
	}
	lck_mtx_unlock(&(cp)->co_interlock);
	smb_co_unlock(cp);
	if ((cp->co_flags & SMBO_GONE) == 0)
		return;

	smb_co_gone(cp, context);
}

static void smb_co_ref(struct smb_connobj *cp)
{
	lck_mtx_lock(&(cp)->co_interlock);
	cp->co_usecount++;
	lck_mtx_unlock(&(cp)->co_interlock);
}

static void smb_co_addchild(struct smb_connobj *parent, struct smb_connobj *child)
{
	smb_co_ref(parent);
	SLIST_INSERT_HEAD(&parent->co_children, child, co_next);
	child->co_parent = parent;
}

static void smb_co_rele(struct smb_connobj *cp, vfs_context_t context)
{
	lck_mtx_lock(&(cp)->co_interlock);
	if (cp->co_usecount > 1) {
		cp->co_usecount--;
		lck_mtx_unlock(&(cp)->co_interlock);
		return;
	}
	if (cp->co_usecount == 0) {
		SMBERROR("negative co_usecount for level %d\n", cp->co_level);
		lck_mtx_unlock(&(cp)->co_interlock);
		return;
	}
	cp->co_usecount--;
	if (cp->co_flags & SMBO_GONE) {
		lck_mtx_unlock(&(cp)->co_interlock);
		return; /* someone is already draining */
	}
	cp->co_flags |= SMBO_GONE;
	lck_mtx_unlock(&(cp)->co_interlock);
	
	smb_co_gone(cp, context);
}

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

int smb_sm_init(void)
{
	smb_co_init(&smb_vclist, SMBL_SM, "smbsm", current_proc());
	smb_co_unlock(&smb_vclist);
	return (0);
}

int smb_sm_done(void)
{
	if (smb_vclist.co_usecount > 1) {
		SMBERROR("%d connections still active\n", smb_vclist.co_usecount - 1);
		return (EBUSY);
	}
	/* XXX Q4BP why are we not iterating on smb_vclist here with SMBCO_FOREACH? */
	smb_co_done(&smb_vclist);
	return (0);
}

static void smb_sm_lockvclist()
{
  	/*
	 * The smb_vclist never goes away so there is no way for smb_co_lock
	 * to fail in this case. 
	 */	
	KASSERT((smb_co_lock(&smb_vclist) == 0), ("smb_sm_lockvclist: lock failed"));
}

static void smb_sm_unlockvclist()
{
	smb_co_unlock(&smb_vclist);
}

/*
 * This routine will reset the virtual circuit. When doing a reconnect we need to
 * keep some of the virtual circuit information around. We only reset the information
 * that is required to do the reconnect.
 */
void smb_vc_reset(struct smb_vc *vcp)
{
	/* 
	 * If these three flags were set keep them for the reconnect. Clear out 
	 * any other flags that may have been set in the original connection. 
	 */
	vcp->vc_hflags2 &= (SMB_FLAGS2_EXT_SEC | SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_UNICODE);
	
	vcp->vc_mid = 0;
	vcp->vc_number = smb_vcnext++;
	/* Reset the smb signing */
	smb_reset_sig(vcp);
}

void smb_vc_ref(struct smb_vc *vcp)
{
	smb_co_ref(VCTOCP(vcp));
}

void smb_vc_rele(struct smb_vc *vcp, vfs_context_t context)
{	
	smb_co_rele(VCTOCP(vcp), context);
}

static void smb_vc_put(struct smb_vc *vcp, vfs_context_t context)
{	
	smb_co_put(VCTOCP(vcp), context);
}

int smb_vc_lock(struct smb_vc *vcp)
{
	return smb_co_lock(VCTOCP(vcp));
}

void smb_vc_unlock(struct smb_vc *vcp)
{
	smb_co_unlock(VCTOCP(vcp));
}

static void smb_vc_free(struct smb_connobj *cp)
{
	struct smb_vc *vcp = CPTOVC(cp);
	
	if (vcp->vc_iod)
		smb_iod_destroy(vcp->vc_iod);
	vcp->vc_iod = NULL;
	SMB_STRFREE(vcp->NativeOS);
	SMB_STRFREE(vcp->NativeLANManager);
	SMB_STRFREE(vcp->vc_username);
	SMB_STRFREE(vcp->vc_uppercase_username);
	SMB_STRFREE(vcp->vc_srvname);
	SMB_STRFREE(vcp->vc_localname);
	SMB_STRFREE(vcp->vc_pass);
	SMB_STRFREE(vcp->vc_domain);
	if (vcp->vc_mackey)
		free(vcp->vc_mackey, M_SMBTEMP);
	if (vcp->vc_saddr)
		free(vcp->vc_saddr, M_SONAME);
	if (vcp->vc_laddr)
		free(vcp->vc_laddr, M_SONAME);
	smb_gss_destroy(&vcp->vc_gss);
	if (vcp->throttle_info)
		throttle_info_release(vcp->throttle_info);
	vcp->throttle_info = NULL;
	smb_co_done(VCTOCP(vcp));
	lck_mtx_destroy(&vcp->vc_stlock, vcst_lck_group);
	free(vcp, M_SMBCONN);
}


/*
 * Destroy VC to server, invalidate shares linked with it.
 * Transport should be locked on entry.
 */
static int smb_vc_disconnect(struct smb_vc *vcp)
{
	if (vcp->vc_iod)
		smb_iod_request(vcp->vc_iod, SMBIOD_EV_DISCONNECT | SMBIOD_EV_SYNC, NULL);
	return (0);
}

/*
 * Called when use count of VC dropped to zero.
 * VC should be locked on enter with LK_DRAIN.
 */
static void smb_vc_gone(struct smb_connobj *cp, vfs_context_t context)
{
#pragma unused(context)
	struct smb_vc *vcp = CPTOVC(cp);
	
	smb_vc_disconnect(vcp);
}

static int smb_vc_create(struct smbioc_negotiate *vcspec, 
						 struct sockaddr *saddr, struct sockaddr *laddr,
						 vfs_context_t context, struct smb_vc **vcpp)
{
	struct smb_vc *vcp;
	int error;
	
	vcp = smb_zmalloc(sizeof(*vcp), M_SMBCONN, M_WAITOK);
	smb_co_init(VCTOCP(vcp), SMBL_VC, "smb_vc", vfs_context_proc(context));
	vcp->obj.co_free = smb_vc_free;
	vcp->obj.co_gone = smb_vc_gone;
	vcp->vc_number = smb_vcnext++;
	vcp->vc_timo = SMB_DEFRQTIMO;
	vcp->vc_smbuid = SMB_UID_UNKNOWN;
	vcp->vc_tdesc = &smb_tran_nbtcp_desc;
	vcp->vc_seqno = 0;
	vcp->vc_mackey = NULL;
	vcp->vc_mackeylen = 0;
	vcp->vc_saddr = saddr;
	vcp->vc_laddr = laddr;
	/* Remove any user setable items */
	vcp->vc_flags &= ~SMBV_USER_LAND_MASK;
	/* Now add the users setable items */
	vcp->vc_flags |= (vcspec->ioc_ssn.ioc_opt & SMBV_USER_LAND_MASK);
	
	/* Now add the throttle info */
	vcp->throttle_info = throttle_info_create();
#ifdef DEBUG_TURN_OFF_EXT_SEC
	vcp->vc_hflags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
#else // DEBUG_TURN_OFF_EXT_SEC
	vcp->vc_hflags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_EXT_SEC | SMB_FLAGS2_UNICODE;
#endif // DEBUG_TURN_OFF_EXT_SEC
	
	vcp->vc_uid = vcspec->ioc_ssn.ioc_owner;
	/* Amount of time to wait while reconnecting */
	vcp->reconnect_wait_time = vcspec->ioc_ssn.ioc_reconnect_wait_time;	
	
	lck_mtx_init(&vcp->vc_stlock, vcst_lck_group, vcst_lck_attr);
	error = 0;
	itry {
		
		ierror((vcp->vc_srvname = smb_strdup(vcspec->ioc_ssn.ioc_srvname, 
						sizeof(vcspec->ioc_ssn.ioc_srvname))) == NULL, ENOMEM);
		
		ierror((vcp->vc_localname = smb_strdup(vcspec->ioc_ssn.ioc_localname, 
						sizeof(vcspec->ioc_ssn.ioc_localname))) == NULL, ENOMEM);
				
		ithrow(smb_iod_create(vcp));
		*vcpp = vcp;
		smb_sm_lockvclist();
		smb_co_addchild(&smb_vclist, VCTOCP(vcp));
		smb_sm_unlockvclist();
	} icatch(error) {
		smb_vc_put(vcp, context);
	} ifinally {
	} iendtry;
	return error;
}

/*
 * On success the vc will have a reference taken and a lock.
 *
 * Only smb_sm_negotiate passes sockaddr, all other routines need to pass in a 
 * vcp to search on.
 */
static int smb_sm_lookupint(struct sockaddr *sap, uid_t owner, char *username, 
							u_int32_t user_flags, struct smb_vc **vcpp)
{
	struct smb_vc *vcp;
	int error;
	
	
	DBG_ASSERT(vcpp);	/* Better have passed us a vcpp */
tryagain:
	smb_sm_lockvclist();
	error = ENOENT;
	SMBCO_FOREACH(vcp, &smb_vclist) {
		
		if (*vcpp && vcp != *vcpp)
			continue;
		else if (*vcpp) {
			/* Found a match, lock it, we are done. */
			error = smb_vc_lock(vcp);
			DBG_ASSERT(error == 0);	/* Don't believe this can happen */
			break;
		} else {
			/* 
			 * We should only get in here from the negotiate routine. We better 
			 * have a sock addr or thats a programming error.
			 */
			DBG_ASSERT(sap);
			
			/* Don't share a vcp that hasn't been authenticated yet */
			if ((vcp->vc_flags & SMBV_AUTH_DONE) != SMBV_AUTH_DONE) {
				continue;
			}
			
			/* 
			 * Socket family, address and port all need to match. In the 
			 * future should we allow port mismatch? See Radar 5483417 for
			 * more on this issue.
			 */
			if (!CONNADDREQ(vcp->vc_saddr, sap)) {
				continue;
			}
			
			/* Must be the same owner */
			if (vcp->vc_uid != owner) {
				continue;
			}
			
			/* Private vc session, don't share this session */
			if (vcp->vc_flags & SMBV_PRIVATE_VC) {
				continue;
			}

			/* Ok we have a lock on the vcp, any error needs to unlock it */
			error = smb_vc_lock(vcp);
			/*
			 * This VC is going away, but it is currently block on the lock we 
			 * hold for smb_vclist. We need to unlock the list and allow the VC 
			 * to be remove. This still may not be the VC we were looking for so
			 * start the search again.
			 */
			if (error) {
				smb_sm_unlockvclist();
				goto tryagain;
			}
			/*
			 * If they ask for authentication then the vcp needs to match that
			 * authentication or we need to keep looking. So here are the 
			 * scenarios we need to deal with here.
			 *
			 * 1. They are requesting kerberos access. If the current vcp isn't
			 *    using kerberos then do not reuse the vcp.
			 * 2. They are requesting guest access. If the current vcp isn't
			 *    using guest then do not reuse the vcp.
			 * 3. They are using user level security. The vcp user name needs to
			 *	  match the one passed in.
			 * 4. They don't care. Always use the authentication of this vcp.
			 */
			
			/*
			 * They are asking for Kerberos access, if the vcp is using Kerberos
			 * then use this vcp otherwise keep looking.
			 */
			if (user_flags & SMBV_KERBEROS_ACCESS) {
				if (vcp->vc_flags & SMBV_KERBEROS_ACCESS) {
					error = 0;
					break;				
				} else {
					smb_vc_unlock(vcp);									
					error = ENOENT;
					continue;				
				}
			}
			
			/*
			 * They are asking for guest access, if the vcp is using guest
			 * then use this vcp otherwise keep looking.
			 */
			if (user_flags & SMBV_GUEST_ACCESS) {
				if (vcp->vc_flags & SMBV_GUEST_ACCESS) {
					error = 0;
					break;				
				} else {
					smb_vc_unlock(vcp);									
					error = ENOENT;
					continue;				
				}
			}
			
			/*
			 * They are asking for user level access, if the vcp is using user
			 * level access and the user names match then use this vcp otherwise 
			 * keep looking.
			 */
			if (username && username[0]) {
				if (vcp->vc_username && 
					((strncmp(vcp->vc_username, username, SMB_MAXUSERNAMELEN + 1)) == 0)) {
					error = 0;
					break;				
				} else {
					smb_vc_unlock(vcp);									
					error = ENOENT;
					continue;				
				}
			}
			/* They don't care which access to use so use this vcp. */
			error = 0;
			break;
		}
	}
	if (vcp && !error) {
		smb_vc_ref(vcp);
		*vcpp = vcp;
	}
	smb_sm_unlockvclist();
	return error;
}

int smb_sm_negotiate(struct smbioc_negotiate *vcspec, vfs_context_t context, 
				 struct smb_vc **vcpp, struct smb_dev *sdp)
{
	struct smb_vc *vcp = NULL;
	struct sockaddr	*saddr, *laddr;
	int error;

	saddr = smb_memdupin(vcspec->ioc_kern_saddr, vcspec->ioc_saddr_len);
	if (saddr == NULL) {
		return ENOMEM;
	}
	laddr = smb_memdupin(vcspec->ioc_kern_laddr, vcspec->ioc_laddr_len);
	if (laddr == NULL) {
		free(saddr, M_SMBDATA);	
		return ENOMEM;
	}
	*vcpp = vcp = NULL;

	if ((vcspec->ioc_ssn.ioc_opt & SMBV_PRIVATE_VC) != SMBV_PRIVATE_VC) {
		error = smb_sm_lookupint(saddr, vcspec->ioc_ssn.ioc_owner, 
								 vcspec->ioc_user, vcspec->ioc_ssn.ioc_opt, &vcp);
	}
	else
		error = ENOENT;
		
	if (error == 0) {
		free(saddr, M_SMBDATA);	
		free(laddr, M_SMBDATA);
		vcspec->ioc_extra_flags |= SMB_SHARING_VC;
	} else {
		/* If smb_vc_create fails it will clean up saddr and laddr */
		error = smb_vc_create(vcspec, saddr, laddr, context, &vcp);
		if (error == 0) {
			/* Flags used to cancel the connection */
			if (vcspec->ioc_extra_flags & TRY_BOTH_PORTS)
				sdp->sd_flags |= NSMBFL_TRYBOTH;
			vcp->connect_flag = &sdp->sd_flags;
			error = smb_vc_negotiate(vcp, context);
			vcp->connect_flag = NULL;
			sdp->sd_flags &= ~NSMBFL_TRYBOTH;
			if (error) /* Remove the lock and reference */
				smb_vc_put(vcp, context);
		}		
	}
	if ((error == 0) && (vcp)) {
		*vcpp = vcp;
		smb_vc_unlock(vcp);
	}
	return error;
}

int smb_sm_ssnsetup(struct smb_vc *vcp, struct smbioc_setup *sspec, 
					vfs_context_t context)
{
	int error;

	/*
	 * Call smb_sm_lookupint to verify that the vcp is still on the
	 * list. If not found then something really bad has happen. Log
	 * it and just return the error. If smb_sm_lookupint returns without 
	 * an error then the vcp will be locked and a refcnt will be taken. 
	 */
	error = smb_sm_lookupint(NULL, 0, NULL, 0, &vcp);
	if (error) {
		SMBERROR("The virtual circtuit was not found: error = %d\n", error);
		return error;
	}
	
	if ((vcp->vc_flags & SMBV_AUTH_DONE) == SMBV_AUTH_DONE)
		goto done;	/* Nothing more to do here */

	/* Remove any user setable items */
	vcp->vc_flags &= ~SMBV_USER_LAND_MASK;
	/* Now add the users setable items */
	vcp->vc_flags |= (sspec->ioc_vcflags & SMBV_USER_LAND_MASK);
	/* 
	 * Reset the username, password, domain, kerb client and service names. We
	 * never want to use any values left over from any previous calls.
	 */
	SMB_STRFREE(vcp->vc_username);
	SMB_STRFREE(vcp->vc_uppercase_username);
	SMB_STRFREE(vcp->vc_pass);
	SMB_STRFREE(vcp->vc_domain);
	SMB_STRFREE(vcp->vc_gss.gss_cpn);
	/* 
	 * Freeing the SPN will make sure we never use the hint. Remember that the 
	 * gss_spn contains the hint from the negotiate. We now require user
	 * land to send us a SPN, if we are going to use one.
	 */
	SMB_STRFREE(vcp->vc_gss.gss_spn);
	vcp->vc_username = smb_strdup(sspec->ioc_user, sizeof(sspec->ioc_user));
	vcp->vc_uppercase_username = smb_strdup(sspec->ioc_uppercase_user, sizeof(sspec->ioc_uppercase_user));
	vcp->vc_pass = smb_strdup(sspec->ioc_password, sizeof(sspec->ioc_password));
	vcp->vc_domain = smb_strdup(sspec->ioc_domain, sizeof(sspec->ioc_domain));
	if ((vcp->vc_pass == NULL) || (vcp->vc_domain == NULL) || 
		(vcp->vc_username == NULL)) {
		error = ENOMEM;
		goto done;
	}
	/* Kerberos client principal name is only set if we are doing kerberos */
	if (sspec->ioc_kclientpn[0])
		vcp->vc_gss.gss_cpn = smb_strdup(sspec->ioc_kclientpn,  sizeof(sspec->ioc_kclientpn));
	/* Kerberos service principal name is only set if we are doing kerberos */
	if (sspec->ioc_kservicepn[0])
		vcp->vc_gss.gss_spn = smb_strdup(sspec->ioc_kservicepn, sizeof(sspec->ioc_kservicepn));
	error = smb_vc_ssnsetup(vcp);
	/* If no error then this virtual circuit has been authorized */
	if (error == 0)		
		vcp->vc_flags |= SMBV_AUTH_DONE;

done:
	if (error) {
		/* 
		 * Authorization failed, reset any authorization
		 * information. This includes removing guest access,
		 * user name, password and the domain name. We should
		 * not every return these values after authorization
		 * fails.
		 */ 
		vcp->vc_flags &= ~(SMBV_GUEST_ACCESS | SMBV_KERBEROS_ACCESS | SMBV_ANONYMOUS_ACCESS);
		SMB_STRFREE(vcp->vc_username);
		SMB_STRFREE(vcp->vc_uppercase_username);
		SMB_STRFREE(vcp->vc_pass);
		SMB_STRFREE(vcp->vc_domain);
		SMB_STRFREE(vcp->vc_gss.gss_cpn);
		SMB_STRFREE(vcp->vc_gss.gss_spn);
	}
	
	/* Release the reference and lock that smb_sm_lookupint took on the vcp */
	smb_vc_put(vcp, context);
	return error;
}

static int smb_vc_cmpshare(struct smb_vc *vcp, struct smb_share *ssp, 
						   char *sh_name,  vfs_context_t context)
{
	if (strncmp(ssp->ss_name, sh_name, SMB_MAXUSERNAMELEN + 1) != 0)
		return 1;
	
	if (smb_vc_access(vcp, context) != 0)
		return 1;
	return (0);
}

static void smb_share_free(struct smb_connobj *cp)
{
	struct smb_share *ssp = CPTOSS(cp);
	
	SMB_STRFREE(ssp->ss_name);
	SMB_STRFREE(ssp->ss_fsname);
	lck_mtx_destroy(&ssp->ss_stlock, ssst_lck_group);
	lck_mtx_destroy(&ssp->ss_mntlock, ssst_lck_group);
	smb_co_done(SSTOCP(ssp));
	free(ssp, M_SMBCONN);
}

static void smb_share_gone(struct smb_connobj *cp, vfs_context_t context)
{
	struct smb_share *ssp = CPTOSS(cp);
	
	DBG_ASSERT(ssp);
	DBG_ASSERT(SSTOVC(ssp));
	DBG_ASSERT(SSTOVC(ssp)->vc_iod);
	smb_smb_treedisconnect(ssp, context);
}

void smb_share_ref(struct smb_share *ssp)
{
	smb_co_ref(SSTOCP(ssp));
}

void smb_share_rele(struct smb_share *ssp, vfs_context_t context)
{	
	smb_co_rele(SSTOCP(ssp), context);
}

/*
 * Look up the share on the given VC. If we find it then take a reference 
 * out on the share. The VC should to be locked on entry and will be left 
 * locked on exit.
 */
static int smb_vc_lookupshare(struct smb_vc *vcp, char *sh_name, 
					   struct smb_share **sspp, vfs_context_t context)
{
	struct smb_share *ssp = NULL;
	int error;
	
	SMBCO_FOREACH(ssp, VCTOCP(vcp)) {
		/* 
		 * We have the vc locked so the share isn't going away. We need to take
		 * a reference only if the tree is connected and the name matches.
		 * NOTE: The name is added to the share before its put on the list
		 * and stays until its remove from the list.
		 */
		if ((ssp->ss_tid != SMB_TID_UNKNOWN) &&
			(smb_vc_cmpshare(vcp, ssp, sh_name, context) == 0)) {
			smb_share_ref(ssp);			
			break;
		}
	}
	if (ssp) {
		*sspp = ssp;
		error = 0;
	} else {
		*sspp = NULL;
		error = ENOENT;
	}
	return error;
}

/*
 * Allocate share structure and attach it to the given VC. The vcp
 * needs to be locked on entry. Share will be returned in unlocked state,
 * but will have a reference on it.
 */
static int
smb_share_create(struct smb_vc *vcp, struct smbioc_share *shspec,
				 struct smb_share **sspp, vfs_context_t context)
{
	struct smb_share *ssp;
	
	ssp = smb_zmalloc(sizeof(*ssp), M_SMBCONN, M_WAITOK);
	if (ssp == NULL)
		return ENOMEM;
	/* The smb_co_init routine locks the share and takes a reference */
	smb_co_init(SSTOCP(ssp), SMBL_SHARE, "smbss", vfs_context_proc(context));
	ssp->obj.co_free = smb_share_free;
	ssp->obj.co_gone = smb_share_gone;
	lck_mtx_init(&ssp->ss_mntlock, ssst_lck_group, ssst_lck_attr);
	lck_mtx_init(&ssp->ss_stlock, ssst_lck_group, ssst_lck_attr);
	ssp->ss_name = smb_strdup(shspec->ioc_share, sizeof(shspec->ioc_share));
	lck_mtx_lock(&ssp->ss_mntlock);
	ssp->ss_mount = NULL;	/* Just to be safe clear it out */
	lck_mtx_unlock(&ssp->ss_mntlock);
	ssp->ss_type = shspec->ioc_stype;
	ssp->ss_tid = SMB_TID_UNKNOWN;
	ssp->ss_fsname = NULL;
	/* unlock the share we no longer need the lock */
	smb_co_unlock(SSTOCP(ssp));
	smb_co_addchild(VCTOCP(vcp), SSTOCP(ssp));
	*sspp = ssp;
	return (0);
}

/*
 * If we already have a connection on the share take a reference and return.
 * Otherwise create the share, add it to the vc list and then do a tree
 * connect.
 */
int smb_sm_tcon(struct smb_vc *vcp, struct smbioc_share *shspec,
			struct smb_share **shpp, vfs_context_t context)
{
	int error;
	
	*shpp = NULL;
	/*
	 * Call smb_sm_lookupint to verify that the vcp is still on the
	 * list. If not found then something really bad has happen. Log
	 * it and just return the error. If smb_sm_lookupint returns without 
	 * an error then the vcp will be locked and a refcnt will be taken. 
	 */
	error = smb_sm_lookupint(NULL, 0, NULL, 0, &vcp);
	if (error) {
		SMBERROR("The virtual circtuit was not found: error = %d\n", error);
		return error;
	}
	/* At this point we have a locked vcp see if we already have the share */
	error = smb_vc_lookupshare(vcp, shspec->ioc_share, shpp, context);
	if (error == 0) {
		/*
		 * We only have a reference on the share. We hold a lock and reference 
		 * on the vc. We are done with the vc lock, so release the lock, but
		 * hold on to the references.
		 */
		smb_vc_unlock(vcp);				
	} else {
		error = smb_share_create(vcp, shspec, shpp, context);
		/*
		 * We hold a lock and reference on the vc. We are done with the vc lock 
		 * so unlock the vc but hold on to the vc references.
		 */
		smb_vc_unlock(vcp);				
		if (error == 0) {
			error = smb_smb_treeconnect(*shpp, context);
			if (error) {
				/* Let the share drain, so it can get removed */
				smb_share_rele(*shpp, context);		
				*shpp = NULL; /* We failed reset it to NULL */
			}
		}
	}
	
	/* Release the reference that smb_sm_lookupint took on the vc */
	smb_vc_rele(vcp, context);
	return error;
}

int smb_vc_access(struct smb_vc *vcp, vfs_context_t context)
{	
	if (vcp->vc_flags & SMBV_GUEST_ACCESS)
		return(0);
	
	if ((vfs_context_suser(context) == 0) || 
		(kauth_cred_getuid(vfs_context_ucred(context)) == vcp->vc_uid))
		return (0);
	return (EACCES);
}

int smb_vc_negotiate(struct smb_vc *vcp, vfs_context_t context)
{
	return smb_iod_request(vcp->vc_iod,
			       SMBIOD_EV_NEGOTIATE | SMBIOD_EV_SYNC, context);
}

int smb_vc_ssnsetup(struct smb_vc *vcp)
{
	return smb_iod_request(vcp->vc_iod,
 			       SMBIOD_EV_SSNSETUP | SMBIOD_EV_SYNC, NULL);
}

static char smb_emptypass[] = "";

const char * smb_vc_getpass(struct smb_vc *vcp)
{
	if (vcp->vc_pass)
		return vcp->vc_pass;
	return smb_emptypass;
}

/*
 * They are in share level security and the share requires
 * a password. Use the vcp password always. On required for
 * Windows 98, should drop support someday.
 */
const char * smb_share_getpass(struct smb_share *ssp)
{
	DBG_ASSERT(SSTOVC(ssp));
	return smb_vc_getpass(SSTOVC(ssp));
}

u_short smb_vc_nextmid(struct smb_vc *vcp)
{
	u_short r;
	struct smb_connobj *cp = &vcp->obj;
	
	lck_mtx_lock(&(cp)->co_interlock);
	r = vcp->vc_mid++;
	lck_mtx_unlock(&(cp)->co_interlock);
	return r;
}

/*
 * The reconnect code needs to get a reference on the vc. First make sure
 * this vc is still in the list and no one has release it yet. If smb_sm_lookupint
 * finds it we will have it locked and a reference on it. Next make sure its
 * not being release. 
 */
int smb_vc_reconnect_ref(struct smb_vc *vcp, vfs_context_t context)
{
	int error;
	
	error = smb_sm_lookupint(NULL, 0, NULL, 0, &vcp);
	if (error)
		return error;
	
	smb_vc_unlock(vcp);
	/* This vc is being release just give up */
	if (vcp->ss_flags & SMBO_GONE) {
		smb_vc_rele(vcp, context);
		error = ENOTCONN;
	}
	return error;
}

/*
 * Called from a thread that is not the main iod thread. Prevents us from
 * getting into a deadlock.
 */
static void smb_reconnect_rel_thread(void *arg) 
{
	struct smbiod *iod = arg;
	
	/* We are done release the reference */
	smb_vc_rele(iod->iod_vc, iod->iod_context);
}

/*
 * The reconnect code takes a reference on the vc. So we need to release that
 * reference, but if we are the last reference the smb_vc_rele routine will 
 * attempt to destroy the vc, which will then attempt to destroy the main iod 
 * thread for the vc. The reconnect code is running under the main iod thread, 
 * which means we can't destroy the thread from that thread without hanging. So
 * start a new thread to just release the reference and do any cleanup required.
 * This will be a short live thread that just hangs around long enough to do the
 * work required to release the vc reference.
 */
void smb_vc_reconnect_rel(struct smb_vc *vcp)
{
	struct smbiod *iod = vcp->vc_iod;
	thread_t	thread;
	int			error;
	
	do {
		error  = kernel_thread_start((thread_continue_t)smb_reconnect_rel_thread, 
									 iod, &thread);
		/*
		 * Never expect an error here, but just in case log it, sleep for one
		 * second and try again. Nothing else we can do at this point.
		 */
		if (error) {
			struct timespec ts;
			
			SMBERROR("Starting the reconnect vc release thread failed! %d\n", 
					 error);
			ts.tv_sec = 1;
			ts.tv_nsec = 0;
			msleep(iod, NULL, PWAIT | PCATCH, "smb_vc_reconnect_rel", &ts);
		}
	} while (error);
	thread_deallocate(thread);
}


