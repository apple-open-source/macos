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
#ifndef _IOHIDPOINTING_H
#define _IOHIDPOINTING_H

#include <IOKit/hidsystem/IOHIDTypes.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include "IOHIDDevice.h"

enum IOHIDPointingButtonType{
    kIOHIDPointingButtonGeneric = 0,
    kIOHIDPointingButtonAbsolute,
    kIOHIDPointingButtonRelative
};
typedef enum IOHIDPointingButtonType IOHIDPointingButtonType;

class IOHIDPointing : public IOHIPointing
{
    OSDeclareDefaultStructors(IOHIDPointing);

private:
    HIDPreparsedDataRef		_preparsedReportDescriptorData;
    Bounds			_bounds;
    IOItemCount			_numButtons;
    IOFixed     		_resolution;
    IOFixed			_scrollResolution;
    
    IOHIDPointingButtonType	_buttonType;
    SInt32			_buttonCollection;
    SInt32			_xRelativeCollection;
    SInt32			_yRelativeCollection;
    SInt32			_xAbsoluteCollection;
    SInt32			_yAbsoluteCollection;
    SInt32			_tipPressureCollection;
    SInt32			_digitizerButtonCollection;
    SInt32			_scrollWheelCollection;
    SInt32			_horzScrollCollection;
    int				_tipPressureMin;
    SInt16			_tipPressureMax;
    
    bool			_absoluteCoordinates;
    bool			_hasInRangeReport;
    bool			_bootProtocol;
    
    UInt32			_reportCount;
    UInt32			_cachedButtonState;
    
    IOHIDDevice *		_provider;

public:
    // Allocator
    static IOHIDPointing * 	Pointing(OSArray * elements, IOHIDDevice * owner);

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
  bool    findDesiredElements(OSArray *elements);

};

#endif /* !_IOHIDPOINTING_H */
