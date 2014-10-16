/*
 * Copyright (c) 2012-2014 Apple Inc. All rights reserved.
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
 * eapaka_plugin.c
 * - EAP-AKA client
 */

/* 
 * Modification History
 *
 * October 8, 2012	Dieter Siegmund (dieter@apple)
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
#include <sys/param.h>
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPClientModule.h>
#include <TargetConditionals.h>
#include "myCFUtil.h"
#include "printdata.h"
#include "fips186prf.h"
#include "SIMAccess.h"
#include "nbo.h"
#include "EAPSIMAKAPersistentState.h"
#include "EAPSIMAKA.h"
#include "EAPSIMAKAUtil.h"
#include "EAPLog.h"

#define EAP_AKA_NAME		"EAP-AKA"

/*
 * kEAPClientPropEAPAKARES
 * kEAPClientPropEAPAKACk
 * kEAPClientPropEAPAKAIk
 * - static (RES, Ck, Ik) used for testing
 */
#define kEAPClientPropEAPAKARES		CFSTR("EAPAKARES")	/* data */
#define kEAPClientPropEAPAKACk		CFSTR("EAPAKACk")	/* data */
#define kEAPClientPropEAPAKAIk		CFSTR("EAPAKAIk")	/* data */

/**
 ** Protocol-specific defines
 **/

enum {
    kEAPAKAClientStateNone = 0,
    kEAPAKAClientStateIdentity = 1,
    kEAPAKAClientStateChallenge = 2,
    kEAPAKAClientStateReauthentication = 3,
    kEAPAKAClientStateSuccess = 4,
    kEAPAKAClientStateFailure = 5
};
typedef int	EAPAKAClientState;

typedef struct {
    CFDataRef		res;
    CFDataRef		ck;
    CFDataRef		ik;
} AKAStaticKeys, * AKAStaticKeysRef;

/*
 * Type: EAPAKAContext
 * Purpose:
 *   Holds the EAP-AKA module's context private data.
 */
typedef struct {
    EAPClientPluginDataRef 	plugin;
    EAPClientState		plugin_state;
    EAPAKAClientState		state;
    int				previous_identifier;
    int				identity_count;
    int				n_required_rands;
    EAPSIMAKAAttributeType	last_identity_type;
    CFDataRef			last_identity;
    EAPSIMAKAKeyInfo		key_info;
    bool			key_info_valid;
    EAPSIMAKAPersistentStateRef	persist;
    bool			reauth_success;
    AKAStaticKeys		static_keys;
    uint8_t			pkt[1500];
} EAPAKAContext, *EAPAKAContextRef;

/**
 ** Identity routines
 **/
STATIC CFStringRef
copy_imsi_identity(CFStringRef imsi, CFStringRef realm)
{

    if (realm != NULL) {
	return (CFStringCreateWithFormat(NULL, NULL,
					 CFSTR("0" "%@" "@" "%@"),
					 imsi, realm));
    }
    return (CFStringCreateWithFormat(NULL, NULL, CFSTR("0" "%@"),
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
					      kEAPClientPropEAPSIMAKARealm));
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
					     kEAPClientPropEAPSIMAKAIMSI));
    if (imsi == NULL) {
	return (NULL);
    }
    return (CFRetain(imsi));
}

STATIC CFStringRef
copy_static_identity(CFDictionaryRef properties)
{
    CFStringRef		imsi;
    CFStringRef		realm;
    CFStringRef		ret_identity;

    imsi = copy_static_imsi(properties);
    if (imsi == NULL) {
	return (NULL);
    }
    realm = copy_static_realm(properties);
    ret_identity = copy_imsi_identity(imsi, realm);
    my_CFRelease(&imsi);
    my_CFRelease(&realm);
    return (ret_identity);
}

#if TARGET_OS_EMBEDDED
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
create_identity(EAPSIMAKAPersistentStateRef persist,
		EAPSIMAKAAttributeType requested_type,
		CFStringRef realm,
		Boolean * is_reauth_id_p)
{
    CFStringRef			ret_identity = NULL;

    if (is_reauth_id_p != NULL) {
	*is_reauth_id_p = FALSE;
    }
    if (persist == NULL) {
	return (NULL);
    }
    if (requested_type == kAT_ANY_ID_REQ
	|| requested_type == kAT_FULLAUTH_ID_REQ) {
	CFStringRef		reauth_id;
	CFStringRef		pseudonym;
	
	reauth_id = EAPSIMAKAPersistentStateGetReauthID(persist);
	pseudonym = EAPSIMAKAPersistentStateGetPseudonym(persist);
	if (requested_type == kAT_ANY_ID_REQ && reauth_id != NULL) {
	    if (is_reauth_id_p != NULL) {
		*is_reauth_id_p = TRUE;
	    }
	    ret_identity = CFRetain(reauth_id);
	}
	else if (pseudonym != NULL) {
	    ret_identity = copy_pseudonym_identity(pseudonym, realm);
	}
    }
    if (ret_identity == NULL) {
	/* use permanent id */
	ret_identity 
	    = copy_imsi_identity(EAPSIMAKAPersistentStateGetIMSI(persist),
				 realm);
    }
    return (ret_identity);
}

STATIC CFStringRef
sim_identity_create(EAPSIMAKAPersistentStateRef persist,
		    CFDictionaryRef properties,
		    EAPSIMAKAAttributeType identity_type,
		    Boolean * is_reauth_id_p)
{
    CFStringRef		realm = NULL;
    CFStringRef		ret_identity = NULL;

    if (is_reauth_id_p != NULL) {
	*is_reauth_id_p = FALSE;
    }
    realm = copy_static_realm(properties);
    if (realm == NULL) {
	realm = SIMCopyRealm();
    }
    ret_identity = create_identity(persist, identity_type, realm, 
				   is_reauth_id_p);
    my_CFRelease(&realm);
    return (ret_identity);
}

#else /* TARGET_OS_EMBEDDED */

STATIC CFStringRef
sim_identity_create(EAPSIMAKAPersistentStateRef persist,
		    CFDictionaryRef properties,
		    EAPSIMAKAAttributeType identity_type, 
		    Boolean * is_reauth_id_p)
{
    if (is_reauth_id_p != NULL) {
	*is_reauth_id_p = FALSE;
    }
    return (NULL);
}

#endif /* TARGET_OS_EMBEDDED */

/**
 ** Utility Routines
 **/
STATIC EAPSIMAKAAttributeType
S_get_identity_type(CFDictionaryRef dict)
{
    CFStringRef			identity_type_str;

    if (dict == NULL) {
	identity_type_str = NULL;
    }
    else {
	identity_type_str
	    = CFDictionaryGetValue(dict, kEAPClientPropEAPSIMAKAIdentityType);
	identity_type_str = isA_CFString(identity_type_str);
    }
    return (EAPSIMAKAIdentityTypeGetAttributeType(identity_type_str));
}

STATIC void
AKAStaticKeysClear(AKAStaticKeysRef keys)
{
    bzero(keys, sizeof(*keys));
    return;
}

STATIC void
AKAStaticKeysRelease(AKAStaticKeysRef keys)
{
    my_CFRelease(&keys->res);
    my_CFRelease(&keys->ck);
    my_CFRelease(&keys->ik);
    return;
}

STATIC bool
AKAStaticKeysInitWithProperties(AKAStaticKeysRef keys,
				CFDictionaryRef properties)
{
    CFDataRef	ck;
    CFDataRef	ik;
    CFDataRef	res;
    bool	success = FALSE;

    if (properties == NULL) {
	goto done;
    }
    res = CFDictionaryGetValue(properties, kEAPClientPropEAPAKARES);
    ck = CFDictionaryGetValue(properties, kEAPClientPropEAPAKACk);
    ik = CFDictionaryGetValue(properties, kEAPClientPropEAPAKAIk);
    if (res == NULL && ck == NULL && ik == NULL) {
	goto done;
    }
    success = TRUE;
    if (isA_CFData(ck) == NULL) {
	EAPLOG_FL(LOG_NOTICE, "invalid/missing EAPAKACk property");
	success = FALSE;
    }
    if (isA_CFData(ik) == NULL) {
	EAPLOG_FL(LOG_NOTICE, "invalid/missing EAPAKAIk property");
	success = FALSE;
    }
    if (isA_CFData(res) == NULL) {
	EAPLOG_FL(LOG_NOTICE, "invalid/missing EAPAKARES property");
	success = FALSE;
    }
    my_FieldSetRetainedCFType(&keys->ck, ck);
    my_FieldSetRetainedCFType(&keys->ik, ik);
    my_FieldSetRetainedCFType(&keys->res, res);

 done:
    return (success);
}

STATIC void
EAPAKAContextSetLastIdentity(EAPAKAContextRef context, CFDataRef identity_data)
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
EAPAKAContextClear(EAPAKAContextRef context)
{
    bzero(context, sizeof(*context));
    context->plugin_state = kEAPClientStateAuthenticating;
    context->state = kEAPAKAClientStateNone;
    context->previous_identifier = -1;
    return;
}

STATIC void
EAPAKAContextFree(EAPAKAContextRef context)
{
    EAPSIMAKAPersistentStateRelease(context->persist);
    EAPAKAContextSetLastIdentity(context, NULL);
    AKAStaticKeysRelease(&context->static_keys);
    EAPAKAContextClear(context);
    free(context);
    return;
}

STATIC EAPPacketRef
make_response_packet(EAPAKAContextRef context,
		     EAPPacketRef in_pkt, EAPSIMAKAPacketSubtype subtype,
		     TLVBufferRef tb_p)
{
    EAPAKAPacketRef	pkt;

    pkt = (EAPAKAPacketRef)context->pkt;
    TLVBufferInit(tb_p, pkt->attrs,
		  sizeof(context->pkt) - offsetof(EAPAKAPacket, attrs));
    pkt->code = kEAPCodeResponse;
    pkt->identifier = in_pkt->identifier;
    pkt->type = kEAPTypeEAPAKA;
    pkt->subtype = subtype;
    net_uint16_set(pkt->reserved, 0);
    return ((EAPPacketRef)pkt);
}

STATIC EAPPacketRef
make_client_error_packet(EAPAKAContextRef context,
			 EAPPacketRef in_pkt, ClientErrorCode code)
{
    AttrUnion			attr;
    EAPPacketRef		pkt;
    TLVBufferDeclare(		tb_p);

    pkt = make_response_packet(context, in_pkt,
			       kEAPSIMAKAPacketSubtypeClientError, tb_p);
    attr.tlv_p = TLVBufferAllocateTLV(tb_p, kAT_CLIENT_ERROR_CODE,
				      sizeof(AT_CLIENT_ERROR_CODE));
    if (attr.tlv_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: failed allocating AT_CLIENT_ERROR_CODE, %s",
	       TLVBufferErrorString(tb_p));
	return (NULL);
    }
    net_uint16_set(attr.at_client_error_code->ce_client_error_code, code);
    EAPPacketSetLength(pkt,
		       offsetof(EAPAKAPacket, attrs) + TLVBufferUsed(tb_p));

    return (pkt);
}

STATIC void
save_persistent_state(EAPAKAContextRef context)
{
    CFStringRef		ssid = NULL;
#if TARGET_OS_EMBEDDED
    CFStringRef		trust_domain;

    trust_domain = CFDictionaryGetValue(context->plugin->properties,
	    				kEAPClientPropTLSTrustExceptionsDomain);
    if (my_CFEqual(trust_domain, kEAPTLSTrustExceptionsDomainWirelessSSID)) {
	ssid = CFDictionaryGetValue(context->plugin->properties,
				    kEAPClientPropTLSTrustExceptionsID);
    }
#endif
    EAPSIMAKAPersistentStateSave(context->persist, context->key_info_valid,
				 ssid);
    return;
}

STATIC EAPPacketRef
eapaka_identity(EAPAKAContextRef context,
		const EAPPacketRef in_pkt,
		TLVListRef tlvs_p,
		EAPClientStatus * client_status)
{
    CFStringRef		identity = NULL;
    CFDataRef		identity_data = NULL;
    EAPSIMAKAAttributeType identity_req_type;
    EAPPacketRef	pkt = NULL;
    Boolean		reauth_id_used = FALSE;
    TLVBufferDeclare(	tb_p);

    if (context->state != kEAPAKAClientStateIdentity) {
	/* starting over */
	context->plugin_state = kEAPClientStateAuthenticating;
	context->identity_count = 0;
	context->last_identity_type = 0;
	context->state = kEAPAKAClientStateIdentity;
    }
    context->identity_count++;
    if (context->identity_count > kEAPSIMAKAIdentityAttributesCount) {
	EAPLOG(LOG_NOTICE, "eapaka: too many Identity packets (%d > %d)",
	       context->identity_count, kEAPSIMAKAIdentityAttributesCount);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    identity_req_type = TLVListLookupIdentityAttribute(tlvs_p);
    switch (identity_req_type) {
    case kAT_ANY_ID_REQ:
	if (context->identity_count > 1) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: AT_ANY_ID_REQ at Identity #%d",
		   context->identity_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    case kAT_FULLAUTH_ID_REQ:
	if (context->identity_count > 1
	    && context->last_identity_type != kAT_ANY_ID_REQ) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: AT_FULLAUTH_ID_REQ follows %s at Identity #%d",
		   EAPSIMAKAAttributeTypeGetString(context->last_identity_type),
		   context->identity_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    case kAT_PERMANENT_ID_REQ:
	if (context->identity_count > 1
	    && context->last_identity_type != kAT_ANY_ID_REQ
	    && context->last_identity_type != kAT_FULLAUTH_ID_REQ) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: AT_PERMANENT_ID_REQ follows %s at Identity #%d",
		   EAPSIMAKAAttributeTypeGetString(context->last_identity_type),
		   context->identity_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    default:
	EAPLOG(LOG_NOTICE, "eapaka: AKA-Identity missing *ID_REQ");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
	break;
    }

    /* create our response */
    context->last_identity_type = identity_req_type;
    pkt = make_response_packet(context, in_pkt,
			       kEAPSIMAKAPacketSubtypeAKAIdentity, tb_p);
    if (context->static_keys.ck != NULL) {
	identity = copy_static_identity(context->plugin->properties);
    }
    else {
	identity = sim_identity_create(context->persist,
				       context->plugin->properties,
				       identity_req_type, &reauth_id_used);
    }
    if (identity == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: can't find SIM identity");
	*client_status = kEAPClientStatusResourceUnavailable;
	pkt = NULL;
	goto done;
    }
    if (!TLVBufferAddIdentityString(tb_p, identity, &identity_data)) {
	EAPLOG(LOG_NOTICE, "eapaka: can't add AT_IDENTITY, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    EAPAKAContextSetLastIdentity(context, identity_data);
    my_CFRelease(&identity_data);

    /* we didn't have a fast re-auth ID */
    if (reauth_id_used == FALSE) {
	context->key_info_valid = FALSE;
    }

    EAPPacketSetLength(pkt,
		       offsetof(EAPAKAPacket, attrs) + TLVBufferUsed(tb_p));

 done:
    my_CFRelease(&identity);
    return (pkt);
}

STATIC bool
eapaka_challenge_process_encr_data(EAPAKAContextRef context, TLVListRef tlvs_p)
{
    uint8_t *		decrypted_buffer = NULL;
    TLVListDeclare(	decrypted_tlvs_p);
    AT_ENCR_DATA * 	encr_data_p;
    AT_IV * 		iv_p;
    CFStringRef		next_reauth_id;
    CFStringRef		next_pseudonym;

    TLVListInit(decrypted_tlvs_p);
    encr_data_p = (AT_ENCR_DATA *)TLVListLookupAttribute(tlvs_p, kAT_ENCR_DATA);
    if (encr_data_p == NULL) {
	return (TRUE);
    }
    iv_p = (AT_IV *)TLVListLookupAttribute(tlvs_p, kAT_IV);
    if (iv_p == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapaka: Challenge missing AT_IV");
	return (FALSE);
    }
    decrypted_buffer 
	= EAPSIMAKAKeyInfoDecryptTLVList(&context->key_info, encr_data_p, iv_p,
					 decrypted_tlvs_p);
    if (decrypted_buffer == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: Challenge decrypt AT_ENCR_DATA failed");
	return (FALSE);
    }
    {
	CFStringRef	str;

	str = TLVListCopyDescription(decrypted_tlvs_p);
	EAPLOG(-LOG_DEBUG, "Decrypted TLVs:\n%@", str);
	CFRelease(str);
    }
    
    /* save the next fast re-auth id */
    next_reauth_id = TLVListCreateStringFromAttribute(decrypted_tlvs_p, 
						      kAT_NEXT_REAUTH_ID);
    if (next_reauth_id != NULL) {
	EAPSIMAKAPersistentStateSetReauthID(context->persist,
					    next_reauth_id);
	CFRelease(next_reauth_id);
    }
    /* save the next pseudonym */
    next_pseudonym = TLVListCreateStringFromAttribute(decrypted_tlvs_p, 
						      kAT_NEXT_PSEUDONYM);
    if (next_pseudonym != NULL) {
	EAPSIMAKAPersistentStateSetPseudonym(context->persist,
					     next_pseudonym);
	CFRelease(next_pseudonym);
    }
    if (decrypted_buffer != NULL) {
	free(decrypted_buffer);
    }
    TLVListFree(decrypted_tlvs_p);
    return (TRUE);
}

STATIC bool
eapaka_authenticate(EAPAKAContextRef context,
		    CFDataRef rand, CFDataRef autn, AKAAuthResultsRef results)
{
    bool	success;

    if (context->static_keys.ck != NULL) {
	/* use statically defined information */
	AKAAuthResultsInit(results);
	AKAAuthResultsSetCK(results, context->static_keys.ck);
	AKAAuthResultsSetIK(results, context->static_keys.ik);
	AKAAuthResultsSetRES(results, context->static_keys.res);
	success = TRUE;
    }
    else {
	success = SIMAuthenticateAKA(rand, autn, results);
    }
    return (success);

}


STATIC EAPPacketRef
eapaka_challenge(EAPAKAContextRef context,
		 const EAPPacketRef in_pkt,
		 TLVListRef tlvs_p,
		 EAPClientStatus * client_status)
{
    AKAAuthResults	aka_results;
    CFDataRef		autn;
    AT_AUTN *		autn_p;
    bool		auth_success;
    CFDataRef		ck;
    CFDataRef		ik;
    int			len;
    AT_MAC *		mac_p;
    EAPPacketRef	pkt = NULL;
    AT_RAND *		rand_p;
    CFDataRef		rand;
    CFDataRef		res;
    AT_RES *		res_p;
    CC_SHA1_CTX		sha1_context;
    TLVBufferDeclare(	tb_p);

    AKAAuthResultsInit(&aka_results);
    context->plugin_state = kEAPClientStateAuthenticating;
    context->state = kEAPAKAClientStateChallenge;
    EAPSIMAKAPersistentStateSetCounter(context->persist, 1); /* XXX */
    context->reauth_success = FALSE;
    rand_p = (AT_RAND *)TLVListLookupAttribute(tlvs_p, kAT_RAND);
    if (rand_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: Challenge is missing AT_RAND");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    autn_p = (AT_AUTN *)TLVListLookupAttribute(tlvs_p, kAT_AUTN);
    if (autn_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: Challenge is missing AT_AUTN");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapaka: Challenge is missing AT_MAC");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* get CK, IK, RES from the SIM */
    rand = CFDataCreateWithBytesNoCopy(NULL, rand_p->ra_rand, RAND_SIZE,
				       kCFAllocatorNull);
    autn = CFDataCreateWithBytesNoCopy(NULL, autn_p->an_autn, AUTN_SIZE,
				       kCFAllocatorNull);
    auth_success = eapaka_authenticate(context, rand, autn, &aka_results);
    CFRelease(rand);
    CFRelease(autn);
    if (auth_success == FALSE) {
	*client_status = kEAPClientStatusInternalError;
	goto done;
    }
    ck = aka_results.ck;
    if (ck == NULL) {
	CFDataRef		auts;
	EAPSIMAKAPacketSubtype	subtype;

	auts = aka_results.auts;
	subtype = (auts != NULL)
	    ? kEAPSIMAKAPacketSubtypeAKASynchronizationFailure
	    : kEAPSIMAKAPacketSubtypeAKAAuthenticationReject;
	pkt = make_response_packet(context, in_pkt, subtype, tb_p);
	if (auts != NULL) {
	    AT_AUTS *		auts_p;
	    int			len;

	    len = (int)CFDataGetLength(auts);
	    if (len != AUTS_SIZE) {
		EAPLOG(LOG_NOTICE,
		       "eapaka: SIM bogus AUTS size %d (should be %d)",
		       len, AUTS_SIZE);
		*client_status = kEAPClientStatusInternalError;
	    }
	    auts_p = (AT_AUTS *)
		TLVBufferAllocateTLV(tb_p, kAT_AUTS, sizeof(AT_AUTS));
	    bcopy(CFDataGetBytePtr(auts), auts_p->as_auts, AUTS_SIZE);
	}
	EAPPacketSetLength(pkt,
			   offsetof(EAPAKAPacket, attrs) + TLVBufferUsed(tb_p));
	goto done;
    }

    /*
     * generate the MK:
     * MK = SHA1(Identity|IK|CK)
     */
    CC_SHA1_Init(&sha1_context);
    if (context->last_identity != NULL) {
	CC_SHA1_Update(&sha1_context, CFDataGetBytePtr(context->last_identity),
		       (int)CFDataGetLength(context->last_identity));
    }
    else {
	CC_SHA1_Update(&sha1_context, context->plugin->username, 
		       context->plugin->username_length);
    }
    ik = aka_results.ik;
    CC_SHA1_Update(&sha1_context, CFDataGetBytePtr(ik),
		   (int)CFDataGetLength(ik));
    CC_SHA1_Update(&sha1_context, CFDataGetBytePtr(ck),
		   (int)CFDataGetLength(ck));
    CC_SHA1_Final(EAPSIMAKAPersistentStateGetMasterKey(context->persist),
		  &sha1_context);

    /* now run PRF to generate keying material */
    fips186_2prf(EAPSIMAKAPersistentStateGetMasterKey(context->persist),
		 context->key_info.key);

    /* validate the MAC */
    if (!EAPSIMAKAKeyInfoVerifyMAC(&context->key_info,
				   in_pkt, mac_p->ma_mac, NULL, 0)) {
	EAPLOG(LOG_NOTICE,
	       "eapaka: Challenge AT_MAC not valid");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* check for and process encrypted data */
    if (eapaka_challenge_process_encr_data(context, tlvs_p) == FALSE) {
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* create our response */
    pkt = make_response_packet(context, in_pkt, 
			       kEAPSIMAKAPacketSubtypeAKAChallenge, tb_p);
    /* AT_MAC */
    mac_p = (AT_MAC *)TLVBufferAllocateTLV(tb_p, kAT_MAC,
					   sizeof(AT_MAC));
    if (mac_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: failed allocating AT_MAC, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    net_uint16_set(mac_p->ma_reserved, 0);

    /* AT_RES */
    res = aka_results.res;
    len = (int)CFDataGetLength(res);
    res_p = (AT_RES *)TLVBufferAllocateTLV(tb_p, kAT_RES,
					   offsetof(AT_RES, rs_res) + len);
    if (res_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: failed allocating AT_RES, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
#define NBITS_PER_BYTE	8
    net_uint16_set(res_p->rs_res_length, len * NBITS_PER_BYTE);
    bcopy(CFDataGetBytePtr(res), res_p->rs_res, len);

    EAPPacketSetLength(pkt,
		       offsetof(EAPAKAPacket, attrs) + TLVBufferUsed(tb_p));

    /* compute/set the MAC value */
    EAPSIMAKAKeyInfoSetMAC(&context->key_info, pkt, mac_p->ma_mac, NULL, 0);

    /* as far as we're concerned, we're successful */
    context->state = kEAPAKAClientStateSuccess;
    context->key_info_valid = TRUE;
    save_persistent_state(context);
    
 done:
    AKAAuthResultsRelease(&aka_results);
    return (pkt);
}

STATIC void
eapaka_compute_reauth_key(EAPAKAContextRef context,
			  AT_COUNTER * counter_p,
			  AT_NONCE_S * nonce_s_p)

{
    const void *	identity;
    int			identity_length;

    if (context->last_identity != NULL) {
	identity = CFDataGetBytePtr(context->last_identity);
	identity_length = (int)CFDataGetLength(context->last_identity);
    }
    else {
	identity = context->plugin->username;
	identity_length = context->plugin->username_length;
    }
    EAPSIMAKAKeyInfoComputeReauthKey(&context->key_info,
				     context->persist,
				     identity, identity_length,
				     counter_p, nonce_s_p);
    return;
}

#define ENCR_BUFSIZE 	(sizeof(AT_COUNTER) + sizeof(AT_COUNTER_TOO_SMALL))
#define ENCR_BUFSIZE_R	AT_ENCR_DATA_ROUNDUP(ENCR_BUFSIZE)

STATIC EAPPacketRef
eapaka_reauthentication(EAPAKAContextRef context,
			const EAPPacketRef in_pkt,
			TLVListRef tlvs_p,
			EAPClientStatus * client_status)
{
    uint16_t		at_counter;
    AT_COUNTER *	counter_p;
    bool		force_fullauth = FALSE;
    uint8_t		encr_buffer[ENCR_BUFSIZE_R];
    TLVBufferDeclare(	encr_tb_p);
    uint8_t *		decrypted_buffer = NULL;
    TLVListDeclare(	decrypted_tlvs_p);
    AT_ENCR_DATA * 	encr_data_p;
    AT_IV * 		iv_p;
    AT_MAC *		mac_p;
    CFStringRef		next_reauth_id;
    AT_NONCE_S *	nonce_s_p;
    EAPPacketRef	pkt = NULL;
    CFStringRef		reauth_id = NULL;
    TLVBufferDeclare(	tb_p);

    TLVListInit(decrypted_tlvs_p);
    if (context->key_info_valid == FALSE) {
	EAPLOG(LOG_NOTICE, 
	       "eapaka: Reauthentication but no key info available");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    reauth_id = EAPSIMAKAPersistentStateGetReauthID(context->persist);
    if (reauth_id == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapaka: received Reauthentication but don't have reauth id");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    context->state = kEAPAKAClientStateReauthentication;
    context->plugin_state = kEAPClientStateAuthenticating;

    /* validate the MAC */
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapaka: Reauthentication is missing AT_MAC");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (!EAPSIMAKAKeyInfoVerifyMAC(&context->key_info, in_pkt, mac_p->ma_mac,
				   NULL, 0)) {
	EAPLOG(LOG_NOTICE,
	       "eapaka: Reauthentication AT_MAC not valid");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* packet must contain AT_ENCR_DATA, AT_IV */
    encr_data_p = (AT_ENCR_DATA *)TLVListLookupAttribute(tlvs_p, kAT_ENCR_DATA);
    iv_p = (AT_IV *)TLVListLookupAttribute(tlvs_p, kAT_IV);
    if (encr_data_p == NULL || iv_p == NULL) {
	if (encr_data_p == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka:  Reauthentication missing AT_ENCR_DATA");
	}
	if (iv_p == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka:  Reauthentication missing AT_IV");
	}
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    decrypted_buffer 
	= EAPSIMAKAKeyInfoDecryptTLVList(&context->key_info, encr_data_p, iv_p,
					 decrypted_tlvs_p);
    if (decrypted_buffer == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapaka: failed to decrypt Reauthentication AT_ENCR_DATA");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    {
	CFStringRef	str;

	str = TLVListCopyDescription(decrypted_tlvs_p);
	EAPLOG(-LOG_DEBUG, "Decrypted TLVs:\n%@", str);
	CFRelease(str);
    }

    /* Reauthentication must contain AT_NONCE_S, AT_COUNTER */
    nonce_s_p 
	= (AT_NONCE_S *)TLVListLookupAttribute(decrypted_tlvs_p, kAT_NONCE_S);
    counter_p 
	= (AT_COUNTER *)TLVListLookupAttribute(decrypted_tlvs_p, kAT_COUNTER);
    if (nonce_s_p == NULL || counter_p == NULL) {
	if (nonce_s_p == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: Reauthentication AT_ENCR_DATA missing AT_NONCE_S");
	}
	if (counter_p == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: Reauthentication AT_ENCR_DATA missing AT_COUNTER");
	}
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* check the at_counter */
    at_counter = net_uint16_get(counter_p->co_counter);
    if (at_counter < EAPSIMAKAPersistentStateGetCounter(context->persist)) {
	force_fullauth = TRUE;
    }
    else {
	/* save the next fast re-auth id */
	next_reauth_id = TLVListCreateStringFromAttribute(decrypted_tlvs_p, 
							  kAT_NEXT_REAUTH_ID);
	if (next_reauth_id != NULL) {
	    EAPSIMAKAPersistentStateSetReauthID(context->persist,
						next_reauth_id);
	    CFRelease(next_reauth_id);
	}
	EAPSIMAKAPersistentStateSetCounter(context->persist, at_counter);
    }
    
    /* create our response */
    pkt = make_response_packet(context, in_pkt,
			       kEAPSIMAKAPacketSubtypeReauthentication, tb_p);

    /* 
     * create nested attributes containing:
     * 	AT_COUNTER
     *  AT_COUNTER_TOO_SMALL (if necessary)
     */
    TLVBufferInit(encr_tb_p, encr_buffer, sizeof(encr_buffer));
    if (TLVBufferAddCounter(encr_tb_p, at_counter) == FALSE) {
	EAPLOG(LOG_NOTICE, "eapaka: failed allocating AT_COUNTER, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    if (force_fullauth
	&& TLVBufferAddCounterTooSmall(encr_tb_p) == FALSE) {
	EAPLOG(LOG_NOTICE,
	       "eapaka: failed allocating AT_COUNTER_TOO_SMALL, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }

    /* AT_IV and AT_ENCR_DATA */
    if (!EAPSIMAKAKeyInfoEncryptTLVs(&context->key_info, tb_p, encr_tb_p)) {
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }

    /* AT_MAC */
    mac_p = (AT_MAC *)TLVBufferAllocateTLV(tb_p, kAT_MAC,
					   sizeof(AT_MAC));
    if (mac_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: failed allocating AT_MAC, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    net_uint16_set(mac_p->ma_reserved, 0);

    /* set the packet length */
    EAPPacketSetLength(pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(tb_p));

    /* compute/set the MAC value */
    EAPSIMAKAKeyInfoSetMAC(&context->key_info, pkt,
			   mac_p->ma_mac, nonce_s_p->nc_nonce_s,
			   sizeof(nonce_s_p->nc_nonce_s));

    if (force_fullauth == FALSE) {
	/* as far as we're concerned, we're successful */
	context->state = kEAPAKAClientStateSuccess;
	eapaka_compute_reauth_key(context, counter_p, nonce_s_p);
	context->key_info_valid = TRUE;
	context->reauth_success = TRUE;
    }
    else {
	context->key_info_valid = FALSE;
    }
    save_persistent_state(context);

 done:
    if (decrypted_buffer != NULL) {
	free(decrypted_buffer);
    }
    TLVListFree(decrypted_tlvs_p);
    return (pkt);
}

#define ENCR_BUFSIZE_NOTIF 	(sizeof(AT_COUNTER))
#define ENCR_BUFSIZE_NOTIF_R	AT_ENCR_DATA_ROUNDUP(ENCR_BUFSIZE_NOTIF)

STATIC EAPPacketRef
eapaka_notification(EAPAKAContextRef context,
		    const EAPPacketRef in_pkt,
		    TLVListRef tlvs_p,
		    EAPClientStatus * client_status,
		    EAPClientDomainSpecificError * error)
{
    bool		after_auth;
    uint16_t		current_at_counter;
    bool		do_replay_protection = FALSE;
    AT_NOTIFICATION *	notification_p;
    AT_MAC *		mac_p;
    uint16_t		notification_code = 0;
    EAPPacketRef	pkt = NULL;
    TLVBufferDeclare(	tb_p);

    *client_status = kEAPClientStatusOK;
    *error = 0;
    notification_p = 
	(AT_NOTIFICATION *)TLVListLookupAttribute(tlvs_p, kAT_NOTIFICATION);

    if (notification_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapaka: Notification does not contain "
	       "AT_NOTIFICATION attribute");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    notification_code = net_uint16_get(notification_p->nt_notification);
    after_auth = ATNotificationPhaseIsAfterAuthentication(notification_code);
    if (ATNotificationCodeIsSuccess(notification_code) && after_auth == FALSE) {
	EAPLOG(LOG_NOTICE, 
	       "eapaka: Notification code '%d' indicates "
	       "success before authentication", notification_code);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    /* validate the MAC */
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	if (after_auth) {
	    EAPLOG(LOG_NOTICE, "eapaka: Notification is missing AT_MAC");
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
    }
    else {
	if (after_auth == FALSE) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: Notification incorrectly contains AT_MAC");
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}

	if (!EAPSIMAKAKeyInfoVerifyMAC(&context->key_info, in_pkt, 
				       mac_p->ma_mac, NULL, 0)) {
	    EAPLOG(LOG_NOTICE, "eapaka: Notification AT_MAC not valid");
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
    }
    current_at_counter = EAPSIMAKAPersistentStateGetCounter(context->persist);
    do_replay_protection = context->reauth_success && after_auth;
    if (do_replay_protection) {
	uint16_t	at_counter;
	uint8_t *	decrypted_buffer;
	AT_ENCR_DATA *	encr_data_p;
	TLVListDeclare(	decrypted_tlvs_p);
	bool		has_counter = FALSE;
	AT_IV * 	iv_p;

	encr_data_p 
	    = (AT_ENCR_DATA *)TLVListLookupAttribute(tlvs_p, kAT_ENCR_DATA);
	iv_p = (AT_IV *)TLVListLookupAttribute(tlvs_p, kAT_IV);
	if (encr_data_p == NULL || iv_p == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: Notification after re-auth missing "
		   "AT_ENCR_DATA (%p) or AT_IV (%p)", encr_data_p, iv_p);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	TLVListInit(decrypted_tlvs_p);
	decrypted_buffer 
	    = EAPSIMAKAKeyInfoDecryptTLVList(&context->key_info,
					     encr_data_p, iv_p,
					     decrypted_tlvs_p);
	if (decrypted_buffer != NULL) {
	    AT_COUNTER *	counter_p;
	    CFStringRef		str;

	    str = TLVListCopyDescription(decrypted_tlvs_p);
	    EAPLOG(-LOG_DEBUG, "Decrypted TLVs:\n%@", str);
	    CFRelease(str);

	    counter_p = (AT_COUNTER *)TLVListLookupAttribute(decrypted_tlvs_p,
							     kAT_COUNTER);
	    if (counter_p != NULL) {
		at_counter = net_uint16_get(counter_p->co_counter);
		has_counter = TRUE;
	    }
	    free(decrypted_buffer);
	    TLVListFree(decrypted_tlvs_p);
	}
	else {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: failed to decrypt Notification AT_ENCR_DATA");
	    *client_status = kEAPClientStatusInternalError;
	    goto done;
	}
	if (!has_counter) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka:  Notification AT_ENCR_DATA missing AT_COUNTER");
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	if (at_counter != current_at_counter) {
	    EAPLOG(LOG_NOTICE, "eapaka: Notification AT_COUNTER (%d) does not "
		   "match current counter (%d)", at_counter,
		   current_at_counter);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
    }

    /* create our response */
    pkt = make_response_packet(context, in_pkt,
			       kEAPSIMAKAPacketSubtypeNotification,
			       tb_p);
    if (do_replay_protection) {
	uint8_t			encr_buffer[ENCR_BUFSIZE_NOTIF_R];
	TLVBufferDeclare(	encr_tb_p);

	/*
	 * create nested attributes containing:
	 *    AT_COUNTER
	 */
	TLVBufferInit(encr_tb_p, encr_buffer, sizeof(encr_buffer));
	if (TLVBufferAddCounter(encr_tb_p, current_at_counter) == FALSE) {
	    EAPLOG(LOG_NOTICE, "eapaka: failed to allocate AT_COUNTER, %s",
		   TLVBufferErrorString(encr_tb_p));
	    *client_status = kEAPClientStatusAllocationFailed;
	    goto done;
	}

	/* AT_IV and AT_ENCR_DATA */
	if (!EAPSIMAKAKeyInfoEncryptTLVs(&context->key_info, tb_p, encr_tb_p)) {
	    *client_status = kEAPClientStatusInternalError;
	    pkt = NULL;
	    goto done;
	}
    }
    if (mac_p != NULL) {
	/* AT_MAC */
	mac_p = (AT_MAC *)TLVBufferAllocateTLV(tb_p, kAT_MAC, sizeof(AT_MAC));
	if (mac_p == NULL) {
	    EAPLOG(LOG_NOTICE, "eapaka: failed allocating AT_MAC, %s",
		   TLVBufferErrorString(tb_p));
	    *client_status = kEAPClientStatusAllocationFailed;
	    pkt = NULL;
	    goto done;
	}
	net_uint16_set(mac_p->ma_reserved, 0);
    }

    /* set the packet length */
    EAPPacketSetLength(pkt, 
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(tb_p));
    if (mac_p != NULL) {
	/* compute/set the MAC value */
	EAPSIMAKAKeyInfoSetMAC(&context->key_info, pkt, mac_p->ma_mac, NULL, 0);
    }
    if (ATNotificationCodeIsSuccess(notification_code)) {
	context->state = kEAPAKAClientStateSuccess;
    }
    else {
	const char *	str;

	context->state = kEAPAKAClientStateFailure;
	*client_status = kEAPClientStatusPluginSpecificError;
	*error = EAPSIMAKAStatusForATNotificationCode(notification_code);
	str = ATNotificationCodeGetString(notification_code);
	if (str == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapaka: Notification code '%d' unrecognized failure",
		   notification_code);
	}
	else {
	    EAPLOG(LOG_NOTICE, "eapaka: Notification: %s", str);
	}
    }

 done:
    return (pkt);
}

STATIC EAPPacketRef
eapaka_request(EAPAKAContextRef context,
	       const EAPPacketRef in_pkt,
	       EAPClientStatus * client_status,
	       EAPClientDomainSpecificError * error)
{
    EAPAKAPacketRef	eapaka_in = (EAPAKAPacketRef)in_pkt;
    EAPPacketRef	eapaka_out = NULL;
    uint16_t		in_length = EAPPacketGetLength(in_pkt);
    uint8_t		subtype;
    TLVListDeclare(	tlvs_p);

    TLVListInit(tlvs_p);
    if (in_length <= kEAPSIMAKAPacketHeaderLength) {
	EAPLOG_FL(LOG_NOTICE, "length %d <= %ld",
		  in_length, kEAPSIMAKAPacketHeaderLength);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (TLVListParse(tlvs_p, eapaka_in->attrs,
		     in_length - kEAPSIMAKAPacketHeaderLength) == FALSE) {
	EAPLOG_FL(LOG_NOTICE, "parse failed: %s",
		  TLVListErrorString(tlvs_p));
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (context->state != kEAPAKAClientStateNone
	&& context->previous_identifier == in_pkt->identifier) {
	/* re-send our previous response */
	return ((EAPPacketRef)context->pkt);
    }
    subtype = eapaka_in->subtype;
    switch (subtype) {
    case kEAPSIMAKAPacketSubtypeAKAChallenge:
	eapaka_out = eapaka_challenge(context, in_pkt, tlvs_p, client_status);
	break;
    case kEAPSIMAKAPacketSubtypeAKAIdentity:
	eapaka_out = eapaka_identity(context, in_pkt, tlvs_p, client_status);
	break;
    case kEAPSIMAKAPacketSubtypeNotification:
	eapaka_out = eapaka_notification(context, in_pkt, tlvs_p,
					 client_status, error);
	break;
    case kEAPSIMAKAPacketSubtypeReauthentication:
	eapaka_out 
	    = eapaka_reauthentication(context, in_pkt, tlvs_p, client_status);
	break;
    default:
	*client_status = kEAPClientStatusProtocolError;
	EAPLOG_FL(LOG_NOTICE, "unexpected Subtype %s",
		  EAPSIMAKAPacketSubtypeGetString(subtype));
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

 done:
    TLVListFree(tlvs_p);
    if (*client_status != kEAPClientStatusOK) {
	context->plugin_state = kEAPClientStateFailure;
	context->state = kEAPAKAClientStateFailure;
    }
    if (eapaka_out == NULL
	&& *client_status == kEAPClientStatusProtocolError) {
	eapaka_out 
	    = make_client_error_packet(context, in_pkt,
				       kClientErrorCodeUnableToProcessPacket);
    }
    if (eapaka_out != NULL) {
	context->previous_identifier = in_pkt->identifier;
    }
    return (eapaka_out);

}

/**
 ** EAP-AKA module functions
 **/

/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect eapaka_introspect;
STATIC EAPClientPluginFuncVersion eapaka_version;
STATIC EAPClientPluginFuncEAPType eapaka_type;
STATIC EAPClientPluginFuncEAPName eapaka_name;
STATIC EAPClientPluginFuncInit eapaka_init;
STATIC EAPClientPluginFuncFree eapaka_free;
STATIC EAPClientPluginFuncProcess eapaka_process;
STATIC EAPClientPluginFuncFreePacket eapaka_free_packet;
STATIC EAPClientPluginFuncSessionKey eapaka_session_key;
STATIC EAPClientPluginFuncServerKey eapaka_server_key;
STATIC EAPClientPluginFuncMasterSessionKeyCopyBytes eapaka_msk_copy_bytes;
STATIC EAPClientPluginFuncPublishProperties eapaka_publish_props;
STATIC EAPClientPluginFuncUserName eapaka_user_name_copy;
STATIC EAPClientPluginFuncCopyIdentity eapaka_copy_identity;
STATIC EAPClientPluginFuncCopyPacketDescription eapaka_copy_packet_description;


STATIC EAPClientStatus
eapaka_init(EAPClientPluginDataRef plugin, CFArrayRef * require_props,
	    EAPClientDomainSpecificError * error)
{
    EAPAKAContextRef		context = NULL;
    EAPSIMAKAAttributeType 	identity_type;
    CFStringRef			imsi = NULL;
    AKAStaticKeys		static_keys;

    AKAStaticKeysClear(&static_keys);
    if (AKAStaticKeysInitWithProperties(&static_keys, plugin->properties)) {
	imsi = copy_static_imsi(plugin->properties);
	if (imsi == NULL) {
	    AKAStaticKeysRelease(&static_keys);
	    return (kEAPClientStatusConfigurationInvalid);
	}
	EAPLOG(LOG_NOTICE, "EAP-AKA: using static information");
    }
    else {
	/* check for a SIM module */
	imsi = SIMCopyIMSI();
	if (imsi == NULL) {
	    EAPLOG(LOG_NOTICE, "EAP-AKA: no SIM available");
	    return (kEAPClientStatusResourceUnavailable);
	}
	EAPLOG(LOG_NOTICE, "EAP-AKA: SIM found");
    }

    /* allocate a context */
    context = (EAPAKAContextRef)malloc(sizeof(*context));
    if (context == NULL) {
	CFRelease(imsi);
	AKAStaticKeysRelease(&static_keys);
	return (kEAPClientStatusAllocationFailed);
    }
    EAPAKAContextClear(context);
    context->static_keys = static_keys;
    identity_type = S_get_identity_type(plugin->properties);
    context->persist 
	= EAPSIMAKAPersistentStateCreate(kEAPTypeEAPAKA,
					 CC_SHA1_DIGEST_LENGTH,
					 imsi, identity_type);
    CFRelease(imsi);
    if (EAPSIMAKAPersistentStateGetReauthID(context->persist) != NULL) {
	/* now run PRF to generate keying material */
	fips186_2prf(EAPSIMAKAPersistentStateGetMasterKey(context->persist),
		     context->key_info.key);
	context->key_info_valid = TRUE;
    }
    context->plugin = plugin;
    plugin->private = context;
    return (kEAPClientStatusOK);
}

STATIC void
eapaka_free(EAPClientPluginDataRef plugin)
{
    EAPAKAContextFree(plugin->private);
    plugin->private = NULL;
    return;
}

STATIC void
eapaka_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    return;
}

STATIC EAPClientState
eapaka_process(EAPClientPluginDataRef plugin, 
	       const EAPPacketRef in_pkt,
	       EAPPacketRef * out_pkt_p, 
	       EAPClientStatus * client_status,
	       EAPClientDomainSpecificError * error)
{
    EAPAKAContextRef	context = (EAPAKAContextRef)plugin->private;

    *client_status = kEAPClientStatusOK;
    *error = 0;
    switch (in_pkt->code) {
    case kEAPCodeRequest:
	*out_pkt_p = eapaka_request(context, in_pkt, client_status, error);
	break;
    case kEAPCodeSuccess:
	context->previous_identifier = -1;
	if (context->state == kEAPAKAClientStateSuccess) {
	    context->plugin_state = kEAPClientStateSuccess;
	}
	break;
    case kEAPCodeFailure:
	context->previous_identifier = -1;
	context->plugin_state = kEAPClientStateFailure;
	break;
    default:
	break;
    }
    return (context->plugin_state);
}

STATIC const char * 
eapaka_failure_string(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

STATIC void * 
eapaka_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPAKAContextRef	context = (EAPAKAContextRef)plugin->private;

    if (context->state == kEAPAKAClientStateSuccess 
	&& context->key_info_valid) {
	*key_length = 32;
	return (context->key_info.s.msk);
    }
    return (NULL);
}

STATIC void * 
eapaka_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPAKAContextRef	context = (EAPAKAContextRef)plugin->private;

    if (context->state == kEAPAKAClientStateSuccess
	&& context->key_info_valid) {
	*key_length = 32;
	return (context->key_info.s.msk + 32);
    }
    return (NULL);
}

STATIC int
eapaka_msk_copy_bytes(EAPClientPluginDataRef plugin, 
		      void * msk, int msk_size)
{
    EAPAKAContextRef	context = (EAPAKAContextRef)plugin->private;
    int			ret_msk_size = sizeof(context->key_info.s.msk);
    
    if (msk_size < ret_msk_size
	|| context->key_info_valid == FALSE
	|| context->state != kEAPAKAClientStateSuccess) {
	ret_msk_size = 0;
    }
    else {
	bcopy(context->key_info.s.msk, msk, ret_msk_size);
    }
    return (ret_msk_size);
}

STATIC CFDictionaryRef
eapaka_publish_props(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

STATIC CFStringRef
eapaka_user_name_copy(CFDictionaryRef properties)
{
    EAPSIMAKAAttributeType 	identity_type;
    CFStringRef			imsi;
    EAPSIMAKAPersistentStateRef persist;
    CFStringRef			ret_identity;

    ret_identity = copy_static_identity(properties);
    if (ret_identity != NULL) {
	return (ret_identity);
    }
    imsi = SIMCopyIMSI();
    if (imsi == NULL) {
	return (NULL);
    }
    identity_type = S_get_identity_type(properties);
    persist = EAPSIMAKAPersistentStateCreate(kEAPTypeEAPAKA,
					     CC_SHA1_DIGEST_LENGTH,
					     imsi, identity_type);
    CFRelease(imsi);
    if (persist != NULL) {
	ret_identity = sim_identity_create(persist, properties, 
					   identity_type, NULL);
	EAPSIMAKAPersistentStateRelease(persist);
    }
    return (ret_identity);
}

/*
 * Function: eapaka_copy_identity
 * Purpose:
 *   Return the current identity we should use in responding to an
 *   EAP Request Identity packet.
 */
STATIC CFStringRef
eapaka_copy_identity(EAPClientPluginDataRef plugin)
{
    EAPAKAContextRef	context = (EAPAKAContextRef)plugin->private;

    EAPAKAContextSetLastIdentity(context, NULL);
    context->state = kEAPAKAClientStateNone;
    context->previous_identifier = -1;
    if (context->static_keys.ck != NULL) {
	return (copy_static_identity(plugin->properties));
    }
    return (sim_identity_create(context->persist, plugin->properties,
				kAT_ANY_ID_REQ, NULL));
}

STATIC CFStringRef
eapaka_copy_packet_description(const EAPPacketRef pkt, bool * packet_is_valid)
{ 
    return (EAPSIMAKAPacketCopyDescription(pkt, packet_is_valid));
}

STATIC EAPType 
eapaka_type(void)
{
    return (kEAPTypeEAPAKA);

}

STATIC const char *
eapaka_name(void)
{
    return (EAP_AKA_NAME);
}

STATIC EAPClientPluginVersion 
eapaka_version(void)
{
    return (kEAPClientPluginVersion);
}

STATIC struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
#if 0
    { kEAPClientPluginFuncNameIntrospect, eapaka_introspect },
#endif /* 0 */
    { kEAPClientPluginFuncNameVersion, eapaka_version },
    { kEAPClientPluginFuncNameEAPType, eapaka_type },
    { kEAPClientPluginFuncNameEAPName, eapaka_name },
    { kEAPClientPluginFuncNameInit, eapaka_init },
    { kEAPClientPluginFuncNameFree, eapaka_free },
    { kEAPClientPluginFuncNameProcess, eapaka_process },
    { kEAPClientPluginFuncNameFreePacket, eapaka_free_packet },
    { kEAPClientPluginFuncNameFailureString, eapaka_failure_string },
    { kEAPClientPluginFuncNameSessionKey, eapaka_session_key },
    { kEAPClientPluginFuncNameServerKey, eapaka_server_key },
    { kEAPClientPluginFuncNameMasterSessionKeyCopyBytes,
      eapaka_msk_copy_bytes },
    { kEAPClientPluginFuncNamePublishProperties, eapaka_publish_props },
    { kEAPClientPluginFuncNameUserName, eapaka_user_name_copy },
    { kEAPClientPluginFuncNameCopyIdentity, eapaka_copy_identity },
    { kEAPClientPluginFuncNameCopyPacketDescription,
      eapaka_copy_packet_description },
    { NULL, NULL},
};


EAPClientPluginFuncRef
eapaka_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;

    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}

#ifdef TEST_EAPAKA_PLUGIN

STATIC uint8_t S_res_static[] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
};
STATIC uint8_t S_ck_static[] = {
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
};
STATIC uint8_t S_ik_static[] = {
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
};
#define RAND_STATIC					\
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,	\
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37

STATIC uint8_t S_rand_static[RAND_SIZE] = {
    RAND_STATIC
};

#define AUTN_STATIC					\
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,	\
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47

STATIC uint8_t S_autn_static[AUTN_SIZE] = {
    AUTN_STATIC
};

STATIC void
add_data_to_dict(CFMutableDictionaryRef dict, CFStringRef key,
		 const uint8_t * val, int val_len)
{
    CFDataRef	data;

    data = CFDataCreateWithBytesNoCopy(NULL, val, val_len, kCFAllocatorNull);
    CFDictionarySetValue(dict, key, data);
    CFRelease(data);
    return;
}

STATIC CFDictionaryRef
make_props(void)
{
    CFMutableDictionaryRef	dict;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(dict, kEAPClientPropEAPSIMAKAIMSI,
			 CFSTR("244070100000001"));
    CFDictionarySetValue(dict, kEAPClientPropEAPSIMAKARealm,
			 CFSTR("eapaka.foo"));
    add_data_to_dict(dict, kEAPClientPropEAPAKARES,
		     S_res_static, sizeof(S_res_static));
    add_data_to_dict(dict, kEAPClientPropEAPAKACk,
		     S_ck_static, sizeof(S_ck_static));
    add_data_to_dict(dict, kEAPClientPropEAPAKAIk,
		     S_ik_static, sizeof(S_ik_static));
    return (dict);
}


/*
  EAP-Request/AKA/AKA-Identity AT_ANY_ID_REQ

  01                   ; Code: Request
  01                   ; Identifier: 1
  00 0c                ; Length: 12 octets
  17                   ; Type: EAP-AKA
  05                ; subtype: AKA-Identity
  00 00             ; (reserved)
  0d                ; Attribute type: AT_ANY_ID_REQ (13 = d)
  01             ; Attribute length: 4 octets (1*4)
  00 00          ; (attribute padding)
*/
const uint8_t	eap_request_aka_identity_any_id[] = {
    0x01,
    0x01,
    0x00, 0x0c,
    0x17,
    0x05,
    0x00, 0x00,
    /* AT_ANY_ID_REQ */
    0x0d,
    0x01,
    0x00, 0x00
};

/*
  EAP-Request/AKA/AKA-Identity AT_FULLAUTH_ID_REQ

  01                   ; Code: Request
  01                   ; Identifier: 2
  00 0c                ; Length: 12 octets
  17                   ; Type: EAP-AKA
  05                ; subtype: AKA-Identity
  00 00             ; (reserved)
  0d                ; Attribute type: AT_FULLAUTH_ID_REQ (17 = 0x11)
  01             ; Attribute length: 4 octets (1*4)
  00 00          ; (attribute padding)
*/
const uint8_t	eap_request_aka_identity_fullauth_id[] = {
    0x01,
    0x02,
    0x00, 0x0c,
    0x17,
    0x05,
    0x00, 0x00,
    /* AT_FULLAUTH_ID_REQ */
    0x11,
    0x01,
    0x00, 0x00
};

/*
  EAP-Request/AKA/AKA-Identity AT_PERMANENT_ID_REQ

  01                   ; Code: Request
  01                   ; Identifier: 3
  00 0c                ; Length: 12 octets
  17                   ; Type: EAP-AKA
  05                ; subtype: AKA-Identity
  00 00             ; (reserved)
  0d                ; Attribute type: AT_PERMANENT_ID_REQ (10 = b)
  01             ; Attribute length: 4 octets (1*4)
  00 00          ; (attribute padding)
*/
const uint8_t	eap_request_aka_identity_permanent_id[] = {
    0x01,
    0x03,
    0x00, 0x0c,
    0x17,
    0x05,
    0x00, 0x00,
    /* AT_PERMANENT_ID_REQ */
    0x0a,
    0x01,
    0x00, 0x00
};

STATIC const uint8_t *	test_identity_good[] = {
    eap_request_aka_identity_any_id,
    eap_request_aka_identity_fullauth_id,
    eap_request_aka_identity_permanent_id,
};
STATIC const uint8_t *	test_identity_bad1[] = {
    eap_request_aka_identity_any_id,
    eap_request_aka_identity_fullauth_id,
    eap_request_aka_identity_permanent_id,
    eap_request_aka_identity_any_id,
};
STATIC const uint8_t *	test_identity_bad2[] = {
    eap_request_aka_identity_fullauth_id,
    eap_request_aka_identity_any_id,
};
STATIC const uint8_t *	test_identity_bad3[] = {
    eap_request_aka_identity_fullauth_id,
    eap_request_aka_identity_permanent_id,
    eap_request_aka_identity_any_id,
};
STATIC const uint8_t *	test_identity_good2[] = {
    eap_request_aka_identity_any_id,
};

#define countof(array)	(sizeof(array) / sizeof(array[0]))

typedef struct {
    const char *	name;
    const uint8_t * *	packet_list;
    int			packet_count;
    bool		expect_failure;
} TestPacketList, * TestPacketListRef;

STATIC TestPacketList S_tests[] = {
    {
	"good", test_identity_good, countof(test_identity_good), FALSE
    },
    {
	"bad1", test_identity_bad1, countof(test_identity_bad1), TRUE
    },
    {
	"bad2", test_identity_bad2, countof(test_identity_bad2), TRUE
    },
    {
	"bad3", test_identity_bad3, countof(test_identity_bad3), TRUE
    },
    {
	"good2", test_identity_good2, countof(test_identity_good2), FALSE
    }
};


STATIC bool
process_packets(EAPClientPluginDataRef data, TestPacketListRef test)
{
    EAPClientState			client_state;
    EAPClientDomainSpecificError 	error;
    bool				got_failure = FALSE;
    int					i;
    EAPPacketRef			out_pkt;
    EAPClientStatus			status;

    for (i = 0; i < test->packet_count; i++) {
	printf("\nReceive packet:\n");
	EAPSIMAKAPacketDump(stdout, (EAPPacketRef)(test->packet_list[i]));
	out_pkt = NULL;
	client_state 
	    = eapaka_process(data,
			     (EAPPacketRef)(test->packet_list[i]),
			     &out_pkt,
			     &status,
			     &error);
	if (client_state == kEAPClientStateFailure) {
	    got_failure = TRUE;
	}
	else {
	    if (out_pkt != NULL) {
		printf("\nSend packet:\n");
		EAPSIMAKAPacketDump(stdout, out_pkt);
	    }
	}
    }
    if (got_failure != test->expect_failure) {
	fprintf(stderr, "%s: process packet %s unexpectedly\n",
		test->name, got_failure ? "failed" : "succeeded");
	return (FALSE);
    }
    return (TRUE);
}

STATIC char S_packet_buffer[1500];

STATIC EAPPacketRef
make_request_packet(int identifier,
		    EAPSIMAKAPacketSubtype subtype,
		    TLVBufferRef tb_p)
{
    EAPAKAPacketRef	pkt;

    pkt = (EAPAKAPacketRef)S_packet_buffer;
    TLVBufferInit(tb_p, pkt->attrs,
		  sizeof(S_packet_buffer) - offsetof(EAPAKAPacket, attrs));
    pkt->code = kEAPCodeRequest;
    pkt->identifier = identifier;
    pkt->type = kEAPTypeEAPAKA;
    pkt->subtype = subtype;
    net_uint16_set(pkt->reserved, 0);
    return ((EAPPacketRef)pkt);

}

STATIC void
send_challenge(EAPClientPluginDataRef data, CFStringRef identity)

{
    AT_AUTN *		autn_p;
    CFDataRef		identity_data;
    EAPSIMAKAKeyInfo	key_info;
    AT_MAC *		mac_p;
    uint8_t		master_key[CC_SHA1_DIGEST_LENGTH];
    EAPPacketRef	pkt;
    AT_RAND *		rand_p;
    CC_SHA1_CTX		sha1_context;
    TestPacketList 	test;
    TLVBufferDeclare(	tb_p);

    pkt = make_request_packet(4, kEAPSIMAKAPacketSubtypeAKAChallenge, tb_p);
    rand_p = (AT_RAND *)
	TLVBufferAllocateTLV(tb_p, kAT_RAND, sizeof(AT_RAND));
    bcopy(S_rand_static, rand_p->ra_rand, RAND_SIZE);
    autn_p = (AT_AUTN *)
	TLVBufferAllocateTLV(tb_p, kAT_AUTN, sizeof(AT_AUTN));
    bcopy(S_autn_static, autn_p->an_autn, AUTN_SIZE);

    /* AT_MAC */
    mac_p = (AT_MAC *)TLVBufferAllocateTLV(tb_p, kAT_MAC,
					   sizeof(AT_MAC));
    if (mac_p == NULL) {
	fprintf(stderr, "failed allocating AT_MAC, %s",
		TLVBufferErrorString(tb_p));
	return;
    }
    net_uint16_set(mac_p->ma_reserved, 0);

    /*
     * generate the MK:
     * MK = SHA1(Identity|IK|CK)
     */
    identity_data 
	= CFStringCreateExternalRepresentation(NULL, identity, 
					       kCFStringEncodingUTF8, 0);
    CC_SHA1_Init(&sha1_context);
    CC_SHA1_Update(&sha1_context, CFDataGetBytePtr(identity_data),
		   CFDataGetLength(identity_data));
    CC_SHA1_Update(&sha1_context, S_ik_static, sizeof(S_ik_static));
    CC_SHA1_Update(&sha1_context, S_ck_static, sizeof(S_ck_static));
    CC_SHA1_Final(master_key, &sha1_context);
    CFRelease(identity_data);

    /* now run PRF to generate keying material */
    fips186_2prf(master_key, key_info.key);
    EAPPacketSetLength(pkt,
		       offsetof(EAPAKAPacket, attrs) + TLVBufferUsed(tb_p));

    /* set the MAC value */
    EAPSIMAKAKeyInfoSetMAC(&key_info, pkt, mac_p->ma_mac, NULL, 0);

    test.name = "challenge";
    test.packet_list = (const uint8_t * *)&pkt;
    test.packet_count = 1;
    test.expect_failure = FALSE;

    if (process_packets(data, &test)) {
	printf("Test challenge: PASSED\n");
    }
    else {
	fprintf(stderr, "Test challenge: FAILED\n");
    }
    return;
}

int
main()
{
    EAPClientState			client_state;
    EAPClientPluginData			data;
    EAPClientDomainSpecificError 	error;
    CFStringRef				last_identity = NULL;
    int					i;
    CFArrayRef				require_props;
    EAPClientStatus			status;

    bzero(&data, sizeof(data));
    *((CFDictionaryRef *)&data.properties) = make_props();
    status = eapaka_init(&data, &require_props, &error);
    if (status != kEAPClientStatusOK) {
	fprintf(stderr, "eapaka_init failed %d\n", status);
	exit(1);
    }
    for (i = 0; i < countof(S_tests); i++) {
	/* this flushes the identity state */
	my_CFRelease(&last_identity);
	last_identity = eapaka_copy_identity(&data);
	if (process_packets(&data, S_tests + i) == FALSE) {
	    fprintf(stderr, "Test %s: FAILED\n", S_tests[i].name);
	}
	else {
	    printf("Test %s: PASSED\n", S_tests[i].name);
	}
    }
    if (last_identity == NULL) {
	fprintf(stderr, "why is last_identity NULL?");
	exit(1);
    }
    send_challenge(&data, last_identity);
    exit(0);
    return (0);
}
#endif /* TEST_EAPAKA_PLUGIN */
