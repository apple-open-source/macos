/*
 * Copyright (c) 2006 - 2010 Apple Inc. All rights reserved.
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
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_gss.h>

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

/*
 * smb_gss_negotiate:
 * This routine is called form smb_smb_negotiate to initialize the vc_gss
 * structure in a vc for doing extened security. We asn.1 decode the security
 * blob passed in, and get the task special port for gssd.
 */
int 
smb_gss_negotiate(struct smb_vc *vcp, vfs_context_t context)
{
	struct smb_gss *gp = &vcp->vc_gss;
	mach_port_t gssd_host_port;
	uid_t uid;
	kauth_cred_t cred;
	kern_return_t kr;

	if (IPC_PORT_VALID(gp->gss_mp))
		return 0;
	
	DBG_ASSERT(context);
	/* Should never happen, but just in case */
	if (context == NULL)
		return EPIPE;

	SMB_FREE(gp->gss_spn, M_SMBTEMP);		
	
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

	kr = mach_gss_lookup(gssd_host_port, uid, gp->gss_asid, &gp->gss_mp);
	if (kr != KERN_SUCCESS || !IPC_PORT_VALID(gp->gss_mp)) {
		if (kr != KERN_SUCCESS)
			SMBERROR("mach_gss_lookup failed: status %x (%d)\n", kr, kr);
		else
			SMBERROR("Port is %s\n", gp->gss_mp == IPC_PORT_DEAD ? "dead" : "null");

		return EPIPE;
	}
	return 0;
}

/*
 * smb_gss_reset:
 *
 * Reset in case we need to reconnect or reauth after an error
 */
static void
smb_gss_reset(struct smb_gss *gp)
{
	
	SMB_FREE(gp->gss_token, M_SMBTEMP);
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
	extern void ipc_port_release_send(ipc_port_t);
	
	/* Release the mach port by  calling the kernel routine needed to release task special port */
	if (IPC_PORT_VALID(gp->gss_mp))
	    ipc_port_release_send(gp->gss_mp);
	SMB_FREE(gp->gss_cpn, M_SMBTEMP);
	SMB_FREE(gp->gss_cpn_display, M_SMBTEMP);
	SMB_FREE(gp->gss_spn, M_SMBTEMP);
	SMB_FREE(gp->gss_token, M_SMBTEMP);
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
	kr = vm_allocate(ipc_kernel_map, &kmem_buf, tbuflen, VM_FLAGS_ANYWHERE);
	if (kr != 0) {
		SMBERROR("gss_mach_alloc_buffer: vm_allocate failed\n");
		return;
	}

	kr = vm_map_wire(ipc_kernel_map, vm_map_trunc_page(kmem_buf),
					 vm_map_round_page(kmem_buf + tbuflen),
					 VM_PROT_READ|VM_PROT_WRITE, FALSE);
	if (kr != 0) {
		SMBERROR("gss_mach_alloc_buffer: vm_map_wire failed kr = %d\n", kr);
		return;
	}

	bcopy(buf, (void *) kmem_buf, buflen);

	kr = vm_map_unwire(ipc_kernel_map, vm_map_trunc_page(kmem_buf),
					   vm_map_round_page(kmem_buf + tbuflen), FALSE);
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
 * and the supplied uid. (Should be the vc_uid field of the enclosing vc).
 */

static kern_return_t
smb_gss_init(struct smb_vc *vcp, uid_t uid)
{
	struct smb_gss *cp = &vcp->vc_gss;
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

	if (!IPC_PORT_VALID(cp->gss_mp)) {
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
	if (vcp->vc_flags & SMBV_ANONYMOUS_ACCESS) {
		flags |= GSSD_ANON_FLAG;
	}
	/* lha says we should set this bit when doing signing */
	if (vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) {
		flags |= GSSD_INTEG_FLAG; /* Doing signing */
	}
	/* See if its ok to touch the home directory */
	if (vcp->vc_hflags2 & SMBV_HOME_ACCESS_OK) {
		gssd_flags |= GSSD_HOME_ACCESS_OK; 
	}
	/* The server doesn't support NTLMSSP, send RAW NTLM */
	if (vcp->vc_flags & SMBV_RAW_NTLMSSP) {
		mechtype = GSSD_NTLM_MECH;
	} else {
		mechtype = GSSD_SPNEGO_MECH;
	}
	
retry:
	*display_name = '\0';
	kr = mach_gss_init_sec_context_v2(
					  cp->gss_mp,
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
		/* Free any old key and reset the sequence number */
		smb_reset_sig(vcp);
		vcp->vc_mackeylen = keylen;
		SMB_MALLOC(vcp->vc_mackey, uint8_t *, vcp->vc_mackeylen, M_SMBTEMP, M_WAITOK);
		error = gss_mach_vmcopyout((vm_map_copy_t) okey, vcp->vc_mackeylen, vcp->vc_mackey);
		if (error) {
			gss_mach_vm_map_copy_discard((vm_map_copy_t)otoken, otokenlen);
			goto out;
		}
		SMBDEBUG("%s keylen = %d seqno = %d\n", vcp->vc_srvname, keylen, vcp->vc_seqno);
		smb_hexdump(__FUNCTION__, "setting vc_mackey = ", vcp->vc_mackey, vcp->vc_mackeylen);
		/* 
		 * Windows expects the sequence number to restart once we get a signing
		 * key. They expect this to happen once the client creates a authorization
		 * token blob to send to the server. This was we can validate the servers
		 * response. When doing Kerberos and now NTLMSSP we don't get the signing
		 * key until after the gss mech has completed. Not sure how to really 
		 * fix this issue, but for now we just reset the sequence number as if
		 * we had the key when the last round went out.
		 */
		vcp->vc_seqno = 2;
	}

	/* If we're done, see if the server is mapping everybody to guest */
	if (SMB_GSS_COMPLETE(cp) && (gssd_flags & GSSD_GUEST_ONLY)) {
		vcp->vc_flags |= SMBV_SFS_ACCESS;
		SMBDEBUG("NTLMSSP simple file sharing\n");
	}

	/* Free context token used as input */
	SMB_FREE(cp->gss_token, M_SMBTEMP);
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
	if (SMB_GSS_ERROR(cp) && (vcp->vc_iod->iod_flags & SMBIOD_RECONNECT))
		return EAGAIN;

	return (0);
	
out:
	SMB_FREE(cp->gss_token, M_SMBTEMP);
	cp->gss_tokenlen = 0;
	
	return (EAUTH);
}

/*
 * smb_gss_vc_caps:
 *
 * Given a virtual circut, determine our capabilities to send to the server
 * as part of "ssandx" message. We now call the general routine smb_vc_caps
 * handle the basic items and add that we are doing extended security.
 */
static uint32_t smb_gss_vc_caps(struct smb_vc *vcp)
{	
	return (smb_vc_caps(vcp) | SMB_CAP_EXT_SECURITY);	
}

/*
 * smb_gss_ssandx:
 *
 * Send a session setup and x message on vc with cred and caps
 */
static int
smb_gss_ssandx(struct smb_vc *vcp, uint32_t caps, uint16_t *action,
                vfs_context_t context)
{
	struct smb_rq *rqp = NULL;
	struct smb_gss *gp = &vcp->vc_gss;
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
	maxtokenlen = vcp->vc_txmax - (SMB_HDRLEN + SMB_SETUPXRLEN);
#endif // SMB_DEBUG

	tokenptr = gp->gss_token;	/* Get the start of the Kerberos blob */
	do {
		uint16_t maxtx = vcp->vc_txmax;
		if (rqp)	/* If we are looping then release it, before getting it again */
			smb_rq_done(rqp);

		/* Allocate the request form a session setup and x */
		error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX, 0, context, &rqp);
		if (error)
			break;
		/* Fill the request with the required parameters */
		smb_rq_wstart(rqp);
        smb_rq_getrequest(rqp, &mbp);
		mb_put_uint8(mbp, 0xff);
		mb_put_uint8(mbp, 0);
		mb_put_uint16le(mbp, 0);
		mb_put_uint16le(mbp, maxtx);
		mb_put_uint16le(mbp, vcp->vc_sopt.sv_maxmux);
		mb_put_uint16le(mbp, vcp->vc_number);
		mb_put_uint32le(mbp, vcp->vc_sopt.sv_skey);
		/* Get the max size we can send in one SetupAndX message */
		tokenlen = (gp->gss_tokenlen > maxtokenlen) ? maxtokenlen : gp->gss_tokenlen;
		mb_put_uint16le(mbp, tokenlen);
		mb_put_uint32le(mbp, 0);		/* reserved */
		mb_put_uint32le(mbp, caps);		/* our caps */
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		/* SPNEGO blob */
		mb_put_mem(mbp, (caddr_t) tokenptr, tokenlen, MB_MSYSTEM);
		smb_put_dstring(mbp, SMB_UNICODE_STRINGS(vcp), SMBFS_NATIVEOS, sizeof(SMBFS_NATIVEOS), NO_SFM_CONVERSIONS);	/* Native OS */
		smb_put_dstring(mbp, SMB_UNICODE_STRINGS(vcp), SMBFS_LANMAN, sizeof(SMBFS_LANMAN), NO_SFM_CONVERSIONS);	/* LAN Mgr */
		smb_rq_bend(rqp);
		/* Send the request and check for reply */
		error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);
		/* Move the pointer to the next offset in the blob */
		tokenptr += tokenlen;
		/* Subtract the size we have already sent */
		gp->gss_tokenlen -= tokenlen;
		/* Save the servers vc identifier if not already set. */
		if ((error == EAGAIN) && (vcp->vc_smbuid == 0))
			vcp->vc_smbuid = rqp->sr_rpuid;
	
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
	 * Save the servers vc identifier. Seems Samba will give us a new one for
	 * every loop of a SetUpAndX NTLMSSP response. Windows server just return
	 * the same one every time. We assume here the last one is the one we 
	 * should always use. Seems to make Samba work correctly.
	 */
	vcp->vc_smbuid = rqp->sr_rpuid;
	
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
	if (SMB_UNICODE_STRINGS(vcp) && (bc > 0) && (!(toklen & 1))) {
		md_get_uint8(mdp, NULL);	/* Skip Padd Byte */
		bc -= 1;
	}
	/*
	 * Now see if we can get the NativeOS and NativeLANManager strings. We 
	 * use these strings to tell if the server is a Win2k or XP system, 
	 * also Shared Computers wants this info.
	 */
	parse_server_os_lanman_strings(vcp, mdp, bc);
	
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
smb_gss_ssnsetup(struct smb_vc *vcp, vfs_context_t context)
{
	int error = 0;
	uint32_t caps;
	uint16_t action = 0;
	
	/* We should always have a gssd port! */
	if (!SMB_USE_GSS(vcp)) {
		SMBERROR("Doing Extended Security, but we don't have a gssd port!\n");
		return (EINVAL);
	}

	/*
	 * set the smbuid to zero so we will pick up the first
	 * value returned from the server in smb_gss_ssandx
	 */
	vcp->vc_smbuid = 0;

	/* Get our caps from the vc. N.B. Seems only Samba uses this */
	caps = smb_gss_vc_caps(vcp);

	do {
		/* Call gss to create a security blob */
        error = smb_gss_init(vcp, vcp->vc_uid);

		/* No token to send or error so just break out */
		if (error || (SMB_GSS_ERROR(&vcp->vc_gss))) {
			SMB_LOG_AUTH("GSSD extended security error = %d gss_major = %d gss_minor = %d\n", 
					   error, vcp->vc_gss.gss_major, vcp->vc_gss.gss_minor);
			/* Always return EAUTH unless we want the reconnect code to try again */
			if (error != EAGAIN)
				error = EAUTH;
			break;
		}
		if ((vcp->vc_gss.gss_tokenlen) && ((error = smb_gss_ssandx(vcp, caps, 
                                                                   &action, 
                                                                   context))))
			break;
	} while (SMB_GSS_CONTINUE_NEEDED(&vcp->vc_gss));

	if ((error == 0) && !SMBV_HAS_GUEST_ACCESS(vcp) && (action & SMB_ACT_GUEST)) {
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
        (void)smb_smb_ssnclose(vcp, context);
        error = EAUTH;
	}
	if ((error == 0) && (action & SMB_ACT_GUEST)) {
		/* And the lying dogs might tells us to do signing when they don't */
		vcp->vc_hflags2 &= ~SMB_FLAGS2_SECURITY_SIGNATURE;
		smb_reset_sig(vcp);
		SMB_FREE(vcp->vc_mackey, M_SMBTEMP);
		vcp->vc_mackeylen = 0;
	}
	if (error)	/* Reset the signature info */
		smb_reset_sig(vcp);

	smb_gss_reset(&vcp->vc_gss);
	return error;
}

/*
 * The VC needs to hold a reference on the credtials until its destroyed.
 * 
 */
void smb_gss_ref_cred(struct smb_vc *vcp)
{
	struct smb_gss *cp = &vcp->vc_gss;
	vm_map_copy_t cpn = NULL;
	gssd_mechtype mechtype;	
	kern_return_t kr;
	int retry_cnt = 0;

	SMBDEBUG("%s\n", vcp->vc_srvname);
	if (!(VC_CAPS(vcp) & SMB_CAP_EXT_SECURITY)) {
		/* Not using gssd then no way to take a reference */
		return;
	}
	if ((cp->gss_cpn_len == 0) || (cp->gss_cpn == NULL)) {
		/* No cred then no way to take a reference */
		return;
	}
	
	if (vcp->vc_flags & SMBV_RAW_NTLMSSP) {
		mechtype = GSSD_NTLM_MECH;
	} else {
		mechtype = GSSD_SPNEGO_MECH;
	}
	
	gss_mach_alloc_buffer(cp->gss_cpn, cp->gss_cpn_len, &cpn);
retry:
	kr = mach_gss_hold_cred(cp->gss_mp, mechtype, cp->gss_client_nt, 
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
 * The VC holds a reference on the credtials release it.
 */
void smb_gss_rel_cred(struct smb_vc *vcp)
{
	struct smb_gss *cp = &vcp->vc_gss;
	vm_map_copy_t cpn = NULL;
	gssd_mechtype mechtype;	
	kern_return_t kr;
	int retry_cnt = 0;

	SMBDEBUG("%s\n", vcp->vc_srvname);
	/* Not using gssd then no way to take a reference */
	if (!(VC_CAPS(vcp) & SMB_CAP_EXT_SECURITY)) {
		return;
	}
	if ((cp->gss_cpn_len == 0) || (cp->gss_cpn == NULL)) {
		/* No cred then no way to take a reference */
		return;
	}
	
	if (vcp->vc_flags & SMBV_RAW_NTLMSSP) {
		mechtype = GSSD_NTLM_MECH;
	} else {
		mechtype = GSSD_SPNEGO_MECH;
	}
	
	gss_mach_alloc_buffer(cp->gss_cpn, cp->gss_cpn_len, &cpn);
retry:
	kr = mach_gss_unhold_cred(cp->gss_mp, mechtype, cp->gss_client_nt,  
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
