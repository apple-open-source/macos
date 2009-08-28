/*
 * Copyright (c) 2003-2008 Apple Inc. All rights reserved.
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
 * eapmschapv2_plugin.c
 * - EAP/MSCHAPv2 plug-in
 */

/* 
 * Modification History
 *
 * May 21, 2003	Dieter Siegmund (dieter@apple)
 * - created
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
#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPUtil.h>
#include <EAP8021X/EAPClientModule.h>
#include <EAP8021X/mschap.h>
#include "myCFUtil.h"
#include "printdata.h"

/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect eapmschapv2_introspect;
static EAPClientPluginFuncVersion eapmschapv2_version;
static EAPClientPluginFuncEAPType eapmschapv2_type;
static EAPClientPluginFuncEAPName eapmschapv2_name;
static EAPClientPluginFuncInit eapmschapv2_init;
static EAPClientPluginFuncFree eapmschapv2_free;
static EAPClientPluginFuncProcess eapmschapv2_process;
static EAPClientPluginFuncFreePacket eapmschapv2_free_packet;
static EAPClientPluginFuncRequireProperties eapmschapv2_require_props;
static EAPClientPluginFuncPublishProperties eapmschapv2_publish_props;
static EAPClientPluginFuncPacketDump eapmschapv2_packet_dump;
static EAPClientPluginFuncSessionKey eapmschapv2_session_key;
static EAPClientPluginFuncServerKey eapmschapv2_server_key;

typedef struct {
    NTPasswordBlock		encrypted_password;
    uint8_t			encrypted_hash[NT_PASSWORD_HASH_SIZE];
    uint8_t			peer_challenge[MSCHAP2_CHALLENGE_SIZE];
    uint8_t			reserved[MSCHAP2_RESERVED_SIZE];
    uint8_t			nt_response[MSCHAP_NT_RESPONSE_SIZE];
    uint8_t			flags[2];
} EAPMSCHAPv2ChangePasswordResponse, * EAPMSCHAPv2ChangePasswordResponseRef;

enum {
    kMSCHAPv2ChangePasswordVersion = 3
};

#define EAP_MSCHAP2_CHANGE_PASSWORD_RESPONSE_LENGTH (NT_PASSWORD_BLOCK_SIZE \
				 + NT_PASSWORD_HASH_SIZE \
				 + MSCHAP2_CHALLENGE_SIZE \
				 + MSCHAP2_RESERVED_SIZE \
				 + MSCHAP_NT_RESPONSE_SIZE \
				 + 2)
enum {
    kMSCHAPv2OpCodeChallenge = 1,
    kMSCHAPv2OpCodeResponse = 2,
    kMSCHAPv2OpCodeSuccess = 3,
    kMSCHAPv2OpCodeFailure = 4,
    kMSCHAPv2OpCodeChangePassword = 7
};
typedef uint8_t		MSCHAPv2OpCode;

static const char *
MSCHAPv2OpCodeStr(MSCHAPv2OpCode op_code)
{
    switch (op_code) {
    case kMSCHAPv2OpCodeChallenge:
	return ("Challenge");
    case kMSCHAPv2OpCodeResponse:
	return ("Response");
    case kMSCHAPv2OpCodeSuccess:
	return ("Success");
    case kMSCHAPv2OpCodeFailure:
	return ("Failure");
    case kMSCHAPv2OpCodeChangePassword:
	return ("ChangePassword");
    default:
	break;
    }
    return ("<unknown>");
}

/*
 * EAP_MSCHAP2_MS_LENGTH_DIFFERENCE
 *
 * pkt.ms_length is always (pkt.length - 5) because ms_length is the number of
 * bytes starting with the op_code field (offsetof(pkt.op_code) == 5).
 */
#define EAP_MSCHAP2_MS_LENGTH_DIFFERENCE	(sizeof(EAPRequestPacket))

typedef struct EAPMSCHAPv2Packet_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		op_code;
    uint8_t		mschapv2_id;
    uint8_t		ms_length[2];	/* must be pkt.length - 5 */
    uint8_t		data[0];
} EAPMSCHAPv2Packet, *EAPMSCHAPv2PacketRef;

typedef struct EAPMSCHAPv2ChallengePacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		op_code;
    uint8_t		mschapv2_id;
    uint8_t		ms_length[2];	/* pkt.length - 5 */
    uint8_t		value_size;	/* MSCHAP2_CHALLENGE_SIZE */
    uint8_t		challenge[MSCHAP2_CHALLENGE_SIZE];
    uint8_t		name[0];
} EAPMSCHAPv2ChallengePacket, *EAPMSCHAPv2ChallengePacketRef;

typedef struct EAPMSCHAPv2ResponsePacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		op_code;
    uint8_t		mschapv2_id;
    uint8_t		ms_length[2];	/* pkt.length - 5 */
    uint8_t		value_size;	/* MSCHAP2_RESPONSE_LENGTH */
    uint8_t		response[MSCHAP2_RESPONSE_LENGTH];
    uint8_t		name[0];
} EAPMSCHAPv2ResponsePacket, *EAPMSCHAPv2ResponsePacketRef;

typedef struct EAPMSCHAPv2SuccessRequestPacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		op_code;
    uint8_t		mschapv2_id;
    uint8_t		ms_length[2];	/* pkt.length - 5 */
    uint8_t		auth_response[MSCHAP2_AUTH_RESPONSE_SIZE];
    uint8_t		message[0];
} EAPMSCHAPv2SuccessRequestPacket, *EAPMSCHAPv2SuccessRequestPacketRef;

typedef struct EAPMSCHAPv2SuccessResponsePacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		op_code;
} EAPMSCHAPv2SuccessResponsePacket, *EAPMSCHAPv2SuccessResponsePacketRef;

typedef struct EAPMSCHAPv2FailureRequestPacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		op_code;
    uint8_t		mschapv2_id;
    uint8_t		ms_length[2];	/* pkt.length - 5 */
    uint8_t		message[0];
} EAPMSCHAPv2FailureRequestPacket, *EAPMSCHAPv2FailureRequestPacketRef;

typedef struct EAPMSCHAPv2FailureResponsePacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		op_code;
} EAPMSCHAPv2FailureResponsePacket, *EAPMSCHAPv2FailureResponsePacketRef;

typedef struct EAPMSCHAPv2ChangePasswordPacket_s {
    uint8_t		code;
    uint8_t		identifier;
    uint8_t		length[2];	/* of entire request/response */
    uint8_t		type;
    uint8_t		op_code;
    uint8_t		mschapv2_id;
    uint8_t		ms_length[2];	/* pkt.length - 5 */
    uint8_t		data[EAP_MSCHAP2_CHANGE_PASSWORD_RESPONSE_LENGTH];
} EAPMSCHAPv2ChangePasswordPacket, *EAPMSCHAPv2ChangePasswordPacketRef;

static __inline__ void
EAPMSCHAPv2PacketSetMSLength(EAPMSCHAPv2PacketRef pkt, uint16_t length)
{
    *((u_short *)pkt->ms_length) = htons(length);
    return;
}

static __inline__ uint16_t
EAPMSCHAPv2PacketGetMSLength(const EAPMSCHAPv2PacketRef pkt)
{
    return (ntohs(*((u_short *)pkt->ms_length)));
}

typedef enum {
    kMSCHAPv2ClientStateNone = 0,
    kMSCHAPv2ClientStateResponseSent,
    kMSCHAPv2ClientStateSuccessResponseSent,
    kMSCHAPv2ClientStateChangePasswordSent,
    kMSCHAPv2ClientStateSuccess,
    kMSCHAPv2ClientStateFailure,
} MSCHAPv2ClientState;

typedef struct {
    MSCHAPv2ClientState		state;
    EAPClientState		plugin_state;
    bool			need_password;
    bool			need_new_password;
    uint32_t			last_generation;
    uint8_t			peer_challenge[MSCHAP2_CHALLENGE_SIZE];
    uint8_t			nt_response[MSCHAP_NT_RESPONSE_SIZE];
    uint8_t			auth_challenge[MSCHAP2_CHALLENGE_SIZE];
    uint8_t			session_key[NT_SESSION_KEY_SIZE * 2];
    bool			session_key_valid;
    uint8_t			pkt_buffer[1024];
} EAPMSCHAPv2PluginData, * EAPMSCHAPv2PluginDataRef;

static void
eapmschapv2_free_context(EAPMSCHAPv2PluginData * context)
{
    free(context);
    return;
}

static void
EAPMSCHAPv2PluginDataInit(EAPMSCHAPv2PluginDataRef context)
{
    context->state = kMSCHAPv2ClientStateNone;
    context->plugin_state = kEAPClientStateAuthenticating;
    context->need_password = FALSE;
    context->need_new_password = FALSE;
    context->session_key_valid = FALSE;
    return;
}

static EAPClientStatus
eapmschapv2_init(EAPClientPluginDataRef plugin, CFArrayRef * require_props,
		 EAPClientDomainSpecificError * error)
{
    EAPMSCHAPv2PluginDataRef	context = NULL;
    EAPClientStatus		result = kEAPClientStatusOK;

    *error = 0;
    *require_props = NULL;
    context = malloc(sizeof(*context));
    plugin->private = context;
    EAPMSCHAPv2PluginDataInit(context);
    context->last_generation = plugin->generation;
    return (result);
}

static void
eapmschapv2_free(EAPClientPluginDataRef plugin)
{
    EAPMSCHAPv2PluginDataRef context;

    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    if (context != NULL) {
	eapmschapv2_free_context(context);
	plugin->private = NULL;
    }
    return;
}

static void
eapmschapv2_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    EAPMSCHAPv2PluginDataRef context;

    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    if ((void*)arg != context->pkt_buffer) {
	free(arg);
    }
    return;
}

static EAPMSCHAPv2ResponsePacketRef
EAPMSCHAPv2ResponsePacketCreate(EAPClientPluginDataRef plugin, 
				int identifier, int mschapv2_id,
				EAPClientStatus * client_status)
{
    CFDataRef				client_challenge;
    EAPMSCHAPv2PluginDataRef 		context;
    EAPMSCHAPv2ResponsePacketRef	out_pkt_p;
    MSCHAP2ResponseRef			resp_p;
    int					out_length;

    /* check for out-of-band client challenge */
    client_challenge
	= CFDictionaryGetValue(plugin->properties,
			       kEAPClientPropEAPMSCHAPv2ClientChallenge);
    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    out_length = sizeof(*out_pkt_p) + plugin->username_length;
    out_pkt_p = (EAPMSCHAPv2ResponsePacketRef)
	EAPPacketCreate(context->pkt_buffer, sizeof(context->pkt_buffer),
			kEAPCodeResponse, identifier,
			kEAPTypeMSCHAPv2, NULL, 
			out_length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE,
			NULL);

    if (client_challenge != NULL) {
	if (CFDataGetLength(client_challenge)
	    != sizeof(context->peer_challenge)) {
	    syslog(LOG_NOTICE,
		   "EAPMSCHAPv2ResponsePacketCreate: internal error %d !=%d",
		   CFDataGetLength(client_challenge),
		   sizeof(context->peer_challenge));
	    *client_status = kEAPClientStatusInternalError;
	    context->plugin_state = kEAPClientStateFailure;
	    return (NULL);
	}
	memcpy(context->peer_challenge, CFDataGetBytePtr(client_challenge),
	       sizeof(context->peer_challenge));
    }
    else {
	MSChapFillWithRandom(context->peer_challenge,
			     sizeof(context->peer_challenge));
    }
    MSChap2(context->auth_challenge, context->peer_challenge, 	
	    plugin->username, plugin->password, plugin->password_length,
	    context->nt_response);

    /* fill in the data */
    out_pkt_p->op_code = kMSCHAPv2OpCodeResponse;
    out_pkt_p->mschapv2_id = mschapv2_id;
    EAPMSCHAPv2PacketSetMSLength((EAPMSCHAPv2PacketRef)out_pkt_p,
				 out_length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE);
    out_pkt_p->value_size = sizeof(out_pkt_p->response);
    resp_p = (MSCHAP2ResponseRef)out_pkt_p->response;
    if (client_challenge == NULL) {
	memcpy(resp_p->peer_challenge, context->peer_challenge, 
	       sizeof(context->peer_challenge));
    }
    else {
	memset(resp_p->peer_challenge, 0, sizeof(resp_p->peer_challenge));
    }
    memset(resp_p->reserved, 0, sizeof(resp_p->reserved));
    memcpy(resp_p->nt_response, context->nt_response, 
	   sizeof(context->nt_response));
    resp_p->flags[0] = 0;
    memcpy(out_pkt_p->name, plugin->username, plugin->username_length);
    return (out_pkt_p);
}

static EAPMSCHAPv2PacketRef
eapmschapv2_challenge(EAPClientPluginDataRef plugin,
		      EAPMSCHAPv2PacketRef in_pkt_p,
		      uint16_t in_length,
		      EAPClientStatus * client_status,
		      EAPClientDomainSpecificError * error)
{
    EAPMSCHAPv2PluginDataRef 		context;
    EAPMSCHAPv2ChallengePacketRef	challenge_p;
    EAPMSCHAPv2ResponsePacketRef	out_pkt_p;
    CFDataRef				server_challenge;

    if (in_length < sizeof(*challenge_p)) {
	syslog(LOG_NOTICE, "eapmschapv2_challenge: length %d < %d",
	       in_length, sizeof(*challenge_p));
	goto done;
    }
    challenge_p = (EAPMSCHAPv2ChallengePacketRef)in_pkt_p;
    context = (EAPMSCHAPv2PluginDataRef)plugin->private;

    /* if we don't have a password, ask for one */
    EAPMSCHAPv2PluginDataInit(context);
    if (plugin->password == NULL) {
	context->need_password = TRUE;
	*client_status = kEAPClientStatusUserInputRequired;
	goto done;
    }

    /* check for out-of-band server challenge */
    server_challenge
	= CFDictionaryGetValue(plugin->properties,
			       kEAPClientPropEAPMSCHAPv2ServerChallenge);
    if (server_challenge != NULL) {
	if (CFDataGetLength(server_challenge) 
	    != sizeof(context->auth_challenge)) {
	    syslog(LOG_NOTICE,
		   "eapmschapv2_challenge: internal error %d !=%d",
		   CFDataGetLength(server_challenge),
		   sizeof(context->auth_challenge));
	    *client_status = kEAPClientStatusInternalError;
	    context->plugin_state = kEAPClientStateFailure;
	    goto done;
	}
	memcpy(context->auth_challenge, CFDataGetBytePtr(server_challenge),
	       sizeof(context->auth_challenge));
    }
    else {
	/* remember the auth challenge for later */
	memcpy(context->auth_challenge, challenge_p->challenge,
	       sizeof(context->auth_challenge));
    }
    out_pkt_p = EAPMSCHAPv2ResponsePacketCreate(plugin, in_pkt_p->identifier,
						challenge_p->mschapv2_id,
						client_status);
    if (out_pkt_p == NULL) {
	goto done;
    }
    context->state = kMSCHAPv2ClientStateResponseSent;
    return ((EAPMSCHAPv2PacketRef)out_pkt_p);

 done:
    return (NULL);
}

static void
eapmschapv2_compute_session_key(EAPClientPluginDataRef plugin)
{
    EAPMSCHAPv2PluginDataRef 		context;
    uint8_t				master_key[NT_MASTER_KEY_SIZE];

    context = (EAPMSCHAPv2PluginDataRef)plugin->private;

    MSChap2_MPPEGetMasterKey(plugin->password, plugin->password_length,
			     context->nt_response,
			     master_key);
    MSChap2_MPPEGetAsymetricStartKey(master_key,
				     context->session_key,
				     NT_SESSION_KEY_SIZE,
				     TRUE, TRUE);
    MSChap2_MPPEGetAsymetricStartKey(master_key,
				     context->session_key + NT_SESSION_KEY_SIZE,
				     NT_SESSION_KEY_SIZE,
				     FALSE, TRUE);
    context->session_key_valid = TRUE;
    return;
}

static EAPMSCHAPv2PacketRef
eapmschapv2_success_request(EAPClientPluginDataRef plugin,
			    EAPMSCHAPv2PacketRef in_pkt_p,
			    uint16_t in_length,
			    EAPClientStatus * client_status,
			    EAPClientDomainSpecificError * error)
{
    EAPMSCHAPv2PluginDataRef 		context;
    EAPMSCHAPv2SuccessRequestPacketRef	r_p;
    EAPMSCHAPv2SuccessResponsePacketRef	out_pkt_p = NULL;

    if (in_length < sizeof(*r_p)) {
	syslog(LOG_NOTICE, "eapmschapv2_success_request: length %d < %d",
	       in_length, sizeof(*r_p));
	goto done;
    }
    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    switch (context->state) {
    case kMSCHAPv2ClientStateResponseSent:
    case kMSCHAPv2ClientStateChangePasswordSent:
	break;
    case kMSCHAPv2ClientStateSuccess:
	break;
    case kMSCHAPv2ClientStateFailure:
    default:
	goto done;
    }
    r_p = (EAPMSCHAPv2SuccessRequestPacketRef)in_pkt_p;
    /* process success request */
    if (MSChap2AuthResponseValid((const uint8_t *)plugin->password,
				 plugin->password_length,
				 context->nt_response,
				 context->peer_challenge,
				 context->auth_challenge,
				 (const uint8_t *)plugin->username,
				 r_p->auth_response) 
	== FALSE) {
	syslog(LOG_NOTICE,
	       "eapmschapv2_success_request: invalid server auth response");
	context->plugin_state = kEAPClientStateFailure;
	context->state = kMSCHAPv2ClientStateFailure;
	*client_status = kEAPClientStatusFailed;
	goto done;
    }
    switch (context->state) {
    case kMSCHAPv2ClientStateResponseSent:
	syslog(LOG_NOTICE,
	       "eapmschapv2_success_request: successfully authenticated");
	break;
    case kMSCHAPv2ClientStateChangePasswordSent:
	syslog(LOG_NOTICE,
	       "eapmschapv2_success_request: change password succeeded");
	break;
    default:
	break;
    }
    eapmschapv2_compute_session_key(plugin);
    context->state = kMSCHAPv2ClientStateSuccess;
    out_pkt_p = (EAPMSCHAPv2SuccessResponsePacketRef)
	EAPPacketCreate(context->pkt_buffer, sizeof(context->pkt_buffer),
			kEAPCodeResponse, in_pkt_p->identifier,
			kEAPTypeMSCHAPv2, NULL,
			sizeof(*out_pkt_p) - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE,
			NULL);
    out_pkt_p->op_code = kMSCHAPv2OpCodeSuccess;
    return ((EAPMSCHAPv2PacketRef)out_pkt_p);

 done:
    if (out_pkt_p != NULL) {
	free(out_pkt_p);
    }
    return (NULL);
}


#if 0
enum {
    kMSCHAP2FailureMessageFlagError = 0x1,
    kMSCHAP2FailureMessageFlagRetry = 0x2,
    kMSCHAP2FailureMessageFlagChallenge = 0x4,
    kMSCHAP2FailureMessageFlagVersion = 0x8,
    kMSCHAP2FailureMessageFlagMessage = 0x10,
};

static bool
mschap2_message_int32_attr(const uint8_t * message, uint16_t message_length,
			   const char * attr, int attr_len, 
			   int32_t * int_value)
{
    bool	present = FALSE;
    char *	val;

    val = strnstr(message, attr, message_length);
    if (val != NULL && message_length > attr_len) {
	val += attr_len;
	
	*int_value = strtol(val, NULL, 10);
	present = TRUE;
    }
    return (present);
}

static bool
mschap2_message_challenge_attr(const uint8_t * message, uint16_t message_length,
			       const char * attr,  int attr_len,
			       uint8_t challenge[MSCHAP2_CHALLENGE_SIZE])
{
    int		i;
    bool	present = FALSE;
    char *	val;

    val = strnstr(message, attr, message_length);
    if (val != NULL 
	&& message_length > (attr_len + MSCHAP2_CHALLENGE_SIZE * 2)) {
	char str[3];

	str[2] = '\0'; /* nul-terminate */
	val += attr_len;
	for (i = 0; i < MSCHAP2_CHALLENGE_SIZE; i++) {
	    str[0] = val[0];
	    str[1] = val[1];
	    challenge[i] = strtoul(str, NULL, 16);
	    val += 2;
	}
	present = TRUE;
    }
    return (present);
}

static uint8_t
MSCHAPv2ParseFailureMessage(const uint8_t * message, 
			    uint16_t message_length,
			    int32_t * error, int32_t * retry,
			    uint8_t challenge[MSCHAP2_CHALLENGE_SIZE],
			    int32_t * version, uint8_t * * ret_message)
{
    uint32_t	flags = 0;
    char *	val;

    /*
     * Parse the message string format:
     *   E=eeeeeeeee R=r C=cccccccccccccccccccccccccccccccc V=vvvvvvvvvv M=mm...
     * We don't assume that they are in any particular order, since that's
     * the most flexible.
     */
    if (mschap2_message_int32_attr(message, message_length, "E=", 2, error)) {
	flags |= kMSCHAP2FailureMessageFlagError;
    }
    if (mschap2_message_int32_attr(message, message_length, "R=", 2, retry)) {
	flags |= kMSCHAP2FailureMessageFlagRetry;
    }
    if (mschap2_message_int32_attr(message, message_length, "V=", 2, version)) {
	flags |= kMSCHAP2FailureMessageFlagVersion;
    }
    if (mschap2_message_challenge_attr(message, message_length, 
				       "C=", 2, challenge)) {
	flags |= kMSCHAP2FailureMessageFlagChallenge;
    }
    val = strnstr(message, "M=", message_length);
    if (val != NULL) {
	*ret_message = val + 2;
	flags |= kMSCHAP2FailureMessageFlagMessage;
    }
    return (flags);
}
#endif 0

static EAPMSCHAPv2PacketRef
eapmschapv2_failure_request(EAPClientPluginDataRef plugin,
			    EAPMSCHAPv2PacketRef in_pkt_p,
			    uint16_t in_length,
			    EAPClientStatus * client_status,
			    EAPClientDomainSpecificError * error)
{
    EAPMSCHAPv2PluginDataRef 		context;
    bool				handled = FALSE;
    char *				message = NULL;
#if 0
    int					message_length;
    uint32_t				flags = 0;
    int32_t				r_error;
    int32_t				r_retry = 0;
    int32_t				r_version = 0;
    uint8_t *				r_message;
#endif 0
    EAPMSCHAPv2FailureRequestPacketRef	r_p;
    EAPMSCHAPv2PacketRef		out_pkt_p = NULL;

    if (in_length < sizeof(*r_p)) {
	syslog(LOG_NOTICE, "eapmschapv2_failure_request: length %d < %d",
	       in_length, sizeof(*r_p));
	goto done;
    }
    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    switch (context->state) {
    case kMSCHAPv2ClientStateResponseSent:
    case kMSCHAPv2ClientStateChangePasswordSent:
	break;
    case kMSCHAPv2ClientStateFailure:
	break;
    case kMSCHAPv2ClientStateSuccess:
    default:
	goto done;
    }
    r_p = (EAPMSCHAPv2FailureRequestPacketRef)in_pkt_p;
#if 0
    do { /* something to break out of */
	/* allocate a message buffer that's guaranteed to be nul-terminated */
	message_length = in_length - sizeof(*r_p);
	if (message_length == 0) {
	    break;
	}
	message = malloc(message_length + 1);
	memcpy(message, r_p->message, message_length);
	message[message_length] = '\0';
	
	/* parse it */
	flags = MSCHAPv2ParseFailureMessage(message, message_length,
					    &r_error, &r_retry, 
					    context->auth_challenge,
					    &r_version, &r_message);
	
	if ((flags & kMSCHAP2FailureMessageFlagError) == 0) {
	    break;
	}
	syslog(LOG_NOTICE, "MSCHAPv2 Error = %d, Retry = %d, Version = %d", 
	       r_error, r_retry, r_version);

	if (r_retry == 0) {
	    /* can't retry, acknowledge the error */
	    break;
	}
	if ((flags & kMSCHAP2FailureMessageFlagChallenge) == 0) {
	    syslog(LOG_NOTICE, 
		   "MSCHAPv2 Failure Request does not contain challenge");
	    break;
	}
	if (r_error != kMSCHAP2_ERROR_PASSWD_EXPIRED) {
	    if (context->last_generation == plugin->generation
		|| plugin->password == NULL) {
		context->need_password = TRUE;
		*client_status = kEAPClientStatusUserInputRequired;
	    }
	    else {
		out_pkt_p = (EAPMSCHAPv2PacketRef)
		    EAPMSCHAPv2ResponsePacketCreate(plugin, 
						    r_p->identifier,
						    r_p->mschapv2_id);
		if (out_pkt_p == NULL) {
		    break;
		}
		context->state = kMSCHAPv2ClientStateResponseSent;
	    }
	}
	else {
	    char *	new_password = NULL;

	    if (r_version != kMSCHAPv2ChangePasswordVersion) {
		break;
	    }
	    if (plugin->password == NULL) {
		break;
	    }
	    if (context->last_generation != plugin->generation
		&& plugin->properties != NULL) {
		CFStringRef		new_password_cf;
		
		new_password_cf 
		    = CFDictionaryGetValue(plugin->properties,
					   kEAPClientPropNewPassword);
		if (new_password_cf != NULL) {
		    new_password = my_CFStringToCString(new_password_cf,
							kCFStringEncodingASCII);
		}
	    }
	    if (new_password != NULL) {
		out_pkt_p 
		    = EAPMSCHAPv2ChangePasswordPacketCreate(plugin,
							    r_p->identifer,
							    r_p->mschapv2_id,
							    new_password,
							    strlen(new_password));
		if (out_pkt_p == NULL) {
		    break;
		}
		context->state = kMSCHAPv2ClientStateChangePasswordSent;
	    }
	    else {
		context->need_new_password = TRUE;
		*client_status = kEAPClientStatusUserInputRequired;
	    }
	    if (new_password != NULL) {
		free(new_password);
	    }
	}
	handled = TRUE;
    } while (0); /* something to break out of */
#endif 0

    if (handled == FALSE) {
	context->state = kMSCHAPv2ClientStateFailure;
	context->plugin_state = kEAPClientStateFailure;

	*client_status = kEAPClientStatusFailed;
	out_pkt_p = (EAPMSCHAPv2PacketRef)
	    EAPPacketCreate(context->pkt_buffer, sizeof(context->pkt_buffer),
			    kEAPCodeResponse, in_pkt_p->identifier,
			    kEAPTypeMSCHAPv2, NULL,
			    sizeof(*out_pkt_p) - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE,
			    NULL);
	out_pkt_p->op_code = kMSCHAPv2OpCodeFailure;
    }

    if (message != NULL) {
	free(message);
    }
    return (out_pkt_p);

 done:
    if (out_pkt_p != NULL) {
	free(out_pkt_p);
    }
    if (message != NULL) {
	free(message);
    }
    return (NULL);
}

static EAPPacketRef
eapmschapv2_request(EAPClientPluginDataRef plugin, 
		    const EAPPacketRef in_pkt,
		    EAPClientStatus * client_status,
		    EAPClientDomainSpecificError * error)
{
    EAPMSCHAPv2PluginDataRef 	context;
    EAPMSCHAPv2PacketRef	mschap_in_p;
    EAPMSCHAPv2PacketRef	mschap_out_p = NULL;
    uint16_t			in_length = EAPPacketGetLength(in_pkt);

    mschap_in_p = (EAPMSCHAPv2PacketRef)in_pkt;
    if (in_length < sizeof(*mschap_in_p)) {
	syslog(LOG_NOTICE, "eapmschapv2_request: length %d < %d",
	       in_length, sizeof(*mschap_in_p));
	goto done;
    }
    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    switch (mschap_in_p->op_code) {
    case kMSCHAPv2OpCodeChallenge:
	mschap_out_p = eapmschapv2_challenge(plugin, mschap_in_p, in_length,
					     client_status, error);
	break;
    case kMSCHAPv2OpCodeResponse:
	/* ignore */
	break;
    case kMSCHAPv2OpCodeSuccess:
	mschap_out_p = eapmschapv2_success_request(plugin, mschap_in_p, 
						   in_length, client_status,
						   error);
	break;
    case kMSCHAPv2OpCodeFailure:
	mschap_out_p = eapmschapv2_failure_request(plugin, mschap_in_p, 
						   in_length, client_status,
						   error);
	break;
    default:
	break;
    }
    return ((EAPPacketRef)mschap_out_p);

 done:
    if (mschap_out_p != NULL) {
	free(mschap_out_p);
    }
    return (NULL);
}

static EAPClientState
eapmschapv2_process(EAPClientPluginDataRef plugin, 
		    const EAPPacketRef in_pkt,
		    EAPPacketRef * out_pkt_p, 
		    EAPClientStatus * client_status,
		    EAPClientDomainSpecificError * error)
{
    EAPMSCHAPv2PluginDataRef 	context;

    context = (EAPMSCHAPv2PluginDataRef)plugin->private;

    *client_status = kEAPClientStatusOK;
    *error = 0;
    *out_pkt_p = NULL;
    switch (in_pkt->code) {
    case kEAPCodeRequest:
	*out_pkt_p = eapmschapv2_request(plugin, in_pkt, client_status,
					 error);
	break;
    case kEAPCodeSuccess:
	if (context->state == kMSCHAPv2ClientStateSuccess) {
	    context->plugin_state = kEAPClientStateSuccess;
	}
	break;
    case kEAPCodeFailure:
	if (context->state == kMSCHAPv2ClientStateFailure) {
	    context->plugin_state = kEAPClientStateFailure;
	}
	break;
    case kEAPCodeResponse:
    default:
	/* ignore */
	break;
    }
    context->last_generation = plugin->generation;
    return (context->plugin_state);
}

static CFDictionaryRef
eapmschapv2_publish_props(EAPClientPluginDataRef plugin)
{
    return (NULL);
}

static CFArrayRef
eapmschapv2_require_props(EAPClientPluginDataRef plugin)
{
    CFMutableArrayRef 		array = NULL;
    EAPMSCHAPv2PluginDataRef	context;

    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    if (context->need_password || context->need_new_password) {
	array = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if (context->need_password) {
	    CFArrayAppendValue(array, kEAPClientPropUserPassword);
	}
	if (context->need_new_password) {
	    CFArrayAppendValue(array, kEAPClientPropNewPassword);
	}
    }
    return (array);
}

static void
eapmschapv2_challenge_dump(FILE * out_f, const EAPPacketRef pkt,
			   int length)
{
    EAPMSCHAPv2ChallengePacketRef	challenge_p;
    int					name_length;

    if (length < sizeof(*challenge_p)) {
	fprintf(out_f, "Error: length %d < %d\n", length, 
		(int)sizeof(*challenge_p));
	return;
    }
    challenge_p = (EAPMSCHAPv2ChallengePacketRef)pkt;
    fprintf(out_f, "MS-CHAPv2-ID %d MS-Length %d Value-Size %d\n",
	    challenge_p->mschapv2_id, 
	    EAPMSCHAPv2PacketGetMSLength((EAPMSCHAPv2PacketRef)pkt),
	    challenge_p->value_size);

    if (challenge_p->value_size != sizeof(challenge_p->challenge)) {
	fprintf(out_f, "Error: Value-Size should be %d\n",
		(int)sizeof(challenge_p->challenge));
    }
    if (EAPMSCHAPv2PacketGetMSLength((EAPMSCHAPv2PacketRef)pkt) 
	!= (length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE)) {
	fprintf(out_f, "Error: MS-Length should be %d\n",
		(int)(length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE));
    }
    fprintf(out_f, "Challenge: ");
    fprint_bytes(out_f, challenge_p->challenge, sizeof(challenge_p->challenge));
    fprintf(out_f, "\n");
    name_length = length - sizeof(*challenge_p);
    if (name_length > 0) {
	fprintf(out_f, "Name: %.*s\n", name_length, challenge_p->name);
    }
    return;
}

static void
eapmschapv2_response_dump(FILE * out_f, const EAPPacketRef pkt,
			   int length)
{
    EAPMSCHAPv2ResponsePacketRef	r_p;
    int					name_length;
    MSCHAP2ResponseRef		resp_p;

    if (length < sizeof(*r_p)) {
	fprintf(out_f, "Error: length %d < %d\n", length, 
		(int)sizeof(*r_p));
	return;
    }
    r_p = (EAPMSCHAPv2ResponsePacketRef)pkt;
    fprintf(out_f, "MS-CHAPv2-ID %d MS-Length %d Value-Size %d\n",
	    r_p->mschapv2_id, 
	    EAPMSCHAPv2PacketGetMSLength((EAPMSCHAPv2PacketRef)pkt),
	    r_p->value_size);

    if (r_p->value_size != sizeof(r_p->response)) {
	fprintf(out_f, "Error: Value-Size should be %d\n",
		(int)sizeof(r_p->response));
    }
    if (EAPMSCHAPv2PacketGetMSLength((EAPMSCHAPv2PacketRef)pkt) 
	!= (length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE)) {
	fprintf(out_f, "Error: MS-Length should be %d\n",
		(int)(length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE));
    }
    resp_p = (MSCHAP2ResponseRef)r_p->response;
    fprintf(out_f, "Response:\n");
    fprintf(out_f, 
	    "Peer Challenge: ");
    fprint_bytes(out_f, 
		 resp_p->peer_challenge, sizeof(resp_p->peer_challenge));
    fprintf(out_f, "\n"
	    "Reserved:       ");
    fprint_bytes(out_f, 
		 resp_p->reserved, sizeof(resp_p->reserved));
    fprintf(out_f, "\n"
	    "NT Response:    ");
    fprint_bytes(out_f, 
		 resp_p->nt_response, sizeof(resp_p->nt_response));
    fprintf(out_f, "\n"
	    "Flags:          ");
    fprint_bytes(out_f, 
		 resp_p->flags, sizeof(resp_p->flags));
    name_length = length - sizeof(*r_p);
    if (name_length > 0) {
	fprintf(out_f, "\n"
		"Name:           %.*s\n", name_length, r_p->name);
    }
    return;
}

static void
eapmschapv2_success_request_dump(FILE * out_f, const EAPPacketRef pkt,
				 int length)
{
    EAPMSCHAPv2SuccessRequestPacketRef	r_p;
    int					message_length;

    if (length < sizeof(*r_p)) {
	fprintf(out_f, "Error: length %d < %d\n", length, (int)sizeof(*r_p));
	return;
    }
    r_p = (EAPMSCHAPv2SuccessRequestPacketRef)pkt;
    fprintf(out_f, "MS-CHAPv2-ID %d MS-Length %d\n",
	    r_p->mschapv2_id, 
	    EAPMSCHAPv2PacketGetMSLength((EAPMSCHAPv2PacketRef)pkt));
    if (EAPMSCHAPv2PacketGetMSLength((EAPMSCHAPv2PacketRef)pkt)
	!= (length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE)) {
	fprintf(out_f, "Error: MS-Length should be %d\n",
		(int)(length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE));
    }
    fprintf(out_f, 
	    "Auth Response: %.*s\n", (int)sizeof(r_p->auth_response), 
	    r_p->auth_response);
    message_length = length - sizeof(*r_p);
    if (message_length > 1) {
	/* skip the space when we print it out */
	fprintf(out_f, 
		"Message:       %.*s\n", message_length - 1,
		r_p->message + 1);
    }
    return;
}

static void
eapmschapv2_failure_request_dump(FILE * out_f, const EAPPacketRef pkt,
				 int length)
{
    EAPMSCHAPv2FailureRequestPacketRef	r_p;
    int					message_length;

    if (length < sizeof(*r_p)) {
	fprintf(out_f, "Error: length %d < %d\n", length, (int)sizeof(*r_p));
	return;
    }
    r_p = (EAPMSCHAPv2FailureRequestPacketRef)pkt;
    fprintf(out_f, "MS-CHAPv2-ID %d MS-Length %d\n",
	    r_p->mschapv2_id, 
	    EAPMSCHAPv2PacketGetMSLength((EAPMSCHAPv2PacketRef)pkt));
    if (EAPMSCHAPv2PacketGetMSLength((EAPMSCHAPv2PacketRef)pkt) 
	!= (length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE)) {
	fprintf(out_f, "Error: MS-Length should be %d\n",
		(int)(length - EAP_MSCHAP2_MS_LENGTH_DIFFERENCE));
    }
    message_length = length - sizeof(*r_p);
    if (message_length > 0) {
	fprintf(out_f, 
		"Message:       %.*s\n", message_length, r_p->message);
    }
    return;
}

static bool
eapmschapv2_packet_dump(FILE * out_f, const EAPPacketRef pkt)
{
    EAPMSCHAPv2PacketRef	ms_pkt_p = (EAPMSCHAPv2PacketRef)pkt;
    int				data_length;
    u_int16_t			length = EAPPacketGetLength(pkt);

    switch (pkt->code) {
    case kEAPCodeRequest:
	break;
    case kEAPCodeResponse:
	break;
    default:
	/* don't handle it */
	return (FALSE);
    }
    if (length < sizeof(EAPMSCHAPv2SuccessResponsePacket)) {
	fprintf(out_f, "invalid packet: length %d < min length %ld",
		length, sizeof(*ms_pkt_p));
	goto done;
    }
    data_length = length - sizeof(EAPMSCHAPv2SuccessResponsePacket);
    fprintf(out_f, 
	    "EAP/MSCHAPv2 %s: Identifier %d Length %d OpCode %s ",
	    EAPCodeStr(pkt->code), pkt->identifier, length,
	    MSCHAPv2OpCodeStr(ms_pkt_p->op_code));
    if (pkt->code == kEAPCodeRequest) {
	/* Request */
	switch (ms_pkt_p->op_code) {
	case kMSCHAPv2OpCodeChallenge:
	    eapmschapv2_challenge_dump(out_f, pkt, length);
	    break;
	case kMSCHAPv2OpCodeResponse:
	    eapmschapv2_response_dump(out_f, pkt, length);
	    fprintf(out_f, 
		    "EAP Request contains MSCHAPv2 Response (invalid)\n");
	    break;
	case kMSCHAPv2OpCodeSuccess:
	    eapmschapv2_success_request_dump(out_f, pkt, length);
	    break;
	case kMSCHAPv2OpCodeFailure:
	    eapmschapv2_failure_request_dump(out_f, pkt, length);
	    break;
	default:
	    if (data_length > 0) {
		fprintf(out_f, "Unknown data:\n");
		fprint_data(out_f, &ms_pkt_p->mschapv2_id,
			    data_length);
	    }
	    break;
	}
    }
    else {
	/* Response */
	switch (ms_pkt_p->op_code) {
	case kMSCHAPv2OpCodeChallenge:
	    eapmschapv2_challenge_dump(out_f, pkt, length);
	    fprintf(out_f, 
		    "EAP Response contains MSCHAPv2 Challenge (invalid)\n");
	    break;
	case kMSCHAPv2OpCodeResponse:
	    eapmschapv2_response_dump(out_f, pkt, length);
	    break;
	case kMSCHAPv2OpCodeSuccess:
	case kMSCHAPv2OpCodeFailure:
	    fprintf(out_f, "\n");
	    break;
	default:
	    if (data_length > 0) {
		fprintf(out_f, "Unknown data:\n");
		fprint_data(out_f, &ms_pkt_p->mschapv2_id,
			    data_length);
	    }
	    break;
	}
    }
 done:
    return (TRUE);
}

static EAPType 
eapmschapv2_type()
{
    return (kEAPTypeMSCHAPv2);

}

static const char *
eapmschapv2_name()
{
    return ("MSCHAPv2");

}

static EAPClientPluginVersion 
eapmschapv2_version()
{
    return (kEAPClientPluginVersion);
}

static void * 
eapmschapv2_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPMSCHAPv2PluginDataRef	context;

    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    *key_length = 0;
    if (context->session_key_valid == FALSE) {
	return (NULL);
    }
    *key_length = sizeof(context->session_key);
    return (context->session_key);
}

static void * 
eapmschapv2_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    EAPMSCHAPv2PluginDataRef	context;

    context = (EAPMSCHAPv2PluginDataRef)plugin->private;
    *key_length = 0;
    if (context->session_key_valid == FALSE) {
	return (NULL);
    }
    *key_length = sizeof(context->session_key);
    return (context->session_key);
}


static struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
#if 0
    { kEAPClientPluginFuncNameIntrospect, eapmschapv2_introspect },
#endif 0
    { kEAPClientPluginFuncNameVersion, eapmschapv2_version },
    { kEAPClientPluginFuncNameEAPType, eapmschapv2_type },
    { kEAPClientPluginFuncNameEAPName, eapmschapv2_name },
    { kEAPClientPluginFuncNameInit, eapmschapv2_init },
    { kEAPClientPluginFuncNameFree, eapmschapv2_free },
    { kEAPClientPluginFuncNameProcess, eapmschapv2_process },
    { kEAPClientPluginFuncNameFreePacket, eapmschapv2_free_packet },
    { kEAPClientPluginFuncNameSessionKey, eapmschapv2_session_key },
    { kEAPClientPluginFuncNameServerKey, eapmschapv2_server_key },
    { kEAPClientPluginFuncNameRequireProperties, eapmschapv2_require_props },
    { kEAPClientPluginFuncNamePublishProperties, eapmschapv2_publish_props },
    { kEAPClientPluginFuncNamePacketDump, eapmschapv2_packet_dump },
    { NULL, NULL},
};


EAPClientPluginFuncRef
eapmschapv2_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;


    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}
