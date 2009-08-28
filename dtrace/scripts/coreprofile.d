/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

inline uint64_t COREPROFILE_RESUME_TRIGGER = 0x1ULL;
#pragma D binding "1.6.2" COREPROFILE_RESUME_TRIGGER
 
inline uint64_t COREPROFILE_PAUSE_TRIGGER = 0x2ULL;
#pragma D binding "1.6.2" COREPROFILE_PAUSE_TRIGGER

inline uint64_t COREPROFILE_FIRE_TRIGGER = 0x3ULL;
#pragma D binding "1.6.2" COREPROFILE_FIRE_TRIGGER

inline uint64_t COREPROFILE_SIGNPOST_POINT = 0x4ULL;
#pragma D binding "1.6.2" COREPROFILE_SIGNPOST_POINT

inline uint64_t COREPROFILE_SIGNPOST_START	= 0x5ULL;
#pragma D binding "1.6.2" COREPROFILE_SIGNPOST_START

inline uint64_t COREPROFILE_SIGNPOST_END	= 0x6ULL;
#pragma D binding "1.6.2" COREPROFILE_SIGNPOST_END

