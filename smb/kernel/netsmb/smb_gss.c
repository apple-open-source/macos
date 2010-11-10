/*
 * Copyright (c) 2006 - 2009 Apple Inc. All rights reserved.
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

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_gss.h>

#include <sys/kauth.h>
#include <fs/smbfs/smbfs.h>
#include <netsmb/smb_converter.h>

#include <gssd/gssd_mach_types.h>
#include <gssd/gssd_mach.h>
#include <sys/random.h>

/*
 * Determine size of ASN.1 DER length
 */
static int der_length_size(int len)
{
	return
	len < (1 <<  7) ? 1 :
	len < (1 <<  8) ? 2 :
	len < (1 << 16) ? 3 :
	len < (1 << 24) ? 4 : 5;
}

/*
 * Encode an ASN.1 DER length field
 */
static void der_length_put(u_char **pp, int len)
{
	int sz = der_length_size(len);
	u_char *p = *pp;
	
	if (sz == 1) {
		*p++ = (u_char) len;
	} else {
		*p++ = (u_char) ((sz-1) | 0x80);
		sz -= 1;
		while (sz--)
			*p++ = (u_char) ((len >> (sz * 8)) & 0xff);
	}
	
	*pp = p;
}

/*
 * decode an ASN.1 DER length field
 */
static int der_length_get(const uint8_t **dptr, const uint8_t *endptr, size_t *len)
{
	size_t i;
	const uint8_t *p = *dptr;
	
	*len = 0;
	if (*p &  0x80) {
		size_t max_len = *p & 0x7f;
		
		/* We limit max length to a size that can fit into a size_t */
		if (((max_len + p) > endptr) || (max_len > (sizeof (*len))))
			return(EBADRPC);
		for (i = max_len; i > 0; i--)
			*len = (*len << 8) + *++p;
	} else
		*len = *p;
	
	if (p >= endptr)
		return(EBADRPC);
	/* Make sure we don't wraparound p */
	if (((*len+p+1) > endptr) || ((*len+p) < p)) {
		SMBDEBUG("bad length field %ld\n", *len);
		return(EBADRPC);
	}
	
	*dptr = p + 1;
	
	return (0);
}

/*
 * Parse the incoming message and get the accept complete response. This routine
 * is called for both the challenge response and the auth response. 
 */
static uint8_t *parse_accept_complete_response(struct smb_gss *gp, const uint8_t *token,
											   const uint8_t *token_end, int *linenum)
{
	size_t length;
	
	/* 
	 * Parse the wrapper
	 *
	 *		a1		context tag for choice 1
	 *		Length
	 *		30		Sequence	 
	 *		Length
	 *
	 *		a0		context tag for choice 1
	 *		Length
	 *		0a		Enumerated
	 *		Length
	 *		GSS_C_COMPLETE
	 */
	if ((token >= token_end) || (*token++ != ASN1_CONTEXT_TAG(1))) {
		*linenum = __LINE__;
		return NULL;
	} 
	
	if (der_length_get(&token, token_end, &length)) {
		*linenum = __LINE__;
		return NULL;
	}
	
	if ((token >= token_end) || (*token++ != ASN1_SEQUENCE_TAG(0))) {
		*linenum = __LINE__;
		return NULL;
	} 	
	
	if (der_length_get(&token, token_end, &length)) {
		*linenum = __LINE__;
		return NULL;
	}
	
	if ((token >= token_end) || (*token++ != ASN1_CONTEXT_TAG(0))) {
		*linenum = __LINE__;
		return NULL;
	} 
	
	if (der_length_get(&token, token_end, &length)) {
		*linenum = __LINE__;
		return NULL;
	}
	
	if ((token >= token_end) || (*token++ != ASN1_ENUMERATED_TAG)) {
		*linenum = __LINE__;
		return NULL;
	}
	
	if (der_length_get(&token, token_end, &length)) {
		*linenum = __LINE__;
		return NULL;
	}
	
	if (token >= token_end) {
		*linenum = __LINE__;
		return NULL;
	}
	
	gp->gss_major = *token++;
	return ((uint8_t *)token);		
}

/* 
 * This routine will parse the auth response sent by the server. Not sure what
 * we should do if we find anything wrong, for now just log it.
 */
static int parse_ntlmssp_negotiate_auth_response(struct smb_vc *vcp)
{
	struct smb_gss *gp = &vcp->vc_gss;
	const uint8_t *token = gp->gss_token;
	uint32_t token_len = gp->gss_tokenlen;
	const uint8_t *token_end = token + token_len;
	int linenum = 0;

	/* 
	 * Parse the wrapper
	 *
	 *  From 2008 trace = 0xa1 0x07 0x30 0x05
	 *		a1		context tag for choice 1
	 *		Length
	 *		30		Sequence	 
	 *		Length
	 *
	 *  From 2008 trace = 0xa0 0x03 0x0a 0x01 0x00
	 *		a0		context tag for choice 1
	 *		Length
	 *		0a		Enumerated
	 *		Length
	 *		GSS_C_COMPLETE
	 */
	token = parse_accept_complete_response(gp, token, token_end, &linenum);
	if (token == NULL)
		goto bad;

	if (gp->gss_major != GSS_C_COMPLETE) {
		SMBERROR("Expected complete, got gp->gss_major = %d\n", gp->gss_major);
		linenum = __LINE__;
		goto bad;
	}
	goto done;
	
bad:
	gp->gss_major = GSS_C_COMPLETE;
	SMBWARNING("Parsing error on line number -  %d\n", linenum);
done:	
	if (gp->gss_token)
		free(gp->gss_token, M_TEMP);
	gp->gss_token = NULL;
	gp->gss_tokenlen = 0;
	return 0;	
	
}

/*
 * Check to see if the server forces all authentication to guest. Windows Simple
 * File Sharing (XPHome).
 */
static void CheckIfServerForcesAllAuthenticationToGuest(struct smb_vc *vcp, uint16_t *startptr, uint16_t len)
{
	uint16_t *endptr;
	
	smb_hexdump(__FUNCTION__, "taget info", (void *)startptr, len);
	endptr = startptr + len;
	
	while ((startptr+2) < endptr) {
		uint16_t AvId = htoles(*startptr++);
		uint16_t AvLen = htoles(*startptr++);
		
		switch (AvId) {
			case kMsvAvFlags:
				if (((startptr+sizeof(uint32_t)) > endptr) || (AvLen != sizeof(uint32_t))) {
					SMBERROR("kMsvAvFlags: Bad size AvLen = %d\n", AvLen); 
				} else {
					u_int32_t GuestAccessRequired = htolel(*(u_int32_t *)startptr);
					
					SMBWARNING("kMsvAvFlags: GuestAccessRequired = %d\n", GuestAccessRequired); 
					if (GuestAccessRequired)
						vcp->vc_flags |= SMBV_GUEST_ACCESS;
				}
				return;
				break;
			case kMsvAvNbComputerName:
			case kMsvAvNbDomainName:
			case kMsvAvDnsComputerName:
			case kMsvAvDnsDomainName:
			case kMsvAvDnsTreeName:
			case kMsvAvTimestamp:
			case kMsAvRestrictions:
				startptr = (uint16_t *)((uint8_t *)startptr + AvLen);
				SMBDEBUG("AvId = %d AvLen = %d startptr %p\n", AvId, AvLen, startptr); 
				break;
			case kMsvAvEOL:	/* We are done */
				SMBDEBUG("kMsvAvEOL: Found the end\n"); 
				return;
				break;
			default:	/* item we don't understand */
				startptr = (uint16_t *)((uint8_t *)startptr + AvLen);
				SMBWARNING("UNKNOWN AvId = %d AvLen = %d startptr %p\n", AvId, AvLen,startptr); 
				break;
		}
	}
}

/* 
 * This routine will parse the challenge request sent by
 * the server. We need to get the ntlm flags, the server
 * challenge and the target information if there is any.
 */
static int parse_ntlmssp_negotiate_challenge(struct smb_vc *vcp, uint32_t *ntlmflags,
											  void **target_info, uint16_t *target_len)
{	
	struct smb_gss *gp = &vcp->vc_gss;
	const uint8_t *token = gp->gss_token;
	uint32_t token_len = gp->gss_tokenlen;
	const uint8_t *token_end = token + token_len;
	size_t length;
	uint32_t *ntlmssp;
	uint32_t ntlm_message_type;
	struct SSPSecurityBuffer domain;
	struct SSPSecurityBuffer address_list;
	uint32_t local_context[2];
	uint8_t	ntlm_os_version[8];
	int error = 0;
	int linenum = 0;
	const uint8_t *domain_offset_ptr = NULL;
	const uint8_t *address_offset_ptr = NULL;

	/* Because of compiler warning do it this way */
	bzero(&domain, sizeof(domain));
	bzero(&address_list, sizeof(address_list));
	/* 
	 * Parse the wrapper
	 *
	 *  From 2003 trace = 0xa1 0x81 0xd6 0x30 0x81 0xd3
	 *		a1		context tag for choice 1
	 *		Length
	 *		30		Sequence	 
	 *		Length
	 *
	 *  From 2003 trace = 0xa0 0x03 0x0a 0x01 0x01
	 *		a0		context tag for choice 1
	 *		Length
	 *		0a		Enumerated
	 *		Length
	 *		GSS_C_CONTINUE_NEEDED 
	 *
	 *
	 *  From 2003 trace = 0xa1 0x0c 0x06 0x0a 0x2b 0x06 0x01 0x04 0x01 0x82 0x37 0x02 0x02 0x0a
	 *		a1		context tag for choice 1
	 *		0c		length field (der encoded) ( 1 byte) should always be 12
	 *		06		OID tag primitive length (1 byte)
	 *		0a		length field (der encoded) always SPNEGO_mechType_NTLMSSP_LEN (1 byte)
	 *		SPNEGO_mechType_NTLMSSP
	 *
	 *  From 2003 trace = 0xa2 0x81 0xbd 0x04 0x81 0xba
	 *		a2		context tag for choice 2
	 *		Length
	 *		04		Sequence	 
	 *		Length
	 *
	 */
	token = parse_accept_complete_response(gp, token, token_end, &linenum);
	if (token == NULL)
		goto bad;
	
	if (gp->gss_major != GSS_C_CONTINUE_NEEDED) {
		SMBERROR("Expected continue need, got gp->gss_major = %d\n", gp->gss_major);
		linenum = __LINE__;
		goto bad;
	}
	
	if ((token >= token_end) || (*token++ != ASN1_CONTEXT_TAG(1))) {
		linenum = __LINE__;
		goto bad;
	}
	
	if (der_length_get(&token, token_end, &length)) {
		linenum = __LINE__;
		goto bad;
	}
	
	if ((token >= token_end) || (*token++ != ASN1_OID_TAG)) {
		linenum = __LINE__;
		goto bad;
	}
	
	/* Note der_length_get will make sure length fits in the buffer */
	if (der_length_get(&token, token_end, &length)) {
		linenum = __LINE__;
		goto bad;
	}
	
	if (length != SPNEGO_mechType_NTLMSSP_LEN) {
		linenum = __LINE__;
		goto bad;
	}
	
	if (bcmp(token, SPNEGO_mechType_NTLMSSP, SPNEGO_mechType_NTLMSSP_LEN) != 0) {
		linenum = __LINE__;
		goto bad;
	}
	token += SPNEGO_mechType_NTLMSSP_LEN;
	
	if ((token >= token_end) || (*token++ != ASN1_CONTEXT_TAG(2))) {
		linenum = __LINE__;
		goto bad;
	} 
	if (der_length_get(&token, token_end, &length)) {
		linenum = __LINE__;
		goto bad;
	}
	
	if ((token >= token_end) || (*token++ != 0x04)) {
		linenum = __LINE__;
		goto bad;
	} 
	
	/* Note der_length_get will make sure length fits in the buffer */
	if (der_length_get(&token, token_end, &length)) {
		linenum = __LINE__;
		goto bad;
	}
	
	if (length < sizeof(NTLMSSP_Signature)) {
		linenum = __LINE__;
		goto bad;
	}
	/* We are done with the SPENGO message, now start decodeing the NTLMSSP message */
	ntlmssp = (uint32_t *)token;	/* Hold on to the starting NTLMSSP message */ 
	if (strncmp((char *)token, (char *)NTLMSSP_Signature, length) != 0) {
		SMBERROR("The NTLMSSP_Signature messages is wrong!\n");
		goto bad;		
	}
	token += sizeof(NTLMSSP_Signature);
	
	if ((token+sizeof(ntlm_message_type)) >= token_end) {
		linenum = __LINE__;
		goto bad;		
	}
	
	ntlm_message_type = letohl(*(uint32_t *)token);
	token += sizeof(ntlm_message_type);
	if ( ntlm_message_type != NTLMSSP_TypeTwoMessage) {
		SMBDEBUG("Wrong NTLM Message type got %d expected %d!\n", ntlm_message_type , NTLMSSP_TypeTwoMessage);
		linenum = __LINE__;
		goto bad;				
	}
	if ((token+sizeof(domain)) >= token_end) {
		linenum = __LINE__;
		goto bad;
	}
	/* 
	 * Get the location of the domain. The online documentation calls this 
	 * field target, but WireShark calls it domain 
	 */
	domain.length = letohs(*(uint16_t *)token);
	token += sizeof(domain.length);
	domain.allocated = letohs(*(uint16_t *)token);
	token += sizeof(domain.allocated);
	domain.offset = letohl(*(uint32_t *)token);
	token += sizeof(domain.offset);
	/*
	 * We need to verify the domain offset and length fields here
	 * so we don't have to worry about it down below. So lets be extra
	 * carefull and do the following checks:
	 *
	 * 1. Verify that offset plus ntlmssp doesn't overflow.
	 * 2. Verify that the domain pointer isn't past the end of buffer
	 * 3. Verify that length plus domain pointer doesn't overflow.
	 * 4. Verify that the domain pointer plus length isn't past the end of buffer
	 */
	if (domain.offset > (VM_MAX_ADDRESS - (vm_offset_t)ntlmssp)) {
		linenum = __LINE__;
		goto bad;		
	}
	domain_offset_ptr = ((uint8_t *)ntlmssp)+domain.offset;
	if (domain_offset_ptr > token_end) {
		linenum = __LINE__;
		goto bad;
	}
	if ((domain.offset+domain.length) > (VM_MAX_ADDRESS - (vm_offset_t)ntlmssp)) {
		linenum = __LINE__;
		goto bad;		
	}
	if ((domain_offset_ptr+domain.length) > token_end) {
		linenum = __LINE__;
		goto bad;
	}
	/* 
	 * One final check here, just to make sure we have a domain, if not
	 * then point it to one byte past the end of the buffer. This will 
	 * force all the checks below to work. We will test later and make 
	 * sure its not past the end of the buffer.
	 */
	if ((domain.length == 0) || (domain.offset == 0))
		domain_offset_ptr = token_end + 1;
		
	if ((token+sizeof(ntlmflags)) > token_end) {
		linenum = __LINE__;
		goto bad;
	}
	
	/* Get the Flags */
	*ntlmflags = letohl(*(uint32_t *)token);
	token += sizeof(*ntlmflags);
	
	SMBDEBUG("ntlmflags = 0x%x XP_CHALLENGE_NTLM_FLAGS = 0x%x\n", *ntlmflags, XP_CHALLENGE_NTLM_FLAGS);
	
#ifdef DEBUG_NTLM_BY_TURNING_OFF_NTLMV2
	SMBERROR("Turning off NTLMV2 support\n");
	vcp->vc_flags |= SMBV_NTLMV2_OFF;
#endif // DEBUG_NTLM_BY_TURNING_OFF_NTLMV2
	
	/*
	 * This may be overkill, but we should make sure they are using the same
	 * encoding that we requested. We expect the same encoding that was 
	 * negotiated at the virtual circtuit level.
	 *
	 * This is not required by the NTLMSSP documents, but we are going to 
	 * require it in the kernel. If we move this out of the kernel then this
	 * requirement should go aways.
	 */
	if (vcp->vc_sopt.sv_caps & SMB_CAP_UNICODE) {
		if ((*ntlmflags & NTLMSSP_NEGOTIATE_UNICODE) != NTLMSSP_NEGOTIATE_UNICODE) {
			SMBERROR("Virtual Circuit is doing unicode, but ntlmssp doesn't support it!\n");
			linenum = __LINE__;
			goto bad;					
		}
	} else {
		if ((*ntlmflags & NTLM_NEGOTIATE_OEM) != NTLM_NEGOTIATE_OEM) {
			SMBERROR("Virtual Circuit is not doing unicode, but ntlmssp doesn't support OEM!\n");
			linenum = __LINE__;
			goto bad;					
		}
	}
	
	if ((token+sizeof(vcp->vc_ch)) > token_end) {
		linenum = __LINE__;
		goto bad;
	}
	
	/* ntlm_challenge */
	bcopy(token, vcp->vc_ch, sizeof(vcp->vc_ch));
	token += sizeof(vcp->vc_ch);
	
	smb_hexdump(__FUNCTION__, "vc_ch = ", vcp->vc_ch, sizeof(vcp->vc_ch));
	
	/* 
	 * Everything after this point is optional and should only cause an error 
	 * if present. If no domain field then domain_offset_ptr will point to one
	 * byte past the end of the buffer.
	 */
	if (((token+sizeof(local_context)) <= domain_offset_ptr) && (token_end >= (token+sizeof(local_context)))) {
		/* Never used by us, but lets get it anyway, local authenication stuff (Displayed as Reserved by Ethereal) */
		local_context[0] = letohl(*(uint32_t *)token);
		token += sizeof(local_context[0]);
		local_context[1] = letohl(*(uint32_t *)token);
		token += sizeof(local_context[1]);
	} else {
		SMBWARNING("local_context check failed: token = %p domain_offset_ptr = %p token_end = %p\n", 
				   token, domain_offset_ptr, token_end);
		goto skip_optional;
	}
	
	if (((token+sizeof(address_list)) <= domain_offset_ptr) && (token_end >= (token+sizeof(address_list)))) {
		/* Get the location of the target info (address list) */
		address_list.length = letohs(*(uint16_t *)token);
		token += sizeof(address_list.length);
		address_list.allocated = letohs(*(uint16_t *)token);
		token += sizeof(address_list.allocated);
		address_list.offset = letohl(*(uint32_t *)token);
		token += sizeof(address_list.offset);
	} else {
		SMBWARNING("address_list check failed: token = %p domain_offset_ptr = %p token_end = %p\n", 
				   token, domain_offset_ptr, token_end);
		goto skip_optional;
	}
	
	/* There can also be OS Version Structure (Optional) here but we ignore it for now */		
	if (((token+sizeof(ntlm_os_version)) <= domain_offset_ptr) && (token_end >= (token+sizeof(ntlm_os_version)))) {
		bcopy(token, ntlm_os_version, sizeof(ntlm_os_version));
		token += sizeof(ntlm_os_version);
	} else {
		SMBWARNING("ntlm_os_version check failed: token = %p domain_offset_ptr = %p token_end = %p\n", 
				   token, domain_offset_ptr, token_end);
		goto skip_optional;
	}
	
skip_optional:
	
	/* 
	 * If the user doesn't supply a domain should we use the servers domain. Windows seems
	 * to try there host name first and then fallback to this method. I need to do some 
	 * testing before adding this code. What should we do if the authentication fails? 
	 *
	 * At this point we have already verified domain_offset_ptr
	 *
	 */
	if (domain_offset_ptr >= token_end) {
		SMBERROR("Bad Domain offset/length: length = %d allocated = %d offset = %d\n", domain.length, domain.allocated, domain.offset);
		goto bad;
	}
	/*
	 * See Radar 6650825
	 *
	 * Seems Samba plays with the NTLMSSP message when doing pass-through. When authenticating
	 * to these servers a domain name is required. So if the user didn't supply a domain
	 * we will now use the one supplied by the server. We have added an undocumented option
	 * use_server_domain that can turn this behavior off.
	 *
	 * NOTE: All Mac OS X Samba Servers require a domain.
	 */
	if (((vcp->vc_domain == NULL) || (vcp->vc_domain[0] == 0)) && (vcp->vc_flags & SMBV_SERVER_DOMAIN)) {
		char * domain_str = NULL;
		if (*ntlmflags & NTLMSSP_NEGOTIATE_UNICODE) {
			size_t maxlen = (domain.length * 3) + 2;
			
			domain_str = malloc(maxlen, M_SMBTEMP, M_WAITOK);
			if (domain_str)
				smb_unitostr(domain_str, (const u_int16_t *)domain_offset_ptr, domain.length, maxlen, 0);
		} else {
			domain_str = malloc(domain.length+1, M_SMBTEMP, M_WAITOK);
			if (domain_str) {
				memcpy(domain_str, (char *)domain_offset_ptr, domain.length);
				domain_str[domain.length] = 0;				
			}
		}
		if (domain_str) {
			SMB_STRFREE(vcp->vc_domain);
			vcp->vc_domain = domain_str;
			SMBWARNING("Using servers domain %s, use the use_server_domain option to turn this off.\n", domain_str);
		} 
	}

	SMBDEBUG(" Target Info length = %d allocated = %d offset = %d\n", address_list.length, address_list.allocated, address_list.offset);
	/* Target Information (optional) */
	if ((address_list.length == 0) || (address_list.allocated == 0)) {
		SMBDEBUG("No target information length = %d allocated = %d offset = %d!\n", address_list.length, address_list.allocated, address_list.offset);	
		error = 0;
		goto done;
	} 
	
	/*
	 * We need to verify the address offset and length fields. So lets be extra
	 * carefull and do the following checks:
	 *
	 * 1. Verify that offset plus ntlmssp doesn't overflow.
	 * 2. Verify that the domain pointer isn't past the end of buffer
	 * 3. Verify that length plus domain pointer doesn't overflow.
	 * 4. Verify that the domain pointer plus length isn't past the end of buffer
	 *
	 * So does a failure here mean a failure or do we just act like we didn't get
	 * the target info?
	 */
	if (address_list.offset > (VM_MAX_ADDRESS - (vm_offset_t)ntlmssp)) {
		linenum = __LINE__;
		goto bad;		
	}
	address_offset_ptr = ((uint8_t *)ntlmssp)+address_list.offset;
	if (address_offset_ptr > token_end) {
		linenum = __LINE__;
		goto bad;
	}
	if ((address_list.offset+address_list.length) > (VM_MAX_ADDRESS - (vm_offset_t)ntlmssp)) {
		linenum = __LINE__;
		goto bad;		
	}
	if ((address_offset_ptr+address_list.length) > token_end) {
		linenum = __LINE__;
		goto bad;
	}
	/* Since address_list.length is a uint16_t this is a pretty safe malloc */
	*target_info = malloc(address_list.length, M_SMBTEMP, M_WAITOK);
	if (*target_info) {		
		memcpy(*target_info, (void *)address_offset_ptr, address_list.length);
		*target_len = address_list.length;
		CheckIfServerForcesAllAuthenticationToGuest(vcp, (uint16_t *)*target_info, *target_len);
	}
	error = 0;
	goto done;
	
bad:
	/* If no error then we couldn't parse it */
	if (!error)
		error = EBADRPC;
	gp->gss_major = GSS_C_COMPLETE;
	if (linenum)
		SMBWARNING("Parsing error on line number -  %d error = %d\n", linenum, error);
done:	
	if (gp->gss_token)
		free(gp->gss_token, M_TEMP);
	gp->gss_token = NULL;
	gp->gss_tokenlen = 0;
	return error;	
}

/*
 * This routine will build the auth response to be sent
 * to the server. 
 *
 */
static int create_ntlmssp_negotiate_auth(struct smb_vc *vcp, uint32_t ntlmflags, 
										 void *target_info, u_int16_t target_len)
{	
	struct smb_gss *gp = &vcp->vc_gss;
	uint32_t *ntlmssp;
	int error = 0;
	uint32_t LengthFieldBytes;
	uint32_t ntlmssp_len;
	uint32_t LengthField[4];
	struct SSPSecurityBuffer secbuffer;
	u_char *pptr, *start_ntlm, *offset_ptr;
	void *lmv2 = NULL, *ntlmv2 = NULL;
	size_t lmv2_len = 0, ntlmv2_len = 0;
	u_int16_t *username = NULL;
	u_int16_t *hostname = NULL;
	u_int16_t *domain_str = NULL;
	u_int64_t server_nonce = *(u_int64_t *)vcp->vc_ch;
	size_t hostname_len = strnlen(vcp->vc_localname, SMB_MAXNetBIOSNAMELEN+1);
	size_t username_len = strnlen(vcp->vc_username, SMB_MAXUSERNAMELEN + 1);
	size_t domain_len = strnlen(vcp->vc_domain, SMB_MAXNetBIOSNAMELEN + 1);
	size_t keyExchange_len = 0;
	u_int8_t *keyExchange = NULL;
	u_int16_t length;
	
	DBG_ASSERT(gp->gss_token == NULL);
#ifdef SMB_DEBUG
	/* We are doing anonymous then we shouldn't have a useranme or domain */
	if (vcp->vc_flags & SMBV_ANONYMOUS_ACCESS) {
		DBG_ASSERT(username_len == 0)
		DBG_ASSERT(domain_len == 0)
	}
#endif // SMB_DEBUG

	/* We have a domain name, create its unicode name */
	if (domain_len) {		
		domain_len += 1;	/* Make room for the null terminator */
		domain_str = malloc(domain_len * 2, M_SMBNODENAME, M_WAITOK);
		if (!domain_str) {
			SMBDEBUG("Can't allocate domain_str pointer?\n");
			error =  ENOMEM;
			goto done;				
		}
		domain_len = smb_strtouni(domain_str, (const char *)vcp->vc_domain, domain_len, UTF_NO_NULL_TERM);
	}
	
	/* We have a user name, create its unicode name */
	if (username_len) {
		username_len += 1;	/* Make room for the null terminator */
		username = malloc(username_len * 2, M_SMBNODENAME, M_WAITOK);
		if (!username) {
			SMBDEBUG("Can't allocate username pointer\n");
			error =  ENOMEM;
			goto done;				
		}
		username_len = smb_strtouni(username, (const char *)vcp->vc_username, username_len, UTF_NO_NULL_TERM);		
	}
	
	/* We have a local host name, create its unicode name */
	if (hostname_len) {
		hostname_len += 1;	/* Make room for the null terminator */
		hostname = malloc(hostname_len * 2, M_SMBNODENAME, M_WAITOK);
		if (!hostname) {
			SMBDEBUG("Can't allocate hostname pointer\n");
			error =  ENOMEM;
			goto done;				
		}
		hostname_len = smb_strtouni(hostname, (const char *)vcp->vc_localname, hostname_len, UTF_NO_NULL_TERM);
	}
	/*
	 * From the Microsoft documtation (Look at the NOTE for NTLMv1 and NTLMv2):
	 *
	 *		3.3.1   NTLM v1 Authentication 
	 *			The following pseudocode defines the details of the algorithms 
	 *			used to calculate the keys used in NTLM v1 authentication.  
	 *			NOTE: The NTLM authentication version is not negotiated by the 
	 *				  protocol. It must be configured on both the client and the
	 *				  server prior to authentication. 
	 *
	 *
	 *		3.3.2   NTLM v2 Authentication 
	 *			The following pseudo code defines the details of the algorithms 
	 *			used to calculate the keys used in NTLM v2 authentication.
	 *			NOTE: The NTLM authentication version is not negotiated by the 
	 *				  protocol. It must be configured on both the client and the
	 *				  server prior to authentication. 
	 *
	 * Those notes seem to indicate that NTLMv1 and NTLMv2 are only configuration
	 * options and do not get negotiated through the protocol. So far I have not found
	 * any servers that support extend security, but don't support NTLMv2. So we 
	 * always do NTLMv2 unless the user has configured us to have NTLMv2 turned off.
	 */
	/* Now create the anonymous, ntlmv2/lmv2 or ntlm/lm blobs */
	if (vcp->vc_flags & SMBV_ANONYMOUS_ACCESS) {
		/*
		 * The Anonymous Response
		 *
		 * The Anonymous Response is seen when the client is establishing an 
		 * anonymous context, rather than a true user-based context. This is 
		 * typically seen when a "placeholder" is needed for operations that 
		 * do not require an authenticated user. Anonymous connections are not 
		 * the same as the Windows "Guest" user (the latter is an actual user 
		 * account, while anonymous connections are associated with no account 
		 * at all).
		 *
		 * In an anonymous Type 3 message, the client indicates the "Negotiate 
		 * Anonymous" flag; the NTLM response field is empty (zero-length); and
		 * the LM response field contains a single null byte ("0x00").
		 */
		ntlmv2_len = 0;
		ntlmv2 = NULL;
		lmv2 = NULL;
		lmv2_len = 1;
		
	} else if ((vcp->vc_flags & SMBV_NTLMV2_OFF) != SMBV_NTLMV2_OFF) {
		uint8_t ntlmv2Hash[SMB_NTLMV2_LEN];
		u_int64_t client_nonce;
			
		read_random((void *)&client_nonce, (u_int)sizeof(client_nonce));
			
		smb_ntlmv2hash(ntlmv2Hash, vcp->vc_domain, vcp->vc_uppercase_username, smb_vc_getpass(vcp));			
			
		lmv2 = smb_lmv2_response(ntlmv2Hash, server_nonce, client_nonce, &lmv2_len);
		if (lmv2 == NULL)
			lmv2_len = 0;
		ntlmv2 = make_ntlmv2_blob(client_nonce, target_info, target_len, &ntlmv2_len);
		if (ntlmv2) {
			smb_ntlmv2_response(ntlmv2Hash, ntlmv2, ntlmv2_len, server_nonce);
			smb_calcv2mackey(vcp, ntlmv2Hash, ntlmv2, NULL, 0);
		}
	} else {
		if (vcp->vc_flags & SMBV_MINAUTH_NTLM) {
			/* Don't put the LM response on the wire - it's too easy to crack. */
			lmv2 = NULL;
			lmv2_len = 0;
		} else {
			/*
			 * Compute the LM response, derived from the challenge and the ASCII password.
			 *
			 * We try w/o uppercasing first so Samba mixed case passwords work.
			 * If that fails, we come back and try uppercasing to satisfy OS/2 and Windows for Workgroups.
			 *
			 * We no longer try uppercase passwords any more. The above comment said we only did this for
			 * OS/2 and Windows for Workgroups. We no longer support those systems. The old code could fail
			 * three times and lock people out of their account. Now we never try more than twice. In the 
			 * future we should clean this code up, but thats for another day when I have more time to test.
			 */
			lmv2_len = 24;
			lmv2 = malloc(lmv2_len, M_SMBTEMP, M_WAITOK);
			if (lmv2)
				smb_lmresponse((u_char *)smb_vc_getpass(vcp), (u_char *)&server_nonce, (u_char *)lmv2);
			else 
				lmv2_len = 0;
		}
		
		/*
		 * Compute the NTLM response, derived from the challenge and the password.
		 */
		ntlmv2_len = 24;
		ntlmv2 = malloc(ntlmv2_len, M_SMBTEMP, M_WAITOK);
		if (ntlmv2)
			smb_ntlmresponse((u_char *)smb_vc_getpass(vcp), (u_char *)&server_nonce, (u_char*)ntlmv2);
		else
			ntlmv2_len = 0;
		smb_calcmackey(vcp, NULL, 0);
	}
	
	/* Determine the length of our Type One NTLMSSP message */
	ntlmssp_len = (uint32_t)(sizeof(NTLMSSP_Signature) + sizeof(uint32_t)  + 
					(sizeof(struct SSPSecurityBuffer) * 6) + sizeof(ntlmflags) + 
					lmv2_len + ntlmv2_len + username_len + hostname_len + domain_len +
					keyExchange_len);
	
#ifdef SMB_DEBUG
	/* We only send the version field if we are building in debug mode */
	ntlmssp_len += NTLMSSP_NEGOTIATE_VERSION_LEN;
#endif // SMB_DEBUG
	/*
	 *  From XP trace = 0xa1 82 01 16 30 82 01 12
	 *		a1		context tag for choice 1
	 *		Length	length field (der encoded) - size of data past this point 2
	 *		30		Sequence
	 *		Length	length field (der encoded) - size of data past this point 1 
	 *
	 *  From XP trace = 0xa2 82 01 0e 04 82 01 0a
	 *		a2		context tag for choice 2
	 *		Length	length field (der encoded) - size of data past this point 0
	 *		04		Sequence
	 *		Length	length field (der encoded) - size of data past this point
	 */
	/* Total size of wrapper except for the unknown length fields */
	gp->gss_tokenlen = 2 + 2;
	
	/* Now lets figure out the unknown length fields sizes working backwards */
	LengthFieldBytes = der_length_size(ntlmssp_len);
	LengthField[0] = ntlmssp_len + LengthFieldBytes + 1;
	LengthFieldBytes += der_length_size(LengthField[0]);
	LengthField[1] = ntlmssp_len + LengthFieldBytes + 2;
	LengthFieldBytes += der_length_size(LengthField[1]);
	LengthField[2] = ntlmssp_len + LengthFieldBytes + 3;
	LengthFieldBytes += der_length_size(LengthField[2]);
	
	gp->gss_tokenlen += LengthFieldBytes + ntlmssp_len;
	/* Lets create the buffer big enough to hold the NTLMSSP message, plus the SPNEGO wrapper */
	gp->gss_token = malloc(gp->gss_tokenlen, M_SMBTEMP, M_WAITOK);
	if (gp->gss_token == NULL) {
		SMBERROR("Can't allocate token pointer\n");
		error =  ENOMEM;
		goto done;		
	}
	memset(gp->gss_token, 0, gp->gss_tokenlen);
	pptr = gp->gss_token;
	
	*pptr++ = ASN1_CONTEXT_TAG(1);
	der_length_put(&pptr, LengthField[2]);
	*pptr++ = ASN1_SEQUENCE_TAG(0);
	der_length_put(&pptr, LengthField[1]);
	
	*pptr++ = ASN1_CONTEXT_TAG(2);
	der_length_put(&pptr, LengthField[0]);
	*pptr++ = 0x04;
	der_length_put(&pptr, ntlmssp_len);
	/* Now the NTLMSSP message */
	start_ntlm = pptr;
	strlcpy((char *)pptr, NTLMSSP_Signature, ntlmssp_len);
	pptr += sizeof(NTLMSSP_Signature);
	
	ntlmssp = (uint32_t *)pptr;
	*ntlmssp++ = letohl(NTLMSSP_TypeThreeMessage);
	pptr = (void *)ntlmssp;
	
	/*
	 * The security buffers need to be in the following order
	 * LMv2, NTLMv2, Domain name, User name and then Host name
	 *
	 * Now since the Domain name, User name and Host name may be in
	 * UNICODE and that means they have to start on even bounderies, 
	 * we need to put them in the buffer before the LMv2 and NTLMv2 
	 * blobs.
	 * 
	 * So when putting the data into the buffer we need to do it in 
	 * the following order Domain name, User name and  Host name,
	 * LMv2 and then NTLMv2.
	 */

	/* 
	 * Start with the end of the buffer and work backwards to find the
	 * location for the LMv2 data.
	 */
	offset_ptr = start_ntlm + ntlmssp_len - (lmv2_len + ntlmv2_len);
	/* Security Buffer for LMv2 */
	length = (u_int16_t)lmv2_len;
	secbuffer.length = letohs(length);
	secbuffer.allocated = secbuffer.length;
	secbuffer.offset = letohl(offset_ptr - start_ntlm); /* Offset to LMv2 data */
	memcpy(pptr, (void *)&secbuffer, sizeof(secbuffer));
	pptr += sizeof(secbuffer);
	/* Now put the LMv2 in the buffer */
	if (lmv2)
		memcpy(offset_ptr, lmv2, lmv2_len);
	/* 
	 * The NTLMv2 data always follows the LMv2 so just move the pointer 
	 * the length of the LMv2 data.
	 */
	offset_ptr += lmv2_len;
	
	/* Security Buffer for NTLMv2 */
	length = (u_int16_t)ntlmv2_len;
	secbuffer.length = letohs(length);
	secbuffer.allocated = secbuffer.length;
	secbuffer.offset = letohl(offset_ptr - start_ntlm); /* Offset to NTLMv2 data */
	memcpy(pptr, (void *)&secbuffer, sizeof(secbuffer));
	pptr += sizeof(secbuffer);
	/* Now put the NTLMv2 at the end of the buffer */
	if (ntlmv2)
		memcpy(offset_ptr, ntlmv2, ntlmv2_len);
	
	/*
	 * Now reset the offset pointer to point at the begining of the data
	 * blob. This is where we will put the Domain Name.
	 */
	offset_ptr = start_ntlm + ntlmssp_len - (lmv2_len + ntlmv2_len + username_len
											 + hostname_len + domain_len + keyExchange_len);
	
	/* Security Buffer for Domain Name */
	length = (u_int16_t)domain_len;
	secbuffer.length = letohs(length);
	secbuffer.allocated = secbuffer.length;
	secbuffer.offset = letohl(offset_ptr - start_ntlm); /* Offset to Domain Name */
	memcpy(pptr, (void *)&secbuffer, sizeof(secbuffer));
	pptr += sizeof(secbuffer);
	/* Now put the Domain Name at the end of the buffer */
	if (domain_str)
		memcpy(offset_ptr, domain_str, domain_len);
	/* Find the offset in the buffer for the start of User Name */
	offset_ptr += domain_len;
	
	/* Security Buffer for User Name */
	length = (u_int16_t)username_len;
	secbuffer.length = letohs(length);
	secbuffer.allocated = secbuffer.length;
	secbuffer.offset = letohl(offset_ptr - start_ntlm); /* Offset to User Name */
	memcpy(pptr, (void *)&secbuffer, sizeof(secbuffer));
	pptr += sizeof(secbuffer);
	/* Now put the User Name at the end of the buffer */
	if (username)
		memcpy(offset_ptr, username, username_len);
	/* Find the offset in the buffer for the start of Host Name */
	offset_ptr += username_len;
	
	/* Security Buffer for Host Name */
	length = (u_int16_t)hostname_len;
	secbuffer.length = letohs(length);
	secbuffer.allocated = secbuffer.length;
	secbuffer.offset = letohl(offset_ptr - start_ntlm); /* Offset to Host Name */
	memcpy(pptr, (void *)&secbuffer, sizeof(secbuffer));
	pptr += sizeof(secbuffer);
	/* Now put the Host Name at the end of the buffer */
	if (hostname)
		memcpy(offset_ptr, hostname, hostname_len);
	/* Find the offset in the buffer for the start of Session Key */
	offset_ptr += hostname_len;

	/*
	 * Currently we do not support key exchange. I have not seen any reason to 
	 * at this point, but if we do in the future this part of the code is ready.
	 * Remember we would have to negotiate key exchange and then create the new
	 * session key.
	 */
	if (ntlmflags & NTLMSSP_NEGOTIATE_KEY_EXCH) {
		SMBERROR("Key exchange not currently supported\n");
	} else {
		/* Just some extra debug checks */
		DBG_ASSERT(keyExchange_len == 0);
		DBG_ASSERT(keyExchange == NULL);
	}
	
	/* Security Buffer for Session Key Exchange */
	length = (u_int16_t)keyExchange_len;
	secbuffer.length = letohs(length);
	secbuffer.allocated = secbuffer.length;
	secbuffer.offset = letohl(offset_ptr - start_ntlm); /* Offset to Session Key */
	memcpy(pptr, (void *)&secbuffer, sizeof(secbuffer));
	pptr += sizeof(secbuffer);
	/* Now put the Host Name at the end of the buffer */
	if (keyExchange)
		memcpy(offset_ptr, keyExchange, keyExchange_len);

	ntlmssp = (uint32_t *)pptr;
	
	/*
	 * Type Three Message Flags
	 *
	 * The XP_AUTH_NTLM_FLAGS is a set of flags sent by XP going to a stand
	 * alone server and a domain server. 
	 * 
	 * The SMB_AUTH_NTLM_FLAGS is a subset of the Windows XP flags.
	 *
	 * So XP sets both the  NTLMSSP_NEGOTIATE_UNICODE and NTLM_NEGOTIATE_OEM
	 * flags. We only set the NTLMSSP_NEGOTIATE_UNICODE if the server supports 
	 * UNICODE. Also we only set the NTLM_NEGOTIATE_OEM if the server doesn't 
	 * support UNICODE.
	 *
	 * We only set the NTLMSSP_NEGOTIATE_SIGN if the server requires signing.
	 *
	 * Windows set NTLMSSP_NEGOTIATE_VERSION, we currently don't. It doesn't seem
	 * to be required.
	 *
	 * We need to set the following if we are going to add signing key exchange:
	 *		NTLMSSP_NEGOTIATE_LM_KEY
	 *		NTLMSSP_NEGOTIATE_NTLM2
	 */
	
	ntlmflags = SMB_AUTH_NTLM_FLAGS;
#ifdef SMB_DEBUG
	ntlmflags |= NTLMSSP_NEGOTIATE_VERSION;
#endif // SMB_DEBUG
	/* 
	 * We are doing an anonymous connection set the flag telling the server 
	 * that we are connecting as anonymous. Looking at XP they only set this
	 * in the ntlmssp auth response, they never set it in the ntlmssp 
	 * negotiate message.
	 *
	 * NOTE: NTLMSSP_RESERVED_7 was NTLMSSP_Negotiate_Anonymous sent question on
	 * the forum to explain.
	 */
	if (vcp->vc_flags & SMBV_ANONYMOUS_ACCESS)
		ntlmflags |= NTLMSSP_RESERVED_7;	/* Was NTLMSSP_Negotiate_Anonymous? */

	
	if (vcp->vc_sopt.sv_caps & SMB_CAP_UNICODE)
		ntlmflags |= NTLMSSP_NEGOTIATE_UNICODE;
	else
		ntlmflags |= NTLM_NEGOTIATE_OEM;

	if (vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) /* Doing Signing */
		ntlmflags |= NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
	
	*ntlmssp++ = letohl(ntlmflags);
#ifdef SMB_DEBUG
	/* We only send the version field if we are building in debug mode */
	{
		uint8_t *version = (uint8_t *)ntlmssp;
		
		*version++ = WINDOWS_MAJOR_VERSION_6;
		*version++ = WINDOWS_MINOR_VERSION_0;
		*((uint16_t *)version) = WINDOWS_BUILD_VERSION;
		version += 5; /* Skip over the build version and reserved fields */
		*version++ = NTLMSSP_REVISION_W2K3;
	}
#endif // SMB_DEBUG
	
done:
	if (target_info)
		free(target_info, M_SMBTEMP);		
	if (lmv2)
		free(lmv2, M_TEMP);
	if (ntlmv2)
		free(ntlmv2, M_TEMP);
	if (username)
		free(username, M_TEMP);
	if (hostname)
		free(hostname, M_TEMP);
	if (domain_str)
		free(domain_str, M_TEMP);
	if (error) {
		if (gp->gss_token)
			free(gp->gss_token, M_TEMP);
		gp->gss_token = NULL;
		gp->gss_tokenlen = 0;		
	}
	return error;
}

/*
 * This routine will create a NTLMSSP type one message wiht the SPNEGO
 * wrapper.
 */
static int create_ntlmssp_negotiate_message(struct smb_vc *vcp)
{
	uint8_t *token, *pptr;
	uint32_t *ntlmssp;
	uint32_t ntlmssp_len;
	uint32_t LengthField[4];
	uint32_t token_len;
	uint32_t LengthFieldBytes;
	uint32_t ntlmflags = 0;
	struct smb_gss *gp = &vcp->vc_gss;

	/* Determine the length of our Type One NTLMSSP message */
	ntlmssp_len = (uint32_t)(sizeof(NTLMSSP_Signature) + sizeof(uint32_t) + 
							 sizeof(ntlmflags) + sizeof(struct SSPSecurityBuffer) + 
							 sizeof(struct SSPSecurityBuffer));
#ifdef SMB_DEBUG
	/* We only send the version field if we are building in debug mode */
	ntlmssp_len += NTLMSSP_NEGOTIATE_VERSION_LEN;
#endif // SMB_DEBUG
	/* 
	 * Wrapper layout for the Type One NTLMSSP message
	 *
	 *  From XP trace = 0x60 0x40
	 *		60		ASN.1 [Application 0] per RFC 2743 ( 1 byte)
	 *				ASN1_APPLICATION_TAG(0)
	 *		40		length field (der encoded) - size of data past this point
	 *	
	 *  From XP trace = 0x06 0x06 0x2b 0x06 0x01 0x05 0x05 0x02
	 *		06		OID tag primitive length (1 byte)
	 *		06		length field (der encoded) always SPNEGO_OID_1_3_6_1_5_5_2_LEN (1 byte)
	 *		SPNEGO_OID_1_3_6_1_5_5_2
	 *
	 *  From XP trace = 0xa0 0x36 0x30 0x34
	 *		a0		context tag for choice 0 NegTokenInit
	 *		36		length field (der encoded) - size of data past this point
	 *		30		Sequence
	 *		34		length field (der encoded) - size of data past this point
	 *
	 *  From XP trace = 0xa0 0x0e 0x30 0x0c
	 *		a0		context tag for choice 0 NegTokenInit
	 *		0e		length field (der encoded) ( 1 byte) should always be 14
	 *				Two bytes below + Size of the wrapped SPNEGO_mechType_NTLMSSP below
	 *		30		Sequence
	 *		0c		length field (der encoded) ( 1 byte) should always be 12
	 *				Size of the wrapped SPNEGO_mechType_NTLMSSP below
	 *
	 *  From XP trace = 0x06 0xa0 0x2b 0x06 0x01 0x04 0x01 0x82 0x37 0x02 0x02 0x0a
	 *		06		OID tag primitive length (1 byte)
	 *		0a		length field (der encoded) always SPNEGO_mechType_NTLMSSP_LEN (1 byte)
	 *		SPNEGO_mechType_NTLMSSP
	 *
	 *	From XP trace = 0xa2 0x22 0x04 0x20
	 *		a2		context tag for choice 2 NegTokenInit
	 *		22		length field (der encoded) size of data past this point (includes ntlmssp_len)
	 *		04		unknown currenlty
	 *		20		length field (der encoded) size of data past this point (just ntlmssp_len)
	 *
	 */
	
	/* Total size of wrapper except for the unknown length fields */
	token_len = ASN1_APPLICATION_TAG_LEN + 
				1 + 1 + SPNEGO_OID_1_3_6_1_5_5_2_LEN + 
				1 + 1 +
				1 + 1 + 1 + 1 + 
				1 + 1 + SPNEGO_mechType_NTLMSSP_LEN + 
				+ 1 + 1;
	
	/* Now lets figure out the unknown length fields sizes working backwards */
	LengthFieldBytes = der_length_size(ntlmssp_len);
	LengthField[0] = ntlmssp_len + LengthFieldBytes + 1;
	LengthFieldBytes += der_length_size(LengthField[0]);
	LengthField[1] = ntlmssp_len + LengthFieldBytes + 2 + 2 + SPNEGO_mechType_NTLMSSP_LEN + 4;
	LengthFieldBytes += der_length_size(LengthField[1]);
	LengthField[2] = ntlmssp_len + LengthFieldBytes + 2 + 2 + SPNEGO_mechType_NTLMSSP_LEN + 4 + 1;
	LengthFieldBytes += der_length_size(LengthField[2]);
	LengthField[3] = ntlmssp_len + LengthFieldBytes + token_len - 1;
	LengthFieldBytes += der_length_size(ntlmssp_len + LengthFieldBytes + token_len - 1);
	/* Now total it all up */
	token_len += LengthFieldBytes + ntlmssp_len;
	
	/* Lets create the buffer big enough to hold the NTLMSSP message, plus the SPNEGO wrapper */
	token = malloc(token_len, M_SMBTEMP, M_WAITOK);
	if (token == NULL) {
		SMBERROR("Can't allocate token pointer\n");
		return ENOMEM;
	}
	memset(token, 0, token_len);
	pptr = token;
	
	*pptr++ = ASN1_APPLICATION_TAG(0);
	der_length_put(&pptr, LengthField[3]);
	
	*pptr++ = ASN1_OID_TAG;
	*pptr++ = SPNEGO_OID_1_3_6_1_5_5_2_LEN;
	bcopy(SPNEGO_OID_1_3_6_1_5_5_2, pptr, SPNEGO_OID_1_3_6_1_5_5_2_LEN);
	pptr += SPNEGO_OID_1_3_6_1_5_5_2_LEN;
	
	*pptr++ = ASN1_CONTEXT_TAG(0);
	der_length_put(&pptr, LengthField[2]);
	*pptr++ = ASN1_SEQUENCE_TAG(0);
	der_length_put(&pptr, LengthField[1]);
	
	*pptr++ = ASN1_CONTEXT_TAG(0);
	der_length_put(&pptr, SPNEGO_mechType_NTLMSSP_LEN + 2 + 2);
	*pptr++ = ASN1_SEQUENCE_TAG(0);
	der_length_put(&pptr, SPNEGO_mechType_NTLMSSP_LEN + 2);

	*pptr++ = ASN1_OID_TAG;
	*pptr++ = SPNEGO_mechType_NTLMSSP_LEN;
	bcopy(SPNEGO_mechType_NTLMSSP, pptr, SPNEGO_mechType_NTLMSSP_LEN);
	pptr += SPNEGO_mechType_NTLMSSP_LEN;

	
	*pptr++ = ASN1_CONTEXT_TAG(2);
	der_length_put(&pptr, LengthField[0]);
	*pptr++ = 0x04;
	der_length_put(&pptr, ntlmssp_len);	
		
	/* Currently we do not support the OS Version Structure (Optional) 8 bytes. */
	/*
	 *	The Type 1 Message
	 *
	 *		Description					Content
	 *		0	NTLMSSP Signature		Null-terminated ASCII "NTLMSSP" (0x4e544c4d53535000)
	 *		8	NTLM Message Type		long (0x01000000)
	 *		12	Flags					long
	 *		(16) Supplied Domain		(Optional)	security buffer
	 *		(24) Supplied Workstation	(Optional)	security buffer
	 *		(32) OS Version Structure	(Optional)	8 bytes
	 *									Should we support the OS Version Structure?
	 *		(32) (40)					start of data block (if required) 
	 */
	
	strlcpy((char *)pptr, NTLMSSP_Signature, ntlmssp_len);
	pptr += sizeof(NTLMSSP_Signature);
	ntlmssp = (uint32_t *)pptr;
	*ntlmssp++ = letohl(NTLMSSP_TypeOneMessage);
	
	/*
	 * Type One Message Flags
	 *
	 * The XP_NEGOTIATE_NTLM_FLAGS is a set of flags sent by XP going to a stand
	 * alone server and a domain server. 
	 * 
	 * The SMB_NEGOTIATE_NTLM_FLAGS is a subset of the Windows XP flags.
	 *
	 * So XP sets both the  NTLMSSP_NEGOTIATE_UNICODE and NTLM_NEGOTIATE_OEM
	 * flags. We only set the NTLMSSP_NEGOTIATE_UNICODE if the server supports 
	 * UNICODE. Also we only set the NTLM_NEGOTIATE_OEM if the server doesn't 
	 * support UNICODE.
	 *
	 * We only set the NTLMSSP_NEGOTIATE_SIGN if the server requires signing.
	 *
	 * If we some day decide to support key exchange then we will need to add
	 * these two items to the ntlmflags (See docs for more information):
	 *
	 *		NTLMSSP_NEGOTIATE_LM_KEY
	 *		NTLMSSP_NEGOTIATE_KEY_EXCH
	 */
	ntlmflags = SMB_NEGOTIATE_NTLM_FLAGS;
#ifdef SMB_DEBUG
	ntlmflags |= NTLMSSP_NEGOTIATE_VERSION;
#endif // SMB_DEBUG
	
	if (vcp->vc_sopt.sv_caps & SMB_CAP_UNICODE)
		ntlmflags |= NTLMSSP_NEGOTIATE_UNICODE;
	else
		ntlmflags |= NTLM_NEGOTIATE_OEM;
	
	if (vcp->vc_hflags2 & SMB_FLAGS2_SECURITY_SIGNATURE) /* Doing Signing */
		ntlmflags |= NTLMSSP_NEGOTIATE_SIGN | NTLMSSP_NEGOTIATE_ALWAYS_SIGN;
		
	*ntlmssp++ = letohl(ntlmflags);
	/*
	 * Windows XP seems to leave the Domain and Workstation fields set to
	 * NULL, so lets do the same. Note: The supplied domain is a security buffer 
	 * containing the domain in which the client workstation has membership. 
	 * This is always in OEM format, even if Unicode is supported by the client.
	 * The supplied workstation is a security buffer containing the client 
	 * workstation's name. This, too, is in OEM rather than Unicode.
	 */
#ifdef SMB_DEBUG
	/* We only send the version field if we are building in debug mode */
	{
		uint8_t *version = (uint8_t *)ntlmssp;
		
		/* Move pass the Domain and Workstation fields */
		version += (int)(sizeof(struct SSPSecurityBuffer) + sizeof(struct SSPSecurityBuffer));
		*version++ = WINDOWS_MAJOR_VERSION_6;
		*version++ = WINDOWS_MINOR_VERSION_0;
		*((uint16_t *)version) = WINDOWS_BUILD_VERSION;
		version += 5; /* Skip over the build version and reserved fields */
		*version++ = NTLMSSP_REVISION_W2K3;
	}
#endif // SMB_DEBUG
	gp->gss_token = token;
	gp->gss_tokenlen = token_len;
	gp->gss_major = GSS_C_CONTINUE_NEEDED;	/* We need more processing */

	return 0;
}
	
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
static int
smb_asn1_blob2principal(struct smb_vc *vcp, const uint8_t *blobp, u_int16_t blob_len,
			char **princ, size_t *princlen)
{
	size_t ellen;		/* Current element length */
	size_t mechs_ellen;		/* mechTypes element length */
	size_t mechlen;		/* mechType length */
	uint8_t *endOfMechTypes = NULL;
	uint8_t *blobp_end = (uint8_t *)blobp + blob_len;
	
	*princ = NULL;
	*princlen = 0;

	/* Check for application tag 0 per RFC 2743 */
	if ((blobp >= blobp_end) || (*blobp++ != ASN1_APPLICATION_TAG(0)))
		return (0);
	
	if (der_length_get(&blobp, blobp_end, &ellen))
		return (0);
	
	/* Check for the SPNEGO mech id */
	if ((blobp+SPNEGO_INIT_TOKEN_LEN) > blobp_end)
		return (0);
	if (bcmp(blobp, SPNEGO_INIT_TOKEN, SPNEGO_INIT_TOKEN_LEN) != 0)
		return (0);

	blobp += SPNEGO_INIT_TOKEN_LEN;
	
	/* Get the length of the NegTokenInit */
	if (der_length_get(&blobp, blobp_end, &ellen))
		return (0);
	
	/* Now check for Sequence */
	if ((blobp >= blobp_end) || (*blobp++ != ASN1_SEQUENCE_TAG(0)))
		return (0);

	/* Get the length of the sequence */
	if (der_length_get(&blobp, blobp_end, &ellen))
		return (0);

	/* Now check for the required mechTypes tag 0 */
	if ((blobp >= blobp_end) || (*blobp++ != ASN1_CONTEXT_TAG(0)))
		return  (0);

	/* Get the element length of the mechTypes */
	if (der_length_get(&blobp, blobp_end, &ellen))
		return (0);
		
	/* Now check for Sequence */
	if ((blobp >= blobp_end) || (*blobp++ != ASN1_SEQUENCE_TAG(0)))
		return (0);
	
	/* Get the length of the mechTypes */
	if (der_length_get(&blobp, blobp_end, &mechs_ellen))
		return (0);

#ifdef SMB_DEBUG
	if (mechs_ellen != (ellen - 2))
		SMBERROR("mechs_ellen = %x ellen = %x\n", (unsigned int)mechs_ellen, (unsigned int)ellen);
#endif // SMB_DEBUG
		
	/* Now advance our end blob pointer to the Microsoft funny mechListMic. */
	endOfMechTypes = (uint8_t *)blobp + mechs_ellen;
	if (endOfMechTypes > blobp_end)
		return (0);	
	
	/* 
	 * Search the mechType list
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
		
		if (*blobp++ != ASN1_OID_TAG) {
			SMBERROR("Bad Identifier octet (6=OID)\n");
			blobp = endOfMechTypes;
			break;
		}
		if ((der_length_get(&blobp, endOfMechTypes, &mechlen)) || (mechlen == 0)) {
			SMBERROR("Bad mechlen length!\n");
			blobp = endOfMechTypes;
			break;
		}
		if ((mechlen+blobp) > endOfMechTypes) {
			SMBERROR("Bad mechlen length, too long! mechlen = %ld\n", mechlen);
			blobp = endOfMechTypes;
			break;
		}
#ifdef DEBUG_MECH_TYPES
		smb_hexdump(__FUNCTION__, "mechType: ", (u_char *)blobp, mechlen);
#endif // DEBUG_MECH_TYPES
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
	if ((blobp < blobp_end) && (*blobp == ASN1_CONTEXT_TAG(2))) {
		blobp += 1;
		if (der_length_get(&blobp, blobp_end, &ellen))
			return (0);
		blobp += ellen;
	}
	
	/* Check that we are in fact at the mechListMic, tag 3 */
	if ((blobp >= blobp_end) || (*blobp++ != ASN1_CONTEXT_TAG(3)))
		return (0);
	
	if (der_length_get(&blobp, blobp_end, &ellen))
		return (0);

	/*
	 * Just in case Microsoft ever decides to do things right,
	 * at least sort of, lets check if we've got a string type,
	 * which should be octet string, but will take anything.
	 */
	if ((blobp <= blobp_end) && (ASN1_STRING_TYPE(*blobp)))
		goto have_string;

	/* Check that we have a sequence */
	if ((blobp >= blobp_end) || (*blobp++ != ASN1_SEQUENCE_TAG(0)))
		return (0);

	if (der_length_get(&blobp, blobp_end, &ellen))
		return (0);

	/* check for context class constructed tag 0 */
	if ((blobp > blobp_end) || (*blobp++ != ASN1_CONTEXT_TAG(0)))
		return (0);

	if (der_length_get(&blobp, blobp_end, &ellen))
		return (0);

	/*
	 * Check for String type. Should be General, but will
	 * accept any
	 */
	if ((blobp > blobp_end) || (!ASN1_STRING_TYPE(*blobp)))
		return (0);
have_string:
	
	blobp += 1;

	/* We now have our string length and blobp is our principal */
	if (der_length_get(&blobp, blobp_end, &ellen))
		return (0);

	if ((blobp + ellen) > blobp_end)
		return (0);

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
smb_reply2principal(struct smb_vc *vcp, const uint8_t *rp, u_int16_t rplen, char **principal, uint32_t  *princlen)
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
	
	if (!smb_asn1_blob2principal(vcp, rp, rplen, &spn, &spnlen)) 
		return (0);

	/* smb_asn1_blob2principal validates spnlen for us */
	*princlen = (uint32_t)spnlen;
	endp = spn + spnlen;
	service = spn;
	/* Search for end of service part */
	for (host = spn; (host < endp) && (*host != '/'); host++)
		;
	if (host == endp) {
		/* 
		 * No service part. So we will add "cifs" as the service
		 * and the host started with the returned service principal name.
		 */
		host = spn;
		/* We need to add "cifs/ instance part" */
		service = (char *)"cifs";
		s_end = service + 4;
		*princlen +=5;   /* Increase the length to include the `/' */
	} else {
		s_end = host++;
	}

	/* Search for the realm part */
	for (realm = host; (realm < endp) && (*realm != '@'); realm++)
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
	DBG_ASSERT(strnlen(*principal, SMB_MAX_KERB_PN+1)+1 == *princlen);
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
smb_gss_negotiate(struct smb_vc *vcp, vfs_context_t context, uint8_t *token, u_int16_t toklen)
{
	struct smb_gss *gp = &vcp->vc_gss;
	kern_return_t kr;

	if (IPC_PORT_VALID(gp->gss_mp))
		return 0;
	
	DBG_ASSERT(context);
	/* Should never happen, but just in case */
	if (context == NULL)
		return EPIPE;

	if (gp->gss_spn) 
		free(gp->gss_spn, M_TEMP);
	gp->gss_spn = NULL;
	
	/*
	 * If they don't do Kerberos then they must support NTLMSSP. Some servers
	 * will tell you what mech types they support, but it looks like NTLMSSP
	 * is implied if the server supports the extended security model.
	 */
	if (toklen > SMB_GUIDLEN) {
		token += SMB_GUIDLEN;
		toklen -= SMB_GUIDLEN;
		if (!smb_reply2principal(vcp, token, toklen, &gp->gss_spn, &gp->gss_spnlen))
			vcp->vc_flags |= SMBV_MECHTYPE_NTLMSSP;
	} else
		vcp->vc_flags |= SMBV_MECHTYPE_NTLMSSP;

	SMBDEBUG("vcp->vc_flags = 0x%x gp->gss_spn = %s\n", vcp->vc_flags, (gp->gss_spn) ? gp->gss_spn : "NULL");
	
	kr = vfs_context_get_special_port(context, TASK_GSSD_PORT, &gp->gss_mp);
	if (kr != KERN_SUCCESS || !IPC_PORT_VALID(gp->gss_mp)) {
		SMBERROR("Can't get gssd port, status %d\n", kr);
		if (gp->gss_spn)
			free(gp->gss_spn, M_TEMP);
		gp->gss_spn = NULL;
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
	
	if (gp->gss_token)
		free(gp->gss_token, M_TEMP);
	gp->gss_token = NULL;
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
	if (gp->gss_cpn)	/* If we have a client's principal name free it */
		free(gp->gss_cpn, M_TEMP);
	if (gp->gss_spn)
		free(gp->gss_spn, M_TEMP);
	if (gp->gss_token)
		free(gp->gss_token, M_TEMP);
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
 * Someday this should just be a wrapper to make mach up call, using the 
 * parameters in the smb_gss structure and the supplied uid. Currently
 * this code is all in the kernel, this is our gssd.
 */
static kern_return_t smb_gss_init_ntlmssp(struct smb_vc *vcp)
{
	int error;
	struct smb_gss *gp = &vcp->vc_gss;

	if (gp->gss_ctx == NTLMSSP_NEGOTIATE) {
		error = create_ntlmssp_negotiate_message(vcp);
		gp->gss_ctx = NTLMSSP_CHALLENGE;
	} else if (gp->gss_ctx == NTLMSSP_CHALLENGE) {
		uint32_t ntlmflags = 0;
		void *target_info = NULL;
		uint16_t target_len = 0;
		
		if (gp->gss_token)
			error = parse_ntlmssp_negotiate_challenge(vcp, &ntlmflags, &target_info, &target_len);
		else {
			error = EBADRPC;
			SMBERROR("No token returned from negotiate challenge message\n");
		}
		if (error == 0)
			error = create_ntlmssp_negotiate_auth(vcp, ntlmflags, target_info, target_len);
		gp->gss_ctx = NTLMSSP_AUTH;
	} else {
		DBG_ASSERT(gp->gss_ctx == NTLMSSP_AUTH);
		/* 
		 * If the server return a security blob process it otherwise
		 * we assume that no blob means everything is done. A standalone
		 * 2003 system will not return a blob, but 2008 does, go figure.
		 */
		if (gp->gss_tokenlen && gp->gss_token) {
			smb_hexdump(__FUNCTION__, "Parsing negotiate_auth_response ", gp->gss_token, gp->gss_tokenlen);
			error = parse_ntlmssp_negotiate_auth_response(vcp);
		} else {
			gp->gss_major = GSS_C_COMPLETE;
			gp->gss_tokenlen = 0;	/* Reset will remove the token */
			/* NetApp return this error when it doesn't like your message */
			if (gp->gss_smb_error == EAGAIN) {
				SMBERROR("NTLMSSP Complete, but we got an EAGAIN error?\n");
				error = EAUTH;
			} else 
				error = 0;		
		}
		gp->gss_ctx = NTLMSSP_DONE;
	}
	return error;
}

/*
 * Wrapper to make mach up call, using the parameters in the smb_gss structure
 * and the supplied uid. (Should be the vc_uid field of the enclosing vc).
 */
static kern_return_t
smb_gss_init(struct smb_vc *vcp, uid_t uid)
{
	struct smb_gss *cp = &vcp->vc_gss;
	int win2k = (vcp->vc_flags & SMBV_WIN2K_XP) ? GSSD_WIN2K_HACK : 0;
	kern_return_t kr;
	byte_buffer okey = NULL;
	int retry_cnt = 0;
	vm_map_copy_t itoken = NULL;
	byte_buffer otoken = NULL;
	mach_msg_type_number_t otokenlen;
	int error = 0;
	const char *gss_cpn = (cp->gss_cpn) ? cp->gss_cpn : "";
	
	if (!IPC_PORT_VALID(cp->gss_mp)) {
		SMBWARNING("smb_gss_init: gssd port not valid\n");
		goto out;
	}

	if (cp->gss_tokenlen > 0)
		gss_mach_alloc_buffer(cp->gss_token, cp->gss_tokenlen, &itoken);
	
retry:
	kr = mach_gss_init_sec_context(
		cp->gss_mp,
		SPNEGO_MECH,
		(byte_buffer) itoken, (mach_msg_type_number_t) cp->gss_tokenlen,
		uid,
		(char *)gss_cpn,
		cp->gss_spn,
		GSSD_MUTUAL_FLAG | GSSD_C_DELEG_POLICY_FLAG,
		win2k,
		&cp->gss_ctx,
		&cp->gss_cred,
		&cp->gss_rflags,		       
		&okey,  (mach_msg_type_number_t *) &vcp->vc_mackeylen,
		&otoken, (mach_msg_type_number_t *) &otokenlen,
		&cp->gss_major,
		&cp->gss_minor);

	if (kr != 0) {
		SMBERROR("smb_gss_init: mach_gss_init_sec_context failed: %x %d\n", kr, kr);
		if (kr == MIG_SERVER_DIED && cp->gss_cred == 0 &&
			retry_cnt++ < GSS_MACH_MAX_RETRIES) {
			if (cp->gss_tokenlen > 0)
				gss_mach_alloc_buffer(cp->gss_token, cp->gss_tokenlen, &itoken);
			goto retry;	
		}
		goto out;
	}

	if (vcp->vc_mackeylen > 0) {
#ifdef SMB_DEBUG
		/*
		 * XXX NFS code we check that the returned key is SKEYLEN, a simple
		 * des key. I don't think we should restrict our selves for SMB
		 * NFS has to use specific des based routines to be compliant with
		 * RFCs.
		 */
		if (vcp->vc_mackeylen != SKEYLEN)
			SMBERROR("smb_gss_init: non-nfs key length (%d)\n", vcp->vc_mackeylen);
		else 
			SMBERROR("smb_gss_init: key length (%d)\n", vcp->vc_mackeylen);			
#endif // SMB_DEBUG
		//Free any old key
		if (vcp->vc_mackey)
			FREE(vcp->vc_mackey, M_TEMP);
		MALLOC(vcp->vc_mackey, uint8_t *, vcp->vc_mackeylen, M_TEMP, M_WAITOK);
		error = gss_mach_vmcopyout((vm_map_copy_t) okey, vcp->vc_mackeylen, vcp->vc_mackey);
		if (error) {
			gss_mach_vm_map_copy_discard((vm_map_copy_t)otoken, otokenlen);
			goto out;
		}
	}

	/* Free context token used as input */
	if (cp->gss_token)
		FREE(cp->gss_token, M_TEMP);
	cp->gss_token = NULL;
	cp->gss_tokenlen = 0;
	
	if (otokenlen > 0) {
		MALLOC(cp->gss_token, uint8_t *, otokenlen, M_TEMP, M_WAITOK);
		error = gss_mach_vmcopyout((vm_map_copy_t) otoken, otokenlen, cp->gss_token);
		if (error)
			goto out;
		cp->gss_tokenlen = otokenlen;
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
	if (cp->gss_token)
		FREE(cp->gss_token, M_TEMP);
	cp->gss_token = NULL;
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
smb_gss_ssandx(struct smb_vc *vcp, vfs_context_t context, uint32_t caps, u_int16_t *action)
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
	maxtokenlen = vcp->vc_sopt.sv_maxtx - (SMB_HDRLEN + SMB_SETUPXRLEN);
#endif // SMB_DEBUG

	tokenptr = gp->gss_token;	/* Get the start of the Kerberos blob */
	do {
		if (rqp)	/* If we are looping then release it, before getting it again */
			smb_rq_done(rqp);

		/* Allocate the request form a session setup and x */
		error = smb_rq_alloc(VCTOCP(vcp), SMB_COM_SESSION_SETUP_ANDX, context, &rqp);
		if (error)
			break;
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
		smb_put_dstring(mbp, vcp, SMBFS_NATIVEOS, sizeof(SMBFS_NATIVEOS), NO_SFM_CONVERSIONS);	/* Native OS */
		smb_put_dstring(mbp, vcp, SMBFS_LANMAN, sizeof(SMBFS_LANMAN), NO_SFM_CONVERSIONS);	/* LAN Mgr */
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
	free(gp->gss_token, M_TEMP);
	gp->gss_token = NULL;
	gp->gss_tokenlen = 0;
	gp->gss_smb_error = error;	/* Hold on to the last smb error returned */
	/* At this point error should have the correct error, we only support NTStatus code with extended security */
	if (error && (error != EAGAIN)) {
		SMBWARNING("Extended security authorization failed! %d\n", error);
		goto bad;
	}
	/* Not really need, but just to be safe, EAGAIN is not a real error */
	if (error == EAGAIN) {
		error = 0;
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
	gp->gss_token = malloc(gp->gss_tokenlen, M_SMBTEMP, M_WAITOK);
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
	if ((vcp->vc_hflags2 & SMB_FLAGS2_UNICODE) && (bc > 0) && (!(toklen & 1))) {
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
	u_int16_t action = 0;
	
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
		if (vcp->vc_flags & SMBV_KERBEROS_ACCESS)
			error = smb_gss_init(vcp, vcp->vc_uid);
		else
			error = smb_gss_init_ntlmssp(vcp);
		/* No token to send or error so just break out */
		if (error || (SMB_GSS_ERROR(&vcp->vc_gss))) {
			SMBWARNING("%s extended security error = %d SMB_GSS_ERROR = %d\n", 
					   (vcp->vc_flags & SMBV_KERBEROS_ACCESS) ? "Kerberos" : "NTLMSSP", 
					   error, SMB_GSS_ERROR(&vcp->vc_gss));
			/* Always return EAUTH unless we want the reconnect code to try again */
			if (error != EAGAIN)
				error = EAUTH;
			break;
		}
		if ((vcp->vc_gss.gss_tokenlen) && ((error = smb_gss_ssandx(vcp, context, caps, &action))))
			break;
	} while (SMB_GSS_CONTINUE_NEEDED(&vcp->vc_gss));

	if ((error == 0) && ((vcp->vc_flags & SMBV_GUEST_ACCESS) != SMBV_GUEST_ACCESS) && (action & SMB_ACT_GUEST)) {
		/* 
		 * We wanted to only login the users as guest if they ask to be login as guest. Window system will
		 * login any bad user name as guest if guest is turn on. The problem here is with XPHome. XPHome
		 * doesn't care if the user is real or made up, it always logs them is as guest. We now have a 
		 * way to tell if Simple File Sharing (XPHome) is being used. So in the extended security case
		 * we will log the user out and return EAUTH.
		 */
		if (vcp->vc_pass && vcp->vc_pass[0]) {
			SMBWARNING("Got guest access, but wanted real access, logging off.\n");
			(void)smb_smb_ssnclose(vcp, context);
			error = EAUTH;
			
		} else {
			/* Vista will log you in as guest if your account has no password. */
			SMBWARNING("Wanted real access, but got guest access because we had no password.\n");			
		}
	}

	if (error)	/* Reset the signature info */
		smb_reset_sig(vcp);

	smb_gss_reset(&vcp->vc_gss);
	return error;
}

