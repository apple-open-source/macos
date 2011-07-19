#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <notify.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFUserNotification.h>
#include <mach/mach_port.h>
#include <mach/mach_interface.h>
#include <mach/mach_init.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPM.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#if !TARGET_OS_EMBEDDED
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#endif /* !TARGET_OS_EMBEDDED */
#include <IOKit/IOMessage.h>

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "schedule.h"
#include "debug.h"

#include "isakmp_var.h"
#include "isakmp.h"
#include "handler.h"

#ifndef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
IONotificationPortRef notify;
io_object_t			  iterator;
io_connect_t          gIOPort;
CFUserNotificationRef gSleepNotification = NULL;
#endif // !kIOPMAcknowledgmentOptionSystemCapabilityRequirements

pthread_t power_mgmt_thread;
time_t    slept_at = 0;
time_t    woke_at = 0;
time_t    swept_at = 0;

static int sleeping = 0;

#ifdef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
#define WAKE_CAPS               (kIOPMSystemPowerStateCapabilityCPU | kIOPMSystemPowerStateCapabilityNetwork)

IOPMConnection                       gPMConnection = NULL;

static void
iosleep_capabilities_notifier(void *param, IOPMConnection connection, IOPMConnectionMessageToken token, IOPMSystemPowerStateCapabilities capabilities)
{
	plog(LLV_DEBUG, LOCATION, NULL,"received power-mgmt event: capabilities %X%s%s%s%s%s",
		   capabilities,
		   capabilities & kIOPMSystemPowerStateCapabilityCPU     ? " CPU"     : "",
		   capabilities & kIOPMSystemPowerStateCapabilityVideo   ? " Video"   : "",
		   capabilities & kIOPMSystemPowerStateCapabilityAudio   ? " Audio"   : "",
		   capabilities & kIOPMSystemPowerStateCapabilityNetwork ? " Network" : "",
		   capabilities & kIOPMSystemPowerStateCapabilityDisk    ? " Disk"    : "");

	if ((capabilities & WAKE_CAPS) != WAKE_CAPS) {
		if (!sleeping) {
			plog(LLV_DEBUG, LOCATION, NULL,
				 "received power-mgmt event: will sleep\n");		
			sleeping = 1;
			slept_at = current_time();
		} else {
			plog(LLV_DEBUG, LOCATION, NULL,
				 "ignored power-mgmt event: sleep(%x) while asleep\n", capabilities);		
		}
		IOPMConnectionAcknowledgeEvent(connection, token );
	} else if ((capabilities & WAKE_CAPS) == WAKE_CAPS) {
		// allow processing of packets
		if (sleeping) {
			plog(LLV_DEBUG, LOCATION, NULL,
				 "received power-mgmt event: will wake(%x)\n", capabilities);
			sleeping = 0;
			woke_at = current_time();
		} else {
			plog(LLV_DEBUG, LOCATION, NULL,
				 "ignored power-mgmt event: wake(%x) while not asleep\n", capabilities);
		}
		IOPMConnectionAcknowledgeEvent(connection, token);
	} else {
		plog(LLV_DEBUG, LOCATION, NULL,
			 "ignored power-mgmt event: capabilities(%x)\n", capabilities);
		IOPMConnectionAcknowledgeEvent(connection, token);
	}
}

#else

static 
void iosleep_notifier(void * x, io_service_t y, natural_t messageType, void *messageArgument)
{
	switch ( messageType ) {
		case kIOMessageSystemWillSleep:
			sleeping = 1;
			slept_at = current_time();
			plog(LLV_DEBUG, LOCATION, NULL,
				 "received power-mgmt event: will sleep\n");
			IOAllowPowerChange(gIOPort, (long)messageArgument);
			break;
		case kIOMessageCanSystemSleep:
			IOAllowPowerChange(gIOPort, (long)messageArgument);
			break;
		case kIOMessageSystemWillNotSleep:
			/* someone refused an idle sleep */
			plog(LLV_DEBUG, LOCATION, NULL,
				 "received power-mgmt event: will not sleep\n");
			sleeping = 0;
			slept_at = 0;
			break;
		case kIOMessageSystemWillPowerOn:
			if (sleeping) {
				plog(LLV_DEBUG, LOCATION, NULL,
					 "received power-mgmt event: will wake\n");
				sleeping = 0;
			} else {
				plog(LLV_DEBUG, LOCATION, NULL,
					 "received power-mgmt event: will power-on\n");
			}
			break;
		case kIOMessageSystemHasPoweredOn:
			woke_at = current_time();
			if (slept_at) {
				plog(LLV_DEBUG, LOCATION, NULL,
					 "received power-mgmt event: has woken\n");
			} else {
				plog(LLV_DEBUG, LOCATION, NULL,
					 "received power-mgmt event: has powered-on\n");
			}
			break;
		default:
			plog(LLV_DEBUG, LOCATION, NULL,
				 "received power-mgmt event: %x\n", messageType);
			break;
	}
}
#endif // kIOPMAcknowledgmentOptionSystemCapabilityRequirements

void *
power_mgmt_thread_func (void *arg)
{
#ifdef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
	IOReturn ret;

	ret = IOPMConnectionCreate(CFSTR("racoon power-mgmt"),
							   WAKE_CAPS,
							   &gPMConnection);
	if (ret != kIOReturnSuccess) {
		plog(LLV_ERROR, LOCATION, NULL,"IOPMConnectionCreate failed (%d) power-mgmt thread\n", ret);
		return NULL;
	}
	
	ret = IOPMConnectionSetNotification(gPMConnection, NULL, iosleep_capabilities_notifier);
	if (ret != kIOReturnSuccess) {
		plog(LLV_ERROR, LOCATION, NULL,"IOPMConnectionCreate failed (%d) power-mgmt thread\n", ret);
		return NULL;
	}
	
	ret = IOPMConnectionScheduleWithRunLoop(gPMConnection, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	if (ret != kIOReturnSuccess) {
		plog(LLV_ERROR, LOCATION, NULL,"IOPMConnectionCreate failed (%d) power-mgmt thread\n", ret);
		return NULL;
	}
#else		
	if ((gIOPort = IORegisterForSystemPower(0, &notify, iosleep_notifier, &iterator)) == MACH_PORT_NULL) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "IORegisterForSystemPower failed for power-mgmt thread\n");
		return NULL;
	}

	CFRunLoopAddSource(CFRunLoopGetCurrent(),
			   IONotificationPortGetRunLoopSource(notify),
			   kCFRunLoopDefaultMode);
#endif // kIOPMAcknowledgmentOptionSystemCapabilityRequirements

	CFRunLoopRun();
	return NULL;
}

int
init_power_mgmt (void)
{
	int err;

	if ((err = pthread_create(&power_mgmt_thread, NULL, power_mgmt_thread_func, NULL))) {
		plog(LLV_ERROR, LOCATION, NULL,
			 "failed to create power-mgmt thread: %d\n", err);
		return -1;
	}

	return 0;
}

void
check_power_mgmt (void)
{
	if (slept_at && woke_at) {
		plog(LLV_DEBUG, LOCATION, NULL,
			 "handling power-mgmt event: sleep-wake\n");
		swept_at = current_time();		
		sweep_sleepwake();
		slept_at = 0;
		woke_at = 0;
	} else if (woke_at) {
		plog(LLV_DEBUG, LOCATION, NULL,
			 "handling power-mgmt event: power-on\n");
		woke_at = 0;
	}
}
