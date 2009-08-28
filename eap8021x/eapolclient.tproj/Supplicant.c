
/*
 * Copyright (c) 2001-2009 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */
 
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <net/if.h>
#include <string.h>
#include <paths.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonHMAC.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPClientModule.h>
#include <EAP8021X/EAPClientProperties.h>
#include <EAP8021X/EAPOLControlTypes.h>
#include <EAP8021X/EAPOLControl.h>
#include <EAP8021X/SupplicantTypes.h>
#include <EAP8021X/EAPKeychainUtil.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFBundle.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#include <TargetConditionals.h>
#if ! TARGET_OS_EMBEDDED
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include "Dialogue.h"
#endif /* ! TARGET_OS_EMBEDDED */

#include "Supplicant.h"
#include "Timer.h"
#include "EAPOLSocket.h"
#include "printdata.h"
#include "mylog.h"
#include "myCFUtil.h"
#include "ClientControlInterface.h"

#define START_PERIOD_SECS		30
#define AUTH_PERIOD_SECS		30
#define HELD_PERIOD_SECS		60
#define LINK_ACTIVE_PERIOD_SECS		4
#define LINK_INACTIVE_PERIOD_SECS	1
#define MAX_START			3

#define BAD_IDENTIFIER		(-1)

static int	S_start_period_secs = START_PERIOD_SECS;
static int	S_auth_period_secs = AUTH_PERIOD_SECS;
static int	S_held_period_secs = HELD_PERIOD_SECS;
static int	S_link_active_period_secs = LINK_ACTIVE_PERIOD_SECS;
static int	S_link_inactive_period_secs = LINK_INACTIVE_PERIOD_SECS;
static int	S_max_start = MAX_START;	

struct eap_client {
    EAPClientModuleRef		module;
    EAPClientPluginData		plugin_data;
    CFArrayRef			required_props;
    CFDictionaryRef		published_props;
    EAPType			last_type;
    const char *		last_type_name;
};

typedef struct {
    int	*		types;
    int			count;
    int			index;
    bool		use_identity;
} EAPAcceptTypes, * EAPAcceptTypesRef;

struct Supplicant_s {
    SupplicantState		state;
    TimerRef			timer;
    EAPOLSocket *		sock;

    uint32_t			generation;

    CFDictionaryRef		orig_config_dict;
    CFDictionaryRef		config_dict;
    CFMutableDictionaryRef	ui_config_dict;
    CFStringRef			config_id;

    int				previous_identifier;
    char *			identity;
    int				identity_length;
    char *			username;
    int				username_length;
    char *			password;
    int				password_length;
    bool			one_time_password;
    bool			ignore_password;

    EAPAcceptTypes		eap_accept;

    CFArrayRef			identity_attributes;

#if ! TARGET_OS_EMBEDDED
    UserPasswordDialogueRef	pw_prompt;

    TrustDialogueRef		trust_prompt;
#endif /* ! TARGET_OS_EMBEDDED */

    int				start_count;

    bool			no_authenticator;

    struct eap_client		eap;

    EAPOLSocketReceiveData	last_rx_packet;
    EAPClientStatus		last_status;
    EAPClientDomainSpecificError last_error;

    bool			debug;

    bool			pmk_set;

    bool			no_ui;
};

typedef enum {
    kSupplicantEventStart,
    kSupplicantEventData,
    kSupplicantEventTimeout,
    kSupplicantEventUserResponse,
    kSupplicantEventMoreDataAvailable,
} SupplicantEvent;

static CFDictionaryRef
eapolclient_default_dict(void);

static bool
S_set_user_password(SupplicantRef supp);

static void
Supplicant_acquired(SupplicantRef supp, SupplicantEvent event, 
		    void * evdata);
static void
Supplicant_authenticated(SupplicantRef supp, SupplicantEvent event, 
			 void * evdata);
static void
Supplicant_authenticating(SupplicantRef supp, SupplicantEvent event, 
			  void * evdata);
static void
Supplicant_connecting(SupplicantRef supp, SupplicantEvent event, 
		      void * evdata);
static void
Supplicant_held(SupplicantRef supp, SupplicantEvent event, 
		void * evdata);

static void
Supplicant_logoff(SupplicantRef supp, SupplicantEvent event, void * evdata);

static void
Supplicant_inactive(SupplicantRef supp, SupplicantEvent event, void * evdata);

static void 
Supplicant_report_status(SupplicantRef supp);

static void
respond_to_notification(SupplicantRef supp, int identifier);

/**
 ** Logging
 **/
static void
eapolclient_log_config_dict(uint32_t flags, CFDictionaryRef d);

/**
 ** EAP client module access convenience routines
 **/

static void
eap_client_free_properties(SupplicantRef supp)
{
    my_CFRelease((CFDictionaryRef *)&supp->eap.plugin_data.properties);
}

#if TARGET_OS_EMBEDDED

static void
eap_client_set_properties(SupplicantRef supp)
{
    CFStringRef		config_domain;
    CFStringRef		config_ident;
    
    eap_client_free_properties(supp);

    config_domain 
	= CFDictionaryGetValue(supp->config_dict,
			       kEAPClientPropTLSTrustExceptionsDomain);
    config_ident
	= CFDictionaryGetValue(supp->config_dict,
			       kEAPClientPropTLSTrustExceptionsID);
    if (config_domain != NULL && config_ident != NULL) {
	/* configuration already specifies the trust domain/ID */
	*((CFDictionaryRef *)&supp->eap.plugin_data.properties) 
	    = CFRetain(supp->config_dict);
    }
    else {
	CFMutableDictionaryRef	dict;
	CFStringRef		domain;
	CFStringRef		ident;
	CFStringRef		if_name_cf = NULL;

	ident = NULL;
	if (EAPOLSocketIsWireless(supp->sock)) {
	    ident = EAPOLSocketGetSSID(supp->sock);
	}
	if (ident != NULL) {
	    domain = kEAPTLSTrustExceptionsDomainWirelessSSID;
	}
	else if (supp->config_id != NULL) {
	    domain = kEAPTLSTrustExceptionsDomainProfileID;
	    ident = supp->config_id;
	}
	else {
	    if_name_cf 
		= CFStringCreateWithCString(NULL, 
					    EAPOLSocketIfName(supp->sock, NULL),
					    kCFStringEncodingASCII);
	    domain = kEAPTLSTrustExceptionsDomainNetworkInterfaceName;
	    ident = if_name_cf;
	}
	dict = CFDictionaryCreateMutableCopy(NULL, 0, 
					     supp->config_dict);
	CFDictionarySetValue(dict,
			     kEAPClientPropTLSTrustExceptionsDomain,
			     domain);
	CFDictionarySetValue(dict,
			     kEAPClientPropTLSTrustExceptionsID,
			     ident);
	*((CFDictionaryRef *)&supp->eap.plugin_data.properties) = dict;
	my_CFRelease(&if_name_cf);
    }
    return;
}
#else /* TARGET_OS_EMBEDDED */

static void
eap_client_set_properties(SupplicantRef supp)
{
    eap_client_free_properties(supp);
    *((CFDictionaryRef *)&supp->eap.plugin_data.properties) 
	= CFRetain(supp->config_dict);
    return;
}
#endif /* TARGET_OS_EMBEDDED */

static void
eap_client_free(SupplicantRef supp)
{
    if (supp->eap.module != NULL) {
	EAPClientModulePluginFree(supp->eap.module, &supp->eap.plugin_data);
	supp->eap.module = NULL;
	eap_client_free_properties(supp);
	bzero(&supp->eap.plugin_data, sizeof(supp->eap.plugin_data));
    }
    my_CFRelease(&supp->eap.required_props);
    my_CFRelease(&supp->eap.published_props);
    supp->eap.last_type = kEAPTypeInvalid;
    supp->eap.last_type_name = NULL;
    return;
}

static EAPType
eap_client_type(SupplicantRef supp)
{
    if (supp->eap.module == NULL) {
	return (kEAPTypeInvalid);
    }
    return (EAPClientModulePluginEAPType(supp->eap.module));
}

static __inline__ void
S_set_uint32(const uint32_t * v_p, uint32_t value)
{
    *((uint32_t *)v_p) = value;
    return;
}

static bool
eap_client_init(SupplicantRef supp, EAPType type)
{
    EAPClientModule *		module;

    supp->eap.last_type = kEAPTypeInvalid;
    supp->eap.last_type_name = NULL;

    if (supp->eap.module != NULL) {
	my_log(LOG_NOTICE, "eap_client_init: already initialized");
	return (TRUE);
    }
    module = EAPClientModuleLookup(type);
    if (module == NULL) {
	return (FALSE);
    }
    my_CFRelease(&supp->eap.required_props);
    my_CFRelease(&supp->eap.published_props);
    bzero(&supp->eap.plugin_data, sizeof(supp->eap.plugin_data));
    supp->eap.plugin_data.unique_id 
	= EAPOLSocketIfName(supp->sock, (uint32_t *)
			    &supp->eap.plugin_data.unique_id_length);
    S_set_uint32(&supp->eap.plugin_data.mtu,
		 EAPOLSocketMTU(supp->sock) - sizeof(EAPOLPacket));

    supp->eap.plugin_data.username = (uint8_t *)supp->username;
    S_set_uint32(&supp->eap.plugin_data.username_length, 
		 supp->username_length);
    supp->eap.plugin_data.password = (uint8_t *)supp->password;
    S_set_uint32(&supp->eap.plugin_data.password_length, 
		 supp->password_length);
    eap_client_set_properties(supp);
    *((bool *)&supp->eap.plugin_data.log_enabled) = supp->debug;
    *((bool *)&supp->eap.plugin_data.system_mode) 
	= (EAPOLSocketGetMode(supp->sock) == kEAPOLControlModeSystem);
    supp->last_status = 
	EAPClientModulePluginInit(module, &supp->eap.plugin_data,
				  &supp->eap.required_props, 
				  &supp->last_error);
    supp->eap.last_type_name = EAPClientModulePluginEAPName(module);
    supp->eap.last_type = type;
    if (supp->last_status != kEAPClientStatusOK) {
	return (FALSE);
    }
    supp->eap.module = module;
    return (TRUE);
}

static CFArrayRef
eap_client_require_properties(SupplicantRef supp)
{
    return (EAPClientModulePluginRequireProperties(supp->eap.module,
						   &supp->eap.plugin_data));
}

static CFDictionaryRef
eap_client_publish_properties(SupplicantRef supp)
{
    return (EAPClientModulePluginPublishProperties(supp->eap.module,
						   &supp->eap.plugin_data));
}

static EAPClientState
eap_client_process(SupplicantRef supp, EAPPacketRef in_pkt_p,
		   EAPPacketRef * out_pkt_p, EAPClientStatus * status,
		   EAPClientDomainSpecificError * error)
{
    EAPClientState cstate;

    supp->eap.plugin_data.username = (uint8_t *)supp->username;
    S_set_uint32(&supp->eap.plugin_data.username_length, 
		 supp->username_length);
    supp->eap.plugin_data.password = (uint8_t *)supp->password;
    S_set_uint32(&supp->eap.plugin_data.password_length, 
		 supp->password_length);
    S_set_uint32(&supp->eap.plugin_data.generation, 
		 supp->generation);
    eap_client_set_properties(supp);
    *((bool *)&supp->eap.plugin_data.log_enabled) = supp->debug;
    cstate = EAPClientModulePluginProcess(supp->eap.module,
					  &supp->eap.plugin_data,
					  in_pkt_p, out_pkt_p,
					  status, error);
    return (cstate);
}

static void
eap_client_free_packet(SupplicantRef supp, EAPPacketRef out_pkt_p)
{
    EAPClientModulePluginFreePacket(supp->eap.module, 
				    &supp->eap.plugin_data,
				    out_pkt_p);
}

static void
eap_client_log_failure(SupplicantRef supp)
{
    const char * err;
    err = EAPClientModulePluginFailureString(supp->eap.module,
					     &supp->eap.plugin_data);
    if (err) {
	my_log(LOG_NOTICE, "error string '%s'", err);
    }
    return;
}

static void *
eap_client_session_key(SupplicantRef supp, int * key_length)
{
    return (EAPClientModulePluginSessionKey(supp->eap.module,
					    &supp->eap.plugin_data,
					    key_length));
}

static void *
eap_client_server_key(SupplicantRef supp, int * key_length)
{
    return (EAPClientModulePluginServerKey(supp->eap.module,
					   &supp->eap.plugin_data,
					   key_length));
}

/**
 ** EAPAcceptTypes routines
 **/

static void
EAPAcceptTypesCopy(EAPAcceptTypesRef dest, EAPAcceptTypesRef src)
{
    *dest = *src;
    dest->types = (int *)malloc(src->count * sizeof(*dest->types));
    bcopy(src->types, dest->types, src->count * sizeof(*dest->types));
    return;
}

static void
EAPAcceptTypesFree(EAPAcceptTypesRef accept)
{
    if (accept->types != NULL) {
	free(accept->types);
    }
    bzero(accept, sizeof(*accept));
    return;
}

static void
EAPAcceptTypesInit(EAPAcceptTypesRef accept, CFDictionaryRef config_dict)
{
    CFArrayRef		accept_types = NULL;
    int			i;
    int			count;
    int			tunneled_count;
    CFNumberRef		type_cf;
    int			type_i;
    int *		types;
    int			types_count;

    EAPAcceptTypesFree(accept);
    
    if (config_dict != NULL) {
	accept_types = CFDictionaryGetValue(config_dict,
					    kEAPClientPropAcceptEAPTypes);
    }
    if (accept_types == NULL) {
	return;
    }
    count = CFArrayGetCount(accept_types);
    if (count == 0) {
	return;
    }
    types = (int *)malloc(count * sizeof(*types));
    tunneled_count = types_count = 0;
    for (i = 0; i < count; i++) {
	type_cf = CFArrayGetValueAtIndex(accept_types, i);
	if (isA_CFNumber(type_cf) == NULL) {
	    my_log(LOG_NOTICE, 
		   "AcceptEAPTypes[%d] contains invalid type, ignoring", i);
	    continue;
	}
	if (CFNumberGetValue(type_cf, kCFNumberIntType, &type_i) == FALSE) {
	    my_log(LOG_NOTICE, 
		   "AcceptEAPTypes[%d] contains invalid number, ignoring", i);
	    continue;
	}
	if (EAPClientModuleLookup(type_i) == NULL) {
	    my_log(LOG_NOTICE, 
		   "AcceptEAPTypes[%d] specifies unsupported type %d, ignoring",
		   i, type_i);
	    continue;
	}
	types[types_count++] = type_i;
	if (type_i == kEAPTypePEAP
	    || type_i == kEAPTypeTTLS
	    || type_i == kEAPTypeEAPFAST) {
	    tunneled_count++;
	}
    }
    if (types_count == 0) {
	free(types);
    }
    else {
	accept->types = types;
	accept->count = types_count;
	if (tunneled_count == types_count) {
	    accept->use_identity = TRUE;
	}
    }
    return;
}

static EAPType
EAPAcceptTypesNextType(EAPAcceptTypesRef accept)
{
    if (accept->types == NULL
	|| accept->index >= accept->count) {
	return (kEAPTypeInvalid);
    }
    return (accept->types[accept->index++]);
}

static void
EAPAcceptTypesReset(EAPAcceptTypesRef accept)
{
    accept->index = 0;
    return;
}

static bool
EAPAcceptTypesIsSupportedType(EAPAcceptTypesRef accept, EAPType type)
{
    int			i;

    if (accept->types == NULL) {
	return (FALSE);
    }
    for (i = 0; i < accept->count; i++) {
	if (accept->types[i] == type) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static bool
EAPAcceptTypesUseIdentity(EAPAcceptTypesRef accept)
{
    return (accept->use_identity);
}

/**
 ** Supplicant routines
 **/

static void
clear_password(SupplicantRef supp)
{
    supp->ignore_password = TRUE;

    /* clear the password */
    free(supp->password);
    supp->password = NULL;
    supp->password_length = 0;
    return;
}

static void
free_last_packet(SupplicantRef supp)
{
    if (supp->last_rx_packet.eapol_p != NULL) {
	free(supp->last_rx_packet.eapol_p);
	bzero(&supp->last_rx_packet, sizeof(supp->last_rx_packet));
    }
    return;
}

static void
save_last_packet(SupplicantRef supp, EAPOLSocketReceiveDataRef rx_p)
{
    EAPOLPacketRef	last_eapol_p;

    last_eapol_p = supp->last_rx_packet.eapol_p;
    if (last_eapol_p == rx_p->eapol_p) {
	/* don't bother re-saving the same buffer */
	return;
    }
    bzero(&supp->last_rx_packet, sizeof(supp->last_rx_packet));
    supp->last_rx_packet.eapol_p = (EAPOLPacketRef)malloc(rx_p->length);
    supp->last_rx_packet.length = rx_p->length;
    bcopy(rx_p->eapol_p, supp->last_rx_packet.eapol_p, rx_p->length);
    if (last_eapol_p != NULL) {
	free(last_eapol_p);
    }
    return;
}

static void
Supplicant_cancel_pending_events(SupplicantRef supp)
{
    EAPOLSocketDisableReceive(supp->sock);
    Timer_cancel(supp->timer);
    return;
}

static void
S_update_identity_attributes(SupplicantRef supp, void * data, int length)
{
    int		props_length;
    void * 	props_start;
    CFStringRef	props_string;

    my_CFRelease(&supp->identity_attributes);

    props_start = memchr(data, '\0', length);
    if (props_start == NULL) {
	return;
    }
    props_start++;	/* skip '\0' */
    props_length = length - (props_start - data);
    if (length <= 0) {
	/* no props there */
	return;
    }
    props_string = CFStringCreateWithBytes(NULL, props_start, props_length,
					   kCFStringEncodingASCII, FALSE);
    if (props_string != NULL) {
	supp->identity_attributes =
	    CFStringCreateArrayBySeparatingStrings(NULL, props_string, 
						   CFSTR(","));
	my_CFRelease(&props_string);
    }
    return;
}

/* 
 * Function: eapol_key_verify_signature
 *
 * Purpose:
 *   Verify that the signature in the key packet is valid.
 * Notes:
 *   As documented in IEEE802.1X, section 7.6.8,
 *   compute the HMAC-MD5 hash over the entire EAPOL key packet
 *   with a zero signature using the server key as the key.
 *   The resulting signature is compared with the signature in
 *   the packet.  If they match, the packet is valid, if not
 *   it is invalid.
 * Returns:
 *   TRUE if the signature is valid, FALSE otherwise.
 */
static bool
eapol_key_verify_signature(EAPOLPacketRef packet, int packet_length,
			   char * server_key, int server_key_length,
			   bool debug)
{
    EAPOLKeyDescriptorRef	descr_p;
    EAPOLPacketRef		packet_copy;
    u_char			signature[16];
    bool			valid = FALSE;

    /* make a copy of the entire packet */
    packet_copy = (EAPOLPacketRef)malloc(packet_length);
    bcopy(packet, packet_copy, packet_length);
    descr_p = (EAPOLKeyDescriptorRef)packet_copy->body;
    bzero(descr_p->key_signature, sizeof(descr_p->key_signature));
    CCHmac(kCCHmacAlgMD5,
	   server_key, server_key_length,
	   packet_copy, packet_length,
	   signature);
    descr_p = (EAPOLKeyDescriptorRef)packet->body;
    valid = (bcmp(descr_p->key_signature, signature, sizeof(signature)) == 0);
    if (debug) {
	printf("Signature: ");
	print_bytes(signature, sizeof(signature));
	printf(" is %s\n", valid ? "valid" : "INVALID");
	fflush(stdout);
    }
    free(packet_copy);
    return (valid);
}

static void
process_key(SupplicantRef supp, EAPOLPacketRef eapol_p)
{
    int				body_length;
    EAPOLKeyDescriptorRef	descr_p;
    int				key_length;
    int				key_data_length;
    int				packet_length;
    char *			server_key;
    int				server_key_length = 0;
    uint8_t *			session_key;
    int				session_key_length = 0;
    wirelessKeyType		type;

    descr_p = (EAPOLKeyDescriptorRef)eapol_p->body;
    session_key = eap_client_session_key(supp, &session_key_length);
    if (session_key == NULL) {
	my_log(LOG_NOTICE, "Supplicant process_key: session key is NULL");
	eapolclient_log(kLogFlagBasic,
			"key process: NULL session key\n");
	return;
    }
    server_key = eap_client_server_key(supp, &server_key_length);
    if (server_key == NULL) {
	my_log(LOG_NOTICE, "Supplicant process_key: server key is NULL");
	eapolclient_log(kLogFlagBasic,
			"key process: NULL server key\n");
	return;
    }
    body_length = EAPOLPacketGetLength(eapol_p);
    packet_length = sizeof(EAPOLPacket) + body_length;

    if (eapol_key_verify_signature(eapol_p, packet_length,
				   server_key, server_key_length,
				   supp->debug) == FALSE) {
	my_log(LOG_NOTICE,
	       "Supplicant process key: key signature mismatch, ignoring");
	eapolclient_log(kLogFlagBasic,
			"key process: signature mismatch\n");
	return;
    }
    if (descr_p->key_index & kEAPOLKeyDescriptorIndexUnicastFlag) {
	type = kKeyTypeIndexedTx;
    }
    else {
	type = kKeyTypeMulticast;
    }
    key_length = EAPOLKeyDescriptorGetLength(descr_p);
    key_data_length = body_length - sizeof(*descr_p);
    if (key_data_length > 0) {
	size_t 			bytes_processed;
	CCCryptorStatus		c_status;
	uint8_t *		enc_key;
	uint8_t *		rc4_key;
	int			rc4_key_length;
	
	/* decrypt the key from the packet */
	rc4_key_length = sizeof(descr_p->key_IV) + session_key_length;
	rc4_key = malloc(rc4_key_length);
	bcopy(descr_p->key_IV, rc4_key, sizeof(descr_p->key_IV));
	bcopy(session_key, rc4_key + sizeof(descr_p->key_IV),
	      session_key_length);
	enc_key = malloc(key_data_length);
	c_status = CCCrypt(kCCDecrypt, kCCAlgorithmRC4, 0,
			   rc4_key, rc4_key_length,
			   NULL,
			   descr_p->key, key_data_length,
			   enc_key, key_data_length,
			   &bytes_processed);
	if (c_status != kCCSuccess) {
	    eapolclient_log(kLogFlagBasic,
			    "key process: RC4 decrypt failed %d\n",
			    c_status);
	}
	else {
	    eapolclient_log(kLogFlagBasic,
			    "set %s key length %d using descriptor\n",
			    (type == kKeyTypeIndexedTx) 
			    ? "Unicast" : "Broadcast",
			    key_length);
	    EAPOLSocketSetKey(supp->sock, type, 
			      (descr_p->key_index 
			       & kEAPOLKeyDescriptorIndexMask),
			      enc_key, key_length);
	}
	free(enc_key);
	free(rc4_key);
    }
    else {
	eapolclient_log(kLogFlagBasic,
			"set %s key length %d using session key\n",
			(type == kKeyTypeIndexedTx) 
			? "Unicast" : "Broadcast",
			key_length);
	EAPOLSocketSetKey(supp->sock, type, 
			  descr_p->key_index & kEAPOLKeyDescriptorIndexMask,
			  session_key, key_length);
    }
    return;
}

static void
clear_wpa_key_info(SupplicantRef supp)
{
    (void)EAPOLSocketSetPMK(supp->sock, NULL, 0);
    supp->pmk_set = FALSE;
    return;
}

static void
set_wpa_key_info(SupplicantRef supp)
{
    uint8_t *	session_key;
    int		session_key_length;

    if (supp->pmk_set) {
	/* already set */
	return;
    }
    session_key = eap_client_session_key(supp, &session_key_length);
    if (session_key != NULL
	&& EAPOLSocketSetPMK(supp->sock, session_key,
			     session_key_length)) {
	supp->pmk_set = TRUE;
    }
    return;
}

static void
Supplicant_authenticated(SupplicantRef supp, SupplicantEvent event, 
			 void * evdata)
{
    EAPRequestPacket * 		req_p;
    EAPOLSocketReceiveDataRef	rx = evdata;

    switch (event) {
    case kSupplicantEventStart:
	Supplicant_cancel_pending_events(supp);
	supp->state = kSupplicantStateAuthenticated;
	free_last_packet(supp);
#if ! TARGET_OS_EMBEDDED
	UserPasswordDialogue_free(&supp->pw_prompt);
#endif /* ! TARGET_OS_EMBEDDED */
	if (supp->one_time_password) {
	    clear_password(supp);
	}
	Supplicant_report_status(supp);
	EAPOLSocketEnableReceive(supp->sock, 
				 (void *)Supplicant_authenticated,
				 (void *)supp, 
				 (void *)kSupplicantEventData);
	break;
    case kSupplicantEventData:
	Timer_cancel(supp->timer);
	switch (rx->eapol_p->packet_type) {
	case kEAPOLPacketTypeEAPPacket:
	    req_p = (EAPRequestPacket *)rx->eapol_p->body;
	    switch (req_p->code) {
	    case kEAPCodeRequest:
		switch (req_p->type) {
		case kEAPTypeIdentity:
		    Supplicant_acquired(supp, kSupplicantEventStart, evdata);
		    break;
		case kEAPTypeNotification:
		    /* need to display information to the user XXX */
		    my_log(LOG_NOTICE, "Authenticated: Notification '%.*s'",
			   EAPPacketGetLength((EAPPacketRef)req_p) - sizeof(*req_p),
			   req_p->type_data);
		    respond_to_notification(supp, req_p->identifier);
		    break;
		default:
		    Supplicant_authenticating(supp,
					      kSupplicantEventStart,
					      evdata);
		    break;
		}
	    }
	    break;
	case kEAPOLPacketTypeKey:
	    if (EAPOLSocketIsWireless(supp->sock)) {
		EAPOLKeyDescriptorRef	descr_p;

		descr_p = (EAPOLKeyDescriptorRef)(rx->eapol_p->body);
		switch (descr_p->descriptor_type) {
		case kEAPOLKeyDescriptorTypeRC4:
		    process_key(supp, rx->eapol_p);
		    break;
		case kEAPOLKeyDescriptorTypeIEEE80211:
		    /* wireless family takes care of these */
		    break;
		default:
		    break;
		}
	    }
	    break;
	default:
	    break;
	}
	break;
    default:
	break;
    }
    return;
}

static void
Supplicant_cleanup(SupplicantRef supp)
{
    supp->previous_identifier = BAD_IDENTIFIER;
    EAPAcceptTypesReset(&supp->eap_accept);
    supp->last_status = kEAPClientStatusOK;
    supp->last_error = 0;
    eap_client_free(supp);
    free_last_packet(supp);
    return;
}

static void
Supplicant_disconnected(SupplicantRef supp, SupplicantEvent event, 
			void * evdata)
{
    switch (event) {
    case kSupplicantEventStart:
	Supplicant_cancel_pending_events(supp);
	supp->state = kSupplicantStateDisconnected;
	Supplicant_report_status(supp);
	Supplicant_cleanup(supp);
	Supplicant_connecting(supp, kSupplicantEventStart, NULL);
	break;
    default:
	break;
    }
    return;
}

static void
Supplicant_no_authenticator(SupplicantRef supp, SupplicantEvent event, 
			    void * evdata)
{
    switch (event) {
    case kSupplicantEventStart:
	Supplicant_cancel_pending_events(supp);
	supp->state = kSupplicantStateNoAuthenticator;
	supp->no_authenticator = TRUE;
	Supplicant_report_status(supp);
	/* let Connecting state handle any packets that may arrive */
	EAPOLSocketEnableReceive(supp->sock, 
				 (void *)Supplicant_connecting,
				 (void *)supp, 
				 (void *)kSupplicantEventStart);
	break;
    default:
	break;
    }
    return;
}

static void
Supplicant_connecting(SupplicantRef supp, SupplicantEvent event, 
		      void * evdata)
{
    EAPOLSocketReceiveDataRef 	rx = evdata;
    struct timeval 		t = {S_start_period_secs, 0};

    switch (event) {
    case kSupplicantEventStart:
	Supplicant_cancel_pending_events(supp);
	supp->state = kSupplicantStateConnecting;
	Supplicant_report_status(supp);
	supp->start_count = 0;
	EAPOLSocketEnableReceive(supp->sock, 
				 (void *)Supplicant_connecting,
				 (void *)supp, 
				 (void *)kSupplicantEventData);
	/* FALL THROUGH */
    case kSupplicantEventData:
	if (rx != NULL) {
	    EAPOLIEEE80211KeyDescriptorRef	ieee80211_descr_p;
	    EAPRequestPacketRef			req_p;

	    switch (rx->eapol_p->packet_type) {
	    case kEAPOLPacketTypeEAPPacket:
		req_p = (void *)rx->eapol_p->body;
		if (req_p->code == kEAPCodeRequest) {
		    if (req_p->type == kEAPTypeIdentity) {
			Supplicant_acquired(supp,
					    kSupplicantEventStart,
					    evdata);
		    }
		    else {
			Supplicant_authenticating(supp,
						  kSupplicantEventStart,
						  evdata);
		    }
		}
		return;
 	    case kEAPOLPacketTypeKey:
		ieee80211_descr_p = (void *)rx->eapol_p->body;
		if (ieee80211_descr_p->descriptor_type
		    == kEAPOLKeyDescriptorTypeIEEE80211) {
		    break;
		}
		return;
	    default:
		return;
	    }
	}
	/* FALL THROUGH */
    case kSupplicantEventTimeout:
	if (rx == NULL) {
	    if (supp->start_count == S_max_start) {
		/* no response from Authenticator */
		Supplicant_no_authenticator(supp, kSupplicantEventStart, NULL);
		break;
	    }
	    supp->start_count++;
	}
	EAPOLSocketTransmit(supp->sock,
			    kEAPOLPacketTypeStart,
			    NULL, 0);
	Timer_set_relative(supp->timer, t,
			   (void *)Supplicant_connecting,
			   (void *)supp, 
			   (void *)kSupplicantEventTimeout,
			   NULL);
	break;

    default:
	break;
    }
}

static bool
S_retrieve_identity(SupplicantRef supp)
{
    CFStringRef			identity_cf;
    char *			identity;
    
    /* no module is active yet, use what we have */
    if (supp->eap.module == NULL) {
	goto use_default;
    }
    /* if we have an active module, ask it for the identity to use */
    identity_cf	= EAPClientModulePluginCopyIdentity(supp->eap.module,
						    &supp->eap.plugin_data);
    if (identity_cf == NULL) {
	goto use_default;
    }
    identity = my_CFStringToCString(identity_cf, kCFStringEncodingUTF8);
    my_CFRelease(&identity_cf);
    if (identity == NULL) {
	goto use_default;
    }
    if (supp->username != NULL) {
	free(supp->username);
    }
    supp->username = identity;
    supp->username_length = strlen(identity);

 use_default:
    return (supp->username != NULL);
}

static bool
respond_with_identity(SupplicantRef supp, int identifier)
{
    char			buf[256];
    char *			identity;
    int				length;
    EAPPacketRef		pkt_p;
    int				size;

    if (S_retrieve_identity(supp) == FALSE) {
	return (FALSE);
    }
    if (supp->identity != NULL) {
	identity = supp->identity;
	length = supp->identity_length;
    }
    else {
	identity = supp->username;
	length = supp->username_length;
    }

    eapolclient_log(kLogFlagBasic,
		    "EAP Response Identity %.*s\n",
		    length, identity);

    /* transmit a response/Identity */
    pkt_p = EAPPacketCreate(buf, sizeof(buf), 
			    kEAPCodeResponse, identifier,
			    kEAPTypeIdentity, 
			    identity, length,
			    &size);
    if (EAPOLSocketTransmit(supp->sock,
			    kEAPOLPacketTypeEAPPacket,
			    pkt_p, size) < 0) {
	my_log(LOG_NOTICE, 
	       "EAPOL_transmit Identity failed");
    }
    if ((char *)pkt_p != buf) {
	free(pkt_p);
    }
    return (TRUE);
}

static void
Supplicant_acquired(SupplicantRef supp, SupplicantEvent event, 
		    void * evdata)
{
    SupplicantState		prev_state = supp->state;
    EAPOLSocketReceiveDataRef 	rx;
    EAPRequestPacket *		req_p;
    struct timeval 		t = {S_auth_period_secs, 0};
    
    switch (event) { 
    case kSupplicantEventStart:
	EAPAcceptTypesReset(&supp->eap_accept);
	Supplicant_cancel_pending_events(supp);
	supp->state = kSupplicantStateAcquired;
	EAPOLSocketEnableReceive(supp->sock, 
				 (void *)Supplicant_acquired,
				 (void *)supp,
				 (void *)kSupplicantEventData);
	/* FALL THROUGH */
    case kSupplicantEventData:
	supp->no_authenticator = FALSE;
	rx = evdata;
	if (rx->eapol_p->packet_type != kEAPOLPacketTypeEAPPacket) {
	    break;
	}
	req_p = (EAPRequestPacket *)rx->eapol_p->body;
	if (req_p->code == kEAPCodeRequest
	    && req_p->type == kEAPTypeIdentity) {
	    int				len;

	    len = EAPPacketGetLength((EAPPacketRef)req_p) - sizeof(*req_p);
	    S_update_identity_attributes(supp, req_p->type_data, len);
	    eapolclient_log(kLogFlagBasic,
			    "EAP Request Identity\n");
	    supp->previous_identifier = req_p->identifier;

	    if (respond_with_identity(supp, req_p->identifier)) {
		supp->last_status = kEAPClientStatusOK;
		Supplicant_report_status(supp);
		
		/* set a timeout */
		Timer_set_relative(supp->timer, t,
				   (void *)Supplicant_acquired,
				   (void *)supp, 
				   (void *)kSupplicantEventTimeout,
				   NULL);
	    }
	    else if (supp->no_ui) {
		eapolclient_log(kLogFlagBasic,
				"Acquired: cannot prompt for "
				"missing user name\n");
		my_log(LOG_NOTICE, 
		       "No user name provided and user interaction not allowed,"
		       " authentication held.");
		supp->last_status = kEAPClientStatusUserInputNotPossible;
		Supplicant_held(supp, kSupplicantEventStart, NULL);
	    }
	    else {
		supp->last_status = kEAPClientStatusUserInputRequired;
		Supplicant_report_status(supp);
	    }
	    break;
	}
	else {
	    if (event == kSupplicantEventStart) {
		/* this will not happen if we're bug free */
		my_log(LOG_NOTICE, "internal error: "
		       "Supplicant_acquired: "
		       "recursion avoided from state %s",
		       SupplicantStateString(prev_state));
		break;
	    }
	    Supplicant_authenticating(supp,
				      kSupplicantEventStart,
				      evdata);
	}
	break;
    case kSupplicantEventMoreDataAvailable:
	if (respond_with_identity(supp, supp->previous_identifier)) {
	    supp->last_status = kEAPClientStatusOK;
	    Supplicant_report_status(supp);
	    /* set a timeout */
	    Timer_set_relative(supp->timer, t,
			       (void *)Supplicant_acquired,
			       (void *)supp, 
			       (void *)kSupplicantEventTimeout,
			       NULL);
	    
	}
	else {
	    supp->last_status = kEAPClientStatusUserInputRequired;
	    Supplicant_report_status(supp);
	}
	break;

    case kSupplicantEventTimeout:
	Supplicant_connecting(supp, kSupplicantEventStart, NULL);
	break;
    default:
	break;
    }
    return;
}

static void
respond_to_notification(SupplicantRef supp, int identifier)
{
    EAPNotificationPacket	notif;
    int				size;

    eapolclient_log(kLogFlagBasic,
		    "EAP Response Notification\n");

    /* transmit a response/Notification */
    (void)EAPPacketCreate(&notif, sizeof(notif), 
			  kEAPCodeResponse, identifier,
			  kEAPTypeNotification, NULL, 0, &size);
    if (EAPOLSocketTransmit(supp->sock,
			    kEAPOLPacketTypeEAPPacket,
			    &notif, sizeof(notif)) < 0) {
	my_log(LOG_NOTICE, 
	       "EAPOL_transmit Notification failed");
    }
    return;
}

static void
respond_with_nak(SupplicantRef supp, int identifier, uint8_t desired_type)
{
    EAPNakPacket		nak;
    int				size;

    /* transmit a response/Nak */
    (void)EAPPacketCreate(&nak, sizeof(nak),
			  kEAPCodeResponse, identifier,
			  kEAPTypeNak, NULL, 
			  sizeof(nak) - sizeof(EAPRequestPacket), &size);
    nak.desired_type = desired_type;
    if (EAPOLSocketTransmit(supp->sock,
			    kEAPOLPacketTypeEAPPacket,
			    &nak, sizeof(nak)) < 0) {
	my_log(LOG_NOTICE, "EAPOL_transmit Nak failed");
    }
    return;
}

static void
process_packet(SupplicantRef supp, EAPOLSocketReceiveDataRef rx)
{
    EAPPacketRef	in_pkt_p = (EAPPacketRef)(rx->eapol_p->body);
    EAPPacketRef	out_pkt_p = NULL;
    EAPRequestPacket *	req_p = (EAPRequestPacket *)in_pkt_p;
    EAPClientState	state;
    struct timeval 	t = {S_auth_period_secs, 0};

    if (supp->username == NULL) {
	return;
    }

    switch (in_pkt_p->code) {
    case kEAPCodeRequest:
	if (req_p->type == kEAPTypeInvalid) {
	    return;
	}
	if (req_p->type != eap_client_type(supp)) {
	    if (EAPAcceptTypesIsSupportedType(&supp->eap_accept, 
					      req_p->type) == FALSE) {
		EAPType eap_type = EAPAcceptTypesNextType(&supp->eap_accept);
		if (eap_type == kEAPTypeInvalid) {
		    eapolclient_log(kLogFlagBasic,
				    "EAP Request: EAP type %d not enabled\n",
				    req_p->type);
		    supp->last_status = kEAPClientStatusProtocolNotSupported;
		    Supplicant_held(supp, kSupplicantEventStart, NULL);
		    return;
		}
		eapolclient_log(kLogFlagBasic,
				"EAP Request: NAK'ing EAP type %d with %d\n",
				req_p->type, eap_type);
		respond_with_nak(supp, in_pkt_p->identifier,
				 eap_type);
		/* set a timeout */
		Timer_set_relative(supp->timer, t,
				   (void *)Supplicant_authenticating,
				   (void *)supp, 
				   (void *)kSupplicantEventTimeout,
				   NULL);
		return;
	    }
	    Timer_cancel(supp->timer);
	    eap_client_free(supp);
	    if (eap_client_init(supp, req_p->type) == FALSE) {
		if (supp->last_status 
		    != kEAPClientStatusUserInputRequired) {
		    eapolclient_log(kLogFlagBasic,
				    "EAP Request: EAP type %d"
				    " init failed, %d\n",
				    req_p->type, supp->last_status);

		    my_log(LOG_NOTICE,
			   "eap_client_init type %d failed, %d",
			   req_p->type,
			   supp->last_status);
		    Supplicant_held(supp, kSupplicantEventStart, NULL);
		    return;
		}
		save_last_packet(supp, rx);
		Supplicant_report_status(supp);
		return;
	    }
	    eapolclient_log(kLogFlagBasic,
			    "EAP Request: EAP type %d accepted\n",
			    req_p->type);
	    Supplicant_report_status(supp);
	}
	else {
	    eapolclient_log(kLogFlagBasic,
			    "EAP Request: EAP type %d\n",
			    req_p->type);
	}
	break;
    case kEAPCodeResponse:
	if (req_p->type != eap_client_type(supp)) {
	    /* this should not happen, but if it does, ignore the packet */
	    return;
	}
	eapolclient_log(kLogFlagBasic,
			"EAP Response: EAP type %d\n", req_p->type);
	break;
    case kEAPCodeFailure:
	eapolclient_log(kLogFlagBasic, "EAP Failure\n");
	if (supp->eap.module == NULL) {
	    supp->last_status = kEAPClientStatusFailed;
	    Supplicant_held(supp, kSupplicantEventStart, NULL);
	    return;
	}
	break;
    case kEAPCodeSuccess:
	eapolclient_log(kLogFlagBasic, "EAP Success\n");
	if (supp->eap.module == NULL) {
	    Supplicant_authenticated(supp, kSupplicantEventStart, NULL);
	    return;
	}
	break;
    default:
	break;
    }
    if (supp->eap.module == NULL) {
	return;
    }
    /* invoke the authentication method "process" function */
    my_CFRelease(&supp->eap.required_props);
    my_CFRelease(&supp->eap.published_props);
    state = eap_client_process(supp, in_pkt_p, &out_pkt_p,
			       &supp->last_status, &supp->last_error);
    if (out_pkt_p != NULL) {
	/* send the output packet */
	if (EAPOLSocketTransmit(supp->sock,
				kEAPOLPacketTypeEAPPacket,
				out_pkt_p,
				EAPPacketGetLength(out_pkt_p)) < 0) {
	    my_log(LOG_NOTICE, "process_packet: EAPOL_transmit %d failed",
		   out_pkt_p->code);
	}
	/* and free the packet */
	eap_client_free_packet(supp, out_pkt_p);
    }

    supp->eap.published_props = eap_client_publish_properties(supp);

    switch (state) {
    case kEAPClientStateAuthenticating:
	if (supp->last_status == kEAPClientStatusUserInputRequired) {
	    save_last_packet(supp, rx);
	    supp->eap.required_props = eap_client_require_properties(supp);
	    if (supp->no_ui) {
		CFTypeRef	props = supp->eap.required_props;

		if (props == NULL) {
		    props = CFSTR("<NULL>");
		}
		SCLog(TRUE, LOG_NOTICE, 
		      CFSTR("Cannot prompt for missing properties,"
			    " authentication held\nMissing properties %@"),
		      props);
		eapolclient_log(kLogFlagBasic,
				"Authenticating: missing properties\n");
		if (supp->eap.required_props != NULL) {
		    eapolclient_log_plist(kLogFlagBasic,
					  supp->eap.required_props);
		}
		supp->last_status = kEAPClientStatusUserInputNotPossible;
		Supplicant_held(supp, kSupplicantEventStart, NULL);
		return;
	    }
	    eapolclient_log(kLogFlagBasic,
			    "Authenticating: user input required\n");
	    if (supp->eap.required_props != NULL) {
		eapolclient_log_plist(kLogFlagBasic,
				      supp->eap.required_props);
	    }
	}
	Supplicant_report_status(supp);

	/* try to set the session key, if it is available */
	if (EAPOLSocketIsWireless(supp->sock)) {
	    set_wpa_key_info(supp);
	}
	break;
    case kEAPClientStateSuccess:
	/* authentication method succeeded */
	my_log(LOG_NOTICE, "%s: successfully authenticated",
	       supp->eap.last_type_name);
	/* try to set the session key, if it is available */
	if (EAPOLSocketIsWireless(supp->sock)) {
	    set_wpa_key_info(supp);
	}
	Supplicant_authenticated(supp, kSupplicantEventStart, NULL);
	break;
    case kEAPClientStateFailure:
	/* authentication method failed */
	eap_client_log_failure(supp);
	my_log(LOG_NOTICE, "%s: authentication failed with status %d",
	       supp->eap.last_type_name, supp->last_status);
	Supplicant_held(supp, kSupplicantEventStart, NULL);
	break;
    }
    return;
}

static void
Supplicant_authenticating(SupplicantRef supp, SupplicantEvent event, 
			  void * evdata)
{
    EAPRequestPacket *		req_p;
    SupplicantState		prev_state = supp->state;
    EAPOLSocketReceiveDataRef 	rx = evdata;

    switch (event) {
    case kSupplicantEventStart:
	if (EAPOLSocketIsWireless(supp->sock)) {
	    clear_wpa_key_info(supp);
	}
	supp->state = kSupplicantStateAuthenticating;
	Supplicant_report_status(supp);
	EAPOLSocketEnableReceive(supp->sock, 
				 (void *)Supplicant_authenticating,
				 (void *)supp,
				 (void *)kSupplicantEventData);
	/* FALL THROUGH */
    case kSupplicantEventData:
	Timer_cancel(supp->timer);
	supp->no_authenticator = FALSE;
	if (rx->eapol_p->packet_type != kEAPOLPacketTypeEAPPacket) {
	    break;
	}
	req_p = (EAPRequestPacket *)rx->eapol_p->body;
	switch (req_p->code) {
	case kEAPCodeSuccess:
	    process_packet(supp, rx);
	    break;

	case kEAPCodeFailure:
	    process_packet(supp, rx);
	    break;

	case kEAPCodeRequest:
	case kEAPCodeResponse:
	    switch (req_p->type) {
	    case kEAPTypeIdentity:
		if (req_p->code == kEAPCodeResponse) {
		    /* don't care about responses */
		    break;
		}
		if (event == kSupplicantEventStart) {
		    /* this will not happen if we're bug free */
		    my_log(LOG_NOTICE, "internal error:"
			   " Supplicant_authenticating: "
			   "recursion avoided from state %s",
			   SupplicantStateString(prev_state));
		    break;
		}
		Supplicant_acquired(supp, kSupplicantEventStart, evdata);
		break;
		
	    case kEAPTypeNotification:	
		if (req_p->code == kEAPCodeResponse) {
		    /* don't care about responses */
		    break;
		}
		/* need to display information to the user XXX */
		my_log(LOG_NOTICE, "Authenticating: Notification '%.*s'",
		       EAPPacketGetLength((EAPPacketRef)req_p) - sizeof(*req_p),
		       req_p->type_data);

		eapolclient_log(kLogFlagBasic,
				"EAP Request Notification '%.*s'\n",
				EAPPacketGetLength((EAPPacketRef)req_p) 
				- sizeof(*req_p),
				req_p->type_data);
		respond_to_notification(supp, req_p->identifier);
		break;
	    default:
		process_packet(supp, rx);
		break;
	    } /* switch (req_p->type) */
	    break;
	default:
	    break;
	} /* switch (req_p->code) */
	break;
    case kSupplicantEventTimeout:
	Supplicant_connecting(supp, kSupplicantEventStart, NULL);
	break;
    default:
	break;
    }
    return;
}

static void
dictInsertNumber(CFMutableDictionaryRef dict, CFStringRef prop, uint32_t num)
{
    CFNumberRef			num_cf;

    num_cf = CFNumberCreate(NULL, kCFNumberSInt32Type, &num);
    CFDictionarySetValue(dict, prop, num_cf);
    my_CFRelease(&num_cf);
    return;
}

static void
dictInsertEAPTypeInfo(CFMutableDictionaryRef dict, EAPType type,
		      const char * type_name)
{
    if (type == kEAPTypeInvalid) {
	return;
    }

    /* EAPTypeName */
    if (type_name != NULL) {
	CFStringRef		eap_type_name_cf;
	eap_type_name_cf 
	    = CFStringCreateWithCString(NULL, type_name, 
					kCFStringEncodingASCII);
	CFDictionarySetValue(dict, kEAPOLControlEAPTypeName, 
			     eap_type_name_cf);
	my_CFRelease(&eap_type_name_cf);
    }
    /* EAPType */
    dictInsertNumber(dict, kEAPOLControlEAPType, type);
    return;
}

static void
dictInsertSupplicantState(CFMutableDictionaryRef dict, SupplicantState state)
{
    dictInsertNumber(dict, kEAPOLControlSupplicantState, state);
    return;
}

static void
dictInsertClientStatus(CFMutableDictionaryRef dict,
		       EAPClientStatus status, int error)
{
    dictInsertNumber(dict, kEAPOLControlClientStatus, status);
    dictInsertNumber(dict, kEAPOLControlDomainSpecificError, error);
    return;
}

static void
dictInsertGeneration(CFMutableDictionaryRef dict, uint32_t generation)
{
    dictInsertNumber(dict, kEAPOLControlConfigurationGeneration, generation);
}

static void
dictInsertRequiredProperties(CFMutableDictionaryRef dict,
			     CFArrayRef required_props)
{
    if (required_props == NULL) {
	CFMutableArrayRef array;

	/* to start, we at least need the user name */
	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(array, kEAPClientPropUserName);
	CFDictionarySetValue(dict, kEAPOLControlRequiredProperties,
			     array);
	my_CFRelease(&array);
    }
    else {
	CFDictionarySetValue(dict, kEAPOLControlRequiredProperties,
			     required_props);
    }
    return;
}

static void
dictInsertPublishedProperties(CFMutableDictionaryRef dict,
			      CFDictionaryRef published_props)
{
    if (published_props == NULL) {
	return;
    }
    CFDictionarySetValue(dict, kEAPOLControlAdditionalProperties,
			 published_props);
}

static void
dictInsertIdentityAttributes(CFMutableDictionaryRef dict,
			     CFArrayRef identity_attributes)
{
    if (identity_attributes == NULL) {
	return;
    }
    CFDictionarySetValue(dict, kEAPOLControlIdentityAttributes,
			 identity_attributes);
}

static void
dictInsertMode(CFMutableDictionaryRef dict, EAPOLControlMode mode)
{
    if (mode == kEAPOLControlModeNone) {
	return;
    }
    dictInsertNumber(dict, kEAPOLControlMode, mode);
    if (mode == kEAPOLControlModeSystem) {
	CFDictionarySetValue(dict, kEAPOLControlSystemMode, kCFBooleanTrue);
    }
    return;
}

void
Supplicant_stop(SupplicantRef supp)
{
    eapolclient_log(kLogFlagBasic, "stop\n");
    eap_client_free(supp);
    Supplicant_logoff(supp, kSupplicantEventStart, NULL);
    Supplicant_free(&supp);
    fflush(stdout);
    fflush(stderr);
    return;
}

static void
user_supplied_data(SupplicantRef supp)
{
    eapolclient_log(kLogFlagBasic, "user_supplied_data\n");
    switch (supp->state) {
    case kSupplicantStateAcquired:
	Supplicant_acquired(supp, 
			    kSupplicantEventMoreDataAvailable,
			    NULL);
	break;
    case kSupplicantStateAuthenticating:
	if (supp->last_rx_packet.eapol_p != NULL) {
	    process_packet(supp, &supp->last_rx_packet);
	}
	break;
    default:
	break;
    }
}

static void
create_ui_config_dict(SupplicantRef supp)
{
    if (supp->ui_config_dict != NULL) {
	return;
    }
    supp->ui_config_dict 
	= CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);
    
    return;
}

#if ! TARGET_OS_EMBEDDED
static Boolean
dicts_compare_arrays(CFDictionaryRef dict1, CFDictionaryRef dict2,
		     CFStringRef propname)
{
    CFArrayRef	array1 = NULL;
    CFArrayRef	array2 = NULL;

    if (dict1 != NULL && dict2 != NULL) {
	array1 = CFDictionaryGetValue(dict1, propname);
	array2 = CFDictionaryGetValue(dict2, propname);
    }
    if (array1 == NULL || array2 == NULL) {
	return (FALSE);
    }
    return (CFEqual(array1, array2));
}

static void
trust_callback(const void * arg1, const void * arg2,
	       TrustDialogueResponseRef response)
{
    CFDictionaryRef		config_dict = NULL;
    SupplicantRef		supp = (SupplicantRef)arg1;
    CFDictionaryRef		trust_info;
    CFArrayRef			trust_proceed;

    if (supp->trust_prompt == NULL) {
	return;
    }
    trust_info = TrustDialogue_trust_info(supp->trust_prompt);
    if (trust_info != NULL) {
	CFRetain(trust_info);
    }
    TrustDialogue_free(&supp->trust_prompt);
    if (trust_info == NULL) {
	return;
    }
    if (response->proceed == FALSE) {
	my_log(LOG_NOTICE, "%s: user cancelled", 
	       EAPOLSocketIfName(supp->sock, NULL));
	EAPOLControlStop(EAPOLSocketIfName(supp->sock, NULL));
	goto done;
    }
    if (supp->last_status != kEAPClientStatusUserInputRequired
	|| supp->eap.published_props == NULL) {
	goto done;
    }
    create_ui_config_dict(supp);
    if (dicts_compare_arrays(trust_info, supp->eap.published_props,
			     kEAPClientPropTLSServerCertificateChain)
	== FALSE) {
	CFDictionaryRemoveValue(supp->ui_config_dict, 
				kEAPClientPropTLSUserTrustProceedCertificateChain);
    }
    else {
	trust_proceed
	    = CFDictionaryGetValue(supp->eap.published_props,
				   kEAPClientPropTLSServerCertificateChain);
	if (trust_proceed != NULL) {
	    CFDictionarySetValue(supp->ui_config_dict, 
				 kEAPClientPropTLSUserTrustProceedCertificateChain,
				 trust_proceed);
	}
    }
    config_dict = CFDictionaryCreateCopy(NULL, supp->orig_config_dict);
    Supplicant_update_configuration(supp, config_dict);
    my_CFRelease(&config_dict);
    user_supplied_data(supp);

 done:
    my_CFRelease(&trust_info);
    return;
}

static void
password_callback(const void * arg1, const void * arg2, 
		  UserPasswordDialogueResponseRef response)
{
    CFDictionaryRef		config_dict = NULL;
    SupplicantRef		supp = (SupplicantRef)arg1;

    UserPasswordDialogue_free(&supp->pw_prompt);
    if (response->user_cancelled) {
	my_log(LOG_NOTICE, "%s: user cancelled", 
	       EAPOLSocketIfName(supp->sock, NULL));
	EAPOLControlStop(EAPOLSocketIfName(supp->sock, NULL));
	return;
    }
    if (supp->last_status != kEAPClientStatusUserInputRequired) {
	return;
    }
    create_ui_config_dict(supp);
    if (response->username != NULL) {
	CFDictionarySetValue(supp->ui_config_dict, kEAPClientPropUserName,
			     response->username);
    }
    if (response->password != NULL) {
	CFDictionarySetValue(supp->ui_config_dict, 
			     kEAPClientPropUserPassword,
			     response->password);
	supp->ignore_password = FALSE;
    }
    config_dict = CFDictionaryCreateCopy(NULL, supp->orig_config_dict);
    Supplicant_update_configuration(supp, config_dict);
    my_CFRelease(&config_dict);
    user_supplied_data(supp);
    return;
}

static Boolean
my_CFArrayContainsValue(CFArrayRef list, CFStringRef value)
{
    if (list == NULL) {
	return (FALSE);
    }
    return (CFArrayContainsValue(list, CFRangeMake(0, CFArrayGetCount(list)),
				 value));
}

#define EAPOLCONTROLLER_PATH	"/System/Library/SystemConfiguration/EAPOLController.bundle"

static CFBundleRef
get_bundle(void)
{
    static CFBundleRef		bundle = NULL;
    CFURLRef 			url;

    if (bundle != NULL) {
	return (bundle);
    }
    url = CFURLCreateWithFileSystemPath(NULL,
					CFSTR(EAPOLCONTROLLER_PATH),
					kCFURLPOSIXPathStyle, 1);
    if (url == NULL) {
	my_log(LOG_NOTICE, "can't find EAPOLController bundle");
	goto failed;
    }
    bundle = CFBundleCreate(NULL, url);
    CFRelease(url);
    if (bundle == NULL) {
	my_log(LOG_NOTICE,
	       "EAPOLController bundle create failed - using main bundle");
	goto failed;
    }
    return (bundle);

 failed:
    return (CFBundleGetMainBundle());
}

#define kAirPort8021XTitleFormat  "Authenticating to network \"%@\""
#define kEthernet8021XTitle	  "Authenticating to 802.1X network"

static CFStringRef
copy_localized_title(SupplicantRef supp)
{
    CFStringRef		title = NULL;
    CFStringRef		ssid = EAPOLSocketGetSSID(supp->sock);

    if (ssid != NULL) {
	CFStringRef	format;

	format 
	    = CFBundleCopyLocalizedString(get_bundle(),
					  CFSTR(kAirPort8021XTitleFormat),
					  CFSTR(kAirPort8021XTitleFormat),
					  NULL);
	if (format != NULL) {
	    title = CFStringCreateWithFormat(NULL, NULL, format, ssid);
	    CFRelease(format);
	}
    }
    else {
	title = CFBundleCopyLocalizedString(get_bundle(),
					    CFSTR(kEthernet8021XTitle),
					    CFSTR(kEthernet8021XTitle),
					    NULL);
    }
    if (title == NULL) {
	title = CFRetain(CFSTR(kEthernet8021XTitle));
    }
    return (title);
}

#endif /* ! TARGET_OS_EMBEDDED */

static void 
Supplicant_report_status(SupplicantRef supp)
{
    CFMutableDictionaryRef	dict;
#if ! TARGET_OS_EMBEDDED
    Boolean			need_username = FALSE;
    Boolean			need_password = FALSE;
    Boolean			need_trust = FALSE;
#endif /* ! TARGET_OS_EMBEDDED */
    CFDateRef			timestamp = NULL;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    dictInsertMode(dict, EAPOLSocketGetMode(supp->sock));
    if (supp->config_id != NULL) {
	CFDictionarySetValue(dict, kEAPOLControlUniqueIdentifier,
			     supp->config_id);
    }
    dictInsertSupplicantState(dict, supp->state);
    if (supp->no_authenticator) {
	/* don't report EAP type information if no auth was present */
	dictInsertClientStatus(dict, kEAPClientStatusOK, 0);
    }
    else {
	dictInsertEAPTypeInfo(dict, supp->eap.last_type,
			      supp->eap.last_type_name);
	dictInsertClientStatus(dict, supp->last_status,
			       supp->last_error);
	if (supp->last_status == kEAPClientStatusUserInputRequired) {
	    if (supp->username == NULL) {
		dictInsertRequiredProperties(dict, NULL);
#if ! TARGET_OS_EMBEDDED
		need_username = TRUE;
#endif /* ! TARGET_OS_EMBEDDED */
	    }
	    else {
		dictInsertRequiredProperties(dict, supp->eap.required_props);
#if ! TARGET_OS_EMBEDDED
		need_password 
		    = my_CFArrayContainsValue(supp->eap.required_props,
					      kEAPClientPropUserPassword);
		need_trust
		    = my_CFArrayContainsValue(supp->eap.required_props,
					      kEAPClientPropTLSUserTrustProceedCertificateChain);
#endif /* ! TARGET_OS_EMBEDDED */
	    }
	}
	dictInsertPublishedProperties(dict, supp->eap.published_props);
	dictInsertIdentityAttributes(dict, supp->identity_attributes);
    }
    dictInsertGeneration(dict, supp->generation);
    timestamp = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    CFDictionarySetValue(dict, kEAPOLControlTimestamp, timestamp);
    my_CFRelease(&timestamp);
    if (eapolclient_should_log(kLogFlagBasic)) {
	eapolclient_log(kLogFlagBasic, "Supplicant %s status: state=%s\n",
			EAPOLSocketName(supp->sock),
			SupplicantStateString(supp->state));
    }
    eapolclient_log_plist(kLogFlagStatus, dict);

    EAPOLSocketReportStatus(supp->sock, dict);
    my_CFRelease(&dict);

#if ! TARGET_OS_EMBEDDED
    if (supp->no_ui) {
	goto no_ui;
    }
    if (need_username || need_password) {
	if (supp->pw_prompt == NULL) {
	    CFStringRef password;
	    CFStringRef	title;
	    CFStringRef	username;

	    title = copy_localized_title(supp);
	    username = CFDictionaryGetValue(supp->config_dict,
					    kEAPClientPropUserName);
	    if (supp->one_time_password) {
		password = NULL;
	    }
	    else {
		password = CFDictionaryGetValue(supp->config_dict,
						kEAPClientPropUserPassword);
	    }
	    supp->pw_prompt 
		= UserPasswordDialogue_create(password_callback, supp, NULL,
					      NULL, title, NULL, 
					      username, password);
	    my_CFRelease(&title);
	}
    }
    if (need_trust) {
	if (supp->trust_prompt == NULL) {
	    CFStringRef	title;

	    title = copy_localized_title(supp);
	    supp->trust_prompt 
		= TrustDialogue_create(trust_callback, supp, NULL,
				       supp->eap.published_props,
				       NULL, title);
	    my_CFRelease(&title);
	}
    }
 no_ui:
#endif /* ! TARGET_OS_EMBEDDED */

    return;
}

static void
Supplicant_held(SupplicantRef supp, SupplicantEvent event, 
		void * evdata)
{
    EAPRequestPacket *		req_p;
    EAPOLSocketReceiveDataRef 	rx = evdata;
    struct timeval 		t = {S_held_period_secs, 0};

    switch (event) {
    case kSupplicantEventStart:
	if (EAPOLSocketIsWireless(supp->sock)) {
	    clear_wpa_key_info(supp);
	}
	Supplicant_cancel_pending_events(supp);
	supp->state = kSupplicantStateHeld;
	Supplicant_report_status(supp);
	supp->previous_identifier = BAD_IDENTIFIER;
	EAPAcceptTypesReset(&supp->eap_accept);
	if (supp->eap.module != NULL 
	    && supp->no_ui == FALSE
	    && supp->last_status == kEAPClientStatusFailed) {
	    clear_password(supp);
	}
	supp->last_status = kEAPClientStatusOK;
	supp->last_error = 0;
	free_last_packet(supp);
	eap_client_free(supp);
#if ! TARGET_OS_EMBEDDED
	UserPasswordDialogue_free(&supp->pw_prompt);
	TrustDialogue_free(&supp->trust_prompt);
#endif /* ! TARGET_OS_EMBEDDED */
	EAPOLSocketEnableReceive(supp->sock, 
				 (void *)Supplicant_held,
				 (void *)supp,
				 (void *)kSupplicantEventData);
	/* set a timeout */
	Timer_set_relative(supp->timer, t,
			   (void *)Supplicant_held,
			   (void *)supp, 
			   (void *)kSupplicantEventTimeout,
			   NULL);
	break;
    case kSupplicantEventTimeout:
	Supplicant_connecting(supp, kSupplicantEventStart, NULL);
	break;
    case kSupplicantEventData:
	if (rx->eapol_p->packet_type != kEAPOLPacketTypeEAPPacket) {
	    break;
	}
	req_p = (EAPRequestPacket *)rx->eapol_p->body;
	switch (req_p->code) {
	case kEAPCodeRequest:
	    switch (req_p->type) {
	    case kEAPTypeIdentity:
		Supplicant_acquired(supp, kSupplicantEventStart, evdata);
		break;
	    case kEAPTypeNotification:
		/* need to display information to the user XXX */
		my_log(LOG_NOTICE, "Held: Notification '%.*s'",
		       EAPPacketGetLength((EAPPacketRef)req_p) - sizeof(*req_p),
		       req_p->type_data);
		respond_to_notification(supp, req_p->identifier);
		break;
	    default:
		break;
	    }
	    break;
	default:
	    break;
	}
	break;

    default:
	break;
    }
}

void
Supplicant_start(SupplicantRef supp)
{
    if (EAPOLSocketIsLinkActive(supp->sock)) {
	Supplicant_disconnected(supp, kSupplicantEventStart, NULL);
    }
    else {
	Supplicant_inactive(supp, kSupplicantEventStart, NULL);
    }
    return;
}

static void
Supplicant_inactive(SupplicantRef supp, SupplicantEvent event, void * evdata)
{
    switch (event) {
    case kSupplicantEventStart:
	Supplicant_cancel_pending_events(supp);
	supp->state = kSupplicantStateInactive;
	supp->no_authenticator = TRUE;
	Supplicant_report_status(supp);
	EAPOLSocketEnableReceive(supp->sock, 
				 (void *)Supplicant_connecting,
				 (void *)supp, 
				 (void *)kSupplicantEventStart);
	break;
	
    default:
	break;
    }
    return;
}

static void
Supplicant_logoff(SupplicantRef supp, SupplicantEvent event, void * evdata)
{
    switch (event) {
    case kSupplicantEventStart:
	Supplicant_cancel_pending_events(supp);
	if (supp->state != kSupplicantStateAuthenticated) {
	    break;
	}
	supp->state = kSupplicantStateLogoff;
	supp->last_status = kEAPClientStatusOK;
	eap_client_free(supp);
	EAPOLSocketTransmit(supp->sock,
			    kEAPOLPacketTypeLogoff,
			    NULL, 0);
	Supplicant_report_status(supp);
	break;
    default:
	break;
    }
    return;
}

static bool
my_strcmp(char * s1, char * s2)
{
    if (s1 == NULL || s2 == NULL) {
	if (s1 == s2) {
	    return (0);
	}
	if (s1 == NULL) {
	    return (-1);
	}
	return (1);
    }
    return (strcmp(s1, s2));
}

static char *
eap_method_user_name(EAPAcceptTypesRef accept, CFDictionaryRef config_dict)
{
    int 	i;

    for (i = 0; i < accept->count; i++) {
	EAPClientModuleRef	module;
	CFStringRef		eap_user;

	module = EAPClientModuleLookup(accept->types[i]);
	if (module == NULL) {
	    continue;
	}
	eap_user = EAPClientModulePluginUserName(module, config_dict);
	if (eap_user != NULL) {
	    char *	user;

	    user = my_CFStringToCString(eap_user, kCFStringEncodingUTF8);
	    my_CFRelease(&eap_user);
	    return (user);
	}
    }
    return (NULL);
}

#if ! TARGET_OS_EMBEDDED
static OSStatus
mySecKeychainCopySystemKeychain(SecKeychainRef * ret_keychain)
{
    SecKeychainRef	keychain = NULL;
    OSStatus		status;
    
    status = SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);
    if (status != noErr) {
	my_log(LOG_NOTICE, "SecKeychainSetPreferenceDomain() failed, %ld",
	       status);
	goto done;
    }
    status = SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					  &keychain);
    if (status != noErr) {
	my_log(LOG_NOTICE, "SecKeychainCopyDomainDefault() failed, %ld",
	       status);
    }

 done:
    *ret_keychain = keychain;
    return (status);
}
#endif /* ! TARGET_OS_EMBEDDED */

static char *
S_copy_password_from_keychain(bool use_system_keychain,
			      CFStringRef unique_id_str)
{
    SecKeychainRef 	keychain = NULL;
    char *		password = NULL;
    CFDataRef		password_data = NULL;
    int			password_length;
    OSStatus		status;

#if ! TARGET_OS_EMBEDDED
    if (use_system_keychain) {
	status = mySecKeychainCopySystemKeychain(&keychain);
	if (status != noErr) {
	    goto done;
	}
    }
#endif /* ! TARGET_OS_EMBEDDED */

    status = EAPSecKeychainPasswordItemCopy(keychain, unique_id_str,
					    &password_data);
    if (status != noErr) {
	my_log(LOG_NOTICE, "SecKeychainFindGenericPassword failed, %d",
	       status);
	goto done;
    }
    password_length = CFDataGetLength(password_data);
    password = malloc(password_length + 1);
    bcopy(CFDataGetBytePtr(password_data), password, password_length);
    password[password_length] = '\0';

 done:
    my_CFRelease(&password_data);
    my_CFRelease(&keychain);
    return ((char *)password);
}

static Boolean
myCFDictionaryGetBooleanValue(CFDictionaryRef properties, CFStringRef propname,
			       Boolean def_value)
{
    bool		ret = def_value;

    if (properties != NULL) {
	CFBooleanRef	val;

	val = CFDictionaryGetValue(properties, propname);
	if (isA_CFBoolean(val)) {
	    ret = CFBooleanGetValue(val);
	}
    }
    return (ret);
}
static bool
S_set_user_password(SupplicantRef supp)
{
    bool		change = FALSE;
    CFStringRef		identity_cf = NULL;
    char *		identity = NULL;
    CFStringRef		name_cf = NULL;
    char *		name = NULL;
    CFStringRef		password_cf = NULL;
    char *		password = NULL;

    if (supp->config_dict == NULL) {
	return (TRUE);
    }
    /* extract the username */
    name_cf = CFDictionaryGetValue(supp->config_dict, kEAPClientPropUserName);
    name_cf = isA_CFString(name_cf);
    if (name_cf != NULL) {
	name = my_CFStringToCString(name_cf, kCFStringEncodingUTF8);
    }
    if (name == NULL) {
	/* no username specified, ask EAP types if they can come up with one */
	name = eap_method_user_name(&supp->eap_accept,
				    supp->config_dict);
    }
    if (my_strcmp(supp->username, name) != 0) {
	change = TRUE;
    }
    if (supp->username != NULL) {
	free(supp->username);
    }
    supp->username = name;
    if (name != NULL) {
	supp->username_length = strlen(name);
    }
    else {
	supp->username_length = 0;
    }

    /* check whether one-time use password */
    supp->one_time_password = 
	myCFDictionaryGetBooleanValue(supp->config_dict,
				      kEAPClientPropOneTimeUserPassword,
				      supp->one_time_password);
    /* extract the password */
    if (supp->ignore_password == FALSE) {
	password_cf = CFDictionaryGetValue(supp->config_dict, 
					   kEAPClientPropUserPassword);
	password_cf = isA_CFString(password_cf);
	if (password_cf != NULL) {
	    password = my_CFStringToCString(password_cf, 
					    kCFStringEncodingMacRoman);
	}
	else {
	    CFStringRef	item_cf;
	    
	    item_cf 
		= CFDictionaryGetValue(supp->config_dict, 
				       kEAPClientPropUserPasswordKeychainItemID);
	    item_cf = isA_CFString(item_cf);
	    if (item_cf != NULL) {
		bool	system_mode;

		system_mode = (EAPOLSocketGetMode(supp->sock)
			       == kEAPOLControlModeSystem);
		password = S_copy_password_from_keychain(system_mode, 
							 item_cf);
		if (password == NULL) {
		    my_log(LOG_NOTICE, 
			   "%s: failed to retrieve password from keychain",
			   EAPOLSocketIfName(supp->sock, NULL));
		}
	    }
	}
    }
    if (supp->password != NULL) {
	free(supp->password);
    }
    supp->password = password;
    if (password != NULL) {
	supp->password_length = strlen(password);
    }
    else {
	supp->password_length = 0;
    }

    /* extract the identity */
    if (EAPAcceptTypesUseIdentity(&supp->eap_accept) == TRUE) {
	identity_cf = CFDictionaryGetValue(supp->config_dict, 
					   kEAPClientPropOuterIdentity);
	identity_cf = isA_CFString(identity_cf);
	if (identity_cf != NULL) {
	    identity = my_CFStringToCString(identity_cf, kCFStringEncodingUTF8);
	}
    }
    if (my_strcmp(supp->identity, identity) != 0) {
	change = TRUE;
    }
    if (supp->identity != NULL) {
	free(supp->identity);
    }
    supp->identity = identity;
    if (identity != NULL) {
	supp->identity_length = strlen(identity);
    }
    else {
	supp->identity_length = 0;
    }

    return (change);
}

static void 
dict_add_key_value(const void * key, const void * value, void * context)
{
    CFMutableDictionaryRef	new_dict = (CFMutableDictionaryRef)context;

    /* add the (key, value) if the key doesn't already exist */
    CFDictionaryAddValue(new_dict, key, value);
    return;
}

static void 
dict_set_key_value(const void * key, const void * value, void * context)
{
    CFMutableDictionaryRef	new_dict = (CFMutableDictionaryRef)context;

    /* set the (key, value) */
    CFDictionarySetValue(new_dict, key, value);
    return;
}

static bool
cfstring_is_empty(CFStringRef str)
{
    if (str == NULL) {
	return (FALSE);
    }
    return (isA_CFString(str) == NULL || CFStringGetLength(str) == 0);
}

static bool
debug_properties_present(CFDictionaryRef dict)
{
    bool	ret = FALSE;

    if (CFDictionaryGetValue(dict,
			     kEAPOLControlLogLevel) != NULL
	|| CFDictionaryGetValue(dict, CFSTR("_debug")) != NULL) {
	ret = TRUE;
    }
    return (ret);
}

bool
Supplicant_update_configuration(SupplicantRef supp, CFDictionaryRef config_dict)
{
    bool			change = FALSE;
    CFStringRef			config_id;
    bool			debug_on = FALSE;
    CFDictionaryRef		default_dict;
    CFDictionaryRef		default_eap_config = NULL;
    CFDictionaryRef		eap_config;
    bool			empty_password = FALSE;
    bool			empty_user = FALSE;
#if ! TARGET_OS_EMBEDDED
    CFBooleanRef		enable_ui;
#endif /* ! TARGET_OS_EMBEDDED */

    /* keep a copy of the original around */
    my_CFRelease(&supp->orig_config_dict);
    supp->orig_config_dict = CFDictionaryCreateCopy(NULL, config_dict);

    /* get the new configuration */
    eap_config = CFDictionaryGetValue(config_dict,
				      kEAPOLControlEAPClientConfiguration);
    if (isA_CFDictionary(eap_config) == NULL) {
	eap_config = config_dict;
    }
    empty_user 
	= cfstring_is_empty(CFDictionaryGetValue(eap_config,
						 kEAPClientPropUserName));
    empty_password 
	= cfstring_is_empty(CFDictionaryGetValue(eap_config,
						 kEAPClientPropUserPassword));
    default_dict = eapolclient_default_dict();
    if (default_dict != NULL) {
	if (debug_properties_present(default_dict)) {
	    debug_on = TRUE;
	}
	default_eap_config
	    = CFDictionaryGetValue(default_dict,
				   kEAPOLControlEAPClientConfiguration);
	if (default_eap_config == NULL) {
	    default_eap_config = default_dict;
	}
    }

    my_CFRelease(&supp->config_dict);

    /* clean up empty username/password, add UI properties and default */
    if (empty_user || empty_password 
	|| supp->ui_config_dict != NULL
	|| default_eap_config != NULL) {
	CFMutableDictionaryRef		new_eap_config = NULL;

	new_eap_config = CFDictionaryCreateMutableCopy(NULL, 0, eap_config);
	if (empty_user) {
	    CFDictionaryRemoveValue(new_eap_config, kEAPClientPropUserName);
	}
	if (empty_password) {
	    CFDictionaryRemoveValue(new_eap_config, kEAPClientPropUserPassword);
	}
	if (supp->ui_config_dict != NULL) {
	    CFDictionaryApplyFunction(supp->ui_config_dict, dict_add_key_value, 
				      new_eap_config);
	}
	if (default_eap_config != NULL) {
	    CFDictionaryApplyFunction(default_eap_config, dict_add_key_value, 
				      new_eap_config);
	}
	supp->config_dict = new_eap_config;
    }
    else {
	supp->config_dict = CFRetain(eap_config);
    }

    /* bump the configuration generation */
    supp->generation++;

    /* get the configuration identifier */
    my_CFRelease(&supp->config_id);
    config_id = CFDictionaryGetValue(config_dict,
				     kEAPOLControlUniqueIdentifier);
    if (isA_CFString(config_id) != NULL) {
	supp->config_id = CFRetain(config_id);
    }

#if ! TARGET_OS_EMBEDDED
    /* check whether we should allow UI or not */
    enable_ui = CFDictionaryGetValue(config_dict,
				     kEAPOLControlEnableUserInterface);
    if (isA_CFBoolean(enable_ui) != NULL) {
	supp->no_ui = !CFBooleanGetValue(enable_ui);
    }
#endif /* ! TARGET_OS_EMBEDDED */

    /* update the list of EAP types we accept */
    EAPAcceptTypesInit(&supp->eap_accept, supp->config_dict);
    if (S_set_user_password(supp)) {
	change = TRUE;
    }
    if (EAPAcceptTypesIsSupportedType(&supp->eap_accept,
				      eap_client_type(supp)) == FALSE) {
	/* negotiated EAP type is no longer valid, start over */
	eap_client_free(supp);
	change = TRUE;
    }

    /* enable debugging */
    if (debug_on
	|| debug_properties_present(config_dict)
	|| debug_properties_present(supp->config_dict)) {
	Supplicant_set_debug(supp, TRUE);
    }
    else {
	Supplicant_set_debug(supp, FALSE);
    }
    eapolclient_log(kLogFlagBasic, "update_configuration\n");
    eapolclient_log_config_dict(kLogFlagConfig, supp->config_dict);
    return (change);
}

bool
Supplicant_control(SupplicantRef supp,
		   EAPOLClientControlCommand command,
		   CFDictionaryRef control_dict)
{
    bool			change;
    CFDictionaryRef		config_dict = NULL;
    bool			should_stop = FALSE;
    CFDictionaryRef		user_input_dict = NULL;

    switch (command) {
    case kEAPOLClientControlCommandRetry:
	if (supp->state != kSupplicantStateInactive) {
	    Supplicant_connecting(supp, kSupplicantEventStart, NULL);
	}
	break;
    case kEAPOLClientControlCommandTakeUserInput:
	user_input_dict = CFDictionaryGetValue(control_dict,
					       kEAPOLClientControlUserInput);
	if (user_input_dict != NULL) {
	    /* add the user input to the ui_config_dict */
	    create_ui_config_dict(supp);
	    CFDictionaryApplyFunction(user_input_dict, dict_set_key_value, 
				      supp->ui_config_dict);
	}
	config_dict = CFDictionaryCreateCopy(NULL, supp->orig_config_dict);
	Supplicant_update_configuration(supp, config_dict);
	my_CFRelease(&config_dict);
	user_supplied_data(supp);
	break;
    case kEAPOLClientControlCommandRun:
	config_dict = CFDictionaryGetValue(control_dict,
					   kEAPOLClientControlConfiguration);
	if (config_dict == NULL) {
	    should_stop = TRUE;
	    break;
	}
	change = Supplicant_update_configuration(supp, config_dict);
	if (EAPOLSocketIsLinkActive(supp->sock) == FALSE) {
	    /* no point in doing anything if the link is down */
	    break;
	}
	if (supp->last_status == kEAPClientStatusUserInputRequired) {
	    switch (supp->state) {
	    case kSupplicantStateAcquired:
		change = FALSE;
		if (supp->username != NULL) {
		    Supplicant_acquired(supp, 
					kSupplicantEventMoreDataAvailable,
					NULL);
		}
		break;
	    case kSupplicantStateAuthenticating:
		if (change == FALSE && supp->last_rx_packet.eapol_p != NULL) {
		    process_packet(supp, &supp->last_rx_packet);
		}
		break;
	    default:
		break;
	    }
	}
	if (change) {
	    Supplicant_disconnected(supp, kSupplicantEventStart, NULL);
	}
	break;
    case kEAPOLClientControlCommandStop:
	should_stop = TRUE;
	break;
    default:
	break;

    }					  
    if (supp->debug) {
	fflush(stdout);
	fflush(stderr);
    }
    return (should_stop);
}

void
Supplicant_link_status_changed(SupplicantRef supp, bool active)
{
    struct timeval	t = {0, 0};

    if (active) {

	t.tv_sec = S_link_active_period_secs;
	switch (supp->state) {
	case kSupplicantStateInactive:
	case kSupplicantStateConnecting:
	    /* give the Authenticator a chance to initiate */
	    t.tv_sec = 0;
	    t.tv_usec = 500 * 1000; /* 1/2 second */
	    /* FALL THROUGH */
	default:
	    /*
	     * wait awhile before entering connecting state to avoid
	     * disrupting an existing conversation
	     */
	    Timer_set_relative(supp->timer, t,
			       (void *)Supplicant_connecting,
			       (void *)supp,
			       (void *)kSupplicantEventStart,
			       NULL);
	    break;
	}
    }
    else {
	/*
	 * wait awhile before entering the inactive state to avoid
	 * disrupting an existing conversation
	 */
	t.tv_sec = S_link_inactive_period_secs;
	
	/* if link is down, enter wait for link state */
	Timer_set_relative(supp->timer, t,
			   (void *)Supplicant_inactive,
			   (void *)supp,
			   (void *)kSupplicantEventStart,
			   NULL);
    }
    return;
}

static CFDictionaryRef
eapolclient_default_dict(void)
{
    static CFDictionaryRef	default_dict = NULL;
    static bool			done = FALSE;
    CFBundleRef			bundle;
    uint8_t			config_file[MAXPATHLEN];
    Boolean			ok;
    CFURLRef			url;

    if (done) {
	return (default_dict);
    }
    done = TRUE;
    bundle = CFBundleGetMainBundle();
    if (bundle == NULL) {
	goto done;
    }
    url = CFBundleCopyResourceURL(bundle, CFSTR("eapolclient_default"), 
				  CFSTR("plist"), NULL);
    if (url == NULL) {
	goto done;
    }
    ok = CFURLGetFileSystemRepresentation(url, TRUE, config_file, 
					  sizeof(config_file));
    CFRelease(url);
    if (ok == FALSE) {
	goto done;
    }
    default_dict = my_CFPropertyListCreateFromFile((char *)config_file);
    if (default_dict != NULL) {
	if (isA_CFDictionary(default_dict) == NULL 
	    || CFDictionaryGetCount(default_dict) == 0) {
	    my_CFRelease(&default_dict);
	}
    }
 done:
    return (default_dict);

}


SupplicantRef
Supplicant_create(EAPOLSocketRef sock)
{
    SupplicantRef		supp = NULL;
    TimerRef			timer = NULL;

    timer = Timer_create();
    if (timer == NULL) {
	my_log(LOG_NOTICE, "Supplicant_create: Timer_create failed");
	goto failed;
    }

    supp = malloc(sizeof(*supp));
    if (supp == NULL) {
	my_log(LOG_NOTICE, "Supplicant_create: malloc failed");
	goto failed;
    }

    bzero(supp, sizeof(*supp));
    supp->timer = timer;
    supp->sock = sock;
    return (supp);

 failed:
    if (supp != NULL) {
	free(supp);
    }
    Timer_free(&timer);
    return (NULL);
}

SupplicantRef
Supplicant_create_with_supplicant(EAPOLSocketRef sock, SupplicantRef main_supp)
{
    SupplicantRef	supp;

    supp = Supplicant_create(sock);
    if (supp == NULL) {
	return (NULL);
    }
    supp->generation = main_supp->generation;
    supp->config_dict = CFRetain(main_supp->config_dict);
    if (main_supp->ui_config_dict) {
	supp->ui_config_dict 
	    = CFDictionaryCreateMutableCopy(NULL, 0, main_supp->ui_config_dict);
    }
    if (main_supp->identity != NULL) {
	supp->identity = strdup(main_supp->identity);
	supp->identity_length = main_supp->identity_length;
    }
    if (main_supp->username != NULL) {
	supp->username = strdup(main_supp->username);
	supp->username_length = main_supp->username_length;
    }
    if (main_supp->password != NULL) {
	supp->password = strdup(main_supp->password);
	supp->password_length = main_supp->password_length;
    }
    EAPAcceptTypesCopy(&supp->eap_accept, &main_supp->eap_accept);
    supp->debug = main_supp->debug;
    supp->no_ui = TRUE;

    return (supp);
}

void
Supplicant_free(SupplicantRef * supp_p)
{
    SupplicantRef supp;

    if (supp_p == NULL) {
	return;
    }
    supp = *supp_p;
    if (supp != NULL) {
#if ! TARGET_OS_EMBEDDED
	UserPasswordDialogue_free(&supp->pw_prompt);
	TrustDialogue_free(&supp->trust_prompt);
#endif /* ! TARGET_OS_EMBEDDED */
	Timer_free(&supp->timer);
	my_CFRelease(&supp->orig_config_dict);
	my_CFRelease(&supp->config_dict);
	my_CFRelease(&supp->ui_config_dict);
	my_CFRelease(&supp->config_id);
	my_CFRelease(&supp->identity_attributes);
	if (supp->identity != NULL) {
	    free(supp->identity);
	}
	if (supp->username != NULL) {
	    free(supp->username);
	}
	if (supp->password != NULL) {
	    free(supp->password);
	}
	EAPAcceptTypesFree(&supp->eap_accept);
	free_last_packet(supp);
	eap_client_free(supp);
	free(supp);
    }
    *supp_p = NULL;
    return;
}

void
Supplicant_set_debug(SupplicantRef supp, bool debug)
{
    supp->debug = debug;
    EAPOLSocketSetDebug(debug);
    /* my_log_set_verbose(debug); */
    return;
}

SupplicantState
Supplicant_get_state(SupplicantRef supp, EAPClientStatus * last_status)
{
    *last_status = supp->last_status;
    return (supp->state);
}

void
Supplicant_set_no_ui(SupplicantRef supp)
{
    supp->no_ui = TRUE;
    return;
}

static void
eapolclient_log_config_dict(uint32_t flags, CFDictionaryRef d)
{
    CFStringRef		password;
    CFStringRef		new_password;

    if (eapolclient_should_log(flags) == FALSE) {
	return;
    }
    password = CFDictionaryGetValue(d, kEAPClientPropUserPassword);
    new_password = CFDictionaryGetValue(d, kEAPClientPropNewPassword);
    if (password != NULL || new_password != NULL) {
	CFMutableDictionaryRef	d_copy;

	d_copy = CFDictionaryCreateMutableCopy(NULL, 0, d);
	if (password != NULL) {
	    CFDictionarySetValue(d_copy, kEAPClientPropUserPassword,
				 CFSTR("XXXXXXXX"));
	}
	if (new_password != NULL) {
	    CFDictionarySetValue(d_copy, kEAPClientPropNewPassword,
				 CFSTR("XXXXXXXX"));
	}
	eapolclient_log_plist(flags, d_copy);
	CFRelease(d_copy);
    }
    else {
	eapolclient_log_plist(flags, d);
    }
    return;
}

