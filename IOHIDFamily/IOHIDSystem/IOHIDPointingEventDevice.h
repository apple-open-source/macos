//
//  IOHIDPointingEventDevice.hpp
//  IOHIDFamily
//
//  Created by yg on 1/26/16.
//
//

#ifndef IOHIDPointingEventDevice_hpp
#define IOHIDPointingEventDevice_hpp

#include "IOHIDDeviceShim.h"
#include "IOHIPointing.h"

class IOHIDPointingEventDevice : public IOHIDDeviceShim
{
  OSDeclareDefaultStructors( IOHIDPointingEventDevice )
  
private:

  IOBufferMemoryDescriptor    *_report;
 
  static void _relativePointerEvent(
                                    IOHIDPointingEventDevice * self,
                                    int        buttons,
                                    int        dx,
                                    int        dy,
                                    AbsoluteTime ts,
                                    OSObject * sender,
                                    void *     refcon __unused);

  static void _absolutePointerEvent(
                                    IOHIDPointingEventDevice *   self,
                                    int             buttons,
                                    IOGPoint *      newLoc,
                                    IOGBounds *     bounds,
                                    bool            proximity,
                                    int             pressure,
                                    int             stylusAngle,
                                    AbsoluteTime    ts,
                                    OSObject *      sender,
                                    void *          refcon __unused);
  

  static void _scrollWheelEvent(    IOHIDPointingEventDevice * self,
                                    short   deltaAxis1,
                                    short   deltaAxis2,
                                    short   deltaAxis3,
                                    IOFixed fixedDelta1,
                                    IOFixed fixedDelta2,
                                    IOFixed fixedDelta3,
                                    SInt32  pointDeltaAxis1,
                                    SInt32  pointDeltaAxis2,
                                    SInt32  pointDeltaAxis3,
                                    UInt32  options,
                                    AbsoluteTime ts,
                                    OSObject * sender,
                                    void *     refcon __unused);

protected:

  typedef struct __attribute__((packed)) {
    UInt8   buttons;
    SInt16   x;
    SInt16   y;
    SInt16   hscroll;
    SInt16   vscroll;
  } GenericReport;

  virtual void free(void) APPLE_KEXT_OVERRIDE;
  
  virtual bool handleStart( IOService * provider ) APPLE_KEXT_OVERRIDE;

  virtual bool start( IOService * provider ) APPLE_KEXT_OVERRIDE;

public:
  static IOHIDPointingEventDevice	* newPointingDeviceAndStart(IOService * owner);
  
  virtual bool initWithLocation( UInt32 location = 0 ) APPLE_KEXT_OVERRIDE;
  
  virtual IOReturn newReportDescriptor(IOMemoryDescriptor ** descriptor ) const APPLE_KEXT_OVERRIDE;
  
  virtual OSString * newProductString(void) const APPLE_KEXT_OVERRIDE;
    
  virtual OSNumber * newVendorIDNumber(void) const APPLE_KEXT_OVERRIDE;
  virtual OSNumber * newProductIDNumber(void) const APPLE_KEXT_OVERRIDE;
  virtual OSString * newManufacturerString(void) const APPLE_KEXT_OVERRIDE;
  
  virtual IOReturn getReport( IOMemoryDescriptor * report,
                             IOHIDReportType      reportType,
                             IOOptionBits         options ) APPLE_KEXT_OVERRIDE;
  
  virtual void postMouseEvent(UInt8 buttons, SInt16 x, SInt16 y, SInt16 vscroll=0, SInt16 hscroll=0);

  virtual IOReturn message(UInt32 type, IOService * provider, void * argument) APPLE_KEXT_OVERRIDE;

  virtual bool matchPropertyTable(
                                  OSDictionary *              table,
                                  SInt32 *                    score) APPLE_KEXT_OVERRIDE;

};

#endif /* IOHIDPointingEventDevice_hpp */
