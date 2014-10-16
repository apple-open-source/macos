/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2013 Apple Inc. All rights reserved.
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
#include <netsmb/smb_2.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_trantcp.h>
#include <netsmb/smb_gss.h>
#include <netsmb/netbios.h>

extern uint32_t smbfs_deadtimer;

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
	if (cp->co_flags & SMBO_GONE) {
		/* 
		 * This can happen when we are doing a tree disconnect or a VC log off.
		 * In the future we could fix the tree disconnect by only taking a reference
		 * on the VC. Not sure what to do about the VC. If we could solve those 
		 * two issues then we should make this a fatal error.
		 */
		SMBDEBUG("The object is in the gone state level = 0x%x\n",cp->co_level);
	}
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

	SMB_MALLOC(sa2, struct sockaddr *, sa->sa_len, M_SONAME,
	       canwait ? M_WAITOK : M_NOWAIT);
	if (sa2)
		bcopy(sa, sa2, sa->sa_len);
	return (sa2);
}

int smb_sm_init(void)
{
	smb_co_init(&smb_vclist, SMBL_VCLIST, "smb_vclist", current_proc());
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
	vcp->vc_low_pid = 1;

    vcp->vc_message_id = 1;
    
    /* leave vc_misc_flags untouched as it has preferences flags */
    //vcp->vc_misc_flags = 0; 
    
    /* Save previous sessID for reconnects SessionSetup request */
    vcp->vc_prev_session_id = vcp->vc_session_id;
    vcp->vc_session_id = 0;

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
	struct smb_vc *vcp = (struct smb_vc*)cp;
	
	smb_gss_rel_cred(vcp);
    
	if (vcp->vc_iod)
		smb_iod_destroy(vcp->vc_iod);
	vcp->vc_iod = NULL;
    
    if (vcp->negotiate_token) {
        SMB_FREE(vcp->negotiate_token, M_SMBTEMP);
    }
    
    if (vcp->NativeOS) {
        SMB_FREE(vcp->NativeOS, M_SMBSTR);
    }
    
    if (vcp->NativeLANManager) {
        SMB_FREE(vcp->NativeLANManager, M_SMBSTR);
    }
    
    if (vcp->vc_username) {
        SMB_FREE(vcp->vc_username, M_SMBSTR);
    }
    
    if (vcp->vc_srvname) {
        SMB_FREE(vcp->vc_srvname, M_SMBSTR);
    }
    
    if (vcp->vc_localname) {
        SMB_FREE(vcp->vc_localname, M_SMBSTR);
    }
    
    if (vcp->vc_pass) {
        SMB_FREE(vcp->vc_pass, M_SMBSTR);
    }
    
    if (vcp->vc_domain) {
        SMB_FREE(vcp->vc_domain, M_SMBSTR);
    }
    
	if (vcp->vc_mackey) {
		SMB_FREE(vcp->vc_mackey, M_SMBTEMP);
    }
    
    if (vcp->vc_saddr) {
		SMB_FREE(vcp->vc_saddr, M_SONAME);
    }
    
    if (vcp->vc_laddr) {
		SMB_FREE(vcp->vc_laddr, M_SONAME);
    }
    
	smb_gss_destroy(&vcp->vc_gss);
    
	if (vcp->throttle_info)
		throttle_info_release(vcp->throttle_info);
	vcp->throttle_info = NULL;
    
	if (vcp->vc_model_info) {
		SMB_FREE(vcp->vc_model_info, M_SMBTEMP);
    }
	
    smb_co_done(VCTOCP(vcp));
	lck_mtx_destroy(&vcp->vc_stlock, vcst_lck_group);
    if (vcp) {
        SMB_FREE(vcp, M_SMBCONN);
    }
}

/*
 * Force reconnect on vc
 */
int smb_vc_force_reconnect(struct smb_vc *vcp)
{
	if (vcp->vc_iod) {
		smb_iod_request(vcp->vc_iod, SMBIOD_EV_FORCE_RECONNECT | SMBIOD_EV_SYNC, NULL);
    }

	return (0);
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
	struct smb_vc *vcp = (struct smb_vc*)cp;
	smb_vc_disconnect(vcp);
}

static int smb_vc_create(struct smbioc_negotiate *vcspec, 
						 struct sockaddr *saddr, struct sockaddr *laddr,
						 vfs_context_t context, struct smb_vc **vcpp)
{
	struct smb_vc *vcp;
	int error = 0;
	
	/* Should never happen, but just to be safe */
	if (context == NULL) {
		return ENOTSUP;
	}
	SMB_MALLOC(vcp, struct smb_vc *, sizeof(*vcp), M_SMBCONN, M_WAITOK | M_ZERO);
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
    vcp->vc_smb3_signing_key_len = 0;
    vcp->vc_smb3_encrypt_key_len = 0;
    vcp->vc_smb3_decrypt_key_len = 0;
	vcp->vc_saddr = saddr;
	vcp->vc_laddr = laddr;
	/* Remove any user setable items */
	vcp->vc_flags &= ~SMBV_USER_LAND_MASK;
	/* Now add the users setable items */
	vcp->vc_flags |= (vcspec->ioc_userflags & SMBV_USER_LAND_MASK);
	
	/* Now add the throttle info */
	vcp->throttle_info = throttle_info_create();
#ifdef DEBUG_TURN_OFF_EXT_SEC
	vcp->vc_hflags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
#else // DEBUG_TURN_OFF_EXT_SEC
	vcp->vc_hflags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_EXT_SEC | SMB_FLAGS2_UNICODE;
#endif // DEBUG_TURN_OFF_EXT_SEC
	
	vcp->vc_uid = vcspec->ioc_ssn.ioc_owner;
	vcp->vc_gss.gss_asid = AU_ASSIGN_ASID;
	
	/* Amount of time to wait while reconnecting */
	vcp->reconnect_wait_time = vcspec->ioc_ssn.ioc_reconnect_wait_time;	
	
    lck_mtx_init(&vcp->vc_credits_lock, vc_credits_lck_group, vc_credits_lck_attr);

	lck_mtx_init(&vcp->vc_stlock, vcst_lck_group, vcst_lck_attr);

	vcp->vc_srvname = smb_strndup(vcspec->ioc_ssn.ioc_srvname, sizeof(vcspec->ioc_ssn.ioc_srvname));
	if (vcp->vc_srvname)
		vcp->vc_localname = smb_strndup(vcspec->ioc_ssn.ioc_localname,  sizeof(vcspec->ioc_ssn.ioc_localname));
	if ((vcp->vc_srvname == NULL) || (vcp->vc_localname == NULL)) {
		error = ENOMEM;
	}

	vcp->vc_message_id = 1;
    vcp->vc_misc_flags = SMBV_HAS_FILEIDS;  /* assume File IDs supported */
    vcp->vc_server_caps = 0;
	vcp->vc_volume_caps = 0;
	vcp->vc_model_info = NULL;

	if (!error)
		error = smb_iod_create(vcp);
	if (error) {
		smb_vc_put(vcp, context);
		return error;
	}
	*vcpp = vcp;
	
	/* is SMB 1 or SMB 2/3 only flags set? */
	if (vcspec->ioc_extra_flags & SMB_SMB1_ONLY) {
		vcp->vc_misc_flags |= SMBV_NEG_SMB1_ONLY;
	}
	else if (vcspec->ioc_extra_flags & SMB_SMB2_ONLY) {
		vcp->vc_misc_flags |= SMBV_NEG_SMB2_ONLY;
	}
    else if (vcspec->ioc_extra_flags & SMB_SMB3_ONLY) {
		vcp->vc_misc_flags |= SMBV_NEG_SMB3_ONLY;
	}
	
	if (vcspec->ioc_extra_flags & SMB_SIGNING_REQUIRED) {
		vcp->vc_misc_flags |= SMBV_CLIENT_SIGNING_REQUIRED;
	}
	
	/* Save client Guid */
	memcpy(vcp->vc_client_guid, vcspec->ioc_client_guid, sizeof(vcp->vc_client_guid));
    
    /* Set default max amount of time to wait for any response from server */
    if ((vcspec->ioc_max_resp_timeout != 0) &&
        (vcspec->ioc_max_resp_timeout <= 600)) {
        vcp->vc_resp_wait_timeout = vcspec->ioc_max_resp_timeout;
        SMBWARNING("vc_resp_wait_timeout changed from default to %d \n", vcp->vc_resp_wait_timeout);
    }
    else {
        vcp->vc_resp_wait_timeout = SMB_RESP_WAIT_TIMO;
    }
    
	smb_sm_lockvclist();
	smb_co_addchild(&smb_vclist, VCTOCP(vcp));
	smb_sm_unlockvclist();
	return 0;
}

/*
 * So we have three types of sockaddr strcutures, IPv4, IPv6 or NetBIOS. 
 *
 * If both sa_family equal AF_NETBIOS then we can just compare the two sockaddr
 * structures.
 * 
 * If neither sa_family equal AF_NETBIOS then we can just compare the two sockaddr
 * structures.
 * 
 * If the search sa_family equal AF_NETBIOS and the vc sa_family doesn't then we
 * can just compare, since its its not going to match. We never support sharing
 * a AF_NETBIOS with a non AF_NETBIOS connection.
 * 
 * Now that just leaves the cases were the VC is connected using AF_NETBIOS and
 * the search sockaddr is either IPv4 or IPv6. We need to compare using the real
 * sockaddr that is inside the AF_NETBIOS sockaddr_nb structure.
 */
static int addressMatch(struct smb_vc *vcp, struct sockaddr *saddr)
{
	struct sockaddr *vc_saddr = vcp->vc_saddr;

	if ((vc_saddr->sa_family == AF_NETBIOS) && (saddr->sa_family != AF_NETBIOS)) {		
		vc_saddr = (struct sockaddr *)&((struct sockaddr_nb *)vcp->vc_saddr)->snb_addrin;
	}
	
	if ((vc_saddr->sa_len == saddr->sa_len) && (memcmp(vc_saddr, saddr, saddr->sa_len) == 0))
		return TRUE;
	
	return FALSE;
}

/*
 * On success the vc will have a reference taken and a lock.
 *
 * Only smb_sm_negotiate passes sockaddr, all other routines need to pass in a 
 * vcp to search on.
 */
static int smb_sm_lookupint(struct sockaddr *sap, uid_t owner, char *username, 
							uint32_t user_flags, struct smb_vc **vcpp)
{
	struct smb_vc *vcp, *tvcp;
	int error;
	
	
	DBG_ASSERT(vcpp);	/* Better have passed us a vcpp */
tryagain:
	smb_sm_lockvclist();
	error = ENOENT;
	SMBCO_FOREACH_SAFE(vcp, &smb_vclist, tvcp) {
		
		if (*vcpp && vcp != *vcpp)
			continue;
		else if (*vcpp) {
			/* Found a match, lock it, we are done. */
			error = smb_vc_lock(vcp);
            if (error != 0) {
                /* Can happen with bad servers */
                SMBDEBUG("smb_vc_lock returned error %d\n", error);
            }
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
			
			/* The sock address structure needs to match. */
			if (!addressMatch(vcp, sap)) {
				continue;
			}
			
			/* Must be the same owner */
			if (vcp->vc_uid != owner) {
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
			 * The VC must be active and not in reconnect, otherwise we should 
			 * just skip this VC.
			 */
			if ((vcp->vc_iod->iod_state != SMBIOD_ST_VCACTIVE) || 
				(vcp->vc_iod->iod_flags & SMBIOD_RECONNECT)) {
				SMBWARNING("Skipping %s because its down or in reconnect: flags = 0x%x state = 0x%x\n",
						   vcp->vc_srvname, vcp->vc_iod->iod_flags, vcp->vc_iod->iod_state);
				smb_vc_unlock(vcp);									
				error = ENOENT;
				continue;				
			}
			
			/*
			 * If they ask for authentication then the VC needs to match that
			 * authentication or we need to keep looking. So here are the 
			 * scenarios we need to deal with here.
			 *
			 * 1. If they are asking for a private guest access and the VC has
			 *    private guest access set then use this VC. If either is set,
			 *    but not both then don't reuse the VC.
			 * 2. If they are asking for a anonymous access and the VC has
			 *    anonymous access set then use this VC. If either is set,
			 *    but not both then don't reuse the VC.
			 * 3. They are requesting kerberos access. If the current VC isn't
			 *    using kerberos then don't reuse the vcp.
			 * 4. They are requesting guest access. If the current VC isn't
			 *    using guest then don't reuse the VC.
			 * 4. They are using user level security. The VC user name needs to
			 *	  match the one passed in.
			 * 4. They don't care. Always use the authentication of this VC.
			 */
			if ((vcp->vc_flags & SMBV_SFS_ACCESS)) {
				/* We're guest no matter what the user says, just use this VC */
				error = 0;
				break;
			} else if ((user_flags & SMBV_PRIV_GUEST_ACCESS) || (vcp->vc_flags & SMBV_PRIV_GUEST_ACCESS)) {
				if ((user_flags & SMBV_PRIV_GUEST_ACCESS) && (vcp->vc_flags & SMBV_PRIV_GUEST_ACCESS)) {
					error = 0;
					break;				
				} else {
					smb_vc_unlock(vcp);									
					error = ENOENT;
					continue;				
				}
			} else if ((user_flags & SMBV_ANONYMOUS_ACCESS) || (vcp->vc_flags & SMBV_ANONYMOUS_ACCESS)) {
				if ((user_flags & SMBV_ANONYMOUS_ACCESS) && (vcp->vc_flags & SMBV_ANONYMOUS_ACCESS)) {
					error = 0;
					break;				
				} else {
					smb_vc_unlock(vcp);									
					error = ENOENT;
					continue;				
				}
			} else if (user_flags & SMBV_KERBEROS_ACCESS) {
				if (vcp->vc_flags & SMBV_KERBEROS_ACCESS) {
					error = 0;
					break;				
				} else {
					smb_vc_unlock(vcp);									
					error = ENOENT;
					continue;				
				}
			} else if (user_flags & SMBV_GUEST_ACCESS) {
				if (vcp->vc_flags & SMBV_GUEST_ACCESS) {
					error = 0;
					break;				
				} else {
					smb_vc_unlock(vcp);									
					error = ENOENT;
					continue;				
				}
			} else if (username && username[0]) {
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
				 struct smb_vc **vcpp, struct smb_dev *sdp, int searchOnly)
{
	struct smb_vc *vcp = NULL;
	struct sockaddr	*saddr = NULL, *laddr = NULL;
	int error;

	saddr = smb_memdupin(vcspec->ioc_kern_saddr, vcspec->ioc_saddr_len);
	if (saddr == NULL) {
		return ENOMEM;
	}

	*vcpp = vcp = NULL;

	if (vcspec->ioc_extra_flags & SMB_FORCE_NEW_SESSION) {
		error = ENOENT;	/* Force a new virtual circuit session */
	} else {
		error = smb_sm_lookupint(saddr, vcspec->ioc_ssn.ioc_owner, vcspec->ioc_user, 
							 vcspec->ioc_userflags, &vcp);
	}
		
	if ((error == 0) || (searchOnly)) {
		SMB_FREE(saddr, M_SMBDATA);
		vcspec->ioc_extra_flags |= SMB_SHARING_VC;
	} else {
		/* NetBIOS connections require a local address */
		if (saddr->sa_family == AF_NETBIOS) {
			laddr = smb_memdupin(vcspec->ioc_kern_laddr, vcspec->ioc_laddr_len);
			if (laddr == NULL) {
				SMB_FREE(saddr, M_SMBDATA);	
				return ENOMEM;
			}
		}
		/* If smb_vc_create fails it will clean up saddr and laddr */
		error = smb_vc_create(vcspec, saddr, laddr, context, &vcp);
		if (error == 0) {
			/* Flags used to cancel the connection */
			vcp->connect_flag = &sdp->sd_flags;
			error = smb_vc_negotiate(vcp, context);
			vcp->connect_flag = NULL;
			if (error) /* Remove the lock and reference */
				smb_vc_put(vcp, context);
		}		
	}
	if ((error == 0) && (vcp)) {
		/* 
		 * They don't want us to touch the home directory, remove the flag. This
		 * will prevent any shared sessions to touch the home directory when they
		 * shouldn't.
		 */
		if ((vcspec->ioc_userflags & SMBV_HOME_ACCESS_OK) != SMBV_HOME_ACCESS_OK) {
			vcp->vc_flags &= ~SMBV_HOME_ACCESS_OK;							
		}		
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
	vcp->vc_flags |= (sspec->ioc_userflags & SMBV_USER_LAND_MASK);
	/* 
	 * Reset the username, password, domain, kerb client and service names. We
	 * never want to use any values left over from any previous calls.
	 */
    if (vcp->vc_username != NULL) {
        SMB_FREE(vcp->vc_username, M_SMBSTR);
    }
    if (vcp->vc_pass != NULL) {
        SMB_FREE(vcp->vc_pass, M_SMBSTR);
    }
    if (vcp->vc_domain != NULL) {
        SMB_FREE(vcp->vc_domain, M_SMBSTR);
    }
    if (vcp->vc_gss.gss_cpn != NULL) {
        SMB_FREE(vcp->vc_gss.gss_cpn, M_SMBSTR);
    }
	/* 
	 * Freeing the SPN will make sure we never use the hint. Remember that the 
	 * gss_spn contains the hint from the negotiate. We now require user
	 * land to send us a SPN, if we are going to use one.
	 */
    if (vcp->vc_gss.gss_spn != NULL) {
        SMB_FREE(vcp->vc_gss.gss_spn, M_SMBSTR);
    }
	vcp->vc_username = smb_strndup(sspec->ioc_user, sizeof(sspec->ioc_user));
	vcp->vc_pass = smb_strndup(sspec->ioc_password, sizeof(sspec->ioc_password));
	vcp->vc_domain = smb_strndup(sspec->ioc_domain, sizeof(sspec->ioc_domain));

	if ((vcp->vc_pass == NULL) || (vcp->vc_domain == NULL) || 
		(vcp->vc_username == NULL)) {
		error = ENOMEM;
		goto done;
	}

	/* GSS principal names are only set if we are doing kerberos or ntlmssp */
	if (sspec->ioc_gss_client_size) {
		vcp->vc_gss.gss_cpn = smb_memdupin(sspec->ioc_gss_client_name, sspec->ioc_gss_client_size);
	}
	vcp->vc_gss.gss_cpn_len = sspec->ioc_gss_client_size;
	vcp->vc_gss.gss_client_nt = sspec->ioc_gss_client_nt;

	if (sspec->ioc_gss_target_size) {
		vcp->vc_gss.gss_spn = smb_memdupin(sspec->ioc_gss_target_name, sspec->ioc_gss_target_size);
	}
	vcp->vc_gss.gss_spn_len = sspec->ioc_gss_target_size;
	vcp->vc_gss.gss_target_nt = sspec->ioc_gss_target_nt;
	if (!(sspec->ioc_userflags & SMBV_ANONYMOUS_ACCESS)) {
		SMB_LOG_AUTH("client size = %d client name type = %d\n", 
				   sspec->ioc_gss_client_size, vcp->vc_gss.gss_client_nt);
		SMB_LOG_AUTH("taget size = %d target name type = %d\n", 
				   sspec->ioc_gss_target_size, vcp->vc_gss.gss_target_nt);
	}
	
	error = smb_vc_ssnsetup(vcp);
	/* If no error then this virtual circuit has been authorized */
	if (error == 0) {
		smb_gss_ref_cred(vcp);
		vcp->vc_flags |= SMBV_AUTH_DONE;
	}

done:
	if (error) {
		/* 
		 * Authorization failed, reset any authorization
		 * information. This includes removing guest access,
		 * user name, password and the domain name. We should
		 * not every return these values after authorization
		 * fails.
		 */ 
		vcp->vc_flags &= ~(SMBV_GUEST_ACCESS | SMBV_PRIV_GUEST_ACCESS | 
						   SMBV_KERBEROS_ACCESS | SMBV_ANONYMOUS_ACCESS);
        if (vcp->vc_username) {
            SMB_FREE(vcp->vc_username, M_SMBSTR);
        }
        if (vcp->vc_pass) {
            SMB_FREE(vcp->vc_pass, M_SMBSTR);
        }
        if (vcp->vc_domain) {
            SMB_FREE(vcp->vc_domain, M_SMBSTR);
        }
        if (vcp->vc_gss.gss_cpn) {
            SMB_FREE(vcp->vc_gss.gss_cpn, M_SMBSTR);
        }
        if (vcp->vc_gss.gss_spn) {
            SMB_FREE(vcp->vc_gss.gss_spn, M_SMBSTR);
        }
        
		vcp->vc_gss.gss_spn_len = 0;
		vcp->vc_gss.gss_cpn_len = 0;
	}
	
	/* Release the reference and lock that smb_sm_lookupint took on the vcp */
	smb_vc_put(vcp, context);
	return error;
}

static void smb_share_free(struct smb_connobj *cp)
{
	struct smb_share *share = (struct smb_share *)cp;
	
	SMB_FREE(share->ss_name, M_SMBSTR);
	lck_mtx_destroy(&share->ss_stlock, ssst_lck_group);
	lck_mtx_destroy(&share->ss_shlock, ssst_lck_group);
	lck_mtx_destroy(&share->ss_fid_lock, fid_lck_grp);
	smb_co_done(SSTOCP(share));
	SMB_FREE(share, M_SMBCONN);
}

static void smb_share_gone(struct smb_connobj *cp, vfs_context_t context)
{
	struct smb_share *share = (struct smb_share *)cp;
	
	DBG_ASSERT(share);
	DBG_ASSERT(SSTOVC(share));
	DBG_ASSERT(SSTOVC(share)->vc_iod);
	smb_smb_treedisconnect(share, context);
}

void smb_share_ref(struct smb_share *share)
{
	smb_co_ref(SSTOCP(share));
}

void smb_share_rele(struct smb_share *share, vfs_context_t context)
{	
	smb_co_rele(SSTOCP(share), context);
}

/*
 * Allocate share structure and attach it to the given VC. The vcp
 * needs to be locked on entry. Share will be returned in unlocked state,
 * but will have a reference on it.
 */
static int
smb_share_create(struct smb_vc *vcp, struct smbioc_share *shspec,
				 struct smb_share **outShare, vfs_context_t context)
{
	struct smb_share *share;
    int i;
	
	/* Should never happen, but just to be safe */
	if (context == NULL)
		return ENOTSUP;
	
	SMB_MALLOC(share, struct smb_share *, sizeof(*share), M_SMBCONN, M_WAITOK | M_ZERO);
	if (share == NULL) {
		return ENOMEM;
	}
	share->ss_name = smb_strndup(shspec->ioc_share, sizeof(shspec->ioc_share));
	if (share->ss_name == NULL) {
		SMB_FREE(share, M_SMBCONN);
		return ENOMEM;
	}
	/* The smb_co_init routine locks the share and takes a reference */
	smb_co_init(SSTOCP(share), SMBL_SHARE, "smbss", vfs_context_proc(context));
	share->obj.co_free = smb_share_free;
	share->obj.co_gone = smb_share_gone;

    /* alloc FID mapping stuff */
    lck_mtx_init(&share->ss_fid_lock, fid_lck_grp, fid_lck_attr);
    for (i = 0; i < SMB_FID_TABLE_SIZE; i++) {
        LIST_INIT(&share->ss_fid_table[i].fid_list);
    }
    share->ss_fid_collisions = 0;
    share->ss_fid_inserted = 0;
    share->ss_fid_max_iter = 0;
    
    lck_mtx_init(&share->ss_shlock, ssst_lck_group, ssst_lck_attr);
	lck_mtx_init(&share->ss_stlock, ssst_lck_group, ssst_lck_attr);
	lck_mtx_lock(&share->ss_shlock);
	share->ss_mount = NULL;	/* Just to be safe clear it out */
	/* Set the default dead timer */
	share->ss_dead_timer = smbfs_deadtimer;
	lck_mtx_unlock(&share->ss_shlock);
	share->ss_tid = SMB_TID_UNKNOWN;
	share->ss_tree_id = SMB2_TID_UNKNOWN;

    /* unlock the share we no longer need the lock */
	smb_co_unlock(SSTOCP(share));
	smb_co_addchild(VCTOCP(vcp), SSTOCP(share));
	*outShare = share;
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
	/* At this point we have a locked vcp create the share */
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
	if (*shpp && (error == 0)) {
		shspec->ioc_optionalSupport = (*shpp)->optionalSupport;
        /* 
         * ioc_fstype will always be 0 at this time because ss_fstype is filled
         * in at mount time. 
         */
		shspec->ioc_fstype = (*shpp)->ss_fstype;
	}
	
	/* Release the reference that smb_sm_lookupint took on the vc */
	smb_vc_rele(vcp, context);
	return error;
}

int smb_vc_access(struct smb_vc *vcp, vfs_context_t context)
{	
	if (SMBV_HAS_GUEST_ACCESS(vcp))
		return(0);
	
	/* The smbfs_vnop_strategy routine has no context, we always allow these */
	if (context == NULL) {
		return(0);
	}
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
const char * smb_share_getpass(struct smb_share *share)
{
	DBG_ASSERT(SSTOVC(share));
	return smb_vc_getpass(SSTOVC(share));
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


