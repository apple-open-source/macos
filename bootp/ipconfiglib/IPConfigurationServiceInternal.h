/*
 * Copyright (c) 2011-2022 Apple Inc. All rights reserved.
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

/*
 * IPConfigurationServiceInternal.h
 * - internal definitions
 */

/* 
 * Modification History
 *
 * April 14, 2011 	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#ifndef _IPCONFIGURATIONSERVICEINTERNAL_H
#define _IPCONFIGURATIONSERVICEINTERNAL_H

#include <CoreFoundation/CFString.h>
#include "symbol_scope.h"
#include "IPConfigurationService.h"

#define kIPConfigurationServiceOptions	CFSTR("__IPConfigurationServiceOptions") /* dictionary */

#define _kIPConfigurationServiceOptionClearState	\
    CFSTR("ClearState")		/* boolean */
#define _kIPConfigurationServiceOptionEnableDAD	\
    CFSTR("EnableDAD")		/* boolean */
#define	_kIPConfigurationServiceOptionEnableCLAT46 \
    CFSTR("EnableCLAT46")	/* boolean */
#define _kIPConfigurationServiceOptionMonitorPID	\
    CFSTR("MonitorPID") 	/* boolean */
#define _kIPConfigurationServiceOptionMTU	\
    CFSTR("MTU")		/* number */
#define _kIPConfigurationServiceOptionNoPublish	\
    CFSTR("NoPublish")		/* boolean */
#define _kIPConfigurationServiceOptionPerformNUD	\
    CFSTR("PerformNUD")		/* boolean */
#define _kIPConfigurationServiceOptionServiceID		\
    CFSTR("ServiceID")		/* string (UUID) */
#define _kIPConfigurationServiceOptionAPNName	\
    CFSTR("APNName")		/* string */

#define _kIPConfigurationServiceOptionIPv6Entity	\
    CFSTR("IPv6Entity")	/* dictionary */
#define _kIPConfigurationServiceOptionIPv4Entity	\
    CFSTR("IPv4Entity")	/* dictionary */

#define IPCONFIGURATION_SERVICE_FORMAT CFSTR("Plugin:IPConfigurationService:%@")

INLINE CFStringRef
IPConfigurationServiceKey(CFStringRef serviceID)
{
    return (CFStringCreateWithFormat(NULL, NULL,
				     IPCONFIGURATION_SERVICE_FORMAT,
				     serviceID));
}

IPConfigurationServiceRef
IPConfigurationServiceCreateInternal(CFStringRef interface_name,
				     CFDictionaryRef options);
Boolean
IPConfigurationServiceIsValid(IPConfigurationServiceRef service);

Boolean
IPConfigurationServiceStart(IPConfigurationServiceRef service);

/*
 * Type: ObjectWrapperRef
 *
 * Purpose:
 *  Provides a level of indirection between an object
 *  (e.g. IPConfigurationServiceRef) and any other object(s) that might need
 *  to reference it (e.g. SCDynamicStoreRef).
 *
 *  For an object that is invalidated by calling CFRelease
 *  (e.g. IPConfigurationServiceRef), that means there is normally only
 *  a single reference to that object. If there's an outstanding block that
 *  was scheduled (but not run) while calling CFRelease() on that object,
 *  when the block does eventually run, it can't validly reference that
 *  that object anymore, it's been deallocated.
 *
 *  The ObjectWrapperRef is a simple, reference-counted structure that just
 *  stores an object pointer. In the *Deallocate function of the object,
 *  it synchronously calls ObjectWrapperClearObject(). When the block
 *  referencing the wrapper runs, it calls ObjectWrapperGetObject(), and if
 *  it is NULL, does not continue.
 */
typedef struct ObjectWrapper * ObjectWrapperRef;

ObjectWrapperRef
ObjectWrapperAlloc(const void * obj);

const void *
ObjectWrapperRetain(const void * info);

void
ObjectWrapperRelease(const void * info);

const void *
ObjectWrapperGetObject(ObjectWrapperRef wrapper);

void
ObjectWrapperClearObject(ObjectWrapperRef wrapper);

SCDynamicStoreRef
store_create(const void * object,
	     CFStringRef label,
	     dispatch_queue_t queue,
	     SCDynamicStoreCallBack change_callback,
	     SCDynamicStoreDisconnectCallBack disconnect_callback,
	     ObjectWrapperRef * ret_wrapper);
#endif /* _IPCONFIGURATIONSERVICEINTERNAL_H */
