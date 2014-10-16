/*
 * Copyright (c) 2012-2013 Apple Inc.
 * All rights reserved.
 */
#ifndef __APP_LAYER_H__
#define __APP_LAYER_H__

void app_layer_init(CFRunLoopRef rl, CFStringRef rl_mode);
void app_layer_remove_app(CFStringRef bundle_id);
void app_layer_install_app(CFStringRef signing_id);
void app_layer_handle_network_detection_change(CFStringRef service_id);

#endif /* __APP_LAYER_H__ */
