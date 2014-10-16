/*
 * Copyright (c) 2002,2008,2011 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

#ifndef _H_SA_TYPES
#define _H_SA_TYPES

#define __MigTypeCheck 1

#include <sys/types.h>
#include <Security/Authorization.h>
#include <Security/AuthorizationPlugin.h>

#ifdef __cplusplus
extern "C" {
#endif // C++

#include <mach/mach.h>

// force unmangled name
boolean_t secagentreply_server(mach_msg_header_t *, mach_msg_header_t *);

typedef u_int32_t SessionId;
typedef uint32_t MigBoolean;
typedef uint32_t SATransactionId;

typedef AuthorizationItemSet AuthorizationItemSetBlob;
typedef AuthorizationItemSet *AuthorizationItemSetPtr;
typedef AuthorizationValueVector AuthorizationValueVectorBlob;
typedef AuthorizationValueVector *AuthorizationValueVectorPtr;

typedef AuthorizationMechanismId PluginId;
typedef AuthorizationMechanismId MechanismId;

// pass structured arguments in/out of IPC calls. See "data walkers" for details
#define BLOB(copy)			copy, copy.length(), copy
#define BLOB_OUT(copy)		&copy, &copy##Length, &copy##Base
#define BLOB_DECL(type,name) type *name, *name##Base; mach_msg_type_number_t name##Length
#define BLOB_FUNC_DECL(type,name) type *name, type *name##Base, mach_msg_type_number_t name##Length

//
// Customization macros for MIG code
//
/*
#define __AfterSendRpc(id, name) \
	if (msg_result == MACH_MSG_SUCCESS && Out0P->Head.msgh_id == MACH_NOTIFY_DEAD_NAME) \
		return MIG_SERVER_DIED;

#define UseStaticTemplates 0
*/

#ifdef __cplusplus
}
#endif

#endif /* _H_SA_TYPES */

