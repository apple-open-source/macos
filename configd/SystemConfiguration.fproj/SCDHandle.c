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

#include <SystemConfiguration/SCD.h>
#include "SCDPrivate.h"


SCDHandleRef
SCDHandleInit()
{
	SCDHandlePrivateRef privateHandle = CFAllocatorAllocate(NULL, sizeof(SCDHandlePrivate), 0);

	/* set data */
	privateHandle->data = NULL;

	/* set instance */
	privateHandle->instance = 0;

	return (SCDHandleRef)privateHandle;
}


void
SCDHandleRelease(SCDHandleRef handle)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	if (privateHandle->data)
		CFRelease(privateHandle->data);

	CFAllocatorDeallocate(NULL, privateHandle);
	return;
}


int
SCDHandleGetInstance(SCDHandleRef handle)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	return privateHandle->instance;
}


void
_SCDHandleSetInstance(SCDHandleRef handle, int instance)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	privateHandle->instance = instance;
	return;
}


CFPropertyListRef
SCDHandleGetData(SCDHandleRef handle)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	if (privateHandle->data == NULL) {
		return CFSTR("SCDHandleRef not initialized.");
	}

	return privateHandle->data;
}


void
SCDHandleSetData(SCDHandleRef handle, CFPropertyListRef data)
{
	SCDHandlePrivateRef privateHandle = (SCDHandlePrivateRef)handle;

	/* remove reference to data previously associated with handle */
	if (privateHandle->data)
		CFRelease(privateHandle->data);

	/* associate new data with handle, keep a reference as needed */
	privateHandle->data = data;
	if (privateHandle->data)
		CFRetain(privateHandle->data);

	return;
}
