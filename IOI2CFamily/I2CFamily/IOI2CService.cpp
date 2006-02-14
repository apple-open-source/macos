/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CService.cpp,v 1.4 2006/02/02 00:24:46 hpanther Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CService.cpp,v $
 *		Revision 1.4  2006/02/02 00:24:46  hpanther
 *		Replace flawed IOLock synchronization with semaphores.
 *		A bit of cleanup on the logging side.
 *		
 *		Revision 1.3  2005/07/01 16:09:52  bwpang
 *		[4086434] added APSL headers
 *		
 *		Revision 1.2  2004/09/17 20:22:01  jlehrer
 *		Removed ASPL headers.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#include <IOKit/IODeviceTreeSupport.h>
#include "IOI2CService.h"

#define super IOService
OSDefineMetaClassAndStructors( IOI2CService, IOService )


bool
IOI2CService::compareName(OSString *name, OSString **matched) const
{
//	DLOG("IOI2CService::compareName\n");
	return(IODTCompareNubName(this, name, matched)
			|| super::compareName(name, matched));
}

