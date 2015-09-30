/*
 * Copyright (c) 2012-2013, 2015 Apple Inc. All rights reserved.
 */

#include <SystemConfiguration/SCPrivate.h>
#include "app_layer.h"

void
app_layer_install_app(CFStringRef signing_id)
{
#pragma unused(signing_id)
}

void
app_layer_remove_app(CFStringRef signing_id)
{
#pragma unused(signing_id)
}

void
app_layer_handle_network_detection_change(CFStringRef service_id)
{
#pragma unused(service_id)
}

void
app_layer_prefs_changed(SCPreferencesRef prefs, SCPreferencesNotification notification_type, void *info)
{
#pragma unused(info, notification_type, info)
}

void
app_layer_init(CFRunLoopRef rl, CFStringRef rl_mode)
{
#pragma unused(rl, rl_mode)
}
