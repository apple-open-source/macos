/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2012 Apple Computer, Inc.  All Rights Reserved.
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <AssertMacros.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>
#include "IOHIDDescriptorParser.h"
#include "IOHIDDescriptorParserPrivate.h"

typedef struct _IOHIDElementValue
{
    IOHIDElementCookie  cookie;
    uint32_t            flags:8;
    uint32_t            totalSize:24;
    uint64_t            timestamp;
    uint32_t            generation;
    uint32_t            value[1];
} IOHIDElementValue;

struct IOHIDElementPrivate_IVars
{
    IOHIDElementContainer       *owner;
    IOHIDElementType            type;
    IOHIDElementCookie          cookie;
    IOHIDElementPrivate         *nextReportHandler;
    IOHIDElementValue           *elementValue;
    void                        *elementValueLocation;
    IOHIDElementPrivate         *parent;
    OSArray                     *childArray;
    uint32_t                    flags;
    IOHIDElementCollectionType  collectionType;
    
    uint32_t                    reportSize;
    uint32_t                    reportCount;
    uint32_t                    rawReportCount;
    uint32_t                    reportStartBit;
    uint32_t                    reportBits;
    uint32_t                    reportID;
    
    uint32_t                    usagePage;
    uint32_t                    usage;
    uint32_t                    usageMin;
    uint32_t                    usageMax;
    
    uint32_t                    logicalMin;
    uint32_t                    logicalMax;
    uint32_t                    physicalMin;
    uint32_t                    physicalMax;
    
    uint32_t                    unitExponent;
    uint32_t                    units;
    
    uint32_t                    transactionState;
    
    OSData                      *dataValue;
    
    IOHIDElementPrivate         *duplicateReportHandler;
    IOHIDElementPrivate         *arrayReportHandler;
    IOHIDElementPrivate         **rollOverElementPtr;
    OSDictionary                *colArrayReportHandlers;
    OSArray                     *arrayItems;
    OSArray                     *duplicateElements;
    uint32_t                    *oldArraySelectors;
    
    bool                        isInterruptReportHandler;
    bool                        shouldTickleActivity;
    uint8_t                     variableSize;
    uint32_t                    currentReportSizeBits;
    uint32_t                    previousValue;
    
    struct {
        int32_t    satMin;
        int32_t    satMax;
        int32_t    dzMin;
        int32_t    dzMax;
        int32_t    min;
        int32_t    max;
        IOFixed    gran;
    } calibration;
};

#define _owner                      ivars->owner
#define _type                       ivars->type
#define _cookie                     ivars->cookie
#define _nextReportHandler          ivars->nextReportHandler
#define _elementValue               ivars->elementValue
#define _elementValueLocation       ivars->elementValueLocation
#define _parent                     ivars->parent
#define _childArray                 ivars->childArray
#define _flags                      ivars->flags
#define _collectionType             ivars->collectionType
#define _reportSize                 ivars->reportSize
#define _reportCount                ivars->reportCount
#define _reportStartBit             ivars->reportStartBit
#define _reportBits                 ivars->reportBits
#define _reportID                   ivars->reportID
#define _usagePage                  ivars->usagePage
#define _usage                      ivars->usage
#define _usageMin                   ivars->usageMin
#define _usageMax                   ivars->usageMax
#define _logicalMin                 ivars->logicalMin
#define _logicalMax                 ivars->logicalMax
#define _physicalMin                ivars->physicalMin
#define _physicalMax                ivars->physicalMax
#define _unitExponent               ivars->unitExponent
#define _units                      ivars->units
#define _transactionState           ivars->transactionState
#define _dataValue                  ivars->dataValue
#define _duplicateReportHandler     ivars->duplicateReportHandler
#define _arrayReportHandler         ivars->arrayReportHandler
#define _rollOverElementPtr         ivars->rollOverElementPtr
#define _colArrayReportHandlers     ivars->colArrayReportHandlers
#define _arrayItems                 ivars->arrayItems
#define _duplicateElements          ivars->duplicateElements
#define _oldArraySelectors          ivars->oldArraySelectors
#define _isInterruptReportHandler   ivars->isInterruptReportHandler
#define _shouldTickleActivity       ivars->shouldTickleActivity
#define _variableSize               ivars->variableSize
#define _currentReportSizeBits      ivars->currentReportSizeBits
#define _previousValue              ivars->previousValue
#define _calibration                ivars->calibration
#define _rawReportCount             ivars->rawReportCount

#define IsArrayElement(element) \
            ((element->_flags & kHIDDataArrayBit) == kHIDDataArray)

#define GetArrayItemIndex(sel) \
            (sel - _logicalMin)

#define super OSContainer

#ifndef min
#define min(a, b) ((a < b) ? a : b)
#endif

bool IOHIDElementPrivate::init(IOHIDElementContainer * owner,
                               IOHIDElementType type)
{
    bool ret = false;
    
    ret = super::init();
    require_action(ret, exit, HIDLogError("Init:%x", ret));
    
    ivars = IONewZero(IOHIDElementPrivate_IVars, 1);
    
    _owner = owner;
    _type = type;
    _reportCount = 1;
    _rawReportCount = _reportCount;
    
    ret = true;
    
exit:
    return ret;
}

void IOHIDElementPrivate::free()
{
    OSSafeReleaseNULL(_childArray);
    OSSafeReleaseNULL(_arrayItems);
    OSSafeReleaseNULL(_duplicateElements);
    OSSafeReleaseNULL(_colArrayReportHandlers);
    OSSafeReleaseNULL(_dataValue);
    OSSafeReleaseNULL(_childArray);
    
    if (_oldArraySelectors) {
        IOFree(_oldArraySelectors, sizeof(UInt32) * _reportCount);
    }
    
    IOSafeDeleteNULL(ivars, IOHIDElementPrivate_IVars, 1);
    
    super::free();
}

IOHIDElementPrivate *IOHIDElementPrivate::buttonElement(
                                                IOHIDElementContainer *owner,
                                                IOHIDElementType type,
                                                HIDButtonCapabilitiesPtr button,
                                                IOHIDElementPrivate *parent)
{
    IOHIDElementPrivate *element = NULL;
    bool result = false;
    
    require(button, exit);
    
    element = OSTypeAlloc(IOHIDElementPrivate);
    require(element && element->init(owner, type), exit);
    
    element->_flags = button->bitField;
    element->_reportStartBit = button->startBit;
    element->_reportID = button->reportID;
    element->_usagePage = button->usagePage;
    element->_logicalMin = element->_physicalMin = 0;
    element->_logicalMax = element->_physicalMax = 1;
    
    if (button->isRange) {
        element->_usageMin = button->u.range.usageMin;
        element->_usageMax = button->u.range.usageMax;        
    } else {
        element->_usageMin = button->u.notRange.usage;
        element->_usageMax = button->u.notRange.usage;
    }
    
    element->_usage = element->_usageMin;
    
    if (IsArrayElement(element)) {
        // RY: Hack to gain the logical min/max for
        // elements.
        element->_logicalMin = element->_physicalMin = button->u.notRange.reserved2;
        element->_logicalMax = element->_physicalMax = button->u.notRange.reserved3;
        
        // RY: Hack to gain the report size and report
        // count for Array type elements.  This works 
        // out because array elements do not make use
        // of the unit and unit exponent.  Plus, this
        // keeps us binary compatible.  Appropriate
        // changes have been made to the HIDParser to
        // support this.
        element->_reportBits = button->unitExponent;
        element->_reportCount = button->units;
        
        // RY: Let's set the minimum range for keys that this keyboard supports.
        // This is needed because some keyboard describe support for 101 keys, but
        // in actuality support a far greater amount of keys.  Ex. JIS keyboard on
        // Q41B and Q16B.
        if (button->isRange &&
            element->_usagePage == kHIDPage_KeyboardOrKeypad &&
            element->_usageMax < (kHIDUsage_KeyboardLeftControl - 1)) {
            element->_usageMax = kHIDUsage_KeyboardLeftControl - 1;
        }
    } else {
        element->_reportBits = 1;
        element->_units = button->units;
        element->_unitExponent = button->unitExponent;
    }
    
    element->_rawReportCount = element->_reportCount;
    element->_currentReportSizeBits = element->_reportBits * element->_reportCount;
    
    if (parent) {
        require(parent->addChildElement(element, IsArrayElement(element)), exit);
    }
    
    owner->registerElement(element, &element->_cookie);
    
    require(element->createSubElements(), exit);
    
    result = true;
    
exit:
    if (!result && element) {
        element->release();
        element = NULL;
    }
    
    return element;
}

IOHIDElementPrivate *IOHIDElementPrivate::valueElement(
                                                IOHIDElementContainer *owner,
                                                IOHIDElementType type,
                                                HIDValueCapabilitiesPtr value,
                                                IOHIDElementPrivate *parent)
{
    IOHIDElementPrivate *element = NULL;
    bool result = false;
    
    require(value, exit);
    
    element = OSTypeAlloc(IOHIDElementPrivate);
    require(element && element->init(owner, type), exit);
    
    element->_flags = value->bitField;
    element->_reportBits = value->bitSize;
    element->_reportCount = value->reportCount;
    element->_reportStartBit = value->startBit;
    element->_reportID = value->reportID;
    element->_usagePage = value->usagePage;
    element->_logicalMin = value->logicalMin;
    element->_logicalMax = value->logicalMax;
    element->_physicalMin = value->physicalMin;
    element->_physicalMax = value->physicalMax;
    element->_units = value->units;
    element->_unitExponent = value->unitExponent;
    element->_rawReportCount = element->_reportCount;
    
    if (value->isRange) {
        element->_usageMin = value->u.range.usageMin;
        element->_usageMax = value->u.range.usageMax;

        element->_reportCount = 1;
    } else {
        element->_usageMin = value->u.notRange.usage;
        element->_usageMax = value->u.notRange.usage;
    }
    
    element->_usage = element->_usageMin;
    
    element->_currentReportSizeBits = element->_reportBits * element->_reportCount;
    
    // If we have usage ranges, we set report count as 1
    if (element->_reportCount > 1)
    {
        element->_reportBits *= element->_reportCount;
        element->_reportCount = 1;
    }
    
    if (parent && parent->getUsagePage() == kHIDPage_AppleVendor &&
        (parent->getUsage() == kHIDUsage_AppleVendor_Message ||
         parent->getUsage() == kHIDUsage_AppleVendor_Payload)) {
        element->_variableSize |= kIOHIDElementVariableSizeElement;
    }
    // Register with owner and parent, then spawn sub-elements.
    
    owner->registerElement(element, &element->_cookie);
    
    if (parent) {
        require(parent->addChildElement(element, IsArrayElement(element)), exit);
    }
    
    require(element->createSubElements(), exit);
    
    result = true;
    
exit:
    if (!result && element) {
        element->release();
        element = NULL;
    }
    
    return element;
}

IOHIDElementPrivate *IOHIDElementPrivate::collectionElement(
                                        IOHIDElementContainer *owner,
                                        IOHIDElementType type,
                                        HIDCollectionExtendedNodePtr collection,
                                        IOHIDElementPrivate *parent)
{
    IOHIDElementPrivate *element = NULL;
    bool result = false;
    
    require(collection, exit);
    
    element = OSTypeAlloc(IOHIDElementPrivate);
    require(element && element->init(owner, type), exit);
    
    element->_usagePage = collection->collectionUsagePage;
    element->_usage = collection->collectionUsage;
    element->_usageMin = collection->collectionUsage;
    element->_usageMax = collection->collectionUsage;
    element->_collectionType = (IOHIDElementCollectionType)collection->data;
    
    element->_shouldTickleActivity = (element->_usagePage == kHIDPage_GenericDesktop);
    
    owner->registerElement(element, &element->_cookie);
    
    if (parent) {
        require(parent->addChildElement(element, false), exit);
    }
    
    result = true;
    
exit:
    if (!result && element) {
        element->release();
        element = NULL;
    }
    
    return element;
}

IOHIDElementPrivate *IOHIDElementPrivate::nullElement(
                                                IOHIDElementContainer *owner,
                                                uint32_t reportID,
                                                IOHIDElementPrivate *parent)
{
    IOHIDElementPrivate *element = NULL;
    bool result = false;
    
    element = OSTypeAlloc(IOHIDElementPrivate);
    require(element && element->init(owner, kIOHIDElementTypeInput_NULL), exit);
    
    element->_reportID = reportID;
    
    owner->registerElement(element, &element->_cookie);
    
    if (parent) {
        parent->addChildElement(element, false);
    }
    
    result = true;
    
exit:
    if (!result && element) {
        element->release();
        element = NULL;
    }
    
    return element;
}

IOHIDElementPrivate *IOHIDElementPrivate::reportHandlerElement(
                                                IOHIDElementContainer *owner,
                                                IOHIDElementType type,
                                                uint32_t reportID,
                                                uint32_t reportBits)
{
    IOHIDElementPrivate *element = NULL;
    bool result = false;
    
    require(reportBits, exit);
    
    element = OSTypeAlloc(IOHIDElementPrivate);
    require(element && element->init(owner, type), exit);
    
    element->_isInterruptReportHandler = true;
    element->_flags = kHIDDataVariable | kHIDDataRelative;
    element->_reportCount = 1;
    element->_reportID = reportID;
    element->_reportBits = element->_reportSize	= reportBits;
    element->_currentReportSizeBits = element->_reportBits * element->_reportCount;
    
    owner->registerElement(element, &element->_cookie);
    
    result = true;
    
exit:
    if (!result && element) {
        element->release();
        element = NULL;
    }
    
    return element;
}

IOHIDElementPrivate *IOHIDElementPrivate::newSubElement(uint16_t rangeIndex) const
{
    IOHIDElementPrivate *element = NULL;
    bool result = false;
    
    element = OSTypeAlloc(IOHIDElementPrivate);
    require(element && element->init(_owner, _type), exit);
    
    element->_flags = _flags;
    element->_reportID = _reportID;
    element->_usagePage = _usagePage;
    element->_usageMin = _usageMin;
    element->_usageMax = _usageMax;
    element->_arrayReportHandler = _arrayReportHandler;
    element->_reportBits = _reportBits;
    element->_reportStartBit = _reportStartBit + (rangeIndex * _reportBits);
    element->_logicalMin = _logicalMin;
    element->_logicalMax = _logicalMax;
    element->_physicalMin = _physicalMin;
    element->_physicalMax = _physicalMax;
    element->_units = _units;
    element->_unitExponent = _unitExponent;
    element->_rawReportCount = _reportCount;
    element->_currentReportSizeBits = element->_reportBits * element->_reportCount;
    
    if (element->_usageMax == element->_usageMin) {
        element->_usage = element->_usageMin;
    } else {
        element->_usage = element->_usageMin + rangeIndex;
    }
	
    // RY: Special handling for array elements.
    // FYI, if this an array and button element, then we
    // know that this is a dummy array element.  The start
    // is not used to process the report, but instead used
    // to identify the which arrayHandler is belongs to.
    // Therefore, all subelements should contain the same
    // start bit.
    if (IsArrayElement(this) && _reportBits == 1) {
        element->_reportStartBit = _reportStartBit;        
    }
    
    if (_duplicateElements) {
        _duplicateElements->setObject(element);
        element->_duplicateReportHandler = _duplicateReportHandler;
    }
    
    _owner->registerElement(element, &element->_cookie);
    
    if (_parent) {
        require(_parent->addChildElement(element, false), exit);
    }
    
    result = true;
    
exit:
    if (!result && element) {
        element->release();
        element = NULL;
    }
    
    return element;
}

bool IOHIDElementPrivate::createSubElements()
{
    bool ret = false;
    uint32_t rangeCount, rangeIndex;
    
    if (_reportCount > 1) {
        rangeCount = _reportCount;
        rangeIndex = 0;
    } else {
        rangeCount = _usageMax - _usageMin + 1;
        rangeIndex = 1;
    }
    
    while(rangeIndex < rangeCount) {
        IOHIDElementPrivate *element = newSubElement(rangeIndex++);
        require(element, exit);
        
        element->release();
    }
    
    ret = true;
    
exit:
    return ret;
}

bool IOHIDElementPrivate::addChildElement(IOHIDElementPrivate *child,
                                          bool arrayHeader)
{
    bool result = false;
    
    if (!_childArray) {
        _childArray = OSArray::withCapacity(4);
    }
    require(_childArray, exit);
            
    // Perform special processing if this is an array item
    // that doesn't directly handle the report.  Basically,
    // we want to group all related array elements together.
    // This will help out for elements that are not part of 
    // a range.  Since collections can span multiple 
    // reports, we will use the following as a unique ID:
    //	    8bits: reportID
    //	   32bits: startBit
    //	   32bits: elementType
    if (child->_type != kIOHIDElementTypeCollection &&
        IsArrayElement(child) &&
        child != child->_arrayReportHandler &&
        (arrayHeader || !child->_duplicateReportHandler))
    {
        IOHIDElementPrivate *arrayReportHandler;
        char uniqueID[32];
        
        if (!_colArrayReportHandlers) {
            _colArrayReportHandlers = OSDictionary::withCapacity(1);
        }
        require(_colArrayReportHandlers, exit);
        
        snprintf(uniqueID, sizeof(uniqueID), "%4.4x%4.4x%4.4x",
                 (unsigned)child->_type, (unsigned)child->_reportStartBit, (unsigned)child->_reportID);
        
        arrayReportHandler = (IOHIDElementPrivate *)_colArrayReportHandlers->getObject(uniqueID);
        
        if (arrayReportHandler) {
            child->_arrayReportHandler = arrayReportHandler;
        } else {
            // We need to create array head based on info from
            // the child.
            arrayReportHandler = arrayHandlerElement(child->_owner, child->_type, child, this);
            require(arrayReportHandler, exit);
            
            // Register this new element with this collection
            _colArrayReportHandlers->setObject(uniqueID, arrayReportHandler);
            arrayReportHandler->release();
        }
        
        // Now that we have the info from the child, revert
        // it back to a button.        
        child->_arrayReportHandler = arrayReportHandler;
        child->_reportBits = 1;
        child->_reportCount = 1;
        child->_logicalMin = child->_physicalMin = 0;
        child->_logicalMax = child->_physicalMax = 1;
        
        // Add the child to the array list
        arrayReportHandler->_arrayItems->setObject(child);
    }
    
    _childArray->setObject(child);
    child->_parent = this;
    
    // RY: only override the child if you are not the root element
    if (_cookie != 0) {
        child->_shouldTickleActivity = _shouldTickleActivity;
    }
    
    result = true;
    
exit:
    return result;
}

IOHIDElementPrivate *IOHIDElementPrivate::arrayHandlerElement(
                                                IOHIDElementContainer *owner,
                                                IOHIDElementType type,
                                                IOHIDElementPrivate *child,
                                                IOHIDElementPrivate *parent)
{
    IOHIDElementPrivate *element = NULL;
    bool result = false;
    
    element = OSTypeAlloc(IOHIDElementPrivate);
    require(element && element->init(owner, type), exit);
    
    element->_arrayReportHandler = element;
    element->_parent = parent;
    element->_flags = child->_flags;
    element->_reportID = child->_reportID;
    element->_usagePage = child->_usagePage;
    element->_usageMin = 0xffffffff;
    element->_usageMax = 0xffffffff;
    element->_usage  = 0xffffffff;
    element->_reportBits = child->_reportBits;
    element->_reportCount = child->_reportCount;
    element->_reportStartBit = child->_reportStartBit;
    element->_logicalMin = child->_logicalMin;
    element->_logicalMax = child->_logicalMax;
    element->_physicalMin = child->_physicalMin;
    element->_physicalMax = child->_physicalMax;
    element->_rawReportCount = child->_reportCount;
    element->_currentReportSizeBits = child->_reportBits * child->_reportCount;
    
    // Allocate the array for the array elements.
    element->_arrayItems = OSArray::withCapacity((child->_usageMax - child->_usageMin) + 1);
    require(element->_arrayItems, exit);
    
    // RY: Allocate a buffer that will contain the old Array selector.
    // This needed to compare the old report to the new report to
    // deterine which array items need to be turned on/off.
    element->_oldArraySelectors = (uint32_t *)IOMalloc(sizeof(UInt32) * element->_reportCount);
    require(element->_oldArraySelectors, exit);
    
    bzero (element->_oldArraySelectors, sizeof(UInt32) * element->_reportCount);
    
    if (element->_reportCount > 1) {
        element->_duplicateReportHandler = element;
        
        element->_duplicateElements = OSArray::withCapacity(element->_reportCount);        
        require(element->_duplicateElements, exit);
    }
    
    owner->registerElement(element, &element->_cookie);
    
    if (parent) {
        require(parent->addChildElement(element, false), exit);
    }
    
    require(element->createSubElements(), exit);
        
    result = true;
    
exit:
    if (!result && element) {
        element->release();
        element = NULL;
    }
    
    return element;
}

uint32_t IOHIDElementPrivate::getElementValueSize() const
{
    uint32_t size        = sizeof(IOHIDElementValue);
    uint32_t reportWords = (_reportBits * _reportCount) / (sizeof(UInt32) * 8);
    
    // RY: Don't forget the remainder.
    reportWords += ((_reportBits * _reportCount) % (sizeof(UInt32) * 8)) ? 1 : 0;
    
    if (reportWords > 1) {
        size += ((reportWords - 1) * sizeof(UInt32));
    }

    return size;
}

#define BIT_MASK(bits)  ((1UL << (bits)) - 1)

#define UpdateByteOffsetAndShift(bits, offset, shift)  \
    do { offset = bits >> 3; shift = bits & 0x07; } while (0)

#define UpdateWordOffsetAndShift(bits, offset, shift)  \
    do { offset = bits >> 5; shift = bits & 0x1f; } while (0)

static void readReportBits(const uint8_t *src,
                           uint32_t *dst,
                           uint32_t bitsToCopy,
                           uint32_t srcStartBit = 0,
                           bool shouldSignExtend = false,
                           bool *valueChanged = 0)
{
    uint32_t srcOffset = 0;
    uint32_t srcShift = 0;
    uint32_t dstShift = 0;
    uint32_t dstStartBit = 0;
    uint32_t dstOffset = 0;
    uint32_t lastDstOffset = 0;
    uint32_t word = 0;
    uint8_t bitsProcessed = 0;
    uint32_t totalBitsProcessed = 0;

    while (bitsToCopy) {
        uint32_t tmp;
        
        UpdateByteOffsetAndShift(srcStartBit, srcOffset, srcShift);
        
        bitsProcessed = min(bitsToCopy,
                            min(8 - srcShift, 32 - dstShift));
        
        tmp = (src[srcOffset] >> srcShift) & BIT_MASK(bitsProcessed);
        
        word |= (tmp << dstShift);
        
        dstStartBit += bitsProcessed;
        srcStartBit += bitsProcessed;
        bitsToCopy  -= bitsProcessed;
		totalBitsProcessed += bitsProcessed;
        
        UpdateWordOffsetAndShift(dstStartBit, dstOffset, dstShift);
        
        if (dstOffset != lastDstOffset || bitsToCopy == 0) {
            // sign extend negative values
			// if this is the leftmost word of the result
			if (lastDstOffset == 0 &&
				// and the logical min or max is less than zero
				// so we should sign extend
				shouldSignExtend) {
                
				// is this less than a full word
				if (totalBitsProcessed < 32 &&
					// and the value negative (high bit set)
                    (word & (1 << (totalBitsProcessed - 1))))
                {
                    // or in all 1s above the significant bit
                    word |= ~(BIT_MASK(totalBitsProcessed));
                }
			}
            
			if (dst[lastDstOffset] != word) {
                dst[lastDstOffset] = word;
                if (valueChanged) {
                    *valueChanged = true;
                }
            }
            
            word = 0;
            lastDstOffset = dstOffset;
        }
    }
}

static void writeReportBits(const uint32_t *src,
                            uint8_t *dst,
                            uint32_t bitsToCopy,
                            uint32_t dstStartBit = 0)
{
    uint32_t dstOffset = 0;
    uint32_t dstShift = 0;
    uint32_t srcShift = 0;
    uint32_t srcStartBit = 0;
    uint32_t srcOffset = 0;
    uint8_t bitsProcessed = 0;
    uint32_t tmp = 0;

    while (bitsToCopy) {
        UpdateByteOffsetAndShift(dstStartBit, dstOffset, dstShift);

        bitsProcessed = min(bitsToCopy,
                            min(8 - dstShift, 32 - srcShift));
        
        tmp = (src[srcOffset] >> srcShift) & BIT_MASK(bitsProcessed);
        
        dst[dstOffset] |= (tmp << dstShift);
        
        dstStartBit += bitsProcessed;
        srcStartBit += bitsProcessed;
        bitsToCopy  -= bitsProcessed;
        
        UpdateWordOffsetAndShift(srcStartBit, srcOffset, srcShift);
    }
}

bool IOHIDElementPrivate::processReport(uint8_t reportID,
                                        void *reportData,
                                        uint32_t reportBits,
                                        uint64_t timestamp,
                                        IOHIDElementPrivate **next,
                                        IOOptionBits options)
{
    bool changed = false;
    bool shouldProcess = false;
    uint32_t readSize = 0;
    
    if (_type == kIOHIDElementTypeInput_NULL
        && reportID == _reportID) {
        _elementValue->timestamp = timestamp;
        *next = NULL;
        goto exit;
    }
    
    // Set next pointer to the next report handler in the chain.
    // If this is an array, set the report handler to the one
    // the array.
    if (next) {
        *next = _nextReportHandler;
        
        require(_reportID == reportID, exit);
        
        // Verify incoming report size.
        if (!_variableSize && _reportSize && (reportBits < _reportSize)) {
            *next = 0;
            return false;
        }
        
        if (_isInterruptReportHandler && (options & kIOHIDReportOptionNotInterrupt)) {
            return false;
        }
        
        if (IsArrayElement(this) && this != _arrayReportHandler) {
            *next = _arrayReportHandler;
            return false;
        }
    }
    
    require(_reportID == reportID, exit);
    
    if (_variableSize & kIOHIDElementVariableSizeElement) {
        require(_reportStartBit < reportBits, exit);
    } else {
        uint32_t startingBit = _reportStartBit + (_reportBits * _reportCount);
        require(startingBit <= reportBits, exit);
    }
    
    if (_usagePage == kHIDPage_KeyboardOrKeypad &&
        _usage >= kHIDUsage_KeyboardLeftControl &&
        _usage <= kHIDUsage_KeyboardRightGUI &&
        _rollOverElementPtr &&
        *_rollOverElementPtr &&
        (*_rollOverElementPtr)->getValue())
    {
        uint64_t rollOverTS = (*_rollOverElementPtr)->getTimeStamp();
        require(timestamp != rollOverTS, exit);
    }
    
    // The generation is incremented before and after
    // processing the report.  An odd value tells us
    // that the information is incomplete and should
    // not be trusted.  An even value tells us that
    // the value is complete.
    _elementValue->generation++;
    
    _previousValue = _elementValue->value[0];
    
    // Get the element value from the report.
    readSize = _reportBits * _reportCount;
    if (_variableSize & kIOHIDElementVariableSizeElement) {
      uint32_t remainingBitSize = reportBits - _reportStartBit;
      readSize = (remainingBitSize < readSize) ? remainingBitSize : readSize;
    }
    
    readReportBits((uint8_t *) reportData,   /* source buffer      */
                   _elementValue->value,   /* destination buffer */
                   readSize,               /* bits to copy       */
                   _reportStartBit,        /* source start bit   */
                   (((int32_t )_logicalMin < 0) || ((int32_t )_logicalMax < 0)), /* should sign extend */
                   &changed);              /* did value change?  */
    
    _currentReportSizeBits = readSize;
    // Set a timestamp to indicate the last modification time.
    // We should set the time stamp if the generation is 1 regardless if the value
    // changed.  This will insure that an initial value of 0 will have the correct
    // timestamp
    
    shouldProcess = (changed || _isInterruptReportHandler || (_flags & kHIDDataRelativeBit));
    
    if (shouldProcess) {
        // Let's not update the timestamp in the case where the element is relative, the value is 0, and there is no change
        if (((_flags & kHIDDataRelativeBit) == 0) || (_reportBits > 32) || changed || _previousValue) {
            _elementValue->timestamp = timestamp;
        }
        
        if (IsArrayElement(this) && this == _arrayReportHandler) {
            processArrayReport(reportID, reportData, reportBits, _elementValue->timestamp);
        }
    }
    
    _elementValue->generation++;
    
    // If this element is part of a transaction
    // set its state to idle
    if (_transactionState) {
        _transactionState = kIOHIDTransactionStateIdle;
    }
    
exit:
    return changed;
}

bool IOHIDElementPrivate::createReport(uint8_t reportID,
                                       void *reportData,
                                       uint32_t *reportLength,
                                       IOHIDElementPrivate **next)
{
    bool handled = false;
    
    if (_type == kIOHIDElementTypeInput_NULL
        && reportID == _reportID) {
        *next = NULL;
        goto exit;
    }
    
    if (next) {
        *next = _nextReportHandler;
    }
    
    require(_reportID == reportID, exit);

    //------------------------------------------------
    // Changed this portion of the method.
    // The report is now allocated outside of the
    // method
    
    if (_reportSize) {
        *reportLength = _reportSize / 8;
        
        require_action(reportData, exit, {
            if (next) {
                *next = NULL;
            }
        });
        
        bzero(reportData, *reportLength);
    }
    
    //------------------------------------------------
    
    // Set next pointer to the next report handler in the chain.
    // If this is an array or duplicate, set the next to the
    // appropriate handler.
    if (next) {
        if (IsArrayElement(this)) {
            require_action(this == _arrayReportHandler, exit, {
                *next = _arrayReportHandler;
            });
            
            // RY: Only bother creating an array report is this element
            // is idle.
            if (_transactionState == kIOHIDTransactionStateIdle) {
                return createArrayReport(reportID, reportData, reportLength);
            }
        } else if (_duplicateReportHandler) {
            require_action(this == _duplicateReportHandler, exit, {
                *next = _duplicateReportHandler;
            });
            
            // RY: Only bother creating a report if the duplicate report
            // elements are idle.
            if (_transactionState == kIOHIDTransactionStateIdle) {
                return createDuplicateReport(reportID, reportData, reportLength);
            }
        }
    }
    
    // If this element has not been set, an out of bounds
    // value must be set.  This will cause the device
    // to ignore the report for this element.
    if (_transactionState == kIOHIDTransactionStateIdle) {
            setOutOfBoundsValue();
    }
    
    // Set the element value to the report.
    if (reportData) {
        writeReportBits(_elementValue->value,   	/* source buffer      */
                       (uint8_t *) reportData,  	/* destination buffer */
                       (_reportBits * _reportCount),/* bits to copy       */
                       _reportStartBit);       	/* dst start bit      */
        
        handled = true;
        
        // Clear the transaction state
        _transactionState = kIOHIDTransactionStateIdle;
    }
    
exit:
    return handled;
}

void IOHIDElementPrivate::setMemoryForElementValue(IOVirtualAddress address,
                                                   void *location)
{
    _elementValue = (IOHIDElementValue *)address;
    _elementValueLocation = location;
    
    bzero(_elementValue, getElementValueSize());
    
	_elementValue->cookie = _cookie;
	_elementValue->totalSize = getElementValueSize();
}

bool IOHIDElementPrivate::getReportType(IOHIDReportType *reportType) const
{
    if (_type <= kIOHIDElementTypeInput_NULL) {
        *reportType = kIOHIDReportTypeInput;
    } else if (_type == kIOHIDElementTypeOutput) {
        *reportType = kIOHIDReportTypeOutput;
    } else if (_type == kIOHIDElementTypeFeature) {
        *reportType = kIOHIDReportTypeFeature;
    } else {
        return false;
    }
    
    return true;
}

//---------------------------------------------------------------------------
// This methods will set an out of bounds element value.  This value will
// be based on the _logicalMin or _logicalMax depending on bit space.  If
// no room is available to go outside the range, the value will remain 
// unchanged.
void IOHIDElementPrivate::setOutOfBoundsValue()
{
    // Make sure we are not dealing with long element value type
    if (_elementValue->totalSize == sizeof(IOHIDElementValue)) {
        
        // Simple case:  If the _logicalMin > 0, then we can just
        // set the elementValue to 0
        if (_logicalMin > 0) {
            _elementValue->value[0] = 0;
        } else {
            // Make sure there is room
            if (((_logicalMax - _logicalMin) + 1) < (1 << _reportBits)) {
                // handle overflow
                if (((_logicalMax + 1) & BIT_MASK(_reportBits)) == (_logicalMax + 1)) {
                    _elementValue->value[0] = _logicalMax + 1;
                } else {
                    _elementValue->value[0] = _logicalMin - 1;
                }
            }
        }
    }
}

bool IOHIDElementPrivate::createDuplicateReport(uint8_t reportID,
                                                void *reportData,
                                                uint32_t *reportLength)
{
    bool pending = false;
    IOHIDElementPrivate *element;
    
    // RY: Then, check the other duplicates to see if they are currently 
    // pending.  
    for (unsigned int i = 0; _duplicateElements && i < _duplicateElements->getCount(); i++) {
        element = (IOHIDElementPrivate *)_duplicateElements->getObject(i);
        
        if (element->_transactionState == kIOHIDTransactionStatePending) {
            pending = true;
        }
        
        element->createReport(reportID, reportData, reportLength, 0);
    }
    
    return pending;
}

bool IOHIDElementPrivate::createArrayReport(uint8_t reportID,
                                            void *reportData,
                                            uint32_t *reportLength)
{
    IOHIDElementPrivate *element, *arrayElement;
    uint32_t arraySel;
    uint32_t i, reportIndex = 0;
    
    if (createDuplicateReport(reportID, reportData, reportLength)) {
        return true;
    }
                        
    for (i = 0; i < _arrayItems->getCount(); i++) {
        element = (IOHIDElementPrivate *)_arrayItems->getObject(i);
        
        if (!element) {
            continue;
        }
        
        if (element->_transactionState == kIOHIDTransactionStateIdle) {
            continue;
        }
            
        if (element->_elementValue->value[0] == 0) {
            continue;
        }
        
        arraySel = i + _logicalMin;
        
        if (_duplicateElements) {
            arrayElement = (IOHIDElementPrivate *)_duplicateElements->getObject(reportIndex);
        } else {
            arrayElement = this;
        }
        
        if (arrayElement) {
            arrayElement->_elementValue->value[0] = arraySel;
            arrayElement->_transactionState = kIOHIDTransactionStatePending;
            arrayElement->createReport(reportID, reportData, reportLength, 0);
        }
        
        reportIndex++;
        
        element->_transactionState = kIOHIDTransactionStateIdle;
        
        // Make sure we don't add to many usages to the report
        if (reportIndex >= _reportCount) {
            break;
        }
    }
    
    // Clear out the remaining portions of the report for this array
    arraySel = 0;
    for (i = reportIndex; i < _reportCount; i++) {
        if (_duplicateElements) {
            arrayElement = (IOHIDElementPrivate *)_duplicateElements->getObject(reportIndex);
        } else {
            arrayElement = this;
        }
        
        if (arrayElement) {
            arrayElement->_elementValue->value[0] = arraySel;
            arrayElement->_transactionState = kIOHIDTransactionStatePending;
            arrayElement->createReport(reportID, reportData, reportLength, 0);
        }
    }
    
    return true;
}

void IOHIDElementPrivate::setArrayElementValue(uint32_t index, uint32_t value)
{
    IOHIDElementPrivate *element;
    
    if (!_arrayItems || (index > _arrayItems->getCount())) {
        return;
    }
    
    element = (IOHIDElementPrivate *)(_arrayItems->getObject(index));
    
    if (!element)  {
        return;
    }
    
    // Bump the generation count.  An odd value tells us
    // that the information is incomplete and should not
    // be trusted.  An even value tells us that the value
    // is complete. 
    element->_elementValue->generation ++;
    
    element->_previousValue = element->_elementValue->value[0];
    element->_elementValue->value[0] = value;
    element->_elementValue->timestamp = _elementValue->timestamp;
    
    element->_elementValue->generation ++;
}

bool IOHIDElementPrivate::processArrayReport(uint8_t reportID,
                                             void *reportData,
                                             uint32_t reportBits,
                                             uint64_t timestamp)
{
    IOHIDElementPrivate *element = NULL;
    uint32_t arraySel	= 0;
    uint32_t iNewArray = 0;
    uint32_t iOldArray = 0;
    bool found = false;
    bool changed = false;
        
    // RY: Process the arry selector elements.  If any of their values
    // haven't changed, don't bother with any further processing.  
    if (_duplicateElements) {
        bool keyboard = found = (_usagePage == kHIDPage_KeyboardOrKeypad);
        for (iNewArray = 0; iNewArray < _reportCount; iNewArray++) {
            element = (IOHIDElementPrivate *)_duplicateElements->getObject(iNewArray);
            
            if (!element) {
                continue;
            }
            
            changed |= element->processReport(reportID, reportData, reportBits, timestamp, 0, 0);
            if (keyboard && (element->_elementValue->value[0] != kHIDUsage_KeyboardErrorRollOver)) {
                found = false;
            }
        }
        
        if (!changed) {
            return changed;
        } else if (keyboard) {
            setArrayElementValue(GetArrayItemIndex(kHIDUsage_KeyboardErrorRollOver), (found ? 1 : 0));
            
            if (found) {
                return false;
            }
        }
    }
                                    
    // Check the existing indexes against the originals
    for (iOldArray = 0; iOldArray < _reportCount; iOldArray++) {
        arraySel = _oldArraySelectors[iOldArray];
        
        found = false;
        
        for (iNewArray = 0; iNewArray < _reportCount; iNewArray++) {
            if (_duplicateElements) {
                element = (IOHIDElementPrivate *)_duplicateElements->getObject(iNewArray);
            } else {
                element = this;
            }
            
            if (!element) {
                continue;
            }
            
            if (arraySel == element->_elementValue->value[0]) {
                found = true;
                break;
            }
        }
        
        // The index is no longer present.  Set its value to 0.
        if (!found) {
            setArrayElementValue(GetArrayItemIndex(arraySel), 0);
        }
    }
    
    // Now add new indexes to _oldArraySelectors
    for (iNewArray = 0; iNewArray < _reportCount; iNewArray++) {
        if (_duplicateElements) {
            element = (IOHIDElementPrivate *)_duplicateElements->getObject(iNewArray);
        } else {
            element = this;
        }
        
        if (!element) {
            continue;
        }
        
        arraySel = element->_elementValue->value[0];
        
        found = false;
        
        for (iOldArray = 0; iOldArray < _reportCount; iOldArray++) {
            if (arraySel == _oldArraySelectors[iOldArray]) {
                found = true;
                break;
            }
        }
        
        // This is a new index.  Set its value to 1.
        if (!found) {
            setArrayElementValue(GetArrayItemIndex(arraySel), 1);
        }
    }
    
    // save the new array to _oldArraySelectors for future reference
    for (iOldArray = 0; iOldArray < _reportCount; iOldArray++) {
        if (_duplicateElements) {
            element = (IOHIDElementPrivate *)_duplicateElements->getObject(iOldArray);
        } else {
            element = this;
        }
        
        if (!element) {
            continue;
        }
        
        _oldArraySelectors[iOldArray] = element->_elementValue->value[0];
    }
    
    return changed;
}

OSData *IOHIDElementPrivate::getDataValue()
{   
    uint32_t byteSize = (UInt32)getCurrentByteSize();
    
#if defined(__LITTLE_ENDIAN__)
    if (_dataValue && _dataValue->getLength() == byteSize) {
        bcopy((const void *)_elementValue->value, (void *)_dataValue->getBytesNoCopy(), byteSize);
    } else {
        OSSafeReleaseNULL(_dataValue);
        _dataValue = OSData::withBytes((const void *)_elementValue->value, byteSize);
    }
#else
    uint32_t bitsToCopy = _currentReportSizeBits;
    if (!_dataValue || _dataValue->getLength() != byteSize) {
        uint8_t * bytes[byteSize];
        OSSafeReleaseNULL(_dataValue);
        _dataValue = OSData::withBytes(bytes, byteSize);
    }
    
    if (_dataValue) {
        bzero((void *)_dataValue->getBytesNoCopy(), byteSize);
        writeReportBits((const UInt32*)_elementValue->value, (uint8_t *)_dataValue->getBytesNoCopy(), bitsToCopy);
    }
#endif
    return _dataValue;
}

OSData *IOHIDElementPrivate::getDataValue(IOOptionBits options)
{
    if (options & kIOHIDValueOptionsUpdateElementValues) {
        IOReturn status = _owner->updateElementValues(&_cookie, 1);
        if (status) {
            HIDLogError("getDataValue failed (%lu):%x", (uintptr_t)_cookie, status);
        }
    }
    
    return getDataValue();
}

void IOHIDElementPrivate::setValue(uint32_t value)
{ 
    uint32_t previousValue = _elementValue->value[0];
    
    if (previousValue == value && !(_flags & kHIDDataRelativeBit)) {
        return;
    }
    
    _elementValue->value[0] = value;
}

void IOHIDElementPrivate::setDataValue(OSData *value)
{
    OSData *previousValue;
    
    if (!value) {
        return;
    }
    
    previousValue = getDataValue();
    
    setDataBits(value);
}

void IOHIDElementPrivate::setDataBits(OSData *value)
{
    uint32_t bitsToCopy;

    if (!value) {
        return;
    }
    
    bitsToCopy = min((value->getLength() << 3), (_reportBits * _reportCount));
	
    readReportBits((const UInt8*)value->getBytesNoCopy(), _elementValue->value, bitsToCopy);
}

IOByteCount IOHIDElementPrivate::getByteSize() const
{
    IOByteCount byteSize;
    uint32_t bitCount = _reportBits * _reportCount;

    byteSize = bitCount >> 3;
    byteSize += (bitCount % 8) ? 1 : 0;

    return byteSize;
}

IOByteCount IOHIDElementPrivate::getCurrentByteSize()
{
    IOByteCount byteSize;
    uint32_t bitCount = _currentReportSizeBits;

    byteSize = bitCount >> 3;
    byteSize += (bitCount % 8) ? 1 : 0;

    return byteSize;
}

void IOHIDElementPrivate::setCalibration(uint32_t min,
                                         uint32_t max,
                                         uint32_t saturationMin,
                                         uint32_t saturationMax,
                                         uint32_t deadZoneMin,
                                         uint32_t deadZoneMax,
                                         IOFixed granularity)
{
    _calibration.satMin = saturationMin;
    _calibration.satMax = saturationMax;
    _calibration.dzMin  = deadZoneMin;
    _calibration.dzMax  = deadZoneMax;
    _calibration.min    = min;
    _calibration.max    = max;
    _calibration.gran   = granularity;
}

uint32_t IOHIDElementPrivate::getScaledValue(IOHIDValueScaleType type)
{
    int64_t logicalValue    = (int32_t )getValue();
    int64_t logicalMin      = (int32_t )getLogicalMin();
    int64_t logicalMax      = (int32_t )getLogicalMax();
    int64_t logicalRange    = 0;
    int64_t scaledMin       = 0;
    int64_t scaledMax       = 0;
    int64_t scaledRange     = 0;
    int64_t returnValue     = 0;

    if (type == kIOHIDValueScaleTypeCalibrated) {
        
        if (_calibration.min != _calibration.max) {
            scaledMin = _calibration.min;
            scaledMax = _calibration.max;
        } else {
            scaledMin = -1;
            scaledMax = 1;
        }
        
        // check saturation first
        if (_calibration.satMin != _calibration.satMax) {
            if (logicalValue <= _calibration.satMin) {
                return (UInt32)scaledMin;
            }
            if (logicalValue >= _calibration.satMax) {
                return (UInt32)scaledMax;
            }
            
            logicalMin = _calibration.satMin;
            logicalMax = _calibration.satMax;
        }
        
        // now check the dead zone
        if (_calibration.dzMin != _calibration.dzMax) {
            int64_t scaledMid = scaledMin + ((scaledMax - scaledMin) / 2);
            if (logicalValue < _calibration.dzMin) {
                logicalMax = _calibration.dzMin;
                scaledMax = scaledMid;
            } else if (logicalValue > _calibration.dzMax) {
                logicalMin = _calibration.dzMax;
                scaledMin = scaledMid;
            } else {
                return (UInt32)scaledMid;
            }
        }
        
    } else { // kIOHIDValueScaleTypePhysical
        scaledMin = getPhysicalMin();
        scaledMax = getPhysicalMax();
    }
    
    logicalRange = logicalMax - logicalMin;
    scaledRange = scaledMax - scaledMin;
    
    if (logicalRange) {
        returnValue = ((logicalValue - logicalMin) * scaledRange / logicalRange) + scaledMin;
    } else {
        returnValue = logicalValue;
    }
    
    return (UInt32)returnValue;
}

IOFixed IOHIDElementPrivate::getScaledFixedValue(IOHIDValueScaleType type,
                                                 IOOptionBits options)
{
    if (options & kIOHIDValueOptionsUpdateElementValues) {
        IOReturn status = _owner->updateElementValues(&_cookie, 1);
        if (status) {
            HIDLogError("updateElementValues failed (%lu):%x", (uintptr_t)_cookie, status);
        }
    }
    
    return getScaledFixedValue(type);
}

IOFixed IOHIDElementPrivate::getScaledFixedValue(IOHIDValueScaleType type)
{
    int64_t logicalValue = (int32_t )getValue();
    int64_t logicalMin = (int32_t )getLogicalMin();
    int64_t logicalMax = (int32_t )getLogicalMax();
    int64_t logicalRange = 0;
    int64_t physicalMin = (int32_t )getPhysicalMin();
    int64_t physicalMax = (int32_t )getPhysicalMax();
    IOFixed returnValue = 0;
    
    if (type == kIOHIDValueScaleTypeCalibrated) {
        if (_calibration.min != _calibration.max) {
            physicalMin = _calibration.min;
            physicalMax = _calibration.max;
        } else {
            physicalMin = -1;
            physicalMax =  1;
        }
        
        // check saturation first
        if (_calibration.satMin != _calibration.satMax) {
            if (logicalValue <= _calibration.satMin) {
                return (IOFixed) (physicalMin << 16);
            }
            if (logicalValue >= _calibration.satMax) {
                return (IOFixed) (physicalMax << 16);
            }
            
            logicalMin = _calibration.satMin;
            logicalMax = _calibration.satMax;
        }
        
        // now check the dead zone
        if (_calibration.dzMin != _calibration.dzMax) {
            int64_t  physicalMid = physicalMin + ((physicalMax - physicalMin) / 2);
            if (logicalValue < _calibration.dzMin) {
                logicalMax = _calibration.dzMin;
                physicalMax = physicalMid;
            } else if (logicalValue > _calibration.dzMax) {
                logicalMin = _calibration.dzMax;
                physicalMin = physicalMid;
            } else {
                return (IOFixed)(physicalMid << 16);
            }
        }
    }
    
    int64_t physicalRange = physicalMax - physicalMin;
    
    logicalRange = logicalMax - logicalMin;
    
    if (!logicalRange) {
        logicalRange = 1;
    }
    
    int64_t num = (logicalValue - logicalMin) * physicalRange + physicalMin * logicalRange;
    int64_t denom = logicalRange;

    if (type == kIOHIDValueScaleTypeExponent) {
        int resExponent = _unitExponent & 0x0F;
        
        if (resExponent < 8) {
            for (int i = resExponent; i > 0; i--) {
                num *=  10;
            }
        } else {
            for (int i = 0x10 - resExponent; i > 0; i--) {
                denom *= 10;
            }
        }
    }
    
    returnValue = (IOFixed)((num << 16) / denom);
    
    return returnValue;
}

uint32_t IOHIDElementPrivate::getValue(IOOptionBits options)
{
    uint32_t newValue = 0;
    
    if ((_reportBits * _reportCount) <= 32) {
        if (options & kIOHIDValueOptionsUpdateElementValues) {
            IOReturn status = _owner->updateElementValues(&_cookie, 1);
            if (status) {
                HIDLogError("updateElementValues failed (%lu):%x", (uintptr_t)_cookie, status);
            }
        }
        
        newValue = (options & kIOHIDValueOptionsFlagPrevious) ? _previousValue : _elementValue->value[0];
        
        if (options & kIOHIDValueOptionsFlagRelativeSimple) {
            if ((_flags & kIOHIDElementFlagsWrapMask) && newValue == getLogicalMin() && _previousValue == getLogicalMax()) {
                newValue = 1;
            } else if ((_flags & kIOHIDElementFlagsWrapMask) &&  newValue == getLogicalMax() && _previousValue == getLogicalMin()) {
                newValue = -1;
            } else {
                newValue -= _previousValue;
            }
        }
    }
    
    return newValue;
}

IOReturn IOHIDElementPrivate::commit(IOHIDElementCommitDirection direction)
{
    IOReturn ret = kIOReturnError;
    
    if (direction == kIOHIDElementCommitDirectionIn) {
        ret = _owner->updateElementValues(&_cookie, 1);
    } else {
        ret = _owner->postElementValues(&_cookie, 1);
    }
    
    if (ret != kIOReturnSuccess) {
        HIDLogError("commit failed (%lu %d): 0x%x", (uintptr_t)_cookie, direction, ret);
    }
    
    return ret;
}

void IOHIDElementPrivate::setRollOverElementPtr(IOHIDElementPrivate **rollOverElementPtr)
{
    _rollOverElementPtr = rollOverElementPtr;
}

void IOHIDElementPrivate::setReportSize(uint32_t numberOfBits)
{
    _reportSize = numberOfBits;
}

bool IOHIDElementPrivate::shouldTickleActivity() const
{
    return _shouldTickleActivity;
}

IOHIDElementPrivate *IOHIDElementPrivate::getNextReportHandler() const
{
    return _nextReportHandler;
}

void IOHIDElementPrivate::setNextReportHandler(IOHIDElementPrivate *element)
{
    _nextReportHandler = element;
}

uint32_t IOHIDElementPrivate::getTransactionState() const
{
    return _transactionState;
}

void IOHIDElementPrivate::setTransactionState(uint32_t state)
{
    _transactionState = state;
}

IOHIDElementCookie IOHIDElementPrivate::getCookie(void)
{
    return _cookie;
}

IOHIDElementType IOHIDElementPrivate::getType(void)
{
    return _type;
}

IOHIDElementCollectionType IOHIDElementPrivate::getCollectionType(void)
{
    return _collectionType;
}

OSArray *IOHIDElementPrivate::getChildElements(void)
{
    return _childArray;
}

IOHIDElementPrivate *IOHIDElementPrivate::getParentElement(void)
{
    return _parent;
}

uint32_t IOHIDElementPrivate::getUsagePage(void)
{
    return _usagePage;
}

uint32_t IOHIDElementPrivate::getUsage(void)
{
    return _usage;
}

uint32_t IOHIDElementPrivate::getReportID(void)
{
    return _reportID;
}

uint32_t IOHIDElementPrivate::getFlags(void)
{
    return _flags;
}

uint32_t IOHIDElementPrivate::getReportSize(void)
{
    return _reportSize;
}

uint32_t IOHIDElementPrivate::getReportBits(void)
{
    return _reportBits;
}

uint32_t IOHIDElementPrivate::getReportCount(void)
{
    return _reportCount;
}

uint32_t IOHIDElementPrivate::getLogicalMin(void)
{
    return _logicalMin;
}

uint32_t IOHIDElementPrivate::getLogicalMax(void)
{
    return _logicalMax;
}

uint32_t IOHIDElementPrivate::getPhysicalMin(void)
{
    return _physicalMin;
}

uint32_t IOHIDElementPrivate::getPhysicalMax(void)
{
    return _physicalMax;
}

uint32_t IOHIDElementPrivate::getUnit(void)
{
    return _units;
}

uint32_t IOHIDElementPrivate::getUnitExponent(void)
{
    return _unitExponent;
}

uint64_t IOHIDElementPrivate::getTimeStamp(void)
{
    return _elementValue->timestamp;
    
}

uint32_t IOHIDElementPrivate::getValue(void)
{
    return getValue(0);
}

bool IOHIDElementPrivate::isVariableSize()
{
    return _variableSize & kIOHIDElementVariableSizeElement;
    
}

void IOHIDElementPrivate::setVariableSizeInfo(uint8_t variableSize)
{
    _variableSize = variableSize;
}

uint8_t IOHIDElementPrivate::getVariableSizeInfo()
{
    return _variableSize;
}

//char *IOHIDElementPrivate::copyDescription()
//{
//    size_t size = 1024;
//    char *description = (char *)malloc(size);
//    AbsoluteTime ts = { 0, 0 };
//    uint64_t timestamp = 0;
//
//    if (_elementValue) {
//        ts = getTimeStamp();
//    }
//
//    timestamp = ts.hi;
//    timestamp = (timestamp << 32) | ts.lo;
//
//    uint32_t val = _elementValue ? _elementValue->value[0] : 0;
//
//    snprintf(description, size, "%p: ts: %llu type: %d cook: %d uP: 0x%0x u: 0x%0x id: %d val: %d pval: %d",
//             this, timestamp, _type, _cookie, _usagePage, _usage, _reportID, val, _previousValue);
//
//    return description;
//}
