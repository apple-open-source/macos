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
    UInt32               _reportStartBit;
    UInt32               _reportBits;
    UInt8                _reportID;
    UInt8                _reportType;

    UInt16               _usagePage;
    UInt16               _usageMin;
    UInt16               _usageMax;
    UInt16               _arrayIndex;

    UInt32               _logicalMin;
    UInt32               _logicalMax;
    UInt32               _physicalMin;
    UInt32               _physicalMax;

    virtual bool init( IOHIDDevice * owner, IOHIDElementType type );

    virtual void free();

    virtual IOHIDElement * newSubElement( UInt16 arrayIndex ) const;

    virtual bool createSubElements();

public:
    static IOHIDElement * buttonElement(
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                HIDButtonCapsPtr button,
                                IOHIDElement *   parent = 0 );

    static IOHIDElement * valueElement(
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                HIDValueCapsPtr  value,
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
                               void **         reportData,
                               UInt32 *        reportLength,
                               IOHIDElement ** next );

    virtual bool setMemoryForElementValue( IOVirtualAddress address,
                                           void *           location );

    virtual IOHIDElement * setNextReportHandler( IOHIDElement * element );

    virtual UInt32 getElementValueSize() const;

    virtual UInt32 getArrayCount() const;

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

    inline UInt32 getArrayIndex() const
    { return _arrayIndex; }
};

#endif /* !_IOKIT_HID_IOHIDELEMENT_H */
