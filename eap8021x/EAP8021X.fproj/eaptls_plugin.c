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
 * eaptls_plugin.c
 * - EAP/TLS client using SecureTransport API's
 */

/* 
 * Modification History
 *
 * August 26, 2002	Dieter Siegmund (dieter@apple)
 * - created
 *
 * September 8, 2004	Dieter Siegmund (dieter@apple)
 * - use SecTrustEvaluate, and enable user interaction to decide whether to
 *   proceed or not, instead of just generating an error
 */
 
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <SystemConfiguration/SCValidation.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFDictionary.h>
#include <EAP8021X/EAPClientPlugin.h>
#include <EAP8021X/EAPClientProperties.h>
#include <EAP8021X/EAPTLSUtil.h>
#include <EAP8021X/EAPCertificateUtil.h>
#include <Security/SecureTransport.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecureTransportPriv.h>
#include "myCFUtil.h"
#include "printdata.h"

#define EAPTLS_EAP_TYPE		13

/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect eaptls_introspect;
static EAPClientPluginFuncVersion eaptls_version;
static EAPClientPluginFuncEAPType eaptls_type;
static EAPClientPluginFuncEAPName eaptls_name;
static EAPClientPluginFuncInit eaptls_init;
static EAPClientPluginFuncFree eaptls_free;
static EAPClientPluginFuncProcess eaptls_process;
static EAPClientPluginFuncFreePacket eaptls_free_packet;
static EAPClientPluginFuncSessionKey eaptls_session_key;
static EAPClientPluginFuncServerKey eaptls_server_key;
static EAPClientPluginFuncRequireProperties eaptls_require_props;
static EAPClientPluginFuncPublishProperties eaptls_publish_props;
static EAPClientPluginFuncPacketDump eaptls_packet_dump;
static EAPClientPluginFuncUserName eaptls_user_name;

typedef enum {
    kRequestTypeStart,
    kRequestTypeAck,
    kRequestTypeData,
} RequestType;

typedef struct {
    SSLContextRef		ssl_context;
    memoryBuffer		read_buffer;
    memoryBuffer		write_buffer;
    int				last_write_size;
    int				previous_identifier;
    memoryIO			mem_io;
    EAPClientState		plugin_state;
    CFArrayRef			certs;
    int				mtu;
    OSStatus			last_ssl_error;
    EAPClientStatus		last_client_status;
    bool			cert_requested;
    OSStatus			trust_ssl_error;
    EAPClientStatus		trust_status;
    bool			trust_proceed;
    bool			key_data_valid;
    char			key_data[128];
    CFArrayRef			server_certs;
    bool			resume_sessions;
    bool			session_was_resumed;
} EAPTLSPluginData, * EAPTLSPluginDataRef;

enum {
    kAvoidDenialOfServiceSize = 128 * 1024
};

#define BAD_IDENTIFIER			(-1)

#define kEAPTLSClientLabel		"client EAP encryption"
#define kEAPTLSClientLabelLength	(sizeof(kEAPTLSClientLabel) - 1)

static bool
eaptls_compute_session_key(EAPTLSPluginDataRef context)
{
    OSStatus		status;

    context->key_data_valid = FALSE;
    status = EAPTLSComputeKeyData(context->ssl_context, 
				  kEAPTLSClientLabel, 
				  kEAPTLSClientLabelLength,
				  context->key_data,
				  sizeof(context->key_data));
    if (status != noErr) {
	syslog(LOG_NOTICE, 
	       "eaptls_compute_session_key: EAPTLSComputeSessionKey failed, %s",
	       EAPSSLErrorString(status));
	return (FALSE);
    }
    context->key_data_valid = TRUE;
    return (TRUE);
}

static void
eaptls_free_context(EAPTLSPluginDataRef context)
{
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
eaptls_start(EAPClientPluginDataRef plugin)
{
    EAPTLSPluginDataRef	context = (EAPTLSPluginDataRef)plugin->private;
    SSLContextRef	ssl_context = NULL;
    OSStatus		status = noErr;

    if (context->ssl_context != NULL) {
	SSLDisposeContext(context->ssl_context);
	context->ssl_context = NULL;
    }
    my_CFRelease(&context->server_certs);
    memoryIOClearBuffers(&context->mem_io);
    ssl_context = EAPTLSMemIOContextCreate(FALSE, &context->mem_io, NULL, 
					   &status);
    if (ssl_context == NULL) {
	syslog(LOG_NOTICE, "eaptls_start: EAPTLSMemIOContextCreate failed, %s",
	       EAPSSLErrorString(status));
	goto failed;
    }
    status = SSLSetSessionOption(ssl_context,
				 kSSLSessionOptionBreakOnCertRequested,
				 TRUE);
    if (status != noErr) {
	syslog(LOG_NOTICE,
	       "eaptls_start: SSLSetOption("
	       "kSSLSessionOptionBreakOnCertRequested) failed, %s",
	       EAPSSLErrorString(status));
	goto failed;
    }
    status = SSLSetEnableCertVerify(ssl_context, FALSE);
    if (status != noErr) {
	syslog(LOG_NOTICE, "eaptls_start: SSLSetEnableCertVerify failed, %s",
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
    status = SSLSetCertificate(ssl_context, context->certs);
    if (status != noErr) {
	syslog(LOG_NOTICE, 
	       "SSLSetCertificate failed, %s", EAPSSLErrorString(status));
	goto failed;
    }
    context->ssl_context = ssl_context;
    context->plugin_state = kEAPClientStateAuthenticating;
    context->previous_identifier = BAD_IDENTIFIER;
    context->last_ssl_error = noErr;
    context->last_client_status = kEAPClientStatusOK;
    context->cert_requested = FALSE;
    context->trust_proceed = FALSE;
    context->key_data_valid = FALSE;
    context->last_write_size = 0;
    context->session_was_resumed = FALSE;
    return (status);
 failed:
    if (ssl_context != NULL) {
	SSLDisposeContext(ssl_context);
    }
    return (status);

}

static OSStatus
copy_identity(EAPClientPluginDataRef plugin,
	      CFArrayRef * ret_array)
{
    EAPSecIdentityHandleRef	id_handle = NULL;

    if (plugin->sec_identity != NULL) {
	return (EAPSecIdentityCreateTrustChain(plugin->sec_identity,
					       ret_array));
    }
    if (plugin->properties != NULL) {
	id_handle = CFDictionaryGetValue(plugin->properties,
					 kEAPClientPropTLSIdentityHandle);
    }
    return (EAPSecIdentityHandleCreateSecIdentityTrustChain(id_handle,
							    ret_array));
}

static EAPClientStatus
eaptls_init(EAPClientPluginDataRef plugin, CFArrayRef * required_props,
	    EAPClientDomainSpecificError * error)
{
    EAPTLSPluginDataRef	context = NULL;
    EAPClientStatus	result = kEAPClientStatusOK;
    OSStatus		status = noErr;

    *error = 0;
    context = malloc(sizeof(*context));
    if (context == NULL) {
	result = kEAPClientStatusAllocationFailed;
	goto failed;
    }
    bzero(context, sizeof(*context));
    context->mtu = plugin->mtu;
    status = copy_identity(plugin, &context->certs);
    if (status != noErr) {
	result = kEAPClientStatusSecurityError;
	*error = status;
	syslog(LOG_NOTICE, 
	       "eaptls_init: failed to find client cert/identity, %s (%d)",
	       EAPSSLErrorString(status), (int)status);
	goto failed;
    }
    context->resume_sessions
	= my_CFDictionaryGetBooleanValue(plugin->properties, 
					 kEAPClientPropTLSEnableSessionResumption,
					 TRUE);
    /* memoryIOInit() initializes the memoryBuffer structures as well */
    memoryIOInit(&context->mem_io, &context->read_buffer,
		 &context->write_buffer);
    //memoryIOSetDebug(&context->mem_io, TRUE);

    plugin->private = context;
    status = eaptls_start(plugin);
    if (status != noErr) {
	result = kEAPClientStatusSecurityError;
	*error = status;
	goto failed;
    }
    return (result);

 failed:
    plugin->private = NULL;
    if (context != NULL) {
	eaptls_free_context(context);
    }
    return (result);
}

static void
eaptls_free(EAPClientPluginDataRef plugin)
{
    EAPTLSPluginDataRef	context = (EAPTLSPluginDataRef)plugin->private;

    if (context != NULL) {
	eaptls_free_context(context);
	plugin->private = NULL;
    }
    return;
}

static void
eaptls_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    if (arg != NULL) {
	free(arg);
    }
    return;
}

static EAPPacketRef
EAPTLSPacketCreateAck(u_char identifier)
{
    return (EAPTLSPacketCreate(kEAPCodeResponse, EAPTLS_EAP_TYPE,
			       identifier, 0, NULL, NULL));
}

static EAPPacketRef
eaptls_verify_server(EAPClientPluginDataRef plugin,
		     int identifier, EAPClientStatus * client_status)
{
    EAPTLSPluginDataRef 	context = (EAPTLSPluginDataRef)plugin->private;
    EAPPacketRef		pkt = NULL;
    memoryBufferRef		write_buf = &context->write_buffer;

    context->trust_status
	= EAPTLSVerifyServerCertificateChain(plugin->properties, 
					     context->server_certs,
					     &context->trust_ssl_error);
    if (context->trust_status != kEAPClientStatusOK) {
	syslog(LOG_NOTICE, 
	       "eaptls_verify_server: server certificate not trusted"
	       ", status %d %d", context->trust_status,
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
	*client_status = context->trust_status;
	context->last_ssl_error = context->trust_ssl_error;
	context->plugin_state = kEAPClientStateFailure;
	SSLClose(context->ssl_context);
	pkt = EAPTLSPacketCreate(kEAPCodeResponse,
				 kEAPTypeTLS,
				 identifier,
				 context->mtu,
				 write_buf,
				 &context->last_write_size);
	break;
    }
    return (pkt);
}

static void
eaptls_set_session_was_resumed(EAPTLSPluginDataRef context)
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
eaptls_handshake(EAPClientPluginDataRef plugin,
		  int identifier, EAPClientStatus * client_status)

{
    EAPTLSPluginDataRef context = (EAPTLSPluginDataRef)plugin->private;
    EAPPacketRef	eaptls_out = NULL;
    memoryBufferRef	read_buf = &context->read_buffer;
    OSStatus		status = noErr;
    memoryBufferRef	write_buf = &context->write_buffer; 

    if (identifier == context->previous_identifier) {
	if (context->cert_requested && context->trust_proceed == FALSE) {
	    eaptls_out
		= eaptls_verify_server(plugin, identifier, client_status);
	    if (context->trust_proceed == FALSE) {
		return (eaptls_out);
	    }
	}
    }
    else {
	read_buf->offset = 0;
    }
    status = SSLHandshake(context->ssl_context);
    if (status == errSSLClientCertRequested) {
	/* before we present our cert, make sure it's someone we trust */
	context->cert_requested = TRUE;
	my_CFRelease(&context->server_certs);
	(void)EAPSSLCopyPeerCertificates(context->ssl_context,
					 &context->server_certs);
	eaptls_out = eaptls_verify_server(plugin, identifier, client_status);
	if (context->trust_proceed == FALSE) {
	    goto done;
	}
	/* do it again to get us past the cert requested */
	status = SSLHandshake(context->ssl_context);
    }

    switch (status) {
    case noErr:
	/* handshake complete */
	if (context->cert_requested == FALSE) {
	    /* session was resumed, re-evaluate now */
	    my_CFRelease(&context->server_certs);
	    (void)EAPSSLCopyPeerCertificates(context->ssl_context,
					     &context->server_certs);
	    eaptls_out 
		= eaptls_verify_server(plugin, identifier, client_status);
	    if (context->trust_proceed == FALSE) {
		break;
	    }
	}
	eaptls_compute_session_key(context);
	eaptls_set_session_was_resumed(context);
	eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					kEAPTypeTLS,
					identifier,
					context->mtu,
					write_buf,
					&context->last_write_size);
	break;
    default:
	syslog(LOG_NOTICE, "eaptls_handshake: SSLHandshake failed, %s",
	       EAPSSLErrorString(status));
	context->last_ssl_error = status;
	my_CFRelease(&context->server_certs);
	(void)EAPSSLCopyPeerCertificates(context->ssl_context,
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
		    = EAPTLSPacketCreateAck(identifier);
	    }
	}
	else {
	    eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					    kEAPTypeTLS,
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
eaptls_request(EAPClientPluginDataRef plugin,
	       SSLSessionState ssl_state,
	       const EAPPacketRef in_pkt,
	       EAPClientStatus * client_status)
{
    EAPTLSPluginDataRef	context = (EAPTLSPluginDataRef)plugin->private;
    EAPTLSPacketRef 	eaptls_in = (EAPTLSPacketRef)in_pkt; 
    EAPTLSLengthIncludedPacketRef eaptls_in_l;
    EAPPacketRef	eaptls_out = NULL;
    int			in_data_length;
    void *		in_data_ptr = NULL;
    u_int16_t		in_length = EAPPacketGetLength(in_pkt);
    memoryBufferRef	write_buf = &context->write_buffer; 
    memoryBufferRef	read_buf = &context->read_buffer;
    OSStatus		status = noErr;
    u_int32_t		tls_message_length = 0;
    RequestType		type;

    eaptls_in_l = (EAPTLSLengthIncludedPacketRef)in_pkt;
    if (in_length < sizeof(*eaptls_in)) {
	syslog(LOG_NOTICE, "eaptls_request: length %d < %ld",
	       in_length, sizeof(*eaptls_in));
	goto done;
    }
    in_data_ptr = eaptls_in->tls_data;
    tls_message_length = in_data_length = in_length - sizeof(EAPTLSPacket);

    type = kRequestTypeData;
    if ((eaptls_in->flags & kEAPTLSPacketFlagsStart) != 0) {
	type = kRequestTypeStart;
	switch (ssl_state) {
	case kSSLConnected:
	case kSSLClosed:
	case kSSLAborted:
	case kSSLHandshake:
	    /* reinitialize */
	    status = eaptls_start(plugin);
	    if (status != noErr) {
		context->last_ssl_error = status;
		context->plugin_state = kEAPClientStateFailure;
		goto done;
	    }
	    ssl_state = kSSLIdle;
	    break;
	
	default:
	case kSSLIdle:
	    break;
	}
    }
    else if (in_length == sizeof(*eaptls_in)) {
	type = kRequestTypeAck;
    }
    else if ((eaptls_in->flags & kEAPTLSPacketFlagsLengthIncluded) != 0) {
	if (in_length < sizeof(EAPTLSLengthIncludedPacket)) {
	    syslog(LOG_NOTICE, 
		   "eaptls_request: packet too short %d < %ld",
		   in_length, sizeof(EAPTLSLengthIncludedPacket));
	    goto done;
	}
	in_data_ptr = eaptls_in_l->tls_data;
	in_data_length = in_length - sizeof(EAPTLSLengthIncludedPacket);
	tls_message_length 
	    = ntohl(*((u_int32_t *)eaptls_in_l->tls_message_length));
	if (tls_message_length > kAvoidDenialOfServiceSize) {
	    syslog(LOG_NOTICE, 
		   "eaptls_request: received message too large, %d > %d",
		   tls_message_length, kAvoidDenialOfServiceSize);
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
	    syslog(LOG_NOTICE, 
		   "eaptls_request: ignoring non EAP-TLS start frame");
	    goto done;
	}
	status = SSLHandshake(context->ssl_context);
	if (status != errSSLWouldBlock) {
	    syslog(LOG_NOTICE, 
		   "eaptls_request: SSLHandshake failed, %s (%d)",
		   EAPSSLErrorString(status), (int)status);
	    context->last_ssl_error = status;
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
	eaptls_out = EAPTLSPacketCreate(kEAPCodeResponse,
					EAPTLS_EAP_TYPE,
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
						EAPTLS_EAP_TYPE,
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
						EAPTLS_EAP_TYPE,
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
	if (in_pkt->identifier == context->previous_identifier) {
	    eaptls_out = eaptls_handshake(plugin, eaptls_in->identifier,
					  client_status);
	    break;
	}
	if (type != kRequestTypeData) {
	    syslog(LOG_NOTICE, "eaptls_request: unexpected %s frame",
		   type == kRequestTypeAck ? "Ack" : "Start");
	    goto done;
	}
	if (read_buf->data == NULL) {
	    read_buf->data = malloc(tls_message_length);
	    read_buf->length = tls_message_length;
	    read_buf->offset = 0;
	}
	else if (in_pkt->identifier == context->previous_identifier) {
	    if ((eaptls_in->flags & kEAPTLSPacketFlagsMoreFragments) == 0) {
		syslog(LOG_NOTICE, "eaptls_request: re-sent packet does not"
		       " have more fragments bit set, ignoring");
		goto done;
	    }
	    /* just ack it, we've already seen the fragment */
	    eaptls_out = EAPTLSPacketCreateAck(eaptls_in->identifier);
	    break;
	}
	if ((read_buf->offset + in_data_length) > read_buf->length) {
	    syslog(LOG_NOTICE, 
		   "eaptls_request: fragment too large %ld + %d > %ld",
		   read_buf->offset, in_data_length, read_buf->length);
	    goto done;
	}
	if ((read_buf->offset + in_data_length) < read_buf->length
	    && (eaptls_in->flags & kEAPTLSPacketFlagsMoreFragments) == 0) {
	    syslog(LOG_NOTICE, 
		   "eaptls_request: expecting more data but "
		   "more fragments bit is not set, ignoring");
	    goto done;
	}
	bcopy(in_data_ptr,
	      read_buf->data + read_buf->offset, in_data_length);
	read_buf->offset += in_data_length;
	if (read_buf->offset < read_buf->length) {
	    /* we haven't received the entire TLS message */
	    eaptls_out = EAPTLSPacketCreateAck(eaptls_in->identifier);
	    break;
	}
	eaptls_out = eaptls_handshake(plugin, eaptls_in->identifier,
				      client_status);
	break;
    default:
	break;
    }
    context->previous_identifier = in_pkt->identifier;
 done:
    return (eaptls_out);
}

static EAPClientState
eaptls_process(EAPClientPluginDataRef plugin, 
	       const EAPPacketRef in_pkt,
	       EAPPacketRef * out_pkt_p,
	       EAPClientStatus * client_status,
	       EAPClientDomainSpecificError * error)
{
    EAPTLSPluginDataRef	context = (EAPTLSPluginDataRef)plugin->private;
    SSLSessionState	ssl_state = kSSLIdle;
    OSStatus		status = noErr;

    *client_status = kEAPClientStatusOK;
    *error = 0;
    status = SSLGetSessionState(context->ssl_context, &ssl_state);
    if (status != noErr) {
	syslog(LOG_NOTICE, "eaptls_process: SSLGetSessionState failed, %s",
	       EAPSSLErrorString(status));
	context->plugin_state = kEAPClientStateFailure;
	goto done;
    }

    *out_pkt_p = NULL;
    switch (in_pkt->code) {
    case kEAPCodeRequest:
	*out_pkt_p = eaptls_request(plugin, ssl_state, in_pkt, client_status);
	break;
    case kEAPCodeSuccess:
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
 done:
    if (context->plugin_state == kEAPClientStateFailure) {
	if (context->last_ssl_error == noErr) {
	    if (context->server_certs != NULL) {
		*client_status = kEAPClientStatusFailed;
	    }
	    else {
		*client_status = kEAPClientStatusProtocolError;
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
eaptls_failure_string(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

static void * 
eaptls_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPTLSPluginDataRef	context = (EAPTLSPluginDataRef)plugin->private;

    *key_length = 0;
    if (context->key_data_valid == FALSE) {
	return (NULL);
    }

    /* return the first 32 bytes of key data */
    *key_length = 32;
    return (context->key_data);
}

static void * 
eaptls_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPTLSPluginDataRef	context = (EAPTLSPluginDataRef)plugin->private;

    *key_length = 0;
    if (context->key_data_valid == FALSE) {
	return (NULL);
    }

    /* return the second 32 bytes of key data */
    *key_length = 32;
    return (context->key_data + 32);
}

static CFArrayRef
eaptls_require_props(EAPClientPluginDataRef plugin)
{
    CFArrayRef 			array = NULL;
    EAPTLSPluginDataRef		context = (EAPTLSPluginDataRef)plugin->private;

    if (context->last_client_status != kEAPClientStatusUserInputRequired) {
	goto done;
    }
    if (context->trust_proceed == FALSE) {
	CFStringRef	str = kEAPClientPropTLSUserTrustProceedCertificateChain;
	array = CFArrayCreate(NULL, (const void **)&str,
			      1, &kCFTypeArrayCallBacks);
    }
 done:
    return (array);
}

static CFDictionaryRef
eaptls_publish_props(EAPClientPluginDataRef plugin)
{
    CFArrayRef			cert_list;
    SSLCipherSuite		cipher = SSL_NULL_WITH_NULL_NULL;
    EAPTLSPluginDataRef		context = (EAPTLSPluginDataRef)plugin->private;
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
    my_CFRelease(&cert_list);
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

static bool
eaptls_packet_dump(FILE * out_f, const EAPPacketRef pkt)
{
    EAPTLSPacketRef 	eaptls_pkt = (EAPTLSPacketRef)pkt;
    EAPTLSLengthIncludedPacketRef eaptls_pkt_l;
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
    fprintf(out_f, "EAP-TLS %s: Identifier %d Length %d Flags 0x%x%s",
	    pkt->code == kEAPCodeRequest ? "Request" : "Response",
	    pkt->identifier, length, eaptls_pkt->flags,
	    eaptls_pkt->flags != 0 ? " [" : "");
    eaptls_pkt_l = (EAPTLSLengthIncludedPacketRef)pkt;
    data_ptr = eaptls_pkt->tls_data;
    tls_message_length = data_length = length - sizeof(EAPTLSPacket);

    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsStart) != 0) {
	fprintf(out_f, " start");
    }
    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsLengthIncluded) != 0) {
	if (length < sizeof(EAPTLSLengthIncludedPacket)) {
	    fprintf(out_f, "\ninvalid packet: length %d < %lu",
		    length, sizeof(EAPTLSLengthIncludedPacket));
	    goto done;
	}
	data_ptr = eaptls_pkt_l->tls_data;
	data_length = length - sizeof(EAPTLSLengthIncludedPacket);
	tls_message_length 
	    = ntohl(*((u_int32_t *)eaptls_pkt_l->tls_message_length));
	fprintf(out_f, " length=%u", tls_message_length);
	
    }
    if ((eaptls_pkt->flags & kEAPTLSPacketFlagsMoreFragments) != 0) {
	fprintf(out_f, " more");
    }
    fprintf(out_f, "%s Data Length %d\n", eaptls_pkt->flags != 0 ? " ]" : "",
	    data_length);
    if (tls_message_length > kAvoidDenialOfServiceSize) {
	fprintf(out_f, "rejecting packet to avoid DOS attack %u > %d\n",
		tls_message_length, kAvoidDenialOfServiceSize);
	goto done;
    }
    fprint_data(out_f, data_ptr, data_length);
 done:
    return (TRUE);
}

static EAPType 
eaptls_type()
{
    return (EAPTLS_EAP_TYPE);

}

static const char *
eaptls_name()
{
    return ("TLS");

}

static EAPClientPluginVersion 
eaptls_version()
{
    return (kEAPClientPluginVersion);
}

static CFStringRef
eaptls_user_name(CFDictionaryRef properties)
{
    SecCertificateRef		cert = NULL;
    EAPSecIdentityHandleRef	id_handle = NULL;
    SecIdentityRef		identity = NULL;
    OSStatus			status;
    CFStringRef			user_name = NULL;

    if (properties != NULL) {
	id_handle = CFDictionaryGetValue(properties,
					 kEAPClientPropTLSIdentityHandle);
    }
    status = EAPSecIdentityHandleCreateSecIdentity(id_handle, &identity);
    if (status != noErr) {
	goto done;
    }
    status = SecIdentityCopyCertificate(identity, &cert);
    if (status != noErr) {
	goto done;
    }
    user_name = EAPSecCertificateCopyUserNameString(cert);
 done:
    my_CFRelease(&cert);
    my_CFRelease(&identity);
    return (user_name);
}

static struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
#if 0
    { kEAPClientPluginFuncNameIntrospect, eaptls_introspect },
#endif /* 0 */
    { kEAPClientPluginFuncNameVersion, eaptls_version },
    { kEAPClientPluginFuncNameEAPType, eaptls_type },
    { kEAPClientPluginFuncNameEAPName, eaptls_name },
    { kEAPClientPluginFuncNameInit, eaptls_init },
    { kEAPClientPluginFuncNameFree, eaptls_free },
    { kEAPClientPluginFuncNameProcess, eaptls_process },
    { kEAPClientPluginFuncNameFreePacket, eaptls_free_packet },
    { kEAPClientPluginFuncNameFailureString, eaptls_failure_string },
    { kEAPClientPluginFuncNameSessionKey, eaptls_session_key },
    { kEAPClientPluginFuncNameServerKey, eaptls_server_key },
    { kEAPClientPluginFuncNameRequireProperties, eaptls_require_props },
    { kEAPClientPluginFuncNamePublishProperties, eaptls_publish_props },
    { kEAPClientPluginFuncNamePacketDump, eaptls_packet_dump },
    { kEAPClientPluginFuncNameUserName, eaptls_user_name },
    { NULL, NULL},
};


EAPClientPluginFuncRef
eaptls_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;


    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}
