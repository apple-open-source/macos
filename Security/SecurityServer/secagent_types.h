/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


//
// secagent_types - type equivalence declarations for SecurityAgent MIG
//
#include "SecurityAgentClient.h"

// @@@ who forgot that one?
extern "C" kern_return_t mig_deallocate(vm_address_t addr, vm_size_t size);

namespace Security
{

using namespace SecurityAgent;


typedef char *String;
typedef const char *ConstString;
typedef Client::KeychainChoice Choice;
typedef char *Username;
typedef uint32 MigBoolean;


// fix const-blindless in MIG's internals
inline int mig_strncpy(char *dest, const char *src, int length)
{ return ::mig_strncpy(dest, const_cast<char *>(src), length); }


//
// Customization macros for MIG code
//
#define __AfterSendRpc(id, name) \
	if (msg_result == MACH_MSG_SUCCESS && Out0P->Head.msgh_id == MACH_NOTIFY_DEAD_NAME) \
		return MIG_SERVER_DIED;

#define UseStaticTemplates 0

} // end namespace Security
