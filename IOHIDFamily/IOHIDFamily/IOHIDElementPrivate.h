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
#include "IOHIDParserPriv.h"
#include "IOHIDLibUserClient.h"
#include "IOHIDDevice.h"

class IOHIDDevice;
class IOHIDEventQueue;

enum {
    kIOHIDTransactionStateIdle,
    kIOHIDTransactionStatePending,
};

//===========================================================================
// An object that describes a single HID element.
    
class IOHIDElementPrivate: public IOHIDElement
{
    OSDeclareDefaultStructors( IOHIDElementPrivate )

protected:
    IOHIDDevice            *_owner;
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
    
    virtual bool init( IOHIDDevice * owner, IOHIDElementType type );

    virtual void free();

    virtual IOHIDElementPrivate * newSubElement( UInt16 rangeIndex ) const;

    virtual bool createSubElements();
    
    virtual IOHIDElementPrivate * arrayHandlerElement(                                
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                IOHIDElementPrivate * child,
                                IOHIDElementPrivate * parent);

	OSDictionary*  createProperties() const;

	virtual IOByteCount		getByteSize();
	
	virtual void			setupResolution();

	void setDataBits(OSData *data);
    
    unsigned int    iteratorSize() const;
    bool            initIterator(void * iterationContext) const;
    bool            getNextObjectForIterator(void      * iterationContext,
                                             OSObject ** nextObject) const;
    
        
public:
    static IOHIDElementPrivate * buttonElement(
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                HIDButtonCapabilitiesPtr button,
                                IOHIDElementPrivate *   parent = 0 );

    static IOHIDElementPrivate * valueElement(
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                HIDValueCapabilitiesPtr  value,
                                IOHIDElementPrivate *   parent = 0 );
    
    static IOHIDElementPrivate * collectionElement(
                                IOHIDDevice *        owner,
                                IOHIDElementType     type,
                                HIDCollectionExtendedNodePtr collection,
                                IOHIDElementPrivate *       parent = 0 );
                                
    static IOHIDElementPrivate * reportHandlerElement(
                                IOHIDDevice *        owner,
                                IOHIDElementType     type,
                                UInt32               reportID,
                                UInt32               reportBits );

    virtual bool serialize( OSSerialize * s ) const;

    virtual bool fillElementStruct(IOHIDElementStruct *element);

    virtual bool addChildElement( IOHIDElementPrivate * child, bool arrayHeader = false );

    virtual bool processReport( UInt8                       reportID,
                                void *                      reportData,
                                UInt32                      reportBits,
                                const AbsoluteTime *        timestamp,
                                IOHIDElementPrivate **      next    = 0,
                                IOOptionBits                options = 0 );

    virtual bool createReport( UInt8           reportID,
                               void *		reportData, // report should be allocated outside this method
                               UInt32 *        reportLength,
                               IOHIDElementPrivate ** next );
                               
    virtual bool processArrayReport(UInt8		 reportID,
                                    void *               reportData,
                                    UInt32               reportBits,
                                    const AbsoluteTime * timestamp);

    virtual bool createDuplicateReport(UInt8           reportID,
                               void *		reportData, // report should be allocated outside this method
                               UInt32 *        reportLength);
                                    
    virtual bool createArrayReport(UInt8           reportID,
                               void *		reportData, // report should be allocated outside this method
                               UInt32 *        reportLength);
    
    virtual void setArrayElementValue(UInt32 index, UInt32 value);

    virtual bool setMemoryForElementValue(
                                    IOVirtualAddress        address,
                                    void *                  location);

    virtual IOHIDElementPrivate * setNextReportHandler( IOHIDElementPrivate * element );

    virtual void setRollOverElementPtr(IOHIDElementPrivate ** rollOverElementPtr);
    virtual UInt32 getElementValueSize() const;

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

    inline IOHIDDevice * getOwner() const
    { return _owner; }

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
    
    virtual IOHIDElementCookie			getCookie();
    virtual IOHIDElementType			getType();
    virtual IOHIDElementCollectionType  getCollectionType();
    virtual OSArray *					getChildElements();
	virtual IOHIDElement *				getParentElement();
    virtual UInt32						getUsagePage();
    virtual UInt32						getUsage();
	virtual UInt32						getReportID();
    virtual UInt32						getReportSize();
    virtual UInt32						getReportCount();
	virtual UInt32						getFlags();
    virtual UInt32						getLogicalMin();
    virtual UInt32						getLogicalMax();
    virtual UInt32						getPhysicalMin();
    virtual UInt32						getPhysicalMax();
    virtual UInt32						getUnit();
    virtual UInt32						getUnitExponent();
	virtual AbsoluteTime				getTimeStamp();
    virtual UInt32						getValue();
	virtual OSData *					getDataValue();
	virtual void						setValue(UInt32 value);
	virtual void						setDataValue(OSData * value);

    unsigned int getCount() const;
    unsigned int getCapacity() const;
    unsigned int getCapacityIncrement() const;
    unsigned int setCapacityIncrement(unsigned increment);
    unsigned int ensureCapacity(unsigned int newCapacity);
    void flushCollection();
    virtual unsigned setOptions(unsigned   options,
                                unsigned   mask,
                                void     * context = 0);
    virtual OSCollection *copyCollection(OSDictionary * cycleDict = 0);
};

#endif /* !_IOKIT_HID_IOHIDELEMENTPRIVATE_H */
