/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CService.cpp,v 1.2 2004/09/17 20:22:01 jlehrer Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CService.cpp,v $
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
//	kprintf("IOI2CService::compareName\n");
	return(IODTCompareNubName(this, name, matched)
			|| super::compareName(name, matched));
}

