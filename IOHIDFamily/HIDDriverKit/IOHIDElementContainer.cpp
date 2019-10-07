//
//  IOHIDElementContainer.cpp
//  IOHIDFamily
//
//  Created by dekom on 9/12/18.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <os/overflow.h>
#include <AssertMacros.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>
#include "IOHIDDescriptorParser.h"
#include "IOHIDDescriptorParserPrivate.h"

#define super OSContainer

struct IOHIDElementContainer_IVars
{
    OSArray                 *elements;
    OSArray                 *flattenedElements;
    OSArray                 *flattenedCollections;
    OSArray                 *inputReportElements;
    void                    *elementValuesMem;
    uint32_t                elementValuesMemSize;
    
    IOHIDReportHandler      reportHandlers[kReportHandlerSlots];
    IOHIDElementPrivate     *rollOverElement;
    
    uint32_t                maxInputReportSize;
    uint32_t                maxOutputReportSize;
    uint32_t                maxFeatureReportSize;
    uint32_t                dataElementIndex;
    uint32_t                reportCount;
};

#define _elements                   ivars->elements
#define _flattenedElements          ivars->flattenedElements
#define _flattenedCollections       ivars->flattenedCollections
#define _inputReportElements        ivars->inputReportElements
#define _elementValuesMem           ivars->elementValuesMem
#define _elementValuesMemSize       ivars->elementValuesMemSize
#define _reportHandlers             ivars->reportHandlers
#define _rollOverElement            ivars->rollOverElement
#define _maxInputReportSize         ivars->maxInputReportSize
#define _maxOutputReportSize        ivars->maxOutputReportSize
#define _maxFeatureReportSize       ivars->maxFeatureReportSize
#define _dataElementIndex           ivars->dataElementIndex
#define _reportCount                ivars->reportCount

// Convert from a report ID to a dispatch table slot index.
//
#define GetReportHandlerSlot(id)    ((id) & (kReportHandlerSlots - 1))

#define GetHeadElement(slot, type)  _reportHandlers[slot].head[type]

#ifndef ULONG_MAX
#define ULONG_MAX       0xffffffffffffffffUL    /* max unsigned long */
#endif

bool IOHIDElementContainer::init(void *descriptor,
                                 IOByteCount length)
{
    OSStatus status = kIOReturnError;
    HIDPreparsedDataRef parseData;
    IOReturn ret = kIOReturnError;
    bool result = false;
    
    ret = super::init();
    require_action(ret, exit, HIDLogError("Init:%x", ret));
    
    ivars = IONewZero(IOHIDElementContainer_IVars, 1);
    require(ivars, exit);
    
    status = HIDOpenReportDescriptor(descriptor, length, &parseData, 0);
    require_noerr_action(status, exit, {
        HIDLogError("Failed to open report descriptor: 0x%x", (unsigned int)status);
    });
    
    ret = createElementHierarchy(parseData);
    require_noerr_action(ret, exit, {
        HIDLogError("Failed to create element hierarchy: 0x%x", ret);
    });
    
    getReportCountAndSizes(parseData);
    
    HIDCloseReportDescriptor(parseData);
    
    _flattenedElements = createFlattenedElements((IOHIDElementPrivate *)_elements->getObject(0));
    require(_flattenedElements, exit);
    
    _flattenedCollections = createFlattenedCollections((IOHIDElementPrivate *)_elements->getObject(0));
    require(_flattenedCollections, exit);
    
    result = true;
    
exit:
    return result;
}

IOHIDElementContainer *IOHIDElementContainer::withDescriptor(void *descriptor,
                                                             IOByteCount length)
{
    IOHIDElementContainer *me = NULL;
    
    me = OSTypeAlloc(IOHIDElementContainer);
    
    if (me && !me->init(descriptor, length)) {
        me->release();
        return NULL;
    }
    
    return me;
}

void IOHIDElementContainer::free()
{
    OSSafeReleaseNULL(_elements);
    OSSafeReleaseNULL(_flattenedElements);
    OSSafeReleaseNULL(_flattenedCollections);
    OSSafeReleaseNULL(_inputReportElements);
    
    if (_elementValuesMem) {
        IOFree(_elementValuesMem, _elementValuesMemSize);
    }
    
    IOSafeDeleteNULL(ivars, IOHIDElementContainer_IVars, 1);
    
    super::free();
}

IOReturn IOHIDElementContainer::createElementHierarchy(HIDPreparsedDataRef parseData)
{
    OSStatus status;
    HIDCapabilities caps;
    bool result = false;
    IOReturn ret = kIOReturnError;
    
    // Get a summary of device capabilities.
    status = HIDGetCapabilities(parseData, &caps);
    require_noerr_action(status, exit, {
        HIDLogError("createElementHierarchy HIDGetCapabilities failed: 0x%x", (unsigned int)status);
    });
    
    _maxInputReportSize = (UInt32)caps.inputReportByteLength;
    _maxOutputReportSize = (UInt32)caps.outputReportByteLength;
    _maxFeatureReportSize = (UInt32)caps.featureReportByteLength;
    
    _elements = OSArray::withCapacity(caps.numberCollectionNodes +
                                      caps.numberInputButtonCaps +
                                      caps.numberInputValueCaps +
                                      caps.numberOutputButtonCaps +
                                      caps.numberOutputValueCaps +
                                      caps.numberFeatureButtonCaps +
                                      caps.numberFeatureValueCaps +
                                      10);
    require(_elements, exit);
    
    // Add collections to the element array.
    result = createCollectionElements(parseData,
                                      _elements,
                                      caps.numberCollectionNodes);
    require_action(result, exit, {
        HIDLogError("createElementHierarchy createCollectionElements failed");
    });
    
    // Everything added to the element array from this point on
    // are "data" elements. We cache the starting index.
    
    _dataElementIndex = _elements->getCount();
    
    createNULLElements(parseData,
                       OSDynamicCast(IOHIDElementPrivate, _elements->getObject(0)));
    
    // Add input buttons to the element array.
    result = createButtonElements(parseData,
                                  _elements,
                                  kHIDInputReport,
                                  kIOHIDElementTypeInput_Button,
                                  caps.numberInputButtonCaps);
    require_action(result, exit, {
        HIDLogError("createElementHierarchy createButtonElements (input) failed");
    });
    
    // Add output buttons to the element array.
    result = createButtonElements(parseData,
                                  _elements,
                                  kHIDOutputReport,
                                  kIOHIDElementTypeOutput,
                                  caps.numberOutputButtonCaps);
    require_action(result, exit, {
        HIDLogError("createElementHierarchy createButtonElements (output) failed");
    });
    
    // Add feature buttons to the element array.
    result = createButtonElements(parseData,
                                  _elements,
                                  kHIDFeatureReport,
                                  kIOHIDElementTypeFeature,
                                  caps.numberFeatureButtonCaps);
    require_action(result, exit, {
        HIDLogError("createElementHierarchy createButtonElements (feature) failed");
    });
    
    // Add input values to the element array.
    result = createValueElements(parseData,
                                 _elements,
                                 kHIDInputReport,
                                 kIOHIDElementTypeInput_Misc,
                                 caps.numberInputValueCaps);
    require_action(result, exit, {
        HIDLogError("createElementHierarchy createValueElements (input) failed");
    });
    
    // Add output values to the element array.
    result = createValueElements(parseData,
                                 _elements,
                                 kHIDOutputReport,
                                 kIOHIDElementTypeOutput,
                                 caps.numberOutputValueCaps);
    require_action(result, exit, {
        HIDLogError("createElementHierarchy createValueElements (output) failed");
    });
    
    // Add feature values to the element array.
    result = createValueElements(parseData,
                                 _elements,
                                 kHIDFeatureReport,
                                 kIOHIDElementTypeFeature,
                                 caps.numberFeatureValueCaps);
    require_action(result, exit, {
        HIDLogError("createElementHierarchy createValueElements (feature) failed");
    });
    
    result = createReportHandlerElements(parseData);
    require_action(result, exit, {
        HIDLogError("createElementHierarchy createReportHandlerElements failed");
    });
    
    // Create a memory to store current element values.
    createElementValuesMemory();
    require_action(_elementValuesMem, exit, {
        HIDLogError("createElementHierarchy createElementValuesMemory failed");
    });
    
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

OSArray *IOHIDElementContainer::createFlattenedElements(IOHIDElementPrivate *collection)
{
    OSArray *result = NULL;
    OSArray *elements = NULL;
    
    require(collection, exit);
    
    elements = collection->getChildElements();
    require_quiet(elements, exit);
    
    result = OSArray::withCapacity(elements->getCount());
    require(result, exit);
    
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        OSArray *subElements = NULL;
        IOHIDElementPrivate *element = NULL;
        
        element = (IOHIDElementPrivate *)elements->getObject(i);
        if (!element) {
            continue;
        }
        
        result->setObject(element);
        
        subElements = createFlattenedElements(element);
        
        if (subElements) {
            result->merge(subElements);
            subElements->release();
        }
    }
    
exit:
    return result;
}

OSArray *IOHIDElementContainer::createFlattenedCollections(IOHIDElementPrivate *root)
{
    OSArray *result = NULL;
    OSArray *elements = NULL;
    
    require(root, exit);
    
    elements = root->getChildElements();
    require(elements, exit);
    
    result = OSArray::withCapacity(elements->getCount());
    require(result, exit);
    
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        OSArray *subContainer = NULL;
        IOHIDElementPrivate *element = NULL;

        element = OSDynamicCast(IOHIDElementPrivate, elements->getObject(i));
        if (!element ||
            element->getType() != kIOHIDElementTypeCollection ||
            element->getCollectionType() != kIOHIDElementCollectionTypeApplication) {
            continue;
        }

        subContainer = createFlattenedElements(element);

        if (subContainer) {
            subContainer->setObject(0, element);
            result->setObject(subContainer);
            subContainer->release();
        }
        else {
            subContainer = OSArray::withObjects((const OSObject **)&element, 1, 1);
            if (!subContainer) {
                continue;
            }
            result->setObject(subContainer);
            subContainer->release();
        }
    }
    
exit:
    return result;
}

bool IOHIDElementContainer::createCollectionElements(HIDPreparsedDataRef parseData,
                                                     OSArray *array,
                                                     uint32_t maxCount)
{
    OSStatus status;
    HIDCollectionExtendedNodePtr collections = NULL;
    uint32_t count = maxCount;
    bool result = false;
    uint32_t index;
    uint32_t allocSize = 0;
    
    require(!os_mul_overflow(maxCount, sizeof(HIDButtonCapabilities), &allocSize), exit);
    
    collections = (HIDCollectionExtendedNodePtr)IOMalloc(allocSize);
    require(collections, exit);
    
    status = HIDGetCollectionExtendedNodes(collections, &count, parseData);
    require_noerr(status, exit);
    
    // Create an IOHIDElementPrivate for each collection.
    for (index = 0; index < count; index++) {
        IOHIDElementPrivate *element;
        
        element = IOHIDElementPrivate::collectionElement(this,
                                                         kIOHIDElementTypeCollection,
                                                         &collections[index],
                                                         0);
        require(element, exit);
        
        element->release();
    }
    
    // Create linkage for the collection hierarchy.
    // Starts at 1 to skip the root (virtual) collection.
    for (index = 1; index < count; index++) {
        IOHIDElementPrivate *child = NULL;
        IOHIDElementPrivate *parent = NULL;
        uint32_t parentIdx = collections[index].parent;
        
        child = (IOHIDElementPrivate *)array->getObject(index);
        parent = (IOHIDElementPrivate *)array->getObject(parentIdx);
        
        require(parent && parent->addChildElement(child, false), exit);
    }
    
    result = true;
    
exit:
    if (collections) {
        IOFree(collections, allocSize);
    }
    
    return result;
}

void IOHIDElementContainer::createNULLElements(HIDPreparsedDataRef parseData,
                                               IOHIDElementPrivate *parent)
{
    HIDPreparsedDataPtr data = (HIDPreparsedDataPtr)parseData;
    HIDReportSizes *report = data->reports;
    IOHIDElementPrivate *element = 0;
    
    for (uint32_t num = 0; num < data->reportCount; num++, report++) {
        element = IOHIDElementPrivate::nullElement(this,
                                                   report->reportID,
                                                   parent);
        
        if (!element) {
            continue;
        }
        
        element->release();
    }
}

bool IOHIDElementContainer::createButtonElements(HIDPreparsedDataRef parseData,
                                                 OSArray *array,
                                                 uint32_t hidReportType,
                                                 IOHIDElementType elementType,
                                                 uint32_t maxCount)
{
    OSStatus status;
    HIDButtonCapabilitiesPtr buttons = NULL;
    uint32_t count = maxCount;
    bool result = false;
    uint32_t allocSize = 0;
    
    require_action_quiet(maxCount, exit, result = true);
    
    require(!os_mul_overflow(maxCount, sizeof(HIDButtonCapabilities), &allocSize), exit);
    
    buttons = (HIDButtonCapabilitiesPtr)IOMalloc(allocSize);
    require(buttons, exit);
    
    status = HIDGetButtonCapabilities(hidReportType, buttons, &count, parseData);
    require_noerr(status, exit);
    
    for (uint32_t i = 0; i < count; i++) {
        IOHIDElementPrivate *element = NULL;
        IOHIDElementPrivate *parent = NULL;
        uint32_t idx = buttons[i].collection;
        
        parent = (IOHIDElementPrivate *)array->getObject(idx);
        
        element = IOHIDElementPrivate::buttonElement(this,
                                                     elementType,
                                                     &buttons[i],
                                                     parent);
        require(element, exit);
        element->release();
    }
    
    result = true;
    
exit:
    if (buttons) {
        IOFree(buttons, allocSize);
    }
    
    return result;
}

bool IOHIDElementContainer::createValueElements(HIDPreparsedDataRef parseData,
                                                OSArray *array,
                                                uint32_t hidReportType,
                                                IOHIDElementType elementType,
                                                uint32_t maxCount)
{
    OSStatus status;
    HIDValueCapabilitiesPtr values = NULL;
    uint32_t count = maxCount;
    bool result = false;
    uint32_t allocSize = 0;
    
    require_action_quiet(maxCount, exit, result = true);
    
    require(!os_mul_overflow(maxCount, sizeof(HIDValueCapabilities), &allocSize), exit);
    
    values = (HIDValueCapabilitiesPtr)IOMalloc(allocSize);
    require(values, exit);
    
    status = HIDGetValueCapabilities(hidReportType, values, &count, parseData);
    require_noerr(status, exit);
    
    for (uint32_t i = 0; i < count; i++) {
        IOHIDElementPrivate *element = NULL;
        IOHIDElementPrivate *parent = NULL;
        uint32_t idx = values[i].collection;
        
        parent = (IOHIDElementPrivate *)array->getObject(idx);
        
        element = IOHIDElementPrivate::valueElement(this,
                                                    elementType,
                                                    &values[i],
                                                    parent);
        require(element, exit);
        element->release();
    }
    
    result = true;
    
exit:
    if (values) {
        IOFree(values, allocSize);
    }
    
    return result;
}

bool IOHIDElementContainer::createReportHandlerElements(HIDPreparsedDataRef parseData)
{
    HIDPreparsedDataPtr data = (HIDPreparsedDataPtr)parseData;
    HIDReportSizes *report = data->reports;
    bool result = false;
    
    _inputReportElements = OSArray::withCapacity(data->reportCount);
    require(_inputReportElements, exit);
    
    for (uint32_t i = 0; i < data->reportCount; i++, report++)
    {
        IOHIDElementPrivate *element = NULL;
        
        element = IOHIDElementPrivate::reportHandlerElement(this,
                                                            kIOHIDElementTypeInput_Misc,
                                                            report->reportID,
                                                            report->inputBitCount);
        
        if (!element) {
            continue;
        }
        
        _inputReportElements->setObject(element);
        element->release();
    }
    
    result = true;
    
exit:
    return result;
}

bool IOHIDElementContainer::registerElement(IOHIDElementPrivate *element,
                                            IOHIDElementCookie *cookie)
{
    IOHIDReportType reportType;
    uint32_t index = _elements->getCount();
    bool result = false;
    
    // Add the element to the elements array.
    require(_elements->setObject(index, element), exit);
    
    // If the element can contribute to an Input, Output, or Feature
    // report, then add it to the chain of report handlers.
    if (element->getReportType(&reportType)) {
        IOHIDReportHandler *reportHandler;
        uint32_t slot;
        
        slot = GetReportHandlerSlot(element->getReportID());
        
        reportHandler = &_reportHandlers[slot];
        
        if (reportHandler->head[reportType]) {
            element->setNextReportHandler(reportHandler->head[reportType]);
        }
        
        reportHandler->head[reportType] = element;
        
        if (element->getUsagePage() == kHIDPage_KeyboardOrKeypad) {
            uint32_t usage = element->getUsage();
            
            if (usage == kHIDUsage_KeyboardErrorRollOver) {
                _rollOverElement = element;
            }
            
            if (usage >= kHIDUsage_KeyboardLeftControl &&
                usage <= kHIDUsage_KeyboardRightGUI) {
                element->setRollOverElementPtr(&(_rollOverElement));
            }
        }
    }
    
    // The cookie returned is simply an index to the element in the
    // elements array. We may decide to obfuscate it later on.
    *cookie = (IOHIDElementCookie)index;
    result = true;
    
exit:
    return result;
}

void IOHIDElementContainer::createElementValuesMemory()
{
    void *memory = NULL;
    IOHIDElementPrivate *element = NULL;
    uint32_t capacity = 0;
    uint8_t *beginning = NULL;
    uint8_t *buffer = NULL;
    
    // Discover the amount of memory required to publish the
    // element values for all "data" elements.
    
    for (uint32_t slot = 0; slot < kReportHandlerSlots; slot++) {
        for (uint32_t type = 0; type < kIOHIDReportTypeCount; type++) {
            element = GetHeadElement(slot, type);
            while (element) {
                uint32_t remaining = (UInt32)ULONG_MAX - capacity;
                
                require(element->getElementValueSize() <= remaining, exit);
                
                capacity += element->getElementValueSize();
                element = element->getNextReportHandler();
            }
        }
    }
    
    memory = IOMalloc(capacity);
    require(memory, exit);
    
    // Now assign the update memory area for each report element.
    beginning = buffer = (uint8_t *)memory;
    
    for (uint32_t slot = 0; slot < kReportHandlerSlots; slot++) {
        for (uint32_t type = 0; type < kIOHIDReportTypeCount; type++) {
            element = GetHeadElement(slot, type);
            while (element) {
                element->setMemoryForElementValue((IOVirtualAddress)buffer,
                                                  (void *)(buffer - beginning));
                
                buffer += element->getElementValueSize();
                element = element->getNextReportHandler();
            }
        }
    }
    
    _elementValuesMem = memory;
    _elementValuesMemSize = capacity;
    
exit:
    return;
}

void IOHIDElementContainer::getReportCountAndSizes(HIDPreparsedDataRef parseData)
{
    HIDPreparsedDataPtr data = (HIDPreparsedDataPtr)parseData;
    HIDReportSizes *report = data->reports;
    
    _reportCount = data->reportCount;
    
    for (uint32_t i = 0; i < data->reportCount; i++, report++) {
        setReportSize(report->reportID,
                      kIOHIDReportTypeInput,
                      report->inputBitCount);
        
        setReportSize(report->reportID,
                      kIOHIDReportTypeOutput,
                      report->outputBitCount);
        
        setReportSize(report->reportID,
                      kIOHIDReportTypeFeature,
                      report->featureBitCount);
    }
}

void IOHIDElementContainer::setReportSize(uint8_t reportID,
                                          IOHIDReportType reportType,
                                          uint32_t numberOfBits)
{
    IOHIDElementPrivate *element = NULL;
    uint8_t variableReportSizeInfo = 0;
    
    if (reportType == kIOHIDReportTypeInput ||
        reportType == kIOHIDReportTypeFeature) {
        for (unsigned int i = 0; i < _elements->getCount(); i++) {
            element = (IOHIDElementPrivate *)_elements->getObject(i);
            IOHIDReportType elementReportType;
            
            if (element->getReportID() != reportID ||
                !element->getReportType(&elementReportType) ||
                elementReportType != reportType ||
                !element->isVariableSize()) {
                continue;
            }
            
            variableReportSizeInfo |= kIOHIDElementVariableSizeReport;
            
            if (reportType != kIOHIDReportTypeInput) {
                break;
            }
            
            for (unsigned int j = 0; j < _inputReportElements->getCount(); j++) {
                element = (IOHIDElementPrivate *)_inputReportElements->getObject(j);
                if (element->getReportID() == reportID) {
                    element->setVariableSizeInfo(kIOHIDElementVariableSizeElement |
                                                 kIOHIDElementVariableSizeReport);
                    break;
                }
            }
            
            break;
        }
    }
    
    element = GetHeadElement(GetReportHandlerSlot(reportID), reportType);
    
    while (element) {
        if (element->getReportID() == reportID) {
            element->setVariableSizeInfo(element->getVariableSizeInfo() |
                                         variableReportSizeInfo);
            element->setReportSize(numberOfBits);
            break;
        }
        
        element = element->getNextReportHandler();
    }
}

IOReturn IOHIDElementContainer::updateElementValues(IOHIDElementCookie *cookies __unused,
                                                    uint32_t cookieCount __unused)
{
    return kIOReturnUnsupported;
}

IOReturn IOHIDElementContainer::postElementValues(IOHIDElementCookie *cookies __unused,
                                                  uint32_t cookieCount __unused)
{
    return kIOReturnUnsupported;
}

void IOHIDElementContainer::createReport(IOHIDReportType reportType,
                                         uint8_t reportID,
                                         IOBufferMemoryDescriptor *report)
{
    IOHIDElementPrivate *element = NULL;
    uint8_t *reportData = NULL;
    uint32_t reportLength = 0;
    
    // Start at the head element and iterate through
    element = GetHeadElement(GetReportHandlerSlot(reportID), reportType);
    
    // TODO:
    //reportData = (uint8_t *)report->getBytesNoCopy();
    
    while (element) {
        element->createReport(reportID, reportData, &reportLength, &element);
        
        // If the reportLength was set, then this is
        // the head element for this report
        if (reportLength) {
            // TODO:
            //report->setLength(reportLength);
            reportLength = 0;
        }
    }
}

bool IOHIDElementContainer::processReport(IOHIDReportType reportType,
                                          uint8_t reportID,
                                          void *reportData,
                                          uint32_t reportLength,
                                          uint64_t timestamp,
                                          bool *shouldTickle,
                                          IOOptionBits options)
{
    bool changed = false;
    IOHIDElementPrivate *element = NULL;
    
    // Get the first element in the report handler chain.
    element = GetHeadElement(GetReportHandlerSlot(reportID), reportType);
    
    while (element) {
        if (shouldTickle) {
            *shouldTickle |= element->shouldTickleActivity();
        }
        
        changed |= element->processReport(reportID,
                                          reportData,
                                          (UInt32)reportLength << 3,
                                          timestamp,
                                          &element,
                                          options);
    }
    
    return changed;
}

OSArray *IOHIDElementContainer::getElements()
{
    return _elements;
}

OSArray *IOHIDElementContainer::getFlattenedElements()
{
    return _flattenedElements;
}

OSArray *IOHIDElementContainer::getFlattenedCollections()
{
    return _flattenedCollections;
}

OSArray *IOHIDElementContainer::getInputReportElements()
{
    return _inputReportElements;
}

uint32_t IOHIDElementContainer::getMaxInputReportSize()
{
    return _maxInputReportSize;
}

uint32_t IOHIDElementContainer::getMaxOutputReportSize()
{
    return _maxOutputReportSize;
}

uint32_t IOHIDElementContainer::getMaxFeatureReportSize()
{
    return _maxFeatureReportSize;
}

uint32_t IOHIDElementContainer::getDataElementIndex()
{
    return _dataElementIndex;
}

uint32_t IOHIDElementContainer::getReportCount()
{
    return _reportCount;
}
