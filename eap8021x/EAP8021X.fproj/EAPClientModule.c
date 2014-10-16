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
 * November 1, 2001	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/queue.h>
#include <string.h>
#include <SystemConfiguration/SCPrivate.h>

#include "EAPClientModule.h"

/*
 * Type: EAPClientInfo
 * Purpose:
 *   A structure containing EAPClientInfo member functions.
 * Fields:
 *   type		the IANA/IEEE assigned EAP type
 */
typedef struct {
    EAPType					eap_type;
    const char *				eap_name;

    EAPClientPluginFuncIntrospect *		introspect;
    EAPClientPluginFuncVersion *		version;
    EAPClientPluginFuncInit *			init;
    EAPClientPluginFuncFree *			free;
    EAPClientPluginFuncProcess *		process;
    EAPClientPluginFuncFreePacket *		free_packet;
    EAPClientPluginFuncFailureString *		failure_string;
    EAPClientPluginFuncSessionKey *		session_key;
    EAPClientPluginFuncSessionKey *		server_key;
    EAPClientPluginFuncMasterSessionKeyCopyBytes * msk_copy_bytes;
    EAPClientPluginFuncRequireProperties *	require_properties;
    EAPClientPluginFuncPublishProperties *	publish_properties;
    EAPClientPluginFuncPacketDump *		packet_dump;
    EAPClientPluginFuncUserName *		user_name;
    EAPClientPluginFuncCopyIdentity *		copy_identity;
    EAPClientPluginFuncCopyPacketDescription *	copy_packet_description;
} EAPClientInfo, *EAPClientInfoRef;

struct EAPClientModule_s {
    TAILQ_ENTRY(EAPClientModule_s)	link;

    char *				name;
    EAPClientInfoRef			info;
};

static TAILQ_HEAD(EAPClientModuleHead, EAPClientModule_s) S_head 
     = TAILQ_HEAD_INITIALIZER(S_head);
static struct EAPClientModuleHead * S_EAPClientModuleHead_p = &S_head;

EAPClientModuleRef
EAPClientModuleLookup(EAPType type)
{
    EAPClientModuleRef	scan;

    TAILQ_FOREACH(scan, S_EAPClientModuleHead_p, link) {
	if (scan->info->eap_type == type)
	    return (scan);
    }
    return (NULL);
}

EAPType
EAPClientModulePluginEAPType(EAPClientModuleRef module)
{
    return (module->info->eap_type);
}

const char *
EAPClientModulePluginEAPName(EAPClientModuleRef module)
{
    return (module->info->eap_name);
}

EAPType
EAPClientModuleDefaultType(void)
{
    return (TAILQ_FIRST(S_EAPClientModuleHead_p)->info->eap_type);
}

static EAPClientModuleStatus
EAPClientModuleValidatePlugin(const EAPClientInfoRef info)
{
    if (info->init == NULL
	|| info->version == NULL
	|| info->free == NULL
	|| info->process == NULL
	|| info->free_packet == NULL) {
	return (kEAPClientModuleStatusPluginIncomplete);
    }
    if ((*info->version)() != kEAPClientPluginVersion) {
	return (kEAPClientModuleStatusPluginInvalidVersion);
    }
    return (kEAPClientModuleStatusOK);
}

static EAPClientModuleStatus
EAPClientModuleAdd(const EAPClientInfoRef info)
{
    EAPClientModuleRef	module;

    module = malloc(sizeof(*module));
    if (module == NULL) {
	return (kEAPClientModuleStatusAllocationFailed);
    }
    bzero(module, sizeof(*module));
    module->info = info;
    TAILQ_INSERT_TAIL(S_EAPClientModuleHead_p, module, link);
    return (kEAPClientModuleStatusOK);
}

static void
EAPClientModuleInit(void)
{
    static int first;

    if (first == TRUE) {
	return;
    }
    first = TRUE;
    return;
}

EAPClientModuleStatus
EAPClientModuleAddBuiltinModule(EAPClientPluginFuncIntrospect * func)
{
    EAPType			eap_type;
    const char *		eap_name;
    EAPClientInfoRef		info = NULL;
    EAPClientPluginFuncEAPName *name_func;
    EAPClientModuleStatus	status = kEAPClientModuleStatusOK;
    EAPClientPluginFuncEAPType *type_func;

    EAPClientModuleInit();
    type_func = (EAPClientPluginFuncEAPType *)
	(*func)(kEAPClientPluginFuncNameEAPType);
    if (type_func == NULL) {
	status = kEAPClientModuleStatusPluginIncomplete;
	goto failed;
    }
    name_func = (EAPClientPluginFuncEAPName *)
	(*func)(kEAPClientPluginFuncNameEAPName);
    if (name_func == NULL) {
	status = kEAPClientModuleStatusPluginIncomplete;
	goto failed;
    }
    eap_type = (*type_func)();
    if (eap_type <= 0 || eap_type > 255) {
	status = kEAPClientModuleStatusInvalidType;
	goto failed;
    }
    eap_name = (*name_func)();
    if (eap_name == NULL) {
	status = kEAPClientModuleStatusPluginIncomplete;
	goto failed;
    }
    if (EAPClientModuleLookup(eap_type)) {
	status = kEAPClientModuleStatusTypeAlreadyLoaded;
	goto failed;
    }
    info = malloc(sizeof(*info));
    if (info == NULL) {
	status = kEAPClientModuleStatusAllocationFailed;
	goto failed;
    }
    bzero(info, sizeof(*info));
    info->eap_type = eap_type;
    info->eap_name = eap_name;
    info->introspect = (EAPClientPluginFuncIntrospect *)
	(*func)(kEAPClientPluginFuncNameIntrospect);
    info->init = (EAPClientPluginFuncInit *)
	(*func)(kEAPClientPluginFuncNameInit);
    info->version = (EAPClientPluginFuncVersion *)
	(*func)(kEAPClientPluginFuncNameVersion);
    info->free = (EAPClientPluginFuncFree *)
	(*func)(kEAPClientPluginFuncNameFree);
    info->process = (EAPClientPluginFuncProcess *)
	(*func)(kEAPClientPluginFuncNameProcess);
    info->free_packet = (EAPClientPluginFuncFreePacket *)
	(*func)(kEAPClientPluginFuncNameFreePacket);
    info->failure_string = (EAPClientPluginFuncFailureString *)
	(*func)(kEAPClientPluginFuncNameFailureString);
    info->session_key = (EAPClientPluginFuncSessionKey *)
	(*func)(kEAPClientPluginFuncNameSessionKey);
    info->server_key = (EAPClientPluginFuncServerKey *)
	(*func)(kEAPClientPluginFuncNameServerKey);
    info->msk_copy_bytes = (EAPClientPluginFuncMasterSessionKeyCopyBytes *)
	(*func)(kEAPClientPluginFuncNameMasterSessionKeyCopyBytes);
    info->require_properties = (EAPClientPluginFuncRequireProperties *)
	(*func)(kEAPClientPluginFuncNameRequireProperties);
    info->publish_properties = (EAPClientPluginFuncPublishProperties *)
	(*func)(kEAPClientPluginFuncNamePublishProperties);
    info->packet_dump = (EAPClientPluginFuncPacketDump *)
	(*func)(kEAPClientPluginFuncNamePacketDump);
    info->user_name = (EAPClientPluginFuncUserName *)
	(*func)(kEAPClientPluginFuncNameUserName);
    info->copy_identity = (EAPClientPluginFuncCopyIdentity *)
	(*func)(kEAPClientPluginFuncNameCopyIdentity);
    info->copy_packet_description = (EAPClientPluginFuncCopyPacketDescription *)
	(*func)(kEAPClientPluginFuncNameCopyPacketDescription);
    status = EAPClientModuleValidatePlugin(info);
    if (status != kEAPClientModuleStatusOK) {
	goto failed;
    }
    status = EAPClientModuleAdd(info);
    if (status != kEAPClientModuleStatusOK) {
	goto failed;
    }
    return (status);

 failed:
    if (info != NULL) {
	free (info);
    }
    return (status);
}

/*
 * Functions to call the plug-in, given a module
 */

EAPClientPluginFuncRef
EAPClientModulePluginIntrospect(EAPClientModuleRef module,
				EAPClientPluginFuncName name)
{
    const EAPClientInfoRef	info = module->info;

    if (info->introspect == NULL) {
	return (NULL);
    }
    return ((*info->introspect)(name));
}

EAPClientStatus
EAPClientModulePluginInit(EAPClientModuleRef module, 
			  EAPClientPluginDataRef plugin, 
			  CFArrayRef * require_props, int * error)
{
    EAPClientPluginFuncInit *	init = module->info->init;

    if (init == NULL) {
	return (kEAPClientStatusFailed);
    }
    return (*init)(plugin, require_props, error);
}

void 
EAPClientModulePluginFree(EAPClientModuleRef module,
			  EAPClientPluginDataRef plugin)
{
    EAPClientPluginFuncFree *	free = module->info->free;

    if (free == NULL) {
	return;
    }
    return (*free)(plugin);
}

void 
EAPClientModulePluginFreePacket(EAPClientModuleRef module,
				EAPClientPluginDataRef plugin,
				EAPPacketRef pkt_p)
{
    EAPClientPluginFuncFreePacket *	free_packet = module->info->free_packet;

    if (free_packet == NULL) {
	return;
    }
    return (*free_packet)(plugin, pkt_p);
}

EAPClientState 
EAPClientModulePluginProcess(EAPClientModuleRef module,
			     EAPClientPluginDataRef plugin,
			     const EAPPacketRef in_pkt,
			     EAPPacketRef * out_pkt_p,
			     EAPClientStatus * status, 
			     EAPClientDomainSpecificError * error)
{
    EAPClientPluginFuncProcess *	process = module->info->process;

    if (process == NULL) {
	return (kEAPClientStateFailure);
    }
    return (*process)(plugin, in_pkt, out_pkt_p, status, error);
}

const char * 
EAPClientModulePluginFailureString(EAPClientModuleRef module,
				   EAPClientPluginDataRef plugin)
{
    EAPClientPluginFuncFailureString *	failure_string;

    failure_string = module->info->failure_string;
    if (failure_string == NULL) {
	return (NULL);
    }
    return (*failure_string)(plugin);
}


void * 
EAPClientModulePluginSessionKey(EAPClientModuleRef module,
				EAPClientPluginDataRef plugin, int * key_length)
{
    EAPClientPluginFuncSessionKey *	session_key;

    session_key = module->info->session_key;
    if (session_key == NULL) {
	return (NULL);
    }
    return (*session_key)(plugin, key_length);
}

void * 
EAPClientModulePluginServerKey(EAPClientModuleRef module,
			       EAPClientPluginDataRef plugin, int * key_length)
{
    EAPClientPluginFuncServerKey *	server_key;

    server_key = module->info->server_key;
    if (server_key == NULL) {
	return (NULL);
    }
    return (*server_key)(plugin, key_length);
}

int
EAPClientModulePluginMasterSessionKeyCopyBytes(EAPClientModuleRef module,
					       EAPClientPluginDataRef plugin, 
					       uint8_t * msk, int msk_size)
{
    EAPClientPluginFuncMasterSessionKeyCopyBytes * msk_copy_bytes;

    msk_copy_bytes = module->info->msk_copy_bytes;
    if (msk_copy_bytes == NULL) {
	return (0);
    }
    return (*msk_copy_bytes)(plugin, msk, msk_size);
}

CFArrayRef
EAPClientModulePluginRequireProperties(EAPClientModuleRef module,
				       EAPClientPluginDataRef plugin)
{
    EAPClientPluginFuncRequireProperties *	require_properties;

    require_properties = module->info->require_properties;
    if (require_properties == NULL) {
	return (NULL);
    }
    return (*require_properties)(plugin);
}

CFDictionaryRef
EAPClientModulePluginPublishProperties(EAPClientModuleRef module,
				       EAPClientPluginDataRef plugin)
{
    EAPClientPluginFuncPublishProperties *	publish_properties;

    publish_properties = module->info->publish_properties;
    if (publish_properties == NULL) {
	return (NULL);
    }
    return (*publish_properties)(plugin);
}

static bool
S_dump_packet_description(FILE * out_f, const EAPPacketRef pkt,
			  EAPClientPluginFuncCopyPacketDescription * copy_descr)
{
    bool		packet_is_valid = FALSE;
    CFStringRef		str;

    str = (*copy_descr)(pkt, &packet_is_valid);
    if (str != NULL) {
	SCPrint(TRUE, out_f, CFSTR("%@"), str);
	CFRelease(str);
    }
    return (packet_is_valid);
}


bool
EAPClientModulePluginPacketDump(EAPClientModuleRef module,
				FILE * out_f, const EAPPacketRef packet)
{
    EAPClientPluginFuncCopyPacketDescription *	copy_packet_description;
    EAPClientPluginFuncPacketDump *		packet_dump;

    packet_dump = module->info->packet_dump;
    copy_packet_description = module->info->copy_packet_description;
    if (packet_dump == NULL && copy_packet_description == NULL) {
	return (FALSE);
    }
    if (out_f == NULL || packet == NULL) {
	/* just testing for existence of packet dump routine */
	return (TRUE);
    }
    if (packet_dump != NULL) {
	return (*packet_dump)(out_f, packet);
    }
    return (S_dump_packet_description(out_f, packet, copy_packet_description));
}

CFStringRef
EAPClientModulePluginUserName(EAPClientModuleRef module,
			      CFDictionaryRef properties)
{
    EAPClientPluginFuncUserName *	user_name;

    user_name = module->info->user_name;
    if (user_name == NULL) {
	return (NULL);
    }
    return (*user_name)(properties);
}

CFStringRef
EAPClientModulePluginCopyIdentity(EAPClientModuleRef module,
				  EAPClientPluginDataRef plugin)
{
    EAPClientPluginFuncCopyIdentity *	identity;

    identity = module->info->copy_identity;
    if (identity == NULL) {
	return (NULL);
    }
    return (*identity)(plugin);
}

CFStringRef
EAPClientModulePluginCopyPacketDescription(EAPClientModuleRef module,
					   const EAPPacketRef packet,
					   bool * packet_is_valid)
{
    EAPClientPluginFuncCopyPacketDescription *	copy_packet_description;

    copy_packet_description = module->info->copy_packet_description;
    if (copy_packet_description == NULL) {
	return (NULL);
    }
    return (*copy_packet_description)(packet, packet_is_valid);
}

