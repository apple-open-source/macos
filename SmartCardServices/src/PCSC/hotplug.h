/*
 *  Copyright (c) 2000-2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  hotplug.h
 *  SmartCardServices
 */

/*
 * MUSCLE SmartCard Development ( http://www.linuxnet.com )
 *
 * Copyright (C) 2000-2003
 *  David Corcoran <corcoran@linuxnet.com>
 *
 * $Id: hotplug.h 2310 2007-01-06 21:14:56Z rousseau $
 */

/**
 * @file
 * @brief This provides a search API for hot pluggble devices.
 */

#ifndef __hotplug_h__
#define __hotplug_h__

#include "pthread.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define PCSCLITE_HP_BASE_PORT		0x200000

	LONG HPSearchHotPluggables();
 	LONG HPRegisterForHotplugEvents();
	LONG HPStopHotPluggables(void);
	void HPReCheckSerialReaders(void);
	int SendHotplugSignal(void);
	LONG HPCancelHotPluggables(void);
	LONG HPJoinHotPluggables(void);

	LONG HPRegisterForHotplugEventsT(pthread_t *wthread);

	void systemAwakeAndReadyCheck();

#ifdef __cplusplus
}
#endif

#endif
