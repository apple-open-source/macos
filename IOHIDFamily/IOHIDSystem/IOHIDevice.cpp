/*
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
#include <IOKit/assert.h>
#include <IOKit/IOService.h>
#include <IOKit/hidsystem/IOHIDevice.h>
#include <IOKit/hidsystem/IOHIDParameter.h>

#define super IOService
OSDefineMetaClassAndStructors(IOHIDevice, IOService);

bool IOHIDevice::init(OSDictionary * properties)
{
  if (!super::init(properties))  return false;

  /*
   * Initialize minimal state.
   */

  return true;
}

void IOHIDevice::free()
{
  super::free();
}

bool IOHIDevice::open(IOService *    forClient,
                      IOOptionBits   options,
                      void *         arg)
{
    if(forClient == this) return true;
    
    return super::open(forClient, options, arg);
}


IOHIDKind IOHIDevice::hidKind()
{
  return kHIUnknownDevice;
}

UInt32 IOHIDevice::interfaceID()
{
  return 0;
}

UInt32 IOHIDevice::deviceType()
{
  return 0;
}

UInt64 IOHIDevice::getGUID()
{
  return(0xffffffffffffffffULL);
}

bool IOHIDevice::updateProperties( void )
{
    bool ok;

    ok = setProperty( kIOHIDKindKey, hidKind(), 32 )
    &    setProperty( kIOHIDInterfaceIDKey, interfaceID(), 32 )
    &    setProperty( kIOHIDSubinterfaceIDKey, deviceType(), 32 );

    return( ok );
}

IOReturn IOHIDevice::setParamProperties( OSDictionary * dict )
{
    return( kIOReturnSuccess );
}


