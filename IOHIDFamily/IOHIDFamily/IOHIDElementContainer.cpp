//
//  IOHIDElementContainer.cpp
//  IOHIDFamily
//
//  Created by dekom on 9/12/18.
//

#include "IOHIDElementContainer.h"
#include <AssertMacros.h>
#include "IOHIDDescriptorParserPrivate.h"
#include "IOHIDDebug.h"
#include "IOHIDElementPrivate.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include "IOHIDDevice.h"

#define super OSObject
OSDefineMetaClassAndStructors(IOHIDElementContainer, OSObject)

#define _elements                   _reserved->elements
#define _flattenedElements          _reserved->flattenedElements
#define _flattenedCollections       _reserved->flattenedCollections
#define _inputReportElements        _reserved->inputReportElements
#define _elementValuesDescriptor    _reserved->elementValuesDescriptor
#define _reportHandlers             _reserved->reportHandlers
#define _rollOverElement            _reserved->rollOverElement
#define _maxInputReportSize         _reserved->maxInputReportSize
#define _maxOutputReportSize        _reserved->maxOutputReportSize
#define _maxFeatureReportSize       _reserved->maxFeatureReportSize
#define _dataElementIndex           _reserved->dataElementIndex
#define _reportCount                _reserved->reportCount

// Convert from a report ID to a dispatch table slot index.
//
#define GetReportHandlerSlot(id)    ((id) & (kReportHandlerSlots - 1))

#define GetHeadElement(slot, type)  _reportHandlers[slot].head[type]

#define DescriptorLog(fmt, ...) HIDLog("[ElementContainer] " fmt "\n", ##__VA_ARGS__);

#define kMaxStackString 256

static void printDescriptor(void *descriptor,
                            IOByteCount length)
{
    IOByteCount descLen;
    if (os_mul_and_add_overflow(length, 2, 1, &descLen)) {
        HIDLogError("[ElementContainer] Unable to print descriptor. Descriptor length is invalid.\n");
        return;
    }

    char *str = reinterpret_cast<char*>(IOMalloc(descLen));
    if (str) {
        char *target = str;
        for (unsigned int i = 0; i < length; i++) {
            uint8_t byte = ((uint8_t *)descriptor)[i];
            target += snprintf(target, descLen-i*2, "%02x", byte);
        }
        DescriptorLog("Descriptor: %s", str);
        IOFree(str, descLen);
    } else {
        HIDLogError("[ElementContainer] Unable to print descriptor. Memory allocation failed.\n");
    }
}

bool IOHIDElementContainer::init(void *descriptor,
                                 IOByteCount length)
{
    OSStatus status = kIOReturnError;
    HIDPreparsedDataRef parseData;
    IOReturn ret = kIOReturnError;
    bool result = false;
    
    require(super::init(), exit);
    
    _reserved = IONew(ExpansionData, 1);
    require(_reserved, exit);
    
    bzero(_reserved, sizeof(ExpansionData));
    
    status = HIDOpenReportDescriptor(descriptor, length, &parseData, 0);
    require_noerr_action(status, exit, {
        // This usually indicates a malformed descriptor was passed in.
        DescriptorLog("Failed to open report descriptor: 0x%x", (unsigned int)status);
        printDescriptor(descriptor, length);
    });
    
    ret = createElementHierarchy(parseData);
    require_noerr_action(ret, exit, {
        DescriptorLog("Failed to create element hierarchy: 0x%x", ret);
    });
    
    getReportCountAndSizes(parseData);
    
    HIDCloseReportDescriptor(parseData);
    
    _flattenedElements = createFlattenedElements((IOHIDElement *)_elements->getObject(0));
    require(_flattenedElements, exit);
    
    _flattenedCollections = createFlattenedCollections((IOHIDElement *)_elements->getObject(0));
    require(_flattenedCollections, exit);
    
    result = true;
    
exit:
    return result;
}

IOHIDElementContainer *IOHIDElementContainer::withDescriptor(void *descriptor,
                                                             IOByteCount length)
{
    IOHIDElementContainer *me = new IOHIDElementContainer;
    
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
    OSSafeReleaseNULL(_elementValuesDescriptor);
    
    if (_reserved) {
        IODelete(_reserved, ExpansionData, 1);
    }
    
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
        DescriptorLog("createElementHierarchy HIDGetCapabilities failed: 0x%x", (unsigned int)status);
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
    
    _elements->setCapacityIncrement(10);
    
    // Add collections to the element array.
    result = createCollectionElements(parseData,
                                      _elements,
                                      caps.numberCollectionNodes);
    require_action(result, exit, {
        DescriptorLog("createElementHierarchy createCollectionElements failed");
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
        DescriptorLog("createElementHierarchy createButtonElements (input) failed");
    });
    
    // Add output buttons to the element array.
    result = createButtonElements(parseData,
                                  _elements,
                                  kHIDOutputReport,
                                  kIOHIDElementTypeOutput,
                                  caps.numberOutputButtonCaps);
    require_action(result, exit, {
        DescriptorLog("createElementHierarchy createButtonElements (output) failed");
    });
    
    // Add feature buttons to the element array.
    result = createButtonElements(parseData,
                                  _elements,
                                  kHIDFeatureReport,
                                  kIOHIDElementTypeFeature,
                                  caps.numberFeatureButtonCaps);
    require_action(result, exit, {
        DescriptorLog("createElementHierarchy createButtonElements (feature) failed");
    });
    
    // Add input values to the element array.
    result = createValueElements(parseData,
                                 _elements,
                                 kHIDInputReport,
                                 kIOHIDElementTypeInput_Misc,
                                 caps.numberInputValueCaps);
    require_action(result, exit, {
        DescriptorLog("createElementHierarchy createValueElements (input) failed");
    });
    
    // Add output values to the element array.
    result = createValueElements(parseData,
                                 _elements,
                                 kHIDOutputReport,
                                 kIOHIDElementTypeOutput,
                                 caps.numberOutputValueCaps);
    require_action(result, exit, {
        DescriptorLog("createElementHierarchy createValueElements (output) failed");
    });
    
    // Add feature values to the element array.
    result = createValueElements(parseData,
                                 _elements,
                                 kHIDFeatureReport,
                                 kIOHIDElementTypeFeature,
                                 caps.numberFeatureValueCaps);
    require_action(result, exit, {
        DescriptorLog("createElementHierarchy createValueElements (feature) failed");
    });
    
    result = createReportHandlerElements(parseData);
    require_action(result, exit, {
        DescriptorLog("createElementHierarchy createReportHandlerElements failed");
    });
    
    // Create a memory to store current element values.
    _elementValuesDescriptor = createElementValuesMemory();
    require_action(_elementValuesDescriptor, exit, {
        DescriptorLog("createElementHierarchy createElementValuesMemory failed");
    });
    
    ret = kIOReturnSuccess;
    
exit:
    return ret;
}

OSArray *IOHIDElementContainer::createFlattenedElements(IOHIDElement *collection)
{
    OSArray *result = NULL;
    OSArray *elements = NULL;
    
    require(collection, exit);
    
    elements = collection->getChildElements();
    require(elements, exit);
    
    result = OSArray::withCapacity(elements->getCount());
    require(result, exit);
    
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        OSArray *subElements = NULL;
        IOHIDElement *element = NULL;
        
        element = OSDynamicCast(IOHIDElement, elements->getObject(i));
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

OSArray *IOHIDElementContainer::createFlattenedCollections(IOHIDElement *root)
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
                                                     UInt32 maxCount)
{
    OSStatus status;
    HIDCollectionExtendedNodePtr collections = NULL;
    UInt32 count = maxCount;
    bool result = false;
    UInt32 index;
    UInt32 allocSize = 0;
    
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
                                                         &collections[index]);
        require(element, exit);
        
        element->release();
    }
    
    // Create linkage for the collection hierarchy.
    // Starts at 1 to skip the root (virtual) collection.
    for (index = 1; index < count; index++) {
        IOHIDElementPrivate *child = NULL;
        IOHIDElementPrivate *parent = NULL;
        UInt32 parentIdx = collections[index].parent;
        
        child = OSDynamicCast(IOHIDElementPrivate, array->getObject(index));
        parent = OSDynamicCast(IOHIDElementPrivate, array->getObject(parentIdx));
        
        require(parent && parent->addChildElement(child), exit);
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
    
    for (UInt32 num = 0; num < data->reportCount; num++, report++) {
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
                                                 UInt32 hidReportType,
                                                 IOHIDElementType elementType,
                                                 UInt32 maxCount)
{
    OSStatus status;
    HIDButtonCapabilitiesPtr buttons = NULL;
    UInt32 count = maxCount;
    bool result = false;
    UInt32 allocSize = 0;
    
    require_action(maxCount, exit, result = true);
    
    require(!os_mul_overflow(maxCount, sizeof(HIDButtonCapabilities), &allocSize), exit);
    
    buttons = (HIDButtonCapabilitiesPtr)IOMalloc(allocSize);
    require(buttons, exit);
    
    status = HIDGetButtonCapabilities(hidReportType, buttons, &count, parseData);
    require_noerr(status, exit);
    
    for (UInt32 i = 0; i < count; i++) {
        IOHIDElementPrivate *element = NULL;
        IOHIDElementPrivate *parent = NULL;
        UInt32 idx = buttons[i].collection;
        
        parent = OSDynamicCast(IOHIDElementPrivate, array->getObject(idx));
        
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
                                                UInt32 hidReportType,
                                                IOHIDElementType elementType,
                                                UInt32 maxCount)
{
    OSStatus status;
    HIDValueCapabilitiesPtr values = NULL;
    UInt32 count = maxCount;
    bool result = false;
    UInt32 allocSize = 0;
    
    require_action(maxCount, exit, result = true);
    
    require(!os_mul_overflow(maxCount, sizeof(HIDValueCapabilities), &allocSize), exit);
    
    values = (HIDValueCapabilitiesPtr)IOMalloc(allocSize);
    require(values, exit);
    
    status = HIDGetValueCapabilities(hidReportType, values, &count, parseData);
    require_noerr(status, exit);
    
    for (UInt32 i = 0; i < count; i++) {
        IOHIDElementPrivate *element = NULL;
        IOHIDElementPrivate *parent = NULL;
        UInt32 idx = values[i].collection;
        
        parent = OSDynamicCast(IOHIDElementPrivate, array->getObject(idx));
        
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
    
    for (UInt32 i = 0; i < data->reportCount; i++, report++)
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
    UInt32 index = _elements->getCount();
    bool result = false;
    
    // Add the element to the elements array.
    require(_elements->setObject(index, element), exit);
    
    // If the element can contribute to an Input, Output, or Feature
    // report, then add it to the chain of report handlers.
    if (element->getReportType(&reportType)) {
        IOHIDReportHandler *reportHandler;
        UInt32 slot;
        
        slot = GetReportHandlerSlot(element->getReportID());
        
        reportHandler = &_reportHandlers[slot];
        
        if (reportHandler->head[reportType]) {
            element->setNextReportHandler(reportHandler->head[reportType]);
        }
        
        reportHandler->head[reportType] = element;
        
        if (element->getUsagePage() == kHIDPage_KeyboardOrKeypad) {
            UInt32 usage = element->getUsage();
            
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

IOBufferMemoryDescriptor *IOHIDElementContainer::createElementValuesMemory()
{
    IOBufferMemoryDescriptor *descriptor = NULL;
    IOHIDElementPrivate *element = NULL;
    UInt32 capacity = 0;
    UInt8 *beginning = NULL;
    UInt8 *buffer = NULL;
    
    // Discover the amount of memory required to publish the
    // element values for all "data" elements.
    
    for (UInt32 slot = 0; slot < kReportHandlerSlots; slot++) {
        for (UInt32 type = 0; type < kIOHIDReportTypeCount; type++) {
            element = GetHeadElement(slot, type);
            while (element) {
                UInt32 remaining = (UInt32)ULONG_MAX - capacity;
                
                require(element->getElementValueSize() <= remaining, exit);
                
                capacity += element->getElementValueSize();
                element = element->getNextReportHandler();
            }
        }
    }
    
    DescriptorLog("Element value capacity %ld", (long)capacity);
    
    descriptor = IOBufferMemoryDescriptor::withOptions(kIOMemoryUnshared,
                                                       capacity);
    require(descriptor, exit);
    
    // Now assign the update memory area for each report element.
    beginning = buffer = (UInt8 *)descriptor->getBytesNoCopy();
    
    for (UInt32 slot = 0; slot < kReportHandlerSlots; slot++) {
        for (UInt32 type = 0; type < kIOHIDReportTypeCount; type++) {
            element = GetHeadElement(slot, type);
            while (element) {
                element->setMemoryForElementValue((IOVirtualAddress)buffer,
                                                  (void *)(buffer - beginning));
                
                buffer += element->getElementValueSize();
                element = element->getNextReportHandler();
            }
        }
    }
    
exit:
    return descriptor;
}

void IOHIDElementContainer::getReportCountAndSizes(HIDPreparsedDataRef parseData)
{
    HIDPreparsedDataPtr data = (HIDPreparsedDataPtr)parseData;
    HIDReportSizes *report = data->reports;
    
    _reportCount = data->reportCount;
    
    DescriptorLog("Report count: %ld", (long)_reportCount);
    
    for (UInt32 i = 0; i < data->reportCount; i++, report++) {
        
        DescriptorLog("Report ID: %ld input:%ld output:%ld feature:%ld",
                      (long)report->reportID,
                      (long)report->inputBitCount,
                      (long)report->outputBitCount,
                      (long)report->featureBitCount);
        
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

void IOHIDElementContainer::setReportSize(UInt8 reportID,
                                          IOHIDReportType reportType,
                                          UInt32 numberOfBits)
{
    IOHIDElementPrivate *element = NULL;
    UInt8 variableReportSizeInfo = 0;
    
    if (reportType == kIOHIDReportTypeInput ||
        reportType == kIOHIDReportTypeFeature) {
        for (unsigned int i = 0; i < _elements->getCount(); i++) {
            element = OSDynamicCast(IOHIDElementPrivate,
                                    _elements->getObject(i));
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
                element = OSDynamicCast(IOHIDElementPrivate,
                                        _inputReportElements->getObject(j));
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
                                                    UInt32 cookieCount __unused)
{
    return kIOReturnUnsupported;
}

IOReturn IOHIDElementContainer::postElementValues(IOHIDElementCookie *cookies __unused,
                                                  UInt32 cookieCount __unused)
{
    return kIOReturnUnsupported;
}

void IOHIDElementContainer::createReport(IOHIDReportType reportType,
                                         UInt8 reportID,
                                         IOBufferMemoryDescriptor *report)
{
    IOHIDElementPrivate *element = NULL;
    UInt8 *reportData = NULL;
    UInt32 reportLength = 0;
    
    // Start at the head element and iterate through
    element = GetHeadElement(GetReportHandlerSlot(reportID), reportType);
    
    reportData = (UInt8 *)report->getBytesNoCopy();
    
    while (element) {
        element->createReport(reportID, reportData, &reportLength, &element);
        
        // If the reportLength was set, then this is
        // the head element for this report
        if (reportLength) {
            report->setLength(reportLength);
            reportLength = 0;
        }
    }
}

bool IOHIDElementContainer::processReport(IOHIDReportType reportType,
                                          UInt8 reportID,
                                          void *reportData,
                                          UInt32 reportLength,
                                          AbsoluteTime timestamp,
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
                                          &timestamp,
                                          &element,
                                          options);
    }
    
    return changed;
}
