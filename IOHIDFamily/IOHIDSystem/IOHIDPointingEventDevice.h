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

  virtual void free();
  
  virtual bool handleStart( IOService * provider );

  virtual bool start( IOService * provider );

public:
  static IOHIDPointingEventDevice	* newPointingDeviceAndStart(IOService * owner);
  
  virtual bool initWithLocation( UInt32 location = 0 );
  
  virtual IOReturn newReportDescriptor(IOMemoryDescriptor ** descriptor ) const;
  
  virtual OSString * newProductString() const;
    
  virtual OSNumber * newVendorIDNumber() const;
  virtual OSNumber * newProductIDNumber() const;
  virtual OSString * newManufacturerString() const;
  
  virtual IOReturn getReport( IOMemoryDescriptor * report,
                             IOHIDReportType      reportType,
                             IOOptionBits         options );
  
  virtual void postMouseEvent(UInt8 buttons, SInt16 x, SInt16 y, SInt16 vscroll=0, SInt16 hscroll=0);

  virtual IOReturn message(UInt32 type, IOService * provider, void * argument);

  virtual bool matchPropertyTable(
                                  OSDictionary *              table,
                                  SInt32 *                    score);

};

#endif /* IOHIDPointingEventDevice_hpp */
