/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All Rights Reserved.
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please
 * obtain a copy of the License at http://www.apple.com/publicsource and
 * read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please
 * see the License for the specific language governing rights and
 * limitations under the License.
 */

/******************************************************************

	MUSCLE SmartCard Development ( http://www.linuxnet.com )
	    Title  : hotplug_macosx.c
	    Package: pcsc lite
      Author : Stephen M. Webb <stephenw@cryptocard.com>
      Date   : 03 Dec 2002
	    License: Copyright (C) 2002 David Corcoran
	             <corcoran@linuxnet.com>
            Purpose: This provides a search API for hot pluggble
	             devices.
	            
********************************************************************/

#include <MacTypes.h>
#include "wintypes.h"

#include "hotplug.h"
#include "pthread.h"
#include "PCSCDriverBundles.h"
#include "pcscdserver.h"
#include "pcscdmonitor.h"
#include <security_utilities/debugging.h>

const uint32_t kPCSCLITE_HP_BASE_PORT = 0x200000;
PCSCDMonitor *gPCSCDMonitor = NULL;
static Security::MachPlusPlus::Port gMainServerPort;

#ifndef HOTPLUGTEST
	#include "readerfactory.h"
#else
LONG RFAddReader(LPSTR, DWORD, LPSTR)
{
	return 0;
}

LONG RFRemoveReader(LPSTR, DWORD)
{
	return 0;
}
#endif

// See PCSCDMonitor::addDevice for where this is actually called

int32_t WrapRFAddReader(const char *name, uint32_t address, const char *pathLibrary, const char *deviceName)
{
	secdebug("device", "RFAddReader: name: %s, address: %04X, pathLibrary: %s, pathDevice: %s", name, address, pathLibrary, deviceName);
	return RFAddReader(const_cast<char *>(name), kPCSCLITE_HP_BASE_PORT+address, const_cast<char *>(pathLibrary), const_cast<char *>(deviceName));
}

int32_t WrapRFRemoveReader(const char *name, uint32_t address)
{
	secdebug("device", "RFRemoveReader: name: %s, address: %04X", name, address);
	return RFRemoveReader(const_cast<char *>(name), kPCSCLITE_HP_BASE_PORT+address);
}

int32_t WrapRFAwakeAllReaders()
{
	secdebug("device", "RFAwakeAllReaders");
	RFAwakeAllReaders();
	return 0;
}

int32_t WrapRFSuspendAllReaders()
{
	secdebug("device", "RFSuspendAllReaders");
	RFSuspendAllReaders();
	return 0;
}

static void *HPDeviceNotificationThread(void *foo)
{
	try
	{
		// Thread runner (does not return)
		PCSCD::DriverBundles bdls;
		PCSCD::Server myserv("hotplug");
		PCSCDMonitor xmon(myserv,bdls);
		gPCSCDMonitor = &xmon;
		gMainServerPort = myserv.primaryServicePort();
		xmon.setCallbacks(WrapRFAddReader, WrapRFRemoveReader, WrapRFSuspendAllReaders, WrapRFAwakeAllReaders);
		bdls.update();
		myserv.run();
	}
	catch (Security::MachPlusPlus::Error e)
	{
		char *perr = (char *)mach_error_string(e.error);
		if (perr)
			secdebug("device", "Caught error in xx: %s, error: %04lX", perr, e.osStatus());
		else
			secdebug("device", "Caught error in xx: %04X", e.error);
	}
	catch (...)
	{
	}
	exit(0);
	return NULL;	// never gets here
}

void systemAwakeAndReadyCheck()
{
	gPCSCDMonitor->systemAwakeAndReadyCheck();
}

/*
 * Scans the hotplug driver directory and looks in the system for matching devices.
 * Adds or removes matching readers as necessary.
 */
int32_t HPSearchHotPluggables()
{
	// this function is a no-op now
    return 0;
}

static pthread_t sHotplugWatcherThread;

int32_t HPRegisterForHotplugEvents()
{
	return HPRegisterForHotplugEventsT(&sHotplugWatcherThread);
}

int32_t HPRegisterForHotplugEventsT(pthread_t *wthread)
{
	// Sets up callbacks for device hotplug events
	int rx = pthread_create(wthread, NULL, HPDeviceNotificationThread, NULL);
    return rx;
}

LONG HPStopHotPluggables(void)
{
	int rx = pthread_detach(sHotplugWatcherThread);
	return rx;
}

void HPReCheckSerialReaders(void)
{
}

LONG HPCancelHotPluggables(void)
{
	int rx = pthread_cancel(sHotplugWatcherThread);
	return rx;
}

LONG HPJoinHotPluggables(void)
{
	char *value_ptr;
	int rx = pthread_join(sHotplugWatcherThread, (void **)&value_ptr);
	return rx;
}
