/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#ifndef _IOKIT_HID_IOHIDELEMENT_H
#define _IOKIT_HID_IOHIDELEMENT_H

#include <libkern/c++/OSArray.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include "IOHIDLibUserClient.h"

class IOHIDDevice;
class IOHIDEventQueue;

enum {
    kIOHIDTransactionStateIdle,
    kIOHIDTransactionStatePending,
};

//===========================================================================
// An object that describes a single HID element.
    
class IOHIDElement: public OSObject
{
    OSDeclareDefaultStructors( IOHIDElement )

protected:
    IOHIDDevice *        _owner;
    IOHIDElementType     _type;
    IOHIDElementCookie   _cookie;
    IOHIDElement *       _nextReportHandler;
    IOHIDElementValue *  _elementValue;
    void *               _elementValueLocation;
    IOHIDElement *       _parent;
    OSArray *            _childArray;
    OSArray *            _queueArray;
    UInt32               _flags;

    UInt32               _reportSize;
    UInt32		 _reportCount;
    UInt32               _reportStartBit;
    UInt32               _reportBits;
    UInt8                _reportID;
    UInt8                _reportType;

    UInt32               _usagePage;
    UInt32               _usageMin;
    UInt32               _usageMax;
    UInt32               _rangeIndex;

    UInt32               _logicalMin;
    UInt32               _logicalMax;
    UInt32               _physicalMin;
    UInt32               _physicalMax;
    
    UInt32               _unitExponent;
    UInt32               _units;

    UInt32               _transactionState;
    
    IOHIDElement *	 _arrayReportHandler;
    OSDictionary *       _colArrayReportHandlers;
    OSArray *       	 _arrayItems;
    UInt32 *		 _oldArraySelectors;
    
    

    virtual bool init( IOHIDDevice * owner, IOHIDElementType type );

    virtual void free();

    virtual IOHIDElement * newSubElement( UInt16 rangeIndex ) const;

    virtual bool createSubElements();
    
    static IOHIDElement * arrayHandlerElement(                                
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                IOHIDElement * child,
                                IOHIDElement * parent);
        
public:
    static IOHIDElement * buttonElement(
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                HIDButtonCapabilitiesPtr button,
                                IOHIDElement *   parent = 0 );

    static IOHIDElement * valueElement(
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                HIDValueCapabilitiesPtr  value,
                                IOHIDElement *   parent = 0 );
    
    static IOHIDElement * collectionElement(
                                IOHIDDevice *        owner,
                                IOHIDElementType     type,
                                HIDCollectionNodePtr collection,
                                IOHIDElement *       parent = 0 );

    virtual bool serialize( OSSerialize * s ) const;

    virtual bool addChildElement( IOHIDElement * child );

    virtual bool processReport( UInt8                reportID,
                                void *               reportData,
                                UInt32               reportLength,
                                const AbsoluteTime * timestamp,
                                IOHIDElement **      next );

    virtual bool createReport( UInt8           reportID,
                               void *		reportData, // report should be allocated outside this method
                               UInt32 *        reportLength,
                               IOHIDElement ** next );
                               
    virtual void processArrayReport(void * reportData);
    
    virtual void createArrayReport(void * reportData);
    
    virtual void setArrayElementValue(UInt32 index, UInt32 value);

    virtual bool setMemoryForElementValue( IOVirtualAddress address,
                                           void *           location );

    virtual IOHIDElement * setNextReportHandler( IOHIDElement * element );

    virtual UInt32 getElementValueSize() const;

    virtual UInt32 getRangeCount() const;

    virtual bool getReportType( IOHIDReportType * reportType ) const;

    virtual UInt32 setReportSize( UInt32 numberOfBits );

    virtual bool addEventQueue( IOHIDEventQueue * queue );
    virtual bool removeEventQueue( IOHIDEventQueue * queue );
    virtual bool hasEventQueue( IOHIDEventQueue * queue );

    inline UInt8 getReportID() const
    { return _reportID; }

    inline IOHIDElement * getNextReportHandler() const
    { return _nextReportHandler; }

    inline IOHIDElementCookie getElementCookie() const
    { return _cookie; }

    inline IOHIDDevice * getOwner() const
    { return _owner; }

    inline OSArray * getChildArray() const
    { return _childArray; }

    inline IOHIDElementType getElementType() const
    { return _type; }

    inline UInt32 getRangeIndex() const
    { return _rangeIndex; }
    
    inline UInt32 getReportSize() const
    { return _reportSize; }
    
    inline UInt32 getReportBits() const
    { return _reportBits; }
    
    inline UInt32 getReportCount() const
    { return _reportCount; }
    
    inline UInt32 getLogicalMin() const
    { return _logicalMin; }

    inline UInt32 getLogicalMax() const
    { return _logicalMax; }

    inline UInt32 getPhysicalMin() const
    { return _physicalMin; }

    inline UInt32 getPhysicalMax() const
    { return _physicalMax; }

    inline UInt32 getUsagePage() const
    { return _usagePage; }
    
    inline UInt32 getUnits() const
    { return _units; }
    
    inline UInt32 getUnitExponent() const
    { return _unitExponent; }
    
    inline UInt32 getUsage() const
    { return (_usageMax != _usageMin) ?
                 _usageMin + _rangeIndex  :
                 _usageMin; }
                 
    inline IOHIDElementValue * getElementValue() const
    { return _elementValue;}
    
    inline void setTransactionState(UInt32 state)
    { _transactionState = state;}
    
    inline UInt32 getTransactionState() const
    { return _transactionState; }
    
    virtual void setOutOfBoundsValue();
};

#endif /* !_IOKIT_HID_IOHIDELEMENT_H */
