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
 * November 1, 2001	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _EAP8021X_EAPCLIENTMODULE_H
#define _EAP8021X_EAPCLIENTMODULE_H

#include <stdint.h>

#include <EAP8021X/EAP.h>
#include <EAP8021X/EAPClientPlugin.h>

typedef struct EAPClientModule_s EAPClientModule, *EAPClientModuleRef;

enum {
    kEAPClientModuleStatusOK = 0,
    kEAPClientModuleStatusInvalidType = 1,
    kEAPClientModuleStatusTypeAlreadyLoaded = 2,
    kEAPClientModuleStatusAllocationFailed = 3,
    kEAPClientModuleStatusPluginInvalidVersion = 4,
    kEAPClientModuleStatusPluginIncomplete = 5,
};
typedef uint32_t EAPClientModuleStatus;

EAPClientModuleRef
EAPClientModuleLookup(EAPType type);

EAPType
EAPClientModuleDefaultType(void);

EAPClientModuleStatus
EAPClientModuleAddBuiltinModule(EAPClientPluginFuncIntrospect * func);

/*
 * Function: EAPClientModulePluginIntrospect
 * Returns:
 *   Given a function name, returns the corresponding function pointer
 *   by calling the plugin's "introspect" function, if supplied.
 *   The caller needs to know the prototype for the function i.e.
 *   what arguments to pass, and the return value.
 *   A module may or may not supply its introspect function for this
 *   purpose.
 */
EAPClientPluginFuncRef
EAPClientModulePluginIntrospect(EAPClientModuleRef module,
				EAPClientPluginFuncName);


EAPType
EAPClientModulePluginEAPType(EAPClientModuleRef module);

const char *
EAPClientModulePluginEAPName(EAPClientModuleRef module);

/*
 * EAPClientModulePlugin*
 * Functions to call the individual plug-in, given an EAPClientModule.
 * Note:
 *   These check for a NULL function pointer before calling the
 *   corresponding function.
 */

EAPClientStatus
EAPClientModulePluginInit(EAPClientModuleRef module, 
			  EAPClientPluginDataRef plugin, 
			  CFArrayRef * required_props,
			  int * error);

void 
EAPClientModulePluginFree(EAPClientModuleRef module,
			  EAPClientPluginDataRef plugin);

void 
EAPClientModulePluginFreePacket(EAPClientModuleRef module,
				EAPClientPluginDataRef plugin,
				EAPPacketRef pkt_p);
EAPClientState 
EAPClientModulePluginProcess(EAPClientModuleRef module,
			     EAPClientPluginDataRef plugin,
			     const EAPPacketRef in_pkt,
			     EAPPacketRef * out_pkt_p,
			     EAPClientStatus * status,
			     EAPClientDomainSpecificError * error);

const char * 
EAPClientModulePluginFailureString(EAPClientModuleRef module,
				   EAPClientPluginDataRef plugin);

void * 
EAPClientModulePluginSessionKey(EAPClientModuleRef module,
				EAPClientPluginDataRef plugin, 
				int * key_length);

void * 
EAPClientModulePluginServerKey(EAPClientModuleRef module,
			       EAPClientPluginDataRef plugin, 
			       int * key_length);

CFArrayRef
EAPClientModulePluginRequireProperties(EAPClientModuleRef module,
				       EAPClientPluginDataRef plugin);
CFDictionaryRef
EAPClientModulePluginPublishProperties(EAPClientModuleRef module,
				       EAPClientPluginDataRef plugin);

bool
EAPClientModulePluginPacketDump(EAPClientModuleRef module,
				FILE * out_f, const EAPPacketRef packet);

CFStringRef
EAPClientModulePluginUserName(EAPClientModuleRef module,
			      CFDictionaryRef properties);
CFStringRef
EAPClientModulePluginCopyIdentity(EAPClientModuleRef module,
				  EAPClientPluginDataRef plugin);

#endif /* _EAP8021X_EAPCLIENTMODULE_H */
