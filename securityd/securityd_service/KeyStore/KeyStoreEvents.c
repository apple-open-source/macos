/* Copyright (c) 2013 Apple Inc. All rights reserved. */

#include "AppleKeyStoreEvents.h"

#include <unistd.h>
#include <notify.h>
#include <syslog.h>
#include <AssertMacros.h>
#include <IOKit/IOKitLib.h>
#include <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>
#include <xpc/event_private.h>

static void aksNotificationCallback(void *refcon,io_service_t service, natural_t messageType, void *messageArgument)
{
	if(messageType == kAppleKeyStoreLockStateChangeMessage) {
//        syslog(LOG_ERR, "KeyStoreNotifier - %s posting notification: %s\n", __func__, kAppleKeyStoreLockStatusNotificationID);
		notify_post(kAppleKeyStoreLockStatusNotificationID);
	} else if (messageType == kAppleKeyStoreFirstUnlockMessage) {
//        syslog(LOG_ERR, "KeyStoreNotifier - %s posting notification: %s\n", __func__, kAppleKeyStoreFirstUnlockNotificationID);
		notify_post(kAppleKeyStoreFirstUnlockNotificationID);
    }
}

static void start(dispatch_queue_t queue)
{
	IOReturn result;
	io_service_t aksService = IO_OBJECT_NULL;
	IONotificationPortRef aksNotifyPort = IO_OBJECT_NULL;
	io_object_t notification = IO_OBJECT_NULL;

	aksService = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching(kAppleKeyStoreServiceName));
	require_action(aksService, cleanup, syslog(LOG_ERR, "KeyStoreNotifier - Can't find %s service", kAppleKeyStoreServiceName));

	aksNotifyPort = IONotificationPortCreate(kIOMasterPortDefault);
	require_action(aksNotifyPort, cleanup, syslog(LOG_ERR, "KeyStoreNotifier - Can't create notification port"));

    IONotificationPortSetDispatchQueue(aksNotifyPort, queue);

	result = IOServiceAddInterestNotification(aksNotifyPort, aksService, kIOGeneralInterest, aksNotificationCallback, NULL, &notification);
	require_noerr_action(result, cleanup, syslog(LOG_ERR, "KeyStoreNotifier - Can't register for notification: %08x", result));
	return;

cleanup:
	if (aksNotifyPort) IONotificationPortDestroy(aksNotifyPort);
    if (notification) IOObjectRelease(notification);
    if (aksService) IOObjectRelease(aksService);
	return;
}

void
init_keystore_events(xpc_event_module_t module)
{
    start(xpc_event_module_get_queue(module));
}
