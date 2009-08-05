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

#include <TargetConditionals.h>

#include <IOKit/IOLib.h>    // IOMalloc/IOFree
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/hidsystem/IOHIDSystem.h>
#include "IOHIDDevice.h"
#include "IOHIDElementPrivate.h"
#include "IOHIDParserPriv.h"
#include "IOHIDInterface.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDFamilyPrivate.h"
#include "IOHIDLibUserClient.h"

#if !TARGET_OS_EMBEDDED
#include "IOHIKeyboard.h"
#include "IOHIPointing.h"
#endif


//===========================================================================
// IOHIDDevice class

#undef  super
#define super IOService

OSDefineMetaClassAndAbstractStructors( IOHIDDevice, IOService )

// RESERVED IOHIDDevice CLASS VARIABLES
// Defined here to avoid conflicts from within header file
#define _clientSet					_reserved->clientSet
#define _seizedClient				_reserved->seizedClient
#define _eventDeadline				_reserved->eventDeadline
#define _inputInterruptElementArray	_reserved->inputInterruptElementArray
#define _publishNotify				_reserved->publishNotify
#define _performTickle				_reserved->performTickle
#define _interfaceNub				_reserved->interfaceNub
#define _rollOverElement            _reserved->rollOverElement
#define _hierarchElements           _reserved->hierarchElements

#define kIOHIDEventThreshold	10

// Number of slots in the report handler dispatch table.
//
#define kReportHandlerSlots	8

// Convert from a report ID to a dispatch table slot index.
//
#define GetReportHandlerSlot(id)    ((id) & (kReportHandlerSlots - 1))

#define GetElement(index)  \
    (IOHIDElementPrivate *) _elementArray->getObject((UInt32)index)

// Serialize access to the elements for report handling,
// event queueing, and report creation.
//
#define ELEMENT_LOCK                IORecursiveLockLock( _elementLock )
#define ELEMENT_UNLOCK              IORecursiveLockUnlock( _elementLock )

// Describes the handler(s) at each report dispatch table slot.
//
struct IOHIDReportHandler
{
    IOHIDElementPrivate * head[ kIOHIDReportTypeCount ];
};

#define GetHeadElement(slot, type)  _reportHandlers[slot].head[type]

// #define DEBUG 1
#ifdef  DEBUG
#define DLOG(fmt, args...)  IOLog(fmt, args)
#else
#define DLOG(fmt, args...)
#endif

#ifndef kIOUserClientCrossEndianKey
#define kIOUserClientCrossEndianKey "IOUserClientCrossEndian"
#endif

#ifndef kIOUserClientCrossEndianCompatibleKey
#define kIOUserClientCrossEndianCompatibleKey "IOUserClientCrossEndianCompatible"
#endif
            
// *** GAME DEVICE HACK ***
static SInt32 g3DGameControllerCount = 0;
// *** END GAME DEVICE HACK ***

static IOService *  gDisplayManager = 0;

//---------------------------------------------------------------------------
// Notification handler to grab an instance of the IOHIDSystem
bool IOHIDDevice::_publishNotificationHandler(
			void * target,
			void * /* ref */,
			IOService * newService )
{
    IOHIDDevice * self = (IOHIDDevice *) target;

    if( newService->metaCast("IODisplayWrangler")) {
        if( !gDisplayManager) {
            gDisplayManager = newService;
        }
        
        if ( self->_publishNotify ) {
            self->_publishNotify->remove();
            self->_publishNotify = 0;
        }
    }
    
    return true;
}

//---------------------------------------------------------------------------
// Initialize an IOHIDDevice object.

bool IOHIDDevice::init( OSDictionary * dict )
{
    _reserved = IONew( ExpansionData, 1 );

    if (!_reserved)
        return false;
		
	bzero(_reserved, sizeof(ExpansionData));
            
    // Create an OSSet to store client objects. Initial capacity
    // (which can grow) is set at 2 clients.

    _clientSet = OSSet::withCapacity(2);
    if ( _clientSet == 0 )
        return false;

    return super::init(dict);
}

//---------------------------------------------------------------------------
// Free an IOHIDDevice object after its retain count drops to zero.
// Release all resource.

void IOHIDDevice::free()
{
    if ( _reportHandlers )
    {
        IOFree( _reportHandlers,
                sizeof(IOHIDReportHandler) * kReportHandlerSlots );
        _reportHandlers = 0;
    }

    if ( _elementArray )
    {
        _elementArray->release();
        _elementArray = 0;
    }
    
    if ( _hierarchElements ) 
    {
        _hierarchElements->release();
        _hierarchElements = 0;
    }

    
    if ( _elementValuesDescriptor )
    {
        _elementValuesDescriptor->release();
        _elementValuesDescriptor = 0;
    }

    if ( _elementLock )
    {
        IORecursiveLockFree( _elementLock );
        _elementLock = 0;
    }
    
    if ( _clientSet )
    {
        // Should not have any clients.
        assert(_clientSet->getCount() == 0);
        _clientSet->release();
        _clientSet = 0;
    }
    
    if (_publishNotify)
    {
        _publishNotify->remove();
        _publishNotify = 0;
    }

    if (_inputInterruptElementArray)
    {
        _inputInterruptElementArray->release();
        _inputInterruptElementArray = 0;
    }

    if (_interfaceNub)
    {
        _interfaceNub->release();
        _interfaceNub = 0;
    }
        
    if ( _reserved )
    {        
        IODelete( _reserved, ExpansionData, 1 );
    }


    return super::free();
}

static inline OSArray * CreateHierarchticalElementList(IOHIDElement * root)
{
    OSArray *       resultArray = 0;
    OSArray *       subElements = 0;
    OSArray *       elements    = 0;
    IOHIDElement *  element     = 0;
    IOItemCount     count;
    
    if ( !root ) return NULL;
    
    elements = root->getChildElements();
    
    if ( !elements ) return NULL;
    
    count = elements->getCount();
    
    resultArray = OSArray::withCapacity(count);
    
    if ( !resultArray ) return NULL;
    
    for ( UInt32 index=0; index < count; index++ )
    {
        element = OSDynamicCast(IOHIDElement, elements->getObject(index));
        
        if ( !element ) continue;
        
        resultArray->setObject(element);
                
        subElements = CreateHierarchticalElementList(element);
        
        if ( subElements )
        {
            resultArray->merge(subElements);
            subElements->release();
            subElements = 0;
        }
    }
    
    return resultArray;
}


//---------------------------------------------------------------------------
// Start up the IOHIDDevice.

bool IOHIDDevice::start( IOService * provider )
{
    IOMemoryDescriptor * reportDescriptor;
    IOReturn             ret;

    if ( super::start(provider) != true )
        return false;

    // Allocate a mutex lock to serialize report handling.

    _elementLock = IORecursiveLockAlloc();
    if ( _elementLock == 0 )
        return false;

    // Allocate memory for report handler dispatch table.

    _reportHandlers = (IOHIDReportHandler *)
                      IOMalloc( sizeof(IOHIDReportHandler) *
                                kReportHandlerSlots );
    if ( _reportHandlers == 0 )
        return false;

    bzero( _reportHandlers, sizeof(IOHIDReportHandler) * kReportHandlerSlots );

    // Call handleStart() before fetching the report descriptor.

    if ( handleStart(provider) != true )
        return false;

    // Fetch report descriptor for the device, and parse it.

    if ( ( newReportDescriptor(&reportDescriptor) != kIOReturnSuccess ) ||
         ( reportDescriptor == 0 ) )
        return false;

    ret = parseReportDescriptor( reportDescriptor );
    reportDescriptor->release();

    if ( ret != kIOReturnSuccess )
        return false;

    _hierarchElements = CreateHierarchticalElementList((IOHIDElement *)_elementArray->getObject( 0 ));
    
    if ( _hierarchElements == NULL )
        return false;
    
	_interfaceNub = IOHIDInterface::withElements( _hierarchElements );
    	
	if ( _interfaceNub == NULL )
		return false;

    // Once the report descriptors have been parsed, we are ready
    // to handle reports from the device.

    _readyForInputReports = true;
		
    // Publish properties to the registry before any clients are
    // attached.

    if ( publishProperties(provider) != true )
        return false;
		
    OSNumber *primaryUsagePage = OSDynamicCast(OSNumber,
                                    getProperty(kIOHIDPrimaryUsagePageKey));
    OSNumber *primaryUsage = OSDynamicCast(OSNumber,
                                    getProperty(kIOHIDPrimaryUsageKey));

    // *** GAME DEVICE HACK ***
    if ((primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == 0x05)) &&
        (primaryUsage && (primaryUsage->unsigned32BitValue() == 0x01))) {
        OSIncrementAtomic(&g3DGameControllerCount);
    }
    // *** END GAME DEVICE HACK ***

    // Publish ourself to the registry and trigger client matching.
    // Do it synchronously, so that the client initer has done its
    // thing before the interface is registered.  This avoids a 
    // nasty resource race condition.

    registerService(kIOServiceSynchronous);
    
    if ( _interfaceNub->attach(this) )
    {
        if (!_interfaceNub->start(this))
        {
            _interfaceNub->detach(this);
            _interfaceNub->release();
            _interfaceNub = 0;
            return false;
        }
    }
    else 
    {
        _interfaceNub->release();
        _interfaceNub = 0;
        return false;
    }
    
    return true;
}

//---------------------------------------------------------------------------
// Stop the IOHIDDevice.

void IOHIDDevice::stop(IOService * provider)
{
    // *** GAME DEVICE HACK ***
    OSNumber *primaryUsagePage = OSDynamicCast(OSNumber,
                                    getProperty(kIOHIDPrimaryUsagePageKey));
    OSNumber *primaryUsage = OSDynamicCast(OSNumber,
                                    getProperty(kIOHIDPrimaryUsageKey));
                                    
    if ((primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == 0x05)) &&
        (primaryUsage && (primaryUsage->unsigned32BitValue() == 0x01))) {
        OSDecrementAtomic(&g3DGameControllerCount);
    }
    // *** END GAME DEVICE HACK ***
    
    handleStop(provider);

    if ( _elementLock )
    {
        ELEMENT_LOCK;
        _readyForInputReports = false;
        ELEMENT_UNLOCK;
    }

    super::stop(provider);
}


bool IOHIDDevice::matchPropertyTable(OSDictionary * table, SInt32 * score)
{
    bool    match       = true;

    // Ask our superclass' opinion.
    if (super::matchPropertyTable(table, score) == false)  return false;

    match = MatchPropertyTable(this, table, score);
    		
    // *** HACK ***
    // RY: For games that are accidentaly matching on the keys
    // PrimaryUsage = 0x01
    // PrimaryUsagePage = 0x05
    // If there no devices present that contain these values,
    // then return true.
    if (!match && (g3DGameControllerCount <= 0) && table) {
        OSNumber *primaryUsage = OSDynamicCast(OSNumber, table->getObject(kIOHIDPrimaryUsageKey));
        OSNumber *primaryUsagePage = OSDynamicCast(OSNumber, table->getObject(kIOHIDPrimaryUsagePageKey));

        if ((primaryUsage && (primaryUsage->unsigned32BitValue() == 0x01)) &&
            (primaryUsagePage && (primaryUsagePage->unsigned32BitValue() == 0x05))) {
            match = true;
            IOLog("IOHIDManager: It appears that an application is attempting to locate an invalid device.  A workaround is in currently in place, but will be removed after version 10.2\n");
        }
    }
    // *** END HACK ***
        
    return match;
}



//---------------------------------------------------------------------------
// Fetch and publish HID properties to the registry.

bool IOHIDDevice::publishProperties(IOService * provider)
{
    OSObject * prop;
    
#define SET_PROP_INT(key, prop) \
            setProperty(key, prop);  \
			_interfaceNub->setProperty(key, prop);

#define SET_PROP(func, key)          \
    do {                             \
        prop = func ();           \
        if (prop) {                  \
            SET_PROP_INT(key, prop);  \
            prop->release();         \
        }                            \
    } while (0)
    
    SET_PROP( newTransportString,        kIOHIDTransportKey );
    SET_PROP( newVendorIDNumber,         kIOHIDVendorIDKey );
    SET_PROP( newVendorIDSourceNumber,   kIOHIDVendorIDSourceKey );
    SET_PROP( newProductIDNumber,        kIOHIDProductIDKey );
    SET_PROP( newVersionNumber,          kIOHIDVersionNumberKey );
    SET_PROP( newManufacturerString,     kIOHIDManufacturerKey );
    SET_PROP( newProductString,          kIOHIDProductKey );
    SET_PROP( newLocationIDNumber,       kIOHIDLocationIDKey );
    SET_PROP( newCountryCodeNumber,      kIOHIDCountryCodeKey );    
    SET_PROP( newSerialNumberString,     kIOHIDSerialNumberKey );
    SET_PROP( newPrimaryUsageNumber,     kIOHIDPrimaryUsageKey );
    SET_PROP( newPrimaryUsagePageNumber, kIOHIDPrimaryUsagePageKey );
    SET_PROP( newReportIntervalNumber,   kIOHIDReportIntervalKey );
	SET_PROP( newDeviceUsagePairs,       kIOHIDDeviceUsagePairsKey );
    
    if ( getProvider() ) 
    {
        SET_PROP_INT("BootProtocol", getProvider()->getProperty("bInterfaceProtocol"));
        SET_PROP_INT("HIDDefaultBehavior", getProperty("HIDDefaultBehavior"));
    }

    return true;
}

//---------------------------------------------------------------------------
// Derived from start() and stop().

bool IOHIDDevice::handleStart(IOService * provider)
{
    return true;
}

void IOHIDDevice::handleStop(IOService * provider)
{
}

static inline bool ShouldPostDisplayActivityTickles(IOService *device, OSSet * clientSet, bool isSeized)
{
    OSNumber *              primaryUsagePage;

    if (!clientSet->getCount() ||
        !(primaryUsagePage = OSDynamicCast(OSNumber, device->getProperty(kIOHIDPrimaryUsagePageKey))) || 
         (primaryUsagePage->unsigned32BitValue() != kHIDPage_GenericDesktop))
        return false;

    // We have clients and this device is generic desktop.
    // Probe the client list to make sure that we are not 
    // openned by an IOHIDEventService.  If so, there is 
    // no reason to tickle the display, as the HID System
    // already does this.
    OSCollectionIterator *  iterator;
    OSObject *              object;
    bool                    returnValue = true;

    if ( !isSeized && (iterator = OSCollectionIterator::withCollection(clientSet)) )
    {
        while ( object = iterator->getNextObject() )
        {
            if ( object->metaCast("IOHIDEventService"))
            {
                returnValue = false;
                break;
            }
        }
        iterator->release();
    }
    return returnValue;
}
//---------------------------------------------------------------------------
// Handle a client open on the interface.

bool IOHIDDevice::handleOpen(IOService *  client,
                                    IOOptionBits options,
                                    void *       argument)
{
    bool  		accept = false;

    do {
        if ( _seizedClient )
            break;
            
        // Was this object already registered as our client?

        if ( _clientSet->containsObject(client) )
        {
            DLOG("%s: multiple opens from client %lx\n",
                 getName(), (UInt32) client);
            accept = true;
            break;
        }

        // Add the new client object to our client set.

        if ( _clientSet->setObject(client) == false )
            break;
        
        if (options & kIOServiceSeize)
        {
            messageClients( kIOMessageServiceIsRequestingClose, (void *) options);
            //if (kIOReturnSuccess != retval)
            //    break;
                    
            _seizedClient = client;
            
#if !TARGET_OS_EMBEDDED
            IOHIKeyboard * keyboard = 0;
            IOHIPointing * pointing = 0;
            if ( keyboard = OSDynamicCast(IOHIKeyboard, getProvider()) )
                keyboard->IOHIKeyboard::message(kIOHIDSystemDeviceSeizeRequestMessage, this, (void *)true);
            else if ( pointing = OSDynamicCast(IOHIPointing, getProvider()) )
                pointing->IOHIPointing::message(kIOHIDSystemDeviceSeizeRequestMessage, this, (void *)true);
#endif
        }

        accept = true;
    }
    while (false);

    _performTickle = ShouldPostDisplayActivityTickles(this, _clientSet, _seizedClient);
    
    // RY: Add a notification to get an instance of the Display
    // Manager.  This will allow us to tickle it upon receiveing
    // new reports. 
    if ( _performTickle && !gDisplayManager )
    {
        _publishNotify = addNotification( gIOPublishNotification, 
                        serviceMatching("IODisplayWrangler"),
                        &IOHIDDevice::_publishNotificationHandler,
                        this, 0 );
    }
    
    return accept;
}

//---------------------------------------------------------------------------
// Handle a client close on the interface.

void IOHIDDevice::handleClose(IOService * client, IOOptionBits options)
{
    // Remove the object from the client OSSet.

    if ( _clientSet->containsObject(client) )
    {
        // Remove the client from our OSSet.
        _clientSet->removeObject(client);
        
        if (client == _seizedClient)
        {
            _seizedClient = 0;
            
#if !TARGET_OS_EMBEDDED
            IOHIKeyboard * keyboard = 0;
            IOHIPointing * pointing = 0;
            if ( keyboard = OSDynamicCast(IOHIKeyboard, getProvider()) )
                keyboard->IOHIKeyboard::message(kIOHIDSystemDeviceSeizeRequestMessage, this, (void *)false);
            else if ( pointing = OSDynamicCast(IOHIPointing, getProvider()) )
                pointing->IOHIPointing::message(kIOHIDSystemDeviceSeizeRequestMessage, this, (void *)false);
#endif
        }
        
        _performTickle = ShouldPostDisplayActivityTickles(this, _clientSet, _seizedClient);
    }
}

//---------------------------------------------------------------------------
// Query whether a client has an open on the interface.

bool IOHIDDevice::handleIsOpen(const IOService * client) const
{
    if (client)
        return _clientSet->containsObject(client);
    else
        return (_clientSet->getCount() > 0);
}


//---------------------------------------------------------------------------
// Create a new user client.

IOReturn IOHIDDevice::newUserClient( task_t          owningTask,
                                     void *          security_id,
                                     UInt32          type,
                                     OSDictionary *  properties,
                                     IOUserClient ** handler )
{
    // RY: This is really skanky.  Apparently there are some subclasses out there
    // that want all the benefits of IOHIDDevice w/o supporting the default HID
    // User Client.  I know!  Shocking!  Anyway, passing a type known only to the
    // default hid clients to ensure that at least connect to our correct client.
    if ( type == kIOHIDLibUserClientConnectManager ) {
    	if (isInactive()) {
			IOLog("IOHIDDevice::newUserClient called on an inactive device\n");
    		*handler = NULL;
    		return kIOReturnNotReady;
    	}
    
        if ( properties ) {
            properties->setObject(kIOUserClientCrossEndianCompatibleKey, kOSBooleanTrue);
        }
        
        IOWorkLoop *loop = getWorkLoop();
        IOReturn result = kIOReturnNotReady;
        if (loop) {
        	result = loop->runAction(OSMemberFunctionCast(IOWorkLoop::Action, this, &IOHIDDevice::newUserClientGated),
                          this, owningTask, security_id, properties, handler);
		}
		else {
			IOLog("IOHIDDevice::newUserClient failed to get a workloop\n");
		}
        return result;
    }
    
    return super::newUserClient(owningTask, security_id, type, properties, handler);
}

IOReturn IOHIDDevice::newUserClientGated(	task_t          owningTask,
											void *          security_id,
											OSDictionary *  properties,
											IOUserClient ** handler )
{

    IOUserClient * client = new IOHIDLibUserClient;
    
    if ( !client->initWithTask(owningTask, security_id, kIOHIDLibUserClientConnectManager, properties) ) {
        client->release();
        return kIOReturnBadArgument;
    }
    
    if ( !client->attach(this) ) {
        client->release();
        return kIOReturnUnsupported;
    }
    
    if ( !client->start(this) ) {
        client->detach(this);
        client->release();
        return kIOReturnUnsupported;
    }
    
    *handler = client;
    
    return kIOReturnSuccess;
    
}

//---------------------------------------------------------------------------
// Default implementation of the HID property 'getter' functions.

OSString * IOHIDDevice::newTransportString() const
{
    return 0;
}

OSString * IOHIDDevice::newManufacturerString() const
{
    return 0;
}

OSString * IOHIDDevice::newProductString() const
{
    return 0;
}

OSNumber * IOHIDDevice::newVendorIDNumber() const
{
    return 0;
}

OSNumber * IOHIDDevice::newProductIDNumber() const
{
    return 0;
}

OSNumber * IOHIDDevice::newVersionNumber() const
{
    return 0;
}

OSNumber * IOHIDDevice::newSerialNumber() const
{
    return 0;
}

OSNumber * IOHIDDevice::newPrimaryUsageNumber() const
{
    OSArray * 		childArray;
    IOHIDElementPrivate * 	child;
    IOHIDElementPrivate * 	root;

    if ( (root = (IOHIDElementPrivate *) _elementArray->getObject(0)) && 
         (childArray = root->getChildElements()) &&
         (child = (IOHIDElementPrivate *) childArray->getObject(0)) )
    {
        return OSNumber::withNumber(child->getUsage(), 32);
    }
    
    return 0;
}

OSNumber * IOHIDDevice::newPrimaryUsagePageNumber() const
{
    OSArray * 		childArray;
    IOHIDElementPrivate * 	child;
    IOHIDElementPrivate * 	root;

    if ( (root = (IOHIDElementPrivate *) _elementArray->getObject(0)) && 
         (childArray = root->getChildElements()) &&
         (child = (IOHIDElementPrivate *) childArray->getObject(0)) )
    {
        return OSNumber::withNumber(child->getUsagePage(), 32);
    }
    
    return 0;
}

//---------------------------------------------------------------------------
// Handle input reports (USB Interrupt In pipe) from the device.

IOReturn IOHIDDevice::handleReport( IOMemoryDescriptor * report,
                                    IOHIDReportType      reportType,
                                    IOOptionBits         options )
{
    AbsoluteTime   currentTime;

    clock_get_uptime( &currentTime );

	return handleReportWithTime( currentTime, report, reportType, options );
}

//---------------------------------------------------------------------------
// Get a report from the device.

IOReturn IOHIDDevice::getReport( IOMemoryDescriptor * report,
                                 IOHIDReportType      reportType,
                                 IOOptionBits         options )
{
    return getReport(report, reportType, options, 0, 0);
}

//---------------------------------------------------------------------------
// Send a report to the device.

IOReturn IOHIDDevice::setReport( IOMemoryDescriptor * report,
                                 IOHIDReportType      reportType,
                                 IOOptionBits         options)
{
    return setReport(report, reportType, options, 0, 0);
}

//---------------------------------------------------------------------------
// Parse a report descriptor, and update the property table with
// the IOHIDElementPrivate hierarchy discovered.

IOReturn IOHIDDevice::parseReportDescriptor( IOMemoryDescriptor * report,
                                             IOOptionBits         options )
{
    OSStatus             status = kIOReturnError;
    HIDPreparsedDataRef  parseData;
    void *               reportData;
    IOByteCount          reportLength;
    IOReturn             ret;

    reportLength = report->getLength();
    
    if ( !reportLength )
        return kIOReturnBadArgument;
        
    reportData = IOMalloc(reportLength);
    
    if ( !reportData )
        return kIOReturnNoMemory;
        
    report->readBytes( 0, reportData, reportLength );

    // Parse the report descriptor.

    status = HIDOpenReportDescriptor(
                reportData,      /* report descriptor */
                reportLength,    /* report size in bytes */
                &parseData,      /* pre-parse data */
                0 );             /* flags */

    // Release the buffer
    IOFree( reportData, reportLength );

    if ( status != kHIDSuccess )
    {
        return kIOReturnError;
    }

    // Create a hierarchy of IOHIDElementPrivate objects.

    ret = createElementHierarchy( parseData );

    getReportCountAndSizes( parseData );

    // Release memory.

    HIDCloseReportDescriptor( parseData );

    return ret;
}

//---------------------------------------------------------------------------
// Build the element hierarchy to describe the device capabilities to
// user-space.

IOReturn
IOHIDDevice::createElementHierarchy( HIDPreparsedDataRef parseData )
{
    OSStatus   		status;
    HIDCapabilities	caps;
    IOReturn		ret = kIOReturnNoMemory;

    do {    
        // Get a summary of device capabilities.

        status = HIDGetCapabilities( parseData, &caps );
        if ( status != kHIDSuccess )
        {
            ret = kIOReturnError;
            break;
        }

        // Dump HIDCapabilities structure contents.

        DLOG("Report bytes: input:%ld output:%ld feature:%ld\n",
             caps.inputReportByteLength,
             caps.outputReportByteLength,
             caps.featureReportByteLength);
        DLOG("Collections : %ld\n", caps.numberCollectionNodes);
        DLOG("Buttons     : input:%ld output:%ld feature:%ld\n",
             caps.numberInputButtonCaps,
             caps.numberOutputButtonCaps,
             caps.numberFeatureButtonCaps);
        DLOG("Values      : input:%ld output:%ld feature:%ld\n",
             caps.numberInputValueCaps,
             caps.numberOutputValueCaps,
             caps.numberFeatureValueCaps);        

        _maxInputReportSize    = caps.inputReportByteLength;
        _maxOutputReportSize   = caps.outputReportByteLength;
        _maxFeatureReportSize  = caps.featureReportByteLength;
        
        // RY: These values are useful to the subclasses.  Post them.
        setProperty(kIOHIDMaxInputReportSizeKey, _maxInputReportSize, 32);
        setProperty(kIOHIDMaxOutputReportSizeKey, _maxOutputReportSize, 32);
        setProperty(kIOHIDMaxFeatureReportSizeKey, _maxFeatureReportSize, 32);

        
        // Create an OSArray to store all HID elements.

        _elementArray = OSArray::withCapacity(
                                     caps.numberCollectionNodes   +
                                     caps.numberInputButtonCaps   +
                                     caps.numberInputValueCaps    +
                                     caps.numberOutputButtonCaps  +
                                     caps.numberOutputValueCaps   +
                                     caps.numberFeatureButtonCaps +
                                     caps.numberFeatureValueCaps  +
                                     10 );
        if ( _elementArray == 0 ) break;

        _elementArray->setCapacityIncrement(10);

        // Add collections to the element array.

        if ( !createCollectionElements(
                                  parseData,
                                  _elementArray,
                                  caps.numberCollectionNodes ) ) break;

        // Everything added to the element array from this point on
        // are "data" elements. We cache the starting index.

        _dataElementIndex = _elementArray->getCount();

        // Add input buttons to the element array.

        if ( !createButtonElements( parseData,
                                    _elementArray,
                                    kHIDInputReport,
                                    kIOHIDElementTypeInput_Button,
                                    caps.numberInputButtonCaps ) ) break;

        // Add output buttons to the element array.

        if ( !createButtonElements( parseData,
                                    _elementArray,
                                    kHIDOutputReport,
                                    kIOHIDElementTypeOutput,
                                    caps.numberOutputButtonCaps ) ) break;

        // Add feature buttons to the element array.
        
        if ( !createButtonElements( parseData,
                                    _elementArray,
                                    kHIDFeatureReport,
                                    kIOHIDElementTypeFeature,
                                    caps.numberFeatureButtonCaps ) ) break;

        // Add input values to the element array.

        if ( !createValueElements( parseData,
                                   _elementArray,
                                   kHIDInputReport,
                                   kIOHIDElementTypeInput_Misc,
                                   caps.numberInputValueCaps ) ) break;

        // Add output values to the element array.

        if ( !createValueElements( parseData,
                                   _elementArray,
                                   kHIDOutputReport,
                                   kIOHIDElementTypeOutput,
                                   caps.numberOutputValueCaps ) ) break;

        // Add feature values to the element array.
    
        if ( !createValueElements( parseData,
                                   _elementArray,
                                   kHIDFeatureReport,
                                   kIOHIDElementTypeFeature,
                                   caps.numberFeatureValueCaps ) ) break;
                                   
        // Add the input report handler to the element array.
        if ( !createReportHandlerElements(parseData) ) break;


        // Create a memory to store current element values.

        _elementValuesDescriptor = createMemoryForElementValues();
        if ( _elementValuesDescriptor == 0 )
            break;

        // Element hierarchy has been built, add it to the property table.

        IOHIDElementPrivate * root = (IOHIDElementPrivate *) _elementArray->getObject( 0 );
        if ( root )
        {
            setProperty( kIOHIDElementKey, root->getChildElements() );
        }
        
        // Add the interrupt report handlers to the property table as well.
        setProperty(kIOHIDInputReportElementsKey, 
                        _inputInterruptElementArray);
                                
        ret = kIOReturnSuccess;
    }
    while ( false );

    return ret;
}

//---------------------------------------------------------------------------
// Fetch the all the possible functions of the device

static OSDictionary * CreateDeviceUsagePairFromElement(IOHIDElementPrivate * element)
{
    OSDictionary *	pair		= 0;
    OSNumber *		usage 		= 0;
    OSNumber *		usagePage 	= 0;
    OSNumber *		type 		= 0;
	
	pair		= OSDictionary::withCapacity(2);
	usage		= OSNumber::withNumber(element->getUsage(), 32);
	usagePage	= OSNumber::withNumber(element->getUsagePage(), 32);
	type		= OSNumber::withNumber(element->getCollectionType(), 32);
	
	pair->setObject(kIOHIDDeviceUsageKey, usage);
	pair->setObject(kIOHIDDeviceUsagePageKey, usagePage);
	//pair->setObject(kIOHIDElementCollectionTypeKey, type);

	usage->release();
	usagePage->release();
	type->release();
	
	return pair;
 }

OSArray * IOHIDDevice::newDeviceUsagePairs()
{
    IOHIDElementPrivate *	element			= 0;
    OSArray *				functions		= 0;
    OSDictionary *			pair			= 0;
    UInt32					elementCount 	= _elementArray->getCount();    
    
	if ( elementCount <= 1 ) // this include vitual collection
		return NULL;
		
	functions = OSArray::withCapacity(2);
	
    // starts at one to avoid the virtual collection
    for (unsigned i=1; i<elementCount; i++)
    {
        element = (IOHIDElementPrivate *)_elementArray->getObject(i);

        if ((element->getType() == kIOHIDElementTypeCollection) &&
            ((element->getCollectionType() == kIOHIDElementCollectionTypeApplication) ||
            (element->getCollectionType() == kIOHIDElementCollectionTypePhysical)))
        {            
            pair = CreateDeviceUsagePairFromElement(element);
            
            UInt32 	pairCount = functions->getCount();
            bool 	found = false;
            for(unsigned i=0; i<pairCount; i++)
            {
                OSDictionary *tempPair = (OSDictionary *)functions->getObject(i);
                
                if (found = tempPair->isEqualTo(pair))
                    break;
            }
            
            if (!found) 
            {
                functions->setObject(functions->getCount(), pair);                
            }
            
            pair->release();
        }
    }
	
	if ( ! functions->getCount() ) {
		pair = CreateDeviceUsagePairFromElement((IOHIDElementPrivate *)_elementArray->getObject(1));
		functions->setObject(pair);
		pair->release();
	}

    return functions;
}


//---------------------------------------------------------------------------
// Fetch the total number of reports and the size of each report.

bool IOHIDDevice::getReportCountAndSizes( HIDPreparsedDataRef parseData )
{
    HIDPreparsedDataPtr data   = (HIDPreparsedDataPtr) parseData;
    HIDReportSizes *    report = data->reports;

    _reportCount = data->reportCount;

    DLOG("Report count: %ld\n", _reportCount);
    
    for ( UInt32 num = 0; num < data->reportCount; num++, report++ )
    {

        DLOG("Report ID: %ld input:%ld output:%ld feature:%ld\n",
             report->reportID,
             report->inputBitCount,
             report->outputBitCount,
             report->featureBitCount);
        
        setReportSize( report->reportID,
                       kIOHIDReportTypeInput,
                       report->inputBitCount );
        
        setReportSize( report->reportID,
                       kIOHIDReportTypeOutput,
                       report->outputBitCount );

        setReportSize( report->reportID,
                       kIOHIDReportTypeFeature,
                       report->featureBitCount );
    }
    
    return true;
}

//---------------------------------------------------------------------------
// Set the report size for the first element in the report handler chain.

bool IOHIDDevice::setReportSize( UInt8           reportID,
                                 IOHIDReportType reportType,
                                 UInt32          numberOfBits )
{
    IOHIDElementPrivate * element;
    bool           ret = false;
    
    element = GetHeadElement( GetReportHandlerSlot(reportID), reportType );
    
    while ( element )
    {
        if ( element->getReportID() == reportID )
        {
            element->setReportSize( numberOfBits );
            ret = true;
            break;
        }
        element = element->getNextReportHandler();
    }
    return ret;
}

//---------------------------------------------------------------------------
// Add collection elements to the OSArray object provided.

bool
IOHIDDevice::createCollectionElements( HIDPreparsedDataRef parseData,
                                       OSArray *           array,
                                       UInt32              maxCount )
{
    OSStatus              	  status;
    HIDCollectionExtendedNodePtr  collections;
    UInt32                        count = maxCount;
    bool                  	  ret   = false;
    UInt32                        index;

    do {
        // Allocate memory to fetch all collections from the parseData.

        collections = (HIDCollectionExtendedNodePtr)
                      IOMalloc( maxCount * sizeof(HIDCollectionExtendedNode) );

        if ( collections == 0 ) break;

        status = HIDGetCollectionExtendedNodes(
                    collections,    /* collectionNodes     */
                    &count,         /* collectionNodesSize */
                    parseData );    /* preparsedDataRef    */

        if ( status != kHIDSuccess ) break;

        // Create an IOHIDElementPrivate for each collection.

        for ( index = 0; index < count; index++ )
        {
            IOHIDElementPrivate * element;

            element = IOHIDElementPrivate::collectionElement(
                                              this,
                                              kIOHIDElementTypeCollection,
                                              &collections[index] );
            if ( element == 0 ) break;

            element->release();
        }
        if ( index < count ) break;

        // Create linkage for the collection hierarchy.
        // Starts at 1 to skip the root (virtual) collection.

        for ( index = 1; index < count; index++ )
        {
            if ( !linkToParent( array, collections[index].parent, index ) )
                break;
        }
        if ( index < count ) break;

        ret = true;
    }
    while ( false );

    if ( collections )
        IOFree( collections, maxCount * sizeof(HIDCollectionExtendedNode) );

    return ret;
}

//---------------------------------------------------------------------------
// Link an element in the array to another element in the array as its child.

bool IOHIDDevice::linkToParent( const OSArray * array,
                                UInt32          parentIndex,
                                UInt32          childIndex )
{
    IOHIDElementPrivate * child  = (IOHIDElementPrivate *) array->getObject( childIndex );
    IOHIDElementPrivate * parent = (IOHIDElementPrivate *) array->getObject( parentIndex );

    return ( parent ) ? parent->addChildElement( child ) : false;
}

//---------------------------------------------------------------------------
// Add Button elements (1 bit value) to the collection.

bool IOHIDDevice::createButtonElements( HIDPreparsedDataRef parseData,
                                        OSArray *           array,
                                        UInt32              hidReportType,
                                        IOHIDElementType    elementType,
                                        UInt32              maxCount )
{
    OSStatus          		status;
    HIDButtonCapabilitiesPtr 	buttons = 0;
    UInt32			count   = maxCount;
    bool			ret     = false;
    IOHIDElementPrivate *		element;
    IOHIDElementPrivate *		parent;

    do {
        if ( maxCount == 0 )
        {
            ret = true;
            break;
        }
        
        // Allocate memory to fetch all button elements from the parseData.

        buttons = (HIDButtonCapabilitiesPtr) IOMalloc( maxCount *
                                               sizeof(HIDButtonCapabilities) );
        if ( buttons == 0 ) break;

        status = HIDGetButtonCapabilities( hidReportType,  /* HIDReportType    */
                                   buttons,        /* buttonCaps       */
                                   &count,         /* buttonCapsSize   */
                                   parseData );    /* preparsedDataRef */

        if ( status != kHIDSuccess ) break;

        // Create an IOHIDElementPrivate for each button and link it to its
        // parent collection.

        ret = true;

        for ( UInt32 i = 0; i < count; i++ )
        {            
            parent  = (IOHIDElementPrivate *) array->getObject(
                                              buttons[i].collection );

            element = IOHIDElementPrivate::buttonElement(
                                          this,
                                          elementType,
                                          &buttons[i],
                                          parent );
            if ( element == 0 )
            {
                ret = false;
                break;
            }
            element->release();
        }
    }
    while ( false );

    if ( buttons )
        IOFree( buttons, maxCount * sizeof(HIDButtonCapabilities) );
    
    return ret;
}

//---------------------------------------------------------------------------
// Add Value elements to the collection.

bool IOHIDDevice::createValueElements( HIDPreparsedDataRef parseData,
                                       OSArray *           array,
                                       UInt32              hidReportType,
                                       IOHIDElementType    elementType,
                                       UInt32              maxCount )
{
    OSStatus         status;
    HIDValueCapabilitiesPtr  values = 0;
    UInt32           count  = maxCount;
    bool             ret    = false;
    IOHIDElementPrivate *   element;
    IOHIDElementPrivate *   parent;

    do {
        if ( maxCount == 0 )
        {
            ret = true;
            break;
        }

        // Allocate memory to fetch all value elements from the parseData.

        values = (HIDValueCapabilitiesPtr) IOMalloc( maxCount *
                                             sizeof(HIDValueCapabilities) );
        if ( values == 0 ) break;

        status = HIDGetValueCapabilities( hidReportType,  /* HIDReportType    */
                                  values,         /* valueCaps        */
                                  &count,         /* valueCapsSize    */
                                  parseData );    /* preparsedDataRef */

        if ( status != kHIDSuccess ) break;

        // Create an IOHIDElementPrivate for each value and link it to its
        // parent collection.

        ret = true;

        for ( UInt32 i = 0; i < count; i++ )
        {
            parent  = (IOHIDElementPrivate *) array->getObject(
                                              values[i].collection );

            element = IOHIDElementPrivate::valueElement(
                                         this,
                                         elementType,
                                         &values[i],
                                         parent );

            if ( element == 0 )
            {
                ret = false;
                break;
            }
            element->release();
        }
    }
    while ( false );

    if ( values )
        IOFree( values, maxCount * sizeof(HIDValueCapabilities) );
    
    return ret;
}

//---------------------------------------------------------------------------
// Add report handler elements.

bool IOHIDDevice::createReportHandlerElements( HIDPreparsedDataRef parseData)
{
    HIDPreparsedDataPtr data   = (HIDPreparsedDataPtr) parseData;
    HIDReportSizes *    report = data->reports;
    IOHIDElementPrivate * 	element = 0;

    if ( !(_inputInterruptElementArray = OSArray::withCapacity(data->reportCount)))
        return false;
    
    for ( UInt32 num = 0; num < data->reportCount; num++, report++ )
    {
        element = IOHIDElementPrivate::reportHandlerElement(
                                    this, 
                                    kIOHIDElementTypeInput_Misc, 
                                    report->reportID, 
                                    report->inputBitCount);
        
        if ( element == 0 )
            continue;
            
        _inputInterruptElementArray->setObject(element);
        
        element->release();
    }

    return true;
}

//---------------------------------------------------------------------------
// Called by an IOHIDElementPrivate to register itself.

bool IOHIDDevice::registerElement( IOHIDElementPrivate * element,
                                   IOHIDElementCookie * cookie )
{
    IOHIDReportType reportType;
    UInt32          index = _elementArray->getCount();

    // Add the element to the elements array.

    if ( _elementArray->setObject( index, element ) != true )
    {
        return false;
    }

    // If the element can contribute to an Input, Output, or Feature
    // report, then add it to the chain of report handlers.
    if ( element->getReportType( &reportType ) )
    {
        IOHIDReportHandler * reportHandler;
        UInt32               slot;

        slot = GetReportHandlerSlot( element->getReportID() );

        reportHandler = &_reportHandlers[slot];

        if ( reportHandler->head[reportType] )
        {
            element->setNextReportHandler( reportHandler->head[reportType] );
        }
        reportHandler->head[reportType] = element;
        
        if ( element->getUsagePage() == kHIDPage_KeyboardOrKeypad )
        {
            UInt32 usage = element->getUsage();
            
            if ( usage == kHIDUsage_KeyboardErrorRollOver)
                _rollOverElement = element;
         
            if ( usage >= kHIDUsage_KeyboardLeftControl && usage <= kHIDUsage_KeyboardRightGUI )
                element->setRollOverElementPtr(&(_rollOverElement));
        }
    }

    // The cookie returned is simply an index to the element in the
    // elements array. We may decide to obfuscate it later on.

    *cookie = (IOHIDElementCookie) index;

    return true;
}

//---------------------------------------------------------------------------
// Create a buffer memory descriptor, and divide the memory buffer
// for each data element.

IOBufferMemoryDescriptor * IOHIDDevice::createMemoryForElementValues()
{
    IOBufferMemoryDescriptor *  descriptor;
    IOHIDElementPrivate *       element;
    UInt32                      capacity = 0;
    UInt8 *                     start;
    UInt8 *                     buffer;
    
    // Discover the amount of memory required to publish the
    // element values for all "data" elements.

    for ( UInt32 slot = 0; slot < kReportHandlerSlots; slot++ )
    {
        for ( UInt32 type = 0; type < kIOHIDReportTypeCount; type++ )
        {
            element = GetHeadElement(slot, type);
            while ( element )
            {
                capacity += element->getElementValueSize();
                element   = element->getNextReportHandler();
            }
        }
    }

    // Allocate an IOBufferMemoryDescriptor object.

	DLOG("Element value capacity %ld\n", capacity);

    descriptor = IOBufferMemoryDescriptor::withOptions(
                   kIOMemorySharingTypeMask,
                   capacity );

    if ( ( descriptor == 0 ) || ( descriptor->getBytesNoCopy() == 0 ) )
    {
        if ( descriptor ) descriptor->release();
        return 0;
    }

    // Now assign the update memory area for each report element.
    start = buffer = (UInt8 *) descriptor->getBytesNoCopy();

    for ( UInt32 slot = 0; slot < kReportHandlerSlots; slot++ )
    {
        for ( UInt32 type = 0; type < kIOHIDReportTypeCount; type++ )
        {
            element = GetHeadElement(slot, type);
            while ( element )
            {
                assert ( buffer < (start + capacity) );
            
                element->setMemoryForElementValue( (IOVirtualAddress) buffer,
                                                (void *) (buffer - start));
    
                buffer += element->getElementValueSize();
                element = element->getNextReportHandler();
            }
        }
    }

    return descriptor;
}

//---------------------------------------------------------------------------
// Get a reference to the memory descriptor created by
// createMemoryForElementValues().

IOMemoryDescriptor * IOHIDDevice::getMemoryWithCurrentElementValues() const
{
    return _elementValuesDescriptor;
}

//---------------------------------------------------------------------------
// Start delivering events from the given element to the specified
// event queue.

IOReturn IOHIDDevice::startEventDelivery( IOHIDEventQueue *  queue,
                                          IOHIDElementCookie cookie,
                                          IOOptionBits       options )
{
    IOHIDElementPrivate * element;
    UInt32         elementIndex = (UInt32) cookie;
    IOReturn       ret = kIOReturnBadArgument;

    if ( ( queue == 0 ) || ( elementIndex < _dataElementIndex ) )
        return kIOReturnBadArgument;

    ELEMENT_LOCK;

	do {
        if (( element = GetElement(elementIndex) ) == 0)
            break;
        
        ret = element->addEventQueue( queue ) ?
              kIOReturnSuccess : kIOReturnNoMemory;
    }
    while ( false );

    ELEMENT_UNLOCK;
    
    return ret;
}

//---------------------------------------------------------------------------
// Stop delivering events from the given element to the specified
// event queue.

IOReturn IOHIDDevice::stopEventDelivery( IOHIDEventQueue *  queue,
                                         IOHIDElementCookie cookie )
{
    IOHIDElementPrivate * element;
    UInt32         elementIndex = (UInt32) cookie;
    bool           removed      = false;

    // If the cookie provided was zero, then loop and remove the queue
    // from all elements.

    if ( elementIndex == 0 )
        elementIndex = _dataElementIndex;
	else if ( (queue == 0 ) || ( elementIndex < _dataElementIndex ) )
        return kIOReturnBadArgument;

    ELEMENT_LOCK;

	do {
        if (( element = GetElement(elementIndex++) ) == 0)
            break;

        removed = element->removeEventQueue( queue ) || removed;
    }
    while ( cookie == 0 );

    ELEMENT_UNLOCK;
    
    return removed ? kIOReturnSuccess : kIOReturnNotFound;
}

//---------------------------------------------------------------------------
// Check whether events from the given element will be delivered to
// the specified event queue.

IOReturn IOHIDDevice::checkEventDelivery( IOHIDEventQueue *  queue,
                                          IOHIDElementCookie cookie,
                                          bool *             started )
{
    IOHIDElementPrivate * element = GetElement( cookie );

    if ( !queue || !element || !started )
        return kIOReturnBadArgument;

    ELEMENT_LOCK;

    *started = element->hasEventQueue( queue );

    ELEMENT_UNLOCK;
    
    return kIOReturnSuccess;
}

#define SetCookiesTransactionState(element, cookies, count, state, index, offset) \
    for (index = offset; index < count; index++) { 			\
        element = GetElement(cookies[index]); 				\
        if (element == NULL) 						\
            continue; 							\
        element->setTransactionState (state);				\
    }

//---------------------------------------------------------------------------
// Update the value of the given element, by getting a report from
// the device.  Assume that the cookieCount > 0

OSMetaClassDefineReservedUsed(IOHIDDevice,  0);
IOReturn IOHIDDevice::updateElementValues(IOHIDElementCookie *cookies, UInt32 cookieCount) {
    IOMemoryDescriptor *	report = NULL;
    IOHIDElementPrivate *		element = NULL;
    IOHIDReportType		reportType;
    IOByteCount			maxReportLength;
    UInt8			reportID;
    UInt32			index;
    IOReturn			ret = kIOReturnError;
    
    maxReportLength = max(_maxOutputReportSize, 
                            max(_maxFeatureReportSize, _maxInputReportSize));
    
    // Allocate a mem descriptor with the maxReportLength.
    // This way, we only have to allocate one mem discriptor
    report = IOBufferMemoryDescriptor::withCapacity(maxReportLength, kIODirectionNone);
        
    if (report == NULL)
        return kIOReturnNoMemory;

    ELEMENT_LOCK;
    
    SetCookiesTransactionState(element, cookies, 
            cookieCount, kIOHIDTransactionStatePending, index, 0);
                
    // Iterate though all the elements in the 
    // transaction.  Generate reports if needed.
    for (index = 0; index < cookieCount; index++) {
        element = GetElement(cookies[index]);
        
        if (element == NULL)
            continue;
            
        if ( element->getTransactionState() 
                != kIOHIDTransactionStatePending )
            continue;
                        
        if ( !element->getReportType(&reportType) )
            continue;

        reportID = element->getReportID();

        // calling down into our subclass, so lets unlock
        ELEMENT_UNLOCK;
        
        ret = getReport(report, reportType, reportID);

        ELEMENT_LOCK;
    
        if (ret != kIOReturnSuccess)
            break;
            
        // If we have a valid report, go ahead and process it.
        ret = handleReport(report, reportType, kIOHIDReportOptionNotInterrupt);
        
        if (ret != kIOReturnSuccess)
            break;
    }
    
    // release the report
    report->release();

    // If needed, set the transaction state for the 
    // remaining elements to idle.
    SetCookiesTransactionState(element, cookies, 
            cookieCount, kIOHIDTransactionStateIdle, index, 0);
    ELEMENT_UNLOCK;
        
    return ret;
}

//---------------------------------------------------------------------------
// Post the value of the given element, by sending a report to
// the device.  Assume that the cookieCount > 0
OSMetaClassDefineReservedUsed(IOHIDDevice,  1);
IOReturn IOHIDDevice::postElementValues(IOHIDElementCookie * cookies, UInt32 cookieCount)
{
    IOBufferMemoryDescriptor	*report = NULL;
    IOHIDElementPrivate 		*element = NULL;
    IOHIDElementPrivate 		*cookieElement = NULL;
    UInt8			*reportData = NULL;
    IOByteCount			maxReportLength = 0;
    IOByteCount			reportLength = 0;
    IOHIDReportType		reportType;
    UInt8			reportID = 0;
    UInt32 			index;
    IOReturn			ret = kIOReturnError;
    
    // Return an error if no cookies are being set
    if (cookieCount == 0)
        return ret;
        
    // Get the max report size
    maxReportLength = max(_maxOutputReportSize, _maxFeatureReportSize);

    // Allocate a buffer mem descriptor with the maxReportLength.
    // This way, we only have to allocate one mem buffer.
    report = IOBufferMemoryDescriptor::withCapacity(maxReportLength, kIODirectionNone);
    
    if ( report == NULL )
        return kIOReturnNoMemory;

    ELEMENT_LOCK;
    
    // Set the transaction state on the specified cookies
    SetCookiesTransactionState(cookieElement, cookies, 
            cookieCount, kIOHIDTransactionStatePending, index, 0);
            
    // Obtain the buffer
    reportData = (UInt8 *)report->getBytesNoCopy();
 
    // Iterate though all the elements in the 
    // transaction.  Generate reports if needed. 
    for (index = 0; index < cookieCount; index ++) {

        cookieElement = GetElement(cookies[index]);
    
        if ( cookieElement == NULL )
            continue;
          
        // Continue on to the next element if 
        // we've already processed this one
        if ( cookieElement->getTransactionState() 
                != kIOHIDTransactionStatePending )
            continue;
            
        if ( !cookieElement->getReportType(&reportType) )
            continue;
                
        reportID = cookieElement->getReportID();

        // Start at the head element and iterate through
        element = GetHeadElement(GetReportHandlerSlot(reportID), reportType);
                
        while ( element ) {
            
            element->createReport(reportID, reportData, &reportLength, &element);
            
            // If the reportLength was set, then this is
            // the head element for this report
            if ( reportLength ) {
                report->setLength(reportLength);
                reportLength = 0;
            }
                
        }
        
        // If there are multiple reports, append
        // the reportID to the first byte
        if ( _reportCount > 1 ) 
            reportData[0] = reportID;
                  
        ELEMENT_UNLOCK;
        
        ret = setReport( report, reportType, reportID);
        
        ELEMENT_LOCK;

        if ( ret != kIOReturnSuccess )
            break;                          
    }

    // If needed, set the transaction state for the 
    // remaining elements to idle.
    SetCookiesTransactionState(cookieElement, cookies, 
            cookieCount, kIOHIDTransactionStateIdle, index, 0);
    
    ELEMENT_UNLOCK;
    
    if ( report )
        report->release();
    
    return ret;
}

OSMetaClassDefineReservedUsed(IOHIDDevice,  2);
OSString * IOHIDDevice::newSerialNumberString() const
{
	OSString * string = 0;
	OSNumber * number = 0;
	
	if ( number = newSerialNumber() )
	{
		char	str[11];
		sprintf(str, "%d", number->unsigned32BitValue());
		string = OSString::withCString(str);
		number->release();
	}
	
    return string;
}

OSMetaClassDefineReservedUsed(IOHIDDevice,  3);
OSNumber * IOHIDDevice::newLocationIDNumber() const
{
    return 0;
}

//---------------------------------------------------------------------------
// Get an async report from the device.

OSMetaClassDefineReservedUsed(IOHIDDevice,  4);
IOReturn IOHIDDevice::getReport( IOMemoryDescriptor * report,
                                IOHIDReportType      reportType,
                                IOOptionBits         options,
                                UInt32               completionTimeout,
                                IOHIDCompletion	*    completion)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Send an async report to the device.

OSMetaClassDefineReservedUsed(IOHIDDevice,  5);
IOReturn IOHIDDevice::setReport( IOMemoryDescriptor * report,
                                IOHIDReportType      reportType,
                                IOOptionBits         options,
                                UInt32               completionTimeout,
                                IOHIDCompletion	*    completion)
{
    return kIOReturnUnsupported;
}

//---------------------------------------------------------------------------
// Return the vendor id source

OSMetaClassDefineReservedUsed(IOHIDDevice,  6);
OSNumber * IOHIDDevice::newVendorIDSourceNumber() const
{
    return 0;
}

//---------------------------------------------------------------------------
// Return the country code

OSMetaClassDefineReservedUsed(IOHIDDevice,  7);
OSNumber * IOHIDDevice::newCountryCodeNumber() const
{
    return 0;
}

//---------------------------------------------------------------------------
// Handle input reports (USB Interrupt In pipe) from the device.

OSMetaClassDefineReservedUsed(IOHIDDevice,  8);
IOReturn IOHIDDevice::handleReportWithTime( 
									AbsoluteTime         timeStamp,
									IOMemoryDescriptor * report,
                                    IOHIDReportType      reportType,
                                    IOOptionBits         options)
{
    void *         reportData;
    IOByteCount    reportLength;
    IOReturn       ret = kIOReturnNotReady;
    bool           changed = false;
    bool           shouldTickle = false;
	UInt8          reportID = 0;

    // Only input reports are currently handled.
    
    //if ( reportType != kIOHIDReportTypeInput )
    //    return kIOReturnUnsupported;

    // Get a pointer to the data in the descriptor.
    if ( !report )
        return kIOReturnBadArgument;
        
    reportLength = report->getLength();
    
    if ( !reportLength )
        return kIOReturnBadArgument;
        
    reportData = IOMalloc(reportLength);
    
    if ( !reportData )
        return kIOReturnNoMemory;
        
    report->readBytes( 0, reportData, reportLength );

    ELEMENT_LOCK;

    if ( _readyForInputReports )
    {
        IOHIDElementPrivate * element;

        // The first byte in the report, may be the report ID.
        // XXX - Do we need to advance the start of the report data?
        
        reportID = ( _reportCount > 1 ) ? *((UInt8 *) reportData) : 0;

        // Get the first element in the report handler chain.
            
        element = GetHeadElement( GetReportHandlerSlot(reportID),
                                    reportType);

        while ( element )
        {
            shouldTickle |= element->shouldTickleActivity();
            changed |= element->processReport( reportID,
                                    reportData,
                                    reportLength << 3,
                                    &timeStamp,
                                    &element,
                                    options );
        }

        ret = kIOReturnSuccess;
    }

	if ( ( reportType == kIOHIDReportTypeInput ) &&
             (( options & kIOHIDReportOptionNotInterrupt ) == 0 ) && _interfaceNub && !_seizedClient)
	{
		_interfaceNub->handleReport(timeStamp, report, reportType, reportID, options);
	}

    ELEMENT_UNLOCK;

	// Release the buffer
    IOFree(reportData, reportLength);

#if 0 // XXX - debugging
{
    UInt32 * buf = (UInt32 *) _elementValuesDescriptor->getBytesNoCopy();
    
    for (UInt32 words = 0; words < (_elementValuesDescriptor->getLength() / 4);
         words+=6, buf+=6)
    {
        IOLog("%3ld: %08lx %08lx %08lx %08lx %08lx %08lx\n",
              words,
              buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    }
}
#endif

    // RY: If this is a non-system HID device, post a null hid
    // event to prevent the system from sleeping.
    if (gDisplayManager 
        && changed 
        && shouldTickle
        && _performTickle 
        && (CMP_ABSOLUTETIME(&timeStamp, &_eventDeadline) > 0))
    {		
        AbsoluteTime ts;
        
        nanoseconds_to_absolutetime(kIOHIDEventThreshold, &ts);

        _eventDeadline = ts;
        
        ADD_ABSOLUTETIME(&_eventDeadline, &timeStamp);
        
        gDisplayManager->activityTickle(0,0);
    }
    
    return ret;
}

//---------------------------------------------------------------------------
// Return the country code

OSMetaClassDefineReservedUsed(IOHIDDevice, 9);
OSNumber * IOHIDDevice::newReportIntervalNumber() const
{
    // default to 8 milliseconds
    return OSNumber::withNumber(8000, 32);
}

OSMetaClassDefineReservedUnused(IOHIDDevice, 10);
OSMetaClassDefineReservedUnused(IOHIDDevice, 11);
OSMetaClassDefineReservedUnused(IOHIDDevice, 12);
OSMetaClassDefineReservedUnused(IOHIDDevice, 13);
OSMetaClassDefineReservedUnused(IOHIDDevice, 14);
OSMetaClassDefineReservedUnused(IOHIDDevice, 15);
OSMetaClassDefineReservedUnused(IOHIDDevice, 16);
OSMetaClassDefineReservedUnused(IOHIDDevice, 17);
OSMetaClassDefineReservedUnused(IOHIDDevice, 18);
OSMetaClassDefineReservedUnused(IOHIDDevice, 19);
OSMetaClassDefineReservedUnused(IOHIDDevice, 20);
OSMetaClassDefineReservedUnused(IOHIDDevice, 21);
OSMetaClassDefineReservedUnused(IOHIDDevice, 22);
OSMetaClassDefineReservedUnused(IOHIDDevice, 23);
OSMetaClassDefineReservedUnused(IOHIDDevice, 24);
OSMetaClassDefineReservedUnused(IOHIDDevice, 25);
OSMetaClassDefineReservedUnused(IOHIDDevice, 26);
OSMetaClassDefineReservedUnused(IOHIDDevice, 27);
OSMetaClassDefineReservedUnused(IOHIDDevice, 28);
OSMetaClassDefineReservedUnused(IOHIDDevice, 29);
OSMetaClassDefineReservedUnused(IOHIDDevice, 30);
OSMetaClassDefineReservedUnused(IOHIDDevice, 31);

#ifndef __ppc__
    OSMetaClassDefineReservedUnused(IOHIDDevice, 32);
    OSMetaClassDefineReservedUnused(IOHIDDevice, 33);
    OSMetaClassDefineReservedUnused(IOHIDDevice, 34);
    OSMetaClassDefineReservedUnused(IOHIDDevice, 35);
    OSMetaClassDefineReservedUnused(IOHIDDevice, 36);
    OSMetaClassDefineReservedUnused(IOHIDDevice, 37);
    OSMetaClassDefineReservedUnused(IOHIDDevice, 38);
    OSMetaClassDefineReservedUnused(IOHIDDevice, 39);
    OSMetaClassDefineReservedUnused(IOHIDDevice, 40);
#endif

