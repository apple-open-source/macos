/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *
 *	IOCDBInterface
 .h
 *
 */
#ifndef _IOCDBINTERFACE_H
#define _IOCDBINTERFACE_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOLocks.h>
#include <libkern/c++/OSObject.h>
#include <IOKit/IOService.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOEventSource.h>
#include <IOKit/IOInterruptEventSource.h>
#include <libkern/c++/OSDictionary.h>
#include <architecture/byte_order.h>

#include <IOKit/cdb/CDBCommand.h>
#include <IOKit/cdb/IOCDBDevice.h>
#include <IOKit/cdb/IOCDBCommand.h>

#endif
