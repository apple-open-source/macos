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

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOLib.h>
#include "IOHIDElement.h"
#include "IOHIDDevice.h"
#include "IOHIDEventQueue.h"
#include "IOHIDParserPriv.h"

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
            
#define GetArrayItemIndex(sel) \
            (sel - _logicalMin)

#define GetArrayItemSel(index) \
            (index + _logicalMin)

#define super OSObject
OSDefineMetaClassAndStructors( IOHIDElement, OSObject )

//---------------------------------------------------------------------------
// 
 
bool IOHIDElement::init( IOHIDDevice * owner, IOHIDElementType type )
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

IOHIDElement *
IOHIDElement::buttonElement( IOHIDDevice *     owner,
                             IOHIDElementType  type,
                             HIDButtonCapabilitiesPtr  button,
                             IOHIDElement *    parent )
{
    IOHIDElement * element = new IOHIDElement;

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
        
        if (!IsArrayElement(element))
        {
            element->_reportCount = 1;
        }
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
    }
    else
    {
        element->_reportBits     = 1;
        element->_units          = button->units;
        element->_unitExponent   = button->unitExponent;
        
        if (element->_reportCount > 1)
        {
            element->_duplicateReportHandler = element;
            element->_duplicateElements = OSArray::withCapacity(element->_reportCount);        
            
            if (element->_duplicateElements == NULL)
                goto BUTTON_ELEMENT_RELEASE;        
        }
    }

    // Register with owner and parent, then spawn sub-elements.

    if ( ( parent && ( parent->addChildElement(element, IsArrayElement(element)) == true ) )
    &&   ( owner->registerElement( element, &element->_cookie ) == true )
    &&   ( element->createSubElements() == true ))
    {
        return element;
    }

BUTTON_ELEMENT_RELEASE:
    element->release();
    element = 0;

    return element;
}

//---------------------------------------------------------------------------
// 

IOHIDElement *
IOHIDElement::valueElement( IOHIDDevice *     owner,
                            IOHIDElementType  type,
                            HIDValueCapabilitiesPtr   value,
                            IOHIDElement *    parent )
{
    IOHIDElement * element = new IOHIDElement;

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
        return element;
    }

VALUE_ELEMENT_RELEASE:
    element->release();
    element = 0;

    return element;
}

//---------------------------------------------------------------------------
// 

IOHIDElement *
IOHIDElement::collectionElement( IOHIDDevice *         owner,
                                 IOHIDElementType      type,
                                 HIDCollectionExtendedNodePtr  collection,
                                 IOHIDElement *        parent )
{
	IOHIDElement * element = new IOHIDElement;

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
    element->_collectionType = collection->data;

    // Register with owner and parent.

    if ( ( owner->registerElement( element, &element->_cookie ) == false )
    ||   ( ( parent && ( parent->addChildElement(element) == false ) ) ) )
    {
        element->release();
        element = 0;
    }

    return element;
}

//---------------------------------------------------------------------------
// 

IOHIDElement * IOHIDElement::reportHandlerElement(
                                            IOHIDDevice *        owner,
                                            IOHIDElementType     type,
                                            UInt32               reportID,
                                            UInt32               reportBits )
{
    IOHIDElement * element = new IOHIDElement;

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
    
    return element;
}


//---------------------------------------------------------------------------
// 

IOHIDElement * IOHIDElement::newSubElement( UInt16 rangeIndex ) const
{
    IOHIDElement * element = new IOHIDElement;

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

    return element;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElement::createSubElements()
{
    IOHIDElement * element;
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

void IOHIDElement::free()
{
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
    
    super::free();
}

//---------------------------------------------------------------------------
// 

bool IOHIDElement::addChildElement( IOHIDElement * child, bool arrayHeader)
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

        IOHIDElement *	arrayReportHandler;
        char		uniqueID[32];
        
        sprintf(uniqueID, "%4.4x%4.4x%4.4x",child->_type, child->_reportStartBit, child->_reportID);
        
        arrayReportHandler = _colArrayReportHandlers->getObject(uniqueID);
        
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

    return true;
}

IOHIDElement * IOHIDElement::arrayHandlerElement(                                
                                IOHIDDevice *    owner,
                                IOHIDElementType type,
                                IOHIDElement * child,
                                IOHIDElement * parent)
{
    IOHIDElement * element = new IOHIDElement;

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
	return element;
    }
        
    
ARRAY_HANDLER_ELEMENT_RELEASE:
    element->release();
    element = 0;
    
    return element;
}
//---------------------------------------------------------------------------
// 

bool IOHIDElement::serialize( OSSerialize * s ) const
{
    IORegistryEntry * entry;
    UInt32            usage;
    bool              ret = false;
        
    do {
        entry = new IORegistryEntry;
        if ( entry == 0 ) break;

        if ( entry->init() == false ) break;

        usage = (_usageMax != _usageMin) ?
                 _usageMin + _rangeIndex  :
                 _usageMin;

        entry->setProperty( kIOHIDElementKey, _childArray );
        entry->setProperty( kIOHIDElementCookieKey, (UInt32) _cookie, 32 );
        entry->setProperty( kIOHIDElementTypeKey, _type, 32 );
        entry->setProperty( kIOHIDElementUsageKey, usage, 32 );
        entry->setProperty( kIOHIDElementUsagePageKey, _usagePage, 32 );

        if ( _type == kIOHIDElementTypeCollection )
        {
            entry->setProperty( kIOHIDElementCollectionTypeKey, _collectionType, 32 );
            
            ret = true;
            break;
        }

        entry->setProperty( kIOHIDElementSizeKey, (_reportBits * _reportCount), 32 );

        if (_reportCount > 1)
        {
            entry->setProperty( kIOHIDElementReportSizeKey, _reportBits, 32 );
            entry->setProperty( kIOHIDElementReportCountKey, _reportCount, 32 );
        }

        entry->setProperty( kIOHIDElementValueLocationKey,
                            (UInt32) _elementValueLocation, 32 );
                            
        if ( _isInterruptReportHandler )
        {
            ret = true;
            break;
        }

        entry->setProperty( kIOHIDElementHasNullStateKey,
                            _flags & kHIDDataNullState );
        entry->setProperty( kIOHIDElementHasPreferredStateKey,
                            !(_flags & kHIDDataNoPreferred) );
        entry->setProperty( kIOHIDElementIsNonLinearKey,
                            _flags & kHIDDataNonlinear );
        entry->setProperty( kIOHIDElementIsRelativeKey,
                            _flags & kHIDDataRelative );
        entry->setProperty( kIOHIDElementIsWrappingKey,
                            _flags & kHIDDataWrap );
        entry->setProperty( kIOHIDElementIsArrayKey, 
                            IsArrayElement(this) );
        entry->setProperty( kIOHIDElementMaxKey, _logicalMax, 32 );
        entry->setProperty( kIOHIDElementMinKey, _logicalMin, 32 );
        entry->setProperty( kIOHIDElementScaledMaxKey, _physicalMax, 32 );
        entry->setProperty( kIOHIDElementScaledMinKey, _physicalMin, 32 );
                
        if (IsDuplicateElement(this) && !IsDuplicateReportHandler(this))
        {
            entry->setProperty( kIOHIDElementDuplicateIndexKey, _rangeIndex, 32);
        }
        
        // RY: No reason to publish the unit and unit exponent
        // for array elements.
        if ( !IsArrayElement(this) )
        {
            entry->setProperty( kIOHIDElementUnitKey, _units, 32 );
            entry->setProperty( kIOHIDElementUnitExponentKey, _unitExponent, 32 );
        }

        
        ret = true;
    }
    while ( false );

    if ( entry )
    {
        if ( ret ) ret = entry->serializeProperties(s);
        entry->release();
    }
    
    return ret;
}

//---------------------------------------------------------------------------
// 

UInt32 IOHIDElement::getElementValueSize() const
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
                           UInt32        srcStartBit,
                           UInt32        bitsToCopy,
                           bool          shouldSignExtend,
                           bool *        valueChanged )
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
                *valueChanged = true;
            }
            word = 0;
            lastDstOffset = dstOffset;
        }
    }
}

static void writeReportBits( const UInt32 * src,
                           UInt8 *        dst,
                           UInt32         dstStartBit,
                           UInt32         bitsToCopy)
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

bool IOHIDElement::processReport( UInt8                reportID,
                                  void *               reportData,
                                  UInt32               reportBits,
                                  const AbsoluteTime * timestamp,
                                  IOHIDElement **      next,
                                  IOOptionBits         options)
{
    IOHIDEventQueue * queue;
    bool              changed = false;
        
    // Set next pointer to the next report handler in the chain.
    // If this is an array, set the report handler to the one
    // the array.
    if (next)
    {
        *next = _nextReportHandler;
        
        if (_isInterruptReportHandler && (options & kIOHIDReportOptionNotInterrupt))
        {
            return false;
        }

        if (IsArrayElement(this) && !IsArrayReportHandler(this))
        {
            *next = _arrayReportHandler;
            return false;
        }
        
        // Verify incoming report size.

        if ( _reportSize && ( reportBits < _reportSize ) )
        {
            *next = 0;
            return false;
        }

    }

    do {
        // Ignore report that does not match our report ID.

        if ( _reportID != reportID )
            break;
        
        // The generation is incremented before and after
        // processing the report.  An odd value tells us
        // that the information is incomplete and should
        // not be trusted.  An even value tells us that
        // the value is complete.
        _elementValue->generation++;

        // Get the element value from the report.

        readReportBits( (UInt8 *) reportData,   /* source buffer      */
                       _elementValue->value,   /* destination buffer */
                       _reportStartBit,        /* source start bit   */
                       (_reportBits * _reportCount), /* bits to copy       */
                       (((SInt32)_logicalMin < 0) || ((SInt32)_logicalMax < 0)), /* should sign extend */
                       &changed );             /* did value change?  */

        // Set a timestamp to indicate the last modification time.
        // We should set the time stamp if the generation is 1 regardless if the value
        // changed.  This will insure that an initial value of 0 will have the correct
        // timestamp
        if ( changed || (_flags & kHIDDataRelativeBit) || (_elementValue->generation == 1))
        {
            _elementValue->timestamp = *timestamp;

            if (IsArrayElement(this) && IsArrayReportHandler(this))
                processArrayReport(reportID, reportData, reportBits, timestamp);

            if ( _queueArray )
            {
                for ( UInt32 i = 0;
                      (queue = (IOHIDEventQueue *) _queueArray->getObject(i));
                      i++ )
                {
                    queue->enqueue( (void *) _elementValue,
                                    _elementValue->totalSize );
                }
            }
        }

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

bool IOHIDElement::createReport( UInt8           reportID,
                                 void *		 reportData,  // this report should be alloced outisde this method.
                                 UInt32 *        reportLength,
                                 IOHIDElement ** next )
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
                           _reportStartBit,       	/* dst start bit      */                           
                           (_reportBits * _reportCount));/* bits to copy       */

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

bool IOHIDElement::setMemoryForElementValue( IOVirtualAddress address,
                                             void *           location )
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

UInt32 IOHIDElement::getRangeCount() const
{
    // FIXME - shouldn't we use logical min/max?

    // Check to see if we have multiple elements with the same usage.
    // If so, return the _reportCount
    return (_reportCount > 1) ? _reportCount : (_usageMax-_usageMin + 1 );
}

//---------------------------------------------------------------------------
// Return the number of elements in a usage range.

UInt32 IOHIDElement::getStartingRangeIndex() const
{
    // Check to see if we have multiple elements with the same usage.
    return (_reportCount > 1) ? 0 : 1;
}


//---------------------------------------------------------------------------
// 

IOHIDElement *
IOHIDElement::setNextReportHandler( IOHIDElement * element )
{
    IOHIDElement * prev = _nextReportHandler;
    _nextReportHandler  = element;
    return prev;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElement::getReportType( IOHIDReportType * reportType ) const
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

bool IOHIDElement::addEventQueue( IOHIDEventQueue * queue )
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

bool IOHIDElement::removeEventQueue( IOHIDEventQueue * queue )
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

bool IOHIDElement::hasEventQueue( IOHIDEventQueue * queue )
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

UInt32 IOHIDElement::setReportSize( UInt32 numberOfBits )
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
void IOHIDElement::setOutOfBoundsValue()
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

bool IOHIDElement::createDuplicateReport(UInt8		reportID,
                                        void *		reportData,
                                        UInt32 *	reportLength)
{
    bool		pending = false;
    IOHIDElement 	*element;

    // RY: Then, check the other duplicates to see if they are currently 
    // pending.  
    for (int i=0;  _duplicateElements && i<_duplicateElements->getCount(); i++) 
    {
        if ((element = _duplicateElements->getObject(i)) &&
            (element->_transactionState == kIOHIDTransactionStatePending))
        {
            pending = true;
        }
        
        element->createReport(reportID, reportData, reportLength, 0);
    }
    
    return pending;
}

bool IOHIDElement::createArrayReport(UInt8	reportID,
                                    void *	reportData,
                                    UInt32 *	reportLength) 
{
    IOHIDElement 	*element, *arrayElement;
    bool		changed;
    UInt32		arraySel;
    UInt32		reportIndex = 0;
    
    if (createDuplicateReport(reportID, reportData, reportLength))
        return true;
                        
    for (int i=0; i<_arrayItems->getCount(); i++)
    {
        element = (IOHIDElement *)(_arrayItems->getObject(i));

        if (!element)
            continue;
        if ( element->_transactionState == kIOHIDTransactionStateIdle )
            continue;
            
        if (element->_elementValue->value[0] == 0)
            continue;

        arraySel = GetArrayItemSel(i);

        if (arrayElement = (_duplicateElements) ? _duplicateElements->getObject(reportIndex) : this)
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
        if (arrayElement = (_duplicateElements) ? _duplicateElements->getObject(reportIndex) : this)
        {
            arrayElement->_elementValue->value[0] = arraySel;
            arrayElement->_transactionState = kIOHIDTransactionStatePending;
            arrayElement->createReport(reportID, reportData, reportLength, 0);
        }
    }
    
    return true;
}

void IOHIDElement::setArrayElementValue(UInt32 index, UInt32 value)
{
    IOHIDElement 	*element;
    IOHIDEventQueue	*queue;
    
    if ( !_arrayItems || (index > _arrayItems->getCount()))
        return;
        
    element = (IOHIDElement *)(_arrayItems->getObject(index));
    
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

bool IOHIDElement::processArrayReport(	UInt8			reportID,
                                        void *			reportData,
                                        UInt32			reportBits,
                                        const AbsoluteTime *	timestamp)
{
    IOHIDElement *element;
    UInt32	arraySel, prevArraySel;
    UInt32	iNewArray, iOldArray, startBit;
    bool	found, changed;
    
    // If the generation == 1, we know that this is first time that 
    // we have processed this array report.  Set the time stamp on 
    // all array elements.
    if (_elementValue->generation == 1)
    {            
        for (int i=0; i < _arrayItems->getCount(); i++)
            setArrayElementValue(i, 0);
    }
    
    // RY: Process the arry selector elements.  If any of their values
    // haven't changed, don't bother with any further processing.  
    if (_duplicateElements)
    {
        changed = false;
        for (iNewArray = 0; iNewArray < _reportCount; iNewArray ++)
        {
            if (element = _duplicateElements->getObject(iNewArray))
            {
                changed |= element->processReport(reportID, reportData, reportBits, timestamp, 0);
            }
        }
        
        if (!changed)
            return changed;
    }
                                    
    // Check the existing indexes against the originals
    for (iOldArray = 0; iOldArray < _reportCount; iOldArray ++)
    {
        arraySel = _oldArraySelectors[iOldArray];
        
        // If we've seen this value before,
        // we can break out of this loop.
        if ((iOldArray > 0) && (prevArraySel == arraySel))
                break;
                
        prevArraySel = arraySel;
        found = false;

        for (iNewArray = 0; iNewArray < _reportCount; iNewArray ++)
        {
            element = (_duplicateElements) ? _duplicateElements->getObject(iNewArray) : this;
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
        if (!(element = (_duplicateElements) ? _duplicateElements->getObject(iNewArray) : this))
            continue;
            
        arraySel = element->_elementValue->value[0];
                
        // If we've seen this value before,
        // we can break out of this loop.
        if ((iNewArray > 0) && (prevArraySel == arraySel))
                break;
                
        prevArraySel = arraySel;
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
        if (element = (_duplicateElements) ? _duplicateElements->getObject(iOldArray) : this)
        _oldArraySelectors[iOldArray] = element->_elementValue->value[0];
    }

    return changed;
}
