/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
 
 
#ifndef _CMD646ROOT_H
#define _CMD646ROOT_H

#include <IOKit/IOTypes.h>
#include <IOKit/IOService.h>

#include <IOKit/IODeviceTreeSupport.h>

/*! @class CMD646Root : public IOService
    @abstract The specific driver for cmd646-ata controllers as shipped in apple equipment.
    @serves as the place holder in the device tree for the CMD chip vs. the CMD-ATA controllers.

*/    

class CMD646Root : public IOService
{
    OSDeclareDefaultStructors(CMD646Root)

public:

	/*--- Overrides from IOService ---*/

	virtual IOService* probe( IOService* provider,	SInt32*	score );

	virtual bool start( IOService * provider );

    virtual IOService * createNub( IORegistryEntry * from );

    virtual void processNub( IOService * nub );

	void publishBelow( IORegistryEntry * root );


    virtual bool compareNubName( const IOService * nub, OSString * name,
				 OSString ** matched = 0 ) const;

    virtual IOReturn getNubResources( IOService * nub );	

};


/*! @class CMD646Device : public IOService
	@abstract The nub published by the CMD 646 chip, which represents the nub
	to which the IOATAcontroller will attach in the device tree.
*/

class CMD646Device : public IOService
{
  OSDeclareDefaultStructors(CMD646Device);
  
public:
  virtual bool compareName( OSString * name, OSString ** matched = 0 ) const;
  virtual IOService *matchLocation(IOService *client);
  virtual IOReturn getResources( void );
  virtual IOService* getRootCMD( void );
  virtual IOService* getPCINub( void );
  
};

#endif // _CMD646ROOT_H
