#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <notify.h>
#include <dispatch/dispatch.h>

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

time_t    slept_at = 0;
time_t    woke_at = 0;
time_t    swept_at = 0;

static int sleeping = 0;

int check_power_context;      // dummy field for dispatch call 
extern void check_power_mgmt (void*);

#ifdef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
#define WAKE_CAPS               (kIOPMSystemPowerStateCapabilityCPU | kIOPMSystemPowerStateCapabilityNetwork)

IOPMConnection                       gPMConnection = NULL;

static void
iosleep_capabilities_notifier(void *param, IOPMConnection connection, IOPMConnectionMessageToken token, IOPMSystemPowerStateCapabilities capabilities)
{
	plog(ASL_LEVEL_DEBUG, "received power-mgmt event: capabilities %X%s%s%s%s%s",
		   capabilities,
		   capabilities & kIOPMSystemPowerStateCapabilityCPU     ? " CPU"     : "",
		   capabilities & kIOPMSystemPowerStateCapabilityVideo   ? " Video"   : "",
		   capabilities & kIOPMSystemPowerStateCapabilityAudio   ? " Audio"   : "",
		   capabilities & kIOPMSystemPowerStateCapabilityNetwork ? " Network" : "",
		   capabilities & kIOPMSystemPowerStateCapabilityDisk    ? " Disk"    : "");

	if ((capabilities & WAKE_CAPS) != WAKE_CAPS) {
		if (!sleeping) {
			plog(ASL_LEVEL_DEBUG, 
				 "received power-mgmt event: will sleep\n");		
			sleeping = 1;
			slept_at = current_time();
		} else {
			plog(ASL_LEVEL_DEBUG, 
				 "ignored power-mgmt event: sleep(%x) while asleep\n", capabilities);		
		}
		IOPMConnectionAcknowledgeEvent(connection, token );
	} else if ((capabilities & WAKE_CAPS) == WAKE_CAPS) {
		// allow processing of packets
		if (sleeping) {
			plog(ASL_LEVEL_DEBUG, 
				 "received power-mgmt event: will wake(%x)\n", capabilities);
			sleeping = 0;
			woke_at = current_time();
		} else {
			plog(ASL_LEVEL_DEBUG, 
				 "ignored power-mgmt event: wake(%x) while not asleep\n", capabilities);
		}
		IOPMConnectionAcknowledgeEvent(connection, token);
	} else {
		plog(ASL_LEVEL_DEBUG, 
			 "ignored power-mgmt event: capabilities(%x)\n", capabilities);
		IOPMConnectionAcknowledgeEvent(connection, token);
	}
    dispatch_async_f(dispatch_get_main_queue(), &check_power_context, &check_power_mgmt);
}

#else

static 
void iosleep_notifier(void * x, io_service_t y, natural_t messageType, void *messageArgument)
{
	switch ( messageType ) {
		case kIOMessageSystemWillSleep:
			sleeping = 1;
			slept_at = current_time();
			plog(ASL_LEVEL_DEBUG, 
				 "received power-mgmt event: will sleep\n");
			IOAllowPowerChange(gIOPort, (long)messageArgument);
			break;
		case kIOMessageCanSystemSleep:
			IOAllowPowerChange(gIOPort, (long)messageArgument);
			break;
		case kIOMessageSystemWillNotSleep:
			/* someone refused an idle sleep */
			plog(ASL_LEVEL_DEBUG, 
				 "received power-mgmt event: will not sleep\n");
			sleeping = 0;
			slept_at = 0;
			break;
		case kIOMessageSystemWillPowerOn:
			if (sleeping) {
				plog(ASL_LEVEL_DEBUG, 
					 "received power-mgmt event: will wake\n");
				sleeping = 0;
			} else {
				plog(ASL_LEVEL_DEBUG, 
					 "received power-mgmt event: will power-on\n");
			}
			break;
		case kIOMessageSystemHasPoweredOn:
			woke_at = current_time();
			if (slept_at) {
				plog(ASL_LEVEL_DEBUG, 
					 "received power-mgmt event: has woken\n");
			} else {
				plog(ASL_LEVEL_DEBUG, 
					 "received power-mgmt event: has powered-on\n");
			}
			break;
		default:
			plog(ASL_LEVEL_DEBUG, 
				 "received power-mgmt event: %x\n", messageType);
			break;
	}
    dispatch_async_f(dispatch_get_main_queue(), &check_power_context, &check_power_mgmt);
}
#endif // kIOPMAcknowledgmentOptionSystemCapabilityRequirements

int
init_power_mgmt (void)
{
#ifdef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
	IOReturn ret;

	ret = IOPMConnectionCreate(CFSTR("racoon power-mgmt"),
							   WAKE_CAPS,
							   &gPMConnection);
	if (ret != kIOReturnSuccess) {
		plog(ASL_LEVEL_ERR, "IOPMConnectionCreate failed (%d) power-mgmt thread\n", ret);
		return -1;
	}
	
	ret = IOPMConnectionSetNotification(gPMConnection, NULL, iosleep_capabilities_notifier);
	if (ret != kIOReturnSuccess) {
		plog(ASL_LEVEL_ERR, "IOPMConnectionCreate failed (%d) power-mgmt thread\n", ret);
		return -1;
	}
	
    IOPMConnectionSetDispatchQueue(gPMConnection, dispatch_get_main_queue());
    
#else		
	if ((gIOPort = IORegisterForSystemPower(0, &notify, iosleep_notifier, &iterator)) == MACH_PORT_NULL) {
		plog(ASL_LEVEL_ERR, 
			 "IORegisterForSystemPower failed for power-mgmt thread\n");
		return -1;
	}
    
    IONotificationPortSetDispatchQueue(notify, dispatch_get_main_queue());
	
#endif // kIOPMAcknowledgmentOptionSystemCapabilityRequirements

	return 0;
}

void
cleanup_power_mgmt (void)
{
#ifdef kIOPMAcknowledgmentOptionSystemCapabilityRequirements
    
    IOPMConnectionSetDispatchQueue(gPMConnection, NULL);    
    IOPMConnectionRelease(gPMConnection);
    
#else
    
    IODeregisterForSystemPower(&iterator);
    IONotificationPortDestroy(notify);
    
#endif // kIOPMAcknowledgmentOptionSystemCapabilityRequirements
    
}

void
check_power_mgmt (void *context)
{
	if (slept_at && woke_at) {
		plog(ASL_LEVEL_DEBUG, 
			 "handling power-mgmt event: sleep-wake\n");
		swept_at = current_time();		
		sweep_sleepwake();
		slept_at = 0;
		woke_at = 0;
	} else if (woke_at) {
		plog(ASL_LEVEL_DEBUG, 
			 "handling power-mgmt event: power-on\n");
		woke_at = 0;
	}
}
