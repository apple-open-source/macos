/*
 * Copyright (c) 2002-2009 Apple Inc. All rights reserved.
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


#ifndef _S_CONTROLLER_H
#define _S_CONTROLLER_H

#include <sys/types.h>
#include <sys/queue.h>
#include <mach/mach.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <TargetConditionals.h>

#include "eapolcontroller_types.h"

int 
ControllerCopyStateAndStatus(if_name_t if_name, 
			     int * state,
			     CFDictionaryRef * status_dict);

int 
ControllerGetState(if_name_t if_name, int * state);

int
ControllerStart(if_name_t if_name, uid_t uid, gid_t gid,
		CFDictionaryRef config_dict, mach_port_t bootstrap);

int
ControllerStartSystem(if_name_t if_name, uid_t uid, gid_t gid,
		      CFDictionaryRef options);

int
ControllerUpdate(if_name_t if_name, uid_t uid, gid_t gid,
		 CFDictionaryRef config_dict);

int
ControllerProvideUserInput(if_name_t if_name, uid_t uid, gid_t gid,
			   CFDictionaryRef user_input_dict);

int
ControllerRetry(if_name_t if_name, uid_t uid, gid_t gid);

int
ControllerStop(if_name_t if_name, uid_t uid, gid_t gid);

int
ControllerSetLogLevel(if_name_t if_name, uid_t uid, gid_t gid,
		      int32_t level);

#if ! TARGET_OS_EMBEDDED
int 
ControllerCopyLoginWindowConfiguration(if_name_t if_name, 
				       CFDictionaryRef * config_data_p);
#endif /* ! TARGET_OS_EMBEDDED */

int
ControllerClientAttach(pid_t pid, if_name_t if_name,
		       mach_port_t notify_port,
		       mach_port_t * session_port,
		       CFDictionaryRef * control_dict,
		       mach_port_t * bootstrap);

int
ControllerClientDetach(mach_port_t session_port);

int
ControllerClientGetConfig(mach_port_t session_port,
			  CFDictionaryRef * control_dict);

int
ControllerClientReportStatus(mach_port_t session_port,
			     CFDictionaryRef status_dict);

int
ControllerClientForceRenew(mach_port_t session_port);

int
ControllerClientPortDead(mach_port_t session_port);
#endif _S_CONTROLLER_H
