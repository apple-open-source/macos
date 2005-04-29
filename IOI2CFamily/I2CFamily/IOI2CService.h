/*
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 *	File: $Id: IOI2CService.h,v 1.2 2004/09/17 20:22:20 jlehrer Exp $
 *
 *  DRI: Joseph Lehrer
 *
 *		$Log: IOI2CService.h,v $
 *		Revision 1.2  2004/09/17 20:22:20  jlehrer
 *		Removed ASPL headers.
 *		
 *		Revision 1.1  2004/06/07 21:53:41  jlehrer
 *		Initial Checkin
 *		
 *
 */

#ifndef _IOI2CService_H
#define _IOI2CService_H 

#include <IOKit/IOService.h>


class IOI2CService : public IOService
{
    OSDeclareDefaultStructors(IOI2CService)

public:
	bool compareName(OSString *name, OSString **matched = 0) const;

};


#endif // _IOI2CService_H
