/*
 * Copyright (c) 2002-2013 Apple Inc. All rights reserved.
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

/*
 * eapfast_plugin.c
 * - EAP-FAST client
 */

/* 
 * Modification History
 *
 * February 14, 2006	Dieter Siegmund (dieter@apple)
 * - created (from peap_plugin.c)
 */
 
#include <EAP8021X/EAPClientPlugin.h>
#include <EAP8021X/EAPClientProperties.h>
#include <SystemConfiguration/SCValidation.h>
#include <mach/boolean.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "EAPLog.h"
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#include <Security/SecureTransport.h>
#include <Security/SecCertificate.h>
#include <sys/param.h>
#include <EAP8021X/EAPTLSUtil.h>
#include <EAP8021X/EAPCertificateUtil.h>
#include <EAP8021X/EAPKeychainUtil.h>
#include <EAP8021X/EAPSecurity.h>
#include <Security/SecureTransportPriv.h>
#include <Security/CipherSuite.h>
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPClientModule.h>
#include <EAP8021X/mschap.h>
#include <CoreFoundation/CFPreferences.h>
#include <TargetConditionals.h>
#include "myCFUtil.h"
#include "printdata.h"
#include "nbo.h"

#define STRLOG(__string, __level, __format, ...)			\
    { if (__string != NULL)						\
	    CFStringAppendFormat(__string, NULL, CFSTR(__format),	\
				 ## __VA_ARGS__);			\
	else EAPLog(__level, CFSTR(__format), ## __VA_ARGS__); }

#define EAP_FAST_NAME		"EAP-FAST"
#define EAP_FAST_NAME_LENGTH	(sizeof(EAP_FAST_NAME) - 1)

enum {
    kEAPFASTPacketFlagsVersionMask	= 0x7,
    kEAPFASTPacketFlagsFlagsMask	= 0xf8,
    kEAPFASTVersion1			= 1,
};

INLINE uint8_t
EAPFASTPacketFlagsFlags(uint8_t flags)
{
    return (flags & kEAPFASTPacketFlagsFlagsMask);
}

INLINE uint8_t
EAPFASTPacketFlagsVersion(uint8_t flags)
{
    return (flags & kEAPFASTPacketFlagsVersionMask);
}

INLINE void
EAPFASTPacketFlagsSetVersion(EAPTLSPacketRef eap_tls, uint8_t version)
{
    eap_tls->flags &= ~kEAPFASTPacketFlagsVersionMask;
    eap_tls->flags |= EAPFASTPacketFlagsVersion(version);
    return;
}

typedef struct AuthorityIDData_s {
    uint8_t		auid_type[2];
    uint8_t		auid_length[2];
    uint8_t		auid_id[1];
} AuthorityIDData, * AuthorityIDDataRef;

INLINE uint16_t
AuthorityIDDataGetType(const AuthorityIDDataRef auid)
{
    return (net_uint16_get(auid->auid_type));
}

INLINE uint16_t
AuthorityIDDataGetLength(const AuthorityIDDataRef auid)
{
    return (net_uint16_get(auid->auid_length));
}

enum {
    kAuthorityIDDataType = 4
};	

enum {
    kTLVTypeMask = 0x3fff,
    kTLVTypeMandatoryBit = 0x8000,
    kTLVTypeReservedBit = 0x4000,
    kTLVTypeResult = 3,
    kTLVTypeNAK = 4,
    kTLVTypeError = 5,
    kTLVTypeVendorSpecific = 7,
    kTLVTypeEAPPayload = 9,
    kTLVTypeIntermediateResult = 10,
    kTLVTypePAC = 11,
    kTLVTypeCryptoBinding = 12,
    kTLVTypeServerTrustedRoot = 18,
    kTLVTypeRequestAction = 19,
    kTLVTypePKCS7 = 20,
};
typedef uint16_t	TLVType;
typedef uint16_t	TLVLength;

STATIC const char *
TLVTypeName(TLVType tlv_type)
{
    const char *	tlv_name = "Unknown";

    switch (tlv_type) {
    case kTLVTypeResult:
	tlv_name = "Result";
	break;
    case kTLVTypeNAK:
	tlv_name = "NAK";
	break;
    case kTLVTypeError:
	tlv_name = "Error";
	break;
    case kTLVTypeVendorSpecific:
	tlv_name = "VendorSpecific";
	break;
    case kTLVTypeEAPPayload:
	tlv_name = "EAPPayload";
	break;
    case kTLVTypeIntermediateResult:
	tlv_name = "IntermediateResult";
	break;
    case kTLVTypePAC:
	tlv_name = "PAC";
	break;
    case kTLVTypeCryptoBinding:
	tlv_name = "CryptoBinding";
	break;
    case kTLVTypeServerTrustedRoot:
	tlv_name = "ServerTrustedRoot";
	break;
    case kTLVTypeRequestAction:
	tlv_name = "RequestAction";
	break;
    case kTLVTypePKCS7:
	tlv_name = "PKCS7";
	break;
    default:
	break;
    }
    return (tlv_name);
}

typedef struct TLV_s {
    uint8_t		tlv_type[2];
    uint8_t		tlv_length[2];
    uint8_t		tlv_value[1];
} TLV, * TLVRef;
#define TLV_HEADER_LENGTH		offsetof(TLV, tlv_value)

INLINE void
TLVSetLength(TLVRef tlv, TLVLength length)
{
    net_uint16_set(tlv->tlv_length, length);
    return;
}

INLINE TLVLength
TLVGetLength(const TLVRef tlv)
{
    return (net_uint16_get(tlv->tlv_length));
}

INLINE void
TLVSetType(TLVRef tlv, TLVType type)
{
    net_uint16_set(tlv->tlv_type, type);
    return;
}

INLINE TLVType
TLVGetType(const TLVRef tlv)
{
    return (net_uint16_get(tlv->tlv_type));
}

INLINE bool
TLVTypeIsMandatory(TLVType tlv_type)
{
    return ((tlv_type & kTLVTypeMandatoryBit) != 0);
}

INLINE TLVType
TLVTypeGetType(TLVType tlv_type)
{
    return (tlv_type & kTLVTypeMask);
}

typedef struct ResultTLV_s {
    uint8_t		res_type[2];	/* kTLVTypeResult */
    uint8_t		res_length[2];
    uint8_t		res_status[2];	/* 1 (Success), 2 (Failure) */
} ResultTLV, * ResultTLVRef;
#define RESULT_TLV_LENGTH		2

enum {
    kTLVStatusSuccess = 1,
    kTLVStatusFailure = 2,
};
typedef uint16_t TLVStatus;

INLINE void
ResultTLVSetStatus(ResultTLVRef tlv, TLVStatus status)
{
    net_uint16_set(tlv->res_status, status);
    return;
}

INLINE TLVStatus
ResultTLVGetStatus(const ResultTLVRef tlv)
{
    return (net_uint16_get(tlv->res_status));
}

typedef struct NAKTLV_s {
    uint8_t		na_type[2];	/* kTLVTypeNAK */
    uint8_t		na_length[2];
    uint8_t		na_vendor_id[4];
    uint8_t		na_nak_type[2];
    uint8_t		na_tlvs[1];	/* optional, variable length */
} NAKTLV, * NAKTLVRef;
#define NAK_TLV_MIN_LENGTH	6

typedef uint32_t VendorId;

INLINE void
NAKTLVSetVendorId(NAKTLVRef tlv, VendorId vendor_id)
{
    net_uint32_set(tlv->na_vendor_id, vendor_id);
    return;
}

INLINE VendorId
NAKTLVGetVendorId(NAKTLVRef tlv)
{
    return (net_uint32_get(tlv->na_vendor_id));
}

INLINE void
NAKTLVSetNAKType(NAKTLVRef tlv, TLVType nak_type)
{
    net_uint16_set(tlv->na_nak_type, nak_type);
    return;
}

INLINE TLVType
NAKTLVGetNAKType(NAKTLVRef tlv)
{
    return (net_uint16_get(tlv->na_nak_type));
}

typedef struct ErrorTLV_s {
    uint8_t		er_type[2];	/* kTLVTypeError */
    uint8_t		er_length[2];
    uint8_t		er_error_code[4];
} ErrorTLV, * ErrorTLVRef;
#define ERROR_TLV_LENGTH		4

enum {
    kErrorTLVErrorCodeTunnelCompromise = 2001,
    kErrorTLVErrorCodeUnexpectedTLVsExchanged = 2002
};
typedef uint32_t ErrorTLVErrorCode;

INLINE void
ErrorTLVSetErrorCode(ErrorTLVRef tlv, ErrorTLVErrorCode error_code)
{
    net_uint32_set(tlv->er_error_code, error_code);
    return;
}

INLINE ErrorTLVErrorCode
ErrorTLVGetErrorCode(const ErrorTLVRef tlv)
{
    return ((ErrorTLVErrorCode)net_uint32_get(tlv->er_error_code));
}

typedef struct VendorSpecificTLV_s {
    uint8_t		vs_type[2];	/* kTLVTypeVendorSpecific */
    uint8_t		vs_length[2];
    uint8_t		vs_vendor_id[4];
    uint8_t		vs_tlvs[1];	/* variable length */
} VendorSpecificTLV, * VendorSpecificTLVRef;
#define VENDOR_SPECIFIC_TLV_MIN_LENGTH		4

typedef struct EAPPayloadTLV_s {
    uint8_t		ep_type[2];	/* kTLVTypeVendorSpecific */
    uint8_t		ep_length[2];
    uint8_t		ep_eap_packet[1];/* variable length */
    /*
      uint8_t		ep_tlvs[1];
    */
} EAPPayloadTLV, * EAPPayloadTLVRef;

typedef struct IntermediateResultTLV_s {
    uint8_t		ir_type[2];	/* kTLVTypeIntermediateResult */
    uint8_t		ir_length[2];
    uint8_t		ir_status[2];
    uint8_t		ir_tlvs[1];	/* optional, variable length */
} IntermediateResultTLV, * IntermediateResultTLVRef;
#define INTERMEDIATE_RESULT_TLV_MIN_LENGTH	2

INLINE void
IntermediateResultTLVSetStatus(IntermediateResultTLVRef tlv,
			       TLVStatus status)
{
    net_uint16_set(tlv->ir_status, status);
    return;
}

INLINE TLVStatus
IntermediateResultTLVGetStatus(const IntermediateResultTLVRef tlv)
{
    return (net_uint16_get(tlv->ir_status));
}

typedef struct CryptoBindingTLV_s {
    uint8_t		cb_type[2];	/* kTLVTypeCryptoBinding */
    uint8_t		cb_length[2];
    uint8_t		cb_reserved;
    uint8_t		cb_version;
    uint8_t		cb_received_version;
    uint8_t		cb_sub_type;
    uint8_t		cb_nonce[32];
    uint8_t		cb_compound_mac[20];
} CryptoBindingTLV, * CryptoBindingTLVRef;
#define CRYPTO_BINDING_TLV_LENGTH (sizeof(CryptoBindingTLV) - TLV_HEADER_LENGTH)

enum {
    kCryptoBindingSubTypeBindingRequest = 0,
    kCryptoBindingSubTypeBindingResponse = 1
};

typedef struct RequestActionTLV_s {
    uint8_t		re_type[2];	/* kTLVTypeRequestAction */
    uint8_t		re_length[2];
    uint8_t		re_action[2];
} RequestActionTLV, * RequestActionTLVRef;

enum {
    kRequestActionTLVActionProcessTLV = 1,
    kRequestActionTLVActionNegotiateEAP = 2,
};
typedef uint16_t RequestActionTLVAction;

typedef struct PACTLV_s {
    uint8_t		pa_type[2];	/* kTLVTypePAC */
    uint8_t		pa_length[2];
    uint8_t		pa_attributes[1];	/* variable length */
} PACTLV, * PACTLVRef;

enum {
    kPACTLVAttributeTypePAC_Key = 1,
    kPACTLVAttributeTypePAC_Opaque = 2,
    kPACTLVAttributeTypeCRED_LIFETIME = 3,
    kPACTLVAttributeTypeA_ID = 4,
    kPACTLVAttributeTypeI_ID = 5,
    kPACTLVAttributeTypeReserved = 6,
    kPACTLVAttributeTypeA_ID_Info  = 7,
    kPACTLVAttributeTypePAC_Acknowledgement = 8,
    kPACTLVAttributeTypePAC_Info = 9,
    kPACTLVAttributeTypePAC_Type = 10
};
typedef uint16_t	PACTLVAttributeType;

STATIC const char *
PACTLVAttributeTypeName(PACTLVAttributeType pac_type)
{
    const char *	attr_name = "Unknown";

    switch (pac_type) {
    case kPACTLVAttributeTypePAC_Key:
	attr_name = "PAC-Key";
	break;
    case kPACTLVAttributeTypePAC_Opaque:
	attr_name = "PAC-Opaque";
	break;
    case kPACTLVAttributeTypeCRED_LIFETIME:
	attr_name = "CRED_LIFETIME";
	break;
    case kPACTLVAttributeTypeA_ID:
	attr_name = "A-ID";
	break;
    case kPACTLVAttributeTypeI_ID:
	attr_name = "I-ID";
	break;
    case kPACTLVAttributeTypeReserved:
	attr_name = "Reserved";
	break;
    case kPACTLVAttributeTypeA_ID_Info :
	attr_name = "A-ID-Info ";
	break;
    case kPACTLVAttributeTypePAC_Acknowledgement:
	attr_name = "PAC-Acknowledgement";
	break;
    case kPACTLVAttributeTypePAC_Info:
	attr_name = "PAC-Info";
	break;
    case kPACTLVAttributeTypePAC_Type:
	attr_name = "PAC-Type";
	break;
    default:
	break;
    }
    return (attr_name);
}

enum {
    kPACTypeTunnel = 1,
    kPACTypeMachineAuthentication = 2,
    kPACTypeUserAuthorization = 3
};
typedef uint16_t	PACType;

typedef struct PACTypeTLV_s {
    uint8_t		pt_type[2];	/* kPACTLVAttributeTypePAC_Type */
    uint8_t		pt_length[2];
    uint8_t		pt_pac_type[2];	/* 1=Tunnel, 2=Machine, 3=User */
} PACTypeTLV, * PACTypeTLVRef;
#define PAC_TYPE_TLV_LENGTH	2

INLINE void
PACTypeTLVSetPACType(PACTypeTLVRef tlv, PACType pac_type)
{
    net_uint16_set(tlv->pt_pac_type, pac_type);
    return;
}

INLINE PACType
PACTypeTLVGetPACType(const PACTypeTLVRef tlv)
{
    return net_uint16_get(tlv->pt_pac_type);
}

typedef struct PACTLVAttributeList_s {
    TLVRef		key;
    TLVRef		opaque;
    TLVRef		info;
    PACTypeTLVRef	type;
    TLVRef		a_id;
    TLVRef		a_id_info;
    TLVRef		i_id;
} PACTLVAttributeList, * PACTLVAttributeListRef;

typedef struct TLVList_s {
    ResultTLVRef		result;
    TLVStatus			result_status;
    NAKTLVRef			nak;
    ErrorTLVRef			error;
    ErrorTLVErrorCode		error_code;
    EAPPayloadTLVRef		eap;
    IntermediateResultTLVRef	intermediate;
    CryptoBindingTLVRef		crypto;
    TLVRef 			mandatory;
    PACTLVRef			pac;
    PACTLVAttributeList		pac_tlvs;
} TLVList, * TLVListRef;

#define kTLSKeyExpansionLabel		"key expansion"
#define kTLSKeyExpansionLabelLength	(sizeof(kTLSKeyExpansionLabel) - 1)

#define kPACToMasterLabel		"PAC to master secret label hash"
#define kPACToMasterLabelLength		(sizeof(kPACToMasterLabel) - 1)

#define kIMCKLabel			"Inner Methods Compound Keys"
#define kIMCKLabelLength 		(sizeof(kIMCKLabel) - 1)

#define kSessionKeyLabel		"Session Key Generating Function"
#define kSessionKeyLabelLength		(sizeof(kSessionKeyLabel) - 1)

#define kExtendedSessionKeyLabel	"Extended Session Key Generating Function"
#define kExtendedSessionKeyLabelLength	(sizeof(kExtendedSessionKeyLabel) - 1)

#define SESSION_KEY_SEED_LENGTH		40
#define IMCK_LENGTH			60
#define S_IMCK_LENGTH			SESSION_KEY_SEED_LENGTH
#define CMK_LENGTH			20
#define MASTER_SECRET_LENGTH		48
#define MSK_LENGTH			32
#define MASTER_KEY_LENGTH		64
#define EXTENDED_MASTER_KEY_LENGTH	64


/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect eapfast_introspect;
STATIC EAPClientPluginFuncVersion eapfast_version;
STATIC EAPClientPluginFuncEAPType eapfast_type;
STATIC EAPClientPluginFuncEAPName eapfast_name;
STATIC EAPClientPluginFuncInit eapfast_init;
STATIC EAPClientPluginFuncFree eapfast_free;
STATIC EAPClientPluginFuncProcess eapfast_process;
STATIC EAPClientPluginFuncFreePacket eapfast_free_packet;
STATIC EAPClientPluginFuncSessionKey eapfast_session_key;
STATIC EAPClientPluginFuncServerKey eapfast_server_key;
STATIC EAPClientPluginFuncRequireProperties eapfast_require_props;
STATIC EAPClientPluginFuncPublishProperties eapfast_publish_props_copy;
STATIC EAPClientPluginFuncCopyPacketDescription eapfast_copy_packet_description;

typedef enum {
    kRequestTypeStart,
    kRequestTypeAck,
    kRequestTypeData,
} RequestType;

static int inner_auth_types[] = {
    kEAPTypeMSCHAPv2,
    kEAPTypeMD5Challenge,
    kEAPTypeGenericTokenCard,
    kEAPTypeTLS
};

static int inner_auth_types_count = sizeof(inner_auth_types) / sizeof(inner_auth_types[0]);
    
struct eap_client {
    EAPClientModuleRef		module;
    EAPClientPluginData		plugin_data;
    CFArrayRef			require_props;
    CFDictionaryRef		publish_props;
    EAPType			last_type;
    const char *		last_type_name;
    EAPClientStatus		last_status;
    int				last_error;
};

enum {
    kEAPFASTInnerAuthStateUnknown = 0,
    kEAPFASTInnerAuthStateSuccess = 1,
    kEAPFASTInnerAuthStateFailure = 2,
};
typedef int EAPFASTInnerAuthState;

#define TLS_PEER_ID_DATA_LENGTH		16

typedef struct {
    SSLContextRef		ssl_context;
    memoryBuffer		read_buffer;
    int				last_read_size;
    memoryBuffer		write_buffer;
    int				last_write_size;
    int				previous_identifier;
    memoryIO			mem_io;
    EAPClientState		plugin_state;
    bool			cert_is_required;
    CFArrayRef			certs;
    int				mtu;
    bool			eapfast_version_set;
    uint8_t			eapfast_version;
    uint8_t			eapfast_received_version;
    OSStatus			last_ssl_error;
    EAPClientStatus		last_client_status;
    bool			handshake_complete;
    EAPFASTInnerAuthState	inner_auth_state;
    struct eap_client		eap;
    char			in_message_buf[32 * 1024];
    size_t			in_message_size;
    TLVList			in_message_tlvs;
    int				last_eap_type_index;
    OSStatus			trust_ssl_error;
    EAPClientStatus		trust_status;
    bool			trust_proceed;
    bool			master_key_valid;
    uint8_t			master_key[MASTER_KEY_LENGTH];
    uint8_t			extended_master_key[EXTENDED_MASTER_KEY_LENGTH];
    bool			server_auth_completed;
    CFArrayRef			server_certs;
    bool			resume_sessions;
    bool			use_pac;
    bool			provision_pac;
    bool			provision_pac_anonymously;
    uint8_t *			tls_peer_id;
    uint8_t			tls_peer_id_data[TLS_PEER_ID_DATA_LENGTH];
    int				tls_peer_id_length;
    bool			session_was_resumed;
    CFDictionaryRef		pac_dict;
    bool			pac_was_requested;
    bool			pac_was_used;
    bool			pac_was_provisioned;
    bool			must_use_mschapv2;
    bool			crypto_binding_done;
    uint8_t			last_s_imck[S_IMCK_LENGTH];
    uint8_t			mschap_server_challenge[MSCHAP2_CHALLENGE_SIZE];
    uint8_t			mschap_client_challenge[MSCHAP2_CHALLENGE_SIZE];
} EAPFASTPluginData, *EAPFASTPluginDataRef;

enum {
    kEAPTLSAvoidDenialOfServiceSize = 128 * 1024
};

#define BAD_IDENTIFIER			(-1)

typedef struct Buffer {
    uint8_t *	data;
    int		size;
    int		used;
} Buffer, * BufferRef;

INLINE void
BufferInit(BufferRef buf, uint8_t * data, int size)
{
    buf->data = data;
    buf->size = size;
    buf->used = 0;
    return;
}

INLINE uint8_t *
BufferGetPtr(BufferRef buf)
{
    return (buf->data);
}

INLINE uint8_t *
BufferGetWritePtr(BufferRef buf)
{
    return (buf->data + buf->used);
}

INLINE int
BufferGetSize(BufferRef buf)
{
    return (buf->size);
}

INLINE int
BufferGetUsed(BufferRef buf)
{
    return (buf->used);
}

INLINE int
BufferGetSpace(BufferRef buf)
{
    return (buf->size - buf->used);
}

INLINE bool
BufferAdvanceWritePtr(BufferRef buf, int size)
{
    if (size > BufferGetSpace(buf)) {
	EAPLOG(LOG_NOTICE, "EAP-FAST: BufferAdvanceWritePtr failed: %d > %d",
	       size, BufferGetSpace(buf));
	return (FALSE);
    }
    buf->used += size;
    return (TRUE);
}

/**
 ** PAC preferences/keychain routines
 **/

#define kEAPFASTApplicationID	CFSTR("com.apple.network.eapclient.eapfast")

#define kPACList		CFSTR("PACList")
#define kPACKey			CFSTR("PACKey")
#define kPACKeyKeychainItemID	CFSTR("PACKeyKeychainItemID")
#define kPACOpaque		CFSTR("PACOpaque")
#define kPACInfo		CFSTR("PACInfo")
#define kAuthorityID		CFSTR("AuthorityID")
#define kAuthorityIDInfo	CFSTR("AuthorityIDInfo")
#define kInitiatorID		CFSTR("InitiatorID")

STATIC CFArrayRef
pac_list_copy(void)
{
    CFArrayRef	pac_list;

    pac_list = CFPreferencesCopyValue(kPACList, kEAPFASTApplicationID,
				      kCFPreferencesCurrentUser,
				      kCFPreferencesAnyHost);
    if (pac_list == NULL) {
	return (NULL);
    }
    if (isA_CFArray(pac_list) == NULL) {
	my_CFRelease(&pac_list);
    }
    return (pac_list);
}

STATIC int
pac_list_find_pac(CFArrayRef pac_list, 
		  const void * auid, uint16_t auid_length,
		  const uint8_t * initiator, int initiator_length)
{
    int			count;
    int			i;
    CFStringRef		initiator_cf = NULL;
    int			ret = -1;

    if (initiator != NULL) {
	initiator_cf
	    = CFStringCreateWithBytes(NULL,
				      initiator, initiator_length,
				      kCFStringEncodingUTF8,
				      TRUE);
    }
    count = CFArrayGetCount(pac_list);
    for (i = 0; i < count; i++) {
	CFStringRef		initiator;
	CFDictionaryRef		item;
	CFDataRef		this_auid;

	item = CFArrayGetValueAtIndex(pac_list, i);
	if (isA_CFDictionary(item) == NULL) {
	    continue;
	}
	this_auid = CFDictionaryGetValue(item, kAuthorityID);
	if (isA_CFData(this_auid) == NULL) {
	    continue;
	}
	if (CFDataGetLength(this_auid) != auid_length
	    || memcmp(auid, CFDataGetBytePtr(this_auid), auid_length) != 0) {
	    continue;
	}
	initiator = CFDictionaryGetValue(item, kInitiatorID);
	if (initiator == NULL) {
	    ret = i;
	    if (initiator_cf == NULL) {
		/* we were looking for a non-specific one, and found it */
		break;
	    }
	    /* remember non-user-specific but keep looking */
	    continue;
	}
	if (initiator_cf == NULL) {
	    /* we're looking for a non-specific one */
	    continue;
	}
	if (isA_CFString(initiator) == NULL) {
	    /* unexpected type, skip it */
	    continue;
	}
	if (CFEqual(initiator_cf, initiator)) {
	    ret = i;
	    break;
	}
    }
    my_CFRelease(&initiator_cf);
    return (ret);
}

CF_RETURNS_RETAINED STATIC CFDictionaryRef
pac_dict_insert_key(CFDictionaryRef orig_pac_dict)
{
    CFMutableDictionaryRef	dict;
    CFDictionaryRef		pac_dict = NULL;
    CFDataRef			pac_key = NULL;
    OSStatus			status;
    CFStringRef			unique_id_str;

    if (isA_CFData(CFDictionaryGetValue(orig_pac_dict, kPACOpaque)) == NULL) {
	goto done;
    }
    if (isA_CFData(CFDictionaryGetValue(orig_pac_dict, kPACKey)) != NULL) {
	pac_dict = CFRetain(orig_pac_dict);
	goto done;
    }
    unique_id_str = CFDictionaryGetValue(orig_pac_dict, kPACKeyKeychainItemID);
    if (isA_CFString(unique_id_str) == NULL) {
	goto done;
    }
    status = EAPSecKeychainPasswordItemCopy(NULL, unique_id_str, &pac_key);
    if (status != noErr) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: EAPSecKeychainPasswordItemCopy failed, %s (%d)\n",
	       EAPSSLErrorString(status), (int)status);
	goto done;
    }
    dict = CFDictionaryCreateMutableCopy(NULL, 0, orig_pac_dict);
    CFDictionarySetValue(dict, kPACKey, pac_key);
    pac_dict = CFDictionaryCreateCopy(NULL, dict);
    CFRelease(dict);

 done:
    my_CFRelease(&pac_key);
    return (pac_dict);
}

STATIC CFDictionaryRef
pac_dict_copy(const void * auid, uint16_t auid_length, 
	      const uint8_t * initiator, int initiator_length)
{
    int			i;
    CFDictionaryRef	pac_dict = NULL;
    CFArrayRef		pac_list;

    pac_list = pac_list_copy();
    if (pac_list == NULL) {
	goto done;
    }
    i = pac_list_find_pac(pac_list, auid, auid_length,
			  initiator, initiator_length);
    if (i == -1) {
	goto done;
    }
    pac_dict = CFArrayGetValueAtIndex(pac_list, i);
    pac_dict = pac_dict_insert_key(pac_dict);

 done:
    my_CFRelease(&pac_list);
    return (pac_dict);
}

STATIC void
dict_add_tlv_as_data(CFMutableDictionaryRef dict, CFStringRef key, TLVRef tlv,
		     bool header_too)
{
    CFDataRef			data;
    int				len = 0;

    if (header_too) {
	len = TLV_HEADER_LENGTH;
    }

    data = CFDataCreate(NULL, header_too ? (void *)tlv : tlv->tlv_value,
			TLVGetLength(tlv) + len);
    CFDictionarySetValue(dict, key, data);
    my_CFRelease(&data);
    return;
}

STATIC void
dict_add_tlv_as_string(CFMutableDictionaryRef dict, CFStringRef key, TLVRef tlv)
{
    CFStringRef		str;

    str = CFStringCreateWithBytes(NULL, (const UInt8 *)tlv->tlv_value,
				  TLVGetLength(tlv), kCFStringEncodingUTF8,
				  TRUE);
    CFDictionarySetValue(dict, key, str);
    my_CFRelease(&str);
    return;
}

#define PAC_KEY_LABEL		"802.1X EAP-FAST"
#define PAC_KEY_LABEL_SIZE	(sizeof(PAC_KEY_LABEL) - 1)
#define PAC_KEY_DESCR		"PAC-Key"
#define PAC_KEY_DESCR_SIZE 	(sizeof(PAC_KEY_DESCR) - 1)

STATIC OSStatus
pac_keychain_init_items(bool system_mode,
			const uint8_t * initiator, int initiator_length,
			CFDataRef * initiator_p, SecAccessRef * access_p,
			CFDataRef * descr_p, CFDataRef * label_p)
{
    OSStatus		status = noErr;

#if TARGET_OS_EMBEDDED
    *access_p = NULL;
#else /* TARGET_OS_EMBEDDED */
    if (system_mode) {
	CFErrorRef	error = NULL;

	*access_p = SecAccessCreateWithOwnerAndACL(0, 0, kSecUseOnlyUID,
						   NULL, &error);
	if (*access_p == NULL) {
	    status = errSecAllocate;
	    if (error != NULL) {
		EAPLOG(LOG_NOTICE,
		       "EAP-FAST: mySecAccessCreateWithUid failed, %d",
		       (int)CFErrorGetCode(error));
		CFRelease(error);
	    }
	    goto done;
	}
    }
    else {
	status = SecAccessCreate(CFSTR("802.1X EAP-FAST Plug-in"),
				 NULL, access_p);
	if (status != noErr) {
	    EAPLOG(LOG_NOTICE, "EAP-FAST: SecAccessCreate failed, %s (%d)",
		   EAPSecurityErrorString(status), (int)status);
	    goto done;
	}
    }
#endif /* TARGET_OS_EMBEDDED */
    *initiator_p = CFDataCreate(NULL, initiator, initiator_length);
    *label_p = CFDataCreateWithBytesNoCopy(NULL,
					   (void *)PAC_KEY_LABEL,
					   PAC_KEY_LABEL_SIZE,
					   kCFAllocatorNull);
    *descr_p = CFDataCreateWithBytesNoCopy(NULL,
					   (void *)PAC_KEY_DESCR,
					   PAC_KEY_DESCR_SIZE,
					   kCFAllocatorNull);
#if ! TARGET_OS_EMBEDDED
 done:
#endif /* ! TARGET_OS_EMBEDDED */
    return (status);
}

STATIC CFStringRef
pac_keychain_item_create(bool system_mode,
			 const uint8_t * password, int password_length,
			 const uint8_t * initiator, int initiator_length)
{
    SecAccessRef	access = NULL;
    CFDataRef		descr = NULL;
    SecKeychainRef	keychain = NULL;
    CFDataRef		label = NULL;
    CFDataRef		initiator_cf = NULL;
    CFDataRef		password_cf = NULL;
    CFStringRef		unique_id_str = NULL;
    OSStatus		status;

    status = pac_keychain_init_items(system_mode,
				     initiator, initiator_length,
				     &initiator_cf, &access, &descr, &label);
    if (status != noErr) {
	goto done;
    }
    password_cf = CFDataCreate(NULL, password, password_length);
#if ! TARGET_OS_EMBEDDED
    if (system_mode) {
	status =  SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					       &keychain);
	if (status != noErr) {
	    goto done;
	}
    }
#endif /* ! TARGET_OS_EMBEDDED */
    status 
	= EAPSecKeychainPasswordItemCreateUniqueWithAccess(keychain,
							   access,
							   label,
							   descr,
							   initiator_cf,
							   password_cf,
							   &unique_id_str);
    if (status != noErr) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: "
	       "EAPSecKeychainPasswordItemCreateUniqueWithAccess failed,"
	       "%s (%d)", EAPSecurityErrorString(status), (int)status);
    }

 done:
    my_CFRelease(&keychain);
    my_CFRelease(&access);
    my_CFRelease(&descr);
    my_CFRelease(&label);
    my_CFRelease(&initiator_cf);
    my_CFRelease(&password_cf);
    return (unique_id_str);

}

STATIC OSStatus
pac_keychain_item_recreate(bool system_mode,
			   CFStringRef unique_id_str,
			   CFDataRef password_cf,
			   const uint8_t * initiator, int initiator_length)
{
    SecAccessRef	access = NULL;
    CFDataRef		descr = NULL;
    SecKeychainRef	keychain = NULL;
    CFDataRef		label = NULL;
    CFDataRef		initiator_cf = NULL;
    OSStatus		status;

    status = pac_keychain_init_items(system_mode, initiator, initiator_length,
				     &initiator_cf, &access, &descr, &label);
    if (status != noErr) {
	goto done;
    }
#if ! TARGET_OS_EMBEDDED
    if (system_mode) {
	status =  SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					       &keychain);
	if (status != noErr) {
	    goto done;
	}
    }
#endif /* ! TARGET_OS_EMBEDDED */
    status = EAPSecKeychainPasswordItemCreateWithAccess(keychain,
							access,
							unique_id_str,
							label,
							descr,
							initiator_cf,
							password_cf);
    if (status != noErr) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: "
	       "EAPSecKeychainPasswordItemCreateWithAccess failed,"
	       "%s (%d)", EAPSecurityErrorString(status), (int)status);
    }

 done:
    my_CFRelease(&keychain);
    my_CFRelease(&access);
    my_CFRelease(&descr);
    my_CFRelease(&label);
    my_CFRelease(&initiator_cf);
    return (status);
}

STATIC OSStatus
pac_keychain_item_update(bool system_mode, CFStringRef unique_id_str,
			 const uint8_t * password, int password_length,
			 const uint8_t * initiator, int initiator_length)
{
    CFDataRef		password_cf;
    OSStatus		status;

    password_cf = CFDataCreate(NULL, password, password_length);
    status = EAPSecKeychainPasswordItemSet(NULL, unique_id_str, password_cf);
    switch (status) {
    case noErr:
	/* we're done */
	break;
    case errSecItemNotFound:
	status = pac_keychain_item_recreate(system_mode, 
					    unique_id_str, password_cf,
					    initiator, initiator_length);
	break;
    default:
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: EAPSecKeychainPasswordItemSet failed, %s (%d)",
	       EAPSecurityErrorString(status), (int)status);
	break;
    }
    my_CFRelease(&password_cf);
    return (status);
}

STATIC void
remove_pac(CFDictionaryRef pac_dict,
	   const uint8_t * initiator, int initiator_length)
{
    CFDataRef		auid;
    int			i;
    CFArrayRef		pac_list;
    CFMutableArrayRef	new_pac_list;
    OSStatus		status;
    CFStringRef		unique_id_str;

    unique_id_str = CFDictionaryGetValue(pac_dict, kPACKeyKeychainItemID);
    auid = CFDictionaryGetValue(pac_dict, kAuthorityID);
    pac_list = pac_list_copy();
    if (pac_list == NULL) {
	goto done;
    }
    i = pac_list_find_pac(pac_list, 
			  CFDataGetBytePtr(auid), CFDataGetLength(auid),
			  initiator, initiator_length);
    if (i == -1) {
	goto done;
    }
    new_pac_list = CFArrayCreateMutableCopy(NULL, 0, pac_list);
    CFArrayRemoveValueAtIndex(new_pac_list, i);
    status = EAPSecKeychainPasswordItemRemove(NULL, unique_id_str);
    if (status != noErr) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: EAPSecKeychainPasswordItemRemove failed, %s (%d)",
	       EAPSecurityErrorString(status), (int)status);
    }
    CFPreferencesSetValue(kPACList, new_pac_list, kEAPFASTApplicationID,
			  kCFPreferencesCurrentUser,
			  kCFPreferencesAnyHost);
    my_CFRelease(&new_pac_list);
    (void)CFPreferencesAppSynchronize(kEAPFASTApplicationID);

 done:
    my_CFRelease(&pac_list);
    return;
}

STATIC bool
save_pac(bool system_mode, PACTLVAttributeListRef tlvlist_p)
{
    int				i = -1;
    const uint8_t *		initiator = NULL;
    int				initiator_length = 0;
    CFStringRef			key_id = NULL;
    CFMutableArrayRef		new_pac_list = NULL;
    CFArrayRef			old_pac_list = NULL;
    CFMutableDictionaryRef	pac_dict;
    bool			saved = FALSE;

    /* generate a new PAC dictionary */
    pac_dict = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
    /* AuthorityID */
    dict_add_tlv_as_data(pac_dict, kAuthorityID, tlvlist_p->a_id, FALSE);

    /* AuthorityIDInfo */
    if (tlvlist_p->a_id_info != NULL) {
	dict_add_tlv_as_string(pac_dict, kAuthorityIDInfo,
			       tlvlist_p->a_id_info);
    }

    /* InitiatorID */
    if (tlvlist_p->i_id != NULL) {
	dict_add_tlv_as_string(pac_dict, kInitiatorID, tlvlist_p->i_id);
    }

    /* PAC-Opaque */
    dict_add_tlv_as_data(pac_dict, kPACOpaque, tlvlist_p->opaque, TRUE);

    /* PAC-Info */
    dict_add_tlv_as_data(pac_dict, kPACInfo, tlvlist_p->info, FALSE);

    if (tlvlist_p->i_id != NULL) {
	initiator = tlvlist_p->i_id->tlv_value;
	initiator_length = TLVGetLength(tlvlist_p->i_id);
    }
    old_pac_list = pac_list_copy();
    if (old_pac_list == NULL) {
	new_pac_list = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    }
    else {
	const void *		auid;
	int			auid_length;

	new_pac_list = CFArrayCreateMutableCopy(NULL, 0, old_pac_list);
	auid = tlvlist_p->a_id->tlv_value;
	auid_length = TLVGetLength(tlvlist_p->a_id);
	i = pac_list_find_pac(new_pac_list, auid, auid_length,
			      initiator, initiator_length);
	if (i != -1) {
	    CFDictionaryRef	old_pac_dict;
	    
	    old_pac_dict = CFArrayGetValueAtIndex(new_pac_list, i);
	    key_id = CFDictionaryGetValue(old_pac_dict, kPACKeyKeychainItemID);
	}
    }
    if (key_id == NULL) {
	key_id
	    = pac_keychain_item_create(system_mode,
				       tlvlist_p->key->tlv_value,
				       TLVGetLength(tlvlist_p->key),
				       initiator, initiator_length);
	if (key_id == NULL) {
	    goto done;
	}
	CFDictionarySetValue(pac_dict, kPACKeyKeychainItemID, key_id);
	my_CFRelease(&key_id);
    }
    else {
	CFDictionarySetValue(pac_dict, kPACKeyKeychainItemID, key_id);
	if (pac_keychain_item_update(system_mode, key_id,
				     tlvlist_p->key->tlv_value,
				     TLVGetLength(tlvlist_p->key),
				     initiator, initiator_length)
	    != noErr) {
	    goto done;
	}
    }
    if (i == -1) {
	CFArrayAppendValue(new_pac_list, pac_dict);
    }
    else {
	CFArraySetValueAtIndex(new_pac_list, i, pac_dict);
    }
    CFPreferencesSetValue(kPACList, new_pac_list, kEAPFASTApplicationID,
			  kCFPreferencesCurrentUser,
			  kCFPreferencesAnyHost);
    saved = CFPreferencesAppSynchronize(kEAPFASTApplicationID);

 done:
    my_CFRelease(&pac_dict);
    my_CFRelease(&old_pac_list);
    my_CFRelease(&new_pac_list);
    return (saved);
}

/**
 ** EAP-FAST Crypto routines
 **/

/*
 * Function: T_PRF
 * Purpose:
 *   Implements the T-PRF() function as described in the EAP-FAST
 *   internet draft version 03, section 5.5.
 */
STATIC void
T_PRF(const void * key, int key_length,
      const void * label, int label_length,
      const void * seed, int seed_length,
      void * key_material, int key_material_length)
{
    uint8_t *		data;
    /* ALIGN: data_buf is aligned to at least sizeof(uint16_t) */
    uint16_t		data_buf[256/sizeof(uint16_t)];
    int			data_length;
    int			i;
    int			left;
    void *		output;
    uint8_t		t_buf[CC_SHA1_DIGEST_LENGTH];

    if (key_material_length == 0) {
	EAPLOG_FL(LOG_NOTICE, "key_material_length is 0");
	return;
    }
    data_length	
	= label_length 		/* label */
	+ 1			/* 0x00 */
	+ seed_length 		/* seed */
	+ sizeof(t_buf)		/* Ti */
	+ sizeof(uint16_t)	/* outputlength */
	+ 1;			/* 0xNN */
    /* allocate a buffer to store the intermediate data */
    if (data_length > sizeof(data_buf)) {
	data = (uint8_t *)malloc(data_length);
    }
    else {
	data = (uint8_t *)data_buf;
    }
    left = key_material_length;
    output = key_material;
    for (i = 0; ; i++) {
	uint8_t *	offset = data;

	if (i != 0) {
	    memcpy(offset, t_buf, sizeof(t_buf));
	    offset += sizeof(t_buf);
	}
	/* S = label + 0x00 + seed */
	memcpy(offset, label, label_length);
	offset += label_length;
	*offset++ = 0x00;
	if (seed != NULL && seed_length != 0) {
	    memcpy(offset, seed, seed_length);
	    offset += seed_length;
	}
	net_uint16_set((void *)offset, key_material_length); /* outputlength */
	offset += sizeof(uint16_t);
	*offset++ = i + 1;

	/* Ti = HMAC-SHA1 (key, [T(i-1) +] S + outputlength + i) */
	CCHmac(kCCHmacAlgSHA1,
	       key, key_length, data, (offset - data), t_buf);
	if (left <= sizeof(t_buf)) {
	    memcpy(output, t_buf, left);
	    break;
	}
	memcpy(output, t_buf, sizeof(t_buf));
	output += sizeof(t_buf);
	left -= sizeof(t_buf);
    }
    if ((void *)data != (void *)data_buf) {
	free(data);
    }
    return;
}

/**
 ** EAP client module access convenience routines
 **/

STATIC void
eap_client_free(EAPClientPluginDataRef plugin)
{
    EAPFASTPluginDataRef 	context = (EAPFASTPluginDataRef)plugin->private;

    if (context->eap.module != NULL) {
	EAPClientModulePluginFree(context->eap.module, 
				  &context->eap.plugin_data);
	context->eap.module = NULL;
	my_CFRelease((CFDictionaryRef *)&context->eap.plugin_data.properties);
	bzero(&context->eap.plugin_data, sizeof(context->eap.plugin_data));
    }
    my_CFRelease(&context->eap.require_props);
    my_CFRelease(&context->eap.publish_props);
    context->eap.last_type = kEAPTypeInvalid;
    context->eap.last_type_name = NULL;
    context->eap.last_status = kEAPClientStatusOK;
    context->eap.last_error = 0;
    return;
}

STATIC EAPType
eap_client_type(EAPFASTPluginDataRef context)
{
    if (context->eap.module == NULL) {
	return (kEAPTypeInvalid);
    }
    return (EAPClientModulePluginEAPType(context->eap.module));
}

INLINE void
S_set_uint32(const uint32_t * v_p, uint32_t value)
{
    *((uint32_t *)v_p) = value;
    return;
}

STATIC void
eap_client_set_properties(EAPClientPluginDataRef plugin)
{
    EAPFASTPluginDataRef 	context = (EAPFASTPluginDataRef)plugin->private;

    if (context->must_use_mschapv2) {
	CFDataRef			client_challenge;
	CFMutableDictionaryRef		dict;
	CFDataRef			server_challenge;

	dict = CFDictionaryCreateMutableCopy(NULL, 0, 
					     plugin->properties);
	server_challenge
	    = CFDataCreateWithBytesNoCopy(NULL,
					  context->mschap_server_challenge,
					  sizeof(context->mschap_server_challenge),
					  kCFAllocatorNull);
	CFDictionarySetValue(dict, kEAPClientPropEAPMSCHAPv2ServerChallenge,
			     server_challenge);
	CFRelease(server_challenge);
	client_challenge
	    = CFDataCreateWithBytesNoCopy(NULL,
					  context->mschap_client_challenge,
					  sizeof(context->mschap_client_challenge),
					  kCFAllocatorNull);
	CFDictionarySetValue(dict, kEAPClientPropEAPMSCHAPv2ClientChallenge,
			     client_challenge);
	CFRelease(client_challenge);
	my_CFRelease((CFDictionaryRef *)&context->eap.plugin_data.properties);
	*((CFDictionaryRef *)&context->eap.plugin_data.properties) = dict;
    }
    else {
	*((CFDictionaryRef *)&context->eap.plugin_data.properties) 
	    = CFRetain(plugin->properties);
    }
    return;
}

STATIC bool
eap_client_init(EAPClientPluginDataRef plugin, EAPType type)
{
    EAPFASTPluginDataRef 	context = (EAPFASTPluginDataRef)plugin->private;
    EAPClientModule *	module;

    context->eap.last_type = kEAPTypeInvalid;
    context->eap.last_type_name = NULL;

    if (context->eap.module != NULL) {
	EAPLOG_FL(LOG_NOTICE, "already initialized");
	return (TRUE);
    }
    module = EAPClientModuleLookup(type);
    if (module == NULL) {
	return (FALSE);
    }
    my_CFRelease(&context->eap.require_props);
    my_CFRelease(&context->eap.publish_props);
    bzero(&context->eap.plugin_data, sizeof(context->eap.plugin_data));
    S_set_uint32(&context->eap.plugin_data.mtu, plugin->mtu);
    context->eap.plugin_data.username = plugin->username;
    S_set_uint32(&context->eap.plugin_data.username_length, 
		 plugin->username_length);
    context->eap.plugin_data.password = plugin->password;
    S_set_uint32(&context->eap.plugin_data.password_length, 
		 plugin->password_length);
    eap_client_set_properties(plugin);
    context->eap.plugin_data.unique_id = plugin->unique_id;
    S_set_uint32(&context->eap.plugin_data.unique_id_length,
		 plugin->unique_id_length);
    context->eap.last_status = 
	EAPClientModulePluginInit(module, &context->eap.plugin_data,
				  &context->eap.require_props, 
				  &context->eap.last_error);
    context->eap.last_type_name = EAPClientModulePluginEAPName(module);
    context->eap.last_type = type;
    if (context->eap.last_status != kEAPClientStatusOK) {
	return (FALSE);
    }
    context->eap.module = module;
    return (TRUE);
}

STATIC CFArrayRef
eap_client_require_properties(EAPFASTPluginDataRef context)
{
    return (EAPClientModulePluginRequireProperties(context->eap.module,
						   &context->eap.plugin_data));
}

STATIC CFDictionaryRef
eap_client_publish_properties(EAPFASTPluginDataRef context)
{
    return (EAPClientModulePluginPublishProperties(context->eap.module,
						   &context->eap.plugin_data));
}

STATIC EAPClientState
eap_client_process(EAPClientPluginDataRef plugin, EAPPacketRef in_pkt_p,
		   EAPPacketRef * out_pkt_p)
{
    EAPFASTPluginDataRef 	context = (EAPFASTPluginDataRef)plugin->private;
    EAPClientState 	cstate;

    context->eap.plugin_data.username = plugin->username;
    S_set_uint32(&context->eap.plugin_data.username_length, 
		 plugin->username_length);
    context->eap.plugin_data.password = plugin->password;
    S_set_uint32(&context->eap.plugin_data.password_length, 
		 plugin->password_length);
    S_set_uint32(&context->eap.plugin_data.generation, 
		 plugin->generation);
    eap_client_set_properties(plugin);
    cstate = EAPClientModulePluginProcess(context->eap.module,
					  &context->eap.plugin_data,
					  in_pkt_p, out_pkt_p,
					  &context->eap.last_status, 
					  &context->eap.last_error);
    return (cstate);
}

STATIC void
eap_client_free_packet(EAPFASTPluginDataRef context, EAPPacketRef out_pkt_p)
{
    EAPClientModulePluginFreePacket(context->eap.module, 
				    &context->eap.plugin_data,
				    out_pkt_p);
}

STATIC void * 
eap_client_session_key(EAPFASTPluginDataRef context, int * key_length)
{
    return (EAPClientModulePluginSessionKey(context->eap.module, 
					    &context->eap.plugin_data,
					    key_length));
}

/**
 ** eap_client end
 **/
 
STATIC OSStatus
ssl_get_server_client_random(SSLContextRef ssl_context,
			     void * random, int * random_size)
{
    size_t		offset;
    size_t		size;
    OSStatus		status;

    /* get the server random + client random */
    size = *random_size;
    status = SSLInternalServerRandom(ssl_context, random, &size);
    if (status != noErr) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: ssl_get_server_client_random:"
	       " SSLInternalServerRandom failed, %s (%d)\n",
	       EAPSSLErrorString(status), (int)status);
	goto done;
    }
    if ((size + SSL_CLIENT_SRVR_RAND_SIZE) > *random_size) {
	EAPLOG(LOG_NOTICE, 
	       "EAP-FAST: ssl_get_server_client_random:"
	       " SSLInternalServerRandom %ld > %d\n",
	       size + SSL_CLIENT_SRVR_RAND_SIZE, *random_size);
	status = errSSLBufferOverflow;
	goto done;
    }
    offset = size;
    size = *random_size - size;
    status = SSLInternalClientRandom(ssl_context,
				     random + offset, &size);
    if (status != noErr) {
	EAPLOG(LOG_NOTICE, 
	       "EAP-FAST: ssl_get_server_client_random:"
	       " SSLInternalClientRandom failed, %s\n",
	       EAPSSLErrorString(status));
	goto done;
    }
    *random_size = offset + size;
 done:
    return (status);
}

/*
 * Function: eapfast_generate_keying_material
 *
 * Purpose:
 *  Compute the EAP-FAST key block as described in [EAP-FAST-Provisioning] 
 *  section 3.4.  This includes the session_key_seed and the MSCHAPv2
 *  ServerChallenge and ClientChallenge.
 *
 * Returns:
 *  If the return value is noErr, returns values in the supplied 
 *  session_key_seed, server_challenge, and client_challenge buffers.
 *  
 *  Otherwise, returns the error that occurred.
 *
 * Notes:
 *   Using the PRF defined in RFC2246 (TLS 1.0), compute a key block using
 *   of the following form:
 *	client_write_MAC_secret[SecurityParameters.hash_size]
 *	server_write_MAC_secret[SecurityParameters.hash_size]
 *	client_write_key[SecurityParameters.key_material_length]
 *	server_write_key[SecurityParameters.key_material_length]
 *	client_write_IV[SecurityParameters.IV_size]
 *	server_write_IV[SecurityParameters.IV_size]
 *	session_key_seed[40]
 *	MSCHAPv2 ServerChallenge[16]
 *	MSCHAPv2 ClientChallenge[16]
 */
STATIC OSStatus
eapfast_generate_key_material(EAPFASTPluginDataRef context,
			      uint8_t session_key_seed[SESSION_KEY_SEED_LENGTH],
			      uint8_t server_challenge[MSCHAP2_CHALLENGE_SIZE],
			      uint8_t client_challenge[MSCHAP2_CHALLENGE_SIZE])
{
    size_t		digest_size;
    size_t		iv_size;
    uint8_t *		key_block = NULL;
    uint8_t		key_block_data[256];
    int			key_block_length;
    int			key_block_offset;
    char		master_secret[SSL_MASTER_SECRET_SIZE];
    size_t		master_secret_length;
    char		random[SSL_CLIENT_SRVR_RAND_SIZE * 2];
    int			random_size;
    OSStatus		status;
    size_t		symmetric_key_size;

    /* get the server random + client random */
    random_size = sizeof(random);
    status = ssl_get_server_client_random(context->ssl_context,
					  random, &random_size);
    if (status != noErr) {
	goto done;
    }

    /* get the master secret */
    master_secret_length = sizeof(master_secret);
    status = SSLInternalMasterSecret(context->ssl_context, master_secret,
				     &master_secret_length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, 
		  "SSLInternalMasterSecret failed, %s (%d)",
		  EAPSSLErrorString(status), (int)status);
	goto done;
    }
    /* figure out how large the key_block needs to be */
    status = SSLGetCipherSizes(context->ssl_context, &digest_size,
			       &symmetric_key_size, &iv_size);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE,
		  "SSLGetCipherSizes failed, %s (%d)",
		  EAPSSLErrorString(status), (int)status);
	goto done;
    }
    key_block_offset = (digest_size + symmetric_key_size + iv_size) * 2;
    key_block_length = key_block_offset + SESSION_KEY_SEED_LENGTH
	+ MSCHAP2_CHALLENGE_SIZE * 2;
    if (key_block_length > sizeof(key_block_data)) {
	key_block = (uint8_t *)malloc(key_block_length);
    }
    else {
	key_block = key_block_data;
    }

    /* compute the key_block */
    status = SSLInternal_PRF(context->ssl_context,
			     master_secret, master_secret_length,
			     kTLSKeyExpansionLabel,
			     kTLSKeyExpansionLabelLength,
			     random, random_size, 
			     key_block,
			     key_block_length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE,
		  "SSLInternal_PRF failed, %s (%d)\n",
		  EAPSSLErrorString(status), (int)status);
	goto done;
    }
    /* session key seed */
    memcpy(session_key_seed, key_block + key_block_offset,
	   SESSION_KEY_SEED_LENGTH);
    key_block_offset += SESSION_KEY_SEED_LENGTH;

    /* server challenge */
    memcpy(server_challenge, key_block + key_block_offset,
	   MSCHAP2_CHALLENGE_SIZE);
    key_block_offset += MSCHAP2_CHALLENGE_SIZE;

    /* client challenge */
    memcpy(client_challenge, key_block + key_block_offset,
	   MSCHAP2_CHALLENGE_SIZE);
    status = noErr;

 done:
    if (key_block != NULL && key_block != key_block_data) {
	free(key_block);
    }
    return (status);
}

STATIC void
eapfast_compute_session_key(EAPFASTPluginDataRef context)
{
    T_PRF(context->last_s_imck, sizeof(context->last_s_imck),
	  kSessionKeyLabel, kSessionKeyLabelLength,
	  NULL, 0, 
	  context->master_key, sizeof(context->master_key));

    T_PRF(context->last_s_imck, sizeof(context->last_s_imck),
	  kExtendedSessionKeyLabel, kExtendedSessionKeyLabelLength,
	  NULL, 0, 
	  context->extended_master_key, sizeof(context->extended_master_key));
    context->master_key_valid = TRUE;
    return;
}

STATIC void
eapfast_free_context(EAPClientPluginDataRef plugin)
{
    EAPFASTPluginDataRef 	context = (EAPFASTPluginDataRef)plugin->private;

    if (context == NULL) {
	return;
    }
    eap_client_free(plugin);
    if (context->ssl_context != NULL) {
	CFRelease(context->ssl_context);
	context->ssl_context = NULL;
    }
    if (context->tls_peer_id != NULL
	&& context->tls_peer_id != context->tls_peer_id_data) {
	free(context->tls_peer_id);
    }
    my_CFRelease(&context->certs);
    my_CFRelease(&context->server_certs);
    memoryIOClearBuffers(&context->mem_io);
    my_CFRelease(&context->pac_dict);
    free(context);

    return;
}

STATIC const void *
GetAuthorityID(const void * tls_data, int tls_data_length,
	       uint16_t *ret_length)
{
    const void *	auid = NULL;
    AuthorityIDDataRef	auid_data;
    uint16_t		auid_length;

    *ret_length = 0;
    auid_data = (AuthorityIDDataRef)tls_data;
    if (tls_data_length < sizeof(AuthorityIDData)
	|| (AuthorityIDDataGetType(auid_data) != kAuthorityIDDataType)) {
	goto done;
    }
    auid_length = AuthorityIDDataGetLength(auid_data);
    if (auid_length == 0) {
	goto done;
    }
    if (auid_length > (tls_data_length - offsetof(AuthorityIDData, auid_id))) {
	EAPLOG(LOG_NOTICE, "EAP-FAST: GetAuthorityID %d > %ld, ignoring",
	       auid_length,
	       tls_data_length - offsetof(AuthorityIDData, auid_id));
	goto done;
    }
    auid = auid_data->auid_id;
    *ret_length = auid_length;

 done:
    return (auid);
}

STATIC void
eapfast_context_clear(EAPFASTPluginDataRef context)
{
    context->plugin_state = kEAPClientStateAuthenticating;
    context->previous_identifier = BAD_IDENTIFIER;
    context->last_ssl_error = noErr;
    context->last_client_status = kEAPClientStatusOK;
    context->handshake_complete = FALSE;
    context->trust_proceed = FALSE;
    context->server_auth_completed = FALSE;
    context->in_message_size = 0;
    context->inner_auth_state = kEAPFASTInnerAuthStateUnknown;
    context->master_key_valid = FALSE;
    context->last_write_size = 0;
    context->last_read_size = 0;
    context->eapfast_version_set = FALSE;
    context->session_was_resumed = FALSE;
    context->pac_was_requested = FALSE;
    context->pac_was_used = FALSE;
    context->crypto_binding_done = FALSE;
    context->must_use_mschapv2 = FALSE;
    context->pac_was_provisioned = FALSE;
    return;
}

STATIC void 
eapfast_compute_master_secret(SSLContextRef ctx, const void * arg,
			      void * secret, size_t *secret_length)
{
    EAPFASTPluginDataRef 	context = (EAPFASTPluginDataRef)arg;
    CFDataRef			pac_key;
    char			random[SSL_CLIENT_SRVR_RAND_SIZE * 2];
    int				random_size;
    OSStatus			status;

    if (context->pac_dict == NULL) {
	EAPLOG_FL(LOG_NOTICE, "pac_dict is NULL");
	goto failed;
    }
    pac_key = CFDictionaryGetValue(context->pac_dict, kPACKey);
    if (pac_key == NULL) {
	EAPLOG_FL(LOG_NOTICE, "pac_key is NULL");
	goto failed;
    }
    if (*secret_length < MASTER_SECRET_LENGTH) {
	EAPLOG_FL(LOG_NOTICE, "%lu < %d",
		  *secret_length, MASTER_SECRET_LENGTH);
	goto failed;
    }
    random_size = sizeof(random);
    status = ssl_get_server_client_random(context->ssl_context,
					  random, &random_size);
    if (status != noErr) {
	goto failed;
    }
    T_PRF(CFDataGetBytePtr(pac_key), CFDataGetLength(pac_key),
	  kPACToMasterLabel, kPACToMasterLabelLength,
	  random, random_size,
	  secret, MASTER_SECRET_LENGTH);
    *secret_length = MASTER_SECRET_LENGTH;
    context->pac_was_used = TRUE;
    return;

 failed:
    *secret_length = 0;
    return;
}

STATIC OSStatus
eapfast_start(EAPClientPluginDataRef plugin, 
	      const void * in_data_ptr, int in_data_length)
{
    const void *	auid;
    uint16_t 		auid_length;
    EAPFASTPluginDataRef context = (EAPFASTPluginDataRef)plugin->private;
    SSLContextRef	ssl_context = NULL;
    OSStatus		status = noErr;

    auid = GetAuthorityID(in_data_ptr, in_data_length, &auid_length);

    context->last_eap_type_index = 0;
    if (context->ssl_context != NULL) {
	CFRelease(context->ssl_context);
	context->ssl_context = NULL;
    }
    my_CFRelease(&context->server_certs);
    my_CFRelease(&context->pac_dict);
    memoryIOClearBuffers(&context->mem_io);
    ssl_context = EAPTLSMemIOContextCreate(FALSE, &context->mem_io, NULL, 
					   &status);
    if (ssl_context == NULL) {
	EAPLOG_FL(LOG_NOTICE, "EAPTLSMemIOContextCreate failed, %s",
		  EAPSSLErrorString(status));
	goto failed;
    }
    if (context->cert_is_required) {
	if (context->certs == NULL) {
	    status = EAPTLSCopyIdentityTrustChain(plugin->sec_identity,
						  plugin->properties,
						  &context->certs);
	    if (status != noErr) {
		EAPLOG_FL(LOG_NOTICE, 
			  "failed to find client cert/identity, %s (%ld)",
			  EAPSSLErrorString(status), (long)status);
		goto failed;
	    }
	}
	status = SSLSetCertificate(ssl_context, context->certs);
	if (status != noErr) {
	    EAPLOG_FL(LOG_NOTICE, 
		      "SSLSetCertificate failed, %s",
		      EAPSSLErrorString(status));
	    goto failed;
	}
    }
    if (context->use_pac && auid != NULL) {
	CFDictionaryRef		pac_dict;

	pac_dict = pac_dict_copy(auid, auid_length,
				 plugin->username, plugin->username_length);
	if (pac_dict != NULL) {
	    CFDataRef	pac_opaque;

	    pac_opaque = CFDictionaryGetValue(pac_dict, kPACOpaque);
	    status
		= SSLInternalSetSessionTicket(ssl_context,
					      CFDataGetBytePtr(pac_opaque),
					      CFDataGetLength(pac_opaque));
	    if (status != noErr) {
		my_CFRelease(&pac_dict);
		EAPLOG_FL(LOG_NOTICE, 
			  "SSLInternalSetSessionTicket failed, %s",
			  EAPSSLErrorString(status));
		goto failed;
	    }
	    status
		= SSLInternalSetMasterSecretFunction(ssl_context,
						     eapfast_compute_master_secret,
						     context);
	    if (status != noErr) {
		my_CFRelease(&pac_dict);
		EAPLOG_FL(LOG_NOTICE,
			  "SSLInternalSetMasterSecretFunction failed, %s",
			  EAPSSLErrorString(status));
		goto failed;
	    }
	    context->pac_dict = pac_dict;
	}
	else if (context->provision_pac_anonymously) {
	    SSLCipherSuite	cipher = TLS_DH_anon_WITH_AES_128_CBC_SHA;

	    status = SSLSetEnabledCiphers(ssl_context, &cipher, 1);
	    if (status != noErr) {
		EAPLOG_FL(LOG_NOTICE,
			  "SSLSetEnabledCiphers failed, %s",
			  EAPSSLErrorString(status));
		goto failed;
	    }
	}
    }
    if (context->pac_dict == NULL
	&& context->resume_sessions && context->tls_peer_id != NULL) {
	status = SSLSetPeerID(ssl_context, context->tls_peer_id,
			      context->tls_peer_id_length);
	if (status != noErr) {
	    EAPLOG_FL(LOG_NOTICE, 
		      "SSLSetPeerID failed, %s", EAPSSLErrorString(status));
	    goto failed;
	}
    }
    context->ssl_context = ssl_context;
    eapfast_context_clear(context);
    return (status);
 failed:
    if (ssl_context != NULL) {
	CFRelease(ssl_context);
    }
    return (status);
}

STATIC EAPClientStatus
eapfast_init(EAPClientPluginDataRef plugin, CFArrayRef * require_props,
	     EAPClientDomainSpecificError * error)
{
    EAPFASTPluginDataRef	context = NULL;

    *error = 0;
    context = malloc(sizeof(*context));
    bzero(context, sizeof(*context));
    context->cert_is_required 
	= my_CFDictionaryGetBooleanValue(plugin->properties,
					 kEAPClientPropTLSCertificateIsRequired,
					 FALSE);
    context->mtu = plugin->mtu;
    context->resume_sessions
	= my_CFDictionaryGetBooleanValue(plugin->properties, 
					 kEAPClientPropTLSEnableSessionResumption,
					 TRUE);
    if (context->resume_sessions) {
	int		len;

	len = EAP_FAST_NAME_LENGTH;
	if (plugin->unique_id != NULL) {
	    len += plugin->unique_id_length;
	}
	if (len > sizeof(context->tls_peer_id_data)) {
	    context->tls_peer_id = malloc(len);
	}
	else {
	    context->tls_peer_id = context->tls_peer_id_data;
	}
	memcpy(context->tls_peer_id, EAP_FAST_NAME, EAP_FAST_NAME_LENGTH);
	if (plugin->unique_id != NULL) {
	    memcpy(context->tls_peer_id + EAP_FAST_NAME_LENGTH,
		   plugin->unique_id, plugin->unique_id_length);
	}
	context->tls_peer_id_length = len;
    }
    context->use_pac
	= my_CFDictionaryGetBooleanValue(plugin->properties, 
					 kEAPClientPropEAPFASTUsePAC,
					 FALSE);
    if (context->use_pac) {
	context->provision_pac
	    = my_CFDictionaryGetBooleanValue(plugin->properties, 
					     kEAPClientPropEAPFASTProvisionPAC,
					     FALSE);
	context->provision_pac_anonymously
	    = my_CFDictionaryGetBooleanValue(plugin->properties, 
					     kEAPClientPropEAPFASTProvisionPACAnonymously,
					     FALSE);
    }

    /* memoryIOInit() initializes the memoryBuffer structures as well */
    memoryIOInit(&context->mem_io, &context->read_buffer,
		 &context->write_buffer);
    //memoryIOSetDebug(&context->mem_io, TRUE);
    plugin->private = context;
    eapfast_context_clear(context);
    return (kEAPClientStatusOK);
}

STATIC void
eapfast_free(EAPClientPluginDataRef plugin)
{
    eapfast_free_context(plugin);
    plugin->private = NULL;
    return;
}

STATIC void
eapfast_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    if (arg != NULL) {
	free(arg);
    }
    return;
}

STATIC EAPPacketRef
EAPFASTPacketCreateAck(int identifier)
{
    return (EAPTLSPacketCreate(kEAPCodeResponse, kEAPTypeEAPFAST,
			       identifier, 0, NULL, NULL));
}

STATIC bool
is_supported_type(EAPFASTPluginDataRef context, EAPType type)
{
    int			i;

    if (context->must_use_mschapv2) {
	return (type == kEAPTypeMSCHAPv2);
    }

    for (i = 0; i < inner_auth_types_count; i++) {
	if (inner_auth_types[i] == type) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

STATIC EAPType
next_eap_type(EAPFASTPluginDataRef context)
{
    if (context->must_use_mschapv2) {
	if (context->last_eap_type_index == 0) {
	    context->last_eap_type_index++;
	    return (kEAPTypeMSCHAPv2);
	}
	return (kEAPTypeInvalid);
    }
    if (context->last_eap_type_index >= inner_auth_types_count) {
	return (kEAPTypeInvalid);
    }
    return (inner_auth_types[context->last_eap_type_index++]);
}

STATIC EAPResponsePacketRef
eapfast_eap_process(EAPClientPluginDataRef plugin, EAPRequestPacketRef in_pkt_p,
		    char * out_buf, int * out_buf_size, 
		    EAPClientStatus * client_status,
		    bool * call_module_free_packet)
{
    EAPFASTPluginDataRef	context = (EAPFASTPluginDataRef)plugin->private;
    uint8_t			desired_type;
    EAPResponsePacketRef	out_pkt_p = NULL;
    EAPClientState		state;

    *call_module_free_packet = FALSE;
    switch (in_pkt_p->code) {
    case kEAPCodeRequest:
	if (in_pkt_p->type == kEAPTypeInvalid) {
	    goto done;
	}
	if (in_pkt_p->type != eap_client_type(context)) {
	    if (is_supported_type(context, in_pkt_p->type) == FALSE) {
		EAPType eap_type = next_eap_type(context);
		if (eap_type == kEAPTypeInvalid) {
		    *client_status = kEAPClientStatusProtocolNotSupported;
		    context->plugin_state = kEAPClientStateFailure;
		    goto done;
		}
		desired_type = eap_type;
		out_pkt_p = (EAPResponsePacketRef)
		    EAPPacketCreate(out_buf, *out_buf_size,
				    kEAPCodeResponse, 
				    in_pkt_p->identifier,
				    kEAPTypeNak, &desired_type,
				    1, 
				    out_buf_size);
		goto done;
	    }
	    eap_client_free(plugin);
	    if (eap_client_init(plugin, in_pkt_p->type) == FALSE) {
		if (context->eap.last_status 
		    != kEAPClientStatusUserInputRequired) {
		    EAPLOG_FL(LOG_NOTICE,
			      "eap_client_init type %d failed",
			      in_pkt_p->type);
		    *client_status = context->eap.last_status;
		    context->plugin_state = kEAPClientStateFailure;
		    goto done;
		}
		*client_status = context->eap.last_status;
		goto done;
	    }
	}
	break;
    case kEAPCodeResponse:
	if (in_pkt_p->type != eap_client_type(context)) {
	    /* this should not happen, but if it does, ignore the packet */
	    goto done;
	}
	break;
    case kEAPCodeFailure:
	break;
    case kEAPCodeSuccess:
	break;
    default:
	break;
    }
	
    if (context->eap.module == NULL) {
	goto done;
    }

    /* invoke the authentication method "process" function */
    my_CFRelease(&context->eap.require_props);
    my_CFRelease(&context->eap.publish_props);

    state = eap_client_process(plugin, (EAPPacketRef)in_pkt_p, 
			       (EAPPacketRef *)&out_pkt_p);
    if (out_pkt_p != NULL) {
	*call_module_free_packet = TRUE;
	*out_buf_size = EAPPacketGetLength((EAPPacketRef)out_pkt_p);
    }
    context->eap.publish_props = eap_client_publish_properties(context);

    switch (state) {
    case kEAPClientStateAuthenticating:
	if (context->eap.last_status == kEAPClientStatusUserInputRequired) {
	    context->eap.require_props 
		= eap_client_require_properties(context);
	    *client_status = context->last_client_status =
		context->eap.last_status;
	}
	break;
    case kEAPClientStateSuccess:
	/* authentication method succeeded */
	context->inner_auth_state = kEAPFASTInnerAuthStateSuccess;
	break;
    case kEAPClientStateFailure:
	/* authentication method failed */
	context->inner_auth_state = kEAPFASTInnerAuthStateFailure;
	*client_status = context->eap.last_status;
	//context->plugin_state = kEAPClientStateFailure;
	break;
    }

 done:
    return (out_pkt_p);
}

STATIC bool
PACTLVAttributeListParse(PACTLVAttributeListRef tlvlist_p,
			 const void * buf, int buf_size,
			 CFMutableStringRef str)
{
    int			left;
    bool		ret = FALSE;
    TLVRef		scan;

    scan = (const TLVRef)buf;
    left = buf_size;
    while (1) {
	TLVType		tlv_type;
	TLVLength	tlv_length;

	if (left == 0) {
	    break; /* we're done */
	}
	if (left < TLV_HEADER_LENGTH) {
	    STRLOG(str, LOG_NOTICE,
		   "EAP-FAST: TLV attribute is too short (%d < %d)",
		   left, (int)TLV_HEADER_LENGTH);
	    goto done;
	}
	tlv_type = TLVGetType(scan);
	tlv_length = TLVGetLength(scan);
	if (left < (TLV_HEADER_LENGTH + tlv_length)) {
	    STRLOG(str, LOG_NOTICE,
		   "EAP-FAST: TLV attribute is too short (%d < %d)",
		   left, (int)TLV_HEADER_LENGTH);
	    goto done;
	}
	if (str != NULL) {
	    TLVType t = TLVTypeGetType(tlv_type);

	    STRING_APPEND(str, 
			  "%s PACTLV Attribute (type=%d) Length=%d%s\n", 
			  PACTLVAttributeTypeName(t), t, tlv_length,
			  TLVTypeIsMandatory(tlv_type) ? " [mandatory]" : "");
	    print_data_cfstr(str, scan->tlv_value, tlv_length);
	}
	switch (TLVTypeGetType(tlv_type)) {
	case kPACTLVAttributeTypePAC_Key:
	    if (tlv_length > 0
		&& tlvlist_p != NULL && tlvlist_p->key == NULL) {
		tlvlist_p->key = scan;
	    }
	    break;
	case kPACTLVAttributeTypePAC_Opaque:
	    if (tlv_length > 0
		&& tlvlist_p != NULL && tlvlist_p->opaque == NULL) {
		tlvlist_p->opaque = scan;
	    }
	    break;
	case kPACTLVAttributeTypePAC_Info:
	    if (tlvlist_p == NULL || tlvlist_p->info == NULL) {
		/* recurse */
		if (PACTLVAttributeListParse(tlvlist_p,
					     scan->tlv_value, tlv_length,
					     str)
		    && tlv_length > 0 && tlvlist_p != NULL) {
		    tlvlist_p->info = scan;
		}
	    }
	    break;
	case kPACTLVAttributeTypePAC_Type:
	    if (tlv_length < PAC_TYPE_TLV_LENGTH) {
		goto done;
	    }
	    if (tlvlist_p != NULL && tlvlist_p->type == NULL) {
		tlvlist_p->type = (PACTypeTLVRef)scan;
	    }
	    break;
	case kPACTLVAttributeTypeA_ID:
	    if (tlv_length > 0 
		&& tlvlist_p != NULL && tlvlist_p->a_id == NULL) {
		tlvlist_p->a_id = scan;
	    }
	    break;
	case kPACTLVAttributeTypeI_ID:
	    if (tlv_length > 0
		&& tlvlist_p != NULL && tlvlist_p->i_id == NULL) {
		tlvlist_p->i_id = scan;
	    }
	    break;
	case kPACTLVAttributeTypeA_ID_Info:
	    if (tlv_length > 0
		&& tlvlist_p != NULL && tlvlist_p->a_id_info == NULL) {
		tlvlist_p->a_id_info = scan;
	    }
	    break;
	case kPACTLVAttributeTypeCRED_LIFETIME:
	    break;
	case kPACTLVAttributeTypeReserved:
	    break;
	case kPACTLVAttributeTypePAC_Acknowledgement:
	    break;
	default:
	    break;
	}
	left -= (TLV_HEADER_LENGTH + tlv_length);
	scan = (TLVRef)(((void *)scan) + TLV_HEADER_LENGTH + tlv_length);
    }
    ret = TRUE;

 done:
    return (ret);
}

STATIC bool
TLVListParse(TLVListRef tlvlist_p,
	     const void * buf, int buf_size,
	     CFMutableStringRef str)
{
    int			left;
    PACTLVAttributeListRef pac_tlvs = NULL;
    TLVStatus 		result_status;
    bool		ret = FALSE;
    TLVRef		scan;

    if (tlvlist_p != NULL) {
	bzero(tlvlist_p, sizeof(*tlvlist_p));
    }
    scan = (const TLVRef)buf;
    left = buf_size;
    while (1) {
	TLVType		tlv_type;
	TLVLength	tlv_length;

	if (left == 0) {
	    break; /* we're done */
	}
	if (left < TLV_HEADER_LENGTH) {
	    STRLOG(str, LOG_NOTICE,
		   "EAP-FAST: TLV is too short (%d < %d)",
		   left, (int)TLV_HEADER_LENGTH);
	    goto done;
	}
	tlv_type = TLVGetType(scan);
	tlv_length = TLVGetLength(scan);
	if (left < (TLV_HEADER_LENGTH + tlv_length)) {
	    STRLOG(str, LOG_NOTICE,
		   "EAP-FAST: TLV is too short (%d < %d)",
		   left, (int)TLV_HEADER_LENGTH);
	    goto done;
	}
	if (str != NULL) {
	    TLVType t = TLVTypeGetType(tlv_type);

	    STRING_APPEND(str, "%s TLV (type=%d) Length=%d%s\n", 
			  TLVTypeName(t), t, tlv_length,
			  TLVTypeIsMandatory(tlv_type) ? " [mandatory]" : "");
	    print_data_cfstr(str, scan->tlv_value, tlv_length);
	}
	switch (TLVTypeGetType(tlv_type)) {
	case kTLVTypeResult:
	    if (tlv_length < RESULT_TLV_LENGTH) {
		STRLOG(str, LOG_NOTICE,
		       "EAP-FAST: Result TLV is too short (%d < %d)",
		       tlv_length, RESULT_TLV_LENGTH);
		goto done;
	    }
	    result_status = ResultTLVGetStatus((const ResultTLVRef)scan);
	    switch (result_status) {
	    case kTLVStatusSuccess:
		if (str != NULL) {
		    STRING_APPEND(str, "Success\n");
		}
		break;
	    case kTLVStatusFailure:
		if (str != NULL) {
		    STRING_APPEND(str, "Failure\n");
		}
		break;
	    default:
		STRLOG(str, LOG_NOTICE,
		       "EAP-FAST: Result TLV unrecognized status = %d",
		       result_status);
		goto done;
	    }
	    if (tlvlist_p != NULL) {
		if (tlvlist_p->result != NULL) {
		    STRLOG(str, LOG_NOTICE,
			   "EAP-FAST: multiple Result TLV's defined");
		    goto done;
		}
		tlvlist_p->result = (ResultTLVRef)scan;
		tlvlist_p->result_status = result_status;
	    }
	    break;
	case kTLVTypeNAK:
	    if (tlv_length < NAK_TLV_MIN_LENGTH) {
		STRLOG(str, LOG_NOTICE,
		       "EAP-FAST: NAK TLV is too short (%d < %d)",
		       tlv_length, NAK_TLV_MIN_LENGTH);
		goto done;
	    }
	    if (tlvlist_p != NULL && tlvlist_p->nak == NULL) {
		tlvlist_p->nak = (NAKTLVRef)scan;
	    }
	    break;
	case kTLVTypeError:
	    if (tlv_length < ERROR_TLV_LENGTH) {
		STRLOG(str, LOG_NOTICE,
		       "EAP-FAST: Error TLV is too short (%d < %d)",
		       tlv_length, ERROR_TLV_LENGTH);
		goto done;
	    }
	    if (str) {
		STRING_APPEND(str, "ErrorCode = %d\n",
			      ErrorTLVGetErrorCode((ErrorTLVRef)scan));
	    }
	    if (tlvlist_p != NULL && tlvlist_p->error == NULL) {
		tlvlist_p->error = (ErrorTLVRef)scan;
		tlvlist_p->error_code
		    = ErrorTLVGetErrorCode((ErrorTLVRef)scan);
	    }
	    break;
	case kTLVTypeEAPPayload:
	    if (tlvlist_p != NULL) {
		if (tlvlist_p->eap != NULL) {
		    STRLOG(str, LOG_NOTICE,
			   "EAP-FAST: EAP Payload TLV appears multiple times");
		    goto done;
		}
		tlvlist_p->eap = (EAPPayloadTLVRef)scan;
	    }
	    if (EAPPacketIsValid((EAPPacketRef)scan->tlv_value, tlv_length, 
				 NULL) == FALSE) {
		STRLOG(str, LOG_NOTICE, 
		       "EAP-FAST: EAP Payload TLV invalid");
		if (str != NULL) {
		    (void)EAPPacketIsValid((EAPPacketRef)scan->tlv_value,
					   tlv_length, str);
		}
		goto done;
	    }
	    break;
	case kTLVTypeIntermediateResult:
	    if (tlv_length < INTERMEDIATE_RESULT_TLV_MIN_LENGTH) {
		STRLOG(str, LOG_NOTICE,
		       "EAP-FAST: Intermediate Result TLV too short (%d < %d)",
		       tlv_length, INTERMEDIATE_RESULT_TLV_MIN_LENGTH);
		goto done;
	    }
	    if (tlvlist_p != NULL) {
		if (tlvlist_p->intermediate != NULL) {
		    STRLOG(str, LOG_NOTICE,
			   "EAP-FAST: multiple Intermediate Result TLV's");
		    goto done;
		}
		tlvlist_p->intermediate = (IntermediateResultTLVRef)scan;
	    }
	    break;
	case kTLVTypeCryptoBinding:
	    if (tlv_length < CRYPTO_BINDING_TLV_LENGTH) {
		STRLOG(str, LOG_NOTICE,
		       "EAP-FAST: Crypto Binding TLV too short (%d < %ld)",
		       tlv_length, CRYPTO_BINDING_TLV_LENGTH);
		goto done;
	    }
	    if (tlvlist_p != NULL) {
		if (tlvlist_p->crypto != NULL) {
		    STRLOG(str, LOG_NOTICE,
			   "EAP-FAST: multiple Crypto Binding TLV's defined");
		    goto done;
		}
		tlvlist_p->crypto = (CryptoBindingTLVRef)scan;
	    }
	    break;
	case kTLVTypePAC:
	    if (tlvlist_p != NULL && tlvlist_p->pac == NULL) {
		pac_tlvs = &tlvlist_p->pac_tlvs;
	    }
	    if (PACTLVAttributeListParse(pac_tlvs,
					 scan->tlv_value, tlv_length,
					 str) == FALSE) {
		STRLOG(str, LOG_NOTICE, "EAP-FAST: PAC TLV parse failed");
		goto done;
	    }
	    if (pac_tlvs != NULL) {
		tlvlist_p->pac = (PACTLVRef)scan;
	    }
	    break;
	case kTLVTypeVendorSpecific:
	    if (tlv_length < VENDOR_SPECIFIC_TLV_MIN_LENGTH) {
		STRLOG(str, LOG_NOTICE,
		       "EAP-FAST: Vendor Specific TLV too short (%d < %d)",
		       tlv_length, VENDOR_SPECIFIC_TLV_MIN_LENGTH);
		goto done;
	    }
	    /* FALL THROUGH */
	default:
	    if (tlvlist_p != NULL) {
		if (TLVTypeIsMandatory(tlv_type)
		    && tlvlist_p->mandatory == NULL) {
		    /* we don't understand this TLV, and it's mandatory */
		    tlvlist_p->mandatory = scan;
		}
	    }
	    break;
	}
	left -= (TLV_HEADER_LENGTH + tlv_length);
	scan = (TLVRef)(((void *)scan) + TLV_HEADER_LENGTH + tlv_length);
    }
    ret = TRUE;

 done:
    return (ret);
}

STATIC bool
make_result(BufferRef buf, TLVStatus result_status)
{
    ResultTLVRef	res;

    res = (ResultTLVRef)BufferGetWritePtr(buf);
    if (BufferAdvanceWritePtr(buf, TLV_HEADER_LENGTH + RESULT_TLV_LENGTH)
	== FALSE) {
	EAPLOG(LOG_NOTICE, "EAP-FAST: make_result(): buffer too small");
	return (FALSE);
    }
    TLVSetLength((TLVRef)res, RESULT_TLV_LENGTH);
    TLVSetType((TLVRef)res, kTLVTypeMandatoryBit | kTLVTypeResult);
    ResultTLVSetStatus(res, result_status);
    return (TRUE);
}

STATIC bool
make_intermediate_result(BufferRef buf, TLVStatus result_status)
{
    IntermediateResultTLVRef	ires;

    ires = (IntermediateResultTLVRef)BufferGetWritePtr(buf);
    if (BufferAdvanceWritePtr(buf, 
			      (TLV_HEADER_LENGTH
			       + INTERMEDIATE_RESULT_TLV_MIN_LENGTH))
	== FALSE) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: make_intermediate_result(): buffer too small");
	return (FALSE);
    }
    TLVSetLength((TLVRef)ires, INTERMEDIATE_RESULT_TLV_MIN_LENGTH);
    TLVSetType((TLVRef)ires, kTLVTypeMandatoryBit | kTLVTypeIntermediateResult);
    IntermediateResultTLVSetStatus(ires, result_status);
    return (TRUE);
}

STATIC bool
make_error(BufferRef buf, ErrorTLVErrorCode code)
{
    ErrorTLVRef		err;

    err = (ErrorTLVRef)BufferGetWritePtr(buf);
    if (BufferAdvanceWritePtr(buf, TLV_HEADER_LENGTH + ERROR_TLV_LENGTH)
	== FALSE) {
	EAPLOG(LOG_NOTICE, "EAP-FAST: make_error(): buffer too small");
	return (FALSE);
    }
    TLVSetLength((TLVRef)err, ERROR_TLV_LENGTH);
    TLVSetType((TLVRef)err, kTLVTypeMandatoryBit | kTLVTypeError);
    ErrorTLVSetErrorCode(err, code);
    return (TRUE);
}

STATIC bool
make_nak(BufferRef buf, const TLVRef tlv)
{
    NAKTLVRef		nak;

    nak = (NAKTLVRef)BufferGetWritePtr(buf);
    if (BufferAdvanceWritePtr(buf, TLV_HEADER_LENGTH + NAK_TLV_MIN_LENGTH)
	== FALSE) {
	EAPLOG(LOG_NOTICE, "EAP-FAST: make_nak(): buffer too small");
	return (FALSE);
    }
    TLVSetLength((TLVRef)nak, NAK_TLV_MIN_LENGTH);
    TLVSetType((TLVRef)nak, kTLVTypeMandatoryBit | kTLVTypeNAK);
    NAKTLVSetNAKType(nak, TLVTypeGetType(TLVGetType(tlv)));
    NAKTLVSetVendorId(nak, 0);
    return (TRUE);
}

STATIC bool
make_eap(BufferRef buf, const EAPPacketRef out_pkt_p, int out_pkt_size)
{
    EAPPayloadTLVRef	eap;

    eap = (EAPPayloadTLVRef)BufferGetWritePtr(buf);
    if (BufferAdvanceWritePtr(buf, out_pkt_size + TLV_HEADER_LENGTH) == FALSE) {
	EAPLOG(LOG_NOTICE, "EAP-FAST: make_eap(): buffer too small");
	return (FALSE);
    }
    TLVSetLength((TLVRef)eap, out_pkt_size);
    TLVSetType((TLVRef)eap, kTLVTypeMandatoryBit | kTLVTypeEAPPayload);
    memcpy(eap->ep_eap_packet, out_pkt_p, out_pkt_size);
    return (TRUE);
}

STATIC bool
make_pac_request(BufferRef buf)
{
    PACTLVRef		pac;
    PACTypeTLVRef	pac_type;

    pac = (PACTLVRef)BufferGetWritePtr(buf);
    if (BufferAdvanceWritePtr(buf, TLV_HEADER_LENGTH * 2 + PAC_TYPE_TLV_LENGTH)
	== FALSE) {
	EAPLOG(LOG_NOTICE, "EAP-FAST: make_pac_request(): buffer too small");
	return (FALSE);
    }
    /* set the PAC TLV header to encapsulate the PAC-Type TLV */
    TLVSetLength((TLVRef)pac, TLV_HEADER_LENGTH + PAC_TYPE_TLV_LENGTH);
    TLVSetType((TLVRef)pac, kTLVTypePAC);

    /* PAC-Type TLV */
    pac_type = (PACTypeTLVRef)pac->pa_attributes;
    TLVSetLength((TLVRef)pac_type, PAC_TYPE_TLV_LENGTH);
    TLVSetType((TLVRef)pac_type, kPACTLVAttributeTypePAC_Type);
    PACTypeTLVSetPACType(pac_type, kPACTypeTunnel);
    return (TRUE);
}

STATIC bool
make_pac_ack(BufferRef buf, TLVStatus result_status)
{
    PACTLVRef		pac;
    ResultTLVRef	pac_ack;

    pac = (PACTLVRef)BufferGetWritePtr(buf);
    if (BufferAdvanceWritePtr(buf, TLV_HEADER_LENGTH * 2 + RESULT_TLV_LENGTH)
	== FALSE) {
	EAPLOG(LOG_NOTICE, "EAP-FAST: make_pac_ack(): buffer too small");
	return (FALSE);
    }
    /* set the PAC TLV header to encapsulate the PAC-Acknowledgement TLV */
    TLVSetLength((TLVRef)pac, TLV_HEADER_LENGTH + PAC_TYPE_TLV_LENGTH);
    TLVSetType((TLVRef)pac, kTLVTypePAC);

    /* PAC-Type TLV */
    pac_ack = (ResultTLVRef)pac->pa_attributes;
    TLVSetLength((TLVRef)pac_ack, RESULT_TLV_LENGTH);
    TLVSetType((TLVRef)pac_ack, kPACTLVAttributeTypePAC_Acknowledgement);
    ResultTLVSetStatus(pac_ack, result_status);
    return (TRUE);
}

STATIC bool
process_crypto_binding(EAPFASTPluginDataRef context,
		       const CryptoBindingTLVRef crypto, BufferRef buf)
{
    CryptoBindingTLVRef		cb_p;
    uint8_t			compound_mac[20];
    void *			inner_auth_key;
    int				inner_auth_key_length;
    uint8_t			imck[IMCK_LENGTH];
    uint8_t			msk[MSK_LENGTH];
    bool			ret = FALSE;

    /* make sure there's room for a Crypto Binding TLV */
    if (BufferGetSpace(buf) < (CRYPTO_BINDING_TLV_LENGTH + TLV_HEADER_LENGTH)) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: process_crypto_binding: buffer too small %d < %ld",
	       BufferGetSpace(buf),
	       CRYPTO_BINDING_TLV_LENGTH + TLV_HEADER_LENGTH);
	goto done;
    }

    if (crypto->cb_version != kEAPFASTVersion1) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: process_crypto_binding version is %d != %d",
	       crypto->cb_version, kEAPFASTVersion1);
	goto done;
    }
    if (crypto->cb_received_version != context->eapfast_version) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: process_crypto_binding received_version is %d != %d",
	       crypto->cb_received_version, context->eapfast_version);
	goto done;
    }
    if (crypto->cb_sub_type != kCryptoBindingSubTypeBindingRequest) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: process_crypto_binding sub_type %d != %d",
	       crypto->cb_sub_type, kCryptoBindingSubTypeBindingRequest);
	goto done;
    }
    inner_auth_key = eap_client_session_key(context, &inner_auth_key_length);
    if (inner_auth_key != NULL) {
	if (inner_auth_key_length > sizeof(msk)) {
	    inner_auth_key_length = sizeof(msk);
	}
	memcpy(msk, inner_auth_key, inner_auth_key_length);
	if (inner_auth_key_length < sizeof(msk)) {
	    /* pad to MSK_LENGTH bytes */
	    memset(msk + inner_auth_key_length, 0,
		   sizeof(msk) - inner_auth_key_length);
	}
    }
    else {
	if (context->must_use_mschapv2) {
	    EAPLOG(LOG_NOTICE, "EAP-FAST: anonymous PAC provisioning "
		   "requires MSCHAPv2 - possible malicious server");
	    goto done;
	}
	/* no inner-auth key */
	memset(msk, 0, sizeof(msk));
    }

    /* compute the new IMCK[j] */
    T_PRF(context->last_s_imck, S_IMCK_LENGTH,
	  kIMCKLabel, kIMCKLabelLength,
	  msk, MSK_LENGTH, imck, IMCK_LENGTH);

    /* use TLV buffer for Compound MAC calculation and to store our response */
    cb_p = (CryptoBindingTLVRef)BufferGetWritePtr(buf);
    *cb_p = *crypto;

    /* calculate the Server Compound MAC, and compare against input TLV */
    memset(cb_p->cb_compound_mac, 0, sizeof(cb_p->cb_compound_mac));
    CCHmac(kCCHmacAlgSHA1,
	   imck + S_IMCK_LENGTH, CMK_LENGTH,
	   (unsigned char *)cb_p, sizeof(*cb_p), compound_mac);
    if (bcmp(crypto->cb_compound_mac, compound_mac,
	     sizeof(compound_mac)) != 0) {
	EAPLOG(LOG_NOTICE,
	       "EAP-FAST: process_crypto_binding Compound MAC is incorrect");
	goto done;
    }

    /* turn this into our response */
    TLVSetLength((TLVRef)cb_p, CRYPTO_BINDING_TLV_LENGTH);
    TLVSetType((TLVRef)cb_p, kTLVTypeMandatoryBit | kTLVTypeCryptoBinding);
    cb_p->cb_received_version = context->eapfast_received_version;
    cb_p->cb_nonce[sizeof(cb_p->cb_nonce) - 1] |= 0x01; /* LSBit must be 1 */
    cb_p->cb_sub_type = kCryptoBindingSubTypeBindingResponse;
    /* calculate the Client Compound MAC */
    CCHmac(kCCHmacAlgSHA1,
	   imck + S_IMCK_LENGTH, CMK_LENGTH,
	   (unsigned char *)cb_p, sizeof(*cb_p), compound_mac);
    memcpy(cb_p->cb_compound_mac, compound_mac, sizeof(compound_mac));
    BufferAdvanceWritePtr(buf, CRYPTO_BINDING_TLV_LENGTH + TLV_HEADER_LENGTH);

    /* remember S-IMCK[j] in last_s_imck for subsequent calculations */
    memcpy(context->last_s_imck, imck, sizeof(context->last_s_imck));
    ret = TRUE;

 done:
    return (ret);
}

STATIC bool
eapfast_eap(EAPClientPluginDataRef plugin, EAPTLSPacketRef eaptls_in,
	    EAPClientStatus * client_status)
{
    EAPFASTPluginDataRef context = (EAPFASTPluginDataRef)plugin->private;
    bool		do_log = FALSE;
    uint8_t 		out_tlvs[16 * 1024];
    Buffer		out_tlvs_buf;
    Buffer		out_tlvs_buf_saved = { 0, 0, 0 };
    size_t		out_tlvs_size;
    bool		result_success = FALSE;
    bool		ret = FALSE;
    OSStatus		status;
    TLVListRef		tlvlist_p = &context->in_message_tlvs;

    BufferInit(&out_tlvs_buf, out_tlvs, sizeof(out_tlvs));
    if (context->in_message_size == 0) {
	bool			parse_ok;

	do_log = TRUE;
	status = SSLRead(context->ssl_context, context->in_message_buf,
			 sizeof(context->in_message_buf),
			 &context->in_message_size);
	if (status == errSSLWouldBlock) {
	    /* no more data to read, so send our response or ACK */
	    ret = TRUE;
	    goto done;
	}
	if (status != noErr) {
	    EAPLOG_FL(LOG_NOTICE, "SSLRead failed, %s (%d)",
		      EAPSSLErrorString(status), (int)status);
	    context->plugin_state = kEAPClientStateFailure;
	    context->last_ssl_error = status;
	    goto done;
	}
	if (context->in_message_size == 0) {
	    EAPLOG_FL(LOG_NOTICE, "zero-length TLV");
	    context->plugin_state = kEAPClientStateFailure;
	    make_result(&out_tlvs_buf, kTLVStatusFailure);
	    make_error(&out_tlvs_buf,
		       kErrorTLVErrorCodeUnexpectedTLVsExchanged);
	    goto send_message;
	}
	{
	    CFMutableStringRef	str = NULL;

	    if (plugin->log_enabled) {
		str = CFStringCreateMutable(NULL, 0);
	    }
	    parse_ok = TLVListParse(tlvlist_p,
				    context->in_message_buf,
				    context->in_message_size,
				    str);
	    if (str != NULL) {
		EAPLOG(-LOG_DEBUG, "-------- Receive TLVs: ----------\n%@",
		       str);
		CFRelease(str);
	    }
	}
	if (parse_ok == FALSE) {
	    context->plugin_state = kEAPClientStateFailure;
	    make_result(&out_tlvs_buf, kTLVStatusFailure);
	    make_error(&out_tlvs_buf,
		       kErrorTLVErrorCodeUnexpectedTLVsExchanged);
	    goto send_message;
	}
	/* check whether we got a mandatory TLV that we didn't understand */
	if (tlvlist_p->mandatory != NULL) {
	    if (tlvlist_p->result != NULL) {
		/* supply a Result + Error */
		make_result(&out_tlvs_buf, kTLVStatusFailure);
		make_error(&out_tlvs_buf,
			   kErrorTLVErrorCodeUnexpectedTLVsExchanged);
	    }
	    else {
		/* send a NAK */
		make_nak(&out_tlvs_buf, tlvlist_p->mandatory);
	    }
	    goto send_message;
	}

	/* Intermediate Result TLV */
	if (tlvlist_p->intermediate != NULL) {
	    TLVStatus	result_status;

	    if (eap_client_type(context) == kEAPTypeInvalid) {
		EAPLOG_FL(LOG_NOTICE,
			  "Intermediate Result TLV supplied"
			  " but no inner EAP method negotiated");
		context->plugin_state = kEAPClientStateFailure;
		make_result(&out_tlvs_buf, kTLVStatusFailure);
		make_error(&out_tlvs_buf,
			   kErrorTLVErrorCodeUnexpectedTLVsExchanged);
		goto send_message;
	    }
	    result_status = 
		IntermediateResultTLVGetStatus(tlvlist_p->intermediate);
	    switch (result_status) {
	    case kTLVStatusSuccess:
		if (tlvlist_p->crypto != NULL) {
		    make_intermediate_result(&out_tlvs_buf,
					     kTLVStatusSuccess);
		    break;
		}
		/* intermediate status success requires crypto binding TLV */
		EAPLOG_FL(LOG_NOTICE, "Crypto Binding TLV is missing");
		context->plugin_state = kEAPClientStateFailure;
		make_result(&out_tlvs_buf, kTLVStatusFailure);
		make_error(&out_tlvs_buf,
			   kErrorTLVErrorCodeUnexpectedTLVsExchanged);
		goto send_message;
		break;
	    default:
	    case kTLVStatusFailure:
		if (result_status == kTLVStatusFailure) {
		    EAPLOG_FL(LOG_NOTICE, "Intermediate Result TLV Failure");
		}
		else {
		    EAPLOG_FL(LOG_NOTICE, "Intermediate Result TLV:"
			      " unrecognized status = %d",
			      result_status);
		}
		context->plugin_state = kEAPClientStateFailure;
		make_result(&out_tlvs_buf, kTLVStatusFailure);
		make_error(&out_tlvs_buf,
			   kErrorTLVErrorCodeUnexpectedTLVsExchanged);
		goto send_message;
		break;
	    }
	}
	/* Result TLV */
	if (tlvlist_p->result != NULL) {
	    switch (tlvlist_p->result_status) {
	    default:
	    case kTLVStatusFailure:
		context->inner_auth_state = kEAPFASTInnerAuthStateFailure;
		if (tlvlist_p->error != NULL) {
		    EAPLOG_FL(LOG_NOTICE,
			      "Result TLV Failure, Error %d",
			      tlvlist_p->error_code);
		}
		else {
		    EAPLOG_FL(LOG_NOTICE,
			      "Result TLV Failure");
		}
		break;
	    case kTLVStatusSuccess:
		/* we're done, right? */
		result_success = TRUE;
		break;
	    }
	    /* remember where we were in case we need to back out */
	    out_tlvs_buf_saved = out_tlvs_buf;
	    make_result(&out_tlvs_buf, tlvlist_p->result_status);
	}
	/* Crypto-binding TLV */
	if (tlvlist_p->crypto != NULL) {
	    if (process_crypto_binding(context, tlvlist_p->crypto,
				       &out_tlvs_buf) == FALSE) {
		EAPLOG_FL(LOG_NOTICE, "Crypto Binding TLV validation failed");
		context->plugin_state = kEAPClientStateFailure;
		if (tlvlist_p->result != NULL) {
		    out_tlvs_buf = out_tlvs_buf_saved;
		}
		make_result(&out_tlvs_buf, kTLVStatusFailure);
		make_error(&out_tlvs_buf,
			   kErrorTLVErrorCodeTunnelCompromise);
		goto send_message;
	    }
	    context->crypto_binding_done = TRUE;
	}

	/* PAC TLV */
	if (tlvlist_p->pac != NULL) {
	    if (context->crypto_binding_done
		&& context->use_pac
		&& tlvlist_p->pac_tlvs.key != NULL
		&& tlvlist_p->pac_tlvs.opaque != NULL
		&& tlvlist_p->pac_tlvs.a_id != NULL
		&& save_pac(plugin->system_mode, &tlvlist_p->pac_tlvs)) {
		make_pac_ack(&out_tlvs_buf, kTLVStatusSuccess);
		context->pac_was_provisioned = TRUE;
		EAPLOG(LOG_NOTICE, "EAP-FAST: PAC was provisioned");
	    }
	    else {
		make_pac_ack(&out_tlvs_buf, kTLVStatusFailure);
	    }
	}

	/* compute session key, ask for a PAC (if necessary) */
	if (result_success) {
	    context->inner_auth_state = kEAPFASTInnerAuthStateSuccess;
	    eapfast_compute_session_key(context);
	    if (context->provision_pac
		&& tlvlist_p->pac == NULL
		&& context->pac_dict == NULL
		&& context->pac_was_requested == FALSE) {
		context->pac_was_requested = TRUE;
		make_pac_request(&out_tlvs_buf);
	    }
	}
	if (tlvlist_p->result != NULL) {
	    goto send_message;
	}
    }
    if (tlvlist_p->eap != NULL) {
	bool 			call_module_free_packet = FALSE;
	EAPRequestPacketRef 	in_pkt_p = NULL;
	char 			out_pkt_buf[2048];
	EAPResponsePacketRef 	out_pkt_p = NULL;
	int			out_pkt_size;

	in_pkt_p = (EAPRequestPacketRef)tlvlist_p->eap->ep_eap_packet;
	if (plugin->log_enabled && do_log) {
	    CFMutableStringRef	log_msg;

	    /* we haven't seen it before, so log it */
	    log_msg = CFStringCreateMutable(NULL, 0);
	    EAPPacketIsValid((const EAPPacketRef)in_pkt_p,
			     EAPPacketGetLength((const EAPPacketRef)in_pkt_p),
			     log_msg);
	    EAPLOG(-LOG_DEBUG, "EAP-FAST Receive EAP Payload:\n%@", log_msg);
	    CFRelease(log_msg);
	}
	switch (in_pkt_p->code) {
	case kEAPCodeRequest:
	    switch (in_pkt_p->type) {
	    case kEAPTypeIdentity:
		out_pkt_p = (EAPResponsePacketRef)
		    EAPPacketCreate(out_pkt_buf, sizeof(out_pkt_buf), 
				    kEAPCodeResponse, in_pkt_p->identifier,
				    kEAPTypeIdentity, plugin->username,
				    plugin->username_length, 
				    &out_pkt_size);
		break;
	    case kEAPTypeNotification:
		out_pkt_p = (EAPResponsePacketRef)
		    EAPPacketCreate(out_pkt_buf, sizeof(out_pkt_buf), 
				    kEAPCodeResponse, in_pkt_p->identifier,
				    kEAPTypeNotification, NULL, 0, 
				    &out_pkt_size);
		break;
	    default:
		out_pkt_size = sizeof(out_pkt_buf);
		out_pkt_p = eapfast_eap_process(plugin, in_pkt_p,
						out_pkt_buf, &out_pkt_size,
						client_status,
						&call_module_free_packet);
		break;
	    }
	    break;
	case kEAPCodeResponse:
	case kEAPCodeSuccess:
	case kEAPCodeFailure:
	    out_pkt_p = eapfast_eap_process(plugin, in_pkt_p,
					    out_pkt_buf, &out_pkt_size,
					    client_status,
					    &call_module_free_packet);
	    break;
	}

	if (out_pkt_p == NULL) {
	    goto done;
	}
	if (plugin->log_enabled) {
	    CFMutableStringRef		log_msg;

	    log_msg = CFStringCreateMutable(NULL, 0);
	    EAPPacketIsValid((const EAPPacketRef)out_pkt_p,
			     EAPPacketGetLength((const EAPPacketRef)out_pkt_p),
			     log_msg);
	    EAPLOG(-LOG_DEBUG, "EAP-FAST Send EAP Payload:\n%@", log_msg);
	    CFRelease(log_msg);
	}
	if (make_eap(&out_tlvs_buf, (void *)out_pkt_p, out_pkt_size)
	    == FALSE) {
	    EAPLOG_FL(LOG_NOTICE, "failed to insert EAP Payload TLV");
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
	if ((char *)out_pkt_p != out_pkt_buf) {
	    if (call_module_free_packet) {
		eap_client_free_packet(context, (EAPPacketRef)out_pkt_p);
	    }
	    else {
		free(out_pkt_p);
	    }
	}
    }

 send_message:
    out_tlvs_size = BufferGetUsed(&out_tlvs_buf);
    if (out_tlvs_size == 0) {
	EAPLOG_FL(LOG_NOTICE, "nothing to send?");
	goto done;
    }
    context->in_message_size = 0;
    if (plugin->log_enabled) {
	CFMutableStringRef	str;

	str = CFStringCreateMutable(NULL, 0);
	STRING_APPEND(str, "======== Send TLVs: ========\n");
	(void)TLVListParse(NULL, BufferGetPtr(&out_tlvs_buf),
			   out_tlvs_size, str);
	EAPLOG(-LOG_DEBUG, "%@", str);
	my_CFRelease(&str);
    }
    status = SSLWrite(context->ssl_context, BufferGetPtr(&out_tlvs_buf),
		      out_tlvs_size, &out_tlvs_size);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, 
		  "SSLWrite failed, %s (%d)",
		  EAPSSLErrorString(status), (int)status);
    }
    else {
	ret = TRUE;
    }
    /* close the tunnel if we know we've failed */
    if (context->plugin_state == kEAPClientStateFailure) {
	/* if this is a fatal error, close the SSL tunnel */
	SSLClose(context->ssl_context);
    }

 done:

    return (ret);
}

STATIC EAPPacketRef
eapfast_verify_server(EAPClientPluginDataRef plugin,
		      int identifier, EAPClientStatus * client_status)
{
    EAPFASTPluginDataRef context = (EAPFASTPluginDataRef)plugin->private;
    EAPPacketRef	pkt = NULL;
    memoryBufferRef	write_buf = &context->write_buffer;

    /* if we used the PAC, we don't need to verify the server */
    if (context->pac_was_used == TRUE) {
	context->trust_proceed = TRUE;
	return (NULL);
    }
    if (context->pac_dict == NULL
	&& context->provision_pac_anonymously) {
	SSLCipherSuite		cipher;

	if (SSLGetNegotiatedCipher(context->ssl_context, &cipher) == noErr
	    && cipher == TLS_DH_anon_WITH_AES_128_CBC_SHA) {
	    context->trust_proceed = TRUE;
	    context->must_use_mschapv2 = TRUE;
	    return (NULL);
	}
    }

    context->trust_status
	= EAPTLSVerifyServerCertificateChain(plugin->properties, 
					     context->server_certs,
					     &context->trust_ssl_error);
    if (context->trust_status != kEAPClientStatusOK) {
	EAPLOG_FL(LOG_NOTICE, "server certificate not trusted, status %d %d",
		  context->trust_status,
		  (int)context->trust_ssl_error);
    }
    switch (context->trust_status) {
    case kEAPClientStatusOK:
	context->trust_proceed = TRUE;
	break;
    case kEAPClientStatusUserInputRequired:
	/* ask user whether to proceed or not */
	*client_status = context->last_client_status 
	    = kEAPClientStatusUserInputRequired;
	break;
    default:
	*client_status = context->last_client_status = context->trust_status;
	context->last_ssl_error = context->trust_ssl_error;
	context->plugin_state = kEAPClientStateFailure;
	SSLClose(context->ssl_context);
	pkt = EAPTLSPacketCreate(kEAPCodeResponse,
				 kEAPTypeEAPFAST,
				 identifier,
				 context->mtu,
				 write_buf,
				 &context->last_write_size);
	break;
    }
    return (pkt);
}

STATIC EAPPacketRef
eapfast_tunnel(EAPClientPluginDataRef plugin, EAPTLSPacketRef eaptls_in,
	       EAPClientStatus * client_status)
{
    EAPFASTPluginDataRef context = (EAPFASTPluginDataRef)plugin->private;
    EAPPacketRef	pkt = NULL;
    memoryBufferRef	write_buf = &context->write_buffer; 

    if (eapfast_eap(plugin, eaptls_in, client_status)) {
	pkt = EAPTLSPacketCreate2(kEAPCodeResponse,
				  kEAPTypeEAPFAST,
				  eaptls_in->identifier,
				  context->mtu,
				  write_buf,
				  &context->last_write_size,
				  FALSE);
    }
    return (pkt);
}

STATIC void
eapfast_set_session_was_resumed(EAPFASTPluginDataRef context)
{
    char		buf[MAX_SESSION_ID_LENGTH];
    size_t		buf_len = sizeof(buf);
    Boolean		resumed = FALSE;
    OSStatus		status;

    status = SSLGetResumableSessionInfo(context->ssl_context,
					&resumed, buf, &buf_len);
    if (status == noErr) {
	context->session_was_resumed = resumed;
    }
    return;
}

STATIC EAPPacketRef
eapfast_handshake(EAPClientPluginDataRef plugin, EAPTLSPacketRef eaptls_in,
		  EAPClientStatus * client_status)
{
    EAPFASTPluginDataRef context = (EAPFASTPluginDataRef)plugin->private;
    EAPPacketRef	eaptls_out = NULL;
    OSStatus		status = noErr;
    memoryBufferRef	write_buf = &context->write_buffer;

    if (context->server_auth_completed && context->trust_proceed == FALSE) {
	eaptls_out = eapfast_verify_server(plugin, eaptls_in->identifier, 
					   client_status);
	if (context->trust_proceed == FALSE) {
	    goto done;
	}
    }
    status = SSLHandshake(context->ssl_context);
    if (status == errSSLServerAuthCompleted) {
	if (context->server_auth_completed) {
	    /* this should not happen */
	    EAPLOG_FL(LOG_NOTICE, "AuthCompleted again?");
	    goto done;
	}
	context->server_auth_completed = TRUE;
	my_CFRelease(&context->server_certs);
	(void)EAPSSLCopyPeerCertificates(context->ssl_context,
					 &context->server_certs);
	eaptls_out = eapfast_verify_server(plugin, eaptls_in->identifier,
					   client_status);
	if (context->trust_proceed == FALSE) {
	    goto done;
	}
	/* handshake again to get past the AuthCompleted status */
	status = SSLHandshake(context->ssl_context);
    }
    switch (status) {
    case noErr:
	/* handshake complete, tunnel established */
	if (context->trust_proceed == FALSE) {
	    my_CFRelease(&context->server_certs);
	    (void)EAPSSLCopyPeerCertificates(context->ssl_context,
					     &context->server_certs);
	    eaptls_out = eapfast_verify_server(plugin, eaptls_in->identifier,
					       client_status);
	    if (context->trust_proceed == FALSE) {
		/* this should not happen */
		EAPLOG_FL(LOG_NOTICE, "trust_proceed is FALSE?");
		break;
	    }
	}
	status
	    = eapfast_generate_key_material(context,
					    context->last_s_imck,
					    context->mschap_server_challenge,
					    context->mschap_client_challenge);
	if (status != noErr) {
	    goto close_up_shop;
	}
	context->handshake_complete = TRUE;
	eapfast_set_session_was_resumed(context);

	/* kick off authentication */
	if (eapfast_eap(plugin, eaptls_in, client_status)) {
	    eaptls_out = EAPTLSPacketCreate2(kEAPCodeResponse,
					     kEAPTypeEAPFAST,
					     eaptls_in->identifier,
					     context->mtu,
					     write_buf,
					     &context->last_write_size,
					     FALSE);
	}
	break;
    default:
	EAPLOG_FL(LOG_NOTICE, "SSLHandshake failed, %s (%d)",
		  EAPSSLErrorString(status), (int)status);
    close_up_shop:
	context->last_ssl_error = status;
	my_CFRelease(&context->server_certs);
	(void) EAPSSLCopyPeerCertificates(context->ssl_context,
					  &context->server_certs);
	/* close_up_shop */
	context->plugin_state = kEAPClientStateFailure;
	SSLClose(context->ssl_context);
	if (status == errSSLPeerBadCert && context->pac_dict) {
	    /* our PAC is no good, remove it */
	    remove_pac(context->pac_dict,
		       plugin->username, plugin->username_length);
	}
	/* FALL THROUGH */
    case errSSLWouldBlock:
	if (write_buf->data == NULL) {
	    /* send an ACK */
	    eaptls_out = EAPFASTPacketCreateAck(eaptls_in->identifier);
	}
	else {
	    eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					    kEAPTypeEAPFAST,
					    eaptls_in->identifier,
					    context->mtu,
					    write_buf,
					    &context->last_write_size);
	}
	break;
    }

 done:
    return (eaptls_out);
}


STATIC EAPPacketRef
eapfast_request(EAPClientPluginDataRef plugin,
		const EAPPacketRef in_pkt, EAPClientStatus * client_status)
{
    EAPFASTPluginDataRef context = (EAPFASTPluginDataRef)plugin->private;
    EAPTLSPacketRef 	eaptls_in = (EAPTLSPacketRef)in_pkt; 
    EAPTLSLengthIncludedPacketRef eaptls_in_l;
    EAPPacketRef	eaptls_out = NULL;
    int			in_data_length;
    void *		in_data_ptr = NULL;
    u_int16_t		in_length = EAPPacketGetLength(in_pkt);
    memoryBufferRef	read_buf = &context->read_buffer;
    SSLSessionState	ssl_state = kSSLIdle;
    OSStatus		status = noErr;
    u_int32_t		tls_message_length = 0;
    RequestType		type;
    memoryBufferRef	write_buf = &context->write_buffer; 

    /* ALIGN: void * cast OK, we don't expect proper alignment */
    eaptls_in_l = (EAPTLSLengthIncludedPacketRef)(void *)in_pkt;
    if (in_length < sizeof(*eaptls_in)) {
	EAPLOG_FL(LOG_NOTICE, "length %d < %ld", in_length, sizeof(*eaptls_in));
	goto done;
    }
    if (context->ssl_context != NULL) {
	status = SSLGetSessionState(context->ssl_context, &ssl_state);
	if (status != noErr) {
	    EAPLOG_FL(LOG_NOTICE, "SSLGetSessionState failed, %s",
		      EAPSSLErrorString(status));
	    context->plugin_state = kEAPClientStateFailure;
	    context->last_ssl_error = status;
	    goto done;
	}
    }
    in_data_ptr = eaptls_in->tls_data;
    tls_message_length = in_data_length = in_length - sizeof(EAPTLSPacket);

    type = kRequestTypeData;
    if ((eaptls_in->flags & kEAPTLSPacketFlagsStart) != 0) {
	type = kRequestTypeStart;
	/* only reset our state if this is not a re-transmitted Start packet */
	if (ssl_state != kSSLHandshake
	    || write_buf->data == NULL
	    || in_pkt->identifier != context->previous_identifier) {
	    ssl_state = kSSLIdle;
	}
    }
    else if (in_length == sizeof(*eaptls_in)) {
	type = kRequestTypeAck;
    }
    else if ((eaptls_in->flags & kEAPTLSPacketFlagsLengthIncluded) != 0) {
	if (in_length < sizeof(EAPTLSLengthIncludedPacket)) {
	    EAPLOG_FL(LOG_NOTICE, 
		      "packet too short %d < %ld",
		      in_length, sizeof(EAPTLSLengthIncludedPacket));
	    goto done;
	}
	tls_message_length 
	    = EAPTLSLengthIncludedPacketGetMessageLength(eaptls_in_l);
	if (tls_message_length > kEAPTLSAvoidDenialOfServiceSize) {
	    EAPLOG_FL(LOG_NOTICE, 
		      "received message too large, %d > %d",
		      tls_message_length, (int)kEAPTLSAvoidDenialOfServiceSize);
	    goto done;
	}
	in_data_ptr = eaptls_in_l->tls_data;
	in_data_length = in_length - sizeof(EAPTLSLengthIncludedPacket);
	if (tls_message_length == 0) {
	    type = kRequestTypeAck;
	}
    }

    switch (ssl_state) {
    case kSSLClosed:
    case kSSLAborted:
	break;

    case kSSLIdle:
	if (type != kRequestTypeStart) {
	    /* ignore it: XXX should this be an error? */
	    EAPLOG_FL(LOG_NOTICE, "ignoring non EAP-FAST start frame");
	    goto done;
	}
	status = eapfast_start(plugin, in_data_ptr, in_data_length);
	if (status != noErr) {
	    context->last_ssl_error = status;
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
	status = SSLHandshake(context->ssl_context);
	if (status != errSSLWouldBlock) {
	    EAPLOG_FL(LOG_NOTICE, "SSLHandshake failed, %s (%d)",
		      EAPSSLErrorString(status), (int)status);
	    context->last_ssl_error = status;
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
	eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					kEAPTypeEAPFAST,
					eaptls_in->identifier,
					context->mtu,
					write_buf,
					&context->last_write_size);
	break;
    case kSSLHandshake:
    case kSSLConnected:
	if (write_buf->data != NULL) {
	    /* we have data to write */
	    if (in_pkt->identifier == context->previous_identifier) {
		/* resend the existing fragment */
		eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
						kEAPTypeEAPFAST,
						in_pkt->identifier,
						context->mtu,
						write_buf,
						&context->last_write_size);
		break;
	    }
	    if ((write_buf->offset + context->last_write_size)
		< write_buf->length) {
		/* advance the offset, and send the next fragment */
		write_buf->offset += context->last_write_size;
		eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
						kEAPTypeEAPFAST,
						in_pkt->identifier,
						context->mtu,
						write_buf,
						&context->last_write_size);
		break;
	    }
	    /* we're done, release the write buffer */
	    memoryBufferClear(write_buf);
	    context->last_write_size = 0;
	}
	if (type != kRequestTypeData) {
	    EAPLOG_FL(LOG_NOTICE, "unexpected %s frame",
		      type == kRequestTypeAck ? "Ack" : "Start");
	    goto done;
	}
	if (in_pkt->identifier == context->previous_identifier) {
	    if ((eaptls_in->flags & kEAPTLSPacketFlagsMoreFragments) != 0) {
		/* just ack it, we've already seen the fragment */
		eaptls_out = EAPFASTPacketCreateAck(eaptls_in->identifier);
		break;
	    }
	}
	else {
	    if (read_buf->data == NULL) {
		memoryBufferAllocate(read_buf, tls_message_length);
	    }
	    if (memoryBufferAddData(read_buf, in_data_ptr, in_data_length)
		== FALSE) {
		EAPLOG_FL(LOG_NOTICE, 
			  "fragment too large %d",
			  in_data_length);
		goto done;
	    }
	    if (memoryBufferIsComplete(read_buf) == FALSE) {
		if ((eaptls_in->flags & kEAPTLSPacketFlagsMoreFragments) == 0) {
		    EAPLOG_FL(LOG_NOTICE, 
			      "expecting more data but "
			      "more fragments bit is not set, ignoring");
		}
		/* we haven't received the entire TLS message */
		eaptls_out = EAPFASTPacketCreateAck(eaptls_in->identifier);
		break;
	    }
	}
	/* we've got the whole TLS message, process it */
	if (context->handshake_complete) {
	    /* subsequent request */
	    eaptls_out = eapfast_tunnel(plugin, eaptls_in,
					client_status);
	}
	else {
	    eaptls_out = eapfast_handshake(plugin, eaptls_in,
					   client_status);
	}
	break;
    default:
	break;
    }

    context->previous_identifier = in_pkt->identifier;
    if (context->eapfast_version_set == FALSE) {
	uint8_t	eapfast_version;

	context->eapfast_received_version = 
	    eapfast_version = EAPFASTPacketFlagsVersion(eaptls_in_l->flags);
	if (eapfast_version != kEAPFASTVersion1) {
	    eapfast_version = kEAPFASTVersion1;
	}
	context->eapfast_version = eapfast_version;
    }
    if (eaptls_out != NULL) {
	EAPFASTPacketFlagsSetVersion((EAPTLSPacketRef)eaptls_out,
				     context->eapfast_version);
    }

 done:
    return (eaptls_out);
}

STATIC EAPClientState
eapfast_process(EAPClientPluginDataRef plugin, 
		const EAPPacketRef in_pkt,
		EAPPacketRef * out_pkt_p, 
		EAPClientStatus * client_status,
		EAPClientDomainSpecificError * error)
{
    EAPFASTPluginDataRef context = (EAPFASTPluginDataRef)plugin->private;

    *client_status = kEAPClientStatusOK;
    *error = 0;
    *out_pkt_p = NULL;
    switch (in_pkt->code) {
    case kEAPCodeRequest:
	*out_pkt_p = eapfast_request(plugin, in_pkt, client_status);
	break;
    case kEAPCodeSuccess:
	if (context->inner_auth_state == kEAPFASTInnerAuthStateSuccess) {
	    context->plugin_state = kEAPClientStateSuccess;
	}
	break;
    case kEAPCodeFailure:
	if (context->inner_auth_state == kEAPFASTInnerAuthStateFailure
	    || context->pac_was_provisioned == FALSE) {
	    context->plugin_state = kEAPClientStateFailure;
	}
	break;
    case kEAPCodeResponse:
    default:
	break;
    }
    if (context->plugin_state == kEAPClientStateFailure) {
	if (context->last_ssl_error == noErr) {
	    switch (context->last_client_status) {
	    case kEAPClientStatusOK:
	    case kEAPClientStatusUserInputRequired:
		*client_status = kEAPClientStatusFailed;
		break;
	    default:
		*client_status = context->last_client_status;
		break;
	    }
	}
	else {
	    *error = context->last_ssl_error;
	    *client_status = kEAPClientStatusSecurityError;
	}
    }
    return (context->plugin_state);
}

STATIC const char * 
eapfast_failure_string(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

STATIC void * 
eapfast_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPFASTPluginDataRef	context = (EAPFASTPluginDataRef)plugin->private;

    *key_length = 0;
    if (context->master_key_valid == FALSE) {
	return (NULL);
    }
    *key_length = MASTER_KEY_LENGTH / 2;
    return (context->master_key);
}

STATIC void * 
eapfast_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPFASTPluginDataRef	context = (EAPFASTPluginDataRef)plugin->private;

    *key_length = 0;
    if (context->master_key_valid == FALSE) {
	return (NULL);
    }
    *key_length = MASTER_KEY_LENGTH / 2;
    return (context->master_key + (MASTER_KEY_LENGTH / 2));
}

STATIC void
dictInsertEAPTypeInfo(CFMutableDictionaryRef dict, EAPType type,
		      const char * type_name)
{
    CFNumberRef			eap_type_cf;
    int				eap_type = type;

    if (type == kEAPTypeInvalid) {
	return;
    }

    /* EAPTypeName */
    if (type_name != NULL) {
	CFStringRef		eap_type_name_cf;
	eap_type_name_cf 
	    = CFStringCreateWithCString(NULL, type_name, 
					kCFStringEncodingASCII);
	CFDictionarySetValue(dict, kEAPClientInnerEAPTypeName, 
			     eap_type_name_cf);
	my_CFRelease(&eap_type_name_cf);
    }
    /* EAPType */
    eap_type_cf = CFNumberCreate(NULL, kCFNumberIntType, &eap_type);
    CFDictionarySetValue(dict, kEAPClientInnerEAPType, eap_type_cf);
    my_CFRelease(&eap_type_cf);

    return;
}

STATIC CFDictionaryRef
eapfast_publish_props_copy(EAPClientPluginDataRef plugin)
{
    CFArrayRef			cert_list = NULL;
    SSLCipherSuite		cipher = SSL_NULL_WITH_NULL_NULL;
    EAPFASTPluginDataRef	context = (EAPFASTPluginDataRef)plugin->private;
    CFMutableDictionaryRef	dict;

    if (context->server_certs != NULL) {
	cert_list
	    = EAPSecCertificateArrayCreateCFDataArray(context->server_certs);
	if (cert_list == NULL) {
	    return (NULL);
	}
    }
    if (context->handshake_complete && context->eap.publish_props != NULL) {
	dict = CFDictionaryCreateMutableCopy(NULL, 0, 
					     context->eap.publish_props);
    }
    else {
	dict = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
    }
    if (cert_list != NULL) {
	CFDictionarySetValue(dict, kEAPClientPropTLSServerCertificateChain,
			     cert_list);
	CFRelease(cert_list);
    }
    if (context->ssl_context != NULL) {
	(void)SSLGetNegotiatedCipher(context->ssl_context, &cipher);
	if (cipher != SSL_NULL_WITH_NULL_NULL) {
	    CFNumberRef	c;
	    int		tmp = cipher;
	    
	    c = CFNumberCreate(NULL, kCFNumberIntType, &tmp);
	    CFDictionarySetValue(dict, kEAPClientPropTLSNegotiatedCipher, c);
	    CFRelease(c);
	}
    }
    if (context->pac_was_provisioned) {
	CFDictionarySetValue(dict, kEAPClientPropEAPFASTPACWasProvisioned,
			     kCFBooleanTrue);
    }
    CFDictionarySetValue(dict, kEAPClientPropTLSSessionWasResumed,
			 context->session_was_resumed 
			 ? kCFBooleanTrue
			 : kCFBooleanFalse);
    if (context->eap.module != NULL) {
	dictInsertEAPTypeInfo(dict, context->eap.last_type,
			      context->eap.last_type_name);
    }
    if (context->last_client_status == kEAPClientStatusUserInputRequired
	&& context->trust_proceed == FALSE) {
	CFNumberRef	num;
	num = CFNumberCreate(NULL, kCFNumberSInt32Type,
			     &context->trust_status);
	CFDictionarySetValue(dict, kEAPClientPropTLSTrustClientStatus, num);
	CFRelease(num);
    }
    return (dict);
}

STATIC CFArrayRef
eapfast_require_props(EAPClientPluginDataRef plugin)
{
    CFArrayRef		array = NULL;
    EAPFASTPluginDataRef	context = (EAPFASTPluginDataRef)plugin->private;

    if (context->last_client_status != kEAPClientStatusUserInputRequired) {
	goto done;
    }
    if (context->trust_proceed == FALSE) {
	CFStringRef	str = kEAPClientPropTLSUserTrustProceedCertificateChain;
	array = CFArrayCreate(NULL, (const void **)&str,
			      1, &kCFTypeArrayCallBacks);
    }
    else if (context->handshake_complete) {
	if (context->eap.require_props != NULL) {
	    array = CFRetain(context->eap.require_props);
	}
    }
 done:
    return (array);
}

STATIC CFStringRef
eapfast_copy_packet_description(const EAPPacketRef pkt, bool * packet_is_valid)
{
    EAPTLSPacketRef 	eaptls_pkt = (EAPTLSPacketRef)pkt;
    EAPTLSLengthIncludedPacketRef eaptls_pkt_l;
    int			data_length;
    void *		data_ptr = NULL;
    u_int16_t		length = EAPPacketGetLength(pkt);
    CFMutableStringRef	str;
    u_int32_t		tls_message_length = 0;

    *packet_is_valid = FALSE;
    switch (pkt->code) {
    case kEAPCodeRequest:
    case kEAPCodeResponse:
	break;
    default:
	return (NULL);
    }
    str = CFStringCreateMutable(NULL, 0);
    if (length < sizeof(*eaptls_pkt)) {
	STRING_APPEND(str, "EAPTLSPacket header truncated %d < %d\n",
		      length, (int)sizeof(*eaptls_pkt));
	goto done;
    }
    STRING_APPEND(str, "EAP-FAST Version %d %s: "
		  "Identifier %d Length %d Flags 0x%x%s",
		  EAPFASTPacketFlagsVersion(eaptls_pkt->flags),
		  pkt->code == kEAPCodeRequest ? "Request" : "Response",
		  pkt->identifier, length, eaptls_pkt->flags,
		  (EAPFASTPacketFlagsFlags(eaptls_pkt->flags) != 0)
		  ? " [" : "");

    /* ALIGN: void * cast OK, we don't expect proper alignment */ 
    eaptls_pkt_l = (EAPTLSLengthIncludedPacketRef)(void *)pkt;
    
    data_ptr = eaptls_pkt->tls_data;
    tls_message_length = data_length = length - sizeof(EAPTLSPacket);

    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsStart) != 0) {
	STRING_APPEND(str, " start");
    }
    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsLengthIncluded) != 0) {
	if (length >= sizeof(EAPTLSLengthIncludedPacket)) {
	    data_ptr = eaptls_pkt_l->tls_data;
	    data_length = length - sizeof(EAPTLSLengthIncludedPacket);
	    tls_message_length 
		= EAPTLSLengthIncludedPacketGetMessageLength(eaptls_pkt_l);
	    STRING_APPEND(str, " length=%u", tls_message_length);
	}
    }
    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsMoreFragments) != 0) {
	STRING_APPEND(str, " more");
    }
    STRING_APPEND(str, "%s Data Length %d\n", 
		  EAPFASTPacketFlagsFlags(eaptls_pkt->flags) != 0 ? " ]" : "",
		  data_length);
    if (tls_message_length > kEAPTLSAvoidDenialOfServiceSize) {
	STRING_APPEND(str, "potential DOS attack %u > %d\n",
		      tls_message_length, kEAPTLSAvoidDenialOfServiceSize);
	STRING_APPEND(str, "bogus EAP Packet:\n");
	print_data_cfstr(str, (void *)pkt, length);
	goto done;
    }
    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsStart) != 0
	&& data_length >= sizeof(AuthorityIDData)
	&& (AuthorityIDDataGetType((AuthorityIDDataRef)data_ptr)
	    == kAuthorityIDDataType)) {
	AuthorityIDDataRef	auid = (AuthorityIDDataRef)data_ptr;
	uint16_t		auid_length;
	
	auid_length = AuthorityIDDataGetLength(auid);
	STRING_APPEND(str, "Authority ID Data Length %d ID ", auid_length);
	if (auid_length > (data_length - offsetof(AuthorityIDData, auid_id))) {
	    auid_length = data_length - offsetof(AuthorityIDData, auid_id);
	    STRING_APPEND(str, "> available %d! ", auid_length);
	}
	print_bytes_cfstr(str, auid->auid_id, auid_length);
	STRING_APPEND(str, "\n");
    }
    else {
	print_data_cfstr(str, data_ptr, data_length);
    }
    *packet_is_valid = TRUE;

 done:
    return (str);
}

STATIC EAPType 
eapfast_type()
{
    return (kEAPTypeEAPFAST);

}

STATIC const char *
eapfast_name()
{
    return (EAP_FAST_NAME);

}

STATIC EAPClientPluginVersion 
eapfast_version()
{
    return (kEAPClientPluginVersion);
}

STATIC struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
#if 0
    { kEAPClientPluginFuncNameIntrospect, eapfast_introspect },
#endif /* 0 */
    { kEAPClientPluginFuncNameVersion, eapfast_version },
    { kEAPClientPluginFuncNameEAPType, eapfast_type },
    { kEAPClientPluginFuncNameEAPName, eapfast_name },
    { kEAPClientPluginFuncNameInit, eapfast_init },
    { kEAPClientPluginFuncNameFree, eapfast_free },
    { kEAPClientPluginFuncNameProcess, eapfast_process },
    { kEAPClientPluginFuncNameFreePacket, eapfast_free_packet },
    { kEAPClientPluginFuncNameFailureString, eapfast_failure_string },
    { kEAPClientPluginFuncNameSessionKey, eapfast_session_key },
    { kEAPClientPluginFuncNameServerKey, eapfast_server_key },
    { kEAPClientPluginFuncNameRequireProperties, eapfast_require_props },
    { kEAPClientPluginFuncNamePublishProperties, eapfast_publish_props_copy },
    { kEAPClientPluginFuncNameCopyPacketDescription,
      eapfast_copy_packet_description },
    { NULL, NULL},
};


EAPClientPluginFuncRef
eapfast_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;


    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}
