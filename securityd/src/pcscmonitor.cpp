/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
// PCSCMonitor is the "glue" between PCSC and the securityd objects representing
// smartcard-related things. Its job is to manage the daemon and translate real-world
// events (such as card and device insertions) into the securityd object web.
//
// PCSCMonitor uses multiple inheritance to the hilt. It is (among others)
//	(*) A notification listener, to listen to pcscd state notifications
//  (*) A MachServer::Timer, to handle timed actions
//  (*) A NotificationPort::Receiver, to get IOKit notifications of device insertions
//  (*) A Child, to watch and manage the pcscd process
//
#include "pcscmonitor.h"
#include <security_utilities/logging.h>
#include <IOKit/usb/IOUSBLib.h>


//
// Fixed configuration parameters
//
static const char PCSCD_EXEC_PATH[] = "/usr/sbin/pcscd";	// override with $PCSCDAEMON
static const char PCSCD_WORKING_DIR[] = "/var/run/pcscd";	// pcscd's working directory
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

//
// Construct a PCSCMonitor.
// We strongly assume there's only one of us around here.
//
// Note that this constructor may well run before the server loop has started.
// Don't call anything here that requires an active server loop (like Server::active()).
// In fact, you should push all the hard work into a timer, so as not to hold up the
// general startup process.
//
PCSCMonitor::PCSCMonitor(Server &server, const char* pathToCache, ServiceLevel level)
	: Listener(kNotificationDomainPCSC, SecurityServer::kNotificationAllEvents),
	  MachServer::Timer(true), // "heavy" timer task
	  server(server),
	  mServiceLevel(level),
	  mTimerAction(&PCSCMonitor::initialSetup),
	  mGoingToSleep(false),
	  mCachePath(pathToCache),
	  mTokenCache(NULL)
{
	// do all the smartcard-related work once the event loop has started
	server.setTimer(this, Time::now());		// ASAP
}


//
// Poll PCSC for smartcard status.
// We are enumerating all readers on each call.
//
void PCSCMonitor::pollReaders()
{
	// open PCSC session if it's not already open
	mSession.open();

	// enumerate readers
	vector<string> names;  // will hold reader name C strings throughout
	mSession.listReaders(names);
	size_t count = names.size();
	secdebug("pcsc", "%ld reader(s) in system", count);
	
	// build the PCSC status inquiry array
	vector<PCSC::ReaderState> states(count); // reader status array (PCSC style)
	for (unsigned int n = 0; n < count; n++) {
		PCSC::ReaderState &state = states[n];
		ReaderMap::iterator it = mReaders.find(names[n]);
		if (it == mReaders.end()) { // new reader
			state.clearPod();
			state.name(names[n].c_str());
			// lastKnown(PCSC_STATE_UNKNOWN)
			// userData<Reader>() = NULL
		} else {
			state = it->second->pcscState();
			state.name(names[n].c_str());  // OUR pointer
			state.lastKnown(state.state());
			state.userData<Reader>() = it->second;
		}
	}
	
	// now ask PCSC for status changes
	mSession.statusChange(states);
#if 0 //DEBUGDUMP
	if (Debug::dumping("pcsc"))
		for (unsigned int n = 0; n < count; n++)
			states[n].dump();
#endif

	// make a set of previously known reader objects (to catch those who disappeared)
	ReaderSet current;
	copy_second(mReaders.begin(), mReaders.end(), inserter(current, current.end()));
		
	// match state array against them
	for (unsigned int n = 0; n < count; n++) {
		PCSC::ReaderState &state = states[n];
		if (Reader *reader = state.userData<Reader>()) {
			// if PCSC flags a change, notify the Reader
			if (state.changed())
				reader->update(state);
			// accounted for this reader
			current.erase(reader);
		} else {
			RefPointer<Reader> newReader = new Reader(tokenCache(), state);
			mReaders.insert(make_pair(state.name(), newReader));
			Syslog::notice("Token reader %s inserted into system", state.name());
			newReader->update(state);		// initial state setup
		}
	}
	
	// now deal with known readers that PCSC did not report
	for (ReaderSet::iterator it = current.begin(); it != current.end(); it++) {
		switch ((*it)->type()) {
		case Reader::pcsc:
			// previous PCSC reader - was removed from system
			secdebug("pcsc", "removing reader %s", (*it)->name().c_str());
			Syslog::notice("Token reader %s removed from system", (*it)->name().c_str());
			(*it)->kill();						// prepare to die
			mReaders.erase((*it)->name());		// remove from reader map
			break;
		case Reader::software:
			// previous software reader - keep
			break;
		}
	}
}


//
// Remove some types of readers
//
void PCSCMonitor::clearReaders(Reader::Type type)
{
	if (!mReaders.empty()) {
		secdebug("pcsc", "%ld readers present - clearing type %d", mReaders.size(), type);
		for (ReaderMap::iterator it = mReaders.begin(); it != mReaders.end(); ) {
			ReaderMap::iterator cur = it++;
			Reader *reader = cur->second;
			if (reader->isType(type)) {
				secdebug("pcsc", "removing reader %s", reader->name().c_str());
				reader->kill();						// prepare to die
				mReaders.erase(cur);
			}
		}
	}
}


//
// Poll PCSC for smartcard status.
// We are enumerating all readers on each call.
//
TokenCache& PCSCMonitor::tokenCache()
{
	if (mTokenCache == NULL)
		mTokenCache = new TokenCache(mCachePath.c_str());
	return *mTokenCache;
}



void PCSCMonitor::launchPcscd()
{
	// launch pcscd
	secdebug("pcsc", "launching pcscd to handle smartcard device(s)");
	assert(Child::state() != alive);
	Child::reset();
	Child::fork();

	// if pcscd doesn't report a reader found soon, we'll kill it off
	scheduleTimer(true);
}


//
// Code to launch pcscd (run in child as a result of Child::fork())
//
void PCSCMonitor::childAction()
{
	// move aside any old play area
	const char *aside = tempnam("/tmp", "pcscd");
	if (::rename(PCSCD_WORKING_DIR, aside))
		switch (errno) {
		case ENOENT:		// no /tmp/pcsc (fine)
			break;
		default:
			secdebug("pcsc", "failed too move %s - errno=%d", PCSCD_WORKING_DIR, errno);
			_exit(101);
		}
	else
		secdebug("pcsc", "old /tmp/pcsc moved to %s", aside);

	// lessen the pain for debugging
#if !defined(NDEBUG)
	freopen("/tmp/pcsc.debuglog", "a", stdout);	// shut up pcsc dumps to stdout
#endif //NDEBUG
	
	// execute the daemon
	const char *pcscdPath = PCSCD_EXEC_PATH;
	if (const char *env = getenv("PCSCDAEMON"))
		pcscdPath = env;
	secdebug("pcsc", "exec(%s,-f)", pcscdPath);
	execl(pcscdPath, pcscdPath, "-f", NULL);
}


//
// Event notifier.
// These events are sent by pcscd for our (sole) benefit.
//
void PCSCMonitor::notifyMe(Notification *message)
{
	Server::active().longTermActivity();
	StLock<Mutex> _(*this);
	assert(mServiceLevel == externalDaemon || Child::state() == alive);
	if (message->event == kNotificationPCSCInitialized)
		clearReaders(Reader::pcsc);
	pollReaders();
	scheduleTimer(mReaders.empty() && !mGoingToSleep);
}


//
// Power event notifications
//
void PCSCMonitor::systemWillSleep()
{
	StLock<Mutex> _(*this);
	secdebug("pcsc", "setting sleep marker (%ld readers as of now)", mReaders.size());
	mGoingToSleep = true;
	server.clearTimer(this);
}

void PCSCMonitor::systemIsWaking()
{
	StLock<Mutex> _(*this);
	secdebug("pcsc", "clearing sleep marker (%ld readers as of now)", mReaders.size());
	mGoingToSleep = false;
	scheduleTimer(mReaders.empty());
}


//
// Timer action.
//
void PCSCMonitor::action()
{
	StLock<Mutex> _(*this);
	(this->*mTimerAction)();
	mTimerAction = &PCSCMonitor::noDeviceTimeout;
}


//
// Update the timeout timer as requested (and indicated by context)
//
void PCSCMonitor::scheduleTimer(bool enable)
{
	if (Child::state() == alive)	// we ran pcscd; let's manage it
		if (enable) {
			secdebug("pcsc", "setting idle timer for %g seconds", PCSCD_IDLE_SHUTDOWN.seconds());
			server.setTimer(this, PCSCD_IDLE_SHUTDOWN);
		} else if (Timer::scheduled()) {
			secdebug("pcsc", "clearing idle timer");
			server.clearTimer(this);
		}
}


//
// Perform the initial PCSC subsystem initialization.
// This runs (shortly) after securityd is fully functional and the
// server loop has started.
//
void PCSCMonitor::initialSetup()
{
	switch (mServiceLevel) {
	case forcedOff:
		secdebug("pcsc", "smartcard operation is FORCED OFF");
		break;
	
	case forcedOn:
		secdebug("pcsc", "pcscd launch is forced on");
		launchPcscd();
		startSoftTokens();
		break;

	case externalDaemon:
		secdebug("pcsc", "using external pcscd (if any); no launch operations");
		startSoftTokens();
		break;
	
	default:
		secdebug("pcsc", "setting up automatic PCSC management in %s mode",
			mServiceLevel == conservative ? "conservative" : "aggressive");

		// receive Mach-based IOKit notifications through mIOKitNotifier
		server.add(mIOKitNotifier);
		
		// receive power event notifications (through our IOPowerWatcher personality)
		server.add(this);
	
		// ask for IOKit notifications for all new USB devices and process present ones
		IOKit::DeviceMatch usbSelector(kIOUSBInterfaceClassName);
		IOKit::DeviceMatch pcCardSelector("IOPCCard16Device");
		mIOKitNotifier.add(usbSelector, *this);	// this will scan existing USB devices
		mIOKitNotifier.add(pcCardSelector, *this);	// ditto for PC Card devices
		if (mServiceLevel == aggressive) {
			// catch custom non-composite USB devices - they don't have IOServices attached
			IOKit::DeviceMatch customUsbSelector(::IOServiceMatching("IOUSBDevice"));
			mIOKitNotifier.add(customUsbSelector, *this);	// ditto for custom USB devices
		}
		
		// find and start software tokens
		startSoftTokens();

		break;
	}
	
	// we are NOT scanning for PCSC devices here. Pcscd will send us a notification when it's up
}


//
// This function is called (as a timer function) when there haven't been any (recognized)
// smartcard devicees in the system for a while.
//
void PCSCMonitor::noDeviceTimeout()
{
	secdebug("pcsc", "killing pcscd (no smartcard devices present for %g seconds)",
		PCSCD_IDLE_SHUTDOWN.seconds());
	assert(mReaders.empty());
	Child::kill(SIGTERM);
}


//
// IOKit device event notification.
// Here we listen for newly inserted devices and check whether to launch pcscd.
//
void PCSCMonitor::ioChange(IOKit::DeviceIterator &iterator)
{
	assert(mServiceLevel != externalDaemon && mServiceLevel != forcedOff);
	if (Child::state() == alive) {
		secdebug("pcsc", "pcscd is alive; ignoring device insertion(s)");
		return;
	}
	secdebug("pcsc", "processing device insertion notices");
	while (IOKit::Device dev = iterator()) {
		bool launch = false;
		switch (deviceSupport(dev)) {
		case definite:
			launch = true;
			break;
		case possible:
			launch = (mServiceLevel == aggressive);
			break;
		case impossible:
			break;
		}
		if (launch) {
			launchPcscd();
			return;
		}
	}
	secdebug("pcsc", "no relevant devices found");
}


//
// Check an IOKit device that's just come online to see if it's
// a smartcard device of some sort.
//
PCSCMonitor::DeviceSupport PCSCMonitor::deviceSupport(const IOKit::Device &dev)
{
	try {
		secdebug("scsel", "%s", dev.path().c_str());

               // composite USB device with interface class
		if (CFRef<CFNumberRef> cfInterface = dev.property<CFNumberRef>("bInterfaceClass"))
			switch (uint32 clas = cfNumber(cfInterface)) {
			case kUSBChipSmartCardInterfaceClass:		// CCID smartcard reader - go
				secdebug("scsel", "  CCID smartcard reader recognized");
				return definite;
			case kUSBVendorSpecificInterfaceClass:
				secdebug("scsel", "  Vendor-specific interface - possible match");
				if (isExcludedDevice(dev))
				{
					secdebug("scsel", "  interface class %d is not a smartcard device (excluded)", clas);
					return impossible;
				}
				return possible;
			default:
				secdebug("scsel", "  interface class %d is not a smartcard device", clas);
				return impossible;
			}

               // noncomposite USB device
		if (CFRef<CFNumberRef> cfDevice = dev.property<CFNumberRef>("bDeviceClass"))
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
               if (CFRef<CFStringRef> ioName = dev.property<CFStringRef>("IOName"))
                       if (cfString(ioName).find("pccard", 0, 1) == 0) {
                               secdebug("scsel", "  PCCard - possible match");
                               return possible;
                       }
		return impossible;
	} catch (...) {
		secdebug("scsel", "  exception while examining device - ignoring it");
		return impossible;
	}
}

bool PCSCMonitor::isExcludedDevice(const IOKit::Device &dev)
{
	uint32_t vendorID = 0, productID = 0;
	// Simplified version of getVendorAndProductID in pcscd
	if (CFRef<CFNumberRef> cfVendorID = dev.property<CFNumberRef>(kUSBVendorID))
		vendorID = cfNumber(cfVendorID);

	if (CFRef<CFNumberRef> cfProductID = dev.property<CFNumberRef>(kUSBProductID))
		productID = cfNumber(cfProductID);
	
	secdebug("scsel", "  checking device for possible exclusion [vendor id: 0x%08X, product id: 0x%08X]", vendorID, productID);

	if ((vendorID & kVendorProductMask) != kVendorIDApple)
		return false;	// i.e. it is not an excluded device
	
	// Since Apple does not manufacture smartcard readers, just exclude
	// If we even start making them, we should make it a CCID reader anyway
	
	return true;
}

//
// This gets called (by the Unix/Child system) when pcscd has died for any reason
//
void PCSCMonitor::dying()
{
	Server::active().longTermActivity();
	StLock<Mutex> _(*this);
	assert(Child::state() == dead);
	clearReaders(Reader::pcsc);
	//@@@ this is where we would attempt a restart, if we wanted to...
}


//
// Software token support
//
void PCSCMonitor::startSoftTokens()
{
	// clear all software readers. This will kill the respective TokenDaemons
	clearReaders(Reader::software);

	// scan for new ones
	CodeRepository<Bundle> candidates("Security/tokend", ".tokend", "TOKENDAEMONPATH", false);
	candidates.update();
	for (CodeRepository<Bundle>::iterator it = candidates.begin(); it != candidates.end(); ++it) {
		if (CFTypeRef type = (*it)->infoPlistItem("TokendType"))
			if (CFEqual(type, CFSTR("software")))
				loadSoftToken(*it);
	}
}

void PCSCMonitor::loadSoftToken(Bundle *tokendBundle)
{
	try {
		string bundleName = tokendBundle->identifier();
		
		// prepare a virtual reader, removing any existing one (this would kill a previous tokend)
		assert(mReaders.find(bundleName) == mReaders.end());	// not already present
		RefPointer<Reader> reader = new Reader(tokenCache(), bundleName);

		// now launch the tokend
		RefPointer<TokenDaemon> tokend = new TokenDaemon(tokendBundle,
			reader->name(), reader->pcscState(), reader->cache);
		
		if (tokend->state() == ServerChild::dead) {	// ah well, this one's no good
			secdebug("pcsc", "softtoken %s tokend launch failed", bundleName.c_str());
			Syslog::notice("Software token %s failed to run", tokendBundle->canonicalPath().c_str());
			return;
		}
		
		// probe the (single) tokend
		if (!tokend->probe()) {		// non comprende...
			secdebug("pcsc", "softtoken %s probe failed", bundleName.c_str());
			Syslog::notice("Software token %s refused operation", tokendBundle->canonicalPath().c_str());
			return;
		}
		
		// okay, this seems to work. Set it up
		mReaders.insert(make_pair(reader->name(), reader));
		reader->insertToken(tokend);
		Syslog::notice("Software token %s activated", bundleName.c_str());
	} catch (...) {
		secdebug("pcsc", "exception loading softtoken %s - continuing", tokendBundle->identifier().c_str());
	}
}
