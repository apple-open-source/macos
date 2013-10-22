/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2012 Apple Computer, Inc.  All Rights Reserved.
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
#include <IOKit/system.h>
#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include "HIDMacTypes.h"

__private_extern__ void *PoolAllocateResident (vm_size_t size, unsigned char clear)
{
	void *mem = IOMalloc(size);

	if (clear) {
		bzero(mem, size);
	}

	return mem;
}

__private_extern__ OSStatus PoolDeallocate (void *ptr, vm_size_t size)
{
	IOFree(ptr, size);
	return 0;
}
