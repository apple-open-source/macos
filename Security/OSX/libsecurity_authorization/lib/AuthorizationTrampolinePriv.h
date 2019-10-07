/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
 *  AuthorizationTrampolinePriv.h -- Authorization defines for communication with
 *  authtrampoline.
 *  
 */

#ifndef _SECURITY_AUTHORIZATIONTRAMPOLINEPRIV_H_
#define _SECURITY_AUTHORIZATIONTRAMPOLINEPRIV_H_

#define XPC_REQUEST_CREATE_PROCESS "createp"
#define XPC_REPLY_MSG              "rpl"
#define XPC_EVENT_MSG              "evt"
#define XPC_REQUEST_ID             "req"

#define XPC_EVENT_TYPE             "evtt"
#define XPC_EVENT_TYPE_CHILDEND    "ce"

#define PARAM_TOOL_PATH            "tool"   // path to the executable
#define PARAM_TOOL_PARAMS          "params" // parameters passed to the executable
#define PARAM_ENV                  "env"    // environment
#define PARAM_CWD                  "cwd"    // current working directory
#define PARAM_EUID                 "requid" // uid under which executable should be running
#define PARAM_AUTHREF              "auth"   // authorization
#define PARAM_EXITCODE             "ec"     // exit code of that tool
#define PARAM_STDIN                "in"     // stdin FD
#define PARAM_STDOUT               "out"    // stdout FD
#define PARAM_STDERR               "err"    // stderr FD
#define PARAM_DATA                 "data"   // data to be written to the executable's stdin
#define PARAM_CHILDEND_NEEDED      "cen"    // indicates client needs to be notified when executable finishes

#define RETVAL_STATUS              "status"
#define RETVAL_CHILD_PID           "cpid"


#endif /* !_SECURITY_AUTHORIZATIONTRAMPOLINEPRIV_H_ */
