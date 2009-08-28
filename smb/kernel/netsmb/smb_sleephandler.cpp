/*
 * Copyright (c) 2003 - 2008 Apple Inc. All rights reserved.
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
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>

extern "C"
{
#include <sys/smb_apple.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <netsmb/smb_subr.h>

#ifdef SMB_DEBUG
	int32_t gSMBSleeping = 0;
#endif // SMB_DEBUG
	struct timespec gWakeTime = {0, 0};
    void wakeup(void *);
}
#include <netsmb/smb_sleephandler.h>


static IOReturn
smb_sleepwakehandler(void *target, void *refCon, UInt32 messageType, IOService *provider, void *messageArgument, vm_size_t argSize)
{
#pragma unused (target, refCon, provider, messageArgument, argSize)
	switch (messageType) {

	case kIOMessageSystemWillSleep:
		SMBDEBUG(" going to sleep\n");
#ifdef SMB_DEBUG
		gSMBSleeping = 1;
#endif // SMB_DEBUG
		break;

	case kIOMessageSystemHasPoweredOn:
		SMBDEBUG("  waking up\n");
#ifdef SMB_DEBUG
		gSMBSleeping = 0;
#endif // SMB_DEBUG
		nanouptime(&gWakeTime);
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
		fNotifier = registerSleepWakeInterest(smb_sleepwakehandler, NULL, NULL);
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
