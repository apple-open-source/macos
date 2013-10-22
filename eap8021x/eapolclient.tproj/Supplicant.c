/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
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
#include <EAP8021X/EAPOLControlTypesPrivate.h>
#include <EAP8021X/EAPOLControl.h>
#include <EAP8021X/SupplicantTypes.h>
#include <EAP8021X/EAPKeychainUtil.h>
#include <TargetConditionals.h>
#if ! TARGET_OS_EMBEDDED
#include <EAP8021X/EAPOLClientConfiguration.h>
#include <EAP8021X/EAPOLClientConfigurationPrivate.h>
#include <notify.h>
#endif /* ! TARGET_OS_EMBEDDED */
#include <EAP8021X/EAPCertificateUtil.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFBundle.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCValidation.h>
#include <SystemConfiguration/SCPrivate.h>

#if ! TARGET_OS_EMBEDDED
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include "Dialogue.h"
#include <OpenDirectory/OpenDirectoryPriv.h>
#include <opendirectory/adsupport.h>
#endif /* ! TARGET_OS_EMBEDDED */
#include <Security/SecureTransport.h>
#include "symbol_scope.h"
#include "Supplicant.h"
#include "Timer.h"
#include "EAPOLSocket.h"
#include "printdata.h"
#include "mylog.h"
#include "myCFUtil.h"
#include "ClientControlInterface.h"

#define START_PERIOD_SECS		5
#define START_ATTEMPTS_MAX		3
#define AUTH_PERIOD_SECS		5
#define AUTH_ATTEMPTS_MAX		4
#define HELD_PERIOD_SECS		60
#define LINK_ACTIVE_PERIOD_SECS		5
#define LINK_INACTIVE_PERIOD_SECS	1

#define BAD_IDENTIFIER		(-1)

#define kSupplicant		CFSTR("Supplicant")
#define kStartPeriodSeconds	CFSTR("StartPeriodSeconds")
#define kStartAttemptsMax	CFSTR("StartAttemptsMax")
#define kAuthPeriodSeconds	CFSTR("AuthPeriodSeconds")
#define kAuthAttemptsMax	CFSTR("AuthAttemptsMax")
#define kHeldPeriodSeconds	CFSTR("HeldPeriodSeconds")

static int	S_start_period_secs = START_PERIOD_SECS;
static int	S_start_attempts_max = START_ATTEMPTS_MAX;
static int	S_auth_period_secs = AUTH_PERIOD_SECS;
static int	S_auth_attempts_max = AUTH_ATTEMPTS_MAX;
static int	S_held_period_secs = HELD_PERIOD_SECS;

static int	S_link_active_period_secs = LINK_ACTIVE_PERIOD_SECS;
static int	S_link_inactive_period_secs = LINK_INACTIVE_PERIOD_SECS;

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
    bool		use_outer_identity;
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
    char *			outer_identity;
    int				outer_identity_length;
    char *			username;
    int				username_length;
    char *			password;
    int				password_length;
    bool			username_derived;
    bool			one_time_password;
    bool			ignore_password;
    bool			ignore_username;
    bool			ignore_sec_identity;
    bool			remember_information;

    SecIdentityRef		sec_identity;

    EAPAcceptTypes		eap_accept;

    CFArrayRef			identity_attributes;

#if ! TARGET_OS_EMBEDDED
    EAPOLClientItemIDRef	itemID;
    EAPOLClientConfigurationRef	eapolcfg;
    AlertDialogueRef		alert_prompt;
    CredentialsDialogueRef	cred_prompt;
    TrustDialogueRef		trust_prompt;
    long			credentials_access_time;
    CFStringRef			manager_name;
    struct {
	CFMachPortRef		mp;
	int			token;
    } config_change;
#endif /* ! TARGET_OS_EMBEDDED */

    int				start_count;
    int				auth_attempts_count;

    bool			no_authenticator;

    struct eap_client		eap;

    EAPOLSocketReceiveData	last_rx_packet;
    EAPClientStatus		last_status;
    EAPClientDomainSpecificError last_error;

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

#define INDEX_NONE		(-1)

static Boolean
myCFDictionaryGetBooleanValue(CFDictionaryRef properties, CFStringRef propname,
			      Boolean def_value);
static bool
S_set_credentials(SupplicantRef supp);

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
static CFStringRef
copy_cleaned_config_dict(CFDictionaryRef d);

static void
log_eap_notification(SupplicantState state, EAPRequestPacketRef req_p)
{
    int			len;
    CFStringRef		str;

    len = EAPPacketGetLength((EAPPacketRef)req_p) - sizeof(*req_p);
    if (len > 0) {
	str =  CFStringCreateWithBytes(NULL, req_p->type_data, len,
				       kCFStringEncodingUTF8, FALSE);
    }
    else {
	str = NULL;
    }
    EAPLOG(LOG_NOTICE, "%s: Notification '%@'",
	   SupplicantStateString(state),
	   (str != NULL) ? str : CFSTR(""));
    my_CFRelease(&str);
    return;
}

/**
 ** Utility routines
 **/
static bool
S_array_contains_int(CFArrayRef array, int val)
{
    int		count;
    int		i;

    if (array == NULL) {
	goto done;
    }

    count = CFArrayGetCount(array);
    for (i = 0; i < count; i++) {
	CFNumberRef	num = CFArrayGetValueAtIndex(array, i);
	int		num_val;

	if (isA_CFNumber(num) == NULL) {
	    continue;
	}
	if (CFNumberGetValue(num, kCFNumberIntType, &num_val)
	    && num_val == val) {
	    return (TRUE);
	}
    }

 done:
    return (FALSE);
}

static char *
S_identity_copy_name(SecIdentityRef sec_identity)
{
    SecCertificateRef		cert;
    char *			name = NULL;
    OSStatus			status;

    status = SecIdentityCopyCertificate(sec_identity, &cert);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE,
		  "EAPSecIdentityHandleCreateSecIdentity failed, %d",
		  status);
    }
    else {
	CFStringRef		name_cf;

	name_cf = EAPSecCertificateCopyUserNameString(cert);
	CFRelease(cert);
	if (name_cf != NULL) {
	    name = my_CFStringToCString(name_cf, kCFStringEncodingUTF8);
	    CFRelease(name_cf);
	}
    }
    return (name);
}

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

	ident = EAPOLSocketGetSSID(supp->sock);
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
	EAPLOG_FL(LOG_NOTICE, "already initialized");
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
    supp->eap.plugin_data.sec_identity = supp->sec_identity;
    *((bool *)&supp->eap.plugin_data.log_enabled) 
	= ((eapolclient_log_flags() & kLogFlagDisableInnerDetails) == 0);
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
    *((bool *)&supp->eap.plugin_data.log_enabled)
	= ((eapolclient_log_flags() & kLogFlagDisableInnerDetails) == 0);
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
	EAPLOG(LOG_NOTICE, "error string '%s'", err);
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
EAPAcceptTypesInit(EAPAcceptTypesRef accept, CFArrayRef accept_types)
{
    int			i;
    int			count;
    int			tunneled_count;
    CFNumberRef		type_cf;
    int			type_i;
    int *		types;
    int			types_count;

    EAPAcceptTypesFree(accept);
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
	int		j;

	type_cf = CFArrayGetValueAtIndex(accept_types, i);
	if (isA_CFNumber(type_cf) == NULL) {
	    EAPLOG(LOG_NOTICE, 
		   "AcceptEAPTypes[%d] contains invalid type, ignoring", i);
	    continue;
	}
	if (CFNumberGetValue(type_cf, kCFNumberIntType, &type_i) == FALSE) {
	    EAPLOG(LOG_NOTICE, 
		   "AcceptEAPTypes[%d] contains invalid number, ignoring", i);
	    continue;
	}
	if (EAPClientModuleLookup(type_i) == NULL) {
	    EAPLOG(LOG_NOTICE, 
		   "AcceptEAPTypes[%d] specifies unsupported type %d, ignoring",
		   i, type_i);
	    continue;
	}
	for (j = 0; j < types_count; j++) {
	    if (types[j] == type_i) {
		EAPLOG(LOG_NOTICE,
		       "AcceptEAPTypes[%d] %s (%d) already specified at [%d], "
		       "ignoring", i, EAPTypeStr(type_i), type_i, j);
		continue;
	    }
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
	    accept->use_outer_identity = TRUE;
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

static int
EAPAcceptTypesIndexOfType(EAPAcceptTypesRef accept, EAPType type)
{
    int			i;

    if (accept->types == NULL) {
	return (INDEX_NONE);
    }
    for (i = 0; i < accept->count; i++) {
	if (accept->types[i] == type) {
	    return (i);
	}
    }
    return (INDEX_NONE);
}

static bool
EAPAcceptTypesIsSupportedType(EAPAcceptTypesRef accept, EAPType type)
{
    return (EAPAcceptTypesIndexOfType(accept, type) != INDEX_NONE);
}

static bool
EAPAcceptTypesUseOuterIdentity(EAPAcceptTypesRef accept)
{
    return (accept->use_outer_identity);
}

static void
EAPAcceptTypesRemoveTypeAtIndex(EAPAcceptTypesRef accept, int type_index)
{
    int		i;

    if (type_index >= accept->count || type_index < 0) {
	/* invalid arg */
	return;
    }
    for (i = type_index; i < (accept->count - 1); i++) {
	accept->types[i] = accept->types[i + 1];
    }
    accept->count--;
    return;
}

#if ! TARGET_OS_EMBEDDED

static boolean_t
EAPAcceptTypesRequirePassword(EAPAcceptTypesRef accept)
{
    int			i;

    if (accept->types == NULL) {
	return (FALSE);
    }
    for (i = 0; i < accept->count; i++) {
	switch (accept->types[i]) {
	case kEAPTypeTLS:
	case kEAPTypeEAPSIM:
	case kEAPTypeEAPAKA:
	    break;
	default:
	    return (TRUE);
	}
    }
    return (FALSE);
}

#endif /* ! TARGET_OS_EMBEDDED */

/**
 ** Supplicant routines
 **/

#if ! TARGET_OS_EMBEDDED

#define CREDENTIALS_ACCESS_DELAY_SECS		2

static void
S_set_credentials_access_time(SupplicantRef supp)
{
    supp->credentials_access_time = Timer_current_secs();
    return;
}

static void
S_check_for_updated_credentials(SupplicantRef supp)
{
    long	current_time;
    long	delta;
    
    if (EAPOLSocketGetMode(supp->sock) != kEAPOLControlModeSystem) {
	/* don't bother unless it's System mode */
	return;
    }
    current_time = Timer_current_secs();
    delta = current_time - supp->credentials_access_time;
    if (delta < 0
	|| delta >= CREDENTIALS_ACCESS_DELAY_SECS) {
	eapolclient_log(kLogFlagBasic,
			"Re-reading credentials");
	(void)S_set_credentials(supp);
    }
    return;
}

#endif /* ! TARGET_OS_EMBEDDED */

static void
clear_password(SupplicantRef supp)
{
    supp->ignore_password = TRUE;

    /* clear the password */
    if (supp->password != NULL) {
	free(supp->password);
	supp->password = NULL;
    }
    supp->password_length = 0;
    return;
}

static void
clear_username(SupplicantRef supp)
{
    supp->ignore_username = TRUE;

    if (supp->username != NULL) {
	free(supp->username);
	supp->username = NULL;
    }
    supp->username_length = 0;
    supp->username_derived = FALSE;
    return;
}

static void
clear_sec_identity(SupplicantRef supp)
{
    supp->ignore_sec_identity = TRUE;
    my_CFRelease(&supp->sec_identity);
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
					   kCFStringEncodingUTF8, FALSE);
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
			   CFMutableStringRef debug_str)
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
    if (debug_str != NULL) {
	STRING_APPEND(debug_str, "Signature: ");
	print_bytes_cfstr(debug_str, signature, sizeof(signature));
	STRING_APPEND(debug_str, " is %s", valid ? "valid" : "INVALID");
    }
    free(packet_copy);
    return (valid);
}

static void
process_key(SupplicantRef supp, EAPOLPacketRef eapol_p)
{
    int				body_length;
    CFMutableStringRef		debug_str = NULL;
    EAPOLKeyDescriptorRef	descr_p;
    bool			is_valid;
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
	EAPLOG_FL(LOG_NOTICE, "session key is NULL");
	return;
    }
    server_key = eap_client_server_key(supp, &server_key_length);
    if (server_key == NULL) {
	EAPLOG_FL(LOG_NOTICE, "server key is NULL");
	return;
    }
    body_length = EAPOLPacketGetLength(eapol_p);
    packet_length = sizeof(EAPOLPacket) + body_length;
    if (eapolclient_should_log(kLogFlagPacketDetails)) {
	debug_str = CFStringCreateMutable(NULL, 0);
    }
    is_valid = eapol_key_verify_signature(eapol_p, packet_length,
					  server_key, server_key_length,
					  debug_str);
    if (debug_str != NULL) {
	EAPLOG(-LOG_NOTICE, "%@", debug_str);
	CFRelease(debug_str);
    }
    if (is_valid == FALSE) {
	EAPLOG_FL(LOG_NOTICE,
		  "key signature mismatch, ignoring");
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
			    "key process: RC4 decrypt failed %d",
			    c_status);
	}
	else {
	    eapolclient_log(kLogFlagBasic,
			    "set %s key length %d using descriptor",
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
			"set %s key length %d using session key",
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
    (void)EAPOLSocketSetWPAKey(supp->sock, NULL, 0, NULL, 0);
    supp->pmk_set = FALSE;
    return;
}

static void
set_wpa_key_info(SupplicantRef supp)
{
    const uint8_t *	server_key;
    int			server_key_length;
    const uint8_t *	session_key;
    int			session_key_length;

    if (supp->pmk_set) {
	/* already set */
	return;
    }
    session_key = eap_client_session_key(supp, &session_key_length);
    server_key = eap_client_server_key(supp, &server_key_length);
    if (session_key != NULL
	&& server_key != NULL
	&& EAPOLSocketSetWPAKey(supp->sock, 
				session_key, session_key_length,
				server_key, server_key_length)) {
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
	supp->auth_attempts_count = 0;
	supp->state = kSupplicantStateAuthenticated;
	free_last_packet(supp);
	if (supp->one_time_password == FALSE) {
	    CFStringRef	new_password;
	    
	    new_password
		= CFDictionaryGetValue(supp->config_dict,
				       kEAPClientPropNewPassword);
	    if (isA_CFString(new_password) != NULL) {
		if (supp->password != NULL) {
		    free(supp->password);
		}
		supp->password = my_CFStringToCString(new_password,
						      kCFStringEncodingUTF8);
		if (supp->password != NULL) {
		    supp->password_length = strlen(supp->password);
		}
	    }
	}
#if ! TARGET_OS_EMBEDDED
	AlertDialogue_free(&supp->alert_prompt);
	CredentialsDialogue_free(&supp->cred_prompt);
	TrustDialogue_free(&supp->trust_prompt);
	if (supp->remember_information && supp->itemID != NULL) {
	    CFDataRef	name_data = NULL;
	    CFDataRef	password_data = NULL;

	    if (supp->username != NULL && supp->username_derived == FALSE) {
		name_data
		    = CFDataCreate(NULL, (const UInt8 *)supp->username,
				   supp->username_length);
	    }
	    if (supp->eap.last_type == kEAPTypeTLS
		&& supp->sec_identity != NULL) {
		if (EAPOLClientItemIDSetIdentity(supp->itemID,
						 kEAPOLClientDomainUser,
						 supp->sec_identity)
		    == FALSE) {
		    EAPLOG_FL(LOG_NOTICE, "Failed to save identity selection");
		}
		else {
		    OSStatus		status;

		    eapolclient_log(kLogFlagBasic,
				    "Identity selection saved");
		    status = EAPOLClientSetACLForIdentity(supp->sec_identity);
		    if (status != noErr) {
			EAPLOG_FL(LOG_NOTICE,
				  "Failed to set ACL for identity, %d",
				  (int)status);
		    }
		}
	    }
	    else if (supp->password != NULL) {
		password_data 
		    = CFDataCreate(NULL, (const UInt8 *)supp->password,
				   supp->password_length);
	    }
	    if (name_data != NULL || password_data != NULL) {
		if (EAPOLClientItemIDSetPasswordItem(supp->itemID,
						     kEAPOLClientDomainUser,
						     name_data, password_data)
		    == FALSE) {
		    EAPLOG_FL(LOG_NOTICE, "Failed to save password");
		}
		else {
		    eapolclient_log(kLogFlagBasic, "Password saved");
		}
	    }
	    my_CFRelease(&name_data);
	    my_CFRelease(&password_data);
	}
	supp->remember_information = FALSE;
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
		    log_eap_notification(supp->state, req_p);
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
	    if (supp->start_count == S_start_attempts_max) {
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
    supp->username_derived = TRUE;

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
    if (supp->outer_identity != NULL) {
	identity = supp->outer_identity;
	length = supp->outer_identity_length;
    }
    else {
	identity = supp->username;
	length = supp->username_length;
    }

    eapolclient_log(kLogFlagBasic,
		    "EAP Response Identity %.*s",
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
	EAPLOG_FL(LOG_NOTICE, "EAPOLSocketTransmit failed");
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
	supp->auth_attempts_count++;
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
			    "EAP Request Identity");
	    supp->previous_identifier = req_p->identifier;
#if ! TARGET_OS_EMBEDDED	    
	    S_check_for_updated_credentials(supp);
#endif /* ! TARGET_OS_EMBEDDED */

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
		EAPLOG(LOG_NOTICE,
		       "Acquired: cannot prompt for missing user name");
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
		EAPLOG_FL(LOG_NOTICE, 
			  "internal error: recursion avoided from state %s",
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
	if (supp->auth_attempts_count >= S_auth_attempts_max) {
	    supp->auth_attempts_count = 0;
	    supp->last_status = kEAPClientStatusAuthenticationStalled;
	    Supplicant_held(supp, kSupplicantEventStart, NULL);
	}
	else {
	    Supplicant_connecting(supp, kSupplicantEventStart, NULL);
	}
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
		    "EAP Response Notification");

    /* transmit a response/Notification */
    (void)EAPPacketCreate(&notif, sizeof(notif), 
			  kEAPCodeResponse, identifier,
			  kEAPTypeNotification, NULL, 0, &size);
    if (EAPOLSocketTransmit(supp->sock,
			    kEAPOLPacketTypeEAPPacket,
			    &notif, sizeof(notif)) < 0) {
	EAPLOG_FL(LOG_NOTICE, "EAPOLSocketTransmit failed");
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
	EAPLOG_FL(LOG_NOTICE, "EAPOLSocketTransmit failed");
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
				    "EAP Request: EAP type %d not enabled",
				    req_p->type);
		    supp->last_status = kEAPClientStatusProtocolNotSupported;
		    Supplicant_held(supp, kSupplicantEventStart, NULL);
		    return;
		}
		eapolclient_log(kLogFlagBasic,
				"EAP Request: NAK'ing EAP type %d with %d",
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
		    EAPLOG(LOG_NOTICE,
			   "EAP Request: EAP type %d"
			   " init failed, %d",
			   req_p->type, supp->last_status);
		    Supplicant_held(supp, kSupplicantEventStart, NULL);
		    return;
		}
		save_last_packet(supp, rx);
		Supplicant_report_status(supp);
		return;
	    }
	    eapolclient_log(kLogFlagBasic,
			    "EAP Request: EAP type %d accepted",
			    req_p->type);
	    Supplicant_report_status(supp);
	}
	else {
	    eapolclient_log(kLogFlagBasic,
			    "EAP Request: EAP type %d",
			    req_p->type);
	}
	break;
    case kEAPCodeResponse:
	if (req_p->type != eap_client_type(supp)) {
	    /* this should not happen, but if it does, ignore the packet */
	    return;
	}
	eapolclient_log(kLogFlagBasic,
			"EAP Response: EAP type %d", req_p->type);
	break;
    case kEAPCodeFailure:
	eapolclient_log(kLogFlagBasic, "EAP Failure");
	if (supp->eap.module == NULL) {
	    supp->last_status = kEAPClientStatusFailed;
	    Supplicant_held(supp, kSupplicantEventStart, NULL);
	    return;
	}
	break;
    case kEAPCodeSuccess:
	eapolclient_log(kLogFlagBasic, "EAP Success");
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
	    EAPLOG_FL(LOG_NOTICE, "EAPOLSocketTransmit %d failed",
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
		EAPLOG(LOG_NOTICE,
		       "Authenticating: can't prompt for missing properties %@",
		       supp->eap.required_props);
		supp->last_status = kEAPClientStatusUserInputNotPossible;
		Supplicant_held(supp, kSupplicantEventStart, NULL);
		return;
	    }
	    EAPLOG(LOG_DEBUG, 
		   "Authenticating: user input required for properties %@",
		   supp->eap.required_props);
	}
	Supplicant_report_status(supp);

	/* try to set the session key, if it is available */
	if (EAPOLSocketIsWireless(supp->sock)) {
	    set_wpa_key_info(supp);
	}
	break;
    case kEAPClientStateSuccess:
	/* authentication method succeeded */
	EAPLOG(LOG_NOTICE,
	       "%s %s: successfully authenticated",
	       EAPOLSocketIfName(supp->sock, NULL),
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
	EAPLOG(LOG_NOTICE,
	       "%s %s: authentication failed with status %d",
	       EAPOLSocketIfName(supp->sock, NULL),
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
		    EAPLOG_FL(LOG_NOTICE,
			      "internal error: recursion avoided from state %s",
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
		log_eap_notification(supp->state, req_p);
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

PRIVATE_EXTERN void
Supplicant_stop(SupplicantRef supp)
{
    eap_client_free(supp);
    Supplicant_logoff(supp, kSupplicantEventStart, NULL);
    Supplicant_free(&supp);
    return;
}

static void
user_supplied_data(SupplicantRef supp)
{
    eapolclient_log(kLogFlagBasic, "user_supplied_data");
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
	EAPLOG(LOG_NOTICE, "%s: user cancelled", 
	       EAPOLSocketIfName(supp->sock, NULL));
	EAPOLSocketStopClient(supp->sock);
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
    Supplicant_update_configuration(supp, config_dict, NULL);
    my_CFRelease(&config_dict);
    user_supplied_data(supp);

 done:
    my_CFRelease(&trust_info);
    return;
}

static void
credentials_callback(const void * arg1, const void * arg2, 
		     CredentialsDialogueResponseRef response)
{
    CFDictionaryRef		config_dict = NULL;
    SupplicantRef		supp = (SupplicantRef)arg1;

    CredentialsDialogue_free(&supp->cred_prompt);
    if (response->user_cancelled) {
	EAPLOG(LOG_NOTICE, "%s: user cancelled", 
	       EAPOLSocketIfName(supp->sock, NULL));
	EAPOLSocketStopClient(supp->sock);
	return;
    }
    if (supp->last_status != kEAPClientStatusUserInputRequired) {
	return;
    }
    create_ui_config_dict(supp);
    if (response->username != NULL) {
	CFDictionarySetValue(supp->ui_config_dict, kEAPClientPropUserName,
			     response->username);
	supp->ignore_username = FALSE;
    }
    if (response->password != NULL) {
	CFDictionarySetValue(supp->ui_config_dict, 
			     kEAPClientPropUserPassword,
			     response->password);
	supp->ignore_password = FALSE;
    }
    if (response->new_password != NULL) {
	CFDictionarySetValue(supp->ui_config_dict, 
			     kEAPClientPropNewPassword,
			     response->new_password);
	supp->ignore_password = FALSE;
    }
    if (response->chosen_identity != NULL) {
	EAPSecIdentityHandleRef	id_handle;

	id_handle = EAPSecIdentityHandleCreate(response->chosen_identity);
	CFDictionarySetValue(supp->ui_config_dict,
			     kEAPClientPropTLSIdentityHandle,
			     id_handle);
	CFRelease(id_handle);
	supp->ignore_sec_identity = FALSE;
    }
    supp->remember_information = response->remember_information;

    config_dict = CFDictionaryCreateCopy(NULL, supp->orig_config_dict);
    Supplicant_update_configuration(supp, config_dict, NULL);
    my_CFRelease(&config_dict);
    user_supplied_data(supp);
    return;
}

static void
alert_callback(const void * arg1, const void * arg2)
{
    SupplicantRef 	supp = (SupplicantRef)arg1;

    AlertDialogue_free(&supp->alert_prompt);
    EAPOLSocketStopClient(supp->sock);
    return;
}


static void
present_alert_dialogue(SupplicantRef supp)
{
    CFStringRef		message = NULL;

    if (supp->no_ui) {
	return;
    }
    if (supp->alert_prompt != NULL) {
	AlertDialogue_free(&supp->alert_prompt);
    }
    switch (supp->last_status) {
    case kEAPClientStatusOK:
	break;
    case kEAPClientStatusFailed:
	/* assume it's a password error */
	break;
    case kEAPClientStatusSecurityError:
	switch (supp->last_error) {
	case errSSLCrypto:
	    break;
	default:
	    message = CFSTR("EAPOLCLIENT_FAILURE_MESSAGE_DEFAULT");
	    break;
	}
	break;
    case kEAPClientStatusProtocolNotSupported:
    case kEAPClientStatusInnerProtocolNotSupported:
	message = CFSTR("EAPOLCLIENT_FAILURE_MESSAGE_DEFAULT");
	break;
    case kEAPClientStatusServerCertificateNotTrusted:
	/* trust settings not correct */
	message = CFSTR("EAPOLCLIENT_FAILURE_MESSAGE_SERVER_NOT_TRUSTED");
	break;
    case kEAPClientStatusAuthenticationStalled:
	message = CFSTR("EAPOLCLIENT_FAILURE_MESSAGE_AUTHENTICATION_STALLED");
	break;

    default:
	message = CFSTR("EAPOLCLIENT_FAILURE_MESSAGE_DEFAULT");
	break;
    }
    if (message != NULL) {
	supp->alert_prompt 
	    = AlertDialogue_create(alert_callback, supp, NULL,
				   message, EAPOLSocketGetSSID(supp->sock));

    }
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
dictInsertAuthenticatorMACAddress(CFMutableDictionaryRef dict,
				  EAPOLSocketRef sock)
{
    const struct ether_addr *	authenticator_mac;
    CFDataRef			data;

    authenticator_mac = EAPOLSocketGetAuthenticatorMACAddress(sock);
    if (authenticator_mac == NULL) {
	return;
    }
    data = CFDataCreate(NULL, (const UInt8 *)authenticator_mac, 
			sizeof(*authenticator_mac));
    CFDictionarySetValue(dict, kEAPOLControlAuthenticatorMACAddress, data);
    CFRelease(data);
    return;
}

static void
S_config_changed(CFMachPortRef port, void * msg, CFIndex size, void * info)
{
    EAPOLClientConfigurationRef	cfg;
    CFStringRef			profileID;
    SupplicantRef		supp = (SupplicantRef)info;

    if (supp->itemID == NULL) {
	return;
    }
    profileID = EAPOLClientItemIDGetProfileID(supp->itemID);
    if (profileID == NULL) {
	return;
    }
    cfg = EAPOLClientConfigurationCreate(NULL);
    if (cfg == NULL) {
	EAPLOG_FL(LOG_ERR, "EAPOLClientConfiguration() failed");
	return;
    }
    if (EAPOLClientConfigurationGetProfileWithID(cfg, profileID) == NULL) {
	EAPLOG(LOG_NOTICE, "%s: profile no longer exists, stopping",
	       EAPOLSocketIfName(supp->sock, NULL));
	EAPOLControlStop(EAPOLSocketIfName(supp->sock, NULL));
    }
    CFRelease(cfg);
    return;
}

static void
S_add_config_notification(SupplicantRef supp)
{
    CFMachPortContext		context = {0, NULL, NULL, NULL, NULL};
    CFMachPortRef		notify_port_cf;
    mach_port_t			notify_port;
    int				notify_token;
    CFRunLoopSourceRef		rls;
    uint32_t			status;

    if (supp->config_change.mp != NULL) {
	/* already registered, nothing to do */
	return;
    }
    notify_port = MACH_PORT_NULL;
    status 
	= notify_register_mach_port(kEAPOLClientConfigurationChangedNotifyKey,
				    &notify_port, 0, &notify_token);
    if (status != NOTIFY_STATUS_OK) {
	EAPLOG_FL(LOG_ERR, "notify_register_mach_port() failed");
	return;
    }
    context.info = supp;
    notify_port_cf = CFMachPortCreateWithPort(NULL, notify_port,
					      S_config_changed, 
					      &context,
					      NULL);
    if (notify_port_cf == NULL) {
	EAPLOG_FL(LOG_ERR, "CFMachPortCreateWithPort() failed");
	(void)notify_cancel(notify_token);
	return;
    }
    rls = CFMachPortCreateRunLoopSource(NULL, notify_port_cf, 0);
    if (rls == NULL) {
	EAPLOG_FL(LOG_ERR, "CFMachPortCreateRunLoopSource() failed");
	CFRelease(notify_port_cf);
	(void)notify_cancel(notify_token);
	return;
    }
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, kCFRunLoopDefaultMode);
    CFRelease(rls);
    supp->config_change.mp = notify_port_cf;
    supp->config_change.token = notify_token;
    return;
}

/*
 * Function: S_system_mode_use_od
 * Purpose:
 *   Check whether System Mode should use Open Directory machine
 *   credentials.
 */
static bool
S_system_mode_use_od(CFDictionaryRef dict, CFStringRef * ret_nodename)
{
    CFBooleanRef	use_od_cf;
    bool		use_od = FALSE;

    *ret_nodename = NULL;
    if (dict == NULL) {
	return (FALSE);
    }
    use_od_cf
	= CFDictionaryGetValue(dict,
			       kEAPClientPropSystemModeUseOpenDirectoryCredentials);
    if (isA_CFBoolean(use_od_cf) != NULL) {
	if (CFBooleanGetValue(use_od_cf)) {
	    CFStringRef	nodename;

	    use_od = TRUE;
	    nodename
		= CFDictionaryGetValue(dict,
				       kEAPClientPropSystemModeOpenDirectoryNodeName);
	    if (isA_CFString(nodename) != NULL) {
		*ret_nodename = nodename;
	    }
	}
    }
    else {
	CFStringRef	cred_source;

	cred_source
	    = CFDictionaryGetValue(dict,
				   kEAPClientPropSystemModeCredentialsSource);
	if (isA_CFString(cred_source) != NULL) {
	    use_od = CFEqual(cred_source, 
			     kEAPClientCredentialsSourceActiveDirectory);
	}
    }
    return (use_od);
}

#endif /* ! TARGET_OS_EMBEDDED */

static void 
Supplicant_report_status(SupplicantRef supp)
{
    CFMutableDictionaryRef	dict;
    EAPOLControlMode		mode;
#if ! TARGET_OS_EMBEDDED
    Boolean			need_username = FALSE;
    Boolean			need_password = FALSE;
    Boolean			need_new_password = FALSE;
    Boolean			need_trust = FALSE;
#endif /* ! TARGET_OS_EMBEDDED */
    CFDateRef			timestamp = NULL;

    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
    mode = EAPOLSocketGetMode(supp->sock);
    dictInsertMode(dict, mode);
    if (supp->config_id != NULL) {
	CFDictionarySetValue(dict, kEAPOLControlUniqueIdentifier,
			     supp->config_id);
    }
    dictInsertSupplicantState(dict, supp->state);
#if ! TARGET_OS_EMBEDDED
    dictInsertAuthenticatorMACAddress(dict, supp->sock);
    if (mode == kEAPOLControlModeUser) {
	dictInsertNumber(dict, kEAPOLControlUID, getuid());
    }
    if (supp->manager_name != NULL) {
	CFDictionarySetValue(dict, kEAPOLControlManagerName,
			     supp->manager_name);
    }
#endif /* ! TARGET_OS_EMBEDDED */
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
		need_new_password 
		    = my_CFArrayContainsValue(supp->eap.required_props,
					      kEAPClientPropNewPassword);
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
	CFStringRef	str = NULL;

	if (dict != NULL) {
	    str = my_CFPropertyListCopyAsXMLString(dict);
	}
	EAPLOG(-LOG_DEBUG, "Supplicant %s status: state=%s\n%@",
	       EAPOLSocketName(supp->sock),
	       SupplicantStateString(supp->state),
	       str == NULL ? CFSTR("") : str);
	my_CFRelease(&str);
    }
    EAPOLSocketReportStatus(supp->sock, dict);
    my_CFRelease(&dict);

#if ! TARGET_OS_EMBEDDED
    if (supp->no_ui) {
	goto no_ui;
    }
    if (need_username || need_password || need_new_password) {
	if (supp->cred_prompt == NULL) {
	    Boolean			ask_for_password = TRUE;
	    CFMutableDictionaryRef	details;
	    CFArrayRef			identities = NULL;
	    CFStringRef			ssid;

	    if (need_new_password == FALSE && need_password == FALSE) {
		Boolean			cert_required;
		Boolean			tls_specified;

		need_password 
		    = EAPAcceptTypesRequirePassword(&supp->eap_accept);
		tls_specified
		    = EAPAcceptTypesIsSupportedType(&supp->eap_accept,
						    kEAPTypeTLS);
		cert_required
		    = myCFDictionaryGetBooleanValue(supp->config_dict,
						    kEAPClientPropTLSCertificateIsRequired,
						    FALSE);
		if (tls_specified || cert_required) {
		    Boolean		only_tls_specified;

		    only_tls_specified 
			= (tls_specified && need_password == FALSE);
		    (void)EAPSecIdentityListCreate(&identities);
		    if (identities == NULL) {
			if (only_tls_specified || cert_required) {
			    /* we need a cert to authenticate */
			    /* XXX tell the user there aren't any certs */
			    EAPLOG(LOG_NOTICE, "User has no certificates");
			    return;
			}
		    }
		    else if (only_tls_specified) {
			/* no need for a password */
			ask_for_password = FALSE;
		    }
		}
	    }
	    details
		= CFDictionaryCreateMutable(NULL, 0,
					    &kCFTypeDictionaryKeyCallBacks,
					    &kCFTypeDictionaryValueCallBacks);
	    ssid = EAPOLSocketGetSSID(supp->sock);
	    if (ssid != NULL) {
		CFDictionarySetValue(details, kCredentialsDialogueSSID, ssid);
	    }
	    if (supp->itemID != NULL && supp->one_time_password == FALSE) {
		CFDictionarySetValue(details, 
				     kCredentialsDialogueRememberInformation,
				     kCFBooleanTrue);
	    }
	    if (supp->username != NULL) {
		CFStringRef	str;

		str = my_CFStringCreateWithCString(supp->username);
		CFDictionarySetValue(details,
				     kCredentialsDialogueAccountName,
				     str);
		CFRelease(str);
	    }
	    if (need_new_password && supp->password != NULL) {
		CFStringRef	str;

		/* password change dialogue */
		CFDictionarySetValue(details,
				     kCredentialsDialoguePasswordChangeRequired,
				     kCFBooleanTrue);
		str = my_CFStringCreateWithCString(supp->password);
		CFDictionarySetValue(details,
				     kCredentialsDialoguePassword,
				     str);
		CFRelease(str);
	    }
	    else if (ask_for_password == FALSE) {
		CFDictionarySetValue(details,
				     kCredentialsDialoguePassword,
				     kCFNull);
	    }
	    if (identities != NULL) {
		CFDictionarySetValue(details,
				     kCredentialsDialogueCertificates,
				     identities);
		CFRelease(identities);
	    }
	    supp->remember_information = FALSE;
	    supp->cred_prompt 
		= CredentialsDialogue_create(credentials_callback, supp, NULL,
					     details);
	    my_CFRelease(&details);
	}
    }
    if (need_trust) {
	if (supp->trust_prompt == NULL) {
	    supp->trust_prompt 
		= TrustDialogue_create(trust_callback, supp, NULL,
				       supp->eap.published_props,
				       EAPOLSocketGetSSID(supp->sock));
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
#if ! TARGET_OS_EMBEDDED
	present_alert_dialogue(supp);
	CredentialsDialogue_free(&supp->cred_prompt);
	TrustDialogue_free(&supp->trust_prompt);
#endif /* ! TARGET_OS_EMBEDDED */
	if (supp->eap.module != NULL 
	    && supp->no_ui == FALSE
	    && (supp->last_status == kEAPClientStatusFailed
		|| (supp->last_status == kEAPClientStatusSecurityError
		    && supp->last_error == errSSLCrypto))) {
	    clear_sec_identity(supp);
	    clear_username(supp);
	    clear_password(supp);
#if ! TARGET_OS_EMBEDDED
	    if (EAPOLSocketIsWireless(supp->sock)) {
		/* force a re-association so we immediately prompt the user */
		EAPOLSocketReassociate(supp->sock);
	    }
#endif /* ! TARGET_OS_EMBEDDED */
	}
	supp->last_status = kEAPClientStatusOK;
	supp->last_error = 0;
	free_last_packet(supp);
	eap_client_free(supp);
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
		log_eap_notification(supp->state, req_p);
		respond_to_notification(supp, req_p->identifier);
		break;
	    default:
		Supplicant_authenticating(supp, kSupplicantEventStart, evdata);
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

PRIVATE_EXTERN void
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
	if (EAPOLSocketIsWireless(supp->sock)) {
	    EAPOLSocketClearPMKCache(supp->sock);
	}
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

static int
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

/*
 * Function: S_string_copy_from_data
 * Purpose:
 *   Take the bytes from the specified CFDataRef and allocate a C-string
 *   large enough to hold the bytes plus the terminating nul char.
 *
 *   The assumption here is that the data represents a string but in data
 *   form.
 */
static char *
S_string_from_data(CFDataRef data)
{
    int		data_length;
    char *	str;

    data_length = CFDataGetLength(data);
    str = malloc(data_length + 1);
    bcopy(CFDataGetBytePtr(data), str, data_length);
    str[data_length] = '\0';
    return (str);
}

static char *
S_copy_password_from_keychain(bool use_system_keychain,
			      CFStringRef unique_id_str)
{
    SecKeychainRef 	keychain = NULL;
    char *		password = NULL;
    CFDataRef		password_data = NULL;
    OSStatus		status;

#if ! TARGET_OS_EMBEDDED
    if (use_system_keychain) {
	status =  SecKeychainCopyDomainDefault(kSecPreferencesDomainSystem,
					       &keychain);
	if (status != noErr) {
	    goto done;
	}
    }
#endif /* ! TARGET_OS_EMBEDDED */

    status = EAPSecKeychainPasswordItemCopy(keychain, unique_id_str,
					    &password_data);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SecKeychainFindGenericPassword failed, %d",
		  status);
	goto done;
    }
    password = S_string_from_data(password_data);

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

/*
 * Function: S_filter_eap_accept_types
 * Purpose:
 *   Filter out protocols we accept depending on which credential type(s)
 *   are specified.
 *
 *   If there's just a single protocol specified, no filtering is required.
 *   If both password and identity are specifed, or both password and identity
 *   are not specified, no filtering is possible.
 *   Otherwise, either make EAP-TLS the only choice if an identity is 
 *   specified, or exclude EAP-TLS if a password is specified.
 */
static void
S_filter_eap_accept_types(SupplicantRef supp, CFArrayRef accept_types,
			  bool password_specified, bool identity_specified)
{
    EAPAcceptTypesRef		accept_p = &supp->eap_accept;
    int				tls_index = -1;

    if (accept_p->count < 2 || (password_specified == identity_specified)) {
	return;
    }
    tls_index = EAPAcceptTypesIndexOfType(accept_p, kEAPTypeTLS);
    if (identity_specified) {
	if (tls_index == INDEX_NONE) {
	    if (myCFDictionaryGetBooleanValue(supp->config_dict,
					      kEAPClientPropTLSCertificateIsRequired,
					      FALSE) == FALSE) {
		/* this should not happen */
		EAPLOG(LOG_NOTICE,
		       "%s: identity specified but EAP-TLS isn't enabled",
		       EAPOLSocketIfName(supp->sock, NULL));
	    }
	}
	else {
	    /* only accept EAP-TLS */
	    accept_p->count = 1;
	    accept_p->types[0] = kEAPTypeTLS;
	    eapolclient_log(kLogFlagBasic,
			    "identity is specified, enabling EAP-TLS only");
	}
    }
    else if (tls_index != INDEX_NONE) {
	/* exclude EAP-TLS */
	eapolclient_log(kLogFlagBasic,
			"password is specified, disabling EAP-TLS");
	EAPAcceptTypesRemoveTypeAtIndex(accept_p, tls_index);
    }
    return;
}


static bool
S_set_credentials(SupplicantRef supp)
{
    CFArrayRef			accept_types = NULL;
    bool			cert_required = FALSE;
    bool			change = FALSE;
#if ! TARGET_OS_EMBEDDED
    EAPOLClientDomain		domain;
    CFStringRef			nodename = NULL;
#endif /* ! TARGET_OS_EMBEDDED */
    EAPSecIdentityHandleRef	id_handle = NULL;
    CFArrayRef			inner_accept_types;
    char *			name = NULL;
    CFStringRef			outer_identity_cf = NULL;
    char *			outer_identity = NULL;
    char *			password = NULL;
    bool			remember_information = FALSE;
    SecIdentityRef		sec_identity = NULL;
    OSStatus			status;
    bool			system_mode = FALSE;
    bool			username_derived = FALSE;

    if (supp->config_dict != NULL) {
	accept_types 
	    = CFDictionaryGetValue(supp->config_dict,
				   kEAPClientPropAcceptEAPTypes);
    }
    if (isA_CFArray(accept_types) == NULL) {
	EAPAcceptTypesFree(&supp->eap_accept);
	return (TRUE);
    }
    EAPAcceptTypesInit(&supp->eap_accept, accept_types);

    inner_accept_types
	= CFDictionaryGetValue(supp->config_dict,
			       kEAPClientPropInnerAcceptEAPTypes);
    inner_accept_types = isA_CFArray(inner_accept_types);

    switch (EAPOLSocketGetMode(supp->sock)) {
    case kEAPOLControlModeSystem:
	system_mode = TRUE;
#if ! TARGET_OS_EMBEDDED
	S_set_credentials_access_time(supp);
	domain = kEAPOLClientDomainSystem;
#endif /* ! TARGET_OS_EMBEDDED */
	break;
    case kEAPOLControlModeUser:
	/* check whether one-time use password */
	supp->one_time_password 
	    = myCFDictionaryGetBooleanValue(supp->config_dict,
					    kEAPClientPropOneTimeUserPassword,
					    FALSE);
#if ! TARGET_OS_EMBEDDED
	domain = kEAPOLClientDomainUser;
	remember_information
	    = myCFDictionaryGetBooleanValue(supp->config_dict,
					    kEAPClientPropSaveCredentialsOnSuccessfulAuthentication,
					    FALSE);
#endif /* ! TARGET_OS_EMBEDDED */
	break;
    default:
#if ! TARGET_OS_EMBEDDED
	domain = 0;
#endif /* ! TARGET_OS_EMBEDDED */
	break;
    }

#if ! TARGET_OS_EMBEDDED
    /* in system mode, check for OD password if so configured */
    if (system_mode
	&& S_system_mode_use_od(supp->config_dict, &nodename)) {
	CFStringRef	domain_cf = NULL;
	CFStringRef	password_cf = NULL;
	CFStringRef	username_cf = NULL;
	
	if (ODTrustInfoCopy(nodename, 
			    &domain_cf,
			    &username_cf, 
			    &password_cf)
	    && username_cf != NULL
	    && password_cf != NULL) {
	    if (domain_cf != NULL) {
		/* Create the fully-qualified user name */
		CFMutableStringRef fqusername_cf = 
		    CFStringCreateMutableCopy(NULL, 0, domain_cf);
		CFStringAppend(fqusername_cf, CFSTR("\\"));
		CFStringAppend(fqusername_cf, username_cf);
		CFRelease(username_cf);
		username_cf = fqusername_cf;
	    }

	    name = my_CFStringToCString(username_cf, kCFStringEncodingUTF8);
	    password = my_CFStringToCString(password_cf, kCFStringEncodingUTF8);
	}
	my_CFRelease(&username_cf);
	my_CFRelease(&password_cf);
	my_CFRelease(&domain_cf);
	if (name != NULL && password != NULL) {
	    /* successfully retrieved OpenDirectory credentials */
	    EAPLOG(LOG_NOTICE,
		   "System Mode using OD account '%s'",
		   name);
	}
	else {
	    /* failed to get OpenDirectory credentials */
	    EAPLOG(LOG_NOTICE, "System Mode OD credentials unavailable");
	}
	goto filter_eap_types;
    }
#endif /* ! TARGET_OS_EMBEDDED */

    /* extract the username */
    if (supp->ignore_username == FALSE) {
	CFStringRef		name_cf;

	name_cf = CFDictionaryGetValue(supp->config_dict, 
				       kEAPClientPropUserName);
	name_cf = isA_CFString(name_cf);
	if (name_cf != NULL) {
	    name = my_CFStringToCString(name_cf, kCFStringEncodingUTF8);
	    if (remember_information) {
		supp->remember_information = TRUE;
	    }
	}
    }

    /* extract the password */
    if (supp->ignore_password == FALSE) {
	CFStringRef	item_cf;
	CFStringRef	password_cf;

	password_cf = CFDictionaryGetValue(supp->config_dict, 
					   kEAPClientPropUserPassword);
	item_cf
	    = CFDictionaryGetValue(supp->config_dict, 
				   kEAPClientPropUserPasswordKeychainItemID);
	if (isA_CFString(password_cf) != NULL) {
	    password = my_CFStringToCString(password_cf, 
					    kCFStringEncodingMacRoman);
	    if (remember_information) {
		supp->remember_information = TRUE;
	    }
	}
	else if (isA_CFString(item_cf) != NULL) {
	    password = S_copy_password_from_keychain(system_mode,
						     item_cf);
	    if (password == NULL) {
		EAPLOG_FL(LOG_NOTICE, 
			  "%s: failed to retrieve password from keychain",
			  EAPOLSocketIfName(supp->sock, NULL));
	    }
	}
#if ! TARGET_OS_EMBEDDED
	else if (name == NULL && supp->itemID != NULL) {
	    CFDataRef		name_data = NULL;
	    CFDataRef		password_data = NULL;

	    if (EAPOLClientItemIDCopyPasswordItem(supp->itemID, 
						  domain,
						  &name_data,
						  &password_data)) {
		if (password_data != NULL) {
		    password = S_string_from_data(password_data);
		}
		if (name_data != NULL) {
		    name = S_string_from_data(name_data);
		}
		my_CFRelease(&name_data);
		my_CFRelease(&password_data);
	    }
	}
#endif /* ! TARGET_OS_EMBEDDED */
    }

    /* check for a SecIdentity */
    if (supp->ignore_sec_identity == FALSE) {
	bool		tls_specified;

	tls_specified
	    = (S_array_contains_int(accept_types, kEAPTypeTLS)
	       || S_array_contains_int(inner_accept_types, kEAPTypeTLS));
	cert_required
	    = myCFDictionaryGetBooleanValue(supp->config_dict,
					    kEAPClientPropTLSCertificateIsRequired,
					    tls_specified);
    }
    if (cert_required) {
	id_handle = CFDictionaryGetValue(supp->config_dict,
					 kEAPClientPropTLSIdentityHandle);
	if (id_handle != NULL) {
	    status = EAPSecIdentityHandleCreateSecIdentity(id_handle,
							   &sec_identity);
	    if (status != noErr) {
		EAPLOG_FL(LOG_NOTICE,
			  "EAPSecIdentityHandleCreateSecIdentity failed, %d",
			  status);
	    }
	    else if (remember_information) {
		supp->remember_information = TRUE;
	    }
	}

#if ! TARGET_OS_EMBEDDED
	/* grab itemID-based identity */
	if (sec_identity == NULL && supp->itemID != NULL) {
	    sec_identity = EAPOLClientItemIDCopyIdentity(supp->itemID, domain);
	}
#endif /* ! TARGET_OS_EMBEDDED */
	my_CFRelease(&supp->sec_identity);
	supp->sec_identity = sec_identity;

	if (name == NULL && sec_identity != NULL) {
	    name = S_identity_copy_name(sec_identity);
	    if (name != NULL) {
		username_derived = TRUE;
	    }
	}
    }

#if ! TARGET_OS_EMBEDDED
 filter_eap_types:
#endif /* ! TARGET_OS_EMBEDDED */

    /* update the list of protocols we accept */
    S_filter_eap_accept_types(supp, accept_types, (password != NULL),
			      (sec_identity != NULL));

    /* name */
    if (cert_required == FALSE && name == NULL) {
	/* no username specified, ask EAP types if they can come up with one */
	name = eap_method_user_name(&supp->eap_accept, supp->config_dict);
	if (name != NULL) {
	    username_derived = TRUE;
	}
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
    supp->username_derived = username_derived;

    /* password */
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
    
    /* extract the outer identity */
    if (EAPAcceptTypesUseOuterIdentity(&supp->eap_accept) == TRUE) {
	outer_identity_cf = CFDictionaryGetValue(supp->config_dict, 
						 kEAPClientPropOuterIdentity);
	outer_identity_cf = isA_CFString(outer_identity_cf);
	if (outer_identity_cf != NULL) {
	    outer_identity = my_CFStringToCString(outer_identity_cf,
						  kCFStringEncodingUTF8);
	}
    }
    if (my_strcmp(supp->outer_identity, outer_identity) != 0) {
	change = TRUE;
    }
    if (supp->outer_identity != NULL) {
	free(supp->outer_identity);
    }
    supp->outer_identity = outer_identity;
    if (outer_identity != NULL) {
	supp->outer_identity_length = strlen(outer_identity);
    }
    else {
	supp->outer_identity_length = 0;
    }
    return (change);
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

PRIVATE_EXTERN bool
Supplicant_update_configuration(SupplicantRef supp, CFDictionaryRef config_dict,
				bool * should_stop)
{
#if ! TARGET_OS_EMBEDDED
    EAPOLClientConfigurationRef	cfg = NULL;
    EAPOLClientItemIDRef	itemID = NULL;
    CFDictionaryRef		item_dict;
    CFStringRef			manager_name;
    EAPOLClientProfileRef	profile = NULL;
    CFDictionaryRef		password_info = NULL;
#endif /* ! TARGET_OS_EMBEDDED */
    bool			change = FALSE;
    CFStringRef			config_id = NULL;
    CFDictionaryRef		eap_config;
    bool			empty_password = FALSE;
    bool			empty_user = FALSE;

    if (should_stop != NULL) {
	*should_stop = FALSE;
    }

#if ! TARGET_OS_EMBEDDED
    /* check for a manager name */
    my_CFRelease(&supp->manager_name);
    manager_name = CFDictionaryGetValue(config_dict,
					kEAPOLControlManagerName);
    if (isA_CFString(manager_name) != NULL) {
	supp->manager_name = CFRetain(manager_name);
    }

    /* check whether there is an itemID */
    item_dict = CFDictionaryGetValue(config_dict, kEAPOLControlClientItemID);
    if (item_dict != NULL) {
	if (isA_CFDictionary(item_dict) == NULL) {
	    EAPLOG_FL(LOG_NOTICE, "invalid item dict");
	    if (should_stop != NULL) {
		*should_stop = TRUE;
	    }
	    goto done;
	}
	my_CFRelease(&supp->itemID);
	my_CFRelease(&supp->eapolcfg);
	cfg = EAPOLClientConfigurationCreate(NULL);
	itemID = EAPOLClientItemIDCreateWithDictionary(cfg, item_dict);
	supp->itemID = itemID;
	supp->eapolcfg = cfg;
	if (itemID == NULL) {
	    EAPLOG_FL(LOG_NOTICE, "couldn't instantiate item");
	    if (should_stop != NULL) {
		*should_stop = TRUE;
	    }
	    goto done;
	}
	profile = EAPOLClientItemIDGetProfile(itemID);
	if (profile == NULL) {
	    eap_config = 
		EAPOLClientConfigurationGetDefaultAuthenticationProperties(cfg);
	}
	else {
	    eap_config = EAPOLClientProfileGetAuthenticationProperties(profile);
	    if (eap_config == NULL) {
		EAPLOG_FL(LOG_NOTICE,
			  "profile has no authentication properties");
		if (should_stop != NULL) {
		    *should_stop = TRUE;
		}
		goto done;
	    }
	    config_id = EAPOLClientProfileGetID(profile);
	    /* we're using a profile, monitor whether it gets removed */
	    S_add_config_notification(supp);
	}

	/* name/password may be passed on the side in this dictionary */
	password_info
	    = CFDictionaryGetValue(config_dict,
				   kEAPOLControlEAPClientConfiguration);
	password_info = isA_CFDictionary(password_info);
    }
    else {
	my_CFRelease(&supp->itemID);
	my_CFRelease(&supp->eapolcfg);
#endif /* ! TARGET_OS_EMBEDDED */

	/* get the new configuration */
	eap_config = CFDictionaryGetValue(config_dict,
					  kEAPOLControlEAPClientConfiguration);
	if (isA_CFDictionary(eap_config) == NULL) {
	    eap_config = config_dict;
	}
	config_id = CFDictionaryGetValue(config_dict,
					 kEAPOLControlUniqueIdentifier);

#if ! TARGET_OS_EMBEDDED
    }
#endif /* ! TARGET_OS_EMBEDDED */

    /* keep a copy of the original around */
    my_CFRelease(&supp->orig_config_dict);
    supp->orig_config_dict = CFDictionaryCreateCopy(NULL, config_dict);

    empty_user 
	= cfstring_is_empty(CFDictionaryGetValue(eap_config,
						 kEAPClientPropUserName));
    empty_password 
	= cfstring_is_empty(CFDictionaryGetValue(eap_config,
						 kEAPClientPropUserPassword));

    my_CFRelease(&supp->config_dict);

    /* clean up empty username/password, add UI properties */
    if (empty_user || empty_password
	|| supp->ui_config_dict != NULL
#if ! TARGET_OS_EMBEDDED
	|| password_info != NULL
	|| profile != NULL
#endif /* ! TARGET_OS_EMBEDDED */
	|| CFDictionaryContainsKey(eap_config,
				   kEAPClientPropProfileID)) {
	CFMutableDictionaryRef		new_eap_config = NULL;

	new_eap_config = CFDictionaryCreateMutableCopy(NULL, 0, eap_config);
	if (empty_user) {
	    CFDictionaryRemoveValue(new_eap_config, kEAPClientPropUserName);
	}
	if (empty_password) {
	    CFDictionaryRemoveValue(new_eap_config, kEAPClientPropUserPassword);
	}
	if (supp->ui_config_dict != NULL) {
	    CFDictionaryApplyFunction(supp->ui_config_dict, dict_set_key_value, 
				      new_eap_config);
	}
	CFDictionaryRemoveValue(new_eap_config,
				kEAPClientPropProfileID);
#if ! TARGET_OS_EMBEDDED
	if (password_info != NULL) {
	    CFDictionaryApplyFunction(password_info, dict_set_key_value, 
				      new_eap_config);
	}
	if (profile != NULL) {
	    CFDictionarySetValue(new_eap_config,
				 kEAPClientPropProfileID,
				 EAPOLClientProfileGetID(profile));
	}
#endif /* TARGET_OS_EMBEDDED */
	supp->config_dict = new_eap_config;
    }
    else {
	supp->config_dict = CFRetain(eap_config);
    }
    if (eapolclient_should_log(kLogFlagBasic)) {
	CFStringRef	str;

	str = copy_cleaned_config_dict(supp->config_dict);
	EAPLOG(-LOG_DEBUG, "update_configuration\n%@", str);
	CFRelease(str);
    }

    /* bump the configuration generation */
    supp->generation++;

    /* get the configuration identifier */
    my_CFRelease(&supp->config_id);
    if (config_id != NULL && isA_CFString(config_id) != NULL) {
	supp->config_id = CFRetain(config_id);
    }

    /* update the name/password, identity, and list of EAP types we accept */
    if (S_set_credentials(supp)) {
	change = TRUE;
    }
    if (EAPAcceptTypesIsSupportedType(&supp->eap_accept,
				      eap_client_type(supp)) == FALSE) {
	/* negotiated EAP type is no longer valid, start over */
	eap_client_free(supp);
	change = TRUE;
    }

#if ! TARGET_OS_EMBEDDED
 done:
#endif /* ! TARGET_OS_EMBEDDED */
    return (change);
}

PRIVATE_EXTERN bool
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
	Supplicant_update_configuration(supp, config_dict, NULL);
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
	change = Supplicant_update_configuration(supp, config_dict,
						 &should_stop);
	if (should_stop) {
	    break;
	}
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
    return (should_stop);
}

PRIVATE_EXTERN void
Supplicant_link_status_changed(SupplicantRef supp, bool active)
{
    struct timeval	t = {0, 0};

    supp->auth_attempts_count = 0;
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

PRIVATE_EXTERN SupplicantRef
Supplicant_create(EAPOLSocketRef sock)
{
    SupplicantRef		supp = NULL;
    TimerRef			timer = NULL;

    timer = Timer_create();
    if (timer == NULL) {
	EAPLOG_FL(LOG_NOTICE, "Timer_create failed");
	goto failed;
    }

    supp = malloc(sizeof(*supp));
    if (supp == NULL) {
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
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

PRIVATE_EXTERN SupplicantRef
Supplicant_create_with_supplicant(EAPOLSocketRef sock, SupplicantRef main_supp)
{
    SupplicantRef	supp;

    supp = Supplicant_create(sock);
    if (supp == NULL) {
	return (NULL);
    }
    supp->generation = main_supp->generation;
#if ! TARGET_OS_EMBEDDED
    if (main_supp->itemID != NULL) {
	CFRetain(main_supp->itemID);
	supp->itemID = main_supp->itemID;
    }
#endif /* ! TARGET_OS_EMBEDDED */
    if (main_supp->sec_identity != NULL) {
	CFRetain(main_supp->sec_identity);
	supp->sec_identity = main_supp->sec_identity;
    }
    supp->config_dict = CFRetain(main_supp->config_dict);
    if (main_supp->ui_config_dict) {
	supp->ui_config_dict 
	    = CFDictionaryCreateMutableCopy(NULL, 0, main_supp->ui_config_dict);
    }
    if (main_supp->outer_identity != NULL) {
	supp->outer_identity = strdup(main_supp->outer_identity);
	supp->outer_identity_length = main_supp->outer_identity_length;
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
    supp->no_ui = TRUE;

    return (supp);
}

PRIVATE_EXTERN void
Supplicant_free(SupplicantRef * supp_p)
{
    SupplicantRef supp;

    if (supp_p == NULL) {
	return;
    }
    supp = *supp_p;
    if (supp != NULL) {
#if ! TARGET_OS_EMBEDDED
	AlertDialogue_free(&supp->alert_prompt);
	CredentialsDialogue_free(&supp->cred_prompt);
	TrustDialogue_free(&supp->trust_prompt);
#endif /* ! TARGET_OS_EMBEDDED */
	Timer_free(&supp->timer);
	my_CFRelease(&supp->orig_config_dict);
	my_CFRelease(&supp->config_dict);
	my_CFRelease(&supp->ui_config_dict);
	my_CFRelease(&supp->config_id);
	my_CFRelease(&supp->identity_attributes);
#if ! TARGET_OS_EMBEDDED
	my_CFRelease(&supp->eapolcfg);
	my_CFRelease(&supp->itemID);
	my_CFRelease(&supp->manager_name);
	if (supp->config_change.mp != NULL) {
	    CFMachPortInvalidate(supp->config_change.mp);
	    my_CFRelease(&supp->config_change.mp);
	    (void)notify_cancel(supp->config_change.token);
	}
#endif /* ! TARGET_OS_EMBEDDED */
	my_CFRelease(&supp->sec_identity);
	if (supp->outer_identity != NULL) {
	    free(supp->outer_identity);
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

PRIVATE_EXTERN SupplicantState
Supplicant_get_state(SupplicantRef supp, EAPClientStatus * last_status)
{
    *last_status = supp->last_status;
    return (supp->state);
}

PRIVATE_EXTERN void
Supplicant_set_no_ui(SupplicantRef supp)
{
    supp->no_ui = TRUE;
    return;
}

static CFStringRef
copy_cleaned_config_dict(CFDictionaryRef d)
{
    CFStringRef		password;
    CFStringRef		new_password;
    CFStringRef		str;

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
	str = my_CFPropertyListCopyAsXMLString(d_copy);
	CFRelease(d_copy);
    }
    else {
	str = my_CFPropertyListCopyAsXMLString(d);
    }
    return (str);
}

#define SUCCESS_SIZE		(offsetof(EAPOLPacket, body)	\
				 + sizeof(EAPSuccessPacket))
PRIVATE_EXTERN void
Supplicant_simulate_success(SupplicantRef supp)
{
    uint32_t			buf[roundup(SUCCESS_SIZE, sizeof(uint32_t))];
    EAPOLPacketRef		eapol_p;
    EAPOLSocketReceiveData 	rx;
    EAPPacketRef		success_pkt;

    if (supp->state != kSupplicantStateAuthenticating) {
	return;
    }
    eapolclient_log(kLogFlagBasic, "Simulating EAP Success packet");
    eapol_p = (EAPOLPacketRef)buf;
    eapol_p->protocol_version = EAPOL_802_1_X_PROTOCOL_VERSION;
    eapol_p->packet_type = kEAPOLPacketTypeEAPPacket;
    EAPOLPacketSetLength(eapol_p, sizeof(EAPSuccessPacket));
    success_pkt = (EAPPacketRef)eapol_p->body;
    success_pkt->code = kEAPCodeSuccess;
    success_pkt->identifier = 0;
    EAPPacketSetLength(success_pkt, sizeof(EAPSuccessPacket));
    rx.eapol_p = eapol_p;
    rx.length = SUCCESS_SIZE;
    Supplicant_authenticating(supp, kSupplicantEventData, &rx);
    return;
}

PRIVATE_EXTERN void
Supplicant_set_globals(SCPreferencesRef prefs)
{
    CFDictionaryRef	plist;

    if (prefs == NULL) {
	return;
    }
    plist = SCPreferencesGetValue(prefs, kSupplicant);
    if (isA_CFDictionary(plist) == NULL) {
	return;
    }
    S_start_period_secs
	= get_plist_int(plist, kStartPeriodSeconds, START_PERIOD_SECS);
    S_start_attempts_max
	= get_plist_int(plist, kStartAttemptsMax, START_ATTEMPTS_MAX);
    S_auth_period_secs 
	= get_plist_int(plist, kAuthPeriodSeconds, AUTH_PERIOD_SECS);
    S_auth_attempts_max
	= get_plist_int(plist, kAuthAttemptsMax, AUTH_ATTEMPTS_MAX);
    S_held_period_secs
	= get_plist_int(plist, kHeldPeriodSeconds, HELD_PERIOD_SECS);
    return;
}
