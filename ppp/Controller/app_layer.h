/*
 * Copyright (c) 2012-2013 Apple Inc.
 * All rights reserved.
 */
#ifndef __APP_LAYER_H__
#define __APP_LAYER_H__

void app_layer_init(CFRunLoopRef rl, CFStringRef rl_mode);
void app_layer_remove_app(CFStringRef bundle_id);

#endif /* __APP_LAYER_H__ */
