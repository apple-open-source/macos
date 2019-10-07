//
//  IOHIDElementContainer.h
//  IOHIDFamily
//
//  Created by dekom on 9/12/18.
//

#ifndef IOHIDElementContainer_h
#define IOHIDElementContainer_h

#include <libkern/c++/OSObject.h>
#include <libkern/c++/OSArray.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include <IOKit/hid/IOHIDKeys.h>

class IOHIDElementPrivate;
class IOHIDElement;

// Number of slots in the report handler dispatch table.
//
#define kReportHandlerSlots    8

// Describes the handler(s) at each report dispatch table slot.
//
struct IOHIDReportHandler
{
    IOHIDElementPrivate *head[kIOHIDReportTypeCount];
};

class IOHIDElementContainer : public OSObject
{
    OSDeclareDefaultStructors(IOHIDElementContainer)
    
    friend class IOHIDElementPrivate;
    
private:
    struct ExpansionData {
        OSArray                     *elements;
        OSArray                     *flattenedElements;
        OSArray                     *flattenedCollections;
        OSArray                     *inputReportElements;
        IOBufferMemoryDescriptor    *elementValuesDescriptor;
        
        IOHIDReportHandler          reportHandlers[kReportHandlerSlots];
        IOHIDElementPrivate         *rollOverElement;
        
        UInt32                      maxInputReportSize;
        UInt32                      maxOutputReportSize;
        UInt32                      maxFeatureReportSize;
        UInt32                      dataElementIndex;
        UInt32                      reportCount;
    };
    
    ExpansionData                   *_reserved;
    
    IOReturn createElementHierarchy(HIDPreparsedDataRef parseData);
    OSArray *createFlattenedElements(IOHIDElement *collection);
    OSArray *createFlattenedCollections(IOHIDElement *root);
    
    // HID report descriptor parsing support.
    
    bool createCollectionElements(HIDPreparsedDataRef parseData,
                                  OSArray *array,
                                  UInt32 maxCount);
    
    void createNULLElements(HIDPreparsedDataRef parseData,
                            IOHIDElementPrivate *parent);
    
    bool createValueElements(HIDPreparsedDataRef parseData,
                             OSArray *array,
                             UInt32 hidReportType,
                             IOHIDElementType elementType,
                             UInt32 maxCount);
    
    bool createButtonElements(HIDPreparsedDataRef parseData,
                              OSArray *array,
                              UInt32 hidReportType,
                              IOHIDElementType elementType,
                              UInt32 maxCount);
    
    bool createReportHandlerElements(HIDPreparsedDataRef parseData);
    
    IOBufferMemoryDescriptor *createElementValuesMemory();
    
    void getReportCountAndSizes(HIDPreparsedDataRef parseData);
    
    void setReportSize(UInt8 reportID, IOHIDReportType reportType, UInt32 bits);
    
protected:
    bool registerElement(IOHIDElementPrivate *element,
                         IOHIDElementCookie *cookie);
    
    virtual bool init(void *descriptor, IOByteCount length);
    virtual void free() APPLE_KEXT_OVERRIDE;
    
public:
    static IOHIDElementContainer *withDescriptor(void *descriptor,
                                                 IOByteCount length);
    
    virtual IOReturn updateElementValues(IOHIDElementCookie *cookies,
                                 UInt32 cookieCount = 1);
    
    virtual IOReturn postElementValues(IOHIDElementCookie *cookies,
                               UInt32 cookieCount = 1);
    
    void createReport(IOHIDReportType reportType,
                      UInt8 reportID,
                      IOBufferMemoryDescriptor *report);
    
    bool processReport(IOHIDReportType reportType,
                       UInt8 reportID,
                       void *reportData,
                       UInt32 reportLength,
                       AbsoluteTime timestamp,
                       bool *shouldTickle = 0,
                       IOOptionBits options = 0);
    
    OSArray *getElements() { return _reserved->elements; }
    OSArray *getFlattenedElements() { return _reserved->flattenedElements; }
    OSArray *getFlattenedCollections() { return _reserved->flattenedCollections; }
    OSArray *getInputReportElements() { return _reserved->inputReportElements; }
    IOBufferMemoryDescriptor *getElementValuesDescriptor() { return _reserved->elementValuesDescriptor; }
    UInt32 getMaxInputReportSize() { return _reserved->maxInputReportSize; }
    UInt32 getMaxOutputReportSize() { return _reserved->maxOutputReportSize; }
    UInt32 getMaxFeatureReportSize() { return _reserved->maxFeatureReportSize; }
    UInt32 getDataElementIndex() { return _reserved->dataElementIndex; }
    UInt32 getReportCount() { return _reserved->reportCount; }
};

#endif /* IOHIDElementContainer_h */
