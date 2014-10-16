/*
 * Copyright (c) 2003-2004,2011,2014 Apple Inc. All Rights Reserved.
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


#ifndef _KTRACE_CODES_H_
#define _KTRACE_CODES_H_

#include <security_utilities/debugging.h>
#include <sys/kdebug.h>

/*
    we format as follows (not really done this way because bitfields are compiler dependent):
    
    struct DebugCode
    {
        int DebugClass : 8;
        int SubClass : 8;
        int SecurityAppClass : 4;
        int SecurityCodeClass : 10;
        int FunctionQualifier : 2;
    };
*/

// Define the following as macros to keep objective c happy.

// define app class constants
#define APP_DEBUG_CLASS 0x40

// define the sub class for security
#define SECURITY_SUB_CLASS 0xAA

// define the app classes used by security
#define APP_CLASS_SFAUTHORIZATION 0
#define APP_CLASS_SECURITY_AGENT 1
#define APP_CLASS_AUTHORIZATION 2
#define APP_CLASS_SECURITY_SERVER 3
#define APP_CLASS_ADHOC 4

// define function qualifiers
#define FUNCTION_START DBG_FUNC_START
#define FUNCTION_END DBG_FUNC_END
#define FUNCTION_TIMEPOINT DBG_FUNC_NONE

// define SFAuthorization code class
#define CODE_CLASS_SFAUTHORIZATION_BUTTON_PRESSED 0
#define CODE_CLASS_SFAUTHORIZATION_AUTHORIZATION 1

// define SecurityAgent code class
#define CODE_CLASS_SECURITY_AGENT_START 0
#define CODE_CLASS_SECURITY_AGENT_STARTED_BY_SECURITY_SERVER 1
#define CODE_CLASS_SECURITY_AGENT_BEFORE_MECHANISM_INVOKE 2
#define CODE_CLASS_SECURITY_AGENT_CONFIRM_ACCESS 3

// define Authorization code classes
#define CODE_CLASS_AUTHORIZATION_CREATE 0
#define CODE_CLASS_AUTHORIZATION_COPY_RIGHTS 1
#define CODE_CLASS_AUTHORIZATION_COPY_INFO 2

// define SecurityServer code classes
#define CODE_CLASS_SECURITY_SERVER_INITIALIZE 0

// define adhoc code classes (may change by need)
#define CODE_CLASS_ADHOC_FINDGENERICPASSWORD_BEGIN 0
#define CODE_CLASS_ADHOC_UCSP_CLIENT_BEGIN 1
#define CODE_CLASS_ADHOC_UCSP_SERVER_DECRYPT_BEGIN 2
#define CODE_CLASS_ADHOC_UCSP_QUERYKEYCHAINACCESS_BEGIN 3

// define SecurityServer code classes
#define TRACECODE(_debugclass, _subclass, _appclass, _codeclass, _functionqualifier) \
    ((_debugclass << 24) | (_subclass << 16) | (_appclass << 12) | (_codeclass << 2) | (_functionqualifier))

/*
 * Trace code allocations.  
 */
    enum {
        kSecTraceSFAuthorizationButtonPressedStart =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SFAUTHORIZATION, CODE_CLASS_SFAUTHORIZATION_BUTTON_PRESSED, FUNCTION_START),
    	kSecTraceSFAuthorizationAuthorizationStart =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SFAUTHORIZATION, CODE_CLASS_SFAUTHORIZATION_AUTHORIZATION, FUNCTION_START),
        kSecTraceSFAuthorizationAuthorizationEnd = 
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SFAUTHORIZATION, CODE_CLASS_SFAUTHORIZATION_AUTHORIZATION, FUNCTION_END),
    	kSecTraceSFAuthorizationButtonPressedEnd =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SFAUTHORIZATION, CODE_CLASS_SFAUTHORIZATION_BUTTON_PRESSED, FUNCTION_END),

    	kSecTraceSecurityAgentStart =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SECURITY_AGENT, CODE_CLASS_SECURITY_AGENT_START, FUNCTION_TIMEPOINT),
    	kSecTraceSecurityAgentStartedBySecurityServer =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SECURITY_AGENT, CODE_CLASS_SECURITY_AGENT_STARTED_BY_SECURITY_SERVER,
                       FUNCTION_TIMEPOINT),
        kSecTraceSecurityAgentBeforeMechanismInvoke =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SECURITY_AGENT, CODE_CLASS_SECURITY_AGENT_BEFORE_MECHANISM_INVOKE,
                       FUNCTION_TIMEPOINT),
        kSecTraceSecurityAgentConfimAccess =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SECURITY_AGENT, CODE_CLASS_SECURITY_AGENT_CONFIRM_ACCESS, FUNCTION_TIMEPOINT),

        kSecTraceAuthorizationCreateStart =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_AUTHORIZATION, CODE_CLASS_AUTHORIZATION_CREATE, FUNCTION_START),
    	kSecTraceAuthorizationCreateEnd =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_AUTHORIZATION, CODE_CLASS_AUTHORIZATION_CREATE, FUNCTION_END),
    	kSecTraceAuthorizationCopyRightsStart =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_AUTHORIZATION, CODE_CLASS_AUTHORIZATION_COPY_RIGHTS, FUNCTION_START),
    	kSecTraceAuthorizationCopyRightsEnd =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_AUTHORIZATION, CODE_CLASS_AUTHORIZATION_COPY_RIGHTS, FUNCTION_END),
    	kSecTraceAuthorizationCopyInfoStart =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_AUTHORIZATION, CODE_CLASS_AUTHORIZATION_COPY_INFO, FUNCTION_START),
    	kSecTraceAuthorizationCopyInfoEnd =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_AUTHORIZATION, CODE_CLASS_AUTHORIZATION_COPY_INFO, FUNCTION_END),

    	kSecTraceSecurityServerStart =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SECURITY_SERVER, CODE_CLASS_SECURITY_SERVER_INITIALIZE, FUNCTION_START),
    	kSecTraceSecurityServerInitialized =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_SECURITY_SERVER, CODE_CLASS_SECURITY_SERVER_INITIALIZE, FUNCTION_END),
    
    	kSecTraceSecurityFrameworkSecKeychainFindGenericPasswordBegin =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_ADHOC, CODE_CLASS_ADHOC_FINDGENERICPASSWORD_BEGIN, FUNCTION_TIMEPOINT),
    	kSecTraceUCSPClientDecryptBegin =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_ADHOC, CODE_CLASS_ADHOC_UCSP_CLIENT_BEGIN, FUNCTION_TIMEPOINT),
    	kSecTraceUCSPServerDecryptBegin =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_ADHOC, CODE_CLASS_ADHOC_UCSP_SERVER_DECRYPT_BEGIN, FUNCTION_TIMEPOINT),
    	kSecTraceSecurityServerQueryKeychainAccess =
            TRACECODE (APP_DEBUG_CLASS, SECURITY_SUB_CLASS, APP_CLASS_ADHOC, CODE_CLASS_ADHOC_UCSP_QUERYKEYCHAINACCESS_BEGIN, FUNCTION_TIMEPOINT)
    };

#endif	/* _KTRACE_CODES_H_ */
