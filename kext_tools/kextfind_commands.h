/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
#ifndef _KEXTFIND_COMMANDS_H_
#define _KEXTFIND_COMMANDS_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/kext/KXKextManager.h>

#include "kextfind.h"

void printKext(
    KXKextRef theKext,
    PathSpec relativePath,
    Boolean extra_info,
    char lineEnd);

void printKextProperty(
    KXKextRef theKext,
    CFStringRef propKey,
    char lineEnd);
void printKextMatchProperty(
    KXKextRef theKext,
    CFStringRef propKey,
    char lineEnd);
void printKextArches(
    KXKextRef theKext,
    char lineEnd,
    Boolean printLineEnd);

void printKextDependencies(
    KXKextRef theKext,
    PathSpec pathSpec,
    Boolean extra_info,
    char lineEnd);
void printKextDependents(
    KXKextRef theKext,
    PathSpec pathSpec,
    Boolean extra_info,
    char lineEnd);
void printKextPlugins(
    KXKextRef theKext,
    PathSpec pathSpec,
    Boolean extra_info,
    char lineEnd);

void printKextInfoDictionary(
    KXKextRef theKext,
    PathSpec pathSpec,
    char lineEnd);
void printKextExecutable(
    KXKextRef theKext,
    PathSpec pathSpec,
    char lineEnd);

const char * nameForIntegrityState(KXKextIntegrityState state);

char * getKextPath(
    KXKextRef theKext,
    PathSpec pathSpec);

char * getKextInfoDictionaryPath(
    KXKextRef theKext,
    PathSpec pathSpec);

char * getKextExecutablePath(
    KXKextRef theKext,
    PathSpec pathSpec);

#endif /* _KEXTFIND_COMMANDS_H_ */