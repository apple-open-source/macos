/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

/*!
 @header featureflags.h - For functions related to feature flags used in clients/frameworks and servers/daemons
 */

#ifndef _SECURITYD_FEATUREFLAGS_H_
#define _SECURITYD_FEATUREFLAGS_H_

#include <stdbool.h>

// Helpers for SystemKeychainAlways feature flag
bool _SecSystemKeychainAlwaysIsEnabled(void);
void _SecSystemKeychainAlwaysOverride(bool value);
void _SecSystemKeychainAlwaysClearOverride(void);

#endif // _SECURITYD_FEATUREFLAGS_H_
