/*
 * Copyright (c) 2002-2010 Apple Inc. All rights reserved.
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
 * peap_plugin.c
 * - PEAP client using SecureTransport API's
 */

/* 
 * Modification History
 *
 * May 14, 2003	Dieter Siegmund (dieter@apple)
 * - created (from eapttls_plugin.c)
 *
 * September 8, 2004	Dieter Siegmund (dieter@apple)
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
#include <syslog.h>
#include <Security/SecureTransport.h>
#include <Security/SecCertificate.h>
#include <sys/param.h>
#include <EAP8021X/EAPTLSUtil.h>
#include <EAP8021X/EAPCertificateUtil.h>
#include <Security/SecureTransportPriv.h>
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPClientModule.h>
#include "myCFUtil.h"
#include "nbo.h"
#include "printdata.h"

enum {
    kPEAPPacketFlagsVersionMask		= 0x7,
    kPEAPPacketFlagsFlagsMask		= 0xf8,
    kPEAPVersion0			= 0,
    kPEAPVersion1			= 1,
};

static __inline__ uint8_t
PEAPPacketFlagsFlags(uint8_t flags)
{
    return (flags & kPEAPPacketFlagsFlagsMask);
}

static __inline__ uint8_t
PEAPPacketFlagsVersion(uint8_t flags)
{
    return (flags & kPEAPPacketFlagsVersionMask);
}

static __inline__ void
PEAPPacketFlagsSetVersion(EAPTLSPacketRef eap_tls, uint8_t version)
{
    eap_tls->flags &= ~kPEAPPacketFlagsVersionMask;
    eap_tls->flags |= PEAPPacketFlagsVersion(version);
    return;
}

/*
 * Extensions "protocol" definitions:
 */
typedef struct EAPExtensionsPacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		avp_type[2];
    uint8_t		avp_length[2];
    uint8_t		data[0];
} EAPExtensionsPacket, *EAPExtensionsPacketRef;

typedef struct EAPExtensionsResultPacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		avp_type[2];
    uint8_t		avp_length[2];
    uint8_t		status[2];
} EAPExtensionsResultPacket, *EAPExtensionsResultPacketRef;

enum {
    kEAPExtensionsAVPTypeResult = 3,
};

enum {
    kEAPExtensionsAVPTypeMask = 0x3fff,
    kEAPExtensionsAVPTypeMandatory = 0x8000,
    kEAPExtensionsAVPTypeReserved = 0x4000,
};

typedef uint16_t	EAPExtensionsAVPType;
typedef uint16_t	EAPExtensionsResultStatus;

enum {
    kEAPExtensionsResultStatusSuccess = 1,
    kEAPExtensionsResultStatusFailure = 2,
};

static __inline__ void
EAPExtensionsPacketSetAVPLength(EAPExtensionsPacketRef pkt, uint16_t length)
{
    net_uint16_set(pkt->avp_length, length); 
    return;
}

static __inline__ uint16_t
EAPExtensionsPacketGetAVPLength(const EAPExtensionsPacketRef pkt)
{
    return (net_uint16_get(pkt->avp_length));
}

static __inline__ void
EAPExtensionsPacketSetAVPType(EAPExtensionsPacketRef pkt, uint16_t type)
{
    net_uint16_set(pkt->avp_type, type);
    return;
}

static __inline__ uint16_t
EAPExtensionsPacketGetAVPType(const EAPExtensionsPacketRef pkt)
{
    return (net_uint16_get(pkt->avp_type));
}

static __inline__ void
EAPExtensionsResultPacketSetStatus(EAPExtensionsResultPacketRef pkt, 
				   uint16_t status)
{
    net_uint16_set(pkt->status, status);
    return;
}

static __inline__ uint16_t
EAPExtensionsResultPacketGetStatus(const EAPExtensionsResultPacketRef pkt)
{
    return (net_uint16_get(pkt->status));
}

static __inline__ bool
EAPExtensionsAVPTypeIsMandatory(uint16_t avp_type)
{
    return ((avp_type & kEAPExtensionsAVPTypeMandatory) != 0);
}

static __inline__ uint16_t
EAPExtensionsAVPTypeType(uint16_t avp_type)
{
    return (avp_type & kEAPExtensionsAVPTypeMask);
}

/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect peap_introspect;
static EAPClientPluginFuncVersion peap_version;
static EAPClientPluginFuncEAPType peap_type;
static EAPClientPluginFuncEAPName peap_name;
static EAPClientPluginFuncInit peap_init;
static EAPClientPluginFuncFree peap_free;
static EAPClientPluginFuncProcess peap_process;
static EAPClientPluginFuncFreePacket peap_free_packet;
static EAPClientPluginFuncSessionKey peap_session_key;
static EAPClientPluginFuncServerKey peap_server_key;
static EAPClientPluginFuncRequireProperties peap_require_props;
static EAPClientPluginFuncPublishProperties peap_publish_props;
static EAPClientPluginFuncPacketDump peap_packet_dump;

typedef enum {
    kRequestTypeStart,
    kRequestTypeAck,
    kRequestTypeData,
} RequestType;

static int inner_auth_types[3] = {
    kEAPTypeMSCHAPv2,
    kEAPTypeMD5Challenge,
    kEAPTypeGenericTokenCard,
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


enum {
    kPEAPInnerAuthStateUnknown = 0,
    kPEAPInnerAuthStateSuccess = 1,
    kPEAPInnerAuthStateFailure = 2,
};
typedef int PEAPInnerAuthState;

typedef struct {
    SSLContextRef		ssl_context;
    memoryBuffer		read_buffer;
    int				last_read_size;
    memoryBuffer		write_buffer;
    int				last_write_size;
    int				previous_identifier;
    memoryIO			mem_io;
    EAPClientState		plugin_state;
    CFArrayRef			certs;
    int				mtu;
    int				peap_version;
    bool			bogus_l_bit;
    OSStatus			last_ssl_error;
    EAPClientStatus		last_client_status;
    bool			handshake_complete;
    PEAPInnerAuthState		inner_auth_state;
    struct eap_client		eap;
    EAPPacketRef		last_packet;
    char			last_packet_buf[1024];
    int				last_eap_type_index;
    OSStatus			trust_ssl_error;
    EAPClientStatus		trust_status;
    bool			trust_proceed;
    bool			key_data_valid;
    char			key_data[128];
    CFArrayRef			server_certs;
    bool			resume_sessions;
    bool			session_was_resumed;
} PEAPPluginData, *PEAPPluginDataRef;

enum {
    kEAPTLSAvoidDenialOfServiceSize = 128 * 1024
};

#define BAD_IDENTIFIER			(-1)
#define BAD_VERSION			(-1)

static void
free_last_packet(PEAPPluginDataRef context)
{
    if (context->last_packet != NULL
	&& (void *)context->last_packet != context->last_packet_buf) {
	free(context->last_packet);
    }
    context->last_packet = NULL;
    return;
}

static void
save_last_packet(PEAPPluginDataRef context, EAPPacketRef packet)
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

/**
 ** EAP client module access convenience routines
 **/
static void
eap_client_free(PEAPPluginDataRef context)
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
eap_client_type(PEAPPluginDataRef context)
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
    PEAPPluginDataRef 	context = (PEAPPluginDataRef)plugin->private;
    EAPClientModule *	module;

    context->eap.last_type = kEAPTypeInvalid;
    context->eap.last_type_name = NULL;

    if (context->eap.module != NULL) {
	printf("eap_client_init: already initialized\n");
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
eap_client_require_properties(PEAPPluginDataRef context)
{
    return (EAPClientModulePluginRequireProperties(context->eap.module,
						   &context->eap.plugin_data));
}

static CFDictionaryRef
eap_client_publish_properties(PEAPPluginDataRef context)
{
    return (EAPClientModulePluginPublishProperties(context->eap.module,
						   &context->eap.plugin_data));
}

static EAPClientState
eap_client_process(EAPClientPluginDataRef plugin, EAPPacketRef in_pkt_p,
		   EAPPacketRef * out_pkt_p)
{
    PEAPPluginDataRef 	context = (PEAPPluginDataRef)plugin->private;
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
eap_client_free_packet(PEAPPluginDataRef context, EAPPacketRef out_pkt_p)
{
    EAPClientModulePluginFreePacket(context->eap.module, 
				    &context->eap.plugin_data,
				    out_pkt_p);
}

/**
 ** eap_client end
 **/
 
static bool
module_packet_dump(FILE * out_f, const EAPRequestPacketRef req_p)
{
    bool			logged = FALSE;
    EAPClientModuleRef		module;

    switch (req_p->code) {
    case kEAPCodeRequest:
    case kEAPCodeResponse:
	module = EAPClientModuleLookup(req_p->type);
	if (module != NULL) {
	    logged = EAPClientModulePluginPacketDump(module, out_f, 
						     (EAPPacketRef)req_p);
	}
	break;
    }
    return (logged);
}

/*
 * According to the PEAP version 1 spec, the label should be
 * "client PEAP encryption", but apparently it's not actually used
 * by PEAP versions 0 or 1.
 */
#define kEAPTLSClientLabel		"client EAP encryption"
#define kEAPTLSClientLabelLength	(sizeof(kEAPTLSClientLabel) - 1)

static bool
peap_compute_session_key(PEAPPluginDataRef context)
{
    OSStatus		status;

    context->key_data_valid = FALSE;
    status = EAPTLSComputeKeyData(context->ssl_context, 
				  kEAPTLSClientLabel, kEAPTLSClientLabelLength,
				  context->key_data,
				  sizeof(context->key_data));
    if (status != noErr) {
	syslog(LOG_NOTICE, 
	       "peap_compute_session_key: EAPTLSComputeSessionKey failed, %s",
	       EAPSSLErrorString(status));
	return (FALSE);
    }
    context->key_data_valid = TRUE;
    return (TRUE);
}

static void
peap_free_context(PEAPPluginDataRef context)
{
    eap_client_free(context);
    free_last_packet(context);
    if (context->ssl_context != NULL) {
	SSLDisposeContext(context->ssl_context);
	context->ssl_context = NULL;
    }
    my_CFRelease(&context->certs);
    my_CFRelease(&context->server_certs);
    memoryIOClearBuffers(&context->mem_io);
    free(context);

    return;
}

static OSStatus
peap_start(EAPClientPluginDataRef plugin)
{
    PEAPPluginDataRef 	context = (PEAPPluginDataRef)plugin->private;
    SSLContextRef	ssl_context = NULL;
    OSStatus		status = noErr;

    free_last_packet(context);
    context->last_eap_type_index = 0;
    if (context->ssl_context != NULL) {
	SSLDisposeContext(context->ssl_context);
	context->ssl_context = NULL;
    }
    my_CFRelease(&context->server_certs);
    memoryIOClearBuffers(&context->mem_io);
    ssl_context = EAPTLSMemIOContextCreate(FALSE, &context->mem_io, NULL, 
					   &status);
    if (ssl_context == NULL) {
	syslog(LOG_NOTICE, "peap_start: EAPTLSMemIOContextCreate failed, %s",
	       EAPSSLErrorString(status));
	goto failed;
    }
    status = SSLSetEnableCertVerify(ssl_context, FALSE);
    if (status != noErr) {
	syslog(LOG_NOTICE, "peap_start: SSLSetEnableCertVerify failed, %s",
	       EAPSSLErrorString(status));
	goto failed;
    }
    if (context->resume_sessions && plugin->unique_id != NULL) {
	status = SSLSetPeerID(ssl_context, plugin->unique_id,
			      plugin->unique_id_length);
	if (status != noErr) {
	    syslog(LOG_NOTICE, 
		   "SSLSetPeerID failed, %s", EAPSSLErrorString(status));
	    goto failed;
	}
    }
    if (context->certs != NULL) {
	status = SSLSetCertificate(ssl_context, context->certs);
	if (status != noErr) {
	    syslog(LOG_NOTICE, 
		   "SSLSetCertificate failed, %s", EAPSSLErrorString(status));
	    goto failed;
	}
    }
    context->ssl_context = ssl_context;
    context->plugin_state = kEAPClientStateAuthenticating;
    context->previous_identifier = BAD_IDENTIFIER;
    context->last_ssl_error = noErr;
    context->last_client_status = kEAPClientStatusOK;
    context->handshake_complete = FALSE;
    context->trust_proceed = FALSE;
    context->inner_auth_state = kPEAPInnerAuthStateUnknown;
    context->key_data_valid = FALSE;
    context->last_write_size = 0;
    context->last_read_size = 0;
    context->peap_version = BAD_VERSION;
    context->bogus_l_bit = FALSE;
    context->session_was_resumed = FALSE;
    return (status);
 failed:
    if (ssl_context != NULL) {
	SSLDisposeContext(ssl_context);
    }
    return (status);
}

static EAPClientStatus
peap_init(EAPClientPluginDataRef plugin, CFArrayRef * require_props,
	  EAPClientDomainSpecificError * error)
{
    PEAPPluginDataRef	context = NULL;
    EAPClientStatus	result = kEAPClientStatusOK;
    OSStatus		status = noErr;

    *error = 0;
    context = malloc(sizeof(*context));
    if (context == NULL) {
	goto failed;
    }
    bzero(context, sizeof(*context));
    context->mtu = plugin->mtu;
    context->resume_sessions
	= my_CFDictionaryGetBooleanValue(plugin->properties, 
					 kEAPClientPropTLSEnableSessionResumption,
					 TRUE);
    /* memoryIOInit() initializes the memoryBuffer structures as well */
    memoryIOInit(&context->mem_io, &context->read_buffer,
		 &context->write_buffer);
    //memoryIOSetDebug(&context->mem_io, TRUE);
    plugin->private = context;
    status = peap_start(plugin);
    if (status != noErr) {
	result = kEAPClientStatusSecurityError;
	*error = status;
	goto failed;
    }
    return (result);

 failed:
    plugin->private = NULL;
    if (context != NULL) {
	peap_free_context(context);
    }
    return (result);
}

static void
peap_free(EAPClientPluginDataRef plugin)
{
    PEAPPluginDataRef	context = (PEAPPluginDataRef)plugin->private;

    if (context != NULL) {
	peap_free_context(context);
	plugin->private = NULL;
    }
    return;
}

static void
peap_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    if (arg != NULL) {
	free(arg);
    }
    return;
}

static EAPPacketRef
PEAPPacketCreateAck(int identifier)
{
    return (EAPTLSPacketCreate(kEAPCodeResponse, kEAPTypePEAP,
			       identifier, 0, NULL, NULL));
}

static EAPResponsePacketRef
peap_process_extensions(PEAPPluginDataRef context,
			EAPExtensionsPacketRef in_pkt_p,
			char * out_buf, int * out_buf_size, 
			EAPClientStatus * client_status)
{
    EAPExtensionsAVPType		avp_type;
    EAPExtensionsResultStatus		avp_result_status;
    uint16_t				in_length;
    EAPExtensionsResultPacketRef	out_pkt_p;
    EAPExtensionsResultPacketRef	r_p;

    in_length = EAPPacketGetLength((EAPPacketRef)in_pkt_p);
    if (in_length < sizeof(EAPExtensionsPacket)) {
	syslog(LOG_NOTICE, 
	       "peap_process_extensions: packet too short %d < %ld",
	       in_length, sizeof(EAPExtensionsPacket));
	return (NULL);
    }
    avp_type = EAPExtensionsPacketGetAVPType(in_pkt_p);
    if (EAPExtensionsAVPTypeType(avp_type) != kEAPExtensionsAVPTypeResult) {
	return (NULL);
    }
    r_p = (EAPExtensionsResultPacketRef)in_pkt_p;
    avp_result_status = EAPExtensionsResultPacketGetStatus(r_p);
    switch (avp_result_status) {
    case kEAPExtensionsResultStatusSuccess:
	context->inner_auth_state = kPEAPInnerAuthStateSuccess;
	break;
    case kEAPExtensionsResultStatusFailure:
	context->inner_auth_state = kPEAPInnerAuthStateFailure;
	break;
    }
    out_pkt_p = (EAPExtensionsResultPacketRef)
	EAPPacketCreate(out_buf, *out_buf_size,
			kEAPCodeResponse, 
			in_pkt_p->identifier,
			kEAPTypeExtensions,
			NULL,
			sizeof(*out_pkt_p) - sizeof(EAPRequestPacket),
			out_buf_size);
    EAPExtensionsPacketSetAVPType((EAPExtensionsPacketRef)out_pkt_p, 
				  kEAPExtensionsAVPTypeResult);
    EAPExtensionsPacketSetAVPLength((EAPExtensionsPacketRef)out_pkt_p, 
				    sizeof(out_pkt_p->status));
    EAPExtensionsResultPacketSetStatus(out_pkt_p, avp_result_status);
    return ((EAPResponsePacketRef)out_pkt_p);
}

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
next_eap_type(PEAPPluginDataRef context)
{
    if (context->last_eap_type_index >= inner_auth_types_count) {
	return (kEAPTypeInvalid);
    }
    return (inner_auth_types[context->last_eap_type_index++]);
}



static EAPResponsePacketRef
peap_eap_process(EAPClientPluginDataRef plugin, EAPRequestPacketRef in_pkt_p,
		 char * out_buf, int * out_buf_size, 
		 EAPClientStatus * client_status,
		 bool * call_module_free_packet)
{
    PEAPPluginDataRef		context = (PEAPPluginDataRef)plugin->private;
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
		    syslog(LOG_NOTICE, 
			   "peap_eap_process: eap_client_init type %d failed",
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
	context->inner_auth_state = kPEAPInnerAuthStateSuccess;
	break;
    case kEAPClientStateFailure:
	/* authentication method failed */
	context->inner_auth_state = kPEAPInnerAuthStateFailure;
	*client_status = context->eap.last_status;
	context->plugin_state = kEAPClientStateFailure;
	break;
    }

 done:
    return (out_pkt_p);
}

static bool
peap_eap(EAPClientPluginDataRef plugin, EAPTLSPacketRef eaptls_in,
	 EAPClientStatus * client_status)
{
    bool 		call_module_free_packet = FALSE;
    PEAPPluginDataRef 	context = (PEAPPluginDataRef)plugin->private;
    char 		in_buf[2048];
    size_t		in_data_size = 0;
    EAPRequestPacketRef	in_pkt_p;
    int			offset;
    char 		out_buf[2048];
    size_t		out_data_size;
    EAPResponsePacketRef out_pkt_p = NULL;
    int			out_pkt_size;
    memoryBufferRef	read_buf = &context->read_buffer;
    bool		ret = FALSE;
    OSStatus		status;

    switch (context->peap_version) {
    case kPEAPVersion0:
	/* leave room for header, in case it's needed */
	offset = sizeof(EAPPacket);
	break;
    default:
	offset = 0;
	break;
    }
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
	read_buf->offset = 0;
	status = SSLRead(context->ssl_context, in_buf + offset, 
			 sizeof(in_buf) - offset, &in_data_size);
	if (status != noErr) {
	    syslog(LOG_NOTICE, "peap_eap: SSLRead failed, %s (%d)",
		   EAPSSLErrorString(status), (int)status);
	    context->plugin_state = kEAPClientStateFailure;
	    context->last_ssl_error = status;
	    goto done;
	}
	in_pkt_p = (EAPRequestPacketRef)(in_buf + offset);
	switch (context->peap_version) {
	case kPEAPVersion0:
	    if (in_data_size >= sizeof(*in_pkt_p)
		&& in_pkt_p->code == kEAPCodeRequest
		&& in_pkt_p->type == kEAPTypeExtensions
		&& EAPPacketGetLength((EAPPacketRef)in_pkt_p) == in_data_size) {
		/* no need to insert EAP header */
	    }
	    else {
		/* insert missing EAP header */
		in_pkt_p = (EAPRequestPacketRef)in_buf;
		in_pkt_p->code = eaptls_in->code;
		in_pkt_p->identifier = eaptls_in->identifier;
		in_data_size += sizeof(EAPPacket);
		EAPPacketSetLength((EAPPacketRef)in_pkt_p,
				   in_data_size);
	    }
	    break;
	default:
	    break;
	}
	if (EAPPacketValid((EAPPacketRef)in_pkt_p, in_data_size, NULL)
	    == FALSE) {
	    if (plugin->log_enabled) {
		(void)EAPPacketValid((EAPPacketRef)in_pkt_p, 
				     in_data_size, stdout);
	    }
	    goto done;
	}
	if (plugin->log_enabled) {
	    printf("Receive packet:\n");
	    if (module_packet_dump(stdout, in_pkt_p)  == FALSE) {
		(void)EAPPacketValid((EAPPacketRef)in_pkt_p, 
				     in_data_size, stdout);
	    }
	}
    }
    switch (in_pkt_p->code) {
    case kEAPCodeRequest:
	switch (in_pkt_p->type) {
	case kEAPTypeIdentity:
	    out_pkt_p = (EAPResponsePacketRef)
		EAPPacketCreate(out_buf, sizeof(out_buf), 
				kEAPCodeResponse, in_pkt_p->identifier,
				kEAPTypeIdentity, plugin->username,
				plugin->username_length, 
				&out_pkt_size);
	    break;
	case kEAPTypeNotification:
	    out_pkt_p = (EAPResponsePacketRef)
		EAPPacketCreate(out_buf, sizeof(out_buf), 
				kEAPCodeResponse, in_pkt_p->identifier,
				kEAPTypeNotification, NULL, 0, 
				&out_pkt_size);
	    break;
	case kEAPTypeExtensions:
	    out_pkt_size = sizeof(out_buf);
	    out_pkt_p 
		= peap_process_extensions(context,
					  (EAPExtensionsPacketRef) in_pkt_p,
					  out_buf, &out_pkt_size,
					  client_status);
	    break;
	default:
	    out_pkt_size = sizeof(out_buf);
	    out_pkt_p = peap_eap_process(plugin, in_pkt_p,
					 out_buf, &out_pkt_size,
					 client_status,
					 &call_module_free_packet);
	    break;
	}
	break;
    case kEAPCodeResponse:
	out_pkt_p = peap_eap_process(plugin, in_pkt_p,
				     out_buf, &out_pkt_size,
				     client_status,
				     &call_module_free_packet);
	break;
    case kEAPCodeSuccess:
	out_pkt_p = peap_eap_process(plugin, in_pkt_p,
				     out_buf, &out_pkt_size,
				     client_status,
				     &call_module_free_packet);
	if (context->peap_version == kPEAPVersion1) {
	    context->inner_auth_state = kPEAPInnerAuthStateSuccess;
	    ret = TRUE;
	}
	break;
    case kEAPCodeFailure:
	out_pkt_p = peap_eap_process(plugin, in_pkt_p,
				     out_buf, &out_pkt_size,
				     client_status,
				     &call_module_free_packet);
	if (context->peap_version == kPEAPVersion1) {
	    context->inner_auth_state = kPEAPInnerAuthStateFailure;
	    ret = TRUE;
	}
	break;
    }

    if (out_pkt_p == NULL) {
	goto done;
    }
    if (plugin->log_enabled) {
	printf("\nSend packet:\n");
	if (module_packet_dump(stdout, out_pkt_p) == FALSE) {
	    (void)EAPPacketValid((EAPPacketRef)out_pkt_p, out_pkt_size, stdout);
	}
    }
    switch (context->peap_version) {
    case kPEAPVersion0:
	if (out_pkt_size >= sizeof(*out_pkt_p)
	    && out_pkt_p->code == kEAPCodeResponse
	    && out_pkt_p->type == kEAPTypeExtensions) {
	    /* don't strip EAP header */
	    offset = 0;
	}
	break;
    default:
	break;
    }
    free_last_packet(context);
    status = SSLWrite(context->ssl_context, ((void *)out_pkt_p) + offset, 
		      out_pkt_size - offset, &out_data_size);
    if ((char *)out_pkt_p != out_buf) {
	if (call_module_free_packet) {
	    eap_client_free_packet(context, (EAPPacketRef)out_pkt_p);
	}
	else {
	    free(out_pkt_p);
	}
    }
    if (status != noErr) {
	syslog(LOG_NOTICE, 
	       "peap_eap: SSLWrite failed, %s", EAPSSLErrorString(status));
    }
    else {
	ret = TRUE;
    }

 done:
    return (ret);
}

static EAPPacketRef
peap_verify_server(EAPClientPluginDataRef plugin,
		   int identifier, EAPClientStatus * client_status)
{
    PEAPPluginDataRef 	context = (PEAPPluginDataRef)plugin->private;
    EAPPacketRef	pkt = NULL;
    memoryBufferRef	write_buf = &context->write_buffer;

    context->trust_status
	= EAPTLSVerifyServerCertificateChain(plugin->properties, 
					     context->server_certs,
					     &context->trust_ssl_error);
    if (context->trust_status != kEAPClientStatusOK) {
	syslog(LOG_NOTICE, 
	       "peap_verify_server: server certificate not trusted"
	       ", status %d %d", context->trust_status,
	       (int)context->trust_ssl_error);
    }
    switch (context->trust_status) {
    case kEAPClientStatusOK:
	context->trust_proceed = TRUE;
	peap_compute_session_key(context);
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
				 kEAPTypePEAP,
				 identifier,
				 context->mtu,
				 write_buf,
				 &context->last_write_size);
	break;
    }
    return (pkt);
}

static EAPPacketRef
peap_tunnel(EAPClientPluginDataRef plugin, EAPTLSPacketRef eaptls_in,
	    EAPClientStatus * client_status)
{
    PEAPPluginDataRef 	context = (PEAPPluginDataRef)plugin->private;
    EAPPacketRef	pkt = NULL;
    memoryBufferRef	read_buf = &context->read_buffer; 
    memoryBufferRef	write_buf = &context->write_buffer; 

    if (context->trust_proceed == FALSE) {
	if (eaptls_in->identifier == context->previous_identifier) {
	    /* we've already seen this packet, discard buffer contents */
	    memoryBufferClear(read_buf);
	}
	pkt = peap_verify_server(plugin, eaptls_in->identifier, 
				 client_status);
	if (context->trust_proceed == TRUE) {
	    pkt = EAPTLSPacketCreate(kEAPCodeResponse,
				     kEAPTypePEAP,
				     eaptls_in->identifier,
				     context->mtu,
				     write_buf,
				     &context->last_write_size);
	}
	return (pkt);
    }
    if (peap_eap(plugin, eaptls_in, client_status)) {
	pkt = EAPTLSPacketCreate2(kEAPCodeResponse,
				  kEAPTypePEAP,
				  eaptls_in->identifier,
				  context->mtu,
				  write_buf,
				  &context->last_write_size,
				  FALSE);
	if (pkt != NULL && write_buf->data != NULL && context->bogus_l_bit) {
	    /* this is a hack/work-around for buggy Cisco PEAP XXX */
	    ((EAPTLSPacket *)pkt)->flags |= kEAPTLSPacketFlagsLengthIncluded;
	}
    }
    return (pkt);
}

static void
peap_set_session_was_resumed(PEAPPluginDataRef context)
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
peap_handshake(EAPClientPluginDataRef plugin, int identifier,
	       EAPClientStatus * client_status)
{
    PEAPPluginDataRef 	context = (PEAPPluginDataRef)plugin->private;
    EAPPacketRef	eaptls_out = NULL;
    memoryBufferRef	read_buf = &context->read_buffer;
    OSStatus		status = noErr;
    memoryBufferRef	write_buf = &context->write_buffer; 

    read_buf->offset = 0;
    status = SSLHandshake(context->ssl_context);
    switch (status) {
    case noErr:
	/* handshake complete, tunnel established */
	context->handshake_complete = TRUE;
	peap_set_session_was_resumed(context);
	my_CFRelease(&context->server_certs);
	(void) EAPSSLCopyPeerCertificates(context->ssl_context,
					  &context->server_certs);
	eaptls_out = peap_verify_server(plugin, identifier, client_status);
	if (context->trust_proceed == FALSE) {
	    break;
	}
	eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					kEAPTypePEAP,
					identifier,
					context->mtu,
					write_buf,
					&context->last_write_size);
	break;
    default:
	syslog(LOG_NOTICE, "peap_handshake: SSLHandshake failed, %s (%d)",
	       EAPSSLErrorString(status), (int)status);
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
		    = PEAPPacketCreateAck(identifier);
	    }
	}
	else {
	    eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					    kEAPTypePEAP,
					    identifier,
					    context->mtu,
					    write_buf,
					    &context->last_write_size);
	}
	break;
    }
    return (eaptls_out);
}


static EAPPacketRef
peap_request(EAPClientPluginDataRef plugin, SSLSessionState ssl_state,
	     const EAPPacketRef in_pkt, EAPClientStatus * client_status)
{
    PEAPPluginDataRef 	context = (PEAPPluginDataRef)plugin->private;
    EAPTLSPacket * 	eaptls_in = (EAPTLSPacket *)in_pkt; 
    EAPTLSLengthIncludedPacket * eaptls_in_l;
    EAPPacketRef	eaptls_out = NULL;
    int			in_data_length;
    void *		in_data_ptr = NULL;
    u_int16_t		in_length = EAPPacketGetLength(in_pkt);
    memoryBufferRef	write_buf = &context->write_buffer; 
    memoryBufferRef	read_buf = &context->read_buffer;
    OSStatus		status = noErr;
    u_int32_t		tls_message_length = 0;
    RequestType		type;

    /* ALIGN: void * cast OK, we don't expect proper alignment */
    eaptls_in_l = (EAPTLSLengthIncludedPacket *)(void *)in_pkt;
    if (in_length < sizeof(*eaptls_in)) {
	syslog(LOG_NOTICE, "peap_request: length %d < %ld",
	       in_length, sizeof(*eaptls_in));
	goto ignore;
    }
    in_data_ptr = eaptls_in->tls_data;
    tls_message_length = in_data_length = in_length - sizeof(EAPTLSPacket);

    type = kRequestTypeData;
    if ((eaptls_in->flags & kEAPTLSPacketFlagsStart) != 0) {
	type = kRequestTypeStart;
	if (ssl_state != kSSLIdle) {
	    /* reinitialize */
	    status = peap_start(plugin);
	    if (status != noErr) {
		context->last_ssl_error = status;
		goto ignore;
	    }
	    ssl_state = kSSLIdle;
	}
    }
    else if (in_length == sizeof(*eaptls_in)) {
	type = kRequestTypeAck;
    }
    else if ((eaptls_in->flags & kEAPTLSPacketFlagsLengthIncluded) != 0) {
	if (in_length < sizeof(EAPTLSLengthIncludedPacket)) {
	    syslog(LOG_NOTICE, 
		   "peap_request: packet too short %d < %ld",
		   in_length, sizeof(EAPTLSLengthIncludedPacket));
	    goto ignore;
	}
	tls_message_length 
	    = EAPTLSLengthIncludedPacketGetMessageLength(eaptls_in_l);
	if (tls_message_length > kEAPTLSAvoidDenialOfServiceSize) {
	    if ((eaptls_in->flags & kEAPTLSPacketFlagsMoreFragments) != 0) {
		syslog(LOG_NOTICE, 
		       "peap_request: received message too large, %d > %d",
		       tls_message_length, kEAPTLSAvoidDenialOfServiceSize);
		goto ignore;
	    }
	    else {
		tls_message_length = in_data_length;
		context->bogus_l_bit = TRUE;
	    }
	}
	else {
	    in_data_ptr = eaptls_in_l->tls_data;
	    in_data_length = in_length - sizeof(EAPTLSLengthIncludedPacket);
	    if (tls_message_length == 0) {
		type = kRequestTypeAck;
	    }
	}
    }

    switch (ssl_state) {
    case kSSLClosed:
    case kSSLAborted:
	break;

    case kSSLIdle:
	if (type != kRequestTypeStart) {
	    /* ignore it: XXX should this be an error? */
	    syslog(LOG_NOTICE, 
		   "peap_request: ignoring non PEAP start frame");
	    goto ignore;
	}
	status = SSLHandshake(context->ssl_context);
	if (status != errSSLWouldBlock) {
	    syslog(LOG_NOTICE, 
		   "peap_request: SSLHandshake failed, %s (%d)",
		   EAPSSLErrorString(status), (int)status);
	    context->last_ssl_error = status;
	    context->plugin_state = kEAPClientStateFailure;
	    goto ignore;
	}
	eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					kEAPTypePEAP,
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
						kEAPTypePEAP,
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
						kEAPTypePEAP,
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
	    syslog(LOG_NOTICE, "peap_request: unexpected %s frame",
		   type == kRequestTypeAck ? "Ack" : "Start");
	    goto ignore;
	}
	if (read_buf->data == NULL) {
	    read_buf->data = malloc(tls_message_length);
	    read_buf->length = tls_message_length;
	    read_buf->offset = 0;
	}
	else if (in_pkt->identifier == context->previous_identifier) {
	    if ((eaptls_in->flags & kEAPTLSPacketFlagsMoreFragments) == 0) {
		syslog(LOG_NOTICE, "peap_request: re-sent packet does not"
		       " have more fragments bit set, ignoring");
		goto ignore;
	    }
	    /* just ack it, we've already seen the fragment */
	    eaptls_out = PEAPPacketCreateAck(eaptls_in->identifier);
	    break;
	}
	if ((read_buf->offset + in_data_length) > read_buf->length) {
	    syslog(LOG_NOTICE, 
		   "peap_request: fragment too large %ld + %d > %ld",
		   read_buf->offset, in_data_length, read_buf->length);
	    goto ignore;
	}
	if ((read_buf->offset + in_data_length) < read_buf->length
	    && (eaptls_in->flags & kEAPTLSPacketFlagsMoreFragments) == 0) {
	    syslog(LOG_NOTICE, 
		   "peap_request: expecting more data but "
		   "more fragments bit is not set, ignoring");
	    goto ignore;
	}
	bcopy(in_data_ptr,
	      read_buf->data + read_buf->offset, in_data_length);
	read_buf->offset += in_data_length;
	context->last_read_size = in_data_length;
	if (read_buf->offset < read_buf->length) {
	    /* we haven't received the entire TLS message */
	    eaptls_out = PEAPPacketCreateAck(eaptls_in->identifier);
	    break;
	}
	/* we've got the whole TLS message, process it */
	if (context->handshake_complete) {
	    /* subsequent request */
	    eaptls_out = peap_tunnel(plugin, eaptls_in,
				     client_status);
	}
	else {
	    eaptls_out = peap_handshake(plugin, eaptls_in->identifier,
					client_status);
	}
	break;
    default:
	break;
    }

    context->previous_identifier = in_pkt->identifier;
    if (context->peap_version == BAD_VERSION) {
	uint8_t	peap_version;

	peap_version = PEAPPacketFlagsVersion(eaptls_in_l->flags);
	if (peap_version > kPEAPVersion1) {
	    peap_version = kPEAPVersion1;
	}
	context->peap_version = peap_version;
    }
    if (eaptls_out != NULL) {
	PEAPPacketFlagsSetVersion((EAPTLSPacketRef)eaptls_out,
				  context->peap_version);
    }

 ignore:
    return (eaptls_out);
}

static EAPClientState
peap_process(EAPClientPluginDataRef plugin, 
	     const EAPPacketRef in_pkt,
	     EAPPacketRef * out_pkt_p, 
	     EAPClientStatus * client_status,
	     EAPClientDomainSpecificError * error)
{
    PEAPPluginDataRef	context = (PEAPPluginDataRef)plugin->private;
    SSLSessionState	ssl_state = kSSLIdle;
    OSStatus		status = noErr;

    *client_status = kEAPClientStatusOK;
    *error = 0;
    context->bogus_l_bit = FALSE;

    status = SSLGetSessionState(context->ssl_context, &ssl_state);
    if (status != noErr) {
	syslog(LOG_NOTICE, "peap_process: SSLGetSessionState failed, %s",
	       EAPSSLErrorString(status));
	context->plugin_state = kEAPClientStateFailure;
	context->last_ssl_error = status;
	goto done;
    }

    *out_pkt_p = NULL;
    switch (in_pkt->code) {
    case kEAPCodeRequest:
	*out_pkt_p = peap_request(plugin, ssl_state, in_pkt, client_status);
	break;
    case kEAPCodeSuccess:
	if (context->inner_auth_state == kPEAPInnerAuthStateSuccess) {
	    context->plugin_state = kEAPClientStateSuccess;
	}
	else if (context->peap_version == kPEAPVersion1
		 && context->handshake_complete && context->trust_proceed) {
	    context->plugin_state = kEAPClientStateSuccess;
	}
	break;
    case kEAPCodeFailure:
	if (context->inner_auth_state == kPEAPInnerAuthStateFailure) {
	    context->plugin_state = kEAPClientStateFailure;
	}
	else if (context->peap_version == kPEAPVersion1
		 && context->handshake_complete) {
	    context->plugin_state = kEAPClientStateFailure;
	}
	break;
    case kEAPCodeResponse:
    default:
	break;
    }
 done:
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
peap_failure_string(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

static void * 
peap_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    PEAPPluginDataRef	context = (PEAPPluginDataRef)plugin->private;

    *key_length = 0;
    if (context->key_data_valid == FALSE) {
	return (NULL);
    }

    /* return the first 32 bytes of key data */
    *key_length = 32;
    return (context->key_data);
}

static void * 
peap_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    PEAPPluginDataRef	context = (PEAPPluginDataRef)plugin->private;

    *key_length = 0;
    if (context->key_data_valid == FALSE) {
	return (NULL);
    }

    /* return the second 32 bytes of key data */
    *key_length = 32;
    return (context->key_data + 32);
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
peap_publish_props(EAPClientPluginDataRef plugin)
{
    CFArrayRef			cert_list;
    SSLCipherSuite		cipher = SSL_NULL_WITH_NULL_NULL;
    PEAPPluginDataRef		context = (PEAPPluginDataRef)plugin->private;
    CFMutableDictionaryRef	dict;

    if (context->server_certs == NULL) {
	return (NULL);
    }
    cert_list = EAPSecCertificateArrayCreateCFDataArray(context->server_certs);
    if (cert_list == NULL) {
	return (NULL);
    }
    if (context->handshake_complete && context->eap.publish_props != NULL) {
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
    (void)SSLGetNegotiatedCipher(context->ssl_context, &cipher);
    if (cipher != SSL_NULL_WITH_NULL_NULL) {
	CFNumberRef	c;
	
	c = CFNumberCreate(NULL, kCFNumberIntType, &cipher);
	CFDictionarySetValue(dict, kEAPClientPropTLSNegotiatedCipher, c);
	CFRelease(c);
    }
    my_CFRelease(&cert_list);
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

static CFArrayRef
peap_require_props(EAPClientPluginDataRef plugin)
{
    CFArrayRef		array = NULL;
    PEAPPluginDataRef	context = (PEAPPluginDataRef)plugin->private;

    if (context->last_client_status != kEAPClientStatusUserInputRequired) {
	goto done;
    }
    if (context->trust_proceed == FALSE) {
	CFStringRef	str = kEAPClientPropTLSUserTrustProceedCertificateChain;
	array = CFArrayCreate(NULL, (const void **)&str,
			      1, &kCFTypeArrayCallBacks);
    }
    else if (context->handshake_complete) {
	if (context->eap.require_props != NULL) {
	    array = CFRetain(context->eap.require_props);
	}
    }
 done:
    return (array);
}

static bool
peap_packet_dump(FILE * out_f, const EAPPacketRef pkt)
{
    EAPTLSPacket * 	eaptls_pkt = (EAPTLSPacket *)pkt;
    EAPTLSLengthIncludedPacket * eaptls_pkt_l;
    int			data_length;
    void *		data_ptr = NULL;
    u_int16_t		length = EAPPacketGetLength(pkt);
    u_int32_t		tls_message_length = 0;

    switch (pkt->code) {
    case kEAPCodeRequest:
    case kEAPCodeResponse:
	break;
    default:
	/* just return */
	return (FALSE);
	break;
    }
    if (length < sizeof(*eaptls_pkt)) {
	fprintf(out_f, "invalid packet: length %d < min length %ld",
		length, sizeof(*eaptls_pkt));
	goto done;
    }
    fprintf(out_f, "PEAP Version %d %s: Identifier %d Length %d Flags 0x%x%s",
	    PEAPPacketFlagsVersion(eaptls_pkt->flags),
	    pkt->code == kEAPCodeRequest ? "Request" : "Response",
	    pkt->identifier, length, eaptls_pkt->flags,
	    (PEAPPacketFlagsFlags(eaptls_pkt->flags) != 0) ? " [" : "");

    /* ALIGN: void * cast OK, we don't expect proper alignment */ 
    eaptls_pkt_l = (EAPTLSLengthIncludedPacket *)(void *)pkt;
    
    data_ptr = eaptls_pkt->tls_data;
    tls_message_length = data_length = length - sizeof(EAPTLSPacket);

    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsStart) != 0) {
	fprintf(out_f, " start");
    }
    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsLengthIncluded) != 0) {
	if (length >= sizeof(EAPTLSLengthIncludedPacket)) {
	    data_ptr = eaptls_pkt_l->tls_data;
	    data_length = length - sizeof(EAPTLSLengthIncludedPacket);
	    tls_message_length 
		= EAPTLSLengthIncludedPacketGetMessageLength(eaptls_pkt_l);
	    fprintf(out_f, " length=%u", tls_message_length);
	
	}
    }
    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsMoreFragments) != 0) {
	fprintf(out_f, " more");
    }
    fprintf(out_f, "%s Data Length %d\n", 
	    PEAPPacketFlagsFlags(eaptls_pkt->flags) != 0 ? " ]" : "",
	    data_length);
    if (tls_message_length > kEAPTLSAvoidDenialOfServiceSize) {
	fprintf(out_f, "potential DOS attack %u > %d\n",
		tls_message_length, kEAPTLSAvoidDenialOfServiceSize);
	fprintf(out_f, "bogus EAP Packet:\n");
	fprint_data(out_f, (void *)pkt, length);
	goto done;
    }
    fprint_data(out_f, data_ptr, data_length);

 done:
    return (TRUE);
}

static EAPType 
peap_type()
{
    return (kEAPTypePEAP);

}

static const char *
peap_name()
{
    return ("PEAP");

}

static EAPClientPluginVersion 
peap_version()
{
    return (kEAPClientPluginVersion);
}

static struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
#if 0
    { kEAPClientPluginFuncNameIntrospect, peap_introspect },
#endif /* 0 */
    { kEAPClientPluginFuncNameVersion, peap_version },
    { kEAPClientPluginFuncNameEAPType, peap_type },
    { kEAPClientPluginFuncNameEAPName, peap_name },
    { kEAPClientPluginFuncNameInit, peap_init },
    { kEAPClientPluginFuncNameFree, peap_free },
    { kEAPClientPluginFuncNameProcess, peap_process },
    { kEAPClientPluginFuncNameFreePacket, peap_free_packet },
    { kEAPClientPluginFuncNameFailureString, peap_failure_string },
    { kEAPClientPluginFuncNameSessionKey, peap_session_key },
    { kEAPClientPluginFuncNameServerKey, peap_server_key },
    { kEAPClientPluginFuncNameRequireProperties, peap_require_props },
    { kEAPClientPluginFuncNamePublishProperties, peap_publish_props },
    { kEAPClientPluginFuncNamePacketDump, peap_packet_dump },
    { NULL, NULL},
};


EAPClientPluginFuncRef
peap_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;


    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}
