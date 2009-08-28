/*
 * Copyright (c) 2006 - 2008 Apple Inc. All rights reserved.
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
#ifndef SMB_GSS_H
#define SMB_GSS_H

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

#define SPNEGO_OID_1_3_6_1_5_5_2 "\x2b\x06\x01\x05\x05\x02"
#define SPNEGO_OID_1_3_6_1_5_5_2_LEN 6


#define ASN1_APPLICATION_TAG_LEN 1
#define ASN1_APPLICATION_TAG(value) (0x60+(value))
#define ASN1_OID_TAG 0x06
#define ASN1_CONTEXT_TAG(value) (0xA0+value)
#define ASN1_ENUMERATED_TAG 0x0A
#define ASN1_SEQUENCE_TAG(value) (0x30+value)


#define ASN1_OCTET_STRING 0x4
#define ASN1_BOOLEAN 0x1
#define ASN1_INTEGER 0x2
#define ASN1_BITFIELD 0x3
#define ASN1_SET 0x31

#define ASN1_MAX_OIDS 20

/*
 * NTLMSSP Negotiate Flags Information:
 *
 * NTLMSSP_NEGOTIATE_56
	If set, requests 56-bit encryption. If the client sends NTLMSSP_NEGOTIATE_56 to the server 
	in the NEGOTIATE_MESSAGE, the server MUST return NTLMSSP_NEGOTIATE_56 to the client 
	in the CHALLENGE_MESSAGE only if the client sets NTLMSSP_NEGOTIATE_SEAL or 
	NTLMSSP_NEGOTIATE_SIGN. Otherwise it is ignored. If both NTLMSSP_NEGOTIATE_56 and 
	NTLMSSP_NEGOTIATE_128 are requested and supported by the client and server, 
	NTLMSSP_NEGOTIATE_56 and NTLMSSP_NEGOTIATE_128 will both be returned to the client. 
	It is recommended that clients and servers that set NTLMSSP_NEGOTIATE_SEAL always set 
	NTLMSSP_NEGOTIATE_56 if it is supported. 
 * NTLMSSP_NEGOTIATE_KEY_EXCH
	If set, requests an explicit key exchange. If the client sends 
	NTLMSSP_NEGOTIATE_KEY_EXCH to the server in the NEGOTIATE_MESSAGE, the server 
	MUST return NTLMSSP_NEGOTIATE_KEY_EXCH to the client in the CHALLENGE_MESSAGE and 
	use key exchange only if the client sets NTLMSSP_NEGOTIATE_SIGN or 
	NTLMSSP_NEGOTIATE_SEAL. Otherwise it is ignored. Use of this capability is recommended 
	because message integrity or confidentiality can be provided only when this flag is negotiated 
	and a key exchange key is created. See sections 3.1.5.1 and 3.1.5.2 for details.
 * NTLMSSP_NEGOTIATE_128
	If set, requests 128-bit session key negotiation. If the client sends 
	NTLMSSP_NEGOTIATE_128 to the server in the NEGOTIATE_MESSAGE, the server MUST 
	return NTLMSSP_NEGOTIATE_128 to the client in the CHALLENGE_MESSAGE only if the client 
	sets NTLMSSP_NEGOTIATE_SEAL or NTLMSSP_NEGOTIATE_SIGN. Otherwise it is ignored. If 
	both NTLMSSP_NEGOTIATE_56 and NTLMSSP_NEGOTIATE_128 are requested and supported 
	by the client and server, NTLMSSP_NEGOTIATE_56 and NTLMSSP_NEGOTIATE_128 will both 
	be returned to the client. It is recommended that clients and servers that set 
	NTLMSSP_NEGOTIATE_SEAL always set NTLMSSP_NEGOTIATE_128 if it is supported.
 * NTLMSSP_RESERVED_1
	This bit is unused and MUST be zero.
 * NTLMSSP_RESERVED_2
	This bit is unused and MUST be zero.
 * NTLMSSP_RESERVED_3
	This bit is unused and MUST be zero.
 * NTLMSSP_NEGOTIATE_VERSION
	If set, requests the protocol version number. The data corresponding to this 
	flag is provided in the Version field of the NEGOTIATE_MESSAGE, the CHALLENGE_MESSAGE, 
	and the AUTHENTICATE_MESSAGE.
 * NTLMSSP_RESERVED_4
	This bit is unused and MUST be zero.
 * NTLMSSP_NEGOTIATE_TARGET_INFO
	If set, requests extended information about the server authentication realm to be sent as 
	AV_PAIR in the TargetInfo payload, as specified in section 2.2.2.7. If the client sends 
	NTLMSSP_NEGOTIATE_TARGET_INFO to the server in the NEGOTIATE_MESSAGE, the server 
	MUST support the request and return NTLMSSP_NEGOTIATE_TARGET_INFO to the client in the 
	CHALLENGE_MESSAGE. In that case, the data corresponding to this flag is provided by the 
	server in the TargetInfo field of the CHALLENGE_MESSAGE.
 * NTLMSSP_REQUEST_NON_NT_SESSION_KEY
	If set, requests the usage of the LMOWF, as specified in 
	section 3.3(NTLM v1 and NTLM v2 Messages).
 * NTLMSSP_RESERVED_5
	This bit is unused and MUST be zero.
 * NTLMSSP_NEGOTIATE_IDENTIFY
	If set, requests an identify level token.
 * NTLMSSP_NEGOTIATE_NTLM2
	If set, requests usage of the NTLM v2 session security. NTLMSSP_NEGOTIATE_LM_KEY and 
	NTLMSSP_NEGOTIATE_NTLM2 are mutually exclusive. If both NTLMSSP_NEGOTIATE_NTLM2 
	and NTLMSSP_NEGOTIATE_LM_KEY are requested, NTLMSSP_NEGOTIATE_NTLM2 alone MUST 
	be returned to the client. NTLM v2 authentication session key generation must be supported 
	by both the client and the DC in order to be used, and NTLM v2 session security signing and 
	sealing requires support from the client and the server in order to be used. 
 * NTLMSSP_TARGET_TYPE_SHARE
	If set, TargetName MUST be a share name. The data corresponding to this flag is provided 
	by the server in the TargetName field of the CHALLENGE_MESSAGE. This flag MUST be 
	ignored in the NEGOTIATE_MESSAGE and the AUTHENTICATE_MESSAGE.
 * NTLMSSP_TARGET_TYPE_SERVER
	If set, TargetName MUST be a server name. The data corresponding to this flag is provided 
	by the server in the TargetName field of the CHALLENGE_MESSAGE. This flag MUST be 
	ignored in the NEGOTIATE_MESSAGE and the AUTHENTICATE_MESSAGE. 
 * NTLMSSP_TARGET_TYPE_DOMAIN
	 If set, TargetName MUST be a domain name. The data corresponding to this flag is 
	 provided by the server in the TargetName field of the CHALLENGE_MESSAGE. This flag MUST 
	 be ignored in the NEGOTIATE_MESSAGE and the AUTHENTICATE_MESSAGE.
 * NTLMSSP_NEGOTIATE_ALWAYS_SIGN
	If set, requests the presence of a signature block on all messages. 
	NTLMSSP_NEGOTIATE_NTLM and NTLMSSP_NEGOTIATE_ALWAYS_SIGN MUST be set in the 
	NEGOTIATE_MESSAGE to the server and the CHALLENGE_MESSAGE to the client. 
	NTLMSSP_NEGOTIATE_ALWAYS_SIGN is overridden by NTLMSSP_NEGOTIATE_SIGN and 
	NTLMSSP_NEGOTIATE_SEAL, if they are supported.  
 * NTLMSSP_RESERVED_6
	This bit is unused and MUST be zero.
 * NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED
	This flag indicates whether the Workstation field is present. If this flag is not set, the 
	Workstation field MUST be ignored. If this flag is set, the length field of the Workstation 
	field specifies whether the workstation name is non-empty or not.
 * NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED
	If set, the domain name is provided, as specified in section 2.2.1.1. 
 * NTLMSSP_RESERVED_7
	This bit is unused and MUST be zero. 
	NOTE: Was NTLMSSP_Negotiate_Anonymous and XP sent it and had the follow
	defination: Sent by the client in the Type 3 message to indicate that an anonymous 
	context has been established. This also affects the response fields 
	(as detailed in the "Anonymous Response" section). Very Strange?
 * NTLMSSP_NEGOTIATE_NT_ONLY
	If set, LM authentication is not allowed and only NT authentication is used.
 * NTLMSSP_NEGOTIATE_NTLM
	If set, requests usage of the NTLM v1 session security protocol. NTLMSSP_NEGOTIATE_NTLM 
	and NTLMSSP_NEGOTIATE_ALWAYS_SIGN MUST be set in the NEGOTIATE_MESSAGE to the 
	server and the CHALLENGE_MESSAGE to the client.
 * NTLMSSP_RESERVED_8
	This bit is unused and MUST be zero. 
 * NTLMSSP_NEGOTIATE_LM_KEY
	If set, requests LAN Manager (LM) session key computation. NTLMSSP_NEGOTIATE_LM_KEY 
	and NTLMSSP_NEGOTIATE_NTLM2 are mutually exclusive. If both 
	NTLMSSP_NEGOTIATE_LM_KEY and NTLMSSP_NEGOTIATE_NTLM2 are requested, 
	NTLMSSP_NEGOTIATE_NTLM2 alone MUST be returned to the client. NTLM v2 authentication 
	session key generation MUST be supported by both the client and the domain controller (DC) 
	in order to be used, and NTLM v2 session security signing and sealing requires support from 
	the client and the server to be used.
 * NTLMSSP_NEGOTIATE_DATAGRAM
	If set, requests datagram-oriented (connectionless) authentication. If 
	NTLMSSP_NEGOTIATE_DATAGRAM is set, then NTLMSSP_NEGOTIATE_KEY_EXCH MUST 
	always be set in the AUTHENTICATE_MESSAGE to the server and the CHALLENGE_MESSAGE 
	to the client.
 * NTLMSSP_NEGOTIATE_SEAL
	If set, requests session key negotiation for message confidentiality. If the client sends 
	NTLMSSP_NEGOTIATE_SEAL to the server in the NEGOTIATE_MESSAGE, the server MUST 
	return NTLMSSP_NEGOTIATE_SEAL to the client in the CHALLENGE_MESSAGE. Clients and 
	servers that set NTLMSSP_NEGOTIATE_SEAL SHOULD always set NTLMSSP_NEGOTIATE_56 
	and NTLMSSP_NEGOTIATE_128, if they are supported. 
 * NTLMSSP_NEGOTIATE_SIGN
	If set, requests session key negotiation for message signatures. If the client sends 
	NTLMSSP_NEGOTIATE_SIGN to the server in the NEGOTIATE_MESSAGE, the server MUST 
	return NTLMSSP_NEGOTIATE_SIGN to the client in the CHALLENGE_MESSAGE.
 * NTLMSSP_RESERVED_9
	This bit is unused and MUST be zero. 
 * NTLMSSP_REQUEST_TARGET
	If set, a TargetName field of the CHALLENGE_MESSAGE (section 2.2.1.2) MUST be 
	supplied.
 * NTLM_NEGOTIATE_OEM
	If A==1, the choice of character set encoding MUST be UNICODE. If A==0 and B==1, the 
	choice of character set MUST be OEM. If A==0 and B==0, the protocol MUST terminate.
 * NTLMSSP_NEGOTIATE_UNICODE
	If A==1, the choice of character set encoding MUST be UNICODE. If A==0 and B==1, the 
	choice of character set MUST be OEM. If A==0 and B==0, the protocol MUST terminate. 
 */

/* NTLMSSP Flags */
#define NTLMSSP_NEGOTIATE_UNICODE				0x00000001
#define NTLM_NEGOTIATE_OEM						0x00000002
#define NTLMSSP_REQUEST_TARGET					0x00000004
#define NTLMSSP_RESERVED_9						0x00000008
#define NTLMSSP_NEGOTIATE_SIGN					0x00000010
#define NTLMSSP_NEGOTIATE_SEAL					0x00000020
#define NTLMSSP_NEGOTIATE_DATAGRAM				0x00000040
#define NTLMSSP_NEGOTIATE_LM_KEY				0x00000080
#define NTLMSSP_RESERVED_8						0x00000100
#define NTLMSSP_NEGOTIATE_NTLM					0x00000200
#define NTLMSSP_NEGOTIATE_NT_ONLY				0x00000400
#define NTLMSSP_RESERVED_7						0x00000800
#define NTLMSSP_NEGOTIATE_OEM_DOMAIN_SUPPLIED	0x00001000
#define NTLMSSP_NEGOTIATE_OEM_WORKSTATION_SUPPLIED	0x00002000
#define NTLMSSP_RESERVED_6						0x00004000
#define NTLMSSP_NEGOTIATE_ALWAYS_SIGN			0x00008000
#define NTLMSSP_TARGET_TYPE_DOMAIN				0x00010000
#define NTLMSSP_TARGET_TYPE_SERVER				0x00020000
#define NTLMSSP_TARGET_TYPE_SHARE				0x00040000
#define NTLMSSP_NEGOTIATE_NTLM2					0x00080000
#define NTLMSSP_NEGOTIATE_IDENTIFY				0x00100000
#define NTLMSSP_RESERVED_5						0x00200000
#define NTLMSSP_REQUEST_NON_NT_SESSION_KEY		0x00400000
#define NTLMSSP_NEGOTIATE_TARGET_INFO			0x00800000
#define NTLMSSP_RESERVED_4						0x01000000
#define NTLMSSP_NEGOTIATE_VERSION				0x02000000
#define NTLMSSP_RESERVED_3						0x04000000
#define NTLMSSP_RESERVED_2						0x08000000
#define NTLMSSP_RESERVED_1						0x10000000
#define NTLMSSP_NEGOTIATE_128					0x20000000
#define NTLMSSP_NEGOTIATE_KEY_EXCH				0x40000000
#define NTLMSSP_NEGOTIATE_56					0x80000000

#define NTLMSSP_Signature "NTLMSSP"
#define NTLMSSP_TypeOneMessage					0x00000001
#define NTLMSSP_TypeTwoMessage					0x00000002
#define NTLMSSP_TypeThreeMessage				0x00000003

/*
 * NEGOTIATE	- Client Sends
 * CHALLENGE	- Server returns
 * AUTH			- Client Sends
 *
 * Several of these are optional and we need to put them in only if required. As an example
 * NTLMSSP_NEGOTIATE_SIGN, in the trace I have XP always sets this, but we only want to do 
 * that if sign is required. So for these predefines I have left that one off.
 */
#define XP_NEGOTIATE_NTLM_FLAGS (NTLMSSP_NEGOTIATE_UNICODE | NTLM_NEGOTIATE_OEM | NTLMSSP_REQUEST_TARGET | \
								NTLMSSP_NEGOTIATE_LM_KEY | \
								NTLMSSP_NEGOTIATE_NTLM |	\
								NTLMSSP_NEGOTIATE_ALWAYS_SIGN | \
								NTLMSSP_NEGOTIATE_NTLM2 | \
								NTLMSSP_NEGOTIATE_VERSION | \
								NTLMSSP_NEGOTIATE_KEY_EXCH | NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_56)

#define XP_CHALLENGE_NTLM_FLAGS (NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_REQUEST_TARGET | \
								NTLMSSP_NEGOTIATE_NTLM |	\
								NTLMSSP_NEGOTIATE_ALWAYS_SIGN | \
								NTLMSSP_TARGET_TYPE_SERVER | NTLMSSP_NEGOTIATE_NTLM2 | \
								NTLMSSP_NEGOTIATE_TARGET_INFO | \
								NTLMSSP_NEGOTIATE_VERSION | \
								NTLMSSP_NEGOTIATE_KEY_EXCH | NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_56)

#define XP_AUTH_NTLM_FLAGS		(NTLMSSP_NEGOTIATE_UNICODE | NTLMSSP_REQUEST_TARGET | \
								NTLMSSP_NEGOTIATE_NTLM |	\
								NTLMSSP_NEGOTIATE_NTLM2 | \
								NTLMSSP_NEGOTIATE_TARGET_INFO | \
								NTLMSSP_NEGOTIATE_VERSION | \
								NTLMSSP_NEGOTIATE_KEY_EXCH | NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_56)


#define SMB_NEGOTIATE_NTLM_FLAGS	(NTLMSSP_REQUEST_TARGET | \
									NTLMSSP_NEGOTIATE_NTLM |	\
									NTLMSSP_NEGOTIATE_NTLM2 | \
									NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_56)


#define SMB_AUTH_NTLM_FLAGS			(NTLMSSP_REQUEST_TARGET | \
									NTLMSSP_NEGOTIATE_NTLM |	\
									NTLMSSP_NEGOTIATE_NTLM2 | \
									NTLMSSP_NEGOTIATE_128 | NTLMSSP_NEGOTIATE_56)

struct SSPSecurityBuffer {
	uint16_t	length;
	uint16_t	allocated;
	uint32_t	offset;
};

/* Version info, only used for debugging, see above for more info */
#define NTLMSSP_NEGOTIATE_VERSION_LEN	8

#define WINDOWS_MAJOR_VERSION_5		0x05 /* The major version of the Windows operating system */ 
#define WINDOWS_MAJOR_VERSION_6		0x06 /* The major version of the Windows operating system */
#define WINDOWS_MINOR_VERSION_0		0x00 /* The minor version of the Windows operating system */ 
#define WINDOWS_MINOR_VERSION_1		0x01 /* The minor version of the Windows operating system */  
#define WINDOWS_MINOR_VERSION_2		0x02 /* The minor version of the Windows operating system */  
#define WINDOWS_BUILD_VERSION		0x0717	/* VISTA build number for now */
#define NTLMSSP_REVISION_W2K3		0x0F /* Version 15 of the NTLMSSP is in use. */
#define NTLMSSP_REVISION_W2K3_RC1	0x0A /* Version 10 of the NTLMSSP is in use. */

/*
 * Target Info Elements (AV_PAIR)
 *
 *  The AV_PAIR structure defines an attribute/value pair. Sequences of AV_PAIR structures are used in 
 *  the CHALLENGE_MESSAGE and AUTHENTICATE_MESSAGE structures.Note   Although the following 
 *  figure suggests that the most significant bit (MSB) of AvId is aligned with the MSB of a 32-bit word, 
 *  an AV_PAIR can be aligned on any byte boundary and can be 4+N bytes long for arbitrary N (N = 
 *  the contents of AvLen). 
 */

#define kMsvAvEOL				0  /* NONE; AvLen MUST be 0. */
#define kMsvAvNbComputerName	1 /* The server's NetBIOS computer name. The name MUST be in Unicode, and is not null-terminated. */
#define kMsvAvNbDomainName		2 /* The server's NetBIOS domain name. The name MUST be in Unicode, and is not null-terminated. */
#define kMsvAvDnsComputerName	3 /* The server's Active Directory (AD) DNS computer name. The name MUST be in Unicode, and is not null-terminated. */
#define kMsvAvDnsDomainName		4 /* 
								   * The server's Active Directory (AD) DNS domain name. The name MUST be in 
								   * Unicode, and is not null-terminated.
								   */
#define kMsvAvDnsTreeName		5 /* 
								   * The server's Active Directory (AD) DNS forest tree name. The name MUST be in 
								   * Unicode, and is not null-terminated. 
								   */ 
#define kMsvAvFlags				6 /* 
								   * An optional field containing a 32-bit value indicating server configuration. 
								   * The only defined value is 0x00000001, which indicates the server forces all 
								   * authentication to the guest account. This indicator MAY be used by a client 
								   * application to indicate forced authentication to the guest account. 
								   */
#define kMsvAvTimestamp			7 /* 
								   * A FILETIME structure (as specified in [MS-DTYP] section 2.3.1) in little-endian 
								   * byte order that contains the server local time. 
								   */
#define kMsAvRestrictions		8 /* 
								   * A Restrictions Encoding structure, as defined in section 2.2.2.2. The Restrictions 
								   * field contains a structure representing the integrity level of the security principal, 
								   * as well as a MachineID used to detect when the authentication attempt is looping back 
								   * to the same machine. 
								   */

/* State table used by NTLMSSP in the kernel stored in gss_ctx */
#define NTLMSSP_NEGOTIATE	0
#define NTLMSSP_CHALLENGE	1
#define NTLMSSP_AUTH		2
#define NTLMSSP_DONE		3

#ifndef GSS_C_COMPLETE
#define GSS_C_COMPLETE 0
#endif

#ifndef GSS_C_CONTINUE_NEEDED
#define GSS_C_CONTINUE_NEEDED 1
#endif

#define SMB_USE_GSS(vp) (IPC_PORT_VALID((vp)->vc_gss.gss_mp))
#define SMB_GSS_CONTINUE_NEEDED(p) ((p)->gss_major == GSS_C_CONTINUE_NEEDED)
#define SMB_GSS_ERROR(p) ((p)->gss_major != GSS_C_COMPLETE && \
	(p)->gss_major != GSS_C_CONTINUE_NEEDED)
int smb_gss_negotiate(struct smb_vc *, vfs_context_t , uint8_t *token, u_int16_t toklen);
int smb_gss_ssnsetup(struct smb_vc *, vfs_context_t );
void smb_gss_destroy(struct smb_gss *);

#ifdef SMB_DEBUG
//#define DEBUG_NTLM_BY_TURNING_OFF_NTLMV2 1
//#define DEBUG_TURN_OFF_EXT_SEC 1
#endif // SMB_DEBUG

#endif
