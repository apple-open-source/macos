/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Modification History
 *
 * July 17, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#ifndef __CFMANAGER_H
#define __CFMANAGER_H

#include <CoreFoundation/CoreFoundation.h>
#include <sys/cdefs.h>


__BEGIN_DECLS

CFArrayRef	configRead	__P((const char *path));
void		configWrite	__P((const char *path, CFArrayRef config));
void		configSet	__P((CFMutableArrayRef config, CFStringRef key, CFStringRef value));
void		configRemove	__P((CFMutableArrayRef config, CFStringRef key));

__END_DECLS


#endif	/* __CFMANAGER_H */
