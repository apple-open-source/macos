/*
 * Copyright (c) 2006 - 2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <mach/task.h>
#include <mach/host_special_ports.h>
#include <mach/mig_errors.h>
#include <mach/vm_map.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>

#include <sys/smb_apple.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_gss.h>
#include <netsmb/smb_gss_2.h>
#include <netsmb/smb_rq_2.h>

#include <sys/kauth.h>
#include <smbfs/smbfs.h>
#include <netsmb/smb_converter.h>

#include <gssd/gssd_mach_types.h>
#include <gssd/gssd_mach.h>
#include <sys/random.h>

extern host_priv_t host_priv_self(void);
extern kern_return_t host_get_special_port(host_priv_t, int, int, ipc_port_t *);

#define kauth_cred_getasid(cred) ((cred)->cr_audit.as_aia_p->ai_asid)
#define kauth_cred_getauid(cred) ((cred)->cr_audit.as_aia_p->ai_auid)

#define SMB2_KRB5_SESSION_KEYLEN 16

/*
 * smb_gss_negotiate:
 * This routine is called from smb_smb_negotiate to initialize the iod_gss
 * structure in a session for doing extened security. We asn.1 decode the security
 * blob passed in, and get the task special port for gssd.
 */
int 
smb_gss_negotiate(struct smbiod *iod, vfs_context_t context)
{
	struct smb_gss *gp = &iod->iod_gss;
	mach_port_t gssd_host_port;
	uid_t uid;
	kauth_cred_t cred;
	kern_return_t kr;
    
	if (IPC_PORT_VALID(iod->iod_session->gss_mp))
		return 0;
	
	DBG_ASSERT(context);
	/* Should never happen, but just in case */
	if (context == NULL)
		return EPIPE;

    if (gp->gss_spn != NULL) {
        SMB_FREE(gp->gss_spn, M_SMBTEMP);
    }
	
	/*
	 * Get a mach port to talk to gssd.
	 * gssd lives in the root bootstrap, so we call gssd's lookup routine
	 * to get a send right to talk to a new gssd instance that launchd has launched
	 * based on the cred's uid and audit session id.
	 */
	kr = host_get_gssd_port(host_priv_self(), &gssd_host_port);
	if (kr != KERN_SUCCESS) {
		SMBERROR("Can't get gssd port, status %x (%d)\n", kr, kr);
		return EPIPE;
	}
	if (!IPC_PORT_VALID(gssd_host_port)) {
		SMBERROR("gssd port not valid\n");
		return EPIPE;
	}

	cred = vfs_context_ucred(context);
	if (gp->gss_asid == AU_ASSIGN_ASID)
		gp->gss_asid = kauth_cred_getasid(cred);

	uid = kauth_cred_getauid(cred);
	if (uid == AU_DEFAUDITID)
		uid = kauth_cred_getuid(cred);

	kr = mach_gss_lookup(gssd_host_port, uid, gp->gss_asid, &iod->iod_session->gss_mp);
	if (kr != KERN_SUCCESS || !IPC_PORT_VALID(iod->iod_session->gss_mp)) {
		if (kr != KERN_SUCCESS)
			SMBERROR("mach_gss_lookup failed: status %x (%d)\n", kr, kr);
		else
			SMBERROR("Port is %s\n", iod->iod_session->gss_mp == IPC_PORT_DEAD ? "dead" : "null");

		return EPIPE;
	}
	return 0;
}

/*
 * smb_gss_reset:
 *
 * Reset in case we need to reconnect or reauth after an error
 */
__unused static void
smb_gss_reset(__unused struct smb_gss *gp)
{
	if (gp->gss_token != NULL) {
        SMB_FREE(gp->gss_token, M_SMBTEMP);
    }
	gp->gss_tokenlen = 0;
	gp->gss_ctx = 0;
	gp->gss_cred = 0;
	gp->gss_major = 0;
	gp->gss_minor = 0;
}

/*
 * smb_gss_destroy:
 *
 * Cleanup a struct smb_gss
 */
void
smb_gss_destroy(struct smb_gss *gp)
{
	
    if (gp->gss_cpn) {
        SMB_FREE(gp->gss_cpn, M_SMBTEMP);
    }
    if (gp->gss_cpn_display) {
        SMB_FREE(gp->gss_cpn_display, M_SMBTEMP);
    }
    if (gp->gss_spn) {
        SMB_FREE(gp->gss_spn, M_SMBTEMP);
    }
    if (gp->gss_token) {
        SMB_FREE(gp->gss_token, M_SMBTEMP);
    }
	bzero(gp, sizeof(struct smb_gss));
}

/*
 * The token that is sent and received in the gssd upcall
 * has unbounded variable length.  Mach RPC does not pass
 * the token in-line.  Instead it uses page mapping to handle
 * these parameters.  This function allocates a VM buffer
 * to hold the token for an upcall and copies the token
 * (received from the client) into it.  The VM buffer is
 * marked with a src_destroy flag so that the upcall will
 * automatically de-allocate the buffer when the upcall is
 * complete.
 */
static void
gss_mach_alloc_buffer(u_char *buf, uint32_t buflen, vm_map_copy_t *addr)
{
	kern_return_t kr;
	vm_offset_t kmem_buf;
	vm_size_t tbuflen;

	*addr = NULL;
	if (buf == NULL || buflen == 0)
		return;

	tbuflen = round_page(buflen);
    if (tbuflen < buflen) {
        SMBERROR("gss_mach_alloc_buffer: invalid buflen %u\n", buflen);
        return;
    }
	kr = vm_allocate(ipc_kernel_map, &kmem_buf, tbuflen, VM_FLAGS_ANYWHERE);
	if (kr != 0) {
		SMBERROR("gss_mach_alloc_buffer: vm_allocate failed\n");
		return;
	}

	kr = vm_map_wire(ipc_kernel_map, vm_map_trunc_page(kmem_buf,
                    vm_map_page_mask(ipc_kernel_map)),
                    vm_map_round_page(kmem_buf + tbuflen,
                    vm_map_page_mask(ipc_kernel_map)),
                    VM_PROT_READ|VM_PROT_WRITE, FALSE);

	if (kr != 0) {
		SMBERROR("gss_mach_alloc_buffer: vm_map_wire failed kr = %d\n", kr);
        vm_deallocate(ipc_kernel_map, kmem_buf, tbuflen);
		return;
	}

	bcopy(buf, (void *) kmem_buf, buflen);

	kr = vm_map_unwire(ipc_kernel_map,
        vm_map_trunc_page(kmem_buf, vm_map_page_mask(ipc_kernel_map)),
        vm_map_round_page(kmem_buf + tbuflen, vm_map_page_mask(ipc_kernel_map)),
        FALSE);

	if (kr != 0) {
		SMBERROR("gss_mach_alloc_buffer: vm_map_unwire failed kr = %d\n", kr);
		return;
	}

	kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t) kmem_buf, 
					   (vm_map_size_t) buflen, TRUE, addr);
	if (kr != 0) {
		SMBERROR("gss_mach_alloc_buffer: vm_map_copyin failed kr = %d\n", kr);
		return;
	}
}

/*
 * Here we handle a token received from the gssd via an upcall.
 * The received token resides in an allocate VM buffer.
 * We copy the token out of this buffer to a chunk of malloc'ed
 * memory of the right size, then de-allocate the VM buffer.
 */
static int
gss_mach_vmcopyout(vm_map_copy_t in, uint32_t len, u_char *out)
{
	vm_map_offset_t map_data;
	vm_offset_t data;
	int error;
    
    if (!out) {
        SMBERROR("No MEM!!");
        return ENOMEM;
    }
	
	error = vm_map_copyout(ipc_kernel_map, &map_data, in);
	if (error)
		return (error);
	
	data = CAST_DOWN(vm_offset_t, map_data);
	bcopy((void *) data, out, len);
	vm_deallocate(ipc_kernel_map, data, len);
	
	return (0);
}

/*
 * This is a tempory hack until vm_map_copy_discard is available to kexts
 */
static void
gss_mach_vm_map_copy_discard(vm_map_copy_t copy, mach_msg_type_number_t len)
{
	vm_map_offset_t map_data;
	
	if (vm_map_copyout(ipc_kernel_map, &map_data, copy))
		return;
	vm_deallocate(ipc_kernel_map, CAST_DOWN(vm_offset_t, map_data), (vm_size_t)len);
}

/*
 * Wrapper to make mach up call, using the parameters in the smb_gss structure
 * and the supplied uid. (Should be the session_uid field of the enclosing session).
 */
static kern_return_t
smb_gss_init(struct smbiod *iod, uid_t uid, uint32_t tryGuestWithIntegFlag)
{
	struct smb_gss *cp = &iod->iod_gss;
    struct smb_session *sessionp = iod->iod_session;
	uint32_t gssd_flags = GSSD_NO_DEFAULT;
	uint32_t flags = GSSD_MUTUAL_FLAG | GSSD_DELEG_POLICY_FLAG;
	kern_return_t kr;
	gssd_byte_buffer okey = NULL;
	int retry_cnt = 0;
	vm_map_copy_t itoken = NULL;
	vm_map_copy_t cpn = NULL;
	vm_map_copy_t spn = NULL;
	gssd_byte_buffer otoken = NULL;
	mach_msg_type_number_t otokenlen;
	mach_msg_type_number_t keylen;
	int error = 0;
	char display_name[MAX_DISPLAY_STR];
	gssd_mechtype mechtype;	

	if (!IPC_PORT_VALID(sessionp->gss_mp)) {
		SMBWARNING("smb_gss_init: gssd port not valid\n");
		goto out;
	}

	if (cp->gss_tokenlen > 0)
		gss_mach_alloc_buffer(cp->gss_token, cp->gss_tokenlen, &itoken);
	if (cp->gss_cpn_len > 0)
		gss_mach_alloc_buffer(cp->gss_cpn, cp->gss_cpn_len, &cpn);
	if (cp->gss_spn_len > 0)
		gss_mach_alloc_buffer(cp->gss_spn, cp->gss_spn_len, &spn);
	
	/* lha says we should set this bit when doing anonymous */
	if (sessionp->session_flags & SMBV_ANONYMOUS_ACCESS) {
		flags |= GSSD_ANON_FLAG;
	}
    
    /*
     * GSSD_INTEG_FLAG will set the "Negotiate Always Sign" bit in the
     * negotiate flags during NTLM exchange. Microsoft servers with June 2019
     * update now requires this bit be set else you get
     * STATUS_INVALID_PARAMETER when you try to log in. Now we set this flag
     * for all SMB versions instead of just SMB 2/3
     */
    if (!(sessionp->session_flags & SMBV_ANONYMOUS_ACCESS) &&
        !(sessionp->session_flags & SMBV_GUEST_ACCESS)) {
        flags |= GSSD_INTEG_FLAG;
    }

    /* Try Guest log in with GSSD_INTEG_FLAG set (Probably a Windows Server) */
    if ((sessionp->session_flags & SMBV_GUEST_ACCESS) &&
        (tryGuestWithIntegFlag == 1)) {
        flags |= GSSD_INTEG_FLAG;
    }

	/* The server doesn't support NTLMSSP, send RAW NTLM */
	if (sessionp->session_flags & SMBV_RAW_NTLMSSP) {
		mechtype = GSSD_NTLM_MECH;
	}
    else {
		mechtype = GSSD_SPNEGO_MECH;
	}
	
retry:
	*display_name = '\0';
	kr = mach_gss_init_sec_context_v2(
					  sessionp->gss_mp,
					  mechtype,
					  (gssd_byte_buffer) itoken, (mach_msg_type_number_t) cp->gss_tokenlen,
					  uid,
					  cp->gss_client_nt,
					  (gssd_byte_buffer) cpn,
					  (mach_msg_type_number_t) cp->gss_cpn_len,
					  cp->gss_target_nt,
					  (gssd_byte_buffer) spn,
					  (mach_msg_type_number_t) cp->gss_spn_len,
					  flags,
					  &gssd_flags,
					  &cp->gss_ctx,
					  &cp->gss_cred,
					  &cp->gss_rflags,		       
					  &okey,  &keylen,
					  &otoken, (mach_msg_type_number_t *) &otokenlen,
					  display_name,
					  &cp->gss_major,
					  &cp->gss_minor);
					  
	if (kr != 0) {
		SMBERROR("smb_gss_init: mach_gss_init_sec_context failed: %x %d\n", kr, kr);
		if (kr == MIG_SERVER_DIED && cp->gss_cred == 0 &&
			retry_cnt++ < GSS_MACH_MAX_RETRIES) {
			if (cp->gss_tokenlen > 0)
				gss_mach_alloc_buffer(cp->gss_token, cp->gss_tokenlen, &itoken);
			if (cp->gss_cpn_len > 0)
				gss_mach_alloc_buffer(cp->gss_cpn, cp->gss_cpn_len, &cpn);
			if (cp->gss_spn_len > 0)
				gss_mach_alloc_buffer(cp->gss_spn, cp->gss_spn_len, &spn);
			goto retry;
		}
		goto out;
	}

    if (keylen > 0) {
        if (iod->iod_flags & SMBIOD_ALTERNATE_CHANNEL) {
            /* Alternate channel: Derive SMB 3 channel signing key */
            if (SMBV_SMB3_OR_LATER(sessionp)) {
                iod->iod_mackeylen = keylen;
                SMB_MALLOC(iod->iod_mackey, uint8_t *,
                           iod->iod_mackeylen, M_SMBTEMP, M_WAITOK);

                error = gss_mach_vmcopyout((vm_map_copy_t) okey,
                                           iod->iod_mackeylen,
                                           iod->iod_mackey);
                if (error) {
                    SMBERROR("gss_mach_vmcopyout returned %d.\n", error);
                    gss_mach_vm_map_copy_discard((vm_map_copy_t)otoken, otokenlen);
                    goto out;
                }

                /*
                 * MS-SMB2 3.2.1.3 Per Session
                 * "Session.SessionKey: the first 16 bytes of the cryptographic key
                 * for this authenticated context...."
                 *
                 * So we must truncate the session key if required.
                 */
                if (sessionp->session_flags & SMBV_SMB2) {
                    if (iod->iod_mackeylen > SMB2_KRB5_SESSION_KEYLEN) {
                        iod->iod_mackeylen = SMB2_KRB5_SESSION_KEYLEN;
                    }
                }
            }
        }
        else {
            /*
             * Main channel
             * Free any old key and reset the sequence number
             */
            smb_reset_sig(sessionp);
            sessionp->session_mackeylen = keylen;
            SMB_MALLOC(sessionp->session_mackey, uint8_t *,
                       sessionp->session_mackeylen, M_SMBTEMP, M_WAITOK);
            error = gss_mach_vmcopyout((vm_map_copy_t) okey,
                                       sessionp->session_mackeylen,
                                       sessionp->session_mackey);
            if (error) {
                gss_mach_vm_map_copy_discard((vm_map_copy_t)otoken, otokenlen);
                goto out;
            }
            
            /*
             * MS-SMB2 3.2.1.3 Per Session
             * "Session.SessionKey: the first 16 bytes of the cryptographic key
             * for this authenticated context...."
             *
             * So we must truncate the session key if required.
             * See <rdar://problem/13591834>.
             */
            if (sessionp->session_flags & SMBV_SMB2) {
                if (sessionp->session_mackeylen > SMB2_KRB5_SESSION_KEYLEN) {
                    sessionp->session_mackeylen = SMB2_KRB5_SESSION_KEYLEN;
                }
            }

            SMBDEBUG("%s keylen = %d seqno = %d\n", sessionp->session_srvname, keylen, sessionp->session_seqno);
            smb_hexdump(__FUNCTION__, "setting session_mackey = ", sessionp->session_mackey, sessionp->session_mackeylen);
            /*
             * Windows expects the sequence number to restart once we get a signing
             * key. They expect this to happen once the client creates a authorization
             * token blob to send to the server. This was we can validate the servers
             * response. When doing Kerberos and now NTLMSSP we don't get the signing
             * key until after the gss mech has completed. Not sure how to really
             * fix this issue, but for now we just reset the sequence number as if
             * we had the key when the last round went out.
             */
            sessionp->session_seqno = 2;
        }
	}

	/* If we're done, see if the server is mapping everybody to guest */
	if (SMB_GSS_COMPLETE(cp) && (gssd_flags & GSSD_GUEST_ONLY)) {
		sessionp->session_flags |= SMBV_SFS_ACCESS;
		SMBDEBUG("NTLMSSP simple file sharing\n");
	}

	/* Free context token used as input */
    if (cp->gss_token != NULL) {
        SMB_FREE(cp->gss_token, M_SMBTEMP);
    }
	cp->gss_tokenlen = 0;
	
	if (otokenlen > 0) {
		SMB_MALLOC(cp->gss_token, uint8_t *, otokenlen, M_SMBTEMP, M_WAITOK);
		error = gss_mach_vmcopyout((vm_map_copy_t) otoken, otokenlen, cp->gss_token);
		if (error)
			goto out;
		cp->gss_tokenlen = otokenlen;
	}

	if (cp->gss_cpn_display == NULL && display_name[0]) {
		size_t len = strnlen(display_name, MAX_DISPLAY_STR);
		SMB_MALLOC(cp->gss_cpn_display, char *, len, M_SMBTEMP, M_WAITOK);
		if (cp->gss_cpn_display)
			strlcpy(cp->gss_cpn_display, display_name, len);
		SMBDEBUG("Received display name %s\n", display_name);
	}
	
	/*
	 * We have a gss error, could be the Kerberos creditials doesn't exist yet or
	 * has expired. If we are in reconnect then we may need to wait for the user
	 * to correct the problem. So lets return EAGAIN so the reconnect code can 
	 * try again later.
	 */
    if (SMB_GSS_ERROR(cp) && (iod->iod_flags & SMBIOD_RECONNECT))
		return EAGAIN;

	return (0);
	
out:
	SMB_FREE(cp->gss_token, M_SMBTEMP);
	cp->gss_tokenlen = 0;
	
	return (EAUTH);
}

/*
 * smb_gss_session_caps:
 *
 * Given a session, determine our capabilities to send to the server
 * as part of "ssandx" message. We now call the general routine smb_session_caps
 * handle the basic items and add that we are doing extended security.
 */
static uint32_t smb_gss_session_caps(struct smb_session *sessionp)
{	
	return (smb_session_caps(sessionp) | SMB_CAP_EXT_SECURITY);	
}

/*
 * smb_gss_alt_ch_session_setup:
 *
 * Negotiate Session-Setup on alternate channel (as part of multichanneling).
 */
int
smb_gss_alt_ch_session_setup(struct smbiod *iod)
{
    struct smb_session *sessionp = iod->iod_session;
    int error = 0;
    uint32_t caps;
    uint16_t action = 0;
    uint32_t derive_key = 0;

    /*
     * Multichannel can not work with Guest logins on the main channel.
     * Alt channels are set up by using the signing key from the main channel
     * on the Session Setup requests, but Guest logins have no signing keys.
     */
    
    /* Get our caps from the session. N.B. Seems only Samba uses this */
    caps = smb_gss_session_caps(sessionp);

    do {
        /* Call gss to create a security blob */
        error = smb_gss_init(iod, sessionp->session_uid, 0);
        
        /* No token to send or error so just break out */
        if (error || (SMB_GSS_ERROR(&iod->iod_gss))) {
            SMB_LOG_AUTH("GSSD extended security error = %d gss_major = %d gss_minor = %d\n",
                       error, iod->iod_gss.gss_major, iod->iod_gss.gss_minor);

            /* Always return EAUTH unless we want the reconnect code to try again */
            if (error != EAGAIN)
                error = EAUTH;
            break;
        }
        
        // exchange session setups with the server
        if (iod->iod_gss.gss_tokenlen) {
            error = smb_gss_ssandx(iod, caps, &action, iod->iod_context);
            if (error) {
                break;
            }
        }
    } while (SMB_GSS_CONTINUE_NEEDED(&iod->iod_gss));
    
    if ((error == 0) && !SMBV_HAS_GUEST_ACCESS(sessionp)
        && !SMBV_HAS_ANONYMOUS_ACCESS(sessionp) &&
        (action & SMB_ACT_GUEST)) {
        /*
         * We wanted to only login the users as guest if they ask to be login as
         * guest. Window system will login any bad user name as guest if guest is
         * turn on.
         * The first problem here is with XPHome. XPHome doesn't care if the
         * user is real or made up, it always logs them is as guest. We now have
         * a way to tell if Simple File Sharing (XPHome) is being used, so this
         * is no longer an issue.
         *
         * The second problem here is with Vista. Vista will log you in as guest
         * if your account has no password. The problem here is we don't always
         * know if the user provided a password or not. We could have the Network
         * Authorization Helper tell us, but this would not work in the cache
         * case.
         *
         * The user wanted authenticated access but got guest access, log them
         * out and return EAUTH.
         */
        SMBWARNING("Got guest access, but wanted real access, logging off.\n");
        error = EAUTH;
    }
    
    if (!error) {
        if ((SESSION_CAPS(sessionp) & SMB_CAP_EXT_SECURITY)) {
            if (sessionp->session_flags & SMBV_SMB2) {
                /* Not in reconnect, its now safe to start up crediting */
                smb2_rq_credit_start(iod, 0);
            }
        }
    }

    /*
     * Should we derive signing/sealing keys?
     * 1. Has to be no error and SMB v3.x or later
     * 2. Dont need to derive keys if SMB v3.0.2 and Guest or Anonymous login
     * 3. SMB v3.1.1 needs the signing key for Guest/Anonymous for the
     *    pre auth integrity check
     */
    if ((error == 0) && SMBV_SMB3_OR_LATER(sessionp)) {
        if (sessionp->session_flags & SMBV_SMB311) {
            derive_key = 1;
        }
        else {
            if (!SMBV_HAS_GUEST_ACCESS(sessionp) &&
                !SMBV_HAS_ANONYMOUS_ACCESS(sessionp)) {
                derive_key = 1;
            }
        }
    }

    /* Derive SMB 3 keys */
    if (derive_key == 1) {
        error = smb3_derive_channel_keys(iod);
        if (error) {
            SMBERROR("smb3_derive_channel_keys returned %d. id: %u \n",
                     error, iod->iod_id);
            /* For alt channel, dont logoff or clear the signature keys */
            //(void)smb_smb_ssnclose(sessionp, iod->iod_context);
            //smb_reset_sig(sessionp);
            return(EAUTH);
        }
        
        if (iod->iod_sess_setup_reply != NULL) {
            /*
             * Need to verify last session setup reply from server.
             * This might be due to SMB v3.1.1 pre auth integrity
             * check or SMB 3.x.x multichannel
             */
            error = smb3_verify_session_setup(sessionp, iod,
                                              iod->iod_sess_setup_reply,
                                              iod->iod_sess_setup_reply_len);
            
            /* Free the saved Session Setup reply */
            SMB_FREE(iod->iod_sess_setup_reply, M_SMBTEMP);
            iod->iod_sess_setup_reply = NULL;

            if (error) {
                SMBERROR("smb3_verify_session_setup returned %d. id: %u\n",
                         error, iod->iod_id);
                /* For alt channel, dont logoff or clear the signature keys */
                //(void)smb_smb_ssnclose(sessionp, iod->iod_context);
                //smb_reset_sig(sessionp);
                error = EAUTH;
           }
        }
    }

    return error;
}


/*
 * smb_gss_ssandx:
 *
 * Send a session setup and x message on session with cred and caps
 */
int
smb1_gss_ssandx(struct smb_session *sessionp, uint32_t caps, uint16_t *action,
                vfs_context_t context)
{
	struct smb_rq *rqp = NULL;
	struct smb_gss *gp = &sessionp->session_iod->iod_gss;  // valid for smb1 (single channel)
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t		wc;
	uint16_t	bc, toklen;
	int			error;
	uint32_t	tokenlen;
	uint32_t	maxtokenlen;
	uint8_t *	tokenptr;
	
#ifdef SMB_DEBUG
	/* For testing use a smaller max size */
	maxtokenlen = 2048;
#else // SMB_DEBUG
	/* Get the max blob size we can send in SetupAndX message */
	maxtokenlen = sessionp->session_txmax - (SMB_HDRLEN + SMB_SETUPXRLEN);
#endif // SMB_DEBUG

	tokenptr = gp->gss_token;	/* Get the start of the Kerberos blob */
	do {
		uint16_t maxtx = sessionp->session_txmax;
		if (rqp)	/* If we are looping then release it, before getting it again */
			smb_rq_done(rqp);

		/* Allocate the request form a session setup and x */
		error = smb_rq_alloc(SESSION_TO_CP(sessionp), SMB_COM_SESSION_SETUP_ANDX, 0, context, &rqp);
		if (error)
			break;
		/* Fill the request with the required parameters */
		smb_rq_wstart(rqp);
        smb_rq_getrequest(rqp, &mbp);
		mb_put_uint8(mbp, 0xff);
		mb_put_uint8(mbp, 0);
		mb_put_uint16le(mbp, 0);
		mb_put_uint16le(mbp, maxtx);
		mb_put_uint16le(mbp, sessionp->session_sopt.sv_maxmux);
		mb_put_uint16le(mbp, sessionp->session_number);
		mb_put_uint32le(mbp, sessionp->session_sopt.sv_skey);
		/* Get the max size we can send in one SetupAndX message */
		tokenlen = (gp->gss_tokenlen > maxtokenlen) ? maxtokenlen : gp->gss_tokenlen;
		mb_put_uint16le(mbp, tokenlen);
		mb_put_uint32le(mbp, 0);		/* reserved */
		mb_put_uint32le(mbp, caps);		/* our caps */
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		/* SPNEGO blob */
		mb_put_mem(mbp, (caddr_t) tokenptr, tokenlen, MB_MSYSTEM);
		smb_put_dstring(mbp, SMB_UNICODE_STRINGS(sessionp), SMBFS_NATIVEOS, sizeof(SMBFS_NATIVEOS), NO_SFM_CONVERSIONS);	/* Native OS */
		smb_put_dstring(mbp, SMB_UNICODE_STRINGS(sessionp), SMBFS_LANMAN, sizeof(SMBFS_LANMAN), NO_SFM_CONVERSIONS);	/* LAN Mgr */
		smb_rq_bend(rqp);
		/* Send the request and check for reply */
		error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);
		/* Move the pointer to the next offset in the blob */
		tokenptr += tokenlen;
		/* Subtract the size we have already sent */
		gp->gss_tokenlen -= tokenlen;
		/* Save the servers session identifier if not already set. */
		if ((error == EAGAIN) && (sessionp->session_smbuid == 0))
			sessionp->session_smbuid = rqp->sr_rpuid;
	
	} while (gp->gss_tokenlen && (error == EAGAIN));
	
	/* Free the gss spnego token that we sent */
	SMB_FREE(gp->gss_token, M_SMBTEMP);
	gp->gss_tokenlen = 0;
	gp->gss_smb_error = error;	/* Hold on to the last smb error returned */
	/* EAGAIN is not  really an  error, reset it to no error */
	if (error == EAGAIN) {
		error = 0;
	}
	/* At this point error should have the correct error, we only support NTStatus code with extended security */
	if (error) {
		SMB_LOG_AUTH("Extended security authorization failed! %d\n", error);
		goto bad;
	}
		
	/*
	 * Save the servers session identifier. Seems Samba will give us a new one for
	 * every loop of a SetUpAndX NTLMSSP response. Windows server just return
	 * the same one every time. We assume here the last one is the one we 
	 * should always use. Seems to make Samba work correctly.
	 */
	sessionp->session_smbuid = rqp->sr_rpuid;
	
	/* Get the reply  and decode the result */
	smb_rq_getreply(rqp, &mdp);
	error = md_get_uint8(mdp, &wc);
	if (error)
		goto bad;
	if (wc != 4) {
		error = EBADRPC;
		goto bad;
	}
	md_get_uint8(mdp, NULL);	/* secondary cmd */
	md_get_uint8(mdp, NULL);	/* mbz */
	md_get_uint16le(mdp, NULL);	/* andxoffset */
	md_get_uint16le(mdp, action);	/* action */
	md_get_uint16le(mdp, &toklen);	/* Spnego token length */
	gp->gss_tokenlen = toklen;
	md_get_uint16le(mdp, &bc); /* remaining bytes */
	/*
	 * Set the gss token from the server
	 */
    SMB_MALLOC(gp->gss_token, uint8_t *, gp->gss_tokenlen, M_SMBTEMP, M_WAITOK);
    if (gp->gss_token == NULL)
        goto bad;
	error = md_get_mem(mdp, (caddr_t) gp->gss_token, gp->gss_tokenlen, MB_MSYSTEM);
	if (error)
		goto bad;
	/* Determine the amount of data left in the buffer */
	bc = (toklen > bc) ? 0 : bc - toklen;
	/* 
	 * The rest of the message should contain the NativeOS and NativeLANManager
	 * strings. If the message is in UNICODE then the byte count field is always
	 * on an odd boundry, so we need to check to see if the security blob token 
	 * is odd or even. If the security blob toklen is even then we need to skip 
	 * the padd byte.
	 */
	if (SMB_UNICODE_STRINGS(sessionp) && (bc > 0) && (!(toklen & 1))) {
		md_get_uint8(mdp, NULL);	/* Skip Padd Byte */
		bc -= 1;
	}
	/*
	 * Now see if we can get the NativeOS and NativeLANManager strings. We 
	 * use these strings to tell if the server is a Win2k or XP system, 
	 * also Shared Computers wants this info.
	 */
	parse_server_os_lanman_strings(sessionp, mdp, bc);
	
bad:	
	smb_rq_done(rqp);
	return (error);
}

/*
 * smb_gss_ssnsetup:
 *
 * If we'er using gss, then we should be called from smb_smb_ssnsetup
 * to do an extended security session setup and x to the server
 */

int
smb_gss_ssnsetup(struct smbiod *iod, vfs_context_t context)
{
	int error = 0;
	uint32_t caps;
	uint16_t action = 0;
    struct smb_session *sessionp = iod->iod_session;
    uint32_t tryGuestWithIntegFlag = 0;
    uint32_t derive_key = 0;
	
	/* We should always have a gssd port! */
	if (!SMB_USE_GSS(sessionp)) {
		SMBERROR("Doing Extended Security, but we don't have a gssd port!\n");
		return (EINVAL);
	}

again:
    derive_key = 0;

    /*
	 * set the smbuid to zero so we will pick up the first
	 * value returned from the server in smb_gss_ssandx
	 */
	sessionp->session_smbuid = 0;
	sessionp->session_session_id = 0;

	/* Get our caps from the session. N.B. Seems only Samba uses this */
	caps = smb_gss_session_caps(sessionp);

	do {
		/* Call gss to create a security blob */
        error = smb_gss_init(iod, sessionp->session_uid, tryGuestWithIntegFlag);

		/* No token to send or error so just break out */
		if (error || (SMB_GSS_ERROR(&iod->iod_gss))) {
			SMB_LOG_AUTH("GSSD extended security error = %d gss_major = %d gss_minor = %d\n", 
					   error, iod->iod_gss.gss_major, iod->iod_gss.gss_minor);
            
            if ((sessionp->session_session_id != 0) || (sessionp->session_smbuid != 0)) {
                /* 
                 * <13687368> If the GSS Auth fails on the client side in the
                 * middle of SessionSetup exchanges (session_session_id != 0 and 
                 * session_smbuid != 0), then just logout which will tell the server
                 * that the auth has failed 
                 */
                (void)smb_smb_ssnclose(sessionp, context);
            }
            
			/* Always return EAUTH unless we want the reconnect code to try again */
			if (error != EAGAIN)
				error = EAUTH;
			break;
		}
		if ((iod->iod_gss.gss_tokenlen) &&
            ((error = smb_gss_ssandx(iod, caps, &action, context))))
			break;
	} while (SMB_GSS_CONTINUE_NEEDED(&iod->iod_gss));

    /*
     * Windows 2016 Server wants GSSD_INTEG_FLAG flag set for their Guest
     * logins, but other third party servers will fail Guest logins if it is
     * set. So try without it set and if that fails, then try again with it
     * set for Guest.
     */
    if ((error) &&
        (tryGuestWithIntegFlag == 0) &&
        (sessionp->session_flags & SMBV_GUEST_ACCESS)) {
        tryGuestWithIntegFlag = 1;
        goto again;
    }

	if ((error == 0) && !SMBV_HAS_GUEST_ACCESS(sessionp)
        && !SMBV_HAS_ANONYMOUS_ACCESS(sessionp) &&
        (action & SMB_ACT_GUEST)) {
		/* 
		 * We wanted to only login the users as guest if they ask to be login as 
		 * guest. Window system will login any bad user name as guest if guest is 
		 * turn on. 
		 * The first problem here is with XPHome. XPHome doesn't care if the 
		 * user is real or made up, it always logs them is as guest. We now have 
		 * a way to tell if Simple File Sharing (XPHome) is being used, so this
		 * is no longer an issue. 
		 *
		 * The second problem here is with Vista. Vista will log you in as guest 
		 * if your account has no password. The problem here is we don't always
		 * know if the user provided a password or not. We could have the Network
		 * Authorization Helper tell us, but this would not work in the cache 
		 * case.
		 *
		 * The user wanted authenticated access but got guest access, log them 
		 * out and return EAUTH.
		 */
        SMBWARNING("Got guest access, but wanted real access, logging off.\n");
        (void)smb_smb_ssnclose(sessionp, context);
        error = EAUTH;
	}
    
	if ((error == 0) && (action & SMB_ACT_GUEST)) {
		/* And the lying dogs might tells us to do signing when they don't */
		sessionp->session_hflags2 &= ~SMB_FLAGS2_SECURITY_SIGNATURE;
		smb_reset_sig(sessionp);
	}
    
    if (error) {
    	/* Reset the signature info */
		smb_reset_sig(sessionp);
    }
    
    /*
     * Should we derive signing/sealing keys?
     * 1. Has to be no error and SMB v3.x or later
     * 2. Dont need to derive keys if SMB v3.0.2 and Guest or Anonymous login
     * 3. SMB v3.1.1 needs the signing key for user logins for the
     *    pre auth integrity check
     * 4. SMB v3.1.1 and Guest logins MAY need the signing key if its not a
     *    true Guest login. [MS-SMB2] Section 3.3.5.5.3, step 12. If
     *    SMB2_SESSION_FLAG_IS_GUEST is NOT set in the SessionFlags field, then
     *    its a user account with the name of GUEST and no password and the
     *    Session Setup reply WILL be signed for the pre auth integrity check.
     *    If SMB2_SESSION_FLAG_IS_GUEST is set, then its a true Guest login and
     *    there will be no session keys to be derived and the pre auth integrity
     *    check is skipped.
     */
    if ((error == 0) && SMBV_SMB3_OR_LATER(sessionp)) {
        if (sessionp->session_flags & SMBV_SMB311) {
            if (!(action & SMB_ACT_GUEST)) {
                /* Regular user login or non true Guest login */
                derive_key = 1;
            }
            else {
                /*
                 * SMB2_SESSION_FLAG_IS_GUEST is set, so true Guest and we wont
                 * have any session keys to use so do not derive keys.
                 */
                if (iod->iod_sess_setup_reply != NULL) {
                    /* Free the saved Session Setup reply */
                    SMB_FREE(iod->iod_sess_setup_reply, M_SMBTEMP);
                    iod->iod_sess_setup_reply = NULL;
                }
            }
        }
        else {
            if (!SMBV_HAS_GUEST_ACCESS(sessionp) &&
                !SMBV_HAS_ANONYMOUS_ACCESS(sessionp)) {
                derive_key = 1;
            }
        }
    }
    
    /* Derive SMB 3 keys */
    if (derive_key == 1) {
        error = smb3_derive_keys(iod);
        if (error) {
            SMBERROR("smb3_derive_keys returned %d. id: %u \n",
                     error, iod->iod_id);
            (void)smb_smb_ssnclose(sessionp, context);
            smb_reset_sig(sessionp);
            return(EAUTH);
        }
        
        if (iod->iod_sess_setup_reply != NULL) {
            /*
             * Need to verify last session setup reply from server.
             * This might be due to SMB v3.1.1 pre auth integrity
             * check or SMB 3.x.x multichannel
             */
            error = smb3_verify_session_setup(sessionp, iod,
                                              iod->iod_sess_setup_reply,
                                              iod->iod_sess_setup_reply_len);
            
            /* Free the saved Session Setup reply */
            SMB_FREE(iod->iod_sess_setup_reply, M_SMBTEMP);
            iod->iod_sess_setup_reply = NULL;

            if (error) {
                SMBERROR("smb3_verify_session_setup returned %d. id: %u\n",
                         error, iod->iod_id);
                (void)smb_smb_ssnclose(sessionp, context);
                smb_reset_sig(sessionp);
                error = EAUTH;
           }
        }
    }
    
	return error;
}

/*
 * The session needs to hold a reference on the credentials until its destroyed.
 * 
 */
void smb_gss_ref_cred(struct smbiod *iod)
{
	struct smb_gss *cp = &iod->iod_gss;
    struct smb_session *sessionp = iod->iod_session;
	vm_map_copy_t cpn = NULL;
	gssd_mechtype mechtype;	
	kern_return_t kr;
	int retry_cnt = 0;

	SMBDEBUG("id %d, srvname: %s\n", iod->iod_id, sessionp->session_srvname);
	if (!(SESSION_CAPS(sessionp) & SMB_CAP_EXT_SECURITY)) {
		/* Not using gssd then no way to take a reference */
		return;
	}
	if ((cp->gss_cpn_len == 0) || (cp->gss_cpn == NULL)) {
		/* No cred then no way to take a reference */
		return;
	}
	
	if (sessionp->session_flags & SMBV_RAW_NTLMSSP) {
		mechtype = GSSD_NTLM_MECH;
	} else {
		mechtype = GSSD_SPNEGO_MECH;
	}
	
	gss_mach_alloc_buffer(cp->gss_cpn, cp->gss_cpn_len, &cpn);
retry:
	kr = mach_gss_hold_cred(iod->iod_session->gss_mp, mechtype, cp->gss_client_nt,
							(gssd_byte_buffer)cpn, 
							(mach_msg_type_number_t)cp->gss_cpn_len, 
							&cp->gss_major, &cp->gss_minor);
	if (kr != KERN_SUCCESS) {
		SMB_LOG_AUTH("mach_gss_hold_cred failed: kr = 0x%x kr = %d\n", kr, kr);
		if (kr == MIG_SERVER_DIED && retry_cnt++ < GSS_MACH_MAX_RETRIES) {
			if (cp->gss_cpn_len > 0)
				gss_mach_alloc_buffer(cp->gss_cpn, cp->gss_cpn_len, &cpn);
			goto retry;	
		}
	} else if (SMB_GSS_ERROR(cp)) {
		SMB_LOG_AUTH("FAILED: gss_major = %d gss_minor = %d\n", 
				   cp->gss_major, cp->gss_minor);
	}
}

/*
 * The session holds a reference on the credtials release it.
 */
void smb_gss_rel_cred(struct smbiod *iod)
{
    struct smb_gss *cp = &iod->iod_gss;
    struct smb_session *sessionp = iod->iod_session;
	vm_map_copy_t cpn = NULL;
	gssd_mechtype mechtype;	
	kern_return_t kr;
	int retry_cnt = 0;

    SMBDEBUG("id %d,%s\n", iod->iod_id, sessionp->session_srvname);
	/* Not using gssd then no way to take a reference */
	if (!(SESSION_CAPS(sessionp) & SMB_CAP_EXT_SECURITY)) {
		return;
	}
	if ((cp->gss_cpn_len == 0) || (cp->gss_cpn == NULL)) {
		/* No cred then no way to take a reference */
		return;
	}
	
	if (sessionp->session_flags & SMBV_RAW_NTLMSSP) {
		mechtype = GSSD_NTLM_MECH;
	} else {
		mechtype = GSSD_SPNEGO_MECH;
	}
	
	gss_mach_alloc_buffer(cp->gss_cpn, cp->gss_cpn_len, &cpn);
retry:
	kr = mach_gss_unhold_cred(iod->iod_session->gss_mp, mechtype, cp->gss_client_nt,
							  (gssd_byte_buffer)cpn, 
							  (mach_msg_type_number_t)cp->gss_cpn_len, 
							  &cp->gss_major, &cp->gss_minor);
	if (kr != KERN_SUCCESS) {
		SMB_LOG_AUTH("mach_gss_unhold_cred failed: kr = 0x%x kr = %d\n", kr, kr);
		if (kr == MIG_SERVER_DIED && retry_cnt++ < GSS_MACH_MAX_RETRIES) {
			if (cp->gss_cpn_len > 0)
				gss_mach_alloc_buffer(cp->gss_cpn, cp->gss_cpn_len, &cpn);
			goto retry;	
		}
	} else if (SMB_GSS_ERROR(cp)) {
		SMB_LOG_AUTH("FAILED: gss_major = %d gss_minor = %d\n", 
				   cp->gss_major, cp->gss_minor);
	}
}

int
smb_gss_dup(struct smb_gss *parent_gp, struct smb_gss *new_gp) {

    int error = 0;
    
    *new_gp = *parent_gp;
    new_gp->gss_cpn = NULL;
    new_gp->gss_spn = NULL;
    new_gp->gss_cpn_display = NULL;

    // Clear gss_token
    new_gp->gss_tokenlen = 0;
    new_gp->gss_token    = NULL;

    // dup gss_cpn
    if (parent_gp->gss_cpn_len) {
        SMB_MALLOC(new_gp->gss_cpn, typeof(new_gp->gss_cpn), parent_gp->gss_cpn_len, M_SMBTEMP, M_WAITOK);
        if (!new_gp->gss_cpn) {
            error = ENOMEM;
            goto exit;
        }
        memcpy(new_gp->gss_cpn, parent_gp->gss_cpn, parent_gp->gss_cpn_len);
        new_gp->gss_cpn_len = parent_gp->gss_cpn_len;
    }

    // dup gss_spn
    if (parent_gp->gss_spn_len) {
        SMB_MALLOC(new_gp->gss_spn, typeof(new_gp->gss_spn), parent_gp->gss_spn_len, M_SMBTEMP, M_WAITOK);
        if (!new_gp->gss_spn) {
            error = ENOMEM;
            goto exit;
        }
        memcpy(new_gp->gss_spn, parent_gp->gss_spn, parent_gp->gss_spn_len);
        new_gp->gss_spn_len = parent_gp->gss_spn_len;
    }

    // dup gss_cpn_display
    if (parent_gp->gss_cpn_display && parent_gp->gss_cpn_display[0]) {
        size_t len = strnlen(parent_gp->gss_cpn_display, MAX_DISPLAY_STR);
        SMB_MALLOC(new_gp->gss_cpn_display, char *, len, M_SMBTEMP, M_WAITOK);
        if (!new_gp->gss_cpn_display) {
            error = ENOMEM;
            goto exit;
        }
        strlcpy(new_gp->gss_cpn_display, parent_gp->gss_cpn_display, len);
    }

exit:
    return error;
}




