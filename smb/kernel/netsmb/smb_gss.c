/*
 * Copyright (c) 2006 - 2007 Apple Inc. All rights reserved.
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
#include <mach/task_special_ports.h>
#include <mach/mig_errors.h>
#include <mach/vm_map.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>

#include <sys/smb_apple.h>
#include <sys/smb_iconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_gss.h>

#include <sys/kauth.h>
#include <fs/smbfs/smbfs.h>

#include <gssd/gssd_mach_types.h>
#include <gssd/gssd_mach.h>

#define GSS_MACH_MAX_RETRIES 3
#define SKEYLEN 8

#define ASN1_STRING_TYPE(x) (((x) >= 18 && (x) <= 22) ||	\
			     ((x) >= 25 && (x) <= 30) ||	\
			     (x) == 4 || (x) == 12)
#define SPNEGO_INIT_TOKEN "\x06\x06\x2b\x06\x01\x05\x05\x02\xa0"
#define SPNEGO_INIT_TOKEN_LEN 9

#define SPNEGO_mechType_MSKRB5 "\x2a\x86\x48\x82\xf7\x12\x01\x02\x02"
#define SPNEGO_mechType_MSKRB5_LEN 9

#define SPNEGO_mechType_KRB5 "\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\0x03"
#define SPNEGO_mechType_KRB5_LEN 9
#define SPNEGO_mechType_KRB5_V3_LEN 10

#define SPNEGO_mechType_NTLMSSP "\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x0a"
#define SPNEGO_mechType_NTLMSSP_LEN 10

/*
 * smb_blob2principal. This routine parses the pseudo spnego init 
 * token from a negotiate reply from Microsoft. This token reply 
 * overloads the
 * optional mechListMic with the serers principal name. This
 * is the only thing we care about in this token. 
 * This token looks as follows:
 * 		60		ASN.1 [Application 0] per RFC 2743
 *		length
 *		06		OID tag primitive length
 *		06		length of SPNEGO OID
 *		2b		oid octets { 1.3.
 *		06		6.
 *		01		1.
 *		05		5.
 *		05		5.
 * 		02		2 }
 *		a0		context  tag for choice 0 NegTokenInit
 *		length		der encoded (assume 1 byte) currently 5a
 *		30		Sequence
 *		length -2
 *		a0		context tag 0 mechTypes
 *		mech_length
 *		30		sequence of oids
 *		mech_length - 2
 *		06 oidlen oid ...
 *		~~~
 *		~~~
 *  		a3		context tag 3 
 *		mic length	
 * Ok. this get weird. One would expect an octet string at this point
 * but, what we have is
 * 	mechListMIC ::= SEQUENCE {
 *	  severPrincipal	[0] GENERAL STRING,
 *     }
 * Continuing
 *		30		sequence
 *		mic length - 2	
 *		a0		context tag 0
 *		mic length - 4
 *		1b		general string
 *		mic length - 6
 *		what we finaly want
 *
 * Note we are assuming that the optional reqFlags are not present
 * as recommended in RFC 4178. (Microsoft does do this, though this
 * whole token is not really RFC 4178 compliant and not really part
 * of SPNEGO)
 *
 * Thus we can quickly extract the principal as follows by
 * skipping the first 10 bytes, 
 * 
 */

static size_t 
derLen(const uint8_t **dptr)
{
	int i;
	const uint8_t *p = *dptr;
	size_t len = 0;
	
	if (*p &  0x80) {
		for (i = *p & 0x7f; i > 0; i--)
			len = (len << 8) + *++p;
	} else
		len = *p;
	
	*dptr = p + 1;
	
	
	return (len);
}

static int
smb_asn1_blob2principal(struct smb_vc *vcp, const uint8_t *blobp, 
			char **princ, size_t *princlen)
{
	size_t ellen;		/* Current element length */
	size_t mechs_ellen;		/* mechTypes element length */
	size_t mechlen;		/* mechType length */
	uint8_t *endOfMechTypes;
	
	*princ = NULL;
	*princlen = 0;

	/* Check for application tag 0 per RFC 2743 */
	if (*blobp++ != 0x60)
		return (0);
	ellen = derLen(&blobp);
	
	/* Check for the SPNEGO mech id */
	if (bcmp(blobp, SPNEGO_INIT_TOKEN, SPNEGO_INIT_TOKEN_LEN) != 0)
		return (0);

	blobp += SPNEGO_INIT_TOKEN_LEN;
	
	/* Get the length of the NegTokenInit */
	ellen = derLen(&blobp);

	/* Now check for Sequence */
	if (*blobp++ != 0x30)
		return (0);

	/* Get the length of the sequence */
	ellen = derLen(&blobp);

	/* Now check for the required mechTypes tag 0 */
	if (*blobp++ != 0xa0)
		return  (0);

	/* Get the element length of the mechTypes */
	ellen = derLen(&blobp);
		
	/* Now check for Sequence */
	if (*blobp++ != 0x30)
		return (0);
	
	/* Get the length of the mechTypes */
	mechs_ellen = derLen(&blobp);

#ifdef DEBUG
	if (mechs_ellen != (ellen - 2))
		SMBERROR("mechs_ellen = %x ellen = %x\n", (unsigned int)mechs_ellen, (unsigned int)ellen);
#endif // DEBUG
		
	/* Now advance our end blob pointer to the Microsoft funny mechListMic. */
	endOfMechTypes = (uint8_t *)blobp + mechs_ellen;
	
	
	/* 
	 * Seach the mechType list
	 *
	 * From RFC 2743 - Page 85
	 *
	 *	06 09 2A 86 48 86 F7 12 01 02 02    DER-encoded ASN.1 value of type OID; Kerberos V5 mechanism OID indicates
	 *										Kerberos V5 exported name
	 *
	 *	in Detail:      
	 *	06                  Identifier octet (6=OID)
	 *	09					Length octet(s)
	 *	2A 86 48 86 F7 12 01 02 02   Content octet(s)
	 *
	 *	hx xx xx xl		4-byte length of the immediately following exported name blob, most significant octet first
	 *					pp qq ... zz exported name blob of specified length, bits and bytes specified in the 
	 *					(Kerberos 5) GSS-API v2 mechanism spec
	 */
	while (endOfMechTypes > blobp) {
		
		if (*blobp++ != 0x06) {
			SMBERROR("Bad Identifier octet (6=OID)\n");
			blobp = endOfMechTypes;
			break;
		}
		mechlen = derLen(&blobp);
#ifdef DEBUG_MECH_TYPES
		smb_hexdump(__FUNCTION__, "mechType: ", (u_char *)blobp, (int)mechlen);
#endif // DEBUG_MECH_TYPES
		if (mechlen == 0) /* Just to be careful */ {
			SMBERROR("Bad mechlen length!\n");
			blobp = endOfMechTypes;
			break;
		}
		if ((mechlen == SPNEGO_mechType_MSKRB5_LEN) && 
			(bcmp(blobp, SPNEGO_mechType_MSKRB5, SPNEGO_mechType_MSKRB5_LEN) == 0))
			vcp->vc_flags |= SMBV_MECHTYPE_KRB5;
		else if ((mechlen == SPNEGO_mechType_KRB5_LEN) &&
			(bcmp(blobp, SPNEGO_mechType_KRB5, SPNEGO_mechType_KRB5_LEN) == 0))
			vcp->vc_flags |= SMBV_MECHTYPE_KRB5;
		else if ((mechlen == SPNEGO_mechType_KRB5_V3_LEN) &&
				 (bcmp(blobp, SPNEGO_mechType_KRB5, SPNEGO_mechType_KRB5_V3_LEN) == 0))
			vcp->vc_flags |= SMBV_MECHTYPE_KRB5;
		else if ((mechlen == SPNEGO_mechType_NTLMSSP_LEN) &&
				 (bcmp(blobp, SPNEGO_mechType_NTLMSSP, SPNEGO_mechType_NTLMSSP_LEN) == 0))
			vcp->vc_flags |= SMBV_MECHTYPE_NTLMSSP;
		blobp += mechlen;
	}
	/* No Kerberos MechType just get out nothing else to do */
	if ((vcp->vc_flags & SMBV_MECHTYPE_KRB5) != SMBV_MECHTYPE_KRB5)
		return 0;
	
	if (blobp != endOfMechTypes) {
		SMBERROR("Something bad happen blobp = %p endOfMechTypes = %p!\n", blobp, endOfMechTypes);	
		blobp = endOfMechTypes;
	}

	/*
	 * Check for optional tag 2 (regFlags) and skip it
	 * if its there.
	 */
	if (*blobp == 0xa2) {
		blobp += 1;
		ellen = derLen(&blobp);
		blobp += ellen;
	}
	
	/* Check that we are in fact at the mechListMic, tag 3 */
	if (*blobp++ != 0xa3)
		return (0);
	ellen = derLen(&blobp);

	/*
	 * Just in case Microsoft ever decides to do things right,
	 * at least sort of, lets check if we've got a string type,
	 * which should be octet string, but will take anything.
	 */
	if (ASN1_STRING_TYPE(*blobp))
		goto have_string;

	/* Check that we have a sequence */
	if (*blobp++ != 0x30)
		return (0);
	ellen = derLen(&blobp);

	/* check for context class constructed tag 0 */
	if (*blobp++ != 0xa0)
		return (0);
	ellen = derLen(&blobp);

	/*
	 * Check for String type. Should be General, but will
	 * accept any
	 */

	if (!ASN1_STRING_TYPE(*blobp))
		return (0);
have_string:
	
	blobp += 1;

	/* We now have our string length and blobp is our principal */
	ellen = derLen(&blobp);
	
	*princ = (char *)blobp;
	*princlen = ellen;
	SMBDEBUG("princ = %s\n", *princ);
	
	return (1);
}

/*
 * smb_reply2principal:
 *
 * Given the blob from the negotiate reply and return the service name.
 * We call smb_asn1_blob2principal to get the server principal name
 * from the reply. If the return name is name@realm or name$@realm
 * we convert this to the service principal name ciffs/name@realm.
 * This routine will allocate storage for the name, the caller is
 * responsible to free the principal name. Note the name will be null
 * terminated and the length returned will account for the terminating
 * null byte.
 *
 * Returns: true on success (non-zero)
 *	     false on failure (zero)
 * This routine will only fail if the asn.1 decode failes, i.e. a bad blob.
 */
static int
smb_reply2principal(struct smb_vc *vcp, const uint8_t *rp, char **principal, uint32_t  *princlen)
{
	char *spn;    /* servers principal name */
	size_t spnlen; /* servers principal name length */
	char *p, *endp; /* beginning and end of principal */
	char *service, *s_end; /* Beginning and ending of service part of principal, e.g. "cifs" */
	char *host, *h_end; /* Beginning and ending of host part of principal */
	char *realm;  /* The realm part of principal */
	size_t left;  /* What's left of the principal should be the realm lenght */
	
	*principal = NULL;
	*princlen = 0;
	
	if (!smb_asn1_blob2principal(vcp, rp, &spn, &spnlen)) 
		return (0);

	*princlen = spnlen;
	endp = spn + spnlen;
	service = spn;
	/* Search for end of service part */
	for (host = spn; *host != '/' && host < endp; host++)
		;
	if (host == endp) {
		/* 
		 * No service part. So we will add "cifs" as the service
		 * and the host started with the returned service principal name.
		 */
		host = spn;
		/* We need to add "cifs/ instance part" */
		service = "cifs";
		s_end = service + 4;
		*princlen +=5;   /* Increase the length to include the `/' */
	} else {
		s_end = host++;
	}

	/* Search for the realm part */
	for (realm = host; *realm != '@' && realm < endp; realm++)
		;
	if (realm < endp && realm > host) {
		h_end = realm;
		/* Check  and see if we have a Netbios name */
		if (*(h_end - 1) == '$') {
			/*
			 * Convert it to a regular host name 
			 * N.B. we might want to defer this to 
			 * gssd. As it stands gssd will try and
			 * canonicalize the host name and that
			 * might not work for windows. I would hate
			 * to have put netbios hacks in gssd though.
			 */
			h_end -= 1;
			*princlen -= 1;
		}
		realm += 1;		/* Move past '@' for the actual realm name */
		left = endp - realm;
	} else {
		h_end = endp;
		left = 0;
	}
	
	/*
	 * We now should have the service, host, and realm and
	 * the total length *princlen
	 */
	*princlen += 1; /* Allocate the null termination byte */
	MALLOC(*principal, char *, *princlen, M_TEMP, M_WAITOK);
	p = *principal;
	/* Copy the service part of principal */
	bcopy(service, p, s_end - service);
	p += (s_end - service);
	/* Set the instance marker */
	*p++ = '/';
	/* Copy the host part of principal */
	bcopy(host, p, h_end - host);
	p += (h_end - host);
	/* If we have one, copy the realm */
	if (left) {
		*p++ = '@';
		bcopy(realm, p, left);
		p += left;
	}
	/* Set the null terminations */
	*p = '\0';
	DBG_ASSERT(strlen(*principal)+1 == *princlen);
	SMBDEBUG("principal name is %s, len is %d\n", *principal, *princlen);
	
	return (1);
}

/*
 * smb_gss_negotiate:
 * This routine is called form smb_smb_negotiate to initialize the vc_gss
 * structure in a vc for doing extened security. We asn.1 decode the security
 * blob passed in, and get the task special port for gssd.
 */
int 
smb_gss_negotiate(struct smb_vc *vcp, struct smb_cred *scred, caddr_t token)
{
	struct smb_gss *gp = &vcp->vc_gss;
	kern_return_t kr;

	if (IPC_PORT_VALID(gp->gss_mp))
		return (1);
	
	DBG_ASSERT(scred);
	/* Should never happen, but just in case */
	if (scred == NULL)
		return (0);	
	
	if (gp->gss_spn) 
		free(gp->gss_spn, M_TEMP);

	if (!smb_reply2principal(vcp, (uint8_t *)token + SMB_GUIDLEN,
		&gp->gss_spn, &gp->gss_spnlen))
		return (0);

	SMBDEBUG("sgp->gss_spn %s\n", gp->gss_spn);
	kr = vfs_context_get_special_port(scred->scr_vfsctx, TASK_GSSD_PORT, &gp->gss_mp);
	if (kr != KERN_SUCCESS || !IPC_PORT_VALID(gp->gss_mp)) {
		SMBERROR("Can't get gssd port, status %d\n", kr);
		free(gp->gss_spn, M_TEMP);
		return (0);
	}
	return (1);
}
/*
 * smb_gss_reset:
 *
 * Reset in case we need to reconnect or reauth after an error
 */
static void
smb_gss_reset(struct smb_gss *gp)
{
	
	if (gp->gss_token)
		free(gp->gss_token, M_TEMP);
	gp->gss_token = NULL;
	gp->gss_tokenlen = 0;

	/* Need to look at this one closer */
	if (gp->gss_skey)
		free(gp->gss_skey, M_TEMP);
	gp->gss_skey = NULL;
	gp->gss_skeylen = 0;
	gp->gss_ctx = 0;
	gp->gss_verif = 0;
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
	if (gp->gss_cpn)	/* If we have a client's principal name free it */
		free(gp->gss_cpn, M_TEMP);
	if (gp->gss_spn)
		free(gp->gss_spn, M_TEMP);
	if (gp->gss_token)
		free(gp->gss_token, M_TEMP);
	if (gp->gss_skey)
		free(gp->gss_skey, M_TEMP);
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
	
	bcopy(buf, (void *) kmem_buf, buflen);
	
	kr = vm_map_unwire(ipc_kernel_map, vm_map_trunc_page(kmem_buf),
	vm_map_round_page(kmem_buf + tbuflen), FALSE);
	if (kr != 0) {
		SMBERROR("gss_mach_alloc_buffer: vm_map_unwire failed\n");
		return;
	}
	
	kr = vm_map_copyin(ipc_kernel_map, (vm_map_address_t) kmem_buf,
	(vm_map_size_t) buflen, TRUE, addr);
	if (kr != 0) {
		SMBERROR("gss_mach_alloc_buffer: vm_map_copyin failed\n");
		return;
	}
	
	if (buflen != tbuflen)
		kmem_free(ipc_kernel_map, kmem_buf + buflen, tbuflen - buflen);
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
 * Wrapper to make mach up call, using the parameters in the smb_gss structure
 * and the supplied uid. (Should be the vc_uid field of the enclosing vc).
 */
static kern_return_t
smb_gss_init(struct smb_vc *vcp, uid_t uid)
{
	struct smb_gss *cp = &vcp->vc_gss;
	int win2k = (vcp->vc_flags & SMBV_WIN2K) ? GSSD_WIN2K_HACK : 0;
	kern_return_t kr;
	byte_buffer okey = NULL;
	int retry_cnt = 0;
	vm_map_copy_t itoken = NULL;
	byte_buffer otoken = NULL;
	int error = 0;
	char *gss_cpn = (cp->gss_cpn) ? cp->gss_cpn : "";
	
	if (!IPC_PORT_VALID(cp->gss_mp)) {
		SMBWARNING("smb_gss_init: gssd port not valid\n");
		return (EAUTH);
	}

	if (cp->gss_tokenlen > 0) {
		gss_mach_alloc_buffer(cp->gss_token, cp->gss_tokenlen, &itoken);
		/*
		 * Our data is now in itoken, so we can free gss_token for use 
		 * in storing the output token.
		 */
		free(cp->gss_token, M_TEMP);
		cp->gss_token = NULL;
	}
	
retry:
	kr = mach_gss_init_sec_context(
		cp->gss_mp,
		SPNEGO_MECH,
		(byte_buffer) itoken, (mach_msg_type_number_t) cp->gss_tokenlen,
		uid,
		gss_cpn,
		cp->gss_spn,
		GSSD_MUTUAL_FLAG | GSSD_NO_UI | GSSD_NO_HOME_ACCESS | win2k,			   
		&cp->gss_verif,
		&cp->gss_ctx,
		&cp->gss_cred,
		&okey,  (mach_msg_type_number_t *) &cp->gss_skeylen,
		&otoken, (mach_msg_type_number_t *) &cp->gss_tokenlen,
		&cp->gss_major,
		&cp->gss_minor);

	if (kr != 0) {
		SMBERROR("smb_gss_init: mach_gss_init_sec_context failed: %x\n", kr);
		if (kr == MIG_SERVER_DIED && cp->gss_cred == 0 &&
			retry_cnt++ < GSS_MACH_MAX_RETRIES)
			goto retry;	
		return (EAUTH);
	}

	if (cp->gss_skeylen > 0) {
#ifdef DEBUG
		/*
		 * XXX NFS code we check that the returned key is SKEYLEN, a simple
		 * des key. I don't think we should restrict our selves for SMB
		 * NFS has to use specific des based routines to be compliant with
		 * RFCs.
		 */
		if (cp->gss_skeylen != SKEYLEN) {
			SMBERROR("smb_gss_init: non-nfs key length (%d)\n", cp->gss_skeylen);
#if 0
			return (EAUTH);
#endif
		}
		else 
			SMBERROR("smb_gss_init: key length (%d)\n", cp->gss_skeylen);			
#endif // DEBUG
		MALLOC(cp->gss_skey, uint8_t *, cp->gss_skeylen, M_TEMP, M_WAITOK);		
		error = gss_mach_vmcopyout((vm_map_copy_t) okey,
									cp->gss_skeylen, cp->gss_skey);
		if (error)
			return (EAUTH);
	}

	if (cp->gss_tokenlen > 0) {
		MALLOC(cp->gss_token, uint8_t *, cp->gss_tokenlen, M_TEMP, M_WAITOK);
		error = gss_mach_vmcopyout((vm_map_copy_t) otoken,
									cp->gss_tokenlen, cp->gss_token);
		if (error)
			return (EAUTH);
	}

	return (0);
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
smb_gss_ssandx(struct smb_vc *vcp, struct smb_cred *scred, uint32_t caps)
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
	
#ifdef DEBUG
	/* For testing use a smaller max size */
	maxtokenlen = 2048;
#else // DEBUG
	/* Get the max blob size we can send in SetupAndX message */
	maxtokenlen = vcp->vc_sopt.sv_maxtx - (SMB_HDRLEN + SMB_SETUPXRLEN);
#endif // DEBUG

	tokenptr = gp->gss_token;	/* Get the start of the Kerberos blob */
	do {
		if (rqp)	/* If we are looping then release it, before getting it again */
			smb_rq_done(rqp);

		/* Allocate the request form a session setup and x */
		error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX, scred, &rqp);
		if (error)
			return (error);
		
		/* Fill the request with the required parameters */
		smb_rq_wstart(rqp);
		mbp = &rqp->sr_rq;
		mb_put_uint8(mbp, 0xff);
		mb_put_uint8(mbp, 0);
		mb_put_uint16le(mbp, 0);
		mb_put_uint16le(mbp, vcp->vc_sopt.sv_maxtx);
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
		smb_put_dstring(mbp, vcp, SMBFS_NATIVEOS, NO_SFM_CONVERSIONS);	/* Native OS */
		smb_put_dstring(mbp, vcp, SMBFS_LANMAN, NO_SFM_CONVERSIONS);	/* LAN Mgr */
		smb_rq_bend(rqp);
		/* Send the request and check for reply */
		error = smb_rq_simple_timed(rqp, SMBSSNSETUPTIMO);
		/* Move the pointer to the next offset in the blob */
		tokenptr += tokenlen;
		/* Subtract the size we have already sent */
		gp->gss_tokenlen -= tokenlen;
		/* Save the servers vc identifier if not already set, we always want the first one sent */
		if ((error == EAGAIN) && (vcp->vc_smbuid == 0))
			vcp->vc_smbuid = rqp->sr_rpuid;
	
	} while (gp->gss_tokenlen && (error == EAGAIN));
	
	/* Free the gss spnego token that we sent */
	free(gp->gss_token, M_TEMP);
	gp->gss_token = NULL;
	gp->gss_tokenlen = 0;
	/* At this point error should have the correct error, we only support NTStatus code with extended security */
	if (error) {
			/* Not sure what else to return in this case, something bad happened */
		if (error == EAGAIN) {
			SMBERROR("Server thinks we have more processing when we think we are done?\n");
			error = EAUTH;
		} else
			SMBWARNING("Kerberos authorization failed! %d\n", error);
		goto bad;
	}

	/* Save the servers vc identifier if not already set */
	if (vcp->vc_smbuid == 0)
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
	md_get_uint16le(mdp, NULL);	/* action */
	md_get_uint16le(mdp, &toklen);	/* Spnego token length */
	gp->gss_tokenlen = toklen;
	md_get_uint16le(mdp, &bc); /* remaining bytes */
	/*
	 * Set the gss token from the server
	 */
	gp->gss_token = malloc(gp->gss_tokenlen, M_SMBTEMP, M_WAITOK);
	error = md_get_mem(mdp, (caddr_t) gp->gss_token, gp->gss_tokenlen, MB_MSYSTEM);
	
	/* If we need to, check if this is a win2k server */
	if ((vcp->vc_flags & SMBV_WIN2K) == 0)
		vcp->vc_flags |= (smb_check_for_win2k(mdp, bc - toklen) ? SMBV_WIN2K : 0);

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
smb_gss_ssnsetup(struct smb_vc *vcp, struct smb_cred *scred)
{
	int error;
	uint32_t caps;
	
	/* We better be doing extended securtiy! */
	if (!(vcp->vc_hflags2 & SMB_FLAGS2_EXT_SEC))
		return (EINVAL);

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
			SMBWARNING("Kerberos error = %d SMB_GSS_ERROR = %d\n", error, SMB_GSS_ERROR(&vcp->vc_gss));
			error = EAUTH;
			break;
		}
		if ((vcp->vc_gss.gss_tokenlen) && ((error = smb_gss_ssandx(vcp, scred, caps))))
			break;
	} while (SMB_GSS_CONTINUE_NEEDED(&vcp->vc_gss));
	
	/* We now have session keys in vcp->vc_gss.gss_skey */
	if ((error == 0) && (vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE)) {
		/* 
		 * May want to do this in vc_reset, but for now do it here. In the
		 * future we should remove the gss entries and just use these. The
		 * NTLMSSP code will clean this up for us.
		 */
		if (vcp->vc_mackey != NULL) {
			free(vcp->vc_mackey, M_SMBTEMP);
			vcp->vc_mackey = NULL;
			vcp->vc_mackeylen = 0;
			vcp->vc_seqno = 0;
		}		
		/*
		 * GROSS HACK HERE, since we want the new GSS code to work
		 * with the current code and we only use the session key for signing,
		 * we will simpley move the session key from the smb_gss sub-structure
		 * to vc_mackey, and then set skey to null, so on destruction of the vc
		 * every thing will be ship shape.
		 */
		vcp->vc_mackey = vcp->vc_gss.gss_skey;
		vcp->vc_mackeylen = vcp->vc_gss.gss_skeylen;
		vcp->vc_gss.gss_skey = NULL;
		vcp->vc_gss.gss_skeylen = 0;
	}

	smb_gss_reset(&vcp->vc_gss);
	return error;
}

