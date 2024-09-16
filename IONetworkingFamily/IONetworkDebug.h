/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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

#ifndef _IONETWORKDEBUG_H
#define _IONETWORKDEBUG_H

extern uint32_t gIONetworkDebugFlags;

enum {
    kIONF_kprintf   = 0x01,
    kIONF_IOLog     = 0x02
};

#if DEVELOPMENT
#define DLOG(fmt, args...)                              \
        do {                                            \
            if (gIONetworkDebugFlags & kIONF_kprintf)   \
                kprintf(fmt, ## args);                  \
            if (gIONetworkDebugFlags & kIONF_IOLog)     \
                IOLog(fmt, ## args);                    \
        } while (0)
#else
#define DLOG(fmt, args...)
#endif

#define LOG(fmt, args...)  \
        do { kprintf(fmt, ## args); IOLog(fmt, ## args); } while(0)

#endif /* _IONETWORKDEBUG_H */
