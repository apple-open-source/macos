/*
 * Copyright (c) 2001-2014 Apple Inc. All rights reserved.
 */

/* 
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */
 
#include <EAP8021X/EAPClientPlugin.h>
#include <EAP8021X/EAPClientProperties.h>
#include <EAP8021X/mschap.h>
#include <mach/boolean.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "symbol_scope.h"

#define LEAP_VERSION		1

#define LEAP_EAP_TYPE		17

/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect leap_introspect;
static EAPClientPluginFuncVersion leap_version;
static EAPClientPluginFuncEAPType leap_type;
static EAPClientPluginFuncEAPName leap_name;
static EAPClientPluginFuncInit leap_init;
static EAPClientPluginFuncFree leap_free;
static EAPClientPluginFuncProcess leap_process;
static EAPClientPluginFuncFreePacket leap_free_packet;
static EAPClientPluginFuncSessionKey leap_session_key;
static EAPClientPluginFuncServerKey leap_server_key;
static EAPClientPluginFuncMasterSessionKeyCopyBytes leap_msk_copy_bytes;

typedef struct {
    u_char		code;
    u_char		identifier;
    u_char		length[2];	/* of entire request/response */
    u_char		type;
    u_char		leap_version;
    u_char		leap_reserved;
    u_char		leap_data_length;
    u_char		leap_data[0];
} LEAPPacket, *LEAPPacketRef;

enum {
    kLEAPStateInit = 0,
    kLEAPStateResponseSent,
    kLEAPStateRequestSent,
    kLEAPStateSuccess,
    kLEAPStateFailure
};

typedef uint32_t LEAPState;

typedef struct {
    LEAPState		state;
    uint8_t			server_challenge[MSCHAP_NT_CHALLENGE_SIZE];
    uint8_t			client_challenge[MSCHAP_NT_CHALLENGE_SIZE];
    uint8_t			session_key[NT_SESSION_KEY_SIZE];
    char			failure_string[128];
} LEAPData;

static bool
LEAPVerifyRequest(LEAPData * context, LEAPPacketRef req_p)
{
    short	length = EAPPacketGetLength((EAPPacketRef)req_p);
    
    if (length < sizeof(*req_p)
	|| req_p->leap_data_length != MSCHAP_NT_CHALLENGE_SIZE
	|| (length - sizeof(*req_p) < req_p->leap_data_length)) {
	snprintf(context->failure_string, sizeof(context->failure_string),
		 "LEAPVerifyRequest: packet is too short %d", length);
	return (FALSE);
    }
    return (TRUE);
}

static bool
LEAPVerifyResponse(LEAPData * context, LEAPPacketRef resp_p, 
		   const uint8_t * password, int password_length)
{
    short	length = EAPPacketGetLength((EAPPacketRef)resp_p);
    uint8_t	expected[MSCHAP_NT_RESPONSE_SIZE];
    
    if (length < sizeof(*resp_p)
	|| resp_p->leap_data_length != MSCHAP_NT_RESPONSE_SIZE
	|| ((length - sizeof(*resp_p)) < resp_p->leap_data_length)) {
	snprintf(context->failure_string, sizeof(context->failure_string),
		 "LEAPVerifyResponse: packet is too short %d", length);
	return (FALSE);
    }

    /* 8-byte challenge gives 24-byte response */
    MSChap_MPPE(context->client_challenge, password, password_length,
		expected);
    if (bcmp(expected, resp_p->leap_data, MSCHAP_NT_RESPONSE_SIZE)) {
	snprintf(context->failure_string, sizeof(context->failure_string),
		 "LEAPVerifyResponse: server failed mutual authentication");
	return (FALSE);
    }
    NTSessionKey16(password, password_length,
		   context->client_challenge, 
		   resp_p->leap_data, /* server response */
		   context->server_challenge,
		   context->session_key);
    return (TRUE);
}

static EAPPacketRef
LEAPPacketCreate(EAPCode code, void * leap_data, int leap_data_length,
		 uint8_t identifier, const uint8_t * user, int user_length)
{
    LEAPPacketRef	leap_p;
    int			size;

    size = sizeof(*leap_p) + leap_data_length + user_length;
    leap_p = malloc(size);
    if (leap_p == NULL) {
	return (NULL);
    }
    leap_p->code = code;
    leap_p->identifier = identifier;
    EAPPacketSetLength((EAPPacketRef)leap_p, size);
    leap_p->type = LEAP_EAP_TYPE;
    leap_p->leap_version = LEAP_VERSION;
    leap_p->leap_reserved = 0;
    leap_p->leap_data_length = leap_data_length;
    bcopy(leap_data, leap_p->leap_data, leap_data_length);
    bcopy(user, leap_p->leap_data + leap_data_length, user_length);
    return ((EAPPacketRef)leap_p);
}

static CFArrayRef
leap_require_props(EAPClientPluginDataRef plugin)
{
    CFStringRef		prop;

    if (plugin->password != NULL) {
	return (NULL);
    }
    prop = kEAPClientPropUserPassword;
    return (CFArrayCreate(NULL, (const void **)&prop, 1,
			  &kCFTypeArrayCallBacks));
}

static EAPClientStatus
leap_init(EAPClientPluginDataRef plugin, CFArrayRef * required_props,
	  EAPClientDomainSpecificError * error)
{
    LEAPData *		context;

    *error = 0;
    *required_props = NULL;

    context = malloc(sizeof(*context));
    if (context == NULL) {
	return (kEAPClientStatusAllocationFailed);
    }
    bzero(context, sizeof(*context));
    context->state = kLEAPStateInit;
    plugin->private = context;
    return (kEAPClientStatusOK);
}


static void
leap_free(EAPClientPluginDataRef plugin)
{
    if (plugin->private != NULL) {
	free(plugin->private);
    }
    return;
}

static void
leap_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    if (arg != NULL) {
	free(arg);
    }
    return;
}

static EAPClientState
leap_process(EAPClientPluginDataRef plugin, 
	     const EAPPacketRef in_pkt,
	     EAPPacketRef * out_pkt_p,
	     EAPClientStatus * client_status,
	     EAPClientDomainSpecificError * domain_specific_error)
{
    uint8_t		client_response[MSCHAP_NT_RESPONSE_SIZE];
    LEAPData *		context = (LEAPData *)plugin->private;
    LEAPPacketRef	leap_in = (LEAPPacketRef)in_pkt;
    EAPClientState	plugin_state;

    *client_status = kEAPClientStatusOK;
    *domain_specific_error = 0;
    plugin_state = kEAPClientStateAuthenticating;
    *out_pkt_p = NULL;

    if (plugin->password == NULL) {
	*client_status = kEAPClientStatusUserInputRequired;
	goto done;
    }
    switch (in_pkt->code) {
    case kEAPCodeResponse:
	if (context->state == kLEAPStateRequestSent) {
	    if (LEAPVerifyResponse(context, leap_in, plugin->password,
				   plugin->password_length) 
		== FALSE) {
		plugin_state = kEAPClientStateFailure;
		context->state = kLEAPStateFailure;
	    }
	    else {
		plugin_state = kEAPClientStateSuccess;
		context->state = kLEAPStateSuccess;
	    }
	}
	break;
    case kEAPCodeRequest:
	if (LEAPVerifyRequest(context, leap_in) == FALSE) {
	    plugin_state = kEAPClientStateFailure;
	    context->state = kLEAPStateFailure;
	    break;
	}
	/* save a copy to compute session key */
	bcopy(leap_in->leap_data, context->server_challenge, 
	      sizeof(context->server_challenge));
	MSChap(context->server_challenge, plugin->password,
	       plugin->password_length, client_response);
	*out_pkt_p = (EAPPacketRef)
	    LEAPPacketCreate(kEAPCodeResponse,
			     client_response, sizeof(client_response),
			     leap_in->identifier, plugin->username,
			     plugin->username_length);
	context->state = kLEAPStateResponseSent;
	break;
    case kEAPCodeSuccess:
	if (context->state == kLEAPStateResponseSent) {
	    int		i;
	    int		n_vals;
        /* ALIGN: p is aligned to at least sizeof(uint32_t) bytes */
	    uint32_t * 	p = (uint32_t *)(void *)context->client_challenge;
	    
	    n_vals = sizeof(context->client_challenge) / sizeof(*p);
	    
	    /* compute/send challenge */
	    for (i = 0; i < n_vals; i++, p++) {
		*p = arc4random();
	    }
	    *out_pkt_p = (EAPPacketRef)
		LEAPPacketCreate(kEAPCodeRequest,
				 context->client_challenge, 
				 sizeof(context->client_challenge),
				 leap_in->identifier, 
				 plugin->username, plugin->username_length);
	    context->state = kLEAPStateRequestSent;
	    break;
	}
	break;
    case kEAPCodeFailure:
	*client_status = kEAPClientStatusFailed;
	plugin_state = kEAPClientStateFailure;
	context->state = kLEAPStateFailure;
	snprintf(context->failure_string, sizeof(context->failure_string),
		 "server sent failure");
	break;
    default:
	break;
    }

 done:
    return (plugin_state);
}

static const char * 
leap_failure_string(EAPClientPluginDataRef plugin)
{
    LEAPData * context = (LEAPData *)plugin->private;

    if (context->state == kLEAPStateFailure)
	return (context->failure_string);
    return (NULL);
}

static void * 
leap_session_key(EAPClientPluginDataRef plugin, int * key_length)
{
    LEAPData * context = (LEAPData *)plugin->private;

    if (context->state != kLEAPStateSuccess) {
	*key_length = 0;
	return (NULL);
    }
    *key_length = sizeof(context->session_key);
    return (context->session_key);
}

/*
 * For LEAP, the server and session keys are the same
 */
static void * 
leap_server_key(EAPClientPluginDataRef plugin, int * key_length)
{
    return (leap_session_key(plugin, key_length));
}

static int
leap_msk_copy_bytes(EAPClientPluginDataRef plugin, 
		    void * msk, int msk_size)
{
    LEAPData * 	context = (LEAPData *)plugin->private;
    int		ret_msk_size;

    if (msk_size < kEAPMasterSessionKeyMinimumSize
	|| context->state != kLEAPStateSuccess) {
	ret_msk_size = 0;
    }
    else {
	void *	offset;

	offset = msk;

	/* copy session key, pad to 32 bytes */
	bcopy(context->session_key, offset, sizeof(context->session_key));
	offset += sizeof(context->session_key);
	bzero(offset, sizeof(context->session_key));
	offset += sizeof(context->session_key);

	/* repeat: copy session key, pad to 32 bytes */
	bcopy(context->session_key, offset, sizeof(context->session_key));
	offset += sizeof(context->session_key);
	bzero(offset, sizeof(context->session_key));

	ret_msk_size = kEAPMasterSessionKeyMinimumSize;
    }
    return (ret_msk_size);
}

static EAPType 
leap_type()
{
    return (LEAP_EAP_TYPE);

}

static const char *
leap_name()
{
    return ("LEAP");

}

static EAPClientPluginVersion 
leap_version()
{
    return (kEAPClientPluginVersion);
}

static struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
    { kEAPClientPluginFuncNameVersion, leap_version },
    { kEAPClientPluginFuncNameEAPType, leap_type },
    { kEAPClientPluginFuncNameEAPName, leap_name },
    { kEAPClientPluginFuncNameInit, leap_init },
    { kEAPClientPluginFuncNameFree, leap_free },
    { kEAPClientPluginFuncNameProcess, leap_process },
    { kEAPClientPluginFuncNameFreePacket, leap_free_packet },
    { kEAPClientPluginFuncNameFailureString, leap_failure_string },
    { kEAPClientPluginFuncNameSessionKey, leap_session_key },
    { kEAPClientPluginFuncNameServerKey, leap_server_key },
    { kEAPClientPluginFuncNameMasterSessionKeyCopyBytes, leap_msk_copy_bytes },
    { kEAPClientPluginFuncNameRequireProperties, leap_require_props },
    { NULL, NULL},
};


EAPClientPluginFuncRef
leap_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;


    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}
