/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2015 Apple Inc. All rights reserved.
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
#include <netsmb/smb2_mc_support.h>
#include <smbclient/smbclient_internal.h>

extern uint32_t smbfs_deadtimer;

static struct smb_connobj smb_session_list;
static int smb_session_next = 1;	/* next unique id for Session */

extern struct linker_set sysctl_net_smb;

SYSCTL_DECL(_net_smb);

SYSCTL_NODE(_net, OID_AUTO, smb, CTLFLAG_RW, NULL, "SMB protocol");

static void smb_co_put(struct smb_connobj *cp, vfs_context_t context);

/*
 * The smb_co_lock, smb_co_unlock, smb_co_ref, smb_co_rel and smb_co_put deal
 * with the session_list, session and shares. So the session_list owns the session which owns the share.
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
		/* We got the lock, but  the session is going away, so unlock it return EBUSY */
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
		 * This can happen when we are doing a tree disconnect or a session log off.
		 * In the future we could fix the tree disconnect by only taking a reference
		 * on the session. Not sure what to do about the session. If we could solve those
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
	smb_co_init(&smb_session_list, SMBL_SESSION_LIST, "smb_session_list", current_proc());
	smb_co_unlock(&smb_session_list);
	return (0);
}

int smb_sm_done(void)
{
	if (smb_session_list.co_usecount > 1) {
		SMBERROR("%d connections still active\n", smb_session_list.co_usecount - 1);
		return (EBUSY);
	}
	/* XXX Q4BP why are we not iterating on smb_session_list here with SMBCO_FOREACH? */
	smb_co_done(&smb_session_list);
	return (0);
}

static void smb_sm_lock_session_list()
{
  	/*
	 * The smb_session_list never goes away so there is no way for smb_co_lock
	 * to fail in this case. 
	 */	
	KASSERT((smb_co_lock(&smb_session_list) == 0), ("smb_sm_lock_session_list: lock failed"));
}

static void smb_sm_unlock_session_list()
{
	smb_co_unlock(&smb_session_list);
}

/*
 * This routine will reset the session. When doing a reconnect we need to
 * keep some of the session information around. We only reset the information
 * that is required to do the reconnect.
 */
void smb_session_reset(struct smb_session *sessionp)
{
	/* 
	 * If these three flags were set keep them for the reconnect. Clear out 
	 * any other flags that may have been set in the original connection. 
	 */
	sessionp->session_hflags2 &= (SMB_FLAGS2_EXT_SEC | SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_UNICODE);
	
	sessionp->session_mid = 0;
	sessionp->session_low_pid = 1;

    sessionp->session_message_id = 1;
    
    /* leave session_misc_flags untouched as it has preferences flags */
    //sessionp->session_misc_flags = 0; 
    
    /* Save previous sessID for reconnects SessionSetup request */
    sessionp->session_prev_session_id = sessionp->session_session_id;
    sessionp->session_session_id = 0;

	sessionp->session_number = smb_session_next++;
    
	/* Reset the smb signing */
	smb_reset_sig(sessionp);
}

void smb_session_ref(struct smb_session *sessionp)
{
	smb_co_ref(SESSION_TO_CP(sessionp));
}

void smb_session_rele(struct smb_session *sessionp, vfs_context_t context)
{	
	smb_co_rele(SESSION_TO_CP(sessionp), context);
}

static void smb_session_put(struct smb_session *sessionp, vfs_context_t context)
{	
	smb_co_put(SESSION_TO_CP(sessionp), context);
}

int smb_session_lock(struct smb_session *sessionp)
{
	return smb_co_lock(SESSION_TO_CP(sessionp));
}

void smb_session_unlock(struct smb_session *sessionp)
{
	smb_co_unlock(SESSION_TO_CP(sessionp));
}

static void smb_session_free(struct smb_connobj *cp)
{
	struct smb_session *sessionp = (struct smb_session*)cp;
	
	smb_gss_rel_cred(sessionp);
    
	if (sessionp->session_iod)
		smb_iod_destroy(sessionp->session_iod);
	sessionp->session_iod = NULL;
    
    if (sessionp->negotiate_token) {
        SMB_FREE(sessionp->negotiate_token, M_SMBTEMP);
    }
    
    if (sessionp->NativeOS) {
        SMB_FREE(sessionp->NativeOS, M_SMBSTR);
    }
    
    if (sessionp->NativeLANManager) {
        SMB_FREE(sessionp->NativeLANManager, M_SMBSTR);
    }
    
    if (sessionp->session_username) {
        SMB_FREE(sessionp->session_username, M_SMBSTR);
    }
    
    if (sessionp->session_srvname) {
        SMB_FREE(sessionp->session_srvname, M_SMBSTR);
    }
    
    if (sessionp->session_localname) {
        SMB_FREE(sessionp->session_localname, M_SMBSTR);
    }
    
    if (sessionp->session_pass) {
        SMB_FREE(sessionp->session_pass, M_SMBSTR);
    }
    
    if (sessionp->session_domain) {
        SMB_FREE(sessionp->session_domain, M_SMBSTR);
    }
    
	if (sessionp->session_mackey) {
		SMB_FREE(sessionp->session_mackey, M_SMBTEMP);
    }
    
    if (sessionp->session_saddr) {
		SMB_FREE(sessionp->session_saddr, M_SONAME);
    }
    
    if (sessionp->session_laddr) {
		SMB_FREE(sessionp->session_laddr, M_SONAME);
    }
    
	smb_gss_destroy(&sessionp->session_gss);
    
	if (sessionp->throttle_info)
		throttle_info_release(sessionp->throttle_info);
	sessionp->throttle_info = NULL;
    
    smb_co_done(SESSION_TO_CP(sessionp));

    lck_mtx_destroy(&sessionp->session_stlock, session_st_lck_group);
    lck_mtx_destroy(&sessionp->session_model_info_lock, session_st_lck_group);
    lck_mtx_destroy(&sessionp->iod_quantum_lock, session_st_lck_group);

    smb2_mc_destroy(&sessionp->session_interface_table);
    
#if 0
    /* Message ID and credit checking debugging code */
    lck_mtx_destroy(&sessionp->session_mid_lock, session_st_lck_group);
#endif
    if (sessionp) {
        SMB_FREE(sessionp, M_SMBCONN);
    }
}

/*
 * Force reconnect on session
 */
int smb_session_force_reconnect(struct smb_session *sessionp)
{
	if (sessionp->session_iod) {
		smb_iod_request(sessionp->session_iod, SMBIOD_EV_FORCE_RECONNECT | SMBIOD_EV_SYNC, NULL);
    }

	return (0);
}

/*
 * Destroy session to server, invalidate shares linked with it.
 * Transport should be locked on entry.
 */
static int smb_session_disconnect(struct smb_session *sessionp)
{
	if (sessionp->session_iod)
		smb_iod_request(sessionp->session_iod, SMBIOD_EV_DISCONNECT | SMBIOD_EV_SYNC, NULL);
	return (0);
}

/*
 * Called when use count of session dropped to zero.
 * session should be locked on enter with LK_DRAIN.
 */
static void smb_session_gone(struct smb_connobj *cp, vfs_context_t context)
{
#pragma unused(context)
	struct smb_session *sessionp = (struct smb_session*)cp;
	smb_session_disconnect(sessionp);
}

static int smb_session_create(struct smbioc_negotiate *session_spec, 
						 struct sockaddr *saddr, struct sockaddr *laddr,
						 vfs_context_t context, struct smb_session **sessionpp)
{
	struct smb_session *sessionp;
	int error = 0;
	
	/* Should never happen, but just to be safe */
	if (context == NULL) {
		return ENOTSUP;
	}
	SMB_MALLOC(sessionp, struct smb_session *, sizeof(*sessionp), M_SMBCONN, M_WAITOK | M_ZERO);
	smb_co_init(SESSION_TO_CP(sessionp), SMBL_SESSION, "smb_session", vfs_context_proc(context));
	sessionp->obj.co_free = smb_session_free;
	sessionp->obj.co_gone = smb_session_gone;
	sessionp->session_number = smb_session_next++;
	sessionp->session_timo = SMB_DEFRQTIMO;
	sessionp->session_smbuid = SMB_UID_UNKNOWN;
	sessionp->session_tdesc = &smb_tran_nbtcp_desc;
	sessionp->session_seqno = 0;
	sessionp->session_mackey = NULL;
	sessionp->session_mackeylen = 0;
    sessionp->session_smb3_signing_key_len = 0;
    sessionp->session_smb3_encrypt_key_len = 0;
    sessionp->session_smb3_decrypt_key_len = 0;
	sessionp->session_saddr = saddr;
	sessionp->session_laddr = laddr;
	/* Remove any user setable items */
	sessionp->session_flags &= ~SMBV_USER_LAND_MASK;
	/* Now add the users setable items */
	sessionp->session_flags |= (session_spec->ioc_userflags & SMBV_USER_LAND_MASK);
	
	/* Now add the throttle info */
	sessionp->throttle_info = throttle_info_create();
#ifdef DEBUG_TURN_OFF_EXT_SEC
	sessionp->session_hflags2 = SMB_FLAGS2_KNOWS_LONG_NAMES;
#else // DEBUG_TURN_OFF_EXT_SEC
	sessionp->session_hflags2 = SMB_FLAGS2_KNOWS_LONG_NAMES | SMB_FLAGS2_EXT_SEC | SMB_FLAGS2_UNICODE;
#endif // DEBUG_TURN_OFF_EXT_SEC
	
	sessionp->session_uid = session_spec->ioc_ssn.ioc_owner;
	sessionp->session_gss.gss_asid = AU_ASSIGN_ASID;
	
	/* Amount of time to wait while reconnecting */
	sessionp->reconnect_wait_time = session_spec->ioc_ssn.ioc_reconnect_wait_time;	
	
    lck_mtx_init(&sessionp->session_credits_lock, session_credits_lck_group, session_credits_lck_attr);

    lck_mtx_init(&sessionp->session_stlock, session_st_lck_group, session_st_lck_attr);

    lck_mtx_init(&sessionp->session_model_info_lock, session_st_lck_group, session_st_lck_attr);
    lck_mtx_init(&sessionp->iod_quantum_lock, session_st_lck_group, session_st_lck_attr);
#if 0
    /* Message ID and credit checking debugging code */
    lck_mtx_init(&sessionp->session_mid_lock, session_st_lck_group, session_st_lck_attr);
#endif
	sessionp->session_srvname = smb_strndup(session_spec->ioc_ssn.ioc_srvname, sizeof(session_spec->ioc_ssn.ioc_srvname));
	if (sessionp->session_srvname)
		sessionp->session_localname = smb_strndup(session_spec->ioc_ssn.ioc_localname,  sizeof(session_spec->ioc_ssn.ioc_localname));
	if ((sessionp->session_srvname == NULL) || (sessionp->session_localname == NULL)) {
		error = ENOMEM;
	}

	sessionp->session_message_id = 1;
    sessionp->session_misc_flags = SMBV_HAS_FILEIDS;  /* assume File IDs supported */
    sessionp->session_server_caps = 0;
	sessionp->session_volume_caps = 0;
    
    lck_mtx_lock(&sessionp->session_model_info_lock);
    bzero(sessionp->session_model_info, sizeof(sessionp->session_model_info));
    lck_mtx_unlock(&sessionp->session_model_info_lock);

	if (!error)
		error = smb_iod_create(sessionp);
	if (error) {
		smb_session_put(sessionp, context);
		return error;
	}
	*sessionpp = sessionp;
	
	/* What versions of SMB are allowed? */
	if (session_spec->ioc_extra_flags & SMB_SMB1_ENABLED) {
		sessionp->session_misc_flags |= SMBV_NEG_SMB1_ENABLED;
	}
	
	if (session_spec->ioc_extra_flags & SMB_SMB2_ENABLED) {
		sessionp->session_misc_flags |= SMBV_NEG_SMB2_ENABLED;
	}
	
	if (session_spec->ioc_extra_flags & SMB_SMB3_ENABLED) {
		sessionp->session_misc_flags |= SMBV_NEG_SMB3_ENABLED;
	}

	/* Is signing required? */
	if (session_spec->ioc_extra_flags & SMB_SIGNING_REQUIRED) {
		sessionp->session_misc_flags |= SMBV_CLIENT_SIGNING_REQUIRED;

		/* What versions of SMB required signing? */
		if (session_spec->ioc_extra_flags & SMB_SMB1_SIGNING_REQ) {
			sessionp->session_misc_flags |= SMBV_SMB1_SIGNING_REQUIRED;
		}

		if (session_spec->ioc_extra_flags & SMB_SMB2_SIGNING_REQ) {
			sessionp->session_misc_flags |= SMBV_SMB2_SIGNING_REQUIRED;
		}

		if (session_spec->ioc_extra_flags & SMB_SMB3_SIGNING_REQ) {
			sessionp->session_misc_flags |= SMBV_SMB3_SIGNING_REQUIRED;
		}
	}
	
	/* Save client Guid */
	memcpy(sessionp->session_client_guid, session_spec->ioc_client_guid, sizeof(sessionp->session_client_guid));
    
    /* Set default max amount of time to wait for any response from server */
    if ((session_spec->ioc_max_resp_timeout != 0) &&
        (session_spec->ioc_max_resp_timeout <= 600)) {
        sessionp->session_resp_wait_timeout = session_spec->ioc_max_resp_timeout;
        SMBWARNING("session_resp_wait_timeout changed from default to %d \n", sessionp->session_resp_wait_timeout);
    }
    else {
        sessionp->session_resp_wait_timeout = SMB_RESP_WAIT_TIMO;
    }
    
    /* Adaptive Read/Write values */
    lck_mtx_lock(&sessionp->iod_quantum_lock);

    sessionp->iod_readSizes[0] = kSmallReadQuantumSize;
    sessionp->iod_readSizes[1] = kReadMediumQuantumSize;
    sessionp->iod_readSizes[2] = kLargeReadQuantumSize;

    sessionp->iod_writeSizes[0] = kSmallWriteQuantumSize;
    sessionp->iod_writeSizes[1] = kWriteMediumQuantumSize;
    sessionp->iod_writeSizes[2] = kLargeWriteQuantumSize;

    sessionp->iod_readQuantumSize = sessionp->iod_readSizes[1];
    sessionp->iod_readQuantumNumber = kQuantumMedNumber;
    sessionp->iod_writeQuantumSize = sessionp->iod_writeSizes[1];
    sessionp->iod_writeQuantumNumber = kQuantumMedNumber;

    lck_mtx_unlock(&sessionp->iod_quantum_lock);

    /* Init Multi Channel Interfaces table*/
    smb2_mc_init(& sessionp->session_interface_table);
    
    smb_sm_lock_session_list();
	smb_co_addchild(&smb_session_list, SESSION_TO_CP(sessionp));
	smb_sm_unlock_session_list();
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
 * If the search sa_family equal AF_NETBIOS and the session sa_family doesn't then we
 * can just compare, since its its not going to match. We never support sharing
 * a AF_NETBIOS with a non AF_NETBIOS connection.
 * 
 * Now that just leaves the cases were the session is connected using AF_NETBIOS and
 * the search sockaddr is either IPv4 or IPv6. We need to compare using the real
 * sockaddr that is inside the AF_NETBIOS sockaddr_nb structure.
 */
static int addressMatch(struct smb_session *sessionp, struct sockaddr *saddr)
{
	struct sockaddr *session_saddr = NULL;
	
	if ((sessionp == NULL) || (saddr == NULL)) {
		SMBERROR("sessionp or saddr is null \n");
		return FALSE;
	}
	
	session_saddr = sessionp->session_saddr;
	if (session_saddr == NULL) {
		SMBERROR("session_saddr is null \n");
		return FALSE;
	}

	if ((session_saddr->sa_family == AF_NETBIOS) && (saddr->sa_family != AF_NETBIOS)) {
		session_saddr = (struct sockaddr *)&((struct sockaddr_nb *)sessionp->session_saddr)->snb_addrin;
	}
	
    if ((session_saddr->sa_len == saddr->sa_len) && (memcmp(session_saddr, saddr, saddr->sa_len) == 0)) {
        SMB_LOG_AUTH("Address match for <%s> \n", (sessionp == NULL ? "null" : sessionp->session_srvname));
		return TRUE;
    }
	
	return FALSE;
}

static int dnsMatch(struct smb_session *sessionp, char *dns_name)
{
    size_t srvname_len = 0;
    size_t dns_len = 0;
    size_t compare_len = 0;
    char *session_end = NULL;
    char *dns_end = NULL;
    int dns_flag = 0;
    int session_flag = 0;
    bool result = FALSE;
    
    if ((sessionp == NULL) || (dns_name == NULL)) {
        SMBERROR("sessionp or dns_name is NULL \n");
        return result;
    }
    
    if (sessionp->session_srvname == NULL) {
        SMBERROR("sessionp->session_srvname is NULL \n");
        return result;
    }
    
    
    /* Radar-ID:27754583: Use the server name and dns name lengths that are passed to the function*/
    srvname_len = strnlen(sessionp->session_srvname, 255);
    if (srvname_len == 0) {
        SMBERROR("sessionp->session_srvname len is 0 \n");
        return result;
    }
    
    dns_len = strnlen(dns_name, 255);
    if (dns_len == 0) {
        SMBERROR("dns_name len is 0 \n");
        return result;
    }
    
    
    /* Radar-ID:27754583: Find the occurence of the first "." in the server name */
    session_end = strchr(sessionp->session_srvname, '.');
    if (session_end != NULL) {
        session_flag = 1;
        compare_len = session_end - sessionp->session_srvname;
        /* Radar-ID:27754583: need to cap the number of comparisons to 255 */
        if (compare_len > srvname_len) {
            compare_len = srvname_len;
        }
    }
    
    /* Radar-ID:27754583: Find the occurence of the first "." in the dns name */
    dns_end = strchr(dns_name, '.');
    if (dns_end != NULL) {
        dns_flag=1;
        compare_len = dns_end - dns_name;
        /* Radar-ID:27754583: need to cap the number of comparisons to 255 */
        if (compare_len > dns_len) {
            compare_len = dns_len;
        }
    }
    
    /* Radar-ID:27754583: compare the original strings if there is a "." is in both of them or if there is no "." in both of them. Otherwise look for the one that has a "." in its name and comapre only those length of characters in both the strings.(In COSDFS and COSDFS.APPLE.COM, check only the first six characters)
     */
#ifdef SMB_DEBUG
	SMBERROR("session_srvname <%s> dns_name <%s> dns_flag %d session_flag %d compare_len %zu \n",
			 sessionp->session_srvname, dns_name,
			 dns_flag, session_flag, compare_len);
#endif

    if ((dns_flag && !session_flag)) {
        dns_len= compare_len;
    }
    else if ((!dns_flag && session_flag)) {
        srvname_len = compare_len;
    }
    if (srvname_len != dns_len) {
            return result;
    }
    if (strncasecmp(sessionp->session_srvname, dns_name, srvname_len) == 0) {
            result = TRUE;
            return result;
    }
    
    return result;
}

/*
 * On success the session will have a reference taken and a lock.
 *
 * Only smb_sm_negotiate passes sockaddr, all other routines need to pass in a 
 * sessionp to search on.
 */
static int smb_sm_lookupint(struct sockaddr *sap,
                            uid_t owner,
                            char *username,
							uint32_t user_flags,
                            char *dns_name,
                            uint32_t *matched_dns,
                            uint32_t extra_flags,
                            struct smb_session **sessionpp)
{
	struct smb_session *sessionp, *tsessionp;
	int error;
	
	DBG_ASSERT(sessionpp);	/* Better have passed us a sessionpp */
tryagain:
	smb_sm_lock_session_list();
	error = ENOENT;
	SMBCO_FOREACH_SAFE(sessionp, &smb_session_list, tsessionp) {
        /* Reset dns match flag */
        if (matched_dns != NULL) {
            *matched_dns = 0;
        }
        
        if (*sessionpp && sessionp != *sessionpp) {
			continue;
        }
		else if (*sessionpp) {
			/* Found a match, lock it, we are done. */
			error = smb_session_lock(sessionp);
            if (error != 0) {
                /* Can happen with bad servers */
                SMBDEBUG("smb_session_lock returned error %d\n", error);
            }
			break;
		} else {
			/* 
			 * We should only get in here from the negotiate routine. We better 
			 * have a sock addr or thats a programming error.
			 */
			DBG_ASSERT(sap);
			
			/* Don't share a sessionp that hasn't been authenticated yet */
			if ((sessionp->session_flags & SMBV_AUTH_DONE) != SMBV_AUTH_DONE) {
				continue;
			}
			
            /* Do sock address structure match? */
			if (!addressMatch(sessionp, sap)) {
                /* Sock address do NOT match, do we have a DNS name to check? */
                if (dns_name == NULL) {
                    /* No DNS name to check, so not a match */
#ifdef SMB_DEBUG
					SMBERROR("dns_name is null \n");
#endif
                    continue;
                }
                else {
                    /* Check to see if the dns name matches */
                    if (!dnsMatch(sessionp, dns_name)) {
                        /* DNS name fails to match, so continue on */
                        continue;
                    }
                    else {
                        if (matched_dns != NULL) {
#ifdef SMB_DEBUG
							SMBERROR("dns names matched \n");
#endif
                            *matched_dns = 1;
                        }
                    }
                }
			}
            
            /* Sock Address or DNS name matches, do some more checking... */
            
			/* Must be the same owner */
			if (sessionp->session_uid != owner) {
				continue;
			}
			
            /* HiFi modes must match */
            if ((!(sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) && (extra_flags & SMB_HIFI_REQUESTED)) ||
                ((sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) && !(extra_flags & SMB_HIFI_REQUESTED))) {
#ifdef SMB_DEBUG
                SMBERROR("HiFi modes do not match \n");
#endif
                continue;
            }

            /* Ok we have a lock on the sessionp, any error needs to unlock it */
			error = smb_session_lock(sessionp);
			/*
			 * This session is going away, but it is currently block on the lock we
			 * hold for smb_session_list. We need to unlock the list and allow the session
			 * to be remove. This still may not be the session we were looking for so
			 * start the search again.
			 */
			if (error) {
				smb_sm_unlock_session_list();
				goto tryagain;
			}
			
			/* 
			 * The session must be active and not in reconnect, otherwise we should
			 * just skip this session.
			 */
			if ((sessionp->session_iod->iod_state != SMBIOD_ST_SESSION_ACTIVE) || 
				(sessionp->session_iod->iod_flags & SMBIOD_RECONNECT)) {
				SMBWARNING("Skipping %s because its down or in reconnect: flags = 0x%x state = 0x%x\n",
						   sessionp->session_srvname, sessionp->session_iod->iod_flags, sessionp->session_iod->iod_state);
				smb_session_unlock(sessionp);									
				error = ENOENT;
				continue;				
			}
			
			/*
			 * If they ask for authentication then the session needs to match that
			 * authentication or we need to keep looking. So here are the 
			 * scenarios we need to deal with here.
			 *
			 * 1. If they are asking for a private guest access and the session has
			 *    private guest access set then use this session. If either is set,
			 *    but not both then don't reuse the session.
			 * 2. If they are asking for a anonymous access and the session has
			 *    anonymous access set then use this session. If either is set,
			 *    but not both then don't reuse the session.
			 * 3. They are requesting kerberos access. If the current session isn't
			 *    using kerberos then don't reuse the sessionp.
			 * 4. They are requesting guest access. If the current session isn't
			 *    using guest then don't reuse the session.
			 * 4. They are using user level security. The session user name needs to
			 *	  match the one passed in.
			 * 4. They don't care. Always use the authentication of this session.
			 */
			if ((sessionp->session_flags & SMBV_SFS_ACCESS)) {
				/* We're guest no matter what the user says, just use this session */
				error = 0;
				break;
			} else if ((user_flags & SMBV_PRIV_GUEST_ACCESS) || (sessionp->session_flags & SMBV_PRIV_GUEST_ACCESS)) {
				if ((user_flags & SMBV_PRIV_GUEST_ACCESS) && (sessionp->session_flags & SMBV_PRIV_GUEST_ACCESS)) {
					error = 0;
					break;				
				} else {
					smb_session_unlock(sessionp);									
					error = ENOENT;
					continue;				
				}
			} else if ((user_flags & SMBV_ANONYMOUS_ACCESS) || (sessionp->session_flags & SMBV_ANONYMOUS_ACCESS)) {
				if ((user_flags & SMBV_ANONYMOUS_ACCESS) && (sessionp->session_flags & SMBV_ANONYMOUS_ACCESS)) {
					error = 0;
					break;				
				} else {
					smb_session_unlock(sessionp);									
					error = ENOENT;
					continue;				
				}
			} else if (user_flags & SMBV_KERBEROS_ACCESS) {
				if (sessionp->session_flags & SMBV_KERBEROS_ACCESS) {
					error = 0;
					break;				
				} else {
					smb_session_unlock(sessionp);									
					error = ENOENT;
					continue;				
				}
			} else if (user_flags & SMBV_GUEST_ACCESS) {
				if (sessionp->session_flags & SMBV_GUEST_ACCESS) {
					error = 0;
					break;				
				} else {
					smb_session_unlock(sessionp);									
					error = ENOENT;
					continue;				
				}
			} else if (username && username[0]) {
				if (sessionp->session_username && 
					((strncmp(sessionp->session_username, username, SMB_MAXUSERNAMELEN + 1)) == 0)) {
					error = 0;
					break;				
				} else {
					smb_session_unlock(sessionp);									
					error = ENOENT;
					continue;				
				}
			}
			error = 0;
			break;
		}
	}
    
	if (sessionp && !error) {
		smb_session_ref(sessionp);
		*sessionpp = sessionp;
	}
    
	smb_sm_unlock_session_list();
    
	return error;
}

int smb_sm_negotiate(struct smbioc_negotiate *session_spec,
                     vfs_context_t context, struct smb_session **sessionpp,
                     struct smb_dev *sdp, int searchOnly, uint32_t *matched_dns)
{
	struct smb_session *sessionp = NULL;
	struct sockaddr	*saddr = NULL, *laddr = NULL;
	int error;
    char *dns_name = NULL;

	if (session_spec == NULL) {
		/* Paranoid check */
		SMBERROR("session_spec is null \n");
		return EINVAL;
	}
	
	saddr = smb_memdupin(session_spec->ioc_kern_saddr, session_spec->ioc_saddr_len);
	if (saddr == NULL) {
		return ENOMEM;
	}

	if (session_spec->ioc_saddr_len < 3) {
		/* Paranoid check - Have to have at least sa_len and sa_family */
		SMBERROR("invalid ioc_saddr_len (%d) \n", session_spec->ioc_saddr_len);
		SMB_FREE(saddr, M_SMBDATA);
		return EINVAL;
	}
	
	*sessionpp = sessionp = NULL;

	if (session_spec->ioc_extra_flags & SMB_FORCE_NEW_SESSION) {
		error = ENOENT;	/* Force a new session */
	}
    else {
        /* Do we want to also try matching on the DNS name too? */
        if ((session_spec->ioc_extra_flags & SMB_MATCH_DNS) &&
            (strnlen(session_spec->ioc_dns_name, 255) > 0)) {
			/* Make sure dns name is null terminated */
			session_spec->ioc_dns_name[254] = 0;
            dns_name = session_spec->ioc_dns_name;
#ifdef SMB_DEBUG
			SMBERROR("Checking DNS name <%s> \n", dns_name);
#endif
        }
        
		error = smb_sm_lookupint(saddr,
                                 session_spec->ioc_ssn.ioc_owner,
                                 session_spec->ioc_user,
                                 session_spec->ioc_userflags,
                                 dns_name,
                                 matched_dns,
                                 session_spec->ioc_extra_flags,
                                 &sessionp);
	}
		
	if ((error == 0) || (searchOnly)) {
		SMB_FREE(saddr, M_SMBDATA);
		saddr = NULL;
		session_spec->ioc_extra_flags |= SMB_SHARING_SESSION;
	} else {
		/* NetBIOS connections require a local address */
		if (saddr->sa_family == AF_NETBIOS) {
			laddr = smb_memdupin(session_spec->ioc_kern_laddr, session_spec->ioc_laddr_len);
			if (laddr == NULL) {
				SMB_FREE(saddr, M_SMBDATA);
				saddr = NULL;
				return ENOMEM;
			}
		}
		/* If smb_session_create fails it will clean up saddr and laddr */
		error = smb_session_create(session_spec, saddr, laddr, context, &sessionp);
		if (error == 0) {
			/* Flags used to cancel the connection */
			sessionp->connect_flag = &sdp->sd_flags;
			error = smb_session_negotiate(sessionp, context);
			sessionp->connect_flag = NULL;
			if (error) /* Remove the lock and reference */
				smb_session_put(sessionp, context);
		}		
	}
	if ((error == 0) && (sessionp)) {
		/* 
		 * They don't want us to touch the home directory, remove the flag. This
		 * will prevent any shared sessions to touch the home directory when they
		 * shouldn't.
		 */
		if ((session_spec->ioc_userflags & SMBV_HOME_ACCESS_OK) != SMBV_HOME_ACCESS_OK) {
			sessionp->session_flags &= ~SMBV_HOME_ACCESS_OK;							
		}		
		*sessionpp = sessionp;
		smb_session_unlock(sessionp);
	}
	return error;
}

int smb_sm_ssnsetup(struct smb_session *sessionp, struct smbioc_setup *sspec, 
					vfs_context_t context)
{
	int error;

    /*
	 * Call smb_sm_lookupint to verify that the sessionp is still on the
	 * list. If not found then something really bad has happen. Log
	 * it and just return the error. If smb_sm_lookupint returns without 
	 * an error then the sessionp will be locked and a refcnt will be taken. 
	 */
	error = smb_sm_lookupint(NULL, 0, NULL, 0, NULL, NULL, 0, &sessionp);
	if (error) {
		SMBERROR("The session was not found: error = %d\n", error);
		return error;
	}
	
	if ((sessionp->session_flags & SMBV_AUTH_DONE) == SMBV_AUTH_DONE)
		goto done;	/* Nothing more to do here */

	/* Remove any user setable items */
	sessionp->session_flags &= ~SMBV_USER_LAND_MASK;
	/* Now add the users setable items */
	sessionp->session_flags |= (sspec->ioc_userflags & SMBV_USER_LAND_MASK);
	/* 
	 * Reset the username, password, domain, kerb client and service names. We
	 * never want to use any values left over from any previous calls.
	 */
    if (sessionp->session_username != NULL) {
        SMB_FREE(sessionp->session_username, M_SMBSTR);
    }
    if (sessionp->session_pass != NULL) {
        SMB_FREE(sessionp->session_pass, M_SMBSTR);
    }
    if (sessionp->session_domain != NULL) {
        SMB_FREE(sessionp->session_domain, M_SMBSTR);
    }
    if (sessionp->session_gss.gss_cpn != NULL) {
        SMB_FREE(sessionp->session_gss.gss_cpn, M_SMBSTR);
    }
	/* 
	 * Freeing the SPN will make sure we never use the hint. Remember that the 
	 * gss_spn contains the hint from the negotiate. We now require user
	 * land to send us a SPN, if we are going to use one.
	 */
    if (sessionp->session_gss.gss_spn != NULL) {
        SMB_FREE(sessionp->session_gss.gss_spn, M_SMBSTR);
    }
	sessionp->session_username = smb_strndup(sspec->ioc_user, sizeof(sspec->ioc_user));
	sessionp->session_pass = smb_strndup(sspec->ioc_password, sizeof(sspec->ioc_password));
	sessionp->session_domain = smb_strndup(sspec->ioc_domain, sizeof(sspec->ioc_domain));

	if ((sessionp->session_pass == NULL) || (sessionp->session_domain == NULL) || 
		(sessionp->session_username == NULL)) {
		error = ENOMEM;
		goto done;
	}

	/* GSS principal names are only set if we are doing kerberos or ntlmssp */
	if (sspec->ioc_gss_client_size) {
		sessionp->session_gss.gss_cpn = smb_memdupin(sspec->ioc_gss_client_name, sspec->ioc_gss_client_size);
	}
	sessionp->session_gss.gss_cpn_len = sspec->ioc_gss_client_size;
	sessionp->session_gss.gss_client_nt = sspec->ioc_gss_client_nt;

	if (sspec->ioc_gss_target_size) {
		sessionp->session_gss.gss_spn = smb_memdupin(sspec->ioc_gss_target_name, sspec->ioc_gss_target_size);
	}
	sessionp->session_gss.gss_spn_len = sspec->ioc_gss_target_size;
	sessionp->session_gss.gss_target_nt = sspec->ioc_gss_target_nt;
	if (!(sspec->ioc_userflags & SMBV_ANONYMOUS_ACCESS)) {
		SMB_LOG_AUTH("client size = %d client name type = %d\n", 
				   sspec->ioc_gss_client_size, sessionp->session_gss.gss_client_nt);
		SMB_LOG_AUTH("taget size = %d target name type = %d\n", 
				   sspec->ioc_gss_target_size, sessionp->session_gss.gss_target_nt);
	}
	
	error = smb_session_ssnsetup(sessionp);
	/* If no error then this session has been authorized */
	if (error == 0) {
		smb_gss_ref_cred(sessionp);
		sessionp->session_flags |= SMBV_AUTH_DONE;
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
		sessionp->session_flags &= ~(SMBV_GUEST_ACCESS | SMBV_PRIV_GUEST_ACCESS | 
						   SMBV_KERBEROS_ACCESS | SMBV_ANONYMOUS_ACCESS);
        if (sessionp->session_username) {
            SMB_FREE(sessionp->session_username, M_SMBSTR);
        }
        if (sessionp->session_pass) {
            SMB_FREE(sessionp->session_pass, M_SMBSTR);
        }
        if (sessionp->session_domain) {
            SMB_FREE(sessionp->session_domain, M_SMBSTR);
        }
        if (sessionp->session_gss.gss_cpn) {
            SMB_FREE(sessionp->session_gss.gss_cpn, M_SMBSTR);
        }
        if (sessionp->session_gss.gss_spn) {
            SMB_FREE(sessionp->session_gss.gss_spn, M_SMBSTR);
        }
        
		sessionp->session_gss.gss_spn_len = 0;
		sessionp->session_gss.gss_cpn_len = 0;
	}
	
	/* Release the reference and lock that smb_sm_lookupint took on the sessionp */
	smb_session_put(sessionp, context);
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
	DBG_ASSERT(SS_TO_SESSION(share));
	DBG_ASSERT(SS_TO_SESSION(share)->session_iod);
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
 * Allocate share structure and attach it to the given session. The sessionp
 * needs to be locked on entry. Share will be returned in unlocked state,
 * but will have a reference on it.
 */
static int
smb_share_create(struct smb_session *sessionp, struct smbioc_share *shspec,
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
	smb_co_addchild(SESSION_TO_CP(sessionp), SSTOCP(share));
	*outShare = share;
	return (0);
}

/*
 * If we already have a connection on the share take a reference and return.
 * Otherwise create the share, add it to the session list and then do a tree
 * connect.
 */
int smb_sm_tcon(struct smb_session *sessionp, struct smbioc_share *shspec,
			struct smb_share **shpp, vfs_context_t context)
{
	int error;
	
	*shpp = NULL;
	/*
	 * Call smb_sm_lookupint to verify that the sessionp is still on the
	 * list. If not found then something really bad has happen. Log
	 * it and just return the error. If smb_sm_lookupint returns without 
	 * an error then the sessionp will be locked and a refcnt will be taken. 
	 */
	error = smb_sm_lookupint(NULL, 0, NULL, 0, NULL, NULL, 0, &sessionp);
	if (error) {
		SMBERROR("The session was not found: error = %d\n", error);
		return error;
	}
	/* At this point we have a locked sessionp create the share */
    error = smb_share_create(sessionp, shspec, shpp, context);
    /*
     * We hold a lock and reference on the session. We are done with the session lock
     * so unlock the session but hold on to the session references.
     */
    smb_session_unlock(sessionp);				
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
	
    /* if multichannel smb, interogate server for additional interfaces */
    if (!error) {
        error = smb_session_query_net_if(sessionp);
        if (error) {
            SMBERROR("smb_session_query_net_if: error = %d\n", error);
        }
    }

    /* Release the reference that smb_sm_lookupint took on the session */
    smb_session_rele(sessionp, context);
    
	return error;
}

int smb_session_access(struct smb_session *sessionp, vfs_context_t context)
{
	/* 
	 * <28555880> If its Guest mounted and NOT a TM mount, and NOT a HiFi
     * mount, then no need for access check.
	 */
	if ((SMBV_HAS_GUEST_ACCESS(sessionp)) &&
		!(sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) &&
        !(sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
		return(0);
	}
	
	/* The smbfs_vnop_strategy routine has no context, we always allow these */
	if (context == NULL) {
		return(0);
	}
	if ((vfs_context_suser(context) == 0) || 
		(kauth_cred_getuid(vfs_context_ucred(context)) == sessionp->session_uid))
		return (0);
	return (EACCES);
}

int smb_session_negotiate(struct smb_session *sessionp, vfs_context_t context)
{
	return smb_iod_request(sessionp->session_iod,
			       SMBIOD_EV_NEGOTIATE | SMBIOD_EV_SYNC, context);
}

int smb_session_ssnsetup(struct smb_session *sessionp)
{
	return smb_iod_request(sessionp->session_iod,
 			       SMBIOD_EV_SSNSETUP | SMBIOD_EV_SYNC, NULL);
}

int smb_session_query_net_if(struct smb_session *sessionp)
{

    if (sessionp->session_flags & SMBV_MULTICHANNEL_ON) {
        /* MultiChannel is supported on this session */
        return smb_iod_request(sessionp->session_iod, SMBIOD_EV_QUERY_IF_INFO | SMBIOD_EV_SYNC, NULL);
    } else {
        /* MultiChannel is not supported on this session */
        return(0);
    }
}

static char smb_emptypass[] = "";

const char * smb_session_getpass(struct smb_session *sessionp)
{
	if (sessionp->session_pass)
		return sessionp->session_pass;
	return smb_emptypass;
}

/*
 * They are in share level security and the share requires
 * a password. Use the sessionp password always. On required for
 * Windows 98, should drop support someday.
 */
const char * smb_share_getpass(struct smb_share *share)
{
	DBG_ASSERT(SS_TO_SESSION(share));
	return smb_session_getpass(SS_TO_SESSION(share));
}

/*
 * The reconnect code needs to get a reference on the session. First make sure
 * this session is still in the list and no one has release it yet. If smb_sm_lookupint
 * finds it we will have it locked and a reference on it. Next make sure its
 * not being release. 
 */
int smb_session_reconnect_ref(struct smb_session *sessionp, vfs_context_t context)
{
	int error;
	
	error = smb_sm_lookupint(NULL, 0, NULL, 0, NULL, NULL, 0, &sessionp);
	if (error)
		return error;
	
	smb_session_unlock(sessionp);
	/* This session is being release just give up */
	if (sessionp->ss_flags & SMBO_GONE) {
		smb_session_rele(sessionp, context);
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
	smb_session_rele(iod->iod_session, iod->iod_context);
}

/*
 * The reconnect code takes a reference on the session. So we need to release that
 * reference, but if we are the last reference the smb_session_rele routine will 
 * attempt to destroy the session, which will then attempt to destroy the main iod
 * thread for the session. The reconnect code is running under the main iod thread,
 * which means we can't destroy the thread from that thread without hanging. So
 * start a new thread to just release the reference and do any cleanup required.
 * This will be a short live thread that just hangs around long enough to do the
 * work required to release the session reference.
 */
void smb_session_reconnect_rel(struct smb_session *sessionp)
{
	struct smbiod *iod = sessionp->session_iod;
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
			
			SMBERROR("Starting the reconnect session release thread failed! %d\n",
					 error);
			ts.tv_sec = 1;
			ts.tv_nsec = 0;
			msleep(iod, NULL, PWAIT | PCATCH, "smb_session_reconnect_rel", &ts);
		}
	} while (error);
	thread_deallocate(thread);
}


