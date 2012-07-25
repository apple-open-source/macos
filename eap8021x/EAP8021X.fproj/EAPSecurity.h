
/*
 * Copyright (c) 2002-2008 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPSECURITY_H
#define _EAP8021X_EAPSECURITY_H

/* 
 * Modification History
 *
 * April 5, 2004	Dieter Siegmund (dieter@apple)
 * - created
 */

#include <CoreFoundation/CFBase.h>
#include <Security/SecBase.h>
#if ! TARGET_OS_EMBEDDED
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>
#endif /* ! TARGET_OS_EMBEDDED */

const char *
EAPSecurityErrorString(OSStatus err);

#endif /* _EAP8021X_EAPSECURITY_H */
