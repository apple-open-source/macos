/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
#ifndef _IOHIDPOINTING_H
#define _IOHIDPOINTING_H

#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include "IOHIDDevice.h"

class IOHIDPointing : public IOHIPointing
{
    OSDeclareDefaultStructors(IOHIDPointing);

private:
    HIDPreparsedDataRef		_preparsedReportDescriptorData;
    Bounds			_bounds;
    IOItemCount			_numButtons;
    IOFixed     		_resolution;
    IOFixed			_scrollResolution;
    
    SInt32			_buttonCollection;
    SInt32			_xCollection;
    SInt32			_yCollection;
    SInt32			_tipPressureCollection;
    SInt32			_digitizerButtonCollection;
    SInt32			_scrollWheelCollection;
    SInt32			_horzScrollCollection;
    int				_tipPressureMin;
    SInt16			_tipPressureMax;
    
    bool			_absoluteCoordinates;
    bool			_hasInRangeReport;
    
    IOHIDDevice *		_provider;

public:
    // Allocator
    static IOHIDPointing * 	Pointing();

    virtual bool init(OSDictionary * properties = 0);

    virtual bool	start(IOService * provider);
    
//    virtual void 	stop(IOService *  provider);

  virtual void free();
  
  virtual IOReturn parseReportDescriptor( 
                    IOMemoryDescriptor * report,
                    IOOptionBits         options = 0 );
                                    
  virtual IOReturn handleReport(
                    IOMemoryDescriptor * report,
                    IOOptionBits         options = 0 );

protected:
  virtual IOItemCount buttonCount();
  virtual IOFixed     resolution();
  
private:
  // This is needed to pass properties defined
  // in IOHIDDevice to the nub layer
  void	  propagateProperties();

};

#endif /* !_IOHIDPOINTING_H */
