/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOMessage.h>

#include <mach/mach_types.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IONotifier.h>
#include <IOKit/IOLib.h>

extern "C" {
int  smb_share_count(void);
}

#include <netsmb/smb_sleephandler.h>

IOReturn
smb_sleepwakehandler(void *target, void *refCon, UInt32 messageType,
    IOService *provider, void *messageArgument, vm_size_t argSize)
{        
	IOPowerStateChangeNotification *notify =
	    (IOPowerStateChangeNotification *)messageArgument;

	switch (messageType) {

	case kIOMessageSystemWillSleep:
		acknowledgeSleepWakeNotification(notify->powerRef);
		break;
            
	case kIOMessageCanSystemSleep:
		if (smb_share_count() > 0)
			vetoSleepWakeNotification(notify->powerRef);
		else
			acknowledgeSleepWakeNotification(notify->powerRef);
		break;

	case kIOMessageSystemHasPoweredOn:
		break;
        
	default:
		break;
	}
    
	return (IOPMAckImplied);
}

extern "C" {
	IONotifier *fNotifier = NULL;

	__private_extern__ void smbfs_install_sleep_wake_notifier()
	{
		fNotifier = registerSleepWakeInterest(smb_sleepwakehandler,
		    NULL, NULL);
	}

	__private_extern__ void smbfs_remove_sleep_wake_notifier()
	{
		if (fNotifier != NULL) {
			fNotifier->disable();
			//fNotifier->release();  /* if you call this, you kernel panic radar 2946001 */
			fNotifier = NULL;
		}
	}
}
