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

#ifndef _SYSTEMCONFIGURATION_H
#define _SYSTEMCONFIGURATION_H

/*!
	@header SystemConfiguration.h
	The SystemConfiguration framework provides access to the data used to configure a running system.  The APIs provided by this framework communicate with the "configd" daemon.

The "configd" daemon manages a "cache" reflecting the desired configuration settings as well as the current state of the system.  The daemon provides a notification mechanism for user-level processes which need to be aware of changes made to the "cache" data.  Lastly, the daemon loads a number of bundles(or plug-ins) which monitor low-level kernel events and, via a set of policy modules, keep this cached data up to date.

The "configd" daemon also provides an address space/task/process which can be used by other CFRunLoop based functions which would otherwise require their own process/daemon for execution.

 */

/* cache access APIs */
#include <SystemConfiguration/SCD.h>
#include <SystemConfiguration/SCDKeys.h>

/* preference access APIs */
#include <SystemConfiguration/SCP.h>
#include <SystemConfiguration/SCPPath.h>
#include <SystemConfiguration/SCPreferences.h>

/* "console user" APIs */
#include <SystemConfiguration/SCDConsoleUser.h>

/* "computer/host name" APIs */
#include <SystemConfiguration/SCDHostName.h>

/* "network reachability" APIs */
#include <SystemConfiguration/SCNetwork.h>

#endif /* _SYSTEMCONFIGURATION_H */
