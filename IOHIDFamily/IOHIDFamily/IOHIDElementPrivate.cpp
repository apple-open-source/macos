/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2009 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOLib.h>
#include "IOHIDElementPrivate.h"
#include "IOHIDEventQueue.h"
#include "IOHIDParserPriv.h"
#include "IOHIDPrivateKeys.h"

#define IsRange() \
            (_usageMin != _usageMax)

#define IsArrayElement(element) \
            ((element->_flags & kHIDDataArrayBit) == kHIDDataArray)

#define IsArrayReportHandler(reportHandler) \
            (reportHandler == _arrayReportHandler)

#define IsArrayElementTheReportHandler(element) \
            (element == element->_arrayReportHandler)
            
#define IsButtonElement(element) \
            (element->_reportBits == 1)
            
#define IsDuplicateElement(element) \
            (element->_duplicateReportHandler)
            
#define IsDuplicateReportHandler(reportHandler) \
            (reportHandler == _duplicateReportHandler)
            
#define GetDuplicateElementCount(element) \
            ((element->_duplicateReportHandler) ? element->_duplicateReportHandler->_reportCount : 0)
            
#define GetArrayItemIndex(sel) \
            (sel - _logicalMin)

#define GetArrayItemSel(index) \
            (index + _logicalMin)
			
OSDefineMetaClassAndAbstractStructors(IOHIDElement, OSCollection)
OSMetaClassDefineReservedUnused(IOHIDElement,  0);
OSMetaClassDefineReservedUnused(IOHIDElement,  1);
OSMetaClassDefineReservedUnused(IOHIDElement,  2);
OSMetaClassDefineReservedUnused(IOHIDElement,  3);
OSMetaClassDefineReservedUnused(IOHIDElement,  4);
OSMetaClassDefineReservedUnused(IOHIDElement,  5);
OSMetaClassDefineReservedUnused(IOHIDElement,  6);
OSMetaClassDefineReservedUnused(IOHIDElement,  7);
OSMetaClassDefineReservedUnused(IOHIDElement,  8);
OSMetaClassDefineReservedUnused(IOHIDElement,  9);
OSMetaClassDefineReservedUnused(IOHIDElement, 10);
OSMetaClassDefineReservedUnused(IOHIDElement, 11);
OSMetaClassDefineReservedUnused(IOHIDElement, 12);
OSMetaClassDefineReservedUnused(IOHIDElement, 13);
OSMetaClassDefineReservedUnused(IOHIDElement, 14);
OSMetaClassDefineReservedUnused(IOHIDElement, 15);
OSMetaClassDefineReservedUnused(IOHIDElement, 16);
OSMetaClassDefineReservedUnused(IOHIDElement, 17);
OSMetaClassDefineReservedUnused(IOHIDElement, 18);
OSMetaClassDefineReservedUnused(IOHIDElement, 19);
OSMetaClassDefineReservedUnused(IOHIDElement, 20);
OSMetaClassDefineReservedUnused(IOHIDElement, 21);
OSMetaClassDefineReservedUnused(IOHIDElement, 22);
OSMetaClassDefineReservedUnused(IOHIDElement, 23);
OSMetaClassDefineReservedUnused(IOHIDElement, 24);
OSMetaClassDefineReservedUnused(IOHIDElement, 25);
OSMetaClassDefineReservedUnused(IOHIDElement, 26);
OSMetaClassDefineReservedUnused(IOHIDElement, 27);
OSMetaClassDefineReservedUnused(IOHIDElement, 28);
OSMetaClassDefineReservedUnused(IOHIDElement, 29);
OSMetaClassDefineReservedUnused(IOHIDElement, 30);
OSMetaClassDefineReservedUnused(IOHIDElement, 31);
	
#define super IOHIDElement
OSDefineMetaClassAndStructors( IOHIDElementPrivate, super )

//---------------------------------------------------------------------------
// 
 
bool IOHIDElementPrivate::init( IOHIDDevice * owner, IOHIDElementType type )
{
	if ( ( super::init() != true ) || ( owner == 0 ) )
    {
        return false;
    }

    _owner = owner;
    _type  = type;
    _reportSize = 0;
    _reportCount = 1;
    _duplicateReportHandler = 0;    
    _arrayReportHandler = 0;
    _colArrayReportHandlers = 0;
    _arrayItems = 0;
    _duplicateElements = 0;
    _oldArraySelectors = 0;
    _usagePage = 0;
    _usageMin = _usageMax = 0;
    _isInterruptReportHandler = 0;
    
    return true;
}

//---------------------------------------------------------------------------
// 

IOHIDElementPrivate *
IOHIDElementPrivate::buttonElement( IOHIDDevice *     owner,
                             IOHIDElementType  type,
                             HIDButtonCapabilitiesPtr  button,
                             IOHIDElementPrivate *    parent )
{
    IOHIDElementPrivate * element = new IOHIDElementPrivate;

    // Check arguments and call init().

    if ( ( button  == 0 ) ||
         ( element == 0 ) ||
         ( element->init( owner, type ) == false ) )
    {
        if ( element ) element->release();
        return 0;
    }

    // Set HID properties.

    element->_flags          = button->bitField;
    element->_reportStartBit = button->startBit;
    element->_reportID       = button->reportID;
    element->_usagePage      = button->usagePage;
    element->_rangeIndex     = 0;    
    element->_logicalMin     = element->_physicalMin = 0;
    element->_logicalMax     = element->_physicalMax = 1;

    if ( button->isRange )
    {
        element->_usageMin = button->u.range.usageMin;
        element->_usageMax = button->u.range.usageMax;        
    }
    else
    {
        element->_usageMin = button->u.notRange.usage;
        element->_usageMax = button->u.notRange.usage;
    }

    if (IsArrayElement(element))
    {
        // RY: Hack to gain the logical min/max for
        // elements.
        element->_logicalMin    = element->_physicalMin = button->u.notRange.reserved2;
        element->_logicalMax    = element->_physicalMax = button->u.notRange.reserved3;
        
        // RY: Hack to gain the report size and report
        // count for Array type elements.  This works 
        // out because array elements do not make use
        // of the unit and unit exponent.  Plus, this
        // keeps us binary compatible.  Appropriate
        // changes have been made to the HIDParser to
        // support this.
        element->_reportBits	= button->unitExponent;
        element->_reportCount	= button->units;
        
        // RY: Let's set the minimum range for keys that this keyboard supports.
        // This is needed because some keyboard describe support for 101 keys, but
        // in actuality support a far greater amount of keys.  Ex. JIS keyboard on
        // Q41B and Q16B.
        if (button->isRange &&
            ( element->_usagePage == kHIDPage_KeyboardOrKeypad ) && 
            ( element->_usageMax < (kHIDUsage_KeyboardLeftControl - 1) ))
        {
            element->_usageMax = kHIDUsage_KeyboardLeftControl - 1;
        }
    }
    else
    {
        element->_reportBits     = 1;
        element->_units          = button->units;
        element->_unitExponent   = button->unitExponent;
    }

    // Register with owner and parent, then spawn sub-elements.
    if ( ( parent && ( parent->addChildElement(element, IsArrayElement(element)) == true ) )
    &&   ( owner->registerElement( element, &element->_cookie ) == true )
    &&   ( element->createSubElements() == true ))
    {
        element->createProperties();
        return element;
    }

    element->release();
    element = 0;

    return element;
}

//---------------------------------------------------------------------------
// 

IOHIDElementPrivate *
IOHIDElementPrivate::valueElement( IOHIDDevice *     owner,
                            IOHIDElementType  type,
                            HIDValueCapabilitiesPtr   value,
                            IOHIDElementPrivate *    parent )
{
    IOHIDElementPrivate * element = new IOHIDElementPrivate;

    // Check arguments and call init().

    if ( ( value   == 0 ) ||
         ( element == 0 ) ||
         ( element->init( owner, type ) == false ) )
    {
        if ( element ) element->release();
        return 0;
    }

    // Set HID properties.

    element->_flags          = value->bitField;
    element->_reportBits     = value->bitSize;
    element->_reportCount    = value->reportCount;
    element->_reportStartBit = value->startBit;
    element->_reportID       = value->reportID;
    element->_usagePage      = value->usagePage;
    element->_logicalMin     = value->logicalMin;
    element->_logicalMax     = value->logicalMax;
    element->_physicalMin    = value->physicalMin;
    element->_physicalMax    = value->physicalMax;
    element->_units          = value->units;
    element->_unitExponent   = value->unitExponent;
    element->_rangeIndex     = 0;

    if ( value->isRange )
    {
        element->_usageMin = value->u.range.usageMin;
        element->_usageMax = value->u.range.usageMax;

        element->_reportCount = 1;
    }
    else
    {
        element->_usageMin = value->u.notRange.usage;
        element->_usageMax = value->u.notRange.usage;
		
		element->setupResolution();
    }
    
    if (element->_reportCount > 1)
    {
        element->_duplicateReportHandler = element;
        element->_duplicateElements = OSArray::withCapacity(element->_reportCount);        
        
        if (element->_duplicateElements == NULL)
            goto VALUE_ELEMENT_RELEASE;        
    }

    
    // Register with owner and parent, then spawn sub-elements.

    if ( ( owner->registerElement( element, &element->_cookie ) == true )
    &&   ( ( parent && ( parent->addChildElement(element, IsArrayElement(element)) == true ) ) ) 
    &&   ( element->createSubElements() == true ))
    {
        element->createProperties();
        return element;
    }

VALUE_ELEMENT_RELEASE:
    element->release();
    element = 0;

    return element;
}

//---------------------------------------------------------------------------
// 

IOHIDElementPrivate *
IOHIDElementPrivate::collectionElement( IOHIDDevice *         owner,
                                 IOHIDElementType      type,
                                 HIDCollectionExtendedNodePtr  collection,
                                 IOHIDElementPrivate *        parent )
{
	IOHIDElementPrivate * element = new IOHIDElementPrivate;

    // Check arguments and call init().

    if ( ( collection == 0 ) ||
         ( element    == 0 ) ||
         ( element->init( owner, type ) == false ) )
    {
        if ( element ) element->release();
        return 0;
    }

    // Set HID properties.

    element->_usagePage     = collection->collectionUsagePage;
    element->_usageMin      = collection->collectionUsage;
    element->_usageMax      = collection->collectionUsage;
    element->_collectionType = (IOHIDElementCollectionType)collection->data;
    
    element->_shouldTickleActivity = (element->_usagePage == kHIDPage_GenericDesktop);

    // Register with owner and parent.

    if ( ( owner->registerElement( element, &element->_cookie ) == false )
    ||   ( ( parent && ( parent->addChildElement(element) == false ) ) ) )
    {
        element->release();
        element = 0;
    }
    else {
        element->createProperties();
    }

    return element;
}

//---------------------------------------------------------------------------
// 

IOHIDElementPrivate * IOHIDElementPrivate::reportHandlerElement(
                                            IOHIDDevice *        owner,
                                            IOHIDElementType     type,
                                            UInt32               reportID,
                                            UInt32               reportBits )
{
    IOHIDElementPrivate * element = new IOHIDElementPrivate;

    if ( ( reportBits == 0 ) || ( element->init( owner, type ) == false ) )
    {
        element->release();
        return 0;
    }
    
    element->_isInterruptReportHandler	= true;
    element->_flags 			= kHIDDataVariable | kHIDDataRelative;
    element->_reportCount		= 1;
    element->_reportID			= reportID;
    element->_reportBits 		= element->_reportSize	= reportBits;
    
    // Register with owner.

    if ( owner->registerElement( element, &element->_cookie ) == false )
    {
        element->release();
        element = 0;
    }
    else {
        element->createProperties();
    }
    
    return element;
}


//---------------------------------------------------------------------------
// 

IOHIDElementPrivate * IOHIDElementPrivate::newSubElement( UInt16 rangeIndex ) const
{
    IOHIDElementPrivate * element = new IOHIDElementPrivate;

    // Check arguments and call init().

    if ( (element == 0 ) ||
         ( element->init( _owner, _type ) == false ) )
    {
        if ( element ) element->release();
        return 0;
    }

    // Set HID properties.

    element->_flags          		= _flags;
    element->_reportID       		= _reportID;
    element->_usagePage      		= _usagePage;
    element->_usageMin       		= _usageMin;
    element->_usageMax       		= _usageMax;
    element->_rangeIndex     		= rangeIndex;
    element->_arrayReportHandler	= _arrayReportHandler;

    element->_reportBits     		= _reportBits;
    element->_reportStartBit 		= _reportStartBit + (rangeIndex * _reportBits);
    element->_logicalMin     		= _logicalMin;
    element->_logicalMax     		= _logicalMax;
    element->_physicalMin    		= _physicalMin;
    element->_physicalMax    		= _physicalMax;
    element->_units          		= _units;
    element->_unitExponent   		= _unitExponent;

	element->setupResolution();
	
    // RY: Special handling for array elements.
    // FYI, if this an array and button element, then we
    // know that this is a dummy array element.  The start
    // is not used to process the report, but instead used
    // to identify the which arrayHandler is belongs to.
    // Therefore, all subelements should contain the same
    // start bit.
    if ( IsArrayElement(this) && IsButtonElement(this) )
    {
        element->_reportStartBit = _reportStartBit;        
    }
    
    if (_duplicateElements)
    {        
        _duplicateElements->setObject(element);
        element->_duplicateReportHandler = _duplicateReportHandler;
    }

    // Register with owner and parent.

    if ( ( _owner->registerElement( element, &element->_cookie ) == false )
    ||   ( _parent && ( _parent->addChildElement(element) == false ) ) )
    {
        element->release();
        element = 0;
    }
    else {
        element->createProperties();
    }

    return element;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::createSubElements()
{
    IOHIDElementPrivate * element;
    UInt32         count = getRangeCount();
    UInt32         index = getStartingRangeIndex();
    bool           ret = true;

    while ( index < count)
    {
        element = newSubElement( index++ );
        if ( element == 0 )
        {
            ret = false;
            break;
        }
        element->release();
    }

    return ret;
}

//---------------------------------------------------------------------------
//

void IOHIDElementPrivate::free()
{
    super::setOptions(0, kImmutable);
    
    if ( _childArray )
    {
        _childArray->release();
        _childArray = 0;
    }

    if ( _queueArray )
    {
        _queueArray->release();
        _queueArray = 0;
    }
    
    if ( _arrayItems )
    {
        _arrayItems->release();
        _arrayItems = 0;
    }
    
    if ( _duplicateElements )
    {
        _duplicateElements->release();
        _duplicateElements = 0;
    }
    
    if (_oldArraySelectors)
    {
        IOFree (_oldArraySelectors, sizeof(UInt32) * _reportCount);
        _oldArraySelectors = 0;
    }

    if (_colArrayReportHandlers)
    {
        _colArrayReportHandlers->release();
        _colArrayReportHandlers = 0;
    }
    
    OSSafeRelease(_properties);
    _properties = 0;
    
    super::free();
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::addChildElement( IOHIDElementPrivate * child, bool arrayHeader)
{

    if ( _childArray == 0 )
    {
        _childArray = OSArray::withCapacity(4);
    }

    if ( !_childArray ) 
        return false;
            
    // Perform special processing if this is an array item
    // that doesn't directly handle the report.  Basically,
    // we want to group all related array elements together.
    // This will help out for elements that are not part of 
    // a range.  Since collections can span multiple 
    // reports, we will use the following as a unique ID:
    //	    8bits: reportID
    //	   32bits: startBit
    //	   32bits: elementType
    if ( (child->_type != kIOHIDElementTypeCollection) &&
        IsArrayElement(child) &&
        !IsArrayElementTheReportHandler(child) && 
        (arrayHeader || !IsDuplicateElement(child)))
    {
        if (_colArrayReportHandlers ==0)
        {
            _colArrayReportHandlers = OSDictionary::withCapacity(1);
        }
        
        if (! _colArrayReportHandlers)
            return false;

        IOHIDElementPrivate *	arrayReportHandler;
        char		uniqueID[32];
        
        snprintf(uniqueID, sizeof(uniqueID), "%4.4x%4.4x%4.4x", (unsigned)child->_type, (unsigned)child->_reportStartBit, (unsigned)child->_reportID);
        
        arrayReportHandler = (IOHIDElementPrivate *)_colArrayReportHandlers->getObject(uniqueID);
        
        if (arrayReportHandler)
        {
            child->_arrayReportHandler = arrayReportHandler;
        }
        else
        {
            // We need to create array head based on info from
            // the child.
            arrayReportHandler = arrayHandlerElement(child->_owner, child->_type, child, this);
            
            if ( arrayReportHandler == 0 )
            {
                return false;
            }

            // Register this new element with this collection
            _colArrayReportHandlers->setObject(uniqueID, arrayReportHandler);
            arrayReportHandler->release();
        }
                
        // Now that we have the info from the child, revert
        // it back to a button.        
        child->_arrayReportHandler = arrayReportHandler;
        child->_reportBits 	   = 1;
        child->_reportCount        = 1;
        child->_logicalMin         = child->_physicalMin = 0;
        child->_logicalMax         = child->_physicalMax = 1;

        // Add the chile to the array list
        arrayReportHandler->_arrayItems->setObject(child);
    }

    _childArray->setObject( child );
    child->_parent = this;
    
    // RY: only override the child if you are not the root element
    if ( _cookie != 0 )
        child->_shouldTickleActivity = _shouldTickleActivity;

    return true;
}

IOHIDElementPrivate * IOHIDElementPrivate::arrayHandlerElement(                                
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                IOHIDElementPrivate * child,
                                IOHIDElementPrivate * parent)
{
    IOHIDElementPrivate * element = new IOHIDElementPrivate;

    // Check arguments and call init().

    if ( (element == 0 ) ||
        ( element->init( owner, type ) == false ) )
    {
        goto ARRAY_HANDLER_ELEMENT_RELEASE;
    }

    element->_arrayReportHandler = element;
    
    element->_parent         = parent;
    element->_flags          = child->_flags;
    element->_reportID       = child->_reportID;
    element->_usagePage      = child->_usagePage;
    element->_usageMin       = 0xffffffff;
    element->_usageMax       = 0xffffffff;        
    
    element->_reportBits     = child->_reportBits;
    element->_reportCount    = child->_reportCount;
    element->_reportStartBit = child->_reportStartBit;
    element->_logicalMin     = child->_logicalMin;
    element->_logicalMax     = child->_logicalMax;
    element->_physicalMin    = child->_physicalMin;
    element->_physicalMax    = child->_physicalMax;    
                
    // Allocate the array for the array elements.
    element->_arrayItems = OSArray::withCapacity((child->_usageMax - child->_usageMin) + 1);

    if (element->_arrayItems == NULL)
        goto ARRAY_HANDLER_ELEMENT_RELEASE;
        
    // RY: Allocate a buffer that will contain the old Array selector.
    // This needed to compare the old report to the new report to
    // deterine which array items need to be turned on/off.
    element->_oldArraySelectors = (UInt32 *)IOMalloc(sizeof(UInt32) * element->_reportCount);
    
    if (element->_oldArraySelectors == NULL)
        goto ARRAY_HANDLER_ELEMENT_RELEASE;

    bzero ( element->_oldArraySelectors, sizeof(UInt32) * element->_reportCount);
    
    if (element->_reportCount > 1)
    {
        element->_duplicateReportHandler = element;
        element->_duplicateElements = OSArray::withCapacity(element->_reportCount);        
        
        if (element->_duplicateElements == NULL)
            goto ARRAY_HANDLER_ELEMENT_RELEASE;        
    }
                        
    if ( (owner->registerElement( element, &element->_cookie ) == true ) &&
        ( parent && ( parent->addChildElement(element) == true )) &&
        ( element->createSubElements() == true ))
    {
        element->createProperties();
        return element;
    }
        
    
ARRAY_HANDLER_ELEMENT_RELEASE:
    element->release();
    element = 0;
    
    return element;
}

void IOHIDElementPrivate::setupResolution()
{
    IOFixed resolution = 0;

	if ((_usagePage != kHIDPage_GenericDesktop) || (getUsage() != kHIDUsage_GD_X))
		return;
    
    if ((_physicalMin == _logicalMin) || (_physicalMax == _logicalMax))
		return;

	SInt32 logicalDiff = (_logicalMax - _logicalMin);
	SInt32 physicalDiff = (_physicalMax - _physicalMin);
	
	// Since IOFixedDivide truncated fractional part and can't use floating point
	// within the kernel, have to convert equation when using negative exponents:
	// _resolution = ((logMax -logMin) * 10 **(-exp))/(physMax -physMin)

	// Even though unitExponent is stored as SInt32, The real values are only
	// a signed nibble that doesn't expand to the full 32 bits.
	SInt32 resExponent = _unitExponent & 0x0F;
	
	if (resExponent < 8)
	{
		for (int i = resExponent; i > 0; i--)
		{
			physicalDiff *=  10;
		}
	}
	else
	{
		for (int i = 0x10 - resExponent; i > 0; i--)
		{
			logicalDiff *= 10;
		}
	}
	resolution = (logicalDiff / physicalDiff) << 16;

	OSNumber *number = OSNumber::withNumber(resolution, 32);
	if (number) {
		_owner->setProperty(kIOHIDPointerResolutionKey, number);
		number->release();
	}

}

void IOHIDElementPrivate::createProperties()
{
    UInt32          usage;
    
    OSSafeRelease(_properties);
    _properties = OSDictionary::withCapacity(9);

    if (!_properties) {
        kprintf("%s - no properties\n", __PRETTY_FUNCTION__);
        goto done;
    }
    _properties->setCapacityIncrement(15);

    usage = (_usageMax != _usageMin) ? _usageMin + _rangeIndex  : _usageMin;
    
#define SET_NUMBER(Y, Z) \
    do { \
        OSNumber *number = OSNumber::withNumber(Z, 32); \
        _properties->setObject(Y, number); \
        number->release(); \
    } \
    while (false)
    
    SET_NUMBER(kIOHIDElementCookieKey, (UInt32) _cookie);
    SET_NUMBER(kIOHIDElementTypeKey, _type);
    SET_NUMBER(kIOHIDElementUsageKey, usage);
    SET_NUMBER(kIOHIDElementUsagePageKey, _usagePage);
    SET_NUMBER(kIOHIDElementReportIDKey, _reportID);
    
    if ( _type == kIOHIDElementTypeCollection ) {
        SET_NUMBER(kIOHIDElementCollectionTypeKey, _collectionType);
        goto done;
    }
    
    SET_NUMBER(kIOHIDElementSizeKey, (_reportBits * _reportCount));
    SET_NUMBER(kIOHIDElementReportSizeKey, _reportBits);
    SET_NUMBER(kIOHIDElementReportCountKey, _reportCount);
    
    if ( _isInterruptReportHandler ) {
        goto done;
    }
    
    SET_NUMBER(kIOHIDElementFlagsKey, _flags);
    SET_NUMBER(kIOHIDElementMaxKey, _logicalMax);
    SET_NUMBER(kIOHIDElementMinKey, _logicalMin);
    SET_NUMBER(kIOHIDElementScaledMaxKey, _physicalMax);
    SET_NUMBER(kIOHIDElementScaledMinKey, _physicalMin);
    SET_NUMBER(kIOHIDElementUnitKey, _units);
    SET_NUMBER(kIOHIDElementUnitExponentKey, _unitExponent);
    
    if ( IsDuplicateElement(this) && !IsDuplicateReportHandler(this)) {
        SET_NUMBER(kIOHIDElementDuplicateIndexKey, _rangeIndex);
    }

    _properties->setObject( kIOHIDElementHasNullStateKey, OSBoolean::withBoolean( _flags & kHIDDataNullState ));
    _properties->setObject( kIOHIDElementHasPreferredStateKey, OSBoolean::withBoolean( !(_flags & kHIDDataNoPreferred) ));
    _properties->setObject( kIOHIDElementIsNonLinearKey, OSBoolean::withBoolean( _flags & kHIDDataNonlinear ));
    _properties->setObject( kIOHIDElementIsRelativeKey, OSBoolean::withBoolean( _flags & kHIDDataRelative ));
    _properties->setObject( kIOHIDElementIsWrappingKey, OSBoolean::withBoolean( _flags & kHIDDataWrap ));
    _properties->setObject( kIOHIDElementIsArrayKey, OSBoolean::withBoolean( IsArrayElement(this) ));

#undef SET_NUMBER
done:
    unsigned options = setOptions(0,0);
    _properties->setOptions(options, options);
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::serialize( OSSerialize * s ) const
{
    bool            ret = true;
    
    if (!s->previouslySerialized(this)) {
        if ( !(IsDuplicateElement(this) && !IsDuplicateReportHandler(this) && (GetDuplicateElementCount(this) > 32)) ) {
            if ( _properties ) {
                if (_childArray) {
                    // we can use a shallow copy here to save space
                    OSDictionary *copy = OSDictionary::withDictionary(_properties);
                    if (copy) {
                        copy->setObject( kIOHIDElementKey, _childArray );
                        ret = copy->serialize(s);
                        copy->release();
                    }
                    else {
                        kprintf("%s - unable to copy properties\n", __PRETTY_FUNCTION__);
                        _properties->serialize(s);
                    }
                }
                else {
                    ret = _properties->serialize(s);
                }
            }
            else {
                kprintf("%s - no properties\n", __PRETTY_FUNCTION__);
                ret = false;
            }
        }
    }
    return ret;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::fillElementStruct( IOHIDElementStruct * element )
{	 
    if ( (_usageMin != _usageMax) && (_rangeIndex >= 1) )
        return false;
        
    if ( IsDuplicateElement(this) )
    {
        if ( !IsDuplicateReportHandler(this) )
            return false;
        
        IOHIDElementPrivate * dupElement;
        if (element && _duplicateElements && ( dupElement = (IOHIDElementPrivate *)_duplicateElements->getObject(0)))
        {
            element->duplicateValueSize = dupElement->getElementValueSize();
            element->duplicateIndex = 0xffffffff;
        }
    }
    
    if ( !element )
        return true;

    element->cookieMin      = (UInt32)_cookie;
    element->cookieMax      = element->cookieMin + getRangeCount() - getStartingRangeIndex();
    element->parentCookie   = _parent ? (UInt32)_parent->_cookie : 0;
    element->type           = _type;
    element->collectionType = _collectionType;
    element->flags          = _flags;
    element->usagePage      = _usagePage;
    element->usageMin       = _usageMin;
    element->usageMax       = _usageMax;
    element->min            = _logicalMin;
    element->max            = _logicalMax;
    element->scaledMin      = _physicalMin;
    element->scaledMax      = _physicalMax;
    element->size           = _reportBits * _reportCount;
    element->reportSize     = _reportBits;
    element->reportCount    = _reportCount;
    element->reportID       = _reportID;
    element->unit           = _units;
    element->unitExponent   = _unitExponent;
    element->bytes          = getByteSize();
    element->valueLocation  = (uintptr_t)_elementValueLocation;
    element->valueSize      = getElementValueSize();
    
    return true;
}

//---------------------------------------------------------------------------
// 

static inline bool CompareProperty( OSDictionary * properties, OSDictionary * matching, const char * key)
{
    // We return success if we match the key in the dictionary with the key in
    // the property table, or if the prop isn't present
    //
    OSObject 	* value;
    
    value = matching->getObject( key );

    return ( value ) ? value->isEqualTo( properties->getObject( key )) : true;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::matchProperties(OSDictionary * matching)
{
	bool			ret			= true;
	
	// Compare properties.
	do {
		if ( !matching )
			break;
			
		if ( !( _properties ) )
		{
            kprintf("%s - no properties\n", __PRETTY_FUNCTION__);
			ret = false;
			break;
		}			
        
		if (   !CompareProperty(_properties, matching, kIOHIDElementCookieKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementTypeKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementCollectionTypeKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementUsageKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementUsagePageKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementMinKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementMaxKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementScaledMaxKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementSizeKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementReportSizeKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementReportCountKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementIsArrayKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementIsRelativeKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementIsWrappingKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementIsNonLinearKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementHasPreferredStateKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementHasNullStateKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementVendorSpecificKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementUnitKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementUnitExponentKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementNameKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementValueLocationKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementDuplicateIndexKey)
			|| !CompareProperty(_properties, matching, kIOHIDElementParentCollectionKey))
		{
			ret = false;
		}		
	} while ( false );
	
	return ret;
}

//---------------------------------------------------------------------------
// 

UInt32 IOHIDElementPrivate::getElementValueSize() const
{
    UInt32  size        = sizeof(IOHIDElementValue);
    UInt32  reportWords = (_reportBits * _reportCount) / (sizeof(UInt32) * 8);
    
    // RY: Don't forget the remainder.
    reportWords += ((_reportBits * _reportCount) % (sizeof(UInt32) * 8)) ? 1 : 0;

    if ( reportWords > 1 )
    {
        size += ((reportWords - 1) * sizeof(UInt32));
    }

    return size;
}

//---------------------------------------------------------------------------
// Not very efficient, will do for now.

#define BIT_MASK(bits)  ((1 << (bits)) - 1)

#define UpdateByteOffsetAndShift(bits, offset, shift)  \
    do { offset = bits >> 3; shift = bits & 0x07; } while (0)

#define UpdateWordOffsetAndShift(bits, offset, shift)  \
    do { offset = bits >> 5; shift = bits & 0x1f; } while (0)

static void readReportBits( const UInt8 * src,
                           UInt32 *      dst,
                           UInt32        bitsToCopy,
                           UInt32        srcStartBit = 0,
                           bool          shouldSignExtend = false,
                           bool *        valueChanged = 0)
{
    UInt32 srcOffset;
    UInt32 srcShift;
    UInt32 dstShift      = 0;
    UInt32 dstStartBit   = 0;
    UInt32 dstOffset     = 0;
    UInt32 lastDstOffset = 0;
    UInt32 word          = 0;
    UInt8  bitsProcessed;
	UInt32 totalBitsProcessed = 0;

    while ( bitsToCopy )
    {
        UInt32 tmp;

        UpdateByteOffsetAndShift( srcStartBit, srcOffset, srcShift );

        bitsProcessed = min( bitsToCopy,
                             min( 8 - srcShift, 32 - dstShift ) );

        tmp = (src[srcOffset] >> srcShift) & BIT_MASK(bitsProcessed);

        word |= ( tmp << dstShift );

        dstStartBit += bitsProcessed;
        srcStartBit += bitsProcessed;
        bitsToCopy  -= bitsProcessed;
		totalBitsProcessed += bitsProcessed;

        UpdateWordOffsetAndShift( dstStartBit, dstOffset, dstShift );

        if ( ( dstOffset != lastDstOffset ) || ( bitsToCopy == 0 ) )
        {
            // sign extend negative values
			// if this is the leftmost word of the result
			if ((lastDstOffset == 0) &&
				// and the logical min or max is less than zero
				// so we should sign extend
				(shouldSignExtend))
			{
				// SInt32 temp = word;
				
				// is this less than a full word
				if ((totalBitsProcessed < 32) && 
					// and the value negative (high bit set)
					(word & (1 << (totalBitsProcessed - 1))))
					// or in all 1s above the significant bit
					word |= ~(BIT_MASK(totalBitsProcessed));
			}

			
			if ( dst[lastDstOffset] != word )
            {
                dst[lastDstOffset] = word;
				if (valueChanged) *valueChanged = true;
            }
            word = 0;
            lastDstOffset = dstOffset;
        }
    }
}

static void writeReportBits( const UInt32 * src,
                           UInt8 *        dst,
                           UInt32         bitsToCopy,
                           UInt32         dstStartBit = 0)
{
    UInt32 dstOffset;
    UInt32 dstShift;
    UInt32 srcShift    = 0;
    UInt32 srcStartBit = 0;
    UInt32 srcOffset   = 0;
    UInt8  bitsProcessed;
    UInt32 tmp;

    while ( bitsToCopy )
    {
        UpdateByteOffsetAndShift( dstStartBit, dstOffset, dstShift );

        bitsProcessed = min( bitsToCopy,
                             min( 8 - dstShift, 32 - srcShift ) );

        tmp = (src[srcOffset] >> srcShift) & BIT_MASK(bitsProcessed);

        dst[dstOffset] |= ( tmp << dstShift );

        dstStartBit += bitsProcessed;
        srcStartBit += bitsProcessed;
        bitsToCopy  -= bitsProcessed;

        UpdateWordOffsetAndShift( srcStartBit, srcOffset, srcShift );
    }
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::processReport(
                                    UInt8                       reportID,
                                    void *                      reportData,
                                    UInt32                      reportBits,
                                    const AbsoluteTime *        timestamp,
                                    IOHIDElementPrivate **      next,
                                    IOOptionBits                options)
{
    IOHIDEventQueue *   queue;
    UInt32              previousValue;
    bool				changed = false;
        
    // Set next pointer to the next report handler in the chain.
    // If this is an array, set the report handler to the one
    // the array.
    if (next)
    {
        *next = _nextReportHandler;

        if ( _reportID != reportID )
        {
            return false;
        }

        // Verify incoming report size.
        if ( _reportSize && ( reportBits < _reportSize ) )
        {
            *next = 0;
            return false;
        }
        
        if (_isInterruptReportHandler && (options & kIOHIDReportOptionNotInterrupt))
        {
            return false;
        }
        
        if (IsArrayElement(this) && !IsArrayReportHandler(this))
        {
            *next = _arrayReportHandler;
            return false;
        }
        
    }

    do {
        // Ignore report that does not match our report ID.
        if ( _reportID != reportID )
            break;
            
        // Skip reports that are too short
        if ( (_reportStartBit + (_reportBits * _reportCount)) > reportBits )
            break;
            
        if ( ( _usagePage == kHIDPage_KeyboardOrKeypad )
             && ( getUsage() >= kHIDUsage_KeyboardLeftControl )
             && ( getUsage() <= kHIDUsage_KeyboardRightGUI )
             && _rollOverElementPtr
             && *_rollOverElementPtr
             && (*_rollOverElementPtr)->getValue())
        {
            AbsoluteTime rollOverTS = (*_rollOverElementPtr)->getTimeStamp();
            if ( CMP_ABSOLUTETIME(&rollOverTS, timestamp) == 0 )
                break;
        }
        
        // The generation is incremented before and after
        // processing the report.  An odd value tells us
        // that the information is incomplete and should
        // not be trusted.  An even value tells us that
        // the value is complete.
        _elementValue->generation++;

        previousValue		= _elementValue->value[0];
		
        // Get the element value from the report.

        readReportBits( (UInt8 *) reportData,   /* source buffer      */
                       _elementValue->value,   /* destination buffer */
                       (_reportBits * _reportCount), /* bits to copy       */
                       _reportStartBit,        /* source start bit   */
                       (((SInt32)_logicalMin < 0) || ((SInt32)_logicalMax < 0)), /* should sign extend */
                       &changed );             /* did value change?  */

        // Set a timestamp to indicate the last modification time.
        // We should set the time stamp if the generation is 1 regardless if the value
        // changed.  This will insure that an initial value of 0 will have the correct
        // timestamp
        do {
            bool shouldProcess = (changed || _isInterruptReportHandler || (_flags & kHIDDataRelativeBit));
            
            if ( shouldProcess ) {
                // Let's not update the timestamp in the case where the element is relative, the value is 0, and there is no change
                if (((_flags & kHIDDataRelativeBit) == 0) || (_reportBits > 32) || changed || previousValue)
                    _elementValue->timestamp = *timestamp;
                    
                if (IsArrayElement(this) && IsArrayReportHandler(this))
                    processArrayReport(reportID, reportData, reportBits, &(_elementValue->timestamp));
            }
    
            if ( !_queueArray )
                break;
                
            for ( UInt32 i = 0; (queue = (IOHIDEventQueue *) _queueArray->getObject(i)); i++ )
            {
                if ( shouldProcess || (queue->getOptions() & kIOHIDQueueOptionsTypeEnqueueAll))
                    queue->enqueue( (void *) _elementValue, _elementValue->totalSize );
                }
        } while ( 0 );

        _elementValue->generation++;
        
        // If this element is part of a transaction
        // set its state to idle
        if (_transactionState)
            _transactionState = kIOHIDTransactionStateIdle;
    }
    while ( false );

    return changed;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::createReport( UInt8           reportID,
                                 void *		 reportData,  // this report should be alloced outisde this method.
                                 UInt32 *        reportLength,
                                 IOHIDElementPrivate ** next )
{
    bool handled = false;

    if (next)
        *next = _nextReportHandler;

    do {
        // Ignore report that does not match our report ID.
    
        if ( _reportID != reportID )
            break;

        //------------------------------------------------
        // Changed this portion of the method.
        // The report is now allocated outside of the 
        // method
         
        if ( _reportSize )
        {
            *reportLength = _reportSize / 8;            

            if ( reportData == 0 )
            {
                if (next) *next = 0;
                break;
            }

            bzero( reportData, *reportLength );
        } 
        
        //------------------------------------------------
        

        // Set next pointer to the next report handler in the chain.
        // If this is an array or duplicate, set the next to the 
        // appropriate handler.
        if (next)
        {            
            if (IsArrayElement(this))
            {
                if (!IsArrayReportHandler(this))
                {
                    *next = _arrayReportHandler;
                    break;
                }
                
                // RY: Only bother creating an array report is this element
                // is idle.
                if (_transactionState == kIOHIDTransactionStateIdle)
                    return createArrayReport(reportID, reportData, reportLength);
            }            
            else if (IsDuplicateElement(this))
            {
                if (!IsDuplicateReportHandler(this))
                {
                    *next = _duplicateReportHandler;
                    break;
                }
                
                // RY: Only bother creating a report if the duplicate report
                // elements are idle.                
                if (_transactionState == kIOHIDTransactionStateIdle)
                    return createDuplicateReport(reportID, reportData, reportLength);
            }
        }

        // If this element has not been set, an out of bounds
        // value must be set.  This will cause the device
        // to ignore the report for this element.
        if ( _transactionState == kIOHIDTransactionStateIdle )
        {
                setOutOfBoundsValue();
        }

        // Set the element value to the report.
        if ( reportData )
        {
            writeReportBits( _elementValue->value,   	/* source buffer      */
                           (UInt8 *) reportData,  	/* destination buffer */
                           (_reportBits * _reportCount),/* bits to copy       */
                           _reportStartBit);       	/* dst start bit      */                           

            handled = true;
            
            // Clear the transaction state
            _transactionState = kIOHIDTransactionStateIdle;
        }
        
    }
    while ( false );

    return handled;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::setMemoryForElementValue(
                                    IOVirtualAddress        address,
                                    void *                  location)
{
    _elementValue = (IOHIDElementValue *) address;
    _elementValueLocation = location;

    // Clear memory block, and set the invariants.

    bzero( _elementValue, getElementValueSize() );

	_elementValue->cookie    = _cookie;
	_elementValue->totalSize = getElementValueSize();

    return true;
}

//---------------------------------------------------------------------------
// Return the number of elements in a usage range.

UInt32 IOHIDElementPrivate::getRangeCount() const
{
    // FIXME - shouldn't we use logical min/max?

    // Check to see if we have multiple elements with the same usage.
    // If so, return the _reportCount
    return (_reportCount > 1) ? _reportCount : (_usageMax-_usageMin + 1 );
}

//---------------------------------------------------------------------------
// Return the number of elements in a usage range.

UInt32 IOHIDElementPrivate::getStartingRangeIndex() const
{
    // Check to see if we have multiple elements with the same usage.
    return (_reportCount > 1) ? 0 : 1;
}


//---------------------------------------------------------------------------
// 

IOHIDElementPrivate *
IOHIDElementPrivate::setNextReportHandler( IOHIDElementPrivate * element )
{
    IOHIDElementPrivate * prev = _nextReportHandler;
    _nextReportHandler  = element;
    return prev;
}


//---------------------------------------------------------------------------
// 

void IOHIDElementPrivate::setRollOverElementPtr( IOHIDElementPrivate ** elementPtr )
{
    _rollOverElementPtr = elementPtr;
}


//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::getReportType( IOHIDReportType * reportType ) const
{
    if ( _type <= kIOHIDElementTypeInput_ScanCodes )
        *reportType = kIOHIDReportTypeInput;
    else if ( _type == kIOHIDElementTypeOutput )
        *reportType = kIOHIDReportTypeOutput;
    else if ( _type == kIOHIDElementTypeFeature )
        *reportType = kIOHIDReportTypeFeature;
    else
        return false;

    return true;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::addEventQueue( IOHIDEventQueue * queue )
{
    if ( !queue )
        return false;
        
    if ( _queueArray == 0 )
    {
        _queueArray = OSArray::withCapacity(4);
    }

    if ( hasEventQueue(queue) == true )
        return false;

    queue->addElement( this );

    return _queueArray ? _queueArray->setObject( queue ) : false;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::removeEventQueue( IOHIDEventQueue * queue )
{
    OSObject * obj = 0;
    
    if ( !queue )
        return false;
        
    queue->removeElement( this );

    for ( UInt32 i = 0;
          _queueArray && (obj = _queueArray->getObject(i)); i++ )
    {
        if ( obj == (OSObject *) queue )
        {
            _queueArray->removeObject(i);
            if ( _queueArray->getCount() == 0 )
            {
                _queueArray->release();
                _queueArray = 0;
            }
            break;
        }
    }

    return (obj != 0);
}

//---------------------------------------------------------------------------
// 

bool IOHIDElementPrivate::hasEventQueue( IOHIDEventQueue * queue )
{
    OSObject * obj = 0;

    for ( UInt32 i = 0;
          _queueArray && (obj = _queueArray->getObject(i)); i++ )
    {
        if ( obj == (OSObject *) queue )
            break;
    }

    return (obj != 0);
}

//---------------------------------------------------------------------------
// 

UInt32 IOHIDElementPrivate::setReportSize( UInt32 numberOfBits )
{
    UInt32 oldSize = _reportSize;
    _reportSize = numberOfBits;
    return oldSize;
}

//---------------------------------------------------------------------------
// This methods will set an out of bounds element value.  This value will
// be based on the _logicalMin or _logicalMax depending on bit space.  If
// no room is available to go outside the range, the value will remain 
// unchanged.
void IOHIDElementPrivate::setOutOfBoundsValue()
{

    // Make sure we are not dealing with long element value type
    if ( _elementValue->totalSize == sizeof(IOHIDElementValue)) {
    
        // Simple case:  If the _logicalMin > 0, then we can just
        // set the elementValue to 0
        if ( _logicalMin > 0 ) {
            _elementValue->value[0] = 0;
        }
        
        // Other case:  _logicalMin <= 0, thus, we need to set the 
        // elementValue to _logicalMax + 1 or _logicalMin - 1.
        // This could be tricky due to bit space.
        else {
            
            if ( ( BIT_MASK(_reportBits) - _logicalMax ) > 0 ) 
                _elementValue->value[0] = _logicalMax + 1;
                
            
            else if ( ( -(BIT_MASK(_reportBits)) - _logicalMin ) < 0 ) 
                _elementValue->value[0] = _logicalMin - 1;
                                    
        }
    }
}

bool IOHIDElementPrivate::createDuplicateReport(UInt8		reportID,
                                        void *		reportData,
                                        UInt32 *	reportLength)
{
    bool		pending = false;
    IOHIDElementPrivate 	*element;

    // RY: Then, check the other duplicates to see if they are currently 
    // pending.  
    for (unsigned i=0;  _duplicateElements && i<_duplicateElements->getCount(); i++) 
    {
        if ((element = (IOHIDElementPrivate *)_duplicateElements->getObject(i)) &&
            (element->_transactionState == kIOHIDTransactionStatePending))
        {
            pending = true;
        }
        
        element->createReport(reportID, reportData, reportLength, 0);
    }
    
    return pending;
}

bool IOHIDElementPrivate::createArrayReport(UInt8	reportID,
                                    void *	reportData,
                                    UInt32 *	reportLength) 
{
    IOHIDElementPrivate 	*element, *arrayElement;
    UInt32		arraySel;
    UInt32		i, reportIndex = 0;
    
    if (createDuplicateReport(reportID, reportData, reportLength))
        return true;
                        
    for (i=0; i<_arrayItems->getCount(); i++)
    {
        element = (IOHIDElementPrivate *)(_arrayItems->getObject(i));

        if (!element)
            continue;
        if ( element->_transactionState == kIOHIDTransactionStateIdle )
            continue;
            
        if (element->_elementValue->value[0] == 0)
            continue;

        arraySel = GetArrayItemSel(i);

        if (arrayElement = (_duplicateElements) ? (IOHIDElementPrivate *)_duplicateElements->getObject(reportIndex) : this)
        {
            arrayElement->_elementValue->value[0] = arraySel;
            arrayElement->_transactionState = kIOHIDTransactionStatePending;
            arrayElement->createReport(reportID, reportData, reportLength, 0);
        }
        
        reportIndex ++;
        
        element->_transactionState = kIOHIDTransactionStateIdle;
        
        // Make sure we don't add to many usages to the report
        if (reportIndex >= _reportCount)
            break;
    }
    
    // Clear out the remaining portions of the report for this array
    arraySel = 0;
    for (i=reportIndex; i<_reportCount; i++)
    {        
        if (arrayElement = (_duplicateElements) ? (IOHIDElementPrivate *)_duplicateElements->getObject(reportIndex) : this)
        {
            arrayElement->_elementValue->value[0] = arraySel;
            arrayElement->_transactionState = kIOHIDTransactionStatePending;
            arrayElement->createReport(reportID, reportData, reportLength, 0);
        }
    }
    
    return true;
}

void IOHIDElementPrivate::setArrayElementValue(UInt32 index, UInt32 value)
{
    IOHIDElementPrivate 	*element;
    IOHIDEventQueue	*queue;
    
    if ( !_arrayItems || (index > _arrayItems->getCount()))
        return;
        
    element = (IOHIDElementPrivate *)(_arrayItems->getObject(index));
    
    if (!element) 
        return;

    // Bump the generation count.  An odd value tells us
    // that the information is incomplete and should not
    // be trusted.  An even value tells us that the value
    // is complete. 
    element->_elementValue->generation ++;
    
    element->_elementValue->value[0] = value;
    element->_elementValue->timestamp = _elementValue->timestamp;
    
    element->_elementValue->generation ++;

    if ( element->_queueArray )
    {
        for ( UInt32 i = 0;
                (queue = (IOHIDEventQueue *) element->_queueArray->getObject(i));
                i++ )
        {
            queue->enqueue( (void *) element->_elementValue,
                            element->_elementValue->totalSize );
        }
    }
        

}

bool IOHIDElementPrivate::processArrayReport(	UInt8			reportID,
                                        void *			reportData,
                                        UInt32			reportBits,
                                        const AbsoluteTime *	timestamp)
{
    IOHIDElementPrivate *	element		= NULL;
    UInt32		arraySel	= 0;
    UInt32		iNewArray	= 0;
    UInt32		iOldArray	= 0;
    bool		found		= false;
    bool		changed		= false;
        
    // RY: Process the arry selector elements.  If any of their values
    // haven't changed, don't bother with any further processing.  
    if (_duplicateElements)
    {
        bool keyboard = found = (_usagePage == kHIDPage_KeyboardOrKeypad);
        for (iNewArray = 0; iNewArray < _reportCount; iNewArray ++)
        {
            if (element = (IOHIDElementPrivate *)_duplicateElements->getObject(iNewArray))
            {
                changed |= element->processReport(reportID, reportData, reportBits, timestamp, 0);
                if (keyboard && (element->_elementValue->value[0] != kHIDUsage_KeyboardErrorRollOver))
                {
                    found = false;
                }
            }
        }
        
        if (!changed)
            return changed;
        else if (keyboard)
        {
            setArrayElementValue(GetArrayItemIndex(kHIDUsage_KeyboardErrorRollOver), (found ? 1 : 0));
            
            if (found)
                return false;
        }
    }
                                    
    // Check the existing indexes against the originals
    for (iOldArray = 0; iOldArray < _reportCount; iOldArray ++)
    {
        arraySel = _oldArraySelectors[iOldArray];
        
        found = false;

        for (iNewArray = 0; iNewArray < _reportCount; iNewArray ++)
        {
            element = (_duplicateElements) ? (IOHIDElementPrivate *)_duplicateElements->getObject(iNewArray) : this;
            if (element && (arraySel == element->_elementValue->value[0]))
            {
                found = true;
                break;
            }
        }
        
        // The index is no longer present.  Set its value to 0.
        if (!found)
            setArrayElementValue(GetArrayItemIndex(arraySel), 0);
    }
    
    // Now add new indexes to _oldArraySelectors
    for (iNewArray = 0; iNewArray < _reportCount; iNewArray ++)
    {
        if (!(element = (_duplicateElements) ? (IOHIDElementPrivate *)_duplicateElements->getObject(iNewArray) : this))
            continue;
            
        arraySel = element->_elementValue->value[0];
                
        found = false;

        for (iOldArray = 0; iOldArray < _reportCount; iOldArray ++)
        {
            if (arraySel == _oldArraySelectors[iOldArray])
            {
                found = true;
                break;
            }
        }
        
        // This is a new index.  Set its value to 1.
        if (!found)
            setArrayElementValue(GetArrayItemIndex(arraySel), 1);
    }
            
    // save the new array to _oldArraySelectors for future reference
    for (iOldArray = 0; iOldArray < _reportCount; iOldArray ++)
    {
        if (element = (_duplicateElements) ? (IOHIDElementPrivate *)_duplicateElements->getObject(iOldArray) : this)
        _oldArraySelectors[iOldArray] = element->_elementValue->value[0];
    }

    return changed;
}

IOHIDElementCookie IOHIDElementPrivate::getCookie()
{   return _cookie;   }

IOHIDElementType IOHIDElementPrivate::getType()
{   return _type;   }

IOHIDElementCollectionType IOHIDElementPrivate::getCollectionType()
{   return _collectionType;   }

OSArray * IOHIDElementPrivate::getChildElements()
{   return _childArray;   }

IOHIDElement * IOHIDElementPrivate::getParentElement()
{   return _parent;   }

UInt32 IOHIDElementPrivate::getUsagePage()
{   return _usagePage;   }

UInt32 IOHIDElementPrivate::getUsage()
{
	return (_usageMax != _usageMin) ? _usageMin + _rangeIndex  : _usageMin;
}

UInt32 IOHIDElementPrivate::getReportID()
{   return _reportID;   }

UInt32 IOHIDElementPrivate::getReportSize()
{   return _reportBits;   }

UInt32 IOHIDElementPrivate::getReportCount()
{   return _reportCount;   }

UInt32 IOHIDElementPrivate::getFlags()
{   return _flags;  }

UInt32 IOHIDElementPrivate::getLogicalMin()
{   return _logicalMin;   }

UInt32 IOHIDElementPrivate::getLogicalMax()
{   return _logicalMax;   }

UInt32 IOHIDElementPrivate::getPhysicalMin()
{   return _physicalMin;   }

UInt32 IOHIDElementPrivate::getPhysicalMax()
{   return _physicalMax;   }

UInt32 IOHIDElementPrivate::getUnit()
{   return _units;   }

UInt32 IOHIDElementPrivate::getUnitExponent()
{   return _unitExponent;   }

UInt32 IOHIDElementPrivate::getValue()
{   
	return ((_reportBits * _reportCount) < 32) ? _elementValue->value[0] : 0;
}

OSData * IOHIDElementPrivate::getDataValue()
{   
	UInt32  bitsToCopy	= (_reportBits * _reportCount);
	
	if ( !_dataValue)
	{
		_dataValue = OSData::withCapacity(getByteSize());
	}

	writeReportBits((const UInt32*)_elementValue->value, (UInt8 *)_dataValue->getBytesNoCopy(), bitsToCopy);
	
	return _dataValue;
}

void IOHIDElementPrivate::setValue(UInt32 value)
{ 
	UInt32  previousValue = _elementValue->value[0];
	
	if (previousValue == value)
		return;
		
	_elementValue->value[0] = value;
	
	if (_owner->postElementValues(&_cookie, 1) != kIOReturnSuccess)
		_elementValue->value[0] = previousValue;
}

void IOHIDElementPrivate::setDataValue(OSData * value)
{
	OSData * previousValue;
	
	
	if ( !value ) return;
	
	previousValue = getDataValue();
	
	setDataBits(value);
	
	if (_owner->postElementValues(&_cookie, 1) != kIOReturnSuccess)
		setDataBits(previousValue);
}

void IOHIDElementPrivate::setDataBits(OSData *value)
{
	UInt32  bitsToCopy;

	if ( !value ) return;

	bitsToCopy = min ( (value->getLength() << 3), (_reportBits * _reportCount) );
	
	readReportBits((const UInt8*)value->getBytesNoCopy(), _elementValue->value, bitsToCopy);

}

AbsoluteTime IOHIDElementPrivate::getTimeStamp()
{
	return _elementValue->timestamp;
}

IOByteCount IOHIDElementPrivate::getByteSize()
{
	IOByteCount byteSize;
	UInt32		bitCount = (_reportBits * _reportCount);
	
	byteSize = bitCount >> 3;
	byteSize += (bitCount % 8) ? 1 : 0;
	
	return byteSize;
}

unsigned int IOHIDElementPrivate::iteratorSize() const
{
    return 0;
}

bool IOHIDElementPrivate::initIterator(void * iterationContext) const
{
    return false;
}

bool IOHIDElementPrivate::getNextObjectForIterator(void      * iterationContext,
                                                   OSObject ** nextObject) const
{
    *nextObject = NULL;
    return 0;
}

unsigned int IOHIDElementPrivate::getCount() const
{
    return 1;
}

unsigned int IOHIDElementPrivate::getCapacity() const
{
    return 1;
}

unsigned int IOHIDElementPrivate::getCapacityIncrement() const
{
    return 0;
}

unsigned int IOHIDElementPrivate::setCapacityIncrement(unsigned increment __unused)
{
    return 0;
}

unsigned int IOHIDElementPrivate::ensureCapacity(unsigned int newCapacity __unused)
{
    return 0;
}

void IOHIDElementPrivate::flushCollection()
{
}

unsigned IOHIDElementPrivate::setOptions(unsigned options,
                                         unsigned mask,
                                         void * context __unused)
{
    unsigned old = super::setOptions(options, mask);
    if ((old ^ options) & mask) {
        // Value changed need to set all of the child collections
        if (_childArray)
            _childArray->setOptions(options, mask);
        if (_properties)
            _properties->setOptions(options, mask);
    }
    return old;
}

OSCollection * IOHIDElementPrivate::copyCollection(OSDictionary * cycleDict)
{
    OSCollection *ret = NULL;
    if (_properties) {
        // make sure to force deep copy
        ret = _properties->copyCollection(cycleDict);
        OSDictionary *copy = OSDynamicCast(OSDictionary, ret);
        if (_childArray && copy) {
            OSCollection *childCopy = _childArray->copyCollection(cycleDict);
            if (childCopy) {
                copy->setObject( kIOHIDElementKey, childCopy );
                childCopy->release();
            }
        }
    }
    else {
        kprintf("%s - no properties\n", __PRETTY_FUNCTION__);
    }
    return ret;
}

