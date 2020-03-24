/*
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

#ifndef _IOKIT_HID_IOHIDELEMENTPRIVATE_H
#define _IOKIT_HID_IOHIDELEMENTPRIVATE_H

#include <libkern/c++/OSArray.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include "IOHIDElement.h"
#include "IOHIDDescriptorParserPrivate.h"
#include "IOHIDLibUserClient.h"

class IOHIDElementContainer;
class IOHIDEventQueue;

enum {
    kIOHIDTransactionStateIdle,
    kIOHIDTransactionStatePending,
};

enum {
    kIOHIDElementVariableSizeElement    = 0x1,
    kIOHIDElementVariableSizeReport     = 0x2
};

//===========================================================================
// An object that describes a single HID element.
    
class IOHIDElementPrivate: public IOHIDElement
{
    OSDeclareDefaultStructors( IOHIDElementPrivate )

protected:
    IOHIDElementContainer   *_owner;
    IOHIDElementType        _type;
    IOHIDElementCookie      _cookie;
    IOHIDElementPrivate    *_nextReportHandler;
    IOHIDElementValue      *_elementValue;
    void                   *_elementValueLocation;
    IOHIDElementPrivate    *_parent;
    OSArray                *_childArray;
    OSArray                *_queueArray;
    UInt32                  _flags;
    IOHIDElementCollectionType  _collectionType;

    UInt32                  _reportSize;
    UInt32                  _reportCount;
    UInt32                  _reportStartBit;
    UInt32                  _reportBits;
    UInt32                  _reportID;

    UInt32                  _usagePage;
    UInt32                  _usageMin;
    UInt32                  _usageMax;
    UInt32                  _rangeIndex;

    UInt32                  _logicalMin;
    UInt32                  _logicalMax;
    UInt32                  _physicalMin;
    UInt32                  _physicalMax;
    
    UInt32                  _unitExponent;
    UInt32                  _units;
    
    UInt32                  _transactionState;
    
    OSData                 *_dataValue;
    
    IOHIDElementPrivate    *_duplicateReportHandler;
    
    IOHIDElementPrivate    *_arrayReportHandler;
    IOHIDElementPrivate   **_rollOverElementPtr;
    OSDictionary           *_colArrayReportHandlers;
    OSArray                *_arrayItems;
    OSArray                *_duplicateElements;
    UInt32                 *_oldArraySelectors;
    
    bool                    _isInterruptReportHandler;
    
    bool                    _shouldTickleActivity;
  
    
    UInt8                   _variableSize;
    
    UInt32                  _currentReportSizeBits;
  
    struct {
        SInt32     satMin;
        SInt32     satMax;
        SInt32     dzMin;
        SInt32     dzMax;
        SInt32     min;
        SInt32     max;
        IOFixed    gran;
    } _calibration;
    
    UInt32                  _previousValue;
    
    virtual bool init( IOHIDElementContainer * owner, IOHIDElementType type );

    virtual void free(void) APPLE_KEXT_OVERRIDE;

    virtual IOHIDElementPrivate * newSubElement( UInt16 rangeIndex ) const;

    virtual bool createSubElements();
    
    virtual IOHIDElementPrivate * arrayHandlerElement(                                
                                IOHIDElementContainer *owner,
                                IOHIDElementType type,
                                IOHIDElementPrivate * child,
                                IOHIDElementPrivate * parent);

    OSDictionary*  createProperties() const;
    
    unsigned int    iteratorSize(void) const APPLE_KEXT_OVERRIDE;
    bool            initIterator(void * iterationContext) const APPLE_KEXT_OVERRIDE;
    bool            getNextObjectForIterator(void      * iterationContext,
                                             OSObject ** nextObject) const APPLE_KEXT_OVERRIDE;
    bool enqueueValue(void *value, UInt32 valueSize);
public:

    static IOHIDElementPrivate * buttonElement(
                                IOHIDElementContainer *owner,
                                IOHIDElementType type,
                                HIDButtonCapabilitiesPtr button,
                                IOHIDElementPrivate *   parent = 0 );

    static IOHIDElementPrivate * valueElement(
                                IOHIDElementContainer *owner,
                                IOHIDElementType type,
                                HIDValueCapabilitiesPtr  value,
                                IOHIDElementPrivate *   parent = 0 );
    
    static IOHIDElementPrivate * collectionElement(
                                IOHIDElementContainer *owner,
                                IOHIDElementType     type,
                                HIDCollectionExtendedNodePtr collection,
                                IOHIDElementPrivate *       parent = 0 );
    
    static IOHIDElementPrivate *nullElement(
                                IOHIDElementContainer   *owner,
                                UInt32                  reportID,
                                IOHIDElementPrivate     *parent = 0);
                                
    static IOHIDElementPrivate * reportHandlerElement(
                                IOHIDElementContainer *owner,
                                IOHIDElementType     type,
                                UInt32               reportID,
                                UInt32               reportBits );

    virtual bool serialize( OSSerialize * s ) const APPLE_KEXT_OVERRIDE;

    virtual bool fillElementStruct(IOHIDElementStruct *element);

    virtual bool addChildElement( IOHIDElementPrivate * child, bool arrayHeader = false );

    virtual bool processReport( UInt8                       reportID,
                                void *                      reportData,
                                UInt32                      reportBits,
                                const AbsoluteTime *        timestamp,
                                IOHIDElementPrivate **      next    = 0,
                                IOOptionBits                options = 0 );

    virtual bool createReport( UInt8           reportID,
                               void *        reportData, // report should be allocated outside this method
                               UInt32 *        reportLength,
                               IOHIDElementPrivate ** next );
                               
    virtual bool processArrayReport(UInt8         reportID,
                                    void *               reportData,
                                    UInt32               reportBits,
                                    const AbsoluteTime * timestamp);

    virtual bool createDuplicateReport(UInt8           reportID,
                               void *        reportData, // report should be allocated outside this method
                               UInt32 *        reportLength);
                                    
    virtual bool createArrayReport(UInt8           reportID,
                               void *        reportData, // report should be allocated outside this method
                               UInt32 *        reportLength);
    
    virtual void setArrayElementValue(UInt32 index, UInt32 value);

    virtual bool setMemoryForElementValue(
                                    IOVirtualAddress        address,
                                    void *                  location);

    virtual IOHIDElementPrivate * setNextReportHandler( IOHIDElementPrivate * element );

    virtual void setRollOverElementPtr(IOHIDElementPrivate ** rollOverElementPtr);
    virtual UInt32 getElementValueSize() const;
    virtual IOByteCount getByteSize();
    void setDataBits(OSData *data);

    virtual UInt32 getRangeCount() const;
    virtual UInt32 getStartingRangeIndex() const;

    virtual bool getReportType( IOHIDReportType * reportType ) const;

    virtual UInt32 setReportSize( UInt32 numberOfBits );
    
    inline bool shouldTickleActivity() const
    { return _shouldTickleActivity; }

    virtual bool addEventQueue( IOHIDEventQueue * queue );
    virtual bool removeEventQueue( IOHIDEventQueue * queue );
    virtual bool hasEventQueue( IOHIDEventQueue * queue );

    inline IOHIDElementPrivate * getNextReportHandler() const
    { return _nextReportHandler; }

    inline UInt32 getRangeIndex() const
    { return _rangeIndex; }
    
    inline IOHIDElementValue * getElementValue() const
    { return _elementValue;}
    
    inline void setTransactionState(UInt32 state)
    { _transactionState = state;}
    
    inline UInt32 getTransactionState() const
    { return _transactionState; }
    
    virtual void setOutOfBoundsValue();
    
    virtual bool matchProperties(OSDictionary * matching);
    
    virtual IOHIDElementCookie              getCookie(void) APPLE_KEXT_OVERRIDE;
    virtual IOHIDElementType                getType(void) APPLE_KEXT_OVERRIDE;
    virtual IOHIDElementCollectionType      getCollectionType(void) APPLE_KEXT_OVERRIDE;
    virtual OSArray *                       getChildElements(void) APPLE_KEXT_OVERRIDE;
    virtual IOHIDElement *                  getParentElement(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getUsagePage(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getUsage(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getReportID(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getReportSize(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getReportCount(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getFlags(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getLogicalMin(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getLogicalMax(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getPhysicalMin(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getPhysicalMax(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getUnit(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getUnitExponent(void) APPLE_KEXT_OVERRIDE;
    virtual AbsoluteTime                    getTimeStamp(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getValue(void) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getValue(IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual OSData *                        getDataValue(void) APPLE_KEXT_OVERRIDE;
    virtual OSData *                        getDataValue(IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual void                            setValue(UInt32 value) APPLE_KEXT_OVERRIDE;
    virtual void                            setDataValue(OSData * value) APPLE_KEXT_OVERRIDE;
    virtual bool                            conformsTo(UInt32 usagePage, UInt32 usage=0) APPLE_KEXT_OVERRIDE;
    virtual void                            setCalibration(UInt32 min=0, UInt32 max=0, UInt32 saturationMin=0, UInt32 saturationMax=0, UInt32 deadZoneMin=0, UInt32 deadZoneMax=0, IOFixed granularity=0) APPLE_KEXT_OVERRIDE;
    virtual UInt32                          getScaledValue(IOHIDValueScaleType type=kIOHIDValueScaleTypePhysical) APPLE_KEXT_OVERRIDE;
    virtual IOFixed                         getScaledFixedValue(IOHIDValueScaleType type=kIOHIDValueScaleTypePhysical) APPLE_KEXT_OVERRIDE;
    virtual IOFixed                         getScaledFixedValue(IOHIDValueScaleType type, IOOptionBits options) APPLE_KEXT_OVERRIDE;
    
    unsigned int getCount(void) const APPLE_KEXT_OVERRIDE;
    unsigned int getCapacity(void) const APPLE_KEXT_OVERRIDE;
    unsigned int getCapacityIncrement(void) const APPLE_KEXT_OVERRIDE;
    unsigned int setCapacityIncrement(unsigned increment) APPLE_KEXT_OVERRIDE;
    unsigned int ensureCapacity(unsigned int newCapacity) APPLE_KEXT_OVERRIDE;
    void flushCollection(void) APPLE_KEXT_OVERRIDE;
    virtual unsigned setOptions(unsigned   options,
                                unsigned   mask,
                                void     * context = 0) APPLE_KEXT_OVERRIDE;
    virtual OSCollection *copyCollection(OSDictionary * cycleDict = 0) APPLE_KEXT_OVERRIDE;
  
    virtual boolean_t                       isVariableSize() APPLE_KEXT_OVERRIDE
    {  return _variableSize & kIOHIDElementVariableSizeElement; }
    
    void setVariableSizeInfo            (UInt8 variableSize)
    { _variableSize = variableSize; }

    UInt8 getVariableSizeInfo           ()
    { return _variableSize;}

};

#endif /* !_IOKIT_HID_IOHIDELEMENTPRIVATE_H */
