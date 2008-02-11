
/*
 * Copyright (c) 2001-2004 Apple Computer, Inc. All rights reserved.
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
#include <sysexits.h>
#include <string.h>
#include <paths.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rc4.h>

#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPOLClient.h>
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
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>

#include "Supplicant.h"
#include "Timer.h"
#include "EAPOLSocket.h"
#include "printdata.h"
#include "convert.h"
#include "mylog.h"
#include "myCFUtil.h"
#include "ClientControlInterface.h"
#include "Dialogue.h"

#define START_PERIOD_SECS		30
#define AUTH_PERIOD_SECS		30
#define HELD_PERIOD_SECS		60
#define LINK_ACTIVE_PERIOD_SECS		4
#define LINK_INACTIVE_PERIOD_SECS	4
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
    CFStringRef			if_name_cf;
    SupplicantState		state;
    Timer *			timer;
    EAPOLSocket *		sock;
    EAPOLClientRef		client;
    SCDynamicStoreRef		store;

    uint32_t			generation;

    CFDictionaryRef		orig_config_dict;
    CFDictionaryRef		config_dict;
    CFMutableDictionaryRef	ui_config_dict;
    CFStringRef			config_id;
    bool			system_mode;

    CFDictionaryRef		default_dict;

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

    UserPasswordDialogueRef	pw_prompt;

    TrustDialogueRef		trust_prompt;

    int				start_count;

    bool			authenticated;

    bool			no_authenticator;

    struct eap_client		eap;

    EAPOLSocketReceiveData	last_rx_packet;
    EAPClientStatus		last_status;
    EAPClientDomainSpecificError last_error;

    bool			logoff_sent;
    bool			debug;

    bool			no_ui;
};

typedef enum {
    kSupplicantEventStart,
    kSupplicantEventData,
    kSupplicantEventTimeout,
    kSupplicantEventUserResponse,
    kSupplicantEventMoreDataAvailable,
} SupplicantEvent;

static bool
S_set_user_password(SupplicantRef supp);

static bool
is_link_active(SCDynamicStoreRef store, CFStringRef if_name_cf);

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
 ** EAP client module access convenience routines
 **/
static void
eap_client_free(SupplicantRef supp)
{
    if (supp->eap.module != NULL) {
	EAPClientModulePluginFree(supp->eap.module, &supp->eap.plugin_data);
	supp->eap.module = NULL;
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
	= EAPOLSocket_if_name(supp->sock, (uint32_t *)
			      &supp->eap.plugin_data.unique_id_length);
    S_set_uint32(&supp->eap.plugin_data.mtu,
		 EAPOLSocket_mtu(supp->sock) - sizeof(EAPOLPacket));

    supp->eap.plugin_data.username = (uint8_t *)supp->username;
    S_set_uint32(&supp->eap.plugin_data.username_length, 
		 supp->username_length);
    supp->eap.plugin_data.password = (uint8_t *)supp->password;
    S_set_uint32(&supp->eap.plugin_data.password_length, 
		 supp->password_length);
    *((CFDictionaryRef *)&supp->eap.plugin_data.properties) = supp->config_dict;
    *((bool *)&supp->eap.plugin_data.log_enabled) = supp->debug;
    *((bool *)&supp->eap.plugin_data.system_mode) = supp->system_mode;
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
    *((CFDictionaryRef *)&supp->eap.plugin_data.properties) 
	= supp->config_dict;
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
    EAPOLSocket_disable_receive(supp->sock);
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
    HMAC(EVP_md5(), server_key, server_key_length,
	 (const u_char *)packet_copy, packet_length, 
	 signature, NULL);
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
	return;
    }
    server_key = eap_client_server_key(supp, &server_key_length);
    if (server_key == NULL) {
	my_log(LOG_NOTICE, "Supplicant process_key: server key is NULL");
	return;
    }
    body_length = EAPOLPacketGetLength(eapol_p);
    packet_length = sizeof(EAPOLPacket) + body_length;

    if (eapol_key_verify_signature(eapol_p, packet_length,
				   server_key, server_key_length,
				   supp->debug) == FALSE) {
	my_log(LOG_NOTICE,
	       "Supplicant process key: key signature mismatch, ignoring");
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
	uint8_t *		enc_key;
	uint8_t *		rc4_key_data;
	int			rc4_key_data_length;
	RC4_KEY			rc4_key;

	/* decrypt the key from the packet */
	enc_key = malloc(key_length);
	rc4_key_data_length = session_key_length + sizeof(descr_p->key_IV);
	rc4_key_data = malloc(rc4_key_data_length);
	bcopy(descr_p->key_IV, rc4_key_data, sizeof(descr_p->key_IV));
	bcopy(session_key, rc4_key_data + sizeof(descr_p->key_IV),
	      session_key_length);
	RC4_set_key(&rc4_key, rc4_key_data_length, rc4_key_data);
	RC4(&rc4_key, key_length, descr_p->key, enc_key);
	EAPOLSocket_set_key(supp->sock, type, 
			    descr_p->key_index & kEAPOLKeyDescriptorIndexMask,
			    enc_key, key_length);
	free(rc4_key_data);
	free(enc_key);
    }
    else {
	EAPOLSocket_set_key(supp->sock, type, 
			    descr_p->key_index & kEAPOLKeyDescriptorIndexMask,
			    session_key, key_length);
    }
    return;
}

static void
Supplicant_force_renew(SupplicantRef supp)
{
    if (supp->client == NULL) {
	return;
    }

    (void)EAPOLClientForceRenew(supp->client);
    return;
}

static void
clear_wpa_key_info(SupplicantRef supp)
{
    if (EAPOLSocket_set_wpa_session_key(supp->sock, NULL, 0) == FALSE) {
	my_log(LOG_DEBUG, "clearing wpa session key failed");
    }
    return;
}

static void
set_wpa_key_info(SupplicantRef supp)
{
    uint8_t *	session_key;
    int		session_key_length;

    session_key = eap_client_session_key(supp, &session_key_length);
    if (session_key != NULL) {
	if (EAPOLSocket_set_wpa_session_key(supp->sock, session_key,
					    session_key_length) == FALSE) {
	    my_log(LOG_DEBUG, "setting wpa session key failed");
	}
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
	if (supp->authenticated == FALSE) {
	    Supplicant_force_renew(supp);
	    supp->authenticated = TRUE;
	}
	UserPasswordDialogue_free(&supp->pw_prompt);
	if (supp->one_time_password) {
	    clear_password(supp);
	}
	Supplicant_report_status(supp);
	EAPOLSocket_enable_receive(supp->sock, 
				   (void *)Supplicant_authenticated,
				   (void *)supp, 
				   (void *)kSupplicantEventData);
	if (supp->eap.module != NULL && EAPOLSocket_is_wireless(supp->sock)) {
	    set_wpa_key_info(supp);
	}
	break;
    case kSupplicantEventData:
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
	    if (EAPOLSocket_is_wireless(supp->sock)) {
		process_key(supp, rx->eapol_p);
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
    supp->logoff_sent = FALSE;
    supp->previous_identifier = BAD_IDENTIFIER;
    EAPAcceptTypesReset(&supp->eap_accept);
    supp->last_status = kEAPClientStatusOK;
    supp->last_error = 0;
    supp->authenticated = FALSE;
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
	EAPOLSocket_enable_receive(supp->sock, 
				   (void *)Supplicant_connecting,
				   (void *)supp, 
				   (void *)kSupplicantEventData);
	/* FALL THROUGH */
    case kSupplicantEventData:
	if (rx != NULL
	    && rx->eapol_p->packet_type == kEAPOLPacketTypeEAPPacket) {
	    EAPRequestPacket * req_p = (void *)rx->eapol_p->body;
	    
	    if (req_p->code == kEAPCodeRequest) {
		if (req_p->type == kEAPTypeIdentity) {
		    Supplicant_acquired(supp,
					kSupplicantEventStart,
					evdata);
		    break;
		}
		Supplicant_authenticating(supp,
					  kSupplicantEventStart,
					  evdata);
	    }
	    break;
	}
	/* FALL THROUGH */
    case kSupplicantEventTimeout:
	if (supp->start_count == S_max_start) {
	    /* no response from Authenticator */
	    supp->no_authenticator = TRUE;
	    Supplicant_authenticated(supp, kSupplicantEventStart, NULL);
	    break;
	}
	EAPOLSocket_transmit(supp->sock,
			     kEAPOLPacketTypeStart,
			     NULL, 0, NULL, TRUE);
	supp->start_count++;
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

static void
respond_with_identity(SupplicantRef supp, int identifier)
{
    char			buf[256];
    char *			identity;
    int				length;
    EAPPacketRef		pkt_p;
    int				size;

    if (supp->identity != NULL) {
	identity = supp->identity;
	length = supp->identity_length;
    }
    else {
	identity = supp->username;
	length = supp->username_length;
    }

    /* transmit a response/Identity */
    pkt_p = EAPPacketCreate(buf, sizeof(buf), 
			    kEAPCodeResponse, identifier,
			    kEAPTypeIdentity, 
			    identity, length,
			    &size);
    if (EAPOLSocket_transmit(supp->sock,
			     kEAPOLPacketTypeEAPPacket,
			     pkt_p, size, 
			     NULL, TRUE) < 0) {
	my_log(LOG_NOTICE, 
	       "EAPOL_transmit Identity failed");
    }
    if ((char *)pkt_p != buf) {
	free(pkt_p);
    }
    return;
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
	EAPOLSocket_enable_receive(supp->sock, 
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

	    supp->previous_identifier = req_p->identifier;
	    if (supp->username != NULL) {
		supp->last_status = kEAPClientStatusOK;
		Supplicant_report_status(supp);

		/* answer the query with what we have */
		respond_with_identity(supp, req_p->identifier);
		
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
	if (supp->username != NULL) {
	    supp->last_status = kEAPClientStatusOK;
	    Supplicant_report_status(supp);
	    respond_with_identity(supp, supp->previous_identifier);
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

    /* transmit a response/Notification */
    (void)EAPPacketCreate(&notif, sizeof(notif), 
			  kEAPCodeResponse, identifier,
			  kEAPTypeNotification, NULL, 0, &size);
    if (EAPOLSocket_transmit(supp->sock,
			     kEAPOLPacketTypeEAPPacket,
			     &notif, sizeof(notif), NULL, TRUE) < 0) {
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
    if (EAPOLSocket_transmit(supp->sock,
			     kEAPOLPacketTypeEAPPacket,
			     &nak, sizeof(nak), NULL, TRUE) < 0) {
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
		    supp->last_status = kEAPClientStatusProtocolNotSupported;
		    Supplicant_held(supp, kSupplicantEventStart, NULL);
		    return;
		}
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
		    my_log(LOG_NOTICE, "eap_client_init type %d failed",
			   req_p->type);
		    Supplicant_held(supp, kSupplicantEventStart, NULL);
		    return;
		}
		save_last_packet(supp, rx);
		Supplicant_report_status(supp);
		return;
	    }
	    Supplicant_report_status(supp);
	}
	break;
    case kEAPCodeResponse:
	if (req_p->type != eap_client_type(supp)) {
	    /* this should not happen, but if it does, ignore the packet */
	    return;
	}
	break;
    case kEAPCodeFailure:
	if (supp->eap.module == NULL) {
	    supp->last_status = kEAPClientStatusFailed;
	    Supplicant_held(supp, kSupplicantEventStart, NULL);
	    return;
	}
	break;
    case kEAPCodeSuccess:
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
    if (supp->debug && rx->logged == FALSE) {
	if (EAPClientModulePluginPacketDump(supp->eap.module,
					    stdout, in_pkt_p) == FALSE) {
	    eapol_packet_print(rx->eapol_p, rx->length);
	}
	rx->logged = TRUE;
    }
    state = eap_client_process(supp, in_pkt_p, &out_pkt_p,
			       &supp->last_status, &supp->last_error);
    if (out_pkt_p != NULL) {
	/* send the output packet */
	if (EAPOLSocket_transmit(supp->sock,
				 kEAPOLPacketTypeEAPPacket,
				 out_pkt_p,
				 EAPPacketGetLength(out_pkt_p),
				 NULL, FALSE) < 0) {
	    my_log(LOG_NOTICE, "process_packet: EAPOL_transmit %d failed",
		   out_pkt_p->code);
	}
	if (supp->debug) {
	    if (EAPClientModulePluginPacketDump(supp->eap.module,
						stdout, out_pkt_p) == FALSE) {
		eap_packet_print(out_pkt_p,
				 EAPPacketGetLength(out_pkt_p));
	    }
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
	    if (supp->system_mode && supp->eap.required_props != NULL) {
		supp->last_status = kEAPClientStatusUserInputNotPossible;
		Supplicant_held(supp, kSupplicantEventStart, NULL);
	    }
	    else {
		Supplicant_report_status(supp);
	    }
	}
	else {
	    Supplicant_report_status(supp);
	}
	break;
    case kEAPClientStateSuccess:
	/* authentication method succeeded */
	Supplicant_authenticated(supp, kSupplicantEventStart, NULL);
	break;
    case kEAPClientStateFailure:
	/* authentication method failed */
	eap_client_log_failure(supp);
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
	if (EAPOLSocket_is_wireless(supp->sock)) {
	    clear_wpa_key_info(supp);
	}
	supp->state = kSupplicantStateAuthenticating;
	Supplicant_report_status(supp);
	Timer_cancel(supp->timer);
	EAPOLSocket_enable_receive(supp->sock, 
				   (void *)Supplicant_authenticating,
				   (void *)supp,
				   (void *)kSupplicantEventData);
	/* FALL THROUGH */
    case kSupplicantEventData:
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
	CFDictionarySetValue(dict, kEAPOLControlEAPTypeName, 
			     eap_type_name_cf);
	my_CFRelease(&eap_type_name_cf);
    }
    /* EAPType */
    eap_type_cf = CFNumberCreate(NULL, kCFNumberIntType, &eap_type);
    CFDictionarySetValue(dict, kEAPOLControlEAPType, eap_type_cf);
    my_CFRelease(&eap_type_cf);

    return;
}

static void
dictInsertSupplicantState(CFMutableDictionaryRef dict, SupplicantState state)
{
    CFNumberRef		supp_state_cf;

    supp_state_cf = CFNumberCreate(NULL, kCFNumberIntType, &state);
    CFDictionarySetValue(dict, kEAPOLControlSupplicantState, supp_state_cf);
    my_CFRelease(&supp_state_cf);
    return;
}

static void
dictInsertClientStatus(CFMutableDictionaryRef dict,
		       EAPClientStatus status, int error)
{
    CFNumberRef			num_cf;

    /* status */
    num_cf = CFNumberCreate(NULL, kCFNumberIntType, &status);
    CFDictionarySetValue(dict, kEAPOLControlClientStatus, num_cf);
    my_CFRelease(&num_cf);

    /* error */
    num_cf = CFNumberCreate(NULL, kCFNumberIntType, &error);
    CFDictionarySetValue(dict, kEAPOLControlDomainSpecificError, num_cf);
    my_CFRelease(&num_cf);
    return;
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
Supplicant_stop(SupplicantRef supp)
{
    my_log(LOG_NOTICE, "%s STOP",
	   EAPOLSocket_if_name(supp->sock, NULL));
    eap_client_free(supp);
    Supplicant_logoff(supp, kSupplicantEventStart, NULL);
    Supplicant_free(&supp);
    fflush(stdout);
    fflush(stderr);
    exit(EX_OK);
    /* NOT REACHED */
    return;
}

static void
user_supplied_data(SupplicantRef supp)
{
    switch (supp->state) {
    case kSupplicantStateAcquired:
	Supplicant_acquired(supp, 
			    kSupplicantEventMoreDataAvailable,
			    NULL);
	break;
    case kSupplicantStateAuthenticating:
	if (supp->last_rx_packet.eapol_p != NULL) {
	    supp->last_rx_packet.logged = TRUE;
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
	       EAPOLSocket_if_name(supp->sock, NULL));
	EAPOLControlStop(EAPOLSocket_if_name(supp->sock, NULL));
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
    if (supp->orig_config_dict != NULL) {
	config_dict = CFDictionaryCreateCopy(NULL, supp->orig_config_dict);
    }
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
	       EAPOLSocket_if_name(supp->sock, NULL));
	EAPOLControlStop(EAPOLSocket_if_name(supp->sock, NULL));
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
    supp->one_time_password = response->one_time_password;
    if (supp->orig_config_dict != NULL) {
	config_dict = CFDictionaryCreateCopy(NULL, supp->orig_config_dict);
    }
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

static void 
Supplicant_report_status(SupplicantRef supp)
{
    CFMutableDictionaryRef	dict;
    Boolean			need_username = FALSE;
    Boolean			need_password = FALSE;
    Boolean			need_trust = FALSE;
    int 			result;
    CFDateRef			timestamp = NULL;

    if (supp->client == NULL) {
	return;
    }
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    if (supp->system_mode) {
	CFDictionarySetValue(dict, kEAPOLControlSystemMode, kCFBooleanTrue);
    }
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
		need_username = TRUE;
	    }
	    else {
		dictInsertRequiredProperties(dict, supp->eap.required_props);
		need_password 
		    = my_CFArrayContainsValue(supp->eap.required_props,
					      kEAPClientPropUserPassword);
		need_trust
		    = my_CFArrayContainsValue(supp->eap.required_props,
					      kEAPClientPropTLSUserTrustProceedCertificateChain);
	    }
	}
	dictInsertPublishedProperties(dict, supp->eap.published_props);
	dictInsertIdentityAttributes(dict, supp->identity_attributes);
    }
    timestamp = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
    CFDictionarySetValue(dict, kEAPOLControlTimestamp, timestamp);
    my_CFRelease(&timestamp);
    result = EAPOLClientReportStatus(supp->client, dict);
    if (result != 0) {
	my_log(LOG_NOTICE, "EAPLClientReportStatus failed: %s",
	       strerror(result));
    }
    my_CFRelease(&dict);
    if (supp->no_ui) {
	goto no_ui;
    }
    if (need_username || need_password) {
	if (supp->pw_prompt == NULL) {
	    CFStringRef password;
	    CFStringRef	username;

	    username = CFDictionaryGetValue(supp->config_dict,
					    kEAPClientPropUserName);
	    password = CFDictionaryGetValue(supp->config_dict,
					    kEAPClientPropUserPassword);
	    supp->pw_prompt 
		= UserPasswordDialogue_create(password_callback, supp,
					      NULL, NULL, 
					      username, password,
					      supp->one_time_password);
	}
    }
    if (need_trust) {
	if (supp->trust_prompt == NULL) {
	    CFStringRef	copy;
	    copy = CFCopyLocalizedString(CFSTR("802.1X Authentication"),
					 "The label");
	    supp->trust_prompt 
		= TrustDialogue_create(trust_callback, supp, NULL,
				       supp->eap.published_props,
				       copy != NULL 
				       ? copy 
				       : CFSTR("802.1X Authentication"));
	    my_CFRelease(&copy);
	}
    }
 no_ui:
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
	Supplicant_cancel_pending_events(supp);
	supp->state = kSupplicantStateHeld;
	supp->authenticated = FALSE;
	Supplicant_force_renew(supp);
	Supplicant_report_status(supp);
	supp->previous_identifier = BAD_IDENTIFIER;
	EAPAcceptTypesReset(&supp->eap_accept);
	if (supp->eap.module != NULL && supp->system_mode == FALSE
	    && supp->last_status == kEAPClientStatusFailed) {
	    clear_password(supp);
	}
	supp->last_status = kEAPClientStatusOK;
	supp->last_error = 0;
	free_last_packet(supp);
	eap_client_free(supp);
	UserPasswordDialogue_free(&supp->pw_prompt);
	TrustDialogue_free(&supp->trust_prompt);
	EAPOLSocket_enable_receive(supp->sock, 
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
    my_log(LOG_NOTICE, "%s START", 
	   EAPOLSocket_if_name(supp->sock, NULL));
    if (is_link_active(supp->store, supp->if_name_cf)) {
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
	supp->authenticated = FALSE;
	supp->no_authenticator = TRUE;
	Supplicant_report_status(supp);
	EAPOLSocket_enable_receive(supp->sock, 
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
	if (supp->logoff_sent == FALSE) {
	    EAPOLSocket_transmit(supp->sock,
				 kEAPOLPacketTypeLogoff,
				 NULL, 0, NULL, TRUE);
	    supp->logoff_sent = TRUE;
	    Supplicant_report_status(supp);
	    Supplicant_force_renew(supp);
	}
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

static char *
S_copy_password_from_keychain(bool use_system_keychain,
			      CFStringRef unique_id_str)
{
    SecKeychainRef 	keychain = NULL;
    char *		password = NULL;
    CFDataRef		password_data = NULL;
    int			password_length;
    OSStatus		status;

    if (use_system_keychain) {
	status = mySecKeychainCopySystemKeychain(&keychain);
	if (status != noErr) {
	    goto done;
	}
    }
    status = EAPSecKeychainPasswordItemCopy(keychain, unique_id_str,
					    &password_data);
    if (status != noErr) {
	my_log(LOG_NOTICE, "SecKeychainFindGenericPassword failed, %ld",
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
		password = S_copy_password_from_keychain(supp->system_mode, 
							 item_cf);
		if (password == NULL) {
		    my_log(LOG_NOTICE, 
			   "%s: failed to retrieve password from keychain",
			   EAPOLSocket_if_name(supp->sock, NULL));
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

    /* if no username specified, ask EAP types if they can come up with one */
    if (supp->username == NULL) {
	supp->username = eap_method_user_name(&supp->eap_accept,
					      supp->config_dict);
	if (supp->username != NULL) {
	    supp->username_length = strlen(supp->username);
	}
    }
    return (change);
}

bool
Supplicant_attached(SupplicantRef supp)
{
    return (supp->client != NULL);
}

static bool
get_control_command(CFNumberRef command_cf, EAPOLClientControlCommand * command)
{
    if (isA_CFNumber(command_cf) == NULL
	|| CFNumberGetValue(command_cf, kCFNumberIntType,
			    command) == FALSE) {
	return (FALSE);
    }
    return (TRUE);
}

static void 
dict_merge_key_value(const void * key, const void * value, void * context)
{
    CFMutableDictionaryRef	new_dict = (CFMutableDictionaryRef)context;

    /* add the (key, value) if the key doesn't already exist */
    CFDictionaryAddValue(new_dict, key, value);
    return;
}

static CFDictionaryRef
myCFDictionaryCreateMerge(CFDictionaryRef base, CFDictionaryRef additional)
{
    CFDictionaryRef		dict;
    CFMutableDictionaryRef	new_dict;

    if (base == NULL && additional == NULL) {
	return (NULL);
    }
    if (base == NULL) {
	return (CFDictionaryCreateCopy(NULL, additional));
    }
    else if (additional == NULL) {
	return (CFDictionaryCreateCopy(NULL, base));
    }
    new_dict = CFDictionaryCreateMutableCopy(NULL, 0, base);
    CFDictionaryApplyFunction(additional, dict_merge_key_value, 
			      new_dict);
    dict = CFDictionaryCreateCopy(NULL, new_dict);
    my_CFRelease(&new_dict);
    return (dict);
}

static CFDictionaryRef
create_config_dict(SupplicantRef supp, CFDictionaryRef new_config)
{
    CFDictionaryRef	dict;
    CFDictionaryRef	tmp_dict;

    tmp_dict = myCFDictionaryCreateMerge(new_config, 
					 supp->ui_config_dict);
    dict = myCFDictionaryCreateMerge(tmp_dict, supp->default_dict);
    my_CFRelease(&tmp_dict);
    return (dict);
}

static bool
cfstring_is_empty(CFStringRef str)
{
    if (str == NULL) {
	return (FALSE);
    }
    return (isA_CFString(str) == NULL || CFStringGetLength(str) == 0);
}

static CFDictionaryRef
clean_user_password(CFDictionaryRef dict)
{
    CFMutableDictionaryRef	new_dict;
    bool			remove_password = FALSE;
    bool			remove_user = FALSE;

    if (dict == NULL) {
	return (NULL);
    }
    remove_user 
	= cfstring_is_empty(CFDictionaryGetValue(dict, 
						 kEAPClientPropUserName));
    remove_password 
	= cfstring_is_empty(CFDictionaryGetValue(dict,
						 kEAPClientPropUserPassword));
    if (remove_user == FALSE && remove_password == FALSE) {
	return (CFRetain(dict));
    }
    new_dict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
    if (remove_user) {
	CFDictionaryRemoveValue(new_dict, kEAPClientPropUserName);
    }
    if (remove_password) {
	CFDictionaryRemoveValue(new_dict, kEAPClientPropUserPassword);
    }
    dict = CFDictionaryCreateCopy(NULL, new_dict);
    my_CFRelease(&new_dict);
    return (dict);
}

bool
Supplicant_update_configuration(SupplicantRef supp, CFDictionaryRef config_dict)
{
    bool		change = FALSE;
    CFStringRef		config_id;
    CFDictionaryRef	eap_config = NULL;

    /* keep a copy of the original around */
    my_CFRelease(&supp->orig_config_dict);
    supp->orig_config_dict = CFDictionaryCreateCopy(NULL, config_dict);

    /* get the EAP configuration dictionary */
    if (config_dict != NULL) {
	eap_config = CFDictionaryGetValue(config_dict,
					  kEAPOLControlEAPClientConfiguration);
    }
    if (isA_CFDictionary(eap_config) == NULL) {
	eap_config = config_dict;
    }
    supp->generation++;
    my_CFRelease(&supp->config_dict);
    eap_config = clean_user_password(eap_config);
    supp->config_dict = create_config_dict(supp, eap_config);
    my_CFRelease(&eap_config);

    /* get the configuration identifier */
    my_CFRelease(&supp->config_id);
    if (config_dict != NULL) {
	config_id = CFDictionaryGetValue(config_dict,
					 kEAPOLControlUniqueIdentifier);
	if (isA_CFString(config_id) != NULL) {
	    supp->config_id = CFRetain(config_id);
	}
	if (CFDictionaryGetValue(config_dict, 
				 kEAPOLControlLogLevel) != NULL
	    || CFDictionaryGetValue(config_dict, CFSTR("_debug")) != NULL) {
	    Supplicant_set_debug(supp, TRUE);
	}
	else {
	    Supplicant_set_debug(supp, FALSE);
	}
    }

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
    return (change);
}

static void 
eapolclient_notification(EAPOLClientRef client, Boolean server_died,
			 void * context)
{
    bool			change;
    EAPOLClientControlCommand	command;
    CFNumberRef			command_cf;
    CFDictionaryRef		config_dict = NULL;
    CFDictionaryRef		control_dict = NULL;
    int				result;
    SupplicantRef		supp = (SupplicantRef)context;

    if (server_died) {
	my_log(LOG_NOTICE, "Supplicant(%s): server died",
	       EAPOLSocket_if_name(supp->sock, NULL));
	goto stop;
    }
    result = EAPOLClientGetConfig(client, &control_dict);
    if (result != 0) {
	my_log(LOG_NOTICE, "%s: EAPOLClientGetConfig failed, %s",
	       EAPOLSocket_if_name(supp->sock, NULL), strerror(result));
	goto stop;
    }
    if (control_dict == NULL) {
	my_log(LOG_NOTICE, "%s: EAPOLClientGetConfig returned NULL control",
	       EAPOLSocket_if_name(supp->sock, NULL));
	goto stop;
    }
    command_cf = CFDictionaryGetValue(control_dict,
				      kEAPOLClientControlCommand);
    if (get_control_command(command_cf, &command) == FALSE) {
	my_log(LOG_NOTICE, "%s: invalid/missing command",
	       EAPOLSocket_if_name(supp->sock, NULL));
	goto stop;
    }
    switch (command) {
    case kEAPOLClientControlCommandRetry:
	if (supp->state != kSupplicantStateInactive) {
	    Supplicant_connecting(supp, kSupplicantEventStart, NULL);
	}
	break;
    case kEAPOLClientControlCommandRun:
	config_dict = CFDictionaryGetValue(control_dict,
					   kEAPOLClientControlConfiguration);
	if (config_dict == NULL) {
	    goto stop;
	}
	change = Supplicant_update_configuration(supp, config_dict);
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
		    supp->last_rx_packet.logged = TRUE;
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
    default:
	goto stop;
    }					  
    my_CFRelease(&control_dict);
    if (supp->debug) {
	fflush(stdout);
	fflush(stderr);
    }
    return;

 stop:
    Supplicant_stop(supp);
    return;
}

static bool
is_link_active(SCDynamicStoreRef store, CFStringRef if_name_cf)
{
    bool		active = TRUE;
    CFDictionaryRef	dict;
    CFStringRef		key;

    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							if_name_cf,
							kSCEntNetLink);
    dict = SCDynamicStoreCopyValue(store, key);
    if (isA_CFDictionary(dict) != NULL) {
	CFBooleanRef active_cf;

	active_cf = CFDictionaryGetValue(dict, kSCPropNetLinkActive);
	if (isA_CFBoolean(active_cf) != NULL) {
	    active = CFBooleanGetValue(active_cf);
	}
    }
    my_CFRelease(&dict);
    my_CFRelease(&key);
    return (active);
}

static void
link_status_changed(SupplicantRef supp)
{
    bool 		active;
    struct timeval	t = {0, 0};

    active = is_link_active(supp->store, supp->if_name_cf);
    if (active) {
	/*
	 * wait awhile before entering connecting state to avoid
	 * disrupting an existing conversation
	 */
	t.tv_sec = S_link_active_period_secs;
	Timer_set_relative(supp->timer, t,
			   (void *)Supplicant_connecting,
			   (void *)supp,
			   (void *)kSupplicantEventStart,
			   NULL);
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

static void
link_changed(SCDynamicStoreRef store, CFArrayRef changes, void * arg)
{
    int			count = 0;
    Boolean		link_changed = FALSE;
    SupplicantRef	supp = (SupplicantRef)arg;

    if (changes != NULL) {
	count = CFArrayGetCount(changes);
    }
    if (count == 0) {
	return;
    }
    if (EAPOLSocket_is_wireless(supp->sock) == FALSE) {
	link_changed = TRUE;
    }
    else {
	Boolean		airport_changed = FALSE;
	int		i;

	for (i = 0; i < count; i++) {
	    CFStringRef	key = CFArrayGetValueAtIndex(changes, i);
	    
	    if (CFStringHasSuffix(key, kSCEntNetAirPort)) {
		airport_changed = TRUE;
	    }
	    else {
		link_changed = TRUE;
	    }
	}
	if (airport_changed) {
	    Boolean	bssid_changed;
	    bssid_changed = EAPOLSocket_link_update(supp->sock);
	    if (bssid_changed) {
		link_changed = TRUE;
	    }
	}
	else {
	    EAPOLSocket_link_update(supp->sock);
	}
    }
    if (link_changed) {
	link_status_changed(supp);
    }
    return;
}

static SCDynamicStoreRef
link_event_register(CFStringRef if_name_cf, Boolean is_wireless,
		    SCDynamicStoreCallBack func, void * arg)
{
    CFMutableArrayRef		keys = NULL;
    CFStringRef			key;
    CFRunLoopSourceRef		rls;
    SCDynamicStoreRef		store;
    SCDynamicStoreContext	context;

    bzero(&context, sizeof(context));
    context.info = arg;
    store = SCDynamicStoreCreate(NULL, CFSTR("802.1x Supplicant"), 
				 func, &context);
    if (store == NULL) {
	my_log(LOG_NOTICE, "SCDynamicStoreCreate() failed, %s",
	       SCErrorString(SCError()));
	return (NULL);
    }
    keys = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							kSCDynamicStoreDomainState,
							if_name_cf,
							kSCEntNetLink);
    CFArrayAppendValue(keys, key);
    my_CFRelease(&key);
    if (is_wireless) {
	key = SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							    kSCDynamicStoreDomainState,
							    if_name_cf,
							    kSCEntNetAirPort);
	CFArrayAppendValue(keys, key);
	my_CFRelease(&key);
    }
    SCDynamicStoreSetNotificationKeys(store, keys, NULL);
    my_CFRelease(&keys);

    rls = SCDynamicStoreCreateRunLoopSource(NULL, store, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    my_CFRelease(&rls);
    return (store);
}

static CFDictionaryRef
copy_default_dict(void)
{
    CFBundleRef		bundle;
    uint8_t		config_file[MAXPATHLEN];
    CFDictionaryRef	dict = NULL;
    Boolean		ok;
    CFURLRef		url;

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
    dict = my_CFPropertyListCreateFromFile((char *)config_file);
    if (dict != NULL) {
	if (isA_CFDictionary(dict) == NULL || CFDictionaryGetCount(dict) == 0) {
	    my_CFRelease(&dict);
	}
    }
 done:
    return (dict);

}

SupplicantRef
Supplicant_create(int fd, const struct sockaddr_dl * link, bool system_mode)
{
    EAPOLClientRef		client = NULL;
    CFDictionaryRef		control_dict = NULL;
    CFDictionaryRef		config_dict = NULL;
    CFDictionaryRef		default_dict = NULL;
    CFStringRef			if_name_cf = NULL;
    int				result;
    SCDynamicStoreRef		store = NULL;
    EAPOLSocket * 		sock;
    SupplicantRef		supp = NULL;
    Timer *			timer = NULL;

    sock = EAPOLSocket_create(fd, link);
    if (sock == NULL) {
	my_log(LOG_NOTICE, "Supplicant_create: EAPOLSocket_create failed");
	goto failed;
    }
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
    supp->system_mode = system_mode;
    supp->timer = timer;
    supp->sock = sock;
    if_name_cf 
	= CFStringCreateWithCString(NULL, 
				    EAPOLSocket_if_name(supp->sock, NULL),
				    kCFStringEncodingASCII);
    supp->if_name_cf = if_name_cf;
    store = link_event_register(supp->if_name_cf,
				EAPOLSocket_is_wireless(supp->sock),
				link_changed, supp);
    supp->store = store;
    client = EAPOLClientAttach(EAPOLSocket_if_name(supp->sock, NULL), 
			       eapolclient_notification, supp,
			       &control_dict, &result);
    supp->client = client;
    default_dict = copy_default_dict();
    supp->default_dict = default_dict;
    if (client == NULL) {
	my_log(LOG_NOTICE, "EAPOLClientAttach(%s) failed: %s",
	       EAPOLSocket_if_name(supp->sock, NULL), strerror(result));
    }
    else {
	EAPOLClientControlCommand	command;
	CFNumberRef			command_cf;

	if (control_dict == NULL) {
	    my_log(LOG_NOTICE, "%s: control dictionary missing",
		   EAPOLSocket_if_name(supp->sock, NULL));
	    goto failed;
	}
	command_cf = CFDictionaryGetValue(control_dict,
					  kEAPOLClientControlCommand);
	if (get_control_command(command_cf, &command) == FALSE) {
	    goto failed;
	}
	if (command != kEAPOLClientControlCommandRun) {
	    my_log(LOG_NOTICE, "%s: received stop command",
		   EAPOLSocket_if_name(supp->sock, NULL));
	    goto failed;
	}
	config_dict = CFDictionaryGetValue(control_dict,
					   kEAPOLClientControlConfiguration);
	if (config_dict == NULL) {
	    my_log(LOG_NOTICE, "%s: configuration empty - exiting",
		   EAPOLSocket_if_name(supp->sock, NULL));
	    goto failed;
	}
	Supplicant_update_configuration(supp, config_dict);
    }
    my_CFRelease(&control_dict);
    return (supp);

 failed:
    my_CFRelease(&if_name_cf);
    if (supp != NULL) {
	free(supp);
    }
    my_CFRelease(&store);
    EAPOLClientDetach(&client);
    Timer_free(&timer);
    EAPOLSocket_free(&sock);
    my_CFRelease(&control_dict);
    my_CFRelease(&default_dict);
    return (NULL);
}

void
Supplicant_free(SupplicantRef * supp_p)
{
    SupplicantRef supp;

    if (supp_p == NULL) {
	return;
    }
    supp = *supp_p;

    if (supp) {
	UserPasswordDialogue_free(&supp->pw_prompt);
	TrustDialogue_free(&supp->trust_prompt);
	my_CFRelease(&supp->store);
	Timer_free(&supp->timer);
	EAPOLSocket_free(&supp->sock);
	EAPOLClientDetach(&supp->client);
	my_CFRelease(&supp->orig_config_dict);
	my_CFRelease(&supp->config_dict);
	my_CFRelease(&supp->ui_config_dict);
	my_CFRelease(&supp->config_id);
	free_last_packet(supp);
	eap_client_free(supp);
	my_CFRelease(&supp->if_name_cf);
	my_CFRelease(&supp->default_dict);
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
    return;
}

void
Supplicant_set_no_ui(SupplicantRef supp)
{
    supp->no_ui = TRUE;
    return;
}
