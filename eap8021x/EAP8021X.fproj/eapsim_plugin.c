/*
 * Copyright (c) 2008-2009 Apple Inc. All rights reserved.
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
 * eapsim_plugin.c
 * - EAP-SIM client
 */

/* 
 * Modification History
 *
 * December 8, 2008	Dieter Siegmund (dieter@apple)
 * - created
 */
#include "EAPClientProperties.h"
#include <EAP8021X/EAPClientPlugin.h>
#include <EAP8021X/EAPClientProperties.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <SystemConfiguration/SCValidation.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/param.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#include <sys/param.h>
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPClientModule.h>
#include <TargetConditionals.h>
#include "myCFUtil.h"
#include "printdata.h"
#include "fips186prf.h"
#include "SIMAccess.h"

#define INLINE 	static __inline__
#define STATIC	static

#define EAP_SIM_NAME		"EAP-SIM"
#define EAP_SIM_NAME_LENGTH	(sizeof(EAP_SIM_NAME) - 1)

#define kEAPClientPropEAPSIMIMSI		CFSTR("EAPSIMIMSI")
#define kEAPClientPropEAPSIMRealm		CFSTR("EAPSIMRealm")
#define kEAPClientPropEAPSIMNumberOfRANDs	CFSTR("EAPSIMNumberOfRANDs") /* number 2 or 3, default 3 */

/*
 * kEAPClientPropEAPSIMKcList
 * kEAPClientPropEAPSIMSRESList
 * kEAPClientPropEAPSIMRANDList
 * - static (Kc, SRES, RAND) triplets used for testing
 * - these are parallel arrays and must all be defined and have exactly the
 *   same number of elements of the correct size and type
 */

#define kEAPClientPropEAPSIMKcList		CFSTR("EAPSIMKcList") /* array[data] */
#define kEAPClientPropEAPSIMSRESList		CFSTR("EAPSIMSRESList") /* array[data] */
#define kEAPClientPropEAPSIMRANDList		CFSTR("EAPSIMSRANDList") /* array[data] */

/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect eapsim_introspect;
STATIC EAPClientPluginFuncVersion eapsim_version;
STATIC EAPClientPluginFuncEAPType eapsim_type;
STATIC EAPClientPluginFuncEAPName eapsim_name;
STATIC EAPClientPluginFuncInit eapsim_init;
STATIC EAPClientPluginFuncFree eapsim_free;
STATIC EAPClientPluginFuncProcess eapsim_process;
STATIC EAPClientPluginFuncFreePacket eapsim_free_packet;
STATIC EAPClientPluginFuncSessionKey eapsim_session_key;
STATIC EAPClientPluginFuncServerKey eapsim_server_key;
STATIC EAPClientPluginFuncPublishProperties eapsim_publish_props;
STATIC EAPClientPluginFuncPacketDump eapsim_packet_dump;
STATIC EAPClientPluginFuncUserName eapsim_user_name;
STATIC EAPClientPluginFuncCopyIdentity eapsim_copy_identity;

/**
 ** Protocol-specific defines
 **/

#define EAPSIM_K_ENCR_SIZE	16		/* used with AT_ENCR_DATA */
#define EAPSIM_K_AUT_SIZE	16		/* used with AT_MAC */
#define EAPSIM_MSK_SIZE		64
#define EAPSIM_EMSK_SIZE	64
#define EAPSIM_KEY_SIZE		(EAPSIM_K_ENCR_SIZE + EAPSIM_K_AUT_SIZE \
				 + EAPSIM_MSK_SIZE + EAPSIM_EMSK_SIZE)

typedef union {
    struct {
	uint8_t	k_encr[EAPSIM_K_ENCR_SIZE];
	uint8_t	k_aut[EAPSIM_K_AUT_SIZE];
	uint8_t	msk[EAPSIM_MSK_SIZE];
	uint8_t	emsk[EAPSIM_EMSK_SIZE];
    } s;
    uint8_t	key[EAPSIM_KEY_SIZE];
} EAPSIMKeyInfo, *EAPSIMKeyInfoRef;


#define EAPSIM_MAX_RANDS	3

typedef struct {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		subtype;
    uint8_t		reserved[2];
    uint8_t		attrs[1];
} EAPSIMPacket, *EAPSIMPacketRef;

#define EAPSIM_HEADER_LENGTH		offsetof(EAPSIMPacket, attrs)

/* Subtypes: */
enum {
    kEAPSIMSubtypeStart = 10,
    kEAPSIMSubtypeChallenge = 11,
    kEAPSIMSubtypeNotification = 12,
    kEAPSIMSubtypeReauthentication = 13,
    kEAPSIMSubtypeClientError = 14
};
typedef uint8_t	EAPSIMSubtype;

STATIC const char *
EAPSIMSubtypeString(EAPSIMSubtype subtype)
{
    static char		buf[8];

    switch (subtype) {
    case kEAPSIMSubtypeStart:
	return ("Start");
    case kEAPSIMSubtypeChallenge:
	return ("Challenge");
    case kEAPSIMSubtypeNotification:
	return ("Notification");
    case kEAPSIMSubtypeReauthentication:
	return ("Reauthentication");
    case kEAPSIMSubtypeClientError:
	return ("ClientError");
    default:
	snprintf(buf, sizeof(buf), "%d", subtype);
	return (buf);
    }
}

/* Attributes: */
enum {
    kAT_RAND = 1,
    kAT_AUTN = 2,
    kAT_RES = 3,
    kAT_AUTS = 4,
    kAT_PADDING = 6,
    kAT_NONCE_MT = 7,
    kAT_PERMANENT_ID_REQ = 10,
    kAT_MAC = 11,
    kAT_NOTIFICATION = 12,
    kAT_ANY_ID_REQ = 13,
    kAT_IDENTITY = 14,
    kAT_VERSION_LIST = 15,
    kAT_SELECTED_VERSION = 16,
    kAT_FULLAUTH_ID_REQ = 17,
    kAT_COUNTER = 19,
    kAT_COUNTER_TOO_SMALL = 20,
    kAT_NONCE_S = 21,
    kAT_CLIENT_ERROR_CODE = 22,
    kEAPSIM_TLV_SKIPPABLE_RANGE_START = 128,
    kAT_IV = 129,
    kAT_ENCR_DATA = 130,
    kAT_NEXT_PSEUDONYM = 132,
    kAT_NEXT_REAUTH_ID = 133,
    kAT_CHECKCODE = 134,
    kAT_RESULT_IND = 135
};
typedef uint8_t		EAPSIMAttributeType;

STATIC const char *
EAPSIMAttributeTypeString(EAPSIMAttributeType attr)
{
    static char		buf[8];

    switch (attr) {
    case kAT_RAND:
	return "AT_RAND";
    case kAT_AUTN:
	return "AT_AUTN";
    case kAT_RES:
	return "AT_RES";
    case kAT_AUTS:
	return "AT_AUTS";
    case kAT_PADDING:
	return "AT_PADDING";
    case kAT_NONCE_MT:
	return "AT_NONCE_MT";
    case kAT_PERMANENT_ID_REQ:
	return "AT_PERMANENT_ID_REQ";
    case kAT_MAC:
	return "AT_MAC";
    case kAT_NOTIFICATION:
	return "AT_NOTIFICATION";
    case kAT_ANY_ID_REQ:
	return "AT_ANY_ID_REQ";
    case kAT_IDENTITY:
	return "AT_IDENTITY";
    case kAT_VERSION_LIST:
	return "AT_VERSION_LIST";
    case kAT_SELECTED_VERSION:
	return "AT_SELECTED_VERSION";
    case kAT_FULLAUTH_ID_REQ:
	return "AT_FULLAUTH_ID_REQ";
    case kAT_COUNTER:
	return "AT_COUNTER";
    case kAT_COUNTER_TOO_SMALL:
	return "AT_COUNTER_TOO_SMALL";
    case kAT_NONCE_S:
	return "AT_NONCE_S";
    case kAT_CLIENT_ERROR_CODE:
	return "AT_CLIENT_ERROR_CODE";
    case kAT_IV:
	return "AT_IV";
    case kAT_ENCR_DATA:
	return "AT_ENCR_DATA";
    case kAT_NEXT_PSEUDONYM:
	return "AT_NEXT_PSEUDONYM";
    case kAT_NEXT_REAUTH_ID:
	return "AT_NEXT_REAUTH_ID";
    case kAT_CHECKCODE:
	return "AT_CHECKCODE";
    case kAT_RESULT_IND:
	return "AT_RESULT_IND";
    default:
	snprintf(buf, sizeof(buf), "%d", attr);
	return (buf);
    }
}

#define ENCR_DATA_ALIGNMENT	16
#define ENCR_DATA_ROUNDUP(size)	roundup((size), ENCR_DATA_ALIGNMENT)

#define TLV_ALIGNMENT		4
#define TLV_MAX_LENGTH		(UINT8_MAX * TLV_ALIGNMENT)

typedef struct TLV_s {
    uint8_t		tlv_type;
    uint8_t		tlv_length;
    uint8_t		tlv_value[1];
} TLV, * TLVRef;
#define TLV_HEADER_LENGTH		offsetof(TLV, tlv_value)

typedef struct AT_VERSION_LIST_s {
    uint8_t		vl_type;
    uint8_t		vl_length;
    uint8_t		vl_actual_length[2];
    uint8_t		vl_version_list[1];
} AT_VERSION_LIST;

/* EAP-SIM version number */
enum {
    kEAPSIMVersion1 = 1
};

typedef struct AT_SELECTED_VERSION_s {
    uint8_t		sv_type;
    uint8_t		sv_length;	/* = 1 */
    uint8_t		sv_selected_version[2];
} AT_SELECTED_VERSION;

#define NONCE_MT_SIZE	16
typedef struct AT_NONCE_MT_s {
    uint8_t		nm_type;
    uint8_t		nm_length;	/* = 5 */
    uint8_t		nm_reserved[2];
    uint8_t		nm_nonce_mt[NONCE_MT_SIZE];
} AT_NONCE_MT;

typedef struct AT_PERMANENT_ID_REQ_s {
    uint8_t		pi_type;
    uint8_t		pi_length;	/* = 1 */
    uint8_t		pi_reserved[2];
} AT_PERMANENT_ID_REQ;

typedef struct AT_ANY_ID_REQ_s {
    uint8_t		ai_type;
    uint8_t		ai_length;	/* = 1 */
    uint8_t		ai_reserved[2];
} AT_ANY_ID_REQ;

typedef struct AT_FULLAUTH_ID_REQ_s {
    uint8_t		fi_type;
    uint8_t		fi_length;	/* = 1 */
    uint8_t		fi_reserved[2];
} AT_FULLAUTH_ID_REQ;

typedef struct AT_IDENTITY_s {
    uint8_t		id_type;
    uint8_t		id_length;
    uint8_t		id_actual_length[2];
    uint8_t		id_identity[1];
} AT_IDENTITY;

typedef struct AT_RAND_s {
    uint8_t		ra_type;
    uint8_t		ra_length;
    uint8_t		ra_reserved[2];
    uint8_t		ra_rand[1];
} AT_RAND;

typedef struct AT_NEXT_PSEUDONYM_s {
    uint8_t		np_type;
    uint8_t		np_length;
    uint8_t		np_actual_length[2];
    uint8_t		np_next_pseudonym[1];
} AT_NEXT_PSEUDONYM;

typedef struct AT_NEXT_REAUTH_ID_s {
    uint8_t		nr_type;
    uint8_t		nr_length;
    uint8_t		nr_actual_length[2];
    uint8_t		nr_next_reauth_id[1];
} AT_NEXT_REAUTH_ID;

#define IV_SIZE		16
typedef struct AT_IV_s {
    uint8_t		iv_type;
    uint8_t		iv_length;	/* = 5 */
    uint8_t		iv_reserved[2];
    uint8_t		iv_initialization_vector[IV_SIZE];
} AT_IV;

typedef struct AT_ENCR_DATA_s {
    uint8_t		ed_type;
    uint8_t		ed_length;
    uint8_t		ed_reserved[2];
    uint8_t		ed_encrypted_data[1];
} AT_ENCR_DATA;

typedef struct AT_PADDING_s {
    uint8_t		pa_type;
    uint8_t		pa_length;	/* = { 1, 2, 3 } */
    uint8_t		pa_padding[1];
} AT_PADDING;

typedef struct AT_RESULT_IND_s {
    uint8_t		ri_type;
    uint8_t		ri_length;	/* = 1 */
    uint8_t		ri_reserved[2];
} AT_RESULT_IND;

#define MAC_SIZE	16
typedef struct AT_MAC_s {
    uint8_t		ma_type;
    uint8_t		ma_length;	/* = 5 */
    uint8_t		ma_reserved[2];
    uint8_t		ma_mac[MAC_SIZE];
} AT_MAC;

typedef struct AT_COUNTER_s {
    uint8_t		co_type;
    uint8_t		co_length;	/* = 1 */
    uint8_t		co_counter[2];
} AT_COUNTER;

typedef struct AT_COUNTER_TOO_SMALL_s {
    uint8_t		cs_type;
    uint8_t		cs_length;	/* = 1 */
    uint8_t		cs_reserved[2];
} AT_COUNTER_TOO_SMALL;

#define NONCE_S_SIZE	16
typedef struct AT_NONCE_S_s {
    uint8_t		nc_type;
    uint8_t		nc_length;	/* = 5 */
    uint8_t		nc_reserved[2];
    uint8_t		nc_nonce_s[NONCE_S_SIZE];
} AT_NONCE_S;

typedef struct AT_NOTIFICATION_s {
    uint8_t		nt_type;
    uint8_t		nt_length;	/* = 1 */
    uint8_t		nt_notification[2];
} AT_NOTIFICATION;

enum {
    kNotificationGeneralFailureAfterAuthentication = 0,
    kNotificationUserTemporarilyDeniedAccess = 1026,
    kNotificationUserNotSubscribedToRequestedService = 1031,
    kNotificationGeneralFailure = 16384,
    kNotificationSuccess = 32768
};

typedef struct AT_CLIENT_ERROR_CODE_s {
    uint8_t		ce_type;
    uint8_t		ce_length;	/* = 1 */
    uint8_t		ce_client_error_code[2];
} AT_CLIENT_ERROR_CODE;

enum {
    kClientErrorCodeUnableToProcessPacket = 0,
    kClientErrorCodeUnsupportedVersion = 1,
    kClientErrorCodeInsufficientNumberOfChallenges = 2,
    kClientErrorCodeRANDsAreNotFresh = 3
};
typedef uint16_t ClientErrorCode;

typedef union AttrUnion_u {
    AT_RAND *			at_rand;
    AT_PADDING *		at_padding;
    AT_NONCE_MT	*		at_nonce_mt;
    AT_PERMANENT_ID_REQ *	at_permanent_id_req;
    AT_MAC *			at_mac;
    AT_NOTIFICATION *		at_notification;
    AT_ANY_ID_REQ *		at_any_id_req;
    AT_IDENTITY	*		at_identity;
    AT_VERSION_LIST *		at_version_list;
    AT_SELECTED_VERSION *	at_selected_version;
    AT_FULLAUTH_ID_REQ *	at_fullauth_id_req;
    AT_COUNTER *		at_counter;
    AT_COUNTER_TOO_SMALL * 	at_counter_too_small;
    AT_NONCE_S *		at_nonce_s;
    AT_CLIENT_ERROR_CODE *	at_client_error_code;
    AT_IV *			at_iv;
    AT_ENCR_DATA *		at_encr_data;
    AT_NEXT_PSEUDONYM *		at_next_pseudonym;
    AT_NEXT_REAUTH_ID *		at_next_reauth_id;
    AT_RESULT_IND *		at_result_ind;
    TLVRef			tlv_p;
} AttrUnion;

enum {
    kEAPSIMClientStateNone = 0,
    kEAPSIMClientStateStart = 1,
    kEAPSIMClientStateChallenge = 2,
    kEAPSIMClientStateReauthentication = 3,
    kEAPSIMClientStateSuccess = 4,
    kEAPSIMClientStateFailure = 5
};
typedef int	EAPSIMClientState;

STATIC const char *
EAPSIMClientStateName(EAPSIMClientState state)
{
    switch (state) {
    case kEAPSIMClientStateNone:
	return ("None");
    case kEAPSIMClientStateStart:
	return ("Start");
    case kEAPSIMClientStateChallenge:
	return ("Challenge");
    case kEAPSIMClientStateReauthentication:
	return ("Reauthentication");
    case kEAPSIMClientStateSuccess:
	return ("Success");
    case kEAPSIMClientStateFailure:
	return ("Failure");
    default:
	return ("<unknown>");
    }
}

typedef struct {
    CFArrayRef			kc;
    CFArrayRef			sres;
    CFArrayRef			rand;
} SIMStaticTriplets, * SIMStaticTripletsRef;

/*
 * Type: EAPSIMContext
 * Purpose:
 *   Holds the EAP-SIM module's context private data.
 */
typedef struct {
    EAPClientState		plugin_state;
    EAPSIMClientState		state;
    int				previous_identifier;
    int				start_count;
    int				n_required_rands;
    EAPSIMAttributeType		last_identity_type;
    CFDataRef			last_identity;
    SIMStaticTriplets		sim_static;
    CFStringRef			imsi;
    CFStringRef			reauth_id;
    uint8_t			nonce_mt[NONCE_MT_SIZE];
    uint8_t			mk[CC_SHA1_DIGEST_LENGTH];
    EAPSIMKeyInfo		key_info;
    bool			key_info_valid;
    uint16_t *			version_list;
    int				version_list_count;
    uint16_t			at_counter;
    uint8_t			nonce_s[NONCE_S_SIZE];
    uint8_t			pkt[1500];
} EAPSIMContext, *EAPSIMContextRef;

/**
 ** Pseudonym storage
 **/
#define kEAPSIMApplicationID	CFSTR("com.apple.network.eapclient.eapsim")

STATIC CFStringRef
EAPSIMPseudonymCopy(CFStringRef imsi)
{
    CFStringRef		pseudonym;

    pseudonym = CFPreferencesCopyValue(imsi,
				       kEAPSIMApplicationID,
				       kCFPreferencesCurrentUser,
				       kCFPreferencesAnyHost);
    if (pseudonym != NULL && isA_CFString(pseudonym) == NULL) {
	my_CFRelease(&pseudonym);
    }
    return (pseudonym);
}

STATIC void
EAPSIMPseudonymSave(CFStringRef imsi, CFStringRef pseudonym)
{
    CFPreferencesSetValue(imsi, pseudonym,
			  kEAPSIMApplicationID,
			  kCFPreferencesCurrentUser,
			  kCFPreferencesAnyHost);
    CFPreferencesSynchronize(kEAPSIMApplicationID,
			     kCFPreferencesCurrentUser,
			     kCFPreferencesAnyHost);
    return;
}

STATIC CFStringRef
copy_pseudonym_identity(CFStringRef pseudonym, CFStringRef realm)
{
    if (realm != NULL) {
	return (CFStringCreateWithFormat(NULL, NULL,
					 CFSTR("%@" "@" "%@"),
					 pseudonym, realm));
    }
    return (CFRetain(pseudonym));
}

STATIC CFStringRef
copy_imsi_identity(CFStringRef imsi, CFStringRef realm)
{

    if (realm != NULL) {
	return (CFStringCreateWithFormat(NULL, NULL,
					 CFSTR("1" "%@" "@" "%@"),
					 imsi, realm));
    }
    return (CFStringCreateWithFormat(NULL, NULL, CFSTR("1" "%@"),
				     imsi));
}

STATIC CFStringRef
copy_static_realm(CFDictionaryRef properties)
{
    CFStringRef	realm = NULL;

    if (properties == NULL) {
	return (NULL);
    }
    realm = isA_CFString(CFDictionaryGetValue(properties,
					      kEAPClientPropEAPSIMRealm));
    if (realm != NULL) {
	CFRetain(realm);
    }
    return (realm);
}

STATIC CFStringRef
copy_static_imsi(CFDictionaryRef properties)
{
    CFStringRef		imsi;
    
    if (properties == NULL) {
	return (NULL);
    }
    imsi = isA_CFString(CFDictionaryGetValue(properties,
					     kEAPClientPropEAPSIMIMSI));
    if (imsi == NULL) {
	return (NULL);
    }
    return (CFRetain(imsi));
}

STATIC CFStringRef
copy_static_identity(CFDictionaryRef properties, bool use_pseudonym)
{
    CFStringRef		imsi;
    CFStringRef		realm;
    CFStringRef		ret_identity = NULL;

    imsi = copy_static_imsi(properties);
    if (imsi == NULL) {
	return (NULL);
    }
    realm = copy_static_realm(properties);
    if (use_pseudonym) {
	CFStringRef		pseudonym;

	pseudonym = EAPSIMPseudonymCopy(imsi);
	if (pseudonym != NULL) {
	    ret_identity = copy_pseudonym_identity(pseudonym, realm);
	    CFRelease(pseudonym);
	}
    }
    if (ret_identity == NULL) {
	ret_identity = copy_imsi_identity(imsi, realm);
    }
    my_CFRelease(&imsi);
    my_CFRelease(&realm);
    return (ret_identity);
}

/**
 ** SIM Access routines
 **/
#if TARGET_OS_EMBEDDED
#include <CoreTelephony/CTSIMSupport.h>

#define EAPSIM_REALM_FORMAT	"wlan.mnc%@.mcc%@.3gppnetwork.org"

STATIC CFStringRef
sim_realm_create(void)
{
    CFStringRef		mcc;
    CFStringRef		mnc;
    CFStringRef		realm = NULL;

    mcc = CTSIMSupportCopyMobileSubscriberCountryCode(NULL);
    mnc = CTSIMSupportCopyMobileSubscriberNetworkCode(NULL);
    if (mcc != NULL && mnc != NULL) {
	if (CFStringGetLength(mnc) == 2) {
	    CFStringRef		old_mnc = mnc;

	    mnc = CFStringCreateWithFormat(NULL, NULL, CFSTR("0%@"), mnc);
	    CFRelease(old_mnc);
	}
	realm = CFStringCreateWithFormat(NULL, NULL,
					 CFSTR(EAPSIM_REALM_FORMAT),
					 mnc, mcc);
    }
    my_CFRelease(&mcc);
    my_CFRelease(&mnc);
    return (realm);
}

STATIC CFStringRef
sim_imsi_copy(void)
{
    CFStringRef		imsi;

    imsi = CTSIMSupportCopyMobileSubscriberIdentity(NULL);
    if (imsi != NULL) {
	if (CFStringGetLength(imsi) == 0) {
	    CFRelease(imsi);
	    imsi = NULL;
	}
    }
    return (imsi);
}

STATIC CFStringRef
sim_identity_create(CFDictionaryRef properties, bool use_pseudonym)
{
    CFStringRef		imsi;
    CFStringRef		realm = NULL;
    CFStringRef		ret_identity = NULL;

    imsi = sim_imsi_copy();
    if (imsi == NULL) {
	return (NULL);
    }
    realm = copy_static_realm(properties);
    if (realm == NULL) {
	realm = sim_realm_create();
    }
    if (use_pseudonym) {
	CFStringRef		pseudonym;

	pseudonym = EAPSIMPseudonymCopy(imsi);
	if (pseudonym != NULL) {
	    ret_identity = copy_pseudonym_identity(pseudonym, realm);
	    CFRelease(pseudonym);
	}
    }
    if (ret_identity == NULL) {
	ret_identity = copy_imsi_identity(imsi, realm);
    }
    my_CFRelease(&imsi);
    my_CFRelease(&realm);
    return (ret_identity);
}

#else /* TARGET_OS_EMBEDDED */

STATIC CFStringRef
sim_identity_create(CFDictionaryRef properties, bool use_pseudonym)
{
    return (NULL);
}

STATIC CFStringRef
sim_imsi_copy(void)
{
    return (NULL);
}
#endif /* TARGET_OS_EMBEDDED */

/**
 ** Utility Routines
 **/
static int
S_get_plist_int(CFDictionaryRef plist, CFStringRef key, int def)
{
    CFNumberRef 	n;
    int			ret = def;

    n = isA_CFNumber(CFDictionaryGetValue(plist, key));
    if (n) {
	if (CFNumberGetValue(n, kCFNumberIntType, &ret) == FALSE) {
	    ret = def;
	}
    }
    return (ret);
}

STATIC bool
blocks_are_duplicated(const uint8_t * blocks, int n_blocks, int block_size)
{
    int 		i;
    int			j;
    const uint8_t *	scan;

    for (i = 0, scan = blocks; i < (n_blocks - 1); i++, scan += block_size) {
	const uint8_t *	scan_j = scan + block_size;

	for (j = i + 1; j < n_blocks; j++, scan_j += block_size) {
	    if (bcmp(scan, scan_j, block_size) == 0) {
		return (TRUE);
	    }
	}
    }
    return (FALSE);
}

STATIC void
fill_with_random(uint8_t * buf, int len)
{
    int			i;
    int			n;
    u_int32_t * 	p = (u_int32_t *)buf;
    
    n = len / sizeof(*p);
    for (i = 0; i < n; i++, p++) {
	*p = arc4random();
    }
    return;
}

/*
 * Function: uint16_set
 * Purpose:
 *   Set a field in a structure that's at least two bytes long to the given
 *   value, putting it into network byte order
 */
INLINE void
uint16_set(uint8_t * field, uint16_t value)
{
    *((uint16_t *)field) = htons(value);
    return;
}

/*
 * Function: uint16_get
 * Purpose:
 *   Get a field in a structure that's at least two bytes long, converting
 *   to host byte order.
 */
INLINE uint16_t
uint16_get(const uint8_t * field)
{
    return (ntohs(*((uint16_t *)field)));
}

/**
 ** TLVBuffer routines
 **/

INLINE int
TLVRoundUp(int length)
{
    return (roundup(length, TLV_ALIGNMENT));
}

typedef struct TLVBuffer_s {
    uint8_t *		storage;
    int			size;
    int			offset;
    char		err_str[160];
} TLVBuffer, * TLVBufferRef;

STATIC int
TLVBufferUsed(TLVBufferRef tb)
{
    return (tb->offset);
}

STATIC const char *
TLVBufferErrorString(TLVBufferRef tb)
{
    return (tb->err_str);
}

STATIC void
TLVBufferInit(TLVBufferRef tb, uint8_t * storage, int size)
{
    tb->storage = storage;
    tb->size = size;
    tb->offset = 0;
    tb->err_str[0] = '\0';
    return;
}

STATIC TLVRef
TLVBufferAllocateTLV(TLVBufferRef tb, EAPSIMAttributeType type, int length)
{
    int		left;
    int		padded_length;
    TLVRef	tlv_p;
    
    if (length < offsetof(TLV, tlv_value)) {
	return (NULL);
    }
    padded_length = TLVRoundUp(length);
    if (padded_length > TLV_MAX_LENGTH) {
	snprintf(tb->err_str, sizeof(tb->err_str),
		 "padded_length %d > max length %d",
		 padded_length, TLV_MAX_LENGTH);
	return (NULL);
    }
    left = tb->size - tb->offset;
    if (left < padded_length) {
	snprintf(tb->err_str, sizeof(tb->err_str),
		 "available space %d < required %d",
		 left, padded_length);
	return (NULL);
    }

    /* set the type and length */
    tlv_p = (TLVRef)(tb->storage + tb->offset);
    tlv_p->tlv_type = type;
    tlv_p->tlv_length = padded_length / TLV_ALIGNMENT;
    tb->offset += padded_length;
    return (tlv_p);
}

STATIC bool
TLVBufferAddIdentity(TLVBufferRef tb_p, 
		     const uint8_t * identity, int identity_length)
{
    AttrUnion		attr;

    attr.tlv_p = TLVBufferAllocateTLV(tb_p,
				      kAT_IDENTITY,
				      offsetof(AT_IDENTITY, id_identity)
				      + identity_length);
    if (attr.tlv_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: can't add AT_IDENTITY, %s",
	       TLVBufferErrorString(tb_p));
	return (FALSE);
    }
    uint16_set(attr.at_identity->id_actual_length, identity_length);
    bcopy(identity, attr.at_identity->id_identity, identity_length);
    return (TRUE);
}

STATIC bool
TLVBufferAddIdentityString(TLVBufferRef tb_p, CFStringRef identity,
			   CFDataRef * ret_data)
{
    CFDataRef	data;
    bool	result;

    *ret_data = NULL;
    data = CFStringCreateExternalRepresentation(NULL, identity, 
						kCFStringEncodingUTF8, 0);
    if (data == NULL) {
	return (FALSE);
    }
    result = TLVBufferAddIdentity(tb_p, CFDataGetBytePtr(data),
				  CFDataGetLength(data));
    if (result == TRUE && ret_data != NULL) {
	*ret_data = data;
    }
    else {
	CFRelease(data);
    }
    return (result);

}

STATIC bool
TLVBufferAddCounter(TLVBufferRef tb_p, uint16_t at_counter)
{
    AT_COUNTER *	counter_p;

    counter_p = (AT_COUNTER *)TLVBufferAllocateTLV(tb_p, kAT_COUNTER,
						   sizeof(AT_COUNTER));
    if (counter_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed allocating AT_COUNTER, %s",
	       TLVBufferErrorString(tb_p));
	return (FALSE);
    }
    uint16_set(counter_p->co_counter, at_counter);
    return (TRUE);
}

STATIC bool
TLVBufferAddCounterTooSmall(TLVBufferRef tb_p)
{
    AT_COUNTER_TOO_SMALL * counter_too_small_p;
	
    counter_too_small_p = (AT_COUNTER_TOO_SMALL *)
	TLVBufferAllocateTLV(tb_p, kAT_COUNTER_TOO_SMALL,
			     sizeof(AT_COUNTER_TOO_SMALL));
    if (counter_too_small_p == NULL) {
	syslog(LOG_NOTICE,
	       "eapsim: failed allocating AT_COUNTER_TOO_SMALL, %s",
	       TLVBufferErrorString(tb_p));
	return (FALSE);
    }
    uint16_set(counter_too_small_p->cs_reserved, 0);
    return (TRUE);
}

STATIC bool
TLVBufferAddPadding(TLVBufferRef tb_p, int padding_length)
{
    AT_PADDING *	padding_p;

    switch (padding_length) {
    case 4:
    case 8:
    case 12:
	break;
    default:
	syslog(LOG_NOTICE, "eapsim: trying to add invalid AT_PADDING len %d",
	       padding_length);
	return (FALSE);
    }
    padding_p = (AT_PADDING *)
	TLVBufferAllocateTLV(tb_p, kAT_PADDING, padding_length);
    if (padding_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed to allocate AT_PADDING, %s",
	       TLVBufferErrorString(tb_p));
	return (FALSE);
    }
    bzero(padding_p->pa_padding,
	  padding_length - offsetof(AT_PADDING, pa_padding));
    return (TRUE);
}

/**
 ** TLVList routines
 **/
#define N_ATTRS_STATIC			10
typedef struct TLVList_s {
    const void * *	attrs;		/* pointers to attributes */
    const void *	attrs_static[N_ATTRS_STATIC];
    int			count;
    int			size;
    char		err_str[160];
} TLVList, * TLVListRef;

INLINE int
TLVListAttrsStaticSize(void)
{
    const TLVListRef	tlvs_p;

    return ((int)sizeof(tlvs_p->attrs_static)
	    / sizeof(tlvs_p->attrs_static[0]));
}

STATIC const char *
TLVListErrorString(TLVListRef tlvs_p)
{
    return (tlvs_p->err_str);
}

STATIC void
TLVListInit(TLVListRef tlvs_p)
{
    tlvs_p->attrs = NULL;
    tlvs_p->count = tlvs_p->size = 0;
    return;
}

STATIC void
TLVListFree(TLVListRef tlvs_p)
{
    if (tlvs_p->attrs != NULL && tlvs_p->attrs != tlvs_p->attrs_static) {
#ifdef TEST_TLVLIST_PARSE
	printf("freeing data\n");
#endif /* TEST_TLVLIST_PARSE */
	free(tlvs_p->attrs);
    }
    TLVListInit(tlvs_p);
    return;
}

STATIC void
TLSListAddAttribute(TLVListRef tlvs_p, const uint8_t * attr)
{
    if (tlvs_p->attrs == NULL) {
	tlvs_p->attrs = tlvs_p->attrs_static;
	tlvs_p->size = TLVListAttrsStaticSize();
    }
    else if (tlvs_p->count == tlvs_p->size) {
	tlvs_p->size += TLVListAttrsStaticSize();
	if (tlvs_p->attrs == tlvs_p->attrs_static) {
	    tlvs_p->attrs = (const void * *)
		malloc(sizeof(*tlvs_p->attrs) * tlvs_p->size);
	    bcopy(tlvs_p->attrs_static, tlvs_p->attrs,
		  sizeof(*tlvs_p->attrs) * tlvs_p->count);
	}
	else {
	    tlvs_p->attrs = (const void * *)
		reallocf(tlvs_p->attrs,
			 sizeof(*tlvs_p->attrs) * tlvs_p->size);
	}
    }
    tlvs_p->attrs[tlvs_p->count++] = attr;
    return;
}

enum {
    kTLVGood = 0,
    kTLVBad = 1,
    kTLVUnrecognized = 2
};

STATIC int
TLVCheckValidity(TLVListRef tlvs_p, TLVRef tlv_p)
{
    AttrUnion		attr;
    int			i;
    int			len;
    int			offset;
    int			ret = kTLVGood;
    const uint8_t *	scan;
    int			tlv_length;

    attr.tlv_p = tlv_p;
    tlv_length = tlv_p->tlv_length * TLV_ALIGNMENT;
    switch (tlv_p->tlv_type) {
    case kAT_RAND:
	len = tlv_length - offsetof(AT_RAND, ra_rand);
	if ((len % SIM_RAND_SIZE) != 0) {
	    /* must be a multiple of 16 bytes */
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_RAND length %d not multiple of %d",
		     len, SIM_RAND_SIZE);
	    ret = kTLVBad;
	    break;
	}
	if (len == 0) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_RAND contains no RANDs");
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_PADDING:
	switch (tlv_length) {
	case 4:
	case 8:
	case 12:
	    len = tlv_length - offsetof(AT_PADDING, pa_padding);
	    for (i = 0, scan = attr.at_padding->pa_padding;
		 i < len; i++, scan++) {
		if (*scan != 0) {
		    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
			     "AT_PADDING non-zero value 0x%x at offset %d",
			     *scan, i);
		    ret = kTLVBad;
		    break;
		}
	    }
	    break;
	default:
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_PADDING length %d not 4, 8, or 12", tlv_length);
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_NONCE_MT:
    case kAT_IV:
    case kAT_MAC:
    case kAT_NONCE_S:
	offset = offsetof(AT_NONCE_MT, nm_nonce_mt);
	if (tlv_length <= offset) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s truncated %d <= %d",
		    EAPSIMAttributeTypeString(tlv_p->tlv_type),
		    tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	len = tlv_length - offset;
	if (len != sizeof(attr.at_nonce_mt->nm_nonce_mt)) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s invalid length %d != %d",
		    EAPSIMAttributeTypeString(tlv_p->tlv_type),
		    len, (int)sizeof(attr.at_nonce_mt->nm_nonce_mt));
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_IDENTITY:
    case kAT_VERSION_LIST:
    case kAT_NEXT_PSEUDONYM:
    case kAT_NEXT_REAUTH_ID:
	offset = offsetof(AT_IDENTITY, id_identity);
	if (tlv_length <= offset) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s empty/truncated (%d <= %d)",
		    EAPSIMAttributeTypeString(tlv_p->tlv_type),
		    tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	len = uint16_get(attr.at_identity->id_actual_length);
	if (len > (tlv_length - offset)) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		    "%s actual length %d > TLV length %d",
		    EAPSIMAttributeTypeString(tlv_p->tlv_type),
		    len, tlv_length - offset);
	    ret = kTLVBad;
	    break;
	}
	if (tlv_p->tlv_type == kAT_VERSION_LIST && (len & 0x1) != 0) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		    "AT_VERSION_LIST actual length %d not multiple of 2",
		    len);
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_ENCR_DATA:
	offset = offsetof(AT_ENCR_DATA, ed_encrypted_data);
	if (tlv_length <= offset) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "AT_ENCR_DATA empty/truncated (%d <= %d)",
		    tlv_length, offset);
	    ret = kTLVBad;
	    break;
	}
	break;
    case kAT_SELECTED_VERSION:
    case kAT_PERMANENT_ID_REQ:
    case kAT_ANY_ID_REQ:
    case kAT_FULLAUTH_ID_REQ:
    case kAT_RESULT_IND:
    case kAT_COUNTER:
    case kAT_COUNTER_TOO_SMALL:
    case kAT_CLIENT_ERROR_CODE:
    case kAT_NOTIFICATION:
	if (tlv_length != 4) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s length %d != 4",
		    EAPSIMAttributeTypeString(tlv_p->tlv_type),
		    tlv_length);
	    ret = kTLVBad;
	    break;
	}
	break;
    default:
	ret = kTLVUnrecognized;
	break;
    }
    return (ret);
}

STATIC bool
TLVListParse(TLVListRef tlvs_p, const uint8_t * attrs, int attrs_length)
{
    int			offset;
    const uint8_t *	scan;
    bool		success = TRUE;
    int			tlv_length;

    scan = attrs;
    offset = 0;
    while (TRUE) {
	int		left = attrs_length - offset;
	TLVRef		this_tlv;
	int		tlv_validity;

	if (left == 0) {
	    /* we're done */
	    break;
	}
	if (left < TLV_HEADER_LENGTH) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "Missing/truncated attribute at offset %d",
		     offset);
	    success = FALSE;
	    break;
	}
	this_tlv = (TLVRef)scan;
	tlv_length = this_tlv->tlv_length * TLV_ALIGNMENT;
	if (tlv_length > left) {
	    snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
		     "%s too large %d (> %d) at offset %d",
		     EAPSIMAttributeTypeString(this_tlv->tlv_type),
		     tlv_length, left, offset);
	    success = FALSE;
	    break;
	}
	tlv_validity = TLVCheckValidity(tlvs_p, this_tlv);
	if (tlv_validity == kTLVGood) {
	    TLSListAddAttribute(tlvs_p, scan);
	}
	else if (tlv_validity == kTLVBad
		 || ((tlv_validity == kTLVUnrecognized)
		     && (this_tlv->tlv_type 
			 < kEAPSIM_TLV_SKIPPABLE_RANGE_START))) {
	    if (tlv_validity == kTLVUnrecognized) {
		snprintf(tlvs_p->err_str, sizeof(tlvs_p->err_str),
			 "unrecognized attribute %d", this_tlv->tlv_type);
	    }
	    success = FALSE;
	    break;
	}
	offset += tlv_length;;
	scan += tlv_length;
    }
    if (success == FALSE) {
	TLVListFree(tlvs_p);
    }
    return (success);
}

#define _WIDTH	"%18"

STATIC void
TLVPrint(FILE * f, TLVRef tlv_p)
{
    AttrUnion		attr;
    char		buf[128];
    int			count;
    int			i;
    const char *	field_name;
    int			len;
    int			pad_len;
    uint8_t *		scan;
    int			tlv_length;
    uint16_t		val16;

    attr.tlv_p = tlv_p;
    tlv_length = tlv_p->tlv_length * TLV_ALIGNMENT;
    fprintf(f, "%s: Length %d\n",
	    EAPSIMAttributeTypeString(tlv_p->tlv_type),
	    tlv_length);
    field_name = EAPSIMAttributeTypeString(tlv_p->tlv_type) + 3;
    switch (tlv_p->tlv_type) {
    case kAT_RAND:
	fprintf(f, _WIDTH "s:\t", "(reserved)");
	fprint_bytes(f, attr.at_rand->ra_reserved,
		     sizeof(attr.at_rand->ra_reserved));
	len = tlv_length - offsetof(AT_RAND, ra_rand);
	count = len / SIM_RAND_SIZE;
	fprintf(f, "\n" _WIDTH"s: (n=%d)\n", field_name, count);
	for (scan = attr.at_rand->ra_rand, i = 0;
	     i < count; i++, scan += SIM_RAND_SIZE) {
	    fprintf(f, _WIDTH "d:\t", i);
	    fprint_bytes(f, scan, SIM_RAND_SIZE);
	    fprintf(f, "\n");
	}
	break;
    case kAT_PADDING:
	len = tlv_length - offsetof(AT_PADDING, pa_padding);
	fprintf(f, _WIDTH "s: %d bytes\n", field_name, len);
	fprintf(f, _WIDTH "s\t", "");
	fprint_bytes(f, attr.at_padding->pa_padding, len);
	fprintf(f, "\n");
	break;
    case kAT_NONCE_MT:
    case kAT_IV:
    case kAT_MAC:
    case kAT_NONCE_S:
	fprintf(f, _WIDTH "s:\t", "(reserved)");
	fprint_bytes(f, attr.at_nonce_mt->nm_reserved,
		     sizeof(attr.at_nonce_mt->nm_reserved));
	fprintf(f, "\n" _WIDTH "s:\t", field_name);
	fprint_bytes(f, attr.at_nonce_mt->nm_nonce_mt,
		     sizeof(attr.at_nonce_mt->nm_nonce_mt));
	fprintf(f, "\n");
	break;
    case kAT_VERSION_LIST:
	len = uint16_get(attr.at_version_list->vl_actual_length);
	count = len / sizeof(uint16_t);
	fprintf(f, _WIDTH "s: Actual Length %d\n", field_name, len);
	for (scan = attr.at_version_list->vl_version_list, i = 0;
	     i < count; i++, scan += sizeof(uint16_t)) {
	    uint16_t	this_vers = uint16_get(scan);

	    fprintf(f, _WIDTH "d:\t%04d\n", i, this_vers);
	}
	pad_len = (tlv_length - offsetof(AT_VERSION_LIST, vl_version_list))
	    - len;
	snprintf(buf, sizeof(buf), "(%d pad bytes)", pad_len);
	fprintf(f, _WIDTH "s:\t", buf);
	fprint_bytes(f, attr.at_identity->id_identity + len, pad_len);
	fprintf(f, "\n");
	break;
    case kAT_IDENTITY:
    case kAT_NEXT_PSEUDONYM:
    case kAT_NEXT_REAUTH_ID:
	len = uint16_get(attr.at_identity->id_actual_length);
	fprintf(f, _WIDTH "s: Actual Length %d\n", field_name, len);
	fprint_data(f, attr.at_identity->id_identity, len);
	pad_len = (tlv_length - offsetof(AT_IDENTITY, id_identity)) - len;
	if (pad_len != 0) {
	    snprintf(buf, sizeof(buf), "(%d pad bytes)", pad_len);
	    fprintf(f, _WIDTH "s:\t", buf);
	    fprint_bytes(f, attr.at_identity->id_identity + len, pad_len);
	    fprintf(f, "\n");
	}
	break;
    case kAT_ENCR_DATA:
	fprintf(f, _WIDTH "s:\t", "(reserved)");
	fprint_bytes(f, attr.at_encr_data->ed_reserved,
		     sizeof(attr.at_encr_data->ed_reserved));
	len = tlv_length - offsetof(AT_ENCR_DATA, ed_encrypted_data);
	fprintf(f, "\n" _WIDTH "s: Length %d\n", field_name, len);
	fprint_data(f, attr.at_encr_data->ed_encrypted_data, len);
	break;
    case kAT_SELECTED_VERSION:
    case kAT_COUNTER:
    case kAT_CLIENT_ERROR_CODE:
    case kAT_NOTIFICATION:
	val16 = uint16_get(attr.at_selected_version->sv_selected_version);
	fprintf(f, _WIDTH "s:\t%04d\n", field_name, val16);
	break;
    case kAT_PERMANENT_ID_REQ:
    case kAT_ANY_ID_REQ:
    case kAT_FULLAUTH_ID_REQ:
    case kAT_RESULT_IND:
    case kAT_COUNTER_TOO_SMALL:
	fprintf(f, _WIDTH "s:\t", "(reserved)");
	fprint_bytes(f, attr.at_encr_data->ed_reserved,
		     sizeof(attr.at_encr_data->ed_reserved));
	fprintf(f, "\n");
	break;
    default:
	break;
    }
    return;
}

STATIC void
TLVListPrint(FILE * f, TLVListRef tlvs_p)
{
    int			i;
    const void * *	scan;

    for (i = 0, scan = tlvs_p->attrs; i < tlvs_p->count; i++, scan++) {
	TLVRef	tlv_p = (TLVRef)(*scan);
	TLVPrint(f, tlv_p);
    }
    fflush(f);
    return;
}

STATIC TLVRef
TLVListLookupAttribute(TLVListRef tlvs_p, EAPSIMAttributeType type)
{
    int			i;
    const void * *	scan;

    for (i = 0, scan = tlvs_p->attrs; i < tlvs_p->count; i++, scan++) {
	TLVRef	tlv_p = (TLVRef)(*scan);

	if (tlv_p->tlv_type == type) {
	    return (tlv_p);
	}
    }
    return (NULL);
}

STATIC CFStringRef
TLVCreateString(TLVRef tlv_p)
{
    CFDataRef		data;
    int			len;
    AT_IDENTITY *	id_p = (AT_IDENTITY *)tlv_p;
    CFStringRef		str;

    len = uint16_get(id_p->id_actual_length);
    data = CFDataCreateWithBytesNoCopy(NULL, id_p->id_identity, len,
				       kCFAllocatorNull);
    str = CFStringCreateFromExternalRepresentation(NULL, data,
						   kCFStringEncodingUTF8);
    CFRelease(data);
    return (str);
}    

STATIC CFStringRef
TLVListCreateStringFromAttribute(TLVListRef tlvs_p, EAPSIMAttributeType type)
{
    TLVRef	tlv_p;

    switch (type) {
    case kAT_NEXT_REAUTH_ID:
    case kAT_NEXT_PSEUDONYM:
	break;
    default:
	return (NULL);
    }
    tlv_p = TLVListLookupAttribute(tlvs_p, type);
    if (tlv_p == NULL) {
	return (NULL);
    }
    return (TLVCreateString(tlv_p));
} 

/**
 ** eapsim module functions
 **/
STATIC void
EAPSIMContextSetLastIdentity(EAPSIMContextRef context, CFDataRef identity_data)
{
    if (identity_data != NULL) {
	CFRetain(identity_data);
    }
    if (context->last_identity != NULL) {
	CFRelease(context->last_identity);
    }
    context->last_identity = identity_data;
    return;
}

STATIC void
EAPSIMContextSetVersionList(EAPSIMContextRef context,
			    const uint16_t * list, int count)
{
    if (list != NULL) {
	int	size = count * sizeof(*list);

	if (context->version_list != NULL
	    && count == context->version_list_count
	    && bcmp(context->version_list, list, size) == 0) {
	    /* current list is the same, no need to copy it */
	}
	else {
	    if (context->version_list != NULL) {
		free(context->version_list);
	    }
	    context->version_list = (uint16_t *)malloc(size);
	    bcopy(list, context->version_list, size);
	    context->version_list_count = count;
	}
    }
    else if (context->version_list != NULL) {
	free(context->version_list);
	context->version_list = NULL;
	context->version_list_count = 0;
    }
    return;
}

STATIC void
EAPSIMContextClear(EAPSIMContextRef context)
{
    bzero(context, sizeof(*context));
    context->plugin_state = kEAPClientStateAuthenticating;
    context->state = kEAPSIMClientStateNone;
    context->previous_identifier = -1;
    return;
}

STATIC CFArrayRef
copy_data_array(CFDictionaryRef properties, CFStringRef prop_name,
		int data_size)
{
    int			count;
    int			i;
    CFArrayRef		list;

    if (properties == NULL) {
	return (NULL);
    }
    list = CFDictionaryGetValue(properties, prop_name);
    if (isA_CFArray(list) == NULL) {
	return (NULL);
    }
    count = CFArrayGetCount(list);
    if (count == 0) {
	return (NULL);
    }
    for (i = 0; i < count; i++) {
	CFDataRef		data = CFArrayGetValueAtIndex(list, i);

	if (isA_CFData(data) == NULL
	    || CFDataGetLength(data) != data_size) {
	    return (NULL);
	}
    }
    return (CFRetain(list));
}

STATIC bool
SIMStaticTripletsInitFromProperties(SIMStaticTripletsRef sim_static_p,
				    CFDictionaryRef properties)
{
    int			count;
    CFArrayRef		kc;
    CFArrayRef		rand;
    CFArrayRef		sres;

    my_CFRelease(&sim_static_p->kc);
    my_CFRelease(&sim_static_p->sres);
    my_CFRelease(&sim_static_p->rand);
    if (properties == NULL) {
	return (FALSE);
    }
    kc = copy_data_array(properties,
			 kEAPClientPropEAPSIMKcList,
			 SIM_KC_SIZE);
    sres = copy_data_array(properties,
			   kEAPClientPropEAPSIMSRESList,
			   SIM_SRES_SIZE);
    rand = copy_data_array(properties,
			   kEAPClientPropEAPSIMRANDList,
			   SIM_RAND_SIZE);
    if (kc == NULL || sres == NULL || rand == NULL) {
	goto failed;
    }
    count = CFArrayGetCount(kc);
    if (count != CFArrayGetCount(sres)
	|| count != CFArrayGetCount(sres)) {
	/* they need to be parallel arrays */
	goto failed;
    }
    sim_static_p->kc = kc;
    sim_static_p->sres = sres;
    sim_static_p->rand = rand;
    return (TRUE);

 failed:
    my_CFRelease(&kc);
    my_CFRelease(&sres);
    my_CFRelease(&rand);
    return (FALSE);
}

STATIC void
EAPSIMContextFree(EAPSIMContextRef context)
{
    EAPSIMContextSetVersionList(context, NULL, 0);
    SIMStaticTripletsInitFromProperties(&context->sim_static, NULL);
    my_CFRelease(&context->imsi);
    my_CFRelease(&context->reauth_id);
    EAPSIMContextSetLastIdentity(context, NULL);
    EAPSIMContextClear(context);
    free(context);
    return;
}

STATIC int
EAPSIMContextLookupStaticRAND(EAPSIMContextRef context, 
			      const uint8_t rand[SIM_RAND_SIZE])
{
    int		count;
    int		i;

    if (context->sim_static.rand == NULL) {
	return (-1);
    }
    count = CFArrayGetCount(context->sim_static.rand);
    for (i = 0; i < count; i++) {
	CFDataRef	data;

	data = CFArrayGetValueAtIndex(context->sim_static.rand, i);
	if (bcmp(rand, CFDataGetBytePtr(data), SIM_RAND_SIZE) == 0) {
	    return (i);
	}
    }
    return (-1);
}

STATIC bool
EAPSIMContextSIMProcessRAND(EAPSIMContextRef context, 
			    const uint8_t * rand_p, int count,
			    uint8_t * kc_p, uint8_t * sres_p)
{
    int			i;
    uint8_t *		kc_scan;
    const uint8_t *	rand_scan;
    uint8_t *		sres_scan;

    if (context->sim_static.rand != NULL) {
	/* use the static SIM triplet information */
	rand_scan = rand_p;
	kc_scan = kc_p;
	sres_scan = sres_p;
	for (i = 0; i < count; i++) {
	    CFDataRef	kc_data;
	    CFDataRef	sres_data;
	    int		where;
	    
	    where = EAPSIMContextLookupStaticRAND(context, rand_scan);
	    if (where == -1) {
		syslog(LOG_NOTICE, "eapsim: can't find static RAND value");
		return (FALSE);
	    }
	    kc_data = CFArrayGetValueAtIndex(context->sim_static.kc, where);
	    bcopy(CFDataGetBytePtr(kc_data), kc_scan, SIM_KC_SIZE);
	    sres_data = CFArrayGetValueAtIndex(context->sim_static.sres, where);
	    bcopy(CFDataGetBytePtr(sres_data), sres_scan, SIM_SRES_SIZE);
	    
	    /* move to the next element */
	    rand_scan += SIM_RAND_SIZE;
	    kc_scan += SIM_KC_SIZE;
	    sres_scan += SIM_SRES_SIZE;
	}
    }
    else {
	/* ask the SIM to get the (Kc, SRES) pairs from the RAND's */
	if (SIMProcessRAND(rand_p, count, kc_p, sres_p) == FALSE) {
	    syslog(LOG_NOTICE, "SIMProcessRAND failed");
	    return (FALSE);
	}
    }
    return (TRUE);
}

/**
 ** EAP-SIM module functions
 **/
STATIC EAPClientStatus
eapsim_init(EAPClientPluginDataRef plugin, CFArrayRef * require_props,
	    EAPClientDomainSpecificError * error)
{
    EAPSIMContextRef	context = NULL;
    CFStringRef		imsi = NULL;
    SIMStaticTriplets	triplets;

    /* for testing, allow static triplets to override a real SIM */
    bzero(&triplets, sizeof(triplets));
    if (SIMStaticTripletsInitFromProperties(&triplets,
					    plugin->properties)) {
	imsi = copy_static_imsi(plugin->properties);
	if (imsi == NULL) {
	    syslog(LOG_NOTICE,
		   "eapsim: static triplets specified but IMSI missing");
	    SIMStaticTripletsInitFromProperties(&triplets, NULL);
	    return (kEAPClientStatusConfigurationInvalid);
	}
    }
    else {
	/* check for a real SIM module */
	imsi = sim_imsi_copy();
	if (imsi == NULL) {
	    syslog(LOG_NOTICE, "EAP-SIM: no SIM available");
	    return (kEAPClientStatusResourceUnavailable);
	}
	syslog(LOG_NOTICE, "EAP-SIM: SIM found");
    }

    /* allocate a context */
    context = (EAPSIMContextRef)malloc(sizeof(*context));
    if (context == NULL) {
	(void)SIMStaticTripletsInitFromProperties(&triplets, NULL);
	return (kEAPClientStatusAllocationFailed);
    }
    EAPSIMContextClear(context);
    context->imsi = imsi;
    context->sim_static = triplets;
    context->n_required_rands 
	= S_get_plist_int(plugin->properties,
			  kEAPClientPropEAPSIMNumberOfRANDs,
			  EAPSIM_MAX_RANDS);
    if (context->n_required_rands != 2
	&& context->n_required_rands != 3) {
	syslog(LOG_NOTICE,
	       "eapsim: EAPSIMNumberOfRands %d is invalid, using 3 instead",
	       context->n_required_rands);
	context->n_required_rands = EAPSIM_MAX_RANDS;
    }
    plugin->private = context;
    return (kEAPClientStatusOK);
}

STATIC void
eapsim_free(EAPClientPluginDataRef plugin)
{
    EAPSIMContextFree(plugin->private);
    plugin->private = NULL;
    return;
}

STATIC void
eapsim_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    return;
}

STATIC EAPPacketRef
eapsim_make_response(EAPSIMContextRef context,
		     EAPPacketRef in_pkt, EAPSIMSubtype subtype,
		     TLVBufferRef tb_p)
{
    EAPSIMPacketRef	pkt;

    pkt = (EAPSIMPacketRef)context->pkt;
    TLVBufferInit(tb_p, pkt->attrs,
		  sizeof(context->pkt) - offsetof(EAPSIMPacket, attrs));
    pkt->code = kEAPCodeResponse;
    pkt->identifier = in_pkt->identifier;
    pkt->type = kEAPTypeEAPSIM;
    pkt->subtype = subtype;
    uint16_set(pkt->reserved, 0);
    return ((EAPPacketRef)pkt);
}

STATIC EAPPacketRef
eapsim_make_client_error(EAPSIMContextRef context,
			 EAPPacketRef in_pkt, ClientErrorCode code)
{
    AttrUnion			attr;
    EAPPacketRef		pkt;
    TLVBuffer			tb;

    pkt = eapsim_make_response(context, in_pkt,
			       kEAPSIMSubtypeClientError, &tb);
    attr.tlv_p = TLVBufferAllocateTLV(&tb, kAT_CLIENT_ERROR_CODE,
				      sizeof(AT_CLIENT_ERROR_CODE));
    if (attr.tlv_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed allocating AT_CLIENT_ERROR_CODE, %s",
	       TLVBufferErrorString(&tb));
	return (NULL);
    }
    uint16_set(attr.at_client_error_code->ce_client_error_code, code);
    EAPPacketSetLength(pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(&tb));

    return (pkt);
}

STATIC const EAPSIMAttributeType	S_id_req_types[] = {
    kAT_ANY_ID_REQ,
    kAT_FULLAUTH_ID_REQ,
    kAT_PERMANENT_ID_REQ
};

STATIC const int		S_id_req_types_count = sizeof(S_id_req_types)
    / sizeof(S_id_req_types[0]);

STATIC EAPSIMAttributeType
lookup_identity_type(TLVListRef tlvs_p)
{
    int 		i;

    for (i = 0; i < S_id_req_types_count; i++) {
	if (TLVListLookupAttribute(tlvs_p, S_id_req_types[i]) != NULL) {
	    return (S_id_req_types[i]);
	}
    }
    return (0);
}

STATIC EAPPacketRef
eapsim_start(EAPClientPluginDataRef plugin, 
	     const EAPPacketRef in_pkt,
	     TLVListRef tlvs_p,
	     EAPClientStatus * client_status)
{
    AttrUnion		attr;
    int			count;
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;
    int			i;
    CFStringRef		identity = NULL;
    CFDataRef		identity_data = NULL;
    EAPSIMAttributeType	identity_type;
    bool		good_version = FALSE;
    EAPPacketRef	pkt = NULL;
    const uint8_t *	scan;
    AT_VERSION_LIST *	version_list_p;
    TLVBuffer		tb;

    version_list_p = (AT_VERSION_LIST *)
	TLVListLookupAttribute(tlvs_p, kAT_VERSION_LIST);
    if (version_list_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: Start is missing AT_VERSION_LIST");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    count = uint16_get(version_list_p->vl_actual_length) / sizeof(uint16_t);
    for (i = 0, scan = version_list_p->vl_version_list;
	 i < count; i++, scan += sizeof(uint16_t)) {
	uint16_t	this_vers = uint16_get(scan);

	/* we only support version 1 */
	if (this_vers == kEAPSIMVersion1) {
	    good_version = TRUE;
	    break;
	}
    }
    if (good_version == FALSE) {
	pkt = eapsim_make_client_error(context, in_pkt, 
				       kClientErrorCodeUnsupportedVersion);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (count > 1) {
	/* if there was more than one version, save the list */
	EAPSIMContextSetVersionList(context, 
				    (const uint16_t *)
				    version_list_p->vl_version_list, count);
    }
    else {
	EAPSIMContextSetVersionList(context, NULL, 0);
    }
    if (context->state != kEAPSIMClientStateStart) {
	/* starting over */
	context->plugin_state = kEAPClientStateAuthenticating;
	context->key_info_valid = FALSE;
	context->start_count = 0;
	context->last_identity_type = 0;
	context->state = kEAPSIMClientStateStart;
    }
    if (context->start_count == 0) {
	fill_with_random(context->nonce_mt, sizeof(context->nonce_mt));
    }
    context->start_count++;
    if (context->start_count > S_id_req_types_count) {
	/* can't have more then three Start packets */
	syslog(LOG_NOTICE, "eapsim: more than %d Start packets",
	       S_id_req_types_count);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    identity_type = lookup_identity_type(tlvs_p);
    switch (identity_type) {
    case kAT_ANY_ID_REQ:
	if (context->start_count > 1) {
	    syslog(LOG_NOTICE,
		   "eapsim: AT_ANY_ID_REQ at Start #%d",
		   context->start_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    case kAT_FULLAUTH_ID_REQ:
	if (context->start_count > 1
	    && context->last_identity_type != kAT_ANY_ID_REQ) {
	    syslog(LOG_NOTICE,
		   "eapsim: AT_FULLAUTH_ID_REQ follows %s at Start #%d",
		   EAPSIMAttributeTypeString(context->last_identity_type),
		   context->start_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    case kAT_PERMANENT_ID_REQ:
	if (context->start_count > 1
	    && context->last_identity_type != kAT_ANY_ID_REQ
	    && context->last_identity_type != kAT_FULLAUTH_ID_REQ) {
	    syslog(LOG_NOTICE,
		   "eapsim: AT_PERMANENT_ID_REQ follows %s at Start #%d",
		   EAPSIMAttributeTypeString(context->last_identity_type),
		   context->start_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    default:
	if (context->start_count > 1) {
	    syslog(LOG_NOTICE,
		   "eapsim: no *ID_REQ follows %s at Start #%d",
		   EAPSIMAttributeTypeString(context->last_identity_type),
		   context->start_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    }

    /* create our response */
    context->last_identity_type = identity_type;
    pkt = eapsim_make_response(context, in_pkt, kEAPSIMSubtypeStart, &tb);
    switch (identity_type) {
    case kAT_ANY_ID_REQ:
	/* if we have a fast re-auth id use it */
	if (context->reauth_id != NULL) {
	    if (TLVBufferAddIdentityString(&tb, context->reauth_id,
					   &identity_data) == FALSE) {
		*client_status = kEAPClientStatusInternalError;
		pkt = NULL;
		goto done;
	    }
	    EAPSIMContextSetLastIdentity(context, identity_data);
	    /* packet only contains fast re-auth id */
	    goto packet_complete;
	}
	/* FALL THROUGH */
    case kAT_FULLAUTH_ID_REQ:
	/* if we have a pseudonym, use it */
	if (context->sim_static.rand != NULL) {
	    identity = copy_static_identity(plugin->properties, TRUE);
	}
	else {
	    identity = sim_identity_create(plugin->properties, TRUE);
	}
	if (identity != NULL) {
	    if (TLVBufferAddIdentityString(&tb, identity, &identity_data)
		== FALSE) {
		*client_status = kEAPClientStatusInternalError;
		pkt = NULL;
		goto done;
	    }
	    EAPSIMContextSetLastIdentity(context, identity_data);
	    break;
	}
	/* FALL THROUGH */
    case kAT_PERMANENT_ID_REQ:
	/* if we think we have a valid pseudonym, we should error out XXX */
	/* use permanent id */
	if (context->sim_static.rand != NULL) {
	    identity = copy_static_identity(plugin->properties, FALSE);
	}
	else {
	    identity = sim_identity_create(plugin->properties, FALSE);
	}
	if (identity == NULL) {
	    syslog(LOG_NOTICE, "eapsim: can't find SIM identity");
	    *client_status = kEAPClientStatusResourceUnavailable;
	    pkt = NULL;
	    goto done;
	}
	if (TLVBufferAddIdentityString(&tb, identity, &identity_data)
	    == FALSE) {
	    *client_status = kEAPClientStatusInternalError;
	    pkt = NULL;
	    goto done;
	}
	EAPSIMContextSetLastIdentity(context, identity_data);
	break;
    default:
	/* no need to submit an identity */
	break;
    }

    /* set the AT_SELECTED_VERSION attribute */
    attr.tlv_p = TLVBufferAllocateTLV(&tb, kAT_SELECTED_VERSION,
				      sizeof(AT_SELECTED_VERSION));
    if (attr.tlv_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed allocating AT_SELECTED_VERSION, %s",
	       TLVBufferErrorString(&tb));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    uint16_set(attr.at_selected_version->sv_selected_version,
	       kEAPSIMVersion1);

    /* set the AT_NONCE_MT attribute */
    attr.tlv_p = TLVBufferAllocateTLV(&tb, kAT_NONCE_MT,
				      sizeof(AT_NONCE_MT));
    if (attr.tlv_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed allocating AT_NONCE_MT, %s",
	       TLVBufferErrorString(&tb));
	pkt = NULL;
	goto done;
    }
    uint16_set(attr.at_nonce_mt->nm_reserved, 0);
    bcopy(context->nonce_mt, attr.at_nonce_mt->nm_nonce_mt,
	  sizeof(context->nonce_mt));

 packet_complete:
    /* packet full formed, set the EAP packet length */
    EAPPacketSetLength(pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(&tb));

 done:
    my_CFRelease(&identity);
    my_CFRelease(&identity_data);
    return (pkt);
}

/*
 * Function: eapsim_compute_mac
 * Purpose:
 *    Compute the MAC value in the AT_MAC attribute using the
 *    specified EAP packet 'pkt' and assuming 'mac_p' points to 
 *    an area within 'pkt' that holds the MAC value.
 *
 *    This function figures out how much data comes before the 'mac_p'
 *    value and how much comes after, and feeds the before, zero-mac, and after
 *    bytes into the HMAC-SHA1 algorithm.  It also includes the 'extra'
 *    value, whose value depends on which packet is being MAC'd.
 * Returns:
 *    'hash' value is filled in with HMAC-SHA1 results.
 */
STATIC void
eapsim_compute_mac(EAPSIMKeyInfoRef key_info_p, EAPPacketRef pkt,
		   const uint8_t * mac_p, 
		   const uint8_t * extra, int extra_length,
		   uint8_t hash[CC_SHA1_DIGEST_LENGTH])
{
    int			after_mac_size;
    int			before_mac_size;
    CCHmacContext	ctx;
    int			pkt_len = EAPPacketGetLength(pkt);
    uint8_t		zero_mac[MAC_SIZE];

    bzero(&zero_mac, sizeof(zero_mac));
    before_mac_size = mac_p - (const uint8_t *)pkt;
    after_mac_size = pkt_len - (before_mac_size + sizeof(zero_mac));

    /* compute the hash */
    CCHmacInit(&ctx, kCCHmacAlgSHA1, key_info_p->s.k_aut,
	       sizeof(key_info_p->s.k_aut));
    CCHmacUpdate(&ctx, pkt, before_mac_size);
    CCHmacUpdate(&ctx, zero_mac, sizeof(zero_mac));
    CCHmacUpdate(&ctx, mac_p + sizeof(zero_mac), after_mac_size);
    if (extra != NULL) {
	CCHmacUpdate(&ctx, extra, extra_length);
    }
    CCHmacFinal(&ctx, hash);
    return;
}

STATIC bool
eapsim_verify_mac(EAPSIMContextRef context, EAPPacketRef pkt,
		  const uint8_t * mac_p,
		  const uint8_t * extra, int extra_length)
{
    uint8_t		hash[CC_SHA1_DIGEST_LENGTH];

    eapsim_compute_mac(&context->key_info, pkt, mac_p,
		       extra, extra_length, hash);
    return (bcmp(hash, mac_p, MAC_SIZE) == 0);
}

STATIC void
eapsim_set_mac(EAPSIMContextRef context, EAPPacketRef pkt,
	       uint8_t * mac_p,
	       const uint8_t * extra, int extra_length)
{
    uint8_t		hash[CC_SHA1_DIGEST_LENGTH];

    eapsim_compute_mac(&context->key_info, pkt, mac_p, extra, extra_length, 
		       hash);
    bcopy(hash, mac_p, MAC_SIZE);
    return;
}

STATIC uint8_t *
eapsim_process_encr_data(EAPSIMKeyInfoRef key_info_p,
			 AT_ENCR_DATA * encr_data_p, AT_IV * iv_p,
			 TLVListRef decrypted_tlvs_p)
{
    CCCryptorRef	cryptor = NULL;
    size_t		buf_used;
    uint8_t *		decrypted_buffer = NULL;
    int			encr_data_len;
    CCCryptorStatus 	status;
    bool		success = FALSE;

    encr_data_len = encr_data_p->ed_length * TLV_ALIGNMENT
	- offsetof(AT_ENCR_DATA, ed_encrypted_data);
    decrypted_buffer = (uint8_t *)malloc(encr_data_len);
    status = CCCryptorCreate(kCCDecrypt,
			     kCCAlgorithmAES128,
			     0,
			     key_info_p->s.k_encr,
			     sizeof(key_info_p->s.k_encr),
			     iv_p->iv_initialization_vector,
			     &cryptor);
    if (status != kCCSuccess) {
	syslog(LOG_NOTICE, "eapsim: CCCryptoCreate failed with %d",
	       status);
	goto done;
    }
    status = CCCryptorUpdate(cryptor,
			     encr_data_p->ed_encrypted_data,
			     encr_data_len,
			     decrypted_buffer,
			     encr_data_len,
			     &buf_used);
    if (status != kCCSuccess) {
	syslog(LOG_NOTICE, "eapsim: CCCryptoUpdate failed with %d",
	       status);
	goto done;
    }
    if (buf_used != encr_data_len) {
	syslog(LOG_NOTICE,
	       "eapsim: decryption consumed %d bytes (!= %d bytes)",
	       (int)buf_used, encr_data_len);
	goto done;
    }
    if (TLVListParse(decrypted_tlvs_p, decrypted_buffer, encr_data_len)
	== FALSE) {
	syslog(LOG_NOTICE,
	       "eapsim: TLVListParse failed on AT_ENCR_DATA, %s",
	       TLVListErrorString(decrypted_tlvs_p));
	goto done;
    }
    success = TRUE;

 done:
    if (cryptor != NULL) {
	status = CCCryptorRelease(cryptor);
	if (status != kCCSuccess) {
	    syslog(LOG_NOTICE, "eapsim: CCCryptoRelease failed with %d",
		   status);
	}
    }
    if (success == FALSE && decrypted_buffer != NULL) {
	free(decrypted_buffer);
	decrypted_buffer = NULL;
    }
    return (decrypted_buffer);
}

STATIC bool
eapsim_challenge_process_encr_data(EAPSIMContextRef context, TLVListRef tlvs_p)
{
    uint8_t *		decrypted_buffer = NULL;
    TLVList		decrypted_tlvs;
    AT_ENCR_DATA * 	encr_data_p;
    AT_IV * 		iv_p;
    CFStringRef		next_reauth_id;
    CFStringRef		next_pseudonym;

    TLVListInit(&decrypted_tlvs);
    encr_data_p = (AT_ENCR_DATA *)TLVListLookupAttribute(tlvs_p, kAT_ENCR_DATA);
    if (encr_data_p == NULL) {
	return (TRUE);
    }
    iv_p = (AT_IV *)TLVListLookupAttribute(tlvs_p, kAT_IV);
    if (iv_p == NULL) {
	syslog(LOG_NOTICE,
	       "eapsim: Challenge contains AT_ENCR_DATA, AT_IV missing");
	return (FALSE);
    }
    decrypted_buffer 
	= eapsim_process_encr_data(&context->key_info, encr_data_p, iv_p,
				   &decrypted_tlvs);
    if (decrypted_buffer == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed to decrypt Challenge AT_ENCR_DATA");
	return (FALSE);
    }
    printf("Decrypted TLVs:\n");
    TLVListPrint(stdout, &decrypted_tlvs);

    /* save the next fast re-auth id */
    my_CFRelease(&context->reauth_id);
    next_reauth_id = TLVListCreateStringFromAttribute(&decrypted_tlvs, 
						      kAT_NEXT_REAUTH_ID);
    if (next_reauth_id != NULL) {
	context->reauth_id = next_reauth_id;
    }

    /* save the next pseudonym */
    next_pseudonym = TLVListCreateStringFromAttribute(&decrypted_tlvs, 
						      kAT_NEXT_PSEUDONYM);
    if (next_pseudonym != NULL) {
	if (context->imsi != NULL) {
	    EAPSIMPseudonymSave(context->imsi, next_pseudonym);
	}
	CFRelease(next_pseudonym);
    }
    if (decrypted_buffer != NULL) {
	free(decrypted_buffer);
    }
    TLVListFree(&decrypted_tlvs);
    return (TRUE);
}

STATIC EAPPacketRef
eapsim_challenge(EAPClientPluginDataRef plugin, 
		 const EAPPacketRef in_pkt,
		 TLVListRef tlvs_p,
		 EAPClientStatus * client_status)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;
    int			count;
    uint8_t		kc[SIM_KC_SIZE * EAPSIM_MAX_RANDS];
    AT_MAC *		mac_p;
    EAPPacketRef	pkt = NULL;
    AT_RAND *		rand_p;
    uint16_t		selected_version = htons(kEAPSIMVersion1);
    CC_SHA1_CTX		sha1_context;
    uint8_t		sres[SIM_SRES_SIZE * EAPSIM_MAX_RANDS];
    TLVBuffer		tb;

    if (context->state != kEAPSIMClientStateStart) {
	syslog(LOG_NOTICE, "eapsim: Challenge sent without Start");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    context->state = kEAPSIMClientStateChallenge;
    context->at_counter = 1;
    rand_p = (AT_RAND *)TLVListLookupAttribute(tlvs_p, kAT_RAND);
    if (rand_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: Challenge is missing AT_RAND");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    count = ((rand_p->ra_length * TLV_ALIGNMENT) 
	     - offsetof(AT_RAND, ra_rand)) / SIM_RAND_SIZE;
    if (count < context->n_required_rands) {
	syslog(LOG_NOTICE,
	       "eapsim: Challenge AT_RAND has %d RANDs, policy requires %d",
	       count, context->n_required_rands);
	pkt = eapsim_make_client_error(context, in_pkt, 
				       kClientErrorCodeInsufficientNumberOfChallenges);

	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    /* check that there aren't more than EAPSIM_MAX_RANDS */
    if (count > EAPSIM_MAX_RANDS) {
	syslog(LOG_NOTICE,
	       "eapsim: Challenge AT_RAND has %d RANDs > %d",
	       count, EAPSIM_MAX_RANDS);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (blocks_are_duplicated(rand_p->ra_rand, count, SIM_RAND_SIZE)) {
	syslog(LOG_NOTICE,
	       "eapsim: Challenge AT_RAND has duplicate RANDs");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* get the (Kc, SRES) pairs from the SIM */
    if (EAPSIMContextSIMProcessRAND(context, rand_p->ra_rand, count, kc, sres)
	== FALSE) {
	*client_status = kEAPClientStatusInternalError;
	goto done;
    }

    /*
     * generate the MK:
     * MK = SHA1(Identity|n*Kc| NONCE_MT| Version List| Selected Version)
     */
    CC_SHA1_Init(&sha1_context);
    if (context->last_identity != NULL) {
	CC_SHA1_Update(&sha1_context, CFDataGetBytePtr(context->last_identity),
		       CFDataGetLength(context->last_identity));
    }
    else {
	CC_SHA1_Update(&sha1_context, plugin->username, 
		       plugin->username_length);
    }
    CC_SHA1_Update(&sha1_context, kc, SIM_KC_SIZE * count);
    CC_SHA1_Update(&sha1_context, context->nonce_mt, sizeof(context->nonce_mt));
    if (context->version_list != NULL) {
	CC_SHA1_Update(&sha1_context, context->version_list,
		       (sizeof(*context->version_list)
			* context->version_list_count));
    }
    else {
	CC_SHA1_Update(&sha1_context, &selected_version,
		       sizeof(selected_version));
    }
    CC_SHA1_Update(&sha1_context, &selected_version, sizeof(selected_version));
    CC_SHA1_Final(context->mk, &sha1_context);

    /* now run PRF to generate keying material */
    fips186_2prf(context->mk, context->key_info.key);

    /* validate the MAC */
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	syslog(LOG_NOTICE,
	       "eapsim: Challenge is missing AT_MAC");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (eapsim_verify_mac(context, in_pkt, mac_p->ma_mac,
			  context->nonce_mt, sizeof(context->nonce_mt))
	== FALSE) {
	syslog(LOG_NOTICE,
	       "eapsim: Challenge AT_MAC not valid");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* check for and process encrypted data */
    if (eapsim_challenge_process_encr_data(context, tlvs_p) == FALSE) {
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* create our response */
    pkt = eapsim_make_response(context, in_pkt, kEAPSIMSubtypeChallenge, &tb);
    mac_p = (AT_MAC *)TLVBufferAllocateTLV(&tb, kAT_MAC,
					   sizeof(AT_MAC));
    if (mac_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed allocating AT_MAC, %s",
	       TLVBufferErrorString(&tb));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    uint16_set(mac_p->ma_reserved, 0);
    EAPPacketSetLength(pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(&tb));
    /* compute/set the MAC value */
    eapsim_set_mac(context, pkt, mac_p->ma_mac, sres, SIM_SRES_SIZE * count);

    /* as far as we're concerned, we're successful */
    context->state = kEAPSIMClientStateSuccess;
    context->key_info_valid = TRUE;

 done:
    return (pkt);
}

STATIC bool
eapsim_encrypt(EAPSIMKeyInfoRef	key_info_p, const uint8_t * iv_p,
	       const uint8_t * clear, int size, uint8_t * encrypted)
{
    size_t		buf_used;
    CCCryptorRef	cryptor;
    bool		ret = FALSE;
    CCCryptorStatus 	status;

    status = CCCryptorCreate(kCCEncrypt,
			     kCCAlgorithmAES128,
			     0,
			     key_info_p->s.k_encr,
			     sizeof(key_info_p->s.k_encr),
			     iv_p,
			     &cryptor);
    if (status != kCCSuccess) {
	syslog(LOG_NOTICE, "eapsim: encrypt CCCryptoCreate failed with %d",
		status);
	goto done;
    }
    status = CCCryptorUpdate(cryptor, clear, size, encrypted, size, &buf_used);
    if (status != kCCSuccess) {
	syslog(LOG_NOTICE, "eapsim: encrypt CCCryptoUpdate failed with %d",
		status);
	goto done;
    }
    if (buf_used != size) {
	syslog(LOG_NOTICE,
	       "eapsim: encryption consumed %d, should have been %d",
	       (int)buf_used, size);
	goto done;
    }
    ret = TRUE;

 done:
    status = CCCryptorRelease(cryptor);
    if (status != kCCSuccess) {
	syslog(LOG_NOTICE, "eapsim: CCCryptoRelease failed with %d",
	       status);
    }
    return (ret);

}

STATIC void
eapsim_compute_reauth_key(EAPClientPluginDataRef plugin,
			  EAPSIMContextRef context,
			  AT_COUNTER * counter_p,
			  AT_NONCE_S * nonce_s_p)
{
    EAPSIMKeyInfo	key_info;
    CC_SHA1_CTX		sha1_context;
    uint8_t		xkey[CC_SHA1_DIGEST_LENGTH];

    /*
     * generate the XKEY':
     * XKEY' = SHA1(Identity|counter|NONCE_S| MK)
     */
    CC_SHA1_Init(&sha1_context);
    if (context->last_identity != NULL) {
	CC_SHA1_Update(&sha1_context, CFDataGetBytePtr(context->last_identity),
		       CFDataGetLength(context->last_identity));
    }
    else {
	CC_SHA1_Update(&sha1_context,
		       plugin->username, plugin->username_length);
    }
    CC_SHA1_Update(&sha1_context, counter_p->co_counter,
		   sizeof(counter_p->co_counter));
    CC_SHA1_Update(&sha1_context, nonce_s_p->nc_nonce_s,
		   sizeof(nonce_s_p->nc_nonce_s));
    CC_SHA1_Update(&sha1_context, context->mk, sizeof(context->mk));
    CC_SHA1_Final(xkey, &sha1_context);

    /* now run PRF to generate keying material */
    fips186_2prf(xkey, key_info.key);

    /* copy the new MSK */
    bcopy(key_info.key, 
	  context->key_info.s.msk,
	  sizeof(context->key_info.s.msk));

    /* copy the new EMSK */
    bcopy(key_info.key + sizeof(context->key_info.s.msk),
	  context->key_info.s.emsk,
	  sizeof(context->key_info.s.emsk));
    return;
}


#define ENCR_BUFSIZE 	(sizeof(AT_COUNTER) + sizeof(AT_COUNTER_TOO_SMALL))
#define ENCR_BUFSIZE_R	ENCR_DATA_ROUNDUP(ENCR_BUFSIZE)

STATIC EAPPacketRef
eapsim_reauthentication(EAPClientPluginDataRef plugin, 
			const EAPPacketRef in_pkt,
			TLVListRef tlvs_p,
			EAPClientStatus * client_status)
{
    uint16_t		at_counter;
    int			buf_used;
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;
    AT_COUNTER *	counter_p;
    bool		force_fullauth = FALSE;
    uint8_t		encr_buffer[ENCR_BUFSIZE_R];
    TLVBuffer		encr_tb;
    uint8_t *		decrypted_buffer = NULL;
    TLVList		decrypted_tlvs;
    AT_ENCR_DATA * 	encr_data_p;
    AT_IV * 		iv_p;
    AT_MAC *		mac_p;
    CFStringRef		next_reauth_id;
    int			padding_length;
    AT_NONCE_S *	nonce_s_p;
    EAPPacketRef	pkt = NULL;
    TLVBuffer		tb;

    TLVListInit(&decrypted_tlvs);
    if (context->key_info_valid == FALSE) {
	syslog(LOG_NOTICE, 
	       "eapsim: Reauthentication but no key info available");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (context->reauth_id == NULL) {
	syslog(LOG_NOTICE,
	       "eapsim: received Reauthentication but don't have reauth id");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    context->state = kEAPSIMClientStateReauthentication;
    context->plugin_state = kEAPClientStateAuthenticating;

    /* validate the MAC */
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	syslog(LOG_NOTICE,
	       "eapsim: Reauthentication is missing AT_MAC");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (eapsim_verify_mac(context, in_pkt, mac_p->ma_mac, NULL, 0) == FALSE) {
	syslog(LOG_NOTICE,
	       "eapsim:  Reauthentication AT_MAC not valid");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* packet must contain AT_ENCR_DATA, AT_IV */
    encr_data_p = (AT_ENCR_DATA *)TLVListLookupAttribute(tlvs_p, kAT_ENCR_DATA);
    iv_p = (AT_IV *)TLVListLookupAttribute(tlvs_p, kAT_IV);
    if (encr_data_p == NULL || iv_p == NULL) {
	if (encr_data_p == NULL) {
	    syslog(LOG_NOTICE,
		   "eapsim:  Reauthentication missing AT_ENCR_DATA");
	}
	if (iv_p == NULL) {
	    syslog(LOG_NOTICE,
		   "eapsim:  Reauthentication missing AT_IV");
	}
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    decrypted_buffer 
	= eapsim_process_encr_data(&context->key_info, encr_data_p, iv_p,
				   &decrypted_tlvs);
    if (decrypted_buffer == NULL) {
	syslog(LOG_NOTICE,
	       "eapsim: failed to decrypt Reauthentication AT_ENCR_DATA");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    printf("Decrypted TLVs:\n");
    TLVListPrint(stdout, &decrypted_tlvs);

    /* Reauthentication must contain AT_NONCE_S */
    nonce_s_p 
	= (AT_NONCE_S *)TLVListLookupAttribute(&decrypted_tlvs, kAT_NONCE_S);
    counter_p 
	= (AT_COUNTER *)TLVListLookupAttribute(&decrypted_tlvs, kAT_COUNTER);
    if (nonce_s_p == NULL || counter_p == NULL) {
	if (nonce_s_p == NULL) {
	    syslog(LOG_NOTICE,
		   "eapsim:  Reauthentication AT_ENCR_DATA missing AT_NONCE_S");
	}
	if (counter_p == NULL) {
	    syslog(LOG_NOTICE,
		   "eapsim:  Reauthentication AT_ENCR_DATA missing AT_COUNTER");
	}
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* check the at_counter */
    at_counter = uint16_get(counter_p->co_counter);
    if (at_counter < context->at_counter) {
	force_fullauth = TRUE;
    }
    else {
	/* save the next fast re-auth id */
	my_CFRelease(&context->reauth_id);
	next_reauth_id = TLVListCreateStringFromAttribute(&decrypted_tlvs, 
							  kAT_NEXT_REAUTH_ID);
	if (next_reauth_id != NULL) {
	    context->reauth_id = next_reauth_id;
	}
	context->at_counter = at_counter;
    }
    
    /* create our response */
    pkt = eapsim_make_response(context, in_pkt, kEAPSIMSubtypeReauthentication,
			       &tb);

    /* AT_IV */
    iv_p = (AT_IV *) TLVBufferAllocateTLV(&tb, kAT_IV, sizeof(AT_IV));
    if (iv_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed to allocate AT_IV, %s",
	       TLVBufferErrorString(&tb));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    uint16_set(iv_p->iv_reserved, 0);
    fill_with_random(iv_p->iv_initialization_vector, 
		     sizeof(iv_p->iv_initialization_vector));

    /* 
     * create nested attributes containing:
     * 	AT_COUNTER
     *  AT_COUNTER_TOO_SMALL (if necessary)
     *  AT_PADDING.
     */
    TLVBufferInit(&encr_tb, encr_buffer, sizeof(encr_buffer));
    if (TLVBufferAddCounter(&encr_tb, at_counter) == FALSE) {
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    if (force_fullauth
	&& TLVBufferAddCounterTooSmall(&encr_tb) == FALSE) {
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    buf_used = TLVBufferUsed(&encr_tb);
    padding_length = ENCR_DATA_ROUNDUP(buf_used) - buf_used;
    if (padding_length != 0
	&& TLVBufferAddPadding(&encr_tb, padding_length) == FALSE) {
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    if (TLVBufferUsed(&encr_tb) != ENCR_BUFSIZE_R) {
	syslog(LOG_NOTICE, "eapsim: Reauthentication %d != %d",
	       TLVBufferUsed(&encr_tb), ENCR_BUFSIZE_R);
    }
    {
	TLVList		temp;

	TLVListInit(&temp);
	if (TLVListParse(&temp, encr_buffer,
			 TLVBufferUsed(&encr_tb)) == FALSE) {
	    syslog(LOG_NOTICE, "eapsim: Reauthentication "
		   "nested TLVs TLVListParse failed, %s",
		   TLVListErrorString(&temp));
	    *client_status = kEAPClientStatusInternalError;
	    pkt = NULL;
	    goto done;
	}
	else {
	    printf("Encrypted TLVs:\n");
	    TLVListPrint(stdout, &temp); 
	}
	TLVListFree(&temp);
    }
    /* AT_ENCR_DATA */
    encr_data_p = (AT_ENCR_DATA *)
	TLVBufferAllocateTLV(&tb, kAT_ENCR_DATA,
			     offsetof(AT_ENCR_DATA, ed_encrypted_data)
			     + TLVBufferUsed(&encr_tb));
    if (encr_data_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed to allocate AT_ENCR_DATA, %s",
	       TLVBufferErrorString(&encr_tb));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    uint16_set(encr_data_p->ed_reserved, 0);
    if (eapsim_encrypt(&context->key_info, iv_p->iv_initialization_vector,
		       encr_buffer, TLVBufferUsed(&encr_tb),
		       encr_data_p->ed_encrypted_data) == FALSE) {
	syslog(LOG_NOTICE, "eapsim: failed to encrypt AT_ENCR_DATA");
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }

    /* AT_MAC */
    mac_p = (AT_MAC *)TLVBufferAllocateTLV(&tb, kAT_MAC,
					   sizeof(AT_MAC));
    if (mac_p == NULL) {
	syslog(LOG_NOTICE, "eapsim: failed allocating AT_MAC, %s",
	       TLVBufferErrorString(&tb));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    uint16_set(mac_p->ma_reserved, 0);

    /* set the packet length */
    EAPPacketSetLength(pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(&tb));

    /* compute/set the MAC value */
    eapsim_set_mac(context, pkt, mac_p->ma_mac, nonce_s_p->nc_nonce_s,
		   sizeof(nonce_s_p->nc_nonce_s));

    if (force_fullauth == FALSE) {
	/* as far as we're concerned, we're successful */
	context->state = kEAPSIMClientStateSuccess;
	eapsim_compute_reauth_key(plugin, context, counter_p, nonce_s_p);
	context->key_info_valid = TRUE;
    }
    else {
	context->key_info_valid = FALSE;
    }

 done:
    if (decrypted_buffer != NULL) {
	free(decrypted_buffer);
    }
    TLVListFree(&decrypted_tlvs);
    return (pkt);
}

STATIC EAPPacketRef
eapsim_request(EAPClientPluginDataRef plugin, 
	       const EAPPacketRef in_pkt,
	       EAPClientStatus * client_status)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;
    EAPSIMPacketRef	eapsim_in = (EAPSIMPacketRef)in_pkt;
    EAPPacketRef	eapsim_out = NULL;
    uint16_t		in_length = EAPPacketGetLength(in_pkt);
    uint8_t		subtype;
    TLVList		tlvs;

    TLVListInit(&tlvs);
    if (in_length <= EAPSIM_HEADER_LENGTH) {
	syslog(LOG_NOTICE, "eapsim_request: length %d <= %d",
	       in_length, EAPSIM_HEADER_LENGTH);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (TLVListParse(&tlvs, eapsim_in->attrs,
		     in_length - EAPSIM_HEADER_LENGTH) == FALSE) {
	syslog(LOG_NOTICE, "eapsim_request: parse failed: %s",
	       TLVListErrorString(&tlvs));
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (context->state != kEAPSIMClientStateNone
	&& context->previous_identifier == in_pkt->identifier) {
	/* re-send our previous response */
	return ((EAPPacketRef)context->pkt);
    }
    subtype = eapsim_in->subtype;
    switch (subtype) {
    case kEAPSIMSubtypeStart:
	eapsim_out = eapsim_start(plugin, in_pkt, &tlvs, client_status);
	break;
    case kEAPSIMSubtypeChallenge:
	eapsim_out = eapsim_challenge(plugin, in_pkt, &tlvs, client_status);
	break;
    case kEAPSIMSubtypeNotification:
	break;
    case kEAPSIMSubtypeReauthentication:
	eapsim_out 
	    = eapsim_reauthentication(plugin, in_pkt, &tlvs, client_status);
	break;
    default:
	*client_status = kEAPClientStatusProtocolError;
	syslog(LOG_NOTICE, "eapsim_request: unexpected Subtype %s",
	       EAPSIMSubtypeString(subtype));
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

 done:
    TLVListFree(&tlvs);
    if (*client_status != kEAPClientStatusOK) {
	context->plugin_state = kEAPClientStateFailure;
	context->state = kEAPSIMClientStateFailure;
    }
    if (eapsim_out == NULL
	&& *client_status == kEAPClientStatusProtocolError) {
	eapsim_out 
	    = eapsim_make_client_error(context, in_pkt,
				       kClientErrorCodeUnableToProcessPacket);
    }
    if (eapsim_out != NULL) {
	context->previous_identifier = in_pkt->identifier;
    }
    return (eapsim_out);

}

STATIC EAPClientState
eapsim_process(EAPClientPluginDataRef plugin, 
		const EAPPacketRef in_pkt,
		EAPPacketRef * out_pkt_p, 
		EAPClientStatus * client_status,
		EAPClientDomainSpecificError * error)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;

    *client_status = kEAPClientStatusOK;
    *error = 0;
    switch (in_pkt->code) {
    case kEAPCodeRequest:
	*out_pkt_p = eapsim_request(plugin, in_pkt, client_status);
	break;
    case kEAPCodeSuccess:
	context->previous_identifier = -1;
	if (context->state == kEAPSIMClientStateSuccess) {
	    context->plugin_state = kEAPClientStateSuccess;
	}
	break;
    case kEAPCodeFailure:
	context->previous_identifier = -1;
	if (context->state == kEAPSIMClientStateFailure) {
	    context->plugin_state = kEAPClientStateFailure;
	}
	break;
    default:
	break;
    }
    return (context->plugin_state);
}

STATIC const char * 
eapsim_failure_string(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

STATIC void * 
eapsim_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;

    if (context->key_info_valid) {
	*key_length = 32;
	return (context->key_info.s.msk);
    }
    return (NULL);
}

STATIC void * 
eapsim_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;

    if (context->key_info_valid) {
	*key_length = 32;
	return (context->key_info.s.msk + 32);
    }
    return (NULL);
}

STATIC CFDictionaryRef
eapsim_publish_props(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

STATIC CFStringRef
eapsim_user_name(CFDictionaryRef properties)
{
    CFStringRef		ret_identity;

    ret_identity = copy_static_identity(properties, TRUE);
    if (ret_identity != NULL) {
	return (ret_identity);
    }
    ret_identity = sim_identity_create(properties, TRUE);
    return (ret_identity);
}

/*
 * Function: eapsim_copy_identity
 * Purpose:
 *   Return the current identity we should use in responding to an
 *   EAP Request Identity packet.
 */
STATIC CFStringRef
eapsim_copy_identity(EAPClientPluginDataRef plugin)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;
    CFStringRef		ret = NULL;

    EAPSIMContextSetLastIdentity(context, NULL);
    if (context->sim_static.rand != NULL) {
	return (NULL);
    }
    if (context->reauth_id != NULL) {
	ret = CFRetain(context->reauth_id);
    }
    else {
	ret = sim_identity_create(plugin->properties, TRUE);
    }
    return (ret);
}

STATIC bool
eapsim_packet_dump(FILE * out_f, const EAPPacketRef pkt)
{
    int			attrs_length;
    EAPSIMPacketRef	eapsim = (EAPSIMPacketRef)pkt;
    uint16_t		length = EAPPacketGetLength(pkt);
    TLVList		tlvs;

    switch (pkt->code) {
    case kEAPCodeRequest:
    case kEAPCodeResponse:
	break;
    default:
	/* just return */
	return (FALSE);
    }
    if (length <= EAPSIM_HEADER_LENGTH) {
	fprintf(out_f, "invalid packet: length %d <= min length %d\n",
		length, (int)EAPSIM_HEADER_LENGTH);
	return (FALSE);
    }
    attrs_length = length - EAPSIM_HEADER_LENGTH;
    fprintf(out_f,
	    "EAP-SIM %s: Identifier %d Length %d [%s] Length %d\n",
	    pkt->code == kEAPCodeRequest ? "Request" : "Response",
	    pkt->identifier, length, 
	    EAPSIMSubtypeString(eapsim->subtype), attrs_length);
    // fprint_data(out_f, eapsim->attrs, attrs_length);
    TLVListInit(&tlvs);
    if (TLVListParse(&tlvs, eapsim->attrs, attrs_length) == FALSE) {
	fprintf(out_f, "failed to parse TLVs: %s\n",
		TLVListErrorString(&tlvs));
	return (FALSE);
    }
    TLVListPrint(out_f, &tlvs);
    TLVListFree(&tlvs);
    return (TRUE);
}

STATIC EAPType 
eapsim_type()
{
    return (kEAPTypeEAPSIM);

}

STATIC const char *
eapsim_name()
{
    return (EAP_SIM_NAME);

}

STATIC EAPClientPluginVersion 
eapsim_version()
{
    return (kEAPClientPluginVersion);
}

STATIC struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
#if 0
    { kEAPClientPluginFuncNameIntrospect, eapsim_introspect },
#endif 0
    { kEAPClientPluginFuncNameVersion, eapsim_version },
    { kEAPClientPluginFuncNameEAPType, eapsim_type },
    { kEAPClientPluginFuncNameEAPName, eapsim_name },
    { kEAPClientPluginFuncNameInit, eapsim_init },
    { kEAPClientPluginFuncNameFree, eapsim_free },
    { kEAPClientPluginFuncNameProcess, eapsim_process },
    { kEAPClientPluginFuncNameFreePacket, eapsim_free_packet },
    { kEAPClientPluginFuncNameFailureString, eapsim_failure_string },
    { kEAPClientPluginFuncNameSessionKey, eapsim_session_key },
    { kEAPClientPluginFuncNameServerKey, eapsim_server_key },
    { kEAPClientPluginFuncNamePublishProperties, eapsim_publish_props },
    { kEAPClientPluginFuncNamePacketDump, eapsim_packet_dump },
    { kEAPClientPluginFuncNameUserName, eapsim_user_name },
    { kEAPClientPluginFuncNameCopyIdentity, eapsim_copy_identity },
    { NULL, NULL},
};


EAPClientPluginFuncRef
eapsim_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;

    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}

#ifdef TEST_TLVLIST_PARSE
/*
A.3.  EAP-Request/SIM/Start

   The server's first packet looks like this:

      01                   ; Code: Request
      01                   ; Identifier: 1
      00 10                ; Length: 16 octets
      12                   ; Type: EAP-SIM
         0a                ; EAP-SIM subtype: Start
         00 00             ; (reserved)
         0f                ; Attribute type: AT_VERSION_LIST
            02             ; Attribute length: 8 octets (2*4)
            00 02          ; Actual version list length: 2 octets
            00 01          ; Version: 1
            00 00          ; (attribute padding)
*/
const uint8_t	eap_request_sim_start[] = {
      0x01,                   
      0x01,                   
      0x00, 0x10,                
      0x12,                   
         0x0a,                
         0x00, 0x00,             
         0x0f,                
            0x02,             
            0x00, 0x02,          
            0x00, 0x01,          
            0x00, 0x00,          
};

/*
A.4.  EAP-Response/SIM/Start

   The client selects a nonce and responds with the following packet:

      02                   ; Code: Response
      01                   ; Identifier: 1
      00 20                ; Length: 32 octets
      12                   ; Type: EAP-SIM
         0a                ; EAP-SIM subtype: Start
         00 00             ; (reserved)
         07                ; Attribute type: AT_NONCE_MT
            05             ; Attribute length: 20 octets (5*4)
            00 00          ; (reserved)
            01 23 45 67    ; NONCE_MT value
            89 ab cd ef
            fe dc ba 98
            76 54 32 10
         10                ; Attribute type: AT_SELECTED_VERSION
            01             ; Attribute length: 4 octets (1*4)
            00 01          ; Version: 1

*/
const uint8_t	eap_response_sim_start[] = {
    0x02,
    0x01,
    0x00, 0x20,
    0x12,
    0x0a,
    0x00, 0x00,
    0x07,
    0x05,
    0x00, 0x00,
    0x01, 0x23, 0x45, 0x67,
    0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98,
    0x76, 0x54, 0x32, 0x10,
    0x10,
    0x01,
    0x00, 0x01
};

/*
   The EAP packet looks like this:

      01                   ; Code: Request
      02                   ; Identifier: 2
      01 18                ; Length: 280 octets
      12                   ; Type: EAP-SIM
         0b                ; EAP-SIM subtype: Challenge
         00 00             ; (reserved)
         01                ; Attribute type: AT_RAND
            0d             ; Attribute length: 52 octets (13*4)
            00 00          ; (reserved)
            10 11 12 13    ; first RAND
            14 15 16 17
            18 19 1a 1b
            1c 1d 1e 1f
            20 21 22 23    ; second RAND
            24 25 26 27
            28 29 2a 2b
            2c 2d 2e 2f
            30 31 32 33    ; third RAND
            34 35 36 37
            38 39 3a 3b
            3c 3d 3e 3f
         81                ; Attribute type: AT_IV
            05             ; Attribute length: 20 octets (5*4)
            00 00          ; (reserved)
            9e 18 b0 c2    ; IV value
            9a 65 22 63
            c0 6e fb 54
            dd 00 a8 95
         82               ; Attribute type: AT_ENCR_DATA
            2d            ; Attribute length: 180 octets (45*4)
            00 00         ; (reserved)
            55 f2 93 9b bd b1 b1 9e a1 b4 7f c0 b3 e0 be 4c
            ab 2c f7 37 2d 98 e3 02 3c 6b b9 24 15 72 3d 58
            ba d6 6c e0 84 e1 01 b6 0f 53 58 35 4b d4 21 82
            78 ae a7 bf 2c ba ce 33 10 6a ed dc 62 5b 0c 1d
            5a a6 7a 41 73 9a e5 b5 79 50 97 3f c7 ff 83 01
            07 3c 6f 95 31 50 fc 30 3e a1 52 d1 e1 0a 2d 1f
            4f 52 26 da a1 ee 90 05 47 22 52 bd b3 b7 1d 6f
            0c 3a 34 90 31 6c 46 92 98 71 bd 45 cd fd bc a6
            11 2f 07 f8 be 71 79 90 d2 5f 6d d7 f2 b7 b3 20
            bf 4d 5a 99 2e 88 03 31 d7 29 94 5a ec 75 ae 5d
            43 c8 ed a5 fe 62 33 fc ac 49 4e e6 7a 0d 50 4d
         0b                ; Attribute type: AT_MAC
            05             ; Attribute length: 20 octets (5*4)
            00 00          ; (reserved)
            fe f3 24 ac    ; MAC value
            39 62 b5 9f
            3b d7 82 53
            ae 4d cb 6a
*/
const uint8_t	eap_request_fast_reauth[] = {
      0x01,                   
      0x02,                   
      0x01, 0x18,                
      0x12,                   
         0x0b,                
         0x00, 0x00,             
         0x01,                
            0x0d,             
            0x00, 0x00,          
            0x10, 0x11, 0x12, 0x13,    
            0x14, 0x15, 0x16, 0x17,
            0x18, 0x19, 0x1a, 0x1b,
            0x1c, 0x1d, 0x1e, 0x1f,
            0x20, 0x21, 0x22, 0x23,    
            0x24, 0x25, 0x26, 0x27,
            0x28, 0x29, 0x2a, 0x2b,
            0x2c, 0x2d, 0x2e, 0x2f,
            0x30, 0x31, 0x32, 0x33,    
            0x34, 0x35, 0x36, 0x37,
            0x38, 0x39, 0x3a, 0x3b,
      	    0x3c, 0x3d, 0x3e, 0x3f,
         0x81,                
            0x05,             
            0x00, 0x00,          
            0x9e, 0x18, 0xb0, 0xc2,    
            0x9a, 0x65, 0x22, 0x63,
            0xc0, 0x6e, 0xfb, 0x54,
            0xdd, 0x00, 0xa8, 0x95,
         0x82,               
            0x2d,            
            0x00, 0x00,         
            0x55, 0xf2, 0x93, 0x9b, 0xbd, 0xb1, 0xb1, 0x9e, 0xa1, 0xb4, 0x7f, 0xc0, 0xb3, 0xe0, 0xbe, 0x4c,
            0xab, 0x2c, 0xf7, 0x37, 0x2d, 0x98, 0xe3, 0x02, 0x3c, 0x6b, 0xb9, 0x24, 0x15, 0x72, 0x3d, 0x58,
            0xba, 0xd6, 0x6c, 0xe0, 0x84, 0xe1, 0x01, 0xb6, 0x0f, 0x53, 0x58, 0x35, 0x4b, 0xd4, 0x21, 0x82,
            0x78, 0xae, 0xa7, 0xbf, 0x2c, 0xba, 0xce, 0x33, 0x10, 0x6a, 0xed, 0xdc, 0x62, 0x5b, 0x0c, 0x1d,
            0x5a, 0xa6, 0x7a, 0x41, 0x73, 0x9a, 0xe5, 0xb5, 0x79, 0x50, 0x97, 0x3f, 0xc7, 0xff, 0x83, 0x01,
            0x07, 0x3c, 0x6f, 0x95, 0x31, 0x50, 0xfc, 0x30, 0x3e, 0xa1, 0x52, 0xd1, 0xe1, 0x0a, 0x2d, 0x1f,
            0x4f, 0x52, 0x26, 0xda, 0xa1, 0xee, 0x90, 0x05, 0x47, 0x22, 0x52, 0xbd, 0xb3, 0xb7, 0x1d, 0x6f,
            0x0c, 0x3a, 0x34, 0x90, 0x31, 0x6c, 0x46, 0x92, 0x98, 0x71, 0xbd, 0x45, 0xcd, 0xfd, 0xbc, 0xa6,
            0x11, 0x2f, 0x07, 0xf8, 0xbe, 0x71, 0x79, 0x90, 0xd2, 0x5f, 0x6d, 0xd7, 0xf2, 0xb7, 0xb3, 0x20,
            0xbf, 0x4d, 0x5a, 0x99, 0x2e, 0x88, 0x03, 0x31, 0xd7, 0x29, 0x94, 0x5a, 0xec, 0x75, 0xae, 0x5d,
            0x43, 0xc8, 0xed, 0xa5, 0xfe, 0x62, 0x33, 0xfc, 0xac, 0x49, 0x4e, 0xe6, 0x7a, 0x0d, 0x50, 0x4d,
         0x0b,                
            0x05,             
            0x00, 0x00,          
            0xfe, 0xf3, 0x24, 0xac,    
            0x39, 0x62, 0xb5, 0x9f,
            0x3b, 0xd7, 0x82, 0x53,
            0xae, 0x4d, 0xcb, 0x6a 
	  };

/*
   The following plaintext will be encrypted and stored in the
   AT_ENCR_DATA attribute:

         84               ; Attribute type: AT_NEXT_PSEUDONYM
            13            ; Attribute length: 76 octets (19*4)
            00 46         ; Actual pseudonym length: 70 octets
            77 38 77 34 39 50 65 78 43 61 7a 57 4a 26 78 43
            49 41 52 6d 78 75 4d 4b 68 74 35 53 31 73 78 52
            44 71 58 53 45 46 42 45 67 33 44 63 5a 50 39 63
            49 78 54 65 35 4a 34 4f 79 49 77 4e 47 56 7a 78
            65 4a 4f 55 31 47
            00 00          ; (attribute padding)
         85                ; Attribute type: AT_NEXT_REAUTH_ID
            16             ; Attribute length: 88 octets (22*4)
            00 51          ; Actual re-auth identity length: 81 octets
            59 32 34 66 4e 53 72 7a 38 42 50 32 37 34 6a 4f
            4a 61 46 31 37 57 66 78 49 38 59 4f 37 51 58 30
            30 70 4d 58 6b 39 58 4d 4d 56 4f 77 37 62 72 6f
            61 4e 68 54 63 7a 75 46 71 35 33 61 45 70 4f 6b
            6b 33 4c 30 64 6d 40 65 61 70 73 69 6d 2e 66 6f
            6f
            00 00 00       ; (attribute padding)
         06                ; Attribute type: AT_PADDING
            03             ; Attribute length: 12 octets (3*4)
            00 00 00 00
            00 00 00 00
            00 00

*/
const uint8_t	at_encr_attr[] = {
    /* faked out to look like packet */
      0x01,                   
      0x02,                   
      0x00, 0xb8,                
      0x12,                   
         0x0b,                
         0x00, 0x00,             
      /* attrs */
         0x84,               
            0x13,            
            0x00, 0x46,         
            0x77, 0x38, 0x77, 0x34, 0x39, 0x50, 0x65, 0x78, 0x43, 0x61, 0x7a, 0x57, 0x4a, 0x26, 0x78, 0x43,
            0x49, 0x41, 0x52, 0x6d, 0x78, 0x75, 0x4d, 0x4b, 0x68, 0x74, 0x35, 0x53, 0x31, 0x73, 0x78, 0x52,
            0x44, 0x71, 0x58, 0x53, 0x45, 0x46, 0x42, 0x45, 0x67, 0x33, 0x44, 0x63, 0x5a, 0x50, 0x39, 0x63,
            0x49, 0x78, 0x54, 0x65, 0x35, 0x4a, 0x34, 0x4f, 0x79, 0x49, 0x77, 0x4e, 0x47, 0x56, 0x7a, 0x78,
            0x65, 0x4a, 0x4f, 0x55, 0x31, 0x47,
            0x00, 0x00,          
         0x85,                
            0x16,             
            0x00, 0x51,          
            0x59, 0x32, 0x34, 0x66, 0x4e, 0x53, 0x72, 0x7a, 0x38, 0x42, 0x50, 0x32, 0x37, 0x34, 0x6a, 0x4f,
            0x4a, 0x61, 0x46, 0x31, 0x37, 0x57, 0x66, 0x78, 0x49, 0x38, 0x59, 0x4f, 0x37, 0x51, 0x58, 0x30,
            0x30, 0x70, 0x4d, 0x58, 0x6b, 0x39, 0x58, 0x4d, 0x4d, 0x56, 0x4f, 0x77, 0x37, 0x62, 0x72, 0x6f,
            0x61, 0x4e, 0x68, 0x54, 0x63, 0x7a, 0x75, 0x46, 0x71, 0x35, 0x33, 0x61, 0x45, 0x70, 0x4f, 0x6b,
            0x6b, 0x33, 0x4c, 0x30, 0x64, 0x6d, 0x40, 0x65, 0x61, 0x70, 0x73, 0x69, 0x6d, 0x2e, 0x66, 0x6f,
            0x6f,
            0x00, 0x00, 0x00,       
         0x06,                
            0x03,             
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00,
};

const uint8_t	at_permanent_id_req[] = {
      0x01,                   
      0x02,                   
      0x00, 0x0c,                
      0x12,                   
         0x0b,                
         0x00, 0x00,             
      	/* PERMANENT_ID_REQ */
         0x0a,                
            0x01,             
            0x00, 0x00
};

const uint8_t	bad_padding1[] = {
      0x01,                   
      0x02,                   
      0x00, 0x0f,                
      0x12,                   
         0x0b,                
         0x00, 0x00,             
      /* PADDING */
         0x06,                
            0x03,             
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
	    0x00, 0x10,

};

const uint8_t	bad_padding2[] = {
      0x01,                   
      0x02,                   
      0x00, 0x14,                
      0x12,                   
         0x0b,                
         0x00, 0x00,             
      /* PADDING */
         0x06,                
            0x03,             
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
	    0x00, 0x10,

};

const uint8_t	bad_padding3[] = {
      0x01,                   
      0x02,                   
      0x00, 0x18,                
      0x12,                   
         0x0b,                
         0x00, 0x00,             
      /* PADDING */
         0x06,                
            0x04,             
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00
};

const uint8_t	bad_at_encr_attr[] = {
    /* faked out to look like packet */
      0x01,                   
      0x02,                   
      0x00, 0xb8,                
      0x12,                   
         0x0b,                
         0x00, 0x00,             
      /* attrs */
         0x84,               
            0x13,            
            0x00, 0x4a,         
            0x77, 0x38, 0x77, 0x34, 0x39, 0x50, 0x65, 0x78, 0x43, 0x61, 0x7a, 0x57, 0x4a, 0x26, 0x78, 0x43,
            0x49, 0x41, 0x52, 0x6d, 0x78, 0x75, 0x4d, 0x4b, 0x68, 0x74, 0x35, 0x53, 0x31, 0x73, 0x78, 0x52,
            0x44, 0x71, 0x58, 0x53, 0x45, 0x46, 0x42, 0x45, 0x67, 0x33, 0x44, 0x63, 0x5a, 0x50, 0x39, 0x63,
            0x49, 0x78, 0x54, 0x65, 0x35, 0x4a, 0x34, 0x4f, 0x79, 0x49, 0x77, 0x4e, 0x47, 0x56, 0x7a, 0x78,
            0x65, 0x4a, 0x4f, 0x55, 0x31, 0x47,
            0x00, 0x00,          
         0x85,                
            0x16,             
            0x00, 0x51,          
            0x59, 0x32, 0x34, 0x66, 0x4e, 0x53, 0x72, 0x7a, 0x38, 0x42, 0x50, 0x32, 0x37, 0x34, 0x6a, 0x4f,
            0x4a, 0x61, 0x46, 0x31, 0x37, 0x57, 0x66, 0x78, 0x49, 0x38, 0x59, 0x4f, 0x37, 0x51, 0x58, 0x30,
            0x30, 0x70, 0x4d, 0x58, 0x6b, 0x39, 0x58, 0x4d, 0x4d, 0x56, 0x4f, 0x77, 0x37, 0x62, 0x72, 0x6f,
            0x61, 0x4e, 0x68, 0x54, 0x63, 0x7a, 0x75, 0x46, 0x71, 0x35, 0x33, 0x61, 0x45, 0x70, 0x4f, 0x6b,
            0x6b, 0x33, 0x4c, 0x30, 0x64, 0x6d, 0x40, 0x65, 0x61, 0x70, 0x73, 0x69, 0x6d, 0x2e, 0x66, 0x6f,
            0x6f,
            0x00, 0x00, 0x00,       
         0x06,                
            0x03,             
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00,
};

struct {
    const uint8_t *	packet;
    int			size;
    bool		good;
    const char *	name;
} packets[] = {
    { eap_request_sim_start, sizeof(eap_request_sim_start), TRUE, "eap_request_sim_start" },
    { eap_response_sim_start, sizeof(eap_response_sim_start), TRUE, "eap_response_sim_start" },
    { eap_request_fast_reauth, sizeof(eap_request_fast_reauth), TRUE, "eap_request_fast_reauth" },
    { at_encr_attr, sizeof(at_encr_attr), TRUE, "at_encr_attr" },
    { at_permanent_id_req, sizeof(at_permanent_id_req), TRUE, "at_permanent_id_req" },
    { bad_padding1, sizeof(bad_padding1), FALSE, "bad_padding1" },
    { bad_padding2, sizeof(bad_padding2), FALSE, "bad_padding2" },
    { bad_padding3, sizeof(bad_padding3), FALSE, "bad_padding3" },
    { bad_at_encr_attr, sizeof(bad_at_encr_attr), FALSE, "bad_at_encr_attr" },
    { NULL, 0 }
};

int
main(int argc, char * argv[])
{
    AttrUnion			attr;
    int				i;
    EAPSIMPacketRef		pkt;
    uint8_t			buf[1028];
    TLVBuffer			tlv_buf;

    for (i = 0; packets[i].packet != NULL; i++) {
	bool	good;

	pkt = (EAPSIMPacketRef)packets[i].packet;
	good = eapsim_packet_dump(stdout, (EAPPacketRef)pkt);
	printf("Test %d '%s' %s (found%serrors)\n", i,
	       packets[i].name,
	       good == packets[i].good ? "PASSED" : "FAILED",
	       !good ? " " : " no ");
	printf("\n");
    }
    pkt = (EAPSIMPacketRef)buf;
    TLVBufferInit(&tlv_buf, pkt->attrs,
		  sizeof(buf) - offsetof(EAPSIMPacket, attrs));
    attr.tlv_p = TLVBufferAllocateTLV(&tlv_buf, kAT_SELECTED_VERSION,
				      sizeof(AT_SELECTED_VERSION));
    if (attr.tlv_p == NULL) {
	fprintf(stderr, "failed allocating AT_SELECTED_VERSION, %s\n",
		TLVBufferErrorString(&tlv_buf));
	exit(2);
    }
    uint16_set(attr.at_selected_version->sv_selected_version,
	       kEAPSIMVersion1);
    pkt->code = kEAPCodeResponse;
    pkt->identifier = 1;
    pkt->type = kEAPTypeEAPSIM;
    pkt->subtype = kEAPSIMSubtypeStart;
    EAPPacketSetLength((EAPPacketRef)pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(&tlv_buf));
    if (eapsim_packet_dump(stdout, (EAPPacketRef)pkt) == FALSE) {
	fprintf(stderr, "Parse failed!\n");
	exit(2);
    }

    exit(0);
    return (0);
}

#endif /* TEST_TLVLIST_PARSE */

#ifdef TEST_RAND_DUPS
const uint8_t	randval1[3 * SIM_RAND_SIZE] = {
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf
};

const uint8_t	randval2[3 * SIM_RAND_SIZE] = {
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0x10, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf
};

const uint8_t	randval3[3 * SIM_RAND_SIZE] = {
    0x10, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf
};

const uint8_t	randval4[3 * SIM_RAND_SIZE] = {
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0x10, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf,
    0x20, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf
};

struct {
    const uint8_t *	block;
    int			size;
    bool		duplicated;
} rands[] = {
    { randval1, sizeof(randval1), TRUE },
    { randval2, sizeof(randval2), TRUE },
    { randval3, sizeof(randval3), TRUE },
    { randval4, sizeof(randval4), FALSE },
    { NULL, 0 }
};

int
main()
{
    int		i;

    for (i = 0; rands[i].block != NULL; i++) {
	bool	duplicated;

	duplicated = blocks_are_duplicated(rands[i].block,
					   rands[i].size / SIM_RAND_SIZE,
					   SIM_RAND_SIZE);
	if (duplicated == rands[i].duplicated) {
	    printf("Test %d passed (found%sduplicate)\n", i,
		   duplicated ? " " : " no ");
	}
	else {
	    printf("Test %d failed\n", i);
	}
    }
    exit(0);
    return (0);
}

#endif /* TEST_RAND_DUPS */

#ifdef TEST_SIM_CRYPTO

typedef struct {
    uint8_t	Kc[SIM_KC_SIZE];
} SIMKc, *SIMKcRef;

typedef struct {
    uint8_t	SRES[SIM_SRES_SIZE];
} SIMSRES, *SIMSRESRef;

typedef struct {
    uint8_t	RAND[SIM_RAND_SIZE];
} SIMRAND, *SIMRANDRef;

/*
A.5.  EAP-Request/SIM/Challenge

   Next, the server selects three authentication triplets

         (RAND1,SRES1,Kc1) = (10111213 14151617 18191a1b 1c1d1e1f,
                              d1d2d3d4,
                              a0a1a2a3 a4a5a6a7)
         (RAND2,SRES2,Kc2) = (20212223 24252627 28292a2b 2c2d2e2f,
                              e1e2e3e4,
                              b0b1b2b3 b4b5b6b7)
         (RAND3,SRES3,Kc3) = (30313233 34353637 38393a3b 3c3d3e3f,
                              f1f2f3f4,
                              c0c1c2c3 c4c5c6c7)
 */

const SIMKc	test_kc[EAPSIM_MAX_RANDS] = {
    { { 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7 } },
    { { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7 } }, 
    { { 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7 } },
};

const SIMSRES	test_sres[EAPSIM_MAX_RANDS] = {
    { { 0xd1, 0xd2, 0xd3, 0xd4 } },
    { { 0xe1, 0xe2, 0xe3, 0xe4 } },
    { { 0xf1, 0xf2, 0xf3, 0xf4 } }
};

const SIMRAND test_rand[EAPSIM_MAX_RANDS] = {
    { { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f } },
    { { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f } } ,
    { { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
	0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f } }
};

const uint8_t 	test_nonce_mt[NONCE_MT_SIZE] = {
    0x01, 0x23, 0x45, 0x67,    
    0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98,
    0x76, 0x54, 0x32, 0x10
};
const uint8_t	test_identity[] = "1244070100000001@eapsim.foo";

/*
   Next, the MK is calculated as specified in Section 7*.

   MK = e576d5ca 332e9930 018bf1ba ee2763c7 95b3c712

   And the other keys are derived using the PRNG:

         K_encr = 536e5ebc 4465582a a6a8ec99 86ebb620
         K_aut =  25af1942 efcbf4bc 72b39434 21f2a974
         MSK =    39d45aea f4e30601 983e972b 6cfd46d1
                  c3637733 65690d09 cd44976b 525f47d3
                  a60a985e 955c53b0 90b2e4b7 3719196a
                  40254296 8fd14a88 8f46b9a7 886e4488
         EMSK =   5949eab0 fff69d52 315c6c63 4fd14a7f
                  0d52023d 56f79698 fa6596ab eed4f93f
                  bb48eb53 4d985414 ceed0d9a 8ed33c38
                  7c9dfdab 92ffbdf2 40fcecf6 5a2c93b9
 */

const uint8_t	test_mk[CC_SHA1_DIGEST_LENGTH] = {
    0xe5, 0x76, 0xd5, 0xca, 
    0x33, 0x2e, 0x99, 0x30,
    0x01, 0x8b, 0xf1, 0xba,
    0xee, 0x27, 0x63, 0xc7,
    0x95, 0xb3, 0xc7, 0x12 
};

uint8_t key_block[EAPSIM_KEY_SIZE] = {
    0x53, 0x6e, 0x5e, 0xbc, 0x44, 0x65, 0x58, 0x2a, 0xa6, 0xa8, 0xec, 0x99,  0x86, 0xeb, 0xb6, 0x20,
    0x25, 0xaf, 0x19, 0x42, 0xef, 0xcb, 0xf4, 0xbc, 0x72, 0xb3, 0x94, 0x34,  0x21, 0xf2, 0xa9, 0x74,
    0x39, 0xd4, 0x5a, 0xea, 0xf4, 0xe3, 0x06, 0x01, 0x98, 0x3e, 0x97, 0x2b,  0x6c, 0xfd, 0x46, 0xd1,
    0xc3, 0x63, 0x77, 0x33, 0x65, 0x69, 0x0d, 0x09, 0xcd, 0x44, 0x97, 0x6b,  0x52, 0x5f, 0x47, 0xd3,
    0xa6, 0x0a, 0x98, 0x5e, 0x95, 0x5c, 0x53, 0xb0, 0x90, 0xb2, 0xe4, 0xb7,  0x37, 0x19, 0x19, 0x6a,
    0x40, 0x25, 0x42, 0x96, 0x8f, 0xd1, 0x4a, 0x88, 0x8f, 0x46, 0xb9, 0xa7,  0x88, 0x6e, 0x44, 0x88,
    0x59, 0x49, 0xea, 0xb0, 0xff, 0xf6, 0x9d, 0x52, 0x31, 0x5c, 0x6c, 0x63,  0x4f, 0xd1, 0x4a, 0x7f,
    0x0d, 0x52, 0x02, 0x3d, 0x56, 0xf7, 0x96, 0x98, 0xfa, 0x65, 0x96, 0xab,  0xee, 0xd4, 0xf9, 0x3f,
    0xbb, 0x48, 0xeb, 0x53, 0x4d, 0x98, 0x54, 0x14, 0xce, 0xed, 0x0d, 0x9a,  0x8e, 0xd3, 0x3c, 0x38,
    0x7c, 0x9d, 0xfd, 0xab, 0x92, 0xff, 0xbd, 0xf2, 0x40, 0xfc, 0xec, 0xf6,  0x5a, 0x2c, 0x93, 0xb9,
};

const uint8_t		test_version_list[2] = { 0x0, 0x1 };
const uint8_t		test_selected_version[2] = { 0x0, 0x1 };

const uint8_t		test_packet[] = {
    0x01, 	/* code = 1 (request) */
    0x37, 	/* identifier = 55 */
    0x00, 0x50, /* length = 0x50 = 80 */                
    0x12,	/* type = 0x12 = 18 = EAP-SIM */
    0x0b,	/* subtype = 0x0b = 11 = Challenge */
    0x00, 0x00, /* reserved */

    /* AT_RAND */
    0x01,
    0x0d,	/* 0x0d (13) * 4 = 52 bytes */
    0x00, 0x00, /* reserved */
    0x10, 0x11, 0x12, 0x13,    
    0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b,
    0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23,    
    0x24, 0x25, 0x26, 0x27,
    0x28, 0x29, 0x2a, 0x2b,
    0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33,    
    0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x3e, 0x3f,

    /* AT_MAC */
    0x0b,                
    0x05,	/* 0x05 * 4 = 20 bytes */
    0x00, 0x00,	/* padding */
    0x00, 0x97, 0xc3, 0x64,
    0xf8, 0x43, 0x1d, 0xa4,
    0x92, 0x5b, 0xb2, 0xb1,
    0x95, 0xd0, 0xbe, 0x22         
};

static void
dump_plist(FILE * f, CFTypeRef p)
{
    CFDataRef	data;
    data = CFPropertyListCreateXMLData(NULL, p);
    if (data == NULL) {
	return;
    }
    fwrite(CFDataGetBytePtr(data), CFDataGetLength(data), 1, f);
    CFRelease(data);
    return;
}

static void
dump_triplets(void)
{
    CFMutableDictionaryRef	dict;
    int				i;
    CFMutableArrayRef		array;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < EAPSIM_MAX_RANDS; i++) {
	CFDataRef		data;
	
	data = CFDataCreateWithBytesNoCopy(NULL, test_kc[i].Kc, SIM_KC_SIZE, 
					   kCFAllocatorNull);
	CFArrayAppendValue(array, data);
	CFRelease(data);
    }
    CFDictionarySetValue(dict, kEAPClientPropEAPSIMKcList, array);
    CFRelease(array);

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < EAPSIM_MAX_RANDS; i++) {
	CFDataRef		data;
	
	data = CFDataCreateWithBytesNoCopy(NULL, test_sres[i].SRES,
					   SIM_SRES_SIZE, 
					   kCFAllocatorNull);
	CFArrayAppendValue(array, data);
	CFRelease(data);
    }
    CFDictionarySetValue(dict, kEAPClientPropEAPSIMSRESList, array);
    CFRelease(array);

    array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < EAPSIM_MAX_RANDS; i++) {
	CFDataRef		data;
	
	data = CFDataCreateWithBytesNoCopy(NULL, test_rand[i].RAND,
					   SIM_RAND_SIZE, 
					   kCFAllocatorNull);
	CFArrayAppendValue(array, data);
	CFRelease(data);
    }
    CFDictionarySetValue(dict, kEAPClientPropEAPSIMRANDList, array);
    CFRelease(array);

    dump_plist(stdout, dict);
    CFRelease(dict);
    return;
}


const uint8_t	attrs_plaintext[] = {
         0x84,               
            0x13,            
            0x00, 0x46,         
            0x77, 0x38, 0x77, 0x34, 0x39, 0x50, 0x65, 0x78, 0x43, 0x61, 0x7a, 0x57, 0x4a, 0x26, 0x78, 0x43,
            0x49, 0x41, 0x52, 0x6d, 0x78, 0x75, 0x4d, 0x4b, 0x68, 0x74, 0x35, 0x53, 0x31, 0x73, 0x78, 0x52,
            0x44, 0x71, 0x58, 0x53, 0x45, 0x46, 0x42, 0x45, 0x67, 0x33, 0x44, 0x63, 0x5a, 0x50, 0x39, 0x63,
            0x49, 0x78, 0x54, 0x65, 0x35, 0x4a, 0x34, 0x4f, 0x79, 0x49, 0x77, 0x4e, 0x47, 0x56, 0x7a, 0x78,
            0x65, 0x4a, 0x4f, 0x55, 0x31, 0x47,
            0x00, 0x00,          
         0x85,                
            0x16,             
            0x00, 0x51,          
            0x59, 0x32, 0x34, 0x66, 0x4e, 0x53, 0x72, 0x7a, 0x38, 0x42, 0x50, 0x32, 0x37, 0x34, 0x6a, 0x4f,
            0x4a, 0x61, 0x46, 0x31, 0x37, 0x57, 0x66, 0x78, 0x49, 0x38, 0x59, 0x4f, 0x37, 0x51, 0x58, 0x30,
            0x30, 0x70, 0x4d, 0x58, 0x6b, 0x39, 0x58, 0x4d, 0x4d, 0x56, 0x4f, 0x77, 0x37, 0x62, 0x72, 0x6f,
            0x61, 0x4e, 0x68, 0x54, 0x63, 0x7a, 0x75, 0x46, 0x71, 0x35, 0x33, 0x61, 0x45, 0x70, 0x4f, 0x6b,
            0x6b, 0x33, 0x4c, 0x30, 0x64, 0x6d, 0x40, 0x65, 0x61, 0x70, 0x73, 0x69, 0x6d, 0x2e, 0x66, 0x6f,
            0x6f,
            0x00, 0x00, 0x00,       
         0x06,                
            0x03,             
            0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00
};

const static uint8_t	attrs_encrypted[] = {
    0x55, 0xf2, 0x93, 0x9b, 0xbd, 0xb1, 0xb1, 0x9e,
    0xa1, 0xb4, 0x7f, 0xc0, 0xb3, 0xe0, 0xbe, 0x4c,
    0xab, 0x2c, 0xf7, 0x37, 0x2d, 0x98, 0xe3, 0x02,
    0x3c, 0x6b, 0xb9, 0x24, 0x15, 0x72, 0x3d, 0x58,
    0xba, 0xd6, 0x6c, 0xe0, 0x84, 0xe1, 0x01, 0xb6,
    0x0f, 0x53, 0x58, 0x35, 0x4b, 0xd4, 0x21, 0x82,
    0x78, 0xae, 0xa7, 0xbf, 0x2c, 0xba, 0xce, 0x33,
    0x10, 0x6a, 0xed, 0xdc, 0x62, 0x5b, 0x0c, 0x1d,
    0x5a, 0xa6, 0x7a, 0x41, 0x73, 0x9a, 0xe5, 0xb5,
    0x79, 0x50, 0x97, 0x3f, 0xc7, 0xff, 0x83, 0x01,
    0x07, 0x3c, 0x6f, 0x95, 0x31, 0x50, 0xfc, 0x30,
    0x3e, 0xa1, 0x52, 0xd1, 0xe1, 0x0a, 0x2d, 0x1f,
    0x4f, 0x52, 0x26, 0xda, 0xa1, 0xee, 0x90, 0x05,
    0x47, 0x22, 0x52, 0xbd, 0xb3, 0xb7, 0x1d, 0x6f,
    0x0c, 0x3a, 0x34, 0x90, 0x31, 0x6c, 0x46, 0x92,
    0x98, 0x71, 0xbd, 0x45, 0xcd, 0xfd, 0xbc, 0xa6,
    0x11, 0x2f, 0x07, 0xf8, 0xbe, 0x71, 0x79, 0x90,
    0xd2, 0x5f, 0x6d, 0xd7, 0xf2, 0xb7, 0xb3, 0x20,
    0xbf, 0x4d, 0x5a, 0x99, 0x2e, 0x88, 0x03, 0x31,
    0xd7, 0x29, 0x94, 0x5a, 0xec, 0x75, 0xae, 0x5d,
    0x43, 0xc8, 0xed, 0xa5, 0xfe, 0x62, 0x33, 0xfc,
    0xac, 0x49, 0x4e, 0xe6, 0x7a, 0x0d, 0x50, 0x4d
};

const static uint8_t	test_iv[] = {
            0x9e, 0x18, 0xb0, 0xc2,    
            0x9a, 0x65, 0x22, 0x63,
            0xc0, 0x6e, 0xfb, 0x54,
            0xdd, 0x00, 0xa8, 0x95
};

static void
test_encr_data(void)
{
    uint8_t 		buf[sizeof(attrs_encrypted)];
    size_t		buf_used;
    CCCryptorRef	cryptor;
    EAPSIMKeyInfoRef	key_info_p = (EAPSIMKeyInfoRef)key_block;
    CCCryptorStatus 	status;

    status = CCCryptorCreate(kCCEncrypt,
			     kCCAlgorithmAES128,
			     0,
			     key_info_p->s.k_encr,
			     sizeof(key_info_p->s.k_encr),
			     test_iv,
			     &cryptor);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoCreate failed with %d\n",
		status);
	return;
    }
    status = CCCryptorUpdate(cryptor,
			     attrs_plaintext,
			     sizeof(attrs_plaintext),
			     buf,
			     sizeof(buf),
			     &buf_used);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoUpdate failed with %d\n",
		status);
	goto done;
    }
    if (buf_used != sizeof(buf)) {
	fprintf(stderr, "buf consumed %d, should have been %d\n",
		(int)buf_used, (int)sizeof(buf));
	goto done;
    }

    if (bcmp(attrs_encrypted, buf, sizeof(attrs_encrypted))) {
	fprintf(stderr, "encryption yielded different results\n");
	goto done;
    }
    fprintf(stderr, "encryption matches!\n");

 done:
    status = CCCryptorRelease(cryptor);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoRelease failed with %d\n",
		status);
	return;
    }
    return;
}

static void
test_decrypt_data(void)
{
    uint8_t 		buf[sizeof(attrs_encrypted)];
    size_t		buf_used;
    CCCryptorRef	cryptor;
    EAPSIMKeyInfoRef	key_info_p = (EAPSIMKeyInfoRef)key_block;
    CCCryptorStatus 	status;

    status = CCCryptorCreate(kCCDecrypt,
			     kCCAlgorithmAES128,
			     0,
			     key_info_p->s.k_encr,
			     sizeof(key_info_p->s.k_encr),
			     test_iv,
			     &cryptor);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoCreate failed with %d\n",
		status);
	return;
    }
    status = CCCryptorUpdate(cryptor,
			     attrs_encrypted,
			     sizeof(attrs_encrypted),
			     buf,
			     sizeof(buf),
			     &buf_used);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoUpdate failed with %d\n",
		status);
	goto done;
    }
    if (buf_used != sizeof(buf)) {
	fprintf(stderr, "buf consumed %d, should have been %d\n",
		(int)buf_used, (int)sizeof(buf));
	goto done;
    }

    if (bcmp(attrs_plaintext, buf, sizeof(attrs_plaintext))) {
	fprintf(stderr, "decryption yielded different results\n");
	goto done;
    }
    fprintf(stderr, "decryption matches!\n");

 done:
    status = CCCryptorRelease(cryptor);
    if (status != kCCSuccess) {
	fprintf(stderr, "CCCryptoRelease failed with %d\n",
		status);
	return;
    }
    return;
}

int
main(int argc, char * argv[])
{
    int			attrs_length;
    CC_SHA1_CTX		context;
    EAPSIMPacketRef	eapsim;
    uint8_t		hash[CC_SHA1_DIGEST_LENGTH];
    EAPSIMKeyInfo	key_info;
    AT_MAC *		mac_p;
    uint8_t		mk[CC_SHA1_DIGEST_LENGTH];
    TLVList		tlvs;

    /* MK = SHA1(Identity|n*Kc| NONCE_MT| Version List| Selected Version) */
    CC_SHA1_Init(&context);
    CC_SHA1_Update(&context, test_identity, sizeof(test_identity) - 1);
    CC_SHA1_Update(&context, test_kc, sizeof(test_kc));
    CC_SHA1_Update(&context, test_nonce_mt, sizeof(test_nonce_mt));
    CC_SHA1_Update(&context, test_version_list, sizeof(test_version_list));
    CC_SHA1_Update(&context, test_selected_version,
		   sizeof(test_selected_version));
    CC_SHA1_Final(mk, &context);

    if (bcmp(mk, test_mk, sizeof(test_mk))) {
	fprintf(stderr, "The mk values are different\n");
	printf("Computed:\n");
	print_data(mk, sizeof(mk));
	printf("Desired:\n");
	print_data((void *)test_mk, sizeof(test_mk));
    }
    else {
	printf("The MK values are the same!\n");
	printf("Computed:\n");
	print_data(mk, sizeof(mk));
	printf("Desired:\n");
	print_data((void *)test_mk, sizeof(test_mk));
    }

    /* now run PRF to generate keying material */
    fips186_2prf(mk, key_info.key);

    /* make sure the key blocks are the same */
    if (bcmp(key_info.key, key_block, sizeof(key_info.key))) {
	fprintf(stderr, "key blocks are different!\n");
	exit(1);
    }
    else {
	printf("key blocks match\n");
    }

    if (eapsim_packet_dump(stdout, (EAPPacketRef)test_packet) == FALSE) {
	fprintf(stderr, "packet is bad\n");
	exit(1);
    }

    eapsim = (EAPSIMPacketRef)test_packet;
    attrs_length = EAPPacketGetLength((EAPPacketRef)eapsim)
	- EAPSIM_HEADER_LENGTH;
    TLVListInit(&tlvs);
    if (TLVListParse(&tlvs, eapsim->attrs, attrs_length) == FALSE) {
	fprintf(stderr, "failed to parse TLVs: %s\n",
		TLVListErrorString(&tlvs));
	exit(1);
    }
    mac_p = (AT_MAC *)TLVListLookupAttribute(&tlvs, kAT_MAC);
    if (mac_p == NULL) {
	fprintf(stderr, "Challenge is missing AT_MAC\n");
	exit(1);
    }
    eapsim_compute_mac(&key_info, (EAPPacketRef)test_packet,
		       mac_p->ma_mac,
		       test_nonce_mt, sizeof(test_nonce_mt),
		       hash);
    if (bcmp(hash, mac_p->ma_mac, MAC_SIZE) != 0) {
	print_data(hash, sizeof(hash));
	fprintf(stderr, "AT_MAC mismatch\n");
	exit(1);
    }
    else {
	printf("AT_MAC is good\n");
    }

    dump_triplets();
    test_encr_data();
    test_decrypt_data();
    exit(0);
    return (0);
}

#endif /* TEST_SIM_CRYPTO */

#ifdef TEST_SET_VERSION_LIST
int
main(int argc, char * argv[])
{
    EAPSIMContext	context;
    uint16_t		list1[2] = { 0x1234, 0x5678 };
    uint16_t		list2[3] = { 0x1, 0x2, 0x3 };
    uint16_t		list3[2] = { 0x4, 0x5 };

    EAPSIMContextClear(&context);
    EAPSIMContextSetVersionList(&context,
				list1, sizeof(list1) / sizeof(list1[0]));
    EAPSIMContextSetVersionList(&context,
				list1, sizeof(list1) / sizeof(list1[0]));
    EAPSIMContextSetVersionList(&context,
				list2, sizeof(list2) / sizeof(list2[0]));
    EAPSIMContextSetVersionList(&context,
				list2, sizeof(list2) / sizeof(list2[0]));
    EAPSIMContextSetVersionList(&context,
				list3, sizeof(list3) / sizeof(list3[0]));
    EAPSIMContextSetVersionList(&context,
				list3, sizeof(list3) / sizeof(list3[0]));
    EAPSIMContextSetVersionList(&context, NULL, 0);
    exit(0);
    return (0);
}

#endif /* TEST_SET_VERSION_LIST */

#ifdef TEST_SIM_INFO
#if TARGET_OS_EMBEDDED
int
main()
{
    CFStringRef			identity;

    identity = sim_identity_create(NULL, FALSE);
    if (identity != NULL) {
	CFShow(identity);
	CFRelease(identity);
    }
    exit(0);
    return (0);
}
#endif /* TARGET_OS_EMBEDDED */
#endif /* TEST_SIM_INFO */

