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

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOLib.h>
#include "IOHIDElement.h"
#include "IOHIDDevice.h"
#include "IOHIDEventQueue.h"
#include "IOHIDParserPriv.h"

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

    return true;
}

//---------------------------------------------------------------------------
// 

IOHIDElement *
IOHIDElement::buttonElement( IOHIDDevice *     owner,
                             IOHIDElementType  type,
                             HIDButtonCapsPtr  button,
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
    element->_reportBits     = 1;
    element->_reportStartBit = button->startBit;
    element->_reportID       = button->reportID;
    element->_usagePage      = button->usagePage;
    element->_logicalMin     = element->_physicalMin = 0;
    element->_logicalMax     = element->_physicalMax = 1;
    element->_arrayIndex     = 0;    

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

    // Register with owner and parent, then spawn sub-elements.

    if ( ( owner->registerElement( element, &element->_cookie ) == false )
    ||   ( ( parent && ( parent->addChildElement(element) == false ) ) )
    ||   ( element->createSubElements() == false ) )
    {
        element->release();
        element = 0;
    }

    return element;
}

//---------------------------------------------------------------------------
// 

IOHIDElement *
IOHIDElement::valueElement( IOHIDDevice *     owner,
                            IOHIDElementType  type,
                            HIDValueCapsPtr   value,
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
    element->_reportStartBit = value->startBit;
    element->_reportID       = value->reportID;
    element->_usagePage      = value->usagePage;
    element->_logicalMin     = value->logicalMin;
    element->_logicalMax     = value->logicalMax;
    element->_physicalMin    = value->physicalMin;
    element->_physicalMax    = value->physicalMax;
    element->_arrayIndex     = 0;

    if ( value->isRange )
    {
        element->_usageMin = value->u.range.usageMin;
        element->_usageMax = value->u.range.usageMax;
    }
    else
    {
        element->_usageMin = value->u.notRange.usage;
        element->_usageMax = value->u.notRange.usage;
    }

    // Register with owner and parent, then spawn sub-elements.

    if ( ( owner->registerElement( element, &element->_cookie ) == false )
    ||   ( ( parent && ( parent->addChildElement(element) == false ) ) ) 
    ||   ( element->createSubElements() == false ) )
    {
        element->release();
        element = 0;
    }

    return element;
}

//---------------------------------------------------------------------------
// 

IOHIDElement *
IOHIDElement::collectionElement( IOHIDDevice *         owner,
                                 IOHIDElementType      type,
                                 HIDCollectionNodePtr  collection,
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

    element->_usagePage = collection->collectionUsagePage;
    element->_usageMin  = collection->collectionUsage;
    element->_usageMax  = collection->collectionUsage;

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

IOHIDElement * IOHIDElement::newSubElement( UInt16 arrayIndex ) const
{
	IOHIDElement * element = new IOHIDElement;

    // Check arguments and call init().

    if ( ( arrayIndex == 0 ) ||
         ( element    == 0 ) ||
         ( element->init( _owner, _type ) == false ) )
    {
        if ( element ) element->release();
        return 0;
    }

    // Set HID properties.

    element->_flags          = _flags;
    element->_reportBits     = _reportBits;
    element->_reportStartBit = _reportStartBit + (arrayIndex * _reportBits);
    element->_reportID       = _reportID;
    element->_usagePage      = _usagePage;
    element->_logicalMin     = _logicalMin;
    element->_logicalMax     = _logicalMax;
    element->_physicalMin    = _physicalMin;
    element->_physicalMax    = _physicalMax;
    element->_usageMin       = _usageMin;
    element->_usageMax       = _usageMax;
    element->_arrayIndex     = arrayIndex;

    // Register with owner and parent.

    if ( ( _owner->registerElement( element, &element->_cookie ) == false )
    ||   ( ( _parent && ( _parent->addChildElement(element) == false ) ) ) )
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
    UInt32         count = getArrayCount();
    UInt32         index = 1;
    bool           ret = true;

    while ( index < count )
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

    super::free();
}

//---------------------------------------------------------------------------
// 

bool IOHIDElement::addChildElement( IOHIDElement * child )
{
    bool ret;

    if ( _childArray == 0 )
    {
        _childArray = OSArray::withCapacity(4);
    }

    ret = ( _childArray ) ? _childArray->setObject( child )
                          : false;

    if ( ret ) child->_parent = this;

    return ret;
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
                 _usageMin + _arrayIndex  :
                 _usageMin;

        entry->setProperty( kIOHIDElementKey, _childArray );
        entry->setProperty( kIOHIDElementCookieKey, (UInt32) _cookie, 32 );
        entry->setProperty( kIOHIDElementTypeKey, _type, 32 );
        entry->setProperty( kIOHIDElementUsageKey, usage, 32 );
        entry->setProperty( kIOHIDElementUsagePageKey, _usagePage, 32 );

        if ( _type == kIOHIDElementTypeCollection )
        {
            ret = true;
            break;
        }

        entry->setProperty( kIOHIDElementValueLocationKey,
                            (UInt32) _elementValueLocation, 32 );
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
        entry->setProperty( kIOHIDElementMaxKey, _logicalMax, 32 );
        entry->setProperty( kIOHIDElementMinKey, _logicalMin, 32 );
        entry->setProperty( kIOHIDElementSizeKey, _reportBits, 32 );

        
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
    UInt32  reportWords = _reportBits / (sizeof(UInt32) * 8);
    UInt32  size        = sizeof(IOHIDElementValue);

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

static void getReportBits( const UInt8 * src,
                           UInt32 *      dst,
                           UInt32        srcStartBit,
                           UInt32        bitsToCopy,
						   bool			 shouldSignExtend,
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
				SInt32 temp = word;
				
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

static void setReportBits( const UInt32 * src,
                           UInt8 *        dst,
                           UInt32         dstStartBit,
                           UInt32         bitsToCopy )
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
                                  IOHIDElement **      next )
{
    IOHIDEventQueue * queue;
    bool              handled = false;
    bool              changed = false;
        
    // Set next pointer to the next report handler in the chain.

    *next = _nextReportHandler;

    do {
        // Ignore report that does not match our report ID.

        if ( _reportID != reportID )
            break;
        
        // Verify incoming report size.

        if ( _reportSize && ( reportBits < _reportSize ) )
        {
            *next = 0;
            break;
        }

        _elementValue->generation++;

        // Get the element value from the report.

        getReportBits( (UInt8 *) reportData,   /* source buffer      */
                       _elementValue->value,   /* destination buffer */
                       _reportStartBit,        /* source start bit   */
                       _reportBits,            /* bits to copy       */
					   (((SInt32)_logicalMin < 0) || ((SInt32)_logicalMax < 0)),
											   /* should sign extend */
                       &changed );             /* did value change?  */

        // Set a timestamp to indicate the last modification time.

        if ( changed || (_flags & kHIDDataRelativeBit) )
        {
            _elementValue->timestamp = *timestamp;

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

        handled = true;
    }
    while ( false );

    return handled;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElement::createReport( UInt8           reportID,
                                 void **         reportData,
                                 UInt32 *        reportLength,
                                 IOHIDElement ** next )
{
    bool handled = false;

    // Point to next report handler.

    *next = _nextReportHandler;

    do {
        // Ignore report that does not match our report ID.
    
        if ( _reportID != reportID )
            break;

        // Allocate memory for the report.
        // XXX - set report ID byte.

        if ( _reportSize )
        {
            *reportLength = _reportSize / 8;            
            *reportData   = IOMalloc( *reportLength );

            if ( *reportData == 0 )
            {
                *next = 0;
                break;
            }

            bzero( *reportData, *reportLength );
        }

        // Set the element value to the report.

        if ( *reportData )
        {
            setReportBits( _elementValue->value,   /* source buffer      */
                           (UInt8 *) *reportData,  /* destination buffer */
                           _reportStartBit,        /* dst start bit      */
                           _reportBits );          /* bits to copy       */

            handled = true;
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
// Return the number of elements in an array item.

UInt32 IOHIDElement::getArrayCount() const
{
    // FIXME - shouldn't we use logical min/max?

    return ( _usageMax - _usageMin + 1 );
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
    if ( _queueArray == 0 )
    {
        _queueArray = OSArray::withCapacity(4);
    }

    if ( hasEventQueue(queue) == true )
        return false;

    return _queueArray ? _queueArray->setObject( queue ) : false;
}

//---------------------------------------------------------------------------
// 

bool IOHIDElement::removeEventQueue( IOHIDEventQueue * queue )
{
    OSObject * obj = 0;

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
