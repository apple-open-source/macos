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
 * June 1, 2001			Allan Nathanson <ajn@apple.com>
 * - public API conversion
 *
 * March 24, 2000		Allan Nathanson <ajn@apple.com>
 * - initial revision
 */

#ifndef _S_CONFIGD_H
#define _S_CONFIGD_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/cdefs.h>
#include <mach/mach.h>
#include <mach/mach_error.h>

/* configd doesn't need the preference keys */
#define _SCSCHEMADEFINITIONS_H

#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCPrivate.h>
#include "SCDynamicStoreInternal.h"
#include <SystemConfiguration/SCValidation.h>
#include "config_types.h"
#include "_SCD.h"

extern Boolean		_configd_fork;		/* TRUE if process should be run in the background */
extern Boolean		_configd_verbose;	/* TRUE if verbose logging enabled */
extern CFMutableSetRef	_plugins_exclude;	/* bundle identifiers to exclude from loading */
extern CFMutableSetRef	_plugins_verbose;	/* bundle identifiers to enable verbose logging */

__BEGIN_DECLS
__END_DECLS

#endif /* !_S_CONFIGD_H */
