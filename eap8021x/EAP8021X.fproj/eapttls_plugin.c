/*
 * Copyright (c) 2002-2017 Apple Inc. All rights reserved.
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
 * eapttls_plugin.c
 * - EAP-TTLS client using SecureTransport API's
 */

/* 
 * Modification History
 *
 * October 1, 2002	Dieter Siegmund (dieter@apple)
 * - created (from eaptls_plugin.c)
 *
 * September 7, 2004	Dieter Siegmund (dieter@apple)
 * - use SecTrustEvaluate, and enable user interaction to decide whether to
 *   proceed or not, instead of just generating an error
 */
 
#include <EAP8021X/EAPClientPlugin.h>
#include <EAP8021X/EAPClientProperties.h>
#include <SystemConfiguration/SCValidation.h>
#include <mach/boolean.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <Security/SecureTransport.h>
#include <Security/SecCertificate.h>
#include <sys/param.h>
#include <EAP8021X/EAPTLSUtil.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPSecurity.h>
#include <EAP8021X/EAPCertificateUtil.h>
#include <Security/SecureTransportPriv.h>
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPClientModule.h>
#include <EAP8021X/chap.h>
#include <EAP8021X/mschap.h>
#include <EAP8021X/RADIUSAttributes.h>
#include <EAP8021X/DiameterAVP.h>
#include "myCFUtil.h"
#include "printdata.h"
#include "EAPLog.h"

/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect eapttls_introspect;
static EAPClientPluginFuncVersion eapttls_version;
static EAPClientPluginFuncEAPType eapttls_type;
static EAPClientPluginFuncEAPName eapttls_name;
static EAPClientPluginFuncInit eapttls_init;
static EAPClientPluginFuncFree eapttls_free;
static EAPClientPluginFuncProcess eapttls_process;
static EAPClientPluginFuncFreePacket eapttls_free_packet;
static EAPClientPluginFuncSessionKey eapttls_session_key;
static EAPClientPluginFuncServerKey eapttls_server_key;
static EAPClientPluginFuncMasterSessionKeyCopyBytes eapttls_msk_copy_bytes;
static EAPClientPluginFuncRequireProperties eapttls_require_props;
static EAPClientPluginFuncPublishProperties eapttls_publish_props;
static EAPClientPluginFuncCopyPacketDescription eapttls_copy_packet_description;

#define kEAPTTLSClientLabel		"ttls keying material"
#define kEAPTTLSClientLabelLength	 (sizeof(kEAPTTLSClientLabel) - 1)
#define kEAPTTLSChallengeLabel		"ttls challenge"
#define kEAPTTLSChallengeLabelLength	 (sizeof(kEAPTTLSChallengeLabel) - 1)

typedef enum {
    kInnerAuthTypeNone = 0,
    kInnerAuthTypePAP,
    kInnerAuthTypeCHAP,
    kInnerAuthTypeMSCHAP,
    kInnerAuthTypeMSCHAPv2,
    kInnerAuthTypeEAP,
} InnerAuthType;

enum {
    kEAPInnerAuthStateUnknown = 0,
    kEAPInnerAuthStateSuccess = 1,
    kEAPInnerAuthStateFailure = 2,
};
typedef int EAPInnerAuthState;

static const char * auth_strings[] = {
    "none",
    "PAP",
    "CHAP",
    "MSCHAP",
    "MSCHAPv2",
    "EAP",
    NULL,
};

typedef enum {
    kRequestTypeStart,
    kRequestTypeAck,
    kRequestTypeData,
} RequestType;

typedef enum {
    kAuthStateIdle,
    kAuthStateStarted,
    kAuthStateComplete,
} AuthState;

static int inner_auth_types[] = {
    kEAPTypeMSCHAPv2,
    kEAPTypeGenericTokenCard,
    kEAPTypeMD5Challenge,
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

#define TTLS_MSCHAP_RESPONSE_LENGTH	(MSCHAP_NT_RESPONSE_SIZE	\
					 + MSCHAP_LM_RESPONSE_SIZE	\
					 + MSCHAP_FLAGS_SIZE		\
					 + MSCHAP_IDENT_SIZE)

#define TTLS_MSCHAP2_RESPONSE_LENGTH	(MSCHAP2_RESPONSE_LENGTH	\
					 + MSCHAP_IDENT_SIZE)
typedef struct {
    SSLContextRef		ssl_context;
    memoryBuffer		read_buffer;
    memoryBuffer		write_buffer;
    int				last_write_size;
    int				previous_identifier;
    memoryIO			mem_io;
    EAPClientState		plugin_state;
    bool			cert_is_required;
    CFArrayRef			certs;
    int				mtu;
    OSStatus			last_ssl_error;
    EAPClientStatus		last_client_status;
    InnerAuthType		inner_auth_type;
    bool			handshake_complete;
    bool			authentication_started;
    OSStatus			trust_ssl_error;
    EAPClientStatus		trust_status;
    bool			trust_proceed;
    bool			key_data_valid;
    char			key_data[128];
    bool			server_auth_completed;
    CFArrayRef			server_certs;
    bool			resume_sessions;
    bool			session_was_resumed;

    /* EAP state: */
    EAPInnerAuthState		inner_auth_state;
    struct eap_client		eap;
    EAPPacketRef		last_packet;
    char			last_packet_buf[1024];
    int				last_eap_type_index;

    /* MSCHAPv2 state: */
    uint8_t			peer_challenge[MSCHAP2_CHALLENGE_SIZE];
    uint8_t			nt_response[MSCHAP_NT_RESPONSE_SIZE];
    uint8_t			auth_challenge_id[MSCHAP2_CHALLENGE_SIZE + 1];
} EAPTTLSPluginData, * EAPTTLSPluginDataRef;

enum {
    kEAPTLSAvoidDenialOfServiceSize = 128 * 1024
};

#define BAD_IDENTIFIER			(-1)

static void
eap_client_free(EAPTTLSPluginDataRef context);

static void
free_last_packet(EAPTTLSPluginDataRef context)
{
    if (context->last_packet != NULL
	&& (void *)context->last_packet != context->last_packet_buf) {
	free(context->last_packet);
    }
    context->last_packet = NULL;
    return;
}

static void
save_last_packet(EAPTTLSPluginDataRef context, EAPPacketRef packet)
{
    EAPPacketRef	last_packet;
    int			len;

    last_packet = context->last_packet;
    if (last_packet == packet) {
	/* don't bother re-saving the same buffer */
	return;
    }
    len = EAPPacketGetLength(packet);
    if (len > sizeof(context->last_packet_buf)) {
	context->last_packet = (EAPPacketRef)malloc(len);
    }
    else {
	context->last_packet = (EAPPacketRef)context->last_packet_buf;
    }
    memcpy(context->last_packet, packet, len);
    if (last_packet != NULL
	&& (void *)last_packet != context->last_packet_buf) {
	free(last_packet);
    }
    return;
}


static InnerAuthType
InnerAuthTypeFromString(char * str)
{
    int i;

    for (i = 0; auth_strings[i] != NULL; i++) {
	if (strcmp(str, auth_strings[i]) == 0) {
	    return ((InnerAuthType)i);
	}
    }
    return (kInnerAuthTypeNone);
}

static bool
eapttls_compute_session_key(EAPTTLSPluginDataRef context)
{
    OSStatus		status;

    context->key_data_valid = FALSE;
    status = EAPTLSComputeKeyData(context->ssl_context, 
				  kEAPTTLSClientLabel, 
				  kEAPTTLSClientLabelLength,
				  context->key_data,
				  sizeof(context->key_data));
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, 
		  "EAPTLSComputeSessionKey failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	return (FALSE);
    }
    context->key_data_valid = TRUE;
    return (TRUE);
}

static void
eapttls_free_context(EAPTTLSPluginDataRef context)
{
    eap_client_free(context);
    if (context->ssl_context != NULL) {
	CFRelease(context->ssl_context);
	context->ssl_context = NULL;
    }
    my_CFRelease(&context->certs);
    my_CFRelease(&context->server_certs);
    memoryIOClearBuffers(&context->mem_io);
    free(context);
    return;
}

static OSStatus
eapttls_start(EAPClientPluginDataRef plugin)
{
    EAPTTLSPluginDataRef 	context = (EAPTTLSPluginDataRef)plugin->private;
    SSLContextRef		ssl_context = NULL;
    OSStatus			status = noErr;

    if (context->ssl_context != NULL) {
	CFRelease(context->ssl_context);
	context->ssl_context = NULL;
    }
    my_CFRelease(&context->server_certs);
    memoryIOClearBuffers(&context->mem_io);
    ssl_context = EAPTLSMemIOContextCreate(plugin->properties, FALSE, &context->mem_io, NULL,
					   &status);
    if (ssl_context == NULL) {
	EAPLOG_FL(LOG_NOTICE, "EAPTLSMemIOContextCreate failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	goto failed;
    }
    if (context->resume_sessions && plugin->unique_id != NULL) {
	status = SSLSetPeerID(ssl_context, plugin->unique_id,
			      plugin->unique_id_length);
	if (status != noErr) {
	    EAPLOG_FL(LOG_NOTICE, 
		      "SSLSetPeerID failed, %s (%ld)",
		      EAPSSLErrorString(status), (long)status);
	    goto failed;
	}
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
		      "SSLSetCertificate failed, %s (%ld)",
		      EAPSSLErrorString(status), (long)status);
	    goto failed;
	}
    }
    context->ssl_context = ssl_context;
    context->plugin_state = kEAPClientStateAuthenticating;
    context->previous_identifier = BAD_IDENTIFIER;
    context->last_ssl_error = noErr;
    context->last_client_status = kEAPClientStatusOK;
    context->handshake_complete = FALSE;
    context->authentication_started = FALSE;
    context->trust_proceed = FALSE;
    context->inner_auth_state = kEAPInnerAuthStateUnknown;
    context->server_auth_completed = FALSE;
    context->key_data_valid = FALSE;
    context->last_write_size = 0;
    context->session_was_resumed = FALSE;
    return (status);
 failed:
    if (ssl_context != NULL) {
	CFRelease(ssl_context);
    }
    return (status);
}

static InnerAuthType
get_inner_auth_type(CFDictionaryRef properties)
{
    InnerAuthType	inner_auth_type = kInnerAuthTypeNone;
    CFStringRef		inner_auth_cf;

    if (properties != NULL) {
	inner_auth_cf 
	    = CFDictionaryGetValue(properties,
				   kEAPClientPropTTLSInnerAuthentication);
	if (isA_CFString(inner_auth_cf) != NULL) {
	    char *		inner_auth = NULL;
	    
	    inner_auth = my_CFStringToCString(inner_auth_cf,
					      kCFStringEncodingASCII);
	    if (inner_auth != NULL) {
		inner_auth_type = InnerAuthTypeFromString(inner_auth);
		free(inner_auth);
	    }
	}
    }
    return (inner_auth_type);
}

static EAPClientStatus
eapttls_init(EAPClientPluginDataRef plugin, CFArrayRef * required_props,
	     EAPClientDomainSpecificError * error)
{
    EAPTTLSPluginDataRef	context = NULL;
    InnerAuthType		inner_auth_type;

    context = malloc(sizeof(*context));
    bzero(context, sizeof(*context));
    context->cert_is_required 
	= my_CFDictionaryGetBooleanValue(plugin->properties,
					 kEAPClientPropTLSCertificateIsRequired,
					 FALSE);
    context->mtu = plugin->mtu;
    inner_auth_type = get_inner_auth_type(plugin->properties);
    if (inner_auth_type == kInnerAuthTypeNone) {
	inner_auth_type = kInnerAuthTypeEAP;
    }
    context->inner_auth_type = inner_auth_type;
    context->resume_sessions
	= my_CFDictionaryGetBooleanValue(plugin->properties, 
					 kEAPClientPropTLSEnableSessionResumption,
					 TRUE);
    /* memoryIOInit() initializes the memoryBuffer structures as well */
    memoryIOInit(&context->mem_io, &context->read_buffer,
		 &context->write_buffer);
    //memoryIOSetDebug(&context->mem_io, TRUE);
    plugin->private = context;

    *error = 0;
    return (kEAPClientStatusOK);
}

static void
eapttls_free(EAPClientPluginDataRef plugin)
{
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;

    if (context != NULL) {
	eapttls_free_context(context);
	plugin->private = NULL;
    }
    return;
}

static void
eapttls_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    if (arg != NULL) {
	free(arg);
    }
    return;
}

static EAPPacketRef
EAPTTLSPacketCreateAck(int identifier)
{
    return (EAPTLSPacketCreate(kEAPCodeResponse, kEAPTypeTTLS,
			       identifier, 0, NULL, NULL));
}

static bool
eapttls_pap(EAPClientPluginDataRef plugin)
{
    DiameterAVP *	avp;
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    void *		data;
    int			data_length;
    void *		offset;
    size_t		length;
    int			password_length_r;
    bool		ret = TRUE;
    OSStatus		status;
    int			user_length_r;

    /* allocate buffer to hold message */
    password_length_r = roundup(plugin->password_length, 16);
    user_length_r = roundup(plugin->username_length, 4);
    data_length = sizeof(*avp) * 2 + user_length_r + password_length_r;
    data = malloc(data_length);
    if (data == NULL) {
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
	return (FALSE);
    }
    offset = data;

    /* User-Name AVP */
    avp = (DiameterAVP *)offset;
    avp->AVP_code = htonl(kRADIUSAttributeTypeUserName);
    avp->AVP_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(0, sizeof(*avp) 
					   + plugin->username_length));
    offset = (void *)(avp + 1);
    bcopy(plugin->username, offset, plugin->username_length);
    if (user_length_r > plugin->username_length) {
	bzero(offset + plugin->username_length,
	      user_length_r - plugin->username_length);
    }
    offset += user_length_r;

    /* Password-Name AVP */
    avp = (DiameterAVP *)offset;
    avp->AVP_code = htonl(kRADIUSAttributeTypeUserPassword);
    avp->AVP_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(0, 
					   sizeof(*avp) + password_length_r));
    offset = (void *)(avp + 1);
    bcopy(plugin->password, offset, plugin->password_length);
    if (password_length_r > plugin->password_length) {
	bzero(offset + plugin->password_length, 
	      password_length_r - plugin->password_length);
    }
    offset += password_length_r;
#if 0
    printf("\n----------PAP Raw AVP Data START\n");
    print_data(data, offset - data);
    printf("----------PAP Raw AVP Data END\n");
#endif
    status = SSLWrite(context->ssl_context, data, offset - data, &length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	ret = FALSE;
    }
    free(data);
    return (ret);
}

static bool
eapttls_eap_read_avp(EAPClientPluginDataRef plugin, int avp_code,
		     uint8_t * data, uint32_t * data_length)
{
    DiameterAVP 		avp;
    EAPTTLSPluginDataRef 	context = (EAPTTLSPluginDataRef)plugin->private;
    uint32_t			flags_length;
    size_t			len;
    bool			ret = FALSE;
    OSStatus			status = noErr;
    uint32_t			max_data_length = *data_length;

    while (status != errSSLWouldBlock) {
	len = 0;
	status = SSLRead(context->ssl_context, &avp, sizeof(avp), &len);
	if (status != noErr) {
	    if (status != errSSLWouldBlock) {
		EAPLOG_FL(LOG_NOTICE, "SSLRead failed, %s (%d)",
			  EAPSSLErrorString(status), (int)status);
	    }
	    goto done;
	}
	if (len != sizeof(avp)) {
	    EAPLOG_FL(LOG_NOTICE, "EAP AVP is invalid");
	    goto done;
	}
	flags_length
	    = DiameterAVPLengthFromFlagsLength(ntohl(avp.AVP_flags_length));
	if (flags_length <= sizeof(avp)) {
	    EAPLOG_FL(LOG_NOTICE, "EAP AVP is too short %d <= %d",
		      flags_length, (int)sizeof(avp));
	    goto done;
	}
	flags_length -= sizeof(avp);
	flags_length = roundup(flags_length, 4);
	if (flags_length > max_data_length) {
	    EAPLOG_FL(LOG_NOTICE, "EAP AVP is too large %d > %d",
		      flags_length, max_data_length);
	    goto done;
	}
	if (ret == FALSE && ntohl(avp.AVP_code) == avp_code) {
	    status = SSLRead(context->ssl_context, data, flags_length, &len);
	    if (status != noErr) {
		EAPLOG_FL(LOG_NOTICE, "SSLRead failed, %s (%d)",
			  EAPSSLErrorString(status), (int)status);
		goto done;
	    }
	    *data_length = (uint32_t)len;
	    ret = TRUE;
	}
	else {
	    uint8_t	temp_buf[flags_length];

	    status = SSLRead(context->ssl_context, temp_buf, flags_length,
			     &len);
	    if (status != noErr) {
		EAPLOG_FL(LOG_NOTICE, "SSLRead failed, %s (%d)",
			  EAPSSLErrorString(status), (int)status);
		goto done;
	    }
	}
    }

done:
    if (ret == FALSE) {
	context->last_ssl_error = status;
    }
    return ret;
}

/**
 ** EAP client module access convenience routines
 **/
static void
eap_client_free(EAPTTLSPluginDataRef context)
{
    if (context->eap.module != NULL) {
	EAPClientModulePluginFree(context->eap.module, 
				  &context->eap.plugin_data);
	context->eap.module = NULL;
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

static EAPType
eap_client_type(EAPTTLSPluginDataRef context)
{
    if (context->eap.module == NULL) {
	return (kEAPTypeInvalid);
    }
    return (EAPClientModulePluginEAPType(context->eap.module));
}

static __inline__ void
S_set_uint32(const uint32_t * v_p, uint32_t value)
{
    *((uint32_t *)v_p) = value;
    return;
}

static bool
eap_client_init(EAPClientPluginDataRef plugin, EAPType type)
{
    EAPTTLSPluginDataRef 	context = (EAPTTLSPluginDataRef)plugin->private;
    EAPClientModule *	module;

    context->eap.last_type = kEAPTypeInvalid;
    context->eap.last_type_name = NULL;

    if (context->eap.module != NULL) {
	EAPLOG(LOG_NOTICE, "eap_client_init: already initialized\n");
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
    *((CFDictionaryRef *)&context->eap.plugin_data.properties) 
	= plugin->properties;
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

static CFArrayRef
eap_client_require_properties(EAPTTLSPluginDataRef context)
{
    return (EAPClientModulePluginRequireProperties(context->eap.module,
						   &context->eap.plugin_data));
}

static CFDictionaryRef
eap_client_publish_properties(EAPTTLSPluginDataRef context)
{
    return (EAPClientModulePluginPublishProperties(context->eap.module,
						   &context->eap.plugin_data));
}

static EAPClientState
eap_client_process(EAPClientPluginDataRef plugin, EAPPacketRef in_pkt_p,
		   EAPPacketRef * out_pkt_p)
{
    EAPTTLSPluginDataRef 	context = (EAPTTLSPluginDataRef)plugin->private;
    EAPClientState 	cstate;

    context->eap.plugin_data.username = plugin->username;
    S_set_uint32(&context->eap.plugin_data.username_length, 
		 plugin->username_length);
    context->eap.plugin_data.password = plugin->password;
    S_set_uint32(&context->eap.plugin_data.password_length, 
		 plugin->password_length);
    S_set_uint32(&context->eap.plugin_data.generation, 
		 plugin->generation);
    *((CFDictionaryRef *)&context->eap.plugin_data.properties) 
	= plugin->properties;
    cstate = EAPClientModulePluginProcess(context->eap.module,
					  &context->eap.plugin_data,
					  in_pkt_p, out_pkt_p,
					  &context->eap.last_status, 
					  &context->eap.last_error);
    return (cstate);
}

static void
eap_client_free_packet(EAPTTLSPluginDataRef context, EAPPacketRef out_pkt_p)
{
    EAPClientModulePluginFreePacket(context->eap.module, 
				    &context->eap.plugin_data,
				    out_pkt_p);
}

/**
 ** TTLS EAP processing
 **/

static bool
is_supported_type(EAPType type)
{
    int			i;

    for (i = 0; i < inner_auth_types_count; i++) {
	if (inner_auth_types[i] == type) {
	    return (TRUE);
	}
    }
    return (FALSE);
}

static EAPType
next_eap_type(EAPTTLSPluginDataRef context)
{
    if (context->last_eap_type_index >= inner_auth_types_count) {
	return (kEAPTypeInvalid);
    }
    return (inner_auth_types[context->last_eap_type_index++]);
}

bool
eapttls_eap_start(EAPClientPluginDataRef plugin, int identifier)
{
    DiameterAVP *	avp;
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    void *		data;
    int			data_length;
    int			data_length_r;
    void *		offset;
    size_t		length;
    EAPResponsePacket *	resp_p = NULL;
    bool		ret = TRUE;
    OSStatus		status;

    /* allocate buffer to hold message */
    data_length = sizeof(*avp) + plugin->username_length + sizeof(*resp_p);
    data_length_r = roundup(data_length, 4);
    data = malloc(data_length_r);
    if (data == NULL) {
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
	return (FALSE);
    }
    offset = data;

    /* EAP AVP */
    avp = (DiameterAVP *)offset;
    avp->AVP_code = htonl(kRADIUSAttributeTypeEAPMessage);
    avp->AVP_flags_length = htonl(DiameterAVPMakeFlagsLength(0, data_length));
    offset = (void *)(avp + 1);
    resp_p = (EAPResponsePacket *)offset;
    resp_p->code = kEAPCodeResponse;
    resp_p->identifier = 0; /* identifier */
    EAPPacketSetLength((EAPPacketRef)resp_p, 
		       sizeof(*resp_p) + plugin->username_length);

    resp_p->type = kEAPTypeIdentity;
    bcopy(plugin->username, resp_p->type_data, plugin->username_length);
    offset += sizeof(*resp_p) + plugin->username_length;
    if (data_length_r > data_length) {
	bzero(offset, data_length_r - data_length);
    }
    if (plugin->log_enabled) {
	CFMutableStringRef		log_msg;

	log_msg = CFStringCreateMutable(NULL, 0);
	EAPPacketIsValid((const EAPPacketRef)resp_p,
			 EAPPacketGetLength((const EAPPacketRef)resp_p),
			 log_msg);
	EAPLOG(-LOG_DEBUG, "TTLS Send EAP Payload:\n%@", log_msg);
	CFRelease(log_msg);
    }

#if 0
    printf("offset - data %d length %d\n", offset - data, data_length);
#endif /* 0 */
    status = SSLWrite(context->ssl_context, data, data_length_r, &length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	ret = FALSE;
    }
    free(data);
    return (ret);
}

EAPResponsePacketRef
eapttls_eap_process(EAPClientPluginDataRef plugin, EAPRequestPacketRef in_pkt_p,
		    char * out_buf, int * out_buf_size, 
		    EAPClientStatus * client_status,
		    bool * call_module_free_packet)
{
    EAPTTLSPluginDataRef	context = (EAPTTLSPluginDataRef)plugin->private;
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
	    if (is_supported_type(in_pkt_p->type) == FALSE) {
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
	    eap_client_free(context);
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
		save_last_packet(context, (EAPPacketRef)in_pkt_p);
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
	    save_last_packet(context, (EAPPacketRef)in_pkt_p);
	    *client_status = context->last_client_status =
		context->eap.last_status;
	}
	break;
    case kEAPClientStateSuccess:
	/* authentication method succeeded */
	context->inner_auth_state = kEAPInnerAuthStateSuccess;
	break;
    case kEAPClientStateFailure:
	/* authentication method failed */
	context->inner_auth_state = kEAPInnerAuthStateFailure;
	*client_status = context->eap.last_status;
	//context->plugin_state = kEAPClientStateFailure;
	break;
    }

 done:
    return (out_pkt_p);
}

PRIVATE_EXTERN bool
eapttls_eap(EAPClientPluginDataRef plugin, EAPTLSPacketRef eaptls_in,
	    EAPClientStatus * client_status)
{
    DiameterAVP *	avp_p;
    bool 		call_module_free_packet = FALSE;
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    uint8_t		in_buf[2048];
    uint32_t		in_data_size = 0;
    EAPRequestPacketRef	in_pkt_p;
    char 		out_buf[2048];
    void *		out_data;
    int			out_data_size;
    int			out_data_size_r = 0;
    size_t		out_data_size_ret;
    EAPResponsePacketRef out_pkt_p = NULL;
    int			out_pkt_size;
    memoryBufferRef	read_buf = &context->read_buffer;
    bool		ret = FALSE;
    OSStatus		status;

    if (eaptls_in->identifier == context->previous_identifier) {
	/* we've already seen this packet */
	memoryBufferClear(read_buf);
	if (context->last_packet == NULL) {
	    return (FALSE);
	}
	/* use the remembered packet */
	in_pkt_p = (EAPRequestPacketRef)context->last_packet;
    }
    else {
	CFMutableStringRef	log_msg = NULL;
	bool			is_valid;
	bool			found_avp = FALSE;

	in_data_size = sizeof(in_buf);
	found_avp = eapttls_eap_read_avp(plugin,
					 kRADIUSAttributeTypeEAPMessage,
					 in_buf, &in_data_size);
	if (found_avp == TRUE) {
	    in_pkt_p = (EAPRequestPacketRef)(in_buf);
	    log_msg = plugin->log_enabled
		? CFStringCreateMutable(NULL, 0) : NULL;
	    is_valid = EAPPacketIsValid((EAPPacketRef)in_pkt_p, in_data_size,
					log_msg);
	    if (log_msg != NULL) {
		EAPLOG(-LOG_DEBUG, "TTLS Receive EAP Payload%s:\n%@",
		       is_valid ? "" : " Invalid", log_msg);
		CFRelease(log_msg);
	    }
	    if (is_valid == FALSE) {
		if (plugin->log_enabled == FALSE) {
		    EAPLOG(LOG_NOTICE, "TTLS Receive EAP Payload Invalid");
		}
		goto done;
	    }
	}
	else {
	    EAPLOG(LOG_NOTICE, "TTLS EAP Payload is missing");
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
    }
    out_pkt_size = sizeof(out_buf);
    switch (in_pkt_p->code) {
    case kEAPCodeRequest:
	switch (in_pkt_p->type) {
	case kEAPTypeIdentity:
	    out_pkt_p = (EAPResponsePacketRef)
		EAPPacketCreate(out_buf, out_pkt_size, 
				kEAPCodeResponse, in_pkt_p->identifier,
				kEAPTypeIdentity, plugin->username,
				plugin->username_length, 
				&out_pkt_size);
	    break;
	case kEAPTypeNotification:
	    out_pkt_p = (EAPResponsePacketRef)
		EAPPacketCreate(out_buf, out_pkt_size, 
				kEAPCodeResponse, in_pkt_p->identifier,
				kEAPTypeNotification, NULL, 0, 
				&out_pkt_size);
	    break;
	default:
	    out_pkt_p = eapttls_eap_process(plugin, in_pkt_p,
					    out_buf, &out_pkt_size,
					    client_status,
					    &call_module_free_packet);
	    break;
	}
	break;
    case kEAPCodeResponse:
	/* we shouldn't really be processing EAP Responses */
	out_pkt_p = eapttls_eap_process(plugin, in_pkt_p,
					out_buf, &out_pkt_size,
					client_status,
					&call_module_free_packet);
	break;
    case kEAPCodeSuccess:
	out_pkt_p = eapttls_eap_process(plugin, in_pkt_p,
					out_buf, &out_pkt_size,
					client_status,
					&call_module_free_packet);
	break;
    case kEAPCodeFailure:
	out_pkt_p = eapttls_eap_process(plugin, in_pkt_p,
					out_buf, &out_pkt_size,
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
	EAPLOG(-LOG_DEBUG, "TTLS Send EAP Payload:\n%@", log_msg);
	CFRelease(log_msg);
    }

    free_last_packet(context);

    /* need to form complete packet */
    out_data_size = out_pkt_size + sizeof(*avp_p);
    out_data_size_r = roundup(out_data_size, 4);
    out_data = malloc(out_data_size_r);
    bzero(out_data, out_data_size_r);
    avp_p = (DiameterAVP *)out_data;
    avp_p->AVP_code = htonl(kRADIUSAttributeTypeEAPMessage);
    avp_p->AVP_flags_length
	= htonl(DiameterAVPMakeFlagsLength(0, out_data_size));
    bcopy(out_pkt_p, out_data + sizeof(*avp_p), out_pkt_size);

    status = SSLWrite(context->ssl_context, out_data,
		      out_data_size_r, &out_data_size_ret);
    free(out_data);
    if ((char *)out_pkt_p != out_buf) {
	if (call_module_free_packet) {
	    eap_client_free_packet(context, (EAPPacketRef)out_pkt_p);
	}
	else {
	    free(out_pkt_p);
	}
    }
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, 
		  "SSLWrite failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
    }
    else {
	ret = TRUE;
    }

 done:
    return (ret);
}


/*
 * Function: eapttls_chap
 * Purpose:
 *   Generate a packet containing the response to an implicit
 *   CHAP challenge.
 */

static bool
eapttls_chap(EAPClientPluginDataRef plugin)
{
    DiameterAVP *	avp;
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    void *		data;
    int			data_length;
    int			data_length_r;
    uint8_t		key_data[17];
    size_t		length;
    void *		offset;
    bool		ret = TRUE;
    OSStatus		status;
    int			user_length_r;

    user_length_r = roundup(plugin->username_length, 4);
    status = EAPTLSComputeKeyData(context->ssl_context, 
				  kEAPTTLSChallengeLabel,
				  kEAPTTLSChallengeLabelLength,
				  key_data, sizeof(key_data));
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "EAPTLSComputeKeyData failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	return (FALSE);
    }

    /* allocate buffer to hold message */
    data_length = sizeof(*avp) * 3 
	+ user_length_r
	+ 16  /* challenge */ 
	+ 1 + 16; /* identifier + response */
    data_length_r = roundup(data_length, 4);
    data = malloc(data_length_r);
    if (data == NULL) {
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
	return (FALSE);
    }
    offset = data;

    /* User-Name AVP */
    avp = (DiameterAVP *)offset;
    avp->AVP_code = htonl(kRADIUSAttributeTypeUserName);
    avp->AVP_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(0, sizeof(*avp) 
					   + plugin->username_length));
    offset = (void *)(avp + 1);
    bcopy(plugin->username, offset, plugin->username_length);
    if (user_length_r > plugin->username_length) {
	bzero(offset + plugin->username_length,
	      user_length_r - plugin->username_length);
    }
    offset += user_length_r;

    /* CHAP-Challenge AVP */
    avp = (DiameterAVP *)offset;
    avp->AVP_code = htonl(kRADIUSAttributeTypeCHAPChallenge);
    avp->AVP_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(0, sizeof(*avp) + 16));
    offset = (void *)(avp + 1);
    bcopy(key_data, offset, 16);
    offset += 16;

    /* CHAP-Password AVP */
    avp = (DiameterAVP *)offset;
    avp->AVP_code = htonl(kRADIUSAttributeTypeCHAPPassword);
    avp->AVP_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(0, sizeof(*avp) + 17));
    offset = (void *)(avp + 1);
    *((u_char *)offset) = key_data[16]; /* identifier */
    offset++;
    chap_md5(key_data[16], plugin->password, plugin->password_length, 
	     key_data, 16, offset);
    offset += 16;
    { /* pad out with zeroes */
	int 	pad_length = data_length_r - data_length;
	if (pad_length != 0) {
	    bzero(offset, pad_length);
	    offset += pad_length;
	}
    }
#if 0
    printf("\n----------CHAP Raw AVP Data START\n");
    print_data(data, offset - data);
    printf("----------CHAP Raw AVP Data END\n");
#endif /* 0 */
    status = SSLWrite(context->ssl_context, data, offset - data, &length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	ret = FALSE;
    }
    free(data);
    return (ret);
}

/*
 * Function: eapttls_mschap
 * Purpose:
 *   Generate a packet containing the response to an implicit
 *   MS-CHAP challenge.
 */

static bool
eapttls_mschap(EAPClientPluginDataRef plugin)
{
    DiameterAVP *	avp;
    DiameterVendorAVP *	avpv;
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    void *		data;
    int			data_length;
    int			data_length_r;
    uint8_t		key_data[MSCHAP_NT_CHALLENGE_SIZE + MSCHAP_IDENT_SIZE];
    size_t		length;
    void *		offset;
    bool		ret = TRUE;
    uint8_t		response[MSCHAP_NT_RESPONSE_SIZE];
    OSStatus		status;
    int			user_length_r;

    user_length_r = roundup(plugin->username_length, 4);
    status = EAPTLSComputeKeyData(context->ssl_context, 
				  kEAPTTLSChallengeLabel,
				  kEAPTTLSChallengeLabelLength,
				  key_data, sizeof(key_data));
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "EAPTLSComputeKeyData failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	return (FALSE);
    }

    /* allocate buffer to hold message */
    data_length = sizeof(*avp) + sizeof(*avpv) * 2 
	+ user_length_r
	+ MSCHAP_NT_CHALLENGE_SIZE  /* challenge */ 
	+ TTLS_MSCHAP_RESPONSE_LENGTH;/* flags + identifer + {NT,LM}Response */
    data_length_r = roundup(data_length, 4);
    data = malloc(data_length_r);
    if (data == NULL) {
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
	return (FALSE);
    }
    offset = data;

    /* User-Name AVP */
    avp = (DiameterAVP *)offset;
    avp->AVP_code = htonl(kRADIUSAttributeTypeUserName);
    avp->AVP_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(0, sizeof(*avp) 
					   + plugin->username_length));
    offset = (void *)(avp + 1);
    bcopy(plugin->username, offset, plugin->username_length);
    if (user_length_r > plugin->username_length) {
	bzero(offset + plugin->username_length, 
	      user_length_r - plugin->username_length);
    }
    offset += user_length_r;

    /* MS-CHAP-Challenge AVP */
    avpv = (DiameterVendorAVP *)offset;
    avpv->AVPV_code = htonl(kMSRADIUSAttributeTypeMSCHAPChallenge);
    avpv->AVPV_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(kDiameterFlagsVendorSpecific,
					   sizeof(*avpv) 
					   + MSCHAP_NT_CHALLENGE_SIZE));
    avpv->AVPV_vendor = htonl(kRADIUSVendorIdentifierMicrosoft);
    offset = (void *)(avpv + 1);
    bcopy(key_data, offset, MSCHAP_NT_CHALLENGE_SIZE);
    offset += MSCHAP_NT_CHALLENGE_SIZE;

    /* MS-CHAP-Response AVP */
    avpv = (DiameterVendorAVP *)offset;
    avpv->AVPV_code = htonl(kMSRADIUSAttributeTypeMSCHAPResponse);
    avpv->AVPV_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(kDiameterFlagsVendorSpecific,
					   sizeof(*avpv) 
					   + TTLS_MSCHAP_RESPONSE_LENGTH));
    avpv->AVPV_vendor = htonl(kRADIUSVendorIdentifierMicrosoft);
    offset = (void *)(avpv + 1);
    *((u_char *)offset) = key_data[MSCHAP_NT_CHALLENGE_SIZE];	/* ident */
    offset++;
    *((u_char *)offset) = 1;		   /* flags: 1 = use NT-Response */
    offset++;
    bzero(offset, MSCHAP_LM_RESPONSE_SIZE);/* LM-Response: not used */
    offset += MSCHAP_LM_RESPONSE_SIZE;
    MSChap(key_data, plugin->password,
	   plugin->password_length, response);		/* NT-Response */
    bcopy(response, offset, MSCHAP_NT_RESPONSE_SIZE);
    offset += MSCHAP_NT_RESPONSE_SIZE;
    { /* pad out with zeroes */
	int 	pad_length = data_length_r - data_length;
	if (pad_length != 0) {
	    bzero(offset, pad_length);
	    offset += pad_length;
	}
    }
#if 0
    printf("offset - data %d length %d\n", offset - data, data_length);
    printf("\n----------MSCHAP Raw AVP Data START\n");
    print_data(data, offset - data);
    printf("----------MSCHAP Raw AVP Data END\n");
#endif /* 0 */
    status = SSLWrite(context->ssl_context, data, offset - data, &length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	ret = FALSE;
    }
    free(data);
    return (ret);
}

/*
 * Function: eapttls_mschapv2
 * Purpose:
 *   Generate a packet containing the response to an implicit
 *   MS-CHAPv2 challenge.
 */

static bool
eapttls_mschap2(EAPClientPluginDataRef plugin)
{
    DiameterAVP *	avp;
    DiameterVendorAVP *	avpv;
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    void *		data;
    int			data_length;
    int			data_length_r;
    size_t		length;
    void *		offset;
    bool		ret = TRUE;
    OSStatus		status;
    int			user_length_r;

    user_length_r = roundup(plugin->username_length, 4);
    status = EAPTLSComputeKeyData(context->ssl_context, 
				  kEAPTTLSChallengeLabel,
				  kEAPTTLSChallengeLabelLength,
				  context->auth_challenge_id,
				  MSCHAP2_CHALLENGE_SIZE + 1);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "EAPTLSComputeKeyData failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	return (FALSE);
    }

    /* allocate buffer to hold message */
    data_length = sizeof(*avp) + sizeof(*avpv) * 2 
	+ user_length_r
	+ MSCHAP2_CHALLENGE_SIZE
	+ TTLS_MSCHAP2_RESPONSE_LENGTH;
    data_length_r = roundup(data_length, 4);
    data = malloc(data_length_r);
    if (data == NULL) {
	EAPLOG_FL(LOG_NOTICE, "malloc failed");
	return (FALSE);
    }
    offset = data;

    /* User-Name AVP */
    avp = (DiameterAVP *)offset;
    avp->AVP_code = htonl(kRADIUSAttributeTypeUserName);
    avp->AVP_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(0, sizeof(*avp)
					   + plugin->username_length));
    offset = (void *)(avp + 1);
    bcopy(plugin->username, offset, plugin->username_length);
    if (user_length_r > plugin->username_length) {
	bzero(offset + plugin->username_length, 
	      user_length_r - plugin->username_length);
    }
    offset += user_length_r;

    /* MS-CHAP-Challenge AVP */
    avpv = (DiameterVendorAVP *)offset;
    avpv->AVPV_code = htonl(kMSRADIUSAttributeTypeMSCHAPChallenge);
    avpv->AVPV_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(kDiameterFlagsVendorSpecific,
					   sizeof(*avpv) 
					   + MSCHAP2_CHALLENGE_SIZE));
    avpv->AVPV_vendor = htonl(kRADIUSVendorIdentifierMicrosoft);
    offset = (void *)(avpv + 1);
    bcopy(context->auth_challenge_id, offset, MSCHAP2_CHALLENGE_SIZE);
    offset += MSCHAP2_CHALLENGE_SIZE;

    /* MS-CHAP2-Response AVP */
    avpv = (DiameterVendorAVP *)offset;
    avpv->AVPV_code = htonl(kMSRADIUSAttributeTypeMSCHAP2Response);
    avpv->AVPV_flags_length 
	= htonl(DiameterAVPMakeFlagsLength(kDiameterFlagsVendorSpecific,
					   sizeof(*avpv) 
					   + TTLS_MSCHAP2_RESPONSE_LENGTH));
    avpv->AVPV_vendor = htonl(kRADIUSVendorIdentifierMicrosoft);
    offset = (void *)(avpv + 1);
    *((u_char *)offset) 		/* identifier */
	= context->auth_challenge_id[MSCHAP2_CHALLENGE_SIZE];
    offset++;
    *((u_char *)offset) = 0;		/* flags: must be 0 */
    offset++;
    MSChapFillWithRandom(context->peer_challenge, 
			 sizeof(context->peer_challenge));
    bcopy(context->peer_challenge, offset,
	  MSCHAP2_CHALLENGE_SIZE);	/* peer challenge */
    offset += sizeof(context->peer_challenge);
    bzero(offset, MSCHAP2_RESERVED_SIZE);
    offset += MSCHAP2_RESERVED_SIZE;
    MSChap2(context->auth_challenge_id, context->peer_challenge, 	
	    plugin->username, plugin->password, plugin->password_length,
	    context->nt_response);
    bcopy(context->nt_response, offset, 	/* response */
	  MSCHAP_NT_RESPONSE_SIZE);
    offset += MSCHAP_NT_RESPONSE_SIZE;
    { /* pad out with zeroes */
	int 	pad_length = data_length_r - data_length;
	if (pad_length != 0) {
	    bzero(offset, pad_length);
	    offset += pad_length;
	}
    }
#if 0
    printf("offset - data %d length %d\n", offset - data, data_length);
    printf("\n----------MSCHAP2 Raw AVP Data START\n");
    print_data(data, offset - data);
    printf("----------MSCHAP2 Raw AVP Data END\n");
#endif /* 0 */

    status = SSLWrite(context->ssl_context, data, offset - data, &length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	ret = FALSE;
    }
    free(data);
    return (ret);
}

static EAPPacketRef
eapttls_mschap2_verify(EAPClientPluginDataRef plugin, int identifier)
{
    DiameterVendorAVP	avpv;
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    void *		data = NULL;
    size_t		data_size = 0;
    u_int32_t		flags_length;
    size_t		length;
    EAPPacketRef	pkt = NULL;
    OSStatus		status;

    status = SSLRead(context->ssl_context, &avpv, sizeof(avpv), 
		     &data_size);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SSLRead failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	context->plugin_state = kEAPClientStateFailure;
	context->last_ssl_error = status;
	goto done;
    }
    if (data_size != sizeof(avpv)) {
	context->plugin_state = kEAPClientStateFailure;
	goto done;
    }
    flags_length = ntohl(avpv.AVPV_flags_length);
    if ((DiameterAVPFlagsFromFlagsLength(flags_length) 
	 & kDiameterFlagsVendorSpecific) == 0
	|| ntohl(avpv.AVPV_code) != kMSRADIUSAttributeTypeMSCHAP2Success
	|| ntohl(avpv.AVPV_vendor) != kRADIUSVendorIdentifierMicrosoft) {
	context->plugin_state = kEAPClientStateFailure;
	goto done;
    }
    length = DiameterAVPLengthFromFlagsLength(flags_length);
    if (length > kEAPTLSAvoidDenialOfServiceSize) {
	context->plugin_state = kEAPClientStateFailure;
	goto done;
    }
    length -= sizeof(avpv);
    data = malloc(length);
    status = SSLRead(context->ssl_context, data, length, &length);
    if (status != noErr) {
	context->plugin_state = kEAPClientStateFailure;
	goto done;
    }
    if (length < (MSCHAP2_AUTH_RESPONSE_SIZE + 1)
	|| (*((uint8_t *)data)
	    != context->auth_challenge_id[MSCHAP2_CHALLENGE_SIZE])) {
	context->plugin_state = kEAPClientStateFailure;
	goto done;
    }
    if (MSChap2AuthResponseValid(plugin->password, plugin->password_length,
				 context->nt_response,
				 context->peer_challenge,
				 context->auth_challenge_id,
				 plugin->username, data + 1) == FALSE) {
	context->plugin_state = kEAPClientStateFailure;
	goto done;
    }
    pkt = EAPTTLSPacketCreateAck(identifier);
 done:
    if (data != NULL) {
	free(data);
    }
    return (pkt);
}

static EAPPacketRef
eapttls_do_inner_auth(EAPClientPluginDataRef plugin,
		      int identifier, EAPClientStatus * client_status)
{
    EAPTTLSPluginDataRef	context = (EAPTTLSPluginDataRef)plugin->private;
    memoryBufferRef		write_buf = &context->write_buffer; 

    context->authentication_started = TRUE;
    switch (context->inner_auth_type) {
    case kInnerAuthTypePAP:
	if (eapttls_pap(plugin) == FALSE) {
	    /* do something */
	}
	break;
    case kInnerAuthTypeCHAP:
	if (eapttls_chap(plugin) == FALSE) {
	    /* do something */
	}
	break;
    case kInnerAuthTypeMSCHAP:
	if (eapttls_mschap(plugin) == FALSE) {
	    /* do something */
	}
	break;
    case kInnerAuthTypeEAP:
	if (eapttls_eap_start(plugin, identifier) == FALSE) {
	    /* do something */
	}
	break;
    default:
    case kInnerAuthTypeMSCHAPv2:
	if (eapttls_mschap2(plugin) == FALSE) {
	    /* do something */
	}
	break;
    }
    return (EAPTLSPacketCreate(kEAPCodeResponse,
			       kEAPTypeTTLS,
			       identifier,
			       context->mtu,
			       write_buf,
			       &context->last_write_size));
}

static EAPPacketRef
eapttls_start_inner_auth(EAPClientPluginDataRef plugin,
			 int identifier, EAPClientStatus * client_status)
{
    EAPTTLSPluginDataRef	context = (EAPTTLSPluginDataRef)plugin->private;
    memoryBufferRef		write_buf = &context->write_buffer; 

    if (context->session_was_resumed == FALSE) {
	return (eapttls_do_inner_auth(plugin, identifier, client_status));
    }
    return (EAPTLSPacketCreate(kEAPCodeResponse,
			       kEAPTypeTTLS,
			       identifier,
			       context->mtu,
			       write_buf,
			       &context->last_write_size));
}

static EAPPacketRef
eapttls_verify_server(EAPClientPluginDataRef plugin,
		      int identifier, EAPClientStatus * client_status)
{
    EAPTTLSPluginDataRef 	context = (EAPTTLSPluginDataRef)plugin->private;
    EAPPacketRef		pkt = NULL;
    memoryBufferRef		write_buf = &context->write_buffer;

    context->trust_status
	= EAPTLSVerifyServerCertificateChain(plugin->properties, 
					     context->server_certs,
					     &context->trust_ssl_error);
    if (context->trust_status != kEAPClientStatusOK) {
	EAPLOG_FL(LOG_NOTICE, "server certificate not trusted status %d %d",
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
				 kEAPTypeTTLS,
				 identifier,
				 context->mtu,
				 write_buf,
				 &context->last_write_size);
	break;
    }
    return (pkt);
}

static EAPPacketRef
eapttls_tunnel(EAPClientPluginDataRef plugin,
	       EAPTLSPacketRef eaptls_in,
	       EAPClientStatus * client_status)
{
    EAPTTLSPluginDataRef 	context = (EAPTTLSPluginDataRef)plugin->private;
    int 			identifier = eaptls_in->identifier;
    EAPPacketRef		pkt = NULL;
    memoryBufferRef		read_buf = &context->read_buffer;

    if (context->authentication_started == FALSE) {
	if (identifier == context->previous_identifier) {
	    /* we've already seen this packet, discard buffer contents */
	    memoryBufferClear(read_buf);
	}
	if (plugin->password == NULL) {
	    *client_status = kEAPClientStatusUserInputRequired;
	    return (NULL);
	}
	return (eapttls_start_inner_auth(plugin, identifier, client_status));
    }
    switch (context->inner_auth_type) {
    default:
	/* we didn't expect data, Ack it anyways */
	pkt = EAPTTLSPacketCreateAck(identifier);
	break;
    case kInnerAuthTypeMSCHAPv2:
	if (identifier == context->previous_identifier) {
	    /* we've already verified the MSCHAP2 response, just Ack it */
	    pkt = EAPTTLSPacketCreateAck(identifier);
	}
	else {
	    pkt = eapttls_mschap2_verify(plugin, identifier);
	}
	break;
    case kInnerAuthTypeEAP:
	if (eapttls_eap(plugin, eaptls_in, client_status)) {
	    memoryBufferRef	write_buf = &context->write_buffer;

	    pkt = EAPTLSPacketCreate(kEAPCodeResponse,
				     kEAPTypeTTLS,
				     eaptls_in->identifier,
				     context->mtu,
				     write_buf,
				     &context->last_write_size);
	}
	break;
    }
    return (pkt);
}

static void
eapttls_set_session_was_resumed(EAPTTLSPluginDataRef context)
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

static EAPPacketRef
eapttls_handshake(EAPClientPluginDataRef plugin,
		  int identifier, EAPClientStatus * client_status)

{
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    EAPPacketRef	eaptls_out = NULL;
    OSStatus		status = noErr;
    memoryBufferRef	write_buf = &context->write_buffer; 

    if (context->server_auth_completed && context->trust_proceed == FALSE) {
	eaptls_out = eapttls_verify_server(plugin, identifier, client_status);
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
	eaptls_out = eapttls_verify_server(plugin, identifier, client_status);
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
	    eaptls_out = eapttls_verify_server(plugin, identifier,
					       client_status);
	    if (context->trust_proceed == FALSE) {
		/* this should not happen */
		EAPLOG_FL(LOG_NOTICE, "trust_proceed is FALSE?");
		break;
	    }
	}
	context->handshake_complete = TRUE;
	eapttls_compute_session_key(context);
	eapttls_set_session_was_resumed(context);
	if (plugin->password == NULL) {
	    *client_status = context->last_client_status 
		= kEAPClientStatusUserInputRequired;
	    break;
	}
	eaptls_out = eapttls_start_inner_auth(plugin,
					      identifier, client_status);
	break;
    default:
	EAPLOG_FL(LOG_NOTICE, "SSLHandshake failed, %s (%ld)",
		  EAPSSLErrorString(status), (long)status);
	context->last_ssl_error = status;
	my_CFRelease(&context->server_certs);
	(void) EAPSSLCopyPeerCertificates(context->ssl_context,
					  &context->server_certs);
	/* close_up_shop */
	context->plugin_state = kEAPClientStateFailure;
	SSLClose(context->ssl_context);
	/* FALL THROUGH */
    case errSSLWouldBlock:
	if (write_buf->data == NULL) {
	    if (status == errSSLFatalAlert) {
		/* send an ACK if we received a fatal alert message */
		eaptls_out 
		    = EAPTTLSPacketCreateAck(identifier);
	    }
	}
	else {
	    eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					    kEAPTypeTTLS,
					    identifier,
					    context->mtu,
					    write_buf,
					    &context->last_write_size);
	}
	break;
    }

 done:
    return (eaptls_out);
}

static EAPPacketRef
eapttls_request(EAPClientPluginDataRef plugin,
		const EAPPacketRef in_pkt,
		EAPClientStatus * client_status)
{
    EAPTTLSPluginDataRef	context = (EAPTTLSPluginDataRef)plugin->private;
    EAPTLSPacketRef 		eaptls_in = (EAPTLSPacket *)in_pkt; 
    EAPTLSLengthIncludedPacket *eaptls_in_l;
    EAPPacketRef		eaptls_out = NULL;
    int				in_data_length;
    void *			in_data_ptr = NULL;
    u_int16_t			in_length = EAPPacketGetLength(in_pkt);
    memoryBufferRef		write_buf = &context->write_buffer; 
    memoryBufferRef		read_buf = &context->read_buffer;
    SSLSessionState		ssl_state = kSSLIdle;
    OSStatus			status = noErr;
    u_int32_t			tls_message_length = 0;
    RequestType			type;

    /* ALIGN: void * cast OK, we don't expect proper alignment */
    eaptls_in_l = (EAPTLSLengthIncludedPacket *)(void *)in_pkt;
    if (in_length < sizeof(*eaptls_in)) {
	EAPLOG_FL(LOG_NOTICE, "length %d < %ld",
		  in_length, sizeof(*eaptls_in));
	goto done;
    }
    if (context->ssl_context != NULL) {
	status = SSLGetSessionState(context->ssl_context, &ssl_state);
	if (status != noErr) {
	    EAPLOG_FL(LOG_NOTICE, "SSLGetSessionState failed, %s (%ld)",
		      EAPSSLErrorString(status), (long)status);
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
	in_data_ptr = eaptls_in_l->tls_data;
	in_data_length = in_length - sizeof(EAPTLSLengthIncludedPacket);
	tls_message_length 
	    = EAPTLSLengthIncludedPacketGetMessageLength(eaptls_in_l);
	if (tls_message_length > kEAPTLSAvoidDenialOfServiceSize) {
	    EAPLOG_FL(LOG_NOTICE, 
		      "received message too large, %d > %d",
		      tls_message_length, kEAPTLSAvoidDenialOfServiceSize);
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
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
	    EAPLOG_FL(LOG_NOTICE, 
		      "ignoring non TTLS start frame");
	    goto done;
	}
	status = eapttls_start(plugin);
	if (status != noErr) {
	    context->last_ssl_error = status;
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
	status = SSLHandshake(context->ssl_context);
	if (status != errSSLWouldBlock) {
	    EAPLOG_FL(LOG_NOTICE, "SSLHandshake failed, %s (%ld)",
		      EAPSSLErrorString(status), (long)status);
	    context->last_ssl_error = status;
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
	eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					kEAPTypeTTLS,
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
						kEAPTypeTTLS,
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
						kEAPTypeTTLS,
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
	    EAPTTLSPluginDataRef context;

	    context = (EAPTTLSPluginDataRef)plugin->private;
	    if (ssl_state != kSSLConnected
		|| context->session_was_resumed == FALSE
		|| context->trust_proceed == FALSE
		|| context->authentication_started == TRUE) {
		EAPLOG_FL(LOG_NOTICE, "unexpected %s frame",
			  type == kRequestTypeAck ? "Ack" : "Start");
		goto done;
	    }
	    /* server is forcing us to re-auth even though we resumed */
	    EAPLOG(LOG_NOTICE, "server forcing re-auth after resume");
	    eaptls_out
		= eapttls_do_inner_auth(plugin, eaptls_in->identifier,
					client_status);
	    break;
	}
	if (in_pkt->identifier == context->previous_identifier) {
	    if ((eaptls_in->flags & kEAPTLSPacketFlagsMoreFragments) != 0) {
		/* just ack it, we've already seen the fragment */
		eaptls_out = EAPTTLSPacketCreateAck(eaptls_in->identifier);
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
		    goto done;
		}
		/* we haven't received the entire TLS message */
		eaptls_out = EAPTTLSPacketCreateAck(eaptls_in->identifier);
		break;
	    }
	}
	/* we've got the whole TLS message, process it */
	if (context->handshake_complete) {
	    /* subsequent request */
	    eaptls_out = eapttls_tunnel(plugin,
					eaptls_in,
					client_status);
	}
	else {
	    eaptls_out = eapttls_handshake(plugin,
					   eaptls_in->identifier,
					   client_status);
	}
	break;
    default:
	break;
    }

    context->previous_identifier = in_pkt->identifier;
 done:
    return (eaptls_out);
}

static EAPClientState
eapttls_process(EAPClientPluginDataRef plugin,
		const EAPPacketRef in_pkt,
		EAPPacketRef * out_pkt_p, 
		EAPClientStatus * client_status,
		EAPClientDomainSpecificError * error)
{
    EAPTTLSPluginDataRef	context = (EAPTTLSPluginDataRef)plugin->private;

    *client_status = kEAPClientStatusOK;
    *error = 0;

    *out_pkt_p = NULL;
    switch (in_pkt->code) {
    case kEAPCodeRequest:
	*out_pkt_p = eapttls_request(plugin, in_pkt, client_status);
	break;
    case kEAPCodeSuccess:
	if (context->handshake_complete && context->trust_proceed == FALSE) {
	    *out_pkt_p 
		= eapttls_verify_server(plugin, in_pkt->identifier, 
					client_status);
	}
	if (context->trust_proceed) {
	    context->plugin_state = kEAPClientStateSuccess;
	}
	break;
    case kEAPCodeFailure:
	context->plugin_state = kEAPClientStateFailure;
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

static const char * 
eapttls_failure_string(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

static void * 
eapttls_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;

    *key_length = 0;
    if (context->key_data_valid == FALSE) {
	return (NULL);
    }

    /* return the first 32 bytes of key data */
    *key_length = 32;
    return (context->key_data);
}

static void * 
eapttls_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;

    *key_length = 0;
    if (context->key_data_valid == FALSE) {
	return (NULL);
    }

    /* return the second 32 bytes of key data */
    *key_length = 32;
    return (context->key_data + 32);
}

static int
eapttls_msk_copy_bytes(EAPClientPluginDataRef plugin, 
		      void * msk, int msk_size)
{
    EAPTTLSPluginDataRef 	context = (EAPTTLSPluginDataRef)plugin->private;
    int				ret_msk_size;

    if (msk_size < kEAPMasterSessionKeyMinimumSize
	|| context->key_data_valid == FALSE) {
	ret_msk_size = 0;
    }
    else {
	ret_msk_size = kEAPMasterSessionKeyMinimumSize;
	bcopy(context->key_data, msk, ret_msk_size);
    }
    return (ret_msk_size);
}

static CFArrayRef
eapttls_require_props(EAPClientPluginDataRef plugin)
{
    CFArrayRef 			array = NULL;
    EAPTTLSPluginDataRef	context = (EAPTTLSPluginDataRef)plugin->private;

    if (context->last_client_status != kEAPClientStatusUserInputRequired) {
	goto done;
    }
    if (context->trust_proceed == FALSE) {
	CFStringRef	str = kEAPClientPropTLSUserTrustProceedCertificateChain;
	array = CFArrayCreate(NULL, (const void **)&str,
			      1, &kCFTypeArrayCallBacks);
    }
    else if (context->inner_auth_type == kInnerAuthTypeEAP) {
	if (context->handshake_complete) {
	    if (context->eap.require_props != NULL) {
		array = CFRetain(context->eap.require_props);
	    }
	}
    }
    else if (plugin->password == NULL) {
	CFStringRef	str = kEAPClientPropUserPassword;
	array = CFArrayCreate(NULL, (const void **)&str,
			      1, &kCFTypeArrayCallBacks);
    }

 done:
    return (array);
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

static CFDictionaryRef
eapttls_publish_props(EAPClientPluginDataRef plugin)
{
    CFArrayRef			cert_list;
    SSLCipherSuite		cipher = SSL_NULL_WITH_NULL_NULL;
    EAPTTLSPluginDataRef	context = (EAPTTLSPluginDataRef)plugin->private;
    CFMutableDictionaryRef	dict;

    if (context->server_certs == NULL) {
	return (NULL);
    }
    cert_list = EAPSecCertificateArrayCreateCFDataArray(context->server_certs);
    if (cert_list == NULL) {
	return (NULL);
    }
    if (context->inner_auth_type == kInnerAuthTypeEAP
	&& context->handshake_complete
	&& context->eap.publish_props != NULL) {
	dict = CFDictionaryCreateMutableCopy(NULL, 0, 
					     context->eap.publish_props);
    }
    else {
	dict = CFDictionaryCreateMutable(NULL, 0,
					 &kCFTypeDictionaryKeyCallBacks,
					 &kCFTypeDictionaryValueCallBacks);
    }
    CFDictionarySetValue(dict, kEAPClientPropTLSServerCertificateChain,
			 cert_list);
    CFDictionarySetValue(dict, kEAPClientPropTLSSessionWasResumed,
			 context->session_was_resumed 
			 ? kCFBooleanTrue
			 : kCFBooleanFalse);
    my_CFRelease(&cert_list);
    (void)SSLGetNegotiatedCipher(context->ssl_context, &cipher);
    if (cipher != SSL_NULL_WITH_NULL_NULL) {
	CFNumberRef	c;
	int		tmp = cipher;

	c = CFNumberCreate(NULL, kCFNumberIntType, &tmp);
	CFDictionarySetValue(dict, kEAPClientPropTLSNegotiatedCipher, c);
	CFRelease(c);
    }
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

static CFStringRef
eapttls_copy_packet_description(const EAPPacketRef pkt, bool * packet_is_valid)
{ 
    EAPTLSPacketRef 	eaptls_pkt = (EAPTLSPacketRef)pkt;

    return (EAPTLSPacketCopyDescription(eaptls_pkt, packet_is_valid));
}

static EAPType 
eapttls_type()
{
    return (kEAPTypeTTLS);

}

static const char *
eapttls_name()
{
    return (EAPTypeStr(kEAPTypeTTLS));
}

static EAPClientPluginVersion 
eapttls_version()
{
    return (kEAPClientPluginVersion);
}

static struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
#if 0
    { kEAPClientPluginFuncNameIntrospect, eapttls_introspect },
#endif /* 0 */
    { kEAPClientPluginFuncNameVersion, eapttls_version },
    { kEAPClientPluginFuncNameEAPType, eapttls_type },
    { kEAPClientPluginFuncNameEAPName, eapttls_name },
    { kEAPClientPluginFuncNameInit, eapttls_init },
    { kEAPClientPluginFuncNameFree, eapttls_free },
    { kEAPClientPluginFuncNameProcess, eapttls_process },
    { kEAPClientPluginFuncNameFreePacket, eapttls_free_packet },
    { kEAPClientPluginFuncNameFailureString, eapttls_failure_string },
    { kEAPClientPluginFuncNameSessionKey, eapttls_session_key },
    { kEAPClientPluginFuncNameServerKey, eapttls_server_key },
    { kEAPClientPluginFuncNameMasterSessionKeyCopyBytes,
      eapttls_msk_copy_bytes },
    { kEAPClientPluginFuncNameRequireProperties, eapttls_require_props },
    { kEAPClientPluginFuncNamePublishProperties, eapttls_publish_props },
    { kEAPClientPluginFuncNameCopyPacketDescription,
      eapttls_copy_packet_description },
    { NULL, NULL},
};


EAPClientPluginFuncRef
eapttls_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;


    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}
