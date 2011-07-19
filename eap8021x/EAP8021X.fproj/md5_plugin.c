
/*
 * Copyright (c) 2001-2008 Apple Inc. All rights reserved.
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
 * December 10, 2001	Dieter Siegmund (dieter@apple.com)
 * - created
 */
 
#include <EAP8021X/EAPClientPlugin.h>
#include <EAP8021X/EAPClientProperties.h>
#include <EAP8021X/chap.h>
#include <mach/boolean.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

/*
 * Declare these here to ensure that the compiler
 * generates appropriate errors/warnings
 */
EAPClientPluginFuncIntrospect md5_introspect;
static EAPClientPluginFuncVersion md5_version;
static EAPClientPluginFuncEAPType md5_type;
static EAPClientPluginFuncEAPName md5_name;
static EAPClientPluginFuncInit md5_init;
static EAPClientPluginFuncFree md5_free;
static EAPClientPluginFuncProcess md5_process;
static EAPClientPluginFuncRequireProperties md5_require_props;
static EAPClientPluginFuncFreePacket md5_free_packet;

static EAPPacketRef
md5_request(EAPClientPluginDataRef plugin, const EAPPacketRef in_pkt_p)
{
    uint16_t			in_length = EAPPacketGetLength(in_pkt_p);
    EAPMD5ChallengePacketRef	in_md5_p = (EAPMD5ChallengePacketRef)in_pkt_p;
    EAPMD5ResponsePacketRef	out_md5_p = NULL;
    int				size;

    if (in_length < sizeof(*in_md5_p)) {
	syslog(LOG_NOTICE, "md5_request: header too short (length %d < %ld)",
	       in_length, sizeof(*in_md5_p));
	goto failed;
    }
    if (in_length < (sizeof(*in_md5_p) + in_md5_p->value_size)) {
	syslog(LOG_NOTICE, "md5_request: value too short (length %d < %ld)",
	       in_length, sizeof(*in_md5_p) + in_md5_p->value_size);
	goto failed;
    }
    size = sizeof(*out_md5_p) + plugin->username_length;
    out_md5_p = malloc(size);
    if (out_md5_p == NULL) {
	goto failed;
    }
    out_md5_p->code = kEAPCodeResponse;
    out_md5_p->identifier = in_md5_p->identifier;
    EAPPacketSetLength((EAPPacketRef)out_md5_p, size);
    out_md5_p->type = kEAPTypeMD5Challenge;
    out_md5_p->value_size = sizeof(out_md5_p->value);
    chap_md5(in_md5_p->identifier, plugin->password, plugin->password_length,
	     in_md5_p->value, in_md5_p->value_size, out_md5_p->value);
    bcopy(plugin->username, out_md5_p->name, plugin->username_length);
    return ((EAPPacketRef)out_md5_p);
 failed:
    if (out_md5_p != NULL) {
	free(out_md5_p);
    }
    return (NULL);
}

static EAPClientStatus
md5_init(EAPClientPluginDataRef plugin, 
	 CFArrayRef * required_props, 
	 EAPClientDomainSpecificError * error)
{
    *error = 0;
    *required_props = NULL;
    return (kEAPClientStatusOK);
}

static void
md5_free(EAPClientPluginDataRef plugin)
{
    /* ignore, no context data */
    return;
}

static void
md5_free_packet(EAPClientPluginDataRef plugin, EAPPacketRef arg)
{
    if (arg != NULL) {
	/* we malloc'd the packet, so free it */
	free(arg);
    }
    return;
}

static EAPClientState
md5_process(EAPClientPluginDataRef plugin, 
	    const EAPPacketRef in_pkt,
	    EAPPacketRef * out_pkt_p,
	    EAPClientStatus * client_status,
	    EAPClientDomainSpecificError * error)
{
    EAPClientState	plugin_state;

    *client_status = kEAPClientStatusOK;
    *error = 0;
    plugin_state = kEAPClientStateAuthenticating;
    *out_pkt_p = NULL;

    switch (in_pkt->code) {
    case kEAPCodeRequest:
	if (plugin->password == NULL) {
	    *client_status = kEAPClientStatusUserInputRequired;
	}
	else {
	    *out_pkt_p = md5_request(plugin, in_pkt);
	}
	break;
    case kEAPCodeSuccess:
	plugin_state = kEAPClientStateSuccess;
	break;
    case kEAPCodeFailure:
	*client_status = kEAPClientStatusFailed;
	plugin_state = kEAPClientStateFailure;
	break;
    default:
	break;
    }
    return (plugin_state);
}

static CFArrayRef
md5_require_props(EAPClientPluginDataRef plugin)
{
    CFStringRef		prop;

    if (plugin->password != NULL) {
	return (NULL);
    }
    prop = kEAPClientPropUserPassword;
    return (CFArrayCreate(NULL, (const void **)&prop, 1,
			  &kCFTypeArrayCallBacks));
}

static EAPType 
md5_type()
{
    return (kEAPTypeMD5Challenge);
}

static const char *
md5_name()
{
    return ("MD5");
}

static EAPClientPluginVersion 
md5_version()
{
    return (kEAPClientPluginVersion);
}

static struct func_table_ent {
    const char *		name;
    void *			func;
} func_table[] = {
    { kEAPClientPluginFuncNameVersion, md5_version },
    { kEAPClientPluginFuncNameEAPType, md5_type },
    { kEAPClientPluginFuncNameEAPName, md5_name },
    { kEAPClientPluginFuncNameInit, md5_init },
    { kEAPClientPluginFuncNameFree, md5_free },
    { kEAPClientPluginFuncNameProcess, md5_process },
    { kEAPClientPluginFuncNameRequireProperties, md5_require_props },
    { kEAPClientPluginFuncNameFreePacket, md5_free_packet },
    { NULL, NULL},
};


EAPClientPluginFuncRef
md5_introspect(EAPClientPluginFuncName name)
{
    struct func_table_ent * scan;


    for (scan = func_table; scan->name != NULL; scan++) {
	if (strcmp(name, scan->name) == 0) {
	    return (scan->func);
	}
    }
    return (NULL);
}
