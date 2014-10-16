/*
 * Copyright (c) 2002-2014 Apple Inc. All rights reserved.
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

    /* MSCHAPv2 state: */
    uint8_t			peer_challenge[MSCHAP2_CHALLENGE_SIZE];
    uint8_t			nt_response[MSCHAP_NT_RESPONSE_SIZE];
    uint8_t			auth_challenge_id[MSCHAP2_CHALLENGE_SIZE + 1];
} EAPTTLSPluginData, * EAPTTLSPluginDataRef;

enum {
    kEAPTLSAvoidDenialOfServiceSize = 128 * 1024
};

#define BAD_IDENTIFIER			(-1)

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
		  "EAPTLSComputeSessionKey failed, %s",
		  EAPSSLErrorString(status));
	return (FALSE);
    }
    context->key_data_valid = TRUE;
    return (TRUE);
}

static void
eapttls_free_context(EAPTTLSPluginDataRef context)
{
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
    ssl_context = EAPTLSMemIOContextCreate(FALSE, &context->mem_io, NULL, 
					   &status);
    if (ssl_context == NULL) {
	EAPLOG_FL(LOG_NOTICE, "EAPTLSMemIOContextCreate failed, %s",
		  EAPSSLErrorString(status));
	goto failed;
    }
    if (context->resume_sessions && plugin->unique_id != NULL) {
	status = SSLSetPeerID(ssl_context, plugin->unique_id,
			      plugin->unique_id_length);
	if (status != noErr) {
	    EAPLOG_FL(LOG_NOTICE, 
		      "SSLSetPeerID failed, %s", EAPSSLErrorString(status));
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
		      "SSLSetCertificate failed, %s",
		      EAPSSLErrorString(status));
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
	inner_auth_type = kInnerAuthTypeMSCHAPv2;
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
#endif /* 0 */
    status = SSLWrite(context->ssl_context, data, offset - data, &length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s",
		  EAPSSLErrorString(status));
	ret = FALSE;
    }
    free(data);
    return (ret);
}

static bool
eapttls_eap_start(EAPClientPluginDataRef plugin, int identifier)
{
    DiameterAVP *	avp;
    EAPTTLSPluginDataRef context = (EAPTTLSPluginDataRef)plugin->private;
    void *		data;
    int			data_length;
    void *		offset;
    size_t		length;
    EAPResponsePacket *	resp_p = NULL;
    bool		ret = TRUE;
    OSStatus		status;

    /* allocate buffer to hold message */
    data_length = sizeof(*avp) + plugin->username_length + sizeof(*resp_p);
    data = malloc(data_length);
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
#if 0
    printf("offset - data %d length %d\n", offset - data, data_length);
#endif /* 0 */
    status = SSLWrite(context->ssl_context, data, offset - data, &length);
    if (status != noErr) {
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s",
		  EAPSSLErrorString(status));
	ret = FALSE;
    }
    free(data);
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
	EAPLOG_FL(LOG_NOTICE, "EAPTLSComputeKeyData failed, %s",
		  EAPSSLErrorString(status));
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
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s",
		  EAPSSLErrorString(status));
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
	EAPLOG_FL(LOG_NOTICE, "EAPTLSComputeKeyData failed, %s",
		  EAPSSLErrorString(status));
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
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s",
		  EAPSSLErrorString(status));
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
	EAPLOG_FL(LOG_NOTICE, "EAPTLSComputeKeyData failed, %s",
		  EAPSSLErrorString(status));
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
	EAPLOG_FL(LOG_NOTICE, "SSLWrite failed, %s",
		  EAPSSLErrorString(status));
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
	EAPLOG_FL(LOG_NOTICE, "SSLRead failed, %s",
		  EAPSSLErrorString(status));
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
	       int identifier, EAPClientStatus * client_status)
{
    EAPTTLSPluginDataRef 	context = (EAPTTLSPluginDataRef)plugin->private;
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
	EAPLOG_FL(LOG_NOTICE, "SSLHandshake failed, %s",
		  EAPSSLErrorString(status));
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
    EAPTLSPacket * 		eaptls_in = (EAPTLSPacket *)in_pkt; 
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
	    EAPLOG_FL(LOG_NOTICE, "SSLHandshake failed, %s",
		      EAPSSLErrorString(status));
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
					eaptls_in->identifier,
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
	if (context->trust_proceed) {
	    context->plugin_state = kEAPClientStateSuccess;
	}
	else if (context->handshake_complete) {
	    *out_pkt_p 
		= eapttls_verify_server(plugin, in_pkt->identifier, 
					client_status);
	    if (context->trust_proceed) {
		context->plugin_state = kEAPClientStateSuccess;
	    }
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
    else if (plugin->password == NULL) {
	CFStringRef	str = kEAPClientPropUserPassword;
	array = CFArrayCreate(NULL, (const void **)&str,
			      1, &kCFTypeArrayCallBacks);
    }
 done:
    return (array);
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
    dict = CFDictionaryCreateMutable(NULL, 0,
				     &kCFTypeDictionaryKeyCallBacks,
				     &kCFTypeDictionaryValueCallBacks);
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
