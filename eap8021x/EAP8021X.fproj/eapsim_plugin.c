/*
 * Copyright (c) 2008-2014 Apple Inc. All rights reserved.
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

#define EAP_SIM_NAME		"EAP-SIM"

#define kEAPClientPropEAPSIMIMSI			\
    CFSTR("EAPSIMIMSI") 		/* string */
#define kEAPClientPropEAPSIMRealm			\
    CFSTR("EAPSIMRealm") 		/* string */
#define kEAPClientPropEAPSIMNumberOfRANDs				\
    CFSTR("EAPSIMNumberOfRANDs") 	/* number 2 or 3, default 3 */

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
STATIC EAPClientPluginFuncMasterSessionKeyCopyBytes eapsim_msk_copy_bytes;
STATIC EAPClientPluginFuncPublishProperties eapsim_publish_props;
STATIC EAPClientPluginFuncUserName eapsim_user_name_copy;
STATIC EAPClientPluginFuncCopyIdentity eapsim_copy_identity;
STATIC EAPClientPluginFuncCopyPacketDescription eapsim_copy_packet_description;

/**
 ** Protocol-specific defines
 **/

enum {
    kEAPSIMClientStateNone = 0,
    kEAPSIMClientStateStart = 1,
    kEAPSIMClientStateChallenge = 2,
    kEAPSIMClientStateReauthentication = 3,
    kEAPSIMClientStateSuccess = 4,
    kEAPSIMClientStateFailure = 5
};
typedef int	EAPSIMClientState;

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
    EAPClientPluginDataRef 	plugin;
    EAPClientState		plugin_state;
    EAPSIMClientState		state;
    int				previous_identifier;
    int				start_count;
    int				n_required_rands;
    EAPSIMAKAAttributeType	last_identity_type;
    CFDataRef			last_identity;
    SIMStaticTriplets		sim_static;
    EAPSIMAKAKeyInfo		key_info;
    bool			key_info_valid;
    uint8_t			nonce_mt[NONCE_MT_SIZE];
    EAPSIMAKAPersistentStateRef	persist;
    bool			reauth_success;
    uint16_t *			version_list;
    int				version_list_count;
    uint8_t			pkt[1500];
} EAPSIMContext, *EAPSIMContextRef;

/**
 ** Identity routines
 **/
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
    if (realm == NULL) {
	realm 
	    = isA_CFString(CFDictionaryGetValue(properties,
						kEAPClientPropEAPSIMAKARealm));
	if (realm == NULL) {
	    return (NULL);
	}
    }
    return (CFRetain(realm));
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
	imsi = isA_CFString(CFDictionaryGetValue(properties,
						 kEAPClientPropEAPSIMAKAIMSI));
	if (imsi == NULL) {
	    return (NULL);
	}
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
static int
S_get_plist_int(CFDictionaryRef plist, CFStringRef key, int def)
{
    CFNumberRef 	n;
    int			ret = def;

    n = isA_CFNumber(CFDictionaryGetValue(plist, key));
    if (n != NULL) {
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
    int             i;
    int             n;
    void *          p;
    uint32_t        random;

    n = len / sizeof(random);
    for (i = 0, p = buf; i < n; i++, p += sizeof(random)) {
        random = arc4random();
        bcopy(&random, p, sizeof(random));
    }
    return;
}

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
			    const void * list, int count)
{
    if (list != NULL) {
	int	size = count * sizeof(uint16_t);

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
    CFIndex		count;
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
    CFIndex		count;
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
    
    /*
     * If SRES and Kc are specified, use statically defined values.
     * If RAND is NULL, use the first element of the Kc/SRES arrays.
     * If RAND is not NULL, RAND, Kc, and SRES must be parallel arrays;
     * the RAND value provided by the server is looked up in the RAND array
     * and the corresponding Kc and SRES value is retrieved.
     */
    if (kc == NULL || sres == NULL) {
        goto failed;
    }
    count = CFArrayGetCount(kc);
    if (count != CFArrayGetCount(sres) 
	|| (rand != NULL && count != CFArrayGetCount(rand))) {
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
    EAPSIMAKAPersistentStateRelease(context->persist);
    EAPSIMContextSetLastIdentity(context, NULL);
    EAPSIMContextClear(context);
    free(context);
    return;
}

STATIC int
EAPSIMContextLookupStaticRAND(EAPSIMContextRef context, 
			      const uint8_t rand[SIM_RAND_SIZE])
{
    CFIndex	count;
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

    if (context->sim_static.kc != NULL) {
        /* use the static SIM information */
        rand_scan = rand_p;
        kc_scan = kc_p;
        sres_scan = sres_p;
        for (i = 0; i < count; i++) {
            CFDataRef	kc_data;
            CFDataRef	sres_data;
            int		where;
            
            if (context->sim_static.rand != NULL) {
                where = EAPSIMContextLookupStaticRAND(context, rand_scan);
                if (where == -1) {
                    EAPLOG(LOG_NOTICE, "eapsim: can't find static RAND value");
                    return (FALSE);
                }
            }
	    else {
                /* If RAND is NULL, choose first KC and SRES */
                where = 0;
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
        if (SIMAuthenticateGSM(rand_p, count, kc_p, sres_p) == FALSE) {
            EAPLOG(LOG_NOTICE, "SIMAuthenticateGSM failed");
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
    EAPSIMContextRef		context = NULL;
    EAPSIMAKAAttributeType 	identity_type;
    CFStringRef			imsi = NULL;
    SIMStaticTriplets		triplets;

    /* for testing, allow static triplets to override a real SIM */
    bzero(&triplets, sizeof(triplets));
    if (SIMStaticTripletsInitFromProperties(&triplets,
					    plugin->properties)) {
	imsi = copy_static_imsi(plugin->properties);
	if (imsi == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim: static triplets specified but IMSI missing");
	    SIMStaticTripletsInitFromProperties(&triplets, NULL);
	    return (kEAPClientStatusConfigurationInvalid);
	}
    }
    else {
	/* check for a real SIM module */
	imsi = SIMCopyIMSI();
	if (imsi == NULL) {
	    EAPLOG(LOG_NOTICE, "EAP-SIM: no SIM available");
	    return (kEAPClientStatusResourceUnavailable);
	}
	EAPLOG(LOG_NOTICE, "EAP-SIM: SIM found");
    }

    /* allocate a context */
    context = (EAPSIMContextRef)malloc(sizeof(*context));
    if (context == NULL) {
	CFRelease(imsi);
	(void)SIMStaticTripletsInitFromProperties(&triplets, NULL);
	return (kEAPClientStatusAllocationFailed);
    }
    EAPSIMContextClear(context);
    identity_type = S_get_identity_type(plugin->properties);
    context->persist 
	= EAPSIMAKAPersistentStateCreate(kEAPTypeEAPSIM,
					 CC_SHA1_DIGEST_LENGTH,
					 imsi, identity_type);
    CFRelease(imsi);
    context->sim_static = triplets;
    context->n_required_rands 
	= S_get_plist_int(plugin->properties,
			  kEAPClientPropEAPSIMNumberOfRANDs,
			  EAPSIM_MAX_RANDS);
    if (context->n_required_rands != 2
	&& context->n_required_rands != 3) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: EAPSIMNumberOfRands %d is invalid, using 3 instead",
	       context->n_required_rands);
	context->n_required_rands = EAPSIM_MAX_RANDS;
    }
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
		     EAPPacketRef in_pkt, EAPSIMAKAPacketSubtype subtype,
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
    net_uint16_set(pkt->reserved, 0);
    return ((EAPPacketRef)pkt);
}

STATIC EAPPacketRef
eapsim_make_client_error(EAPSIMContextRef context,
			 EAPPacketRef in_pkt, ClientErrorCode code)
{
    AttrUnion			attr;
    EAPPacketRef		pkt;
    TLVBufferDeclare(		tb_p);

    pkt = eapsim_make_response(context, in_pkt,
			       kEAPSIMAKAPacketSubtypeClientError, tb_p);
    attr.tlv_p = TLVBufferAllocateTLV(tb_p, kAT_CLIENT_ERROR_CODE,
				      sizeof(AT_CLIENT_ERROR_CODE));
    if (attr.tlv_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapsim: failed allocating AT_CLIENT_ERROR_CODE, %s",
	       TLVBufferErrorString(tb_p));
	return (NULL);
    }
    net_uint16_set(attr.at_client_error_code->ce_client_error_code, code);
    EAPPacketSetLength(pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(tb_p));

    return (pkt);
}

STATIC void
save_persistent_state(EAPSIMContextRef context)
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
eapsim_start(EAPSIMContextRef context,
	     const EAPPacketRef in_pkt,
	     TLVListRef tlvs_p,
	     EAPClientStatus * client_status)
{
    AttrUnion		attr;
    int			count;
    int			i;
    CFStringRef		identity = NULL;
    EAPSIMAKAAttributeType identity_req_type;
    bool		good_version = FALSE;
    EAPPacketRef	pkt = NULL;
    const uint8_t *	scan;
    bool		skip_identity = FALSE;
    TLVBufferDeclare(	tb_p);
    AT_VERSION_LIST *	version_list_p;

    version_list_p = (AT_VERSION_LIST *)
	TLVListLookupAttribute(tlvs_p, kAT_VERSION_LIST);
    if (version_list_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapsim: Start is missing AT_VERSION_LIST");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    count = net_uint16_get(version_list_p->vl_actual_length) / sizeof(uint16_t);
    for (i = 0, scan = version_list_p->vl_version_list;
	 i < count; i++, scan += sizeof(uint16_t)) {
	uint16_t	this_vers = net_uint16_get(scan);

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
				    version_list_p->vl_version_list, count);
    }
    else {
	EAPSIMContextSetVersionList(context, NULL, 0);
    }
    if (context->state != kEAPSIMClientStateStart) {
	/* starting over */
	context->plugin_state = kEAPClientStateAuthenticating;
	context->start_count = 0;
	context->last_identity_type = 0;
	context->state = kEAPSIMClientStateStart;
    }
    if (context->start_count == 0) {
	fill_with_random(context->nonce_mt, sizeof(context->nonce_mt));
    }
    context->start_count++;
    if (context->start_count > kEAPSIMAKAIdentityAttributesCount) {
	EAPLOG(LOG_NOTICE, "eapsim: too many Start packets (%d > %d)",
	       context->start_count, kEAPSIMAKAIdentityAttributesCount);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    identity_req_type = TLVListLookupIdentityAttribute(tlvs_p);
    switch (identity_req_type) {
    case kAT_ANY_ID_REQ:
	if (context->start_count > 1) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim: AT_ANY_ID_REQ at Start #%d",
		   context->start_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    case kAT_FULLAUTH_ID_REQ:
	if (context->start_count > 1
	    && context->last_identity_type != kAT_ANY_ID_REQ) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim: AT_FULLAUTH_ID_REQ follows %s at Start #%d",
		   EAPSIMAKAAttributeTypeGetString(context->last_identity_type),
		   context->start_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    case kAT_PERMANENT_ID_REQ:
	if (context->start_count > 1
	    && context->last_identity_type != kAT_ANY_ID_REQ
	    && context->last_identity_type != kAT_FULLAUTH_ID_REQ) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim: AT_PERMANENT_ID_REQ follows %s at Start #%d",
		   EAPSIMAKAAttributeTypeGetString(context->last_identity_type),
		   context->start_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	break;
    default:
	if (context->start_count > 1) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim: no *ID_REQ follows %s at Start #%d",
		   EAPSIMAKAAttributeTypeGetString(context->last_identity_type),
		   context->start_count);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	/* no need to submit an identity */
	skip_identity = TRUE;
	break;
    }

    /* create our response */
    context->last_identity_type = identity_req_type;
    pkt = eapsim_make_response(context, in_pkt, kEAPSIMAKAPacketSubtypeSIMStart,
			       tb_p);

    if (!skip_identity) {
	CFDataRef	identity_data = NULL;
	Boolean		reauth_id_used = FALSE;

	if (context->sim_static.kc != NULL) {
	    identity = copy_static_identity(context->plugin->properties);
	}
	else {
	    identity = sim_identity_create(context->persist,
					   context->plugin->properties,
					   identity_req_type,
					   &reauth_id_used);
	}
	if (identity == NULL) {
	    EAPLOG(LOG_NOTICE, "eapsim: can't find SIM identity");
	    *client_status = kEAPClientStatusResourceUnavailable;
	    pkt = NULL;
	    goto done;
	}

	if (!TLVBufferAddIdentityString(tb_p, identity, &identity_data)) {
	    EAPLOG(LOG_NOTICE, "eapsim: can't add AT_IDENTITY, %s",
		   TLVBufferErrorString(tb_p));
	    *client_status = kEAPClientStatusInternalError;
	    pkt = NULL;
	    goto done;
	}
	EAPSIMContextSetLastIdentity(context, identity_data);
	my_CFRelease(&identity_data);

	if (reauth_id_used) {
	    /* packet only contains fast re-auth id */
	    goto packet_complete;
	}
    }

    /* We are now sure that re-auth will not take place */
    context->key_info_valid = FALSE;

    /* set the AT_SELECTED_VERSION attribute */
    attr.tlv_p = TLVBufferAllocateTLV(tb_p, kAT_SELECTED_VERSION,
				      sizeof(AT_SELECTED_VERSION));
    if (attr.tlv_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapsim: failed allocating AT_SELECTED_VERSION, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    net_uint16_set(attr.at_selected_version->sv_selected_version,
		   kEAPSIMVersion1);

    /* set the AT_NONCE_MT attribute */
    attr.tlv_p = TLVBufferAllocateTLV(tb_p, kAT_NONCE_MT,
				      sizeof(AT_NONCE_MT));
    if (attr.tlv_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapsim: failed allocating AT_NONCE_MT, %s",
	       TLVBufferErrorString(tb_p));
	pkt = NULL;
	goto done;
    }
    net_uint16_set(attr.at_nonce_mt->nm_reserved, 0);
    bcopy(context->nonce_mt, attr.at_nonce_mt->nm_nonce_mt,
	  sizeof(context->nonce_mt));

 packet_complete:
    /* packet fully formed, set the EAP packet length */
    EAPPacketSetLength(pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(tb_p));

 done:
    my_CFRelease(&identity);
    return (pkt);
}

STATIC bool
eapsim_challenge_process_encr_data(EAPSIMContextRef context, TLVListRef tlvs_p)
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
	       "eapsim: Challenge missing AT_IV");
	return (FALSE);
    }
    decrypted_buffer 
	= EAPSIMAKAKeyInfoDecryptTLVList(&context->key_info, encr_data_p, iv_p,
					 decrypted_tlvs_p);
    if (decrypted_buffer == NULL) {
	EAPLOG(LOG_NOTICE, "eapsim: Challenge decrypt AT_ENCR_DATA failed");
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

STATIC EAPPacketRef
eapsim_challenge(EAPSIMContextRef context,
		 const EAPPacketRef in_pkt,
		 TLVListRef tlvs_p,
		 EAPClientStatus * client_status)
{
    int			count;
    uint8_t		kc[SIM_KC_SIZE * EAPSIM_MAX_RANDS];
    AT_MAC *		mac_p;
    EAPPacketRef	pkt = NULL;
    AT_RAND *		rand_p;
    uint16_t		selected_version = htons(kEAPSIMVersion1);
    CC_SHA1_CTX		sha1_context;
    uint8_t		sres[SIM_SRES_SIZE * EAPSIM_MAX_RANDS];
    TLVBufferDeclare(	tb_p);

    if (context->state != kEAPSIMClientStateStart) {
	EAPLOG(LOG_NOTICE, "eapsim: Challenge sent without Start");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    context->state = kEAPSIMClientStateChallenge;
    EAPSIMAKAPersistentStateSetCounter(context->persist, 1);
    context->reauth_success = FALSE;
    rand_p = (AT_RAND *)TLVListLookupAttribute(tlvs_p, kAT_RAND);
    if (rand_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapsim: Challenge is missing AT_RAND");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    count = ((rand_p->ra_length * TLV_ALIGNMENT) 
	     - offsetof(AT_RAND, ra_rand)) / SIM_RAND_SIZE;
    if (count < context->n_required_rands) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: Challenge AT_RAND has %d RANDs, policy requires %d",
	       count, context->n_required_rands);
	pkt = eapsim_make_client_error(context, in_pkt, 
				       kClientErrorCodeInsufficientNumberOfChallenges);

	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    /* check that there aren't more than EAPSIM_MAX_RANDS */
    if (count > EAPSIM_MAX_RANDS) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: Challenge AT_RAND has %d RANDs > %d",
	       count, EAPSIM_MAX_RANDS);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (blocks_are_duplicated(rand_p->ra_rand, count, SIM_RAND_SIZE)) {
	EAPLOG(LOG_NOTICE,
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
		       (int)CFDataGetLength(context->last_identity));
    }
    else {
	CC_SHA1_Update(&sha1_context, context->plugin->username, 
		       context->plugin->username_length);
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
    CC_SHA1_Final(EAPSIMAKAPersistentStateGetMasterKey(context->persist),
		  &sha1_context);

    /* now run PRF to generate keying material */
    fips186_2prf(EAPSIMAKAPersistentStateGetMasterKey(context->persist),
		 context->key_info.key);

    /* validate the MAC */
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: Challenge is missing AT_MAC");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (!EAPSIMAKAKeyInfoVerifyMAC(&context->key_info, in_pkt, 
				   mac_p->ma_mac,
				   context->nonce_mt,
				   sizeof(context->nonce_mt))) {
	EAPLOG(LOG_NOTICE,
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
    pkt = eapsim_make_response(context, in_pkt, 
			       kEAPSIMAKAPacketSubtypeSIMChallenge, tb_p);
    mac_p = (AT_MAC *)TLVBufferAllocateTLV(tb_p, kAT_MAC,
					   sizeof(AT_MAC));
    if (mac_p == NULL) {
	EAPLOG(LOG_NOTICE, "eapsim: failed allocating AT_MAC, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    net_uint16_set(mac_p->ma_reserved, 0);
    EAPPacketSetLength(pkt,
		       offsetof(EAPSIMPacket, attrs) + TLVBufferUsed(tb_p));
    /* compute/set the MAC value */
    EAPSIMAKAKeyInfoSetMAC(&context->key_info, pkt, mac_p->ma_mac, 
			   sres, SIM_SRES_SIZE * count);

    /* as far as we're concerned, we're successful */
    context->state = kEAPSIMClientStateSuccess;
    context->key_info_valid = TRUE;
    save_persistent_state(context);

 done:
    return (pkt);
}

STATIC void
eapsim_compute_reauth_key(EAPSIMContextRef context,
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
eapsim_reauthentication(EAPSIMContextRef context,
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
	       "eapsim: Reauthentication but no key info available");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    reauth_id = EAPSIMAKAPersistentStateGetReauthID(context->persist);
    if (reauth_id == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: received Reauthentication but don't have reauth id");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    context->state = kEAPSIMClientStateReauthentication;
    context->plugin_state = kEAPClientStateAuthenticating;

    /* validate the MAC */
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: Reauthentication is missing AT_MAC");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    if (!EAPSIMAKAKeyInfoVerifyMAC(&context->key_info, in_pkt, mac_p->ma_mac,
				   NULL, 0)) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: Reauthentication AT_MAC not valid");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* packet must contain AT_ENCR_DATA, AT_IV */
    encr_data_p = (AT_ENCR_DATA *)TLVListLookupAttribute(tlvs_p, kAT_ENCR_DATA);
    iv_p = (AT_IV *)TLVListLookupAttribute(tlvs_p, kAT_IV);
    if (encr_data_p == NULL || iv_p == NULL) {
	if (encr_data_p == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim:  Reauthentication missing AT_ENCR_DATA");
	}
	if (iv_p == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim:  Reauthentication missing AT_IV");
	}
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }
    decrypted_buffer 
	= EAPSIMAKAKeyInfoDecryptTLVList(&context->key_info, encr_data_p, iv_p,
					 decrypted_tlvs_p);
    if (decrypted_buffer == NULL) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: failed to decrypt Reauthentication AT_ENCR_DATA");
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
		   "eapsim:  Reauthentication AT_ENCR_DATA missing AT_NONCE_S");
	}
	if (counter_p == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim:  Reauthentication AT_ENCR_DATA missing AT_COUNTER");
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
    pkt = eapsim_make_response(context, in_pkt,
			       kEAPSIMAKAPacketSubtypeReauthentication, tb_p);

    /* 
     * create nested attributes containing:
     * 	AT_COUNTER
     *  AT_COUNTER_TOO_SMALL (if necessary)
     */
    TLVBufferInit(encr_tb_p, encr_buffer, sizeof(encr_buffer));
    if (TLVBufferAddCounter(encr_tb_p, at_counter) == FALSE) {
	EAPLOG(LOG_NOTICE, "eapsim: failed allocating AT_COUNTER, %s",
	       TLVBufferErrorString(tb_p));
	*client_status = kEAPClientStatusInternalError;
	pkt = NULL;
	goto done;
    }
    if (force_fullauth
	&& TLVBufferAddCounterTooSmall(encr_tb_p) == FALSE) {
	EAPLOG(LOG_NOTICE,
	       "eapsim: failed allocating AT_COUNTER_TOO_SMALL, %s",
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
	EAPLOG(LOG_NOTICE, "eapsim: failed allocating AT_MAC, %s",
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
	context->state = kEAPSIMClientStateSuccess;
	eapsim_compute_reauth_key(context, counter_p, nonce_s_p);
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
eapsim_notification(EAPSIMContextRef context,
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
	EAPLOG(LOG_NOTICE, "eapsim: Notification does not contain "
	       "AT_NOTIFICATION attribute");
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    notification_code = net_uint16_get(notification_p->nt_notification);
    after_auth = ATNotificationPhaseIsAfterAuthentication(notification_code);
    if (ATNotificationCodeIsSuccess(notification_code) && after_auth == FALSE) {
	EAPLOG(LOG_NOTICE, 
	       "eapsim: Notification code '%d' indicates "
	       "success before authentication", notification_code);
	*client_status = kEAPClientStatusProtocolError;
	goto done;
    }

    /* validate the MAC */
    mac_p = (AT_MAC *)TLVListLookupAttribute(tlvs_p, kAT_MAC);
    if (mac_p == NULL) {
	if (after_auth) {
	    EAPLOG(LOG_NOTICE, "eapsim: Notification is missing AT_MAC");
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
    }
    else {
	if (after_auth == FALSE) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim: Notification incorrectly contains AT_MAC");
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}

	if (!EAPSIMAKAKeyInfoVerifyMAC(&context->key_info, in_pkt, 
				       mac_p->ma_mac, NULL, 0)) {
	    EAPLOG(LOG_NOTICE, "eapsim: Notification AT_MAC not valid");
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
		   "eapsim: Notification after re-auth missing "
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
		   "eapsim: failed to decrypt Notification AT_ENCR_DATA");
	    *client_status = kEAPClientStatusInternalError;
	    goto done;
	}
	if (!has_counter) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim:  Notification AT_ENCR_DATA missing AT_COUNTER");
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
	if (at_counter != current_at_counter) {
	    EAPLOG(LOG_NOTICE, "eapsim: Notification AT_COUNTER (%d) does not "
		   "match current counter (%d)", at_counter,
		   current_at_counter);
	    *client_status = kEAPClientStatusProtocolError;
	    goto done;
	}
    }

    /* create our response */
    pkt = eapsim_make_response(context, in_pkt,
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
	    EAPLOG(LOG_NOTICE, "eapsim: failed to allocate AT_COUNTER, %s",
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
	    EAPLOG(LOG_NOTICE, "eapsim: failed allocating AT_MAC, %s",
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
	context->state = kEAPSIMClientStateSuccess;
    }
    else {
	const char *	str;

	context->state = kEAPSIMClientStateFailure;
	*client_status = kEAPClientStatusPluginSpecificError;
	*error = EAPSIMAKAStatusForATNotificationCode(notification_code);
	str = ATNotificationCodeGetString(notification_code);
	if (str == NULL) {
	    EAPLOG(LOG_NOTICE,
		   "eapsim: Notification code '%d' unrecognized failure",
		   notification_code);
	}
	else {
	    EAPLOG(LOG_NOTICE, "eapsim: Notification: %s", str);
	}
    }

 done:
    return (pkt);
}

STATIC EAPPacketRef
eapsim_request(EAPSIMContextRef context,
	       const EAPPacketRef in_pkt,
	       EAPClientStatus * client_status,
	       EAPClientDomainSpecificError * error)
{
    EAPSIMPacketRef	eapsim_in = (EAPSIMPacketRef)in_pkt;
    EAPPacketRef	eapsim_out = NULL;
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
    if (TLVListParse(tlvs_p, eapsim_in->attrs,
		     in_length - kEAPSIMAKAPacketHeaderLength) == FALSE) {
	EAPLOG_FL(LOG_NOTICE, "parse failed: %s",
		  TLVListErrorString(tlvs_p));
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
    case kEAPSIMAKAPacketSubtypeSIMStart:
	eapsim_out = eapsim_start(context, in_pkt, tlvs_p, client_status);
	break;
    case kEAPSIMAKAPacketSubtypeSIMChallenge:
	eapsim_out = eapsim_challenge(context, in_pkt, tlvs_p, client_status);
	break;
    case kEAPSIMAKAPacketSubtypeNotification:
	eapsim_out = eapsim_notification(context, in_pkt, tlvs_p,
					 client_status, error);
	break;
    case kEAPSIMAKAPacketSubtypeReauthentication:
	eapsim_out 
	    = eapsim_reauthentication(context, in_pkt, tlvs_p, client_status);
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
	*out_pkt_p = eapsim_request(context, in_pkt, client_status, error);
	break;
    case kEAPCodeSuccess:
	context->previous_identifier = -1;
	if (context->state == kEAPSIMClientStateSuccess) {
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
eapsim_failure_string(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

STATIC void * 
eapsim_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;

    if (context->state == kEAPSIMClientStateSuccess 
	&& context->key_info_valid) {
	*key_length = 32;
	return (context->key_info.s.msk);
    }
    return (NULL);
}

STATIC void * 
eapsim_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;

    if (context->state == kEAPSIMClientStateSuccess
	&& context->key_info_valid) {
	*key_length = 32;
	return (context->key_info.s.msk + 32);
    }
    return (NULL);
}

STATIC int
eapsim_msk_copy_bytes(EAPClientPluginDataRef plugin, 
		      void * msk, int msk_size)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;
    int			ret_msk_size = sizeof(context->key_info.s.msk);

    if (msk_size < ret_msk_size
	|| context->key_info_valid == FALSE
	|| context->state != kEAPSIMClientStateSuccess) {
	ret_msk_size = 0;
    }
    else {
	bcopy(context->key_info.s.msk, msk, ret_msk_size);
    }
    return (ret_msk_size);
}

STATIC CFDictionaryRef
eapsim_publish_props(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

STATIC CFStringRef
eapsim_user_name_copy(CFDictionaryRef properties)
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
    persist = EAPSIMAKAPersistentStateCreate(kEAPTypeEAPSIM,
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
 * Function: eapsim_copy_identity
 * Purpose:
 *   Return the current identity we should use in responding to an
 *   EAP Request Identity packet.
 */
STATIC CFStringRef
eapsim_copy_identity(EAPClientPluginDataRef plugin)
{
    EAPSIMContextRef	context = (EAPSIMContextRef)plugin->private;

    EAPSIMContextSetLastIdentity(context, NULL);
    context->state = kEAPSIMClientStateNone;
    context->previous_identifier = -1;
    if (context->sim_static.rand != NULL) {
	return (copy_static_identity(plugin->properties));
    }
    return (sim_identity_create(context->persist, plugin->properties,
				kAT_ANY_ID_REQ, NULL));
}

STATIC CFStringRef
eapsim_copy_packet_description(const EAPPacketRef pkt, bool * packet_is_valid)
{ 
    return (EAPSIMAKAPacketCopyDescription(pkt, packet_is_valid));
}

STATIC EAPType 
eapsim_type(void)
{
    return (kEAPTypeEAPSIM);

}

STATIC const char *
eapsim_name(void)
{
    return (EAP_SIM_NAME);

}

STATIC EAPClientPluginVersion 
eapsim_version(void)
{
    return (kEAPClientPluginVersion);
}

STATIC struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
#if 0
    { kEAPClientPluginFuncNameIntrospect, eapsim_introspect },
#endif /* 0 */
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
    { kEAPClientPluginFuncNameMasterSessionKeyCopyBytes,
      eapsim_msk_copy_bytes },
    { kEAPClientPluginFuncNamePublishProperties, eapsim_publish_props },
    { kEAPClientPluginFuncNameUserName, eapsim_user_name_copy },
    { kEAPClientPluginFuncNameCopyIdentity, eapsim_copy_identity },
    { kEAPClientPluginFuncNameCopyPacketDescription,
      eapsim_copy_packet_description },
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
    CFStringRef	identity;

    identity = eapsim_user_name_copy(NULL);
    if (identity != NULL) {
	CFShow(identity);
	CFRelease(identity);
    }
    exit(0);
    return (0);
}
#endif /* TARGET_OS_EMBEDDED */
#endif /* TEST_SIM_INFO */
