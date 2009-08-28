/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
 * 
 * @APPLE_LICENSE_HEADER_START@
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


//
// pcscmonitor - use PCSC to monitor smartcard reader/card state for securityd
//
// PCSCDMonitor is the "glue" between PCSC and the securityd objects representing
// smartcard-related things. Its job is to manage the daemon and translate real-world
// events (such as card and device insertions) into the securityd object web.
//
// PCSCDMonitor uses multiple inheritance to the hilt. It is (among others)
//	(*) A notification listener, to listen to pcscd state notifications
//  (*) A MachServer::Timer, to handle timed actions
//  (*) A NotificationPort::Receiver, to get IOKit notifications of device insertions
//

#include "pcscdmonitor.h"
#include <security_utilities/logging.h>
#include <security_utilities/refcount.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/IOMessage.h>
#include <asl.h>
//#include <Kernel/IOKit/pccard/IOPCCardBridge.h>
//#include <Kernel/IOKit/pccard/cs.h>

#ifndef _IOKIT_IOPCCARDBRIDGE_H
// Avoid kernel header include
#define kIOPCCardVersionOneMatchKey			"VersionOneInfo"
#define kIOPCCardFunctionNameMatchKey		"FunctionName"
#define kIOPCCardFunctionIDMatchKey			"FunctionID"
#define kIOPCCardVendorIDMatchKey			"VendorID"
#define kIOPCCardDeviceIDMatchKey			"DeviceID"
#define kIOPCCardFunctionExtensionMatchKey	"FunctionExtension"
#define kIOPCCardMemoryDeviceNameMatchKey	"MemoryDeviceName"

// this should be unique across the entire system
#define sub_iokit_pccard        err_sub(21)
#define kIOPCCardCSEventMessage iokit_family_msg(sub_iokit_pccard, 1)
#endif /*  _IOKIT_IOPCCARDBRIDGE_H */

// _LINUX_CS_H
#define CS_EVENT_CARD_INSERTION		0x000004
#define CS_EVENT_CARD_REMOVAL		0x000008
#define CS_EVENT_EJECTION_REQUEST	0x010000

// Locally defined string constants for IOKit values

#define kzIOUSBSerialNumberKey				"Serial Number"
#define kzIOUSBVendorNameKey				"USB Vendor Name"
#define kzIOUSBProductNameKey				"USB Product Name"
#define kzIOUSBLocationIDKey				"locationID"
#define kzIOUSBbInterfaceClassKey			"bInterfaceClass"
#define kzIOUSBbDeviceClassKey				"bDeviceClass"

#define kzIOPCCardIONameKey					"IOName"
#define kzIOPCCardIODeviceMemoryKey			"IODeviceMemory"
#define kzIOPCCardParentKey					"parent"
#define kzIOPCCardAddressKey				"address"

#define kzIOPCCard16DeviceClassName			"IOPCCard16Device"

#define PTRPARAMCAST(X)				(static_cast<unsigned int>(reinterpret_cast<uintptr_t>(X)))

//
// Fixed configuration parameters
//
static const Time::Interval PCSCD_IDLE_SHUTDOWN(120);		// kill daemon if no devices present

// Apple built-in iSight Device VendorID/ProductID: 0x05AC/0x8501

static const uint32_t kVendorProductMask = 0x0000FFFF;
static const uint32_t kVendorIDApple = 0x05AC;
static const uint16_t kProductIDBuiltInISight = 0x8501;

/*
	Copied from USBVideoClass-230.2.3/Digitizers/USBVDC/Camera/USBClient/APW_VDO_USBVDC_USBClient.h
*/

enum {
	kBuiltIniSightProductID = 0x8501,
	kBuiltIniSightWave2ProductID = 0x8502,
	kBuiltIniSightWave3ProductID = 0x8505,
	kUSBWave4ProductID        = 0x8507,
	kUSBWave2InK29ProductID        = 0x8508,
	kUSBWaveReserved1ProductID        = 0x8509,
	kUSBWaveReserved2ProductID        = 0x850a,
	kExternaliSightProductID = 0x1111,
	kLogitechVendorID = 0x046d
};

//static void dumpdictentry(const void *key, const void *value, void *context);

#pragma mark -------------------- Class Methods --------------------

//
// Construct a PCSCDMonitor.
// We strongly assume there's only one of us around here.
//
// Note that this constructor may well run before the server loop has started.
// Don't call anything here that requires an active server loop (like Server::active()).
// In fact, you should push all the hard work into a timer, so as not to hold up the
// general startup process.
//

PCSCDMonitor::PCSCDMonitor(PCSCD::Server &server, PCSCD::DriverBundles &drivers) :
	MachPlusPlus::MachServer::Timer(true),			// "heavy" timer task
	server(server),
	drivers(drivers),
	mAddDeviceCallback(NULL), mRemoveDeviceCallback(NULL),
	mWillSleepCallback(NULL), mIsWakingCallback(NULL),
	mTimerAction(&PCSCDMonitor::initialSetup),
	mGoingToSleep(false),
	mTerminationNoticeReceiver(*this),
	mSleepWakePeriod(false),
	mWakeConditionVariable(mWakeConditionLock)
{
	// do all the smartcard-related work once the event loop has started
	secdebug("pcsc", "PCSCDMonitor server is %p", &server);
	server.setTimer(this, Time::now());				// ASAP
	// timer only used now to call initialSetup
	mDevices.erase(mDevices.begin(),mDevices.end());
}

//
// Power event notifications
//
void PCSCDMonitor::systemWillSleep()
{
	StLock<Mutex> _(mLock);
	secdebug("pcsc", "setting sleep marker (%ld readers as of now)", mDevices.size());
	mGoingToSleep = true;
	server.clearTimer(this);
	if (mWillSleepCallback)
	{
		uint32_t rx = (*mWillSleepCallback)();
		secdebug("pcsc", "  WillSleepCallback returned %d", rx);
	}
	setSystemIsAwakeCondition(false);
}

void PCSCDMonitor::systemIsWaking()
{
	StLock<Mutex> _(mLock);
	secdebug("pcsc", "------------------ Waking from sleep ... ------------------ ");
	secdebug("pcsc", "clearing sleep marker (%ld readers as of now)", mDevices.size());
	mGoingToSleep = false;
	// rescan here
	if (mIsWakingCallback)
	{
		uint32_t rx = (*mIsWakingCallback)();
		secdebug("pcsc", "  IsWakingCallback returned %d", rx);
	}
	setSystemIsAwakeCondition(true);
}

void PCSCDMonitor::setSystemIsAwakeCondition(bool isAwake)
{
	secdebug("pcsc", "  setSystemIsAwakeCondition %d", isAwake);
	if (isAwake)
	{
		sleepWakePeriod(false);
		mWakeConditionVariable.broadcast();
	}
	else
		sleepWakePeriod(true);
}

bool PCSCDMonitor::isSleepWakePeriod() const
{
	StLock<Mutex> _(mSleepWakePeriodLock);
	return mSleepWakePeriod;
}

void PCSCDMonitor::sleepWakePeriod(bool isASleepWakePeriod)
{
	StLock<Mutex> _(mSleepWakePeriodLock);
	mSleepWakePeriod = isASleepWakePeriod;
}

void PCSCDMonitor::systemAwakeAndReadyCheck()
{
//	const long sleepTimeMSec = 100;	// 0.1s
 
	StLock<Mutex> _(mWakeConditionLock);
	while (isSleepWakePeriod())
    {
		secdebug("pcsc", "...### thread paused before waking ###...");
		mWakeConditionVariable.wait();
		secdebug("pcsc", "...### thread resume after waking ###...");
	}
}

void PCSCDMonitor::action()
{
	// Timer action
	StLock<Mutex> _(mLock);
	secdebug("pcsc", "Calling PCSCDMonitor::action()");
	(this->*mTimerAction)();
	mTimerAction = &PCSCDMonitor::noDeviceTimeout;
}

void PCSCDMonitor::scheduleTimer(bool enable)
{
	// Update the timeout timer as requested (and indicated by context)
}

//
// Perform the initial PCSC subsystem initialization.
// This runs (shortly) after securityd is fully functional and the
// server loop has started.
//
void PCSCDMonitor::initialSetup()
{
	secdebug("pcsc", "Calling PCSCDMonitor::initialSetup()");
	// receive Mach-based IOKit notifications through mIOKitNotifier
	server.add(mIOKitNotifier);

	// receive power event notifications (through our IOPowerWatcher personality)
	server.add(this);

	AddIOKitNotifications();
	
	PCSCDMonitor::postNotification(SecurityServer::kNotificationPCSCInitialized);
}

void PCSCDMonitor::AddIOKitNotifications()
{
	try
	{
		// ask for IOKit notifications for all new USB devices and process present ones
		IOKit::DeviceMatch usbSelector(kIOUSBInterfaceClassName);
		IOKit::DeviceMatch pcCardSelector(kzIOPCCard16DeviceClassName);
		mIOKitNotifier.add(usbSelector, *this, kIOMatchedNotification);	// this will scan existing USB devices	
	//	mIOKitNotifier.add(usbSelector, mTerminationNoticeReceiver, kIOTerminatedNotification);	// ditto for PC Card devices
		mIOKitNotifier.add(pcCardSelector, *this, kIOMatchedNotification);	// ditto for PC Card devices
		mIOKitNotifier.add(pcCardSelector, mTerminationNoticeReceiver, kIOTerminatedNotification);	// ditto for PC Card devices

		// catch custom non-composite USB devices - they don't have IOServices attached
		IOKit::DeviceMatch customUsbSelector(::IOServiceMatching(kIOUSBDeviceClassName));
		mIOKitNotifier.add(customUsbSelector, *this, kIOMatchedNotification);	// ditto for custom USB devices
	//	mIOKitNotifier.add(customUsbSelector, mTerminationNoticeReceiver, kIOTerminatedNotification);
	}
	catch (...)
	{
		secdebug("pcscd", "trouble adding IOKit notifications (ignored)");
	}
}
	
void PCSCDMonitor::RemoveIOKitNotifications()
{
}


void PCSCDMonitor::rescanExistingDevices()
{
    kern_return_t kr;
	mach_port_t masterPort = ((IOKit::NotificationPort)mIOKitNotifier).port();
//	mach_port_t masterPort = port();
	io_iterator_t iterator;
	
	// Process existing USB devices
	IOKit::DeviceMatch usbSelector(kIOUSBInterfaceClassName);
	kr = IOServiceGetMatchingServices(masterPort, usbSelector, &iterator);
	IOKit::DeviceIterator usbdev(iterator);
	ioChange(usbdev);

	// Process existing PC Card devices
	IOKit::DeviceMatch pcCardSelector(kzIOPCCard16DeviceClassName);
	kr = IOServiceGetMatchingServices(masterPort, pcCardSelector, &iterator);
	IOKit::DeviceIterator pcdev(iterator);
	ioChange(pcdev);
	
	// catch custom non-composite USB devices - they don't have IOServices attached
	IOKit::DeviceMatch customUsbSelector(::IOServiceMatching(kIOUSBDeviceClassName));
	kr = IOServiceGetMatchingServices(masterPort, customUsbSelector, &iterator);
	IOKit::DeviceIterator customusbdev(iterator);
	ioChange(customusbdev);
}

void PCSCDMonitor::postNotification(const SecurityServer::NotificationEvent event)
{
	// send a change notification to securityd
	// Either kNotificationPCSCStateChange or kNotificationPCSCInitialized
	using namespace SecurityServer;
	ClientSession session(Allocator::standard(), Allocator::standard());
	try {
		session.postNotification(kNotificationDomainPCSC, event, CssmData());
		secdebug("pcscd", "notification sent");
	} catch (const MachPlusPlus::Error &err) {
		switch (err.error) {
		case BOOTSTRAP_UNKNOWN_SERVICE: // securityd not yet available; this is not an error
			secdebug("pcscd", "securityd not up; no notification sent");
			break;
#if !defined(NDEBUG)
		// for debugging only, support a securityd restart. This is NOT thread-safe
		case MACH_SEND_INVALID_DEST:
			secdebug("pcscd", "resetting securityd connection for debugging");
			session.reset();
			try {
				session.postNotification(kNotificationDomainPCSC,
					kNotificationPCSCStateChange, CssmData());
			} catch (...) {
				secdebug("pcscd", "re-send attempt failed, punting");
			}
			break;
#endif //NDEBUG
		default:
			secdebug("pcscd", "exception trying to send notification (ignored)");
		}
	} catch (...) {
		secdebug("pcscd", "trouble sending security notification (ignored)");
	}
}

//
// This function is called (as a timer function) when there haven't been any (recognized)
// smartcard devicees in the system for a while.
//
void PCSCDMonitor::noDeviceTimeout()
{
	secdebug("pcsc", "killing pcscd (no smartcard devices present for %g seconds)",
		PCSCD_IDLE_SHUTDOWN.seconds());
}

void PCSCDMonitor::addInterestNotification()
{
	secdebug("pcsc", "Adding interest notification for service 0x%04X (this=%p)", mServiceOfInterest,this);
	mIOKitNotifier.addInterestNotification(*this, mServiceOfInterest);
}

void PCSCDMonitor::scheduleAddInterestNotification(io_service_t serviceOfInterest)
{
	StLock<Mutex> _(mLock);
	secdebug("pcsc", "Scheduling interest notification for service 0x%04X (this=%p)", serviceOfInterest, this);
	mServiceOfInterest = serviceOfInterest;
	mTimerAction = &PCSCDMonitor::addInterestNotification;
	server.setTimer(this, Time::now());				// ASAP
}

//
// IOKit device event notification.
// Here we listen for newly inserted devices
//
void PCSCDMonitor::ioChange(IOKit::DeviceIterator &iterator)
{
	secdebug("pcsc", "Processing device event notification");
	int def=0, pos=0, total=0;
	// Always drain this iterator
	while (IOKit::Device dev = iterator())
	{
		++total;
		displayPropertiesOfDevice(dev);
		switch (deviceSupport(dev))
		{
		case definite:
			++def;
			addDevice(dev);
			break;
		case possible:
			++pos;
			addDevice(dev);
			break;
		case impossible:
			break;
		}
	}

	dumpDevices();
	secdebug("pcsc", "Relevant devices: %d definite, %d possible, %d total", def, pos, total);
}

// IOKit device event notification.
// Here we listen for newly removed devices
//
void PCSCDMonitor::ioServiceChange(void *refCon, io_service_t service,
	natural_t messageType, void *messageArgument)
{
	uintptr_t messageArg = uintptr_t(messageArgument);
	secdebug("pcsc", "Processing ioServiceChange notice: 0x%08X [refCon=0x%08lX, service=0x%08X, arg=0x%08lX]", 
		messageType, (uintptr_t)refCon, service, messageArg);

	if (mGoingToSleep && isSleepWakePeriod())	// waking up but still drowsy
	{
		secdebug("pcsc", "  ignoring ioServiceChange notice during wake up phase");
		return;
	}

	PCSCDMonitor::displayPropertiesOfDevice(service);
	// This is called since we asked for kIOGeneralInterest notices
	// Usually it is the "device removed" notification
	switch (messageType)
	{
	case kIOMessageServiceIsTerminated:		// We get these when device is removed
		{
			uint32_t address;
			if (deviceAddress(service, address))
			{
				secdebug("pcsc", "  device removed notice: 0x%04X address: 0x%08X", service, address);
				this->removeDevice(service, address);
			}
			else
				secdebug("pcsc", "  device removed notice, but failed to find address for service: 0x%04X", service);
		}
		break;
	case kIOMessageServiceWasClosed:		// We get these when the system sleeps
		{
#ifndef NDEBUG
			uint32_t address;
			deviceAddress(service, address);
			secdebug("pcsc", "  service was closed notice: 0x%04X address: 0x%08X", service, address);
#endif
		}
		break;
	case kIOPCCardCSEventMessage:	// 0xE0054001 - not handled by mach_error_string
		secdebug("pcsc", "  pccard event message: service: 0x%04X, type: 0x%08X", 
			service, (unsigned int)messageArg);
		// Card Services Events are defined in IOKit/pccard/cs.h
		switch (messageArg)
		{
			case CS_EVENT_EJECTION_REQUEST:
				secdebug("pcsc", "  pccard event message: ejection request"); 
				break;
                    
			case CS_EVENT_CARD_REMOVAL:
			{
				uint32_t address;
				if (deviceMemoryAddress(service, address))
				{
					secdebug("pcsc", "  device removed notice: 0x%04X address: 0x%08X", service, address);
					this->removeDevice(service, address);
				}
				else
					secdebug("pcsc", "  device removed notice, but failed to find address for service: 0x%04X", service);
				break;
			}
		}
		break;
	default:
		secdebug("pcsc", "  processing device general notice: 0x%08X", messageType);
		break;
	}
}

void PCSCDMonitor::addDevice(const IOKit::Device &dev)
{
	DeviceMap::iterator it;
	if (!findDevice(dev,it))		// new device
	{
		io_service_t service = dev.ioObject();

		RefPointer<PCSCD::Device> newDevice = new PCSCD::Device(service);
		uint32_t address = 0;

		if (deviceAddress(dev, address))
		{
			newDevice->setAddress(address);
			secdebug("scsel", "  Device address:  0x%08X [service: 0x%04X]", address, service);
			setDeviceProperties(dev, *newDevice);
			if (drivers.find(*newDevice))
			{
				secdebug("driver", "  found matching driver for %s: %s", newDevice->name().c_str(), newDevice->path().c_str());
				setDebugPropertiesForDevice(dev, newDevice);
				insert(make_pair(address, newDevice));
				if (mAddDeviceCallback)
				{
					// kPCSCLITE_HP_BASE_PORT
					uint32_t rx = (*mAddDeviceCallback)(newDevice->name().c_str(), address, newDevice->path().c_str(), newDevice->name().c_str());
					secdebug("pcsc", "  AddDeviceCallback returned %d", rx);
					if (rx != SCARD_S_SUCCESS && rx != SCARD_E_DUPLICATE_READER)
					{
						DeviceMap::iterator it = mDevices.find(address);
						if (it != mDevices.end())		// found it
							remove(it);					// remove from reader map
						return;
					}
				}
				PCSCDMonitor::postNotification(SecurityServer::kNotificationPCSCStateChange);
				secdebug("pcsc", "     added to device map, address:  0x%08X, service: 0x%04X, [class @:%p]", address, service, newDevice.get());
			}
			else
			{
				secdebug("driver", "  no matching driver found for %s: %s", newDevice->name().c_str(), newDevice->path().c_str());
				// Add MessageTracer logging as per <rdar://problem/6432650>. If we get here, pcscd was launched
				// for a device insertion, but the device is not a smartcard reader (or doesn't have a
				// matching driver.
				char buf[256];
				aslmsg msg = asl_new(ASL_TYPE_MSG);
				asl_set(msg, "com.apple.message.domain", "com.apple.security.smartcardservices.unknowndevice" );
				asl_set(msg, "com.apple.message.signature", "Non-smartcard device launched pcscd");
				snprintf(buf, sizeof(buf), "%u", newDevice->vendorid());
				asl_set(msg, "com.apple.message.signature2", buf);	// vendor ID
				snprintf(buf, sizeof(buf), "%u", newDevice->productid());
				asl_set(msg, "com.apple.message.signature3", buf);	// product ID
				snprintf(buf, sizeof(buf), "Non-smartcard device launched pcscd [Vendor: %#X, Product: %#X]", 
					newDevice->vendorid(), newDevice->productid());
				asl_log(NULL, msg, ASL_LEVEL_NOTICE, buf);
				asl_free(msg);
			}
		}
		else
			secdebug("pcsc", "  device added notice, but failed to find address for service: 0x%04X", service);
	}
	else
	{
		PCSCD::Device *theDevice = static_cast<PCSCD::Device *>(it->second);
		secdebug("scsel", "  Already in map: Device address:  0x%08X [service: 0x%04X]", 
			theDevice->address(), dev.ioObject());
		setDeviceProperties(dev, *theDevice);
		setDebugPropertiesForDevice(dev, theDevice);
	}

	// We always try to add the interest notification. It may be that
	// we added the device during a callback for a particular plane,
	// but we didn't have the right information then to add the notification
	io_service_t servicex = dev.ioObject();
	mIOKitNotifier.addInterestNotification(*this, servicex);
	dumpDevices();
}

bool PCSCDMonitor::findDevice(const IOKit::Device &dev, DeviceMap::iterator &it)
{
	uint32_t address = 0;
	deviceAddress(dev, address);
	it = mDevices.find(address);
	return (it != mDevices.end());
}

bool PCSCDMonitor::findDeviceByName(const IOKit::Device &dev, DeviceMap::iterator &outit)
{
	CFRef<CFStringRef> ioName = dev.property<CFStringRef>(kzIOPCCardIONameKey);
	if (!ioName)
		return false;
		
	std::string devname = cfString(ioName);
	for (DeviceMap::iterator it = mDevices.begin(); it != mDevices.end(); ++it)
	{
		PCSCD::Device *theDevice = static_cast<PCSCD::Device *>(it->second);
		if (theDevice->name() == devname)
		{
			outit = it;
			return true;
		}
	}
	
	return false;
}

void PCSCDMonitor::updateDevice(const IOKit::Device &dev)
{
	DeviceMap::iterator it;
	if (findDevice(dev,it))
	{
		PCSCD::Device *theDevice = static_cast<PCSCD::Device *>(it->second);
		setDeviceProperties(dev, *theDevice);
		if (drivers.find(*theDevice))
			secdebug("driver", "  found matching driver for %s: %s", theDevice->name().c_str(), theDevice->path().c_str());
		setDebugPropertiesForDevice(dev, theDevice);
	}
}

bool PCSCDMonitor::hasLegacyDriver(const IOKit::Device &dev)
{
	PCSCD::Device tmpDevice(0);	//dev.ioObject() - fake it
	uint32_t address = 0;
	if (deviceAddress(dev, address))
		tmpDevice.setAddress(address);
	setDeviceProperties(dev, tmpDevice);
	if (drivers.find(tmpDevice))
	{
		secdebug("driver", "  found matching driver for legacy device: %s", tmpDevice.path().c_str());
		return true;
	}

	return false;
}

bool PCSCDMonitor::deviceIsPCCard(const IOKit::Device &dev)
{
	if (CFRef<CFStringRef> ioName = dev.property<CFStringRef>(kzIOPCCardIONameKey))
		if (cfString(ioName).find("pccard", 0, 1) == 0)
			return true;
			
	return false;
}

bool PCSCDMonitor::deviceIsPCCard(io_service_t service)
{
	if (CFRef<CFStringRef> ioName = static_cast<CFStringRef>(::IORegistryEntryCreateCFProperty(
		service, CFSTR(kzIOPCCardIONameKey), kCFAllocatorDefault, 0)))
		if (cfString(ioName).find("pccard", 0, 1) == 0)
			return true;
			
	return false;
}

void PCSCDMonitor::getVendorAndProductID(const IOKit::Device &dev, uint32_t &vendorID, uint32_t &productID, bool &isPCCard)
{
	vendorID = productID = 0;
	isPCCard = deviceIsPCCard(dev);
	
	if (!isPCCard)
	{
		if (CFRef<CFNumberRef> cfVendorID = dev.property<CFNumberRef>(kUSBVendorID))
			vendorID = cfNumber(cfVendorID);

		if (CFRef<CFNumberRef> cfProductID = dev.property<CFNumberRef>(kUSBProductID))
			productID = cfNumber(cfProductID);
	}	
	else
	{
		if (CFRef<CFNumberRef> cfVendorID = dev.property<CFNumberRef>(kIOPCCardVendorIDMatchKey))
			vendorID = cfNumber(cfVendorID);

		if (CFRef<CFNumberRef> cfProductID = dev.property<CFNumberRef>(kIOPCCardDeviceIDMatchKey))
			productID = cfNumber(cfProductID);

		// One special case for legacy OmniKey CardMan 4040 support
		CFRef<CFStringRef> ioName = dev.property<CFStringRef>(kzIOPCCardIONameKey);
		if (ioName && CFEqual(ioName, CFSTR("pccard-no-cis")))
		{
			vendorID = 0x0223;
			productID = 0x0200;
		}
	}
}

void PCSCDMonitor::setDeviceProperties(const IOKit::Device &dev, PCSCD::Device &device)
{
	uint32_t vendorID, productID;
	bool isPCCard;
	
	getVendorAndProductID(dev, vendorID, productID, isPCCard);
	
	device.setIsPCCard(isPCCard);

	if (CFRef<CFNumberRef> cfInterface = dev.property<CFNumberRef>(kzIOUSBbInterfaceClassKey))
		device.setInterfaceClass(cfNumber(cfInterface));

	if (CFRef<CFNumberRef> cfDevice = dev.property<CFNumberRef>(kzIOUSBbDeviceClassKey))
		device.setDeviceClass(cfNumber(cfDevice));

	device.setVendorid(vendorID);
	device.setProductid(productID);
	
	if (CFRef<CFStringRef> ioName = dev.property<CFStringRef>(kzIOPCCardIONameKey))
		device.setName(cfString(ioName));
}

bool PCSCDMonitor::isExcludedDevice(const IOKit::Device &dev)
{
	uint32_t vendorID, productID;
	bool isPCCard;
	
	getVendorAndProductID(dev, vendorID, productID, isPCCard);
	
	if ((vendorID & kVendorProductMask) != kVendorIDApple)
		return false;	// i.e. it is not an excluded device
	
	// Since Apple does not manufacture smartcard readers, just exclude
	// If we even start making them, we should make it a CCID reader anyway
	
	return true;
}

void PCSCDMonitor::setDebugPropertiesForDevice(const IOKit::Device &dev, PCSCD::Device * newDevice)
{
	/*
		Many of these properties are only defined on the "IOUSBDevice" plane, so
		will be non-empty on the third iteration.
	*/
	std::string vendorName, productName, serialNumber;

	if (CFRef<CFStringRef> cfVendorString = dev.property<CFStringRef>(kzIOUSBVendorNameKey))
		vendorName = cfString(cfVendorString);

	if (CFRef<CFStringRef> cfProductString = dev.property<CFStringRef>(kzIOUSBProductNameKey))
		productName = cfString(cfProductString);

	if (CFRef<CFStringRef> cfSerialString = dev.property<CFStringRef>(kzIOUSBSerialNumberKey))
		serialNumber = cfString(cfSerialString);

	if (deviceIsPCCard(dev))
	{
		if (CFRef<CFArrayRef> cfVersionOne = dev.property<CFArrayRef>(kIOPCCardVersionOneMatchKey))
		if (CFArrayGetCount(cfVersionOne) > 1)
		{
			CFStringRef cfVendorString = (CFStringRef)CFArrayGetValueAtIndex(cfVersionOne, 0);
			if (cfVendorString)
				vendorName = cfString(cfVendorString);

			CFStringRef cfProductString = (CFStringRef)CFArrayGetValueAtIndex(cfVersionOne, 1);
			if (cfProductString)
				productName = cfString(cfProductString);
		}
	}
	
	newDevice->setDebugParams(vendorName, productName, serialNumber);
		
//	secdebug("scsel", "  deviceSupport: vendor/product: 0x%04X/0x%04X, vendor:  %s, product: %s, serial: %s", vendorid, productid,
//		vendorName.c_str(), productName.c_str(), serialNumber.c_str());
}

void PCSCDMonitor::removeDevice(io_service_t service, uint32_t address)
{
	secdebug("pcsc", " Size of mDevices: %ld, service: 0x%04X", mDevices.size(), service);
	if (!mDevices.empty())
	{
		secdebug("pcsc", "  device removed notice: 0x%04X address: 0x%08X", service, address);
		DeviceMap::iterator it = mDevices.find(address);
		if (it != mDevices.end())		// found it
		{
			if (mRemoveDeviceCallback)
			{
				uint32_t rx = (*mRemoveDeviceCallback)((it->second)->name().c_str(), address);
				secdebug("pcsc", "  RemoveDeviceCallback returned %d", rx);
			}
			remove(it);					// remove from reader map
		}
		else
			secdebug("pcsc", " service: 0x%04X at address 0x%04X not found ??", service, address);
	}
	dumpDevices();
	::IOObjectRelease(service);		// we don't want notifications here until re-added
}

void PCSCDMonitor::removeDeviceByName(const IOKit::Device &dev)
{
	io_service_t service = dev.ioObject();
	secdebug("pcsc", " Size of mDevices: %ld, service: 0x%04X", mDevices.size(), service);
	if (!mDevices.empty())
	{
		uint32_t address = 0;
		deviceAddress(dev, address);
		DeviceMap::iterator it;
		if (findDeviceByName(dev, it))		// found it
		{
			if (mRemoveDeviceCallback)
			{
				uint32_t rx = (*mRemoveDeviceCallback)((it->second)->name().c_str(), address);
				secdebug("pcsc", "  RemoveDeviceCallback returned %d", rx);
			}
			remove(it);					// remove from reader map
		}
		else
			secdebug("pcsc", " service: 0x%04X at address 0x%04X not found ??", service, address);
	}
	dumpDevices();
	::IOObjectRelease(service);		// we don't want notifications here until re-added
}

void PCSCDMonitor::removeAllDevices()
{
	secdebug("pcsc", ">>>>>> removeAllDevices: Size of mDevices: %ld", mDevices.size());
	for (DeviceMap::iterator it = mDevices.begin(); it != mDevices.end(); ++it)
	{
		PCSCD::Device *dev = static_cast<PCSCD::Device *>(it->second);
		uint32_t address = 0;
	//	PCSCDMonitor::deviceAddress(*dev, &address);
	address = dev->address();
		io_service_t service = dev->ioObject();
		if (mRemoveDeviceCallback)
		{
			uint32_t rx = (*mRemoveDeviceCallback)(dev->name().c_str(), address);
			secdebug("pcsc", "  RemoveDeviceCallback returned %d", rx);
		}
		::IOObjectRelease(service);		// we don't want notifications here until re-added
		remove(it);						// remove from reader map
	}
	secdebug("pcsc", ">>>>>> removeAllDevices [end]: Size of mDevices: %ld", mDevices.size());
}


//
// Check an IOKit device that's just come online to see if it's
// a smartcard device of some sort.
//
PCSCDMonitor::DeviceSupport PCSCDMonitor::deviceSupport(const IOKit::Device &dev)
{
#ifndef NDEBUG
	try
	{
		secdebug("scsel", "path: %s", dev.path().c_str());	// this can fail sometimes
	}
	catch (...)
	{
		secdebug("scsel", "  exception while displaying device path - ignoring error");
	}
#endif
	
	try
	{
		// composite USB device with interface class
		if (CFRef<CFNumberRef> cfInterface = dev.property<CFNumberRef>(kzIOUSBbInterfaceClassKey))
			switch (uint32_t clas = cfNumber(cfInterface))
			{
			case kUSBChipSmartCardInterfaceClass:		// CCID smartcard reader - go
				secdebug("scsel", "  CCID smartcard reader recognized");
				return definite;
			case kUSBVendorSpecificInterfaceClass:
				if (isExcludedDevice(dev))
				{
					secdebug("scsel", "  interface class %d is not a smartcard device (excluded)", clas);
					return impossible;
				}
				secdebug("scsel", "  Vendor-specific interface - possible match");
				return possible;
			default:
				if ((clas == 0) && hasLegacyDriver(dev))
				{
					secdebug("scsel", "  Vendor-specific legacy driver - possible match");
					return possible;
				}
				secdebug("scsel", "  interface class %d is not a smartcard device", clas);
				return impossible;
			}

		// noncomposite USB device
		if (CFRef<CFNumberRef> cfDevice = dev.property<CFNumberRef>(kzIOUSBbDeviceClassKey))
			if (cfNumber(cfDevice) == kUSBVendorSpecificClass)
			{
				if (isExcludedDevice(dev))
				{
					secdebug("scsel", "  device class %d is not a smartcard device (excluded)", cfNumber(cfDevice));
					return impossible;
				}
				secdebug("scsel", "  Vendor-specific device - possible match");
				return possible;
			}

		// PCCard (aka PCMCIA aka ...) interface (don't know how to recognize a reader here)
		if (deviceIsPCCard(dev))
		{
			secdebug("scsel", "  PCCard - possible match");
			return possible;
		}
		
		return impossible;
	}
	catch (...)
	{
		secdebug("scsel", "  exception while examining device - ignoring it");
		return impossible;
	}
}

#pragma mark -------------------- Static Methods --------------------

bool PCSCDMonitor::deviceAddress(io_service_t service, uint32_t &address)
{	
	if (CFRef<CFNumberRef> cfLocationID = static_cast<CFNumberRef>(::IORegistryEntryCreateCFProperty(
		service, CFSTR(kzIOUSBLocationIDKey), kCFAllocatorDefault, 0)))
	{
		address = cfNumber(cfLocationID);
		return true;
	}
	
	// don't bother to test if it is a pc card, just try looking
	return deviceMemoryAddress(service, address);
}

bool PCSCDMonitor::deviceAddress(const IOKit::Device &dev, uint32_t &address)
{
	if (CFRef<CFNumberRef> cfLocationID = dev.property<CFNumberRef>(kzIOUSBLocationIDKey))
	{
		address = cfNumber(cfLocationID);
		return true;
	}

	// don't bother to test if it is a pc card, just try looking
	return deviceMemoryAddress(dev, address);
}

bool PCSCDMonitor::deviceMemoryAddress(const IOKit::Device &dev, uint32_t &address)
{
//	CFRef<CFStringRef> ioName = dev.property<CFStringRef>(kzIOPCCardIONameKey);
	CFRef<CFArrayRef> cfDeviceMemory = dev.property<CFArrayRef>(kzIOPCCardIODeviceMemoryKey);
	return deviceMemoryAddressCore(cfDeviceMemory, dev.path(), address);
}

bool PCSCDMonitor::deviceMemoryAddress(io_service_t service, uint32_t &address)
{
//	CFRef<CFStringRef> ioName = static_cast<CFStringRef>(::IORegistryEntryCreateCFProperty(
//		service, CFSTR(kzIOPCCardIONameKey), kCFAllocatorDefault, 0));
	CFRef<CFArrayRef> cfDeviceMemory = static_cast<CFArrayRef>(::IORegistryEntryCreateCFProperty(
			service, CFSTR(kzIOPCCardIODeviceMemoryKey), kCFAllocatorDefault, 0));
	return deviceMemoryAddressCore(cfDeviceMemory, "", address);
}

bool PCSCDMonitor::deviceMemoryAddressCore(CFArrayRef cfDeviceMemory, std::string path, uint32_t &address)
{
	address = 0;
	try
	{
		if (cfDeviceMemory)
		{
			if (CFRef<CFDictionaryRef> cfTempMem = (CFDictionaryRef)CFRetain(CFArrayGetValueAtIndex(cfDeviceMemory, 0)))
			{
			//	CFDictionaryApplyFunction(cfTempMem, dumpdictentry, NULL);
				if (CFRef<CFArrayRef> cfParent = (CFArrayRef)CFRetain(CFDictionaryGetValue(cfTempMem, CFSTR(kzIOPCCardParentKey))))
					if (CFRef<CFDictionaryRef> cfTempMem2 = (CFDictionaryRef)CFRetain(CFArrayGetValueAtIndex(cfParent, 0)))
						if (CFRef<CFNumberRef> cfAddress = (CFNumberRef)CFRetain(CFDictionaryGetValue((CFDictionaryRef)cfTempMem2, CFSTR(kzIOPCCardAddressKey))))
						{
							address = cfNumber(cfAddress);
							secdebug("scsel", "  address from device memory address property: 0x%08X", address);
							return true;
						}
			}
		}
		else
		if (!path.empty())
		{
		//	std::string name = cfString(ioName);
		//	address = CFHash (ioName);
		//	address = 0xF2000000;
			addressFromPath(path, address);
			secdebug("scsel", "  extracted address: 0x%08X for device [%s]", address, path.c_str());
			return true;
		}
	}
	catch (...)
	{
		secdebug("scsel", "  exception while examining deviceMemoryAddress property");
	}
	return false;
}

bool PCSCDMonitor::addressFromPath(std::string path, uint32_t &address)
{
	/*
		Try to extract the address from the path if the other keys are not present.
		An example path is:
		
			IOService:/MacRISC2PE/pci@f2000000/AppleMacRiscPCI/cardbus@13/IOPCCardBridge/pccard2bd,1003@0,0
			
		where e.g. the address is f2000000, the vendor is 0x2bd, and the product id is 0x1003
	*/
	address = 0;
	#define HEX_TO_INT(x) ((x) >= '0' &&(x) <= '9' ? (x) - '0' : (x) - ('a' - 10)) 
	
	try
	{
		secdebug("scsel", "path: %s", path.c_str());			// this can fail sometimes

		std::string lhs("/pci@");
		std::string rhs("/");

		std::string::size_type start = path.find(lhs)+lhs.length();
		std::string::size_type end = path.find(rhs, start);

		std::string addressString(path, start, end-start);
		
		// now addressString should contain something like f2000000
		uint32_t tmp = 0;
		const char *px = addressString.c_str();
		size_t len = strlen(px);
		for (unsigned int ix=0;ix<len;ix++,px++)
		{
			tmp<<=4;
			tmp += HEX_TO_INT(*px);
		}

		address = tmp;
		
		secdebug("scsel", "  address 0x%08X extracted from path", address);
	}
	catch (...)
	{
		secdebug("scsel", "  exception while displaying device path - ignoring error");
		return false;
	}
	
	return true;
}

#pragma mark -------------------- Termination Notice Receiver --------------------

TerminationNoticeReceiver::~TerminationNoticeReceiver()
{
}

void TerminationNoticeReceiver::ioChange(IOKit::DeviceIterator &iterator)
{
	secdebug("pcsc", "[TerminationNoticeReceiver] Processing ioChange notification");
	// Always drain this iterator
	while (IOKit::Device dev = iterator())
	{
		PCSCDMonitor::displayPropertiesOfDevice(dev);
		parent().removeDeviceByName(dev);
	}
}

void TerminationNoticeReceiver::ioServiceChange(void *refCon, io_service_t service,
	natural_t messageType, void *messageArgument)
{
	uintptr_t messageArg = uintptr_t(messageArgument);
	secdebug("pcsc", "  [TerminationNoticeReceiver] processing ioServiceChange notice: 0x%08X [refCon=0x%08lX, service=0x%08X, arg=0x%08lX]", 
		messageType, (uintptr_t)refCon, service, messageArg);
	parent().ioServiceChange(refCon, service, messageType, messageArgument);
}

#pragma mark -------------------- Debug Routines --------------------

void PCSCDMonitor::displayPropertiesOfDevice(const IOKit::Device &dev)
{
	/*
		Many of these properties are only defined on the "IOUSBDevice" plane, so
		will be non-empty on the third iteration.
	*/
	try
	{
		std::string vendorName, productName, serialNumber, name;

		uint32_t vendorID, productID;
		bool isPCCard;
		
		CFRef<CFStringRef> ioName = dev.property<CFStringRef>(kzIOPCCardIONameKey);
		if (ioName)
			name = cfString(ioName);

		getVendorAndProductID(dev, vendorID, productID, isPCCard);

		if (CFRef<CFStringRef> cfSerialString = dev.property<CFStringRef>(kzIOUSBSerialNumberKey))
			serialNumber = cfString(cfSerialString);

		if (isPCCard)
		{
			if (CFRef<CFArrayRef> cfVersionOne = dev.property<CFArrayRef>(kIOPCCardVersionOneMatchKey))
			if (CFArrayGetCount(cfVersionOne) > 1)
			{
				CFStringRef cfVendorString = (CFStringRef)CFArrayGetValueAtIndex(cfVersionOne, 0);
				if (cfVendorString)
					vendorName = cfString(cfVendorString);

				CFStringRef cfProductString = (CFStringRef)CFArrayGetValueAtIndex(cfVersionOne, 1);
				if (cfProductString)
					productName = cfString(cfProductString);
			}
		
			uint32_t address;
			deviceMemoryAddress(dev, address);
		}
		else
		{
			if (CFRef<CFStringRef> cfVendorString = dev.property<CFStringRef>(kzIOUSBVendorNameKey))
				vendorName = cfString(cfVendorString);

			if (CFRef<CFStringRef> cfProductString = dev.property<CFStringRef>(kzIOUSBProductNameKey))
				productName = cfString(cfProductString);
		}

		secdebug("scsel", "--- properties: service: 0x%04X, name: %s, vendor/product: 0x%04X/0x%04X, vendor: %s, product: %s, serial: %s", 
			dev.ioObject(), name.c_str(), vendorID, productID,
			vendorName.c_str(), productName.c_str(), serialNumber.c_str());
	}
	catch (...)
	{
		secdebug("scsel", "  exception in displayPropertiesOfDevice - ignoring error");
	}
}

void PCSCDMonitor::displayPropertiesOfDevice(io_service_t service)
{
    kern_return_t	kr;
    CFMutableDictionaryRef properties = NULL;

	// get a copy of the in kernel registry object
	kr = IORegistryEntryCreateCFProperties(service, &properties, kCFAllocatorDefault, 0);
	if (kr != KERN_SUCCESS)
	{
	    printf("IORegistryEntryCreateCFProperties failed with %x\n", kr);
	}
	else
	if (properties)
	{
//		CFShow(properties);
		CFRelease(properties);
	}

	try
	{
		std::string vendorName, productName, serialNumber, name;

		uint32_t vendorID, productID;
		bool isPCCard;
		
		CFRef<CFStringRef> ioName = static_cast<CFStringRef>(::IORegistryEntryCreateCFProperty(
			service, CFSTR(kzIOPCCardIONameKey), kCFAllocatorDefault, 0));
		if (ioName)
			name = cfString(ioName);

//		getVendorAndProductID(dev, vendorID, productID, isPCCard);

		CFRef<CFStringRef> cfSerialString = static_cast<CFStringRef>(::IORegistryEntryCreateCFProperty(
			service, CFSTR(kzIOUSBSerialNumberKey), kCFAllocatorDefault, 0));
		if (cfSerialString)
			serialNumber = cfString(cfSerialString);

		if (isPCCard)
		{
			CFRef<CFArrayRef> cfVersionOne = static_cast<CFArrayRef>(::IORegistryEntryCreateCFProperty(
				service, CFSTR(kIOPCCardVersionOneMatchKey), kCFAllocatorDefault, 0));
			if (cfVersionOne && (CFArrayGetCount(cfVersionOne) > 1))
			{
				CFStringRef cfVendorString = (CFStringRef)CFArrayGetValueAtIndex(cfVersionOne, 0);
				if (cfVendorString)
					vendorName = cfString(cfVendorString);

				CFStringRef cfProductString = (CFStringRef)CFArrayGetValueAtIndex(cfVersionOne, 1);
				if (cfProductString)
					productName = cfString(cfProductString);
			}
		
			uint32_t address;
			deviceMemoryAddress(service, address);
		}
		else
		{
			CFRef<CFStringRef> cfVendorString = static_cast<CFStringRef>(::IORegistryEntryCreateCFProperty(
				service, CFSTR(kzIOUSBVendorNameKey), kCFAllocatorDefault, 0));
			if (cfVendorString)
				vendorName = cfString(cfVendorString);

			CFRef<CFStringRef> cfProductString = static_cast<CFStringRef>(::IORegistryEntryCreateCFProperty(
				service, CFSTR(kzIOUSBProductNameKey), kCFAllocatorDefault, 0));
			if (cfProductString)
				productName = cfString(cfProductString);
		}

		secdebug("scsel", "--- properties: service: 0x%04X, name: %s, vendor/product: 0x%04X/0x%04X, vendor: %s, product: %s, serial: %s", 
			service, name.c_str(), vendorID, productID,
			vendorName.c_str(), productName.c_str(), serialNumber.c_str());
	}
	catch (...)
	{
		secdebug("scsel", "  exception in displayPropertiesOfDevice - ignoring error");
	}
}

void PCSCDMonitor::dumpDevices()
{
	secdebug("pcsc", "------------------ Device Map ------------------");
	for (DeviceMap::iterator it = mDevices.begin();it!=mDevices.end();++it)
	{
		PCSCD::Device *dev = static_cast<PCSCD::Device *>(it->second);
		dev->dump();
	}
	secdebug("pcsc", "------------------------------------------------");
}

#if 0
static void dumpdictentry(const void *key, const void *value, void *context)
{
	secdebug("dumpd", "  dictionary key: %s, val: %p, CFGetTypeID: %d", cfString((CFStringRef)key).c_str(), value, (int)CFGetTypeID(value));
}
#endif

