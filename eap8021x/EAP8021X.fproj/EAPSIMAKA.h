/*
 * Copyright (c) 2008-2012 Apple Inc. All rights reserved.
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
 * EAPSIMAKA.h
 * - definitions/routines for EAP-SIM and EAP-AKA
 */

#ifndef _EAP8021X_EAPSIMAKA_H
#define _EAP8021X_EAPSIMAKA_H

/* 
 * Modification History
 *
 * October 8, 2012	Dieter Siegmund (dieter@apple)
 * - created (from eapsim_plugin.c)
 */
#include <stdint.h>
#include <stdbool.h>
#include <sys/param.h>
#include <CommonCrypto/CommonDigest.h>
#include "EAP.h"

typedef struct {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		subtype;
    uint8_t		reserved[2];
    uint8_t		attrs[1];
} EAPSIMAKAPacket, * EAPSIMAKAPacketRef;

typedef EAPSIMAKAPacket EAPSIMPacket, EAPAKAPacket;
typedef EAPSIMAKAPacketRef EAPSIMPacketRef, EAPAKAPacketRef;

#define kEAPSIMAKAPacketHeaderLength	offsetof(EAPSIMAKAPacket, attrs)

/*
 * Type: EAPSIMAKAPacketSubtype
 * Purpose:
 *   Definitions for EAPSIMAKAPacket subtype field.
 */
typedef uint8_t	EAPSIMAKAPacketSubtype;
enum {
    /* EAP-AKA only */
    kEAPSIMAKAPacketSubtypeAKAChallenge			= 1,
    kEAPSIMAKAPacketSubtypeAKAAuthenticationReject	= 2,
    kEAPSIMAKAPacketSubtypeAKASynchronizationFailure	= 4,
    kEAPSIMAKAPacketSubtypeAKAIdentity			= 5,

    /* EAP-SIM only */
    kEAPSIMAKAPacketSubtypeSIMStart 			= 10,
    kEAPSIMAKAPacketSubtypeSIMChallenge 		= 11,

    /* EAP-AKA and EAP-SIM */
    kEAPSIMAKAPacketSubtypeNotification 		= 12,
    kEAPSIMAKAPacketSubtypeReauthentication 		= 13,
    kEAPSIMAKAPacketSubtypeClientError 			= 14
};

typedef uint8_t		EAPSIMAKAAttributeType;

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

const char *
EAPSIMAKAAttributeTypeGetString(EAPSIMAKAAttributeType attr);

/*
 * Identity Attributes:
 *     kAT_ANY_ID_REQ, kAT_FULLAUTH_ID_REQ, kAT_PERMANENT_ID_REQ
 */
#define kEAPSIMAKAIdentityAttributesCount	3

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

/*
 * TLV's
 */
typedef struct TLV_s {
    uint8_t		tlv_type;
    uint8_t		tlv_length;
    uint8_t		tlv_value[1];
} TLV, * TLVRef;

#define TLV_HEADER_LENGTH	offsetof(TLV, tlv_value)

#define TLV_ALIGNMENT		4
#define TLV_MAX_LENGTH		(UINT8_MAX * TLV_ALIGNMENT)
#define SIXTEEN_BYTES		16

static __inline__ int
TLVRoundUp(int length)
{
    return (roundup(length, TLV_ALIGNMENT));
}

typedef struct AT_SELECTED_VERSION_s {
    uint8_t		sv_type;
    uint8_t		sv_length;	/* = 1 */
    uint8_t		sv_selected_version[2];
} AT_SELECTED_VERSION;

#define NONCE_MT_SIZE	SIXTEEN_BYTES
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

#define RAND_SIZE	SIXTEEN_BYTES
typedef struct AT_RAND_s {
    uint8_t		ra_type;
    uint8_t		ra_length;
    uint8_t		ra_reserved[2];
    uint8_t		ra_rand[RAND_SIZE]; /* EAP-SIM has 2 or 3 of these */
} AT_RAND;

#define AUTN_SIZE	SIXTEEN_BYTES
typedef struct AT_AUTN_s {
    uint8_t		an_type;
    uint8_t		an_length;
    uint8_t		an_reserved[2];
    uint8_t		an_autn[AUTN_SIZE];
} AT_AUTN;

typedef struct AT_RES_s {
    uint8_t		rs_type;
    uint8_t		rs_length;
    uint8_t		rs_res_length[2];	/* in bits */
    uint8_t		rs_res[1];
} AT_RES;

#define AUTS_SIZE	14
typedef struct AT_AUTS_s {
    uint8_t		as_type;
    uint8_t		as_length;
    uint8_t		as_auts[AUTS_SIZE];
} AT_AUTS;

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

#define IV_SIZE		SIXTEEN_BYTES
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

#define AT_ENCR_DATA_ALIGNMENT	16
#define AT_ENCR_DATA_ROUNDUP(size)	roundup((size), AT_ENCR_DATA_ALIGNMENT)


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

#define MAC_SIZE	SIXTEEN_BYTES
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

#define NONCE_S_SIZE	SIXTEEN_BYTES
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
    kATNotificationCodeGeneralFailureAfterAuthentication = 0,
    kATNotificationCodeGeneralFailureBeforeAuthentication = 16384,
    kATNotificationCodeSuccess = 32768,
    kATNotificationCodeTemporarilyDeniedAccess = 1026,
    kATNotificationCodeNotSubscribed = 1031,
};

#define AT_NOTIFICATION_P_BIT	(1 << 14)
#define AT_NOTIFICATION_S_BIT	(1 << 15)

const char *
ATNotificationCodeGetString(uint16_t notification_code);

static __inline__ bool
ATNotificationCodeIsSuccess(uint16_t notification_code)
{
    return ((notification_code & AT_NOTIFICATION_S_BIT) != 0);
}

static __inline__ bool
ATNotificationPhaseIsAfterAuthentication(uint16_t notification_code)
{
    return ((notification_code & AT_NOTIFICATION_P_BIT) == 0);
}


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
    AT_AUTN *			at_autn;
    AT_AUTS *			at_auts;
    AT_RES *			at_res;
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


/**
 ** EAP-SIM specific
 **/
#define SIM_KC_SIZE		8
#define SIM_SRES_SIZE		4
#define SIM_RAND_SIZE		RAND_SIZE

#define EAPSIM_MAX_RANDS	3

/**
 ** EAPSIMAKAKeyInfo
 **/
#define EAPSIMAKA_K_ENCR_SIZE	16		/* used with AT_ENCR_DATA */
#define EAPSIMAKA_K_AUT_SIZE	16		/* used with AT_MAC */
#define EAPSIMAKA_MSK_SIZE	64
#define EAPSIMAKA_EMSK_SIZE	64
#define EAPSIMAKA_KEY_SIZE	(EAPSIMAKA_K_ENCR_SIZE + EAPSIMAKA_K_AUT_SIZE \
				 + EAPSIMAKA_MSK_SIZE + EAPSIMAKA_EMSK_SIZE)

typedef union {
    struct {
	uint8_t	k_encr[EAPSIMAKA_K_ENCR_SIZE];
	uint8_t	k_aut[EAPSIMAKA_K_AUT_SIZE];
	uint8_t	msk[EAPSIMAKA_MSK_SIZE];
	uint8_t	emsk[EAPSIMAKA_EMSK_SIZE];
    } s;
    uint8_t	key[EAPSIMAKA_KEY_SIZE];
} EAPSIMAKAKeyInfo, * EAPSIMAKAKeyInfoRef;

#endif /* _EAP8021X_EAPSIMAKA_H */
