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
#ifndef _H_PCSCDMONITOR
#define _H_PCSCDMONITOR

#include <MacTypes.h>
#include <security_utilities/powerwatch.h>
#include <security_utilities/pcsc++.h>
#include <security_utilities/refcount.h>
#include <security_utilities/iodevices.h>
#include <security_utilities/threading.h>
#include <securityd_client/ssclient.h>

#include "pcscdserver.h"
#include "PCSCDevice.h"
#include "PCSCDriverBundles.h"

typedef int32_t (*addDeviceCallback)(const char *name, uint32_t address, const char *pathLibrary, const char *pathDevice);
typedef int32_t (*removeDeviceCallback)(const char *name, uint32_t address);
typedef int32_t (*willSleepCallback)();
typedef int32_t (*isWakingCallback)();

#if defined(__cplusplus)

class PCSCDMonitor;

class TerminationNoticeReceiver : public IOKit::NotificationPort::Receiver
{
public:
	TerminationNoticeReceiver(PCSCDMonitor &parent) : mParent(parent) {}
	virtual ~TerminationNoticeReceiver();
	
	virtual void ioChange(IOKit::DeviceIterator &iterator);
	virtual void ioServiceChange(void *refCon, io_service_t service,	//IOServiceInterestCallback
		natural_t messageType, void *messageArgument);
		
	virtual PCSCDMonitor &parent() { return mParent; }

private:
	PCSCDMonitor &mParent;
};

//
// A PCSCMonitor uses PCSC to monitor the state of smartcard readers and
// tokens (cards) in the system, and dispatches messages and events to the
// various related players in securityd. There should be at most one of these
// objects active within securityd.
//
class PCSCDMonitor :
	private MachPlusPlus::MachServer::Timer,
	private IOKit::NotificationPort::Receiver,
	private MachPlusPlus::PowerWatcher
{
public:

	friend class TerminationNoticeReceiver;
	
	PCSCDMonitor(PCSCD::Server &server, PCSCD::DriverBundles &drivers);
	void setCallbacks(addDeviceCallback theAddDeviceCallback, removeDeviceCallback theRemoveDeviceCallback,
		willSleepCallback theWillSleepCallback, isWakingCallback theIsWakingCallback)
		{ mAddDeviceCallback = theAddDeviceCallback; mRemoveDeviceCallback = theRemoveDeviceCallback;
		  mWillSleepCallback = theWillSleepCallback; mIsWakingCallback = theIsWakingCallback; }
		  
	static void postNotification(const SecurityServer::NotificationEvent event);
	
	void systemAwakeAndReadyCheck();

protected:
	
	PCSCD::Server &server;
	PCSCD::DriverBundles &drivers;
	addDeviceCallback mAddDeviceCallback;
	removeDeviceCallback mRemoveDeviceCallback;
	willSleepCallback mWillSleepCallback;
	isWakingCallback mIsWakingCallback;

protected:
	// MachServer::Timer
	void action();
	
	// NotificationPort::Receiver
	void ioChange(IOKit::DeviceIterator &iterator);
	void ioServiceChange(void *refCon, io_service_t service, natural_t messageType, void *messageArgument);

	// PowerWatcher
	void systemWillSleep();
	void systemIsWaking();
		
protected:
	void scheduleTimer(bool enable);
	void initialSetup();
	void noDeviceTimeout();

	enum DeviceSupport
	{
		impossible,				// certain this is not a smartcard
		definite,				// definitely a smartcard device
		possible				// perhaps... we're not sure
	};
	DeviceSupport deviceSupport(const IOKit::Device &dev);
	
	void addDevice(const IOKit::Device &dev);
	void removeDevice(io_service_t service, uint32_t address);
	void removeDeviceByName(const IOKit::Device &dev);
	bool hasLegacyDriver(const IOKit::Device &dev);
	bool isExcludedDevice(const IOKit::Device &dev);
	void scheduleAddInterestNotification(io_service_t serviceOfInterest);
	void addInterestNotification();
	void removeAllDevices();
	void AddIOKitNotifications();
	void RemoveIOKitNotifications();
	void rescanExistingDevices();

	typedef std::map<uint32_t, RefPointer<PCSCD::Device> > DeviceMap;
	DeviceMap mDevices;

	mutable Mutex mDeviceMapLock;

	void insert(pair<uint32_t, RefPointer<PCSCD::Device> > devicepair) { StLock<Mutex> _(mDeviceMapLock); mDevices.insert(devicepair); }
	void remove(DeviceMap::iterator it) { StLock<Mutex> _(mDeviceMapLock); mDevices.erase(it); }

private:
	void (PCSCDMonitor::*mTimerAction)();		// what to do when our timer fires	
	bool mGoingToSleep;							// between sleep and wakeup; special timer handling

	mutable Mutex mLock;

	IOKit::MachPortNotificationPort mIOKitNotifier;	// IOKit connection
	TerminationNoticeReceiver mTerminationNoticeReceiver;
	
	io_object_t mRemoveNotification;
	io_service_t mServiceOfInterest;

	bool mSleepWakePeriod;
	mutable Mutex mSleepWakePeriodLock;
	mutable Mutex mWakeConditionLock;
	Condition mWakeConditionVariable;
	bool isSleepWakePeriod() const;
	void sleepWakePeriod(bool isASleepWakePeriod);
	void setSystemIsAwakeCondition(bool isAwake);

	bool findDevice(const IOKit::Device &dev, DeviceMap::iterator &it);
	bool findDeviceByName(const IOKit::Device &dev, DeviceMap::iterator &outit);
	void updateDevice(const IOKit::Device &dev);
	void setDeviceProperties(const IOKit::Device &dev, PCSCD::Device &device);

	static void getVendorAndProductID(const IOKit::Device &dev, uint32_t &vendorID, uint32_t &productID, bool &isPCCard);
	static bool deviceIsPCCard(const IOKit::Device &dev);
	static bool deviceIsPCCard(io_service_t service);
	static bool deviceAddress(io_service_t service, uint32_t &address);
	static bool deviceAddress(const IOKit::Device &dev, uint32_t &address);
	static bool deviceMemoryAddress(const IOKit::Device &dev, uint32_t &address);
	static bool deviceMemoryAddress(io_service_t service, uint32_t &address);
	static bool deviceMemoryAddressCore(CFArrayRef cfDeviceMemory, std::string path, uint32_t &address);
	static bool addressFromPath(std::string path, uint32_t &address);

	// debug
	void setDebugPropertiesForDevice(const IOKit::Device &dev, PCSCD::Device* newDevice);
	static void displayPropertiesOfDevice(const IOKit::Device &dev);
	static void displayPropertiesOfDevice(io_service_t service);
	void dumpDevices();
};

#endif /* __cplusplus__ */

#endif //_H_PCSCDMONITOR

