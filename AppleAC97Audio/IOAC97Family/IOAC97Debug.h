/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#ifndef __IOAC97DEBUG_H
#define __IOAC97DEBUG_H

/*
 * Set log level from 0 to 2.
 */
#define AC97_DEBUG_LEVEL  0

#if (AC97_DEBUG_LEVEL >= 1)
#define AC97_DEBUG_LOG_1(fmt, args...)   kprintf(fmt, ## args)
#else
#define AC97_DEBUG_LOG_1(fmt, args...)
#endif

#if (AC97_DEBUG_LEVEL >= 2)
#define AC97_DEBUG_LOG_2(fmt, args...)   kprintf(fmt, ## args)
#else
#define AC97_DEBUG_LOG_2(fmt, args...)
#endif

#define DebugLog   AC97_DEBUG_LOG_1

/*
 * Assertion turned on for development build.
 */
#if (AC97_DEBUG_LEVEL > 0)
#include <kern/assert.h>
#define IOAC97Assert(ex)  \
    ((ex) ? (void)0 : Assert(__FILE__, __LINE__, # ex))
#else
#define IOAC97Assert(ex)
#endif

#endif /* !__IOAC97DEBUG_H */
