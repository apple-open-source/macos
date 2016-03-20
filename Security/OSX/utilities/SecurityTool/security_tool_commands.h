/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


// This is included to make SECURITY_COMMAND macros result in declarations of
// commands for use in SecurityTool

#ifndef SECURITY_COMMAND
#define SECURITY_COMMAND(name, function, parameters, description) int function(int argc, char * const *argv);

#if TARGET_OS_EMBEDDED
#define SECURITY_COMMAND_IOS(name, function, parameters, description) int function(int argc, char * const *argv);
#else
#define SECURITY_COMMAND_IOS(name, function, parameters, description) extern int command_not_on_this_platform(int argc, char * const *argv);
#endif

#if !TARGET_OS_IPHONE
#define SECURITY_COMMAND_MAC(name, function, parameters, description) int function(int argc, char * const *argv);
#else
#define SECURITY_COMMAND_MAC(name, function, parameters, description) extern int command_not_on_this_platform(int argc, char * const *argv);
#endif


#endif
