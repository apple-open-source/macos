/*
 * Copyright (c) 2008-2013 Apple Inc. All rights reserved.
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
 * EAPSIMAKAUtil.h
 * - common definitions/routines for EAP-SIM and EAP-AKA
 */

#ifndef _EAP8021X_EAPSIMAKAUTIL_H
#define _EAP8021X_EAPSIMAKAUTIL_H

/* 
 * Modification History
 *
 * October 8, 2012	Dieter Siegmund (dieter@apple)
 * - created (from eapsim_plugin.c)
 */

#include "symbol_scope.h"
#include "EAPSIMAKA.h"
#include "EAPSIMAKAPersistentState.h"
#include <stdio.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFData.h>

static __inline__ int
_SizeInPointers(int size)
{
    return ((size + sizeof(void *) - 1) / sizeof(void *));
}

#define DECLARE_LOCAL_STRUCTURE(__name, __type, __size_func)		\
    void * 		__name ## _buf[_SizeInPointers(__size_func())]; \
    __type 		__name = (__type) __name ## _buf;


/**
 ** TLVBuffer
 **/
typedef struct TLVBuffer * TLVBufferRef;

int
TLVBufferSizeof(void);

#define TLVBufferDeclare(__name)					\
    DECLARE_LOCAL_STRUCTURE(__name, TLVBufferRef, TLVBufferSizeof)

int
TLVBufferUsed(TLVBufferRef tb);

const char *
TLVBufferErrorString(TLVBufferRef tb);

int
TLVBufferMaxSize(TLVBufferRef tb);

uint8_t *
TLVBufferStorage(TLVBufferRef tb);

void
TLVBufferInit(TLVBufferRef tb, uint8_t * storage, int size);

TLVRef
TLVBufferAllocateTLV(TLVBufferRef tb, 
		     EAPSIMAKAAttributeType type, int length);

Boolean
TLVBufferAddIdentity(TLVBufferRef tb_p, 
		     const uint8_t * identity, int identity_length);

Boolean
TLVBufferAddIdentityString(TLVBufferRef tb_p, CFStringRef identity,
			   CFDataRef * ret_data);
Boolean
TLVBufferAddCounter(TLVBufferRef tb_p, uint16_t at_counter);

Boolean
TLVBufferAddCounterTooSmall(TLVBufferRef tb_p);

Boolean
TLVBufferAddPadding(TLVBufferRef tb_p, int padding_length);

/**
 ** TLV
 **/
CFStringRef
TLVCreateString(TLVRef tlv_p);

void
TLVPrint(FILE * f, TLVRef tlv_p);

/**
 ** TLVList
 **/
typedef struct TLVList * TLVListRef;

int
TLVListSizeof(void);

#define TLVListDeclare(__name)					\
    DECLARE_LOCAL_STRUCTURE(__name, TLVListRef, TLVListSizeof)

const char *
TLVListErrorString(TLVListRef tlvs_p);

void
TLVListInit(TLVListRef tlvs_p);

void
TLVListFree(TLVListRef tlvs_p);

void
TLSListAddAttribute(TLVListRef tlvs_p, const uint8_t * attr);

int
TLVCheckValidity(TLVListRef tlvs_p, TLVRef tlv_p);

CFStringRef
TLVCreateString(TLVRef tlv_p);

Boolean
TLVListParse(TLVListRef tlvs_p, const uint8_t * attrs, int attrs_length);

PRIVATE_EXTERN CFStringRef
TLVListCopyDescription(TLVListRef tlvs_p);

TLVRef
TLVListLookupAttribute(TLVListRef tlvs_p, EAPSIMAKAAttributeType type);

EAPSIMAKAAttributeType
TLVListLookupIdentityAttribute(TLVListRef tlvs_p);

CFStringRef
TLVListCreateStringFromAttribute(TLVListRef tlvs_p, 
				 EAPSIMAKAAttributeType type);

/**
 ** EAPSIMAKAStatus
 ** - values for the domain-specific error
 **/
enum {
    kEAPSIMAKAStatusOK = 0,
    kEAPSIMAKAStatusFailureAfterAuthentication = 1,
    kEAPSIMAKAStatusFailureBeforeAuthentication = 2,
    kEAPSIMAKAStatusAccessTemporarilyDenied = 3,
    kEAPSIMAKAStatusNotSubscribed = 4,
    kEAPSIMAKAStatusUnrecognizedNotification = 5,
};
typedef uint32_t EAPSIMAKAStatus;

void
EAPSIMAKAKeyInfoComputeMAC(EAPSIMAKAKeyInfoRef key_info_p,
			   EAPPacketRef pkt,
			   const uint8_t * mac_p, 
			   const uint8_t * extra, int extra_length,
			   uint8_t hash[CC_SHA1_DIGEST_LENGTH]);
uint8_t *
EAPSIMAKAKeyInfoDecryptTLVList(EAPSIMAKAKeyInfoRef key_info_p,
			       AT_ENCR_DATA * encr_data_p, AT_IV * iv_p,
			       TLVListRef decrypted_tlvs_p);

bool
EAPSIMAKAKeyInfoVerifyMAC(EAPSIMAKAKeyInfoRef key_info,
			  EAPPacketRef pkt,
			  const uint8_t * mac_p,
			  const uint8_t * extra, int extra_length);

void
EAPSIMAKAKeyInfoSetMAC(EAPSIMAKAKeyInfoRef key_info,
		       EAPPacketRef pkt,
		       uint8_t * mac_p,
		       const uint8_t * extra, int extra_length);

void
EAPSIMAKAKeyInfoComputeReauthKey(EAPSIMAKAKeyInfoRef key_info,
				 EAPSIMAKAPersistentStateRef persist,
				 const void * identity,
				 int identity_length,
				 AT_COUNTER * counter_p,
				 AT_NONCE_S * nonce_s_p);
bool
EAPSIMAKAKeyInfoEncryptTLVs(EAPSIMAKAKeyInfoRef key_info,
			    TLVBufferRef tb_p, TLVBufferRef tb_add_p);


/*
 * Function: EAPSIMAKAStatusForATNotificationCode
 * Purpose:
 *   Map the AT Notification code to EAPSIMAKAStatus value.
 */
EAPSIMAKAStatus
EAPSIMAKAStatusForATNotificationCode(uint16_t notification_code);

const char *
EAPSIMAKAPacketSubtypeGetString(EAPSIMAKAPacketSubtype subtype);

const char *
ATNotificationCodeGetString(uint16_t code);

CFStringRef
EAPSIMAKAPacketCopyDescription(const EAPPacketRef pkt, bool * packet_is_valid);

#ifdef EAPSIMAKA_PACKET_DUMP
bool
EAPSIMAKAPacketDump(FILE * out_f, EAPPacketRef pkt);
#endif /* EAPSIMAKA_PACKET_DUMP */

/*
 * Property: kEAPClientPropEAPSIMAKAIMSI
 * Purpose:
 *   Statically configure the IMSI.
 *
 *   Used for testing only.
 */
#define kEAPClientPropEAPSIMAKAIMSI \
    CFSTR("EAPSIMAKAIMSI") 			/* string */

/*
 * Property: kEAPClientPropEAPSIMAKAIMSI
 * Purpose:
 *   Statically configure the realm.  May be required in some configurations
 *   to ensure proper AAA routing.
 */
#define kEAPClientPropEAPSIMAKARealm \
    CFSTR("EAPSIMAKARealm") 			/* string */

/*
 * Property: kEAPClientPropEAPSIMAKAIdentityType
 * Purpose:
 *   Control which identity is used.  If this property is not specified
 *   the default behavior is to accept and use reauth IDs, pseudonyms,
 *   and the permanent ID (IMSI).
 *
 *   If kEAPSIMAKAIdentityTypeFullAuthentication is specified, pseudonyms
 *   and the permanent ID are used.
 *
 *   If kEAPSIMAKAIdentityTypePermanent is specified, only the permanent ID
 *   is used.  Setting this value also ensures that no preferences or 
 *   keychain items will be accessed.
 */
#define kEAPClientPropEAPSIMAKAIdentityType \
    CFSTR("EAPSIMAKAIdentityType")		/* kEAPSIMAKAIdentityType* */

#define kEAPSIMAKAIdentityTypeFullAuthentication CFSTR("FullAuthentication")
#define kEAPSIMAKAIdentityTypePermanent		CFSTR("Permanent")

/*
 * Function: EAPSIMAKAIdentityTypeGetAttributeType
 * Purpose:
 *   Convert from the identity type string kEAPSIMAKAIdentityType* to
 *   the corresponding EAPSIMAKAAttributeType.
 *
 *   If string is kEAPSIMAKAIdentityTypeFullAuthentication, returns
 *   kAT_FULL_AUTH_ID_REQ.
 *
 *   If string is kEAPSIMAKAIdentityTypePermanent, returns kAT_PERMANENT_ID_REQ.
 *
 *   If string is any other value (including NULL), returns kAT_ANY_ID_REQ.
 */
EAPSIMAKAAttributeType
EAPSIMAKAIdentityTypeGetAttributeType(CFStringRef string);

#endif /* _EAP8021X_EAPSIMAKAUTIL_H */
