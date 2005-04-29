/*
 * Copyright (c) 1998-2004 Apple Computer, Inc. All rights reserved.
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

#ifndef __APPLE_3COM_3C90X_DEBUG_H
#define __APPLE_3COM_3C90X_DEBUG_H

/*
 * Set log level from 0 to 3.
 * Must be 0 on a production driver.
 */
#define DEBUG_LOG_LEVEL 0

#if (DEBUG_LOG_LEVEL > 0)
#define LOG_LEVEL_1(fmt, args...) kprintf(fmt, ## args)
#else
#define LOG_LEVEL_1(fmt, args...)
#endif

#if (DEBUG_LOG_LEVEL > 1)
#define LOG_LEVEL_2(fmt, args...) kprintf(fmt, ## args)
#else
#define LOG_LEVEL_2(fmt, args...)
#endif

#if (DEBUG_LOG_LEVEL > 2)
#define LOG_LEVEL_3(fmt, args...) kprintf(fmt, ## args)
#else
#define LOG_LEVEL_3(fmt, args...)
#endif

#define LOG_DEBUG   LOG_LEVEL_1

#define LOG_MEDIA   LOG_LEVEL_2
#define LOG_PHY     LOG_LEVEL_2

#define LOG_TX      LOG_LEVEL_3
#define LOG_INT     LOG_LEVEL_3

#endif /* !__APPLE_3COM_3C90X_DEBUG_H */
